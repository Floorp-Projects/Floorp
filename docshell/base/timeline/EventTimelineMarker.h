/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_EventTimelineMarker_h_
#define mozilla_EventTimelineMarker_h_

#include "TimelineMarker.h"
#include "mozilla/dom/ProfileTimelineMarkerBinding.h"

namespace mozilla {

class EventTimelineMarker : public TimelineMarker
{
public:
  EventTimelineMarker(const nsAString& aType,
                      uint16_t aPhase,
                      MarkerTracingType aTracingType)
    : TimelineMarker("DOMEvent", aTracingType)
    , mType(aType)
    , mPhase(aPhase)
  {}

  virtual void AddDetails(JSContext* aCx, dom::ProfileTimelineMarker& aMarker) override
  {
    TimelineMarker::AddDetails(aCx, aMarker);

    if (GetTracingType() == MarkerTracingType::START) {
      aMarker.mType.Construct(mType);
      aMarker.mEventPhase.Construct(mPhase);
    }
  }

private:
  nsString mType;
  uint16_t mPhase;
};

} // namespace mozilla

#endif // mozilla_EventTimelineMarker_h_
