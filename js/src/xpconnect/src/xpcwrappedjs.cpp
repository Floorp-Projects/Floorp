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

/* Class that wraps JS objects to appear as XPCOM objects. */

#include "xpcprivate.h"

// NOTE: much of the fancy footwork is done in xpcstubs.cpp

NS_IMETHODIMP
nsXPCWrappedJS::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
    if(!IsValid())
        return NS_ERROR_UNEXPECTED;

    if(nsnull == aInstancePtr)
    {
        NS_PRECONDITION(0, "null pointer");
        return NS_ERROR_NULL_POINTER;
    }

    if(aIID.Equals(NS_GET_IID(nsIXPConnectWrappedJS)))
    {
        NS_ADDREF_THIS();
        *aInstancePtr = (void*) NS_STATIC_CAST(nsIXPConnectWrappedJS*,this);
        return NS_OK;
    }

    if(aIID.Equals(NS_GET_IID(nsIXPConnectJSObjectHolder)))
    {
        NS_ADDREF_THIS();
        *aInstancePtr = (void*) NS_STATIC_CAST(nsIXPConnectJSObjectHolder*,this);
        return NS_OK;
    }

    // else...

    return mClass->DelegatedQueryInterface(this, aIID, aInstancePtr);
}

// do chained ref counting

nsrefcnt
nsXPCWrappedJS::AddRef(void)
{
    NS_PRECONDITION(mRoot, "bad root");
    nsrefcnt cnt = (nsrefcnt) PR_AtomicIncrement((PRInt32*)&mRefCnt);
    NS_LOG_ADDREF(this, cnt, "nsXPCWrappedJS", sizeof(*this));
    if(1 == cnt && mRoot && mRoot != this)
        NS_ADDREF(mRoot);

    return cnt;
}

nsrefcnt
nsXPCWrappedJS::Release(void)
{
    NS_PRECONDITION(mRoot, "bad root");
    NS_PRECONDITION(0 != mRefCnt, "dup release");
    nsrefcnt cnt = (nsrefcnt) PR_AtomicDecrement((PRInt32*)&mRefCnt);
    NS_LOG_RELEASE(this, cnt, "nsXPCWrappedJS");
    if(0 == cnt)
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
    return cnt;
}

NS_IMETHODIMP
nsXPCWrappedJS::GetJSObject(JSObject** aJSObj)
{
    NS_PRECONDITION(aJSObj, "bad param");
    NS_PRECONDITION(mJSObj, "bad wrapper");
    if(!(*aJSObj = mJSObj))
        return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
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
    nsXPCWrappedJS* wrapper = nsnull;
    nsXPCWrappedJSClass* clazz = nsnull;
    XPCJSRuntime* rt;
    
    if(!xpcc || !(rt = xpcc->GetRuntime()) || !aJSObj)
    {
        NS_ASSERTION(0,"bad param");    
        return nsnull;
    }

    map = rt->GetWrappedJSMap();
    if(!map)
    {
        NS_ASSERTION(map,"bad map");
        return nsnull;
    }

    clazz = nsXPCWrappedJSClass::GetNewOrUsedClass(rt, aIID);
    if(!clazz)
        return nsnull;
    // from here on we need to return through 'return_wrapper'

    // always find the root JSObject
    rootJSObj = clazz->GetRootJSObject(aJSObj);
    if(!rootJSObj)
        goto return_wrapper;

    // look for the root wrapper
    {   // scoped lock
        nsAutoLock lock(rt->GetMapLock());  
        root = map->Find(rootJSObj);
    }
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
            wrapper = root = new nsXPCWrappedJS(xpcc, aJSObj, clazz, nsnull);
            if(root)
            {   // scoped lock
                nsAutoLock lock(rt->GetMapLock());  
                map->Add(root);
            }
            goto return_wrapper;
        }
        else
        {
            // just a root wrapper
            nsXPCWrappedJSClass* rootClazz;
            rootClazz = nsXPCWrappedJSClass::GetNewOrUsedClass(
                                                rt, NS_GET_IID(nsISupports));
            if(!rootClazz)
                goto return_wrapper;

            root = new nsXPCWrappedJS(xpcc, rootJSObj, rootClazz, nsnull);
            NS_RELEASE(rootClazz);

            if(!root)
                goto return_wrapper;
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
        wrapper = new nsXPCWrappedJS(xpcc, aJSObj, clazz, root);
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

nsXPCWrappedJS::nsXPCWrappedJS(XPCContext* xpcc,
                               JSObject* aJSObj,
                               nsXPCWrappedJSClass* aClass,
                               nsXPCWrappedJS* root)
    : mJSObj(aJSObj),
      mClass(aClass),
      mRoot(root ? root : this),
      mNext(nsnull)
{
#ifdef DEBUG_stats_jband
    static int count = 0;
    static const int interval = 10;
    if(0 == (++count % interval))
        printf("//////// %d instances of nsXPCWrappedJS created\n", count);
#endif

    NS_INIT_REFCNT();
    NS_ADDREF_THIS();
    NS_ADDREF(aClass);
    NS_ASSERTION(xpcc && xpcc->GetJSContext(), "bad context");
    JS_AddNamedRoot(xpcc->GetJSContext(), &mJSObj,
                    "nsXPCWrappedJS::mJSObj");
}

nsXPCWrappedJS::~nsXPCWrappedJS()
{
    NS_PRECONDITION(0 == mRefCnt, "refcounting error");
    // Any destructors called after shutdown are just going to leak stuff.
    if(IsValid())
    {
        XPCJSRuntime* rt = nsXPConnect::GetRuntime();
        if(rt)
        {
            if(mRoot == this)
            {
                JSObject2WrappedJSMap* map = rt->GetWrappedJSMap();
                if(map)
                {
                    nsAutoLock lock(rt->GetMapLock());  
                    map->Remove(this);
                }
            }
            JS_RemoveRootRT(rt->GetJSRuntime(), &mJSObj);
        }
        NS_IF_RELEASE(mClass);
    }
    if(mNext)
        NS_DELETEXPCOM(mNext);  // cascaded delete
}

nsXPCWrappedJS*
nsXPCWrappedJS::Find(REFNSIID aIID)
{
    if(aIID.Equals(NS_GET_IID(nsISupports)))
        return mRoot;

    nsXPCWrappedJS* cur = mRoot;
    do
    {
        if(aIID.Equals(cur->GetIID()))
            return cur;

    } while(nsnull != (cur = cur->mNext));

    return nsnull;
}

NS_IMETHODIMP
nsXPCWrappedJS::GetInterfaceInfo(nsIInterfaceInfo** info)
{
    NS_ASSERTION(GetClass(), "wrapper without class");
    NS_ASSERTION(GetClass()->GetInterfaceInfo(), "wrapper class without interface");

    // Since failing to get this info will crash some platforms(!), we keep
    // mClass valid at shutdown time.

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
    if(!IsValid())
        return NS_ERROR_UNEXPECTED;
    return GetClass()->CallMethod(this, methodIndex, info, params);
}

NS_IMETHODIMP
nsXPCWrappedJS::GetIID(nsIID** iid)
{
    NS_PRECONDITION(iid, "bad param");

    *iid = (nsIID*) nsMemory::Clone(&(GetIID()), sizeof(nsIID));
    return *iid ? NS_OK : NS_ERROR_UNEXPECTED;
}

void 
nsXPCWrappedJS::SystemIsBeingShutDown(JSRuntime* rt)
{
    // XXX It turns out that it is better to leak here then to do any Releases 
    // and have them propagate into all sorts of mischief as the system is being
    // shutdown. This was learned the hard way :(    
    
    // mJSObj == nsnull is used to indicate that the wrapper is no longer valid
    // and that calls should fail without trying to use any of the 
    // xpconnect mechanisms. 'IsValid' is implemented by checking this pointer.

    // NOTE: that mClass is retained so that GetInterfaceInfo can continue to 
    // work (and avoid crashing some platforms).
    mJSObj = nsnull;
    
    // There is no reason to keep this root any longer. Since we've cleared
    // mJSObj our dtor will not remove the root later. So, we do it now. 
    JS_RemoveRootRT(rt, &mJSObj);

    // Notify other wrappers in the chain.
    if(mNext)
        mNext->SystemIsBeingShutDown(rt);
}

/***************************************************************************/

NS_IMETHODIMP
nsXPCWrappedJS::DebugDump(PRInt16 depth)
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
            nsMemory::Free(name);
        char * iid = GetClass()->GetIID().ToString();
        XPC_LOG_ALWAYS(("IID number is %s", iid));
        delete iid;
        XPC_LOG_ALWAYS(("nsXPCWrappedJSClass @ %x", mClass));

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

