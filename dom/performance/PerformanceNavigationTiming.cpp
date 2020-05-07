/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/PerformanceNavigationTiming.h"
#include "mozilla/dom/PerformanceNavigationTimingBinding.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/StaticPrefs_privacy.h"

using namespace mozilla::dom;

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PerformanceNavigationTiming)
NS_INTERFACE_MAP_END_INHERITING(PerformanceResourceTiming)

NS_IMPL_ADDREF_INHERITED(PerformanceNavigationTiming, PerformanceResourceTiming)
NS_IMPL_RELEASE_INHERITED(PerformanceNavigationTiming,
                          PerformanceResourceTiming)

JSObject* PerformanceNavigationTiming::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return PerformanceNavigationTiming_Binding::Wrap(aCx, this, aGivenProto);
}

#define REDUCE_TIME_PRECISION                          \
  return nsRFPService::ReduceTimePrecisionAsMSecs(     \
      rawValue, mPerformance->GetRandomTimelineSeed(), \
      mPerformance->IsSystemPrincipal(), mPerformance->CrossOriginIsolated())

DOMHighResTimeStamp PerformanceNavigationTiming::UnloadEventStart() const {
  DOMHighResTimeStamp rawValue = 0;
  /*
   * Per Navigation Timing Level 2, the value is 0 if
   * a. there is no previous document or
   * b. the same-origin-check fails.
   *
   * The same-origin-check is defined as:
   * 1. If the previous document exists and its origin is not same
   *    origin as the current document's origin, return "fail".
   * 2. Let request be the current document's request.
   * 3. If request's redirect count is not zero, and all of request's
   *    HTTP redirects have the same origin as the current document,
   *    return "pass".
   * 4. Otherwise, return "fail".
   */
  if (mTimingData->AllRedirectsSameOrigin()) {  // same-origin-check:2/3
    /*
     * GetUnloadEventStartHighRes returns 0 if
     * 1. there is no previous document (a, above) or
     * 2. the current URI does not have the same origin as
     *    the previous document's URI. (same-origin-check:1)
     */
    rawValue = mPerformance->GetDOMTiming()->GetUnloadEventStartHighRes();
  }

  REDUCE_TIME_PRECISION;
}

DOMHighResTimeStamp PerformanceNavigationTiming::UnloadEventEnd() const {
  DOMHighResTimeStamp rawValue = 0;

  // See comments in PerformanceNavigationTiming::UnloadEventEnd().
  if (mTimingData->AllRedirectsSameOrigin()) {
    rawValue = mPerformance->GetDOMTiming()->GetUnloadEventEndHighRes();
  }

  REDUCE_TIME_PRECISION;
}

DOMHighResTimeStamp PerformanceNavigationTiming::DomInteractive() const {
  DOMHighResTimeStamp rawValue =
      mPerformance->GetDOMTiming()->GetDomInteractiveHighRes();

  REDUCE_TIME_PRECISION;
}

DOMHighResTimeStamp PerformanceNavigationTiming::DomContentLoadedEventStart()
    const {
  DOMHighResTimeStamp rawValue =
      mPerformance->GetDOMTiming()->GetDomContentLoadedEventStartHighRes();

  REDUCE_TIME_PRECISION;
}

DOMHighResTimeStamp PerformanceNavigationTiming::DomContentLoadedEventEnd()
    const {
  DOMHighResTimeStamp rawValue =
      mPerformance->GetDOMTiming()->GetDomContentLoadedEventEndHighRes();

  REDUCE_TIME_PRECISION;
}

DOMHighResTimeStamp PerformanceNavigationTiming::DomComplete() const {
  DOMHighResTimeStamp rawValue =
      mPerformance->GetDOMTiming()->GetDomCompleteHighRes();

  REDUCE_TIME_PRECISION;
}

DOMHighResTimeStamp PerformanceNavigationTiming::LoadEventStart() const {
  DOMHighResTimeStamp rawValue =
      mPerformance->GetDOMTiming()->GetLoadEventStartHighRes();

  REDUCE_TIME_PRECISION;
}

DOMHighResTimeStamp PerformanceNavigationTiming::LoadEventEnd() const {
  DOMHighResTimeStamp rawValue =
      mPerformance->GetDOMTiming()->GetLoadEventEndHighRes();

  REDUCE_TIME_PRECISION;
}

NavigationType PerformanceNavigationTiming::Type() const {
  switch (mPerformance->GetDOMTiming()->GetType()) {
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

uint16_t PerformanceNavigationTiming::RedirectCount() const {
  return mTimingData->GetRedirectCount();
}

void PerformanceNavigationTiming::UpdatePropertiesFromHttpChannel(
    nsIHttpChannel* aHttpChannel, nsITimedChannel* aChannel) {
  mTimingData->SetPropertiesFromHttpChannel(aHttpChannel, aChannel);
}

bool PerformanceNavigationTiming::Enabled(JSContext* aCx, JSObject* aGlobal) {
  return (StaticPrefs::dom_enable_performance_navigation_timing() &&
          !StaticPrefs::privacy_resistFingerprinting());
}
