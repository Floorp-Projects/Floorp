/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
//
// x86SysCallsRuntime.h
//
// simon

#ifndef _X86_SYSCALLS_RUNTIME_H_
#define _X86_SYSCALLS_RUNTIME_H_


#include "Fundamentals.h"
#include "SysCallsRuntime.h"

extern "C" 
{
	NS_EXTERN SYSCALL_FUNC(void) guardedsysCheckArrayStore();

	NS_EXTERN SYSCALL_FUNC(void) guardedsysMonitorEnter();
	NS_EXTERN SYSCALL_FUNC(void) guardedsysMonitorExit();

	NS_EXTERN SYSCALL_FUNC(void) guardedsysNew();

	NS_EXTERN SYSCALL_FUNC(void) guardedsysNewNDArray();
	NS_EXTERN SYSCALL_FUNC(void) guardedsysNew3DArray();
	NS_EXTERN SYSCALL_FUNC(void) guardedsysNew2DArray();

	NS_EXTERN SYSCALL_FUNC(void) guardedsysNewByteArray();
	NS_EXTERN SYSCALL_FUNC(void) guardedsysNewCharArray();
	NS_EXTERN SYSCALL_FUNC(void) guardedsysNewIntArray();
	NS_EXTERN SYSCALL_FUNC(void) guardedsysNewLongArray();
	NS_EXTERN SYSCALL_FUNC(void) guardedsysNewShortArray();
	NS_EXTERN SYSCALL_FUNC(void) guardedsysNewBooleanArray();
	NS_EXTERN SYSCALL_FUNC(void) guardedsysNewDoubleArray();
	NS_EXTERN SYSCALL_FUNC(void) guardedsysNewFloatArray();
	NS_EXTERN SYSCALL_FUNC(void) guardedsysNewObjectArray();
}

#endif // _X86_SYSCALLS_RUNTIME_H_
