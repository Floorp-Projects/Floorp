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


#ifdef __GNUC__

/* 
  Following code written by rth@cygnus.org.  Comment from fur@netscape.com.

  I suspect that your code will work OK if there are no exceptions while a method is being
  compiled, but you'll probably crash if the compilation terminates with an exception.  That's
  because the Java exception code relies on the presence of a "guard frame" when calling from
  JIT'ed to native code so as to restore the callee-saved registers when unwinding the stack. 
  See x86ExceptionHandler.cpp for some details.  This code is "temporary".  What we
  eventually hoped to do was to use a different calling convention (in terms of which registers
  are volatile) for calls that make an exceptional return versus a normal return, so that the stack
  unwinding code did not have to restore any registers.  Until we can educate the register
  allocator about these constraints, we'll need to retain this hacky guard frame and the various
  bits of code that are used to create it.  (See also x86Win32InvokeNative.cpp).

  Some of the exception debugging code makes assumptions about the location of the call
  instruction inside the static compile stub - assumptions that are broken by the new code.  (And
  the exception debugging code should probably be changed to get rid of that dependency.)
*/

/* Go through one level of extra indirection to isolate ourselves from name
   mangling and cdecl vs stdcall changes.  */

static void* compileStub_1(const CacheEntry&, void *) __attribute__((regparm(2), unused));
static void* compileStub_1(const CacheEntry&, void *) __asm__("compileStub_1");

static void*
compileStub_1(const CacheEntry& inCacheEntry, void *retAddr)
{
  return compileAndBackPatchMethod(inCacheEntry, retAddr);
}

extern void compileStub() __asm__("compileStub");

asm("\n\
compileStub:\n\
movl 0(%esp), %edx\n\
call compileStub_1\n\
jmpl *%eax");

#else /* !__GNUC__ */

static void __declspec( naked )
compileStub()
{
  _asm
    {
      // eax contains the cache entry.

      // make frame
      push    ebp
      mov     ebp,esp

      // save all volatiles (especially for exception handler)
      // ??? Um, these are _non_ volatile, ie callee saved.
      // We shouldn't have to do anything with them.
      push    edi
      push    esi
      push    ebx

      // call compileAndBackPatchMethod with args
      // third argument is not used
      push    [esp + 16]              // second argument -- return address
      push    eax                     // first argument -- cacheEntry
      call    compileAndBackPatchMethod

      // remove  args
      pop     edx                     // <--- compileStubReEntryPoint
      pop     edx

      pop     ebx                     // Restore volatiles
      pop     esi
      pop     edi

      // remove frame
      mov     esp,ebp
      pop     ebp

      // jump to the compiled method
      jmp     eax
    }
}
#endif /* __GNUC__ */

#ifdef DEBUG
// Pointer to the instruction after the call (used by exception handler to check
// I wanted to use:
//		void* compileStubReEntryPoint =  (void*) ((Uint8*)staticCompileStub + 17);
// but MSDev appears to have a bug, in that compileStubReEntryPoint will be set == (void*)staticCompileStub
// which is clearly wrong.
void* compileStubAddress = (void*)compileStub;
// void* compileStubReEntryPoint = (Uint8*)compileStubAddress + 15; // Correct address ?
void* compileStubReEntryPoint = NULL;
#endif // DEBUG

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
  void* stub;
  uint8  *where;

  // Write out the dynamic compile stub

  stub = inCache.acquireMemory(10);
  where = (uint8 *)stub;

  // movl $inCacheEntry, %eax
  *where++ = 0xb8;
  writeLittleWordUnaligned(where, (uint32)&inCacheEntry);
  where += 4;

  // jmp compileStub
  *where++ = 0xe9;
  writeLittleWordUnaligned(where, (Uint8 *) compileStub - (where + 4));

  // Return the address of the dynamic stub.
  return stub;
}

void*
backPatchMethod(void* inMethodAddress, void* inLastPC, void* /*inUserDefined*/)
{

  uint32 curAddress = (uint32) inLastPC;
  uint32 methodAddress = (uint32) inMethodAddress;

  // Compute the relative branch
  uint32*   relativeBranch = ((uint32*)inLastPC)-1;
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
// Purpose: signed right-aligned field extraction
// In:      64 bit source (on  stack)
//          32 bit extraction size (on stack)
// Out:     64 bit result
// Note:    Only works in range 1 <= b <= 63, b is extraction amount

int64 __stdcall
x86Extract64Bit(int64 src, int b)
{
  if (b <= 32)
    {
      b = 32 - b;
      return (int)src << b >> b;
    }
  else
    {
      b = 64 - b;
      return (int)(src >> 32) << b >> b;
    }
}

// 3WayCompare
//
// Purpose: compare two longs
// In:      two longs on the stack
// Out:     depends on condition flags:
//              less = -1
//              equal = 0
//              greater = 1

int64 __stdcall
x86ThreeWayCMP_L(int64 a, int64 b)
{
  return (a > b) - (a < b);
}

// 3WayCompare
//
// Purpose: compare two longs
// In:      two longs on the stack
// Out:     depends on condition flags:
//              less    =  1
//              equal   =  0
//              greater = -1

int64 __stdcall
x86ThreeWayCMPC_L(int64 a, int64 b)
{
  return (a < b) - (a > b);
}

// llmul
//
// Purpose: long multiply (same for signed/unsigned)
// In:      args are passed on the stack:
//              1st pushed: multiplier (QWORD)
//              2nd pushed: multiplicand (QWORD)
// Out:     EDX:EAX - product of multiplier and multiplicand
// Note:    parameters are removed from the stack
// Uses:    ECX

int64 __stdcall
x86Mul64Bit(int64 a, int64 b)
{
  return a * b;
}

// lldiv
//
// Purpose: signed long divide
// In:      args are passed on the stack:
//              1st pushed: divisor (QWORD)
//              2nd pushed: dividend (QWORD)
// Out:     EDX:EAX contains the quotient (dividend/divisor)
// Note:    parameters are removed from the stack
// Uses:    ECX

int64 __stdcall
x86Div64Bit(int64 dividend, int64 divisor)
{
  return dividend / divisor;
}

// llrem
//
// Purpose: signed long remainder
// In:      args are passed on the stack:
//              1st pushed: divisor (QWORD)
//              2nd pushed: dividend (QWORD)
// Out:     EDX:EAX contains the remainder (dividend/divisor)
// Note:    parameters are removed from the stack
// Uses:    ECX

int64 __stdcall
x86Mod64Bit(int64 dividend, int64 divisor)
{
  return dividend % divisor;
}

// llshl
//
// Purpose: long shift left
// In:      args are passed on the stack: (FIX make fastcall)
//              1st pushed: amount (int)
//              2nd pushed: source (long)
// Out:     EDX:EAX contains the result
// Note:    parameters are removed from the stack
// Uses:    ECX, destroyed

int64 __stdcall
x86Shl64Bit(int64 src, int amount)
{
  return src << amount;
}

// llshr
//
// Purpose: long shift right
// In:      args are passed on the stack: (FIX make fastcall)
//              1st pushed: amount (int)
//              2nd pushed: source (long)
// Out:     EDX:EAX contains the result
// Note:    parameters are removed from the stack
// Uses:    ECX, destroyed

uint64 __stdcall
x86Shr64Bit(uint64 src, int amount)
{
  return src >> amount;
}

// llsar
//
// Purpose: long shift right signed
// In:      args are passed on the stack: (FIX make fastcall)
//              1st pushed: amount (int)
//              2nd pushed: source (long)
// Out:     EDX:EAX contains the result
// Note:    parameters are removed from the stack
// Uses:    ECX, destroyed

int64 __stdcall
x86Sar64Bit(int64 src, int amount)
{
  return src >> amount;
}

//================================================================================
