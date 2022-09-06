/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef RTCStatsReport_h_
#define RTCStatsReport_h_

#include "api/units/timestamp.h"  // webrtc::Timestamp
#include "js/RootingAPI.h"        // JS::Rooted
#include "js/Value.h"
#include "mozilla/dom/AutoEntryScript.h"
#include "mozilla/MozPromise.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/dom/PerformanceService.h"
#include "mozilla/dom/RTCStatsReportBinding.h"  // RTCStatsCollection
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/UniquePtr.h"
#include "nsCOMPtr.h"
#include "nsIGlobalObject.h"
#include "nsPIDOMWindow.h"  // nsPIDOMWindowInner
#include "nsWrapperCache.h"
#include "prtime.h"  // PR_Now

namespace mozilla {

extern TimeStamp WebrtcSystemTimeBase();

namespace dom {

/**
 * Class that facilitates creating timestamps for webrtc stats by mimicking
 * dom::Performance, as well as getting and converting timestamps for libwebrtc
 * and our integration with it.
 *
 * It converts and uses time from these clocks:
 * - Moz      : Monotonic, unspecified (but constant) and inaccessible epoch, as
 *              implemented by mozilla::TimeStamp
 * - Realtime : Monotonic, unspecified (but constant) epoch.
 * - 1Jan1970 : Monotonic, unix epoch (00:00:00 UTC on 1 January 1970).
 * - Ntp      : Monotonic, ntp epoch (00:00:00 UTC on 1 January 1900).
 * - Dom      : Monotonic, milliseconds since unix epoch, as the timestamps
 *              defined by webrtc-pc. Corresponds to Performance.timeOrigin +
 *              Performance.now().
 * - WallClock: Non-monotonic, unix epoch.
 */
class RTCStatsTimestampMaker {
 public:
  RTCStatsTimestampMaker();
  explicit RTCStatsTimestampMaker(nsPIDOMWindowInner* aWindow);

  DOMHighResTimeStamp GetNow() const;

  webrtc::Timestamp GetNowRealtime() const;
  webrtc::Timestamp ConvertMozTimeToRealtime(TimeStamp aMozTime) const;
  webrtc::Timestamp ConvertRealtimeTo1Jan1970(
      webrtc::Timestamp aRealtime) const;
  DOMHighResTimeStamp ConvertNtpToDomTime(webrtc::Timestamp aNtpTime) const;
  DOMHighResTimeStamp ReduceRealtimePrecision(
      webrtc::Timestamp aRealtime) const;

  const uint64_t mRandomTimelineSeed;
  const TimeStamp mStartRealtime;
  const bool mCrossOriginIsolated;
  const DOMHighResTimeStamp mStartWallClockRaw;
};

// TODO(bug 1588303): If we ever get move semantics for webidl dictionaries, we
// can stop wrapping these in UniquePtr, which will allow us to simplify code
// in several places.
typedef MozPromise<UniquePtr<RTCStatsCollection>, nsresult, true>
    RTCStatsPromise;

typedef MozPromise<UniquePtr<RTCStatsReportInternal>, nsresult, true>
    RTCStatsReportPromise;

class RTCStatsReport final : public nsWrapperCache {
 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(RTCStatsReport)
  NS_DECL_CYCLE_COLLECTION_NATIVE_WRAPPERCACHE_CLASS(RTCStatsReport)

  explicit RTCStatsReport(nsPIDOMWindowInner* aParent);

  // TODO(bug 1586109): Remove this once we no longer have to create empty
  // RTCStatsReports from JS.
  static already_AddRefed<RTCStatsReport> Constructor(
      const GlobalObject& aGlobal);

  void Incorporate(RTCStatsCollection& aStats);

  nsPIDOMWindowInner* GetParentObject() const { return mParent; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

 private:
  ~RTCStatsReport() = default;
  void Set(const nsAString& aKey, JS::Handle<JSObject*> aValue,
           ErrorResult& aRv);

  template <typename T>
  nsresult SetRTCStats(Sequence<T>& aValues) {
    for (T& value : aValues) {
      nsresult rv = SetRTCStats(value);
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
    return NS_OK;
  }

  // We cannot just declare this as SetRTCStats(RTCStats&), because the
  // conversion function that ToJSValue uses is non-virtual.
  template <typename T>
  nsresult SetRTCStats(T& aValue) {
    static_assert(std::is_base_of<RTCStats, T>::value,
                  "SetRTCStats is for setting RTCStats only");

    if (!aValue.mId.WasPassed()) {
      return NS_OK;
    }

    const nsString key(aValue.mId.Value());

    // Cargo-culted from dom::Promise; converts aValue to a JSObject
    AutoEntryScript aes(mParent->AsGlobal()->GetGlobalJSObject(),
                        "RTCStatsReport::SetRTCStats");
    JSContext* cx = aes.cx();
    JS::Rooted<JS::Value> val(cx);
    if (!ToJSValue(cx, std::forward<T>(aValue), &val)) {
      return NS_ERROR_FAILURE;
    }
    JS::Rooted<JSObject*> jsObject(cx, &val.toObject());

    ErrorResult rv;
    Set(key, jsObject, rv);
    return rv.StealNSResult();
  }

  nsCOMPtr<nsPIDOMWindowInner> mParent;
};

void MergeStats(UniquePtr<dom::RTCStatsCollection> aFromStats,
                dom::RTCStatsCollection* aIntoStats);

void FlattenStats(nsTArray<UniquePtr<dom::RTCStatsCollection>> aFromStats,
                  dom::RTCStatsCollection* aIntoStats);

}  // namespace dom
}  // namespace mozilla

#endif  // RTCStatsReport_h_
