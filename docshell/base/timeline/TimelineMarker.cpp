/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TimelineMarker.h"

namespace mozilla {

TimelineMarker::TimelineMarker(const char* aName,
                               MarkerTracingType aTracingType,
                               MarkerStackRequest aStackRequest)
  : AbstractTimelineMarker(aName, aTracingType)
{
  CaptureStackIfNecessary(aTracingType, aStackRequest);
}

TimelineMarker::TimelineMarker(const char* aName,
                               const TimeStamp& aTime,
                               MarkerTracingType aTracingType,
                               MarkerStackRequest aStackRequest)
  : AbstractTimelineMarker(aName, aTime, aTracingType)
{
  CaptureStackIfNecessary(aTracingType, aStackRequest);
}

bool
TimelineMarker::Equals(const AbstractTimelineMarker& aOther)
{
  // Check whether two markers should be considered the same, for the purpose
  // of pairing start and end markers. Normally this definition suffices.
  return strcmp(GetName(), aOther.GetName()) == 0;
}

void
TimelineMarker::AddDetails(JSContext* aCx, dom::ProfileTimelineMarker& aMarker)
{
  // Nothing to do here for plain markers.
}

JSObject*
TimelineMarker::GetStack()
{
  if (mStackTrace.initialized()) {
    return mStackTrace;
  }
  return nullptr;
}

void
TimelineMarker::CaptureStack()
{
  JSContext* ctx = nsContentUtils::GetCurrentJSContext();
  if (ctx) {
    JS::RootedObject stack(ctx);
    if (JS::CaptureCurrentStack(ctx, &stack)) {
      mStackTrace.init(ctx, stack.get());
    } else {
      JS_ClearPendingException(ctx);
    }
  }
}

void
TimelineMarker::CaptureStackIfNecessary(MarkerTracingType aTracingType,
                                        MarkerStackRequest aStackRequest)
{
  if ((aTracingType == MarkerTracingType::START ||
      aTracingType == MarkerTracingType::TIMESTAMP) &&
      aStackRequest != MarkerStackRequest::NO_STACK) {
    CaptureStack();
  }
}

} // namespace mozilla
