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

/* Data conversion between native and JavaScript types. */

#include "xpcprivate.h"

// XXX these conversion functions need to be finished.
// XXX conversion functions may still need paramInfo to handle the additional
// types


#define JAM_DOUBLE(v,d) (d = (jsdouble)v, DOUBLE_TO_JSVAL(&d))
#define FIT_32(i,d) (INT_FITS_IN_JSVAL(i) ? INT_TO_JSVAL(i) : JAM_DOUBLE(i,d))
// Win32 can't handle uint64 to double conversion
#define JAM_DOUBLE_U64(v,d) JAM_DOUBLE(((int64)v),d)

JSBool
xpc_ConvertNativeData2JS(jsval* d, const void* s, const nsXPTType& type)
{
    NS_PRECONDITION(s, "bad param");
    NS_PRECONDITION(d, "bad param");

    jsdouble dbl;

    switch(type.TagPart())
    {
    case nsXPTType::T_I8    : *d = INT_TO_JSVAL((int32)*((int8*)s));     break;
    case nsXPTType::T_I16   : *d = INT_TO_JSVAL((int32)*((int16*)s));    break;
    case nsXPTType::T_I32   : *d = FIT_32(*((int32*)s),dbl);             break;
    case nsXPTType::T_I64   : *d = JAM_DOUBLE(*((int64*)s),dbl);         break;
    case nsXPTType::T_U8    : *d = INT_TO_JSVAL((int32)*((uint8*)s));    break;
    case nsXPTType::T_U16   : *d = INT_TO_JSVAL((int32)*((uint16*)s));   break;
    case nsXPTType::T_U32   : *d = FIT_32(*((uint32*)s),dbl);            break;
    case nsXPTType::T_U64   : *d = JAM_DOUBLE_U64(*((uint64*)s),dbl);    break;
    case nsXPTType::T_FLOAT : *d = JAM_DOUBLE(*((float*)s),dbl);         break;
    case nsXPTType::T_DOUBLE: *d = DOUBLE_TO_JSVAL(*((double*)s));       break;
    case nsXPTType::T_BOOL  : *d = *((PRBool*)s)?JSVAL_TRUE:JSVAL_FALSE; break;
    case nsXPTType::T_CHAR  : *d = INT_TO_JSVAL((int32)*((char*)s));     break;
    case nsXPTType::T_WCHAR : *d = INT_TO_JSVAL((int32)*((wchar_t*)s));  break;
    default:
        switch(type)
        {
        case nsXPTType::T_P_VOID:
            // XXX implement void*
            NS_ASSERTION(0,"void* params not supported");
            return JS_FALSE;
        case nsXPTType::T_P_IID:
            // XXX implement IID
            NS_ASSERTION(0,"iid params not supported");
            return JS_FALSE;
        case nsXPTType::T_BSTR:
            // XXX implement BSTR
            NS_ASSERTION(0,"string params not supported");
            return JS_FALSE;
        case nsXPTType::T_P_CHAR_STR:
            // XXX implement CHAR_STR
            NS_ASSERTION(0,"string params not supported");
            return JS_FALSE;
        case nsXPTType::T_P_WCHAR_STR:
            // XXX implement WCHAR_STR
            NS_ASSERTION(0,"string params not supported");
            return JS_FALSE;
        case nsXPTType::T_INTERFACE:
            // XXX implement INTERFACE
            // make sure 'src' is an object
            // get the nsIInterfaceInfo* from the param and
            // build a wrapper and then hand over the wrapper.
            // XXX remember to release the wrapper in cleanup below
            NS_ASSERTION(0,"interface params not supported");
            return JS_FALSE;
        case nsXPTType::T_INTERFACE_IS:
            // XXX implement INTERFACE_IS
            NS_ASSERTION(0,"interface_is params not supported");
            return JS_FALSE;
        default:
            NS_ASSERTION(0, "bad type");
            return JS_FALSE;
        }
    }
    return JS_TRUE;
}

JSBool
xpc_ConvertJSData2Native(JSContext* cx, void* d, const jsval* s,
                         const nsXPTType& type)
{
    NS_PRECONDITION(s, "bad param");
    NS_PRECONDITION(d, "bad param");

    int32    ti;
    uint32   tu;
    jsdouble td;
    JSBool   r;

    switch(type.TagPart())
    {
    case nsXPTType::T_I8     :
        r = JS_ValueToECMAInt32(cx, *s, &ti);
        *((int8*)d)  = (int8) ti;
        break;
    case nsXPTType::T_I16    :
        r = JS_ValueToECMAInt32(cx, *s, &ti);
        *((int16*)d)  = (int16) ti;
        break;
    case nsXPTType::T_I32    :
        r = JS_ValueToECMAInt32(cx, *s, (int32*)d);
        break;
    case nsXPTType::T_I64    :
        if(JSVAL_IS_INT(*s))
        {
            r = JS_ValueToECMAInt32(cx, *s, &ti);
            *((int64*)d) = (int64) ti;
        }
        else
        {
            r = JS_ValueToNumber(cx, *s, &td);
            if(r) *((int64*)d) = (int64) td;
        }
        break;
    case nsXPTType::T_U8     :
        r = JS_ValueToECMAUint32(cx, *s, &tu);
        *((uint8*)d)  = (uint8) tu;
        break;
    case nsXPTType::T_U16    :
        r = JS_ValueToECMAUint32(cx, *s, &tu);
        *((uint16*)d)  = (uint16) tu;
        break;
    case nsXPTType::T_U32    :
        r = JS_ValueToECMAUint32(cx, *s, (uint32*)d);
        break;
    case nsXPTType::T_U64    :
        if(JSVAL_IS_INT(*s))
        {
            r = JS_ValueToECMAUint32(cx, *s, &tu);
            *((uint64*)d) = (uint64) tu;
        }
        else
        {
            r = JS_ValueToNumber(cx, *s, &td);
            // XXX Win32 can't handle double to uint64 directly
            if(r) *((uint64*)d) = (uint64)((int64) td);
        }
        break;
    case nsXPTType::T_FLOAT  :
        r = JS_ValueToNumber(cx, *s, &td);
        if(r) *((float*)d) = (float) td;
        break;
    case nsXPTType::T_DOUBLE :
        r = JS_ValueToNumber(cx, *s, (double*)d);
        break;
    case nsXPTType::T_BOOL   :
        r = JS_ValueToBoolean(cx, *s, (PRBool*)d);
        break;
    case nsXPTType::T_CHAR   :
        r = JS_ValueToECMAUint32(cx, *s, &tu);
        *((char*)d)  = (char) tu;
        break;
    case nsXPTType::T_WCHAR  :
        r = JS_ValueToECMAUint32(cx, *s, &tu);
        *((uint16*)d)  = (uint16) tu;
        break;
    default:
        switch(type)
        {
        case nsXPTType::T_P_VOID:
            // XXX implement void*
            NS_ASSERTION(0,"void* params not supported");
            return JS_FALSE;
        case nsXPTType::T_P_IID:
            // XXX implement IID
            NS_ASSERTION(0,"iid params not supported");
            return JS_FALSE;
        case nsXPTType::T_BSTR:
            // XXX implement BSTR
            NS_ASSERTION(0,"string params not supported");
            return JS_FALSE;
        case nsXPTType::T_P_CHAR_STR:
            // XXX implement CHAR_STR
            NS_ASSERTION(0,"string params not supported");
            return JS_FALSE;
        case nsXPTType::T_P_WCHAR_STR:
            // XXX implement WCHAR_STR
            NS_ASSERTION(0,"string params not supported");
            return JS_FALSE;
        case nsXPTType::T_INTERFACE:
            // XXX implement INTERFACE
            // make sure 'src' is an object
            // get the nsIInterfaceInfo* from the param and
            // build a wrapper and then hand over the wrapper.
            // XXX remember to release the wrapper in cleanup below
            NS_ASSERTION(0,"interface params not supported");
            return JS_FALSE;
        case nsXPTType::T_INTERFACE_IS:
            // XXX implement INTERFACE_IS
            NS_ASSERTION(0,"interface_is params not supported");
            return JS_FALSE;
        default:
            NS_ASSERTION(0, "bad type");
            return JS_FALSE;
        }
    }
    return JS_TRUE;
}

