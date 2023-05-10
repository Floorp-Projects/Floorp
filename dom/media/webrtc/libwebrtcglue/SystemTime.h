/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_WEBRTC_LIBWEBRTCGLUE_SYSTEMTIMENANOS_H_
#define DOM_MEDIA_WEBRTC_LIBWEBRTCGLUE_SYSTEMTIMENANOS_H_

#include "jsapi/RTCStatsReport.h"
#include "mozilla/TimeStamp.h"
#include "system_wrappers/include/clock.h"

namespace mozilla {
class RTCStatsTimestampMakerRealtimeClock : public webrtc::Clock {
 public:
  explicit RTCStatsTimestampMakerRealtimeClock(
      const dom::RTCStatsTimestampMaker& aTimestampMaker);

  webrtc::Timestamp CurrentTime() override;

  // Upstream, this method depend on rtc::TimeUTCMicros for converting the
  // monotonic system clock to Ntp, if only for the first call when deciding the
  // Ntp offset.
  // We override this to be able to use our own clock instead of
  // rtc::TimeUTCMicros for ntp timestamps.
  webrtc::NtpTime ConvertTimestampToNtpTime(
      webrtc::Timestamp aRealtime) override;

  const dom::RTCStatsTimestampMaker mTimestampMaker;
};

// The time base used for WebrtcSystemTime(). Completely arbitrary. Constant.
TimeStamp WebrtcSystemTimeBase();

// The returned timestamp denotes the monotonic time passed since
// WebrtcSystemTimeBase(). Libwebrtc uses this to track how time advances from a
// specific point in time. It adds an offset to make the timestamps absolute.
webrtc::Timestamp WebrtcSystemTime();

webrtc::NtpTime CreateNtp(webrtc::Timestamp aTime);
}  // namespace mozilla

#endif
