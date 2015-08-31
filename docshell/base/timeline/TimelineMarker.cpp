/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TimelineMarker.h"

namespace mozilla {

TimelineMarker::TimelineMarker(const char* aName,
                               TracingMetadata aMetaData,
                               TimelineStackRequest aStackRequest)
  : mName(aName)
  , mMetaData(aMetaData)
{
  MOZ_COUNT_CTOR(TimelineMarker);
  MOZ_ASSERT(aName);

  SetCurrentTime();
  CaptureStackIfNecessary(aMetaData, aStackRequest);
}

TimelineMarker::TimelineMarker(const char* aName,
                               const TimeStamp& aTime,
                               TracingMetadata aMetaData,
                               TimelineStackRequest aStackRequest)
  : mName(aName)
  , mMetaData(aMetaData)
{
  MOZ_COUNT_CTOR(TimelineMarker);
  MOZ_ASSERT(aName);

  SetCustomTime(aTime);
  CaptureStackIfNecessary(aMetaData, aStackRequest);
}

TimelineMarker::~TimelineMarker()
{
  MOZ_COUNT_DTOR(TimelineMarker);
}

void
TimelineMarker::SetCurrentTime()
{
  TimeStamp now = TimeStamp::Now();
  SetCustomTime(now);
}

void
TimelineMarker::SetCustomTime(const TimeStamp& aTime)
{
  bool isInconsistent = false;
  mTime = (aTime - TimeStamp::ProcessCreation(isInconsistent)).ToMilliseconds();
}

void
TimelineMarker::CaptureStackIfNecessary(TracingMetadata aMetaData,
                                        TimelineStackRequest aStackRequest)
{
  if ((aMetaData == TRACING_INTERVAL_START ||
      aMetaData == TRACING_TIMESTAMP) &&
      aStackRequest != NO_STACK) {
    CaptureStack();
  }
}

} // namespace mozilla
