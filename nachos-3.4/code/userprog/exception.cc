// exception.cc 
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.  
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "syscall.h"

//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2. 
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions 
//	are in machine.h.
//----------------------------------------------------------------------
void myFork(int);
void myYield();
SpaceId myExec(char *);
int myJoin(int);
void myExit(int);
void myCreate(char *);
OpenFileId myOpen(char *);
int myRead(int, int, OpenFileId);
void myWrite(int, int, OpenFileId);
void myClose(OpenFileId);


void incrRegs(){
	int pc = machine->ReadRegister(PCReg);
	machine->WriteRegister(PrevPCReg, pc);
	pc = machine->ReadRegister(NextPCReg);
	machine->WriteRegister(PCReg, pc);
	machine->WriteRegister(NextPCReg, pc + 4);
}

void
ExceptionHandler(ExceptionType which)
{
    int type = machine->ReadRegister(2);
	char* fileName = new char[128];
	int pid;
	
    if(which == SyscallException)
    {
        switch(type)
        {
	    	case SC_Halt:
           	{
	       		DEBUG('a', "Shutdown, initiated by user program.\n");
	       		interrupt->Halt();
               	break;
            }
            case SC_Fork:
            {
               myFork(machine->ReadRegister(4));
               //machine->WriteRegister(2, result);
               break;
            }
			case SC_Yield:
			{
				myYield();
				break;
			}
			case SC_Exec:
			{
				//printf("in myexec\n");
				int position = 0;
			    int arg = machine->ReadRegister(4);
			    int value;
			    while (value != NULL) {
			    	machine->ReadMem(arg, 1, &value);
			        fileName[position] = (char) value;
			        position++;
			        arg++;
			    }
				
				pid = myExec(fileName);
				//printf("pid returned from myExec: %d\n", pid);
				machine->WriteRegister(2, pid);
				break;
			}
			case SC_Join:
			{
				int arg = machine->ReadRegister(4);
				pid = myJoin(arg);
				machine->WriteRegister(2, pid);
				break;
			}	
			case SC_Exit:
			{
				int arg = machine->ReadRegister(4);
				myExit(arg);
				break;
			}
			case SC_Create:
			{
				int position = 0;
				int arg = machine->ReadRegister(4);
				int value;
				while(value != NULL){
					machine->ReadMem(arg, 1, &value);
					fileName[position] = (char) value;
					position++;
					arg++;
				}
				
				myCreate(fileName);
				break;
			}
			case SC_Open:
			{
				int position = 0;
				int arg = machine->ReadRegister(4);
				int value;
				while(value != NULL){
					machine->ReadMem(arg, 1, &value);
					fileName[position] = (char) value;
					position++;
					arg++;
				}
				int index = myOpen(fileName);
				machine->WriteRegister(2, index);				
				break;
			}
			case SC_Read:
			{
				int arg1 = machine->ReadRegister(4);
				int arg2 = machine->ReadRegister(5);
				int arg3 = machine->ReadRegister(6);
				int tmp = myRead(arg1, arg2, arg3);
				machine->WriteRegister(2, tmp);
				break;
			}
			case SC_Write:
			{
				int arg1 = machine->ReadRegister(4);
				int arg2 = machine->ReadRegister(5);
				int arg3 = machine->ReadRegister(6);
				myWrite(arg1, arg2, arg3);			
				break;
			}
			case SC_Close:
			{
				int arg1 = machine->ReadRegister(4);
				myClose(arg1);
				break;
			}
		}	
		incrRegs();		
    } else {
	printf("Unexpected user mode exception %d %d\n", which, type);
	ASSERT(FALSE);
    }
}

/////////////////////////////////
// Dummy function used by myFork
/////////////////////////////////
void myForkHelper(int funcAddr) {
	currentThread->space->RestoreState(); // load page table register
	machine->WriteRegister(PCReg, funcAddr);
	machine->WriteRegister(NextPCReg, funcAddr + 4);
	machine->Run(); // jump to the user progam
	ASSERT(FALSE); // machine->Run never returns;	
}

/////////////////////////////////
// Fork system call
/////////////////////////////////
void myFork(int funcAddr){
	printf("System Call: %d invoked Fork\n", currentThread->space->pcb->pid);
	
	AddrSpace *space = currentThread->space->Duplicate();
	if(space==NULL)
		return;
	
	PCB *pcb = new PCB();
	Thread* thread = new Thread("new forked thread.");
	
	pcb->thread = thread;
	pcb->pid = procManager->getPID();
	ASSERT(pcb->pid!=-1);
	pcb->parentPid = currentThread->space->pcb->pid; 
	space->pcb = pcb;
	thread->space = space;
	procManager->insertProcess(pcb, pcb->pid);
	space->SaveState();
	thread->Fork(myForkHelper, funcAddr);
	currentThread->Yield();	
		
}

// Yield system call
void myYield(){
	printf("System Call: %d invoked Yield\n", currentThread->space->pcb->pid);
	currentThread->Yield();
}

/////////////////////////////////
// Helper func to create new process in register.
/////////////////////////////////
void newProc(int arg){
	/*printf("***exec stuf\n");
	printf("currentThread: %p\n", currentThread);
	printf("currentThread->space: %p\n", currentThread->space);
	printf("currentThread->space->pcb: %p\n", currentThread->space->pcb);
	printf("currentThread->space->pcb->pid: %i\n", currentThread->space->pcb->pid);*/
	currentThread->space->InitRegisters(); 
	currentThread->space->SaveState();
	currentThread->space->RestoreState(); 
	//printf("before run\n");
	machine->Run();	
}

/////////////////////////////////
// Exec system call
/////////////////////////////////
SpaceId myExec(char *file){
	printf("System Call: %d invoked Exec\n", currentThread->space->pcb->pid);
	printf("Exec Program: %d loading %s\n", currentThread->space->pcb->pid, file);
	
	int spaceID;
	OpenFile *executable = fileSystem->Open(file);
	
	if(executable == NULL){
		printf("Unable to open file %s\n", file);
		return -1;
	}
	AddrSpace *space;
	space = new AddrSpace(executable);	
	
	PCB* pcb = new PCB();
	pcb->pid = procManager->getPID();		
	Thread *t = new Thread("Forked process");
	pcb->pid = procManager->getPID();
	spaceID = pcb->pid;	
	ASSERT(pcb->pid!=-1);
	
	/*
	printf("currentThread: %p\n", currentThread);
	printf("currentThread->space: %p\n", currentThread->space);
	printf("currentThread->space->pcb: %p\n", currentThread->space->pcb);
	printf("currentThread->space->pcb->pid: %i\n", currentThread->space->pcb->pid);
	*/
	pcb->parentPid = currentThread->space->pcb->pid;	
	pcb->thread = t;
	space->pcb = pcb;	
	t->space = space;	
	procManager->insertProcess(pcb, pcb->pid);
	delete executable;
	t->Fork(newProc, NULL);	
	currentThread->Yield();
	return spaceID;	
}

/////////////////////////////////
// Join system call
/////////////////////////////////
int myJoin(int arg){
	printf("System Call: %d invoked Join\n", currentThread->space->pcb->pid);
	
	currentThread->space->pcb->status = BLOCKED;
	
	if(procManager->getStatus(arg) < 0)
		return procManager->getStatus(arg);
		
	procManager->join(arg);
	currentThread->space->pcb->status = RUNNING;
	return procManager->getStatus(arg);	
}

/////////////////////////////////
// Exit system call
/////////////////////////////////
void myExit(int status){
	printf("System Call: %d invoked Exit\n", currentThread->space->pcb->pid);
	int pid = currentThread->space->pcb->pid;
	procManager->Broadcast(pid);
	delete currentThread->space;
	currentThread->space = NULL;
	procManager->clearPID(pid);
	currentThread->Finish();
}

/////////////////////////////////
// Create system call
/////////////////////////////////
void myCreate(char *fileName){
	printf("System Call: %d invoked Create\n", currentThread->space->pcb->pid);
	bool fileCreationWorked = fileSystem->Create(fileName, 0);
	ASSERT(fileCreationWorked);
}

/////////////////////////////////
// Open system call
/////////////////////////////////
OpenFileId myOpen(char *name){
	printf("System Call: %d invoked Open\n", currentThread->space->pcb->pid);
	
	int index = 0;
	
	
	SysOpenFile *sFile= NULL;
	
	for(int i = 0; i < MAX_FILES; i++){
		if(openFilesArray[i] != NULL && strcmp(openFilesArray[i]->fileName, name)==0){
			sFile = openFilesArray[i];
			index = i;
		}
	}
	
	if (sFile == NULL) {
		OpenFile *oFile = fileSystem->Open(name);
		if (oFile == NULL) {
			return -1;
		}
		SysOpenFile *sysFile;
		sysFile->file = oFile;
		sysFile->numUsersAccess = 1;
		strcpy(sysFile->fileName, name);
		for(int i = 0; i < MAX_FILES; i++){
			if(openFilesArray[i] == NULL){
				openFilesArray[i] = sysFile;
				index = i;
				break;
			}
		}
		
		//index = fileManager->Add(sysFile);
	}
	else{
		sFile->numUsersAccess++;
	}
	UserOpenFile *uFile;
	uFile->indexPosition = index;
	uFile->offset = 0;
	OpenFileId oFileId = currentThread->space->pcb->Add(uFile);
	return oFileId;	
}

/////////////////////////////////
// Read system call
/////////////////////////////////
int myRead(int bufferAddress, int size, OpenFileId id){
	printf("System Call: %d invoked Read\n", currentThread->space->pcb->pid);
	char *buffer = new char[size + 1];
	int sizeCopy = size;
	
	if (id == ConsoleInput) {
		int count = 0;

		while (count < size) {
			buffer[count]=getchar();
			count++;
		}
	} 
	else {
		UserOpenFile* userFile =  currentThread->space->pcb->getFile(id);
		if(userFile == NULL)
			return 0;
			
		//SysOpenFile *sysFile = fileManager->getFile(userFile->fileIndex);	
		SysOpenFile *sFile= NULL;		
		//new fileManager = openFilesArray
		if(openFilesArray[userFile->indexPosition] != NULL)
			sFile = openFilesArray[userFile->indexPosition];
			
		sizeCopy = sFile->file->ReadAt(buffer,size,userFile->offset);
		userFile->offset+=sizeCopy;
	}
	//---------Need a read write func----------
	ReadWrite(bufferAddr,buffer,sizeCopy,USER_READ);
	//-----------------------------------------
	delete[] buffer;
	return sizeCopy;	
}

/////////////////////////////////
// Write system call
/////////////////////////////////
void myWrite(int bufferAddress, int size, OpenFileId id){
	printf("System Call: %d invoked Write\n", currentThread->space->pcb->pid);
	char* buffer = new char[size+1];
	
	//-----------Need this func------------------
	ReadWrite(bufferAddr,buffer,size,USER_WRITE);
	//-------------------------------------------
	
	
	if (id == ConsoleOutput) {
		
		buffer[size]=0;
		printf("%s",buffer);
		
	} 
	else {
		buffer = new char[size];
		int writeSize = ReadWrite(bufferAddr,buffer,size,USER_WRITE);
		UserOpenFile* uFile =  currentThread->space->pcb->getFile(id);
		if(uFile == NULL)
			return ;
			
		SysOpenFile *sFile = NULL;
		if(openFilesArray[userFile->indexPosition] != NULL)
			sFile = openFilesArray[userFile->indexPosition];
			
		int bytes = sFile->file->WriteAt(buffer,size,userFile->offset);
		userFile->offset+=bytes;
	}
	delete[] buffer;	
}


/////////////////////////////////
// Close system call
/////////////////////////////////
void myClose(OpenFileId id){
	printf("System Call: %d invoked Close\n", currentThread->space->pcb->pid);
	UserOpenFile* userFile =  currentThread->space->pcb->Get(id);
	if(userFile == NULL){
		return ;
	}
	SysOpenFile *sFile = NULL;	
	if(openFilesArray[userFile->indexPosition] != NULL)
		sFile = openFilesArray[userFile->indexPosition];
	
	sFile->closeOne();
	currentThread->space->pcb->Remove(id);
}

