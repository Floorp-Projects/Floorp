/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the IDispatch implementation for XPConnect
 *
 * The Initial Developer of the Original Code is
 * David Bradley.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "xpcprivate.h"

/**
 * Returns the COM type for a given JS value
 */
VARTYPE XPCDispConvert::JSTypeToCOMType(XPCCallContext& ccx, jsval val)
{
    if(JSVAL_IS_STRING(val))
    {
        return VT_BSTR;
    }
    if(JSVAL_IS_INT(val))
    {
        return VT_I4;
    }
    if(JSVAL_IS_DOUBLE(val))
    {
        return VT_R8;
    }
    if(JSVAL_IS_OBJECT(val))
    {
        if(JS_IsArrayObject(ccx, JSVAL_TO_OBJECT(val)))
            return VT_ARRAY | VT_VARIANT;
        return VT_DISPATCH;
    }
    if(JSVAL_IS_BOOLEAN(val))
    {
        return VT_BOOL;
    }
    if(JSVAL_IS_VOID(val))
    {
        return VT_EMPTY;
    }
    if(JSVAL_IS_NULL(val))
    {
        return VT_NULL;
    }
    NS_ERROR("XPCDispConvert::JSTypeToCOMType was unable to identify the type of the jsval");
    return VT_EMPTY;
}

JSBool XPCDispConvert::JSArrayToCOMArray(XPCCallContext& ccx, JSObject *obj, 
                                        VARIANT & var, nsresult& err)
{
    err = NS_OK;
    jsuint len;
    if(!JS_GetArrayLength(ccx, obj, &len))
    {
        // TODO: I think we should create a specific error for this
        err = NS_ERROR_XPC_NOT_ENOUGH_ELEMENTS_IN_ARRAY;
        return PR_FALSE;
    }
    SAFEARRAY * array = SafeArrayCreateVector(VT_VARIANT, 0, len);
    for(long index = 0; index < len; ++index)
    {
        _variant_t arrayVar;
        jsval val;
        if(JS_GetElement(ccx, obj, index, &val) &&
           JSToCOM(ccx, val, arrayVar, err))
        {
            SafeArrayPutElement(array, &index, &arrayVar);
        }
        else
        {
            if(err == NS_OK)
                err = NS_ERROR_FAILURE;
            // This cleans up the elements as well
            SafeArrayDestroyData(array);
            return JS_FALSE;
        }
    }
    var.vt = VT_ARRAY | VT_VARIANT;
    var.parray = array;
    return JS_TRUE;
}

JSBool XPCDispConvert::JSToCOM(XPCCallContext& ccx,
                              jsval src,
                              VARIANT & dest,
                              nsresult& err)
{
    err = NS_OK;
    if(JSVAL_IS_STRING(src))
    {
        JSString* str = JSVAL_TO_STRING(src);
        if(!str)
        {
            err = NS_ERROR_XPC_BAD_CONVERT_NATIVE;
            return JS_FALSE;
        }

        jschar * chars = JS_GetStringChars(str);
        if(!chars)
        {
            err = NS_ERROR_XPC_BAD_CONVERT_NATIVE;
            return JS_FALSE;
        }

        CComBSTR val(chars);
        dest.vt = VT_BSTR;
        dest.bstrVal = val.Detach();
    }
    else if(JSVAL_IS_INT(src))
    {
        dest.vt = VT_I4;
        dest.lVal = JSVAL_TO_INT(src);
    }
    else if(JSVAL_IS_DOUBLE(src))
    {
        dest.vt = VT_R8;
        dest.dblVal = *JSVAL_TO_DOUBLE(src);
    }
    else if(JSVAL_IS_OBJECT(src))
    {
        JSObject * obj = JSVAL_TO_OBJECT(src);
        if(JS_IsArrayObject(ccx, obj))
        {
            return JSArrayToCOMArray(ccx, obj, dest, err);
        }
        else
        {
            // only wrap JSObjects
            IUnknown * pUnknown;
            XPCConvert::JSObject2NativeInterface(
                ccx, 
                (void**)&pUnknown, 
                obj, 
                &NSID_IDISPATCH,
                nsnull, 
                &err);
            dest.vt = VT_DISPATCH;
            pUnknown->QueryInterface(IID_IDispatch, 
                                     NS_REINTERPRET_CAST(void**,
                                                         &dest.pdispVal));
        }
    }
    else if(JSVAL_IS_BOOLEAN(src))
    {
        dest.vt = VT_BOOL;
        dest.boolVal = JSVAL_TO_BOOLEAN(src);
    }
    else if(JSVAL_IS_VOID(src))
    {
        dest.vt = VT_EMPTY;
    }
    else if(JSVAL_IS_NULL(src))
    {
        dest.vt = VT_NULL;
    }
    else  // Unknown type
    {
        err = NS_ERROR_XPC_BAD_CONVERT_NATIVE;
        return JS_FALSE;
    }
    return JS_TRUE;
}

JSBool XPCDispConvert::COMArrayToJSArray(XPCCallContext& ccx,
                                        const _variant_t & src,
                                        jsval & dest, nsresult& err)
{
    err = NS_OK;
    // We only support one dimensional arrays for now
    if(SafeArrayGetDim(src.parray) != 1)
    {
        err = NS_ERROR_FAILURE;
        return JS_FALSE;
    }
    long size;
    HRESULT hr = SafeArrayGetUBound(src.parray, 1, &size);
    if(FAILED(hr))
    {
        err = NS_ERROR_FAILURE;
        return JS_FALSE;
    }
    JSObject * array = JS_NewArrayObject(ccx, size, nsnull);
    if(!array)
    {
        err = NS_ERROR_OUT_OF_MEMORY;
    }
    // Devine the type of our array
    _variant_t var;
    if((src.vt & VT_ARRAY) != 0)
    {
        var.vt = src.vt & ~VT_ARRAY;
    }
    else // This was maybe a VT_SAFEARRAY
    {
        SafeArrayGetVartype(src.parray, &var.vt);
    }
    jsval val;
    for(long index = 0; index <= size; ++index)
    {
        hr = SafeArrayGetElement(src.parray, &index, &var.byref);
        if(FAILED(hr))
        {
            err = NS_ERROR_FAILURE;
            return JS_FALSE;
        }
        if(!COMToJS(ccx, var, val, err))
            return JS_FALSE;
        JS_SetElement(ccx, array, index, &val);
    }
    dest = OBJECT_TO_JSVAL(array);
    return JS_TRUE;
}

inline
jsval NumberToJSVal(JSContext* cx, int value)
{
    jsval val;
    if (INT_FITS_IN_JSVAL(value))
        val = INT_TO_JSVAL(value);
    else
    {
        if (!JS_NewDoubleValue(cx, NS_STATIC_CAST(jsdouble,value), &val))
            val = JSVAL_ZERO;
    }
    return val;
}

inline
jsval NumberToJSVal(JSContext* cx, double value)
{
    jsval val;
    if (JS_NewDoubleValue(cx, NS_STATIC_CAST(jsdouble, value), &val))
        return val;
    else
        return JSVAL_ZERO;
}

JSBool XPCDispConvert::COMToJS(XPCCallContext& ccx, const _variant_t & src,
                              jsval& dest, nsresult& err)
{
    err = NS_OK;
    if(src.vt & VT_ARRAY || src.vt == VT_SAFEARRAY)
    {
        return COMArrayToJSArray(ccx, src, dest, err);
    }
    switch (src.vt & ~(VT_BYREF))
    {
        case VT_BSTR:
        {
            JSString * str;
            if(src.vt & VT_BYREF)
                str = JS_NewUCStringCopyZ(ccx, *src.pbstrVal);
            else
                str = JS_NewUCStringCopyZ(ccx, src.bstrVal);
            if(!str)
            {
                err = NS_ERROR_OUT_OF_MEMORY;
                return JS_FALSE;
            }
            dest = STRING_TO_JSVAL(str);
        }
        break;
        case VT_I4:
        {
            dest = NumberToJSVal(ccx, src.lVal);
        }
        break;
        case VT_UI1:
        {
            dest = INT_TO_JSVAL(src.bVal);
        }
        break;
        case VT_I2:
        {
            dest = INT_TO_JSVAL(src.iVal);
        }
        break;
        case VT_R4:
        {
            dest = NumberToJSVal(ccx, src.fltVal);
        }
        break;
        case VT_R8:
        {
            dest = NumberToJSVal(ccx, src.dblVal);
        }
        break;
        case VT_BOOL:
        {
            dest = BOOLEAN_TO_JSVAL(src.boolVal);
        }
        break;
        case VT_DISPATCH:
        {
            XPCDispObject::COMCreateFromIDispatch(src.pdispVal, ccx,
                                                  JS_GetGlobalObject(ccx),
                                                  &dest);
        }
        break;
        /**
         * Currently unsupported conversion types
         */
        case VT_ERROR:
        case VT_CY:
        case VT_DATE:
        case VT_UNKNOWN:
        case VT_I1:
        case VT_UI2:
        case VT_UI4:
        case VT_INT:
        case VT_UINT:
        default:
        {
            err = NS_ERROR_XPC_BAD_CONVERT_JS;
            return JS_FALSE;
        }
    }
    return JS_TRUE;
}
