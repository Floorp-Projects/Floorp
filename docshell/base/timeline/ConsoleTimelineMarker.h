/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ConsoleTimelineMarker_h_
#define mozilla_ConsoleTimelineMarker_h_

#include "TimelineMarker.h"
#include "mozilla/dom/ProfileTimelineMarkerBinding.h"

namespace mozilla {

class ConsoleTimelineMarker : public TimelineMarker
{
public:
  explicit ConsoleTimelineMarker(const nsAString& aCause,
                                 TracingMetadata aMetaData)
    : TimelineMarker("ConsoleTime", aMetaData)
    , mCause(aCause)
  {
    // Stack is captured by default on the "start" marker. Explicitly also
    // capture stack on the "end" marker.
    if (aMetaData == TRACING_INTERVAL_END) {
      CaptureStack();
    }
  }

  virtual bool Equals(const TimelineMarker& aOther) override
  {
    if (!TimelineMarker::Equals(aOther)) {
      return false;
    }
    // Console markers must have matching causes as well. It is safe to perform
    // a static_cast here as the previous equality check ensures that this is
    // a console marker instance.
    return mCause == static_cast<const ConsoleTimelineMarker*>(&aOther)->mCause;
  }

  virtual void AddDetails(JSContext* aCx, dom::ProfileTimelineMarker& aMarker) override
  {
    if (GetMetaData() == TRACING_INTERVAL_START) {
      aMarker.mCauseName.Construct(mCause);
    } else {
      aMarker.mEndStack = GetStack();
    }
  }

private:
  nsString mCause;
};

} // namespace mozilla

#endif // mozilla_ConsoleTimelineMarker_h_
