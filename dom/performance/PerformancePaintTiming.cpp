/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PerformancePaintTiming.h"
#include "MainThreadUtils.h"
#include "mozilla/dom/PerformanceMeasureBinding.h"

using namespace mozilla::dom;

PerformancePaintTiming::PerformancePaintTiming(Performance* aPerformance,
                                               const nsAString& aName,
                                               DOMHighResTimeStamp aStartTime)
    : PerformanceEntry(aPerformance->GetParentObject(), aName, u"paint"_ns),
      mStartTime(aStartTime) {}

PerformancePaintTiming::~PerformancePaintTiming() = default;

JSObject* PerformancePaintTiming::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return PerformancePaintTiming_Binding::Wrap(aCx, this, aGivenProto);
}

size_t PerformancePaintTiming::SizeOfIncludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}
