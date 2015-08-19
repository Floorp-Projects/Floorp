/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PerformanceCompositeTiming.h"
#include "mozilla/dom/PerformanceCompositeTimingBinding.h"

using namespace mozilla::dom;

PerformanceCompositeTiming::PerformanceCompositeTiming(nsISupports* aParent,
                                                       const nsAString& aName,
                                                       const DOMHighResTimeStamp& aStartTime,
                                                       uint32_t aSourceFrameNumber)
: PerformanceEntry(aParent, aName, NS_LITERAL_STRING("composite"))
, mStartTime(aStartTime)
, mSourceFrameNumber(aSourceFrameNumber)
{
  MOZ_ASSERT(aParent, "Parent performance object should be provided");
}

PerformanceCompositeTiming::~PerformanceCompositeTiming()
{
}

JSObject*
PerformanceCompositeTiming::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return PerformanceCompositeTimingBinding::Wrap(aCx, this, aGivenProto);
}
