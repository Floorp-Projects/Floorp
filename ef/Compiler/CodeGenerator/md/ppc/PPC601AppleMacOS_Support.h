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
// PPC601AppleMacOS_Support.h
//
// Scott M. Silver

#ifndef _H_PPC601APPLEMACOS_SUPPORT
#define _H_PPC601APPLEMACOS_SUPPORT
#include <string.h>

void* getDynamicPtrGl(void* inFromWhere, Uint8 inWhichRegister);

// JitGlobals
//
// Maintains a table of size 2^16
// 16 bit signed offsets are used to load data out of the table
// The gp (or rtoc), therefore, points to the middle of the table
class JitGlobals
{
public:
	enum
	{
		kTOCSize	= 0x10000
	};
	
	JitGlobals() 
	{ 
		mBegin = (const Uint8*) new Uint8[kTOCSize]; 
		mGlobalPtr = (const Uint8*) mBegin + (kTOCSize >> 1); 
		mEnd = (const Uint8*) mBegin + kTOCSize; 
		mNext = (Uint8*) mBegin; 
	}
	
	~JitGlobals()
	{
		delete mBegin;
	}

public:
	// addData
	// Add data beginning at inData of size inSize
	// to the JitGlobals.  Returns the address of the
	// copied data in JG or NULL if the globals are full.
	// outOffset is the offset in the table where the data
	// was added
	void* addData(const void* inData, Uint16 inSize, Uint16 &outOffset) 
	{
		// ¥¥ FIX-ME BEGIN LOCK
		Uint16	alignedSize = (inSize + 3) & ~3;	// align to 4 bytes
		
		Uint8*	dest = mNext;
		
		// make sure we didn't overflow the tbl
		if (mNext + alignedSize > mEnd)
			return (NULL);
		
		// copy data and return offset into table
		memcpy(dest, inData, inSize);
		outOffset = dest - mGlobalPtr;
		mNext += alignedSize;
		
		return (dest);	
		// ¥¥ÊFIX-ME END LOCK		
	}
	
	// toOffset
	// Returnns true if inData is in JG.  outOffset
	// is the offset to the data if its in the table.
	bool toOffset(void*	inData, Uint16& outOffset)
	{
		if (inData >= (void*)mBegin && inData < (void*)mNext)
		{
			outOffset = (Uint8*) inData - mBegin;
			return (true);
		}
		else
			return (false);	
	}
	
	// unsafe, only use if you are the only thread
	// who will be touching this table
	inline Uint8*	getNext() const { return mNext; }
	
public:
	const Uint8*		mBegin;					// beginning of table
	const Uint8*		mEnd;					// end of table
	const Uint8*		mGlobalPtr;				// what the "global ptr" should point to
private:
	Uint8*	mNext;					// next free entry in table
};

struct TVector
{
	void*	functionPtr;
	void*	toc;
};

extern	JitGlobals gJitGlobals;

//bool canBePCRelative(void* inStart, void* inDest, Int32& outOffset);


// canBePCRelative
//
inline bool canBePCRelative(void* inStart, void* inDest, Int32& outOffset)
{
	outOffset = (PRUptrdiff) inDest - (PRUptrdiff) inStart;

	if (inDest > inStart)
		return (outOffset < 0x1ffffff);
	else
		return (outOffset > (Int32) 0xfe000000);
}

extern JitGlobals&
acquireTOC(Uint16	inDesiredAmount);

void* getCompileStubPtrGl(void* inFromWhere);
const uint32 kCrossTocPtrGlBytes = 24;

void* formatCrossTocPtrGl(void* inStart, int16 inTOCOffset);

#ifndef XP_MAC
inline void* getDynamicPtrGl(void* /* inFromWhere */, Uint8 /* inWhichRegister */)
{
	return NULL;
}
inline void* formatCrossTocPtrGl(void* /* inStart */, int16 /* inTOCOffset */)
{
	return NULL;
}
#endif

// acquireTOC
//
// Try to find a TOC with enough space left in it to fit a desired amount.
// If one cannot be located, create a new TOC and return that.
//
// FIX-ME - right now we only support one TOC
inline JitGlobals&
acquireTOC(Uint16	/*inDesiredAmount*/)
{
	static JitGlobals	sJitGlobals;		// This is a global
	
	return (sJitGlobals);
}

#endif // _H_PPC601APPLEMACOS_SUPPORT
