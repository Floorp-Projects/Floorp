/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_RestyleTimelineMarker_h_
#define mozilla_RestyleTimelineMarker_h_

#include "TimelineMarker.h"
#include "mozilla/dom/ProfileTimelineMarkerBinding.h"

namespace mozilla {

class RestyleTimelineMarker : public TimelineMarker
{
public:
  explicit RestyleTimelineMarker(nsRestyleHint aRestyleHint,
                                 MarkerTracingType aTracingType)
    : TimelineMarker("Styles", aTracingType)
  {
    if (aRestyleHint) {
      mRestyleHint.AssignWithConversion(RestyleManager::RestyleHintToString(aRestyleHint));
    }
  }

  virtual void AddDetails(JSContext* aCx, dom::ProfileTimelineMarker& aMarker) override
  {
    if (GetTracingType() == MarkerTracingType::START) {
      aMarker.mRestyleHint.Construct(mRestyleHint);
    }
  }

private:
  nsAutoString mRestyleHint;
};

} // namespace mozilla

#endif // mozilla_RestyleTimelineMarker_h_
