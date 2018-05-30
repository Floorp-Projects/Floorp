/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/CacheStreamControlParent.h"

#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/cache/CacheTypes.h"
#include "mozilla/dom/cache/ReadStream.h"
#include "mozilla/dom/cache/StreamList.h"
#include "mozilla/ipc/FileDescriptorSetParent.h"
#include "mozilla/ipc/IPCStreamUtils.h"
#include "mozilla/ipc/PBackgroundParent.h"
#include "mozilla/ipc/PFileDescriptorSetParent.h"
#include "nsISupportsImpl.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {
namespace cache {

using mozilla::dom::OptionalFileDescriptorSet;
using mozilla::ipc::AutoIPCStream;
using mozilla::ipc::FileDescriptor;
using mozilla::ipc::FileDescriptorSetParent;
using mozilla::ipc::PFileDescriptorSetParent;

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
  MOZ_DIAGNOSTIC_ASSERT(!mStreamList);
  MOZ_COUNT_DTOR(cache::CacheStreamControlParent);
}

void
CacheStreamControlParent::SerializeControl(CacheReadStream* aReadStreamOut)
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlParent);
  MOZ_DIAGNOSTIC_ASSERT(aReadStreamOut);
  aReadStreamOut->controlChild() = nullptr;
  aReadStreamOut->controlParent() = this;
}

void
CacheStreamControlParent::SerializeStream(CacheReadStream* aReadStreamOut,
                                          nsIInputStream* aStream,
                                          nsTArray<UniquePtr<AutoIPCStream>>& aStreamCleanupList)
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlParent);
  MOZ_DIAGNOSTIC_ASSERT(aReadStreamOut);

  UniquePtr<AutoIPCStream> autoStream(new AutoIPCStream(aReadStreamOut->stream()));
  DebugOnly<bool> ok = autoStream->Serialize(aStream, Manager());
  MOZ_ASSERT(ok);

  aStreamCleanupList.AppendElement(std::move(autoStream));
}

void
CacheStreamControlParent::OpenStream(const nsID& aId,
                                     InputStreamResolver&& aResolver)
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlParent);
  MOZ_DIAGNOSTIC_ASSERT(aResolver);

  if (!mStreamList || !mStreamList->ShouldOpenStreamFor(aId)) {
    aResolver(nullptr);
    return;
  }

  // Make sure to add ourself as a Listener even thought we are using
  // a separate resolver function to signal the completion of the
  // operation.  The Manager uses the existence of the Listener to ensure
  // that its safe to complete the operation.
  mStreamList->GetManager()->ExecuteOpenStream(this, std::move(aResolver), aId);
}

void
CacheStreamControlParent::NoteClosedAfterForget(const nsID& aId)
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlParent);
  RecvNoteClosed(aId);
}

#ifdef DEBUG
void
CacheStreamControlParent::AssertOwningThread()
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlParent);
}
#endif

void
CacheStreamControlParent::ActorDestroy(ActorDestroyReason aReason)
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlParent);
  CloseAllReadStreamsWithoutReporting();
  // If the initial SendPStreamControlConstructor() fails we will
  // be called before mStreamList is set.
  if (!mStreamList) {
    return;
  }
  mStreamList->GetManager()->RemoveListener(this);
  mStreamList->RemoveStreamControl(this);
  mStreamList->NoteClosedAll();
  mStreamList = nullptr;
}

mozilla::ipc::IPCResult
CacheStreamControlParent::RecvOpenStream(const nsID& aStreamId,
                                         OpenStreamResolver&& aResolver)
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlParent);

  OpenStream(aStreamId, [aResolver](nsCOMPtr<nsIInputStream>&& aStream) {
      aResolver(aStream);
    });

  return IPC_OK();
}

mozilla::ipc::IPCResult
CacheStreamControlParent::RecvNoteClosed(const nsID& aId)
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlParent);
  MOZ_DIAGNOSTIC_ASSERT(mStreamList);
  mStreamList->NoteClosed(aId);
  return IPC_OK();
}

void
CacheStreamControlParent::SetStreamList(StreamList* aStreamList)
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlParent);
  MOZ_DIAGNOSTIC_ASSERT(!mStreamList);
  mStreamList = aStreamList;
}

void
CacheStreamControlParent::Close(const nsID& aId)
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlParent);
  NotifyClose(aId);
  Unused << SendClose(aId);
}

void
CacheStreamControlParent::CloseAll()
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlParent);
  NotifyCloseAll();
  Unused << SendCloseAll();
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
  CloseReadStreams(aId);
}

void
CacheStreamControlParent::NotifyCloseAll()
{
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlParent);
  CloseAllReadStreams();
}

} // namespace cache
} // namespace dom
} // namespace mozilla
