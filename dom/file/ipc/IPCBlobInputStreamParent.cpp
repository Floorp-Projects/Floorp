/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IPCBlobInputStreamParent.h"
#include "IPCBlobInputStreamStorage.h"
#include "nsContentUtils.h"

namespace mozilla {
namespace dom {

template<typename M>
/* static */ IPCBlobInputStreamParent*
IPCBlobInputStreamParent::Create(nsIInputStream* aInputStream, uint64_t aSize,
                                 nsresult* aRv, M* aManager)
{
  MOZ_ASSERT(aInputStream);
  MOZ_ASSERT(aRv);

  nsID id;
  *aRv = nsContentUtils::GenerateUUIDInPlace(id);
  if (NS_WARN_IF(NS_FAILED(*aRv))) {
    return nullptr;
  }

  IPCBlobInputStreamStorage::Get()->AddStream(aInputStream, id);

  return new IPCBlobInputStreamParent(id, aSize, aManager);
}

IPCBlobInputStreamParent::IPCBlobInputStreamParent(const nsID& aID,
                                                   uint64_t aSize,
                                                   nsIContentParent* aManager)
  : mID(aID)
  , mSize(aSize)
  , mContentManager(aManager)
  , mPBackgroundManager(nullptr)
{}

IPCBlobInputStreamParent::IPCBlobInputStreamParent(const nsID& aID,
                                                   uint64_t aSize,
                                                   PBackgroundParent* aManager)
  : mID(aID)
  , mSize(aSize)
  , mContentManager(nullptr)
  , mPBackgroundManager(aManager)
{}

void
IPCBlobInputStreamParent::ActorDestroy(IProtocol::ActorDestroyReason aReason)
{
  MOZ_ASSERT(mContentManager || mPBackgroundManager);

  mContentManager = nullptr;
  mPBackgroundManager = nullptr;

  IPCBlobInputStreamStorage::Get()->ForgetStream(mID);
}

mozilla::ipc::IPCResult
IPCBlobInputStreamParent::RecvStreamNeeded()
{
  MOZ_ASSERT(mContentManager || mPBackgroundManager);

  nsCOMPtr<nsIInputStream> stream;
  IPCBlobInputStreamStorage::Get()->GetStream(mID, getter_AddRefs(stream));
  if (!stream) {
    if (!SendStreamReady(void_t())) {
      return IPC_FAIL(this, "SendStreamReady failed");
    }

    return IPC_OK();
  }

  AutoIPCStream ipcStream;
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

  if (!SendStreamReady(ipcStream.TakeValue())) {
    return IPC_FAIL(this, "SendStreamReady failed");
  }

  return IPC_OK();
}

} // namespace dom
} // namespace mozilla
