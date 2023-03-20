/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheStreamControlChild.h"

#include "mozilla/Unused.h"
#include "mozilla/dom/cache/ActorUtils.h"
#include "mozilla/dom/cache/CacheTypes.h"
#include "mozilla/dom/cache/CacheWorkerRef.h"
#include "mozilla/ipc/IPCStreamUtils.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "nsISupportsImpl.h"

namespace mozilla::dom::cache {

using mozilla::ipc::FileDescriptor;

// declared in ActorUtils.h
already_AddRefed<PCacheStreamControlChild> AllocPCacheStreamControlChild() {
  return MakeAndAddRef<CacheStreamControlChild>();
}

CacheStreamControlChild::CacheStreamControlChild()
    : mDestroyStarted(false), mDestroyDelayed(false) {
  MOZ_COUNT_CTOR(cache::CacheStreamControlChild);
}

CacheStreamControlChild::~CacheStreamControlChild() {
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlChild);
  MOZ_COUNT_DTOR(cache::CacheStreamControlChild);
}

void CacheStreamControlChild::StartDestroy() {
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlChild);
  // This can get called twice under some circumstances.  For example, if the
  // actor is added to a CacheWorkerRef that has already been notified and
  // the Cache actor has no mListener.
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

void CacheStreamControlChild::SerializeControl(
    CacheReadStream* aReadStreamOut) {
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlChild);
  MOZ_DIAGNOSTIC_ASSERT(aReadStreamOut);
  aReadStreamOut->control() = this;
}

void CacheStreamControlChild::SerializeStream(CacheReadStream* aReadStreamOut,
                                              nsIInputStream* aStream) {
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlChild);
  MOZ_DIAGNOSTIC_ASSERT(aReadStreamOut);
  MOZ_ALWAYS_TRUE(mozilla::ipc::SerializeIPCStream(
      do_AddRef(aStream), aReadStreamOut->stream(), /* aAllowLazy */ false));
}

void CacheStreamControlChild::OpenStream(const nsID& aId,
                                         InputStreamResolver&& aResolver) {
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlChild);

  if (mDestroyStarted) {
    aResolver(nullptr);
    return;
  }

  // If we are on a worker, then we need to hold it alive until the async
  // IPC operation below completes.  While the IPC layer will trigger a
  // rejection here in many cases, we must handle the case where the
  // MozPromise resolve runnable is already in the event queue when the
  // worker wants to shut down.
  const SafeRefPtr<CacheWorkerRef> holder = GetWorkerRefPtr().clonePtr();

  SendOpenStream(aId)->Then(
      GetCurrentSerialEventTarget(), __func__,
      [aResolver,
       holder = holder.clonePtr()](const Maybe<IPCStream>& aOptionalStream) {
        nsCOMPtr<nsIInputStream> stream = DeserializeIPCStream(aOptionalStream);
        aResolver(std::move(stream));
      },
      [aResolver, holder = holder.clonePtr()](ResponseRejectReason&& aReason) {
        aResolver(nullptr);
      });
}

void CacheStreamControlChild::NoteClosedAfterForget(const nsID& aId) {
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlChild);

  QM_WARNONLY_TRY(OkIf(SendNoteClosed(aId)));

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
void CacheStreamControlChild::AssertOwningThread() {
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlChild);
}
#endif

void CacheStreamControlChild::ActorDestroy(ActorDestroyReason aReason) {
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlChild);
  CloseAllReadStreamsWithoutReporting();
  RemoveWorkerRef();
}

mozilla::ipc::IPCResult CacheStreamControlChild::RecvCloseAll() {
  NS_ASSERT_OWNINGTHREAD(CacheStreamControlChild);
  CloseAllReadStreams();
  return IPC_OK();
}

}  // namespace mozilla::dom::cache
