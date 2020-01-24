/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BroadcastChannelChild.h"
#include "BroadcastChannel.h"

namespace mozilla {

using namespace ipc;

namespace dom {

BroadcastChannelChild::BroadcastChannelChild()
    : mBC(nullptr), mActorDestroyed(false) {}

BroadcastChannelChild::~BroadcastChannelChild() { MOZ_ASSERT(!mBC); }

mozilla::ipc::IPCResult BroadcastChannelChild::RecvNotify(
    const MessageData& aData) {
  if (!mBC) {
    // The object is going to be deleted soon. No notify is required.
    return IPC_OK();
  }

  mBC->MessageReceived(aData);
  return IPC_OK();
}

mozilla::ipc::IPCResult BroadcastChannelChild::RecvRefMessageDelivered(
    const nsID& aMessageID, const uint32_t& aOtherBCs) {
  if (!mBC) {
    // The object is going to be deleted soon. No notify is required.
    return IPC_OK();
  }

  mBC->MessageDelivered(aMessageID, aOtherBCs);
  return IPC_OK();
}

void BroadcastChannelChild::ActorDestroy(ActorDestroyReason aWhy) {
  mActorDestroyed = true;
}

}  // namespace dom
}  // namespace mozilla
