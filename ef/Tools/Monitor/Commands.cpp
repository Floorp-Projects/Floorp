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
// Commands.cpp
//
// Scott M. Silver
//

#include <stdio.h>
#include "Commands.h"
#include "Debugee.h"
#include "DataOutput.h"
#include "Symbols.h"
#include "MonitorExpression.h"
#include "Breakpoints.h"
#include "DebuggerChannel.h"
#include "Win32Util.h"
#include "XDisAsm.h"

extern DebugeeThread*	gCurThread;
extern DebugeeProcess*	gProcess;
extern bool				gEndCommandLine;

#define CHECK_HAVE_PROCESS(inProcess)			\
		if (!inProcess)							\
		{										\
			printf("No process started\n");		\
			return;								\
		}	

void 
quit(char*)
{
	if (gProcess)
		gProcess->kill();
	
	gEndCommandLine = true;
}


void
kill(char*)
{
	CHECK_HAVE_PROCESS(gProcess);

	gProcess->kill();
	gProcess = NULL;
}

void 
stepIn(char*)
{
	CHECK_HAVE_PROCESS(gProcess);

	gCurThread->singleStep();
}


void
stepOver(char*)
{
	CHECK_HAVE_PROCESS(gProcess);

	// set thread breakpoint on instruction after call
	unsigned char	data[32];
	unsigned char*	ip;

	ip = (unsigned char*) gCurThread->getIP();
	if (!gProcess->readMemory(ip, &data, 32, NULL))
		showLastError();

	if (data[0] == 0xe8 || data[0] == 0xff || data[0] == 0x9a)
	{
		unsigned char*	tempStart;
		unsigned char*	nextInstruction;

		tempStart = data;
		nextInstruction = ip + x86InstructionLength(0, (char**)&tempStart, (char*) data + 32, kDisAsmFlag32BitSegments);
		BreakpointManager::newBreakpoint(*gCurThread, nextInstruction).set();
		gCurThread->resume();
	}
	else
		gCurThread->singleStep();
}


void
go(char *)
{
	CHECK_HAVE_PROCESS(gProcess);
	if (gCurThread != NULL)
		gCurThread->resume();
	else
		printf("no thread, use swt\n");
}


void
disassemble(char *inExpression)
{
	CHECK_HAVE_PROCESS(gProcess);
	void *address;
	static void*	sLastAddress = NULL;

	if (*inExpression != '\0')
		address = evaluateExpression(*gCurThread, inExpression);
	else if (sLastAddress)
		address = sLastAddress;
	else
		address = gCurThread->getIP();

	sLastAddress = disassembleN(*gProcess, (char*) address, 10);
}


void
unhandledCommandLine(char *inLine)
{
	CHECK_HAVE_PROCESS(gProcess);

	PrintableValue	value;

	if (lookupSymbol(*gCurThread, inLine, value))
	{
		printf("'%s' = ", inLine);
		printf(value.fmtString, value.value);
		putchar('\n');
	}
}

void* gArg;

void
breakpointCompileLoadAction(const char* inMethodName, void* inAddress, void* inArgument)
{
	DebugeeThread*	thread = (DebugeeThread*) inArgument;

	BreakpointManager::newBreakpoint(*thread, inAddress).set();
}

void
deferredCompileLoadHandler(const char* inMethodName, void* inAddress)
{
	printf("deferred notification at %p: %s\n", inAddress, inMethodName);
	if (gArg)
		breakpointCompileLoadAction(inMethodName, inAddress, gArg);
}

void 
addDeferredCompileLoadAction(const char* inMethodName, void* inArgument)
{
	DebuggerClientChannel*	channel = gProcess->getChannel();

	if (channel)
	{
		gArg = inArgument;
		channel->setCompileOrLoadMethodHandler(deferredCompileLoadHandler);
		channel->requestNotifyOnMethodCompileLoad(inMethodName);
	}
}


// expressions limited now
void
breakExecution(char *inExpression)
{	
	CHECK_HAVE_PROCESS(gProcess);

	void*	address = evaluateExpression(*gCurThread, inExpression);

	if (address)
		BreakpointManager::newBreakpoint(*gCurThread, address).set();
	else
	{
		const char*	methodName;
		if ((methodName = extractMethodFromExpression(inExpression)))
			addDeferredCompileLoadAction(methodName, gCurThread);
		else
			printf("error %s unknown\n", methodName);
	}
}


void
runClass(char* inClassName)
{
	CHECK_HAVE_PROCESS(gProcess);

	if (gProcess->getChannel())
		gProcess->getChannel()->requestRunClass("javasoft/sqe/tests/api/java/lang/System/SystemTests10");
}


void
run(char*)
{
	HANDLE	debugThreadH;
	const char*	testApp = "e:\\trees\\ef1\\ns\\electricalfire\\Driver\\StandAloneJava\\winbuild\\electric\\sajava.exe";

	if (gProcess)
		printf("already running process\n");
	else
		gProcess = DebugeeProcess::createDebugeeProcess(testApp, ::GetCurrentThreadId(), debugThreadH);	
}


void
switchThread(char *inExpression)
{
	CHECK_HAVE_PROCESS(gProcess);

	DebugeeThread* thread = gProcess->idToThread((DWORD) evaluateExpression(*gCurThread, inExpression));
	if (thread)
		gCurThread = thread;
}


extern "C" void
stack(DWORD id)
{
	DebugeeThread* thread = gProcess->idToThread(id);
	if (thread)
		printThreadStack(*thread);
}

void
printThreads(char*)
{
	CHECK_HAVE_PROCESS(gProcess);

	printThreads(*gProcess);
}


void
printThisThread(char*)
{
	CHECK_HAVE_PROCESS(gProcess);
	
	if (gCurThread)
	{
		gCurThread->print();
		printf("\n");
	}
}


void
printThreadStack(char*)
{
	CHECK_HAVE_PROCESS(gProcess);

	if (gCurThread)
		printThreadStack(*gCurThread);
}


void
connectToVM(char*)
{
	CHECK_HAVE_PROCESS(gProcess);

	if (!gProcess->getChannel(true))
		printf("couldn't connect to VM\n");
}


void printFPRegs(char*)
{
	CHECK_HAVE_PROCESS(gProcess);

	
}


void printIntegerRegs(char*)
{
	CHECK_HAVE_PROCESS(gProcess);	
	printIntegerRegs(*gCurThread);

}
void printAllRegs(char*)
{
	CHECK_HAVE_PROCESS(gProcess);

}


void dumpMemory(char*)
{

}

