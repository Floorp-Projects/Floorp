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
// x86Win32_Exception.cpp
//
// simon

#if defined(_WIN32) || defined(LINUX)

#ifndef _X86WIN32_EXCEPTION_H_
#define _X86WIN32_EXCEPTION_H_

#include <excpt.h>

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
	Uint32*	sucessorEBP;

	Uint8*	sourceAddress;

	Uint32*	restoreESP;		// where ESP should be set if returning back to this frame

	CacheEntry* sourceCE;
	CacheEntry* successorCE;

	JavaObject* object;
};

#ifndef LINUX
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

#endif // _X86WIN32_EXCEPTION_H_
#endif // _WIN32
