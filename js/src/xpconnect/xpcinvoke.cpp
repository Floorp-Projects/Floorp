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

#ifdef WIN32

// Remember that on Win32 these 'words' are 32bit DWORDS

static uint32 __stdcall
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
        case nsXPTType::T_I64    :
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

static void __stdcall
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

#pragma warning(disable : 4035) // OK to have no return value
nsresult
xpc_InvokeNativeMethod(void* that, PRUint32 index,
                       uint32 paramCount, nsXPCVariant* params)
{
    __asm {
        push    params
        push    paramCount
        call    invoke_count_words  // stdcall, result in eax
        shl     eax,2               // *= 4
        sub     esp,eax             // make space for params
        mov     edx,esp
        push    params
        push    paramCount
        push    edx
        call    invoke_copy_to_stack // stdcall
        mov     ecx,that            // instance in ecx
        push    ecx                 // push this
        mov     edx,[ecx]           // vtable in edx
        mov     eax,index
        shl     eax,2               // *= 4
        add     edx,eax
        call    [edx]               // stdcall, i.e. callee cleans up stack.
    }
}
#pragma warning(default : 4035) // restore default

#ifdef DEBUG
nsresult
XPC_TestInvoke(void* that, PRUint32 index,
               uint32 paramCount, nsXPCVariant* params)
{
    return xpc_InvokeNativeMethod(that, index, paramCount, params);
}
#endif

#endif

