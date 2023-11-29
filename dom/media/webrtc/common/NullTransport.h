/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NULL_TRANSPORT_H_
#define NULL_TRANSPORT_H_

#include "api/call/transport.h"

namespace mozilla {

/**
 * NullTransport is registered as ExternalTransport to throw away data
 */
class NullTransport : public webrtc::Transport {
 public:
  virtual bool SendRtp(rtc::ArrayView<const uint8_t> packet,
                       const webrtc::PacketOptions& options) {
    (void)packet;
    (void)options;
    return true;
  }

  virtual bool SendRtcp(rtc::ArrayView<const uint8_t> packet) {
    (void)packet;
    return true;
  }
#if 0
  virtual int SendPacket(int channel, const void *data, size_t len)
  {
    (void) channel; (void) data;
    return len;
  }

  virtual int SendRTCPPacket(int channel, const void *data, size_t len)
  {
    (void) channel; (void) data;
    return len;
  }
#endif
  NullTransport() {}

  virtual ~NullTransport() {}

 private:
  NullTransport(const NullTransport& other) = delete;
  void operator=(const NullTransport& other) = delete;
};

}  // namespace mozilla

#endif
