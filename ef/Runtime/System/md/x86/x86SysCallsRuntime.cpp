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
// x86SysCallsRuntime.cpp
//
// simon

#include "JavaObject.h"
#include "SysCallsRuntime.h"

extern void* sysInvokeNativeStubs[1024];

// Assembly glue macro
#if defined(_WIN32)

#define guardedSysCall(target, numArgWords)						\
__declspec(naked) SYSCALL_FUNC(void) guarded##target()			\
{																\
	__asm														\
	{															\
		__asm push	OFFSET target								\
		__asm jmp	[sysInvokeNativeStubs + 4 * numArgWords]	\
	}															\
}

#else
//#error You need to make some inline assembly equivalent to the above
#define guardedSysCall(target, numArgWords) 
#endif

extern "C" {

// Make Guard Stubs for the all SysCalls that can throw exceptions
// NB: SysThrow is already guarded
// form: guardedSysCall(actual destination, number of words for parameters)
guardedSysCall(sysCheckArrayStore, 2)

guardedSysCall(sysMonitorEnter, 1)
guardedSysCall(sysMonitorExit, 1)

guardedSysCall(sysNewNDArray, 2)
guardedSysCall(sysNew2DArray, 3)
guardedSysCall(sysNew3DArray, 4)

guardedSysCall(sysNew, 1)

guardedSysCall(sysNewByteArray, 1)
guardedSysCall(sysNewBooleanArray, 1)
guardedSysCall(sysNewShortArray, 1)
guardedSysCall(sysNewCharArray, 1)
guardedSysCall(sysNewIntArray, 1)
guardedSysCall(sysNewLongArray, 1)
guardedSysCall(sysNewFloatArray, 1)
guardedSysCall(sysNewDoubleArray, 1)
guardedSysCall(sysNewObjectArray, 2)

}
