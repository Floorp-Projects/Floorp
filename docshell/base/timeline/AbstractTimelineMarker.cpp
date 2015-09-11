/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AbstractTimelineMarker.h"
#include "mozilla/TimeStamp.h"

namespace mozilla {

AbstractTimelineMarker::AbstractTimelineMarker(const char* aName,
                                               MarkerTracingType aTracingType)
  : mName(aName)
  , mTracingType(aTracingType)
{
  MOZ_COUNT_CTOR(AbstractTimelineMarker);
  SetCurrentTime();
}

AbstractTimelineMarker::AbstractTimelineMarker(const char* aName,
                                               const TimeStamp& aTime,
                                               MarkerTracingType aTracingType)
  : mName(aName)
  , mTracingType(aTracingType)
{
  MOZ_COUNT_CTOR(AbstractTimelineMarker);
  SetCustomTime(aTime);
}

UniquePtr<AbstractTimelineMarker>
AbstractTimelineMarker::Clone()
{
  MOZ_ASSERT(false, "Clone method not yet implemented on this marker type.");
  return nullptr;
}

AbstractTimelineMarker::~AbstractTimelineMarker()
{
  MOZ_COUNT_DTOR(AbstractTimelineMarker);
}

void
AbstractTimelineMarker::SetCurrentTime()
{
  TimeStamp now = TimeStamp::Now();
  SetCustomTime(now);
}

void
AbstractTimelineMarker::SetCustomTime(const TimeStamp& aTime)
{
  bool isInconsistent = false;
  mTime = (aTime - TimeStamp::ProcessCreation(isInconsistent)).ToMilliseconds();
}

} // namespace mozilla
