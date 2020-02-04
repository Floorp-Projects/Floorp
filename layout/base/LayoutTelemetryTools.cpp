/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layout/LayoutTelemetryTools.h"

#include "MainThreadUtils.h"
#include "mozilla/Atomics.h"
#include "mozilla/Telemetry.h"

using namespace mozilla;
using namespace mozilla::layout_telemetry;

// Returns the key name expected by telemetry. Keep to date with
// toolkits/components/telemetry/Histograms.json.
static nsLiteralCString SubsystemTelemetryKey(LayoutSubsystem aSubsystem) {
  switch (aSubsystem) {
    default:
      MOZ_CRASH("Unexpected LayoutSubsystem value");
    case LayoutSubsystem::Restyle:
      return nsLiteralCString("Restyle");
    case LayoutSubsystem::Reflow:
      return nsLiteralCString("ReflowOther");
    case LayoutSubsystem::ReflowFlex:
      return nsLiteralCString("ReflowFlex");
    case LayoutSubsystem::ReflowGrid:
      return nsLiteralCString("ReflowGrid");
    case LayoutSubsystem::ReflowTable:
      return nsLiteralCString("ReflowTable");
    case LayoutSubsystem::ReflowText:
      return nsLiteralCString("ReflowText");
  }
}

static AutoRecord* sCurrentRecord;

static FlushKind ToKind(FlushType aFlushType) {
  switch (aFlushType) {
    default:
      MOZ_CRASH("Expected FlushType::Style or FlushType::Layout");
    case FlushType::Style:
      return FlushKind::Style;
    case FlushType::Layout:
      return FlushKind::Layout;
  }
}

namespace mozilla {
namespace layout_telemetry {

Data::Data() {
  PodZero(&mReqsPerFlush);
  PodZero(&mFlushesPerTick);
  PodZero(&mLayoutSubsystemDurationMs);
}

void Data::IncReqsPerFlush(FlushType aFlushType) {
  mReqsPerFlush[ToKind(aFlushType)]++;
}

void Data::IncFlushesPerTick(FlushType aFlushType) {
  mFlushesPerTick[ToKind(aFlushType)]++;
}

void Data::PingReqsPerFlushTelemetry(FlushType aFlushType) {
  auto flushKind = ToKind(aFlushType);
  if (flushKind == FlushKind::Layout) {
    auto styleFlushReqs = mReqsPerFlush[FlushKind::Style].value();
    auto layoutFlushReqs = mReqsPerFlush[FlushKind::Layout].value();
    Telemetry::Accumulate(Telemetry::PRESSHELL_REQS_PER_LAYOUT_FLUSH,
                          NS_LITERAL_CSTRING("Style"), styleFlushReqs);
    Telemetry::Accumulate(Telemetry::PRESSHELL_REQS_PER_LAYOUT_FLUSH,
                          NS_LITERAL_CSTRING("Layout"), layoutFlushReqs);
    mReqsPerFlush[FlushKind::Style] = SaturateUint8(0);
    mReqsPerFlush[FlushKind::Layout] = SaturateUint8(0);
  } else {
    auto styleFlushReqs = mReqsPerFlush[FlushKind::Style].value();
    Telemetry::Accumulate(Telemetry::PRESSHELL_REQS_PER_STYLE_FLUSH,
                          styleFlushReqs);
    mReqsPerFlush[FlushKind::Style] = SaturateUint8(0);
  }
}

void Data::PingFlushPerTickTelemetry(FlushType aFlushType) {
  auto flushKind = ToKind(aFlushType);
  auto styleFlushes = mFlushesPerTick[FlushKind::Style].value();
  if (styleFlushes > 0) {
    Telemetry::Accumulate(Telemetry::PRESSHELL_FLUSHES_PER_TICK,
                          NS_LITERAL_CSTRING("Style"), styleFlushes);
    mFlushesPerTick[FlushKind::Style] = SaturateUint8(0);
  }

  auto layoutFlushes = mFlushesPerTick[FlushKind::Layout].value();
  if (flushKind == FlushKind::Layout && layoutFlushes > 0) {
    Telemetry::Accumulate(Telemetry::PRESSHELL_FLUSHES_PER_TICK,
                          NS_LITERAL_CSTRING("Layout"), layoutFlushes);
    mFlushesPerTick[FlushKind::Layout] = SaturateUint8(0);
  }
}

void Data::PingTotalMsPerTickTelemetry(FlushType aFlushType) {
  auto flushKind = ToKind(aFlushType);
  auto range = (flushKind == FlushKind::Style)
                   ? MakeEnumeratedRange(LayoutSubsystem::Restyle,
                                         LayoutSubsystem::Reflow)
                   : MakeEnumeratedRange(LayoutSubsystem::Reflow,
                                         LayoutSubsystem::Count);

  for (auto subsystem : range) {
    auto key = SubsystemTelemetryKey(subsystem);
    double& duration = mLayoutSubsystemDurationMs[subsystem];
    if (duration > 0.0) {
      Telemetry::Accumulate(Telemetry::PRESSHELL_LAYOUT_TOTAL_MS_PER_TICK, key,
                            static_cast<uint32_t>(duration));
      duration = 0.0;
    }
  }
}

void Data::PingPerTickTelemetry(FlushType aFlushType) {
  PingFlushPerTickTelemetry(aFlushType);
  PingTotalMsPerTickTelemetry(aFlushType);
}

AutoRecord::AutoRecord(LayoutSubsystem aSubsystem)
    : AutoRecord(nullptr, aSubsystem) {}

AutoRecord::AutoRecord(Data* aLayoutTelemetry, LayoutSubsystem aSubsystem)
    : mParentRecord(sCurrentRecord),
      mLayoutTelemetry(aLayoutTelemetry),
      mSubsystem(aSubsystem),
      mStartTime(TimeStamp::Now()),
      mDurationMs(0.0) {
  MOZ_ASSERT(NS_IsMainThread());

  // If we're re-entering the same subsystem, don't update the current record.
  if (mParentRecord) {
    if (mParentRecord->mSubsystem == mSubsystem) {
      return;
    }

    mLayoutTelemetry = mParentRecord->mLayoutTelemetry;
    MOZ_ASSERT(mLayoutTelemetry);

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
  mLayoutTelemetry->mLayoutSubsystemDurationMs[mSubsystem] += mDurationMs;

  if (mParentRecord) {
    // Restart the parent recording from this point
    mParentRecord->mStartTime = now;
  }

  // Unlink this record from the current record list
  sCurrentRecord = mParentRecord;
}

}  // namespace layout_telemetry
}  // namespace mozilla
