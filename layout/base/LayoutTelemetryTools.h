/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Tools for collecting and reporting layout and style telemetry */

#ifndef mozilla_LayoutTelemetryTools_h
#define mozilla_LayoutTelemetryTools_h

#include "mozilla/TimeStamp.h"

#define LAYOUT_TELEMETRY_RECORD(subsystem) \
  mozilla::layout_telemetry::AutoRecord a( \
      mozilla::layout_telemetry::LayoutSubsystem::subsystem)

namespace mozilla::layout_telemetry {

// Send the current per-tick telemetry.
void PingPerTickTelemetry();

enum class LayoutSubsystem : uint8_t {
  Restyle,
  Reflow,
  ReflowFlex,
  ReflowGrid,
  ReflowTable,
  ReflowText,
  Count
};

class AutoRecord {
 public:
  explicit AutoRecord(LayoutSubsystem aSubsystem);
  ~AutoRecord();

 private:
  AutoRecord* const mParentRecord;
  const LayoutSubsystem mSubsystem;
  TimeStamp mStartTime;
  double mDurationMs = 0.0;
};

}  // namespace mozilla::layout_telemetry

#endif  // !mozilla_LayoutTelemetryTools_h
