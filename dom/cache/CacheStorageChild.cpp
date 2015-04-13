/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/CacheStorageChild.h"

#include "mozilla/unused.h"
#include "mozilla/dom/cache/CacheChild.h"
#include "mozilla/dom/cache/CacheStorage.h"
#include "mozilla/dom/cache/PCacheOpChild.h"
#include "mozilla/dom/cache/StreamUtils.h"

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

  // TODO: don't destroy if we have outstanding ops

  listener->DestroyInternal(this);

  // CacheStorage listener should call ClearListener() in DestroyInternal()
  MOZ_ASSERT(!mListener);

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
  return true;
}

} // namespace cache
} // namespace dom
} // namespace mozilla
