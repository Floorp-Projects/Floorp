/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

/* Platform specific code to invoke XPCOM methods on native objects */

#include "xpcprivate.h"

// Remember that these 'words' are 32bit DWORDS

#if !defined(LINUX)
#error "This code is for Linux x86 only"
#endif

static uint32
invoke_count_words(uint32 paramCount, nsXPCVariant* s)
{
    uint32 result = 0;
    for(uint32 i = 0; i < paramCount; i++, s++)
    {
        if(s->IsPtrData())
        {
            result++;
            continue;
        }
        switch(s->type)
        {
        case nsXPTType::T_I8     :
        case nsXPTType::T_I16    :
        case nsXPTType::T_I32    :
            result++;
            break;
        case nsXPTType::T_I64    :
            result+=2;
            break;
        case nsXPTType::T_U8     :
        case nsXPTType::T_U16    :
        case nsXPTType::T_U32    :
            result++;
            break;
        case nsXPTType::T_U64    :
            result+=2;
            break;
        case nsXPTType::T_FLOAT  :
            result++;
            break;
        case nsXPTType::T_DOUBLE :
            result+=2;
            break;
        case nsXPTType::T_BOOL   :
        case nsXPTType::T_CHAR   :
        case nsXPTType::T_WCHAR  :
            result++;
            break;
        default:
            // all the others are plain pointer types
            result++;
            break;
        }
    }
    return result;
}

static void
invoke_copy_to_stack(uint32* d, uint32 paramCount, nsXPCVariant* s)
{
    for(uint32 i = 0; i < paramCount; i++, d++, s++)
    {
        if(s->IsPtrData())
        {
            *((void**)d) = s->ptr;
            continue;
        }
        switch(s->type)
        {
        case nsXPTType::T_I8     : *((int8*)   d) = s->val.i8;          break;
        case nsXPTType::T_I16    : *((int16*)  d) = s->val.i16;         break;
        case nsXPTType::T_I32    : *((int32*)  d) = s->val.i32;         break;
        case nsXPTType::T_I64    : *((int64*)  d) = s->val.i64; d++;    break;
        case nsXPTType::T_U8     : *((uint8*)  d) = s->val.u8;          break;
        case nsXPTType::T_U16    : *((uint16*) d) = s->val.u16;         break;
        case nsXPTType::T_U32    : *((uint32*) d) = s->val.u32;         break;
        case nsXPTType::T_U64    : *((uint64*) d) = s->val.u64; d++;    break;
        case nsXPTType::T_FLOAT  : *((float*)  d) = s->val.f;           break;
        case nsXPTType::T_DOUBLE : *((double*) d) = s->val.d;   d++;    break;
        case nsXPTType::T_BOOL   : *((PRBool*) d) = s->val.b;           break;
        case nsXPTType::T_CHAR   : *((char*)   d) = s->val.c;           break;
        case nsXPTType::T_WCHAR  : *((wchar_t*)d) = s->val.wc;          break;
        default:
            // all the others are plain pointer types
            *((void**)d) = s->val.p;
            break;
        }
    }
}

nsresult
xpc_InvokeNativeMethod(void* that, PRUint32 index,
                       uint32 paramCount, nsXPCVariant* params)
{
    uint32 result;
    void* fn_count = invoke_count_words;
    void* fn_copy = invoke_copy_to_stack;

 __asm__ __volatile__(
    "pushl %4\n\t"
    "pushl %3\n\t"
    "movl  %5, %%eax\n\t"
    "call  *%%eax\n\t"         /* count words */
    "addl  $0x8, %%esp\n\t"
    "shl   $2, %%eax\n\t"    /* *= 4 */
    "subl  %%eax, %%esp\n\t" /* make room for params */
    "movl  %%esp, %%edx\n\t"
    "pushl %4\n\t"
    "pushl %3\n\t"
    "pushl %%edx\n\t"
    "movl  %6, %%eax\n\t"
    "call  *%%eax\n\t"       /* copy params */
    "addl  $0xc, %%esp\n\t"
    "movl  %1, %%ecx\n\t"
    "pushl %%ecx\n\t"
    "movl  (%%ecx), %%edx\n\t"
    "movl  %2, %%eax\n\t"   /* function index */
    "shl   $2, %%eax\n\t"   /* *= 4 */
    "addl  $8, %%eax\n\t"   /* += 8 */
    "addl  %%eax, %%edx\n\t"
    "call  *(%%edx)\n\t"    /* safe to not cleanup esp */
    "movl  %%eax, %0"
    : "=g" (result)         /* %0 */
    : "g" (that),           /* %1 */
      "g" (index),          /* %2 */
      "g" (paramCount),     /* %3 */
      "g" (params),         /* %4 */
      "g" (fn_count),       /* %5 */
      "g" (fn_copy)         /* %6 */
    : "ax", "cx", "dx", "memory" 
    );
  
  return result;
}    

#ifdef DEBUG
nsresult
XPC_TestInvoke(void* that, PRUint32 index,
               uint32 paramCount, nsXPCVariant* params)
{
    return xpc_InvokeNativeMethod(that, index, paramCount, params);
}
#endif

