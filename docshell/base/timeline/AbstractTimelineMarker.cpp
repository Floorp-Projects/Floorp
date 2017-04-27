/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AbstractTimelineMarker.h"

#include "mozilla/TimeStamp.h"
#include "MainThreadUtils.h"
#include "nsAppRunner.h"

namespace mozilla {

AbstractTimelineMarker::AbstractTimelineMarker(const char* aName,
                                               MarkerTracingType aTracingType)
  : mName(aName)
  , mTracingType(aTracingType)
  , mProcessType(XRE_GetProcessType())
  , mIsOffMainThread(!NS_IsMainThread())
{
  MOZ_COUNT_CTOR(AbstractTimelineMarker);
  SetCurrentTime();
}

AbstractTimelineMarker::AbstractTimelineMarker(const char* aName,
                                               const TimeStamp& aTime,
                                               MarkerTracingType aTracingType)
  : mName(aName)
  , mTracingType(aTracingType)
  , mProcessType(XRE_GetProcessType())
  , mIsOffMainThread(!NS_IsMainThread())
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

bool
AbstractTimelineMarker::Equals(const AbstractTimelineMarker& aOther)
{
  // Check whether two markers should be considered the same, for the purpose
  // of pairing start and end markers. Normally this definition suffices.
  return strcmp(mName, aOther.mName) == 0;
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
  mTime = (aTime - TimeStamp::ProcessCreation()).ToMilliseconds();
}

void
AbstractTimelineMarker::SetCustomTime(DOMHighResTimeStamp aTime)
{
  mTime = aTime;
}

void
AbstractTimelineMarker::SetProcessType(GeckoProcessType aProcessType)
{
  mProcessType = aProcessType;
}

void
AbstractTimelineMarker::SetOffMainThread(bool aIsOffMainThread)
{
  mIsOffMainThread = aIsOffMainThread;
}

} // namespace mozilla
