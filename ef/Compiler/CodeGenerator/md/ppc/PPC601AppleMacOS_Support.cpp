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
// PPC601AppleMacOS_Support.cpp.
//
// Scott M. Silver
//
// Support for the "Just-In-Time" part of the compiler
// Also a handful of routines for dealing with PowerPC linkage oddities (ptr gl, etc)

#include "NativeCodeCache.h"
#include "NativeFormatter.h"
#include "PPC601AppleMacOS_Support.h"
#include "NativeFormatter.h"
#include "AssembledInstructions.h"
#include "JavaObject.h"


// All volatile registers
struct VolatileRegisters
{
	Uint32	r3; 	Uint32	r4;		Uint32	r5;		Uint32	r6;
	Uint32	r7; 	Uint32	r8;		Uint32	r9;		Uint32	r10;
	Flt64	fp0;	Flt64	fp1;	Flt64	fp2;	Flt64	fp3;
	Flt64	fp4;	Flt64	fp5;	Flt64	fp6;	Flt64	fp7;
	Flt64	fp8;	Flt64	fp9;	Flt64	fp10;	Flt64	fp11;
	Flt64	fp12;	Flt64	fp13;	Flt64	r31;
};	

static void*
formatDynamicPtrGl(void* inStart, uint8 inRegister);

static void
formatCompileStubPtrGl(void* inStart);

asm static void 
locate();

asm void 
saveVolatiles(VolatileRegisters *vr);

asm void 
restoreVolatiles(VolatileRegisters *vr);

const uint32 kDynamicPtrGlBytes = 20;	// see formatDynamicPtrGl

// formatDynamicPtrGl
//
// Output some pointer glue through register inRegister
// beginning at inStart in memory.  We require that
// the size of the buffer be at least kDynamicPtrGlBytes
// long.  This kind of pointer glue is used for 
// dispatching v-table based (dynamic)  calls.
static void*
formatDynamicPtrGl(void* inStart, Uint8 inRegister)
{
	Uint32*	curPC = (Uint32*) inStart;
	
	*curPC++ = makeDForm(36, 2, 1, 20);						// stw		rtoc, 20(sp)
	*curPC++ = makeDForm(32, 0, inRegister, 0);				// lwz 		r0, 0(inRegister)
	*curPC++ = makeDForm(32, 2, inRegister, 4);				// lwz 		rtoc, 4(inRegister);
	*curPC++ = kMtctrR0;									// mtctr 	r0
	*curPC++ = kBctr;										// bctr			
	return (curPC);
}


// formatCrossTocPtrGl
//
// Output cross-toc ptr gl beginning at inStart.
// Use inTOCOffset at the offset in the TOC to find
// the callee's TVector ptr.  The buffer beginning at inStart
// must be at least kCrossTocPtrGlBytes long.
void*
formatCrossTocPtrGl(void* inStart, int16 inTOCOffset)
{
	uint32*	curPC = (uint32*) inStart;
	
	*curPC++ = makeDForm(32, 12, 2, inTOCOffset);	// lwz		r12, offset(rtoc)
	*curPC++ = makeDForm(36, 2, 1, 20);				// stw		rtoc, 20(sp)
	*curPC++ = makeDForm(32, 0, 12, 0);				// lwz 		r0, 0(r12)
	*curPC++ = makeDForm(32, 2, 12, 4);				// lwz 		rtoc, 4(r12);
	*curPC++ = kMtctrR0;							// mtctr 	r0
	*curPC++ = kBctr;								// bctr			
	return (curPC);
}


const uint32 kCompileStubPtrGlBytes = 12;	// see formatCompileStubPtrGl

// formatCompileStubPtrGl
//
// Output a short stub beginng at inStart which loads
// the address of "locate" and jumps to it.  The TOC must be set to
// the system's TOC before this routine is called (which is why a compile stub
// has a TVector with it's TOC set to the system TOC).  inStart is
// assumbed to be a buffer of atleast kCompileStubPtrGlBytes.
static void
formatCompileStubPtrGl(void* inStart)
{
	uint32*	curPC = (uint32*) inStart;
	
	// this is some cruft to locate the offset (from C) to locate's TVector
	uint8*	startTOC = ((uint8**) formatCompileStubPtrGl)[1];		// grab our TOC
	uint8*	compileStubFunction = (uint8*) locate;					// grab &locate (which is ptr to TVector)
	
	*curPC++ = makeDForm(32, 0, 2, compileStubFunction - startTOC);		// lwz 		r0, locate(RTOC)
	*curPC++ = kMtctrR0;												// mtctr 	r0
	*curPC++ = kBctr;													// bctr			
}


// DynamicPtrGlManager
//
// Manages a hunk of dynamic ptr gl
// FIX-ME synchronization
class DynamicPtrGlManager
{
public:
	enum
	{
		kFirstPossibleRegister	= 4,		// since r3 must be the this ptr and can never be used for dynamic dispatching
		kLastPossibleRegister 	= 31,
		kTotalPtrGls			= kLastPossibleRegister - kFirstPossibleRegister + 1
	};
	
	DynamicPtrGlManager() 
	{ 
		void* allPtrGls = NativeCodeCache::getCache().acquireMemory(kTotalPtrGls  * kDynamicPtrGlBytes);
		mPtrGl = new void*[kTotalPtrGls];
		
		void** curPtrGl = mPtrGl;
		
		for (int curReg = kFirstPossibleRegister; curReg < kLastPossibleRegister + 1; curReg++)
		{
			*curPtrGl++ = allPtrGls;
			allPtrGls = formatDynamicPtrGl(allPtrGls, curReg);
		}
	}
	
	void *
	getGl(void* DEBUG_ONLY(inFromWhere), Uint8 inWhichRegister) 
	{  
	  #ifdef DEBUG
		Int32	dummy;
	  #endif
		
		assert(inWhichRegister >= kFirstPossibleRegister && inWhichRegister <= kLastPossibleRegister);
		
		// FIX-ME allocate new ptr glues if this fails
		assert(canBePCRelative(inFromWhere, mPtrGl[inWhichRegister - kFirstPossibleRegister], dummy));	
		
		return (mPtrGl[inWhichRegister - kFirstPossibleRegister]);
	}
	
	
	void**	mPtrGl;
};

DynamicPtrGlManager	sPtrGlueManager;

void* getDynamicPtrGl(void* inFromWhere, Uint8 inWhichRegister)
{
	return (sPtrGlueManager.getGl(inFromWhere, inWhichRegister));
}


// getCompileStubPtrGl
//
// Locate a compile stub ptr gl withing pc-relative range of inFromWhere
// If one does not exist, create one.  Assume that the newly created
// one will be in range.  We also assume we will be reusing this often
// due to (hopefully) monotonically increasing addresses of new blocks
// of memory.  FIX-ME?
//
// sMostRecentCompileStubPtrGl is maintained such that it is a single
// entry cache for the last CompileStubPtrGl we generated.
//
// A compileStubPtrGl is a sequence of instructions which jumps to
// the "locate" function which, in some way, locates/compiles/loads, etc
// a currently uncompiled method.
void* getCompileStubPtrGl(void* inFromWhere)
{
	Int32	dummyOffset;
	static  void*	sMostRecentCompileStubPtrGl = NULL; // This is a global!
	
	// make a new ptr glue for the compile stub
	if ((sMostRecentCompileStubPtrGl == NULL) || (!canBePCRelative(inFromWhere, sMostRecentCompileStubPtrGl, dummyOffset)))
	{
		void* newPtrGl = NativeCodeCache::getCache().acquireMemory(kCompileStubPtrGlBytes);
		formatCompileStubPtrGl(newPtrGl);
		sMostRecentCompileStubPtrGl = newPtrGl;
	}
	
	// assert that we actually succeeded in our goal, this really
	// should result in programattic failure (abort, etc)
	bool compileStubAssertion = canBePCRelative(inFromWhere, sMostRecentCompileStubPtrGl, dummyOffset);
	assert(compileStubAssertion);
	
	return (sMostRecentCompileStubPtrGl);
}


// _MD_createTVector
//
// Create a "transition vector" a two word entry containing a ptr to inFunctionMemory
// (the beginning of a compiled function) and a second word containing the environment
// or TOC pointer...currently we always use the system TOC.  All other TVector's should
// be acquired from the Formatter objects.
void* _MD_createTVector(const MethodDescriptor& /*inMethodDescriptor*/, void* inFunctionMemory)
{	
	TVector*	newTVector = (TVector*) NativeCodeCache::getCache().acquireMemory(sizeof(TVector));
	newTVector->functionPtr = inFunctionMemory;
	newTVector->toc = *((void**)_MD_createTVector + 1);
	
	return (newTVector);
}


// BackPatchInfo
//
// little structure used to communicate between the assembly
// stub and the actual compilation and backpatching of the caller
struct BackPatchInfo
{
	void**		callerTOC;		// caller's TOC
	JavaObject*	thisPtr;		// callee this ptr (if a dynamic dispatch, otherwise undefined)
};


// backPatchMethod
//
// Cause the caller to call inMethodAddress next time the call instruction
// is encountered (instead of going through the stub). inLastPC is the next
// PC to be executed upon return from the intended callee (just after the call
// instruction.  inUserData is assumed to point to a BackPatchInfo*. (above)
//
// The trick is, that this has to be atomic.  That is, atomic from the standpoint
// that any thread entering the caller must not crash, but it would be ok for the
// thread to end up in the compile stub.  
//
// For backpatching vtables, we call a routine on the method object which 
// does all our dirty work.  (It's non-trivial since the same method can occupy
// different positions in a vtable due to interfaces).
// We do the work for static dispatches.  Our strategy is to try to use the cheapest
// call-like instruction.  See the "General Idea" section at the top of this file
// for details of this strategy.
void* backPatchMethod(void* inMethodAddress, void* inLastPC, void* inUserData)
{
	TVector*		methodTVector = (TVector*) inMethodAddress;
	Uint32* 		nopOrRestoreToc = (Uint32*) inLastPC;
	BackPatchInfo* 	backPatchInfo = (BackPatchInfo*) inUserData;
	
	// find bl
	Uint32*	blToPtrGlue = nopOrRestoreToc - 1;
	assert ((*blToPtrGlue & kBl) == kBl);		
	
	// see where the destination of this bl -- it must be some kind of ptrgl
	Int16 	offsetToPtrGlue = (Int16) (*blToPtrGlue & 0x000FFFC);
	Uint32*	ptrGlue = (Uint32*) (((Uint8*) (nopOrRestoreToc - 1)) + offsetToPtrGlue);
	
	if (*ptrGlue == makeDForm(36, 2, 1, 20))					// stw		rtoc, 20(sp)
	{
		// dynamic call
		Uint32*	dynamicLookup = blToPtrGlue - 1;
		
		assert ((*dynamicLookup & makeDForm(32, 0, 0, 0)) == makeDForm(32, 0, 0, 0));  	// lwz		r?, offset(rtoc)
		Int16	vtableOffset = *dynamicLookup & 0xFFFF;									// grab offset in table
		
		// replace value in vtable
		((void**)&backPatchInfo->thisPtr->type)[vtableOffset >> 2] = inMethodAddress;
	}
	else
	{
		// static call
		assert ((*ptrGlue & makeDForm(32, 12, 2, 0)) == makeDForm(32, 12, 2, 0));  	// lwz		r12, offset(rtoc)

		Int32	offsetToCallee;
		bool	sameTOC = (backPatchInfo->callerTOC == methodTVector->toc);
		
		// first try an absolute branch
		// then try a pc-relative branch
		// last use the ptr glue (just change the entry in the TOC)
		if (sameTOC && (Uint32) methodTVector->functionPtr < 0x3FFFFFF)
		{
			*blToPtrGlue  = kBla | ( (Uint32) methodTVector->functionPtr & 0x03FFFFFF);	
			blToPtrGlue[1] = kNop;
		}
		else if (sameTOC && canBePCRelative(blToPtrGlue, methodTVector->functionPtr, offsetToCallee))
		{
			*blToPtrGlue = (kBl | offsetToCallee);			// change bl to branch directly to callee
			blToPtrGlue[1] = kNop;							// don't restore toc, same toc				
		}
		else
		{
			Int16	tocOffset = *ptrGlue & 0xFFFF;			// grab offset in table
			
			backPatchInfo->callerTOC[tocOffset] = inMethodAddress;	// now change the ptr in the table
		}
	}
	
				
	return (inMethodAddress);
}


// locate
//
// Assembly glue called by the locateStub to actually locate/compile/etc a
// method and back patch the caller.  We save all volatiles (and
// of course save non-volatiles if we use them), fill in a BackPatchInfo
// object, and hand off control to compileAndBackPatchMethod.  Upon return
// we restore volatiles, fix up the stack (so it appears we were at the
// callsite in the caller), and jump to the intended callee (returned by
// compileAndBackPatchMethod).
//
// Only one parameter is assumed, r11 = ptr to CacheEntry of method to be located.
// We chose r11 because it is volatile, but not used by any ptr-gl generated by
// the Metrowerks Compilers.  We may have to do something slightly different for
// AIX.  We may just push the cacheEntry on the stack in the stub, and fix up
// the stack in here.
//
// Currently we only "locate" methods by compilation
asm static void locate()
{
	register CacheEntry*	cacheEntry;
	register void* 			oldLR;
	register TVector* 		compiledMethod;
	BackPatchInfo			backPatchInfo;
	VolatileRegisters		volatileRegisters;
	
	fralloc
	mr		cacheEntry, r11					// save r11 somewhere else, in case we end up in ptrgl somewhere
	mflr	oldLR

	// save off volatiles
	stw 	r3,  backPatchInfo.thisPtr		// r3 is saved in backPatchInfo (it is the thisPtr sometimes)
	la		r3, volatileRegisters			// save rest of volatiles
	bl		saveVolatiles

	// fill in the rest of backPatchInfo	
	lwz		r5, 260(SP)						// caller's RTOC (260 is a special constant, dependent on amt of stack space used in this function)
	stw		r5, backPatchInfo.callerTOC

	// compileAndBackPatch
	mr		r3, cacheEntry
	mr		r4, oldLR
	la		r5, backPatchInfo
	bl		compileAndBackPatchMethod		// r3 = TVector to actual method
	mr		compiledMethod, r3				// compiledMethod = TVector to actual method
	
	// restore volatiles
	la		r3, volatileRegisters
	bl		restoreVolatiles
	lwz		r3, backPatchInfo.thisPtr		// restore r3

	// jmp to the real callee
	lwz		r0, compiledMethod->functionPtr	// r0 = ptr to method
	lwz		RTOC, compiledMethod->toc		// RTOC = toc of compiled method	
	mtctr	r0								// ctr = ptr to method
	frfree
	bctr
}


// saveVolatiles
//
// Save all non-volatiles except r3 (because it is used for the incoming parameter)
asm void saveVolatiles(register VolatileRegisters *vr)
{
//	stw		r3,		vr->r3
	stfd	fp0,	vr->fp0
	stw		r4,		vr->r4
	stfd	fp1,	vr->fp1
	stw		r5,		vr->r5
	stfd	fp2,	vr->fp2
	stw		r6,		vr->r6
	stfd	fp3,	vr->fp3
	stw		r7,		vr->r7
	stfd	fp4,	vr->fp4
	stw		r8,		vr->r8
	stfd	fp5,	vr->fp5
	stw		r9,		vr->r9
	stfd	fp6,	vr->fp6
	stw		r10,	vr->r10
	stfd	fp7,	vr->fp7
	stfd	fp8,	vr->fp8
	stfd	fp9,	vr->fp9
	stfd	fp10,	vr->fp10
	stfd	fp11,	vr->fp11
	stfd	fp12,	vr->fp12
	stfd	fp13,	vr->fp13
	blr
}

// restoreVolatiles
//
// Restore all non-volatiles except r3 (because it is used for the incoming parameter)
asm void restoreVolatiles(register VolatileRegisters *vr)
{
//	lwz		r3,		vr->r3
	lfd		fp0,	vr->fp0
	lwz		r4,		vr->r4
	lfd		fp1,	vr->fp1
	lwz		r5,		vr->r5
	lfd		fp2,	vr->fp2
	lwz		r6,		vr->r6
	lfd		fp3,	vr->fp3
	lwz		r7,		vr->r7
	lfd		fp4,	vr->fp4
	lwz		r8,		vr->r8
	lfd		fp5,	vr->fp5
	lwz		r9,		vr->r9
	lfd		fp6,	vr->fp6
	lwz		r10,	vr->r10
	lfd		fp7,	vr->fp7
	lfd		fp8,	vr->fp8
	lfd		fp9,	vr->fp9
	lfd		fp10,	vr->fp10
	lfd		fp11,	vr->fp11
	lfd		fp12,	vr->fp12
	lfd		fp13,	vr->fp13
	blr
}


void *
generateNativeStub(NativeCodeCache& /*inCache*/, const CacheEntry& /*inCacheEntry*/, void * /*nativeFunction*/)
{
	trespass("Not implemented");
	return 0;
}


// generateCompileStub
//
// Genereate a kLocateStubBytes stub from memory acquired fromn inCache.
// The branches to some ptr-gl which jumps to the actual "locate" routine.
// It is assumed that there is already a reachable compileStubPtrGl, or one
// can be created by calling getCompileStubPtrGl.
extern void* 
generateCompileStub(NativeCodeCache& inCache, const CacheEntry& inCacheEntry)
{
	const size_t	kLocateStubBytes = 12;
	
	Uint32*	stub = (Uint32*) inCache.acquireMemory(kLocateStubBytes);
	Uint32* curPC = stub;
		
	*curPC++ = makeDForm(15, 11, 0, ((Uint32) &inCacheEntry) >> 16);								// addis	r11, r0, hiword
	*curPC++ = makeDForm(24, 11, 11, ((Uint32) &inCacheEntry) & 0xFFFF);							// ori		r11, r11, loword
	*curPC = (kB | (((PRUptrdiff) getCompileStubPtrGl(curPC) - (PRUptrdiff) curPC) & 0x03FFFFFF));	// b compileStubPtrGl
	
	return (_MD_createTVector(inCacheEntry.descriptor, stub));
}



