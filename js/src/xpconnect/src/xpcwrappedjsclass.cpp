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

static NS_DEFINE_IID(kWrappedJSClassIID, NS_IXPCONNECT_WRAPPED_JS_CLASS_IID);
NS_IMPL_ISUPPORTS(nsXPCWrappedJSClass, kWrappedJSClassIID)

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
                if(nsXPConnect::IsISupportsDescendent(info))
                {
                    clazz = new nsXPCWrappedJSClass(xpcc, aIID, info);
                    if(!clazz->mDescriptors)
                        NS_RELEASE(clazz);  // sets clazz to NULL
                }
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
      mInfo(aInfo),
      mIID(aIID),
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
            int wordCount = (methodCount/32)+1;
            if(NULL != (mDescriptors = new uint32[wordCount]))
            {
                int i;
                // init flags to 0;
                for(i = wordCount-1; i >= 0; i--)
                    mDescriptors[i] = 0;

                for(i = 0; i < methodCount; i++)
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
    id = xpc_NewIIDObject(cx, aIID);

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

class WrappedJSIdentity
{
    // no instance methods...
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IXPCONNECT_WRAPPED_JS_IDENTITY_CLASS_IID)

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
           NS_OK == aPtr->QueryInterface(WrappedJSIdentity::GetIID(), &result) &&
           result == WrappedJSIdentity::GetSingleton();
}

NS_IMETHODIMP
nsXPCWrappedJSClass::DelegatedQueryInterface(nsXPCWrappedJS* self,
                                             REFNSIID aIID,
                                             void** aInstancePtr)
{
    if(aIID.Equals(nsISupports::GetIID()))
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
    else if(aIID.Equals(WrappedJSIdentity::GetIID()))
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
    JSObject* result = CallQueryInterfaceOnJSObject(aJSObj, nsISupports::GetIID());
    return result ? result : aJSObj;
}

NS_IMETHODIMP
nsXPCWrappedJSClass::CallMethod(nsXPCWrappedJS* wrapper, uint16 methodIndex,
                                const nsXPTMethodInfo* info,
                                nsXPTCMiniVariant* params)
{
#define ARGS_BUFFER_COUNT     16

    jsval argsBuffer[ARGS_BUFFER_COUNT];
    jsval* argv = NULL;
    uint8 i;
    uint8 argc=0;
    jsval result;
    uint8 paramCount=0;
    nsresult retval = NS_ERROR_FAILURE;
    JSErrorReporter older;
    JSBool success;
    JSContext* cx = GetJSContext();
    JSBool InConversionsDone = JS_FALSE;
    nsID* conditional_iid = NULL;
    JSBool iidIsOwned = JS_FALSE;

    // XXX ASSUMES that retval is last arg.
    paramCount = info->GetParamCount();
    argc = paramCount -
            (paramCount && info->GetParam(paramCount-1).IsRetval() ? 1 : 0);

    if(!IsReflectable(methodIndex))
        goto pre_call_clean_up;

    // setup argv
    if(argc > ARGS_BUFFER_COUNT)
    {
        if(!(argv = new jsval[argc]))
        {
            retval = NS_ERROR_OUT_OF_MEMORY;
            goto pre_call_clean_up;
        }
    }
    else
        argv = argsBuffer;

    // build the args
    for(i = 0; i < argc; i++)
    {
        const nsXPTParamInfo& param = info->GetParam(i);
        const nsXPTType& type = param.GetType();
        jsval val;

        if(param.IsIn())
        {
            nsXPTCMiniVariant* pv;

            if(param.IsOut())
                pv = (nsXPTCMiniVariant*) params[i].val.p;
            else
                pv = &params[i];

            if(type.TagPart() == nsXPTType::T_INTERFACE)
            {
                if(NS_FAILED(GetInterfaceInfo()->
                                    GetIIDForParam(&param, &conditional_iid)))
                {
                    goto pre_call_clean_up;
                }
                iidIsOwned = JS_TRUE;
            }
            else if(type.TagPart() == nsXPTType::T_INTERFACE_IS)
            {
                uint8 arg_num = param.GetInterfaceIsArgNumber();
                const nsXPTParamInfo& param = info->GetParam(arg_num);
                const nsXPTType& type = param.GetType();
                if(type.IsPointer() && type.TagPart() == nsXPTType::T_IID)
                {
                    if(param.IsOut())
                       conditional_iid = *((nsID**)params[arg_num].val.p);
                    else
                       conditional_iid = (nsID*) params[arg_num].val.p;
                }
                if(!conditional_iid)
                    goto pre_call_clean_up;
            }

            if(!XPCConvert::NativeData2JS(cx, &val, &pv->val, type,
                                          conditional_iid, NULL))
            {
                goto pre_call_clean_up;
            }
            if(conditional_iid)
            {
                if(iidIsOwned)
                {
                    nsAllocator::Free((void*)conditional_iid);
                    iidIsOwned = JS_FALSE;
                }
                conditional_iid = NULL;
            }
        }

        if(param.IsOut())
        {
            // create an 'out' object
            JSObject* obj = NewOutObject();
            if(param.IsIn())
            {
                if(!JS_SetProperty(cx, obj, XPC_VAL_STR, &val))
                    goto pre_call_clean_up;
            }
            argv[i] = OBJECT_TO_JSVAL(obj);
        }
        else
            argv[i] = val;
    }
    InConversionsDone = JS_TRUE;

pre_call_clean_up:
    // clean up any 'out' params handed in
    for(i = 0; i < paramCount; i++)
    {
        const nsXPTParamInfo& param = info->GetParam(i);
        if(!param.IsOut())
            continue;

        const nsXPTType& type = param.GetType();
        if(!type.IsPointer())
            continue;
        if(!params[i].val.p)
            continue;

        if(type.IsInterfacePointer())
        {
            if(param.IsIn())
            {
                nsISupports** pp;
                if((NULL != (pp = (nsISupports**) params[i].val.p)) &&
                    NULL != *pp)
                {
                    (*pp)->Release();
                    *pp = NULL;
                }
            }
        }
        else
        {
            void** pp;
            if((NULL != (pp = (void**) params[i].val.p)) && NULL != *pp)
            {
                if(param.IsIn())
                    nsAllocator::Free(*pp);
                *pp = NULL;
            }
        }
    }

    if(conditional_iid)
    {
        if(iidIsOwned)
        {
            nsAllocator::Free((void*)conditional_iid);
            iidIsOwned = JS_FALSE;
        }
        conditional_iid = NULL;
    }

    if(!InConversionsDone)
        goto done;

    // do the function call - set the error reporter aside - note exceptions

    older = JS_SetErrorReporter(cx, NULL);
    JS_ClearPendingException(cx);
    success = JS_CallFunctionName(cx, wrapper->GetJSObject(), info->GetName(),
                                  argc, argv, &result);
    if(JS_IsExceptionPending(cx))
    {
        JS_ClearPendingException(cx);
        success = JS_FALSE;
    }
    JS_SetErrorReporter(cx, older);
    if(!success)
        goto done;

    // convert out args and result
    // NOTE: this is the total number of native params, not just the args
    for(i = 0; i < paramCount; i++)
    {
        const nsXPTParamInfo& param = info->GetParam(i);

        if(param.IsOut())
        {
            jsval val;
            JSBool useAllocator = JS_FALSE;
            const nsXPTType& type = param.GetType();
            nsXPTCMiniVariant* pv;

            pv = (nsXPTCMiniVariant*) params[i].val.p;

            if(param.IsRetval())
                val = result;
            else if(!JS_GetProperty(cx, JSVAL_TO_OBJECT(argv[i]), XPC_VAL_STR, &val))
                break;

            // setup allocator and/or iid

            if(type.TagPart() == nsXPTType::T_INTERFACE)
            {
                if(NS_FAILED(GetInterfaceInfo()->
                                    GetIIDForParam(&param, &conditional_iid)))
                {
                    break;
                }
                iidIsOwned = JS_TRUE;
            }
            else if(type.TagPart() == nsXPTType::T_INTERFACE_IS)
            {
                uint8 arg_num = param.GetInterfaceIsArgNumber();
                const nsXPTParamInfo& param = info->GetParam(arg_num);
                const nsXPTType& type = param.GetType();
                if(!type.IsPointer() || type.TagPart() != nsXPTType::T_IID ||
                   !(conditional_iid = *((nsID**)params[arg_num].val.p)))
                    break;
            }
            else if(type.IsPointer() && !param.IsShared())
                useAllocator = JS_TRUE;

            if(!XPCConvert::JSData2Native(cx, &pv->val, val, type,
                                        useAllocator, conditional_iid, NULL))
                break;

            if(conditional_iid)
            {
                if(iidIsOwned)
                {
                    nsAllocator::Free((void*)conditional_iid);
                    iidIsOwned = JS_FALSE;
                }
                conditional_iid = NULL;
            }
        }
    }

    // if we didn't manage all the result conversions then we have
    // to cleanup any junk that *did* get converted.
    if(i != paramCount)
    {
        // XXX major spaghetti!
        InConversionsDone = JS_FALSE;
        goto pre_call_clean_up;
    }

    retval = NS_OK;

done:
    if(argv && argv != argsBuffer)
        delete [] argv;

    if(conditional_iid && iidIsOwned)
        nsAllocator::Free((void*)conditional_iid);

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

NS_IMETHODIMP
nsXPCWrappedJSClass::DebugDump(int depth)
{
#ifdef DEBUG
    depth-- ;
    XPC_LOG_ALWAYS(("nsXPCWrappedJSClass @ %x with mRefCnt = %d", this, mRefCnt));
    XPC_LOG_INDENT();
        char* name;
        mInfo->GetName(&name);
        XPC_LOG_ALWAYS(("interface name is %s", name));
        if(name)
            nsAllocator::Free(name);
        char * iid = mIID.ToString();
        XPC_LOG_ALWAYS(("IID number is %s", iid));
        delete iid;
        XPC_LOG_ALWAYS(("InterfaceInfo @ %x", mInfo));
        uint16 methodCount = 0;
        if(depth)
        {
            uint16 i;
            nsIInterfaceInfo* parent;
            XPC_LOG_INDENT();
            mInfo->GetParent(&parent);
            XPC_LOG_ALWAYS(("parent @ %x", parent));
            mInfo->GetMethodCount(&methodCount);
            XPC_LOG_ALWAYS(("MethodCount = %d", methodCount));
            mInfo->GetConstantCount(&i);
            XPC_LOG_ALWAYS(("ConstantCount = %d", i));
            XPC_LOG_OUTDENT();
        }
        XPC_LOG_ALWAYS(("mXPCContext @ %x", mXPCContext));
        XPC_LOG_ALWAYS(("JSContext @ %x", GetJSContext()));
        XPC_LOG_ALWAYS(("mDescriptors @ %x count = %d", mDescriptors, methodCount));
        if(depth && mDescriptors && methodCount)
        {
            depth--;
            XPC_LOG_INDENT();
            for(uint16 i = 0; i < methodCount; i++)
            {
                XPC_LOG_ALWAYS(("Method %d is %s%s", \
                        i, IsReflectable(i) ? "":" NOT ","reflectable"));
            }
            XPC_LOG_OUTDENT();
            depth++;
        }
    XPC_LOG_OUTDENT();
#endif
    return NS_OK;
}
