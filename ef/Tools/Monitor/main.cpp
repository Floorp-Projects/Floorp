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
// main.cpp
//
// Scott M. Silver
//

#include <stdio.h>
#include <assert.h>
#include <windows.h>
#include "Debugee.h"
#include "CommandLine.h"
#include "prthread.h"

#include "XDisAsm.h"

class Breakpoint;

DWORD WINAPI	debugEventThread(void *);
void			startupDebugee(LPTSTR lpszFileName, LPTSTR lpszTitle, PROCESS_INFORMATION* outProcessInfo);
void			disassembleBytes(char* start, int length);
void			showLastError();

HANDLE			hMessageEvent;
DWORD			gDebugEventHandlerID;
HANDLE			gQueueEventFlag;
HANDLE	hVictimProcess;


DebugeeThread*	gCurThread;
DebugeeProcess*	gProcess;

enum
{
	kDebugTheadMessage = WM_USER

};




enum DebugEventKind
{
	dkBreakpoint
};

struct DebugEvent
{
	DebugEvent(DebugEventKind inEventKind, const DEBUG_EVENT& inPlatformEvent, HANDLE inThreadH) :
		mEventKind(inEventKind),
		mPlatformEvent(inPlatformEvent),
		mThreadH(inThreadH) { }

	DebugEventKind	mEventKind;
	DEBUG_EVENT		mPlatformEvent;
	HANDLE			mThreadH;

};

void
postDebuggerEvent(DebugEvent* inEvent)
{
	::PostThreadMessage(gDebugEventHandlerID, kDebugTheadMessage, (WPARAM) inEvent, NULL);
	::WaitForSingleObject(gQueueEventFlag, INFINITE);
}

inline int
instructionLength(DebugeeProcess* inProcess, char* start, int length)
{
	char data[32];
	assert(::ReadProcessMemory(hVictimProcess, start, &data, 32, NULL));

	char* tempStart = data;

	return (x86InstructionLength(0, &tempStart, data + length, kDisAsmFlag32BitSegments));
}

void realMain(void*);

struct MainArgs
{
	int argc;
	char **argv;
};

int main(int argc, char** argv)
{
	PRThread* thread;
	MainArgs mainArgs = {argc, argv};

	thread = PR_CreateThread(PR_USER_THREAD,
								realMain,
								&mainArgs,
								PR_PRIORITY_NORMAL,
								PR_GLOBAL_THREAD,
								PR_JOINABLE_THREAD,
								0);

	PR_JoinThread(thread);
	return (0);
}

void realMain(void* inArgument)
{
	MainArgs*	args = (MainArgs*) inArgument;

	const char*	testApp = "sajava.exe";
	//"e:\\trees\\ef1\\ns\\electricalfire\\Tools\\Monitor\\Test\\winbuild\\Debug\\testapp.exe";
	printf("Furmon - v0.0.1\n");

	HANDLE	debugThreadH;

	gProcess = DebugeeProcess::createDebugeeProcess(testApp, ::GetCurrentThreadId(), debugThreadH);

	commandLineLoop(args->argc, args->argv);

//	::WaitForSingleObject(debugThreadH, INFINITE);
}






