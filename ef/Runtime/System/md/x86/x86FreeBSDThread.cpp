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
#include <unistd.h>
#if 0
#include <sys/ptrace.h>
#endif
#include <assert.h>
#include <signal.h>
#ifdef _REENTRANT
#include <pthread.h>
#endif
#if 0
#include <linux/ptrace.h>
#include <sys/user.h>
#endif
#include "Thread.h"
#include "NativeCodeCache.h"
#include "Exceptions.h"

// Supend the thread specified by the pid
void _suspendThread(HANDLE  handle) 
{
#ifdef _REENTRANT
    if (pthread_kill(handle, SIGKILL) == -1) {
		assert(0);
    }
#endif
}

// Resume the thread specified by the pid
void _resumeThread(HANDLE  handle) 
{
#ifdef _REENTRANT
    if (pthread_kill(handle, SIGCONT) == -1) {
		assert(0);
    }
#endif
}

// Gets the thread context via ptrace
bool 
_getThreadContext(HANDLE handle, ThreadContext& context) 
{
#if 0
    if (ptrace(PTRACE_ATTACH, handle, NULL, NULL) == -1)
		assert(0);
    if (ptrace(PTRACE_PEEKUSR, handle, offsetof(struct user, regs), &context) 
		== -1)
		assert(0);
    
#endif
    return true;
}

// Sets the thread context via ptrace
void 
_setThreadContext(HANDLE handle, ThreadContext &context) 
{
#if 0
    if (ptrace(PTRACE_ATTACH, handle, NULL, NULL) == -1)
		assert(0);
    if (ptrace(PTRACE_POKEUSR, handle, offsetof(struct user, regs), &context) 
	== -1)
		assert(0);
#endif
}

Int32* 
_getFramePointer(ThreadContext& context) 
{
	return (Int32 *) context.sc_ebp; 
}

void _setFramePointer(ThreadContext& context, Int32* v) 
{
	context.sc_ebp = (long) v;
}

Uint8* 
_getInstructionPointer(ThreadContext &context) 
{
	return (Uint8*)context.sc_eip;
}

void 
_setInstructionPointer(ThreadContext& context, Uint8 *v) 
{
	context.sc_eip = (long) v;
}

void Thread::realHandle() 
{     
	// sets up the OS handles
	HANDLE thr = getpid();
}

/*	
	Special invoke for threads.

	We prepare the stack for a newborn thread as follows.  First we push
	the hardware exception handler (as {previous handler, new handler}),
	and set fs:[0] to point to this structure. This will make win32 call
	new handler when a hardware exception fires (/0, *NULL, or illegal
	instruction).

	Second, we push EBP as it it is upon entry to this routine, followed
	by a pointer to uncaughtException, followed by the sentinel
	NULL. Then we prepare to call __invoke() which corresponds to
	Method::invoke() by pushing the arguments in reverse order. Now,
	regardless whether invoke() returns normally or through an uncaught
	exception, we will return to the line 'add esp,24' which is where we
	clean up afterwards.

	For the entry set-up to uncaughtException() see
	uncaughtExceptionExit() in x86Win32ExceptionHandler.cpp in
	Runtime/System/md/x86.  
*/

// called by Thread::invoke() and is only used to avoid assuming a calling
// convention for C++ methods.
static void 
__invoke(Method* m, JavaObject* obj, JavaObject* arr[], int sz) 
{
	m->invoke(obj,arr,sz);
}

void
Thread::invoke(Method* m, JavaObject* obj, JavaObject* arr[], int sz) 
{
/*
	DWORD w32 = (DWORD)win32HardwareThrow;
	DWORD uE = (DWORD)uncaughtException;
	__asm {
		push w32
		push fs:[0]
		mov fs:[0],esp
		push ebp
		push uE
		push NULL
		mov ecx,esp
		push sz
		push arr
		push obj
		push m
		mov ebp,ecx
		call __invoke
		add esp,24 // pop 4 arguments, NULL, and uncaughtException
		pop ebp
		pop ecx
		mov fs:[0],ecx
		pop ecx  // pop win32HardwareThrow
	}
*/
	m->invoke(obj,arr,sz);
}
