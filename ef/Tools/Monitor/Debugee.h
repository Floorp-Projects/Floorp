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
// Debugee.h

#ifndef _H_DEBUGEE
#define _H_DEBUGEE

#include <Windows.h>
#include "Vector.h"

class DebugeeProcess;
class Breakpoint;

class DebugeeThread
{
	friend DebugeeProcess;

public:
					DebugeeThread(HANDLE inThreadH, DWORD inThreadID, DebugeeProcess& inProcess, bool inSuspendable) :
						mThreadH(inThreadH),
						mProcess(inProcess),
						mThreadID(inThreadID),
						mBp(0),
						mSuspendable(inSuspendable) { }

	DWORD			resume(bool inSingleStepping = false);
	DWORD			suspend();
	void			singleStep();

	DWORD			getThreadID() const { return mThreadID; }
	HANDLE			getThreadHandle() const { return mThreadH; }
	DebugeeProcess&	getProcess() const { return mProcess; }
	void*			getIP();
	void*			getSP();

	BOOL			getContext(DWORD inContextFlags, CONTEXT& outContext);
	BOOL			setContext(DWORD inContextFlags, CONTEXT& ioContext);
	
	void			print();

private:
	void			handleSingleStep();
	void			pushSingleStepAction(Breakpoint* inBp, bool inStaySuspended) { assert(!mBp); mBp = inBp; mStaySuspended = inStaySuspended; } 
	
	bool			mSuspendable;
	bool			mStaySuspended;
	Breakpoint*		mBp;	
	DWORD			mThreadID;
	DebugeeProcess&	mProcess;
	HANDLE			mThreadH;
};

class DebuggerClientChannel;

class DebugeeProcess
{
	Vector<DebugeeThread*>		mThreads;
	CREATE_PROCESS_DEBUG_INFO	mCreateProcessInfo;
	HANDLE						mProcessH;
	HANDLE						mHandledEvent;
	DebuggerClientChannel*		mDebuggerChannel;

public:
	enum SymbolKind
	{
		kNil = 0,
		kNative,
		kJava
	};
							// use this to create a debugee process
	static DebugeeProcess*	createDebugeeProcess(const char* inFullPath, DWORD inDebugEventHandlerID, HANDLE& outDebugThreadH);

	BOOL					writeMemory(void* inDebugeeDest, void* inSrc, DWORD inSrcLen, DWORD* outBytesWritten = NULL);
	BOOL					readMemory(const void* inDebugeeSrc, void* inDest, DWORD inDestLen, DWORD* outBytesRead = NULL);

	DebugeeThread*			handleToThread(HANDLE inHandle);
	DebugeeThread*			idToThread(DWORD inThreadID);

	void					kill();
	void					suspendAll();	// increment suspend count on all threads
	void					resumeAll();	// decrement suspend count on all threads

	DebuggerClientChannel*	getChannel(bool inForce = false);
	Vector<DebugeeThread*>& getThreads() { return mThreads; }
	SymbolKind				getSymbol(const void* inPC, char* outName, DWORD inBufLen, DWORD& outOffset);  
	void*					getAddress(const char* inSymbol);
	HANDLE					getProcessHandle() { return mProcessH; }
	DebugeeThread*			getMainThread() { return mThreads[0]; }


private:
	bool					handleSingleStep(const DEBUG_EVENT& inDebugEvent, DebugeeThread* inThread);
	bool					handleBreakpoint(const DEBUG_EVENT& inDebugEvent, DebugeeThread* inThread);
	void					addThread(DebugeeThread* inNewThread) { mThreads.append(inNewThread); }
	void					handleModuleLoad(HANDLE inFileH, void* inBaseOfImage);
	
	// Bootstrapping stuff (get the debugger thread going, startup up the debugee, etc)
	struct DebugStartupInfo
	{
		const char*		fullPath;				// in
		DWORD			debugEventHandlerID;	// in
		HANDLE			debugeeProcessCreated;	// io event flag protecting next field
		DebugeeProcess*	newDebugeeProcess;		// out new object
	};

	static void			debugEventThread(void* inDebugStartupInfo);
						DebugeeProcess(DEBUG_EVENT*	inNewProcessEvent);
};

#endif // _H_DEBUGEE
