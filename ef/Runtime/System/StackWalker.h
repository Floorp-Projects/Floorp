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
// StackWalker.h
//
// Simon Holmes a Court


#ifndef _STACK_WALKER_
#define _STACK_WALKER_

#include "Fundamentals.h"
#include "NativeCodeCache.h"
#include "LogModule.h"

class NS_EXTERN Frame
{
private:
	Uint8**	pFrame;					// pointer to the EBP
	Uint8*	pMethodAddress;			// holds an address within the method

public:
	Frame();
	void		moveToPrevFrame();
	bool		hasPreviousFrame() { return (pFrame != NULL && *pFrame != NULL); }
    Uint8**		getBase()		{ return pFrame; }
    static int  getStackDepth();
	
	Method &getCallingJavaMethod();

	// returns the Method associated with the frame, or NULL if there is not one
	Method*	getMethod();

	#ifdef DEBUG
	void print(LogModuleObject &f);
	static void printWithArgs(LogModuleObject &f, Uint8* inFrame, Method* inMethod);
    static void printOne(LogModuleObject &f, void*, void*);	
    #endif
};


extern "C" void NS_EXTERN	stackWalker(void*);
extern Uint8*				getCallerPC();
extern void					setCalleeReturnAddress(Uint8* retAddress);
extern Method*				getMethodForPC(void* inCurPC);


#endif // _STACK_WALKER_
