/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   John Bandhauer <jband@netscape.com> (original author)
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* Class that wraps JS objects to appear as XPCOM objects. */

#include "xpcprivate.h"

// NOTE: much of the fancy footwork is done in xpcstubs.cpp

NS_IMPL_CYCLE_COLLECTION_CLASS(nsXPCWrappedJS)

NS_IMETHODIMP
NS_CYCLE_COLLECTION_CLASSNAME(nsXPCWrappedJS)::Traverse
   (nsISupports *s, nsCycleCollectionTraversalCallback &cb)
{
    // REVIEW ME PLEASE: this is a very odd area and it's easy to get
    // it wrong. I'm not sure I got it right.
    //
    // We *might* have a stub that's not actually connected to an
    // nsXPCWrappedJS, so we begin by QI'ing over to a "real"
    // nsIXPConnectWrappedJS. Since that's a mostly-empty class, we
    // then downcast from there to the "true" nsXPCWrappedJS.
    //
    // Is this right? It's hard to know. It seems to work, but who
    // knows for how long. 

    nsresult rv;
    nsIXPConnectWrappedJS *base;
    nsXPCWrappedJS *tmp;
    {
        // Put the nsCOMPtr in a local scope, to avoid messing up the refcount
        // below.
        nsCOMPtr<nsIXPConnectWrappedJS> owner = do_QueryInterface(s, &rv);
        if (NS_FAILED(rv))
            return rv;

        base = owner.get();
        tmp = NS_STATIC_CAST(nsXPCWrappedJS*, base);
        NS_ASSERTION(tmp->mRefCnt.get() > 2,
                     "How can this be, no one else holds a strong ref?");
    }

    NS_ASSERTION(tmp->IsValid(), "How did we get here?");

    nsrefcnt refcnt = tmp->mRefCnt.get();
#ifdef DEBUG_CC
    char name[72];
    JS_snprintf(name, sizeof(name), "nsXPCWrappedJS (%s)",
                tmp->GetClass()->GetInterfaceName());
    cb.DescribeNode(refcnt, sizeof(nsXPCWrappedJS), name);
#else
    cb.DescribeNode(refcnt);
#endif

    // nsXPCWrappedJS keeps its own refcount artificially at or above 1, see the
    // comment above nsXPCWrappedJS::AddRef.
    cb.NoteXPCOMChild(base);

    if(refcnt > 1)
        // nsXPCWrappedJS roots its mJSObj when its refcount is > 1, see
        // the comment above nsXPCWrappedJS::AddRef.
        cb.NoteScriptChild(nsIProgrammingLanguage::JAVASCRIPT,
                           tmp->GetJSObject());

    nsXPCWrappedJS* root = tmp->GetRootWrapper();
    if(root == tmp)
        // The root wrapper keeps the aggregated native object alive.
        cb.NoteXPCOMChild(tmp->GetAggregatedNativeObject());
    else
        // Non-root wrappers keep their root alive.
        cb.NoteXPCOMChild(NS_STATIC_CAST(nsIXPConnectWrappedJS*, root));

    return NS_OK;
}

NS_IMETHODIMP
NS_CYCLE_COLLECTION_CLASSNAME(nsXPCWrappedJS)::Unlink(nsISupports *s)
{
    // NB: We might unlink our outgoing references in the future; for
    // now we do nothing. This is a harmless conservative behavior; it
    // just means that we rely on the cycle being broken by some of
    // the external XPCOM objects' unlink() methods, not our
    // own. Typically *any* unlinking will break the cycle.
    return NS_OK;
}

NS_IMETHODIMP
nsXPCWrappedJS::AggregatedQueryInterface(REFNSIID aIID, void** aInstancePtr)
{
    NS_ASSERTION(IsAggregatedToNative(), "bad AggregatedQueryInterface call");

    if(!IsValid())
        return NS_ERROR_UNEXPECTED;

    // Put this here rather that in DelegatedQueryInterface because it needs
    // to be in QueryInterface before the possible delegation to 'outer', but
    // we don't want to do this check twice in one call in the normal case:
    // once in QueryInterface and once in DelegatedQueryInterface.
    if(aIID.Equals(NS_GET_IID(nsIXPConnectWrappedJS)))
    {
        NS_ADDREF(this);
        *aInstancePtr = (void*) NS_STATIC_CAST(nsIXPConnectWrappedJS*,this);
        return NS_OK;
    }

    return mClass->DelegatedQueryInterface(this, aIID, aInstancePtr);
}

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

    if ( aIID.Equals(NS_GET_IID(nsCycleCollectionParticipant)) ) {
        *aInstancePtr = & NS_CYCLE_COLLECTION_NAME(nsXPCWrappedJS);
        return NS_OK;
    }

    if(aIID.Equals(NS_GET_IID(nsCycleCollectionISupports)))
    {
        NS_ADDREF(this);
        *aInstancePtr =
            NS_CYCLE_COLLECTION_CLASSNAME(nsXPCWrappedJS)::Upcast(this);
        return NS_OK;
    }

    // Always check for this first so that our 'outer' can get this interface
    // from us without recurring into a call to the outer's QI!
    if(aIID.Equals(NS_GET_IID(nsIXPConnectWrappedJS)))
    {
        NS_ADDREF(this);
        *aInstancePtr = (void*) NS_STATIC_CAST(nsIXPConnectWrappedJS*,this);
        return NS_OK;
    }

    nsISupports* outer = GetAggregatedNativeObject();
    if(outer)
        return outer->QueryInterface(aIID, aInstancePtr);

    // else...

    return mClass->DelegatedQueryInterface(this, aIID, aInstancePtr);
}


// Refcounting is now similar to that used in the chained (pre-flattening)
// wrappednative system.
//
// We are now holding an extra refcount for nsISupportsWeakReference support.
//
// Non-root wrappers remove themselves from the chain in their destructors.
// We root the JSObject as the refcount transitions from 1->2. And we unroot
// the JSObject when the refcount transitions from 2->1.
//
// When the transition from 2->1 is made and no one holds a weak ref to the
// (aggregated) object then we decrement the refcount again to 0 (and
// destruct) . However, if a weak ref is held at the 2->1 transition, then we
// leave the refcount at 1 to indicate that state. This leaves the JSObject
// no longer rooted by us and (as far as we know) subject to possible
// collection. Code in XPCJSRuntime watches for JS gc to happen and will do
// the final release on wrappers whose JSObjects get finalized. Note that
// even after tranistioning to this refcount-of-one state callers might do
// an addref and cause us to re-root the JSObject and continue on more normally.

nsrefcnt
nsXPCWrappedJS::AddRef(void)
{
    NS_PRECONDITION(mRoot, "bad root");
    nsrefcnt cnt = (nsrefcnt) PR_AtomicIncrement((PRInt32*)&mRefCnt);
    NS_LOG_ADDREF(this, cnt, "nsXPCWrappedJS", sizeof(*this));

    if(2 == cnt && IsValid())
    {
        XPCCallContext ccx(NATIVE_CALLER);
        if(ccx.IsValid()) {
#ifdef GC_MARK_DEBUG
            mGCRootName = JS_smprintf("nsXPCWrappedJS::mJSObj[%s,0x%p,0x%p]",
                                      GetClass()->GetInterfaceName(),
                                      this, mJSObj);
            JS_AddNamedRoot(ccx.GetJSContext(), &mJSObj, mGCRootName);
#else
            JS_AddNamedRoot(ccx.GetJSContext(), &mJSObj, "nsXPCWrappedJS::mJSObj");
#endif
        }
    }

    return cnt;
}

nsrefcnt
nsXPCWrappedJS::Release(void)
{
    NS_PRECONDITION(mRoot, "bad root");
    NS_PRECONDITION(0 != mRefCnt, "dup release");

#ifdef DEBUG_jband
    NS_ASSERTION(IsValid(), "post xpconnect shutdown call of nsXPCWrappedJS::Release");
#endif

do_decrement:

    nsrefcnt cnt = (nsrefcnt) PR_AtomicDecrement((PRInt32*)&mRefCnt);
    NS_LOG_RELEASE(this, cnt, "nsXPCWrappedJS");

    if(0 == cnt)
    {
        NS_DELETEXPCOM(this);   // also unlinks us from chain
        return 0;
    }
    if(1 == cnt)
    {
        if(IsValid())
        {
            XPCJSRuntime* rt = mClass->GetRuntime();
            if(rt) {
                JS_RemoveRootRT(rt->GetJSRuntime(), &mJSObj);
#ifdef GC_MARK_DEBUG
                JS_smprintf_free(mGCRootName);
                mGCRootName = nsnull;
#endif
            }
        }

        // If we are not the root wrapper or if we are not being used from a
        // weak reference, then this extra ref is not needed and we can let
        // ourself be deleted.
        // Note: HasWeakReferences() could only return true for the root.
        if(!HasWeakReferences())
            goto do_decrement;
    }
    return cnt;
}

NS_IMETHODIMP
nsXPCWrappedJS::GetWeakReference(nsIWeakReference** aInstancePtr)
{
    if(mRoot != this)
        return mRoot->GetWeakReference(aInstancePtr);

    return nsSupportsWeakReference::GetWeakReference(aInstancePtr);
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
nsresult
nsXPCWrappedJS::GetNewOrUsed(XPCCallContext& ccx,
                             JSObject* aJSObj,
                             REFNSIID aIID,
                             nsISupports* aOuter,
                             nsXPCWrappedJS** wrapperResult)
{
    JSObject2WrappedJSMap* map;
    JSObject* rootJSObj;
    nsXPCWrappedJS* root;
    nsXPCWrappedJS* wrapper = nsnull;
    nsXPCWrappedJSClass* clazz = nsnull;
    XPCJSRuntime* rt = ccx.GetRuntime();

    map = rt->GetWrappedJSMap();
    if(!map)
    {
        NS_ASSERTION(map,"bad map");
        return NS_ERROR_FAILURE;
    }

    nsXPCWrappedJSClass::GetNewOrUsed(ccx, aIID, &clazz);
    if(!clazz)
        return NS_ERROR_FAILURE;
    // from here on we need to return through 'return_wrapper'

    // always find the root JSObject
    rootJSObj = clazz->GetRootJSObject(ccx, aJSObj);
    if(!rootJSObj)
        goto return_wrapper;

    // look for the root wrapper
    {   // scoped lock
        XPCAutoLock lock(rt->GetMapLock());
        root = map->Find(rootJSObj);
    }
    if(root)
    {
        if((nsnull != (wrapper = root->Find(aIID))) ||
           (nsnull != (wrapper = root->FindInherited(aIID))))
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
            wrapper = root = new nsXPCWrappedJS(ccx, aJSObj, clazz, nsnull,
                                                aOuter);
            if(root)
            {   // scoped lock
#if DEBUG_xpc_leaks
                printf("Created nsXPCWrappedJS %p, JSObject is %p\n",
                       (void*)wrapper, (void*)aJSObj);
#endif
                XPCAutoLock lock(rt->GetMapLock());
                map->Add(root);
            }
            goto return_wrapper;
        }
        else
        {
            // just a root wrapper
            nsXPCWrappedJSClass* rootClazz = nsnull;
            nsXPCWrappedJSClass::GetNewOrUsed(ccx, NS_GET_IID(nsISupports),
                                              &rootClazz);
            if(!rootClazz)
                goto return_wrapper;

            root = new nsXPCWrappedJS(ccx, rootJSObj, rootClazz, nsnull, aOuter);
            NS_RELEASE(rootClazz);

            if(!root)
                goto return_wrapper;
            {   // scoped lock
#if DEBUG_xpc_leaks
                printf("Created nsXPCWrappedJS %p, JSObject is %p\n",
                       (void*)root, (void*)rootJSObj);
#endif
                XPCAutoLock lock(rt->GetMapLock());
                map->Add(root);
            }
        }
    }

    // at this point we have a root and may need to build the specific wrapper
    NS_ASSERTION(root,"bad root");
    NS_ASSERTION(clazz,"bad clazz");

    if(!wrapper)
    {
        wrapper = new nsXPCWrappedJS(ccx, aJSObj, clazz, root, aOuter);
        if(!wrapper)
            goto return_wrapper;
#if DEBUG_xpc_leaks
        printf("Created nsXPCWrappedJS %p, JSObject is %p\n",
               (void*)wrapper, (void*)aJSObj);
#endif
    }

    wrapper->mNext = root->mNext;
    root->mNext = wrapper;

return_wrapper:
    if(clazz)
        NS_RELEASE(clazz);

    if(!wrapper)
        return NS_ERROR_FAILURE;

    *wrapperResult = wrapper;
    return NS_OK;
}

nsXPCWrappedJS::nsXPCWrappedJS(XPCCallContext& ccx,
                               JSObject* aJSObj,
                               nsXPCWrappedJSClass* aClass,
                               nsXPCWrappedJS* root,
                               nsISupports* aOuter)
    : mJSObj(aJSObj),
      mClass(aClass),
      mRoot(root ? root : this),
      mNext(nsnull),
      mOuter(root ? nsnull : aOuter)
#ifdef GC_MARK_DEBUG
      , mGCRootName(nsnull)
#endif
{
#ifdef DEBUG_stats_jband
    static int count = 0;
    static const int interval = 10;
    if(0 == (++count % interval))
        printf("//////// %d instances of nsXPCWrappedJS created\n", count);
#endif

    InitStub(GetClass()->GetIID());

    // intensionally do double addref - see Release().
    NS_ADDREF_THIS();
    NS_ADDREF_THIS();
    NS_ADDREF(aClass);
    NS_IF_ADDREF(mOuter);

    if(mRoot != this)
        NS_ADDREF(mRoot);

}

nsXPCWrappedJS::~nsXPCWrappedJS()
{
    NS_PRECONDITION(0 == mRefCnt, "refcounting error");

    XPCJSRuntime* rt = nsXPConnect::GetRuntime();

    if(mRoot != this)
    {
        // unlink this wrapper
        nsXPCWrappedJS* cur = mRoot;
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
        NS_ASSERTION(!mNext, "root wrapper with non-empty chain being deleted");

        // Let the nsWeakReference object (if present) know of our demise.
        ClearWeakReferences();

        // remove this root wrapper from the map
        if(rt)
        {
            JSObject2WrappedJSMap* map = rt->GetWrappedJSMap();
            if(map)
            {
                XPCAutoLock lock(rt->GetMapLock());
                map->Remove(this);
            }
        }
    }

    if(IsValid())
    {
        NS_IF_RELEASE(mClass);
        if (mOuter)
        {
            if (rt && rt->GetThreadRunningGC())
            {
                rt->DeferredRelease(mOuter);
                mOuter = nsnull;
            }
            else
            {
                NS_RELEASE(mOuter);
            }
        }
    }
}

nsXPCWrappedJS*
nsXPCWrappedJS::Find(REFNSIID aIID)
{
    if(aIID.Equals(NS_GET_IID(nsISupports)))
        return mRoot;

    for(nsXPCWrappedJS* cur = mRoot; cur; cur = cur->mNext)
    {
        if(aIID.Equals(cur->GetIID()))
            return cur;
    }

    return nsnull;
}

// check if asking for an interface that some wrapper in the chain inherits from
nsXPCWrappedJS*
nsXPCWrappedJS::FindInherited(REFNSIID aIID)
{
    NS_ASSERTION(!aIID.Equals(NS_GET_IID(nsISupports)), "bad call sequence");

    for(nsXPCWrappedJS* cur = mRoot; cur; cur = cur->mNext)
    {
        PRBool found;
        if(NS_SUCCEEDED(cur->GetClass()->GetInterfaceInfo()->
                                HasAncestor(&aIID, &found)) && found)
            return cur;
    }

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
                           const XPTMethodDescriptor* info,
                           nsXPTCMiniVariant* params)
{
    if(!IsValid())
        return NS_ERROR_UNEXPECTED;
    return GetClass()->CallMethod(this, methodIndex, info, params);
}

NS_IMETHODIMP
nsXPCWrappedJS::GetInterfaceIID(nsIID** iid)
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

/* readonly attribute nsISimpleEnumerator enumerator; */
NS_IMETHODIMP 
nsXPCWrappedJS::GetEnumerator(nsISimpleEnumerator * *aEnumerate)
{
    XPCCallContext ccx(NATIVE_CALLER);
    if(!ccx.IsValid())
        return NS_ERROR_UNEXPECTED;

    return nsXPCWrappedJSClass::BuildPropertyEnumerator(ccx, mJSObj, aEnumerate);
}

/* nsIVariant getProperty (in AString name); */
NS_IMETHODIMP 
nsXPCWrappedJS::GetProperty(const nsAString & name, nsIVariant **_retval)
{
    XPCCallContext ccx(NATIVE_CALLER);
    if(!ccx.IsValid())
        return NS_ERROR_UNEXPECTED;

    JSString* jsstr = XPCStringConvert::ReadableToJSString(ccx, name);
    if(!jsstr)
        return NS_ERROR_OUT_OF_MEMORY;

    return nsXPCWrappedJSClass::
        GetNamedPropertyAsVariant(ccx, mJSObj, STRING_TO_JSVAL(jsstr), _retval);
}

/***************************************************************************/

NS_IMETHODIMP
nsXPCWrappedJS::DebugDump(PRInt16 depth)
{
#ifdef DEBUG
    XPC_LOG_ALWAYS(("nsXPCWrappedJS @ %x with mRefCnt = %d", this, mRefCnt.get()));
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
        XPC_LOG_ALWAYS(("IID number is %s", iid ? iid : "invalid"));
        if(iid)
            PR_Free(iid);
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
