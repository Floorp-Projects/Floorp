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
 * The Original Code is the IDispatch implementation for XPConnect.
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

/**
 * \file XPCDispConvert.cpp
 * Contains the implementation of the XPCDispConvert class
 */

#include "xpcprivate.h"

VARTYPE XPCDispConvert::JSTypeToCOMType(XPCCallContext& ccx, jsval val)
{
    if(JSVAL_IS_PRIMITIVE(val))
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
    }
    else
    {
        if(JS_IsArrayObject(ccx, JSVAL_TO_OBJECT(val)))
            return VT_ARRAY | VT_VARIANT;
        return VT_DISPATCH;
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
    // Create the safe array of variants and populate it
    SAFEARRAY * array = nsnull;
    VARIANT* varArray = 0;
    for(jsuint index = 0; index < len; ++index)
    {
        jsval val;
        if(JS_GetElement(ccx, obj, index, &val))
        {
            if(!JSVAL_IS_VOID(val))
            {
                if(!array)
                {
                    // Create an array that starts at index, and has len 
                    // elements
                    array = SafeArrayCreateVector(VT_VARIANT, index, len - index);
                    if(!array)
                    {
                        err = NS_ERROR_OUT_OF_MEMORY;
                        return JS_FALSE;
                    }
                    if(FAILED(SafeArrayAccessData(array, reinterpret_cast<void**>(&varArray))))
                    {
                        err = NS_ERROR_FAILURE;
                        return JS_FALSE;
                    }
                }
                if(!JSToCOM(ccx, val, *varArray, err))
                {
                    SafeArrayUnaccessData(array);
                    err = NS_ERROR_FAILURE;
                    // This cleans up the elements as well
                    SafeArrayDestroyData(array);
                    return JS_FALSE;
                }
            }
            if(varArray)
                ++varArray;
        }
    }
    if(!array)
    {
        array = SafeArrayCreateVector(VT_VARIANT, 0, 0);
        if(!array)
        {
            err = NS_ERROR_OUT_OF_MEMORY;
            return JS_FALSE;
        }
    }
    else
    {
        SafeArrayUnaccessData(array);
    }
    var.vt = VT_ARRAY | VT_VARIANT;
    var.parray = array;
    return JS_TRUE;
}

#define XPC_ASSIGN(src, dest, data) *dest.p##data = src.data

/**
 * Copies a variant to a by ref variant
 * NOTE: This does not perform any reference counting. It simply does
 * a copy of the values, it's up to the caller to manage any ownership issues
 * @param src the variant to be copied
 * @param dest the destination for the copy
 * @return JS_TRUE if the copy was performed JS_FALSE if it failed
 */
inline
JSBool xpc_CopyVariantByRef(VARIANT & src, VARIANT & dest)
{
    VARIANT temp;
    VARTYPE vt = dest.vt & ~(VT_BYREF);
    if(vt != src.vt)
    {
        // TODO: This fails more than I had hoped, we may want to
        // add some logic to handle more conversions
        if(FAILED(VariantChangeType(&temp, &src, VARIANT_ALPHABOOL, vt)))
        {
            return JS_FALSE;
        }
    }
    else
        temp = src;
    switch (vt)
    {
        case VT_I2:
        {
            XPC_ASSIGN(temp, dest, iVal);
        }
        break;
	    case VT_I4:
        {
            XPC_ASSIGN(temp, dest, lVal);
        }
        break;
	    case VT_R4:
        {
            XPC_ASSIGN(temp, dest, fltVal);
        }
        break;
	    case VT_R8:
        {
            XPC_ASSIGN(temp, dest, dblVal);
        }
        break;
	    case VT_CY:
        {
            XPC_ASSIGN(temp, dest, cyVal);
        }
        break;
	    case VT_DATE:
        {
            XPC_ASSIGN(temp, dest, date);
        }
        break;
	    case VT_BSTR:
        {
            XPC_ASSIGN(temp, dest, bstrVal);
        }
        break;
	    case VT_DISPATCH:
        {
            XPC_ASSIGN(temp, dest, pdispVal);
        }
        break;
	    case VT_ERROR:
        {
            XPC_ASSIGN(temp, dest, scode);
        }
        break;
	    case VT_BOOL:
        {
            XPC_ASSIGN(temp, dest, boolVal);
        }
        break;
	    case VT_VARIANT:
        {
            // Not Supported right now
            return JS_FALSE;
        }
        break;
	    case VT_I1:
        {
            XPC_ASSIGN(temp, dest, cVal);
        }
        break;
	    case VT_UI1:
        {
            XPC_ASSIGN(temp, dest, bVal);
        }
        break;
	    case VT_UI2:
        {
            XPC_ASSIGN(temp, dest, iVal);
        }
        break;
	    case VT_UI4:
        {
            XPC_ASSIGN(temp, dest, uiVal);
        }
        break;
	    case VT_INT:
        {
            XPC_ASSIGN(temp, dest, intVal);
        }
        break;
	    case VT_UINT:
        {
            XPC_ASSIGN(temp, dest, uintVal);
        }
        break;
        default:
        {
            return JS_FALSE;
        }
    }
    return JS_TRUE;
}

JSBool XPCDispConvert::JSToCOM(XPCCallContext& ccx,
                               jsval src,
                               VARIANT & dest,
                               nsresult& err,
                               JSBool isByRef)
{
    err = NS_OK;
    VARIANT byRefVariant;
    VARIANT * varDest = isByRef ? &byRefVariant : &dest;
    varDest->vt = JSTypeToCOMType(ccx, src);
    switch (varDest->vt)
    {
        case VT_BSTR:
        {
            JSString* str = JSVAL_TO_STRING(src);
            jschar * chars = JS_GetStringChars(str);
            if(!chars)
            {
                err = NS_ERROR_XPC_BAD_CONVERT_NATIVE;
                // Avoid cleaning up garbage
                varDest->vt = VT_EMPTY;
                return JS_FALSE;
            }

            CComBSTR val(JS_GetStringLength(str),
                         reinterpret_cast<const WCHAR *>(chars));
            varDest->bstrVal = val.Detach();
        }
        break;
        case VT_I4:
        {
            varDest->vt = VT_I4;
            varDest->lVal = JSVAL_TO_INT(src);
        }
        break;
        case VT_R8:
        {
            varDest->vt = VT_R8;
            varDest->dblVal = *JSVAL_TO_DOUBLE(src);
        }
        break;
        case VT_EMPTY:
        case VT_NULL:
        break;
        case VT_ARRAY | VT_VARIANT:
        {
            JSObject * obj = JSVAL_TO_OBJECT(src);
            return JSArrayToCOMArray(ccx, obj, *varDest, err);
        }
        break;
        case VT_DISPATCH:
        {
            JSObject * obj = JSVAL_TO_OBJECT(src);
            IUnknown * pUnknown = nsnull;
            if(!XPCConvert::JSObject2NativeInterface(
                ccx, 
                (void**)&pUnknown, 
                obj, 
                &NSID_IDISPATCH,
                nsnull, 
                &err))
            {
                // Avoid cleaning up garbage
                varDest->vt = VT_EMPTY;
                return JS_FALSE;
            }
            varDest->vt = VT_DISPATCH;
            pUnknown->QueryInterface(IID_IDispatch, 
                                     reinterpret_cast<void**>
                                                     (&varDest->pdispVal));
            NS_IF_RELEASE(pUnknown);
        }
        break;
        case VT_BOOL:
        {
            varDest->boolVal = JSVAL_TO_BOOLEAN(src) ? VARIANT_TRUE : VARIANT_FALSE;
        }
        break;
        default:
        {
            NS_ERROR("This is out of synce with XPCDispConvert::JSTypeToCOMType");
            err = NS_ERROR_XPC_BAD_CONVERT_NATIVE;
            // Avoid cleaning up garbage
            varDest->vt = VT_EMPTY;
            return JS_FALSE;
        }
        break;
    }
    if(isByRef)
    {
        if(!xpc_CopyVariantByRef(byRefVariant, dest))
        {
            // Avoid cleaning up garbage
            dest.vt = VT_EMPTY;
        }
    }
    return JS_TRUE;
}

JSBool XPCDispConvert::COMArrayToJSArray(XPCCallContext& ccx,
                                         const VARIANT & src,
                                         jsval & dest, nsresult& err)
{
    err = NS_OK;
    // We only support one dimensional arrays for now
    if(SafeArrayGetDim(src.parray) != 1)
    {
        err = NS_ERROR_FAILURE;
        return JS_FALSE;
    }
    // Get the upper bound;
    long ubound;
    if(FAILED(SafeArrayGetUBound(src.parray, 1, &ubound)))
    {
        err = NS_ERROR_FAILURE;
        return JS_FALSE;
    }
    // Get the lower bound
    long lbound;
    if(FAILED(SafeArrayGetLBound(src.parray, 1, &lbound)))
    {
        err = NS_ERROR_FAILURE;
        return JS_FALSE;
    }
    // Create the JS Array
    JSObject * array = JS_NewArrayObject(ccx, ubound - lbound + 1, nsnull);
    if(!array)
    {
        err = NS_ERROR_OUT_OF_MEMORY;
        return JS_FALSE;
    }
    AUTO_MARK_JSVAL(ccx, OBJECT_TO_JSVAL(array));
    // Divine the type of our array
    VARTYPE vartype;
    if((src.vt & VT_ARRAY) != 0)
    {
        vartype = src.vt & ~VT_ARRAY;
    }
    else // This was maybe a VT_SAFEARRAY
    {
#ifndef WINCE
        if(FAILED(SafeArrayGetVartype(src.parray, &vartype)))
            return JS_FALSE;
#else
        return JS_FALSE;
#endif
    }
    jsval val = JSVAL_NULL;
    AUTO_MARK_JSVAL(ccx, &val);
    for(long index = lbound; index <= ubound; ++index)
    {
        HRESULT hr;
        _variant_t var;
        if(vartype == VT_VARIANT)
        {
            hr = SafeArrayGetElement(src.parray, &index, &var);
        }
        else
        {
            var.vt = vartype;
            hr = SafeArrayGetElement(src.parray, &index, &var.byref);
        }
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

/**
 * Converts a string to a jsval
 * @param cx the JSContext
 * @param str the unicode string to be converted
 * @param len the length of the string being converted
 * @return the jsval representing the string
 */
inline
jsval StringToJSVal(JSContext* cx, const PRUnichar * str, PRUint32 len)
{
    JSString * s = JS_NewUCStringCopyN(cx,
                                       reinterpret_cast<const jschar *>(str),
                                       len);
    if(s)
        return STRING_TO_JSVAL(s);
    else
        return JSVAL_NULL;
}


#define VALUE(val) (isPtr ? *src.p##val : src.val)
JSBool XPCDispConvert::COMToJS(XPCCallContext& ccx, const VARIANT& src,
                               jsval& dest, nsresult& err)
{
    err = NS_OK;
    if(src.vt & VT_ARRAY || src.vt == VT_SAFEARRAY)
    {
        return COMArrayToJSArray(ccx, src, dest, err);
    }
    PRBool isPtr = src.vt & VT_BYREF;
    switch (src.vt & ~(VT_BYREF))
    {
        case VT_UINT:
        {
            return JS_NewNumberValue(ccx, VALUE(uintVal), &dest);
        }
        break;
        case VT_UI4:
        {
            return JS_NewNumberValue(ccx, VALUE(ulVal), &dest);
        }
        break;
        case VT_INT:
        {
            return JS_NewNumberValue(ccx, VALUE(intVal), &dest);
        }
        break;
        case VT_I4:
        {
            return JS_NewNumberValue(ccx, VALUE(lVal), &dest);
        }
        break;
        case VT_UI1:
        {
            dest = INT_TO_JSVAL(VALUE(bVal));
        }
        break;
        case VT_I1:
        {
            dest = INT_TO_JSVAL(VALUE(cVal));
        }
        break;
        case VT_UI2:
        {
            dest = INT_TO_JSVAL(VALUE(uiVal));
        }
        break;
        case VT_I2:
        {
            dest = INT_TO_JSVAL(VALUE(iVal));
        }
        break;
        case VT_R4:
        {
            return JS_NewNumberValue(ccx, VALUE(fltVal), &dest);
        }
        break;
        case VT_R8:
        {
            return JS_NewNumberValue(ccx, VALUE(dblVal), &dest);
        }
        break;
        case VT_BOOL:
        {
            dest = BOOLEAN_TO_JSVAL(VALUE(boolVal) != VARIANT_FALSE ? JS_TRUE : JS_FALSE);
        }
        break;
        case VT_DISPATCH:
        {
            XPCDispObject::WrapIDispatch(VALUE(pdispVal), ccx,
                                         JS_GetGlobalObject(ccx), &dest);
        }
        break;
        case VT_DATE:
        {
            // Convert date to string and frees it when we're done
            _bstr_t str(src);
            dest = StringToJSVal(ccx, str, str.length());
        }
        break;
        case VT_EMPTY:
        {
            dest = JSVAL_VOID;
        }
        break;
        case VT_NULL:
        {
            dest = JSVAL_NULL;
        }
        break;
        case VT_ERROR:
        {
            return JS_NewNumberValue(ccx, VALUE(scode), &dest);
        }
        break;
        case VT_CY:
        {
            return JS_NewNumberValue(
                ccx, 
                static_cast<double>
                           (isPtr ? src.pcyVal->int64 : 
                                       src.cyVal.int64) / 100.0,
                &dest);
        }
        break;
        /**
         * Currently unsupported conversion types
         */
        case VT_UNKNOWN:
        default:
        {
            // Last ditch effort to convert to string
            if(FAILED(VariantChangeType(const_cast<VARIANT*>(&src), 
                                         const_cast<VARIANT*>(&src), 
                                         VARIANT_ALPHABOOL, VT_BSTR)))
            {
                err = NS_ERROR_XPC_BAD_CONVERT_JS;
                return JS_FALSE;
            }
            isPtr = FALSE;
        } // Fall through on success
        case VT_BSTR:
        {
            dest = StringToJSVal(ccx, VALUE(bstrVal), SysStringLen(VALUE(bstrVal)));
        }
        break;
    }
    return JS_TRUE;
}
