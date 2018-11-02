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

class MOZ_RAII AutoProfilerStyleMarker
{
public:
  explicit AutoProfilerStyleMarker(UniqueProfilerBacktrace aCause)
    : mActive(profiler_is_active())
    , mStartTime(TimeStamp::Now())
    , mCause(std::move(aCause))
  {
    if (!mActive) {
      return;
    }
    MOZ_ASSERT(!ServoTraversalStatistics::sActive,
               "Nested AutoProfilerStyleMarker");
    ServoTraversalStatistics::sSingleton = ServoTraversalStatistics();
    ServoTraversalStatistics::sActive = true;
  }

  ~AutoProfilerStyleMarker()
  {
    if (!mActive) {
      return;
    }
    ServoTraversalStatistics::sActive = false;
    profiler_add_marker("Styles", MakeUnique<StyleMarkerPayload>(
      mStartTime, TimeStamp::Now(), std::move(mCause),
      ServoTraversalStatistics::sSingleton));
  }

private:
  bool mActive;
  TimeStamp mStartTime;
  UniqueProfilerBacktrace mCause;
};

} // namespace mozilla

#endif // mozilla_AutoProfilerStyleMarker_h
