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
// bjorn

#if defined(_WIN32) || defined(LINUX)

#include "SysCalls.h"
#include "prprf.h"
#include "x86Win32ExceptionHandler.h"
#include "ExceptionTable.h"
#include "NativeCodeCache.h"
#include "LogModule.h"

UT_DEFINE_LOG_MODULE(Win32ExceptionHandler);
UT_DEFINE_LOG_MODULE(ExceptionRegs);

extern "C" SYSCALL_FUNC(void) x86JumpToHandler(const JavaObject* inObject, Uint8* handler, Uint32 EBX, Uint32 ESI, Uint32 EDI, Uint32* EBP, Uint32* ESP);
extern "C" SYSCALL_FUNC(void) x86SoftwareExceptionHandler(Uint32 EBP, Uint32 EDI, Uint32 ESI, Uint32 EBX, const JavaObject& inObject);

//--------------------------------------------------------------------------------
/*
								+-------------------------------+
								|		return address			|
						========+===============================+========
		Source					|		EBP link				|	<- sourceEBP
		(JITed Java)			+-------------------------------+
								|		Local Store				|
								+-------------------------------+
								|		Saved Non-volatiles		|
								|		eg.	EDI					|
								|			ESI					|
								|			EBX					|
								+-------------------------------+ 
								|		thrown object			|
								+-------------------------------+	<- initial restoreESP
								|		return address			|
						========+===============================+========
		sysThrow				|		EBP link				|	<- sucessorEBP
		(native)				+-------------------------------+
								|	    arguments				|
						========+===============================+========
  x86SoftwareExceptionHandler	|		EBP link				|
		(native)				+-------------------------------+
								|		Saved Non-volatiles		|
								|		(variable)				|
								+-------------------------------+
								|		'context' structure		|
								+-------------------------------+
*/

//--------------------------------------------------------------------------------
#ifdef DEBUG_LOG
void printRegs(LogModuleObject &f, Context* context);
void printExceptionBegin(LogModuleObject &f, Context* context);
#endif

#ifdef DEBUG
void checkForNativeStub(Context* context);
#endif

Uint8* findExceptionHandler(Context* context);

//--------------------------------------------------------------------------------
extern "C" SYSCALL_FUNC(JavaObject&) x86NewExceptionInstance(StandardClass classKind)
{
	assert(classKind == cNullPointerException || 
		   classKind == cArrayIndexOutOfBoundsException || 
		   classKind == cClassCastException ||
		   classKind == cArithmeticException);
	return const_cast<Class *>(&Standard::get(classKind))->newInstance();
}


//--------------------------------------------------------------------------------
// LINUX specific -- FIX make another file for this
#ifdef LINUX

extern "C"
{
	Uint32 exceptionClass[] = {
		cNullPointerException,
		cArrayIndexOutOfBoundsException,
		cClassCastException,
		cArrayStoreException,
	};
}

#endif

//--------------------------------------------------------------------------------
// Function:	x86SoftwareExceptionHandler
// 
// called by sysThrow
extern "C" SYSCALL_FUNC(void) x86SoftwareExceptionHandler(Uint32 inEBP, Uint32 inEDI, Uint32 inESI, Uint32 inEBX, const JavaObject& inObject)
{
	Context context;	// create a context object on the stack

	context.EDI = inEDI;
	context.ESI = inESI;
	context.EBX = inEBX;
	context.object = (JavaObject*) &inObject;

	// get other EBPs
	context.sucessorEBP = (Uint32*) inEBP;
	context.sourceEBP = *((Uint32**)context.sucessorEBP);

	// The source address will point to the instruction to return to, NOT the instruction that
	// generated the exception. The return address is often (always?) in a different control node, so 
	// it may have a different exception table.
	// The byte before the return address will be part of the call instruction that got us here.
	context.sourceAddress = (Uint8*)(context.sucessorEBP[1]) - 1;

	// cache entries
	context.sourceCE = NativeCodeCache::getCache().lookupByRange((Uint8*)context.sourceAddress);
	context.successorCE = NULL;			// no information yet

	// calculate the ESP if we were to proceed directly to the catch
	// EBP + 4 (return address) + 4 (argument) + 4 (point to last object on stack == saved regs)
	context.restoreESP = (Uint32*) ((Uint8*)context.sucessorEBP + 12);

	// now that the context struct is filled we can print debugging information
	DEBUG_LOG_ONLY(printExceptionBegin(UT_LOG_MODULE(Win32ExceptionHandler), &context));	

	// find the exception handler
	Uint8* handler = findExceptionHandler(&context);

	// jump to it
	UT_LOG(ExceptionRegs, PR_LOG_DEBUG, ("Exit:  "));
	DEBUG_LOG_ONLY(printRegs(UT_LOG_MODULE(ExceptionRegs), &context));
	UT_LOG(Win32ExceptionHandler, PR_LOG_DEBUG, ("\n===============================\n\n"));
	assert(handler);
	x86JumpToHandler(context.object, handler, context.EBX, context.ESI, context.EDI, context.sourceEBP, context.restoreESP);	
}

// Function:	x86PopStackFrame
// In:			the current frame context
// Out:			true if a frame was popped, and false if bottom of stack was reached
bool x86PopStackFrame(Context* context)
{
	assert(context);
	assert(context->sourceEBP);

	// check that we can unwind
	Uint32* newSourceEBP = *(Uint32**)(context->sourceEBP);

	// FIX for now when we get the EBP points to NULL we are at the bottom of the stack
	// As a temporary measure, die and display the error
	if(newSourceEBP == NULL) 
	{
		DEBUG_LOG_ONLY(PR_fprintf(PR_STDOUT,"\n*** Unhandled Exception Error: %s\n", context->object->getClass().getName()));
		return false;
	}

	// roll back to point to previous frame
	context->sucessorEBP = context->sourceEBP;
	context->sourceEBP = newSourceEBP;
	context->sourceAddress = (Uint8*)(context->sucessorEBP[1]) - 1; 	// move back a byte to ensure we're in correct control node

	context->successorCE = context->sourceCE;
	context->sourceCE = NativeCodeCache::getCache().lookupByRange(context->sourceAddress);

	// Restore Registers
	// The callee saves a set of registers EDI, ESI, EBX. To restore to the current 'source' frame we need
	// to restore the set of registers saved from our callee (the 'successor' frame).
	// 
	// We know nothing about the register save policies of native methods. To ensure proper register
	// restoration we include a dummy stack frame below all native frames that has stored all three
	// callee-save registers. 
	//
	// Whenever we see a native method we should restore the registers assuming that the first three locations
	// after EBP are the callee-save values. The first native method will probably load garbage into the registers
	// but the last fake frame that we have inserted will properly restore them all.
	// EBX, ESI and EDI are restored from the successor frame
	//
	// remember that sourceCE and successorCE can be null
	Uint32 numRegs;		// number of registers to restore
	Uint32* restoreReg;	// address of first restore location

	if(context->successorCE)
	{
		// successor is java frame; restore regs as per saved policy
		StackFrameInfo& successorPolicy = context->successorCE->policy;
		restoreReg = (Uint32*) ((Uint8*)(context->sucessorEBP) - successorPolicy.getCalleeSavedBeginOffset());
		numRegs = successorPolicy.getNumSavedGPRWords();
	}
	else
	{
		// successor is native frame; restore all regs from just below EBP
		restoreReg = (Uint32*) ((Uint8*)(context->sucessorEBP) - 4);
		numRegs = 3;
	}

	if(numRegs > 2)
		context->EDI = *restoreReg--;
	if(numRegs > 1)
		context->ESI = *restoreReg--;
	if(numRegs > 0)
		context->EBX = *restoreReg--;

	// Calculate the restore ESP
	if(context->sourceCE)
	{
		// source frame is java frame; restore ESP to correct position
		StackFrameInfo& sourcePolicy = context->sourceCE->policy;
		context->restoreESP = (Uint32*) ((Uint8*)(context->sourceEBP) - (Uint8*)(sourcePolicy.getStackPointerOffset()));
	}
	else
	{
		// source frame is native; cannot restore ESP, so we set it to NULL as a sentinel
		context->restoreESP = NULL;
	}

	DEBUG_LOG_ONLY(printContext(UT_LOG_MODULE(Win32ExceptionHandler), context));
	DEBUG_ONLY(checkForNativeStub(context));
	return true;
}

//--------------------------------------------------------------------------------
#ifdef DEBUG

extern void* compileStubReEntryPoint;

// Function:	isGuardFrame
// Purpose:		determines whether the source frame is a native method / syscall guard frame
bool isGuardFrame(Context* context, bool printIdentifier = false)
{
	Uint32 retAddress = (Uint32)(context->sucessorEBP[1]);
	int i;

	// check unrolled-loop stubs for return address
	extern void* sysInvokeNativeStubs[1024];
	for(i = 0; i < 6; i++)
		if( ((Uint32)(sysInvokeNativeStubs[i]) + 9 + 5 * i) == retAddress)
		{
			if(printIdentifier)
				UT_LOG(Win32ExceptionHandler, PR_LOG_DEBUG, ("Guard Frame (made by sysInvokeNative%d())\n", i));
			return true;
		}
	// check other stubs
	for(i = 6; i < 256; i++)
		if( ((Uint32)(sysInvokeNativeStubs[i]) + 23) == retAddress)
		{
			if(printIdentifier)
				UT_LOG(Win32ExceptionHandler, PR_LOG_DEBUG, ("Guard Frame (made by sysInvokeNative(%d))\n", i));
			return true;
		}

	// check for staticCompileStub
	if((Uint32)compileStubReEntryPoint == retAddress)
	{
		if(printIdentifier)
			UT_LOG(Win32ExceptionHandler, PR_LOG_DEBUG, ("Guard Frame (staticCompileStub)\n"));
		DEBUG_LOG_ONLY(PR_fprintf(PR_STDOUT,"WARNING: exception unwinding past a static compile stub\n"));
		return true;
	}

	// if we are here then we are not a guard frame
	return false;
}

#include "StackWalker.h"

// Function:	checkForNativeStub
// Purpose:		if the current frame is a non-Java frame called by a Java method, check that
//				the frame is a guard frame
void checkForNativeStub(Context* context)
{
	// checks
	// take a peek back at the previous frame
	Uint8* prevAddress = (Uint8*)(context->sourceEBP[1]) - 1;
	assert(prevAddress);
	CacheEntry* prevCacheEntry = NativeCodeCache::getCache().lookupByRange(prevAddress);

	bool sourceIsJITCode = context->sourceCE && context->sourceCE->eTable;
	bool previousIsJITCode = prevCacheEntry && prevCacheEntry->eTable;

	if((!sourceIsJITCode && previousIsJITCode) && !isGuardFrame(context))
	{
		// this is not a GUARD-JAVA interface, but the guard was missing
		stackWalker((void*) 0);
		trespass("\nFATAL ERROR: missing guard frame\n");
	}
}
#endif

#ifdef DEBUG_LOG

// Function:	printExceptionBegin
// Purpose:		call this when the context record has been filled to print out the origin and type of exception
void printExceptionBegin(LogModuleObject &f, Context* context)
{
	// print debugging information
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("\n===============================\nEXCEPTION\n"));
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("Source: "));
	if(context->sourceCE)
	{
		const char* methodName = context->sourceCE->descriptor.method->getName();
		const char* className = context->sourceCE->descriptor.method->getDeclaringClass()->getName();
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("java method '%s'\n", methodName));
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("Class:   %s\n", className));
	}
	else
	{
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("a syscall or native method\n"));
	}
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("Object: '%s' (object = 0x%p)\n", context->object->getClass().getName(), context->object));
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("Return: 0x%p\n", context->sucessorEBP[1]));
	UT_OBJECTLOG(UT_LOG_MODULE(ExceptionRegs), PR_LOG_ALWAYS, ("Entry: "));
	printRegs(UT_LOG_MODULE(ExceptionRegs), context);
}

// Function:	printRegs
// Purpose:		print the state of the registers being restored by the exception handler
void printRegs(LogModuleObject &f, Context* context)
{
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("EBX %8x    ESI %8x    EDI %8x\n", context->EBX, context->ESI, context->EDI));
}

// Function:	printContext
// Purpose:		dump the state of the exception handler context to the log module
void printContext(LogModuleObject &f, Context* context)
{
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("\n-------------------------------\n"));

	// print the method
	if(context->sourceCE)
	{
		const char* methodName = context->sourceCE->descriptor.method->getName();
		const char* className = context->sourceCE->descriptor.method->getDeclaringClass()->getName();
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("Method: %s\n", methodName));
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("Class:  %s\n", className));

		StackFrameInfo& policy = context->sourceCE->policy;
		policy.print(f);
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("Regs:   "));
		printRegs(f, context);

		ExceptionTable* eTable = context->sourceCE->eTable;
		if(eTable && eTable->getNumberofEntries() > 0)
		{
			UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("Exception Table\n"));
			eTable->printShort(f);
		}
	}
	else if(!isGuardFrame(context, true))	// isGuardFrame(..., true) prints reference if guard frame found
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("Method: anonymous\n"));

	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("Return: 0x%p\n", context->sucessorEBP[1]));
}
#endif // DEBUG_LOG

//--------------------------------------------------------------------------------
// this function assumes that the source frame was set up by Thread::invoke(), and
// that it should look like:
/*
	             +-----------------------+
				 |    old EBP            |   <- real sourceEBP
				 |-----------------------|
				 | &uncaughtException    |
				 |-----------------------|
				 |    NULL               |   <- sourceEBP as on entry to uncaughtExitPoint
				 |-----------------------|
				 | 4 arguments to invoke |
				 |      ...              |
				 |-----------------------|
				 |   return address      |   <- restoreESP
				 |-----------------------|
 */
static Uint8* uncaughtExceptionExit(Context* context) 
{
	Uint8* ip = *(Uint8**)(context->sourceEBP+1);				// uncaughtException()
	context->restoreESP = context->sourceEBP-5;					// 4 arguments plus the return address
	context->sourceEBP = *(Uint32**)(context->sourceEBP+2);		// old ebp
	assert(ip);
	return ip;
}

//--------------------------------------------------------------------------------
//	findExceptionHandler
//
Uint8* findExceptionHandler(Context* context)
{
	// look for a handler for the object
	// unwind repeatedly until found
	ExceptionTableEntry* ete;

	while (1)
	{ 
		// every jit'd method has an eTable
		// native frames don't have a sourceCE
		if(context->sourceCE)
		{
			// jit frame
			assert(context->sourceCE->eTable);
			ete = context->sourceCE->eTable->findCatchHandler(context->sourceAddress, *(context->object));
		}
		else
		{
			// native frame
			ete = NULL;
		}
	
		if (ete)		// ete != null iff there has been an exception match
			break;

		if (!x86PopStackFrame(context)) // when popStack() fails, we are faced with an uncaught exception
		{
			Uint8* uncaughtHandler = uncaughtExceptionExit(context);
			UT_LOG(Win32ExceptionHandler, PR_LOG_DEBUG, ("\n-------------------------------\n"));
			UT_LOG(Win32ExceptionHandler, PR_LOG_DEBUG, ("Uncaught Exception -- jumping to handler at 0x%p\n", uncaughtHandler));
			return uncaughtHandler;
		}
	}

	Uint8* pHandler = context->sourceCE->eTable->getStart()+ete->pHandler;
	assert(pHandler);
	UT_LOG(Win32ExceptionHandler, PR_LOG_DEBUG, ("\n-------------------------------\n"));
	UT_LOG(Win32ExceptionHandler, PR_LOG_DEBUG, ("Matched to catch handler at %p\n\n", pHandler));
	assert(context->restoreESP != NULL);	// restore ESP must not be null, could mean that we are trying to return to a native method, NYI
	return pHandler;
}

//--------------------------------------------------------------------------------
#endif	// _WIN32 || LINUX
