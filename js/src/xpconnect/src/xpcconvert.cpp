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

/*
* This is a table driven scheme to determine if the types of the params of the 
* given method exclude that method from being reflected via XPConnect.
*
* The table can be appended and modified as requirements change. However...
*
* The table ASSUMES that all the type idenetifiers are contiguous starting 
* at ZERO. And, it also ASSUMES that the additional criteria of whether or 
* not a give type is reflectable are its use as a pointer and/or 'out' type.
*
* The table has a row for each type and columns for the combinations of
* that type being used as a pointer type and/or as an 'out' param.
*/

#define XPC_MK_BIT(p,o) (1 << (((p)?1:0)+((o)?2:0)))
#define XPC_IS_REFLECTABLE(f, p, o) ((f) & XPC_MK_BIT((p),(o)))
#define XPC_MK_FLAG(np_no,p_no,np_o,p_o) \
        ((uint8)((np_no) | ((p_no) << 1) | ((np_o) << 2) | ((p_o) << 3)))

/***********************************************************/
#define XPC_FLAG_COUNT nsXPTType::T_INTERFACE_IS+1

/* '1' means 'reflectable'. '0' means 'not reflectable'.   */
static uint8 xpc_reflectable_flags[XPC_FLAG_COUNT] = {
    /* 'p' stands for 'pointer' and 'o' stands for 'out'   */
    /*          !p&!o, p&!o, !p&o, p&o                     */
    XPC_MK_FLAG(  1  ,  1  ,   1 ,  0 ), /* T_I8           */ 
    XPC_MK_FLAG(  1  ,  1  ,   1 ,  0 ), /* T_I16          */ 
    XPC_MK_FLAG(  1  ,  1  ,   1 ,  0 ), /* T_I32          */ 
    XPC_MK_FLAG(  1  ,  1  ,   1 ,  0 ), /* T_I64          */ 
    XPC_MK_FLAG(  1  ,  1  ,   1 ,  0 ), /* T_U8           */ 
    XPC_MK_FLAG(  1  ,  1  ,   1 ,  0 ), /* T_U16          */ 
    XPC_MK_FLAG(  1  ,  1  ,   1 ,  0 ), /* T_U32          */ 
    XPC_MK_FLAG(  1  ,  1  ,   1 ,  0 ), /* T_U64          */ 
    XPC_MK_FLAG(  1  ,  1  ,   1 ,  0 ), /* T_FLOAT        */ 
    XPC_MK_FLAG(  1  ,  1  ,   1 ,  0 ), /* T_DOUBLE       */ 
    XPC_MK_FLAG(  1  ,  1  ,   1 ,  0 ), /* T_BOOL         */ 
    XPC_MK_FLAG(  1  ,  1  ,   1 ,  0 ), /* T_CHAR         */ 
    XPC_MK_FLAG(  1  ,  1  ,   1 ,  0 ), /* T_WCHAR        */ 
    XPC_MK_FLAG(  0  ,  0  ,   0 ,  0 ), /* T_VOID         */ 
    XPC_MK_FLAG(  0  ,  1  ,   0 ,  1 ), /* T_IID          */ 
    XPC_MK_FLAG(  0  ,  0  ,   0 ,  0 ), /* T_BSTR         */ 
    XPC_MK_FLAG(  0  ,  1  ,   0 ,  1 ), /* T_CHAR_STR     */ 
    XPC_MK_FLAG(  0  ,  1  ,   0 ,  1 ), /* T_WCHAR_STR    */ 
    XPC_MK_FLAG(  0  ,  1  ,   0 ,  1 ), /* T_INTERFACE    */ 
    XPC_MK_FLAG(  0  ,  1  ,   0 ,  1 )  /* T_INTERFACE_IS */ 
    };
/***********************************************************/

// static
JSBool
XPCConvert::IsMethodReflectable(const nsXPTMethodInfo& info)
{
    if(info.IsHidden())
        return JS_FALSE;

    for(int i = info.GetParamCount()-1; i >= 0; i--)
    {
        const nsXPTParamInfo& param = info.GetParam(i);
        const nsXPTType& type = param.GetType();

        uint8 base_type = type.TagPart();
        NS_ASSERTION(base_type < XPC_FLAG_COUNT, "BAD TYPE");

        if(!XPC_IS_REFLECTABLE(xpc_reflectable_flags[base_type], 
                               type.IsPointer(), param.IsOut()))
            return JS_FALSE;
    }
    return JS_TRUE;
}

/***************************************************************************/

#define JAM_DOUBLE(cx,v,d) (d=JS_NewDouble(cx,(jsdouble)v),DOUBLE_TO_JSVAL(d))
#define FIT_32(cx,i,d) (INT_FITS_IN_JSVAL(i)?INT_TO_JSVAL(i):JAM_DOUBLE(cx,i,d))
// Win32 can't handle uint64 to double conversion
#define JAM_DOUBLE_U64(cx,v,d) JAM_DOUBLE(cx,((int64)v),d)

// static
JSBool
XPCConvert::NativeData2JS(JSContext* cx, jsval* d, const void* s,
                          const nsXPTType& type, const nsID* iid,
                          uintN* pErr)
{
    NS_PRECONDITION(s, "bad param");
    NS_PRECONDITION(d, "bad param");

    jsdouble* dbl;

    if(pErr)
        *pErr = XPCJSError::BAD_CONVERT_NATIVE;

    switch(type.TagPart())
    {
    case nsXPTType::T_I8    : *d = INT_TO_JSVAL((int32)*((int8*)s));     break;
    case nsXPTType::T_I16   : *d = INT_TO_JSVAL((int32)*((int16*)s));    break;
    case nsXPTType::T_I32   : *d = FIT_32(cx,*((int32*)s),dbl);          break;
    case nsXPTType::T_I64   : *d = JAM_DOUBLE(cx,*((int64*)s),dbl);      break;
    case nsXPTType::T_U8    : *d = INT_TO_JSVAL((int32)*((uint8*)s));    break;
    case nsXPTType::T_U16   : *d = INT_TO_JSVAL((int32)*((uint16*)s));   break;
    case nsXPTType::T_U32   : *d = FIT_32(cx,*((uint32*)s),dbl);         break;
    case nsXPTType::T_U64   : *d = JAM_DOUBLE_U64(cx,*((uint64*)s),dbl); break;
    case nsXPTType::T_FLOAT : *d = JAM_DOUBLE(cx,*((float*)s),dbl);      break;
    case nsXPTType::T_DOUBLE: *d = JAM_DOUBLE(cx,*((double*)s),dbl);     break;
    case nsXPTType::T_BOOL  : *d = *((PRBool*)s)?JSVAL_TRUE:JSVAL_FALSE; break;
    case nsXPTType::T_CHAR  :
        {
            char* p = (char*)s;
            if(!p)
                return JS_FALSE;
            JSString* str;
            if(!(str = JS_NewStringCopyN(cx, p, 1)))
                return JS_FALSE;
            *d = STRING_TO_JSVAL(str);
            break;
        }
    case nsXPTType::T_WCHAR :
        {
            jschar* p = (jschar*)s;
            if(!p)
                return JS_FALSE;
            JSString* str;
            if(!(str = JS_NewUCStringCopyN(cx, p, 1)))
                return JS_FALSE;
            *d = STRING_TO_JSVAL(str);
            break;
        }
    default:
        if(!type.IsPointer())
        {
            XPC_LOG_ERROR(("XPCConvert::NativeData2JS : unsupported type"));
            return JS_FALSE;
        }

        // set the default result
        *d = JSVAL_NULL;

        switch(type.TagPart())
        {
        case nsXPTType::T_VOID:
            // XXX implement void* ?
            XPC_LOG_ERROR(("XPCConvert::NativeData2JS : void* params not supported"));
            return JS_FALSE;

        case nsXPTType::T_IID:
            {
                nsID* iid = *((nsID**)s);
                if(!iid)
                    return JS_FALSE;
                JSObject* obj;
                if(!(obj = xpc_NewIIDObject(cx, *iid)))
                    return JS_FALSE;
                *d = OBJECT_TO_JSVAL(obj);
                break;
            }

        case nsXPTType::T_BSTR:
            // XXX implement BSTR ?
            XPC_LOG_ERROR(("XPCConvert::NativeData2JS : BSTR params not supported"));
            return JS_FALSE;

        case nsXPTType::T_CHAR_STR:
            {
                char* p = *((char**)s);
                if(!p)
                    return JS_FALSE;
                JSString* str;
                if(!(str = JS_NewStringCopyZ(cx, p)))
                    return JS_FALSE;
                *d = STRING_TO_JSVAL(str);
                break;
            }

        case nsXPTType::T_WCHAR_STR:
            {
                jschar* p = *((jschar**)s);
                if(!p)
                    return JS_FALSE;
                JSString* str;
                if(!(str = JS_NewUCStringCopyZ(cx, p)))
                    return JS_FALSE;
                *d = STRING_TO_JSVAL(str);
                break;
            }

        case nsXPTType::T_INTERFACE:
        case nsXPTType::T_INTERFACE_IS:
            {
                nsISupports* iface = *((nsISupports**)s);
                if(!iface)
                {
                    *d = JSVAL_NULL;
                    break;
                }
                JSObject* aJSObj;
                // is this a wrapped JS object?
                if(nsXPCWrappedJSClass::IsWrappedJS(iface))
                {
                    nsIXPConnectWrappedJSMethods* methods;
                    if(NS_SUCCEEDED(iface->QueryInterface(
                                nsIXPConnectWrappedJSMethods::GetIID(),
                                (void**)&methods)) &&
                       NS_SUCCEEDED(methods->GetJSObject(&aJSObj)))
                    {
                        NS_RELEASE(methods);
                        *d = OBJECT_TO_JSVAL(aJSObj);
                    }
                }
                else
                {
                    // we need to build a wrapper
                    nsXPCWrappedNative* wrapper=NULL;
                    XPCContext* xpcc;
                    if(!iid || !(xpcc = nsXPConnect::GetContext(cx)) ||
                       !(wrapper = nsXPCWrappedNative::GetNewOrUsedWrapper(xpcc,
                                                                iface, *iid)))
                    {
                        return JS_FALSE;
                    }
                    aJSObj = wrapper->GetJSObject();
                    NS_RELEASE(wrapper);
                    if(aJSObj)
                        *d = OBJECT_TO_JSVAL(aJSObj);
                }

                break;
            }
        default:
            NS_ASSERTION(0, "bad type");
            return JS_FALSE;
        }
    }
    return JS_TRUE;
}

/***************************************************************************/
// static
JSBool
XPCConvert::JSData2Native(JSContext* cx, void* d, jsval s,
                          const nsXPTType& type,
                          JSBool useAllocator, const nsID* iid,
                          uintN* pErr)
{
    NS_PRECONDITION(d, "bad param");

    int32    ti;
    uint32   tu;
    jsdouble td;

    if(pErr)
        *pErr = XPCJSError::BAD_CONVERT_JS;

    switch(type.TagPart())
    {
    case nsXPTType::T_I8     :
        if(!JS_ValueToECMAInt32(cx, s, &ti))
            return JS_FALSE;
        *((int8*)d)  = (int8) ti;
        break;
    case nsXPTType::T_I16    :
        if(!JS_ValueToECMAInt32(cx, s, &ti))
            return JS_FALSE;
        *((int16*)d)  = (int16) ti;
        break;
    case nsXPTType::T_I32    :
        if(!JS_ValueToECMAInt32(cx, s, (int32*)d))
            return JS_FALSE;
        break;
    case nsXPTType::T_I64    :
        if(JSVAL_IS_INT(s))
        {
            if(!JS_ValueToECMAInt32(cx, s, &ti))
                return JS_FALSE;
            *((int64*)d) = (int64) ti;
        }
        else
        {
            if(!JS_ValueToNumber(cx, s, &td))
                return JS_FALSE;
            *((int64*)d) = (int64) td;
        }
        break;
    case nsXPTType::T_U8     :
        if(!JS_ValueToECMAUint32(cx, s, &tu))
            return JS_FALSE;
        *((uint8*)d)  = (uint8) tu;
        break;
    case nsXPTType::T_U16    :
        if(!JS_ValueToECMAUint32(cx, s, &tu))
            return JS_FALSE;
        *((uint16*)d)  = (uint16) tu;
        break;
    case nsXPTType::T_U32    :
        if(!JS_ValueToECMAUint32(cx, s, (uint32*)d))
            return JS_FALSE;
        break;
    case nsXPTType::T_U64    :
        if(JSVAL_IS_INT(s))
        {
            if(!JS_ValueToECMAUint32(cx, s, &tu))
                return JS_FALSE;
            *((uint64*)d) = (uint64) tu;
        }
        else
        {
            if(!JS_ValueToNumber(cx, s, &td))
                return JS_FALSE;
            // XXX Win32 can't handle double to uint64 directly
            *((uint64*)d) = (uint64)((int64) td);
        }
        break;
    case nsXPTType::T_FLOAT  :
        if(!JS_ValueToNumber(cx, s, &td))
            return JS_FALSE;
        *((float*)d) = (float) td;
        break;
    case nsXPTType::T_DOUBLE :
        if(!JS_ValueToNumber(cx, s, (double*)d))
            return JS_FALSE;
        break;
    case nsXPTType::T_BOOL   :
        if(!JS_ValueToBoolean(cx, s, (PRBool*)d))
            return JS_FALSE;
        break;
    case nsXPTType::T_CHAR   :
        {
            char* bytes=NULL;
            JSString* str;

            if(!(str = JS_ValueToString(cx, s))||
               !(bytes = JS_GetStringBytes(str)))
            {
                return JS_FALSE;
            }
            *((char*)d) = bytes[0];
            break;
        }
    case nsXPTType::T_WCHAR  :
        {
            jschar* chars=NULL;
            JSString* str;
            if(!(str = JS_ValueToString(cx, s))||
               !(chars = JS_GetStringChars(str)))
            {
                return JS_FALSE;
            }
            *((uint16*)d)  = (uint16) chars[0];
            break;
        }
    default:
        if(!type.IsPointer())
        {
            NS_ASSERTION(0,"unsupported type");
            return JS_FALSE;
        }

        switch(type.TagPart())
        {
        case nsXPTType::T_VOID:
            // XXX implement void* ?
            XPC_LOG_ERROR(("XPCConvert::JSData2Native : void* params not supported"));
            NS_ASSERTION(0,"void* params not supported");
            return JS_FALSE;
        case nsXPTType::T_IID:
        {
            NS_ASSERTION(useAllocator,"trying to convert a JSID to nsID without allocator");

            JSObject* obj;
            const nsID* pid=NULL;
            if(!JSVAL_IS_OBJECT(s) ||
               (!(obj = JSVAL_TO_OBJECT(s))) ||
               (!(pid = xpc_JSObjectToID(cx, obj))))
            {
                return JS_FALSE;
            }
            *((const nsID**)d) = pid;
            return JS_TRUE;
        }

        case nsXPTType::T_BSTR:
            // XXX implement BSTR
            XPC_LOG_ERROR(("XPCConvert::JSData2Native : BSTR params not supported"));
            return JS_FALSE;

        case nsXPTType::T_CHAR_STR:
        {
            char* bytes=NULL;
            JSString* str;

            if(!(str = JS_ValueToString(cx, s))||
               !(bytes = JS_GetStringBytes(str)))
            {
                return JS_FALSE;
            }
            if(useAllocator)
            {
                int len = strlen(bytes)+1;
                if(!(*((void**)d) = XPCMem::Alloc(len)))
                {
                    return JS_FALSE;
                }
                memcpy(*((void**)d), bytes, len);
            }
            else
                *((char**)d) = bytes;

            return JS_TRUE;
        }

        case nsXPTType::T_WCHAR_STR:
        {
            jschar* chars=NULL;
            JSString* str;

            if(!(str = JS_ValueToString(cx, s))||
               !(chars = JS_GetStringChars(str)))
            {
                return JS_FALSE;
            }
            if(useAllocator)
            {
                int byte_len = (JS_GetStringLength(str)+1)*sizeof(jschar);
                if(!(*((void**)d) = XPCMem::Alloc(byte_len)))
                {
                    // XXX should report error
                    return JS_FALSE;
                }
                memcpy(*((void**)d), chars, byte_len);
            }
            else
                *((jschar**)d) = chars;

            return JS_TRUE;
        }

        case nsXPTType::T_INTERFACE:
        case nsXPTType::T_INTERFACE_IS:
        {
            NS_ASSERTION(iid,"can't do interface conversions without iid");
            JSObject* obj;
            nsISupports* iface = NULL;

            if(JSVAL_IS_VOID(s) || JSVAL_IS_NULL(s))
            {
                *((nsISupports**)d) = NULL;
                return JS_TRUE;
            }

            // only wrap JSObjects
            if(!JSVAL_IS_OBJECT(s) ||
               (!(obj = JSVAL_TO_OBJECT(s))))
            {
                return JS_FALSE;
            }

            // is this really a native xpcom object with a wrapper?
            nsXPCWrappedNative* wrapper;
            if(NULL != (wrapper =
               nsXPCWrappedNativeClass::GetWrappedNativeOfJSObject(cx,obj)))
            {
                iface = wrapper->GetNative();
                // is the underlying object the right interface?
                if(wrapper->GetIID().Equals(*iid))
                    NS_ADDREF(iface);
                else
                    iface->QueryInterface(*iid, (void**)&iface);
            }
            else
            {
                // lets try to build a wrapper around the JSObject
                XPCContext* xpcc;
                if(NULL != (xpcc = nsXPConnect::GetContext(cx)))
                    iface = nsXPCWrappedJS::GetNewOrUsedWrapper(xpcc, obj, *iid);
            }
            if(iface)
            {
                // one AddRef has already been done
                *((nsISupports**)d) = iface;
                return JS_TRUE;
            }
            return JS_FALSE;
        }
        default:
            NS_ASSERTION(0, "bad type");
            return JS_FALSE;
        }
    }
    return JS_TRUE;
}

