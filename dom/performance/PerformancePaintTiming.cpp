/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PerformancePaintTiming.h"
#include "MainThreadUtils.h"
#include "mozilla/dom/PerformanceMeasureBinding.h"

using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_INHERITED(PerformancePaintTiming, PerformanceEntry,
                                   mPerformance)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PerformancePaintTiming)
NS_INTERFACE_MAP_END_INHERITING(PerformanceEntry)

NS_IMPL_ADDREF_INHERITED(PerformancePaintTiming, PerformanceEntry)
NS_IMPL_RELEASE_INHERITED(PerformancePaintTiming, PerformanceEntry)

PerformancePaintTiming::PerformancePaintTiming(Performance* aPerformance,
                                               const nsAString& aName,
                                               const TimeStamp& aStartTime)
    : PerformanceEntry(aPerformance->GetParentObject(), aName, u"paint"_ns),
      mPerformance(aPerformance),
      mStartTime(aStartTime) {}

PerformancePaintTiming::~PerformancePaintTiming() = default;

JSObject* PerformancePaintTiming::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return PerformancePaintTiming_Binding::Wrap(aCx, this, aGivenProto);
}

DOMHighResTimeStamp PerformancePaintTiming::StartTime() const {
  DOMHighResTimeStamp rawValue =
      mPerformance->GetDOMTiming()->TimeStampToDOMHighRes(mStartTime);
  return nsRFPService::ReduceTimePrecisionAsMSecs(
      rawValue, mPerformance->GetRandomTimelineSeed(),
      mPerformance->IsSystemPrincipal(), mPerformance->CrossOriginIsolated());
}

size_t PerformancePaintTiming::SizeOfIncludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}
