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
// Thread.cpp
//
// Bjorn Carlson

#include "Thread.h"
#include "Exceptions.h"
#include "NativeCodeCache.h"
#include "prprf.h"
#include "SysCallsRuntime.h"

#ifdef _WIN32
#include "md/x86/x86Win32Thread.h"
#elif defined(LINUX)
#include "x86LinuxThread.h"
#elif defined(FREEBSD)
#include "x86FreeBSDThread.h"
#else

#define GetPassedException(E)

void Thread::realHandle() {}

void Thread::invoke(Method* m,JavaObject* obj,JavaObject* arr[],int sz) {
	m->invoke(obj,arr,sz);
}

void _suspendThread(HANDLE /*p*/) {}

void _resumeThread(HANDLE /*p*/) {}

bool _getThreadContext(HANDLE /*handle*/, ThreadContext& /*context*/) {	return false; }

void _setThreadContext(HANDLE /*handle*/, ThreadContext& /*context*/) {}

Int32* _getFramePointer(ThreadContext& /*context*/) { return NULL; }

void _setFramePointer(ThreadContext& /*context*/, Int32* /*v*/) {}

Uint8* _getInstructionPointer(ThreadContext& /*context*/) { return NULL; }

void _setInstructionPointer(ThreadContext& /*context*/, Uint8* /*v*/) {}

#endif

Pool* Thread::pool;
PointerHashTable<Thread*>* Thread::table;

/*
	We support threads in C++ as follows. Every instance of java.lang.Thread has a corresponding C++ peer.
	The peer represent all the low-level information we need to implement the native calls for Java threads,
	and is used to bridge between Java threads and native (NSPR/Win32/Solaris/...) threads. Most native
	methods (see Thread.cpp in Packages/java/lang/) check if the C++ instance has been creates and if not
	it is created on the fly (right now from Thread::pool).
	
	Every instance of java.lang.Thread has a property 'eetop' of type Int64 which is only used for native 
	access to the thread object. We store a pointer to the C++ peer in 'eetop'. Hence,	all native methods 
	required by java.lang.Thread can pick up the peer using this field (see Thread.cpp in Packages/java/lang/).

    For every thread that we create, we keep a mapping between its identifier and the C++ object representing it.
	This is added in Thread::run() (see below).
*/

/*
	This is the static initializer for threads. It initializes the global structurs
	used by the C++ methods in here, and creates a Java object, 'obj', of class java.lang.Thread
	to represent the main thread in Java. It does this by setting up some of the properties
	of java.lang.Thread objects for 'obj', and then calling the 'init' method of java.lang.Thread
	on 'obj'. The Field and Method classes are used for reading and writing properties and
	methods of 'obj'. Also, staticInit() adds a mapping between the current thread id and the
	C++ peer of 'obj', named 'thr' below. Hence, current_thread() can use the thread
	id to lookup both the C++ object and the Java object representing the current thread.

*/
void Thread::staticInit() {
	pool = new Pool();
	table = new PointerHashTable<Thread*>(*pool);
	// create an object representing the main thread.
	Class& thrClass = Class::forName("java/lang/Thread");
	JavaObject& obj = thrClass.newInstance();
	Field& fl_daemon = thrClass.getDeclaredField("daemon");
	fl_daemon.setBoolean(&obj,false);
	Field& fl_prio = thrClass.getDeclaredField("priority");
	//Field& fl_normprio = thrClass.getDeclaredField("NORM_PRIORITY");
	//fl_prio.setInt(&obj,fl_normprio.getInt(NULL));
	fl_prio.setInt(&obj,5); // NORM_PRIORITY
	Class& tgClass = Class::forName("java/lang/ThreadGroup");
	const Type* typearr[1];
	JavaObject* objarr[1];
	JavaObject& tgobj = tgClass.getDeclaredConstructor(typearr,0).newInstance(objarr,0);
	Field& fl_group = thrClass.getDeclaredField("group");
	fl_group.set(&obj,tgobj);
	Thread* thr = new(*pool) Thread(&obj);
	Field& fl_eetop = thrClass.getDeclaredField("eetop");
	fl_eetop.setLong(&obj,(Int64)thr);
	thr->state = ALIVE;
	thr->id = getCurrentThread();
	thr->realHandle();
	table->add(thr->id,thr);
	const Type& ifc = Class::forName("java/lang/Runnable");
	const Type& strc = Class::forName("java/lang/String");
	const Type *typearr1[3] = {&tgClass,&ifc,&strc};
	Method& m = thrClass.getDeclaredMethod("init",typearr1,3);
	JavaObject* objarr1[3] = {NULL,NULL,JavaString::make("")};
	invoke(&m,&obj,objarr1,3);
}

// returns the current thread object using the peer pointer
JavaObject* Thread::currentThread() {
	Thread* thr;
	if (table->get(getCurrentThread(),&thr))
		return thr->peer;
	else
		return NULL;
}

void Thread::yield() {
	PR_Sleep(PR_INTERVAL_NO_WAIT);
}

void Thread::sleep(Int64 i) {
	Int32 j = (Int32)i;
#ifdef DEBUG
	PR_fprintf(PR_STDOUT,"About to sleep for %d\n",j);
#endif
	PR_Sleep(PR_MillisecondsToInterval(j));
#ifdef DEBUG
	PR_fprintf(PR_STDOUT,"Waking up from sleep (%d)\n",j);
#endif
}

// this is called if a thread throws an uncaught exception. It calls back to 
// java.lang.ThreadGroup.uncaughtException() to give the thread group a chance to cleanup.
void
uncaughtException() {
	JavaObject* exc = NULL;
	Thread* thr;

	GetPassedException(exc);
	Thread::table->get(Thread::getCurrentThread(),&thr);
	if (thr->state == Thread::CLEANUP) // recursive invocation
		return;
	thr->state = Thread::CLEANUP;
	// call thr->peer->group->uncaughtException(thr->peer,exc);
	Class& thrClass = Class::forName("java/lang/Thread");
	Class& throwClass = Class::forName("java/lang/Throwable");
	const Type* typearr[2] = {&thrClass,&throwClass};
	Class& tgClass = Class::forName("java/lang/ThreadGroup");
	Method& m = tgClass.getDeclaredMethod("uncaughtException",typearr,2);
	JavaObject* objarr[2] = {thr->peer,exc};
	Field& fl_group = thrClass.getDeclaredField("group");
	JavaObject& tg = fl_group.get(thr->peer);
	m.invoke(&tg,objarr,2);
}

// entry point for threads where a callback through Method::invoke() is made to the Java peer's
// run method. First we check that we haven't been killed or suspended already. Then, a call to 
// Monitor::threadInit() sets up thread local data used by the monitors. Finally, the callback is
// made. This call will *always* return, whether an exception happened or not.
void Thread::run(void* arg) {
	Thread* thr = (Thread*)arg;
	thr->lock();
	thr->id = getCurrentThread();
	thr->realHandle();
	table->add(thr->id,thr);
	while (thr->state == SUSPENDED) { // someone has suspended us
		thr->unlock();
		_suspendThread(thr->handle);
	    thr->lock();
	}
	if (thr->state > SUSPENDED) {
		thr->unlock();
		sysThrowNamedException("java/lang/ThreadDeath");
	    return;
	}
	thr->state = ALIVE;
	Monitor::threadInit();
	// call run through thr->peer
	const Type *typearr[1];
	Class* c = const_cast<Class*>(&thr->peer->getClass());
	Method& m = c->getMethod("run",typearr,0);
	JavaObject* objarr[1];
	thr->unlock();
	invoke(&m,thr->peer,objarr,0);
	table->remove(thr->id);
	thr->lock();
	if (thr->state < KILLED)
		thr->state = KILLED;
	thr->unlock();
	// recycle thr if not in pool.
	return;
}

void Thread::start() {
	PRThread* thr = PR_CreateThread(PR_USER_THREAD,run,(void*)this,
						PR_PRIORITY_NORMAL,PR_GLOBAL_THREAD,
						PR_UNJOINABLE_THREAD,0); // Later use join.
	assert(thr != NULL);
	PR_Sleep(PR_INTERVAL_NO_WAIT);
	//PR_Sleep(PR_MillisecondsToInterval(100));
	PR_SetThreadPriority(thr, prio);
}

// stops by:
// if already killed, do nothing.
// otherwise, set the state and call _stop() if the thread has been started.
void Thread::stop(JavaObject* obj) {
	if (obj == NULL) {
		sysThrowNamedException("java/lang/NullPointerException");
		return;
	}
    lock();
    int ostate = state;
    if (state >= KILLED) {
        unlock();
		return;
    }
    state = KILLED;
    unlock();
	if (ostate == ALIVE)
		_stop(obj);
}

void Thread::interrupt() {
    lock();
    int ostate = state;
    if (state >= INTERRUPTED) {
        unlock();
	return;
    }
    state = INTERRUPTED;
    unlock();
	if (ostate == ALIVE)
		_stop(&const_cast<Class *>(&Standard::get(cInterruptedException))->newInstance());
}

bool Thread::isInterrupted(Uint32 /*f*/) {
    int ostate;
    lock();
    ostate = state;
    //if (f == 1) 
    //    state = ALIVE;
    unlock();
    return ostate == INTERRUPTED;
}

bool Thread::isAlive() {
	return state == ALIVE || state == SUSPENDED;
}

void Thread::setPriority(Int32 p) {
    switch (p) {
    case 1:
    case 2:
    case 3:
	prio = PR_PRIORITY_LOW;
	break;
    case 4:
    case 5:
    case 6:
    case 7:
	prio = PR_PRIORITY_NORMAL;
	break;
    case 8:
    case 9:
    case 10:
	prio = PR_PRIORITY_HIGH;
	break;
    default:
	return;
    }
}

// suspends thread by:
// if not running return.
// Otherwise, suspend the thread.
void Thread::suspend() {
    lock();
    if (state > ALIVE) {
        unlock();
		return;
    }
	state = SUSPENDED;
	unlock();
#ifndef __linux__
    if (handle != NULL)
#else 
	if (handle != 0)
#endif
		_suspendThread(handle);
}

void Thread::resume() {	
    lock();
    if (state != SUSPENDED) {
        unlock();
		return;
    }
	state = ALIVE;

#ifndef __linux__
    if (handle != NULL)
#else 
	if (handle != 0)
#endif
		_resumeThread(handle);
    unlock();
}

// counts by following EBP until 0 is reached.
// First the thread is suspended.
Int32 Thread::countStackFrames() {
    ThreadContext context;
    Int32* frame;
    Int32 i = 0;
#ifndef __linux__
    if (handle == NULL)
#else 
	if (handle == 0)
#endif
		return 0;
	if (id != getCurrentThread())
		_suspendThread(handle);
    if (!_getThreadContext(handle,context)) {
		_resumeThread(handle);
		return -1; // error
	}
    frame = _getFramePointer(context);
    while (frame != NULL) {
		i++;
		frame = (Int32*)*frame;
    }
	_resumeThread(handle);
    return i;
}

// support for killing a thread T
// This is what needs to happen:
// 1. Suspend all threads or just T
// 2. copy the code of the current function of T (found through the saved eip)
// 3. find the asynchronous checkpoints, and smash them with 'int 3'.
// 4. add thread local data to the dying thread for the exception handling 
// 5. Set 'eip' of T to point to the instruction in the new code corresponding
// to the old position. Resume T.
// When the async. exception is thrown at runtime, free the code copy, and 
// throw a Java exception corresponding to the exception that was provided
// by kill/stop (default: java.lang.ThreadDeath).
// If an exception is thrown before the breakpoint is hit and which is not caught by
// the local exception handler, before repeating the search a level up, free the
// current code copy, make a copy as before of the receiving function (if one
// exists), insert breakpoints, update the receiver's exception table,
// and throw the exception. Again, the above must be repeated if another exception
// is thrown before a breakpoint is hit (puh!).
// How do we detect whether we should throw normally or not?
// By checking whether the current function is in the cache?
// By checking some thread local data?
// In any way, we will use thread local data to carry state from the asynchronous/hardware
// exceptions into the throw routines.

// for now we simplify as follows:
/*
	First, suspend the thread to be killed.
	Then, scan its stack to find the innermost return address which points to jitted code. Smash this
	return address to point to an illegal instruction, followed by a pointer to the exception
	being provided for the stopped thread. The illegal instruction will generate a hardware exception
	when the thread is resumed, which will initiate the exception handling.
  */
void Thread::_stop(JavaObject* exc)
{
	Int32 *frame, *ret=NULL;
	ThreadContext context;
	Uint8* ip;

#ifndef __linux__
	if (handle == NULL)
#else
	if (handle == 0)
#endif
		return;
	if (id != getCurrentThread())
		_suspendThread(handle);
	NativeCodeCache& ca = NativeCodeCache::getCache();
	if (_getThreadContext(handle,context)) {
		frame = _getFramePointer(context);
		ip = _getInstructionPointer(context);
		CacheEntry* ce = ca.lookupByRange(ip);
		while (ce == NULL && frame != NULL) { // scan to find innermost jit fram.
			ret = frame+1;
			ip = (Uint8*)*ret;
			frame = (Int32*)*frame;
			ce = ca.lookupByRange(ip);
		}
		if (ce != NULL) { // REPLACE
#if defined(XP_PC) || defined(LINUX) || defined(FREEBSD)
			ip = new(*pool) Uint8[10];
			ip[0] = 0xf;                  // 0x0f0b is an illegal instruction
			ip[1] = 0xb;
			if (ret == NULL) {
				*(Int32*)(ip+2) = (Int32)_getInstructionPointer(context);
				_setInstructionPointer(context,ip);
			}
			else {
				*(Int32*)(ip+2) = *ret;   
				*ret = (Int32)ip;             // new return address is illegal instruction
			}
			*(Int32*)(ip+6) = (Int32)exc; // picked up by the code for illegal instructions in the hw-exception handler
#else
			exc;	// force use of exc to make compiler happy
#endif
			_setThreadContext(handle,context);
		}
	}
	if (id != NULL)
	    PR_Interrupt((PRThread*)id);
	_resumeThread(handle);
}

// later use something like the following in the above instead:
/*
		if (ce != NULL) { // REPLACE
			Uint8* newcode = ce->asynchVersion(ca.mPool,ip);
			context.Eip = (DWORD)newcode;
			if (!SetThreadContext(handle,&context)) 
				assert(false);
			cache = new(*pool) ExceptionTableCache(ce->eTable,newcode,exc);
		}
*/
