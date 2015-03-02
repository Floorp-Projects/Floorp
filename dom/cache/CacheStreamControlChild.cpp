/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/CacheStreamControlChild.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/unused.h"
#include "mozilla/dom/cache/ActorUtils.h"
#include "mozilla/dom/cache/ReadStream.h"
#include "nsISupportsImpl.h"

namespace mozilla {
namespace dom {
namespace cache {

// declared in ActorUtils.h
PCacheStreamControlChild*
AllocPCacheStreamControlChild()
{
  return new CacheStreamControlChild();
}

// declared in ActorUtils.h
void
DeallocPCacheStreamControlChild(PCacheStreamControlChild* aActor)
{
  delete aActor;
}

CacheStreamControlChild::CacheStreamControlChild()
  : mDestroyStarted(false)
{
  MOZ_COUNT_CTOR(cache::CacheStreamControlChild);
}

CacheStreamControlChild::~CacheStreamControlChild()
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlChild);
  MOZ_COUNT_DTOR(cache::CacheStreamControlChild);
}

void
CacheStreamControlChild::AddListener(ReadStream* aListener)
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlChild);
  MOZ_ASSERT(aListener);
  MOZ_ASSERT(!mListeners.Contains(aListener));
  mListeners.AppendElement(aListener);
}

void
CacheStreamControlChild::RemoveListener(ReadStream* aListener)
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlChild);
  MOZ_ASSERT(aListener);
  MOZ_ALWAYS_TRUE(mListeners.RemoveElement(aListener));
}

void
CacheStreamControlChild::NoteClosed(const nsID& aId)
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlChild);
  unused << SendNoteClosed(aId);
}

void
CacheStreamControlChild::StartDestroy()
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlChild);
  // This can get called twice under some circumstances.  For example, if the
  // actor is added to a Feature that has already been notified and the Cache
  // actor has no mListener.
  if (mDestroyStarted) {
    return;
  }
  mDestroyStarted = true;

  // Begin shutting down all streams.  This is the same as if the parent had
  // asked us to shutdown.  So simulate the CloseAll IPC message.
  RecvCloseAll();
}

void
CacheStreamControlChild::ActorDestroy(ActorDestroyReason aReason)
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlChild);
  // Note, we cannot trigger IPC traffic here.  So use
  // CloseStreamWithoutReporting().
  ReadStreamList::ForwardIterator iter(mListeners);
  while (iter.HasMore()) {
    nsRefPtr<ReadStream> stream = iter.GetNext();
    stream->CloseStreamWithoutReporting();
  }
  mListeners.Clear();

  RemoveFeature();
}

bool
CacheStreamControlChild::RecvClose(const nsID& aId)
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlChild);
  DebugOnly<uint32_t> closedCount = 0;

  ReadStreamList::ForwardIterator iter(mListeners);
  while (iter.HasMore()) {
    nsRefPtr<ReadStream> stream = iter.GetNext();
    // note, multiple streams may exist for same ID
    if (stream->MatchId(aId)) {
      stream->CloseStream();
      closedCount += 1;
    }
  }

  MOZ_ASSERT(closedCount > 0);

  return true;
}

bool
CacheStreamControlChild::RecvCloseAll()
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlChild);
  ReadStreamList::ForwardIterator iter(mListeners);
  while (iter.HasMore()) {
    nsRefPtr<ReadStream> stream = iter.GetNext();
    stream->CloseStream();
  }
  return true;
}

} // namespace cache
} // namespace dom
} // namespace mozilla
