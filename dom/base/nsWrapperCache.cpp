/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWrapperCacheInlines.h"

#include "jsfriendapi.h"
#include "js/Class.h"
#include "js/Proxy.h"
#include "mozilla/CycleCollectedJSRuntime.h"
#include "mozilla/HoldDropJSObjects.h"
#include "nsCycleCollectionTraversalCallback.h"
#include "nsCycleCollector.h"

using namespace mozilla;
using namespace mozilla::dom;

#ifdef DEBUG
/* static */
bool nsWrapperCache::HasJSObjectMovedOp(JSObject* aWrapper) {
  return js::HasObjectMovedOp(aWrapper);
}
#endif

void nsWrapperCache::HoldJSObjects(void* aScriptObjectHolder,
                                   nsScriptObjectTracer* aTracer,
                                   JS::Zone* aWrapperZone) {
  cyclecollector::HoldJSObjectsImpl(aScriptObjectHolder, aTracer, aWrapperZone);
  if (mWrapper && !JS::ObjectIsTenured(mWrapper)) {
    JS::HeapObjectPostWriteBarrier(&mWrapper, nullptr, mWrapper);
  }
}

static inline bool IsNurseryWrapper(JSObject* aWrapper) {
  return aWrapper && !JS::ObjectIsTenured(aWrapper);
}

void nsWrapperCache::SetWrapperJSObject(JSObject* aNewWrapper) {
  JSObject* oldWrapper = mWrapper;
  mWrapper = aNewWrapper;
  UnsetWrapperFlags(kWrapperFlagsMask);

  if (IsNurseryWrapper(aNewWrapper) && !IsNurseryWrapper(oldWrapper)) {
    CycleCollectedJSRuntime::Get()->NurseryWrapperAdded(this);
  }
}

void nsWrapperCache::ReleaseWrapper(void* aScriptObjectHolder) {
  // If the behavior here changes in a substantive way, you may need
  // to update css::Rule::UnlinkDeclarationWrapper as well.
  if (PreservingWrapper()) {
    SetPreservingWrapper(false);
    cyclecollector::DropJSObjectsImpl(aScriptObjectHolder);
    JS::HeapObjectPostWriteBarrier(&mWrapper, mWrapper, nullptr);
  }
}

#ifdef DEBUG

void nsWrapperCache::AssertUpdatedWrapperZone(const JSObject* aNewObject,
                                              const JSObject* aOldObject) {
  MOZ_ASSERT(js::GetObjectZoneFromAnyThread(aNewObject) ==
             js::GetObjectZoneFromAnyThread(aOldObject));
}

class DebugWrapperTraversalCallback
    : public nsCycleCollectionTraversalCallback {
 public:
  explicit DebugWrapperTraversalCallback(JSObject* aWrapper)
      : mFound(false), mWrapper(JS::GCCellPtr(aWrapper)) {
    mFlags = WANT_ALL_TRACES;
  }

  NS_IMETHOD_(void)
  DescribeRefCountedNode(nsrefcnt aRefCount, const char* aObjName) override {}
  NS_IMETHOD_(void)
  DescribeGCedNode(bool aIsMarked, const char* aObjName,
                   uint64_t aCompartmentAddress) override {}

  NS_IMETHOD_(void) NoteJSChild(JS::GCCellPtr aChild) override {
    if (aChild == mWrapper) {
      mFound = true;
    }
  }
  NS_IMETHOD_(void) NoteXPCOMChild(nsISupports* aChild) override {}
  NS_IMETHOD_(void)
  NoteNativeChild(void* aChild,
                  nsCycleCollectionParticipant* aHelper) override {}

  NS_IMETHOD_(void) NoteNextEdgeName(const char* aName) override {}

  bool mFound;

 private:
  JS::GCCellPtr mWrapper;
};

static void DebugWrapperTraceCallback(JS::GCCellPtr aPtr, const char* aName,
                                      void* aClosure) {
  DebugWrapperTraversalCallback* callback =
      static_cast<DebugWrapperTraversalCallback*>(aClosure);
  if (aPtr.is<JSObject>()) {
    callback->NoteJSChild(aPtr);
  }
}

void nsWrapperCache::CheckCCWrapperTraversal(void* aScriptObjectHolder,
                                             nsScriptObjectTracer* aTracer) {
  JSObject* wrapper = GetWrapperPreserveColor();
  if (!wrapper) {
    return;
  }

  // Temporarily make this a preserving wrapper so that TraceWrapper() traces
  // it.
  bool wasPreservingWrapper = PreservingWrapper();
  SetPreservingWrapper(true);

  DebugWrapperTraversalCallback callback(wrapper);

  // The CC traversal machinery cannot trigger GC; however, the analysis cannot
  // see through the COM layer, so we use a suppression to help it.
  JS::AutoSuppressGCAnalysis suppress;

  aTracer->TraverseNativeAndJS(aScriptObjectHolder, callback);
  MOZ_ASSERT(callback.mFound,
             "Cycle collection participant didn't traverse to preserved "
             "wrapper! This will probably crash.");

  callback.mFound = false;
  aTracer->Trace(aScriptObjectHolder,
                 TraceCallbackFunc(DebugWrapperTraceCallback), &callback);
  MOZ_ASSERT(callback.mFound,
             "Cycle collection participant didn't trace preserved wrapper! "
             "This will probably crash.");

  SetPreservingWrapper(wasPreservingWrapper);
}

#endif  // DEBUG
