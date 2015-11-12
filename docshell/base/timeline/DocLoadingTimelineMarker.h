/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_DocLoadingTimelineMarker_h_
#define mozilla_DocLoadingTimelineMarker_h_

#include "TimelineMarker.h"
#include "mozilla/dom/ProfileTimelineMarkerBinding.h"

namespace mozilla {

class DocLoadingTimelineMarker : public TimelineMarker
{
public:
  explicit DocLoadingTimelineMarker(const char* aName)
    : TimelineMarker(aName, MarkerTracingType::TIMESTAMP)
    , mUnixTime(PR_Now())
  {}

  virtual void AddDetails(JSContext* aCx, dom::ProfileTimelineMarker& aMarker) override
  {
    TimelineMarker::AddDetails(aCx, aMarker);
    aMarker.mUnixTime.Construct(mUnixTime);
  }

private:
  // Certain consumers might use Date.now() or similar for tracing time.
  // However, TimelineMarkers use process creation as an epoch, which provides
  // more precision. To allow syncing, attach an additional unix timestamp.
  // Using this instead of `AbstractTimelineMarker::GetTime()'s` timestamp
  // is strongly discouraged.
  PRTime mUnixTime;
};

} // namespace mozilla

#endif // mozilla_DocLoadingTimelineMarker_h_
