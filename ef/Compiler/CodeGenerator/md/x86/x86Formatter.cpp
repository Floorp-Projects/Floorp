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
// File:	x86Formatter.cpp
//
// Authors:	Peter DeSantis
//			Simon Holmes a Court
//			Scott Silver
//

#include "x86Formatter.h"
#include "FieldOrMethod.h"
#include "MemoryAccess.h"
#include "JavaObject.h"

//-----------------------------------------------------------------------------------------------------------
// Constants
const Uint8 kPush_ebp = 0x55;
const Uint8 kPush_edi = 0x57;
const Uint8 kPush_esi = 0x56;
const Uint8 kPush_ebx = 0x53;

const Uint16 kMov_ebp_esp	= 0x8bec;
const Uint16 kSub_esp_Imm8	= 0x83ec;   // 8-bit immediate subtract (8-bit val extended to 32-bits)
const Uint16 kSub_esp_Imm32	= 0x81ec;   // Signed, 32-bit immediate subtract

const Uint8 kPop_ebp = 0x5d;
const Uint8 kPop_ebx = 0x5b;
const Uint8 kPop_esi = 0x5e;
const Uint8 kPop_edi = 0x5f;

const Uint8 kRet = 0xc3;
const Uint8 kRet_Imm16 = 0xc2;
const Uint16 kMov_esp_ebp = 0x8be5;

//-----------------------------------------------------------------------------------------------------------
// x86Formatter

void x86Formatter::
beginFormatting(const FormattedCodeInfo& inInfo)
{ 
	mFCI = inInfo;
}

void x86Formatter::
initStackFrameInfo() 
{
	mStackFrameInfo.init(mEmitter.mVRAllocator); 
}

void x86Formatter::
calculatePrologEpilog(Method& inMethod, Uint32& outPrologSize, Uint32& outEpilogSize)
{
	Uint32 GPRwords = mStackFrameInfo.getNumSavedGPRWords();
	Uint32 localStoreBytes = mStackFrameInfo.getLocalStoreSizeBytes();

	if(localStoreBytes == 0)
	{	
		mPrologSize_bytes = 3 + GPRwords;
		mEpilogSize_bytes = 4 + GPRwords;	
	}
    else {
        if (localStoreBytes < 128)
    		mPrologSize_bytes = 6 + GPRwords;
        else
            mPrologSize_bytes = 9 + GPRwords;
		mEpilogSize_bytes = 6 + GPRwords;
    }

	// determine how many words were passed to us, so the epilog can clean up the stack
	const Signature& ourSignature = inMethod.getSignature();
	const int numArgs = ourSignature.nArguments;
	const Type** ourArgs = ourSignature.argumentTypes;
	
	mNumberOfArgumentWords = 0;
	for(int i = 0; i < numArgs; i++)
		mNumberOfArgumentWords += nTypeKindSlots(ourArgs[i]->typeKind);

	outPrologSize = mPrologSize_bytes;
	outEpilogSize = mEpilogSize_bytes;
}

void x86Formatter::	
formatPrologToMemory(void* inWhere)
{
	Uint8* where = (uint8*)inWhere;

	// push ebp
	*where++ = kPush_ebp;

	//mov ebp, esp
	writeBigHalfwordUnaligned(where, kMov_ebp_esp);
	where += 2;

	// reserve local store space
	// sub esp, localstore
	Uint32 localStoreBytes = mStackFrameInfo.getLocalStoreSizeBytes();
    if (localStoreBytes) {
        if (localStoreBytes < 128)
        {
            writeBigHalfwordUnaligned(where, kSub_esp_Imm8);
            where += 2;
            *where++ = localStoreBytes;
        }
        else
        {
            writeBigHalfwordUnaligned(where, kSub_esp_Imm32);
            where += 2;
            writeLittleWordUnaligned(where, localStoreBytes);
            where += 4;
        }
    }

	// save GPRs -- FIX change to bitfields sometime
	Uint32 GPRwords = mStackFrameInfo.getNumSavedGPRWords();
	if(GPRwords > 2)
		*where++ = kPush_edi;
	if(GPRwords > 1)
		*where++ = kPush_esi;
	if(GPRwords > 0)
		*where++ = kPush_ebx;
}

void x86Formatter::	
formatEpilogToMemory(void* inWhere)
{
	Uint8* where = (uint8*)inWhere;

	// restore GPRs	-- FIX change to bitfields sometime
	Uint32 GPRwords = mStackFrameInfo.getNumSavedGPRWords();
	if(GPRwords > 0)
		*where++ = kPop_ebx;
	if(GPRwords > 1)
		*where++ = kPop_esi;
	if(GPRwords > 2)
		*where++ = kPop_edi;
  
	// mov esp, ebp
	Uint32 localStoreBytes = mStackFrameInfo.getLocalStoreSizeBytes();
	if(localStoreBytes != 0) 
	{
		writeBigHalfwordUnaligned(where, kMov_esp_ebp);
		where+=2;
	}

	// pop ebp
	*where++ = kPop_ebp;

	// return cleaning the stack of passed in arguments
	if(mNumberOfArgumentWords != 0)
	{
		*where++ = kRet_Imm16;
		writeLittleHalfwordUnaligned(where, mNumberOfArgumentWords * 4);
	} 
	else 
	{
		*where = kRet;
	}
}

Uint8* x86Formatter::
createTransitionVector(const FormattedCodeInfo& inInfo) 
{ 
	return inInfo.methodStart; 
}

Uint8* x86Formatter::
getMethodBegin() 
{
	return mFCI.methodStart;
}

//-----------------------------------------------------------------------------------------------------------
// Debugging

#ifdef DEBUG_LOG

#include "XDisAsm.h"

#define USING_X86_DISASSEMBLER

const int kMaxDissasemblyBytes = 8;	// maximum number of bytes to dump per instruction

// Function:	disassemble1	
void* 
disassemble1(LogModuleObject &f, void* inFrom)
{
#ifdef USING_X86_DISASSEMBLER
	char* disasmText;
	char* startAddr = (char*)inFrom;
	char* curAddr = startAddr;

	disasmText = disasmx86(	0, 
							&curAddr, 
							curAddr + 32, 
							kDisAsmFlag32BitSegments);

	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("0x%p:  ", inFrom));

	// print memory dump
	Uint8 numBytes = (Uint8)curAddr - (Uint8)startAddr;
	for(int i = 0; i < kMaxDissasemblyBytes; i++)
	{ 
		if (i < numBytes) {
			Uint8 address= ((Uint8*)startAddr)[i];
			UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%x%x ", address/16, address%16));
		}
		else
			UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("   "));
	}

	UT_OBJECTLOG(f, PR_LOG_ALWAYS, (" %s\n", disasmText));	
	return (curAddr);
#else
	f = NULL; inFrom = NULL;
	return NULL;
#endif

}

#endif // DEBUG_LOG
