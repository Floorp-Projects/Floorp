/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWrapperCacheInlines.h"

#include "js/Class.h"
#include "js/Proxy.h"
#include "mozilla/dom/DOMJSProxyHandler.h"
#include "mozilla/HoldDropJSObjects.h"
#include "nsCycleCollectionTraversalCallback.h"
#include "nsCycleCollector.h"

using namespace mozilla;
using namespace mozilla::dom;

#ifdef DEBUG
/* static */ bool
nsWrapperCache::HasJSObjectMovedOp(JSObject* aWrapper)
{
    return js::HasObjectMovedOp(aWrapper);
}
#endif

/* static */ void
nsWrapperCache::HoldJSObjects(void* aScriptObjectHolder,
                              nsScriptObjectTracer* aTracer)
{
  cyclecollector::HoldJSObjectsImpl(aScriptObjectHolder, aTracer);
}

void
nsWrapperCache::ReleaseWrapper(void* aScriptObjectHolder)
{
  if (PreservingWrapper()) {
    // PreserveWrapper puts new DOM bindings in the JS holders hash, but they
    // can also be in the DOM expando hash, so we need to try to remove them
    // from both here.
    JSObject* obj = GetWrapperPreserveColor();
    if (IsDOMBinding() && obj && js::IsProxy(obj)) {
      DOMProxyHandler::GetAndClearExpandoObject(obj);
    }
    SetPreservingWrapper(false);
    cyclecollector::DropJSObjectsImpl(aScriptObjectHolder);
  }
}

#ifdef DEBUG

class DebugWrapperTraversalCallback : public nsCycleCollectionTraversalCallback
{
public:
  explicit DebugWrapperTraversalCallback(JSObject* aWrapper)
    : mFound(false)
    , mWrapper(aWrapper)
  {
    mFlags = WANT_ALL_TRACES;
  }

  NS_IMETHOD_(void) DescribeRefCountedNode(nsrefcnt aRefCount,
                                           const char* aObjName)
  {
  }
  NS_IMETHOD_(void) DescribeGCedNode(bool aIsMarked,
                                     const char* aObjName,
                                     uint64_t aCompartmentAddress)
  {
  }

  NS_IMETHOD_(void) NoteJSObject(JSObject* aChild)
  {
    if (aChild == mWrapper) {
      mFound = true;
    }
  }
  NS_IMETHOD_(void) NoteJSScript(JSScript* aChild)
  {
  }
  NS_IMETHOD_(void) NoteXPCOMChild(nsISupports* aChild)
  {
  }
  NS_IMETHOD_(void) NoteNativeChild(void* aChild,
                                    nsCycleCollectionParticipant* aHelper)
  {
  }

  NS_IMETHOD_(void) NoteNextEdgeName(const char* aName)
  {
  }

  bool mFound;

private:
  JSObject* mWrapper;
};

static void
DebugWrapperTraceCallback(JS::GCCellPtr aPtr, const char* aName, void* aClosure)
{
  DebugWrapperTraversalCallback* callback =
    static_cast<DebugWrapperTraversalCallback*>(aClosure);
  if (aPtr.isObject()) {
    callback->NoteJSObject(aPtr.toObject());
  }
}

void
nsWrapperCache::CheckCCWrapperTraversal(void* aScriptObjectHolder,
                                        nsScriptObjectTracer* aTracer)
{
  JSObject* wrapper = GetWrapper();
  if (!wrapper) {
    return;
  }

  DebugWrapperTraversalCallback callback(wrapper);

  // The CC traversal machinery cannot trigger GC; however, the analysis cannot
  // see through the COM layer, so we use a suppression to help it.
  JS::AutoSuppressGCAnalysis suppress;

  aTracer->Traverse(aScriptObjectHolder, callback);
  MOZ_ASSERT(callback.mFound,
             "Cycle collection participant didn't traverse to preserved "
             "wrapper! This will probably crash.");

  callback.mFound = false;
  aTracer->Trace(aScriptObjectHolder,
                 TraceCallbackFunc(DebugWrapperTraceCallback), &callback);
  MOZ_ASSERT(callback.mFound,
             "Cycle collection participant didn't trace preserved wrapper! "
             "This will probably crash.");
}

#endif // DEBUG
