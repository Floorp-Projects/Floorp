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

NS_IMPL_ISUPPORTS(nsXPConnect, NS_IXPCONNECT_IID)

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
                     !mSelf->mArbitraryScriptable))
            NS_RELEASE(mSelf);
    }
    return mSelf;
}

nsXPConnect::nsXPConnect()
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();
    mContextMap = JSContext2XPCContextMap::newMap(CONTEXT_MAP_SIZE);
    mArbitraryScriptable = new nsXPCArbitraryScriptable();

    nsServiceManager::GetService(kAllocatorCID,
                                 kIAllocatorIID,
                                 (nsISupports **)&mAllocator);
}

nsXPConnect::~nsXPConnect()
{
    if(mContextMap)
        delete mContextMap;
    if(mAllocator)
        nsServiceManager::ReleaseService(kAllocatorCID, mAllocator);
    if(mArbitraryScriptable)
        NS_RELEASE(mArbitraryScriptable);

    mSelf = NULL;
}

nsresult
nsXPConnect::InitJSContext(nsIJSContext* aJSContext,
                            nsIJSObject* aGlobalJSObj)
{
    JSContext* cx;
    JSObject* global;

    if(!aJSContext || !aGlobalJSObj ||
       NS_FAILED(aJSContext->GetNative(&cx)) ||
       NS_FAILED(aGlobalJSObj->GetNative(&global)) ||
       !cx || !global || mContextMap->Find(cx) || !NewContext(cx, global))
    {
        NS_ASSERTION(0,"nsXPConnect::InitJSContext failed");
        return NS_ERROR_FAILURE;
    }
    return NS_OK;
}

XPCContext*
nsXPConnect::GetContext(JSContext* cx)
{
    XPCContext* xpcc;
    NS_PRECONDITION(cx,"bad param");
    xpcc = mContextMap->Find(cx);
    if(!xpcc)
        xpcc = NewContext(cx, JS_GetGlobalObject(cx));
    return xpcc;
}

XPCContext*
nsXPConnect::NewContext(JSContext* cx, JSObject* global)
{
    XPCContext* xpcc;
    NS_PRECONDITION(cx,"bad param");
    NS_PRECONDITION(global,"bad param");
    NS_PRECONDITION(!mContextMap->Find(cx),"bad param");

    xpcc = XPCContext::newXPCContext(cx, global,
                                  JS_MAP_SIZE,
                                  NATIVE_MAP_SIZE,
                                  JS_CLASS_MAP_SIZE,
                                  NATIVE_CLASS_MAP_SIZE);
    if(xpcc)
        mContextMap->Add(xpcc);
    return xpcc;
}

nsresult
nsXPConnect::GetInterfaceInfo(REFNSIID aIID,
                               nsIInterfaceInfo** info)
{
    NS_PRECONDITION(info,"bad param");
    // XXX implement...FOR REAL...

    *info = new nsInterfaceInfo(aIID, "HARDCODED_INTERFACE_NAME", NULL);

    return NS_OK;
}

nsresult
nsXPConnect::WrapNative(nsIJSContext* aJSContext,
                         nsISupports* aCOMObj,
                         REFNSIID aIID,
                         nsIXPConnectWrappedNative** aWrapper)
{
    NS_PRECONDITION(aJSContext,"bad param");
    NS_PRECONDITION(aCOMObj,"bad param");
    NS_PRECONDITION(aWrapper,"bad param");

    *aWrapper = NULL;

    JSContext* cx;
    if(NS_FAILED(aJSContext->GetNative(&cx)))
        return NS_ERROR_FAILURE;

    XPCContext* xpcc = GetContext(cx);
    if(!xpcc)
        return NS_ERROR_FAILURE;

    nsXPCWrappedNative* wrapper =
        nsXPCWrappedNative::GetNewOrUsedWrapper(xpcc, aCOMObj, aIID);

    if(!wrapper)
        return NS_ERROR_FAILURE;

    *aWrapper = wrapper;
    return NS_OK;
}

nsresult
nsXPConnect::WrapJS(nsIJSContext* aJSContext,
                     nsIJSObject* aJSObj,
                     REFNSIID aIID,
                     nsIXPConnectWrappedJS** aWrapper)
{
    NS_PRECONDITION(aJSContext,"bad param");
    NS_PRECONDITION(aJSObj,"bad param");
    NS_PRECONDITION(aWrapper,"bad param");

    *aWrapper = NULL;

    JSObject* jsobj;
    if(NS_FAILED(aJSObj->GetNative(&jsobj)) || !jsobj)
    {
        NS_ASSERTION(0, "bad nsIJSObject*");
        return NS_ERROR_FAILURE;
    }

    JSContext* cx;
    if(NS_FAILED(aJSContext->GetNative(&cx)))
        return NS_ERROR_FAILURE;

    XPCContext* xpcc = GetContext(cx);
    if(!xpcc)
        return NS_ERROR_FAILURE;

    nsXPCWrappedJS* wrapper =
        nsXPCWrappedJS::GetNewOrUsedWrapper(xpcc, jsobj, aIID);

    if(!wrapper)
        return NS_ERROR_FAILURE;

    *aWrapper = wrapper;
    return NS_OK;
}

nsresult
nsXPConnect::GetJSObjectOfWrappedJS(nsIXPConnectWrappedJS* aWrapper,
                                     nsIJSObject** aJSObj)

{
    NS_PRECONDITION(aWrapper,"bad param");
    NS_PRECONDITION(aJSObj,"bad param");
    // MAJOR ASSUMPTION
    nsXPCWrappedJS* realWrapper = NS_STATIC_CAST(nsXPCWrappedJS*, aWrapper);
    JSObject* jsobj = realWrapper->GetJSObject();
    NS_ASSERTION(jsobj,"bad WrappedJS");
    nsJSContext* aContext =
        new nsJSContext(realWrapper->GetClass()->GetXPCContext()->GetJSContext());
    *aJSObj = new nsJSObject(aContext, jsobj);
    return NS_OK;
}

nsresult
nsXPConnect::GetJSObjectOfWrappedNative(nsIXPConnectWrappedNative* aWrapper,
                                         nsIJSObject** aJSObj)

{
    NS_PRECONDITION(aWrapper,"bad param");
    NS_PRECONDITION(aJSObj,"bad param");

    // MAJOR ASSUMPTION
    nsXPCWrappedNative* realWrapper = NS_STATIC_CAST(nsXPCWrappedNative*, aWrapper);

    JSObject* jsobj = realWrapper->GetJSObject();
    NS_ASSERTION(jsobj,"bad WrappedNative");
    nsJSContext* aContext =
        new nsJSContext(realWrapper->GetClass()->GetXPCContext()->GetJSContext());
    *aJSObj = new nsJSObject(aContext, jsobj);
    return NS_OK;
}

nsresult
nsXPConnect::GetNativeOfWrappedNative(nsIXPConnectWrappedNative* aWrapper,
                                       nsISupports** aObj)
{
    NS_PRECONDITION(aWrapper,"bad param");
    NS_PRECONDITION(aObj,"bad param");
    // MAJOR ASSUMPTION
    nsXPCWrappedNative* realWrapper = NS_STATIC_CAST(nsXPCWrappedNative*, aWrapper);

    *aObj = realWrapper->GetNative();
    NS_ADDREF(*aObj);
    return NS_OK;
}

nsresult
nsXPConnect::GetWrappedNativeOfJSObject(nsIJSContext* aJSContext,
                                        nsIJSObject* aJSObj,
                                        nsIXPConnectWrappedNative** aWrapper)
{
    JSContext* cx;
    JSObject* jsobj;
    NS_PRECONDITION(aJSContext,"bad param");
    NS_PRECONDITION(aJSObj,"bad param");
    NS_PRECONDITION(aWrapper,"bad param");

    if(aWrapper && aJSObj && 
       NS_SUCCEEDED(aJSObj->GetNative(&jsobj)) && jsobj &&
       NS_SUCCEEDED(aJSContext->GetNative(&cx)) && cx)
        *aWrapper = nsXPCWrappedNativeClass::GetWrappedNativeOfJSObject(cx,jsobj);
    else
        *aWrapper = NULL;
    return *aWrapper ? NS_OK : NS_ERROR_FAILURE;
}

XPC_PUBLIC_API(nsIXPConnect*)
XPC_GetXPConnect()
{
    // temp test...
//    nsXPCWrappedJS* p = new nsXPCWrappedJS();
//    p->Stub5();
    return nsXPConnect::GetXPConnect();
}
