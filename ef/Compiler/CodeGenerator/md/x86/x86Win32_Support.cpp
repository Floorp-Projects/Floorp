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
// File:	x86Win32_support.cpp
//
// Authors:	Peter DeSantis
//			Simon Holmes a Court
//

#include "NativeCodeCache.h"
#include <string.h>
#include "Fundamentals.h"
#include "MemoryAccess.h"


void* JNIenv = 0; 

extern ClassWorld world;

#define Naked   __declspec( naked )

/*
								+-------------------------------+
								|		return address			|
						========+===============================+========
								|		EBP link				|	
								+-------------------------------+
								|		Saved Non-volatiles		|
								|		eg.	EDI					|
								|			ESI					|
								|			EBX					|
								+-------------------------------+
*/

// Fucntion:	staticCompileStub
//
// WARNING: if you change this method, you must change compileStubReEntryPoint below.
// It must point to the instruction after the invokation of compileAndBackPatchMethod
static Naked void staticCompileStub()
{
	_asm 
	{	
		// remove cache entry from the stack
		pop		eax						

		// make frame
		push	ebp
        mov		ebp,esp
		
		// save all volatiles (especially for exception handler)
		push    edi						
        push    esi
        push    ebx

		// call compileAndBackPatchMethod with args
		// third argument is not used
		push	[esp + 16]				// second argument -- return address
		push    eax						// first argument -- cacheEntry
		call	compileAndBackPatchMethod

		// remove  args
		pop		edx						// <--- compileStubReEntryPoint
		pop		edx

		pop		ebx						// Restore volatiles
		pop		esi
		pop		edi

		// remove frame
		mov     esp,ebp
		pop     ebp

		// jump to the compiled method
		push	eax						// ret will jump to this address
		ret								// Jump to function leaving the return address at the top of the stack	
	}	
}

#ifdef DEBUG
// Pointer to the instruction after the call (used by exception handler to check
// I wanted to use:
//		void* compileStubReEntryPoint =  (void*) ((Uint8*)staticCompileStub + 17);
// but MSDev appears to have a bug, in that compileStubReEntryPoint will be set == (void*)staticCompileStub
// which is clearly wrong.
void* compileStubAddress = (void*)staticCompileStub;
void* compileStubReEntryPoint = (Uint8*)compileStubAddress + 17;
#endif // DEBUG

static Naked void compileStub()
{
	_asm {
		push	0xEFBEADDE				// This is a dummy immediate that will be filled in by	
		jmp		staticCompileStub		// generateCompileStub with the cacheEntry.
	}	
}

void *
generateNativeStub(NativeCodeCache& inCache, const CacheEntry& inCacheEntry, void *nativeFunction)
{
	Method* method = inCacheEntry.descriptor.method;
	//Uint32 nWords = method->getSignature().nArguments;
	Uint32 nWords = method->getArgsSize()/sizeof(Int32);

	assert(method->getModifiers() & CR_METHOD_NATIVE);
	assert(nWords <= 256);

	extern void *sysInvokeNativeStubs[];
    Uint8 stubSize = 10;
    void* stub;
		
	// Write out the native stub
	stub = inCache.acquireMemory(stubSize);
	Uint8* where = (Uint8*)stub;
	*where++ = 0x68; // pushl
	writeLittleWordUnaligned(where, (uint32)(nativeFunction));
	where += 4;
	*where++ = 0xe9; // jmp
	writeLittleWordUnaligned(where, (Uint8 *) sysInvokeNativeStubs[nWords] - (where + 4));

	// Return the address of the stub.
	return ((void*)stub);
}

void* 
generateCompileStub(NativeCodeCache& inCache, const CacheEntry& inCacheEntry)
{	
	void*	stub;
	uint8 stubSize;	
	uint8 argumentOffset;
	uint32 locationOfCompileStub;
	
	stubSize = 10;
		
	// Write out the dynamic compile stub
	stub = inCache.acquireMemory(stubSize);
	argumentOffset = 1;
	locationOfCompileStub = (uint32)compileStub ;

	// Copy the stub into the allocated memory
	memcpy(stub, (void*)locationOfCompileStub, stubSize);

	// Write your cacheEntry into the proper spot in the stub
	uint8* loadCacheEntryInstruction = (uint8*)stub + argumentOffset;
	writeLittleWordUnaligned((void*)loadCacheEntryInstruction, (uint32)(&inCacheEntry));	

	// Fix the new dynamic stub to jump to the static stub
	uint32* relativeCallLocation = (uint32*)(loadCacheEntryInstruction + 5);
	uint32 newRelativeDisplacement = locationOfCompileStub -  (uint32)stub + *(uint32*)relativeCallLocation;
	writeLittleWordUnaligned((void*)relativeCallLocation, newRelativeDisplacement);

	// Return the address of the dynamic stub.
	return ((void*)stub);
}

void*
backPatchMethod(void* inMethodAddress, void* inLastPC, void* /*inUserDefined*/)
{
		
	uint32 curAddress = (uint32) inLastPC;
	uint32 methodAddress = (uint32) inMethodAddress;
	
	// Compute the relative branch
	uint32*	relativeBranch = ((uint32*)inLastPC)-1;
	int32 offset = methodAddress - curAddress;

	// Backpatch the method.
	writeLittleWordUnaligned((void*)relativeBranch, offset);

	return (inMethodAddress);
}

// Warning silencing stuff
// #pragma warning( disable : 4035)
// #pragma warning( default : 4035)


//================================================================================
// 64bit Arithmetic Support Functions

// x86Extract64Bit
//
// Purpose:	signed right-aligned field extraction
// In:		64 bit source (on  stack)
//			32 bit extraction size (on stack)
// Out:		64 bit result
// Note:	Only works in range 1 <= b <= 63, b is extraction amount
Naked void x86Extract64Bit()
{
	__asm
	{
		mov eax, [esp+4]		// load low byte of a

		mov ecx, [esp+12]		// load shift amount
		cmp ecx, 0x20
		jg	greater32

		// extract <= than 32 bits
		// shift amount = 32 - extract
		neg	ecx
		add ecx, 0x20			// ecx = 32 - extract
		shl eax, cl
		sar eax, cl
		cdq						// sign extend into EDX:EAX
		ret 12
		
greater32:
		// ext > 32 bits
		// shift amount = 64 - extract
		mov edx, [esp+8]		// load high byte of a
		neg	ecx
		add ecx, 0x40			// ecx = 64 - extract
		shl edx, cl
		sar edx, cl
		ret 12
	}
}

// 3WayCompare
//
// Purpose:	compare two longs
// In:		two longs on the stack
// Out:		depends on condition flags:
//				less = -1
//				equal = 0
//				greater = 1
Naked void x86ThreeWayCMP_L()
{
	// edx:eax is tos, ecx:ebx is nos
	__asm
	{
		mov	ecx,[esp+8]
		mov	edx,[esp+16]

		cmp	ecx,edx
		jl	lcmp_m1

		jg	lcmp_1

		mov	ecx,[esp+4]
		mov	edx,[esp+12]

		cmp	ecx,edx
		ja	lcmp_1

		mov	eax,0
		jb	lcmp_m1

		ret	16

		align	4
	lcmp_m1:
		mov	eax,-1

		ret	16

		align	4
	lcmp_1:
		mov	eax,1

		ret	16
	}
}

// 3WayCompare
//
// Purpose:	compare two longs
// In:		two longs on the stack
// Out:		depends on condition flags:
//				less    =  1
//				equal   =  0
//				greater = -1
Naked void x86ThreeWayCMPC_L()
{
	// edx:eax is tos, ecx:ebx is nos
	__asm
	{
		mov	ecx,[esp+8]
		mov	edx,[esp+16]

		cmp	ecx,edx
		jl	lcmp_m1

		jg	lcmp_1

		mov	ecx,[esp+4]
		mov	edx,[esp+12]

		cmp	ecx,edx
		ja	lcmp_1

		mov	eax,0
		jb	lcmp_m1

		ret	16

		align	4
	lcmp_m1:
		mov	eax,1

		ret	16

		align	4
	lcmp_1:
		mov	eax,-1

		ret	16
	}
}

// llmul
//
// Purpose: long multiply (same for signed/unsigned)
// In:		args are passed on the stack:
//				1st pushed: multiplier (QWORD)
//				2nd pushed: multiplicand (QWORD)
// Out:		EDX:EAX - product of multiplier and multiplicand
// Note:	parameters are removed from the stack
// Uses:	ECX
Naked void x86Mul64Bit()
{
    // IMPLEMENT: Needs to be written
	_asm 
	{
        int     3
	}	
}

// lldiv
//
// Purpose: signed long divide
// In:		args are passed on the stack:
//				1st pushed: divisor (QWORD)
//				2nd pushed: dividend (QWORD)
// Out:		EDX:EAX contains the quotient (dividend/divisor)
// Note:	parameters are removed from the stack
// Uses:	ECX
Naked void x86Div64Bit()
{
    // IMPLEMENT: Needs to be written
	_asm 
	{
        int     3
	}
}

// llrem
//
// Purpose: signed long remainder
// In:		args are passed on the stack:
//				1st pushed: divisor (QWORD)
//				2nd pushed: dividend (QWORD)
// Out:		EDX:EAX contains the quotient (dividend/divisor)
// Note:	parameters are removed from the stack
// Uses:	ECX
Naked void x86Mod64Bit()
{
    // IMPLEMENT: Needs to be written
	_asm 
	{
        int     3
	}
}

// llshl
//
// Purpose: long shift left
// In:		args are passed on the stack: (FIX make fastcall) 
//				1st pushed: amount (int)
//				2nd pushed: source (long)
// Out:		EDX:EAX contains the result
// Note:	parameters are removed from the stack
// Uses:	ECX, destroyed
Naked void x86Shl64Bit()
{
    // IMPLEMENT: Needs to be written
	_asm 
	{
        int     3
	}
}

// llshr
//
// Origin:	MSDev. modified
// Purpose: long shift right
// In:		args are passed on the stack: (FIX make fastcall) 
//				1st pushed: amount (int)
//				2nd pushed: source (long)
// Out:		EDX:EAX contains the result
// Note:	parameters are removed from the stack
// Uses:	ECX, destroyed
Naked void x86Shr64Bit()
{
    // IMPLEMENT: Needs to be written
	_asm 
	{
        int     3
	}
}

// llsar
//
// Origin:	MSDev. modified
// Purpose: long shift right signed
// In:		args are passed on the stack: (FIX make fastcall) 
//				1st pushed: amount (int)
//				2nd pushed: source (long)
// Out:		EDX:EAX contains the result
// Note:	parameters are removed from the stack
// Uses:	ECX, destroyed
Naked void x86Sar64Bit()
{
    // IMPLEMENT: Needs to be written
	_asm 
	{
        int     3
	}
}

//================================================================================
