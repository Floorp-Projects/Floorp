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
#include "mozilla/ipc/IPCStreamUtils.h"
#include "mozilla/ipc/PBackgroundParent.h"
#include "nsISupportsImpl.h"
#include "nsTArray.h"

namespace mozilla::dom::cache {

using mozilla::ipc::FileDescriptor;

// declared in ActorUtils.h
void DeallocPCacheStreamControlParent(PCacheStreamControlParent* aActor) {
  delete aActor;
}

CacheStreamControlParent::CacheStreamControlParent() {
  MOZ_COUNT_CTOR(cache::CacheStreamControlParent);
}

CacheStreamControlParent::~CacheStreamControlParent() {
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlParent);
  MOZ_DIAGNOSTIC_ASSERT(!mStreamList);
  MOZ_COUNT_DTOR(cache::CacheStreamControlParent);
}

void CacheStreamControlParent::SerializeControl(
    CacheReadStream* aReadStreamOut) {
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlParent);
  MOZ_DIAGNOSTIC_ASSERT(aReadStreamOut);
  aReadStreamOut->control() = this;
}

void CacheStreamControlParent::SerializeStream(CacheReadStream* aReadStreamOut,
                                               nsIInputStream* aStream) {
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlParent);
  MOZ_DIAGNOSTIC_ASSERT(aReadStreamOut);

  DebugOnly<bool> ok = mozilla::ipc::SerializeIPCStream(
      do_AddRef(aStream), aReadStreamOut->stream(), /* aAllowLazy */ false);
  MOZ_ASSERT(ok);
}

void CacheStreamControlParent::OpenStream(const nsID& aId,
                                          InputStreamResolver&& aResolver) {
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
  mStreamList->GetManager().ExecuteOpenStream(this, std::move(aResolver), aId);
}

void CacheStreamControlParent::NoteClosedAfterForget(const nsID& aId) {
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlParent);
  RecvNoteClosed(aId);
}

#ifdef DEBUG
void CacheStreamControlParent::AssertOwningThread() {
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlParent);
}
#endif

void CacheStreamControlParent::LostIPCCleanup(
    SafeRefPtr<StreamList> aStreamList) {
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlParent);
  CloseAllReadStreamsWithoutReporting();
  // If the initial SendPStreamControlConstructor() fails we will
  // be called before mStreamList is set.
  if (!aStreamList) {
    return;
  }
  aStreamList->GetManager().RemoveListener(this);
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  aStreamList->GetManager().RecordHaveDeletedCSCP(Id());
#endif
  aStreamList->RemoveStreamControl(this);
  aStreamList->NoteClosedAll();
  mStreamList = nullptr;
}

void CacheStreamControlParent::ActorDestroy(ActorDestroyReason aReason) {
  LostIPCCleanup(std::move(mStreamList));
}

void CacheStreamControlParent::AssertWillDelete() {
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  // If we cannot send, Send__delete__ will not do any cleanup.
  // But if we are still unfreed, that might be wrong.
  if (mStreamList && !CanSend()) {
    MOZ_ASSERT(false, "Attempt to delete blocking CSCP that cannot send.");
    mStreamList->GetManager().RecordMayNotDeleteCSCP(Id());
  }
#endif
}

mozilla::ipc::IPCResult CacheStreamControlParent::RecvOpenStream(
    const nsID& aStreamId, OpenStreamResolver&& aResolver) {
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlParent);

  OpenStream(aStreamId, [aResolver, self = RefPtr{this}](
                            nsCOMPtr<nsIInputStream>&& aStream) {
    Maybe<IPCStream> stream;
    if (self->CanSend() &&
        mozilla::ipc::SerializeIPCStream(aStream.forget(), stream,
                                         /* aAllowLazy */ false)) {
      aResolver(stream);
    } else {
      aResolver(Nothing());
    }
  });

  return IPC_OK();
}

mozilla::ipc::IPCResult CacheStreamControlParent::RecvNoteClosed(
    const nsID& aId) {
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlParent);
  MOZ_DIAGNOSTIC_ASSERT(mStreamList);
  mStreamList->NoteClosed(aId);
  return IPC_OK();
}

void CacheStreamControlParent::SetStreamList(
    SafeRefPtr<StreamList> aStreamList) {
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlParent);
  MOZ_DIAGNOSTIC_ASSERT(!mStreamList);
  mStreamList = std::move(aStreamList);
}

void CacheStreamControlParent::CloseAll() {
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlParent);
  NotifyCloseAll();

  QM_WARNONLY_TRY(OkIf(SendCloseAll()));
}

void CacheStreamControlParent::Shutdown() {
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlParent);

  AssertWillDelete();
  // If child process is gone, warn and allow actor to clean up normally
  QM_WARNONLY_TRY(OkIf(Send__delete__(this)));
}

void CacheStreamControlParent::NotifyCloseAll() {
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlParent);
  CloseAllReadStreams();
}

}  // namespace mozilla::dom::cache
