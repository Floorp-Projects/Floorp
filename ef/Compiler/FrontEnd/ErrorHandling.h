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
#ifndef ERRORHANDLING_H
#define ERRORHANDLING_H

#include "NativeDefs.h"

struct VerifyError
{
	enum Cause
	{
		unknown,								// Unknown cause
		notImplemented,							// Functionality not implemented yet
		noClassDefFound,						// Error in reading class file: file not found, or has errors
		badClassFormat,							// Badly formatted class file
		illegalAccess,							// Class does not have permissions to access a particular field/method
		noSuchField,							// Field not found in class
		noSuchMethod,							// Method not found in class
		noBytecodes,							// No bytecodes in a function
		badBytecode,							// Bad bytecode opcode
		badBytecodeOffset,						// Bad offset to a bytecode instruction
		badNewArrayType,						// Bad type passed to newarray instruction
		badConstantPoolIndex,					// Constant pool index out of range
		badReturn,								// Wrong return instruction used in this function
		badSwitchBytecode,						// Bad tableswitch or lookupswitch bytecode
		multipleRet,							// More than one ret bytecode for the same subroutine
		jsrNestingError,						// Recursive call to a subroutine or multiple returns from the same subroutine
		badType,								// Bad combination of types in a bytecode
		bytecodeStackUnderflow,					// Attempt to pop from an empty bytecode stack
		bytecodeStackOverflow,					// Attempt to push onto a full bytecode stack
		bytecodeStackDynamic,					// Stack depth at a point differs based on how execution got there
		noSuchLocal,							// Index of local variable out of range
		writeToConst,        					// Attempt to write to a constant (final) field
		classNotFound,							// Given class not found in class file
		nonThrowableCatch,						// Catch filter class not a subclass of Throwable
        classCircularity,                       // Class can be its own superclass
        resourceExhausted,                      // Compiler internal limits reached
        abstractMethod,                         // Attempt to invoke abstract method
        incompatibleClassChange                 // Binary incompatibility
	};

	const Cause cause;

	VerifyError(Cause cause): cause(cause) {}
};

NS_EXTERN
void verifyError(VerifyError::Cause cause);

struct RuntimeError
{
	enum Cause
	{
		unknown,								// Unknown cause
		internal,                               // Internal error
		notImplemented,							// Functionality not implemented yet
		illegalArgument,                        // incorrect argument to a method
		illegalAccess,                          // prohibited operation
		securityViolation,                       // Security violation
		IOError,                                 // IO Error
		fileNotFound,
		linkError,                               // Unable to link method
		nullPointer,                             // Null Pointer argument
		notInstantiable                          // Attempt to instantiate an abstract class
	};

	const Cause cause;

	RuntimeError(Cause cause): cause(cause) {}
};

NS_EXTERN
void runtimeError(RuntimeError::Cause cause);


#endif
