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

static NS_DEFINE_IID(kWrappedJSMethodsIID, NS_IXPCONNECT_WRAPPED_JS_METHODS_IID);

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
        if(param.IsOut())
        {
            if(type.IsPointer())
            {
                switch(type.TagPart())
                {
                case nsXPTType::T_I8          :
                case nsXPTType::T_I16         :
                case nsXPTType::T_I32         :
                case nsXPTType::T_I64         :
                case nsXPTType::T_U8          :
                case nsXPTType::T_U16         :
                case nsXPTType::T_U32         :
                case nsXPTType::T_U64         :
                case nsXPTType::T_FLOAT       :
                case nsXPTType::T_DOUBLE      :
                case nsXPTType::T_BOOL        :
                case nsXPTType::T_CHAR        :
                case nsXPTType::T_WCHAR       :
                case nsXPTType::T_VOID        :
                    return JS_FALSE;
                case nsXPTType::T_IID         :
                    continue;
                case nsXPTType::T_BSTR        :
                    return JS_FALSE;
                case nsXPTType::T_CHAR_STR    :
                case nsXPTType::T_WCHAR_STR   :
                case nsXPTType::T_INTERFACE   :
                case nsXPTType::T_INTERFACE_IS:
                    continue;
                default:
                    NS_ASSERTION(0, "bad type");
                    return JS_FALSE;
                }
            }
            else    // !(type.IsPointer())
            {
                switch(type.TagPart())
                {
                case nsXPTType::T_I8          :
                case nsXPTType::T_I16         :
                case nsXPTType::T_I32         :
                case nsXPTType::T_I64         :
                case nsXPTType::T_U8          :
                case nsXPTType::T_U16         :
                case nsXPTType::T_U32         :
                case nsXPTType::T_U64         :
                case nsXPTType::T_FLOAT       :
                case nsXPTType::T_DOUBLE      :
                case nsXPTType::T_BOOL        :
                case nsXPTType::T_CHAR        :
                case nsXPTType::T_WCHAR       :
                    continue;
                case nsXPTType::T_VOID        :
                case nsXPTType::T_IID         :
                case nsXPTType::T_BSTR        :
                case nsXPTType::T_CHAR_STR    :
                case nsXPTType::T_WCHAR_STR   :
                case nsXPTType::T_INTERFACE   :
                case nsXPTType::T_INTERFACE_IS:
                default:
                    NS_ASSERTION(0, "bad type");
                    return JS_FALSE;
                }
            }
        }
        else    // !(param.IsOut())
        {
            if(type.IsPointer())
            {
                switch(type.TagPart())
                {
                case nsXPTType::T_I8          :
                case nsXPTType::T_I16         :
                case nsXPTType::T_I32         :
                case nsXPTType::T_I64         :
                case nsXPTType::T_U8          :
                case nsXPTType::T_U16         :
                case nsXPTType::T_U32         :
                case nsXPTType::T_U64         :
                case nsXPTType::T_FLOAT       :
                case nsXPTType::T_DOUBLE      :
                case nsXPTType::T_BOOL        :
                case nsXPTType::T_CHAR        :
                case nsXPTType::T_WCHAR       :
                    continue;
                case nsXPTType::T_VOID        :
                    return JS_FALSE;
                case nsXPTType::T_IID         :
                    continue;
                case nsXPTType::T_BSTR        :
                    return JS_FALSE;
                case nsXPTType::T_CHAR_STR    :
                case nsXPTType::T_WCHAR_STR   :
                case nsXPTType::T_INTERFACE   :
                case nsXPTType::T_INTERFACE_IS:
                    continue;
                default:
                    NS_ASSERTION(0, "bad type");
                    return JS_FALSE;
                }
            }
            else    // !(type.IsPointer())
            {
                switch(type.TagPart())
                {
                case nsXPTType::T_I8          :
                case nsXPTType::T_I16         :
                case nsXPTType::T_I32         :
                case nsXPTType::T_I64         :
                case nsXPTType::T_U8          :
                case nsXPTType::T_U16         :
                case nsXPTType::T_U32         :
                case nsXPTType::T_U64         :
                case nsXPTType::T_FLOAT       :
                case nsXPTType::T_DOUBLE      :
                case nsXPTType::T_BOOL        :
                case nsXPTType::T_CHAR        :
                case nsXPTType::T_WCHAR       :
                    continue;
                case nsXPTType::T_VOID        :
                case nsXPTType::T_IID         :
                case nsXPTType::T_BSTR        :
                case nsXPTType::T_CHAR_STR    :
                case nsXPTType::T_WCHAR_STR   :
                case nsXPTType::T_INTERFACE   :
                case nsXPTType::T_INTERFACE_IS:
                default:
                    NS_ASSERTION(0, "bad type");
                    return JS_FALSE;
                }
            }
        }
    }
    return JS_TRUE;
}

// XXX these conversion functions need to be finished.
// XXX conversion functions may still need paramInfo to handle the additional
// types


#define JAM_DOUBLE(v,d) (d = (jsdouble)v, DOUBLE_TO_JSVAL(&d))
#define FIT_32(i,d) (INT_FITS_IN_JSVAL(i) ? INT_TO_JSVAL(i) : JAM_DOUBLE(i,d))
// Win32 can't handle uint64 to double conversion
#define JAM_DOUBLE_U64(v,d) JAM_DOUBLE(((int64)v),d)

// static
JSBool
XPCConvert::NativeData2JS(JSContext* cx, jsval* d, const void* s,
                          const nsXPTType& type, const nsID* iid)
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
        if(!type.IsPointer())
        {
            NS_ASSERTION(0,"unsupported type");
            return JS_FALSE;
        }

        // set the default result
        *d = JSVAL_NULL;

        switch(type.TagPart())
        {
        case nsXPTType::T_VOID:
            // XXX implement void* ?
            NS_ASSERTION(0,"void* params not supported");
            return JS_FALSE;

        case nsXPTType::T_IID:
            {
                nsID* iid = *((nsID**)s);
                if(!iid)
                    break;
                JSObject* obj;
                if(!(obj = xpc_NewIDObject(cx, *iid)))
                    break;
                *d = OBJECT_TO_JSVAL(obj);
                break;
            }

        case nsXPTType::T_BSTR:
            // XXX implement BSTR ?
            NS_ASSERTION(0,"'BSTR' string params not supported");
            return JS_FALSE;

        case nsXPTType::T_CHAR_STR:
            {
                char* p = *((char**)s);
                if(!p)
                    break;
                JSString* str;
                if(!(str = JS_NewStringCopyZ(cx, p)))
                    break;
                *d = STRING_TO_JSVAL(str);
                break;
            }

        case nsXPTType::T_WCHAR_STR:
            {
                jschar* p = *((jschar**)s);
                if(!p)
                    break;
                JSString* str;
                if(!(str = JS_NewUCStringCopyZ(cx, p)))
                    break;
                *d = STRING_TO_JSVAL(str);
                break;
            }

        case nsXPTType::T_INTERFACE:
        case nsXPTType::T_INTERFACE_IS:
            {
                nsISupports* iface = *((nsISupports**)s);
                if(!iface)
                    break;
                JSObject* aJSObj;
                // is this a wrapped JS object?
                if(nsXPCWrappedJSClass::IsWrappedJS(iface))
                {
                    nsIXPConnectWrappedJSMethods* methods;
                    if(NS_SUCCEEDED(iface->QueryInterface(kWrappedJSMethodsIID,
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
                    nsXPCWrappedNative* wrapper;
                    XPCContext* xpcc;
                    if(!iid || !(xpcc = nsXPConnect::GetContext(cx)) ||
                       !(wrapper = nsXPCWrappedNative::GetNewOrUsedWrapper(xpcc,
                                                                iface, *iid)))
                    {
                        break;
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

// static
JSBool
XPCConvert::JSData2Native(JSContext* cx, void* d, jsval s,
                          const nsXPTType& type,
                          nsIAllocator* al, const nsID* iid)
{
    NS_PRECONDITION(d, "bad param");

    int32    ti;
    uint32   tu;
    jsdouble td;
    JSBool   r;

    switch(type.TagPart())
    {
    case nsXPTType::T_I8     :
        r = JS_ValueToECMAInt32(cx, s, &ti);
        *((int8*)d)  = (int8) ti;
        break;
    case nsXPTType::T_I16    :
        r = JS_ValueToECMAInt32(cx, s, &ti);
        *((int16*)d)  = (int16) ti;
        break;
    case nsXPTType::T_I32    :
        r = JS_ValueToECMAInt32(cx, s, (int32*)d);
        break;
    case nsXPTType::T_I64    :
        if(JSVAL_IS_INT(s))
        {
            r = JS_ValueToECMAInt32(cx, s, &ti);
            *((int64*)d) = (int64) ti;
        }
        else
        {
            r = JS_ValueToNumber(cx, s, &td);
            if(r) *((int64*)d) = (int64) td;
        }
        break;
    case nsXPTType::T_U8     :
        r = JS_ValueToECMAUint32(cx, s, &tu);
        *((uint8*)d)  = (uint8) tu;
        break;
    case nsXPTType::T_U16    :
        r = JS_ValueToECMAUint32(cx, s, &tu);
        *((uint16*)d)  = (uint16) tu;
        break;
    case nsXPTType::T_U32    :
        r = JS_ValueToECMAUint32(cx, s, (uint32*)d);
        break;
    case nsXPTType::T_U64    :
        if(JSVAL_IS_INT(s))
        {
            r = JS_ValueToECMAUint32(cx, s, &tu);
            *((uint64*)d) = (uint64) tu;
        }
        else
        {
            r = JS_ValueToNumber(cx, s, &td);
            // XXX Win32 can't handle double to uint64 directly
            if(r) *((uint64*)d) = (uint64)((int64) td);
        }
        break;
    case nsXPTType::T_FLOAT  :
        r = JS_ValueToNumber(cx, s, &td);
        if(r) *((float*)d) = (float) td;
        break;
    case nsXPTType::T_DOUBLE :
        r = JS_ValueToNumber(cx, s, (double*)d);
        break;
    case nsXPTType::T_BOOL   :
        r = JS_ValueToBoolean(cx, s, (PRBool*)d);
        break;
    case nsXPTType::T_CHAR   :
        r = JS_ValueToECMAUint32(cx, s, &tu);
        *((char*)d)  = (char) tu;
        break;
    case nsXPTType::T_WCHAR  :
        r = JS_ValueToECMAUint32(cx, s, &tu);
        *((uint16*)d)  = (uint16) tu;
        break;
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
            NS_ASSERTION(0,"void* params not supported");
            return JS_FALSE;
        case nsXPTType::T_IID:
        {
            JSObject* obj;
            const nsID* pid;
            if(!JSVAL_IS_OBJECT(s) ||
               (!(obj = JSVAL_TO_OBJECT(s))) ||
               (!(pid = xpc_JSObjectToID(cx, obj))))
            {
                // XXX should report error
                return JS_FALSE;
            }
            if(al)
            {
                if(!(*((void**)d) = al->Alloc(sizeof(nsID))))
                {
                    // XXX should report error
                    return JS_FALSE;
                }
                memcpy(*((void**)d), pid, sizeof(nsID));
            }
            else
                *((const nsID**)d) = pid;

            return JS_TRUE;
        }

        case nsXPTType::T_BSTR:
            // XXX implement BSTR
            NS_ASSERTION(0,"'BSTR' string params not supported");
            return JS_FALSE;

        case nsXPTType::T_CHAR_STR:
        {
            char* bytes;
            JSString* str;

            if(!(str = JS_ValueToString(cx, s))||
               !(bytes = JS_GetStringBytes(str)))
            {
                // XXX should report error
                return JS_FALSE;
            }
            if(al)
            {
                int len = strlen(bytes)+1;
                if(!(*((void**)d) = al->Alloc(len)))
                {
                    // XXX should report error
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
            jschar* chars;
            JSString* str;

            if(!(str = JS_ValueToString(cx, s))||
               !(chars = JS_GetStringChars(str)))
            {
                // XXX should report error
                return JS_FALSE;
            }
            if(al)
            {
                int byte_len = (JS_GetStringLength(str)+1)*sizeof(jschar);
                if(!(*((void**)d) = al->Alloc(byte_len)))
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

            // only wrpa JSObjects
            if(!JSVAL_IS_OBJECT(s) ||
               (!(obj = JSVAL_TO_OBJECT(s))))
            {
                // XXX should report error
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

