/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PerformanceTiming_h
#define mozilla_dom_PerformanceTiming_h

#include "mozilla/Attributes.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/StaticPrefs_dom.h"
#include "nsContentUtils.h"
#include "nsDOMNavigationTiming.h"
#include "nsRFPService.h"
#include "nsWrapperCache.h"
#include "Performance.h"
#include "nsITimedChannel.h"
#include "mozilla/dom/PerformanceTimingTypes.h"
#include "mozilla/ipc/IPDLParamTraits.h"
#include "ipc/IPCMessageUtils.h"
#include "ipc/IPCMessageUtilsSpecializations.h"
#include "mozilla/net/nsServerTiming.h"

class nsIHttpChannel;

namespace mozilla {
namespace dom {

class PerformanceTiming;

class PerformanceTimingData final {
  friend class PerformanceTiming;
  friend struct mozilla::ipc::IPDLParamTraits<
      mozilla::dom::PerformanceTimingData>;

 public:
  PerformanceTimingData() = default;  // For deserialization
  // This can return null.
  static PerformanceTimingData* Create(nsITimedChannel* aChannel,
                                       nsIHttpChannel* aHttpChannel,
                                       DOMHighResTimeStamp aZeroTime,
                                       nsAString& aInitiatorType,
                                       nsAString& aEntryName);

  PerformanceTimingData(nsITimedChannel* aChannel, nsIHttpChannel* aHttpChannel,
                        DOMHighResTimeStamp aZeroTime);

  explicit PerformanceTimingData(const IPCPerformanceTimingData& aIPCData);

  IPCPerformanceTimingData ToIPC();

  void SetPropertiesFromHttpChannel(nsIHttpChannel* aHttpChannel,
                                    nsITimedChannel* aChannel);

  bool IsInitialized() const { return mInitialized; }

  const nsString& NextHopProtocol() const { return mNextHopProtocol; }

  uint64_t TransferSize() const { return mTransferSize; }

  uint64_t EncodedBodySize() const { return mEncodedBodySize; }

  uint64_t DecodedBodySize() const { return mDecodedBodySize; }

  /**
   * @param   aStamp
   *          The TimeStamp recorded for a specific event. This TimeStamp can
   *          be null.
   * @return  the duration of an event with a given TimeStamp, relative to the
   *          navigationStart TimeStamp (the moment the user landed on the
   *          page), if the given TimeStamp is valid. Otherwise, it will return
   *          the FetchStart timing value.
   */
  inline DOMHighResTimeStamp TimeStampToReducedDOMHighResOrFetchStart(
      Performance* aPerformance, TimeStamp aStamp) {
    MOZ_ASSERT(aPerformance);

    if (aStamp.IsNull()) {
      return FetchStartHighRes(aPerformance);
    }

    DOMHighResTimeStamp rawTimestamp =
        TimeStampToDOMHighRes(aPerformance, aStamp);

    return nsRFPService::ReduceTimePrecisionAsMSecs(
        rawTimestamp, aPerformance->GetRandomTimelineSeed(),
        aPerformance->IsSystemPrincipal(), aPerformance->CrossOriginIsolated());
  }

  /**
   * The nsITimedChannel records an absolute timestamp for each event.
   * The nsDOMNavigationTiming will record the moment when the user landed on
   * the page. This is a window.performance unique timestamp, so it can be used
   * for all the events (navigation timing and resource timing events).
   *
   * The algorithm operates in 2 steps:
   * 1. The first step is to subtract the two timestamps: the argument (the
   * event's timestamp) and the navigation start timestamp. This will result in
   * a relative timestamp of the event (relative to the navigation start -
   * window.performance.timing.navigationStart).
   * 2. The second step is to add any required offset (the mZeroTime). For now,
   * this offset value is either 0 (for the resource timing), or equal to
   * "performance.navigationStart" (for navigation timing).
   * For the resource timing, mZeroTime is set to 0, causing the result to be a
   * relative time.
   * For the navigation timing, mZeroTime is set to
   * "performance.navigationStart" causing the result be an absolute time.
   *
   * @param   aStamp
   *          The TimeStamp recorded for a specific event. This TimeStamp can't
   *          be null.
   * @return  number of milliseconds value as one of:
   * - relative to the navigation start time, time the user has landed on the
   *   page
   * - an absolute wall clock time since the unix epoch
   */
  inline DOMHighResTimeStamp TimeStampToDOMHighRes(Performance* aPerformance,
                                                   TimeStamp aStamp) const {
    MOZ_ASSERT(aPerformance);
    MOZ_ASSERT(!aStamp.IsNull());

    TimeDuration duration = aStamp - aPerformance->CreationTimeStamp();
    return duration.ToMilliseconds() + mZeroTime;
  }

  // The last channel's AsyncOpen time.  This may occur before the FetchStart
  // in some cases.
  DOMHighResTimeStamp AsyncOpenHighRes(Performance* aPerformance);

  // High resolution (used by resource timing)
  DOMHighResTimeStamp WorkerStartHighRes(Performance* aPerformance);
  DOMHighResTimeStamp FetchStartHighRes(Performance* aPerformance);
  DOMHighResTimeStamp RedirectStartHighRes(Performance* aPerformance);
  DOMHighResTimeStamp RedirectEndHighRes(Performance* aPerformance);
  DOMHighResTimeStamp DomainLookupStartHighRes(Performance* aPerformance);
  DOMHighResTimeStamp DomainLookupEndHighRes(Performance* aPerformance);
  DOMHighResTimeStamp ConnectStartHighRes(Performance* aPerformance);
  DOMHighResTimeStamp SecureConnectionStartHighRes(Performance* aPerformance);
  DOMHighResTimeStamp ConnectEndHighRes(Performance* aPerformance);
  DOMHighResTimeStamp RequestStartHighRes(Performance* aPerformance);
  DOMHighResTimeStamp ResponseStartHighRes(Performance* aPerformance);
  DOMHighResTimeStamp ResponseEndHighRes(Performance* aPerformance);

  DOMHighResTimeStamp ZeroTime() const { return mZeroTime; }

  uint8_t RedirectCountReal() const { return mRedirectCount; }
  uint8_t GetRedirectCount() const;

  bool AllRedirectsSameOrigin() const { return mAllRedirectsSameOrigin; }

  // If this is false the values of redirectStart/End will be 0 This is false if
  // no redirects occured, or if any of the responses failed the
  // timing-allow-origin check in HttpBaseChannel::TimingAllowCheck
  //
  // If aEnsureSameOriginAndIgnoreTAO is false, it checks if all redirects pass
  // TAO. When it is true, it checks if all redirects are same-origin and
  // ignores the result of TAO.
  bool ShouldReportCrossOriginRedirect(
      bool aEnsureSameOriginAndIgnoreTAO) const;

  // Cached result of CheckAllowedOrigin. If false, security sensitive
  // attributes of the resourceTiming object will be set to 0
  bool TimingAllowed() const { return mTimingAllowed; }

  nsTArray<nsCOMPtr<nsIServerTiming>> GetServerTiming();

 private:
  // Checks if the resource is either same origin as the page that started
  // the load, or if the response contains the Timing-Allow-Origin header
  // with a value of * or matching the domain of the loading Principal
  bool CheckAllowedOrigin(nsIHttpChannel* aResourceChannel,
                          nsITimedChannel* aChannel);

  nsTArray<nsCOMPtr<nsIServerTiming>> mServerTiming;
  nsString mNextHopProtocol;

  TimeStamp mAsyncOpen;
  TimeStamp mRedirectStart;
  TimeStamp mRedirectEnd;
  TimeStamp mDomainLookupStart;
  TimeStamp mDomainLookupEnd;
  TimeStamp mConnectStart;
  TimeStamp mSecureConnectionStart;
  TimeStamp mConnectEnd;
  TimeStamp mRequestStart;
  TimeStamp mResponseStart;
  TimeStamp mCacheReadStart;
  TimeStamp mResponseEnd;
  TimeStamp mCacheReadEnd;

  // ServiceWorker interception timing information
  TimeStamp mWorkerStart;
  TimeStamp mWorkerRequestStart;
  TimeStamp mWorkerResponseEnd;

  // This is an offset that will be added to each timing ([ms] resolution).
  // There are only 2 possible values: (1) logicaly equal to navigationStart
  // TimeStamp (results are absolute timstamps - wallclock); (2) "0" (results
  // are relative to the navigation start).
  DOMHighResTimeStamp mZeroTime = 0;

  DOMHighResTimeStamp mFetchStart = 0;

  uint64_t mEncodedBodySize = 0;
  uint64_t mTransferSize = 0;
  uint64_t mDecodedBodySize = 0;

  uint8_t mRedirectCount = 0;

  bool mAllRedirectsSameOrigin = false;

  bool mAllRedirectsPassTAO = false;

  bool mSecureConnection = false;

  bool mTimingAllowed = false;

  bool mInitialized = false;
};

// Script "performance.timing" object
class PerformanceTiming final : public nsWrapperCache {
 public:
  /**
   * @param   aPerformance
   *          The performance object (the JS parent).
   *          This will allow access to "window.performance.timing" attribute
   * for the navigation timing (can't be null).
   * @param   aChannel
   *          An nsITimedChannel used to gather all the networking timings by
   * both the navigation timing and the resource timing (can't be null).
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
  PerformanceTiming(Performance* aPerformance, nsITimedChannel* aChannel,
                    nsIHttpChannel* aHttpChannel,
                    DOMHighResTimeStamp aZeroTime);
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(PerformanceTiming)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(PerformanceTiming)

  nsDOMNavigationTiming* GetDOMTiming() const {
    return mPerformance->GetDOMTiming();
  }

  Performance* GetParentObject() const { return mPerformance; }

  virtual JSObject* WrapObject(JSContext* cx,
                               JS::Handle<JSObject*> aGivenProto) override;

  // PerformanceNavigation WebIDL methods
  DOMTimeMilliSec NavigationStart() const {
    if (!StaticPrefs::dom_enable_performance()) {
      return 0;
    }
    return nsRFPService::ReduceTimePrecisionAsMSecs(
        GetDOMTiming()->GetNavigationStart(),
        mPerformance->GetRandomTimelineSeed(),
        mPerformance->IsSystemPrincipal(), mPerformance->CrossOriginIsolated());
  }

  DOMTimeMilliSec UnloadEventStart() {
    if (!StaticPrefs::dom_enable_performance()) {
      return 0;
    }
    return nsRFPService::ReduceTimePrecisionAsMSecs(
        GetDOMTiming()->GetUnloadEventStart(),
        mPerformance->GetRandomTimelineSeed(),
        mPerformance->IsSystemPrincipal(), mPerformance->CrossOriginIsolated());
  }

  DOMTimeMilliSec UnloadEventEnd() {
    if (!StaticPrefs::dom_enable_performance()) {
      return 0;
    }
    return nsRFPService::ReduceTimePrecisionAsMSecs(
        GetDOMTiming()->GetUnloadEventEnd(),
        mPerformance->GetRandomTimelineSeed(),
        mPerformance->IsSystemPrincipal(), mPerformance->CrossOriginIsolated());
  }

  // Low resolution (used by navigation timing)
  DOMTimeMilliSec FetchStart();
  DOMTimeMilliSec RedirectStart();
  DOMTimeMilliSec RedirectEnd();
  DOMTimeMilliSec DomainLookupStart();
  DOMTimeMilliSec DomainLookupEnd();
  DOMTimeMilliSec ConnectStart();
  DOMTimeMilliSec SecureConnectionStart();
  DOMTimeMilliSec ConnectEnd();
  DOMTimeMilliSec RequestStart();
  DOMTimeMilliSec ResponseStart();
  DOMTimeMilliSec ResponseEnd();

  DOMTimeMilliSec DomLoading() {
    if (!StaticPrefs::dom_enable_performance()) {
      return 0;
    }
    return nsRFPService::ReduceTimePrecisionAsMSecs(
        GetDOMTiming()->GetDomLoading(), mPerformance->GetRandomTimelineSeed(),
        mPerformance->IsSystemPrincipal(), mPerformance->CrossOriginIsolated());
  }

  DOMTimeMilliSec DomInteractive() const {
    if (!StaticPrefs::dom_enable_performance()) {
      return 0;
    }
    return nsRFPService::ReduceTimePrecisionAsMSecs(
        GetDOMTiming()->GetDomInteractive(),
        mPerformance->GetRandomTimelineSeed(),
        mPerformance->IsSystemPrincipal(), mPerformance->CrossOriginIsolated());
  }

  DOMTimeMilliSec DomContentLoadedEventStart() const {
    if (!StaticPrefs::dom_enable_performance()) {
      return 0;
    }
    return nsRFPService::ReduceTimePrecisionAsMSecs(
        GetDOMTiming()->GetDomContentLoadedEventStart(),
        mPerformance->GetRandomTimelineSeed(),
        mPerformance->IsSystemPrincipal(), mPerformance->CrossOriginIsolated());
  }

  DOMTimeMilliSec DomContentLoadedEventEnd() const {
    if (!StaticPrefs::dom_enable_performance()) {
      return 0;
    }
    return nsRFPService::ReduceTimePrecisionAsMSecs(
        GetDOMTiming()->GetDomContentLoadedEventEnd(),
        mPerformance->GetRandomTimelineSeed(),
        mPerformance->IsSystemPrincipal(), mPerformance->CrossOriginIsolated());
  }

  DOMTimeMilliSec DomComplete() const {
    if (!StaticPrefs::dom_enable_performance()) {
      return 0;
    }
    return nsRFPService::ReduceTimePrecisionAsMSecs(
        GetDOMTiming()->GetDomComplete(), mPerformance->GetRandomTimelineSeed(),
        mPerformance->IsSystemPrincipal(), mPerformance->CrossOriginIsolated());
  }

  DOMTimeMilliSec LoadEventStart() const {
    if (!StaticPrefs::dom_enable_performance()) {
      return 0;
    }
    return nsRFPService::ReduceTimePrecisionAsMSecs(
        GetDOMTiming()->GetLoadEventStart(),
        mPerformance->GetRandomTimelineSeed(),
        mPerformance->IsSystemPrincipal(), mPerformance->CrossOriginIsolated());
  }

  DOMTimeMilliSec LoadEventEnd() const {
    if (!StaticPrefs::dom_enable_performance()) {
      return 0;
    }
    return nsRFPService::ReduceTimePrecisionAsMSecs(
        GetDOMTiming()->GetLoadEventEnd(),
        mPerformance->GetRandomTimelineSeed(),
        mPerformance->IsSystemPrincipal(), mPerformance->CrossOriginIsolated());
  }

  DOMTimeMilliSec TimeToNonBlankPaint() const {
    if (!StaticPrefs::dom_enable_performance()) {
      return 0;
    }
    return nsRFPService::ReduceTimePrecisionAsMSecs(
        GetDOMTiming()->GetTimeToNonBlankPaint(),
        mPerformance->GetRandomTimelineSeed(),
        mPerformance->IsSystemPrincipal(), mPerformance->CrossOriginIsolated());
  }

  DOMTimeMilliSec TimeToContentfulPaint() const {
    if (!StaticPrefs::dom_enable_performance()) {
      return 0;
    }
    return nsRFPService::ReduceTimePrecisionAsMSecs(
        GetDOMTiming()->GetTimeToContentfulComposite(),
        mPerformance->GetRandomTimelineSeed(),
        mPerformance->IsSystemPrincipal(), mPerformance->CrossOriginIsolated());
  }

  DOMTimeMilliSec TimeToDOMContentFlushed() const {
    if (!StaticPrefs::dom_enable_performance()) {
      return 0;
    }
    return nsRFPService::ReduceTimePrecisionAsMSecs(
        GetDOMTiming()->GetTimeToDOMContentFlushed(),
        mPerformance->GetRandomTimelineSeed(),
        mPerformance->IsSystemPrincipal(), mPerformance->CrossOriginIsolated());
  }

  DOMTimeMilliSec TimeToFirstInteractive() const {
    if (!StaticPrefs::dom_enable_performance()) {
      return 0;
    }
    return nsRFPService::ReduceTimePrecisionAsMSecs(
        GetDOMTiming()->GetTimeToTTFI(), mPerformance->GetRandomTimelineSeed(),
        mPerformance->IsSystemPrincipal(), mPerformance->CrossOriginIsolated());
  }

  PerformanceTimingData* Data() const { return mTimingData.get(); }

 private:
  ~PerformanceTiming();

  bool IsTopLevelContentDocument() const;

  RefPtr<Performance> mPerformance;

  UniquePtr<PerformanceTimingData> mTimingData;
};

}  // namespace dom
}  // namespace mozilla

namespace mozilla {
namespace ipc {

template <>
struct IPDLParamTraits<mozilla::dom::PerformanceTimingData> {
  using paramType = mozilla::dom::PerformanceTimingData;
  static void Write(IPC::Message* aMsg, IProtocol* aActor,
                    const paramType& aParam) {
    WriteIPDLParam(aMsg, aActor, aParam.mServerTiming);
    WriteIPDLParam(aMsg, aActor, aParam.mNextHopProtocol);
    WriteIPDLParam(aMsg, aActor, aParam.mAsyncOpen);
    WriteIPDLParam(aMsg, aActor, aParam.mRedirectStart);
    WriteIPDLParam(aMsg, aActor, aParam.mRedirectEnd);
    WriteIPDLParam(aMsg, aActor, aParam.mDomainLookupStart);
    WriteIPDLParam(aMsg, aActor, aParam.mDomainLookupEnd);
    WriteIPDLParam(aMsg, aActor, aParam.mConnectStart);
    WriteIPDLParam(aMsg, aActor, aParam.mSecureConnectionStart);
    WriteIPDLParam(aMsg, aActor, aParam.mConnectEnd);
    WriteIPDLParam(aMsg, aActor, aParam.mRequestStart);
    WriteIPDLParam(aMsg, aActor, aParam.mResponseStart);
    WriteIPDLParam(aMsg, aActor, aParam.mCacheReadStart);
    WriteIPDLParam(aMsg, aActor, aParam.mResponseEnd);
    WriteIPDLParam(aMsg, aActor, aParam.mCacheReadEnd);
    WriteIPDLParam(aMsg, aActor, aParam.mWorkerStart);
    WriteIPDLParam(aMsg, aActor, aParam.mWorkerRequestStart);
    WriteIPDLParam(aMsg, aActor, aParam.mWorkerResponseEnd);
    WriteIPDLParam(aMsg, aActor, aParam.mZeroTime);
    WriteIPDLParam(aMsg, aActor, aParam.mFetchStart);
    WriteIPDLParam(aMsg, aActor, aParam.mEncodedBodySize);
    WriteIPDLParam(aMsg, aActor, aParam.mTransferSize);
    WriteIPDLParam(aMsg, aActor, aParam.mDecodedBodySize);
    WriteIPDLParam(aMsg, aActor, aParam.mRedirectCount);
    WriteIPDLParam(aMsg, aActor, aParam.mAllRedirectsSameOrigin);
    WriteIPDLParam(aMsg, aActor, aParam.mAllRedirectsPassTAO);
    WriteIPDLParam(aMsg, aActor, aParam.mSecureConnection);
    WriteIPDLParam(aMsg, aActor, aParam.mTimingAllowed);
    WriteIPDLParam(aMsg, aActor, aParam.mInitialized);
  }

  static bool Read(const IPC::Message* aMsg, PickleIterator* aIter,
                   IProtocol* aActor, paramType* aResult) {
    if (!ReadIPDLParam(aMsg, aIter, aActor, &aResult->mServerTiming)) {
      return false;
    }
    if (!ReadIPDLParam(aMsg, aIter, aActor, &aResult->mNextHopProtocol)) {
      return false;
    }
    if (!ReadIPDLParam(aMsg, aIter, aActor, &aResult->mAsyncOpen)) {
      return false;
    }
    if (!ReadIPDLParam(aMsg, aIter, aActor, &aResult->mRedirectStart)) {
      return false;
    }
    if (!ReadIPDLParam(aMsg, aIter, aActor, &aResult->mRedirectEnd)) {
      return false;
    }
    if (!ReadIPDLParam(aMsg, aIter, aActor, &aResult->mDomainLookupStart)) {
      return false;
    }
    if (!ReadIPDLParam(aMsg, aIter, aActor, &aResult->mDomainLookupEnd)) {
      return false;
    }
    if (!ReadIPDLParam(aMsg, aIter, aActor, &aResult->mConnectStart)) {
      return false;
    }
    if (!ReadIPDLParam(aMsg, aIter, aActor, &aResult->mSecureConnectionStart)) {
      return false;
    }
    if (!ReadIPDLParam(aMsg, aIter, aActor, &aResult->mConnectEnd)) {
      return false;
    }
    if (!ReadIPDLParam(aMsg, aIter, aActor, &aResult->mRequestStart)) {
      return false;
    }
    if (!ReadIPDLParam(aMsg, aIter, aActor, &aResult->mResponseStart)) {
      return false;
    }
    if (!ReadIPDLParam(aMsg, aIter, aActor, &aResult->mCacheReadStart)) {
      return false;
    }
    if (!ReadIPDLParam(aMsg, aIter, aActor, &aResult->mResponseEnd)) {
      return false;
    }
    if (!ReadIPDLParam(aMsg, aIter, aActor, &aResult->mCacheReadEnd)) {
      return false;
    }
    if (!ReadIPDLParam(aMsg, aIter, aActor, &aResult->mWorkerStart)) {
      return false;
    }
    if (!ReadIPDLParam(aMsg, aIter, aActor, &aResult->mWorkerRequestStart)) {
      return false;
    }
    if (!ReadIPDLParam(aMsg, aIter, aActor, &aResult->mWorkerResponseEnd)) {
      return false;
    }
    if (!ReadIPDLParam(aMsg, aIter, aActor, &aResult->mZeroTime)) {
      return false;
    }
    if (!ReadIPDLParam(aMsg, aIter, aActor, &aResult->mFetchStart)) {
      return false;
    }
    if (!ReadIPDLParam(aMsg, aIter, aActor, &aResult->mEncodedBodySize)) {
      return false;
    }
    if (!ReadIPDLParam(aMsg, aIter, aActor, &aResult->mTransferSize)) {
      return false;
    }
    if (!ReadIPDLParam(aMsg, aIter, aActor, &aResult->mDecodedBodySize)) {
      return false;
    }
    if (!ReadIPDLParam(aMsg, aIter, aActor, &aResult->mRedirectCount)) {
      return false;
    }
    if (!ReadIPDLParam(aMsg, aIter, aActor,
                       &aResult->mAllRedirectsSameOrigin)) {
      return false;
    }
    if (!ReadIPDLParam(aMsg, aIter, aActor, &aResult->mAllRedirectsPassTAO)) {
      return false;
    }
    if (!ReadIPDLParam(aMsg, aIter, aActor, &aResult->mSecureConnection)) {
      return false;
    }
    if (!ReadIPDLParam(aMsg, aIter, aActor, &aResult->mTimingAllowed)) {
      return false;
    }
    if (!ReadIPDLParam(aMsg, aIter, aActor, &aResult->mInitialized)) {
      return false;
    }
    return true;
  }
};

template <>
struct IPDLParamTraits<nsCOMPtr<nsIServerTiming>> {
  using paramType = nsCOMPtr<nsIServerTiming>;
  static void Write(IPC::Message* aMsg, IProtocol* aActor,
                    const paramType& aParam) {
    nsAutoCString name;
    Unused << aParam->GetName(name);
    double duration = 0;
    Unused << aParam->GetDuration(&duration);
    nsAutoCString description;
    Unused << aParam->GetDescription(description);
    WriteIPDLParam(aMsg, aActor, name);
    WriteIPDLParam(aMsg, aActor, duration);
    WriteIPDLParam(aMsg, aActor, description);
  }

  static bool Read(const IPC::Message* aMsg, PickleIterator* aIter,
                   IProtocol* aActor, paramType* aResult) {
    nsAutoCString name;
    double duration;
    nsAutoCString description;
    if (!ReadIPDLParam(aMsg, aIter, aActor, &name)) {
      return false;
    }
    if (!ReadIPDLParam(aMsg, aIter, aActor, &duration)) {
      return false;
    }
    if (!ReadIPDLParam(aMsg, aIter, aActor, &description)) {
      return false;
    }

    RefPtr<nsServerTiming> timing = new nsServerTiming();
    timing->SetName(name);
    timing->SetDuration(duration);
    timing->SetDescription(description);
    *aResult = timing;
    return true;
  }
};

}  // namespace ipc
}  // namespace mozilla

#endif  // mozilla_dom_PerformanceTiming_h
