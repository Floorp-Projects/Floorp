/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
// File:    x86Win32_support.cpp
//
// Authors: Peter DeSantis
//          Simon Holmes a Court
//

#include "NativeCodeCache.h"
#include <string.h>
#include "Fundamentals.h"
#include "MemoryAccess.h"


void* JNIenv = 0;

extern ClassWorld world;

static void __declspec( naked )
staticCompileStub()
{
  _asm
    {
      // eax contains the cache entry.

      // make frame
      push    ebp
      mov     ebp,esp

      /* Even though ESI and EDI are non-volatile (callee-saved) registers, we need to
         preserve them here in case an exception is thrown.  The exception-handling
         code expects to encounter this specially-prepared stack "guard" frame when
         unwinding the stack.  See x86ExceptionHandler.cpp. */
      push    edi
      push    esi
      push    ebx

      // call compileAndBackPatchMethod with args
      // third argument is not used
      push    [esp + 16]              // second argument -- return address
      push    eax                     // first argument -- cacheEntry
      call    compileAndBackPatchMethod

      // remove frame
      mov     esp,ebp
      pop     ebp

      // jump to the compiled method
      jmp     eax
    }
}

#ifdef DEBUG
// Pointer to the instruction after the call (used by exception handler to check
// I wanted to use:
//		void* compileStubReEntryPoint =  (void*) ((Uint8*)staticCompileStub + 17);
// but MSDev appears to have a bug, in that compileStubReEntryPoint will be set == (void*)staticCompileStub
// which is clearly wrong.
void* compileStubAddress = (void*)staticCompileStub;
// void* compileStubReEntryPoint = (Uint8*)compileStubAddress + 15; // Correct address ?
void* compileStubReEntryPoint = NULL;
#endif // DEBUG

void *
generateNativeStub(NativeCodeCache& inCache, const CacheEntry& inCacheEntry, void *nativeFunction)
{
    Method* method = inCacheEntry.descriptor.method;
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
    void* stub;
    Uint8* where;

    Uint8 stubSize = 10;

    // Write out the dynamic compile stub
    stub = inCache.acquireMemory(stubSize);
    where = (Uint8*)stub;

    // movl $inCacheEntry, %eax
    *where++ = 0xb8;
    writeLittleWordUnaligned(where, (uint32)&inCacheEntry);
    where += 4;

    // jmp compileStub
	*where++ = 0xe9; // jmp
    writeLittleWordUnaligned(where, (Uint8 *) staticCompileStub - (where + 4));

    // Return the address of the dynamic stub.
	return ((void*)stub);
}

void*
backPatchMethod(void* inMethodAddress, void* inLastPC, void* /*inUserDefined*/)
{

    uint32 curAddress = (uint32) inLastPC;
    uint32 methodAddress = (uint32) inMethodAddress;

    // Compute the relative branch
    uint32* relativeBranch = ((uint32*)inLastPC)-1;
    int32 offset = methodAddress - curAddress;

    // Backpatch the method.
    writeLittleWordUnaligned((void*)relativeBranch, offset);

    return (inMethodAddress);
}
