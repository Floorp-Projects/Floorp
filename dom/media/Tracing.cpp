/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Tracing.h"

#include <inttypes.h>
#include <cstdio>

#include "AsyncLogger.h"
#include "mozilla/TimeStamp.h"

using namespace mozilla;

uint64_t
AutoTracer::NowInUs()
{
  static TimeStamp base = TimeStamp::Now();
  return (TimeStamp::Now() - base).ToMicroseconds();
}

void
AutoTracer::PrintEvent(const char* aName,
                       const char* aCategory,
                       const char* aComment,
                       TracingPhase aPhase,
                       uint64_t aTime,
                       uint64_t aPID,
                       uint64_t aThread)
{
  mLogger.Log("{\"name\": \"%s\", \"cat\": \"%s\", \"ph\": \"%c\","
              "\"ts\": %" PRIu64 ", \"pid\": %" PRIu64 ", \"tid\":"
              " %" PRIu64 ", \"args\": { \"comment\": \"%s\"}},",
           aName, aCategory, TRACING_PHASE_STRINGS[static_cast<int>(aPhase)],
           aTime, aPID, aThread, aComment);
}

void
AutoTracer::PrintBudget(const char* aName,
                        const char* aCategory,
                        uint64_t aDuration,
                        uint64_t aPID,
                        uint64_t aThread,
                        uint64_t aFrames)
{
  mLogger.Log("{\"name\": \"%s\", \"cat\": \"%s\", \"ph\": \"X\","
              "\"ts\": %" PRIu64 ", \"dur\": %" PRIu64 ", \"pid\": %" PRIu64 ","
              "\"tid\": %" PRIu64 ", \"args\": { \"comment\": %" PRIu64 "}},",
              aName, aCategory, NowInUs(), aDuration, aPID, aThread, aFrames);
}

AutoTracer::AutoTracer(AsyncLogger& aLogger,
                       const char* aLocation,
                       uint64_t aPID,
                       uint64_t aTID,
                       EventType aEventType,
                       uint64_t aFrames,
                       uint64_t aSampleRate)
  : mLogger(aLogger)
  , mLocation(aLocation)
  , mComment(nullptr)
  , mEventType(aEventType)
  , mPID(aPID)
  , mTID(aTID)
{
  MOZ_ASSERT(aEventType == EventType::BUDGET);

  if (aLogger.Enabled()) {
    float durationUS = (static_cast<float>(aFrames) / aSampleRate) * 1e6;
    PrintBudget(aLocation, "perf", durationUS, mPID, mTID, aFrames, aSampleRate);
  }
}

AutoTracer::AutoTracer(AsyncLogger& aLogger,
                       const char* aLocation,
                       uint64_t aPID,
                       uint64_t aTID,
                       EventType aEventType,
                       const char* aComment)
  : mLogger(aLogger)
  , mLocation(aLocation)
  , mComment(aComment)
  , mEventType(aEventType)
  , mPID(aPID)
  , mTID(aTID)
{
  MOZ_ASSERT(aEventType == EventType::DURATION);
  if (aLogger.Enabled()) {
    PrintEvent(aLocation, "perf", mComment, TracingPhase::BEGIN, NowInUs(), aPID, aTID);
  }
}

AutoTracer::~AutoTracer()
{
  if (mEventType == EventType::DURATION) {
    if (mLogger.Enabled()) {
      PrintEvent(mLocation, "perf", mComment, TracingPhase::END, NowInUs(), mPID, mTID);
    }
  }
}
