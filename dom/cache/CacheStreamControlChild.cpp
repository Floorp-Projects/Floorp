/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/CacheStreamControlChild.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/unused.h"
#include "mozilla/dom/cache/ActorUtils.h"
#include "mozilla/dom/cache/CacheTypes.h"
#include "mozilla/dom/cache/ReadStream.h"
#include "mozilla/ipc/FileDescriptorSetChild.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/ipc/PFileDescriptorSetChild.h"
#include "nsISupportsImpl.h"

namespace mozilla {
namespace dom {
namespace cache {

using mozilla::ipc::AutoIPCStream;
using mozilla::ipc::FileDescriptor;
using mozilla::ipc::FileDescriptorSetChild;
using mozilla::ipc::OptionalFileDescriptorSet;
using mozilla::ipc::PFileDescriptorSetChild;

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
  , mDestroyDelayed(false)
{
  MOZ_COUNT_CTOR(cache::CacheStreamControlChild);
}

CacheStreamControlChild::~CacheStreamControlChild()
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlChild);
  MOZ_COUNT_DTOR(cache::CacheStreamControlChild);
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

  // If any of the streams have started to be read, then wait for them to close
  // naturally.
  if (HasEverBeenRead()) {
    // Note that we are delaying so that we can re-check for active streams
    // in NoteClosedAfterForget().
    mDestroyDelayed = true;
    return;
  }

  // Otherwise, if the streams have not been touched then just pre-emptively
  // close them now.  This handles the case where someone retrieves a Response
  // from the Cache, but never accesses the body.  We should not keep the
  // Worker alive until that Response is GC'd just because of its ignored
  // body stream.

  // Begin shutting down all streams.  This is the same as if the parent had
  // asked us to shutdown.  So simulate the CloseAll IPC message.
  RecvCloseAll();
}

void
CacheStreamControlChild::SerializeControl(CacheReadStream* aReadStreamOut)
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlChild);
  aReadStreamOut->controlParent() = nullptr;
  aReadStreamOut->controlChild() = this;
}

void
CacheStreamControlChild::SerializeStream(CacheReadStream* aReadStreamOut,
                                         nsIInputStream* aStream,
                                         nsTArray<UniquePtr<AutoIPCStream>>& aStreamCleanupList)
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlChild);
  MOZ_ASSERT(aReadStreamOut);
  MOZ_ASSERT(aStream);
  UniquePtr<AutoIPCStream> autoStream(new AutoIPCStream(aReadStreamOut->stream()));
  autoStream->Serialize(aStream, Manager());
  aStreamCleanupList.AppendElement(Move(autoStream));
}

void
CacheStreamControlChild::NoteClosedAfterForget(const nsID& aId)
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlChild);
  Unused << SendNoteClosed(aId);

  // A stream has closed.  If we delayed StartDestry() due to this stream
  // being read, then we should check to see if any of the remaining streams
  // are active.  If none of our other streams have been read, then we can
  // proceed with the shutdown now.
  if (mDestroyDelayed && !HasEverBeenRead()) {
    mDestroyDelayed = false;
    RecvCloseAll();
  }
}

#ifdef DEBUG
void
CacheStreamControlChild::AssertOwningThread()
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlChild);
}
#endif

void
CacheStreamControlChild::ActorDestroy(ActorDestroyReason aReason)
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlChild);
  CloseAllReadStreamsWithoutReporting();
  RemoveFeature();
}

bool
CacheStreamControlChild::RecvClose(const nsID& aId)
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlChild);
  CloseReadStreams(aId);
  return true;
}

bool
CacheStreamControlChild::RecvCloseAll()
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlChild);
  CloseAllReadStreams();
  return true;
}

} // namespace cache
} // namespace dom
} // namespace mozilla
