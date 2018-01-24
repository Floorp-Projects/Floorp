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
  return PerformanceNavigationTimingBinding::Wrap(aCx, this, aGivenProto);
}

DOMHighResTimeStamp
PerformanceNavigationTiming::UnloadEventStart() const
{
  return mPerformance->GetDOMTiming()->GetUnloadEventStartHighRes();
}

DOMHighResTimeStamp
PerformanceNavigationTiming::UnloadEventEnd() const
{
  return mPerformance->GetDOMTiming()->GetUnloadEventEndHighRes();
}

DOMHighResTimeStamp
PerformanceNavigationTiming::DomInteractive() const
{
  return mPerformance->GetDOMTiming()->GetDomInteractiveHighRes();
}

DOMHighResTimeStamp
PerformanceNavigationTiming::DomContentLoadedEventStart() const
{
  return mPerformance->GetDOMTiming()->GetDomContentLoadedEventStartHighRes();
}

DOMHighResTimeStamp
PerformanceNavigationTiming::DomContentLoadedEventEnd() const
{
  return mPerformance->GetDOMTiming()->GetDomContentLoadedEventEndHighRes();
}

DOMHighResTimeStamp
PerformanceNavigationTiming::DomComplete() const
{
  return mPerformance->GetDOMTiming()->GetDomCompleteHighRes();
}

DOMHighResTimeStamp
PerformanceNavigationTiming::LoadEventStart() const
{
  return mPerformance->GetDOMTiming()->GetLoadEventStartHighRes();
}

DOMHighResTimeStamp
PerformanceNavigationTiming::LoadEventEnd() const
{
  return mPerformance->GetDOMTiming()->GetLoadEventEndHighRes();
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
