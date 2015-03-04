/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/CacheStreamControlParent.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/unused.h"
#include "mozilla/dom/cache/ReadStream.h"
#include "mozilla/dom/cache/StreamList.h"
#include "nsISupportsImpl.h"

namespace mozilla {
namespace dom {
namespace cache {

// declared in ActorUtils.h
void
DeallocPCacheStreamControlParent(PCacheStreamControlParent* aActor)
{
  delete aActor;
}

CacheStreamControlParent::CacheStreamControlParent()
{
  MOZ_COUNT_CTOR(cache::CacheStreamControlParent);
}

CacheStreamControlParent::~CacheStreamControlParent()
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlParent);
  MOZ_ASSERT(!mStreamList);
  MOZ_COUNT_DTOR(cache::CacheStreamControlParent);
}

void
CacheStreamControlParent::AddListener(ReadStream* aListener)
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlParent);
  MOZ_ASSERT(aListener);
  MOZ_ASSERT(!mListeners.Contains(aListener));
  mListeners.AppendElement(aListener);
}

void
CacheStreamControlParent::RemoveListener(ReadStream* aListener)
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlParent);
  MOZ_ASSERT(aListener);
  DebugOnly<bool> removed = mListeners.RemoveElement(aListener);
  MOZ_ASSERT(removed);
}

void
CacheStreamControlParent::ActorDestroy(ActorDestroyReason aReason)
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlParent);
  MOZ_ASSERT(mStreamList);
  ReadStreamList::ForwardIterator iter(mListeners);
  while (iter.HasMore()) {
    nsRefPtr<ReadStream> stream = iter.GetNext();
    stream->CloseStreamWithoutReporting();
  }
  mStreamList->RemoveStreamControl(this);
  mStreamList->NoteClosedAll();
  mStreamList = nullptr;
}

bool
CacheStreamControlParent::RecvNoteClosed(const nsID& aId)
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlParent);
  MOZ_ASSERT(mStreamList);
  mStreamList->NoteClosed(aId);
  return true;
}

void
CacheStreamControlParent::SetStreamList(StreamList* aStreamList)
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlParent);
  MOZ_ASSERT(!mStreamList);
  mStreamList = aStreamList;
}

void
CacheStreamControlParent::Close(const nsID& aId)
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlParent);
  NotifyClose(aId);
  unused << SendClose(aId);
}

void
CacheStreamControlParent::CloseAll()
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlParent);
  NotifyCloseAll();
  unused << SendCloseAll();
}

void
CacheStreamControlParent::Shutdown()
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlParent);
  if (!Send__delete__(this)) {
    // child process is gone, allow actor to be destroyed normally
    NS_WARNING("Cache failed to delete stream actor.");
    return;
  }
}

void
CacheStreamControlParent::NotifyClose(const nsID& aId)
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlParent);
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
}

void
CacheStreamControlParent::NotifyCloseAll()
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlParent);
  ReadStreamList::ForwardIterator iter(mListeners);
  while (iter.HasMore()) {
    nsRefPtr<ReadStream> stream = iter.GetNext();
    stream->CloseStream();
  }
}

} // namespace cache
} // namespace dom
} // namespace mozilla
