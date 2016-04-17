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
PerformanceNavigationTiming::UnloadEventStart()
{
  return mTiming->GetDOMTiming()->GetUnloadEventStart();
}

DOMHighResTimeStamp
PerformanceNavigationTiming::UnloadEventEnd()
{
  return mTiming->GetDOMTiming()->GetUnloadEventEnd();
}

DOMHighResTimeStamp
PerformanceNavigationTiming::DomInteractive()
{
  return mTiming->GetDOMTiming()->GetDomInteractive();
}

DOMHighResTimeStamp
PerformanceNavigationTiming::DomContentLoadedEventStart()
{
  return mTiming->GetDOMTiming()->GetDomContentLoadedEventStart();
}

DOMHighResTimeStamp
PerformanceNavigationTiming::DomContentLoadedEventEnd()
{
  return mTiming->GetDOMTiming()->GetDomContentLoadedEventEnd();
}

DOMHighResTimeStamp
PerformanceNavigationTiming::DomComplete()
{
  return mTiming->GetDOMTiming()->GetDomComplete();
}

DOMHighResTimeStamp
PerformanceNavigationTiming::LoadEventStart()
{
  return mTiming->GetDOMTiming()->GetLoadEventStart();
}

DOMHighResTimeStamp
PerformanceNavigationTiming::LoadEventEnd() const
{
  return mTiming->GetDOMTiming()->GetLoadEventEnd();
}

NavigationType
PerformanceNavigationTiming::Type()
{
  switch(mTiming->GetDOMTiming()->GetType()) {
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
      MOZ_CRASH(); // Should not happen
      return NavigationType::Navigate;
  }
}

uint16_t
PerformanceNavigationTiming::RedirectCount()
{
  return mTiming->GetRedirectCount();
}
