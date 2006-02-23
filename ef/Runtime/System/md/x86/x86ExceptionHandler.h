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
// x86ExceptionHandler.h
//
// simon

#ifndef _X86EXCEPTIONHANDLER_H_
#define _X86EXCEPTIONHANDLER_H_

#if defined(_WIN32)
#include <excpt.h>
#endif

#include "SysCallsRuntime.h"
#include "Fundamentals.h"
#include "LogModule.h"

struct JavaObject;
struct CacheEntry;

extern void* sysInvokeNativeStubs[];


//--------------------------------------------------------------------------------
// Do not mess with ordering!
struct Context
{
	// save registers
	Uint32	EBX;
	Uint32	ESI;
	Uint32	EDI;

	Uint32*	sourceEBP;
	Uint32*	successorEBP;

	Uint8*	sourceAddress;

	Uint32*	restoreESP;		// where ESP should be set if returning back to this frame

	CacheEntry* sourceCE;
	CacheEntry* successorCE;

	JavaObject* object;
};

#if defined(_WIN32)
NS_EXTERN EXCEPTION_DISPOSITION win32HardwareThrow(struct _EXCEPTION_RECORD *ExceptionRecord,
					void * EstablisherFrame,
					struct _CONTEXT *ContextRecord,
					void * DispatcherContext);
NS_EXTERN EXCEPTION_DISPOSITION win32HardwareThrowExit(struct _EXCEPTION_RECORD *ExceptionRecord,
					void * EstablisherFrame,
					struct _CONTEXT *ContextRecord,
					void * DispatcherContext);
#endif

#ifdef DEBUG
void printContext(LogModuleObject &f, Context* context);
#endif // DEBUG


//--------------------------------------------------------------------------------

#endif // _X86EXCEPTIONHANDLER_H_
