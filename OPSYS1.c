#include<stdio.h> //standard input output library
#include<pthread.h> //used to create threads
#include<stdlib.h> // used to wait function
#include<time.h> // used to sleep funtion
#include<semaphore.h> // used to create semaphores
#include<unistd.h> // used to sleep funtion
#define CUSTOMER_NUMBER 25 // total number of customers
#define CUSTOMER_ARRIVAL_TIME_MIN 1 // min time required to create customer
#define CUSTOMER_ARRIVAL_TIME_MAX 3 // max time required to create customer
#define REGISTER_NUMBER 5 // total number of register
#define COFFEE_TIME_MIN 2 // minimum time required for the customer to buy coffee
#define COFFEE_TIME_MAX 5 // maximum time required for the customer to buy coffee

typedef struct {//A new species definition is made.
    int customer_id; // to specify customer, unique id
    int register_id; // The register id that the customer is gone will be used to lock this customer 
    int customer_arrival_time; // It is determined how long the customers will be formed at the beginning.
}customer; // name of customer struct

//a new function is declared to the compiler
void printing_customer_id_is_created_after_time_seconds(int customer_id, int seconds);

//a new function is declared to the compiler
void printing_customer_id_goes_to_register_id (int customer_id, int register_id); 

//a new function is declared to the compiler
void printing_customer_id_finished_buying_from_register_id_after_time_seconds(int customer_id, int register_id, int seconds);

//The function that register threads will run is reported to the compiler
void* register_(void* id);

//array where the customers formed in order will be placed
customer customer_array[CUSTOMER_NUMBER];

//the total number of customers created is stored in this static variable
int total_number_of_customers_created = 0;

//the total number of customers buying coffee is kept in this variable
int total_number_of_customers_buying_coffee = 0;

/*
semaphores we will use in the design of this program
*/
sem_t registerPillow, cash_register, seat_belt;

/*
The initial values of the customers will be kept in this pointer
 and the customer customer_array which is in the order of production will be taken.
*/
customer* allCustomers;

//The function to assign initial values to customers is reported to the compiler
void createAllCustomers();

//The function required to generate random numbers in the given range is notified to the compiler.
int generateRandomNumber(int min ,int max);

//Function that customer threads will run
void* customer_(void* customerAddress);

//variable we use in flag logic to check that all customers are complete
int allDone = 0;

int main() {
    
    srand(time(0));//Used to make random number generation unique
    sem_init(&registerPillow, 0, 0); // semaphores are initing
    sem_init(&cash_register, 0, 1); // first parameter is address of semaphore
    sem_init(&seat_belt, 0, 0); // second parameter is about shared memory (process or thread), third parameter is init value of semaphore

    
    int loopVariable; // variable used to loop
    int id_array[REGISTER_NUMBER]; // id will be sent to registers from this array
    
    pthread_t registers[REGISTER_NUMBER]; //We have set up the registers as threads and we keep the threads in this array.

    pthread_t thread_array_of_customers[CUSTOMER_NUMBER];//Also, Customers have threads and are kept in this array.

    /*
    allCustomers is a pointer of type customer that holds the initial values of customers
    Since allCustomer is a pointer, memory allocation is made according to customer number via malloc.
    */
    allCustomers = (customer*)malloc(sizeof(customer) * CUSTOMER_NUMBER);

    //Initial values are given by creating customer objects up to the customer number
    createAllCustomers();
    
    //Generating ids for registers
    for(loopVariable = 0;loopVariable < REGISTER_NUMBER; loopVariable++){
        //generated ids are stored in id_array
        id_array[loopVariable] = loopVariable + 1;
    }

    //creating IDs array for registers
    for(loopVariable = 0;loopVariable < REGISTER_NUMBER; loopVariable++){
        /*
        First of all, register threads must be created
         because registers must be in standby at first and the incoming customer chooses one of the existing registers and gets coffee.
        */
        pthread_create(&registers[loopVariable], NULL, register_, (void*)&id_array[loopVariable]);
    }

    //creating Threads for customers
    for(loopVariable = 0;loopVariable < CUSTOMER_NUMBER; loopVariable++){
        //Waiting time when customers are created is implemented here
        //here wait is provided from allCustomers pointer
        sleep((*(allCustomers + loopVariable)).customer_arrival_time);
        /*
        here customer threads are created
        first parameter -> Is the location where the ID of the newly created thread should be stored, or NULL if the thread ID is not required.
        second parameter -> Is the thread attribute object specifying the attributes for the thread that is being created. If attr is NULL, the thread is created with default attributes. 
        third parameter -> Is the main function for the thread; the thread begins executing user code at this address. 
        fourth parameter -> Is the argument passed to start.  
        */
        pthread_create(&thread_array_of_customers[loopVariable], NULL, customer_, (void*)(allCustomers + loopVariable));
    }

    //join operations for customers
    /*
    first_parameter -> The target thread whose termination you're waiting for. 
    second_parameter -> NULL, or a pointer to a location where the function can store the value passed to pthread_exit() by the target thread.
    */
    for(loopVariable = 0;loopVariable < CUSTOMER_NUMBER; loopVariable++){
        pthread_join(thread_array_of_customers[loopVariable], NULL);
    }
    
    //Now that all customer threads are closed, we can set the variable allDone to one.
    allDone = 1;
    

    //kill the register threads
    /*
    In the last step, we wake up the dormant registers and
    send them to the condition of the while loop,
    this will exit the while loop and close the register threads.
    */
    //loop returns the total number of registers
    for(loopVariable = 0;loopVariable < REGISTER_NUMBER; loopVariable++){
        sem_post(&registerPillow);//Sending signal to wake register threads from sleep mode
    }
    
    //join operation should be done for the last registers because register threads can be closed after customer threads are finished
    //loop returns the total number of registers
    for(loopVariable = 0;loopVariable < REGISTER_NUMBER; loopVariable++){
        pthread_join(registers[loopVariable], NULL);//Joining is applied to each register
    }

    //exit code zero program terminated without error
    return 0;
}

void * register_(void* id) {

    //The unique value of each register id is taken into a variable called register_id for each register
    int register_id = *((int*)id);

    //how many seconds will customers get coffee used to store it and put the program to sleep at the right moment
    int seconds;

    //It is used to temporarily store the customers coming to this register.
    customer temp_customer;

    //The loop is basically designed to run until all customers get coffee.
    while (!allDone)
    {
        /*
        each register is in sleep state at first
        The incoming customer wakes up any of the registers and begins to wait for coffee.
        */
        sem_wait(&registerPillow);

        /*
        The reason we use the if else block is to ensure that if one of the registers has received the last customer,
        the registers will not wait for new customers in vain.
        */
        if(!allDone){
            /*
            register receives customer and sends seat_belt signal so that other registers can receive customer
            The purpose of this structure is not to check
            if a new customer has arrived in parallel with all registers 
            because this causes a customer to go to two or more registers at the same time.
            */
            sem_post(&seat_belt);

            //registers take the next customer in the customer array
            temp_customer = customer_array[total_number_of_customers_buying_coffee];
        
            //Increasing number of customers buying coffee
            total_number_of_customers_buying_coffee++;

            //Which register customer went to is printed on the screen
            printing_customer_id_goes_to_register_id(temp_customer.customer_id, register_id);

            //Here, the program determines how many seconds will the customer receive coffee from the register.
            seconds =  generateRandomNumber(COFFEE_TIME_MIN, CUSTOMER_ARRIVAL_TIME_MAX);

            /*In order to use it later,
            we printed which register the customer went to in the customer_array because the initial value of this variable is - 1
            */
            temp_customer.register_id = register_id;

            //sleeps the program for the given seconds at this stage
            sleep(seconds); 

            //Which customer bought coffee from which register in how many seconds, it is printed at this stage
            printing_customer_id_finished_buying_from_register_id_after_time_seconds(temp_customer.customer_id, register_id, seconds);
        }
        else {
            // if all Customer is done, loop is broken
            break;
        }

    }
    
    pthread_exit(NULL); // exiting the thread

}

void* customer_(void* customerAddress) {
    //Generating existing base variables for a customer
    int customer_id; // unique id
    int customer_arrival_time; // how many seconds after the previous customer from the second type
    int register_id; // inital value is -1 it means it hasn't gone to a register yet

    /*
    Customer temp, whose initial properties are specific, is taken to the Customer variable,
    where the value that comes as a thread parameter is the address of the originally generated customer.
    */
    customer tempCustomerSelf = *((customer*)customerAddress);

    //At this stage, values from customer are copied to tempCustomer
    customer_id = tempCustomerSelf.customer_id;//copying customer_id
    customer_arrival_time = tempCustomerSelf.customer_arrival_time;//copying customer_arrival_time
    register_id = tempCustomerSelf.register_id; //copying register_id (initial value is '-1')

    //Prints how many seconds after the customer came from the previous customer
    printing_customer_id_is_created_after_time_seconds(customer_id, customer_arrival_time);

    /*
    The customer whose time of occurrence is placed in the array
    here we implemented a solution similar to producer consumer problem
    so the generated customers are kept in this array like a buffer
    The allCustomers pointer is only used to assign initial values to customers.
    The array, on the other hand, presents the generated customer to registers, where it works concurrently with the customer threads.
    */
    customer_array[total_number_of_customers_created] = tempCustomerSelf;

    //If there is an accumulation in the cases, the customer is kept in this semaphore.
    sem_wait(&cash_register);

    //here the customer wakes up the idle cash register
    sem_post(&registerPillow);
    
    //The customer who wakes up the cash register is locked in here until he gets his coffee.
    sem_wait(&seat_belt);

    //Sending signal to cash register semaphore
    //this ensures that a thread waiting for exactly this semaphore starts running
    sem_post(&cash_register);

    //total number of created customers is hold this variable and increases with every step
    total_number_of_customers_created++;

    pthread_exit(NULL); // exiting the thread
}

//This function has three purposes.
void createAllCustomers() {
    /*
    First, it creates all customers with their arrival times,
    then determines their id's, and 
    finally sets their register_ids to zero, which means that there is no register that customer went to.
    */
    // int i variable used to loop
    int i;
    customer c;
    for(i = 0;i < CUSTOMER_NUMBER; i++) {
        c.customer_id = i + 1; // assigning id to customer
        // assigning customer_arrival_time to customer
        c.customer_arrival_time = generateRandomNumber(CUSTOMER_ARRIVAL_TIME_MIN, CUSTOMER_ARRIVAL_TIME_MAX);
        c.register_id = - 1;//assigning register_id to customer which means that there is no register that customer went to.
        *(allCustomers + i) = c; // placed in the generated customer allCustomer pointer
    }
}

//this function generates a random number including the borders between the given numbers
int generateRandomNumber(int min, int max) {
    return (rand() % (max - min + 1)) + min;
}

//this function only prints the arrival times of customers
void printing_customer_id_is_created_after_time_seconds(int customer_id, int seconds) {
    printf("CUSTOMER %d IS CREATED AFTER %d SECONDS.\n", customer_id, seconds);
}

//This function only prints which register the customers go to.
void printing_customer_id_goes_to_register_id (int customer_id, int register_id) {
    printf("CUSTOMER %d GOES TO REGISTER %d.\n", customer_id, register_id);    
}

//This function only prints out from which register in how many minutes the customers bought coffee.
void printing_customer_id_finished_buying_from_register_id_after_time_seconds(int customer_id, int register_id, int seconds) {
    printf("CUSTOMER %d FINISHED BUYING FROM REGISTER %d AFTER %d SECONDS.\n", customer_id, register_id, seconds);
}