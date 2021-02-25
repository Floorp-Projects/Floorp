/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RtpPacketQueue_h
#define RtpPacketQueue_h

#include "MediaConduitInterface.h"
#include "nsTArray.h"

namespace mozilla {

class RtpPacketQueue {
 public:
  void Clear() {
    mQueuedPackets.Clear();
    mQueueActive = false;
  }

  void DequeueAll(MediaSessionConduit* conduit) {
    // SSRC is set; insert queued packets
    for (auto& packet : mQueuedPackets) {
      conduit->DeliverPacket(std::move(packet),
                             MediaSessionConduit::PacketType::RTP);
    }
    mQueuedPackets.Clear();
    mQueueActive = false;
  }

  void Enqueue(rtc::CopyOnWriteBuffer packet) {
    mQueuedPackets.AppendElement(std::move(packet));
    mQueueActive = true;
  }

  bool IsQueueActive() { return mQueueActive; }

 private:
  bool mQueueActive = false;
  nsTArray<rtc::CopyOnWriteBuffer> mQueuedPackets;
};

}  // namespace mozilla

#endif  // RtpPacketQueue_h
