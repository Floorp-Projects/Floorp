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

#include "SysCalls.h"
#include "JavaObject.h"
#include <string.h>
#include <stdlib.h>


static const ValueKind kinds_a[] = {vkAddr};
static const ValueKind kinds_aa[] = {vkAddr, vkAddr};
static const ValueKind kinds_m[] = {vkMemory};
static const ValueKind kinds_ma[] = {vkMemory, vkAddr};
static const ValueKind kinds_mai[] = {vkMemory, vkAddr, vkInt};
static const ValueKind kinds_maa[] = {vkMemory, vkAddr, vkAddr};
static const ValueKind kinds_maii[] = {vkMemory, vkAddr, vkInt, vkInt};
static const ValueKind kinds_maiii[] = {vkMemory, vkAddr, vkInt, vkInt, vkInt};
static const ValueKind kinds_mi[] = {vkMemory, vkInt};

static const bool bools_t[] = {true};
static const bool bools_tt[] = {true, true};

// On x86 platforms we need a guard frame beneath every sys call that generates an exception.
// This is achieved by calling a stub method guarded##name. For now, we use a system dependent macro GUARD
// On platforms that need guard frames we have a stub with the name 'guarded##function', others directly call 'function' 
// This will be cleaned up when we bring up exceptions on another platform and understand the issues.
#if defined(_WIN32) // || defined(LINUX)
	// FIX -- make MD
	#include "x86SysCallsRuntime.h"	// declarations
	#define GUARD(function) guarded##function
#else
	#define GUARD(function) function
#endif

#ifdef DEBUG_LOG
#define InitSysCall(name, callNumber, operation, nInputs, nOutputs, inputKinds, outputKinds, outputsNonzero, exceptionClass, function)	\
	const SysCall sc##name = {callNumber, operation, nInputs, nOutputs, inputKinds, outputKinds, outputsNonzero, exceptionClass, function, #name}
#else
#define InitSysCall(name, callNumber, operation, nInputs, nOutputs, inputKinds, outputKinds, outputsNonzero, exceptionClass, function)	\
	const SysCall sc##name = {callNumber, operation, nInputs, nOutputs, inputKinds, outputKinds, outputsNonzero, exceptionClass, function}
#endif

InitSysCall(Throw, 1, poSysCallEC, 1, 0, kinds_a, 0, 0, cThrowable, sysThrow);	// needs no guard
InitSysCall(CheckArrayStore, 6, poSysCallEC, 2, 0, kinds_aa, 0, 0, cArrayStoreException, GUARD(sysCheckArrayStore));
InitSysCall(MonitorEnter, 7, poSysCallEV, 2, 1, kinds_ma, kinds_m, bools_t, cThrowable, GUARD(sysMonitorEnter));
InitSysCall(MonitorExit, 8, poSysCallEV, 2, 1, kinds_ma, kinds_m, bools_t, cIllegalMonitorStateException, GUARD(sysMonitorExit));
InitSysCall(New, 9, poSysCallEV, 2, 2, kinds_ma, kinds_ma, bools_tt, cThrowable, GUARD(sysNew));
InitSysCall(NewBooleanArray, 10, poSysCallEV, 2, 2, kinds_mi, kinds_ma, bools_tt, cThrowable, GUARD(sysNewBooleanArray));
InitSysCall(NewByteArray, 11, poSysCallEV, 2, 2, kinds_mi, kinds_ma, bools_tt, cThrowable, GUARD(sysNewByteArray));
InitSysCall(NewShortArray, 12, poSysCallEV, 2, 2, kinds_mi, kinds_ma, bools_tt, cThrowable, GUARD(sysNewShortArray));
InitSysCall(NewCharArray, 13, poSysCallEV, 2, 2, kinds_mi, kinds_ma, bools_tt, cThrowable, GUARD(sysNewCharArray));
InitSysCall(NewIntArray, 14, poSysCallEV, 2, 2, kinds_mi, kinds_ma, bools_tt, cThrowable, GUARD(sysNewIntArray));
InitSysCall(NewLongArray, 15, poSysCallEV, 2, 2, kinds_mi, kinds_ma, bools_tt, cThrowable, GUARD(sysNewLongArray));
InitSysCall(NewFloatArray, 16, poSysCallEV, 2, 2, kinds_mi, kinds_ma, bools_tt, cThrowable, GUARD(sysNewFloatArray));
InitSysCall(NewDoubleArray, 17, poSysCallEV, 2, 2, kinds_mi, kinds_ma, bools_tt, cThrowable, GUARD(sysNewDoubleArray));
InitSysCall(NewObjectArray, 18, poSysCallEV, 3, 2, kinds_mai, kinds_ma, bools_tt, cThrowable, GUARD(sysNewObjectArray));
InitSysCall(New2DArray, 19, poSysCallEV, 4, 2, kinds_maii, kinds_ma, bools_tt, cThrowable, GUARD(sysNew2DArray));
InitSysCall(New3DArray, 20, poSysCallEV, 5, 2, kinds_maiii, kinds_ma, bools_tt, cThrowable, GUARD(sysNew3DArray));
InitSysCall(NewNDArray, 21, poSysCallEV, 3, 2, kinds_maa, kinds_ma, bools_tt, cThrowable, GUARD(sysNewNDArray));
