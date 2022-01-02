/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PerformanceNavigationTiming_h___
#define mozilla_dom_PerformanceNavigationTiming_h___

#include <stdint.h>
#include <utility>
#include "js/RootingAPI.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/PerformanceNavigationTimingBinding.h"
#include "mozilla/dom/PerformanceResourceTiming.h"
#include "nsDOMNavigationTiming.h"
#include "nsISupports.h"
#include "nsLiteralString.h"
#include "nsString.h"
#include "nsTLiteralString.h"

class JSObject;
class nsIHttpChannel;
class nsITimedChannel;
struct JSContext;

namespace mozilla {
namespace dom {

class Performance;
class PerformanceTimingData;

// https://www.w3.org/TR/navigation-timing-2/#sec-PerformanceNavigationTiming
class PerformanceNavigationTiming final : public PerformanceResourceTiming {
 public:
  NS_DECL_ISUPPORTS_INHERITED

  // Note that aPerformanceTiming must be initalized with zeroTime = 0
  // so that timestamps are relative to startTime, as opposed to the
  // performance.timing object for which timestamps are absolute and has a
  // zeroTime initialized to navigationStart
  // aPerformanceTiming and aPerformance must be non-null.
  PerformanceNavigationTiming(
      UniquePtr<PerformanceTimingData>&& aPerformanceTiming,
      Performance* aPerformance, const nsAString& aName)
      : PerformanceResourceTiming(std::move(aPerformanceTiming), aPerformance,
                                  aName) {
    SetEntryType(u"navigation"_ns);
    SetInitiatorType(u"navigation"_ns);
  }

  DOMHighResTimeStamp Duration() const override {
    return LoadEventEnd() - StartTime();
  }

  DOMHighResTimeStamp StartTime() const override { return 0; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  DOMHighResTimeStamp UnloadEventStart() const;
  DOMHighResTimeStamp UnloadEventEnd() const;

  DOMHighResTimeStamp DomInteractive() const;
  DOMHighResTimeStamp DomContentLoadedEventStart() const;
  DOMHighResTimeStamp DomContentLoadedEventEnd() const;
  DOMHighResTimeStamp DomComplete() const;
  DOMHighResTimeStamp LoadEventStart() const;
  DOMHighResTimeStamp LoadEventEnd() const;

  DOMHighResTimeStamp RedirectStart(
      Maybe<nsIPrincipal*>& aSubjectPrincipal) const override;
  DOMHighResTimeStamp RedirectEnd(
      Maybe<nsIPrincipal*>& aSubjectPrincipal) const override;

  NavigationType Type() const;
  uint16_t RedirectCount() const;

  void UpdatePropertiesFromHttpChannel(nsIHttpChannel* aHttpChannel,
                                       nsITimedChannel* aChannel);

  /*
   * For use with the WebIDL Func attribute to determine whether
   * window.PerformanceNavigationTiming is exposed.
   */
  static bool Enabled(JSContext* aCx, JSObject* aGlobal);

 private:
  ~PerformanceNavigationTiming() = default;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_PerformanceNavigationTiming_h___
