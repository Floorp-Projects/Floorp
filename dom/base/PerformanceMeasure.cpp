/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PerformanceMeasure.h"
#include "mozilla/dom/PerformanceMeasureBinding.h"

using namespace mozilla::dom;

PerformanceMeasure::PerformanceMeasure(nsPerformance* aPerformance,
                                       const nsAString& aName,
                                       DOMHighResTimeStamp aStartTime,
                                       DOMHighResTimeStamp aEndTime)
: PerformanceEntry(aPerformance, aName, NS_LITERAL_STRING("measure")),
  mStartTime(aStartTime),
  mDuration(aEndTime - aStartTime)
{
  MOZ_ASSERT(aPerformance, "Parent performance object should be provided");
}

PerformanceMeasure::~PerformanceMeasure()
{
}

JSObject*
PerformanceMeasure::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return PerformanceMeasureBinding::Wrap(aCx, this, aGivenProto);
}
