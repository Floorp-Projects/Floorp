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
#include "Thread.h"

#include "Exceptions.h"
#include "NativeCodeCache.h"
#include "prprf.h"

void _suspendThread(HANDLE handle) {
	DWORD dw = SuspendThread(handle);
	assert(dw != 0xFFFFFFFF);
}

void _resumeThread(HANDLE handle) {
	ResumeThread(handle);
}

bool _getThreadContext(HANDLE handle, ThreadContext& context) {
	context.ContextFlags = CONTEXT_CONTROL;
    return (GetThreadContext(handle,&context) != 0);
}

void _setThreadContext(HANDLE handle, ThreadContext& context) {
    if (!SetThreadContext(handle,&context))
		assert(false);
}

Int32* _getFramePointer(ThreadContext& context) {
	return (Int32*)context.Ebp;
}

void _setFramePointer(ThreadContext& context, Int32* v) {
	context.Ebp = (DWORD)v;
}

Uint8* _getInstructionPointer(ThreadContext& context) {
	return (Uint8*)context.Eip;
}

void _setInstructionPointer(ThreadContext& context, Uint8* v) {
	context.Eip = (DWORD)v;
}

void Thread::realHandle() {     // sets up the OS handles
	HANDLE thr = GetCurrentThread();
	HANDLE p = GetCurrentProcess();
	if (!DuplicateHandle(p,thr,p,&handle,THREAD_ALL_ACCESS,FALSE,0))
		sysThrowNamedException("java/lang/ThreadDeathException");
}

/*	Special invoke for threads.

	We prepare the stack for a newborn thread as follows.
	First we push the hardware exception handler (as {previous handler, new handler}),
	and set fs:[0] to point to this structure. This will make win32 call new handler when
	a hardware exception fires (/0, *NULL, or illegal instruction).

	Second, we push EBP as it it is upon entry to this routine, followed by a pointer to uncaughtException,
	followed by the sentinel NULL. Then we prepare to call __invoke() which corresponds to Method::invoke()
	by pushing the arguments in reverse order. Now, regardless whether invoke() returns normally or through
	an uncaught exception, we will return to the line 'add esp,24' which is where we clean up afterwards.

	For the entry set-up to uncaughtException() see uncaughtExceptionExit() in x86Win32ExceptionHandler.cpp in 
	Runtime/System/md/x86.
 */
// called by Thread::invoke() and is only used to avoid assuming a calling convention for C++ methods.
static void __cdecl
__invoke(Method* m, JavaObject* obj, JavaObject* arr[], int sz) {
	m->invoke(obj,arr,sz);
}

void
Thread::invoke(Method* m, JavaObject* obj, JavaObject* arr[], int sz) {
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
}
