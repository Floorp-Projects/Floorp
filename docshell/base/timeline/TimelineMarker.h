/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_TimelineMarker_h_
#define mozilla_TimelineMarker_h_

#include "AbstractTimelineMarker.h"
#include "js/RootingAPI.h"

namespace mozilla {

// Objects of this type can be added to the timeline if there is an interested
// consumer. The class can also be subclassed to let a given marker creator
// provide custom details.
class TimelineMarker : public AbstractTimelineMarker
{
public:
  TimelineMarker(const char* aName,
                 MarkerTracingType aTracingType,
                 MarkerStackRequest aStackRequest = MarkerStackRequest::STACK);

  TimelineMarker(const char* aName,
                 const TimeStamp& aTime,
                 MarkerTracingType aTracingType,
                 MarkerStackRequest aStackRequest = MarkerStackRequest::STACK);

  virtual void AddDetails(JSContext* aCx, dom::ProfileTimelineMarker& aMarker) override;
  virtual JSObject* GetStack() override;

protected:
  void CaptureStack();

private:
  // While normally it is not a good idea to make a persistent root,
  // in this case changing nsDocShell to participate in cycle
  // collection was deemed too invasive, and the markers are only held
  // here temporarily to boot.
  JS::PersistentRooted<JSObject*> mStackTrace;

  void CaptureStackIfNecessary(MarkerTracingType aTracingType,
                               MarkerStackRequest aStackRequest);
};

} // namespace mozilla

#endif /* mozilla_TimelineMarker_h_ */
