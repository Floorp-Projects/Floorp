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
// Breakpoint.cpp
//
// Scott M. Silver

#include "Breakpoints.h"
#include "Debugee.h"
#include <windows.h>
#include <assert.h>
#include "Win32Util.h"


Vector<Breakpoint*>	BreakpointManager::sBreakpoints;

Breakpoint& BreakpointManager::
newBreakpoint(DebugeeProcess& inProcess, void* inWhere, bool inGlobal, bool inEnabled)
{
	Breakpoint* newBp = new Breakpoint(*(inProcess.getMainThread()), inWhere, inGlobal, inEnabled);
	sBreakpoints.append(newBp);

	return (*newBp);
}

Breakpoint& BreakpointManager::
newBreakpoint(DebugeeThread& inThread, void* inWhere, bool inGlobal, bool inEnabled)
{
	Breakpoint* newBp = new Breakpoint(inThread, inWhere, inGlobal, inEnabled);
	sBreakpoints.append(newBp);

	return (*newBp);
}


Breakpoint* BreakpointManager::
findBreakpoint(void* inAddress)
{
	Breakpoint**	curBreakpoint;

	for(curBreakpoint = sBreakpoints.begin(); curBreakpoint < sBreakpoints.end(); curBreakpoint++)
		if ((*curBreakpoint)->mAddress == inAddress)
			return *curBreakpoint;

	return (NULL);
}

void Breakpoint:: 
replace()
{
	BYTE	expectedDebugger;

 
	if (!mThread.getProcess().readMemory(mAddress, &expectedDebugger, 1))
	{
		showLastError();
		assert(false);
	}

	assert(expectedDebugger == 0xcc);

	if (!mThread.getProcess().writeMemory(mAddress, &mReplacedCode, 1))
	{	
		showLastError();
		assert(false);
	}				
}

void Breakpoint::
set()
{
	if (!mThread.getProcess().readMemory(mAddress, &mReplacedCode, 1))
	{
		showLastError();
		assert(false);
	}

	BYTE	debugger = 0xcc;

	if (!mThread.getProcess().writeMemory(mAddress, &debugger, 1))
	{	
		showLastError();
		assert(false);
	}				
}


