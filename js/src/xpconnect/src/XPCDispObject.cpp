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

nsID NSID_IDISPATCH = { 0x00020400, 0x0000, 0x0000, { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };
CComModule _Module;

PRBool
XPCDispObject::COMCreateFromIDispatch(IDispatch *pDispatch, JSContext *cx, JSObject *obj, jsval *rval)
{
    if(!pDispatch)
    {
        // TODO: error reporting
        return PR_FALSE;
    }

    // TODO: Do we return an existing COM object if we recognize we've already wrapped this IDispatch?

    // Instantiate the desired COM object
    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    nsresult rv = nsXPConnect::GetXPConnect()->WrapNative(cx, obj, 
                                  NS_REINTERPRET_CAST(nsISupports*, pDispatch),
                                  NSID_IDISPATCH, getter_AddRefs(holder));
    if(FAILED(rv) || !holder)
    {
        // TODO: error reporting
        return PR_FALSE;
    }
    JSObject * jsobj;
    if(NS_FAILED(holder->GetJSObject(&jsobj)))
        return PR_FALSE;
    *rval = OBJECT_TO_JSVAL(jsobj);
    return PR_TRUE;
}

static boolean HasSafeScriptingCategory(const GUID & classID)
{
    CComPtr<ICatInformation> catInfo;
    HRESULT hr = catInfo.CoCreateInstance(CLSID_StdComponentCategoriesMgr);
    if (catInfo == NULL)
    {
        // Must fail if we can't open the category manager
        return false;
    }
     
    // See what categories the class implements
    CComPtr<IEnumCATID> enumCATID;
    if (FAILED(catInfo->EnumImplCategoriesOfClass(classID, &enumCATID)))
    {
        // Can't enumerate classes in category so fail
        return false;
    }
 
    // Search for matching categories
    BOOL bFound = FALSE;
    CATID catidNext = GUID_NULL;
    while (enumCATID->Next(1, &catidNext, NULL) == S_OK)
    {
        if (::IsEqualCATID(CATID_SafeForScripting, catidNext))
        {
            bFound = TRUE;
        }
    }
    if (!bFound)
    {
        return false;
    }
 
     return true;
}

inline
boolean ScriptOK(DWORD value)
{
    return value & (INTERFACESAFE_FOR_UNTRUSTED_CALLER | 
        INTERFACESAFE_FOR_UNTRUSTED_DATA);
}

nsresult XPCDispObject::COMCreateInstance(const char * className, PRBool testScriptability, IDispatch ** result)
{
    _bstr_t bstrName(className);
    CLSID classID;
    HRESULT hr;
    // If this looks like a class ID
    if(className[0] == '{' && className[strlen(className) -1] == '}')
    {
        hr = CLSIDFromString(bstrName, &classID);
    }
    else // it's probably a prog ID
    {
        hr = CLSIDFromProgID(bstrName, &classID);
    }
    if(FAILED(hr))
        return hr;
    PRBool scriptableOK = testScriptability;
    if (testScriptability)
    {
        const DWORD scriptOk = INTERFACESAFE_FOR_UNTRUSTED_CALLER |
                                     INTERFACESAFE_FOR_UNTRUSTED_DATA;
        scriptableOK = HasSafeScriptingCategory(classID);
    }
    
    // Didn't have the safe for scripting category so lets look at IObjectSafety
    CComPtr<IDispatch> disp;
    if (scriptableOK || !testScriptability)
    {
        HRESULT hResult = disp.CoCreateInstance(classID);
        if (FAILED(hResult))
            return hResult;
    }
    // If we're testing scriptability and it didn't have a scripting category
    // we'll check via the IObjectSafety interface
    if (testScriptability && !scriptableOK)
    {
        CComQIPtr<IObjectSafety> objSafety(disp);
        // Didn't have IObjectSafety so we'll bail
        if (objSafety == 0)
            return E_FAIL;
        DWORD supported;
        DWORD state;
        hr = objSafety->GetInterfaceSafetyOptions(IID_IDispatch, &supported, &state);
        if (FAILED(hr))
            return hr;
        if (!ScriptOK(supported) || !ScriptOK(state))
            return E_FAIL;
    }
    disp.CopyTo(result);
    return S_OK;
}

// static
JSBool XPCDispObject::Dispatch(XPCCallContext& ccx, IDispatch * disp,
                               DISPID dispID, CallMode mode, 
                               XPCDispParams & params,
                               jsval* retval,
                               XPCDispInterface::Member * member,
                               XPCJSRuntime* rt)
{
    // avoid deadlock in case the native method blocks somehow
    AutoJSSuspendRequest req(ccx);  // scoped suspend of request

    _variant_t dispResult;
    jsval val;
    uintN err;
    uintN argc = params.GetParamCount();

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
    HRESULT invokeResult= disp->Invoke(
        dispID,    // IDispatch ID
        IID_NULL,               // Reserved must be IID_NULL
        LOCALE_SYSTEM_DEFAULT,  // The locale context, use the system's
        dispFlags,              // Type of Invoke call
        params.GetDispParams(), // Parameters
        &dispResult,            // Where the result is stored
        nsnull,                 // Exception information
        0);                     // Index of an argument error
    if(SUCCEEDED(invokeResult))
    {
        if(mode == CALL_METHOD)
        {
            NS_ASSERTION(member, "member must not be null if this is a method");
            for(PRUint32 index = 0; index < argc; ++index)
            {
                const XPCDispInterface::Member::ParamInfo & paramInfo = member->GetParamInfo(index);
                if(paramInfo.IsOut())
                {
                    if(!XPCDispConvert::COMToJS(ccx, params.GetParamRef(index), val, err))
                        return ThrowBadParam(err, index, ccx);

                    if(paramInfo.IsRetVal())
                    {
                        *retval = val;
                    }
                    else
                    {
                        // we actually assured this before doing the invoke
                        NS_ASSERTION(JSVAL_IS_OBJECT(val), "out var is not an object");
                        if(!OBJ_SET_PROPERTY(ccx, JSVAL_TO_OBJECT(val),
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
    ccx.GetXPCContext()->SetLastResult(invokeResult);

    if(NS_FAILED(invokeResult))
    {
        XPCThrower::ThrowCOMError(ccx, invokeResult);
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
        return Throw(rv, ccx);
    }

    // TODO: Remove type cast and change GetIDispatchMember to use the correct type
    XPCDispInterface::Member* member = NS_REINTERPRET_CAST(XPCDispInterface::Member*,ccx.GetIDispatchMember());
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
#if I_UNDERSTAND_SECURITY
    nsIXPCSecurityManager* sm = xpcc->GetAppropriateSecurityManager(secFlag);
    if(sm && NS_FAILED(sm->CanAccess(secAction, &ccx, ccx,
                                     ccx.GetFlattenedJSObject(),
                                     ccx.GetWrapper()->GetIdentityObject(),
                                     ccx.GetWrapper()->GetClassInfo(), name,
                                     ccx.GetWrapper()->GetSecurityInfoAddr())))
    {
        // the security manager vetoed. It should have set an exception.
        return JS_FALSE;
    }
#endif
    XPCWrappedNative* wrapper = ccx.GetWrapper();
    nsISupports * pObj = ccx.GetTearOff()->GetNative();
    PRUint32 args = member->GetParamCount();
    uintN err;
    // TODO: I'm not sure why we need to do this I would have expected COM
    // to report one parameter
    if(mode == CALL_SETTER)
        args = 1;
    if(argc < args)
        args = argc;
    XPCDispParams params(args);
    jsval val;
    // If this is a setter, we just need to convert the first parameter
    if(mode == CALL_SETTER)
    {
        params.SetNamedPropID();
        if(!XPCDispConvert::JSToCOM(ccx, argv[0],params.GetParamRef(0), err))
            return ThrowBadParam(err, 0, ccx);
    }
    else if(mode != CALL_GETTER)
    {
        for(PRUint32 index = 0; index < args; ++index)
        {
            const XPCDispInterface::Member::ParamInfo & paramInfo = member->GetParamInfo(index);
            if(paramInfo.IsIn())
            {
                val = argv[index];
                if(paramInfo.IsOut())
                {
                    if(JSVAL_IS_PRIMITIVE(val) ||
                        !OBJ_GET_PROPERTY(ccx, JSVAL_TO_OBJECT(val),
                                          rt->GetStringID(XPCJSRuntime::IDX_VALUE),
                                          &val))
                    {
                        ThrowBadParam(NS_ERROR_XPC_NEED_OUT_OBJECT, index, ccx);
                    }
                }
                if(!XPCDispConvert::JSToCOM(ccx, val,params.GetParamRef(index), err))
                {
                    ThrowBadParam(err, index, ccx);
                }
            }
            else
            {
                paramInfo.InitializeOutputParam(params.GetOutputBuffer(index), params.GetParamRef(index));
            }
        }
    }
    // If this is a parameterized property
    if(member->IsParameterizedProperty())
    {
        // We need to get a parameterized property object to return to JS
        if(XPCDispParamPropJSClass::GetNewOrUsed(ccx, wrapper,
                                                  member->GetDispID(),
                                                  params, &val))
        {
            ccx.SetRetVal(val);
            if(!JS_IdToValue(ccx, 1, &val))
                return JS_FALSE;
            JS_SetCallReturnValue2(ccx, val);
            return JS_TRUE;
        }
        return JS_FALSE;
    }
    IDispatch * pDisp;
    // TODO: I'm not sure this QI is really needed
    nsresult result = pObj->QueryInterface(NSID_IDISPATCH, (void**)&pDisp);
    if(NS_SUCCEEDED(result))
    {
        JSBool retval = Dispatch(ccx, pDisp, member->GetDispID(), mode, params, &val, member, rt);
        if(retval && mode == CALL_SETTER)
        {
            ccx.SetRetVal(argv[0]);
        }
        else
        {
            ccx.SetRetVal(val);
        }
        return retval;
    }
    return JS_FALSE;
}

/**
 * throws an error
 */

JSBool XPCDispObject::Throw(uintN errNum, JSContext* cx)
{
    XPCThrower::Throw(errNum, cx);
    return JS_FALSE;
}

static
JSBool GetMember(XPCCallContext& ccx, JSObject* funobj, XPCNativeInterface*& iface, XPCDispInterface::Member*& member)
{
    // We expect funobj to be a clone, we need the real funobj.

    JSFunction* fun = (JSFunction*) JS_GetPrivate(ccx, funobj);
    if(!fun)
        return JS_FALSE;
    JSObject* realFunObj = JS_GetFunctionObject(fun);
    if(!realFunObj)
        return JS_FALSE;
    jsval val;
    if(!JS_GetReservedSlot(ccx, realFunObj, 1, &val))
        return JS_FALSE;
    if(!JSVAL_IS_INT(val))
        return JS_FALSE;
    iface = NS_REINTERPRET_CAST(XPCNativeInterface*,JSVAL_TO_PRIVATE(val));
    if(!JS_GetReservedSlot(ccx, realFunObj, 0, &val))
        return JS_FALSE;
    if(!JSVAL_IS_INT(val))
        return JS_FALSE;
    member = NS_REINTERPRET_CAST(XPCDispInterface::Member*,JSVAL_TO_PRIVATE(val));
    return JS_TRUE;
}

// Handy macro used in many callback stub below.

#define THROW_AND_RETURN_IF_BAD_WRAPPER(cx, wrapper)                         \
    PR_BEGIN_MACRO                                                           \
    if(!wrapper)                                                             \
        return XPCDispObject::Throw(NS_ERROR_XPC_BAD_OP_ON_WN_PROTO, cx);     \
    if(!wrapper->IsValid())                                                  \
        return XPCDispObject::Throw(NS_ERROR_XPC_HAS_BEEN_SHUTDOWN, cx);      \
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
JSBool JS_DLL_CALLBACK
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
    if(GetMember(ccx, funobj, iface, member))
    {
        ccx.SetIDispatchInfo(iface, member);

        return XPCDispObject::Invoke(ccx, XPCDispObject::CALL_METHOD);
    }
    return JS_FALSE;
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
JSBool JS_DLL_CALLBACK
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
    if(GetMember(ccx, funobj, iface, member))
    {
        ccx.SetIDispatchInfo(iface, member);
        return XPCDispObject::Invoke(ccx, argc != 0 ? XPCDispObject::CALL_SETTER : XPCDispObject::CALL_GETTER);
    }
    return JS_FALSE;
}
