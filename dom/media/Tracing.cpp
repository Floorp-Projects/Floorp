/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Tracing.h"

#include <inttypes.h>

using namespace mozilla;

using TracingPhase = mozilla::AsyncLogger::TracingPhase;

mozilla::AsyncLogger gAudioCallbackTraceLogger;
static std::atomic<int> gTracingStarted(0);

void StartAudioCallbackTracing() {
#ifdef MOZ_REAL_TIME_TRACING
  int cnt = gTracingStarted.fetch_add(1, std::memory_order_seq_cst);
  if (cnt == 0) {
    // This is a noop if the logger has not been enabled.
    gAudioCallbackTraceLogger.Start();
  }
#endif
}

void StopAudioCallbackTracing() {
#ifdef MOZ_REAL_TIME_TRACING
  int cnt = gTracingStarted.fetch_sub(1, std::memory_order_seq_cst);
  if (cnt == 1) {
    // This is a noop if the logger has not been enabled.
    gAudioCallbackTraceLogger.Stop();
  }
#endif
}

void AutoTracer::PrintEvent(const char* aName, const char* aCategory,
                            const char* aComment, TracingPhase aPhase) {
#ifdef MOZ_REAL_TIME_TRACING
  mLogger.Log(aName, aCategory, aComment, aPhase);
#endif
}

void AutoTracer::PrintBudget(const char* aName, const char* aCategory,
                             uint64_t aDuration, uint64_t aFrames,
                             uint64_t aSampleRate) {
#ifdef MOZ_REAL_TIME_TRACING
  mLogger.LogDuration(aName, aCategory, aDuration, aFrames, aSampleRate);
#endif
}

AutoTracer::AutoTracer(AsyncLogger& aLogger, const char* aLocation,
                       EventType aEventType, uint64_t aFrames,
                       uint64_t aSampleRate)
    : mLogger(aLogger),
      mLocation(aLocation),
      mComment(nullptr),
      mEventType(aEventType) {
  MOZ_ASSERT(aEventType == EventType::BUDGET);

  if (aLogger.Enabled()) {
    float durationUS = (static_cast<float>(aFrames) / aSampleRate) * 1e6;
    PrintBudget(aLocation, "perf", durationUS, aFrames, aSampleRate);
  }
}

AutoTracer::AutoTracer(AsyncLogger& aLogger, const char* aLocation,
                       EventType aEventType, const char* aComment)
    : mLogger(aLogger),
      mLocation(aLocation),
      mComment(aComment),
      mEventType(aEventType) {
  MOZ_ASSERT(aEventType == EventType::DURATION);
  if (aLogger.Enabled()) {
    PrintEvent(aLocation, "perf", mComment, AsyncLogger::TracingPhase::BEGIN);
  }
}

AutoTracer::~AutoTracer() {
  if (mEventType == EventType::DURATION) {
    if (mLogger.Enabled()) {
      PrintEvent(mLocation, "perf", mComment, AsyncLogger::TracingPhase::END);
    }
  }
}
