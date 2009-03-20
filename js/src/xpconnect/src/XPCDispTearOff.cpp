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
 * \file XPCDispTearOff.cpp
 * Contains the implementation of the XPCDispTearoff class
 */
#include "xpcprivate.h"

/**
 * Sets the COM error from a result code and text message. This is the base
 * implementation for subsequent string based overrides
 * @param hResult the COM error code to be used
 * @param message the message to put in the error
 * @return the error code passed in via hResult
 */
static HRESULT Error(HRESULT hResult, const CComBSTR & message)
{
    CComPtr<ICreateErrorInfo> pCreateError;
    CComPtr<IErrorInfo> pError;
    HRESULT result = CreateErrorInfo(&pCreateError);
    if(FAILED(result))
        return E_NOTIMPL;
    result = pCreateError->QueryInterface(&pError);
    if(FAILED(result))
        return E_NOTIMPL;
    result = pCreateError->SetDescription(message);
    if(FAILED(result))
        return E_NOTIMPL;
    result = pCreateError->SetGUID(IID_IDispatch);
    if(FAILED(result))
        return E_NOTIMPL;
    CComBSTR source(L"@mozilla.XPCDispatchTearOff");
    result = pCreateError->SetSource(source);
    if(FAILED(result))
        return E_NOTIMPL;
    result = SetErrorInfo(0, pError);
    if(FAILED(result))
        return E_NOTIMPL;
    return hResult;
}


/**
 * Sets the COM error from a result code and text message
 * @param hResult the COM error code to be used
 * @param message the message to put in the error
 * @return the error code passed in via hResult
 */
inline
HRESULT Error(HRESULT hResult, const char * message)
{
    CComBSTR someText(message);
    return Error(hResult, someText);
}

/**
 * Helper function that converts an exception to a string
 * @param exception
 * @return the description of the exception
 */
static void BuildMessage(nsIException * exception, nsCString & result)
{
    nsXPIDLCString msg;
    exception->GetMessageMoz(getter_Copies(msg));
    nsXPIDLCString filename;
    exception->GetFilename(getter_Copies(filename));

    PRUint32 lineNumber;
    if(NS_FAILED(exception->GetLineNumber(&lineNumber)))
        lineNumber = 0;
    result = "Error in file ";
    result += filename;
    result += ",#";
    result.AppendInt(lineNumber);
    result += " : ";
    result += msg;
}

/**
 * Sets the COM error given an nsIException
 * @param exception the exception being set
 */
inline
static void SetCOMError(nsIException * exception)
{
    nsCString message;
    BuildMessage(exception, message);
    Error(E_FAIL, message.get());
}

XPCDispatchTearOff::XPCDispatchTearOff(nsIXPConnectWrappedJS * wrappedJS) :
    mWrappedJS(wrappedJS),
    mCOMTypeInfo(nsnull),
    mRefCnt(0)
{
}

XPCDispatchTearOff::~XPCDispatchTearOff()
{
    NS_IF_RELEASE(mCOMTypeInfo);
}

NS_COM_IMPL_ADDREF(XPCDispatchTearOff)
NS_COM_IMPL_RELEASE(XPCDispatchTearOff)

// See bug 127982:
//
// Microsoft's InlineIsEqualGUID global function is multiply defined
// in ATL and/or SDKs with varying namespace requirements. To save the control
// from future grief, this method is used instead. 
static inline BOOL _IsEqualGUID(REFGUID rguid1, REFGUID rguid2)
{
   return (
	  ((PLONG) &rguid1)[0] == ((PLONG) &rguid2)[0] &&
	  ((PLONG) &rguid1)[1] == ((PLONG) &rguid2)[1] &&
	  ((PLONG) &rguid1)[2] == ((PLONG) &rguid2)[2] &&
	  ((PLONG) &rguid1)[3] == ((PLONG) &rguid2)[3]);
}

STDMETHODIMP XPCDispatchTearOff::InterfaceSupportsErrorInfo(REFIID riid)
{
    static const IID* arr[] = 
    {
        &IID_IDispatch,
    };

    for(int i=0;i<sizeof(arr)/sizeof(arr[0]);i++)
    {
        if(_IsEqualGUID(*arr[i],riid))
            return S_OK;
    }
    return S_FALSE;
}

STDMETHODIMP XPCDispatchTearOff::QueryInterface(const struct _GUID & guid,
                                              void ** pPtr)
{
    if(IsEqualIID(guid, IID_IDispatch))
    {
        *pPtr = static_cast<IDispatch*>(this);
        NS_ADDREF_THIS();
        return NS_OK;
    }

    if(IsEqualIID(guid, IID_ISupportErrorInfo))
    {
        *pPtr = static_cast<IDispatch*>(this);
        NS_ADDREF_THIS();
        return NS_OK;
    }

    return mWrappedJS->QueryInterface(XPCDispIID2nsIID(guid), pPtr);
}

STDMETHODIMP XPCDispatchTearOff::GetTypeInfoCount(unsigned int FAR * pctinfo)
{
    *pctinfo = 1;
    return S_OK;
}

XPCDispTypeInfo * XPCDispatchTearOff::GetCOMTypeInfo()
{
    // If one was already created return it
    if(mCOMTypeInfo)
        return mCOMTypeInfo;
    // Build a new one, save the pointer and return it
    XPCCallContext ccx(NATIVE_CALLER);
    if(!ccx.IsValid())
        return nsnull;
    JSObject* obj = GetJSObject();
    if(!obj)
        return nsnull;
    mCOMTypeInfo = XPCDispTypeInfo::New(ccx, obj);
    NS_IF_ADDREF(mCOMTypeInfo);
    return mCOMTypeInfo;
}

STDMETHODIMP XPCDispatchTearOff::GetTypeInfo(unsigned int, LCID, 
                                         ITypeInfo FAR* FAR* ppTInfo)
{
    *ppTInfo = GetCOMTypeInfo();
    NS_ADDREF(*ppTInfo);
    return S_OK;
}

STDMETHODIMP XPCDispatchTearOff::GetIDsOfNames(REFIID riid, 
                                           OLECHAR FAR* FAR* rgszNames, 
                                           unsigned int cNames, LCID  lcid,
                                           DISPID FAR* rgDispId)
{
    ITypeInfo * pTypeInfo = GetCOMTypeInfo();
    if(pTypeInfo != nsnull)
    {
        return pTypeInfo->GetIDsOfNames(rgszNames, cNames, rgDispId);
    }
    return S_OK;
}

void
xpcWrappedJSErrorReporter(JSContext *cx, const char *message,
                          JSErrorReport *report);

STDMETHODIMP XPCDispatchTearOff::Invoke(DISPID dispIdMember, REFIID riid, 
                                        LCID lcid, WORD wFlags,
                                        DISPPARAMS FAR* pDispParams, 
                                        VARIANT FAR* pVarResult, 
                                        EXCEPINFO FAR* pExcepInfo, 
                                        unsigned int FAR* puArgErr)
{
    XPCDispTypeInfo* pTypeInfo = GetCOMTypeInfo();
    if(!pTypeInfo)
    {
        return E_FAIL;
    }
    XPCCallContext ccx(NATIVE_CALLER);
    XPCContext* xpcc;
    JSContext* cx;
    if(ccx.IsValid())
    {
        xpcc = ccx.GetXPCContext();
        cx = ccx.GetJSContext();
    }
    else
    {
        xpcc = nsnull;
        cx = nsnull;
    }
    // Get the name as a flat string
    // This isn't that efficient, but we have to make the conversion somewhere
    NS_LossyConvertUTF16toASCII name(pTypeInfo->GetNameForDispID(dispIdMember));
    if(name.IsEmpty())
        return E_FAIL;
    // Decide if this is a getter or setter
    PRBool getter = (wFlags & DISPATCH_PROPERTYGET) != 0;
    PRBool setter = (wFlags & DISPATCH_PROPERTYPUT) != 0;
    // It's a property
    if(getter || setter)
    {
        jsval val;
        uintN err;
        JSObject* obj;
        if(getter)
        {
            // Get the property and convert the value
            obj = GetJSObject();
            if(!obj)
                return E_FAIL;
            if(!JS_GetProperty(cx, obj, name.get(), &val))
            {
                nsCString msg("Unable to retrieve property ");
                msg += name;
                return Error(E_FAIL, msg.get());
            }
            if(!XPCDispConvert::JSToCOM(ccx, val, *pVarResult, err))
            {
                nsCString msg("Failed to convert value from JS property ");
                msg += name;
                return Error(E_FAIL, msg.get());
            }
        }
        else if(pDispParams->cArgs > 0)
        {
            // Convert the property and then set it
            if(!XPCDispConvert::COMToJS(ccx, pDispParams->rgvarg[0], val, err))
            {
                nsCString msg("Failed to convert value for JS property ");
                msg += name;
                return Error(E_FAIL, msg.get());
            }
            AUTO_MARK_JSVAL(ccx, &val);
            obj = GetJSObject();
            if(!obj)
                return Error(E_FAIL, "The JS wrapper did not return a JS object");
            if(!JS_SetProperty(cx, obj, name.get(), &val))
            {
                nsCString msg("Unable to set property ");
                msg += name;
                return Error(E_FAIL, msg.get());
            }
        }
    }
    else // We're invoking a function
    {
        jsval* stackbase = nsnull;
        jsval* sp = nsnull;
        uint8 i;
        uint8 argc = pDispParams->cArgs;
        uint8 stack_size;
        jsval result;
        uint8 paramCount=0;
        nsresult retval = NS_ERROR_FAILURE;
        nsresult pending_result = NS_OK;
        JSBool success;
        JSBool readyToDoTheCall = JS_FALSE;
        uint8 outConversionFailedIndex;
        JSObject* obj;
        jsval fval;
        nsCOMPtr<nsIException> xpc_exception;
        void* mark;
        JSBool foundDependentParam;
        JSObject* thisObj;
        AutoScriptEvaluate scriptEval(ccx);
        XPCJSRuntime* rt = ccx.GetRuntime();
        int j;

        thisObj = obj = GetJSObject();;

        if(!cx || !xpcc)
            goto pre_call_clean_up;

        scriptEval.StartEvaluating(xpcWrappedJSErrorReporter);

        xpcc->SetPendingResult(pending_result);
        xpcc->SetException(nsnull);
        ccx.GetThreadData()->SetException(nsnull);

        // We use js_AllocStack, js_Invoke, and js_FreeStack so that the gcthings
        // we use as args will be rooted by the engine as we do conversions and
        // prepare to do the function call. This adds a fair amount of complexity,
        // but is a good optimization compared to calling JS_AddRoot for each item.

        // setup stack

        // allocate extra space for function and 'this'
        stack_size = argc + 2;


        // In the xpidl [function] case we are making sure now that the 
        // JSObject is callable. If it is *not* callable then we silently 
        // fallback to looking up the named property...
        // (because jst says he thinks this fallback is 'The Right Thing'.)
        //
        // In the normal (non-function) case we just lookup the property by 
        // name and as long as the object has such a named property we go ahead
        // and try to make the call. If it turns out the named property is not
        // a callable object then the JS engine will throw an error and we'll
        // pass this along to the caller as an exception/result code.
        fval = OBJECT_TO_JSVAL(obj);
        if(JS_TypeOfValue(ccx, fval) != JSTYPE_FUNCTION && 
            !JS_GetProperty(cx, obj, name.get(), &fval))
        {
            // XXX We really want to factor out the error reporting better and
            // specifically report the failure to find a function with this name.
            // This is what we do below if the property is found but is not a
            // function. We just need to factor better so we can get to that
            // reporting path from here.
            goto pre_call_clean_up;
        }

        // if stack_size is zero then we won't be needing a stack
        if(stack_size && !(stackbase = sp = js_AllocStack(cx, stack_size, &mark)))
        {
            retval = NS_ERROR_OUT_OF_MEMORY;
            goto pre_call_clean_up;
        }

        // this is a function call, so push function and 'this'
        if(stack_size != argc)
        {
            *sp++ = fval;
            *sp++ = OBJECT_TO_JSVAL(thisObj);
        }

        // make certain we leave no garbage in the stack
        for(i = 0; i < argc; i++)
        {
            sp[i] = JSVAL_VOID;
        }

        uintN err;
        // build the args
        // NOTE: COM expects args in DISPPARAMS to be in reverse order
        for (j = argc - 1; j >= 0; --j )
        {
            jsval val;
            if((pDispParams->rgvarg[j].vt & VT_BYREF) == 0)
            {
                if(!XPCDispConvert::COMToJS(ccx, pDispParams->rgvarg[j], val, err))
                    goto pre_call_clean_up;
                *sp++ = val;
            }
            else
            {
                // create an 'out' object
                JSObject* out_obj = JS_NewObject(cx, nsnull, nsnull, nsnull);
                if(!out_obj)
                {
                    retval = NS_ERROR_OUT_OF_MEMORY;
                    goto pre_call_clean_up;
                }
                // We'll assume in/out
                // TODO: I'm not sure we tell out vs in/out
                JS_SetPropertyById(cx, out_obj,
                        rt->GetStringID(XPCJSRuntime::IDX_VALUE),
                        &val);
                *sp++ = OBJECT_TO_JSVAL(out_obj);
            }
        }

        readyToDoTheCall = JS_TRUE;

pre_call_clean_up:

        if(!readyToDoTheCall)
            goto done;

        // do the deed - note exceptions

        JS_ClearPendingException(cx);

        if(!JSVAL_IS_PRIMITIVE(fval))
        {
            success = js_Invoke(cx, argc, stackbase, 0);
            result = stackbase[0];
        }
        else
        {
            // The property was not an object so can't be a function.
            // Let's build and 'throw' an exception.

            static const nsresult code =
                    NS_ERROR_XPC_JSOBJECT_HAS_NO_FUNCTION_NAMED;
            static const char format[] = "%s \"%s\"";
            const char * msg;
            char* sz = nsnull;

            if(nsXPCException::NameAndFormatForNSResult(code, nsnull, &msg) && msg)
                sz = JS_smprintf(format, msg, name);

            nsCOMPtr<nsIException> e;

            XPCConvert::ConstructException(code, sz, "IDispatch", name.get(),
                                           nsnull, getter_AddRefs(e), nsnull, nsnull);
            xpcc->SetException(e);
            if(sz)
                JS_smprintf_free(sz);
        }

        if(!success)
        {
            retval = nsXPCWrappedJSClass::CheckForException(ccx, name.get(),
                                                            "IDispatch",
                                                            PR_FALSE);
            goto done;
        }

        ccx.GetThreadData()->SetException(nsnull); // XXX necessary?

        // convert out args and result
        // NOTE: this is the total number of native params, not just the args
        // Convert independent params only.
        // When we later convert the dependent params (if any) we will know that
        // the params upon which they depend will have already been converted -
        // regardless of ordering.

        outConversionFailedIndex = paramCount;
        foundDependentParam = JS_FALSE;
        if(JSVAL_IS_VOID(result) || XPCDispConvert::JSToCOM(ccx, result, *pVarResult, err))
        {
            for(i = 0; i < paramCount; i++)
            {
                jsval val;
                if(JSVAL_IS_PRIMITIVE(stackbase[i+2]) ||
                        !JS_GetPropertyById(cx, JSVAL_TO_OBJECT(stackbase[i+2]),
                            rt->GetStringID(XPCJSRuntime::IDX_VALUE),
                            &val))
                {
                    outConversionFailedIndex = i;
                    break;
                }

            }
        }

        if(outConversionFailedIndex != paramCount)
        {
            // We didn't manage all the result conversions!
            // We have to cleanup any junk that *did* get converted.

            for(PRUint32 index = 0; index < outConversionFailedIndex; index++)
            {
                if((pDispParams->rgvarg[index].vt & VT_BYREF) != 0)
                {
                    VariantClear(pDispParams->rgvarg + i);
                }
            }
        }
        else
        {
            // set to whatever the JS code might have set as the result
            retval = pending_result;
        }

done:
        if(sp)
            js_FreeStack(cx, mark);

        // TODO: I think we may need to translate this error, 
        // for now we'll pass through
        return retval;
    }
    return S_OK;
}

inline
JSObject* XPCDispatchTearOff::GetJSObject()
{
    JSObject* obj;
    if(NS_SUCCEEDED(mWrappedJS->GetJSObject(&obj)))
        return obj;
    return nsnull;
}
