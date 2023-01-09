/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MIDIPortChild.h"
#include "mozilla/dom/MIDIPort.h"
#include "mozilla/dom/MIDIPortInterface.h"
#include "nsContentUtils.h"

using namespace mozilla;
using namespace mozilla::dom;

MIDIPortChild::MIDIPortChild(const MIDIPortInfo& aPortInfo, bool aSysexEnabled,
                             MIDIPort* aPort)
    : MIDIPortInterface(aPortInfo, aSysexEnabled), mDOMPort(aPort) {}

void MIDIPortChild::ActorDestroy(ActorDestroyReason aWhy) {
  if (mDOMPort) {
    mDOMPort->UnsetIPCPort();
    MOZ_ASSERT(!mDOMPort);
  }
  MIDIPortInterface::Shutdown();
}

mozilla::ipc::IPCResult MIDIPortChild::RecvReceive(
    nsTArray<MIDIMessage>&& aMsgs) {
  if (mDOMPort) {
    mDOMPort->Receive(aMsgs);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult MIDIPortChild::RecvUpdateStatus(
    const uint32_t& aDeviceState, const uint32_t& aConnectionState) {
  // Either a device is connected, and can have any connection state, or a
  // device is disconnected, and can only be closed or pending.
  MOZ_ASSERT(mDeviceState == MIDIPortDeviceState::Connected ||
             (mConnectionState == MIDIPortConnectionState::Closed ||
              mConnectionState == MIDIPortConnectionState::Pending));
  mDeviceState = static_cast<MIDIPortDeviceState>(aDeviceState);
  mConnectionState = static_cast<MIDIPortConnectionState>(aConnectionState);
  if (mDOMPort) {
    mDOMPort->FireStateChangeEvent();
  }
  return IPC_OK();
}

nsresult MIDIPortChild::GenerateStableId(const nsACString& aOrigin) {
  const size_t kIdLength = 64;
  mStableId.SetCapacity(kIdLength);
  mStableId.Append(Name());
  mStableId.Append(Manufacturer());
  mStableId.Append(Version());
  nsContentUtils::AnonymizeId(mStableId, aOrigin,
                              nsContentUtils::OriginFormat::Plain);
  return NS_OK;
}
