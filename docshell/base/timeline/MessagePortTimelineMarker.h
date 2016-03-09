/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_MessagePortTimelineMarker_h_
#define mozilla_MessagePortTimelineMarker_h_

#include "TimelineMarker.h"
#include "mozilla/dom/ProfileTimelineMarkerBinding.h"

namespace mozilla {

class MessagePortTimelineMarker : public TimelineMarker
{
public:
  MessagePortTimelineMarker(dom::ProfileTimelineMessagePortOperationType aOperationType,
                            MarkerTracingType aTracingType)
    : TimelineMarker("MessagePort", aTracingType, MarkerStackRequest::NO_STACK)
    , mOperationType(aOperationType)
  {}

  virtual UniquePtr<AbstractTimelineMarker> Clone() override
  {
    MessagePortTimelineMarker* clone =
      new MessagePortTimelineMarker(mOperationType, GetTracingType());
    clone->SetCustomTime(GetTime());
    return UniquePtr<AbstractTimelineMarker>(clone);
  }

  virtual void AddDetails(JSContext* aCx, dom::ProfileTimelineMarker& aMarker) override
  {
    TimelineMarker::AddDetails(aCx, aMarker);

    if (GetTracingType() == MarkerTracingType::START) {
      aMarker.mMessagePortOperation.Construct(mOperationType);
    }
  }

private:
  dom::ProfileTimelineMessagePortOperationType mOperationType;
};

} // namespace mozilla

#endif /* mozilla_MessagePortTimelineMarker_h_ */
