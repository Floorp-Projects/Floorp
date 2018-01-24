/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PerformanceResourceTiming_h___
#define mozilla_dom_PerformanceResourceTiming_h___

#include "mozilla/UniquePtr.h"
#include "nsCOMPtr.h"
#include "Performance.h"
#include "PerformanceEntry.h"
#include "PerformanceTiming.h"

namespace mozilla {
namespace dom {

// http://www.w3.org/TR/resource-timing/#performanceresourcetiming
class PerformanceResourceTiming : public PerformanceEntry
{
public:
  typedef mozilla::TimeStamp TimeStamp;

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(
      PerformanceResourceTiming,
      PerformanceEntry)

  PerformanceResourceTiming(UniquePtr<PerformanceTimingData>&& aPerformanceTimingData,
                            Performance* aPerformance,
                            const nsAString& aName);

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;


  virtual DOMHighResTimeStamp StartTime() const override;

  virtual DOMHighResTimeStamp Duration() const override
  {
    return ResponseEnd() - StartTime();
  }

  void GetInitiatorType(nsAString& aInitiatorType) const
  {
    aInitiatorType = mInitiatorType;
  }

  void SetInitiatorType(const nsAString& aInitiatorType)
  {
    mInitiatorType = aInitiatorType;
  }

  void GetNextHopProtocol(nsAString& aNextHopProtocol) const
  {
    if (mTimingData) {
      aNextHopProtocol = mTimingData->NextHopProtocol();
    }
  }

  DOMHighResTimeStamp WorkerStart() const {
    return mTimingData
        ? mTimingData->WorkerStartHighRes(mPerformance)
        : 0;
  }

  DOMHighResTimeStamp FetchStart() const {
    return mTimingData
        ? mTimingData->FetchStartHighRes(mPerformance)
        : 0;
  }

  DOMHighResTimeStamp RedirectStart() const {
    // We have to check if all the redirect URIs had the same origin (since
    // there is no check in RedirectEndHighRes())
    return mTimingData && mTimingData->ShouldReportCrossOriginRedirect()
        ? mTimingData->RedirectStartHighRes(mPerformance)
        : 0;
  }

  DOMHighResTimeStamp RedirectEnd() const {
    // We have to check if all the redirect URIs had the same origin (since
    // there is no check in RedirectEndHighRes())
    return mTimingData && mTimingData->ShouldReportCrossOriginRedirect()
        ? mTimingData->RedirectEndHighRes(mPerformance)
        : 0;
  }

  DOMHighResTimeStamp DomainLookupStart() const {
    return mTimingData && mTimingData->TimingAllowed()
        ? mTimingData->DomainLookupStartHighRes(mPerformance)
        : 0;
  }

  DOMHighResTimeStamp DomainLookupEnd() const {
    return mTimingData && mTimingData->TimingAllowed()
        ? mTimingData->DomainLookupEndHighRes(mPerformance)
        : 0;
  }

  DOMHighResTimeStamp ConnectStart() const {
    return mTimingData && mTimingData->TimingAllowed()
        ? mTimingData->ConnectStartHighRes(mPerformance)
        : 0;
  }

  DOMHighResTimeStamp ConnectEnd() const {
    return mTimingData && mTimingData->TimingAllowed()
        ? mTimingData->ConnectEndHighRes(mPerformance)
        : 0;
  }

  DOMHighResTimeStamp RequestStart() const {
    return mTimingData && mTimingData->TimingAllowed()
        ? mTimingData->RequestStartHighRes(mPerformance)
        : 0;
  }

  DOMHighResTimeStamp ResponseStart() const {
    return mTimingData && mTimingData->TimingAllowed()
        ? mTimingData->ResponseStartHighRes(mPerformance)
        : 0;
  }

  DOMHighResTimeStamp ResponseEnd() const {
    return mTimingData
        ? mTimingData->ResponseEndHighRes(mPerformance)
        : 0;
  }

  DOMHighResTimeStamp SecureConnectionStart() const
  {
    return mTimingData && mTimingData->TimingAllowed()
        ?  mTimingData->SecureConnectionStartHighRes(mPerformance)
        : 0;
  }

  virtual const PerformanceResourceTiming* ToResourceTiming() const override
  {
    return this;
  }

  uint64_t TransferSize() const
  {
    return mTimingData ? mTimingData->TransferSize() : 0;
  }

  uint64_t EncodedBodySize() const
  {
    return mTimingData ? mTimingData->EncodedBodySize() : 0;
  }

  uint64_t DecodedBodySize() const
  {
    return mTimingData ? mTimingData->DecodedBodySize() : 0;
  }

  size_t
  SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const override;

protected:
  virtual ~PerformanceResourceTiming();

  size_t
  SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const override;

  nsString mInitiatorType;
  UniquePtr<PerformanceTimingData> mTimingData;
  RefPtr<Performance> mPerformance;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_PerformanceResourceTiming_h___ */
