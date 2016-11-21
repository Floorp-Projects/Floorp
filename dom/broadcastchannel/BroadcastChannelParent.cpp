/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BroadcastChannelParent.h"
#include "BroadcastChannelParentMessage.h"
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
  , mStatus(eInitializing)
{
  AssertIsOnBackgroundThread();
  mService->RegisterActor(this, mOriginChannelKey);
}

BroadcastChannelParent::~BroadcastChannelParent()
{
  AssertIsOnBackgroundThread();
}

void
BroadcastChannelParent::Start()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mStatus == eInitializing || mStatus == eClosing);

  // Flushing the pending messages.
  for (uint32_t i = 0; i < mPendingMessages.Length(); ++i) {
    mService->PostMessage(this, mPendingMessages[i], mOriginChannelKey);
  }
  mPendingMessages.Clear();

  // Flushing the delivering messages.
  for (uint32_t i = 0; i < mDeliveringMessages.Length(); ++i) {
    DeliverInternal(mDeliveringMessages[i]);
  }
  mDeliveringMessages.Clear();

  // Maybe we have to send __delete__.
  if (mStatus == eClosing) {
    Shutdown();
    return;
  }

  // We are up and running.
  mStatus = eInitialized;
}

void
BroadcastChannelParent::Shutdown()
{
  AssertIsOnBackgroundThread();

  if (mService) {
    mService->UnregisterActor(this, mOriginChannelKey);
    mService = nullptr;
  }

  Unused << Send__delete__(this);
  mStatus = eDestroyed;
}

mozilla::ipc::IPCResult
BroadcastChannelParent::RecvPostMessage(const ClonedMessageData& aData)
{
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(!mService)) {
    return IPC_FAIL_NO_REASON(this);
  }

  // This should not happen: postMessage should be received only when the actor
  // is still active.
  if (mStatus == eDestroyed) {
    return IPC_FAIL_NO_REASON(this);
  }

  RefPtr<BroadcastChannelParentMessage> msg =
    new BroadcastChannelParentMessage(aData);

  // If not initialized yet, we store the msg in the pending queue.
  if (mStatus == eInitializing) {

    mPendingMessages.AppendElement(msg);
    return IPC_OK();
  }

  // Nothing to do here. We can ignore the msg.
  if (mStatus == eClosing) {
    return IPC_OK();
  }

  // Let's broadcast the message.
  MOZ_ASSERT(mStatus == eInitialized);

  if (NS_WARN_IF(!mPendingMessages.IsEmpty())) {
    return IPC_FAIL_NO_REASON(this);
  }

  mService->PostMessage(this, msg, mOriginChannelKey);
  return IPC_OK();
}

mozilla::ipc::IPCResult
BroadcastChannelParent::RecvClose()
{
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(!mService)) {
    return IPC_FAIL_NO_REASON(this);
  }

  // This should not happen: Close() should be called only when the actor is
  // still active.
  if (mStatus == eDestroyed || mStatus == eClosing) {
    return IPC_FAIL_NO_REASON(this);
  }

  // We are not initialized yet.
  if (mStatus == eInitializing) {
    mStatus = eClosing;
    return IPC_OK();
  }

  MOZ_ASSERT(mStatus == eInitialized);

  // If we are initialized, we don't have any pending nor delivering messages in
  // queue.
  if (NS_WARN_IF(!mPendingMessages.IsEmpty()) ||
      NS_WARN_IF(!mDeliveringMessages.IsEmpty())) {
    return IPC_FAIL_NO_REASON(this);
  }

  Shutdown();
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

  mStatus = eDestroyed;
}

void
BroadcastChannelParent::Deliver(BroadcastChannelParentMessage* aMsg)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aMsg);

  // We are not initialized yet, Let's store this message.
  if (mStatus == eInitializing) {
    RefPtr<BroadcastChannelParentMessage> msg =
      new BroadcastChannelParentMessage(aMsg->Data());
    mDeliveringMessages.AppendElement(msg);
    return;
  }

  // We are able to dispatch messages.
  if (mStatus == eInitialized) {
    DeliverInternal(aMsg);
    return;
  }

  // We don't care about the other states.
}

void
BroadcastChannelParent::DeliverInternal(BroadcastChannelParentMessage* aMsg)
{
  ClonedMessageData newData(aMsg->Data());

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
