/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
// File:    x86Win32_support.cpp
//
// Authors: Peter DeSantis
//          Simon Holmes a Court
//

#include "NativeCodeCache.h"
#include <string.h>
#include "Fundamentals.h"
#include "MemoryAccess.h"

PR_BEGIN_EXTERN_C
extern void staticCompileStub();
PR_END_EXTERN_C

void* JNIenv = 0;

extern ClassWorld world;

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

#ifdef DEBUG
// Pointer to the instruction after the call (used by exception handler to sanity-check stack)
// I wanted to use:
//		void* compileStubReEntryPoint =  (void*) ((Uint8*)staticCompileStub + xx);
// but MSDev appears to have a bug, in that compileStubReEntryPoint will be set == (void*)staticCompileStub
// which is clearly wrong.
void* compileStubAddress = (void*)staticCompileStub;
// void* compileStubReEntryPoint = (Uint8*)compileStubAddress + 15; // Correct address ?
void* compileStubReEntryPoint = (void*)NULL;
#endif // DEBUG
