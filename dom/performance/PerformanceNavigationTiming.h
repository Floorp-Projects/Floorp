/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PerformanceNavigationTiming_h___
#define mozilla_dom_PerformanceNavigationTiming_h___

#include "nsCOMPtr.h"
#include "nsIChannel.h"
#include "nsITimedChannel.h"
#include "nsRFPService.h"
#include "mozilla/dom/PerformanceResourceTiming.h"
#include "mozilla/dom/PerformanceNavigationTimingBinding.h"
#include "nsIHttpChannel.h"

namespace mozilla {
namespace dom {

// https://www.w3.org/TR/navigation-timing-2/#sec-PerformanceNavigationTiming
class PerformanceNavigationTiming final
  : public PerformanceResourceTiming
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  // Note that aPerformanceTiming must be initalized with zeroTime = 0
  // so that timestamps are relative to startTime, as opposed to the
  // performance.timing object for which timestamps are absolute and has a
  // zeroTime initialized to navigationStart
  explicit PerformanceNavigationTiming(PerformanceTiming* aPerformanceTiming,
                                       Performance* aPerformance,
                                       nsIHttpChannel* aChannel)
    : PerformanceResourceTiming(aPerformanceTiming, aPerformance,
                                NS_LITERAL_STRING("document"), aChannel) {
      SetEntryType(NS_LITERAL_STRING("navigation"));
      SetInitiatorType(NS_LITERAL_STRING("navigation"));
    }

  DOMHighResTimeStamp Duration() const override
  {
    return nsRFPService::ReduceTimePrecisionAsMSecs(LoadEventEnd() - StartTime());
  }

  DOMHighResTimeStamp StartTime() const override
  {
    return 0;
  }

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  DOMHighResTimeStamp UnloadEventStart() const;
  DOMHighResTimeStamp UnloadEventEnd() const;

  DOMHighResTimeStamp DomInteractive() const;
  DOMHighResTimeStamp DomContentLoadedEventStart() const;
  DOMHighResTimeStamp DomContentLoadedEventEnd() const;
  DOMHighResTimeStamp DomComplete() const;
  DOMHighResTimeStamp LoadEventStart() const;
  DOMHighResTimeStamp LoadEventEnd() const;
  NavigationType Type() const;
  uint16_t RedirectCount() const;

private:
  ~PerformanceNavigationTiming() {}
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PerformanceNavigationTiming_h___
