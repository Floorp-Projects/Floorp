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
//
// File:	FormatStructures.h
//
// Authors:	Simon Holmes a Court
//
// To avoid circular include problems, data structures used in the md emitter, md formatter
//  and native formatter may be found here.
//

#ifndef _FORMAT_STRUCTURES_
#define _FORMAT_STRUCTURES_

#include "Fundamentals.h"
#include "LogModule.h"

struct Method;
class VirtualRegisterManager;
struct MethodDescriptor;

//-----------------------------------------------------------------------------------------------------------
// FormattedCodeInfo
struct FormattedCodeInfo
{
	Method*		method;			// runtime descriptor of the method we're formatting
	Uint8*		methodStart;	// ptr to beginning of allocated memory for the method
	Uint8*		methodEnd;		// ptr to (FIX byte after?) end of allocated memory for the method

	Uint32 		prologSize;		
	Uint32		epilogSize;
	Uint32		bodySize;		// prologSize + epilogSize + formatted size of on the instructions in all ControlNodes
	Uint32		preMethodSize;	
	Uint32 		postMethodSize;
};

//-----------------------------------------------------------------------------------------------------------
// TVector
#ifdef GENERATE_FOR_PPC
#define USE_TVECTOR
#endif

#ifdef USE_TVECTOR
	extern void* _MD_createTVector(const MethodDescriptor& inMethodDescriptor, void* inFunctionMemory);
	#define M_MD_createTVector(inMethodDescriptor, inFunctionMemory)	(_MD_createTVector(inMethodDescriptor, inFunctionMemory))
#else
	#define M_MD_createTVector(inMethodDescriptor, inFunctionMemory)	(inFunctionMemory)
#endif

//-----------------------------------------------------------------------------------------------------------
// StackFrameInfo
class StackFrameInfo
{
protected:
	Uint8	numSavedGPRs;
	Uint8	numSavedFPRs;
	Uint8	numMonitorSlots;

	Uint32	localStore_bytes;

	DEBUG_ONLY(bool	hasBeenInited;)

public:
				StackFrameInfo() { DEBUG_ONLY(hasBeenInited = false); };
	void		init(VirtualRegisterManager& /*inVRManager*/) {	DEBUG_ONLY(hasBeenInited = true); }
	Uint32		getNumSavedGPRWords()		{ assert(hasBeenInited); return numSavedGPRs; }
	Uint32		getNumSavedFPRWords()		{ assert(hasBeenInited); return numSavedFPRs; }
	Uint32		getNumMonitorSlots()		{ assert(hasBeenInited); return numMonitorSlots; }

	Uint32		getRegisterOffset()			{ assert(hasBeenInited); return numMonitorSlots; }

	Uint32		getLocalStoreSizeBytes()	{ assert(hasBeenInited); return localStore_bytes; }

	// x86 only right now!
	// returns the offset (off EBP) to the beginning of the registers
	Uint32		getCalleeSavedBeginOffset() 
	{ 
		return localStore_bytes + 4;
	}

	// returns the offset (off EBP) to the stack pointer before any arguments are pushed

	Uint32		getStackPointerOffset()
	{
		return localStore_bytes + (4 * numSavedGPRs) + (4 * numSavedFPRs) + (4 * numMonitorSlots);
	}

#ifdef DEBUG_LOG
	void		print(LogModuleObject &f)
	{
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("Policy:	g%d:f%d:l%d\n", getNumSavedGPRWords(), getNumSavedFPRWords(), getLocalStoreSizeBytes()));
	}
#endif // DEBUG_LOG
};

#endif // _FORMAT_STRUCTURES_
