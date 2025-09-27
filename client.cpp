/*
	Original author of the starter code
    Tanzir Ahmed
    Department of Computer Science & Engineering
    Texas A&M University
    Date: 2/8/20
	
	Please include your Name, UIN, and the date below
	Name: Marisol Moreno
	UIN: 134003860
	Date:9-18-25
*/
#include "common.h"
#include "FIFORequestChannel.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <cstdio>
#include <sys/wait.h>


using namespace std;


int main (int argc, char *argv[]) {
	int opt;
	int p = -1;
	double t = -1;
	int e = -1;
	//add buffer capacity as a variable that is given in the command line (default is MAX_MESSAGE)
	int64_t m = MAX_MESSAGE;
	string filename = "";
	bool new_chan = false;
	vector<FIFORequestChannel*> channels_vector;

	while ((opt = getopt(argc, argv, "p:t:e:f:m:c")) != -1) {
		switch (opt) {
			case 'p':
				p = atoi (optarg);
				break;
			case 't':
				t = atof (optarg);
				break;
			case 'e':
				e = atoi (optarg);
				break;
			case 'f':
				filename = optarg;
				break;
			case 'm':
				m = atoi (optarg);
				break;
			case 'c':
				new_chan = true;
				break;
		}
	}

	//Task 1: create server, with correct format as connected
	//give arguments to the server
	//server needs: './server', '-m', '<val for -m arg>', 'NULL' (last arg is null to lets execvp know that we are done giving arguments)
	pid_t pid = fork();
	if (pid < 0) {
		EXITONERROR("Cannot create a child process");
	}
	else if (pid == 0) {
		//child process
		
		//create arguments for execvp
		char *args[5];
		args[0] = (char *)"./server";
		args[1] = (char *)"-m";
		//convert int m to string
		string m_str = to_string(m);
		args[2] = (char *)m_str.c_str();
		args[3] = NULL; //null to indicate end of arguments	
		//execute server
		if (execvp(args[0], args) < 0) {	
			EXITONERROR("Cannot run server");
		} 

	} else {
		//parent process

		FIFORequestChannel con_chan("control", FIFORequestChannel::CLIENT_SIDE);
		channels_vector.push_back(&con_chan);

		//if c flag is used then we want to create a new channel after the control channel has been created
		if(new_chan){
			cout << "New channel REQUESTED" << endl;
			//send new channel request to server
			MESSAGE_TYPE new_chan_msg = NEWCHANNEL_MSG;
			cout << "sending reuest for new channel to server" << endl;
			con_chan.cwrite(&new_chan_msg, sizeof(MESSAGE_TYPE));

			// receive new channel name from the server into a real buffer
			char new_chan_buf[MAX_MESSAGE]; // server sends a short name like "dataN_"
			con_chan.cread(new_chan_buf, sizeof(new_chan_buf));
			cout << "receive new channel name from server: " << new_chan_buf << endl;
			

			//call FIFORequestChannel constructor
			//dynamically create channel because we are inside if statement, need to delete new channel if created
			cout << "CREATING new channel" << endl;
			FIFORequestChannel*  dataChannel = new FIFORequestChannel (new_chan_buf, FIFORequestChannel::CLIENT_SIDE);
			channels_vector.push_back(dataChannel);
			cout << "New channel created succesfully" << endl;
		}
		FIFORequestChannel chan = *(channels_vector.back());

		//make connection first and obtain data_channel if needed (based on flag above)
		//single datapoint, only run when p,t,e != -1
			if (p != -1 && t != -1 && e != -1) {
			// example data point request
			char buf[MAX_MESSAGE]; // 256

			//change to add variable values instead of hardcoding
			datamsg x(p, t, e);
			
			//copy the request into buf
			memcpy(buf, &x, sizeof(datamsg));

			//write buf to the control channel
			chan.cwrite(buf, sizeof(datamsg)); // question

			//create variable to store the value the server gives back
			double reply;

			//read the value from the channel
			chan.cread(&reply, sizeof(double)); //answer

			//output to terminal
			cout << "For person " << p << ", at time " << t << ", the value of ecg " << e << " is " << reply << endl;
		} else if (p != -1) {
			//else if p != -1 then request first 1000 data points
			cout << "Entering first 1000 data points for person " << p << endl;
			//loop over first 1000 data points
			double time = 0;
			// open output file once (append) so we don't truncate every loop iteration
			ofstream file;
			file.open("received/x1.csv");
			while (time <= 4){
				double e1;
				double e2;

				//send request for ecg1 
				char bufe1[MAX_MESSAGE]; 
				datamsg x1(p, time, 1);
				memcpy(bufe1, &x1, sizeof(datamsg));
				chan.cwrite(bufe1, sizeof(datamsg));
				chan.cread(&e1, sizeof(double));

				//send request for ecg2
				char bufe2[MAX_MESSAGE];
				datamsg x2(p, time, 2);
				memcpy(bufe2, &x2, sizeof(datamsg));
				chan.cwrite(bufe2, sizeof(datamsg));
				chan.cread(&e2, sizeof(double));
				
				//write line to received/x1.csv
				file << time << "," << e1 << ","  << e2 << endl;

				//increase time variable
				time += 0.004;
			}
			file.close();
			cout << "first 1000 data points are now in x1.csv" << endl;
		}

		//Task 3 send filemsg request
		if(filename != ""){
			cout << "entering filemsg request" << endl;
			//query server to get file length
			filemsg fm(0, 0);
			int lenOfRequest = sizeof(filemsg) + (filename.size() + 1);

			//initialize request buffer
			char* requestBuf = new char[lenOfRequest];
			//copy filemsg into the buffer
			memcpy(requestBuf, &fm, sizeof(filemsg));
			//copy filename into the buffer
			strcpy(requestBuf + sizeof(filemsg), filename.c_str());
			//send buffer to server
			chan.cwrite(requestBuf, lenOfRequest);  // I want the file length;
			
			//will return the file length
			//create a variable to store the file length
			int64_t totalFileLength = 0;
			//read response from server
			chan.cread(&totalFileLength, sizeof(int64_t));
			cout << "File length is: " << totalFileLength << " bytes" << endl;

			//response buffer will have a different length (the max capacity of the message)
			char* responseBuff = new char[m];

			//do a while loop to keep track of how much data weve read, parse data 
			//loop over the segments in the file -> filesize / buffer capacity
			int64_t bytesReadSoFar = 0;
			ofstream ofile;
			ofile.open("received/" + filename, ios::app);

			while (bytesReadSoFar < totalFileLength) {
				//create filemsg instance
				int msgLength = min(m, (totalFileLength - bytesReadSoFar));
				filemsg fm2(bytesReadSoFar, msgLength);

				//resue request buffer
				filemsg* fileRequest = (filemsg*) requestBuf;
				fileRequest->offset = bytesReadSoFar;
				fileRequest->length = msgLength;

				memcpy(responseBuff, &fm2, sizeof(filemsg));

				//send the request
				chan.cwrite(requestBuf, lenOfRequest);

				//receive the response
				chan.cread(responseBuff, fileRequest->length);

				//write response buffer to file: received/filename
				ofile.write(responseBuff, fileRequest->length);

				bytesReadSoFar += m;
			}
			ofile.close();
			delete[] requestBuf;
			delete [] responseBuff;
		}

		//close connection to DataChannel if needed 
		if(new_chan){
			MESSAGE_TYPE quit = QUIT_MSG;
			FIFORequestChannel* new_channel = channels_vector.back();
			(*new_channel).cwrite(&quit, sizeof(MESSAGE_TYPE));
			delete new_channel;
		}
		
		// closing the control channel  
		MESSAGE_TYPE m = QUIT_MSG;
		con_chan.cwrite(&m, sizeof(MESSAGE_TYPE));
	}
	int status;
	waitpid(pid, &status, 0);
}
