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
// Win32ExceptionHandler.cpp
//
// Win32 Specific Exception Handling
//
// simon
// bjorn


#ifdef _WIN32

#include "Fundamentals.h"
#include "SysCalls.h"
#include "prprf.h"
#include "ExceptionTable.h"
#include "NativeCodeCache.h"
#include "LogModule.h"

#include "x86Win32ExceptionHandler.h"

// declarations
extern "C" SYSCALL_FUNC(void) x86SoftwareExceptionHandler(Uint32 EBP, Uint32 EDI, Uint32 ESI, Uint32 EBX, const JavaObject& inObject);
extern "C" SYSCALL_FUNC(JavaObject&) x86NewExceptionInstance(StandardClass classKind);


//--------------------------------------------------------------------------------
// set up the stack with the saved registers
__declspec(naked) SYSCALL_FUNC(void) sysThrow (const JavaObject& inObject)
{
	_asm
	{
		// make a new stack frame
		push	ebp
		mov		ebp,esp

		// marshall arguments
		push	[ebp + 8]	// inObject
		push	ebx
		push	esi
		push	edi
		push	ebp

		call	x86SoftwareExceptionHandler
		// never returns
	}
}

Uint8* findExceptionHandler(Context* context);

// x86JumpToHandler
// In:	the current frame context
extern "C"
__declspec(naked) SYSCALL_FUNC(void) x86JumpToHandler(const JavaObject* inObject, Uint8* handler, Uint32 EBX, Uint32 ESI, Uint32 EDI, Uint32* EBP, Uint32* ESP)
{
	_asm
	{
/*
	restore when regalloc is fixed

		pop		eax 	// return address
		pop		eax 	// argument for the catch handler
		pop		ecx 	// handler
		pop		ebx
		pop		esi
		pop		edi
		pop		ebp
		pop		esp
		jmp		ecx
*/
		// FIX (hack to work around reg alloc bug)
		pop		eax 	// return address
		pop		ecx 	// argument for the catch handler
		pop		eax 	// handler
		pop		ebx
		pop		esi
		pop		edi
		pop		ebp
		pop		esp
		jmp		eax
	}
}

//--------------------------------------------------------------------------------
__declspec(naked) SYSCALL_FUNC(void) sysThrowNullPointerException()
{
	_asm
	 {
		 push		cNullPointerException		// Push the kind of class
		 call		x86NewExceptionInstance		// Create a new object
		 pop		ecx
		 push		eax
		 push		ecx
		 jmp		sysThrow
	 }
}

__declspec(naked) SYSCALL_FUNC(void) sysThrowClassCastException()
{
	_asm
	 {
		 push		cClassCastException		    // Push the kind of class
		 call		x86NewExceptionInstance		// Create a new object
		 pop		ecx
		 push		eax
		 push		ecx
		 jmp		sysThrow
	 }
}

__declspec(naked) SYSCALL_FUNC(void) sysThrowArrayIndexOutOfBoundsException()
{
	_asm
	 {
		 push		cArrayIndexOutOfBoundsException	// Push the kind of class
		 call		x86NewExceptionInstance		// Create a new object
		 pop		ecx
		 push		eax
		 push		ecx
		 jmp		sysThrow
	 }
}

__declspec(naked) SYSCALL_FUNC(void) sysThrowArrayStoreException()
{
	_asm
	 {
		 push		cArrayStoreException	// Push the kind of class
		 call		x86NewExceptionInstance		// Create a new object
		 pop		ecx
		 push		eax
		 push		ecx
		 jmp		sysThrow
	 }
}

//--------------------------------------------------------------------------------
// Function:	win32SetHandler
// In:			the current frame context
void win32SetHandler(Context* context, Uint8* handler, struct _CONTEXT *ContextRecord)
{
/*
	// restore when regalloc is fixed

	ContextRecord->Eip = (DWORD)handler;
	ContextRecord->Ebx = (DWORD)context->EBX;
	ContextRecord->Esi = (DWORD)context->ESI;
	ContextRecord->Edi = (DWORD)context->EDI;
	ContextRecord->Esp = (DWORD)context->restoreESP;
	ContextRecord->Eax = (DWORD)context->object;
	ContextRecord->Ebp = (DWORD)context->sourceEBP;
*/
	// FIX (hack to work around reg alloc bug)
	ContextRecord->Eip = (DWORD)handler;
	ContextRecord->Ebx = (DWORD)context->EBX;
	ContextRecord->Esi = (DWORD)context->ESI;
	ContextRecord->Edi = (DWORD)context->EDI;
	ContextRecord->Esp = (DWORD)context->restoreESP;
	ContextRecord->Ecx = (DWORD)context->object;
	ContextRecord->Ebp = (DWORD)context->sourceEBP;
}

//--------------------------------------------------------------------------------
// Hardware Exception Handling

// Stack:
/*
								+-------------------------------+
								|		return address			|
						========+===============================+========
		Source					|		EBP link				|	<- ContextRecord.Ebp
		(JITed Java)			+-------------------------------+
								|		Local Store				|
								+-------------------------------+
								|		Saved Non-volatiles		|
								|		eg.	EDI					|
								|			ESI					|
								|			EBX					|
		faulting instruction	+-------------------------------+	<- ContextRecord.Esp
		(= ContextRecord.Eip)
*/

void win32HardwareExceptionHandler(JavaObject* exc, CONTEXT* ContextRecord)
{
	// create a context object on the stack, and save registers
	Context context;	

	context.EBX = ContextRecord->Ebx;
	context.ESI = ContextRecord->Esi;
	context.EDI = ContextRecord->Edi;

	context.object = exc;

	// get top EBP
	context.sucessorEBP = NULL;
	context.sourceEBP = (Uint32*)ContextRecord->Ebp;
	context.sourceAddress = (Uint8*)ContextRecord->Eip;

	// cache entries
	context.sourceCE = NativeCodeCache::getCache().lookupByRange((Uint8*)context.sourceAddress);	// sourceCE could be == NULL
	context.successorCE = NULL;			// no information yet

	// calculate the ESP if we were to proceed directly to the catch
	context.restoreESP = reinterpret_cast<Uint32*>(ContextRecord->Esp);

	// call the exception handler
	Uint8* handler = findExceptionHandler(&context);

	// Assuming that we received an exception that was not a breakpoint.
	// Then it matters if the current thread has been stopped (indicated
	// by ec, i.e. if ec == NULL then the normal case applies.
	// If stopped, we must free the current code copy, and make a new
	// copy (asynchronous one) of the catching function (pHandler).
	win32SetHandler(&context, handler, ContextRecord);
}

// Breakpoint and hardware exceptions
// 1. install an exception handler, which determines the fault and 
// invokes the handler lookup (equivalent to throw()). The handler
// should be installed when the thread is created (see Monitor::run()).
// 2. conditioning some exceptions, e.g. breakpoints, which under normal
// circumstances should be caught by the debugger, but when used for asynchronous
// exceptions, should be dealt with differently.

// This function is used to print the arguments to the assert function of the
// harness code. For example, if there is a call from the test-suite such as:
// QT.assert(b, "A2");
// the function below will print to standard output the value of 'b' and the string
// "A2", *if* sajava was run with:
// -be suntest/quicktest/runtime/QuickTest assert (ZLjava/lang/String;)V 
// It is a kludge, but it is useful while debugging since the printout helps you
// navigate through the source code of the test.

#ifdef DEBUG_LOG
void
printAssertFrame(Int32* esp) 
{
	PR_fprintf(PR_STDOUT,"Arg 1: %x\n",*(esp+2));
	PR_fprintf(PR_STDOUT,"Arg 2: %x\n",*(esp+3));
	JavaString* js = (JavaString*)*(esp+3);
	js->dump();
}
#endif // DEBUG_LOG

#if LANG_TURKISH != 0x1f
#error Turkey has fucked us.
#endif

EXCEPTION_DISPOSITION __cdecl
win32HardwareThrow(	EXCEPTION_RECORD*	ExceptionRecord,
					void*				EstablisherFrame,
					CONTEXT*			ContextRecord,
					void*				DispatcherContext)
{	
	Uint8*ip=NULL;

	// In Win32, an exception handler is called twice, when the exception wasn't handled.
	// The flags are set the second time around, hence, by checking them we know if we
	// should simply ignore the call or not.
	if (ExceptionRecord->ExceptionFlags & LANG_TURKISH) 
		return ExceptionContinueSearch;

	switch (ExceptionRecord->ExceptionCode) 
	{
	case STATUS_INTEGER_DIVIDE_BY_ZERO:
		win32HardwareExceptionHandler(&x86NewExceptionInstance(cArithmeticException),ContextRecord);
		return ExceptionContinueExecution;

	case STATUS_BREAKPOINT:
#ifdef DEBUG
// Use this instead when checking asserts.
//		printAssertFrame((Int32*)ContextRecord->Esp);
//		ContextRecord->Eip++;
//		return ExceptionContinueExecution;
#endif
		return ExceptionContinueSearch;

	case STATUS_ILLEGAL_INSTRUCTION: // these are used for now to do asynchronous exceptions
		ip = (Uint8*)ContextRecord->Eip;
		ContextRecord->Eip = *(DWORD*)(ip+2);
		win32HardwareExceptionHandler((JavaObject*)*(DWORD*)(ip+6),ContextRecord);
		return ExceptionContinueExecution;

	case STATUS_ACCESS_VIOLATION:
		return ExceptionContinueSearch;

	default:
		PR_fprintf(PR_STDOUT,"I am not here am I %d??\n",ExceptionRecord->ExceptionCode);
		return ExceptionContinueSearch;
	}
}

// This fellow might be called if the the above handler doesn't catch the exception.
// The 'might' depends on whether the '-ce' option was set as a command line option.
// All it does is to exit the process.
EXCEPTION_DISPOSITION __cdecl
win32HardwareThrowExit(struct _EXCEPTION_RECORD *ExceptionRecord,
					void * EstablisherFrame,
					CONTEXT *ContextRecord,
					void * DispatcherContext)
{	
    PR_fprintf(PR_STDOUT,"STATUS:EXCEPTION.\n");
	PR_fprintf(PR_STDOUT,"Exception %d\n",ExceptionRecord->ExceptionCode);
	PR_fprintf(PR_STDOUT,"At 0x%x\n",ContextRecord->Eip);
	PR_fprintf(PR_STDOUT,"Exception flags %d\n",ExceptionRecord->ExceptionFlags);
	_exit(-1);
	return ExceptionContinueSearch; // never reached
}

#endif // _WIN32
