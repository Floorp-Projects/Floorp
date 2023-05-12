/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "RTCStatsReport.h"
#include "libwebrtcglue/SystemTime.h"
#include "mozilla/dom/Performance.h"
#include "nsRFPService.h"
#include "WebrtcGlobal.h"

namespace mozilla::dom {

RTCStatsTimestampState::RTCStatsTimestampState()
    : mRandomTimelineSeed(0),
      mStartDomRealtime(WebrtcSystemTimeBase()),
      mStartRealtime(
          WebrtcSystemTime() -
          webrtc::TimeDelta::Micros(
              (TimeStamp::Now() - mStartDomRealtime).ToMicroseconds())),
      mRTPCallerType(RTPCallerType::Normal),
      mStartWallClockRaw(
          PerformanceService::GetOrCreate()->TimeOrigin(mStartDomRealtime)) {}

RTCStatsTimestampState::RTCStatsTimestampState(Performance& aPerformance)
    : mRandomTimelineSeed(aPerformance.GetRandomTimelineSeed()),
      mStartDomRealtime(aPerformance.CreationTimeStamp()),
      mStartRealtime(
          WebrtcSystemTime() -
          webrtc::TimeDelta::Micros(
              (TimeStamp::Now() - mStartDomRealtime).ToMicroseconds())),
      mRTPCallerType(aPerformance.GetRTPCallerType()),
      mStartWallClockRaw(
          PerformanceService::GetOrCreate()->TimeOrigin(mStartDomRealtime)) {}

TimeStamp RTCStatsTimestamp::ToMozTime() const { return mMozTime; }

webrtc::Timestamp RTCStatsTimestamp::ToRealtime() const {
  return ToDomRealtime() +
         webrtc::TimeDelta::Micros(mState.mStartRealtime.us());
}

webrtc::Timestamp RTCStatsTimestamp::To1Jan1970() const {
  return ToDomRealtime() + webrtc::TimeDelta::Millis(mState.mStartWallClockRaw);
}

webrtc::Timestamp RTCStatsTimestamp::ToNtp() const {
  return To1Jan1970() + webrtc::TimeDelta::Seconds(webrtc::kNtpJan1970);
}

webrtc::Timestamp RTCStatsTimestamp::ToDomRealtime() const {
  return webrtc::Timestamp::Micros(
      (mMozTime - mState.mStartDomRealtime).ToMicroseconds());
}

DOMHighResTimeStamp RTCStatsTimestamp::ToDom() const {
  // webrtc-pc says to use performance.timeOrigin + performance.now(), but
  // keeping a Performance object around is difficult because it is
  // main-thread-only. So, we perform the same calculation here. Note that this
  // can be very different from the current wall-clock time because of changes
  // to the wall clock, or monotonic clock drift over long periods of time.
  // We are very careful to do exactly what Performance does, to avoid timestamp
  // discrepancies.

  DOMHighResTimeStamp realtime = ToDomRealtime().ms<double>();
  // mRandomTimelineSeed is not set in the unit-tests.
  if (mState.mRandomTimelineSeed) {
    realtime = nsRFPService::ReduceTimePrecisionAsMSecs(
        realtime, mState.mRandomTimelineSeed, mState.mRTPCallerType);
  }

  // Ugh. Performance::TimeOrigin is not constant, which means we need to
  // emulate this weird behavior so our time stamps are consistent with JS
  // timeOrigin. This is based on the code here:
  // https://searchfox.org/mozilla-central/rev/
  // 053826b10f838f77c27507e5efecc96e34718541/dom/performance/Performance.cpp#111-117
  DOMHighResTimeStamp start = nsRFPService::ReduceTimePrecisionAsMSecs(
      mState.mStartWallClockRaw, 0, mState.mRTPCallerType);

  return start + realtime;
}

/* static */ RTCStatsTimestamp RTCStatsTimestamp::FromMozTime(
    const RTCStatsTimestampMaker& aMaker, TimeStamp aMozTime) {
  return RTCStatsTimestamp(aMaker.mState, aMozTime);
}

/* static */ RTCStatsTimestamp RTCStatsTimestamp::FromRealtime(
    const RTCStatsTimestampMaker& aMaker, webrtc::Timestamp aRealtime) {
  return FromDomRealtime(
      aMaker,
      aRealtime - webrtc::TimeDelta::Micros(aMaker.mState.mStartRealtime.us()));
}

/* static */ RTCStatsTimestamp RTCStatsTimestamp::From1Jan1970(
    const RTCStatsTimestampMaker& aMaker, webrtc::Timestamp a1Jan1970) {
  const auto& state = aMaker.mState;
  return FromDomRealtime(
      aMaker, a1Jan1970 - webrtc::TimeDelta::Millis(state.mStartWallClockRaw));
}

/* static */ RTCStatsTimestamp RTCStatsTimestamp::FromNtp(
    const RTCStatsTimestampMaker& aMaker, webrtc::Timestamp aNtpTime) {
  const auto& state = aMaker.mState;
  const auto domRealtime = aNtpTime -
                           webrtc::TimeDelta::Seconds(webrtc::kNtpJan1970) -
                           webrtc::TimeDelta::Millis(state.mStartWallClockRaw);
  // Ntp times exposed by libwebrtc to stats are always **rounded** to
  // milliseconds. That means they can jump up to half a millisecond into the
  // future. We compensate for that here so that things seem consistent to js.
  return FromDomRealtime(aMaker, domRealtime - webrtc::TimeDelta::Micros(500));
}

/* static */ RTCStatsTimestamp RTCStatsTimestamp::FromDomRealtime(
    const RTCStatsTimestampMaker& aMaker, webrtc::Timestamp aDomRealtime) {
  return RTCStatsTimestamp(aMaker.mState, aMaker.mState.mStartDomRealtime +
                                              TimeDuration::FromMicroseconds(
                                                  aDomRealtime.us<double>()));
}

RTCStatsTimestamp::RTCStatsTimestamp(RTCStatsTimestampState aState,
                                     TimeStamp aMozTime)
    : mState(aState), mMozTime(aMozTime) {}

RTCStatsTimestampMaker::RTCStatsTimestampMaker(RTCStatsTimestampState aState)
    : mState(aState) {}

/* static */
RTCStatsTimestampMaker RTCStatsTimestampMaker::Create(
    nsPIDOMWindowInner* aWindow /* = nullptr */) {
  if (!aWindow) {
    return RTCStatsTimestampMaker(RTCStatsTimestampState());
  }
  if (Performance* p = aWindow->GetPerformance()) {
    return RTCStatsTimestampMaker(RTCStatsTimestampState(*p));
  }
  return RTCStatsTimestampMaker(RTCStatsTimestampState());
}

RTCStatsTimestamp RTCStatsTimestampMaker::GetNow() const {
  return RTCStatsTimestamp::FromMozTime(*this, TimeStamp::Now());
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(RTCStatsReport, mParent)

RTCStatsReport::RTCStatsReport(nsPIDOMWindowInner* aParent)
    : mParent(aParent) {}

/*static*/
already_AddRefed<RTCStatsReport> RTCStatsReport::Constructor(
    const GlobalObject& aGlobal) {
  nsCOMPtr<nsPIDOMWindowInner> window(
      do_QueryInterface(aGlobal.GetAsSupports()));
  RefPtr<RTCStatsReport> report(new RTCStatsReport(window));
  return report.forget();
}

JSObject* RTCStatsReport::WrapObject(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto) {
  return RTCStatsReport_Binding::Wrap(aCx, this, aGivenProto);
}

void RTCStatsReport::Incorporate(RTCStatsCollection& aStats) {
  ForAllPublicRTCStatsCollectionMembers(
      aStats, [&](auto... aMember) { (SetRTCStats(aMember), ...); });
}

void RTCStatsReport::Set(const nsAString& aKey, JS::Handle<JSObject*> aValue,
                         ErrorResult& aRv) {
  RTCStatsReport_Binding::MaplikeHelpers::Set(this, aKey, aValue, aRv);
}

namespace {
template <size_t I, typename... Ts>
bool MoveInto(std::tuple<Ts...>& aFrom, std::tuple<Ts*...>& aInto) {
  return std::get<I>(aInto)->AppendElements(std::move(std::get<I>(aFrom)),
                                            fallible);
}

template <size_t... Is, typename... Ts>
bool MoveInto(std::tuple<Ts...>&& aFrom, std::tuple<Ts*...>& aInto,
              std::index_sequence<Is...>) {
  return (... && MoveInto<Is>(aFrom, aInto));
}

template <typename... Ts>
bool MoveInto(std::tuple<Ts...>&& aFrom, std::tuple<Ts*...>& aInto) {
  return MoveInto(std::move(aFrom), aInto, std::index_sequence_for<Ts...>());
}
}  // namespace

void MergeStats(UniquePtr<RTCStatsCollection> aFromStats,
                RTCStatsCollection* aIntoStats) {
  auto fromTuple = ForAllRTCStatsCollectionMembers(
      *aFromStats,
      [&](auto&... aMember) { return std::make_tuple(std::move(aMember)...); });
  auto intoTuple = ForAllRTCStatsCollectionMembers(
      *aIntoStats,
      [&](auto&... aMember) { return std::make_tuple(&aMember...); });
  if (!MoveInto(std::move(fromTuple), intoTuple)) {
    mozalloc_handle_oom(0);
  }
}

void FlattenStats(nsTArray<UniquePtr<RTCStatsCollection>> aFromStats,
                  RTCStatsCollection* aIntoStats) {
  for (auto& stats : aFromStats) {
    MergeStats(std::move(stats), aIntoStats);
  }
}

}  // namespace mozilla::dom
