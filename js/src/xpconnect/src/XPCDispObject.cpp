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
 * \file XPCDispObject.cpp
 * Contains the XPCDispObject class implementation,
 * XPC_IDispatch_GetterSetter, and XPC_IDispatch_CallMethod
 */
#include "xpcprivate.h"
#include "nsIActiveXSecurityPolicy.h"

/**
 * This is COM's IDispatch IID, but in XPCOM's nsID type
 */
const nsID NSID_IDISPATCH = { 0x00020400, 0x0000, 0x0000, { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };

PRBool
XPCDispObject::WrapIDispatch(IDispatch *pDispatch, XPCCallContext &ccx,
                             JSObject *obj, jsval *rval)
{
    if(!pDispatch)
    {
        return PR_FALSE;
    }

    // Wrap the desired COM object
    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    nsresult rv = ccx.GetXPConnect()->WrapNative(
        ccx, obj, reinterpret_cast<nsISupports*>(pDispatch), NSID_IDISPATCH,
        getter_AddRefs(holder));
    if(NS_FAILED(rv) || !holder)
    {
        return PR_FALSE;
    }
    JSObject * jsobj;
    if(NS_FAILED(holder->GetJSObject(&jsobj)))
        return PR_FALSE;
    *rval = OBJECT_TO_JSVAL(jsobj);
    return PR_TRUE;
}

HRESULT XPCDispObject::SecurityCheck(XPCCallContext & ccx, const CLSID & aCID,
                                     IDispatch ** createdObject)
{
    nsresult rv;
    nsCOMPtr<nsIDispatchSupport> dispSupport = do_GetService(NS_IDISPATCH_SUPPORT_CONTRACTID, &rv);
    if(NS_FAILED(rv)) return E_UNEXPECTED;

    PRUint32 hostingFlags = nsIActiveXSecurityPolicy::HOSTING_FLAGS_HOST_NOTHING;
    dispSupport->GetHostingFlags(nsnull, &hostingFlags);
    PRBool allowSafeObjects;
    if(hostingFlags & (nsIActiveXSecurityPolicy::HOSTING_FLAGS_SCRIPT_SAFE_OBJECTS))
        allowSafeObjects = PR_TRUE;
    else
        allowSafeObjects = PR_FALSE;
    PRBool allowAnyObjects;
    if(hostingFlags & (nsIActiveXSecurityPolicy::HOSTING_FLAGS_SCRIPT_ALL_OBJECTS))
        allowAnyObjects = PR_TRUE;
    else
        allowAnyObjects = PR_FALSE;
    if(!allowSafeObjects && !allowAnyObjects)
        return E_FAIL;

    PRBool classExists = PR_FALSE;
    PRBool ok = PR_FALSE;
    const nsCID & ourCID = XPCDispCLSID2nsCID(aCID);
    dispSupport->IsClassSafeToHost(ccx, ourCID, PR_FALSE, &classExists, &ok);
    if(classExists && !ok)
        return E_FAIL;

    // Test if the object is scriptable
    PRBool isScriptable = PR_FALSE;
    if(!allowAnyObjects)
    {
        PRBool classExists = PR_FALSE;
        dispSupport->IsClassMarkedSafeForScripting(ourCID, &classExists, &isScriptable);
        if(!classExists)
            return REGDB_E_CLASSNOTREG;
    }

    // Create the object
    CComPtr<IDispatch> disp;
    // If createdObject isn't null we need to create the object
    if (createdObject)
    {
        HRESULT hr = disp.CoCreateInstance(aCID);
        if(FAILED(hr))
            return hr;
        // if we don't allow just any object, and it wasn't marked 
        // safe for scripting then ask the object (MS idea of security)
        if (!allowAnyObjects && !isScriptable)
        {
            dispSupport->IsObjectSafeForScripting(disp, NSID_IDISPATCH, &isScriptable);
            if(!isScriptable)
                return E_FAIL;
        }
        disp.CopyTo(createdObject);
    }

    return S_OK;
}

HRESULT XPCDispObject::COMCreateInstance(XPCCallContext & ccx, BSTR className,
                                         PRBool enforceSecurity,
                                         IDispatch ** result)
{
    NS_ENSURE_ARG_POINTER(result);
    // Turn the string into a CLSID
    _bstr_t bstrName(className);
    CLSID classID = CLSID_NULL;
    HRESULT hr = CLSIDFromString(bstrName, &classID);
    if(FAILED(hr))
        hr = CLSIDFromProgID(bstrName, &classID);
    if(FAILED(hr) || ::IsEqualCLSID(classID, CLSID_NULL))
        return hr;
    
    // If the caller cares about security do the necessary checks
    // This results in the object being instantiated, so we'll use
    // it
    if(enforceSecurity)
        return SecurityCheck(ccx, classID, result);
    
    CComPtr<IDispatch> disp;
    hr = disp.CoCreateInstance(classID);
    if(FAILED(hr))
        return hr;

    disp.CopyTo(result);

    return S_OK;
}

// static
JSBool XPCDispObject::Dispatch(XPCCallContext& ccx, IDispatch * disp,
                               DISPID dispID, CallMode mode, 
                               XPCDispParams * params,
                               jsval* retval,
                               XPCDispInterface::Member * member,
                               XPCJSRuntime* rt)
{
    _variant_t dispResult;
    jsval val;
    uintN err;
    uintN argc = params->GetParamCount();
    // Figure out what we're doing (getter/setter/method)
    WORD dispFlags;
    if(mode == CALL_SETTER)
    {
        dispFlags = DISPATCH_PROPERTYPUT;
    }
    else if(mode == CALL_GETTER)
    {
        dispFlags = DISPATCH_PROPERTYGET;
    }
    else
    {
        dispFlags = DISPATCH_METHOD;
    }
    HRESULT invokeResult;
    EXCEPINFO exception;
    // Scope the lock
    {
        // avoid deadlock in case the native method blocks somehow
        AutoJSSuspendRequest req(ccx);  // scoped suspend of request
        // call IDispatch's invoke
        invokeResult= disp->Invoke(
            dispID,                  // IDispatch ID
            IID_NULL,                // Reserved must be IID_NULL
            LOCALE_SYSTEM_DEFAULT,   // The locale context, use the system's
            dispFlags,               // Type of Invoke call
            params->GetDispParams(), // Parameters
            &dispResult,             // Where the result is stored
            &exception,              // Exception information
            0);                      // Index of an argument error
    }
    if(SUCCEEDED(invokeResult))
    {
        *retval = JSVAL_VOID;
        if(mode == CALL_METHOD)
        {
            NS_ASSERTION(member, "member must not be null if this is a method");
            for(PRUint32 index = 0; index < argc; ++index)
            {
                const XPCDispInterface::Member::ParamInfo & paramInfo = member->GetParamInfo(index);
                if(paramInfo.IsOut())
                {
                    if(!XPCDispConvert::COMToJS(ccx, params->GetParamRef(index), val, err))
                        return ThrowBadParam(err, index, ccx);

                    if(paramInfo.IsRetVal())
                    {
                        *retval = val;
                    }
                    else
                    {
                        jsval * argv = ccx.GetArgv();
                        // Out, in/out parameters must be objects
                        if(!JSVAL_IS_OBJECT(argv[index]) ||
                            !JS_SetPropertyById(ccx, JSVAL_TO_OBJECT(argv[index]),
                                rt->GetStringID(XPCJSRuntime::IDX_VALUE), &val))
                            return ThrowBadParam(NS_ERROR_XPC_CANT_SET_OUT_VAL, index, ccx);
                    }
                }
            }
        }
        if(dispResult.vt != VT_EMPTY)
        {
            if(!XPCDispConvert::COMToJS(ccx, dispResult, val, err))
            {
                ThrowBadParam(err, 0, ccx);
            }
            *retval = val;
        }
    }
    // Set the result and throw the error if one occured
    ccx.GetXPCContext()->SetLastResult(invokeResult);

    if(NS_FAILED(invokeResult))
    {
        XPCThrower::ThrowCOMError(ccx, invokeResult, NS_ERROR_XPC_COM_ERROR, 
                                  invokeResult == DISP_E_EXCEPTION ? 
                                      &exception : nsnull);
        return JS_FALSE;
    }
    return JS_TRUE;
}

JSBool XPCDispObject::Invoke(XPCCallContext & ccx, CallMode mode)
{
    nsresult rv = ccx.CanCallNow();
    if(NS_FAILED(rv))
    {
        // If the security manager is complaining then this is not really an
        // internal error in xpconnect. So, no reason to botch the assertion.
        NS_ASSERTION(rv == NS_ERROR_XPC_SECURITY_MANAGER_VETO, 
                     "hmm? CanCallNow failed in XPCDispObject::Invoke. "
                     "We are finding out about this late!");
        XPCThrower::Throw(rv, ccx);
        return JS_FALSE;
    }

    // TODO: Remove type cast and change GetIDispatchMember to use the correct type
    XPCDispInterface::Member* member = reinterpret_cast<XPCDispInterface::Member*>(ccx.GetIDispatchMember());
    XPCJSRuntime* rt = ccx.GetRuntime();
    XPCContext* xpcc = ccx.GetXPCContext();
    XPCPerThreadData* tls = ccx.GetThreadData();
    
    jsval* argv = ccx.GetArgv();
    uintN argc = ccx.GetArgc();

    tls->SetException(nsnull);
    xpcc->SetLastResult(NS_ERROR_UNEXPECTED);

    // set up the method index and do the security check if needed

    PRUint32 secFlag;
    PRUint32 secAction;

    switch(mode)
    {
        case CALL_METHOD:
            secFlag   = nsIXPCSecurityManager::HOOK_CALL_METHOD;
            secAction = nsIXPCSecurityManager::ACCESS_CALL_METHOD;
            break;
        case CALL_GETTER:
            secFlag   = nsIXPCSecurityManager::HOOK_GET_PROPERTY;
            secAction = nsIXPCSecurityManager::ACCESS_GET_PROPERTY;
            break;
        case CALL_SETTER:
            secFlag   = nsIXPCSecurityManager::HOOK_SET_PROPERTY;
            secAction = nsIXPCSecurityManager::ACCESS_SET_PROPERTY;
            break;
        default:
            NS_ASSERTION(0,"bad value");
            return JS_FALSE;
    }
    jsval name = member->GetName();

    nsIXPCSecurityManager* sm = xpcc->GetAppropriateSecurityManager(secFlag);
    XPCWrappedNative* wrapper = ccx.GetWrapper();
    if(sm && NS_FAILED(sm->CanAccess(secAction, &ccx, ccx,
                                     ccx.GetFlattenedJSObject(),
                                     wrapper->GetIdentityObject(),
                                     wrapper->GetClassInfo(), name,
                                     wrapper->GetSecurityInfoAddr())))
    {
        // the security manager vetoed. It should have set an exception.
        return JS_FALSE;
    }

    IDispatch * pObj = reinterpret_cast<IDispatch*>
                                       (ccx.GetTearOff()->GetNative());
    PRUint32 args = member->GetParamCount();
    uintN err;
    // Make sure setter has one argument
    if(mode == CALL_SETTER)
        args = 1;
    // Allow for optional parameters. We'll let COM handle the error if there
    // are not enough parameters
    if(argc < args)
        args = argc;
    XPCDispParams * params = new XPCDispParams(args);
    jsval val;
    // If this is a setter, we just need to convert the first parameter
    if(mode == CALL_SETTER)
    {
        params->SetNamedPropID();
        if(!XPCDispConvert::JSToCOM(ccx, argv[0], params->GetParamRef(0), err))
        {
            delete params;
            return ThrowBadParam(err, 0, ccx);
        }
    }
    else if(mode != CALL_GETTER)    // This is a function
    {
        // Convert the arguments to the function
        for(PRUint32 index = 0; index < args; ++index)
        {
            const XPCDispInterface::Member::ParamInfo & paramInfo = member->GetParamInfo(index);
            if(paramInfo.IsIn())
            {
                val = argv[index];
                if(paramInfo.IsOut())
                {
                    if(JSVAL_IS_PRIMITIVE(val) ||
                        !JS_GetPropertyById(ccx, JSVAL_TO_OBJECT(val),
                            rt->GetStringID(XPCJSRuntime::IDX_VALUE), &val))
                    {
                        delete params;
                        return ThrowBadParam(NS_ERROR_XPC_NEED_OUT_OBJECT, index, ccx);
                    }
                    paramInfo.InitializeOutputParam(params->GetOutputBuffer(index), params->GetParamRef(index));
                }
                if(!XPCDispConvert::JSToCOM(ccx, val, params->GetParamRef(index), err, paramInfo.IsOut()))
                {
                    delete params;
                    return ThrowBadParam(err, index, ccx);
                }
            }
            else
            {
                paramInfo.InitializeOutputParam(params->GetOutputBuffer(index), params->GetParamRef(index));
            }
        }
    }
    // If this is a parameterized property
    if(member->IsParameterizedProperty())
    {
        // We need to get a parameterized property object to return to JS
        // NewInstance takes ownership of params
        if(XPCDispParamPropJSClass::NewInstance(ccx, wrapper,
                                                member->GetDispID(),
                                                params, &val))
        {
            ccx.SetRetVal(val);
            if(!JS_IdToValue(ccx, 1, &val))
            {
                // This shouldn't fail
                NS_ERROR("JS_IdToValue failed in XPCDispParamPropJSClass::NewInstance");
                return JS_FALSE;
            }
            JS_SetCallReturnValue2(ccx, val);
            return JS_TRUE;
        }
        // NewInstance would only fail if there was an out of memory problem
        JS_ReportOutOfMemory(ccx);
        delete params;
        return JS_FALSE;
    }
    JSBool retval = Dispatch(ccx, pObj, member->GetDispID(), mode, params, &val, member, rt);
    if(retval && mode == CALL_SETTER)
    {
        ccx.SetRetVal(argv[0]);
    }
    else
    {
        ccx.SetRetVal(val);
    }
    delete params;
    return retval;
}

static
JSBool GetMember(XPCCallContext& ccx, JSObject* funobj, XPCNativeInterface*& iface, XPCDispInterface::Member*& member)
{
    jsval val;
    if(!JS_GetReservedSlot(ccx, funobj, 1, &val))
        return JS_FALSE;
    if(!JSVAL_IS_INT(val))
        return JS_FALSE;
    iface = reinterpret_cast<XPCNativeInterface*>(JSVAL_TO_PRIVATE(val));
    if(!JS_GetReservedSlot(ccx, funobj, 0, &val))
        return JS_FALSE;
    if(!JSVAL_IS_INT(val))
        return JS_FALSE;
    member = reinterpret_cast<XPCDispInterface::Member*>(JSVAL_TO_PRIVATE(val));
    return JS_TRUE;
}

// Handy macro used in callbacks below.
#define THROW_AND_RETURN_IF_BAD_WRAPPER(cx, wrapper)                         \
    PR_BEGIN_MACRO                                                           \
    if(!wrapper)                                                             \
    {                                                                        \
        XPCThrower::Throw(NS_ERROR_XPC_BAD_OP_ON_WN_PROTO, cx);              \
        return JS_FALSE;                                                     \
    }                                                                        \
    if(!wrapper->IsValid())                                                  \
    {                                                                        \
        XPCThrower::Throw(NS_ERROR_XPC_HAS_BEEN_SHUTDOWN, cx);               \
        return JS_FALSE;                                                     \
    }                                                                        \
    PR_END_MACRO

/**
 * Callback for functions
 * This callback is called by JS when a function on a JSObject proxying
 * for an IDispatch instance
 * @param cx A pointer to a JS context
 * @param obj JS object that the parameterized property is on
 * @param argc Number of arguments in this call
 * @param argv The parameters passed in if any
 * @param vp The return value
 * @return Returns JS_TRUE if the operation succeeded
 */
JSBool
XPC_IDispatch_CallMethod(JSContext* cx, JSObject* obj, uintN argc,
                         jsval* argv, jsval* vp)
{
    NS_ASSERTION(JS_TypeOfValue(cx, argv[-2]) == JSTYPE_FUNCTION, "bad function");
    JSObject* funobj = JSVAL_TO_OBJECT(argv[-2]);
    XPCCallContext ccx(JS_CALLER, cx, obj, funobj, 0, argc, argv, vp);
    XPCWrappedNative* wrapper = ccx.GetWrapper();
    THROW_AND_RETURN_IF_BAD_WRAPPER(cx, wrapper);
    ccx.SetArgsAndResultPtr(argc, argv, vp);

    XPCDispInterface::Member* member;
    XPCNativeInterface* iface;
#ifdef DEBUG
    PRBool ok =
#endif
    GetMember(ccx, funobj, iface, member);
    NS_ASSERTION(ok, "GetMember faild in XPC_IDispatch_CallMethod");
    ccx.SetIDispatchInfo(iface, member);

    return XPCDispObject::Invoke(ccx, XPCDispObject::CALL_METHOD);
}

/**
 * Callback for properties
 * This callback is called by JS when a property is set or retrieved on a 
 * JSObject proxying for an IDispatch instance
 * @param cx A pointer to a JS context
 * @param obj JS object that the parameterized property is on
 * @param argc Number of arguments in this call
 * @param argv The parameters passed in if any
 * @param vp The return value
 * @return Returns JS_TRUE if the operation succeeded
 */
JSBool
XPC_IDispatch_GetterSetter(JSContext *cx, JSObject *obj, uintN argc,
                           jsval *argv, jsval *vp)
{
    NS_ASSERTION(JS_TypeOfValue(cx, argv[-2]) == JSTYPE_FUNCTION, "bad function");
    JSObject* funobj = JSVAL_TO_OBJECT(argv[-2]);

    XPCCallContext ccx(JS_CALLER, cx, obj, funobj);
    XPCWrappedNative* wrapper = ccx.GetWrapper();
    THROW_AND_RETURN_IF_BAD_WRAPPER(cx, wrapper);

    ccx.SetArgsAndResultPtr(argc, argv, vp);
    XPCDispInterface::Member* member;
    XPCNativeInterface* iface;
#ifdef DEBUG
    PRBool ok =
#endif
    GetMember(ccx, funobj, iface, member);
    NS_ASSERTION(ok, "GetMember faild in XPC_IDispatch_CallMethod");

    ccx.SetIDispatchInfo(iface, member);
    return XPCDispObject::Invoke(ccx, argc != 0 ? XPCDispObject::CALL_SETTER : XPCDispObject::CALL_GETTER);
}
