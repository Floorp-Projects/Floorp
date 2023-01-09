/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MIDIPortParent_h
#define mozilla_dom_MIDIPortParent_h

#include "mozilla/dom/PMIDIPortParent.h"
#include "mozilla/dom/MIDIPortBinding.h"
#include "mozilla/dom/MIDIPortInterface.h"

// Header file contents
namespace mozilla::dom {

/**
 * Actor representing the parent (PBackground thread) side of a MIDIPort object.
 *
 */
class MIDIPortParent final : public PMIDIPortParent, public MIDIPortInterface {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MIDIPortParent, override);
  void ActorDestroy(ActorDestroyReason) override;
  mozilla::ipc::IPCResult RecvSend(nsTArray<MIDIMessage>&& aMsg);
  mozilla::ipc::IPCResult RecvOpen();
  mozilla::ipc::IPCResult RecvClose();
  mozilla::ipc::IPCResult RecvClear();
  mozilla::ipc::IPCResult RecvShutdown();
  MOZ_IMPLICIT MIDIPortParent(const MIDIPortInfo& aPortInfo,
                              const bool aSysexEnabled);
  // Sends the current port status to the child actor. May also send message
  // buffer if required.
  bool SendUpdateStatus(const MIDIPortDeviceState& aDeviceState,
                        const MIDIPortConnectionState& aConnectionState);
  uint32_t GetInternalId() const { return mInternalId; }

 protected:
  ~MIDIPortParent() = default;
  // Queue of messages that needs to be sent. Since sending a message on a
  // closed port opens it, we sometimes have to buffer messages from the time
  // Send() is called until the time we get a device state change to Opened.
  nsTArray<MIDIMessage> mMessageQueue;
  const uint32_t mInternalId;
};

}  // namespace mozilla::dom

#endif
