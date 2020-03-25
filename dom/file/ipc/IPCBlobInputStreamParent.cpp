/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IPCBlobInputStreamParent.h"
#include "IPCBlobInputStreamStorage.h"
#include "mozilla/ipc/IPCStreamUtils.h"
#include "mozilla/InputStreamLengthHelper.h"
#include "nsContentUtils.h"

namespace mozilla {
namespace dom {

template <typename M>
/* static */
already_AddRefed<IPCBlobInputStreamParent> IPCBlobInputStreamParent::Create(
    nsIInputStream* aInputStream, uint64_t aSize, uint64_t aChildID,
    nsresult* aRv, M* aManager) {
  MOZ_ASSERT(aInputStream);
  MOZ_ASSERT(aRv);

  nsID id;
  *aRv = nsContentUtils::GenerateUUIDInPlace(id);
  if (NS_WARN_IF(NS_FAILED(*aRv))) {
    return nullptr;
  }

  IPCBlobInputStreamStorage::Get()->AddStream(aInputStream, id, aSize,
                                              aChildID);

  RefPtr<IPCBlobInputStreamParent> parent =
      new IPCBlobInputStreamParent(id, aSize, aManager);
  return parent.forget();
}

/* static */
already_AddRefed<IPCBlobInputStreamParent> IPCBlobInputStreamParent::Create(
    const nsID& aID, uint64_t aSize, PBackgroundParent* aManager) {
  RefPtr<IPCBlobInputStreamParent> actor =
      new IPCBlobInputStreamParent(aID, aSize, aManager);

  actor->mCallback = IPCBlobInputStreamStorage::Get()->TakeCallback(aID);

  return actor.forget();
}

template already_AddRefed<IPCBlobInputStreamParent>
IPCBlobInputStreamParent::Create<mozilla::ipc::PBackgroundParent>(
    nsIInputStream*, uint64_t, uint64_t, nsresult*,
    mozilla::ipc::PBackgroundParent*);

template already_AddRefed<IPCBlobInputStreamParent>
IPCBlobInputStreamParent::Create<ContentParent>(nsIInputStream*, uint64_t,
                                                uint64_t, nsresult*,
                                                ContentParent*);

IPCBlobInputStreamParent::IPCBlobInputStreamParent(const nsID& aID,
                                                   uint64_t aSize,
                                                   ContentParent* aManager)
    : mID(aID),
      mSize(aSize),
      mContentManager(aManager),
      mPBackgroundManager(nullptr),
      mMigrating(false) {}

IPCBlobInputStreamParent::IPCBlobInputStreamParent(const nsID& aID,
                                                   uint64_t aSize,
                                                   PBackgroundParent* aManager)
    : mID(aID),
      mSize(aSize),
      mContentManager(nullptr),
      mPBackgroundManager(aManager),
      mMigrating(false) {}

void IPCBlobInputStreamParent::ActorDestroy(
    IProtocol::ActorDestroyReason aReason) {
  MOZ_ASSERT(mContentManager || mPBackgroundManager);

  mContentManager = nullptr;
  mPBackgroundManager = nullptr;

  RefPtr<IPCBlobInputStreamParentCallback> callback;
  mCallback.swap(callback);

  RefPtr<IPCBlobInputStreamStorage> storage = IPCBlobInputStreamStorage::Get();

  if (mMigrating) {
    if (callback && storage) {
      // We need to assign this callback to the next parent.
      storage->StoreCallback(mID, callback);
    }
    return;
  }

  if (storage) {
    storage->ForgetStream(mID);
  }

  if (callback) {
    callback->ActorDestroyed(mID);
  }
}

void IPCBlobInputStreamParent::SetCallback(
    IPCBlobInputStreamParentCallback* aCallback) {
  MOZ_ASSERT(aCallback);
  MOZ_ASSERT(!mCallback);

  mCallback = aCallback;
}

mozilla::ipc::IPCResult IPCBlobInputStreamParent::RecvStreamNeeded() {
  MOZ_ASSERT(mContentManager || mPBackgroundManager);

  nsCOMPtr<nsIInputStream> stream;
  IPCBlobInputStreamStorage::Get()->GetStream(mID, 0, mSize,
                                              getter_AddRefs(stream));
  if (!stream) {
    if (!SendStreamReady(Nothing())) {
      return IPC_FAIL(this, "SendStreamReady failed");
    }

    return IPC_OK();
  }

  mozilla::ipc::AutoIPCStream ipcStream;
  bool ok = false;

  if (mContentManager) {
    MOZ_ASSERT(NS_IsMainThread());
    ok = ipcStream.Serialize(stream, mContentManager);
  } else {
    MOZ_ASSERT(mPBackgroundManager);
    ok = ipcStream.Serialize(stream, mPBackgroundManager);
  }

  if (NS_WARN_IF(!ok)) {
    return IPC_FAIL(this, "SendStreamReady failed");
  }

  if (!SendStreamReady(Some(ipcStream.TakeValue()))) {
    return IPC_FAIL(this, "SendStreamReady failed");
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult IPCBlobInputStreamParent::RecvLengthNeeded() {
  MOZ_ASSERT(mContentManager || mPBackgroundManager);

  nsCOMPtr<nsIInputStream> stream;
  IPCBlobInputStreamStorage::Get()->GetStream(mID, 0, mSize,
                                              getter_AddRefs(stream));
  if (!stream) {
    if (!SendLengthReady(-1)) {
      return IPC_FAIL(this, "SendLengthReady failed");
    }

    return IPC_OK();
  }

  int64_t length = -1;
  if (InputStreamLengthHelper::GetSyncLength(stream, &length)) {
    Unused << SendLengthReady(length);
    return IPC_OK();
  }

  RefPtr<IPCBlobInputStreamParent> self = this;
  InputStreamLengthHelper::GetAsyncLength(stream, [self](int64_t aLength) {
    if (self->mContentManager || self->mPBackgroundManager) {
      Unused << self->SendLengthReady(aLength);
    }
  });

  return IPC_OK();
}

mozilla::ipc::IPCResult IPCBlobInputStreamParent::RecvClose() {
  MOZ_ASSERT(mContentManager || mPBackgroundManager);

  Unused << Send__delete__(this);
  return IPC_OK();
}

mozilla::ipc::IPCResult IPCBlobInputStreamParent::Recv__delete__() {
  MOZ_ASSERT(mContentManager || mPBackgroundManager);
  mMigrating = true;
  return IPC_OK();
}

bool IPCBlobInputStreamParent::HasValidStream() const {
  nsCOMPtr<nsIInputStream> stream;
  IPCBlobInputStreamStorage::Get()->GetStream(mID, 0, mSize,
                                              getter_AddRefs(stream));
  return !!stream;
}

}  // namespace dom
}  // namespace mozilla
