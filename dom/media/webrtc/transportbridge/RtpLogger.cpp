/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: nohlmeier@mozilla.com

#include "RtpLogger.h"
#include "mozilla/Logging.h"

#include <ctime>
#include <iomanip>
#include <sstream>
#ifdef _WIN32
#  include <time.h>
#  include <sys/timeb.h>
#else
#  include <sys/time.h>
#endif

// Logging context
using namespace mozilla;

mozilla::LazyLogModule gRtpLoggerLog("RtpLogger");

namespace mozilla {

bool RtpLogger::IsPacketLoggingOn() {
  return MOZ_LOG_TEST(gRtpLoggerLog, LogLevel::Debug);
}

void RtpLogger::LogPacket(const MediaPacket& packet, bool input,
                          std::string desc) {
  if (MOZ_LOG_TEST(gRtpLoggerLog, LogLevel::Debug)) {
    bool isRtp = (packet.type() == MediaPacket::RTP);
    std::stringstream ss;
    /* This creates text2pcap compatible format, e.g.:
     *  RTCP_PACKET O 10:36:26.864934  000000 80 c8 00 06 6d ...
     */
    ss << (input ? "I " : "O ");
    std::time_t t = std::time(nullptr);
    std::tm tm = *std::localtime(&t);
    char buf[9];
    if (0 < strftime(buf, sizeof(buf), "%H:%M:%S", &tm)) {
      ss << buf;
    }
    ss << std::setfill('0');
#ifdef _WIN32
    struct timeb tb;
    ftime(&tb);
    ss << "." << (tb.millitm) << " ";
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    ss << "." << (tv.tv_usec) << " ";
#endif
    ss << " 000000";
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < packet.len(); ++i) {
      ss << " " << std::setw(2) << (int)packet.data()[i];
    }
    MOZ_LOG(gRtpLoggerLog, LogLevel::Debug,
            ("%s %s|>> %s", desc.c_str(),
             (isRtp ? "RTP_PACKET" : "RTCP_PACKET"), ss.str().c_str()));
  }
}

}  // namespace mozilla
