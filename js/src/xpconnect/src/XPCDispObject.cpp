/* ***** BEGIN LICENSE BLOCK *****
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

IDispatch * XPCDispObject::COMCreateInstance(const char * className)
{
    // TODO: This needs to have some error handling. We could probably
    // capture some information from the GetLastError
    // allows us to convert to BSTR for CLSID functions below
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
    if(SUCCEEDED(hr))
    {
        IDispatch * pDisp;
        hr = CoCreateInstance(
            classID,
            0,
            CLSCTX_INPROC_SERVER,
            IID_IDispatch,
            (LPVOID*)&pDisp);
        if(SUCCEEDED(hr))
        {
            pDisp->AddRef();
            return pDisp;
        }
    }
    return nsnull;
}

// This is the size of the largest member in the union in VARIANT
const PRUint32 VARIANT_UNION_SIZE = sizeof(VARIANT) - sizeof(VARTYPE) - sizeof(unsigned short) * 3;
#define VARIANT_BUFFER_SIZE(count) (VARIANT_UNION_SIZE * count)

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

    JSBool retval = JS_FALSE;
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

    PRBool setter;
    PRBool getter;
    PRUint32 secFlag;
    PRUint32 secAction;

    switch(mode)
    {
        case CALL_METHOD:
            secFlag   = nsIXPCSecurityManager::HOOK_CALL_METHOD;
            secAction = nsIXPCSecurityManager::ACCESS_CALL_METHOD;
            getter = setter = PR_FALSE;
            break;
        case CALL_GETTER:
            secFlag   = nsIXPCSecurityManager::HOOK_GET_PROPERTY;
            secAction = nsIXPCSecurityManager::ACCESS_GET_PROPERTY;
            setter = PR_FALSE;
            getter = PR_TRUE;
            break;
        case CALL_SETTER:
            secFlag   = nsIXPCSecurityManager::HOOK_SET_PROPERTY;
            secAction = nsIXPCSecurityManager::ACCESS_SET_PROPERTY;
            setter = PR_TRUE;
            getter = PR_FALSE;
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
    if(setter)
        args = 1;
    if(argc < args)
        args = argc;
    const PRUint32 DEFAULT_ARG_ARRAY_SIZE = 8;
    VARIANT stackArgs[DEFAULT_ARG_ARRAY_SIZE];
    char varStackBuffer[VARIANT_BUFFER_SIZE(DEFAULT_ARG_ARRAY_SIZE)];
    char * varBuffer = args <= DEFAULT_ARG_ARRAY_SIZE ? varStackBuffer : new char[VARIANT_BUFFER_SIZE(args)];
    memset(varBuffer, 0, VARIANT_BUFFER_SIZE(args));
    VARIANT dispResult;
    DISPPARAMS dispParams;
    dispParams.cArgs = args;
    dispParams.rgvarg = args <= DEFAULT_ARG_ARRAY_SIZE ? stackArgs : new VARIANT[args];
    DISPID propID;
    // If this is a setter, we just need to convert the first parameter
    if(setter)
    {
        propID = DISPID_PROPERTYPUT;
        dispParams.rgdispidNamedArgs = &propID;
        dispParams.cNamedArgs = 1;
        if(!XPCDispConvert::JSToCOM(ccx, argv[0],dispParams.rgvarg[0], err))
            return ThrowBadParam(err, 0, ccx);
    }
    else if(getter)
    {
        dispParams.rgdispidNamedArgs = 0;
        dispParams.cNamedArgs = 0;
    }
    else
    {
        dispParams.cNamedArgs = 0;
        dispParams.rgdispidNamedArgs = 0;
        VARIANT* pVar = dispParams.rgvarg + args - 1;
        for(PRUint32 index = 0; index < args; ++index, --pVar)
        {
            const XPCDispInterface::Member::ParamInfo & paramInfo = member->GetParamInfo(index);
            if(paramInfo.IsIn())
            {
                jsval val = argv[index];
                if(paramInfo.IsOut())
                {
                    if(JSVAL_IS_PRIMITIVE(argv[index]) ||
                        !OBJ_GET_PROPERTY(ccx, JSVAL_TO_OBJECT(argv[index]),
                                          rt->GetStringID(XPCJSRuntime::IDX_VALUE),
                                          &val))
                    {
                        ThrowBadParam(NS_ERROR_XPC_NEED_OUT_OBJECT, index, ccx);
                    }
                }
                if(!XPCDispConvert::JSToCOM(ccx, val,*pVar, err))
                {
                    ThrowBadParam(err, index, ccx);
                }
            }
            else
            {
                paramInfo.InitializeOutputParam(varBuffer + VARIANT_UNION_SIZE * index, *pVar);
            }
        }
    }
    HRESULT invokeResult = E_FAIL;
    {
        // avoid deadlock in case the native method blocks somehow
        AutoJSSuspendRequest req(ccx);  // scoped suspend of request
        IDispatch * pDisp;
        // TODO: I'm not sure this QI is really needed
        nsresult result = pObj->QueryInterface(NSID_IDISPATCH, (void**)&pDisp);
        if(NS_SUCCEEDED(result))
        {
            WORD dispFlags;
            if(setter)
            {
                dispFlags = DISPATCH_PROPERTYPUT;
            }
            else if(getter)
            {
                dispFlags = DISPATCH_PROPERTYGET;
            }
            else
            {
                dispFlags = DISPATCH_METHOD;
            }
            invokeResult= pDisp->Invoke(member->GetDispID(), IID_NULL, LOCALE_SYSTEM_DEFAULT, dispFlags, &dispParams, &dispResult, 0, 0);
            if(SUCCEEDED(invokeResult))
            {
                if(!setter && !getter)
                {
                    VARIANT* pVar = dispParams.rgvarg + args - 1;
                    for(PRUint32 index = 0; index < args; ++index, --pVar)
                    {
                        const XPCDispInterface::Member::ParamInfo & paramInfo = member->GetParamInfo(index);
                        if(paramInfo.IsOut())
                        {
                            if(paramInfo.IsRetVal())
                            {
                                if(!ccx.GetReturnValueWasSet())
                                    ccx.SetRetVal(argv[index]);
                            }
                            else
                            {
                                jsval val;
                                if(!XPCDispConvert::COMToJS(ccx, *pVar, val, err))
                                    ThrowBadParam(err, index, ccx);
                                // we actually assured this before doing the invoke
                                NS_ASSERTION(JSVAL_IS_OBJECT(argv[index]), "out var is not object");
                                if(!OBJ_SET_PROPERTY(ccx, JSVAL_TO_OBJECT(argv[index]),
                                            rt->GetStringID(XPCJSRuntime::IDX_VALUE), &val))
                                    ThrowBadParam(NS_ERROR_XPC_CANT_SET_OUT_VAL, index, ccx);
                            }
                            CleanupVariant(*pVar);
                        }
                    }
                }
                if(!ccx.GetReturnValueWasSet())
                {
                    if(dispResult.vt != VT_EMPTY)
                    {
                        jsval val;
                        if(!XPCDispConvert::COMToJS(ccx, dispResult, val, err))
                        {
                            ThrowBadParam(err, 0, ccx);
                        }
                        ccx.SetRetVal(val);
                    }
                    else if(setter)
                    {
                        ccx.SetRetVal(argv[0]);
                    }
                }
                retval = JS_TRUE;
            }
        }
    }
    // TODO: I think this needs to be moved up, to only occur if invoke is actually called.
    xpcc->SetLastResult(invokeResult);

    if(NS_FAILED(invokeResult))
    {
        XPCThrower::ThrowCOMError(ccx, invokeResult);
    }

    for(PRUint32 index = 0; index < args; ++index)
        CleanupVariant(dispParams.rgvarg[index]);
    // Cleanup if we allocated the variant array
    if(dispParams.rgvarg != stackArgs)
        delete [] dispParams.rgvarg;
    if(varBuffer != varStackBuffer)
        delete [] varBuffer;
    return retval;
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
 */
JSBool JS_DLL_CALLBACK
XPC_IDispatch_CallMethod(JSContext* cx, JSObject* obj,
                  uintN argc, jsval* argv, jsval* vp)
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
 */
JSBool JS_DLL_CALLBACK
XPC_IDispatch_GetterSetter(JSContext *cx, JSObject *obj,
                    uintN argc, jsval *argv, jsval *vp)
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
