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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/* Platform specific code to invoke XPCOM methods on native objects */

#include "xpcprivate.h"

static const char* XPC_QUERY_INTERFACE_STR = "QueryInterface";

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

NS_IMPL_ISUPPORTS(nsXPCWrappedJSClass, NS_IXPCONNECT_WRAPPED_JS_CLASS_IID)

static uint32 zero_methods_descriptor;

// static
nsXPCWrappedJSClass*
nsXPCWrappedJSClass::GetNewOrUsedClass(XPCContext* xpcc,
                                       REFNSIID aIID)
{
    IID2WrappedJSClassMap* map;
    nsXPCWrappedJSClass* clazz = NULL;

    NS_PRECONDITION(xpcc, "bad param");

    map = xpcc->GetWrappedJSClassMap();
    NS_ASSERTION(map,"bad map");

    clazz = map->Find(aIID);
    if(clazz)
    {
        NS_ADDREF(clazz);
    }
    else
    {
        nsIInterfaceInfoManager* iimgr;
        if(NULL != (iimgr = nsXPConnect::GetInterfaceInfoManager()))
        {
            nsIInterfaceInfo* info;
            if(NS_SUCCEEDED(iimgr->GetInfoForIID(&aIID, &info)))
            {
                clazz = new nsXPCWrappedJSClass(xpcc, aIID, info);
                if(!clazz->mDescriptors)
                    NS_RELEASE(clazz);  // sets clazz to NULL
                NS_RELEASE(info);
            }
            NS_RELEASE(iimgr);
        }
    }
    return clazz;
}

nsXPCWrappedJSClass::nsXPCWrappedJSClass(XPCContext* xpcc, REFNSIID aIID,
                                         nsIInterfaceInfo* aInfo)
    : mXPCContext(xpcc),
      mIID(aIID),
      mInfo(aInfo),
      mDescriptors(NULL)
{
    NS_ADDREF(mInfo);

    NS_INIT_REFCNT();
    NS_ADDREF_THIS();

    mXPCContext->GetWrappedJSClassMap()->Add(this);

    uint16 methodCount;
    if(NS_SUCCEEDED(mInfo->GetMethodCount(&methodCount)))
    {
        if(methodCount)
        {
            if(NULL != (mDescriptors = new uint32[(methodCount/32)+1]))
            {
                for(int i = 0; i < methodCount; i++)
                {
                    const nsXPTMethodInfo* info;
                    if(NS_SUCCEEDED(mInfo->GetMethodInfo(i, &info)))
                        SetReflectable(i, XPCConvert::IsMethodReflectable(*info));
                    else
                    {
                        delete [] mDescriptors;
                        mDescriptors = NULL;
                        break;
                    }
                }
            }
        }
        else
        {
            mDescriptors = &zero_methods_descriptor;
        }
    }
}

nsXPCWrappedJSClass::~nsXPCWrappedJSClass()
{
    if(mDescriptors && mDescriptors != &zero_methods_descriptor)
        delete [] mDescriptors;
    mXPCContext->GetWrappedJSClassMap()->Remove(this);
    NS_RELEASE(mInfo);
}

/***************************************************************************/
// XXX for now IIDs are represented in JS as string objects containing strings
// of the form: {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx} (just like the nsID
// Parse and ToString methods use.

// XXX lots of room for optimization here!

// JSObject*
// nsXPCWrappedJSClass::CreateIIDJSObject(REFNSIID aIID)
// {
//     JSObject* obj = NULL;
//     char* str = aIID.ToString();
//     if(str)
//     {
//         JSContext* cx = GetJSContext();
//         JSString* jsstr = JS_InternString(cx, str);
//         delete [] str;
//         if(jsstr)
//             JS_ValueToObject(cx, STRING_TO_JSVAL(jsstr), &obj);
//     }
//     return obj;
// }

JSObject*
nsXPCWrappedJSClass::CallQueryInterfaceOnJSObject(JSObject* jsobj, REFNSIID aIID)
{
    JSContext* cx = GetJSContext();
    JSObject* id;
    jsval retval;
    JSObject* retObj;
    JSBool success = JS_FALSE;

//    id = CreateIIDJSObject(aIID);
    id = xpc_NewIDObject(cx, aIID);

    if(id)
    {
        JSErrorReporter older = JS_SetErrorReporter(cx, NULL);
        jsval args[1] = {OBJECT_TO_JSVAL(id)};
        success = JS_CallFunctionName(cx, jsobj, XPC_QUERY_INTERFACE_STR,
                                      1, args, &retval);
        if(success)
            success = JS_ValueToObject(cx, retval, &retObj);
        JS_SetErrorReporter(cx, older);
    }

    return success ? retObj : NULL;
}

/***************************************************************************/
// This 'WrappedJSIdentity' class and singleton allow us to figure out if
// any given nsISupports* is implemented by a WrappedJS object. This is done
// using a QueryInterface call on the interface pointer with our ID. If
// that call returns NS_OK and the pointer is to our singleton, then the
// interface must be implemented by a WrappedJS object. NOTE: the
// 'WrappedJSIdentity' object is not a real XPCOM object and should not be
// used for anything else (hence it is declared in this implementation file).

// {5C5C3BB0-A9BA-11d2-BA64-00805F8A5DD7}
#define NS_IXPCONNECT_WRAPPED_JS_IDENTITY_CLASS_IID \
{ 0x5c5c3bb0, 0xa9ba, 0x11d2,                       \
  { 0xba, 0x64, 0x0, 0x80, 0x5f, 0x8a, 0x5d, 0xd7 } }

static NS_DEFINE_IID(kIWrappedJSIdentityIID, NS_IXPCONNECT_WRAPPED_JS_IDENTITY_CLASS_IID);

class WrappedJSIdentity
{
    // no instance methods...
public:
    static void* GetSingleton()
    {
        static WrappedJSIdentity* singleton = NULL;
        if(!singleton)
            singleton = new WrappedJSIdentity();
        return (void*) singleton;
    }
};

/***************************************************************************/

// static
JSBool
nsXPCWrappedJSClass::IsWrappedJS(nsISupports* aPtr)
{
    void* result;
    NS_PRECONDITION(aPtr, "null pointer");
    return aPtr &&
           NS_OK == aPtr->QueryInterface(kIWrappedJSIdentityIID, &result) &&
           result == WrappedJSIdentity::GetSingleton();
}

NS_IMETHODIMP
nsXPCWrappedJSClass::DelegatedQueryInterface(nsXPCWrappedJS* self,
                                             REFNSIID aIID,
                                             void** aInstancePtr)
{
    if(aIID.Equals(kISupportsIID))
    {
        nsXPCWrappedJS* root = self->GetRootWrapper();
        *aInstancePtr = (void*) root;
        NS_ADDREF(root);
        return NS_OK;
    }
    else if(aIID.Equals(self->GetIID()))
    {
        *aInstancePtr = (void*) self;
        NS_ADDREF(self);
        return NS_OK;
    }
    else if(aIID.Equals(kIWrappedJSIdentityIID))
    {
        *aInstancePtr = WrappedJSIdentity::GetSingleton();
        return NS_OK;
    }
    else
    {
        JSObject* jsobj =
            CallQueryInterfaceOnJSObject(self->GetJSObject(), aIID);
        if(jsobj)
        {
            nsXPCWrappedJS* wrapper =
                nsXPCWrappedJS::GetNewOrUsedWrapper(GetXPCContext(),
                                                    jsobj, aIID);
            if(wrapper)
            {
                *aInstancePtr = (void*) wrapper;
                return NS_OK;
            }
        }
    }

    *aInstancePtr = NULL;
    return NS_NOINTERFACE;
}

JSObject*
nsXPCWrappedJSClass::GetRootJSObject(JSObject* aJSObj)
{
    JSObject* result = CallQueryInterfaceOnJSObject(aJSObj, kISupportsIID);
    return result ? result : aJSObj;
}

NS_IMETHODIMP
nsXPCWrappedJSClass::CallMethod(nsXPCWrappedJS* wrapper, uint16 methodIndex,
                                const nsXPTMethodInfo* info,
                                nsXPCMiniVariant* params)
{
#define ARGS_BUFFER_COUNT     32

    jsval argsBuffer[ARGS_BUFFER_COUNT];
    jsval* argv = NULL;
    uint8 i;
    uint8 argc;
    jsval result;
    uint8 paramCount;
    nsresult retval = NS_ERROR_UNEXPECTED;
    JSErrorReporter older;
    JSBool success;
    JSContext* cx = GetJSContext();

    if(!IsReflectable(methodIndex))
    {
        // XXX we need to go through and free and NULL out all 'out' params
        return NS_ERROR_FAILURE;
    }

    // XXX ASSUMES that retval is last arg.
    paramCount = info->GetParamCount();
    argc = paramCount -
            (paramCount && info->GetParam(paramCount-1).IsRetval() ? 1 : 0);

    // XXX do we need to go through and NULL out all 'out' params?

    // setup argv
    if(argc > ARGS_BUFFER_COUNT)
    {
        if(!(argv = new jsval[argc]))
            return NS_ERROR_OUT_OF_MEMORY;
    }
    else
        argv = argsBuffer;
    // from here on out we exit via 'goto done'

    // build the args
    for(i = 0; i < argc; i++)
    {
        const nsXPTParamInfo& param = info->GetParam(i);
        const nsXPTType& type = param.GetType();

        jsval val;

        if(param.IsIn())
        {
            nsXPCMiniVariant* pv;

            if(param.IsOut())
                pv = (nsXPCMiniVariant*) params[i].val.p;
            else
                pv = &params[i];

            if(type.IsPointer())
                pv = (nsXPCMiniVariant*) pv->val.p;

            if(!XPCConvert::NativeData2JS(cx, &val, &pv->val, type, NULL))
            {
                retval = NS_ERROR_FAILURE;
                goto done;
            }
        }

        if(param.IsOut())
        {
            // create an 'out' object
            JSObject* obj = NewOutObject();
            if(param.IsIn())
            {
                if(!JS_SetProperty(cx, obj, XPC_VAL_STR, &val))
                    goto done;
            }
            argv[i] = OBJECT_TO_JSVAL(obj);
        }
        else
            argv[i] = val;
    }

    // do the function call

    older = JS_SetErrorReporter(cx, NULL);
    success = JS_CallFunctionName(cx, wrapper->GetJSObject(), info->GetName(),
                                  argc, argv, &result);
    JS_SetErrorReporter(cx, older);
    if(!success)
    {
        retval = NS_ERROR_FAILURE;
        goto done;
    }

    // convert out args and result
    // NOTE: this is the total number of native params, not just the args
    for(i = 0; i < paramCount; i++)
    {
        const nsXPTParamInfo& param = info->GetParam(i);

        if(param.IsOut())
        {
            jsval val;
            const nsXPTType& type = param.GetType();
            nsXPCMiniVariant* pv;

            if(param.IsRetval())
                val = result;
            else if(!JS_GetProperty(cx, JSVAL_TO_OBJECT(argv[i]), XPC_VAL_STR, &val))
                goto done;

            pv = (nsXPCMiniVariant*) params[i].val.p;
            if(type.IsPointer())
                pv = (nsXPCMiniVariant*) pv->val.p;

            if(!XPCConvert::JSData2Native(cx, &pv->val, val, type, NULL, NULL))
            {
                NS_ASSERTION(0, "bad type");
                goto done;
            }
        }
    }

    retval = NS_OK;

done:
    if(argv && argv != argsBuffer)
        delete [] argv;
    return retval;
}


static JSClass WrappedJSOutArg_class = {
    "XPCOutArg", 0,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   JS_FinalizeStub
};

// static
JSBool
nsXPCWrappedJSClass::InitForContext(XPCContext* xpcc)
{
    if (!JS_InitClass(xpcc->GetJSContext(), xpcc->GetGlobalObject(),
        0, &WrappedJSOutArg_class, 0, 0,
        0, 0,
        0, 0))
        return JS_FALSE;
    return JS_TRUE;
}

JSObject*
nsXPCWrappedJSClass::NewOutObject()
{
    return JS_NewObject(GetJSContext(), &WrappedJSOutArg_class, NULL, NULL);
}
