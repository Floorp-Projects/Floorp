/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/CacheStorageChild.h"

#include "mozilla/Unused.h"
#include "mozilla/dom/cache/CacheChild.h"
#include "mozilla/dom/cache/CacheOpChild.h"
#include "mozilla/dom/cache/CacheStorage.h"
#include "mozilla/dom/cache/CacheWorkerRef.h"

namespace mozilla::dom::cache {

// declared in ActorUtils.h
void DeallocPCacheStorageChild(PCacheStorageChild* aActor) { delete aActor; }

CacheStorageChild::CacheStorageChild(CacheStorage* aListener,
                                     SafeRefPtr<CacheWorkerRef> aWorkerRef)
    : mListener(aListener), mDelayedDestroy(false) {
  MOZ_COUNT_CTOR(cache::CacheStorageChild);
  MOZ_DIAGNOSTIC_ASSERT(mListener);

  SetWorkerRef(std::move(aWorkerRef));
}

CacheStorageChild::~CacheStorageChild() {
  MOZ_COUNT_DTOR(cache::CacheStorageChild);
  NS_ASSERT_OWNINGTHREAD(CacheStorageChild);
  MOZ_DIAGNOSTIC_ASSERT(!mListener);
}

void CacheStorageChild::ClearListener() {
  NS_ASSERT_OWNINGTHREAD(CacheStorageChild);
  MOZ_DIAGNOSTIC_ASSERT(mListener);
  mListener = nullptr;
}

void CacheStorageChild::ExecuteOp(nsIGlobalObject* aGlobal, Promise* aPromise,
                                  nsISupports* aParent,
                                  const CacheOpArgs& aArgs) {
  Unused << SendPCacheOpConstructor(
      new CacheOpChild(GetWorkerRefPtr().clonePtr(), aGlobal, aParent, aPromise,
                       this),
      aArgs);
}

void CacheStorageChild::StartDestroyFromListener() {
  NS_ASSERT_OWNINGTHREAD(CacheStorageChild);

  StartDestroy();
}

void CacheStorageChild::DestroyInternal() {
  RefPtr<CacheStorage> listener = mListener;

  // StartDestroy() can get called from either CacheStorage or the
  // CacheWorkerRef.
  // Theoretically we can get double called if the right race happens.  Handle
  // that by just ignoring the second StartDestroy() call.
  if (!listener) {
    return;
  }

  listener->DestroyInternal(this);

  // CacheStorage listener should call ClearListener() in DestroyInternal()
  MOZ_DIAGNOSTIC_ASSERT(!mListener);

  // Start actor destruction from parent process
  QM_WARNONLY_TRY(OkIf(SendTeardown()));
}

void CacheStorageChild::StartDestroy() {
  NS_ASSERT_OWNINGTHREAD(CacheStorageChild);

  if (NumChildActors() != 0) {
    mDelayedDestroy = true;
    return;
  }
  DestroyInternal();
}

void CacheStorageChild::NoteDeletedActor() {
  // Check to see if DestroyInternal was delayed because of active CacheOpChilds
  // when StartDestroy was called from WorkerRef notification. If the last
  // CacheOpChild is getting destructed; it's the time for us to SendTearDown to
  // the other side.
  if (NumChildActors() == 1 && mDelayedDestroy) DestroyInternal();
}

void CacheStorageChild::ActorDestroy(ActorDestroyReason aReason) {
  NS_ASSERT_OWNINGTHREAD(CacheStorageChild);
  RefPtr<CacheStorage> listener = mListener;
  if (listener) {
    listener->DestroyInternal(this);
    // CacheStorage listener should call ClearListener() in DestroyInternal()
    MOZ_DIAGNOSTIC_ASSERT(!mListener);
  }

  RemoveWorkerRef();
}

PCacheOpChild* CacheStorageChild::AllocPCacheOpChild(
    const CacheOpArgs& aOpArgs) {
  MOZ_CRASH("CacheOpChild should be manually constructed.");
  return nullptr;
}
}  // namespace mozilla::dom::cache
