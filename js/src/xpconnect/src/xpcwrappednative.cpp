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

/* Class that wraps each native interface instance. */

#include "xpcprivate.h"

static NS_DEFINE_IID(kWrappedNativeIID, NS_IXPCONNECT_WRAPPED_NATIVE_IID);
NS_IMPL_QUERY_INTERFACE(nsXPCWrappedNative, kWrappedNativeIID)

// do chained ref counting

nsrefcnt
nsXPCWrappedNative::AddRef(void)
{
    NS_PRECONDITION(mRoot, "bad root");

    if(1 == ++mRefCnt && mRoot != this)
        NS_ADDREF(mRoot);
    else if(2 == mRefCnt)
        JS_AddRoot(mClass->GetXPCContext()->GetJSContext(), &mJSObj);

//    XPC_LOG_DEBUG(("+++ AddRef  of %x with mJSObj %x and mRefCnt = %d",this,mJSObj, mRefCnt));

    return mRefCnt;
}

nsrefcnt
nsXPCWrappedNative::Release(void)
{
    NS_PRECONDITION(mRoot, "bad root");
    NS_PRECONDITION(0 != mRefCnt, "dup release");

    if(0 == --mRefCnt)
    {
//        XPC_LOG_DEBUG(("--- Delete of wrapper %x with mJSObj %x and mRefCnt = %d",this,mJSObj, mRefCnt));
        NS_DELETEXPCOM(this);   // also unlinks us from chain
        return 0;
    }
    if(1 == mRefCnt)
    {
//        XPC_LOG_DEBUG(("--- Removing root of %x with mJSObj %x and mRefCnt = %d",this,mJSObj, mRefCnt));
        JS_RemoveRoot(mClass->GetXPCContext()->GetJSContext(), &mJSObj);
    }

//    XPC_LOG_DEBUG(("--- Release of %x with mJSObj %x and mRefCnt = %d",this,mJSObj, mRefCnt));

    return mRefCnt;
}

void
nsXPCWrappedNative::JSObjectFinalized(JSContext *cx, JSObject *obj)
{
    NS_PRECONDITION(1 == mRefCnt, "bad JSObject finalization");

    nsIXPCScriptable* ds;
    if(NULL != (ds = GetDynamicScriptable()))
        ds->Finalize(cx, obj, this, GetArbitraryScriptable());

    // pass through to the real JSObject finalization code
    JS_FinalizeStub(cx, obj);
    mJSObj = NULL;
    Release();
}

// static
nsXPCWrappedNative*
nsXPCWrappedNative::GetNewOrUsedWrapper(XPCContext* xpcc,
                                       nsISupports* aObj,
                                       REFNSIID aIID)
{
    Native2WrappedNativeMap* map;
    nsISupports* rootObj = NULL;
    nsXPCWrappedNative* root;
    nsXPCWrappedNative* wrapper = NULL;
    nsXPCWrappedNativeClass* clazz = NULL;

    NS_PRECONDITION(xpcc, "bad param");
    NS_PRECONDITION(aObj, "bad param");

    map = xpcc->GetWrappedNativeMap();
    if(!map)
    {
        NS_ASSERTION(map,"bad map");
        return NULL;
    }

    // always find the native root
    if(NS_FAILED(aObj->QueryInterface(nsISupports::GetIID(), (void**)&rootObj)))
        return NULL;

    // look for the root wrapper
    root = map->Find(rootObj);
    if(root)
    {
        wrapper = root->Find(aIID);
        if(wrapper)
        {
            NS_ADDREF(wrapper);
            goto return_wrapper;
        }
    }

    clazz = nsXPCWrappedNativeClass::GetNewOrUsedClass(xpcc, aIID);
    if(!clazz)
        goto return_wrapper;

    // build the root wrapper

    if(!root)
    {
        if(rootObj == aObj)
        {
            // the root will do double duty as the interface wrapper
            wrapper = root = new nsXPCWrappedNative(aObj, clazz, NULL);
            if(!wrapper)
                goto return_wrapper;
            if(!wrapper->mJSObj)
            {
                NS_RELEASE(wrapper);    // sets wrapper to NULL
                goto return_wrapper;
            }

            map->Add(root);
            goto return_wrapper;
        }
        else
        {
            // just a root wrapper
            nsXPCWrappedNativeClass* rootClazz;
            rootClazz = nsXPCWrappedNativeClass::GetNewOrUsedClass(
                                                    xpcc, nsISupports::GetIID());
            if(!rootClazz)
                goto return_wrapper;

            root = new nsXPCWrappedNative(rootObj, rootClazz, NULL);
            NS_RELEASE(rootClazz);

            if(!root)
                goto return_wrapper;
            if(!root->mJSObj)
            {
                NS_RELEASE(root);    // sets root to NULL
                goto return_wrapper;
            }
            map->Add(root);
        }
    }

    // at this point we have a root and may need to build the specific wrapper
    NS_ASSERTION(root,"bad root");
    NS_ASSERTION(clazz,"bad clazz");

    if(!wrapper)
    {
        wrapper = new nsXPCWrappedNative(aObj, clazz, root);
        if(!wrapper)
            goto return_wrapper;
        if(!wrapper->mJSObj)
        {
            NS_RELEASE(wrapper);    // sets wrapper to NULL
            goto return_wrapper;
        }
    }

    wrapper->mNext = root->mNext;
    root->mNext = wrapper;

return_wrapper:
    if(rootObj)
        NS_RELEASE(rootObj);
    if(clazz)
        NS_RELEASE(clazz);
    return wrapper;
}

#ifdef WIN32
#pragma warning(disable : 4355) // OK to pass "this" in member initializer
#endif

nsXPCWrappedNative::nsXPCWrappedNative(nsISupports* aObj,
                                     nsXPCWrappedNativeClass* aClass,
                                     nsXPCWrappedNative* root)
    : mObj(aObj),
      mJSObj(NULL),
      mClass(aClass),
      mDynamicScriptable(NULL),
      mRoot(root ? root : this),
      mNext(NULL)

{
    NS_PRECONDITION(mObj, "bad object to wrap");
    NS_PRECONDITION(mClass, "bad class for wrapper");
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();
    NS_ADDREF(aClass);
    NS_ADDREF(aObj);

    if(mRoot == this)
    {
        nsIXPCScriptable* ds;
        if(NS_SUCCEEDED(mObj->QueryInterface(nsIXPCScriptable::GetIID(),
                                             (void**)&ds)))
            mDynamicScriptable = ds;
    }

    mJSObj = aClass->NewInstanceJSObject(this);
    if(mJSObj)
    {
        // intentional second addref to be released when mJSObj is gc'd
        NS_ADDREF_THIS();

        nsIXPCScriptable* ds;
        if(NULL != (ds = GetDynamicScriptable()))
            ds->Create(GetClass()->GetXPCContext()->GetJSContext(),
                       GetJSObject(), this, GetArbitraryScriptable());
    }
}

nsXPCWrappedNative::~nsXPCWrappedNative()
{
    NS_PRECONDITION(0 == mRefCnt, "refcounting error");
    NS_ASSERTION(mRoot, "wrapper without root deleted");

    if(mRoot != this)
    {
        // unlink this wrapper
        nsXPCWrappedNative* cur = mRoot;
        while(1)
        {
            if(cur->mNext == this)
            {
                cur->mNext = mNext;
                break;   
            }
            cur = cur->mNext;
            NS_ASSERTION(cur, "failed to find wrapper in its own chain");
        }
        // let the root go
        NS_RELEASE(mRoot);
    }
    else 
    {
        // remove this root wrapper from the map
        NS_ASSERTION(!mNext, "root wrapper with non-empty chain being deleted");
        nsXPCWrappedNativeClass* clazz;
        XPCContext* xpcc;
        Native2WrappedNativeMap* map;
        if(NULL != (clazz = GetClass()) &&
           NULL != (xpcc = clazz->GetXPCContext())&&
           NULL != (map = xpcc->GetWrappedNativeMap()))
        {
            map->Remove(this);
        }
    }
    if(mDynamicScriptable)
        NS_RELEASE(mDynamicScriptable);
    if(mClass)
        NS_RELEASE(mClass);
    if(mObj)
        NS_RELEASE(mObj);
}

nsXPCWrappedNative*
nsXPCWrappedNative::Find(REFNSIID aIID)
{
    if(aIID.Equals(nsISupports::GetIID()))
        return mRoot;

    nsXPCWrappedNative* cur = mRoot;
    do
    {
        if(aIID.Equals(GetIID()))
            return cur;

    } while(NULL != (cur = cur->mNext));

    return NULL;
}

/***************************************************************************/

NS_IMETHODIMP
nsXPCWrappedNative::GetArbitraryScriptable(nsIXPCScriptable** p)
{
    NS_PRECONDITION(p, "bad param");
    nsIXPCScriptable* s = GetArbitraryScriptable();
    if(s)
    {
        NS_ADDREF(s);
        *p = s;
        return NS_OK;
    }
    // else...
    *p = NULL;
    return NS_ERROR_NO_INTERFACE;
}

NS_IMETHODIMP
nsXPCWrappedNative::GetDynamicScriptable(nsIXPCScriptable** p)
{
    NS_PRECONDITION(p, "bad param");
    nsIXPCScriptable* s = GetDynamicScriptable();
    if(s)
    {
        NS_ADDREF(s);
        *p = s;
        return NS_OK;
    }
    // else...
    *p = NULL;
    return NS_ERROR_NO_INTERFACE;
}

NS_IMETHODIMP
nsXPCWrappedNative::GetJSObject(JSObject** aJSObj)
{
    NS_PRECONDITION(aJSObj, "bad param");
    NS_PRECONDITION(mJSObj, "bad wrapper");
    if(!(*aJSObj = mJSObj))
        return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsXPCWrappedNative::GetNative(nsISupports** aObj)
{
    NS_PRECONDITION(aObj, "bad param");
    NS_PRECONDITION(mObj, "bad wrapper");
    if(!(*aObj = mObj))
        return NS_ERROR_UNEXPECTED;
    NS_ADDREF(mObj);
    return NS_OK;
}

NS_IMETHODIMP
nsXPCWrappedNative::GetInterfaceInfo(nsIInterfaceInfo** info)
{
    NS_PRECONDITION(info, "bad param");
    NS_PRECONDITION(GetClass(), "bad wrapper");
    NS_PRECONDITION(GetClass()->GetInterfaceInfo(), "bad wrapper");
    if(!(*info = GetClass()->GetInterfaceInfo()))
        return NS_ERROR_UNEXPECTED;
    NS_ADDREF(*info);
    return NS_OK;
}

NS_IMETHODIMP
nsXPCWrappedNative::GetIID(nsIID** iid)
{
    NS_PRECONDITION(iid, "bad param");

    *iid = (nsIID*) nsAllocator::Clone(&GetIID(), sizeof(nsIID));
    return *iid ? NS_OK : NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsXPCWrappedNative::DebugDump(int depth)
{
#ifdef DEBUG
    depth-- ;
    XPC_LOG_ALWAYS(("nsXPCWrappedNative @ %x with mRefCnt = %d", this, mRefCnt));
    XPC_LOG_INDENT();
        PRBool isRoot = mRoot == this;
        XPC_LOG_ALWAYS(("%s wrapper around native @ %x", \
                         isRoot ? "ROOT":"non-root", mObj));
        XPC_LOG_ALWAYS(("interface name is %s", GetClass()->GetInterfaceName()));
        char * iid = GetClass()->GetIID().ToString();
        XPC_LOG_ALWAYS(("IID number is %s", iid));
        delete iid;
        XPC_LOG_ALWAYS(("JSObject @ %x", mJSObj));
        XPC_LOG_ALWAYS(("nsXPCWrappedNativeClass @ %x", mClass));
        if(GetDynamicScriptable())
            XPC_LOG_ALWAYS(("DynamicScriptable @ %x", GetDynamicScriptable()));
        else
            XPC_LOG_ALWAYS(("NO DynamicScriptable"));

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
    return NS_OK;
}

