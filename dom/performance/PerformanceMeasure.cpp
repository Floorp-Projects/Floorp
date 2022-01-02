/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PerformanceMeasure.h"
#include "MainThreadUtils.h"
#include "mozilla/dom/PerformanceMeasureBinding.h"

using namespace mozilla::dom;

PerformanceMeasure::PerformanceMeasure(nsISupports* aParent,
                                       const nsAString& aName,
                                       DOMHighResTimeStamp aStartTime,
                                       DOMHighResTimeStamp aEndTime)
    : PerformanceEntry(aParent, aName, u"measure"_ns),
      mStartTime(aStartTime),
      mDuration(aEndTime - aStartTime) {}

PerformanceMeasure::~PerformanceMeasure() = default;

JSObject* PerformanceMeasure::WrapObject(JSContext* aCx,
                                         JS::Handle<JSObject*> aGivenProto) {
  return PerformanceMeasure_Binding::Wrap(aCx, this, aGivenProto);
}

size_t PerformanceMeasure::SizeOfIncludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}
