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
// StackWalker.cpp
// 
// Simon Holmes a Court
// Scott M. Silver
//

// Routines for walking and printing out the stack.
//
// Interesting debug routines are listed at the end of this function

#include "Fundamentals.h"
#include "StackWalker.h"


// Assembly guff to return the EBP
#if defined(_WIN32) && !defined(__GNUC__)
#define INLINE_GET_EBP(inEBPVariableName) \
    __asm mov inEBPVariableName, ebp
#elif defined(__i386__)
#define INLINE_GET_EBP(inEBPVariableName)				\
    ({ register __typeof__(inEBPVariableName) ebp_ __asm__("ebp");	\
       (inEBPVariableName) = ebp_; })
#else
#error Nothing else in this file is right either.
#endif


// Frame
//
// ACHTUNG ADVENTURER:  Be careful of changing the inlining
// of this constructor, it assumes it is called on real frame down
// from the scope where the Frame object is.
Frame::
Frame()
{
	Uint8 **foo;
	INLINE_GET_EBP(foo);		// inline assembly to grab our base pointer
	pFrame = foo;

	// skip over the constructor frame and return the caller's frame
	moveToPrevFrame();
}


// moveToPrevFrame
//
// Set the cursor for this frame to the previous frame
void Frame::
moveToPrevFrame()
{
	pMethodAddress = (Uint8*) *(pFrame + 1);
	pFrame = (Uint8**) *pFrame;	
}

// Compute the number of stack frames on the stack
int Frame::
getStackDepth()
{
    Frame frame;
    int depth = -1;
    do {
        frame.moveToPrevFrame();
        depth++;
    } while (frame.getBase());
    return depth;
}

// getCallingJavaMethod
//
// Move the cursor of this frame to the closest Java frame and
// return the method corresponding to that frame
Method &Frame::getCallingJavaMethod(int climbDepth)
{
	Method *m;

	do {
		m = 0;
		
		while (!m) {
			pMethodAddress = (Uint8*) *(pFrame + 1);
			pFrame = (Uint8**) *pFrame;	
			m = getMethodForPC(pMethodAddress);
		}

	} while ((m->getModifiers() & CR_METHOD_NATIVE) || --climbDepth);

	return *m;
}


// getMethod
//
// get Method* or NULL associated with this Frame
Method* Frame::
getMethod()
{
	return (getMethodForPC(pMethodAddress));
}


// getCallerPC
//
// Return the address of the function that called the getCallerPC
extern Uint8* getCallerPC()
{
#if defined(WIN32) || defined(LINUX) || defined(FREEBSD)
	Uint8** myEBP;
	
	INLINE_GET_EBP(myEBP);
				
	Uint8** calleeEBP = (Uint8**) *myEBP;			// get the callee's EBP
	Uint8* retAddress = (Uint8*) *(calleeEBP + 1);
	return retAddress;
#else	// WIN32 || LINUX || FREEBSD
	return 0;
#endif
}


// setCalleeReturnAddress
//
// change the return address of the callee's callee to the passed in pointer
// FIX-ME huh??
extern void setCalleeReturnAddress(Uint8* retAddress)
{
#ifdef WIN32
	Uint8** myEBP;
	INLINE_GET_EBP(myEBP);							// myEBP is the EBP of getCallerPC
	Uint8** calleeEBP = (Uint8**) *myEBP;			// get the callee's EBP
	(Uint8*) *(calleeEBP + 1) = retAddress;
#else	// non WIN32
	retAddress;
	trespass("not implemented");
#endif
}

// getMethodForPC
//
// Return Method* or NULL associated with this PC
extern Method* getMethodForPC(void* inCurPC)
{
	CacheEntry* ce = NativeCodeCache::getCache().lookupByRange((Uint8*) inCurPC);

	return (ce ? ce->descriptor.method : 0);
}


#ifdef DEBUG_LOG
UT_DEFINE_LOG_MODULE(StackWalker);

extern "C" 
void NS_EXTERN stackWalker(void* inCurPC)
{
    UT_SET_LOG_LEVEL(StackWalker, PR_LOG_DEBUG);

	UT_LOG(StackWalker, PR_LOG_ALWAYS, ("\n\n_____________________Top of stack______________________\n"));
	UT_LOG(StackWalker, PR_LOG_ALWAYS, ("%10s  %10s\n", "   Frame", "Return Addr"));

	if (inCurPC)
	  Frame::printOne(UT_LOG_MODULE(StackWalker), 0, inCurPC);

	Frame frame;
	do
	{
		frame.print(UT_LOG_MODULE(StackWalker));
		frame.moveToPrevFrame();
	}
	while (frame.hasPreviousFrame());

	UT_LOG(StackWalker, PR_LOG_ALWAYS, ("____________________Bottom of Stack____________________\n\n"));
}


#include "prprf.h"

void Frame::
printWithArgs(LogModuleObject &f, Uint8* inFrame, Method* inMethod)
{
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%s(", inMethod->toString()));

	const Signature& ourSignature = inMethod->getSignature();
	Uint16 numArgs = ourSignature.nArguments;
	const Type** ourArgs = ourSignature.argumentTypes;

	// calc arg size
	Uint16 i;
	Uint8* argListPtr = ((Uint8*) inFrame + 8); // just beyond first argument
	
	for(i = 0; i < numArgs; i++) 
	{
        if (inFrame)
            printValue(f, argListPtr, *ourArgs[i]);
		else
			UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("<frameless>"));
        argListPtr += nTypeKindSlots(ourArgs[i]->typeKind) * 4;
		if(i < (numArgs - 1))
			UT_OBJECTLOG(f, PR_LOG_ALWAYS, (", "));
	}

	UT_OBJECTLOG(f, PR_LOG_ALWAYS, (")"));
}

void Frame::
printOne(LogModuleObject &f, void* inFrame, void* inMethodAddress)
{
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("0x%08x (0x%08x) ", inFrame, inMethodAddress));

	Method* method = getMethodForPC(inMethodAddress);

	if(method)
		printWithArgs(f, (Uint8*)inFrame, method);
	else 
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("anonymous"));
	
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("\n"));
}

void Frame::
print(LogModuleObject &f)
{
  printOne(f, pFrame, pMethodAddress);
}
#endif
