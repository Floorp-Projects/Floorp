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
// Breakpoint.h
//
// Scott M. Silver

#include <Windows.h>
#include "Vector.h"

class DebugeeThread;
class DebugeeProcess;
class Breakpoint;

class BreakpointManager
{
	static Vector<Breakpoint*>	sBreakpoints;

public:
	static Breakpoint&			newBreakpoint(DebugeeProcess& inProcess, void* inWhere, bool inGlobal = false, bool inEnabled = true);
	static Breakpoint&			newBreakpoint(DebugeeThread& inThread, void* inWhere, bool inGlobal = false, bool inEnabled = true);
	static Breakpoint*			findBreakpoint(void* inAddress);
	static Vector<Breakpoint*>& getBreakpoints() { return (sBreakpoints); }
};

class Breakpoint
{
	friend	class BreakpointManager;

	void*			mAddress;
	BYTE			mReplacedCode;
	bool			mEnabled;
	bool			mGlobal;
	DebugeeThread&	mThread;

					Breakpoint(DebugeeThread& inThread, void* inWhere, bool inGlobal = false, bool inEnabled = true) :
						mAddress(inWhere),
						mEnabled(inEnabled),
						mGlobal(inGlobal),
						mThread(inThread) { }
public:
	bool			getEnabled() const { return (mEnabled); }
	void			setEnabled(bool inEnabled) { mEnabled = inEnabled; }
	DebugeeThread&	getThread() const { return (mThread); }
	
	BYTE			getReplaced() { assert(mEnabled); return (mReplacedCode); }
	void			replace();
	void			set();
};

Breakpoint*		findFreeBreakpoint();
