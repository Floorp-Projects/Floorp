/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PerformanceTiming_h
#define mozilla_dom_PerformanceTiming_h

#include "mozilla/Attributes.h"
#include "nsContentUtils.h"
#include "nsDOMNavigationTiming.h"
#include "nsWrapperCache.h"
#include "Performance.h"

class nsIHttpChannel;
class nsITimedChannel;

namespace mozilla {
namespace dom {

// Script "performance.timing" object
class PerformanceTiming final : public nsWrapperCache
{
public:
/**
 * @param   aPerformance
 *          The performance object (the JS parent).
 *          This will allow access to "window.performance.timing" attribute for
 *          the navigation timing (can't be null).
 * @param   aChannel
 *          An nsITimedChannel used to gather all the networking timings by both
 *          the navigation timing and the resource timing (can't be null).
 * @param   aHttpChannel
 *          An nsIHttpChannel (the resource's http channel).
 *          This will be used by the resource timing cross-domain check
 *          algorithm.
 *          Argument is null for the navigation timing (navigation timing uses
 *          another algorithm for the cross-domain redirects).
 * @param   aZeroTime
 *          The offset that will be added to the timestamp of each event. This
 *          argument should be equal to performance.navigationStart for
 *          navigation timing and "0" for the resource timing.
 */
  PerformanceTiming(Performance* aPerformance,
                    nsITimedChannel* aChannel,
                    nsIHttpChannel* aHttpChannel,
                    DOMHighResTimeStamp aZeroTime);
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(PerformanceTiming)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(PerformanceTiming)

  nsDOMNavigationTiming* GetDOMTiming() const
  {
    return mPerformance->GetDOMTiming();
  }

  Performance* GetParentObject() const
  {
    return mPerformance;
  }

  /**
   * @param   aStamp
   *          The TimeStamp recorded for a specific event. This TimeStamp can
   *          be null.
   * @return  the duration of an event with a given TimeStamp, relative to the
   *          navigationStart TimeStamp (the moment the user landed on the
   *          page), if the given TimeStamp is valid. Otherwise, it will return
   *          the FetchStart timing value.
   */
  inline DOMHighResTimeStamp TimeStampToDOMHighResOrFetchStart(TimeStamp aStamp)
  {
    return (!aStamp.IsNull())
        ? TimeStampToDOMHighRes(aStamp)
        : FetchStartHighRes();
  }

  /**
   * The nsITimedChannel records an absolute timestamp for each event.
   * The nsDOMNavigationTiming will record the moment when the user landed on
   * the page. This is a window.performance unique timestamp, so it can be used
   * for all the events (navigation timing and resource timing events).
   *
   * The algorithm operates in 2 steps:
   * 1. The first step is to subtract the two timestamps: the argument (the
   * envet's timesramp) and the navigation start timestamp. This will result in
   * a relative timestamp of the event (relative to the navigation start -
   * window.performance.timing.navigationStart).
   * 2. The second step is to add any required offset (the mZeroTime). For now,
   * this offset value is either 0 (for the resource timing), or equal to
   * "performance.navigationStart" (for navigation timing).
   * For the resource timing, mZeroTime is set to 0, causing the result to be a
   * relative time.
   * For the navigation timing, mZeroTime is set to "performance.navigationStart"
   * causing the result be an absolute time.
   *
   * @param   aStamp
   *          The TimeStamp recorded for a specific event. This TimeStamp can't
   *          be null.
   * @return  number of milliseconds value as one of:
   * - relative to the navigation start time, time the user has landed on the
   * page
   * - an absolute wall clock time since the unix epoch
   */
  inline DOMHighResTimeStamp TimeStampToDOMHighRes(TimeStamp aStamp) const
  {
    MOZ_ASSERT(!aStamp.IsNull());
    TimeDuration duration =
        aStamp - GetDOMTiming()->GetNavigationStartTimeStamp();
    return duration.ToMilliseconds() + mZeroTime;
  }

  virtual JSObject* WrapObject(JSContext *cx,
                               JS::Handle<JSObject*> aGivenProto) override;

  // PerformanceNavigation WebIDL methods
  DOMTimeMilliSec NavigationStart() const
  {
    if (!nsContentUtils::IsPerformanceTimingEnabled()) {
      return 0;
    }
    return GetDOMTiming()->GetNavigationStart();
  }

  DOMTimeMilliSec UnloadEventStart()
  {
    if (!nsContentUtils::IsPerformanceTimingEnabled()) {
      return 0;
    }
    return GetDOMTiming()->GetUnloadEventStart();
  }

  DOMTimeMilliSec UnloadEventEnd()
  {
    if (!nsContentUtils::IsPerformanceTimingEnabled()) {
      return 0;
    }
    return GetDOMTiming()->GetUnloadEventEnd();
  }

  uint16_t GetRedirectCount() const;

  // Checks if the resource is either same origin as the page that started
  // the load, or if the response contains the Timing-Allow-Origin header
  // with a value of * or matching the domain of the loading Principal
  bool CheckAllowedOrigin(nsIHttpChannel* aResourceChannel, nsITimedChannel* aChannel);

  // Cached result of CheckAllowedOrigin. If false, security sensitive
  // attributes of the resourceTiming object will be set to 0
  bool TimingAllowed() const;

  // If this is false the values of redirectStart/End will be 0
  // This is false if no redirects occured, or if any of the responses failed
  // the timing-allow-origin check in HttpBaseChannel::TimingAllowCheck
  bool ShouldReportCrossOriginRedirect() const;

  // High resolution (used by resource timing)
  DOMHighResTimeStamp FetchStartHighRes();
  DOMHighResTimeStamp RedirectStartHighRes();
  DOMHighResTimeStamp RedirectEndHighRes();
  DOMHighResTimeStamp DomainLookupStartHighRes();
  DOMHighResTimeStamp DomainLookupEndHighRes();
  DOMHighResTimeStamp ConnectStartHighRes();
  DOMHighResTimeStamp ConnectEndHighRes();
  DOMHighResTimeStamp RequestStartHighRes();
  DOMHighResTimeStamp ResponseStartHighRes();
  DOMHighResTimeStamp ResponseEndHighRes();

  // Low resolution (used by navigation timing)
  DOMTimeMilliSec FetchStart();
  DOMTimeMilliSec RedirectStart();
  DOMTimeMilliSec RedirectEnd();
  DOMTimeMilliSec DomainLookupStart();
  DOMTimeMilliSec DomainLookupEnd();
  DOMTimeMilliSec ConnectStart();
  DOMTimeMilliSec ConnectEnd();
  DOMTimeMilliSec RequestStart();
  DOMTimeMilliSec ResponseStart();
  DOMTimeMilliSec ResponseEnd();

  DOMTimeMilliSec DomLoading()
  {
    if (!nsContentUtils::IsPerformanceTimingEnabled()) {
      return 0;
    }
    return GetDOMTiming()->GetDomLoading();
  }

  DOMTimeMilliSec DomInteractive() const
  {
    if (!nsContentUtils::IsPerformanceTimingEnabled()) {
      return 0;
    }
    return GetDOMTiming()->GetDomInteractive();
  }

  DOMTimeMilliSec DomContentLoadedEventStart() const
  {
    if (!nsContentUtils::IsPerformanceTimingEnabled()) {
      return 0;
    }
    return GetDOMTiming()->GetDomContentLoadedEventStart();
  }

  DOMTimeMilliSec DomContentLoadedEventEnd() const
  {
    if (!nsContentUtils::IsPerformanceTimingEnabled()) {
      return 0;
    }
    return GetDOMTiming()->GetDomContentLoadedEventEnd();
  }

  DOMTimeMilliSec DomComplete() const
  {
    if (!nsContentUtils::IsPerformanceTimingEnabled()) {
      return 0;
    }
    return GetDOMTiming()->GetDomComplete();
  }

  DOMTimeMilliSec LoadEventStart() const
  {
    if (!nsContentUtils::IsPerformanceTimingEnabled()) {
      return 0;
    }
    return GetDOMTiming()->GetLoadEventStart();
  }

  DOMTimeMilliSec LoadEventEnd() const
  {
    if (!nsContentUtils::IsPerformanceTimingEnabled()) {
      return 0;
    }
    return GetDOMTiming()->GetLoadEventEnd();
  }

private:
  ~PerformanceTiming();

  bool IsInitialized() const;
  void InitializeTimingInfo(nsITimedChannel* aChannel);

  RefPtr<Performance> mPerformance;
  DOMHighResTimeStamp mFetchStart;

  // This is an offset that will be added to each timing ([ms] resolution).
  // There are only 2 possible values: (1) logicaly equal to navigationStart
  // TimeStamp (results are absolute timstamps - wallclock); (2) "0" (results
  // are relative to the navigation start).
  DOMHighResTimeStamp mZeroTime;

  TimeStamp mAsyncOpen;
  TimeStamp mRedirectStart;
  TimeStamp mRedirectEnd;
  TimeStamp mDomainLookupStart;
  TimeStamp mDomainLookupEnd;
  TimeStamp mConnectStart;
  TimeStamp mConnectEnd;
  TimeStamp mRequestStart;
  TimeStamp mResponseStart;
  TimeStamp mCacheReadStart;
  TimeStamp mResponseEnd;
  TimeStamp mCacheReadEnd;
  uint16_t mRedirectCount;
  bool mTimingAllowed;
  bool mAllRedirectsSameOrigin;
  bool mInitialized;

  // If the resourceTiming object should have non-zero redirectStart and
  // redirectEnd attributes. It is false if there were no redirects, or if
  // any of the responses didn't pass the timing-allow-check
  bool mReportCrossOriginRedirect;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PerformanceTiming_h
