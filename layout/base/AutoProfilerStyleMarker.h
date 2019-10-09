/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_AutoProfilerStyleMarker_h
#define mozilla_AutoProfilerStyleMarker_h

#include "mozilla/Attributes.h"
#include "mozilla/ServoTraversalStatistics.h"
#include "mozilla/TimeStamp.h"
#include "GeckoProfiler.h"
#include "ProfilerMarkerPayload.h"

namespace mozilla {

class MOZ_RAII AutoProfilerStyleMarker {
 public:
  explicit AutoProfilerStyleMarker(UniqueProfilerBacktrace aCause,
                                   const Maybe<uint64_t>& aInnerWindowID)
      : mActive(profiler_can_accept_markers()),
        mStartTime(TimeStamp::Now()),
        mCause(std::move(aCause)),
        mInnerWindowID(aInnerWindowID) {
    if (!mActive) {
      return;
    }
    MOZ_ASSERT(!ServoTraversalStatistics::sActive,
               "Nested AutoProfilerStyleMarker");
    ServoTraversalStatistics::sSingleton = ServoTraversalStatistics();
    ServoTraversalStatistics::sActive = true;
  }

  ~AutoProfilerStyleMarker() {
    if (!mActive) {
      return;
    }
    ServoTraversalStatistics::sActive = false;
    PROFILER_ADD_MARKER_WITH_PAYLOAD(
        "Styles", LAYOUT, StyleMarkerPayload,
        (mStartTime, TimeStamp::Now(), std::move(mCause),
         ServoTraversalStatistics::sSingleton, mInnerWindowID));
  }

 private:
  bool mActive;
  TimeStamp mStartTime;
  UniqueProfilerBacktrace mCause;
  Maybe<uint64_t> mInnerWindowID;
};

}  // namespace mozilla

#endif  // mozilla_AutoProfilerStyleMarker_h
