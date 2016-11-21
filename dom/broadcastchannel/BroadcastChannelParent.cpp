/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BroadcastChannelParent.h"
#include "BroadcastChannelService.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/ipc/BlobParent.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/Unused.h"
#include "nsIScriptSecurityManager.h"

namespace mozilla {

using namespace ipc;

namespace dom {

BroadcastChannelParent::BroadcastChannelParent(const nsAString& aOriginChannelKey)
  : mService(BroadcastChannelService::GetOrCreate())
  , mOriginChannelKey(aOriginChannelKey)
{
  AssertIsOnBackgroundThread();
  mService->RegisterActor(this, mOriginChannelKey);
}

BroadcastChannelParent::~BroadcastChannelParent()
{
  AssertIsOnBackgroundThread();
}

mozilla::ipc::IPCResult
BroadcastChannelParent::RecvPostMessage(const ClonedMessageData& aData)
{
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(!mService)) {
    return IPC_FAIL_NO_REASON(this);
  }

  mService->PostMessage(this, aData, mOriginChannelKey);
  return IPC_OK();
}

mozilla::ipc::IPCResult
BroadcastChannelParent::RecvClose()
{
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(!mService)) {
    return IPC_FAIL_NO_REASON(this);
  }

  mService->UnregisterActor(this, mOriginChannelKey);
  mService = nullptr;

  Unused << Send__delete__(this);

  return IPC_OK();
}

void
BroadcastChannelParent::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsOnBackgroundThread();

  if (mService) {
    // This object is about to be released and with it, also mService will be
    // released too.
    mService->UnregisterActor(this, mOriginChannelKey);
  }
}

void
BroadcastChannelParent::Deliver(const ClonedMessageData& aData)
{
  AssertIsOnBackgroundThread();

  // Duplicate the data for this parent.
  ClonedMessageData newData(aData);

  // Create new BlobParent objects for this message.
  for (uint32_t i = 0, len = newData.blobsParent().Length(); i < len; ++i) {
    RefPtr<BlobImpl> impl =
      static_cast<BlobParent*>(newData.blobsParent()[i])->GetBlobImpl();

    PBlobParent* blobParent =
      BackgroundParent::GetOrCreateActorForBlobImpl(Manager(), impl);
    if (!blobParent) {
      return;
    }

    newData.blobsParent()[i] = blobParent;
  }

  Unused << SendNotify(newData);
}

} // namespace dom
} // namespace mozilla
