/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_WEBRTC_LIBWEBRTCGLUE_SYSTEMTIMENANOS_H_
#define DOM_MEDIA_WEBRTC_LIBWEBRTCGLUE_SYSTEMTIMENANOS_H_

#include "mozilla/TimeStamp.h"
#include "system_wrappers/include/clock.h"

namespace mozilla {
// Libwebrtc uses WebrtcSystemTime() to track how time advances from a specific
// point in time. It adds an offset to make the timestamps absolute.
TimeStamp WebrtcSystemTimeBase();
webrtc::Timestamp WebrtcSystemTime();

webrtc::NtpTime CreateNtp(webrtc::Timestamp aTime);
}  // namespace mozilla

#endif
