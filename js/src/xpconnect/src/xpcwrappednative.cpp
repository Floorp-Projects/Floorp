/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   John Bandhauer <jband@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

/* Class that wraps each native interface instance. */

#include "xpcprivate.h"

NS_IMPL_THREADSAFE_QUERY_INTERFACE2(nsXPCWrappedNative, nsIXPConnectWrappedNative, nsIXPConnectJSObjectHolder)

// do chained ref counting

nsrefcnt
nsXPCWrappedNative::AddRef(void)
{
    NS_PRECONDITION(mRoot, "bad root");
    nsrefcnt cnt = (nsrefcnt) PR_AtomicIncrement((PRInt32*)&mRefCnt);
    NS_LOG_ADDREF(this, cnt, "nsXPCWrappedNative", sizeof(*this));
    if(2 == cnt)
    {
        AutoPushCompatibleJSContext 
                    autoContext(mClass->GetRuntime()->GetJSRuntime());
        JSContext* cx = autoContext.GetJSContext();
        if(cx)
            JS_AddNamedRoot(cx, &mJSObj, "nsXPCWrappedNative::mJSObj");
    }
    return cnt;
}

nsrefcnt
nsXPCWrappedNative::Release(void)
{
    NS_PRECONDITION(mRoot, "bad root");
    NS_PRECONDITION(0 != mRefCnt, "dup release");
    nsrefcnt cnt = (nsrefcnt) PR_AtomicDecrement((PRInt32*)&mRefCnt);
    NS_LOG_RELEASE(this, cnt, "nsXPCWrappedNative");
    if(0 == cnt)
    {
        NS_DELETEXPCOM(this);   // also unlinks us from chain
        return 0;
    }
    if(1 == cnt)
    {
        XPCJSRuntime* rt = mClass->GetRuntime();
        if(rt)
            JS_RemoveRootRT(rt->GetJSRuntime(), &mJSObj);
    }
    return cnt;
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

#define SET_ERROR_CODE(_y) if(pErr) *pErr = _y

// static
nsXPCWrappedNative*
nsXPCWrappedNative::GetNewOrUsedWrapper(XPCContext* xpcc,
                                        nsXPCWrappedNativeScope* aScope,
                                        JSObject* aScopingJSObject,
                                        nsISupports* aObj,
                                        REFNSIID aIID,
                                        nsresult* pErr)
{
    Native2WrappedNativeMap* map;
    nsISupports* rootObj = nsnull;
    nsISupports* realObj = nsnull;
    XPCJSRuntime* rt;
    nsXPCWrappedNative* root;
    nsXPCWrappedNative* wrapper = nsnull;
    nsXPCWrappedNativeClass* clazz = nsnull;

    NS_PRECONDITION(xpcc, "bad param");
    NS_PRECONDITION(aObj, "bad param");

    SET_ERROR_CODE(NS_ERROR_FAILURE);
    
    if(!xpcc || !aScope || !(rt = xpcc->GetRuntime()) || !aObj)
    {
        NS_ASSERTION(0,"bad param");    
        return nsnull;
    }

    map = aScope->GetWrappedNativeMap();
    if(!map)
    {
        NS_ASSERTION(map,"bad map");
        return nsnull;
    }

    // always find the native root

    if(NS_FAILED(aObj->QueryInterface(NS_GET_IID(nsISupports), 
                                      (void**)&rootObj)))
        goto return_wrapper;

    // look for the root wrapper

    {   // scoped lock
        nsAutoLock lock(rt->GetMapLock());  
        root = map->Find(rootObj);
    }
    if(root)
    {
        // if we already have a wrapper for this interface then we're done.
        wrapper = root->Find(aIID);
        if(wrapper)
        {
            NS_ADDREF(wrapper);
            goto return_wrapper;
        }

        NS_ADDREF(root);
    }

    // do a QI to make sure the object passed in really supports the
    // interface it is claiming to support.

    if(NS_FAILED(aObj->QueryInterface(aIID, (void**)&realObj)))
        goto return_wrapper;

    // do the security check if necessary

    nsIXPCSecurityManager* sm;
       sm = xpcc->GetAppropriateSecurityManager(
                            nsIXPCSecurityManager::HOOK_CREATE_WRAPPER);
    if(sm && NS_OK != sm->CanCreateWrapper(xpcc->GetJSContext(), aIID, realObj))
    {
        // the security manager vetoed. It should have set an exception.
        SET_ERROR_CODE(NS_ERROR_XPC_SECURITY_MANAGER_VETO);
        goto return_wrapper;
    }

    // need to make a wrapper

    clazz = nsXPCWrappedNativeClass::GetNewOrUsedClass(xpcc, aIID, pErr);
    if(!clazz)
        goto return_wrapper;

    // build the root wrapper

    if(!root)
    {
        if(rootObj == realObj)
        {
            // the root will do double duty as the interface wrapper
            wrapper = root = new nsXPCWrappedNative(xpcc, realObj, aScope, 
                                                    aScopingJSObject,
                                                    clazz, nsnull);
            if(!wrapper)
                goto return_wrapper;
            if(!wrapper->mJSObj)
            {
                NS_RELEASE(wrapper);    // sets wrapper to nsnull
                goto return_wrapper;
            }

            {   // scoped lock
                nsAutoLock lock(rt->GetMapLock());  
                map->Add(root);
            }
            goto return_wrapper;
        }
        else
        {
            // just a root wrapper
            nsXPCWrappedNativeClass* rootClazz;
            rootClazz = nsXPCWrappedNativeClass::GetNewOrUsedClass(
                                  xpcc, NS_GET_IID(nsISupports), pErr);
            if(!rootClazz)
                goto return_wrapper;

            root = new nsXPCWrappedNative(xpcc, rootObj, aScope, 
                                          aScopingJSObject, rootClazz, nsnull);
            NS_RELEASE(rootClazz);

            if(!root)
                goto return_wrapper;
            if(!root->mJSObj)
            {
                NS_RELEASE(root);    // sets root to nsnull
                goto return_wrapper;
            }
            {   // scoped lock
                nsAutoLock lock(rt->GetMapLock());  
                map->Add(root);
            }
        }
    }

    // at this point we have a root and may need to build the specific wrapper
    NS_ASSERTION(root,"bad root");
    NS_ASSERTION(clazz,"bad clazz");

    if(!wrapper)
    {
        wrapper = new nsXPCWrappedNative(xpcc, realObj, aScope, 
                                         aScopingJSObject, clazz, root);
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

    NS_RELEASE(root);

return_wrapper:
    if(rootObj)
        NS_RELEASE(rootObj);
    if(realObj)
        NS_RELEASE(realObj);
    if(clazz)
        NS_RELEASE(clazz);
    
    if(wrapper)
        SET_ERROR_CODE(NS_OK);
    return wrapper;
}

#ifdef WIN32
#pragma warning(disable : 4355) // OK to pass "this" in member initializer
#endif

nsXPCWrappedNative::nsXPCWrappedNative(XPCContext* xpcc,
                                       nsISupports* aObj,
                                       nsXPCWrappedNativeScope* aScope,
                                       JSObject* aScopingJSObject,
                                       nsXPCWrappedNativeClass* aClass,
                                       nsXPCWrappedNative* root)
    : mObj(aObj),
      mJSObj(nsnull),
      mClass(aClass),
      mScope(aScope),
      mDynamicScriptable(nsnull),
      mDynamicScriptableFlags(0),
      mRoot(root ? root : this),
      mNext(nsnull),
      mFinalizeListener(nsnull)
{
    NS_PRECONDITION(xpcc, "bad object to wrap");
    NS_PRECONDITION(mObj, "bad object to wrap");
    NS_PRECONDITION(mClass, "bad class for wrapper");
    NS_PRECONDITION(mScope, "bad scope for wrapper");
    NS_PRECONDITION(aScopingJSObject, "bad scoping js object for wrapper");
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();
    NS_ADDREF(mClass);
    NS_ADDREF(mObj);
    NS_ADDREF(mScope);

#ifdef XPC_CHECK_WRAPPERS_AT_SHUTDOWN
    mScope->GetRuntime()->DEBUG_AddWrappedNative(this);
#endif

#ifdef DEBUG_stats_jband
    static int count = 0;
    static const int interval = 10;
    if(0 == (++count % interval))
        printf("++++++++ %d instances of nsXPCWrappedNative created\n", count);
#endif

    if(mRoot == this)
    {
        nsIXPCScriptable* ds;
        if(NS_SUCCEEDED(mObj->QueryInterface(NS_GET_IID(nsIXPCScriptable),
                                             (void**)&ds)))
        {
            mDynamicScriptable = ds;
        }
    }
    else
    {
        NS_ADDREF(mRoot);
    }

    mJSObj = aClass->NewInstanceJSObject(xpcc, aScope, aScopingJSObject, this);
    if(mJSObj)
    {
        // intentional second addref to be released when mJSObj is gc'd
        NS_ADDREF_THIS();

        nsIXPCScriptable* ds;
        nsIXPCScriptable* as;
        if(nsnull != (ds = GetDynamicScriptable()) &&
           nsnull != (as = GetArbitraryScriptable()))
        {
            ds->Create(xpcc->GetJSContext(), GetJSObject(), this, as);
            if(mRoot == this)
                ds->GetFlags(xpcc->GetJSContext(), GetJSObject(), this,
                             &mDynamicScriptableFlags, as);
        }
    }
}

nsXPCWrappedNative::~nsXPCWrappedNative()
{
    NS_PRECONDITION(0 == mRefCnt, "refcounting error");
    NS_ASSERTION(mRoot, "wrapper without root deleted");

#ifdef XPC_CHECK_WRAPPERS_AT_SHUTDOWN
    mScope->GetRuntime()->DEBUG_RemoveWrappedNative(this);
#endif

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
        Native2WrappedNativeMap* map;
        if(nsnull != (clazz = GetClass()) &&
           nsnull != (map = mScope->GetWrappedNativeMap()))
        {
           // scoped lock
            nsAutoLock lock(mClass->GetRuntime()->GetMapLock());  
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
    NS_IF_RELEASE(mScope);
}

nsXPCWrappedNative*
nsXPCWrappedNative::Find(REFNSIID aIID)
{
    if(aIID.Equals(NS_GET_IID(nsISupports)))
        return mRoot;

    nsXPCWrappedNative* cur = mRoot;
    do
    {
        if(aIID.Equals(cur->GetIID()))
            return cur;

    } while(nsnull != (cur = cur->mNext));

    return nsnull;
}

void 
nsXPCWrappedNative::SystemIsBeingShutDown()
{
    // XXX It turns out that it is better to leak here then to do any Releases 
    // and have them propagate into all sorts of mischief as the system is being
    // shutdown. This was learned the hard way :(    
    
    // mObj == nsnull is used to indicate that the wrapper is no longer valid
    // and that calls from JS should fail without trying to use any of the 
    // xpconnect mechanisms. 'IsValid' is implemented by checking this pointer.
    mObj = nsnull;
    
    // Notify other wrappers in the chain.
    if(mNext)
        mNext->SystemIsBeingShutDown();
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

    *iid = (nsIID*) nsMemory::Clone(&GetIID(), sizeof(nsIID));
    return *iid ? NS_OK : NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsXPCWrappedNative::SetFinalizeListener(nsIXPConnectFinalizeListener* aListener)
{
    /* if the object already has a listener, then we fail */
    if(mFinalizeListener && aListener)
        return NS_ERROR_FAILURE;

    NS_IF_ADDREF(aListener);
    NS_IF_RELEASE(mFinalizeListener);
    mFinalizeListener = aListener;
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
nsXPCWrappedNative::DebugDump(PRInt16 depth)
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
        XPC_LOG_ALWAYS(("nsXPCWrappedNativeScope @ %x", mScope));
        XPC_LOG_ALWAYS(("nsIXPConnectFinalizeListener @ %x", mFinalizeListener));
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

/***************************************************************************/

NS_IMPL_THREADSAFE_ISUPPORTS1(nsXPCJSObjectHolder, nsIXPConnectJSObjectHolder)

NS_IMETHODIMP
nsXPCJSObjectHolder::GetJSObject(JSObject** aJSObj)
{
    NS_PRECONDITION(aJSObj, "bad param");
    NS_PRECONDITION(mJSObj, "bad object state");
    *aJSObj = mJSObj;
    return NS_OK;
}

nsXPCJSObjectHolder::nsXPCJSObjectHolder(JSContext* cx, JSObject* obj)
    : mRuntime(JS_GetRuntime(cx)), mJSObj(obj)
{
    NS_INIT_REFCNT();
    JS_AddNamedRoot(cx, &mJSObj, "nsXPCJSObjectHolder::mJSObj");
}

nsXPCJSObjectHolder::~nsXPCJSObjectHolder()
{
    JS_RemoveRootRT(mRuntime, &mJSObj);
}

nsXPCJSObjectHolder* 
nsXPCJSObjectHolder::newHolder(JSContext* cx, JSObject* obj)
{
    if(!cx || !obj)
    {
        NS_ASSERTION(0, "bad param");
        return nsnull;
    }
    return new nsXPCJSObjectHolder(cx, obj);        
}

