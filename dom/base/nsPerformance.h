/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsPerformance_h___
#define nsPerformance_h___

#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "mozilla/Attributes.h"
#include "nsWrapperCache.h"
#include "nsDOMNavigationTiming.h"
#include "nsContentUtils.h"
#include "nsPIDOMWindow.h"
#include "js/TypeDecls.h"
#include "js/RootingAPI.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/DOMEventTargetHelper.h"

class nsITimedChannel;
class nsPerformance;
class nsIHttpChannel;

namespace mozilla {

class ErrorResult;

namespace dom {

class PerformanceEntry;
class PerformanceObserver;

} // namespace dom
} // namespace mozilla

// Script "performance.timing" object
class nsPerformanceTiming final : public nsWrapperCache
{
public:
  typedef mozilla::TimeStamp TimeStamp;

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
  nsPerformanceTiming(nsPerformance* aPerformance,
                      nsITimedChannel* aChannel,
                      nsIHttpChannel* aHttpChannel,
                      DOMHighResTimeStamp aZeroTime);
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(nsPerformanceTiming)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(nsPerformanceTiming)

  nsDOMNavigationTiming* GetDOMTiming() const;

  nsPerformance* GetParentObject() const
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
    mozilla::TimeDuration duration =
        aStamp - GetDOMTiming()->GetNavigationStartTimeStamp();
    return duration.ToMilliseconds() + mZeroTime;
  }

  virtual JSObject* WrapObject(JSContext *cx, JS::Handle<JSObject*> aGivenProto) override;

  // PerformanceNavigation WebIDL methods
  DOMTimeMilliSec NavigationStart() const {
    if (!nsContentUtils::IsPerformanceTimingEnabled()) {
      return 0;
    }
    return GetDOMTiming()->GetNavigationStart();
  }
  DOMTimeMilliSec UnloadEventStart() {
    if (!nsContentUtils::IsPerformanceTimingEnabled()) {
      return 0;
    }
    return GetDOMTiming()->GetUnloadEventStart();
  }
  DOMTimeMilliSec UnloadEventEnd() {
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

  DOMTimeMilliSec DomLoading() {
    if (!nsContentUtils::IsPerformanceTimingEnabled()) {
      return 0;
    }
    return GetDOMTiming()->GetDomLoading();
  }
  DOMTimeMilliSec DomInteractive() const {
    if (!nsContentUtils::IsPerformanceTimingEnabled()) {
      return 0;
    }
    return GetDOMTiming()->GetDomInteractive();
  }
  DOMTimeMilliSec DomContentLoadedEventStart() const {
    if (!nsContentUtils::IsPerformanceTimingEnabled()) {
      return 0;
    }
    return GetDOMTiming()->GetDomContentLoadedEventStart();
  }
  DOMTimeMilliSec DomContentLoadedEventEnd() const {
    if (!nsContentUtils::IsPerformanceTimingEnabled()) {
      return 0;
    }
    return GetDOMTiming()->GetDomContentLoadedEventEnd();
  }
  DOMTimeMilliSec DomComplete() const {
    if (!nsContentUtils::IsPerformanceTimingEnabled()) {
      return 0;
    }
    return GetDOMTiming()->GetDomComplete();
  }
  DOMTimeMilliSec LoadEventStart() const {
    if (!nsContentUtils::IsPerformanceTimingEnabled()) {
      return 0;
    }
    return GetDOMTiming()->GetLoadEventStart();
  }
  DOMTimeMilliSec LoadEventEnd() const {
    if (!nsContentUtils::IsPerformanceTimingEnabled()) {
      return 0;
    }
    return GetDOMTiming()->GetLoadEventEnd();
  }

private:
  ~nsPerformanceTiming();
  bool IsInitialized() const;
  void InitializeTimingInfo(nsITimedChannel* aChannel);
  RefPtr<nsPerformance> mPerformance;
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

// Script "performance.navigation" object
class nsPerformanceNavigation final : public nsWrapperCache
{
public:
  explicit nsPerformanceNavigation(nsPerformance* aPerformance);
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(nsPerformanceNavigation)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(nsPerformanceNavigation)

  nsDOMNavigationTiming* GetDOMTiming() const;
  nsPerformanceTiming* GetPerformanceTiming() const;

  nsPerformance* GetParentObject() const
  {
    return mPerformance;
  }

  virtual JSObject* WrapObject(JSContext *cx, JS::Handle<JSObject*> aGivenProto) override;

  // PerformanceNavigation WebIDL methods
  uint16_t Type() const {
    return GetDOMTiming()->GetType();
  }
  uint16_t RedirectCount() const {
    return GetPerformanceTiming()->GetRedirectCount();
  }

private:
  ~nsPerformanceNavigation();
  RefPtr<nsPerformance> mPerformance;
};

// Base class for main-thread and worker Performance API
class PerformanceBase : public mozilla::DOMEventTargetHelper 
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(PerformanceBase,
                                           DOMEventTargetHelper)

  PerformanceBase();
  explicit PerformanceBase(nsPIDOMWindowInner* aWindow);

  typedef mozilla::dom::PerformanceEntry PerformanceEntry;
  typedef mozilla::dom::PerformanceObserver PerformanceObserver;

  void GetEntries(nsTArray<RefPtr<PerformanceEntry>>& aRetval);
  void GetEntriesByType(const nsAString& aEntryType,
                        nsTArray<RefPtr<PerformanceEntry>>& aRetval);
  void GetEntriesByName(const nsAString& aName,
                        const mozilla::dom::Optional<nsAString>& aEntryType,
                        nsTArray<RefPtr<PerformanceEntry>>& aRetval);
  void ClearResourceTimings();

  virtual DOMHighResTimeStamp Now() const = 0;

  void Mark(const nsAString& aName, mozilla::ErrorResult& aRv);
  void ClearMarks(const mozilla::dom::Optional<nsAString>& aName);
  void Measure(const nsAString& aName,
               const mozilla::dom::Optional<nsAString>& aStartMark,
               const mozilla::dom::Optional<nsAString>& aEndMark,
               mozilla::ErrorResult& aRv);
  void ClearMeasures(const mozilla::dom::Optional<nsAString>& aName);

  void SetResourceTimingBufferSize(uint64_t aMaxSize);

  void AddObserver(PerformanceObserver* aObserver);
  void RemoveObserver(PerformanceObserver* aObserver);
  void NotifyObservers();
  void CancelNotificationObservers();

protected:
  virtual ~PerformanceBase();

  virtual void InsertUserEntry(PerformanceEntry* aEntry);
  void InsertResourceEntry(PerformanceEntry* aEntry);

  void ClearUserEntries(const mozilla::dom::Optional<nsAString>& aEntryName,
                        const nsAString& aEntryType);

  DOMHighResTimeStamp ResolveTimestampFromName(const nsAString& aName,
                                               mozilla::ErrorResult& aRv);

  virtual nsISupports* GetAsISupports() = 0;

  virtual void DispatchBufferFullEvent() = 0;

  virtual mozilla::TimeStamp CreationTimeStamp() const = 0;

  virtual DOMHighResTimeStamp CreationTime() const = 0;

  virtual bool IsPerformanceTimingAttribute(const nsAString& aName) = 0;

  virtual DOMHighResTimeStamp
  GetPerformanceTimingFromString(const nsAString& aTimingName) = 0;

  bool IsResourceEntryLimitReached() const
  {
    return mResourceEntries.Length() >= mResourceTimingBufferSize;
  }

  void LogEntry(PerformanceEntry* aEntry, const nsACString& aOwner) const;
  void TimingNotification(PerformanceEntry* aEntry, const nsACString& aOwner, uint64_t epoch);

  void RunNotificationObserversTask();
  void QueueEntry(PerformanceEntry* aEntry);

  DOMHighResTimeStamp RoundTime(double aTime) const;

  nsTObserverArray<PerformanceObserver*> mObservers;

private:
  nsTArray<RefPtr<PerformanceEntry>> mUserEntries;
  nsTArray<RefPtr<PerformanceEntry>> mResourceEntries;

  uint64_t mResourceTimingBufferSize;
  static const uint64_t kDefaultResourceTimingBufferSize = 150;
  bool mPendingNotificationObserversTask;
};

// Script "performance" object
class nsPerformance final : public PerformanceBase
{
public:
  nsPerformance(nsPIDOMWindowInner* aWindow,
                nsDOMNavigationTiming* aDOMTiming,
                nsITimedChannel* aChannel,
                nsPerformance* aParentPerformance);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(nsPerformance,
                                                         PerformanceBase)

  static bool IsEnabled(JSContext* aCx, JSObject* aGlobal);

  static bool IsObserverEnabled(JSContext* aCx, JSObject* aGlobal);

  nsDOMNavigationTiming* GetDOMTiming() const
  {
    return mDOMTiming;
  }

  nsITimedChannel* GetChannel() const
  {
    return mChannel;
  }

  nsPerformance* GetParentPerformance() const
  {
    return mParentPerformance;
  }

  JSObject* WrapObject(JSContext *cx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // Performance WebIDL methods
  DOMHighResTimeStamp Now() const override;

  nsPerformanceTiming* Timing();
  nsPerformanceNavigation* Navigation();

  void AddEntry(nsIHttpChannel* channel,
                nsITimedChannel* timedChannel);

  using PerformanceBase::GetEntries;
  using PerformanceBase::GetEntriesByType;
  using PerformanceBase::GetEntriesByName;
  using PerformanceBase::ClearResourceTimings;

  using PerformanceBase::Mark;
  using PerformanceBase::ClearMarks;
  using PerformanceBase::Measure;
  using PerformanceBase::ClearMeasures;
  using PerformanceBase::SetResourceTimingBufferSize;

  void GetMozMemory(JSContext *aCx, JS::MutableHandle<JSObject*> aObj);

  IMPL_EVENT_HANDLER(resourcetimingbufferfull)

  mozilla::TimeStamp CreationTimeStamp() const override;

  DOMHighResTimeStamp CreationTime() const override;

protected:
  ~nsPerformance();

  nsISupports* GetAsISupports() override
  {
    return this;
  }

  void InsertUserEntry(PerformanceEntry* aEntry) override;

  bool IsPerformanceTimingAttribute(const nsAString& aName) override;

  DOMHighResTimeStamp
  GetPerformanceTimingFromString(const nsAString& aTimingName) override;

  void DispatchBufferFullEvent() override;

  RefPtr<nsDOMNavigationTiming> mDOMTiming;
  nsCOMPtr<nsITimedChannel> mChannel;
  RefPtr<nsPerformanceTiming> mTiming;
  RefPtr<nsPerformanceNavigation> mNavigation;
  RefPtr<nsPerformance> mParentPerformance;
  JS::Heap<JSObject*> mMozMemory;
};

inline nsDOMNavigationTiming*
nsPerformanceNavigation::GetDOMTiming() const
{
  return mPerformance->GetDOMTiming();
}

inline nsPerformanceTiming*
nsPerformanceNavigation::GetPerformanceTiming() const
{
  return mPerformance->Timing();
}

inline nsDOMNavigationTiming*
nsPerformanceTiming::GetDOMTiming() const
{
  return mPerformance->GetDOMTiming();
}

#endif /* nsPerformance_h___ */

