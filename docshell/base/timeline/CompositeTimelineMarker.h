/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_CompositeTimelineMarker_h_
#define mozilla_CompositeTimelineMarker_h_

#include "TimelineMarker.h"
#include "mozilla/dom/ProfileTimelineMarkerBinding.h"

namespace mozilla {

class CompositeTimelineMarker : public TimelineMarker
{
public:
  CompositeTimelineMarker(const TimeStamp& aTime,
                          MarkerTracingType aTracingType)
    : TimelineMarker("Composite", aTime, aTracingType)
  {
    // Even though these markers end up being created on the main thread in the
    // content or chrome processes, they actually trace down code in the
    // compositor parent process. All the information for creating these markers
    // is sent along via IPC to an nsView when a composite finishes.
    // Mark this as 'off the main thread' to style it differently in frontends.
    SetOffMainThread(true);
  }
};

} // namespace mozilla

#endif // mozilla_CompositeTimelineMarker_h_
