/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PerformanceRenderTiming.h"
#include "mozilla/dom/PerformanceRenderTimingBinding.h"

using namespace mozilla::dom;

PerformanceRenderTiming::PerformanceRenderTiming(nsISupports* aParent,
                                                 const nsAString& aName,
                                                 const DOMHighResTimeStamp& aStartTime,
                                                 const DOMHighResTimeStamp& aDuration,
                                                 uint32_t aSourceFrameNumber)
: PerformanceEntry(aParent, aName, NS_LITERAL_STRING("render"))
, mStartTime(aStartTime)
, mDuration(aDuration)
, mSourceFrameNumber(aSourceFrameNumber)
{
  MOZ_ASSERT(aParent, "Parent performance object should be provided");
}

PerformanceRenderTiming::~PerformanceRenderTiming()
{
}

JSObject*
PerformanceRenderTiming::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return PerformanceRenderTimingBinding::Wrap(aCx, this, aGivenProto);
}
