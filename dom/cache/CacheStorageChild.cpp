/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/CacheStorageChild.h"

#include "mozilla/unused.h"
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

CacheStorageChild::CacheStorageChild(CacheStorage* aListener, Feature* aFeature)
  : mListener(aListener)
  , mNumChildActors(0)
{
  MOZ_COUNT_CTOR(cache::CacheStorageChild);
  MOZ_ASSERT(mListener);

  SetFeature(aFeature);
}

CacheStorageChild::~CacheStorageChild()
{
  MOZ_COUNT_DTOR(cache::CacheStorageChild);
  NS_ASSERT_OWNINGTHREAD(CacheStorageChild);
  MOZ_ASSERT(!mListener);
}

void
CacheStorageChild::ClearListener()
{
  NS_ASSERT_OWNINGTHREAD(CacheStorageChild);
  MOZ_ASSERT(mListener);
  mListener = nullptr;
}

void
CacheStorageChild::ExecuteOp(nsIGlobalObject* aGlobal, Promise* aPromise,
                             const CacheOpArgs& aArgs)
{
  mNumChildActors += 1;
  unused << SendPCacheOpConstructor(
    new CacheOpChild(GetFeature(), aGlobal, aPromise), aArgs);
}

void
CacheStorageChild::StartDestroy()
{
  NS_ASSERT_OWNINGTHREAD(CacheStorageChild);

  nsRefPtr<CacheStorage> listener = mListener;

  // StartDestroy() can get called from either CacheStorage or the Feature.
  // Theoretically we can get double called if the right race happens.  Handle
  // that by just ignoring the second StartDestroy() call.
  if (!listener) {
    return;
  }

  listener->DestroyInternal(this);

  // CacheStorage listener should call ClearListener() in DestroyInternal()
  MOZ_ASSERT(!mListener);

  // If we have outstanding child actors, then don't destroy ourself yet.
  // The child actors should be short lived and we should allow them to complete
  // if possible.  SendTeardown() will be called when the count drops to zero
  // in NoteDeletedActor().
  if (mNumChildActors) {
    return;
  }

  // Start actor destruction from parent process
  unused << SendTeardown();
}

void
CacheStorageChild::ActorDestroy(ActorDestroyReason aReason)
{
  NS_ASSERT_OWNINGTHREAD(CacheStorageChild);
  nsRefPtr<CacheStorage> listener = mListener;
  if (listener) {
    listener->DestroyInternal(this);
    // CacheStorage listener should call ClearListener() in DestroyInternal()
    MOZ_ASSERT(!mListener);
  }

  RemoveFeature();
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
  MOZ_ASSERT(mNumChildActors);
  mNumChildActors -= 1;
  if (!mNumChildActors && !mListener) {
    unused << SendTeardown();
  }
}

} // namespace cache
} // namespace dom
} // namespace mozilla
