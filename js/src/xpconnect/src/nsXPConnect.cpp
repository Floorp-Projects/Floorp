/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

/* High level class and public functions implementation. */

#include "xpcprivate.h"

#define CONTEXT_MAP_SIZE        16
#define JS_MAP_SIZE             256
#define NATIVE_MAP_SIZE         256
#define JS_CLASS_MAP_SIZE       256
#define NATIVE_CLASS_MAP_SIZE   256

static NS_DEFINE_IID(kXPConnectIID, NS_IXPCONNECT_IID);
NS_IMPL_ISUPPORTS(nsXPConnect, kXPConnectIID)

const char XPC_COMPONENTS_STR[] = "Components";

nsXPConnect* nsXPConnect::mSelf = NULL;

/***************************************************************************/

class xpcPerThreadData
{
public:
    xpcPerThreadData();
    ~xpcPerThreadData();

    nsIXPCException* GetException();
    void             SetException(nsIXPCException* aException);

private:
    nsIXPCException* mException;
};

xpcPerThreadData::xpcPerThreadData()
    :   mException(nsnull)
{
    // empty
}

xpcPerThreadData::~xpcPerThreadData()
{
    NS_IF_RELEASE(mException);
}

nsIXPCException*
xpcPerThreadData::GetException()
{
    NS_IF_ADDREF(mException);
    return mException;
}

void
xpcPerThreadData::SetException(nsIXPCException* aException)
{
    NS_IF_ADDREF(aException);
    NS_IF_RELEASE(mException);
    mException = aException;
}

/*************************************************/

JS_STATIC_DLL_CALLBACK(void)
xpc_ThreadDataDtorCB(void* ptr)
{
    xpcPerThreadData* data = (xpcPerThreadData*) ptr;
    if(data)
        delete data;
}

static xpcPerThreadData*
GetPerThreadData()
{
#define BAD_TLS_INDEX ((PRUintn) -1)
    xpcPerThreadData* data;
    static PRUintn index = BAD_TLS_INDEX;
    if(index == BAD_TLS_INDEX)
    {
        if(PR_FAILURE == PR_NewThreadPrivateIndex(&index, xpc_ThreadDataDtorCB))
        {
            NS_ASSERTION(0, "PR_NewThreadPrivateIndex failed!");
            return NULL;
        }
    }

    data = (xpcPerThreadData*) PR_GetThreadPrivate(index);
    if(!data)
    {
        if(NULL != (data = new xpcPerThreadData()))
        {
            if(PR_FAILURE == PR_SetThreadPrivate(index, data))
            {
                NS_ASSERTION(0, "PR_SetThreadPrivate failed!");
                delete data;
                data = NULL;
            }
        }
        else
        {
            NS_ASSERTION(0, "new xpcPerThreadData failed!");
        }
    }
    return data;
}


/***************************************************************************/

// static
nsXPConnect*
nsXPConnect::GetXPConnect()
{
    // XXX This pattern causes us to retain an extra ref on the singleton.
    // XXX Should the singleton nsXPConnect object *ever* be deleted?
    if(!mSelf)
    {
        mSelf = new nsXPConnect();
        if(mSelf && (!mSelf->mContextMap ||
                     !mSelf->mArbitraryScriptable ||
                     !mSelf->mInterfaceInfoManager ||
                     !mSelf->mThrower ||
                     !mSelf->mContextStack))
            NS_RELEASE(mSelf);
    }
    if(mSelf)
        NS_ADDREF(mSelf);
    return mSelf;
}

// static
nsIInterfaceInfoManager*
nsXPConnect::GetInterfaceInfoManager(nsXPConnect* xpc /*= NULL*/)
{
    nsIInterfaceInfoManager* iim;
    nsXPConnect* xpcl = xpc;

    if(!xpcl && !(xpcl = GetXPConnect()))
        return NULL;
    if(NULL != (iim = xpcl->mInterfaceInfoManager))
        NS_ADDREF(iim);
    if(!xpc)
        NS_RELEASE(xpcl);
    return iim;
}

// static
nsIJSContextStack*
nsXPConnect::GetContextStack(nsXPConnect* xpc /*= NULL*/)
{
    nsIJSContextStack* cs;
    nsXPConnect* xpcl = xpc;

    if(!xpcl && !(xpcl = GetXPConnect()))
        return NULL;
    if(NULL != (cs = xpcl->mContextStack))
        NS_ADDREF(cs);
    if(!xpc)
        NS_RELEASE(xpcl);
    return cs;
}

// static
XPCContext*
nsXPConnect::GetContext(JSContext* cx, nsXPConnect* xpc /*= NULL*/)
{
    NS_PRECONDITION(cx,"bad param");

    XPCContext* xpcc;
    nsXPConnect* xpcl = xpc;

    if(!xpcl && !(xpcl = GetXPConnect()))
        return NULL;
    xpcc = xpcl->mContextMap->Find(cx);
    if(!xpcc)
        xpcc = xpcl->NewContext(cx, JS_GetGlobalObject(cx));
    if(!xpc)
        NS_RELEASE(xpcl);
    return xpcc;
}

// static
XPCJSThrower*
nsXPConnect::GetJSThrower(nsXPConnect* xpc /*= NULL */)
{
    XPCJSThrower* thrower;
    nsXPConnect* xpcl = xpc;

    if(!xpcl && !(xpcl = GetXPConnect()))
        return NULL;
    thrower = xpcl->mThrower;
    if(!xpc)
        NS_RELEASE(xpcl);
    return thrower;
}

// static
JSBool
nsXPConnect::IsISupportsDescendent(nsIInterfaceInfo* info)
{
    if(!info)
        return JS_FALSE;

    nsIInterfaceInfo* oldest = info;
    nsIInterfaceInfo* parent;

    NS_ADDREF(oldest);
    while(NS_SUCCEEDED(oldest->GetParent(&parent)))
    {
        NS_RELEASE(oldest);
        oldest = parent;
    }

    JSBool retval = JS_FALSE;
    nsID* iid;
    if(NS_SUCCEEDED(oldest->GetIID(&iid)))
    {
        retval = iid->Equals(nsCOMTypeInfo<nsISupports>::GetIID());
        nsAllocator::Free(iid);
    }
    NS_RELEASE(oldest);
    return retval;
}

nsXPConnect::nsXPConnect()
    :   mContextMap(NULL),
        mArbitraryScriptable(NULL),
        mInterfaceInfoManager(NULL),
        mThrower(NULL),
        mContextStack(NULL)
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();

    nsXPCWrappedNativeClass::OneTimeInit();
    mContextMap = JSContext2XPCContextMap::newMap(CONTEXT_MAP_SIZE);
    mArbitraryScriptable = new nsXPCArbitraryScriptable();

    // XXX later this will be a service
    mInterfaceInfoManager = XPTI_GetInterfaceInfoManager();
    mThrower = new XPCJSThrower(JS_TRUE);

    nsServiceManager::GetService("nsThreadJSContextStack",
                                 NS_GET_IID(nsIJSContextStack),
                                 (nsISupports **)&mContextStack);
}

nsXPConnect::~nsXPConnect()
{
    if(mContextMap)
        delete mContextMap;
    if(mArbitraryScriptable)
        NS_RELEASE(mArbitraryScriptable);
    // XXX later this will be a service
    if(mInterfaceInfoManager)
        NS_RELEASE(mInterfaceInfoManager);
    if(mThrower)
        delete mThrower;
    if(mContextStack)
        nsServiceManager::ReleaseService("nsThreadJSContextStack", mContextStack);
    mSelf = NULL;
}

NS_IMETHODIMP
nsXPConnect::InitJSContext(JSContext* aJSContext,
                            JSObject* aGlobalJSObj,
                            JSBool AddComponentsObject)
{
    AUTO_PUSH_JSCONTEXT2(aJSContext, this);
    if(!aJSContext)
    {
        XPC_LOG_ERROR(("nsXPConnect::InitJSContext failed with null pointer"));
        return NS_ERROR_NULL_POINTER;
    }

    if(!aGlobalJSObj)
        aGlobalJSObj = JS_GetGlobalObject(aJSContext);
    if(aGlobalJSObj &&
       !mContextMap->Find(aJSContext) &&
       NewContext(aJSContext, aGlobalJSObj)&&
       (!AddComponentsObject ||
        NS_SUCCEEDED(AddNewComponentsObject(aJSContext, aGlobalJSObj))))
    {
        return NS_OK;
    }
    XPC_LOG_ERROR(("nsXPConnect::InitJSContext failed"));
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsXPConnect::InitJSContextWithNewWrappedGlobal(JSContext* aJSContext,
                          nsISupports* aCOMObj,
                          REFNSIID aIID,
                          JSBool AddComponentsObject,
                          nsIXPConnectWrappedNative** aWrapper)
{
    AUTO_PUSH_JSCONTEXT2(aJSContext, this);
    nsXPCWrappedNative* wrapper = NULL;
    XPCContext* xpcc = NULL;
    if(!mContextMap->Find(aJSContext) &&
       NULL != (xpcc = NewContext(aJSContext, NULL, JS_FALSE)))
    {
        wrapper = nsXPCWrappedNative::GetNewOrUsedWrapper(xpcc, aCOMObj, aIID);
        if(wrapper)
        {
            if(JS_InitStandardClasses(aJSContext, wrapper->GetJSObject()) &&
               xpcc->Init(wrapper->GetJSObject()) &&
               (!AddComponentsObject ||
                NS_SUCCEEDED(AddNewComponentsObject(aJSContext, NULL))))
            {
                *aWrapper = wrapper;
                return NS_OK;
            }
        }
    }
    if(wrapper)
        NS_RELEASE(wrapper);
    if(xpcc)
    {
        mContextMap->Remove(xpcc);
        delete xpcc;
    }
    XPC_LOG_ERROR(("nsXPConnect::InitJSContextWithNewWrappedGlobal failed"));
    *aWrapper = NULL;
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsXPConnect::AddNewComponentsObject(JSContext* aJSContext,
                                    JSObject* aGlobalJSObj)
{
    AUTO_PUSH_JSCONTEXT2(aJSContext, this);
    JSBool success;
    nsresult rv;

    if(!aJSContext)
    {
        XPC_LOG_ERROR(("nsXPConnect::AddNewComponentsObject failed with null pointer"));
        return NS_ERROR_NULL_POINTER;
    }

    if(!aGlobalJSObj && !(aGlobalJSObj = JS_GetGlobalObject(aJSContext)))
    {
        XPC_LOG_ERROR(("nsXPConnect::AddNewComponentsObject failed - no global object"));
        return NS_ERROR_FAILURE;
    }

    nsIXPCComponents* comp;
    if(NS_FAILED(rv = CreateComponentsObject(&comp)))
        return rv;

    nsIXPConnectWrappedNative* comp_wrapper;
    if(NS_FAILED(WrapNative(aJSContext, comp,
                            nsIXPCComponents::GetIID(), &comp_wrapper)))
    {
        XPC_LOG_ERROR(("nsXPConnect::AddNewComponentsObject failed - could not build wrapper"));
        NS_RELEASE(comp);
        return NS_ERROR_FAILURE;
    }
    JSObject* comp_jsobj;
    comp_wrapper->GetJSObject(&comp_jsobj);
    jsval comp_jsval = OBJECT_TO_JSVAL(comp_jsobj);
    success = JS_SetProperty(aJSContext, aGlobalJSObj,
                             "Components", &comp_jsval);
    NS_RELEASE(comp_wrapper);
    NS_RELEASE(comp);
    return success ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsXPConnect::CreateComponentsObject(nsIXPCComponents** aComponentsObj)
{
    if(!aComponentsObj)
        return NS_ERROR_NULL_POINTER;

    nsIXPCComponents* obj;
    if(nsnull != (obj = *aComponentsObj = new nsXPCComponents()))
    {
        NS_ADDREF(obj);
        return NS_OK;
    }
    XPC_LOG_ERROR(("nsXPConnect::CreateComponentsObject failed"));
    return NS_ERROR_OUT_OF_MEMORY;
}

XPCContext*
nsXPConnect::NewContext(JSContext* cx, JSObject* global,
                        JSBool doInit /*= JS_TRUE*/)
{
    XPCContext* xpcc;
    NS_PRECONDITION(cx,"bad param");
    NS_PRECONDITION(!mContextMap->Find(cx),"bad param");

    xpcc = XPCContext::newXPCContext(cx, global,
                                  JS_MAP_SIZE,
                                  NATIVE_MAP_SIZE,
                                  JS_CLASS_MAP_SIZE,
                                  NATIVE_CLASS_MAP_SIZE);
    if(doInit && xpcc && !xpcc->Init())
    {
        XPC_LOG_ERROR(("nsXPConnect::NewContext failed"));
        delete xpcc;
        xpcc = NULL;
    }
    if(xpcc)
        mContextMap->Add(xpcc);
    return xpcc;
}

NS_IMETHODIMP
nsXPConnect::WrapNative(JSContext* aJSContext,
                         nsISupports* aCOMObj,
                         REFNSIID aIID,
                         nsIXPConnectWrappedNative** aWrapper)
{
    NS_PRECONDITION(aJSContext,"bad param");
    NS_PRECONDITION(aCOMObj,"bad param");
    NS_PRECONDITION(aWrapper,"bad param");

    AUTO_PUSH_JSCONTEXT2(aJSContext, this);
    XPCContext* xpcc = nsXPConnect::GetContext(aJSContext, this);
    if(xpcc)
    {
        nsXPCWrappedNative* wrapper =
            nsXPCWrappedNative::GetNewOrUsedWrapper(xpcc, aCOMObj, aIID);
        if(wrapper)
        {
            *aWrapper = wrapper;
            return NS_OK;
        }
    }
    XPC_LOG_ERROR(("nsXPConnect::WrapNative failed"));
    *aWrapper = NULL;
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsXPConnect::WrapJS(JSContext* aJSContext,
                     JSObject* aJSObj,
                     REFNSIID aIID,
                     nsISupports** aWrapper)
{
    NS_PRECONDITION(aJSContext,"bad param");
    NS_PRECONDITION(aJSObj,"bad param");
    NS_PRECONDITION(aWrapper,"bad param");

    AUTO_PUSH_JSCONTEXT2(aJSContext, this);
    XPCContext* xpcc = nsXPConnect::GetContext(aJSContext, this);
    if(xpcc)
    {
        nsXPCWrappedJS* wrapper =
            nsXPCWrappedJS::GetNewOrUsedWrapper(xpcc, aJSObj, aIID);
        if(wrapper)
        {
            *aWrapper = wrapper;
            return NS_OK;
        }
    }
    XPC_LOG_ERROR(("nsXPConnect::WrapJS failed"));
    *aWrapper = NULL;
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsXPConnect::GetWrappedNativeOfJSObject(JSContext* aJSContext,
                                        JSObject* aJSObj,
                                        nsIXPConnectWrappedNative** aWrapper)
{
    NS_PRECONDITION(aJSContext,"bad param");
    NS_PRECONDITION(aJSObj,"bad param");
    NS_PRECONDITION(aWrapper,"bad param");

    if(!(*aWrapper = nsXPCWrappedNativeClass::GetWrappedNativeOfJSObject(aJSContext,aJSObj)))
        return NS_ERROR_UNEXPECTED;
    NS_ADDREF(*aWrapper);
    return NS_OK;
}

NS_IMETHODIMP
nsXPConnect::AbandonJSContext(JSContext* aJSContext)
{
    NS_PRECONDITION(aJSContext,"bad param");
    NS_PRECONDITION(mContextMap,"bad state");

    AUTO_PUSH_JSCONTEXT2(aJSContext, this);
    XPCContext* xpcc = mContextMap->Find(aJSContext);
    if(!xpcc)
    {
        NS_ASSERTION(0,"called AbandonJSContext for context that's not init'd");
        return NS_OK;
    }

    mContextMap->Remove(xpcc);
    // XPCContext notifies 'users' of its destruction.
    delete xpcc;
    return NS_OK;
}

NS_IMETHODIMP
nsXPConnect::SetSecurityManagerForJSContext(JSContext* aJSContext,
                                    nsIXPCSecurityManager* aManager,
                                    PRUint16 flags)
{
    if(!aJSContext)
    {
        NS_ASSERTION(0,"called SetSecurityManagerForJSContext with null pointer");
        return NS_ERROR_NULL_POINTER;
    }

    XPCContext* xpcc = mContextMap->Find(aJSContext);
    if(!xpcc)
    {
        NS_ASSERTION(0,"called SetSecurityManagerForJSContext for context that's not init'd");
        return NS_ERROR_INVALID_ARG;
    }

    if(aManager)
        NS_ADDREF(aManager);
    nsIXPCSecurityManager* oldManager = xpcc->GetSecurityManager();
    if(oldManager)
        NS_RELEASE(oldManager);

    xpcc->SetSecurityManager(aManager);
    xpcc->SetSecurityManagerFlags(flags);
    return NS_OK;
}

NS_IMETHODIMP
nsXPConnect::GetSecurityManagerForJSContext(JSContext* aJSContext,
                                    nsIXPCSecurityManager** aManager,
                                    PRUint16* flags)
{
    if(!aJSContext || !aManager || !flags)
    {
        NS_ASSERTION(0,"called GetSecurityManagerForJSContext with null pointer");
        return NS_ERROR_NULL_POINTER;
    }

    XPCContext* xpcc = mContextMap->Find(aJSContext);
    if(!xpcc)
    {
        NS_ASSERTION(0,"called GetSecurityManagerForJSContext for context that's not init'd");
        return NS_ERROR_INVALID_ARG;
    }

    if(NULL != (*aManager = xpcc->GetSecurityManager()))
        NS_ADDREF(*aManager);
    *flags = xpcc->GetSecurityManagerFlags();
    return NS_OK;
}

NS_IMETHODIMP
nsXPConnect::GetCurrentJSStack(nsIJSStackFrameLocation** aStack)
{
    if(!aStack)
    {
        NS_ASSERTION(0,"called GetCurrentJSStack with null pointer");
        return NS_ERROR_NULL_POINTER;
    }

    nsresult rv;
    NS_WITH_SERVICE(nsIJSContextStack, cxstack, "nsThreadJSContextStack", &rv);

    if(NS_FAILED(rv))
    {
        NS_ASSERTION(0,"could not get nsThreadJSContextStack service");
        return NS_ERROR_FAILURE;
    }

    JSContext* cx;
    if(NS_FAILED(cxstack->Peek(&cx)) || !cx)
    {
        // no current context available
        *aStack = nsnull;
        return NS_OK;
    }

    *aStack = XPCJSStack::CreateStack(cx);
    return NS_OK;
}

NS_IMETHODIMP
nsXPConnect::CreateStackFrameLocation(JSBool isJSFrame,
                                      const char* aFilename,
                                      const char* aFunctionName,
                                      PRInt32 aLineNumber,
                                      nsIJSStackFrameLocation* aCaller,
                                      nsIJSStackFrameLocation** aStack)
{
    if(!aStack)
    {
        NS_ASSERTION(0,"called CreateStackFrameLocation with null pointer");
        return NS_ERROR_NULL_POINTER;
    }
    *aStack = XPCJSStack::CreateStackFrameLocation(isJSFrame,
                                                   aFilename,
                                                   aFunctionName,
                                                   aLineNumber,
                                                   aCaller);
    return *aStack ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsXPConnect::GetPendingException(nsIXPCException** aException)
{
    if(!aException)
        return NS_ERROR_NULL_POINTER;

    xpcPerThreadData* data = GetPerThreadData();
    if(!data)
    {
        *aException = nsnull;
        return NS_ERROR_FAILURE;
    }

    *aException = data->GetException();
    return NS_OK;
}

NS_IMETHODIMP
nsXPConnect::SetPendingException(nsIXPCException* aException)
{
    xpcPerThreadData* data = GetPerThreadData();
    if(!data)
        return NS_ERROR_FAILURE;

    data->SetException(aException);
    return NS_OK;
}


/***************************************************************************/
// has to go somewhere...
nsXPCArbitraryScriptable::nsXPCArbitraryScriptable()
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();
}

#ifdef DEBUG
JS_STATIC_DLL_CALLBACK(intN)
ContextMapDumpEnumerator(JSHashEntry *he, intN i, void *arg)
{
    ((XPCContext*)he->value)->DebugDump(*(int*)arg);
    return HT_ENUMERATE_NEXT;
}
#endif

NS_IMETHODIMP
nsXPConnect::DebugDump(int depth)
{
#ifdef DEBUG
    depth-- ;
    XPC_LOG_ALWAYS(("nsXPConnect @ %x with mRefCnt = %d", this, mRefCnt));
    XPC_LOG_INDENT();
        XPC_LOG_ALWAYS(("mArbitraryScriptable @ %x", mArbitraryScriptable));
        XPC_LOG_ALWAYS(("mInterfaceInfoManager @ %x", mInterfaceInfoManager));
        XPC_LOG_ALWAYS(("mContextMap @ %x with %d context(s)", \
                         mContextMap, mContextMap ? mContextMap->Count() : 0));
        // iterate contexts...
        if(depth && mContextMap && mContextMap->Count())
        {
            XPC_LOG_INDENT();
            mContextMap->Enumerate(ContextMapDumpEnumerator, &depth);
            XPC_LOG_OUTDENT();
        }
    XPC_LOG_OUTDENT();
#endif
    return NS_OK;
}

NS_IMETHODIMP
nsXPConnect::DebugDumpObject(nsISupports* p, int depth)
{
#ifdef DEBUG
    if(!depth)
        return NS_OK;
    if(!p)
    {
        XPC_LOG_ALWAYS(("*** Cound not dump object with NULL address"));
        return NS_OK;
    }

    nsIXPConnect* xpc;
    nsIXPCWrappedNativeClass* wnc;
    nsIXPCWrappedJSClass* wjsc;
    nsIXPConnectWrappedNative* wn;
    nsIXPConnectWrappedJSMethods* wjsm;

    if(NS_SUCCEEDED(p->QueryInterface(nsIXPConnect::GetIID(),(void**)&xpc)))
    {
        XPC_LOG_ALWAYS(("Dumping a nsIXPConnect..."));
        xpc->DebugDump(depth);
        NS_RELEASE(xpc);
    }
    else if(NS_SUCCEEDED(p->QueryInterface(nsIXPCWrappedNativeClass::GetIID(),
                        (void**)&wnc)))
    {
        XPC_LOG_ALWAYS(("Dumping a nsIXPCWrappedNativeClass..."));
        wnc->DebugDump(depth);
        NS_RELEASE(wnc);
    }
    else if(NS_SUCCEEDED(p->QueryInterface(nsIXPCWrappedJSClass::GetIID(),
                        (void**)&wjsc)))
    {
        XPC_LOG_ALWAYS(("Dumping a nsIXPCWrappedJSClass..."));
        wjsc->DebugDump(depth);
        NS_RELEASE(wjsc);
    }
    else if(NS_SUCCEEDED(p->QueryInterface(nsIXPConnectWrappedNative::GetIID(),
                        (void**)&wn)))
    {
        XPC_LOG_ALWAYS(("Dumping a nsIXPConnectWrappedNative..."));
        wn->DebugDump(depth);
        NS_RELEASE(wn);
    }
    else if(NS_SUCCEEDED(p->QueryInterface(nsIXPConnectWrappedJSMethods::GetIID(),
                        (void**)&wjsm)))
    {
        XPC_LOG_ALWAYS(("Dumping a nsIXPConnectWrappedJSMethods..."));
        wjsm->DebugDump(depth);
        NS_RELEASE(wjsm);
    }
    else
        XPC_LOG_ALWAYS(("*** Cound not dump the nsISupports @ %x", p));
#endif
    return NS_OK;
}
