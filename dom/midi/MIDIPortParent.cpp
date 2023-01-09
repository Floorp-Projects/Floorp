/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MIDIPortParent.h"
#include "mozilla/dom/MIDIPlatformService.h"
#include "nsContentUtils.h"

// C++ file contents
namespace mozilla::dom {

// Keep an internal ID that we can use for passing information about specific
// MIDI ports back and forth to the Rust libraries.
static uint32_t gId = 0;

mozilla::ipc::IPCResult MIDIPortParent::RecvSend(
    nsTArray<MIDIMessage>&& aMsgs) {
  if (mConnectionState != MIDIPortConnectionState::Open) {
    mMessageQueue.AppendElements(aMsgs);
    if (MIDIPlatformService::IsRunning()) {
      MIDIPlatformService::Get()->Open(this);
    }
    return IPC_OK();
  }
  if (MIDIPlatformService::IsRunning()) {
    MIDIPlatformService::Get()->QueueMessages(MIDIPortInterface::mId, aMsgs);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult MIDIPortParent::RecvOpen() {
  if (MIDIPlatformService::IsRunning() &&
      mConnectionState == MIDIPortConnectionState::Closed) {
    MIDIPlatformService::Get()->Open(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult MIDIPortParent::RecvClose() {
  if (mConnectionState != MIDIPortConnectionState::Closed) {
    if (MIDIPlatformService::IsRunning()) {
      MIDIPlatformService::Get()->Close(this);
    }
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult MIDIPortParent::RecvClear() {
  if (MIDIPlatformService::IsRunning()) {
    MIDIPlatformService::Get()->Clear(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult MIDIPortParent::RecvShutdown() {
  if (mShuttingDown) {
    return IPC_OK();
  }
  Close();
  return IPC_OK();
}

void MIDIPortParent::ActorDestroy(ActorDestroyReason) {
  mMessageQueue.Clear();
  MIDIPortInterface::Shutdown();
  if (MIDIPlatformService::IsRunning()) {
    MIDIPlatformService::Get()->RemovePort(this);
  }
}

bool MIDIPortParent::SendUpdateStatus(
    const MIDIPortDeviceState& aDeviceState,
    const MIDIPortConnectionState& aConnectionState) {
  if (mShuttingDown) {
    return true;
  }
  mDeviceState = aDeviceState;
  mConnectionState = aConnectionState;
  if (aConnectionState == MIDIPortConnectionState::Open &&
      aDeviceState == MIDIPortDeviceState::Disconnected) {
    mConnectionState = MIDIPortConnectionState::Pending;
  } else if (aConnectionState == MIDIPortConnectionState::Open &&
             aDeviceState == MIDIPortDeviceState::Connected &&
             !mMessageQueue.IsEmpty()) {
    if (MIDIPlatformService::IsRunning()) {
      MIDIPlatformService::Get()->QueueMessages(MIDIPortInterface::mId,
                                                mMessageQueue);
    }
    mMessageQueue.Clear();
  }
  return PMIDIPortParent::SendUpdateStatus(
      static_cast<uint32_t>(mDeviceState),
      static_cast<uint32_t>(mConnectionState));
}

MIDIPortParent::MIDIPortParent(const MIDIPortInfo& aPortInfo,
                               const bool aSysexEnabled)
    : MIDIPortInterface(aPortInfo, aSysexEnabled), mInternalId(++gId) {
  MOZ_ASSERT(MIDIPlatformService::IsRunning(),
             "Shouldn't be able to add MIDI port without MIDI service!");
  MIDIPlatformService::Get()->AddPort(this);
}

}  // namespace mozilla::dom
