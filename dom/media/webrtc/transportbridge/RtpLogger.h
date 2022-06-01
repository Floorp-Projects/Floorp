/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: nohlmeier@mozilla.com

#ifndef rtplogger_h__
#define rtplogger_h__

#include "transport/mediapacket.h"

namespace mozilla {

/* This class logs RTP and RTCP packets in hex in a format compatible to
 * text2pcap.
 * Example to convert the MOZ log file into a PCAP file:
 *   egrep '(RTP_PACKET|RTCP_PACKET)' moz.log | \
 *     text2pcap -D -n -l 1 -i 17 -u 1234,1235 -t '%H:%M:%S.' - rtp.pcap
 */
class RtpLogger {
 public:
  static bool IsPacketLoggingOn();
  static void LogPacket(const MediaPacket& packet, bool input,
                        std::string desc);
};

}  // namespace mozilla
#endif
