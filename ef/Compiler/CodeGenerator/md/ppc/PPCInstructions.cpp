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
// PPCInstructions.cpp
//
// Scott M. Silver

#include "PPCInstructions.h"
#include "PPC601AppleMacOSEmitter.h"

/* Instantiate templates explicitly if needed */
#ifdef MANUAL_TEMPLATES
template class AForm<true, true, false, true>;
template class AForm<true, true, false, false>;
#endif


void DFormXY::
formatToMemory(void* inStart, Uint32 /*inOffset*/, MdFormatter& /*inFormatter*/)
{
	struct
	{
		uint OPCD:6;
		uint D:5;
		uint A:5;
		uint IMM:16;
	} dForm;
	
	dForm.OPCD = sDFormInfos[mKind].opcode;
	dForm.D = getD();
	dForm.A = getA();
	dForm.IMM = getIMM();
	
	*(Uint32*) inStart = *(Uint32*) &dForm;
}

void LdD_RTOC::
formatToMemory(void* inStart, Uint32 inOffset, PPCFormatter& inFormatter)
{
	mImmediate += inFormatter.mRealTOCOffset;
	
	LdD_FixedSource<2>::formatToMemory(inStart, inOffset, inFormatter);
}

void XFormXY::
formatToMemory(void* inStart, Uint32 /*inOffset*/, MdFormatter& /*inFormatter*/)
{
	struct
	{
		uint OPCD:6;
		uint D:5;
		uint A:5;
		uint B:5;
		uint OE:1;
		uint XO:9;
		uint RC:1;
	} xForm;
	
	xForm.OPCD = sXFormInfos[mKind].primary;;
	xForm.D = getD();
	xForm.A = getA();
	xForm.B = getB();
	xForm.OE = ((mFlags & pfOE) != 0) ? 1 : 0;
	xForm.XO = sXFormInfos[mKind].opcode;
	xForm.RC = ((mFlags & pfRc) != 0) ? 1 : 0;
	
	*(Uint32*) inStart = *(Uint32*) &xForm;
}

void BranchI::
formatToMemory(void* inStart, Uint32 inOffset, MdFormatter& /*inFormatter*/) 
{ 
	mIForm.LI = (mTarget.getNativeOffset() - inOffset) >> 2;
	
	*(Uint32*) inStart = *(Uint32*) &mIForm; 
}


void BranchCB::
formatToMemory(void* inStart, Uint32 inOffset, MdFormatter& /*inFormatter*/)
{
	mCForm.BD = (mTarget.getNativeOffset() - inOffset) >> 2;
	*(Uint32*) inStart = *(Uint32*) &mCForm;
}

BranchCB::
BranchCB(DataNode* inPrimitive, Pool& inPool, ControlNode& inTarget, Condition2 inCondition, bool inAbsolute, bool inLink) :
	PPCInstructionXY(inPrimitive, inPool, 1, 0),
	mTarget(inTarget)
#if DEBUG
	, mCond(inCondition)
#endif
{
	mCForm.OPCD = 16; 
	
	mCForm.BO = sBranchConditions[inCondition].bo; 
//	mCForm.Y = 0; 
	mCForm.BI = sBranchConditions[inCondition].bi;
	mCForm.AA = inAbsolute; 
	mCForm.LK = inLink;
}

void MForm::
formatToMemory(void* inStart, Uint32 /*inOffset*/, MdFormatter& /*inFormatter*/)
{
	struct
	{
		uint OPCD:6;
		uint S:5;
		uint A:5;
		uint B:5;
		uint MB:5;
		uint ME:5;
		uint RC:1;
	} mForm;
	
	mForm.OPCD = sMFormInfos[mKind].opcode;
	mForm.S = getS();
	mForm.A = getA();
	mForm.B = getB();
	mForm.MB = getMB();
	mForm.ME = getME();
	mForm.RC = ((mFlags & pfRc) != 0) ? 1 : 0;
	
	*(Uint32*) inStart = *(Uint32*) &mForm;
}



