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

#include "xpcprivate.h"

#ifdef WIN32

// Remember that on Win32 these 'words' are 32bit DWORDS

static uint32 __stdcall
invoke_count_words(uint32 paramCount, nsXPCVarient* src)
{
    uint32 result = 0;
    for(uint32 i = 0; i < paramCount; i++, src++)
    {
        switch(src->type)
        {
        case nsXPCType::T_I8     :
        case nsXPCType::T_I16    :
        case nsXPCType::T_I32    :
        case nsXPCType::T_I64    :
        case nsXPCType::T_U8     :
        case nsXPCType::T_U16    :
        case nsXPCType::T_U32    :
            result++;
            break;
        case nsXPCType::T_U64    :
            result+=2;
            break;
        case nsXPCType::T_FLOAT  :
            result++;
            break;
        case nsXPCType::T_DOUBLE :
            result+=2;
            break;
        case nsXPCType::T_BOOL   :
        case nsXPCType::T_CHAR   :
        case nsXPCType::T_WCHAR  :
            result++;
            break;
        // we assert that all the pointer types are lumped as void*
        default:
            NS_ASSERTION(src->type == nsXPCType::T_P_VOID, "bad type");
            result++;
            break;
        }
    }
    return result;
}

static void __stdcall
invoke_copy_to_stack(uint32* d, uint32 paramCount, nsXPCVarient* s)
{
    for(uint32 i = 0; i < paramCount; i++, d++, s++)
    {
        switch(s->type)
        {
        case nsXPCType::T_I8     : *((int8*)   d) = s->val.i8;          break;
        case nsXPCType::T_I16    : *((int16*)  d) = s->val.i16;         break;
        case nsXPCType::T_I32    : *((int32*)  d) = s->val.i32;         break;
        case nsXPCType::T_I64    : *((int64*)  d) = s->val.i64; d++;    break;
        case nsXPCType::T_U8     : *((uint8*)  d) = s->val.u8;          break;
        case nsXPCType::T_U16    : *((uint16*) d) = s->val.u16;         break;
        case nsXPCType::T_U32    : *((uint32*) d) = s->val.u32;         break;
        case nsXPCType::T_U64    : *((uint64*) d) = s->val.u64; d++;    break;
        case nsXPCType::T_FLOAT  : *((float*)  d) = s->val.f;           break;
        case nsXPCType::T_DOUBLE : *((double*) d) = s->val.d;   d++;    break;
        case nsXPCType::T_BOOL   : *((PRBool*) d) = s->val.b;           break;
        case nsXPCType::T_CHAR   : *((char*)   d) = s->val.c;           break;
        case nsXPCType::T_WCHAR  : *((wchar_t*)d) = s->val.wc;          break;
        // we assert that all the pointer types are lumped as void*
        default:
            NS_ASSERTION(s->type == nsXPCType::T_P_VOID, "bad type");
            *((void**)d) = s->val.p;
            break;
        }
    }
}

#pragma warning(disable : 4035) // OK to have no return value
nsresult
xpc_InvokeNativeMethod(void* that, PRUint32 index,
                       uint32 paramCount, nsXPCVarient* params)
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
               uint32 paramCount, nsXPCVarient* params)
{
    return xpc_InvokeNativeMethod(that, index, paramCount, params);
}
#endif

#endif

