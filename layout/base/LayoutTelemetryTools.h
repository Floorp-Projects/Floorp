/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Tools for collecting and reporting layout and style telemetry */

#ifndef mozilla_LayoutTelemetryTools_h
#define mozilla_LayoutTelemetryTools_h

#include "mozilla/EnumeratedArray.h"
#include "mozilla/EnumeratedRange.h"
#include "mozilla/FlushType.h"
#include "mozilla/Saturate.h"
#include "mozilla/TimeStamp.h"

#define LAYOUT_TELEMETRY_RECORD(subsystem) \
  layout_telemetry::AutoRecord a(layout_telemetry::LayoutSubsystem::subsystem)

#define LAYOUT_TELEMETRY_RECORD_BASE(subsystem)     \
  layout_telemetry::AutoRecord a(&mLayoutTelemetry, \
                                 layout_telemetry::LayoutSubsystem::subsystem)

namespace mozilla {
namespace layout_telemetry {

enum class FlushKind : uint8_t { Style, Layout, Count };

enum class LayoutSubsystem : uint8_t {
  Restyle,
  Reflow,
  ReflowFlex,
  ReflowGrid,
  ReflowTable,
  ReflowText,
  Count
};

using LayoutSubsystemDurations =
    EnumeratedArray<LayoutSubsystem, LayoutSubsystem::Count, double>;
using LayoutFlushCount =
    EnumeratedArray<FlushKind, FlushKind::Count, SaturateUint8>;

struct Data {
  Data();

  void IncReqsPerFlush(FlushType aFlushType);
  void IncFlushesPerTick(FlushType aFlushType);

  void PingTelemetry();

  // Send the current number of flush requests for aFlushType to telemetry and
  // reset the count.
  void PingReqsPerFlushTelemetry(FlushType aFlushType);

  // Send the current non-zero number of style and layout flushes to telemetry
  // and reset the count.
  void PingFlushPerTickTelemetry(FlushType aFlushType);

  // Send the current non-zero time spent under style and layout processing this
  // tick to telemetry and reset the total.
  void PingTotalMsPerTickTelemetry(FlushType aFlushType);

  // Send the current per-tick telemetry for `aFlushType`.
  void PingPerTickTelemetry(FlushType aFlushType);

  LayoutFlushCount mReqsPerFlush;
  LayoutFlushCount mFlushesPerTick;
  LayoutSubsystemDurations mLayoutSubsystemDurationMs;
};

class AutoRecord {
 public:
  explicit AutoRecord(LayoutSubsystem aSubsystem);
  AutoRecord(Data* aLayoutTelemetry, LayoutSubsystem aSubsystem);
  ~AutoRecord();

 private:
  AutoRecord* mParentRecord;
  Data* mLayoutTelemetry;
  LayoutSubsystem mSubsystem;
  TimeStamp mStartTime;
  double mDurationMs;
};

}  // namespace layout_telemetry
}  // namespace mozilla

#endif  // !mozilla_LayoutTelemetryTools_h
