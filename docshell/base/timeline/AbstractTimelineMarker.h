/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_AbstractTimelineMarker_h_
#define mozilla_AbstractTimelineMarker_h_

#include "TimelineMarkerEnums.h" // for MarkerTracingType
#include "nsDOMNavigationTiming.h" // for DOMHighResTimeStamp
#include "mozilla/UniquePtr.h"

struct JSContext;

namespace mozilla {
class TimeStamp;

namespace dom {
struct ProfileTimelineMarker;
}

class AbstractTimelineMarker
{
private:
  AbstractTimelineMarker() = delete;
  AbstractTimelineMarker(const AbstractTimelineMarker& aOther) = delete;
  void operator=(const AbstractTimelineMarker& aOther) = delete;

public:
  AbstractTimelineMarker(const char* aName,
                         MarkerTracingType aTracingType);

  AbstractTimelineMarker(const char* aName,
                         const TimeStamp& aTime,
                         MarkerTracingType aTracingType);

  virtual ~AbstractTimelineMarker();

  virtual UniquePtr<AbstractTimelineMarker> Clone();

  virtual bool Equals(const AbstractTimelineMarker& aOther) = 0;
  virtual void AddDetails(JSContext* aCx, dom::ProfileTimelineMarker& aMarker) = 0;
  virtual JSObject* GetStack() = 0;

  const char* GetName() const { return mName; }
  DOMHighResTimeStamp GetTime() const { return mTime; }
  MarkerTracingType GetTracingType() const { return mTracingType; }

private:
  const char* mName;
  DOMHighResTimeStamp mTime;
  MarkerTracingType mTracingType;

  void SetCurrentTime();
  void SetCustomTime(const TimeStamp& aTime);
};

} // namespace mozilla

#endif /* mozilla_AbstractTimelineMarker_h_ */
