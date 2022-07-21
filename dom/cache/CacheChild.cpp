/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/CacheChild.h"

#include "mozilla/Unused.h"
#include "mozilla/dom/cache/ActorUtils.h"
#include "mozilla/dom/cache/Cache.h"
#include "mozilla/dom/cache/CacheOpChild.h"
#include "CacheWorkerRef.h"

namespace mozilla::dom::cache {

// Declared in ActorUtils.h
already_AddRefed<PCacheChild> AllocPCacheChild() {
  return MakeAndAddRef<CacheChild>();
}

// Declared in ActorUtils.h
void DeallocPCacheChild(PCacheChild* aActor) { delete aActor; }

CacheChild::CacheChild()
    : mListener(nullptr), mLocked(false), mDelayedDestroy(false) {
  MOZ_COUNT_CTOR(cache::CacheChild);
}

CacheChild::~CacheChild() {
  MOZ_COUNT_DTOR(cache::CacheChild);
  NS_ASSERT_OWNINGTHREAD(CacheChild);
  MOZ_DIAGNOSTIC_ASSERT(!mListener);
  MOZ_DIAGNOSTIC_ASSERT(!mLocked);
}

void CacheChild::SetListener(Cache* aListener) {
  NS_ASSERT_OWNINGTHREAD(CacheChild);
  MOZ_DIAGNOSTIC_ASSERT(!mListener);
  mListener = aListener;
  MOZ_DIAGNOSTIC_ASSERT(mListener);
}

void CacheChild::ClearListener() {
  NS_ASSERT_OWNINGTHREAD(CacheChild);
  MOZ_DIAGNOSTIC_ASSERT(mListener);
  mListener = nullptr;
}

void CacheChild::ExecuteOp(nsIGlobalObject* aGlobal, Promise* aPromise,
                           nsISupports* aParent, const CacheOpArgs& aArgs) {
  MOZ_ALWAYS_TRUE(SendPCacheOpConstructor(
      new CacheOpChild(GetWorkerRefPtr().clonePtr(), aGlobal, aParent, aPromise,
                       this),
      aArgs));
}

void CacheChild::StartDestroyFromListener() {
  NS_ASSERT_OWNINGTHREAD(CacheChild);

  // The listener should be held alive by any async operations, so if it
  // is going away then there must not be any child actors.  This in turn
  // ensures that StartDestroy() will not trigger the delayed path.
  MOZ_DIAGNOSTIC_ASSERT(NumChildActors() == 0);
  StartDestroy();
}

void CacheChild::DestroyInternal() {
  RefPtr<Cache> listener = mListener;

  // StartDestroy() can get called from either Cache or the WorkerRef.
  // Theoretically we can get double called if the right race happens.  Handle
  // that by just ignoring the second StartDestroy() call.
  if (!listener) {
    return;
  }

  listener->DestroyInternal(this);

  // Cache listener should call ClearListener() in DestroyInternal()
  MOZ_DIAGNOSTIC_ASSERT(!mListener);

  // Start actor destruction from parent process
  QM_WARNONLY_TRY(OkIf(SendTeardown()));
}

void CacheChild::StartDestroy() {
  NS_ASSERT_OWNINGTHREAD(CacheChild);

  if (NumChildActors() != 0 || mLocked) {
    mDelayedDestroy = true;
    return;
  }

  DestroyInternal();
}

void CacheChild::ActorDestroy(ActorDestroyReason aReason) {
  NS_ASSERT_OWNINGTHREAD(CacheChild);
  RefPtr<Cache> listener = mListener;
  if (listener) {
    listener->DestroyInternal(this);
    // Cache listener should call ClearListener() in DestroyInternal()
    MOZ_DIAGNOSTIC_ASSERT(!mListener);
  }

  RemoveWorkerRef();
}

void CacheChild::NoteDeletedActor() {
  // Check to see if DestroyInternal was delayed because there were still active
  // CacheOpChilds when StartDestroy was called from WorkerRef notification. If
  // the last CacheOpChild is getting destructed; it's the time for us to
  // SendTearDown to the other side.
  if (NumChildActors() == 1 && mDelayedDestroy && !mLocked) DestroyInternal();
}

already_AddRefed<PCacheOpChild> CacheChild::AllocPCacheOpChild(
    const CacheOpArgs& aOpArgs) {
  MOZ_CRASH("CacheOpChild should be manually constructed.");
  return nullptr;
}

void CacheChild::Lock() {
  NS_ASSERT_OWNINGTHREAD(CacheChild);
  MOZ_DIAGNOSTIC_ASSERT(!mLocked);
  mLocked = true;
}

void CacheChild::Unlock() {
  NS_ASSERT_OWNINGTHREAD(CacheChild);
  MOZ_DIAGNOSTIC_ASSERT(mLocked);
  mLocked = false;
}

}  // namespace mozilla::dom::cache
