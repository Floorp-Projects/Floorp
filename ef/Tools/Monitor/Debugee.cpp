/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
// Debugee.cpp
//
// Scott M. Silver

#include <Windows.h>
#include "Debugee.h"
#include <assert.h>
#include <stdio.h>
#include "Win32Util.h"
#include "DataOutput.h"
#include "Breakpoints.h"
#include "DebuggerChannel.h"
#include "ImageHlp.h"
#include "prthread.h"

extern DebugeeThread*	gCurThread;
extern DebugeeProcess*	gProcess;

void startupDebugee(LPTSTR lpszFileName, LPTSTR lpszTitle, PROCESS_INFORMATION* outProcessInfo);

DebugeeProcess::
DebugeeProcess(DEBUG_EVENT*	inNewProcessEvent) :
	mCreateProcessInfo(inNewProcessEvent->u.CreateProcessInfo),
	mProcessH(inNewProcessEvent->u.CreateProcessInfo.hProcess),
	mDebuggerChannel(0)
	
{
	assert(inNewProcessEvent->dwDebugEventCode == CREATE_PROCESS_DEBUG_EVENT);

	BOOL success = ::SymInitialize(mProcessH, NULL, FALSE);
	assert(success);

	addThread(new DebugeeThread(mCreateProcessInfo.hThread, inNewProcessEvent->dwThreadId, *this, false));
	printf("CREATE_PROCESS_DEBUG_EVENT\n");
}


void DebugeeProcess::
kill()
{
	::TerminateProcess(mProcessH, 0); 
	setDebuggerThreadID(0); // since we are not reloading libDebuggerChannel, we need to
							// reset the debugger thread ID to zero, so we don't mistakenly
							// suspend the wrong thread
}


void DebugeeProcess::
handleModuleLoad(HANDLE inFileH, void* inBaseOfImage)
{
	if (!::SymLoadModule(mProcessH, inFileH, NULL, NULL, (DWORD) inBaseOfImage, 0)) 
		showLastError();
}


DebugeeThread* DebugeeProcess::
idToThread(DWORD inThreadID)
{
	DebugeeThread**	curThread;

	for (curThread = mThreads.begin(); curThread < mThreads.end(); curThread++)
		if ((*curThread)->getThreadID() == inThreadID)
			return (*curThread);

	return (NULL);
}


// subsequent calls will fail if the first
// time we could not connect to the ef process
DebuggerClientChannel* DebugeeProcess::
getChannel(bool inForce)
{
	if (!mDebuggerChannel || (inForce && mDebuggerChannel == (DebuggerClientChannel*) this))
	{
		mDebuggerChannel = DebuggerClientChannel::createClient();
		
		if (mDebuggerChannel)
			return (mDebuggerChannel);
		else
		{
			mDebuggerChannel = (DebuggerClientChannel*) this;
			return (NULL);
		}
	}
	else if (mDebuggerChannel == (DebuggerClientChannel*) this)
		return (NULL);
	else
		return (mDebuggerChannel);

}

BOOL DebugeeProcess::
writeMemory(void* inDest, void* inSrc, DWORD inSrcLen, DWORD* outBytesWritten)
{
	return (::WriteProcessMemory(mProcessH, inDest, inSrc, inSrcLen, outBytesWritten));
}


BOOL DebugeeProcess::
readMemory(const void* inSrc, void* inDest, DWORD inDestLen, DWORD* outBytesRead)
{
	return (::ReadProcessMemory(mProcessH, inSrc, inDest, inDestLen, outBytesRead));
}


// this only returns the the debugee process starts up and is ready
// for use.  the debugee process is suspended, etc
DebugeeProcess*	DebugeeProcess::
createDebugeeProcess(const char* inFullPath, DWORD inDebugEventHandlerID, HANDLE& outDebugThreadH)
{
	DebugeeProcess::DebugStartupInfo	startupInfo;

	startupInfo.fullPath = inFullPath;
	startupInfo.debugeeProcessCreated = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	startupInfo.debugEventHandlerID = inDebugEventHandlerID;
	startupInfo.newDebugeeProcess = NULL;

	PR_CreateThread(PR_USER_THREAD,
							 &debugEventThread,
							 &startupInfo,
							 PR_PRIORITY_NORMAL,
							 PR_GLOBAL_THREAD,
							 PR_JOINABLE_THREAD,
							 0);

	// now wait until the process actually starts up
	::WaitForSingleObject(startupInfo.debugeeProcessCreated, INFINITE);

	// need to get to first instruction
	startupInfo.newDebugeeProcess->getMainThread()->singleStep();

	return (startupInfo.newDebugeeProcess);
}



void
startupDebugee(LPTSTR lpszFileName, LPTSTR lpszTitle, PROCESS_INFORMATION* outProcessInfo)
{
	// why does this retard not just assign to the structs, instead
	// of having pointers?? 
	STARTUPINFO           StartupInfo;
	LPSTARTUPINFO         lpStartupInfo = &StartupInfo;
	char* args = "-debug -html -sys -classpath \"\\trees\\ef1\\ns\\dist\\classes\\classes.zip:\\trees\\ef1\\ns\\dist\\classes\\tests.zip:\\trees\\ef1\\ns\\dist\\classes\\t1.zip\" javasoft/sqe/tests/api/java/lang/System/SystemTests10";
	char*				commandLine = new char[strlen(lpszFileName) + strlen(args) + 2];

	sprintf(commandLine, "%s %s", lpszFileName, args); 

	lpStartupInfo->cb          = sizeof(STARTUPINFO);
	lpStartupInfo->lpDesktop   = NULL;
	lpStartupInfo->lpTitle     = lpszTitle;
	lpStartupInfo->dwX         = 0;
	lpStartupInfo->dwY         = 0;
	lpStartupInfo->dwXSize     = 0;
	lpStartupInfo->dwYSize     = 0;
	lpStartupInfo->dwFlags     = (DWORD) NULL;
	lpStartupInfo->wShowWindow = SW_SHOWDEFAULT;

	outProcessInfo->hProcess = NULL;

	// create the Debuggee process instead
	if( !::CreateProcess(
		 NULL,
		 commandLine, //lpszFileName,
		 (LPSECURITY_ATTRIBUTES) NULL,
		 (LPSECURITY_ATTRIBUTES) NULL,
		 TRUE,
		 DEBUG_PROCESS | DEBUG_ONLY_THIS_PROCESS | CREATE_NEW_CONSOLE | CREATE_NEW_PROCESS_GROUP,
		 (LPVOID) NULL,
		 (LPTSTR) NULL,
		 lpStartupInfo, outProcessInfo)) 
	{
		showLastError();
		exit(-1);
	}

	delete commandLine;
}

void DebugeeProcess::
debugEventThread(void* inStartupInfo)
{
	bool								fFinished = false;
	DEBUG_EVENT							debugEvent;
	PROCESS_INFORMATION					processInformation;
	DebugeeProcess::DebugStartupInfo*	startupInfo = (DebugeeProcess::DebugStartupInfo*) inStartupInfo;
	DebugeeProcess*						thisProcess = NULL;
	DebugeeThread*						thread;
	bool								didSuspend;

	// start debugee
	startupDebugee((char*) startupInfo->fullPath, (char*) startupInfo->fullPath, &processInformation);

	// debug event processing loop
	for(;;) 
	{	
		didSuspend = false;

		// wait for debug events
		if(!WaitForDebugEvent(&debugEvent, INFINITE)) 
		{
			showLastError();
			fFinished = true;
			break;
		}

		// our strategy is to suspend all (relevant)
		// threads, continue the debug event -- so all threads
		// continue so we can continue grabbing symbols, etc
		if (thisProcess)
		{
			thisProcess->suspendAll();
			didSuspend = true;
			thread = thisProcess->idToThread(debugEvent.dwThreadId); // can be null if CREATE_THREAD_EVENT

			if (!::ContinueDebugEvent(debugEvent.dwProcessId, debugEvent.dwThreadId, DBG_CONTINUE))
				showLastError();
		}

		beginOutput();
 
		switch(debugEvent.dwDebugEventCode) 
		{
			// exception occured
			case EXCEPTION_DEBUG_EVENT:
				// figure out which type of exception
				switch(debugEvent.u.Exception.ExceptionRecord.ExceptionCode) 
				{
					// hardware exceptions
					case EXCEPTION_ACCESS_VIOLATION:
						thread->suspend();
						disassembleN(thread->getProcess(), (char*) thread->getIP(), 10);
						printThreadStack(*thread);
						thread->print();
						::ContinueDebugEvent(debugEvent.dwProcessId, debugEvent.dwThreadId, DBG_CONTINUE);

 						printf("EXCEPTION_ACCESS_VIOLATION\n");
						break;
					case EXCEPTION_DATATYPE_MISALIGNMENT:
 						printf("EXCEPTION_ACCESS_VIOLATION\n");
						break;
					case EXCEPTION_BREAKPOINT:
						printf("EXCEPTION_BREAKPOINT\n");
						// so we need an extra continue for breakpoints??
//						::ContinueDebugEvent(debugEvent.dwProcessId, debugEvent.dwThreadId, DBG_CONTINUE);
						
						thread->suspend();
						thisProcess->handleBreakpoint(debugEvent, thread);

						break;
					case EXCEPTION_SINGLE_STEP:
						printf("EXCEPTION_SINGLE_STEP\n");
						thread->suspend();
						thisProcess->handleSingleStep(debugEvent, thread);
						break;
					case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
						printf("EXCEPTION_ARRAY_BOUNDS_EXCEEDED\n");
						break;
					case EXCEPTION_FLT_DENORMAL_OPERAND:
						printf("EXCEPTION_FLT_DENORMAL_OPERAND\n");
						break;
					case EXCEPTION_FLT_DIVIDE_BY_ZERO:
						printf("EXCEPTION_FLT_DIVIDE_BY_ZERO\n");
						break;
					case EXCEPTION_FLT_INEXACT_RESULT:
						printf("EXCEPTION_FLT_INEXACT_RESULT\n");
						break;
					case EXCEPTION_FLT_INVALID_OPERATION:
						printf("EXCEPTION_FLT_INVALID_OPERATION\n");
						break;
					case EXCEPTION_FLT_OVERFLOW:
						printf("EXCEPTION_FLT_OVERFLOW\n");
						break;
					case EXCEPTION_FLT_STACK_CHECK:
						printf("EXCEPTION_FLT_STACK_CHECK\n");
						break;
					case EXCEPTION_FLT_UNDERFLOW:
						printf("EXCEPTION_FLT_UNDERFLOW\n");
						break;
					case EXCEPTION_INT_DIVIDE_BY_ZERO:
						printf("EXCEPTION_INT_DIVIDE_BY_ZERO\n");
						break;
					case EXCEPTION_INT_OVERFLOW:
						printf("EXCEPTION_INT_OVERFLOW\n");
						break;
					case EXCEPTION_PRIV_INSTRUCTION:
						printf("EXCEPTION_PRIV_INSTRUCTION\n");
						break;
					case EXCEPTION_IN_PAGE_ERROR:
						printf("EXCEPTION_IN_PAGE_ERROR\n");
						break;
					// Debug exceptions
					case DBG_TERMINATE_THREAD:
						printf("DBG_TERMINATE_THREAD\n");
						break;
					case DBG_TERMINATE_PROCESS:
						printf("DBG_TERMINATE_PROCESS\n");
						break;
					case DBG_CONTROL_C:
						printf("DBG_CONTROL_C\n");
						break;
					case DBG_CONTROL_BREAK:
						printf("DBG_CONTROL_BREAK\n");
						break;
					// RPC exceptions (some)
					case RPC_S_UNKNOWN_IF:
						printf("RPC_S_UNKNOWN_IF\n");
						break;
					case RPC_S_SERVER_UNAVAILABLE:
						printf("RPC_S_SERVER_UNAVAILABLE\n");
						break;
					default:
						printf("unhandled event\n");
						break;
				}

				if(1) 
				{
					;
				}
				else 
				{
					if(debugEvent.u.Exception.dwFirstChance != 0)
						;
					else
			 			;
				}
				break;
			case CREATE_THREAD_DEBUG_EVENT:
				{
					printf("CREATE_THREAD_DEBUG_EVENT\n");
					bool suspendable = (getDebuggerThreadID() != 0 && debugEvent.dwThreadId != getDebuggerThreadID()); // if we have a debugger thread ID then this thread is suspendable

					thisProcess->addThread(thread = new DebugeeThread(debugEvent.u.CreateThread.hThread, debugEvent.dwThreadId, *thisProcess, suspendable));
				}
				break; 

			case CREATE_PROCESS_DEBUG_EVENT:
				assert(startupInfo->newDebugeeProcess == NULL);	// can only get here once
				thisProcess = startupInfo->newDebugeeProcess = new DebugeeProcess(&debugEvent);
				gCurThread = thisProcess->getMainThread();
				thisProcess->handleModuleLoad(debugEvent.u.CreateProcessInfo.hFile, debugEvent.u.CreateProcessInfo.lpBaseOfImage);
				gCurThread->suspend();
				::SetEvent(startupInfo->debugeeProcessCreated);
				::ContinueDebugEvent(debugEvent.dwProcessId, debugEvent.dwThreadId, DBG_CONTINUE);
				break;

			case EXIT_THREAD_DEBUG_EVENT:
				printf("EXIT_THREAD_DEBUG_EVENT\n");
				break;

			case EXIT_PROCESS_DEBUG_EVENT:
				fFinished = true;
				printf("EXIT_PROCESS_DEBUG_EVENT\n");
				break;

			case LOAD_DLL_DEBUG_EVENT:
				char	dllName[512];
				retrieveModuleName(dllName, debugEvent.u.LoadDll.hFile);
				printf("Dll Load: %s\n", dllName);
				thisProcess->handleModuleLoad(debugEvent.u.LoadDll.hFile, debugEvent.u.LoadDll.lpBaseOfDll);
				break;

			case UNLOAD_DLL_DEBUG_EVENT:
				printf("UNLOAD_DLL_DEBUG_EVENT\n");
				break;

			case OUTPUT_DEBUG_STRING_EVENT:
				printf("OUTPUT_DEBUG_STRING_EVENT\n");
				break;

			case RIP_EVENT:
				printf("RIP_EVENT\n");
				break;

			default:
				printf("unhandled event\n");
				break;
		}

		if (didSuspend)
			thisProcess->resumeAll();

		endOutput();

		// default action, just continue
		if(fFinished) 
		  break;
	}

	gProcess = NULL;

	// decrement active process count
	::ExitThread(TRUE);

//	return(TRUE);  
}


BOOL DebugeeThread::
getContext(DWORD inContextFlags, CONTEXT& outContext)
{
	outContext.ContextFlags = inContextFlags;

	return (::GetThreadContext(mThreadH, &outContext));
}


BOOL DebugeeThread::
setContext(DWORD inContextFlags, CONTEXT& ioContext)
{
	ioContext.ContextFlags = inContextFlags;

	return (::SetThreadContext(mThreadH, &ioContext));
}


bool DebugeeProcess::
handleSingleStep(const DEBUG_EVENT& inDebugEvent, DebugeeThread* inThread)
{
	disassembleBytes(*this, (char*) inDebugEvent.u.Exception.ExceptionRecord.ExceptionAddress, 32);

	// reset out of single step mode
	CONTEXT	threadContext;
	
	// clear trap bit
	inThread->getContext(CONTEXT_CONTROL, threadContext);
	threadContext.EFlags &= ~0x100;
	inThread->setContext(CONTEXT_CONTROL, threadContext);

	inThread->handleSingleStep();

	return (true);
}


bool DebugeeProcess::
handleBreakpoint(const DEBUG_EVENT& inDebugEvent, DebugeeThread* inThread)
{
	CONTEXT	threadContext;
	
	inThread->getContext(CONTEXT_CONTROL, threadContext);
	// now set back the ip to the beginning of the debug statement
	// if we are at one of our breakpoints, the resume will handle skipping
	// over this
	if (BreakpointManager::findBreakpoint((void*) inDebugEvent.u.Exception.ExceptionRecord.ExceptionAddress))
	{
		threadContext.Eip = (DWORD) inDebugEvent.u.Exception.ExceptionRecord.ExceptionAddress;
		inThread->setContext(CONTEXT_CONTROL, threadContext);
	}

	return (true);
}


// return true if we really did single step
void DebugeeThread::
singleStep()
{
	if (!mSuspendable)
		return;

	// set into single step mode
	CONTEXT	threadContext;
	
	getContext(CONTEXT_CONTROL, threadContext);

	// set trap bit
	threadContext.EFlags |= 0x100;
	setContext(CONTEXT_CONTROL, threadContext);

	resume(true);
}


void DebugeeProcess::
suspendAll()
{
	DebugeeThread**	curThread;

	for(curThread = mThreads.begin(); curThread < mThreads.end(); curThread++)
		(*curThread)->suspend();	
}

void DebugeeProcess::
resumeAll()
{
	DebugeeThread**	curThread;

	for(curThread = mThreads.begin(); curThread < mThreads.end(); curThread++)
		(*curThread)->resume();	
}


DebugeeProcess::SymbolKind DebugeeProcess::
getSymbol(const void* inPC, char* outName, DWORD inBufLen, DWORD& outOffset)
{
	DebugeeProcess::SymbolKind kind = kNil;
	char*	symbolName;

	// will deadlock getting a symbol when the debugger thread or
	// main/io thread is suspended.
	assert(	getDebuggerThreadID() &&	
			!threadSuspendCount(idToThread(getDebuggerThreadID())->getThreadHandle()) &&
			!threadSuspendCount(getMainThread()->getThreadHandle()));

	// IMAGEHLP is silly and wants the data to go at the end of the struct
	IMAGEHLP_SYMBOL* symbol = (IMAGEHLP_SYMBOL*) malloc(sizeof(IMAGEHLP_SYMBOL) + inBufLen);
	symbol->MaxNameLength = inBufLen;
	
	if (::SymGetSymFromAddr(mProcessH, (DWORD) inPC, &outOffset, symbol))
	{
		strcpy(outName, symbol->Name);				// copy to user's buffer
		free(symbol);
		kind = kNative;
	}
	else if (getChannel())
	{
//		return (kind);
		if ((symbolName = getChannel()->requestAddressToMethod(inPC, (Int32&) outOffset)))
		{
			strncpy(outName, symbolName, inBufLen);	// copy to user's buffer
			delete [] symbolName;
			kind = kJava;
		}
	}

	return (kind);
}


void* DebugeeProcess::
getAddress(const char* inMethodName)
{
	void* address = NULL;

	if (getChannel())
		address = getChannel()->requestMethodToAddress(inMethodName);

	return (address);
}


void DebugeeThread::
handleSingleStep()
{
	if (mBp)
	{
		mBp->set();					// reset this breakpoint
		mBp = NULL;
		if (mStaySuspended)
			suspend();				// this will up our suspend count, so we won't get resumed in
									// the next statement
		mProcess.resumeAll();		// resume (put back state) of other threads
	}
}


DWORD DebugeeThread::
suspend()
{
	if (!mSuspendable)
		return 0;

	// no suspending the debugger thread or main thread
	assert(getDebuggerThreadID() != mThreadID);
	assert(mProcess.getMainThread() != this);

	return (::SuspendThread(mThreadH));
}


DWORD DebugeeThread::
resume(bool inSingleStepping)
{
	if (!mSuspendable)
		return 0;

	CONTEXT	threadContext;
	
	getContext(CONTEXT_CONTROL, threadContext);

	// if this thread would be resumed by our resuming it
	// major race condition between when we check to see if we'll be resuming
	// and the actual resume
	// if we have a breakpoint at this instruction
	// suspend all threads
	// put this thread into single step mode
	// push a pending to-do for the single step for this thread
	// (ie put back the breakpoint)
	Breakpoint* bp;
	if ((threadSuspendCount(mThreadH) == 1) && (bp = BreakpointManager::findBreakpoint((void*) threadContext.Eip)))
	{
		mProcess.suspendAll();
		::ResumeThread(mThreadH);			// we shouldn't be suspended twice
		bp->replace();
		threadContext.EFlags |= 0x100;		// single step mode
		setContext(CONTEXT_CONTROL, threadContext);
		pushSingleStepAction(bp, inSingleStepping);
	}

	return (::ResumeThread(mThreadH));	// now really resume this threac
}


void* DebugeeThread::
getIP()
{
	CONTEXT	threadContext;
	
	getContext(CONTEXT_CONTROL, threadContext);
	return ((void*) threadContext.Eip);
}


void* DebugeeThread::
getSP()
{
	CONTEXT	threadContext;
	
	getContext(CONTEXT_CONTROL, threadContext);
	return ((void*) threadContext.Esp);
}


void DebugeeThread::
print()
{
	CONTEXT	threadContext;
	
	getContext(CONTEXT_CONTROL, threadContext);
	DWORD	suspendCount = threadSuspendCount(mThreadH);
	char symbol[512];
	char*	printSymbol;


	DWORD offset;
	printSymbol = (mProcess.getSymbol((void*) threadContext.Eip, symbol, sizeof(symbol), offset)) ? symbol : "<anonymous>";

	printf("%4.4x%5.4x%10s(%5d)%9.8p %s", getThreadID(), mThreadH, (suspendCount >= 0) ? "suspended" : "running", suspendCount, threadContext.Eip, printSymbol);
	
	if (getDebuggerThreadID() == mThreadID)
		printf("[debugger]");
	else if (mProcess.getMainThread() == this)
		printf("[main-i/o]");
}
