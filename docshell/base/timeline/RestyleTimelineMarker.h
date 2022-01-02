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

class RestyleTimelineMarker : public TimelineMarker {
 public:
  RestyleTimelineMarker(bool aIsAnimationOnly, MarkerTracingType aTracingType)
      : TimelineMarker("Styles", aTracingType) {
    mIsAnimationOnly = aIsAnimationOnly;
  }

  virtual void AddDetails(JSContext* aCx,
                          dom::ProfileTimelineMarker& aMarker) override {
    TimelineMarker::AddDetails(aCx, aMarker);

    if (GetTracingType() == MarkerTracingType::START) {
      aMarker.mIsAnimationOnly.Construct(mIsAnimationOnly);
    }
  }

 private:
  bool mIsAnimationOnly;
};

}  // namespace mozilla

#endif  // mozilla_RestyleTimelineMarker_h_
