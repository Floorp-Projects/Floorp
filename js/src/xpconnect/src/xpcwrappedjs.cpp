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

/* Class that wraps JS objects to appear as XPCOM objects. */

#include "xpcprivate.h"

// NOTE: much of the fancy footwork is done in xpcstubs.cpp

NS_IMETHODIMP
nsXPCWrappedJS::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
    if(NULL == aInstancePtr)
    {
        NS_PRECONDITION(0, "null pointer");
        return NS_ERROR_NULL_POINTER;
    }

    if(aIID.Equals(nsIXPConnectWrappedJSMethods::GetIID()))
    {
        if(!mMethods && !(mMethods = new nsXPCWrappedJSMethods(this)))
        {
            *aInstancePtr = NULL;
            return NS_ERROR_OUT_OF_MEMORY;
        }
        // intentional second addref
        NS_ADDREF(mMethods);
        *aInstancePtr = (void*) mMethods;
        return NS_OK;
    }

    return mClass->DelegatedQueryInterface(this, aIID, aInstancePtr);
}

// do chained ref counting

nsrefcnt
nsXPCWrappedJS::AddRef(void)
{
    NS_PRECONDITION(mRoot, "bad root");

    if(1 == ++mRefCnt && mRoot && mRoot != this)
        NS_ADDREF(mRoot);

    return mRefCnt;
}

nsrefcnt
nsXPCWrappedJS::Release(void)
{
    NS_PRECONDITION(mRoot, "bad root");
    NS_PRECONDITION(0 != mRefCnt, "dup release");

    if(0 == --mRefCnt)
    {
        if(mRoot == this)
        {
            NS_DELETEXPCOM(this);   // cascaded delete
        }
        else
        {
            mRoot->Release();
        }
        return 0;
    }
    return mRefCnt;
}

// static
nsXPCWrappedJS*
nsXPCWrappedJS::GetNewOrUsedWrapper(XPCContext* xpcc,
                                    JSObject* aJSObj,
                                    REFNSIID aIID)
{
    JSObject2WrappedJSMap* map;
    JSObject* rootJSObj;
    nsXPCWrappedJS* root;
    nsXPCWrappedJS* wrapper = NULL;
    nsXPCWrappedJSClass* clazz = NULL;

    NS_PRECONDITION(xpcc, "bad param");
    NS_PRECONDITION(aJSObj, "bad param");

    map = xpcc->GetWrappedJSMap();
    if(!map)
    {
        NS_ASSERTION(map,"bad map");
        return NULL;
    }

    clazz = nsXPCWrappedJSClass::GetNewOrUsedClass(xpcc, aIID);
    if(!clazz)
        return NULL;
    // from here on we need to return through 'return_wrapper'

    // always find the root JSObject
    rootJSObj = clazz->GetRootJSObject(aJSObj);
    if(!rootJSObj)
        goto return_wrapper;

    // look for the root wrapper
    root = map->Find(rootJSObj);
    if(root)
    {
        wrapper = root->Find(aIID);
        if(wrapper)
        {
            NS_ADDREF(wrapper);
            goto return_wrapper;
        }
    }
    else
    {
        // build the root wrapper
        if(rootJSObj == aJSObj)
        {
            // the root will do double duty as the interface wrapper
            wrapper = root = new nsXPCWrappedJS(aJSObj, clazz, NULL);
            if(root)
                map->Add(root);
            goto return_wrapper;
        }
        else
        {
            // just a root wrapper
            nsXPCWrappedJSClass* rootClazz;
            rootClazz = nsXPCWrappedJSClass::GetNewOrUsedClass(
                                                    xpcc, nsISupports::GetIID());
            if(!rootClazz)
                goto return_wrapper;

            root = new nsXPCWrappedJS(rootJSObj, rootClazz, NULL);
            NS_RELEASE(rootClazz);

            if(!root)
                goto return_wrapper;
            map->Add(root);
        }
    }

    // at this point we have a root and may need to build the specific wrapper
    NS_ASSERTION(root,"bad root");
    NS_ASSERTION(clazz,"bad clazz");

    if(!wrapper)
    {
        wrapper = new nsXPCWrappedJS(aJSObj, clazz, root);
        if(!wrapper)
            goto return_wrapper;
    }

    wrapper->mNext = root->mNext;
    root->mNext = wrapper;

return_wrapper:
    if(clazz)
        NS_RELEASE(clazz);
    return wrapper;
}

#ifdef WIN32
#pragma warning(disable : 4355) // OK to pass "this" in member initializer
#endif

nsXPCWrappedJS::nsXPCWrappedJS(JSObject* aJSObj,
                               nsXPCWrappedJSClass* aClass,
                               nsXPCWrappedJS* root)
    : mJSObj(aJSObj),
      mClass(aClass),
      mMethods(NULL),
      mRoot(root ? root : this),
      mNext(NULL)
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();
    NS_ADDREF(aClass);
    JS_AddRoot(mClass->GetXPCContext()->GetJSContext(), &mJSObj);
}

nsXPCWrappedJS::~nsXPCWrappedJS()
{
    NS_PRECONDITION(0 == mRefCnt, "refcounting error");
    if(mRoot == this && GetClass())
    {
        XPCContext* xpcc = GetClass()->GetXPCContext();
        if(xpcc)
        {
            JSObject2WrappedJSMap* map;
            map = xpcc->GetWrappedJSMap();
            if(map)
                map->Remove(this);
        }
    }
    JS_RemoveRoot(mClass->GetXPCContext()->GetJSContext(), &mJSObj);
    NS_RELEASE(mClass);
    if(mMethods)
        NS_RELEASE(mMethods);
    if(mNext)
        NS_DELETEXPCOM(mNext);  // cascaded delete
}

nsXPCWrappedJS*
nsXPCWrappedJS::Find(REFNSIID aIID)
{
    if(aIID.Equals(nsISupports::GetIID()))
        return mRoot;

    nsXPCWrappedJS* cur = mRoot;
    do
    {
        if(aIID.Equals(GetIID()))
            return cur;

    } while(NULL != (cur = cur->mNext));

    return NULL;
}

NS_IMETHODIMP
nsXPCWrappedJS::GetInterfaceInfo(nsIInterfaceInfo** info)
{
    NS_ASSERTION(GetClass(), "wrapper without class");
    NS_ASSERTION(GetClass()->GetInterfaceInfo(), "wrapper class without interface");

    if(!(*info = GetClass()->GetInterfaceInfo()))
        return NS_ERROR_UNEXPECTED;
    NS_ADDREF(*info);
    return NS_OK;
}

NS_IMETHODIMP
nsXPCWrappedJS::CallMethod(PRUint16 methodIndex,
                           const nsXPTMethodInfo* info,
                           nsXPTCMiniVariant* params)
{
    return GetClass()->CallMethod(this, methodIndex, info, params);
}

/***************************************************************************/

NS_IMETHODIMP
nsXPCWrappedJSMethods::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
    NS_PRECONDITION(mWrapper, "bad state");
    return mWrapper->QueryInterface(aIID, aInstancePtr);
}

// maintain a weak link to the wrapper

nsrefcnt
nsXPCWrappedJSMethods::AddRef(void)
{
    NS_PRECONDITION(mWrapper, "bad state");
    if(2 == ++mRefCnt)
        NS_ADDREF(mWrapper);
    return mRefCnt;
}

nsrefcnt
nsXPCWrappedJSMethods::Release(void)
{
    NS_PRECONDITION(mWrapper, "bad state");
    if(0 == --mRefCnt)
    {
        NS_DELETEXPCOM(this);
        return 0;
    }
    else if(1 == mRefCnt)
        mWrapper->Release();    // do NOT zero out the ptr (weak ref)
    return mRefCnt;
}

nsXPCWrappedJSMethods::nsXPCWrappedJSMethods(nsXPCWrappedJS* aWrapper)
    : mWrapper(aWrapper)
{
    NS_PRECONDITION(mWrapper, "bad param");
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();
}

nsXPCWrappedJSMethods::~nsXPCWrappedJSMethods()
{
    NS_ASSERTION(0 == mRefCnt, "recounting error");
}

/***************************************/

NS_IMETHODIMP
nsXPCWrappedJSMethods::GetJSObject(JSObject** aJSObj)
{
    NS_PRECONDITION(mWrapper, "bad state");
    NS_PRECONDITION(aJSObj,"bad param");
    if(!(*aJSObj = mWrapper->GetJSObject()))
        return NS_ERROR_UNEXPECTED;
    return NS_OK;
}

NS_IMETHODIMP
nsXPCWrappedJSMethods::GetInterfaceInfo(nsIInterfaceInfo** info)
{
    NS_PRECONDITION(mWrapper, "bad state");
    NS_PRECONDITION(info, "bad param");
    NS_PRECONDITION(mWrapper->GetClass(), "bad wrapper");
    NS_PRECONDITION(mWrapper->GetClass()->GetInterfaceInfo(), "bad wrapper");

    if(!(*info = mWrapper->GetClass()->GetInterfaceInfo()))
        return NS_ERROR_UNEXPECTED;
    NS_ADDREF(*info);
    return NS_OK;
}

NS_IMETHODIMP
nsXPCWrappedJSMethods::GetIID(nsIID** iid)
{
    NS_PRECONDITION(mWrapper, "bad state");
    NS_PRECONDITION(iid, "bad param");

    *iid = (nsIID*) nsAllocator::Clone(&mWrapper->GetIID(), sizeof(nsIID));
    return *iid ? NS_OK : NS_ERROR_UNEXPECTED;
}

/***************************************************************************/

NS_IMETHODIMP
nsXPCWrappedJSMethods::DebugDump(int depth)
{
#ifdef DEBUG
    XPC_LOG_ALWAYS(("nsXPCWrappedJSMethods @ %x with mRefCnt = %d for...", \
                    this, mRefCnt));
        XPC_LOG_INDENT();
        mWrapper->DebugDump(depth);
        XPC_LOG_OUTDENT();
#endif
    return NS_OK;
}

void
nsXPCWrappedJS::DebugDump(int depth)
{
#ifdef DEBUG
    XPC_LOG_ALWAYS(("nsXPCWrappedJS @ %x with mRefCnt = %d", this, mRefCnt));
        XPC_LOG_INDENT();

        PRBool isRoot = mRoot == this;
        XPC_LOG_ALWAYS(("%s wrapper around JSObject @ %x", \
                         isRoot ? "ROOT":"non-root", mJSObj));
        char* name;
        GetClass()->GetInterfaceInfo()->GetName(&name);
        XPC_LOG_ALWAYS(("interface name is %s", name));
        if(name)
            nsAllocator::Free(name);
        char * iid = GetClass()->GetIID().ToString();
        XPC_LOG_ALWAYS(("IID number is %s", iid));
        delete iid;
        XPC_LOG_ALWAYS(("nsXPCWrappedJSClass @ %x", mClass));
        if(mMethods)
            XPC_LOG_ALWAYS(("mMethods @ %x with mRefCnt = %d", \
                            mMethods, mMethods->GetRefCnt()));
        else
            XPC_LOG_ALWAYS(("NO mMethods object"));

        if(!isRoot)
            XPC_LOG_OUTDENT();
        if(mNext)
        {
            if(isRoot)
            {
                XPC_LOG_ALWAYS(("Additional wrappers for this object..."));
                XPC_LOG_INDENT();
            }
            mNext->DebugDump(depth);
            if(isRoot)
                XPC_LOG_OUTDENT();
        }
        if(isRoot)
            XPC_LOG_OUTDENT();
#endif
}
