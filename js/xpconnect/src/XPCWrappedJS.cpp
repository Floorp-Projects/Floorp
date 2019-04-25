/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Class that wraps JS objects to appear as XPCOM objects. */

#include "xpcprivate.h"
#include "mozilla/DeferredFinalize.h"
#include "mozilla/Sprintf.h"
#include "mozilla/jsipc/CrossProcessObjectWrappers.h"
#include "nsCCUncollectableMarker.h"
#include "nsContentUtils.h"
#include "nsThreadUtils.h"

using namespace mozilla;

// NOTE: much of the fancy footwork is done in xpcstubs.cpp

// nsXPCWrappedJS lifetime.
//
// An nsXPCWrappedJS is either rooting its JS object or is subject to
// finalization. The subject-to-finalization state lets wrappers support
// nsSupportsWeakReference in the case where the underlying JS object
// is strongly owned, but the wrapper itself is only weakly owned.
//
// A wrapper is rooting its JS object whenever its refcount is greater than 1.
// In this state, root wrappers are always added to the cycle collector graph.
// The wrapper keeps around an extra refcount, added in the constructor, to
// support the possibility of an eventual transition to the
// subject-to-finalization state. This extra refcount is ignored by the cycle
// collector, which traverses the "self" edge for this refcount.
//
// When the refcount of a rooting wrapper drops to 1, if there is no weak
// reference to the wrapper (which can only happen for the root wrapper), it is
// immediately Destroy()'d. Otherwise, it becomes subject to finalization.
//
// When a wrapper is subject to finalization, the wrapper has a refcount of 1.
// It is now owned exclusively by its JS object. Either a weak reference will be
// turned into a strong ref which will bring its refcount up to 2 and change the
// wrapper back to the rooting state, or it will stay alive until the JS object
// dies. If the JS object dies, then when
// JSObject2WrappedJSMap::UpdateWeakPointersAfterGC is called (via the JS
// engine's weak pointer zone or compartment callbacks) it will find the wrapper
// and call Release() on it, destroying the wrapper. Otherwise, the wrapper will
// stay alive, even if it no longer has a weak reference to it.
//
// When the wrapper is subject to finalization, it is kept alive by an implicit
// reference from the JS object which is invisible to the cycle collector, so
// the cycle collector does not traverse any children of wrappers that are
// subject to finalization. This will result in a leak if a wrapper in the
// non-rooting state has an aggregated native that keeps alive the wrapper's JS
// object.  See bug 947049.

// If traversing wrappedJS wouldn't release it, nor cause any other objects to
// be added to the graph, there is no need to add it to the graph at all.
bool nsXPCWrappedJS::CanSkip() {
  if (!nsCCUncollectableMarker::sGeneration) {
    return false;
  }

  if (IsSubjectToFinalization()) {
    return true;
  }

  // If this wrapper holds a gray object, need to trace it.
  JSObject* obj = GetJSObjectPreserveColor();
  if (obj && JS::ObjectIsMarkedGray(obj)) {
    return false;
  }

  // For non-root wrappers, check if the root wrapper will be
  // added to the CC graph.
  if (!IsRootWrapper()) {
    // mRoot points to null after unlinking.
    NS_ENSURE_TRUE(mRoot, false);
    return mRoot->CanSkip();
  }

  // For the root wrapper, check if there is an aggregated
  // native object that will be added to the CC graph.
  if (!IsAggregatedToNative()) {
    return true;
  }

  nsISupports* agg = GetAggregatedNativeObject();
  nsXPCOMCycleCollectionParticipant* cp = nullptr;
  CallQueryInterface(agg, &cp);
  nsISupports* canonical = nullptr;
  agg->QueryInterface(NS_GET_IID(nsCycleCollectionISupports),
                      reinterpret_cast<void**>(&canonical));
  return cp && canonical && cp->CanSkipThis(canonical);
}

NS_IMETHODIMP
NS_CYCLE_COLLECTION_CLASSNAME(nsXPCWrappedJS)::TraverseNative(
    void* p, nsCycleCollectionTraversalCallback& cb) {
  nsISupports* s = static_cast<nsISupports*>(p);
  MOZ_ASSERT(CheckForRightISupports(s),
             "not the nsISupports pointer we expect");
  nsXPCWrappedJS* tmp = Downcast(s);

  nsrefcnt refcnt = tmp->mRefCnt.get();
  if (cb.WantDebugInfo()) {
    char name[72];
    SprintfLiteral(name, "nsXPCWrappedJS (%s)", tmp->mInfo->Name());
    cb.DescribeRefCountedNode(refcnt, name);
  } else {
    NS_IMPL_CYCLE_COLLECTION_DESCRIBE(nsXPCWrappedJS, refcnt)
  }

  // A wrapper that is subject to finalization will only die when its JS object
  // dies.
  if (tmp->IsSubjectToFinalization()) {
    return NS_OK;
  }

  // Don't let the extra reference for nsSupportsWeakReference keep a wrapper
  // that is not subject to finalization alive.
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "self");
  cb.NoteXPCOMChild(s);

  if (tmp->IsValid()) {
    MOZ_ASSERT(refcnt > 1);
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mJSObj");
    cb.NoteJSChild(JS::GCCellPtr(tmp->GetJSObjectPreserveColor()));
  }

  if (tmp->IsRootWrapper()) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "aggregated native");
    cb.NoteXPCOMChild(tmp->GetAggregatedNativeObject());
  } else {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "root");
    cb.NoteXPCOMChild(ToSupports(tmp->GetRootWrapper()));
  }

  return NS_OK;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsXPCWrappedJS)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsXPCWrappedJS)
  tmp->Unlink();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

// XPCJSContext keeps a table of WJS, so we can remove them from
// the purple buffer in between CCs.
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_BEGIN(nsXPCWrappedJS)
  return true;
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_BEGIN(nsXPCWrappedJS)
  return tmp->CanSkip();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_BEGIN(nsXPCWrappedJS)
  return tmp->CanSkip();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_END

NS_IMETHODIMP
nsXPCWrappedJS::AggregatedQueryInterface(REFNSIID aIID, void** aInstancePtr) {
  MOZ_ASSERT(IsAggregatedToNative(), "bad AggregatedQueryInterface call");
  *aInstancePtr = nullptr;

  if (!IsValid()) {
    return NS_ERROR_UNEXPECTED;
  }

  // Put this here rather that in DelegatedQueryInterface because it needs
  // to be in QueryInterface before the possible delegation to 'outer', but
  // we don't want to do this check twice in one call in the normal case:
  // once in QueryInterface and once in DelegatedQueryInterface.
  if (aIID.Equals(NS_GET_IID(nsIXPConnectWrappedJS))) {
    NS_ADDREF(this);
    *aInstancePtr = (void*)static_cast<nsIXPConnectWrappedJS*>(this);
    return NS_OK;
  }

  return DelegatedQueryInterface(aIID, aInstancePtr);
}

NS_IMETHODIMP
nsXPCWrappedJS::QueryInterface(REFNSIID aIID, void** aInstancePtr) {
  if (nullptr == aInstancePtr) {
    MOZ_ASSERT(false, "null pointer");
    return NS_ERROR_NULL_POINTER;
  }

  *aInstancePtr = nullptr;

  if (aIID.Equals(NS_GET_IID(nsXPCOMCycleCollectionParticipant))) {
    *aInstancePtr = NS_CYCLE_COLLECTION_PARTICIPANT(nsXPCWrappedJS);
    return NS_OK;
  }

  if (aIID.Equals(NS_GET_IID(nsCycleCollectionISupports))) {
    *aInstancePtr = NS_CYCLE_COLLECTION_CLASSNAME(nsXPCWrappedJS)::Upcast(this);
    return NS_OK;
  }

  if (!IsValid()) {
    return NS_ERROR_UNEXPECTED;
  }

  if (aIID.Equals(NS_GET_IID(nsIXPConnectWrappedJSUnmarkGray))) {
    *aInstancePtr = nullptr;

    mJSObj.exposeToActiveJS();

    // Just return some error value since one isn't supposed to use
    // nsIXPConnectWrappedJSUnmarkGray objects for anything.
    return NS_ERROR_FAILURE;
  }

  // Always check for this first so that our 'outer' can get this interface
  // from us without recurring into a call to the outer's QI!
  if (aIID.Equals(NS_GET_IID(nsIXPConnectWrappedJS))) {
    NS_ADDREF(this);
    *aInstancePtr = (void*)static_cast<nsIXPConnectWrappedJS*>(this);
    return NS_OK;
  }

  nsISupports* outer = GetAggregatedNativeObject();
  if (outer) {
    return outer->QueryInterface(aIID, aInstancePtr);
  }

  // else...

  return DelegatedQueryInterface(aIID, aInstancePtr);
}

// For a description of nsXPCWrappedJS lifetime and reference counting, see
// the comment at the top of this file.

MozExternalRefCountType nsXPCWrappedJS::AddRef(void) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread(),
                     "nsXPCWrappedJS::AddRef called off main thread");

  MOZ_ASSERT(int32_t(mRefCnt) >= 0, "illegal refcnt");
  nsISupports* base =
      NS_CYCLE_COLLECTION_CLASSNAME(nsXPCWrappedJS)::Upcast(this);
  nsrefcnt cnt = mRefCnt.incr(base);
  NS_LOG_ADDREF(this, cnt, "nsXPCWrappedJS", sizeof(*this));

  if (2 == cnt && IsValid()) {
    GetJSObject();  // Unmark gray JSObject.
    XPCJSRuntime::Get()->AddWrappedJSRoot(this);
  }

  return cnt;
}

MozExternalRefCountType nsXPCWrappedJS::Release(void) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread(),
                     "nsXPCWrappedJS::Release called off main thread");
  MOZ_ASSERT(int32_t(mRefCnt) > 0, "dup release");
  NS_ASSERT_OWNINGTHREAD(nsXPCWrappedJS);

  bool shouldDelete = false;
  nsISupports* base =
      NS_CYCLE_COLLECTION_CLASSNAME(nsXPCWrappedJS)::Upcast(this);
  nsrefcnt cnt = mRefCnt.decr(base, &shouldDelete);
  NS_LOG_RELEASE(this, cnt, "nsXPCWrappedJS");

  if (0 == cnt) {
    if (MOZ_UNLIKELY(shouldDelete)) {
      mRefCnt.stabilizeForDeletion();
      DeleteCycleCollectable();
    } else {
      mRefCnt.incr(base);
      Destroy();
      mRefCnt.decr(base);
    }
  } else if (1 == cnt) {
    if (IsValid()) {
      RemoveFromRootSet();
    }

    // If we are not a root wrapper being used from a weak reference,
    // then the extra ref is not needed and we can let outselves be
    // deleted.
    if (!HasWeakReferences()) {
      return Release();
    }

    MOZ_ASSERT(IsRootWrapper(),
               "Only root wrappers should have weak references");
  }
  return cnt;
}

NS_IMETHODIMP_(void)
nsXPCWrappedJS::DeleteCycleCollectable(void) { delete this; }

void nsXPCWrappedJS::TraceJS(JSTracer* trc) {
  MOZ_ASSERT(mRefCnt >= 2 && IsValid(), "must be strongly referenced");
  JS::TraceEdge(trc, &mJSObj, "nsXPCWrappedJS::mJSObj");
}

NS_IMETHODIMP
nsXPCWrappedJS::GetWeakReference(nsIWeakReference** aInstancePtr) {
  if (!IsRootWrapper()) {
    return mRoot->GetWeakReference(aInstancePtr);
  }

  return nsSupportsWeakReference::GetWeakReference(aInstancePtr);
}

JSObject* nsXPCWrappedJS::GetJSObject() { return mJSObj; }

JSObject* nsXPCWrappedJS::GetJSObjectGlobal() {
  JSObject* obj = mJSObj;
  if (js::IsCrossCompartmentWrapper(obj)) {
    JS::Compartment* comp = js::GetObjectCompartment(obj);
    return js::GetFirstGlobalInCompartment(comp);
  }
  return JS::GetNonCCWObjectGlobal(obj);
}

// static
nsresult nsXPCWrappedJS::GetNewOrUsed(JSContext* cx, JS::HandleObject jsObj,
                                      REFNSIID aIID,
                                      nsXPCWrappedJS** wrapperResult) {
  // Do a release-mode assert against accessing nsXPCWrappedJS off-main-thread.
  MOZ_RELEASE_ASSERT(NS_IsMainThread(),
                     "nsXPCWrappedJS::GetNewOrUsed called off main thread");

  MOZ_RELEASE_ASSERT(js::GetContextCompartment(cx) ==
                     js::GetObjectCompartment(jsObj));

  const nsXPTInterfaceInfo* info = GetInterfaceInfo(aIID);
  if (!info) {
    return NS_ERROR_FAILURE;
  }

  JS::RootedObject rootJSObj(cx, GetRootJSObject(cx, jsObj));
  if (!rootJSObj) {
    return NS_ERROR_FAILURE;
  }

  xpc::CompartmentPrivate* rootComp = xpc::CompartmentPrivate::Get(rootJSObj);
  MOZ_ASSERT(rootComp);

  // Find any existing wrapper.
  RefPtr<nsXPCWrappedJS> root = rootComp->GetWrappedJSMap()->Find(rootJSObj);
  MOZ_ASSERT_IF(root, !nsXPConnect::GetRuntimeInstance()
                           ->GetMultiCompartmentWrappedJSMap()
                           ->Find(rootJSObj));
  if (!root) {
    root = nsXPConnect::GetRuntimeInstance()
               ->GetMultiCompartmentWrappedJSMap()
               ->Find(rootJSObj);
  }

  nsresult rv = NS_ERROR_FAILURE;
  if (root) {
    RefPtr<nsXPCWrappedJS> wrapper = root->FindOrFindInherited(aIID);
    if (wrapper) {
      wrapper.forget(wrapperResult);
      return NS_OK;
    }
  } else if (rootJSObj != jsObj) {
    // Make a new root wrapper, because there is no existing
    // root wrapper, and the wrapper we are trying to make isn't
    // a root.
    const nsXPTInterfaceInfo* rootInfo =
        GetInterfaceInfo(NS_GET_IID(nsISupports));
    if (!rootInfo) {
      return NS_ERROR_FAILURE;
    }

    root = new nsXPCWrappedJS(cx, rootJSObj, rootInfo, nullptr, &rv);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  RefPtr<nsXPCWrappedJS> wrapper =
      new nsXPCWrappedJS(cx, jsObj, info, root, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }
  wrapper.forget(wrapperResult);
  return NS_OK;
}

nsXPCWrappedJS::nsXPCWrappedJS(JSContext* cx, JSObject* aJSObj,
                               const nsXPTInterfaceInfo* aInfo,
                               nsXPCWrappedJS* root, nsresult* rv)
    : mJSObj(aJSObj), mInfo(aInfo), mRoot(root ? root : this), mNext(nullptr) {
  *rv = InitStub(mInfo->IID());
  // Continue even in the failure case, so that our refcounting/Destroy
  // behavior works correctly.

  // There is an extra AddRef to support weak references to wrappers
  // that are subject to finalization. See the top of the file for more
  // details.
  NS_ADDREF_THIS();

  if (IsRootWrapper()) {
    MOZ_ASSERT(!IsMultiCompartment(), "mNext is always nullptr here");
    if (!xpc::CompartmentPrivate::Get(mJSObj)->GetWrappedJSMap()->Add(cx,
                                                                      this)) {
      *rv = NS_ERROR_OUT_OF_MEMORY;
    }
  } else {
    NS_ADDREF(mRoot);
    mNext = mRoot->mNext;
    mRoot->mNext = this;

    // We always start wrappers in the per-compartment table. If adding
    // this wrapper to the chain causes it to cross compartments, we need
    // to migrate the chain to the global table on the XPCJSContext.
    if (mRoot->IsMultiCompartment()) {
      xpc::CompartmentPrivate::Get(mRoot->mJSObj)
          ->GetWrappedJSMap()
          ->Remove(mRoot);
      auto destMap =
          nsXPConnect::GetRuntimeInstance()->GetMultiCompartmentWrappedJSMap();
      if (!destMap->Add(cx, mRoot)) {
        *rv = NS_ERROR_OUT_OF_MEMORY;
      }
    }
  }
}

nsXPCWrappedJS::~nsXPCWrappedJS() { Destroy(); }

void XPCJSRuntime::RemoveWrappedJS(nsXPCWrappedJS* wrapper) {
  AssertInvalidWrappedJSNotInTable(wrapper);
  if (!wrapper->IsValid()) {
    return;
  }

  // It is possible for the same JS XPCOM implementation object to be wrapped
  // with a different interface in multiple JS::Compartments. In this case, the
  // wrapper chain will contain references to multiple compartments. While we
  // always store single-compartment chains in the per-compartment wrapped-js
  // table, chains in the multi-compartment wrapped-js table may contain
  // single-compartment chains, if they have ever contained a wrapper in a
  // different compartment. Since removal requires a lookup anyway, we just do
  // the remove on both tables unconditionally.
  MOZ_ASSERT_IF(
      wrapper->IsMultiCompartment(),
      !xpc::CompartmentPrivate::Get(wrapper->GetJSObjectPreserveColor())
           ->GetWrappedJSMap()
           ->HasWrapper(wrapper));
  GetMultiCompartmentWrappedJSMap()->Remove(wrapper);
  xpc::CompartmentPrivate::Get(wrapper->GetJSObjectPreserveColor())
      ->GetWrappedJSMap()
      ->Remove(wrapper);
}

#ifdef DEBUG
static JS::CompartmentIterResult NotHasWrapperAssertionCallback(
    JSContext* cx, void* data, JS::Compartment* comp) {
  auto wrapper = static_cast<nsXPCWrappedJS*>(data);
  auto xpcComp = xpc::CompartmentPrivate::Get(comp);
  MOZ_ASSERT_IF(xpcComp, !xpcComp->GetWrappedJSMap()->HasWrapper(wrapper));
  return JS::CompartmentIterResult::KeepGoing;
}
#endif

void XPCJSRuntime::AssertInvalidWrappedJSNotInTable(
    nsXPCWrappedJS* wrapper) const {
#ifdef DEBUG
  if (!wrapper->IsValid()) {
    MOZ_ASSERT(!GetMultiCompartmentWrappedJSMap()->HasWrapper(wrapper));
    if (!mGCIsRunning) {
      JSContext* cx = XPCJSContext::Get()->Context();
      JS_IterateCompartments(cx, wrapper, NotHasWrapperAssertionCallback);
    }
  }
#endif
}

void nsXPCWrappedJS::Destroy() {
  MOZ_ASSERT(1 == int32_t(mRefCnt), "should be stabilized for deletion");

  if (IsRootWrapper()) {
    nsXPConnect::GetRuntimeInstance()->RemoveWrappedJS(this);
  }
  Unlink();
}

void nsXPCWrappedJS::Unlink() {
  nsXPConnect::GetRuntimeInstance()->AssertInvalidWrappedJSNotInTable(this);

  if (IsValid()) {
    XPCJSRuntime* rt = nsXPConnect::GetRuntimeInstance();
    if (rt) {
      if (IsRootWrapper()) {
        rt->RemoveWrappedJS(this);
      }

      if (mRefCnt > 1) {
        RemoveFromRootSet();
      }
    }

    mJSObj = nullptr;
  }

  if (IsRootWrapper()) {
    ClearWeakReferences();
  } else if (mRoot) {
    // unlink this wrapper
    nsXPCWrappedJS* cur = mRoot;
    while (1) {
      if (cur->mNext == this) {
        cur->mNext = mNext;
        break;
      }
      cur = cur->mNext;
      MOZ_ASSERT(cur, "failed to find wrapper in its own chain");
    }

    // Note: unlinking this wrapper may have changed us from a multi-
    // compartment wrapper chain to a single-compartment wrapper chain. We
    // leave the wrapper in the multi-compartment table as it is likely to
    // need to be multi-compartment again in the future and, moreover, we
    // cannot get a JSContext here.

    // let the root go
    NS_RELEASE(mRoot);
  }

  if (mOuter) {
    XPCJSRuntime* rt = nsXPConnect::GetRuntimeInstance();
    if (rt->GCIsRunning()) {
      DeferredFinalize(mOuter.forget().take());
    } else {
      mOuter = nullptr;
    }
  }
}

bool nsXPCWrappedJS::IsMultiCompartment() const {
  MOZ_ASSERT(IsRootWrapper());
  JS::Compartment* compartment = Compartment();
  nsXPCWrappedJS* next = mNext;
  while (next) {
    if (next->Compartment() != compartment) {
      return true;
    }
    next = next->mNext;
  }
  return false;
}

nsXPCWrappedJS* nsXPCWrappedJS::Find(REFNSIID aIID) {
  if (aIID.Equals(NS_GET_IID(nsISupports))) {
    return mRoot;
  }

  for (nsXPCWrappedJS* cur = mRoot; cur; cur = cur->mNext) {
    if (aIID.Equals(cur->GetIID())) {
      return cur;
    }
  }

  return nullptr;
}

// check if asking for an interface that some wrapper in the chain inherits from
nsXPCWrappedJS* nsXPCWrappedJS::FindInherited(REFNSIID aIID) {
  MOZ_ASSERT(!aIID.Equals(NS_GET_IID(nsISupports)), "bad call sequence");

  for (nsXPCWrappedJS* cur = mRoot; cur; cur = cur->mNext) {
    if (cur->mInfo->HasAncestor(aIID)) {
      return cur;
    }
  }

  return nullptr;
}

NS_IMETHODIMP
nsXPCWrappedJS::GetInterfaceIID(nsIID** iid) {
  MOZ_ASSERT(iid, "bad param");

  *iid = GetIID().Clone();
  return NS_OK;
}

void nsXPCWrappedJS::SystemIsBeingShutDown() {
  // XXX It turns out that it is better to leak here then to do any Releases
  // and have them propagate into all sorts of mischief as the system is being
  // shutdown. This was learned the hard way :(

  // mJSObj == nullptr is used to indicate that the wrapper is no longer valid
  // and that calls should fail without trying to use any of the
  // xpconnect mechanisms. 'IsValid' is implemented by checking this pointer.

  // Clear the contents of the pointer using unsafeGet() to avoid
  // triggering post barriers in shutdown, as this will access the chunk
  // containing mJSObj, which may have been freed at this point. This is safe
  // if we are not currently running an incremental GC.
  MOZ_ASSERT(!IsIncrementalGCInProgress(xpc_GetSafeJSContext()));
  *mJSObj.unsafeGet() = nullptr;

  // Notify other wrappers in the chain.
  if (mNext) {
    mNext->SystemIsBeingShutDown();
  }
}

size_t nsXPCWrappedJS::SizeOfIncludingThis(
    mozilla::MallocSizeOf mallocSizeOf) const {
  // mJSObject is a JS pointer, so don't measure the object.  mInfo is
  // not dynamically allocated. mRoot is not measured because it is
  // either |this| or we have already measured it. mOuter is rare and
  // probably not uniquely owned by this.
  size_t n = mallocSizeOf(this);
  n += nsAutoXPTCStub::SizeOfExcludingThis(mallocSizeOf);

  // Wrappers form a linked list via the mNext field, so include them all
  // in the measurement. Only root wrappers are stored in the map, so
  // everything will be measured exactly once.
  if (mNext) {
    n += mNext->SizeOfIncludingThis(mallocSizeOf);
  }

  return n;
}

/***************************************************************************/

NS_IMETHODIMP
nsXPCWrappedJS::DebugDump(int16_t depth) {
#ifdef DEBUG
  XPC_LOG_ALWAYS(
      ("nsXPCWrappedJS @ %p with mRefCnt = %" PRIuPTR, this, mRefCnt.get()));
  XPC_LOG_INDENT();

  XPC_LOG_ALWAYS(("%s wrapper around JSObject @ %p",
                  IsRootWrapper() ? "ROOT" : "non-root", mJSObj.get()));
  const char* name = mInfo->Name();
  XPC_LOG_ALWAYS(("interface name is %s", name));
  char* iid = mInfo->IID().ToString();
  XPC_LOG_ALWAYS(("IID number is %s", iid ? iid : "invalid"));
  if (iid) {
    free(iid);
  }
  XPC_LOG_ALWAYS(("nsXPTInterfaceInfo @ %p", mInfo));

  if (!IsRootWrapper()) {
    XPC_LOG_OUTDENT();
  }
  if (mNext) {
    if (IsRootWrapper()) {
      XPC_LOG_ALWAYS(("Additional wrappers for this object..."));
      XPC_LOG_INDENT();
    }
    mNext->DebugDump(depth);
    if (IsRootWrapper()) {
      XPC_LOG_OUTDENT();
    }
  }
  if (IsRootWrapper()) {
    XPC_LOG_OUTDENT();
  }
#endif
  return NS_OK;
}
