#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <bits/stdc++.h>

using namespace std;

#define MAXI 5

int e = 20, c = 50, s = 20;	// e = number of events c = capacity s = threads
const int k_min = 5; 		// minimum number of tickets to book
const int k_max = 10; 		// maximum number of tickets to books
int T = 1;			// time in minutes
int queryCount = 0;
int end_thread = 0;

vector<int> event_seats(e, c);		// Initializes the initial amount of seats in each event.

struct query {
    int event; 
    int type; 				// 1-inquiry 2-book tickets 3-cancel ticket
    int thread_num;
};

struct query shared_table[MAXI];

pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;		// Creating and Initializing Mutex
pthread_mutex_t mutex2 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex3 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex4 = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
		

std::mutex seat_map_mutex;  					// Mutex to protect the seat map
std::mutex seat_count_mutex; 					// Mutex to protect the seat count

vector<vector<int>> seat_map(e, vector<int>(c,-1));		//Storing thread number to which each seat of each event is allocated

// Inquire function
int inquire(int event, int tid) {
    cout << "\n\nThread " << tid << " asked number of seats for event "<< event << "\nNumber of seats are : " << event_seats[event];
    return event_seats[event];					//Return the number of seats available in event 
}

// Book function
bool book(int event, int k, int thread_num, vector<vector<int>>& my_bookings) {
	seat_count_mutex.lock();			
	if(event_seats[event] >= k){				// Assigning the first K available seats in the event .
		for(int i = 0 ; i < c ; i++){
			seat_map_mutex.lock();
			if(seat_map[event][i] == -1){
				seat_map[event][i] = thread_num;
				my_bookings[event].push_back(i);//Thread storing which seats are booked in this particular event in its private list of bookings
			}
			seat_map_mutex.unlock();
		}
		event_seats[event] -= k;			//Reducing the available number of seats in the event by k
		seat_count_mutex.unlock();
		return true;
	}
    	seat_count_mutex.unlock();
    	return false;						//Returning False in case there are less than K seats available in the event
}

// Cancel function
void cancel(int event, int thread_num, int ticket, int booked_seat, vector<vector<int>>& my_bookings) {
    seat_map_mutex.lock();
    seat_count_mutex.lock();
    seat_map[event][ticket] = -1;				//Removing the thread number from the ticket
    event_seats[event] += 1;					//Increasing the number of available seats in the event
    seat_map_mutex.unlock();
    seat_count_mutex.unlock();
    vector<int> :: iterator it;
    int j = 0;
    for(it = (my_bookings[event]).begin(); j < booked_seat ; it++)
    {
    	j++;
    }
    my_bookings[event].erase(it);		//Removing the ticket from the private booking list of the Thread
    
    cout << "\n\nBooking of event " << event << " ticket number " << ticket << " was cancelled by thread " << thread_num;
}

void *random_query_generator(void * thread_id){

	// Define a vector to store the bookings made by this worker thread
	vector<vector<int>> my_bookings(e);
	srand(time(0));
	
	int num_bookings;
	int seats;
	int tid = *((int *)thread_id);		// Initializing the thread id from the argument 
	
	while (true) {
	
	    int query_type = (rand() % 3) + 1; 	// choose query type randomly
	    
	    int event_num = rand() % e; 	// choose event number randomly
	     	
	    // check if event is in shared table
	    pthread_mutex_lock(&mutex2);
	    
	    int i,j;
	    
	    //checking if read-write or write-write conflict exists
	    for (i = 0; i < MAXI; i++) {
	    	if (shared_table[i].event == event_num) {
	    		if (shared_table[i].type == 1 && query_type == 1) {
		        	continue;
		        }
		        else{
		            	break;
		        }
		}
	     }
            	
             if(i == MAXI){
	     
		     //if no conflict, search for free entry
		     for (j = 0; j < MAXI; j++) {
		     	if (shared_table[j].event == -1) {
			    break;
			}
		     }
		
		     if(j == MAXI){
		     	//no free entry found
			cout << "\n\nThread " << tid << " no free entry means max query reached.";
			sleep(1);
			pthread_mutex_unlock(&mutex2);
			continue;
		     }
	     }
	     else{
	     	//there is conflict
		cout << "\n\nThread " << tid << " skipping query due to read-write or write-write conflict.";
		sleep(1);
		pthread_mutex_unlock(&mutex2);
		continue;
	     }
			
             shared_table[j].event = event_num;				//updating Shared table with the event triple
	     shared_table[j].type = query_type;
	     shared_table[j].thread_num = tid;
		
             pthread_mutex_unlock(&mutex2);
             cout << "\n\nThread " << tid << " waiting for query to finish.";
        
    	     if(query_type == 1){
    	     	seats = inquire(event_num, tid);
    	     }
    	     else if (query_type == 2) {
        	int k = rand() % (k_max - k_min + 1) + k_min; 		// choose number of tickets to book randomly
        	bool success  = book(event_num, k, tid, my_bookings);
        	if(success) cout << "\n\nBooked " << k << " tickets of event " << event_num << " for thread " << tid;
        	else cout << "\n\nThread " << tid << " requested to book " << k << " seats but " << k << " seats not available for event. " << event_num;
    	     }
    	     else if (query_type == 3) {
    		num_bookings = (my_bookings[event_num]).size();
    		if (num_bookings > 0) {					// Choose a random booking from this worker thread's private list to cancel if there is a booking to cancel
        		int booked_seat = rand() % num_bookings;
        		int ticket = my_bookings[event_num][booked_seat];
        		cancel(event_num, tid, ticket, booked_seat, my_bookings);
        	}
    	     }
    	
	    // remove entry from shared table
	    pthread_mutex_lock(&mutex1);
	    shared_table[j].event = -1;
	    pthread_mutex_unlock(&mutex1);
	    
	    cout << "\n\nThread " << tid << " on 1-2 second timeout before making next query.";
		
	    // Wait for a random amount of time before making the next query
	    int timeToSleep = (rand() % 1000) + 1;
	    this_thread::sleep_for(chrono::milliseconds(timeToSleep));
	    
            if(end_thread == 1) pthread_exit(NULL);
	}
    	
 	pthread_exit(NULL);
}

// Print reservation status for each event
void print_status() {
    cout << endl;
    cout << "\n\nReservation status for each event:" << endl;
    for (int i = 0; i < e; i++) {
        cout << "Event " << i+1 << ": " << event_seats[i] << " seats not booked, " << c - event_seats[i] << " seats booked." << endl;
    }
}


int main(int argc, char* argv[])
{
	
	for(int i = 0 ; i < MAXI ; i++){
		shared_table[i].event = -1;											//Initializing the Shared Table
	}
	 
	pthread_t threads[s];
	
	int ids[s];
	for(int i = 0 ; i < s ; i++) 
		ids[i] = i;

	for(int i = 0 ; i < s ; i++)
	{
		pthread_create(&threads[i], NULL, random_query_generator, (void *)&ids[i]);					//Creating worker threads
	}
	
	auto start_time = std::chrono::steady_clock::now();
	while (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start_time).count() < T*60);	// Main Thread waits for T minutes
	
	// Signal threads to exit
    	for (int i = 0; i < s; i++) {
        	pthread_cancel(threads[i]);
        	end_thread = 1;
    	}
     	
     	print_status();					//Print the final reservation status
	
	for(int i = 0 ; i < s ; i++)
	{
		pthread_mutex_unlock(&mutex1);		//Unlock Mutex and wait for child process to exit
		pthread_mutex_unlock(&mutex2);
		pthread_mutex_unlock(&mutex4);
		pthread_mutex_unlock(&mutex3);
		pthread_cond_signal(&cond);
		pthread_join(threads[i], NULL);
	}
     	
    	pthread_mutex_destroy(&mutex1);			//Destroy Mutex and condition variables
	pthread_mutex_destroy(&mutex2);
	pthread_mutex_destroy(&mutex3);
	pthread_mutex_destroy(&mutex4);
	pthread_cond_destroy(&cond);
    	
    	return 0;
}
