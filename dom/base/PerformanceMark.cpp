/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PerformanceMark.h"
#include "mozilla/dom/PerformanceMarkBinding.h"

using namespace mozilla::dom;

PerformanceMark::PerformanceMark(nsPerformance* aPerformance,
                                 const nsAString& aName)
: PerformanceEntry(aPerformance, aName, NS_LITERAL_STRING("mark"))
{
  MOZ_ASSERT(aPerformance, "Parent performance object should be provided");
  mStartTime = aPerformance->GetDOMTiming()->TimeStampToDOMHighRes(mozilla::TimeStamp::Now());
}

PerformanceMark::~PerformanceMark()
{
}

JSObject*
PerformanceMark::WrapObject(JSContext* aCx)
{
  return PerformanceMarkBinding::Wrap(aCx, this);
}
