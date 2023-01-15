#include <fstream>
#include <iostream>
#include <vector>
#include <deque>
#include <string>
#include <climits>
#include "thread.h"
#include "disk.h"

using std::cout;
using std::endl;

mutex queueLock;
cv reqcv;
cv servcv;

// Globals
int num_file = 0;
int max_disk_queue = 0;
int cur_file_num = 0;
int req_num_cum = 0;
int cur_act = 0;
std::vector<int> queue;
std::vector<int> check_left;
char** file_names = nullptr;


void request(void *a) {
    queueLock.lock();
    int req_num = req_num_cum;
    req_num_cum ++;
    queueLock.unlock();
    
    // read in address in this file
    int input = 0;
    std::ifstream inp(file_names[req_num + 2]);
    std::deque<int> address;
    while (inp >> input) address.push_back(input);
    check_left[req_num] = address.size();

    queueLock.lock();
    if (address.size() == 0) cur_act --;
    queueLock.unlock();
    inp.close();

    while (address.size() != 0) {
        queueLock.lock();

        // check if the queue is full
        while (cur_file_num >= max_disk_queue) {
            reqcv.wait(queueLock);
        }

        // If the queue is not full, add a new element to the queue
        queue[req_num] = address[0];
        address.pop_front();
        cur_file_num ++;
        print_request(req_num, queue[req_num]);
        servcv.signal();

        // wait for the previous memeory address on this file to be processed
        while (queue[req_num] != -1) {
            reqcv.wait(queueLock);
        }
        queueLock.unlock();
    }
}



void service(void *a) {
    int cur_address = 0;
    while (cur_file_num != 0  || cur_act != 0) {
        // check the current disk status
        queueLock.lock();
        while (cur_file_num < std::min(max_disk_queue, cur_act)) {
            servcv.wait(queueLock);
        }
        // find the min distance addresss
        int min_ind = 0;
        int min_dis = INT_MAX;
        for (int i = 0; i < num_file; i ++) {
            if (queue[i] != -1) {
                if (min_dis > abs(queue[i] - cur_address)) {
                    min_ind = i;
                    min_dis = abs(queue[i] - cur_address);
                }
            }
        }

        // set current address as new one and pop out of the disk
        cur_address = queue[min_ind];
        queue[min_ind] = -1;
        check_left[min_ind] --;
        if (check_left[min_ind] == 0) cur_act --;
        cur_file_num --;
        print_service(min_ind, cur_address);
        reqcv.broadcast();

        queueLock.unlock();
    }
}

void scheduler(void *a) {
    for (int i = 0; i < num_file; i ++) thread t1 ((thread_startfunc_t) request, (void *) 0);
    thread t2 ((thread_startfunc_t) service, (void *) 0);
}


int main(int argc, char ** argv) {
    max_disk_queue = atoi(argv[1]);
    num_file = argc - 2;
    cur_act = num_file;
    for (int i = 0; i < num_file; i ++) {
        queue.push_back(-1);
        check_left.push_back(0);
    }
    file_names = argv;
    cpu::boot((thread_startfunc_t) scheduler , (void *) 100, 0);
    return 0;
}