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
#ifndef _DEBUGGER_H_
#define _DEBUGGER_H_

#include "nspr.h"
#include "Pool.h"
#include "InterestingEvents.h"
#include "DebuggerChannel.h"

#define MAX_BREAKPOINTS 10

// The BreakPoint class holds the 
class NS_EXTERN BreakPoint {
	Uint32		pc;					// The address of the instruction
	Uint64		old_instruction;	// Saved copy of the instruction
	bool		enabled;			// Whether it's enabled

 public:
	void clear() { pc = 0; enabled = false; };
};	

class DebuggerChannel;

//
// The debugger object holds the current state of the debugger
class NS_EXTERN DebuggerState {
	BreakPoint *breakpoints;		// Breakpoints
	Int32		nbreakpoints;		// number of breakpoints set so far
	bool		enabled;			// Was -debug flag passed ?
	Pool		&pool;				// All memory allocations
	PRThread	*thread;			// The debugger thread

public:
	void setBreakPoint(void *pc);
	getBreakPoints();
	void clearBreakPoint(void *pc);
	void clearAllBreakPoints();
	
	// Constructors 
	DebuggerState(Pool& p);
	DebuggerState(Pool&p, bool _enabled);

	// Enabled ?
	void enable();
	bool getEnabled() { return enabled; };
	PRThread *getThread() { return thread; };
	static void eventHandler(void *arg);
	void waitForDebugger();

	static void handleAddressToMethod(DebuggerChannel& inChannel);
	static void handleMethodToAddress(DebuggerChannel& inChannel);
};

// Takes a pc offset from the beginning of a method and converts it to a
// bytecode offset, 
class pcToByteCodeTable {
	// A dumb implementation to start with. Space can be saved by storing
	// this as a range
	Int32 *table;
	Int32 nEntries;
 public:
	pcToByteCodeTable() { table = NULL; nEntries = 0; };
	// XXX - memory leak. Need to use a pool.
	pcToByteCodeTable(Int32 size) { table = new Int32[size]; nEntries = size; };
	void setSize(Int32 size);
	Int32 operator [](Int32 pcOffset) const { return table[pcOffset]; };
};

class DebuggerServerChannel :
	EventListener
{
	DebuggerStream  mSync;
	DebuggerStream  mAsync;

public:
	static PRThread*              spawnServer();
	virtual void listenToMessage(BroadcastEventKind inKind, void* inArgument);

protected:
	DebuggerServerChannel(PRFileDesc* inSync, PRFileDesc* inAsync) :
		EventListener(gCompileOrLoadBroadcaster),
	    mAsync(inAsync),
	    mSync(inSync) { }

	void    handleRequest(Int32 inRequest);	
	void    handleMethodToAddress();
	void    handleRequestDebuggerThread();
	void    handleAddressToMethod();
	void	handleNotifyOnCompileLoadMethod();
	void	handleRunClass();
	
	static void serverThread(void*);
	void serverLoop();
};


// Platform specific stuff
int SetBreakPoint(void *code);

#ifndef WIN32
void SetupBreakPointHandler();
#endif

inline 	void
pcToByteCodeTable::setSize(Int32 size)
{
	assert(table == NULL);
	table = new Int32[size];
	nEntries = size;
}

#endif /* _DEBUGGER_H_ */
