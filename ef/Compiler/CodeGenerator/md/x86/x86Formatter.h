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
// File:	x86Formatter.h
//
// Authors:	Peter DeSantis
//			Simon Holmes a Court
//			Scott Silver
//

#ifndef X86_FORMATTER
#define X86_FORMATTER

#include "Fundamentals.h"
#include "FormatStructures.h"	// for FCI, StackFrameInfo
#include "x86Win32Emitter.h"	// temp?

const int kStackSlotSize = 8;

//-----------------------------------------------------------------------------------------------------------
// x86StackFrameInfo
class x86StackFrameInfo :
	public StackFrameInfo
{
public:
			x86StackFrameInfo() { }

	void	init(VirtualRegisterManager& inVRManager)
	{
		// eventually the register allocator will tell us all this information
		numSavedGPRs = 3;
		numSavedFPRs = 0;
		numMonitorSlots = 0;

		localStore_bytes = inVRManager.nUsedStackSlots * kStackSlotSize;

		StackFrameInfo::init(inVRManager);
	}

	// return number of bytes than EBP of the stack slot
	Int32	getStackSlotOffset(VirtualRegister& inVr)
	{
		assert(hasBeenInited);
		assert(inVr.getClass() == StackSlotRegister);
		return (inVr.getColor() * - kStackSlotSize) - kStackSlotSize;
	}

private:
};

//-----------------------------------------------------------------------------------------------------------
// x86Formatter
class x86Formatter
{
protected:
	Uint16	mNumberOfArgumentWords;

	Uint32	mFrameSize_words;
	Uint32	mPrologSize_bytes;
	Uint32	mEpilogSize_bytes;	

	x86StackFrameInfo	mStackFrameInfo;

	// later should be object, not pointer
	FormattedCodeInfo	mFCI;			// cached format info
	
public:	
	// fields
	const	MdEmitter& mEmitter;
	
	// methods
			x86Formatter(const MdEmitter& inEmitter) : mEmitter(inEmitter) { }
	void	calculatePrologEpilog(Method& /*inMethod*/, Uint32& outPrologSize, Uint32& outEpilogSize);
	void	formatEpilogToMemory(void* inWhere);
	void	formatPrologToMemory(void* inWhere);

	void	beginFormatting(const FormattedCodeInfo& inInfo);
	void	endFormatting(const FormattedCodeInfo& /*inInfo*/) { }

	void	calculatePrePostMethod(Method& /*inMethod*/, Uint32& outPreMethodSize, Uint32& outPostMethodSize) { outPreMethodSize = outPostMethodSize = 0; }
	void	formatPostMethodToMemory(void* /*inWhere*/, const FormattedCodeInfo& /*inInfo*/) { }
	void	formatPreMethodToMemory(void* /*inWhere*/, const FormattedCodeInfo& /*inInfo*/) { }

	// stack frame stuff
	void	initStackFrameInfo();
	StackFrameInfo&	getStackFrameInfo() {return mStackFrameInfo; }
	Int32	getStackSlotOffset(VirtualRegister& inVr) { return mStackFrameInfo.getStackSlotOffset(inVr); }

	Uint8*	createTransitionVector(const FormattedCodeInfo& inInfo);

	Uint8*	getMethodBegin();
};


#ifdef DEBUG_LOG
void* disassemble1(LogModuleObject &f, void* inFrom);
#endif


#endif // X86_FORMATTER
