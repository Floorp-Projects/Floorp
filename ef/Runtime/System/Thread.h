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
#ifndef _Thread
#define _Thread
#include "Monitor.h"
#include "JavaObject.h"
#include "FieldOrMethod.h"

#ifdef _WIN32

typedef CONTEXT ThreadContext;

#elif defined (__linux__)

#include <sigcontext.h>

typedef void* HANDLE;
typedef struct sigcontext ThreadContext; 

#elif defined (__FreeBSD__)

#include <machine/signal.h>

typedef void* HANDLE;
typedef struct sigcontext ThreadContext;

#else

typedef void* HANDLE;
typedef int ThreadContext;

#endif


void _suspendThread(HANDLE p);

void _resumeThread(HANDLE p);

bool _getThreadContext(HANDLE handle, ThreadContext& context);

void _setThreadContext(HANDLE handle, ThreadContext& context);

Int32* _getFramePointer(ThreadContext& context);

void _setFramePointer(ThreadContext& context, Int32* v);

Uint8* _getInstructionPointer(ThreadContext& context);

void _setInstructionPointer(ThreadContext& context, Uint8* v);

class NS_EXTERN Thread {
	enum { DEFAULT, ALIVE, SUSPENDED, INTERRUPTED, KILLED, CLEANUP };
	static PointerHashTable<Thread*>* table;
	PRLock* _lock;
	PRThreadPriority prio;  // NSPR priority
	void* id;				// NSPR
	HANDLE handle;			// Win32
	JavaObject* peer;       // Java instance of thread
	int state;              // used for suspend/resume/interrupt
	static void run(void*); // C++ version of java.lang.Thread.run()
	void realHandle();
	void _stop(JavaObject* exc); // implements asynchronous stop
public:
	static Pool* pool;
	struct JavaThread : JavaObject { // shadow structure. See java_lang_Thread.h
        JavaObject* name;
        Int32 priority;
        JavaObject* threadQ;
        Int64 eetop;
		Uint32 single_step;
		Uint32 daemon;
		Uint32 stillborn;
		JavaObject* target;
		JavaObject* group;
		JavaObject* inheritedAccessControlContext;
		
		// dummy constructor, never used (only used for casting)
		JavaThread(const Type& type) : JavaObject(type) { }
	};
	Thread(JavaObject* o) : prio(PR_PRIORITY_NORMAL), id(NULL), handle(NULL), peer(o), state(DEFAULT) {
	    _lock = PR_NewLock();
	}
	~Thread() {
	    PR_DestroyLock(_lock);
	}
	inline void lock() {
	    PR_Lock(_lock);
	}
	inline void unlock() {
	    PR_Unlock(_lock);
	}
	static void* getCurrentThread() {
	    return (void*)PR_GetCurrentThread();
	}
	Int32 getStatus() {
		return state;
	}
	static void staticInit();
	static JavaObject* currentThread();
	static void yield();
	static void sleep(Int64 i);
	static void exit() {
		// clean up
		::exit(0);
	}
	void start();
	void stop(JavaObject* exc);
	void interrupt();
	bool isInterrupted(Uint32 f);
	bool isAlive();
	Int32 countStackFrames();
	void setPriority(Int32 p);
	void suspend();
	void resume();
	static void invoke(Method* m,JavaObject* obj,JavaObject* arr[],int sz);
	friend void uncaughtException();
};

#endif

