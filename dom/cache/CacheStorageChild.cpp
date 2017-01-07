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

namespace mozilla {
namespace dom {
namespace cache {

// declared in ActorUtils.h
void
DeallocPCacheStorageChild(PCacheStorageChild* aActor)
{
  delete aActor;
}

CacheStorageChild::CacheStorageChild(CacheStorage* aListener,
                                     CacheWorkerHolder* aWorkerHolder)
  : mListener(aListener)
  , mNumChildActors(0)
  , mDelayedDestroy(false)
{
  MOZ_COUNT_CTOR(cache::CacheStorageChild);
  MOZ_DIAGNOSTIC_ASSERT(mListener);

  SetWorkerHolder(aWorkerHolder);
}

CacheStorageChild::~CacheStorageChild()
{
  MOZ_COUNT_DTOR(cache::CacheStorageChild);
  NS_ASSERT_OWNINGTHREAD(CacheStorageChild);
  MOZ_DIAGNOSTIC_ASSERT(!mListener);
}

void
CacheStorageChild::ClearListener()
{
  NS_ASSERT_OWNINGTHREAD(CacheStorageChild);
  MOZ_DIAGNOSTIC_ASSERT(mListener);
  mListener = nullptr;
}

void
CacheStorageChild::ExecuteOp(nsIGlobalObject* aGlobal, Promise* aPromise,
                             nsISupports* aParent, const CacheOpArgs& aArgs)
{
  mNumChildActors += 1;
  Unused << SendPCacheOpConstructor(
    new CacheOpChild(GetWorkerHolder(), aGlobal, aParent, aPromise), aArgs);
}

void
CacheStorageChild::StartDestroyFromListener()
{
  NS_ASSERT_OWNINGTHREAD(CacheStorageChild);

  // The listener should be held alive by any async operations, so if it
  // is going away then there must not be any child actors.  This in turn
  // ensures that StartDestroy() will not trigger the delayed path.
  MOZ_DIAGNOSTIC_ASSERT(!mNumChildActors);

  StartDestroy();
}

void
CacheStorageChild::StartDestroy()
{
  NS_ASSERT_OWNINGTHREAD(CacheStorageChild);

  // If we have outstanding child actors, then don't destroy ourself yet.
  // The child actors should be short lived and we should allow them to complete
  // if possible.  NoteDeletedActor() will call back into this Shutdown()
  // method when the last child actor is gone.
  if (mNumChildActors) {
    mDelayedDestroy = true;
    return;
  }

  RefPtr<CacheStorage> listener = mListener;

  // StartDestroy() can get called from either CacheStorage or the
  // CacheWorkerHolder.
  // Theoretically we can get double called if the right race happens.  Handle
  // that by just ignoring the second StartDestroy() call.
  if (!listener) {
    return;
  }

  listener->DestroyInternal(this);

  // CacheStorage listener should call ClearListener() in DestroyInternal()
  MOZ_DIAGNOSTIC_ASSERT(!mListener);

  // Start actor destruction from parent process
  Unused << SendTeardown();
}

void
CacheStorageChild::ActorDestroy(ActorDestroyReason aReason)
{
  NS_ASSERT_OWNINGTHREAD(CacheStorageChild);
  RefPtr<CacheStorage> listener = mListener;
  if (listener) {
    listener->DestroyInternal(this);
    // CacheStorage listener should call ClearListener() in DestroyInternal()
    MOZ_DIAGNOSTIC_ASSERT(!mListener);
  }

  RemoveWorkerHolder();
}

PCacheOpChild*
CacheStorageChild::AllocPCacheOpChild(const CacheOpArgs& aOpArgs)
{
  MOZ_CRASH("CacheOpChild should be manually constructed.");
  return nullptr;
}

bool
CacheStorageChild::DeallocPCacheOpChild(PCacheOpChild* aActor)
{
  delete aActor;
  NoteDeletedActor();
  return true;
}

void
CacheStorageChild::NoteDeletedActor()
{
  MOZ_DIAGNOSTIC_ASSERT(mNumChildActors);
  mNumChildActors -= 1;
  if (!mNumChildActors && mDelayedDestroy) {
    StartDestroy();
  }
}

} // namespace cache
} // namespace dom
} // namespace mozilla
