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
#ifndef SYSCALLS_H
#define SYSCALLS_H

#include "Primitives.h"
#include "SysCallsRuntime.h"

struct SysCall
{
	uint callNumber;							// Unique number identifying this call type
	PrimitiveOperation operation;				// The PrimitiveOperation to use for instances of this system call
	uint nInputs;								// Number of DataConsumers in this system call
	uint nOutputs;								// Number of outgoing data edges in this system call
	const ValueKind *inputKinds;				// Kinds of DataConsumers in this system call
	const ValueKind *outputKinds;				// Kinds of outgoing data edges in this system call
	const bool *outputsNonzero;					// Array of flags that state whether the outgoing data edges are always nonzero
	StandardClass exceptionClass;				// Superclass of all possible classes of exceptions thrown by this system call
												// or cNone if this system call cannot throw any exceptions
	void *function;								// Function which implements this system call

  #ifdef DEBUG_LOG
	const char *name;							// Name of this call
  #endif

	bool hasMemoryOutput() const {return operation == poSysCallV || operation == poSysCallEV;}
	bool hasMemoryInput() const {return operation == poSysCall || operation == poSysCallE || hasMemoryOutput();}
};

extern const SysCall scThrow;
extern const SysCall scCheckArrayStore;
extern const SysCall scMonitorEnter;
extern const SysCall scMonitorExit;
extern const SysCall scNew;
extern const SysCall scNewBooleanArray;
extern const SysCall scNewByteArray;
extern const SysCall scNewShortArray;
extern const SysCall scNewCharArray;
extern const SysCall scNewIntArray;
extern const SysCall scNewLongArray;
extern const SysCall scNewFloatArray;
extern const SysCall scNewDoubleArray;
extern const SysCall scNewObjectArray;
extern const SysCall scNew2DArray;
extern const SysCall scNew3DArray;
extern const SysCall scNewNDArray;

#endif
