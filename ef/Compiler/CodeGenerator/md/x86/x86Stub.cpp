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

#ifndef WIN32
    #error "This is a Win32-only file"
#endif

#include "NativeCodeCache.h"

/* Calls to a Java method are routed to this routine, which arranges for
   compilation of the method, backpatching of any involved 'call' instruction,
   and resumption of execution in the compiled method. */

PR_BEGIN_EXTERN_C
void __declspec( naked )
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

      // <== compileStubReEntryPoint should point here (the instruction after the call)

      // remove frame
      mov     esp,ebp
      pop     ebp

      // jump to the compiled method
      jmp     eax
    }
}
PR_END_EXTERN_C
