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
#include "Fundamentals.h"
#include "ErrorHandling.h"


#ifdef DEBUG

char* verifyErrorString[] =
{
	"Unknown cause",
	"Functionality not implemented yet",
	"Error in reading class file: file not found, or has errors",
	"Badly formatted class file",
	"Class does not have permissions to access a particular field/method",
	"Field not found in class",
	"Method not found in class",
	"No bytecodes in a function",
	"Bad bytecode opcode",
	"Bad offset to a bytecode instruction",
	"Bad type passed to newarray instruction",
	"Constant pool index out of range",
	"Wrong return instruction used in this function",
	"Bad tableswitch or lookupswitch bytecode",
	"More than one ret bytecode for the same subroutine",
	"Recursive call to a subroutine or multiple returns from the same subroutine",
	"Bad combination of types in a bytecode",
	"Attempt to pop from an empty bytecode stack",
	"Attempt to push onto a full bytecode stack",
	"Stack depth at a point differs based on how execution got there",
	"Index of local variable out of range",
	"Attempt to write to a constant (final) field",
	"Given class not found in class file",
	"Catch filter class not a subclass of Throwable",
    "Class can be its own superclass",
    "Compiler internal limits reached",
    "Attempt to invoke abstract method",
    "Binary incompatibility / incompatibleClassChange"
};

char* runtimeErrorString[] =
{
	"Unknown cause",
	"Internal error",
	"Functionality not implemented yet",
	"incorrect argument to a method",
	"prohibited operation",
	"Security violation",
	"IO Error",
	"fileNotFound",
	"Unable to link method",
	"Null Pointer argument",
	"Attempt to instantiate an abstract class"
};

#endif

//
// This is called when there's something wrong with a class.
//
NS_EXTERN void 
verifyError(VerifyError::Cause cause)
{
#ifdef DEBUG
	fprintf(stderr, "\n*** NOTE: Throwing verify error: %s\n", verifyErrorString[cause]);
#endif

        throw VerifyError(cause);
}

//
// This is called when there is an error during runtime.
//
NS_EXTERN void 
runtimeError(RuntimeError::Cause cause)
{
#ifdef DEBUG
	fprintf(stderr, "\n*** NOTE: Throwing runtime error: %s\n", runtimeErrorString[cause]);
#endif

	throw RuntimeError(cause);
}
