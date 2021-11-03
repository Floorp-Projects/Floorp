/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SystemTime.h"

namespace mozilla {
TimeStamp WebrtcSystemTimeBase() {
  static TimeStamp base = TimeStamp::Now();
  return base;
}

webrtc::Timestamp WebrtcSystemTime() {
  const TimeStamp base = WebrtcSystemTimeBase();
  const TimeStamp now = TimeStamp::Now();
  return webrtc::Timestamp::Micros((now - base).ToMicroseconds());
}
}  // namespace mozilla

namespace rtc {
int64_t SystemTimeNanos() { return mozilla::WebrtcSystemTime().us() * 1000; }
}  // namespace rtc
