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
#include "NativeMethodStubs.h"
#include "NativeCodeCache.h"
#include "MemoryAccess.h"
#include "jni.h"

extern JNIEnv *jniEnv;
extern void throwJNIExceptionIfNeeded();

// XXX This will eventually have to be done on a per-thread basis!!
static Uint32 JNIReturnAddressInteger = 0;
static void *JNIReturnAddress = &JNIReturnAddressInteger;

// Stub sizes
static const JNIStaticStubSize = 34;
static const JNIInstanceStubSize = 32;

// x86 opcodes that we're interested in
static const Uint8 popEax = 0x58;
static const Uint8 pushl = 0x68;
static const Uint8 jmpl = 0xe9;
static const Uint8 pushEax = 0x50;
static const Uint8 calll = 0xe8;
static const Uint8 moveFromEax = 0xa3;

static const Uint16 jmplIndirectLow = 0xff;
static const Uint16 jmplIndirectHigh = 0x25;

#define writeRelativeDisplacement(symbol) \
  writeLittleWordUnaligned(where, reinterpret_cast<Uint32>(symbol) - Uint32(where + 4));\
  where += 4;

#define writeGlobalLocation(symbol) \
  writeLittleWordUnaligned(where, reinterpret_cast<Uint32>(symbol));\
  where += 4;

/* Generate stub glue for a native JNI function.
 * The net effect is to generate code that does the following:
 * 
 *  pop	eax	                          // Save return address in EAX
 *  mov JNIReturnAddress, eax             // Tuck return address safely away
 *  push declaringClass                   // Push declaring class; static methods only
 *  push JNIenv	                          // Push JNI pointer
 *  call nativeMethod                     // Jump to the actual JNI native method
 *  push eax                              // Store the return value on the stack
 *  call throwJNIExceptionIfNeeded        // Check and throw JNI exceptions if needed
 *  pop eax                               // Restore the return value
 *  jmp [JNIReturnAddress]                // jump to previously stored return address
 */
void *generateJNIGlue(const Method &method,
		      void *nativeFunction)
{
  void*	stub;
  int stubSize = (method.getModifiers() & CR_METHOD_STATIC) ?
    JNIStaticStubSize : JNIInstanceStubSize;
  
  assert(method.getModifiers() & CR_METHOD_NATIVE);
  assert(nativeFunction);
  
  // Write out the JNI stub
  stub = NativeCodeCache::sNativeCodeCache.acquireMemory(stubSize);

  Uint8* where = (Uint8*) stub;
  *where++ = popEax;      // popl eax

  *where++ = moveFromEax;
  writeGlobalLocation(JNIReturnAddress);


  // We're pushing arguments right-to-left, so the jclass
  // argument is pushed before the JNIEnv.
  if (method.getModifiers() & CR_METHOD_STATIC) {
    *where++ = pushl;     // pushl
    writeGlobalLocation((method.getDeclaringClass()));
  }

  *where++ = pushl;       // pushl
  writeGlobalLocation(jniEnv);
  
  // Put the return address back on the stack
  //*where++ = pushEax;        // push eax 

  *where++ = calll;        // call
  writeRelativeDisplacement(nativeFunction);

  // Store the return value on the stack
  *where++ = pushEax;     // push eax 

  // Call throwJNIExceptionIfNeeded
  *where++ = calll;
  writeRelativeDisplacement(&throwJNIExceptionIfNeeded);

  // Restore return value
  *where++ = popEax;      // pop eax

  // Get back to where we once belonged...
  *where++ = jmplIndirectLow;
  *where++ = jmplIndirectHigh;
  writeGlobalLocation(JNIReturnAddress);

  return stub; 
}


