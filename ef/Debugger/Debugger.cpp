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

#include <stdio.h>
#include "CatchAssert.h"
#include "Debugger.h"
#include "jvmdi.h"
#include "LogModule.h"
#include "NativeCodeCache.h"
#include "DebuggerChannel.h"
#include "StringUtils.h"
#include "JavaVM.h"

UT_DEFINE_LOG_MODULE(Debugger);

DebuggerState::DebuggerState(Pool& p) : 
	breakpoints(NULL),
	enabled(false),
	pool(p)
{
}

DebuggerState::DebuggerState(Pool& p, bool _enabled) : pool(p)
{
	enabled = _enabled;
	
	if (enabled) 
		enable();
}

void
DebuggerState::enable()
{
	enabled = true;
	breakpoints = new (pool) BreakPoint[MAX_BREAKPOINTS];

	thread = DebuggerServerChannel::spawnServer();

#ifdef LINUX
	// Set the breakpoint handler up.
	SetupBreakPointHandler();
#endif
}

void
DebuggerState::waitForDebugger()
{
	if (thread)
		PR_JoinThread(thread);
}

static PRNetAddr sNetaddr;

PRThread* DebuggerServerChannel::
spawnServer()
{
	PRFileDesc* connector;

	// Create a TCP socket
	if ((connector = PR_NewTCPSocket()) == NULL) 
	{
		UT_LOG(Debugger, PR_LOG_DEBUG, ("Debugger: PR_NewTCPSocket failed\n"));
		return (NULL);
	}
	
	sNetaddr.inet.family = AF_INET;
	sNetaddr.inet.port = htons(kDebuggerPort);
	sNetaddr.inet.ip = htonl(INADDR_ANY);

	if (PR_Bind(connector, &sNetaddr) < 0) 
	{
		UT_LOG(Debugger, PR_LOG_DEBUG, ("Debugger: PR_Bind failed\n"));
		return (NULL);
	}

	// Allow atmost 2 debugger connections 
	// 1 sync 1 async
	if (PR_Listen(connector, 2) < 0) 
	{
		UT_LOG(Debugger, PR_LOG_DEBUG, ("Debugger: PR_Listen failed\n"));
		return (NULL);
	}

	PRThread* thread;

	// Spawn the debugger listener thread
	thread = PR_CreateThread(PR_USER_THREAD,
							 &serverThread,
							 connector,
							 PR_PRIORITY_NORMAL,
							 PR_GLOBAL_THREAD,
							 PR_JOINABLE_THREAD,
							 0);
	return (thread);
}

void DebuggerServerChannel::
serverThread(void* inArgument)
{
#ifdef _WIN32
	SET_DEBUGGER_THREAD_ID(::GetCurrentThreadId());
#endif

	PRFileDesc* connector = (PRFileDesc*) inArgument;
	PRFileDesc* sync;
	PRFileDesc* async;

	if (!(sync = PR_Accept(connector, &sNetaddr, PR_INTERVAL_NO_TIMEOUT)))
		assert(false);

	if (!(async = PR_Accept(connector, &sNetaddr, PR_INTERVAL_NO_TIMEOUT)))
		assert(false);


	DebuggerServerChannel  serverChannel(sync, async);
	
	serverChannel.serverLoop();
}


void
DebuggerState:: clearAllBreakPoints()
{
	for (int i=0; i < MAX_BREAKPOINTS; i++)
		breakpoints[i].clear();
}
 



void DebuggerServerChannel::
serverLoop()
{
	#define HELL_HAS_NOT_FROZEN_OVER 1

	while (HELL_HAS_NOT_FROZEN_OVER)
	{
		Int32	request;

		request = mSync.readRequest();
		UT_LOG(Debugger, PR_LOG_DEBUG, ("command received: %c\n", (char) request));

		switch(request) 
		{
		case 'b':
			JVMDI_SetBreakpoint(NULL, 0, 0, 0);
			break;
		case 'g':
			// Not implemented
			break;
		case kAddressToMethod:
			// request address to method
			handleAddressToMethod();
			break;
		case kMethodToAddress:
			// request method to address
			handleMethodToAddress();
			break;
		case kRequestNotifyMethodCompiledLoaded:
			handleNotifyOnCompileLoadMethod();
			break;
		case kRunClass:
			handleRunClass();
			break;
		case 'q':
			// Debugger thread quits
			return;
		default:
			mSync.writeResponse(-1);
			break;
		}
	}
}

void
DebuggerState::setBreakPoint(void *pc)
{
#ifdef LINUX
	SetBreakPoint(pc);
#else
	pc;
#endif
}

void
DebuggerState::clearBreakPoint(void *pc)
{
	clearBreakPoint(pc);
}

// Access to the database of classes
#include "ClassWorld.h"
#include "StringPool.h"
extern ClassWorld world;
extern StringPool sp;

void DebuggerServerChannel::
handleAddressToMethod()
{
	DEBUG_TRACER(UT_LOG_MODULE(Debugger), "handleAddressToMethod");

	void*			address;
	CacheEntry*		ce;

	address = mSync.readPtr();
	ce = NativeCodeCache::getCache().lookupByRange((Uint8*) address);

	if (ce) {
		mSync.writeResponse(0);
		mSync.writeString(ce->descriptor.method->toString());
		mSync.writePtr((void*)((Uint8*) address - ce->start.getFunctionAddress()));
	}
	else
		mSync.writeResponse(-1);
}

void DebuggerServerChannel::
handleMethodToAddress()
{
	DEBUG_TRACER(UT_LOG_MODULE(Debugger), "handleMethodToAddress");
	char*			methodName;
	const char*		internedMethodName;

	// lookup method name in string pool (getMethod requires interned string)
	internedMethodName = sp.get(methodName = mSync.readString());
	delete [] methodName;


	if (internedMethodName) {
		Method* method;
		
		// now grab the method 
		if (world.getMethod(internedMethodName, method)) {
			// lookup method in cache
			MethodDescriptor	methodDescriptor(*method);

			void*		address = addressFunction(NativeCodeCache::getCache().lookupByDescriptor(methodDescriptor, true, true));
			if (address) {
				mSync.writeResponse(0);
				mSync.writePtr(address);
				return;
			}
		}
	}

	// if we fall-through the method was not found
	mSync.writeResponse(-1);
}

Vector<char*>	sNotifyList;

void DebuggerServerChannel::
handleNotifyOnCompileLoadMethod()
{
	DEBUG_TRACER(UT_LOG_MODULE(Debugger), "handleNotifyOnCompileLoadMethod");
	char*			methodName = mSync.readString();
	
	sNotifyList.append(methodName);		// methodName is new'ed for us by DebuggerChannel
	
	mSync.writeResponse(0);
}

void executeClass(void *inArgument);

void executeClass(void *inArgument)
{
	char* className = (char*) inArgument; 
	VM::theVM.execute(className, NULL, NULL, NULL, 0);
}


void DebuggerServerChannel::
handleRunClass()
{
	DEBUG_TRACER(UT_LOG_MODULE(Debugger), "handleRunClass");

	char*	  className = mSync.readString();
	PRThread* thread;

	// do not tie up this thread -- because we
	// we need this one for communication
	thread = PR_CreateThread(PR_USER_THREAD,
							 &executeClass,
							 className,
							 PR_PRIORITY_NORMAL,
							 PR_GLOBAL_THREAD,
							 PR_JOINABLE_THREAD,
							 0);

	mSync.writeResponse(thread ? 0 : -1); // return -1 if couldn't create thread
	   
	// FIX-ME leak className because too lazy
	// to make sure thread makes its own copy
	//delete [] className;
}

void DebuggerServerChannel::
listenToMessage(BroadcastEventKind inKind, void* inArgument)
{
	if (inKind == kEndCompileOrLoad) {
		assert(inArgument);
		Method& method = *(Method*) inArgument;

		char**	curPos;
		for (curPos = sNotifyList.begin(); curPos < sNotifyList.end(); curPos++)
			if (!PL_strcmp(*curPos, method.toString())) {
				mAsync.writeRequest(kNotifyMethodCompiledLoaded);
				mAsync.writeString(*curPos);

				MethodDescriptor	methodDescriptor(method);
				void*		address = addressFunction(NativeCodeCache::getCache().lookupByDescriptor(methodDescriptor, true, true));
				assert(address);
				mAsync.writePtr(address);				

				// now wait for a response
				assert(!mAsync.readResponse());
				break;
			}		
	}
}
