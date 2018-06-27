/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/PerformanceNavigationTiming.h"
#include "mozilla/dom/PerformanceNavigationTimingBinding.h"

using namespace mozilla::dom;

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PerformanceNavigationTiming)
NS_INTERFACE_MAP_END_INHERITING(PerformanceResourceTiming)

NS_IMPL_ADDREF_INHERITED(PerformanceNavigationTiming, PerformanceResourceTiming)
NS_IMPL_RELEASE_INHERITED(PerformanceNavigationTiming, PerformanceResourceTiming)

JSObject*
PerformanceNavigationTiming::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return PerformanceNavigationTiming_Binding::Wrap(aCx, this, aGivenProto);
}

#define REDUCE_TIME_PRECISION                               \
  if (mPerformance->IsSystemPrincipal()) {                  \
    return rawValue;                                        \
  }                                                         \
  return nsRFPService::ReduceTimePrecisionAsMSecs(rawValue, \
    mPerformance->GetRandomTimelineSeed())

DOMHighResTimeStamp
PerformanceNavigationTiming::UnloadEventStart() const
{
  DOMHighResTimeStamp rawValue =
    mPerformance->GetDOMTiming()->GetUnloadEventStartHighRes();

  REDUCE_TIME_PRECISION;
}

DOMHighResTimeStamp
PerformanceNavigationTiming::UnloadEventEnd() const
{
  DOMHighResTimeStamp rawValue =
    mPerformance->GetDOMTiming()->GetUnloadEventEndHighRes();

  REDUCE_TIME_PRECISION;
}

DOMHighResTimeStamp
PerformanceNavigationTiming::DomInteractive() const
{
  DOMHighResTimeStamp rawValue =
    mPerformance->GetDOMTiming()->GetDomInteractiveHighRes();

  REDUCE_TIME_PRECISION;
}

DOMHighResTimeStamp
PerformanceNavigationTiming::DomContentLoadedEventStart() const
{
  DOMHighResTimeStamp rawValue =
    mPerformance->GetDOMTiming()->GetDomContentLoadedEventStartHighRes();

  REDUCE_TIME_PRECISION;
}

DOMHighResTimeStamp
PerformanceNavigationTiming::DomContentLoadedEventEnd() const
{
  DOMHighResTimeStamp rawValue =
    mPerformance->GetDOMTiming()->GetDomContentLoadedEventEndHighRes();

  REDUCE_TIME_PRECISION;
}

DOMHighResTimeStamp
PerformanceNavigationTiming::DomComplete() const
{
  DOMHighResTimeStamp rawValue =
    mPerformance->GetDOMTiming()->GetDomCompleteHighRes();

  REDUCE_TIME_PRECISION;
}

DOMHighResTimeStamp
PerformanceNavigationTiming::LoadEventStart() const
{
  DOMHighResTimeStamp rawValue =
    mPerformance->GetDOMTiming()->GetLoadEventStartHighRes();

  REDUCE_TIME_PRECISION;
}

DOMHighResTimeStamp
PerformanceNavigationTiming::LoadEventEnd() const
{
  DOMHighResTimeStamp rawValue =
    mPerformance->GetDOMTiming()->GetLoadEventEndHighRes();

  REDUCE_TIME_PRECISION;
}

NavigationType
PerformanceNavigationTiming::Type() const
{
  switch(mPerformance->GetDOMTiming()->GetType()) {
    case nsDOMNavigationTiming::TYPE_NAVIGATE:
      return NavigationType::Navigate;
      break;
    case nsDOMNavigationTiming::TYPE_RELOAD:
      return NavigationType::Reload;
      break;
    case nsDOMNavigationTiming::TYPE_BACK_FORWARD:
      return NavigationType::Back_forward;
      break;
    default:
      // The type is TYPE_RESERVED or some other value that was later added.
      // We fallback to the default of Navigate.
      return NavigationType::Navigate;
  }
}

uint16_t
PerformanceNavigationTiming::RedirectCount() const
{
  return mTimingData->GetRedirectCount();
}
