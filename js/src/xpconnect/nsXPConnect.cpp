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

/* High level class and public functions implementation. */

#include "xpcprivate.h"

#define CONTEXT_MAP_SIZE        16
#define JS_MAP_SIZE             256
#define NATIVE_MAP_SIZE         256
#define JS_CLASS_MAP_SIZE       256
#define NATIVE_CLASS_MAP_SIZE   256

static NS_DEFINE_IID(kXPConnectIID, NS_IXPCONNECT_IID);
NS_IMPL_ISUPPORTS(nsXPConnect, kXPConnectIID)

nsXPConnect* nsXPConnect::mSelf = NULL;

static NS_DEFINE_IID(kAllocatorCID, NS_ALLOCATOR_CID);
static NS_DEFINE_IID(kIAllocatorIID, NS_IALLOCATOR_IID);

// static
nsXPConnect*
nsXPConnect::GetXPConnect()
{
    if(mSelf)
        NS_ADDREF(mSelf);
    else
    {
        mSelf = new nsXPConnect();
        if(mSelf && (!mSelf->mContextMap ||
                     !mSelf->mAllocator ||
                     !mSelf->mArbitraryScriptable ||
                     !mSelf->mInterfaceInfoManager))
            NS_RELEASE(mSelf);
    }
    return mSelf;
}

// static
nsIAllocator*
nsXPConnect::GetAllocator(nsXPConnect* xpc /*= NULL*/)
{
    nsIAllocator* al;
    nsXPConnect* xpcl = xpc;

    if(!xpcl && !(xpcl = GetXPConnect()))
        return NULL;
    if(NULL != (al = xpcl->mAllocator))
        NS_ADDREF(al);
    if(!xpc)
        NS_RELEASE(xpcl);
    return al;
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

nsXPConnect::nsXPConnect()
    :   mContextMap(NULL),
        mAllocator(NULL),
        mArbitraryScriptable(NULL),
        mInterfaceInfoManager(NULL)
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();

    nsXPCWrappedNativeClass::OneTimeInit();
    mContextMap = JSContext2XPCContextMap::newMap(CONTEXT_MAP_SIZE);
    mArbitraryScriptable = new nsXPCArbitraryScriptable();

    nsServiceManager::GetService(kAllocatorCID,
                                 kIAllocatorIID,
                                 (nsISupports **)&mAllocator);

    // XXX later this will be a service
    mInterfaceInfoManager = XPT_GetInterfaceInfoManager();
}

nsXPConnect::~nsXPConnect()
{
    if(mContextMap)
        delete mContextMap;
    if(mAllocator)
        nsServiceManager::ReleaseService(kAllocatorCID, mAllocator);
    if(mArbitraryScriptable)
        NS_RELEASE(mArbitraryScriptable);
    // XXX later this will be a service
    if(mInterfaceInfoManager)
        NS_RELEASE(mInterfaceInfoManager);

    mSelf = NULL;
}

NS_IMETHODIMP
nsXPConnect::InitJSContext(JSContext* aJSContext,
                            JSObject* aGlobalJSObj)
{
    if(aJSContext)
    {
        if(!aGlobalJSObj)
            aGlobalJSObj = JS_GetGlobalObject(aJSContext);
        if(aGlobalJSObj &&
           !mContextMap->Find(aJSContext) &&
           NewContext(aJSContext, aGlobalJSObj))
        {
            return NS_OK;
        }
    }
    NS_ASSERTION(0,"nsXPConnect::InitJSContext failed");
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsXPConnect::InitJSContextWithNewWrappedGlobal(JSContext* aJSContext,
                          nsISupports* aCOMObj,
                          REFNSIID aIID,
                          nsIXPConnectWrappedNative** aWrapper)
{
    nsXPCWrappedNative* wrapper = NULL;
    XPCContext* xpcc = NULL;
    if(!mContextMap->Find(aJSContext) &&
       NULL != (xpcc = NewContext(aJSContext, NULL, JS_FALSE)))
    {
        wrapper = nsXPCWrappedNative::GetNewOrUsedWrapper(xpcc, aCOMObj, aIID);
        if(wrapper)
        {
            if(JS_InitStandardClasses(aJSContext, wrapper->GetJSObject()) &&
               xpcc->Init(wrapper->GetJSObject()))
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
    *aWrapper = NULL;
    return NS_ERROR_FAILURE;
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

    *aWrapper = NULL;

    XPCContext* xpcc = nsXPConnect::GetContext(aJSContext, this);
    if(!xpcc)
        return NS_ERROR_FAILURE;

    nsXPCWrappedNative* wrapper =
        nsXPCWrappedNative::GetNewOrUsedWrapper(xpcc, aCOMObj, aIID);

    if(!wrapper)
        return NS_ERROR_FAILURE;

    *aWrapper = wrapper;
    return NS_OK;
}

NS_IMETHODIMP
nsXPConnect::WrapJS(JSContext* aJSContext,
                     JSObject* aJSObj,
                     REFNSIID aIID,
                     nsIXPConnectWrappedJS** aWrapper)
{
    NS_PRECONDITION(aJSContext,"bad param");
    NS_PRECONDITION(aJSObj,"bad param");
    NS_PRECONDITION(aWrapper,"bad param");

    *aWrapper = NULL;

    XPCContext* xpcc = nsXPConnect::GetContext(aJSContext, this);
    if(!xpcc)
        return NS_ERROR_FAILURE;

    nsXPCWrappedJS* wrapper =
        nsXPCWrappedJS::GetNewOrUsedWrapper(xpcc, aJSObj, aIID);

    if(!wrapper)
        return NS_ERROR_FAILURE;

    *aWrapper = wrapper;
    return NS_OK;
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
    return NS_OK;
}

#ifdef DEBUG
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
        XPC_LOG_ALWAYS(("mAllocator @ %x", mAllocator));
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

XPC_PUBLIC_API(nsIXPConnect*)
XPC_GetXPConnect()
{
    // temp test...
//    nsXPCWrappedJS* p = new nsXPCWrappedJS();
//    p->Stub5();
    return nsXPConnect::GetXPConnect();
}

#ifdef DEBUG
XPC_PUBLIC_API(void)
XPC_Dump(nsISupports* p, int depth)
{
    if(!depth)
        return;
    if(!p)
    {
        XPC_LOG_ALWAYS(("*** Cound not dump object with NULL address"));
        return;
    }

    nsIXPConnect* xpc;
    nsIXPCWrappedNativeClass* wnc;
    nsIXPCWrappedJSClass* wjsc;
    nsIXPConnectWrappedNative* wn;
    nsIXPConnectWrappedJSMethods* wjsm;

    if(NS_SUCCEEDED(p->QueryInterface(nsIXPConnect::IID(),(void**)&xpc)))
    {
        XPC_LOG_ALWAYS(("Dumping a nsIXPConnect..."));
        xpc->DebugDump(depth);
        NS_RELEASE(xpc);
    }
    else if(NS_SUCCEEDED(p->QueryInterface(nsIXPCWrappedNativeClass::IID(),
                        (void**)&wnc)))
    {
        XPC_LOG_ALWAYS(("Dumping a nsIXPCWrappedNativeClass..."));
        wnc->DebugDump(depth);
        NS_RELEASE(wnc);
    }
    else if(NS_SUCCEEDED(p->QueryInterface(nsIXPCWrappedJSClass::IID(),
                        (void**)&wjsc)))
    {
        XPC_LOG_ALWAYS(("Dumping a nsIXPCWrappedJSClass..."));
        wjsc->DebugDump(depth);
        NS_RELEASE(wjsc);
    }
    else if(NS_SUCCEEDED(p->QueryInterface(nsIXPConnectWrappedNative::IID(),
                        (void**)&wn)))
    {
        XPC_LOG_ALWAYS(("Dumping a nsIXPConnectWrappedNative..."));
        wn->DebugDump(depth);
        NS_RELEASE(wn);
    }
    else if(NS_SUCCEEDED(p->QueryInterface(nsIXPConnectWrappedJSMethods::IID(),
                        (void**)&wjsm)))
    {
        XPC_LOG_ALWAYS(("Dumping a nsIXPConnectWrappedJSMethods..."));
        wjsm->DebugDump(depth);
        NS_RELEASE(wjsm);
    }
    else
        XPC_LOG_ALWAYS(("*** Cound not dump the nsISupports @ %x", p));

}
#endif

