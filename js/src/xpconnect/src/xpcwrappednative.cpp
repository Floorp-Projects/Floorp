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

/* Class that wraps each native interface instance. */

#include "xpcprivate.h"

NS_IMPL_QUERY_INTERFACE1(nsXPCWrappedNative, nsIXPConnectWrappedNative)

// do chained ref counting

nsrefcnt
nsXPCWrappedNative::AddRef(void)
{
    NS_PRECONDITION(mRoot, "bad root");
    XPCContext* xpcc;

    if(1 == ++mRefCnt && mRoot != this)
        NS_ADDREF(mRoot);
    else if(2 == mRefCnt && nsnull != (xpcc = mClass->GetXPCContext()))
        JS_AddNamedRoot(xpcc->GetJSContext(), &mJSObj,
                        "nsXPCWrappedNative::mJSObj");
    return mRefCnt;
}

nsrefcnt
nsXPCWrappedNative::Release(void)
{
    NS_PRECONDITION(mRoot, "bad root");
    NS_PRECONDITION(0 != mRefCnt, "dup release");

    if(0 == --mRefCnt)
    {
        NS_DELETEXPCOM(this);   // also unlinks us from chain
        return 0;
    }
    if(1 == mRefCnt)
    {
        XPCContext* xpcc;
        if(nsnull != (xpcc = mClass->GetXPCContext()))
            JS_RemoveRoot(xpcc->GetJSContext(), &mJSObj);
    }
    return mRefCnt;
}

void
nsXPCWrappedNative::JSObjectFinalized(JSContext *cx, JSObject *obj)
{
    nsIXPCScriptable* ds;
    nsIXPCScriptable* as;
    if(nsnull != (ds = GetDynamicScriptable()) &&
       nsnull != (as = GetArbitraryScriptable()))
        ds->Finalize(cx, obj, this, as);

    // pass through to the real JSObject finalization code
    JS_FinalizeStub(cx, obj);
    mJSObj = nsnull;
    Release();
}

// static
nsXPCWrappedNative*
nsXPCWrappedNative::GetNewOrUsedWrapper(XPCContext* xpcc,
                                       nsISupports* aObj,
                                       REFNSIID aIID)
{
    Native2WrappedNativeMap* map;
    nsISupports* rootObj = nsnull;
    nsISupports* realObj = nsnull;
    nsXPCWrappedNative* root;
    nsXPCWrappedNative* wrapper = nsnull;
    nsXPCWrappedNativeClass* clazz = nsnull;

    NS_PRECONDITION(xpcc, "bad param");
    NS_PRECONDITION(aObj, "bad param");

    map = xpcc->GetWrappedNativeMap();
    if(!map)
    {
        NS_ASSERTION(map,"bad map");
        return nsnull;
    }

    // always find the native root

    if(NS_FAILED(aObj->QueryInterface(nsCOMTypeInfo<nsISupports>::GetIID(), (void**)&rootObj)))
        goto return_wrapper;

    // look for the root wrapper

    root = map->Find(rootObj);
    if(root)
    {
        // if we already have a wrapper for this interface then we're done.
        wrapper = root->Find(aIID);
        if(wrapper)
        {
            NS_ADDREF(wrapper);
            goto return_wrapper;
        }
    }

    // do a QI to make sure the object passed in really supports the
    // interface it is claiming to support.

    if(NS_FAILED(aObj->QueryInterface(aIID, (void**)&realObj)))
        goto return_wrapper;

    // do the security check if necessary

    nsIXPCSecurityManager* sm;
    if(nsnull != (sm = xpcc->GetSecurityManager()) &&
       (xpcc->GetSecurityManagerFlags() &
        nsIXPCSecurityManager::HOOK_CREATE_WRAPPER) &&
       NS_OK != sm->CanCreateWrapper(xpcc->GetJSContext(), aIID, realObj))
    {
        // the security manager vetoed. It should have set an exception.
        goto return_wrapper;
    }

    // need to make a wrapper

    clazz = nsXPCWrappedNativeClass::GetNewOrUsedClass(xpcc, aIID);
    if(!clazz)
        goto return_wrapper;

    // build the root wrapper

    if(!root)
    {
        if(rootObj == realObj)
        {
            // the root will do double duty as the interface wrapper
            wrapper = root = new nsXPCWrappedNative(realObj, clazz, nsnull);
            if(!wrapper)
                goto return_wrapper;
            if(!wrapper->mJSObj)
            {
                NS_RELEASE(wrapper);    // sets wrapper to nsnull
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
                                                    xpcc, nsCOMTypeInfo<nsISupports>::GetIID());
            if(!rootClazz)
                goto return_wrapper;

            root = new nsXPCWrappedNative(rootObj, rootClazz, nsnull);
            NS_RELEASE(rootClazz);

            if(!root)
                goto return_wrapper;
            if(!root->mJSObj)
            {
                NS_RELEASE(root);    // sets root to nsnull
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
        wrapper = new nsXPCWrappedNative(realObj, clazz, root);
        if(!wrapper)
            goto return_wrapper;
        if(!wrapper->mJSObj)
        {
            NS_RELEASE(wrapper);    // sets wrapper to nsnull
            goto return_wrapper;
        }
    }

    // The logic above requires this. If not so then someone hacked it!
    NS_ASSERTION(wrapper && wrapper != root ,"bad wrapper");

    // splice into the wrapper chain

    wrapper->mNext = root->mNext;
    root->mNext = wrapper;

return_wrapper:
    if(rootObj)
        NS_RELEASE(rootObj);
    if(realObj)
        NS_RELEASE(realObj);
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
      mJSObj(nsnull),
      mClass(aClass),
      mDynamicScriptable(nsnull),
      mDynamicScriptableFlags(0),
      mRoot(root ? root : this),
      mNext(nsnull),
      mFinalizeListener(nsnull)
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
        {
            mDynamicScriptable = ds;
        }
    }

    mJSObj = aClass->NewInstanceJSObject(this);
    if(mJSObj)
    {
        // intentional second addref to be released when mJSObj is gc'd
        NS_ADDREF_THIS();

        nsIXPCScriptable* ds;
        nsIXPCScriptable* as;
        XPCContext* xpcc;

        if(nsnull != (ds = GetDynamicScriptable()) &&
           nsnull != (as = GetArbitraryScriptable()) &&
           nsnull != (xpcc = GetClass()->GetXPCContext()))
        {
            ds->Create(xpcc->GetJSContext(), GetJSObject(), this, as);
            if(mRoot == this)
                ds->GetFlags(xpcc->GetJSContext(), GetJSObject(), this,
                             &mDynamicScriptableFlags, as);
        }
    }
}

void
nsXPCWrappedNative::XPCContextBeingDestroyed()
{
    XPCContext* xpcc;
    if(mJSObj && mClass && nsnull != (xpcc = mClass->GetXPCContext()))
        JS_RemoveRoot(xpcc->GetJSContext(), &mJSObj);
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
        if(nsnull != (clazz = GetClass()) &&
           nsnull != (xpcc = clazz->GetXPCContext()) &&
           nsnull != (map = xpcc->GetWrappedNativeMap()))
        {
            map->Remove(this);
        }
    }
    NS_IF_RELEASE(mDynamicScriptable);

    if(mFinalizeListener)
    {
        if(mObj)
            mFinalizeListener->AboutToRelease(mObj);
        NS_RELEASE(mFinalizeListener);
    }
    NS_IF_RELEASE(mObj);
    NS_IF_RELEASE(mClass);
}

nsXPCWrappedNative*
nsXPCWrappedNative::Find(REFNSIID aIID)
{
    if(aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID()))
        return mRoot;

    nsXPCWrappedNative* cur = mRoot;
    do
    {
        if(aIID.Equals(cur->GetIID()))
            return cur;

    } while(nsnull != (cur = cur->mNext));

    return nsnull;
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
    *p = nsnull;
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
    *p = nsnull;
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
nsXPCWrappedNative::SetFinalizeListener(nsIXPConnectFinalizeListener* aListener)
{
    /* if the object already has a listener, then we fail */
    if(mFinalizeListener && aListener)
        return NS_ERROR_FAILURE;

    if(mFinalizeListener)
        NS_RELEASE(mFinalizeListener);
    mFinalizeListener = aListener;
    if(mFinalizeListener)
        NS_ADDREF(mFinalizeListener);
    return NS_OK;
}

NS_IMETHODIMP
nsXPCWrappedNative::GetJSObjectPrototype(JSObject** aJSObj)
{
    NS_PRECONDITION(aJSObj, "bad param");
    NS_PRECONDITION(mJSObj, "bad wrapper");
    return NS_ERROR_NOT_IMPLEMENTED;
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

