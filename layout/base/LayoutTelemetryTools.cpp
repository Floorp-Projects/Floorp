/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layout/LayoutTelemetryTools.h"

#include "MainThreadUtils.h"
#include "mozilla/EnumeratedArray.h"
#include "mozilla/EnumeratedRange.h"
#include "mozilla/Telemetry.h"

namespace mozilla::layout_telemetry {

using LayoutSubsystemDurations =
    EnumeratedArray<LayoutSubsystem, double, size_t(LayoutSubsystem::Count)>;

struct PerTickData {
  constexpr PerTickData() = default;
  LayoutSubsystemDurations mLayoutSubsystemDurationMs{0.0, 0.0, 0.0,
                                                      0.0, 0.0, 0.0};
};

static PerTickData sData;
static AutoRecord* sCurrentRecord;

// Returns the key name expected by telemetry. Keep to date with
// toolkits/components/telemetry/Histograms.json.
static nsLiteralCString SubsystemTelemetryKey(LayoutSubsystem aSubsystem) {
  switch (aSubsystem) {
    default:
      MOZ_CRASH("Unexpected LayoutSubsystem value");
    case LayoutSubsystem::Restyle:
      return "Restyle"_ns;
    case LayoutSubsystem::Reflow:
      return "ReflowOther"_ns;
    case LayoutSubsystem::ReflowFlex:
      return "ReflowFlex"_ns;
    case LayoutSubsystem::ReflowGrid:
      return "ReflowGrid"_ns;
    case LayoutSubsystem::ReflowTable:
      return "ReflowTable"_ns;
    case LayoutSubsystem::ReflowText:
      return "ReflowText"_ns;
  }
}

void PingPerTickTelemetry() {
  auto range =
      MakeEnumeratedRange(LayoutSubsystem::Restyle, LayoutSubsystem::Count);
  for (auto subsystem : range) {
    auto key = SubsystemTelemetryKey(subsystem);
    double& duration = sData.mLayoutSubsystemDurationMs[subsystem];
    if (duration > 0.0) {
      Telemetry::Accumulate(Telemetry::PRESSHELL_LAYOUT_TOTAL_MS_PER_TICK, key,
                            static_cast<uint32_t>(duration));
      duration = 0.0;
    }
  }
}

AutoRecord::AutoRecord(LayoutSubsystem aSubsystem)
    : mParentRecord(sCurrentRecord), mSubsystem(aSubsystem) {
  MOZ_ASSERT(NS_IsMainThread());
  // If we're re-entering the same subsystem, don't update the current record.
  if (mParentRecord && mParentRecord->mSubsystem == mSubsystem) {
    return;
  }

  mStartTime = TimeStamp::Now();
  if (mParentRecord) {
    // If we're entering a new subsystem, record the amount of time spent in the
    // parent record before setting the new current record.
    mParentRecord->mDurationMs +=
        (mStartTime - mParentRecord->mStartTime).ToMilliseconds();
  }

  sCurrentRecord = this;
}

AutoRecord::~AutoRecord() {
  if (sCurrentRecord != this) {
    // If this record is not head of the list, do nothing.
    return;
  }

  TimeStamp now = TimeStamp::Now();
  mDurationMs += (now - mStartTime).ToMilliseconds();
  sData.mLayoutSubsystemDurationMs[mSubsystem] += mDurationMs;

  if (mParentRecord) {
    // Restart the parent recording from this point
    mParentRecord->mStartTime = now;
  }

  // Unlink this record from the current record list
  sCurrentRecord = mParentRecord;
}

}  // namespace mozilla::layout_telemetry
