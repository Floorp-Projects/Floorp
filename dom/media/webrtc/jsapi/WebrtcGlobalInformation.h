/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _WEBRTC_GLOBAL_INFORMATION_H_
#define _WEBRTC_GLOBAL_INFORMATION_H_

#include "mozilla/Attributes.h"
#include "mozilla/dom/WebrtcGlobalInformationBinding.h"
#include "mozilla/dom/BindingDeclarations.h"  // for Optional
#include "nsDOMNavigationTiming.h"
#include "WebrtcGlobalStatsHistory.h"

namespace mozilla {
class PeerConnectionImpl;
class ErrorResult;

namespace dom {

class GlobalObject;
class WebrtcGlobalStatisticsCallback;
class WebrtcGlobalStatisticsHistoryPcIdsCallback;
class WebrtcGlobalLoggingCallback;
struct RTCStatsReportInternal;

class WebrtcGlobalInformation {
 public:
  MOZ_CAN_RUN_SCRIPT
  static void GetAllStats(const GlobalObject& aGlobal,
                          WebrtcGlobalStatisticsCallback& aStatsCallback,
                          const Optional<nsAString>& aPcIdFilter,
                          ErrorResult& aRv);

  MOZ_CAN_RUN_SCRIPT
  static void GetStatsHistoryPcIds(
      const GlobalObject& aGlobal,
      WebrtcGlobalStatisticsHistoryPcIdsCallback& aPcIdsCallback,
      ErrorResult& aRv);

  MOZ_CAN_RUN_SCRIPT
  static void GetStatsHistorySince(
      const GlobalObject& aGlobal,
      WebrtcGlobalStatisticsHistoryCallback& aStatsCallback,
      const nsAString& aPcIdFilter, const Optional<DOMHighResTimeStamp>& aAfter,
      const Optional<DOMHighResTimeStamp>& aSdpAfter, ErrorResult& aRv);

  static void GetMediaContext(const GlobalObject& aGlobal,
                              WebrtcGlobalMediaContext& aContext);

  static void GatherHistory();

  static void ClearAllStats(const GlobalObject& aGlobal);

  MOZ_CAN_RUN_SCRIPT
  static void GetLogging(const GlobalObject& aGlobal, const nsAString& aPattern,
                         WebrtcGlobalLoggingCallback& aLoggingCallback,
                         ErrorResult& aRv);

  static void ClearLogging(const GlobalObject& aGlobal);

  static void SetAecDebug(const GlobalObject& aGlobal, bool aEnable);
  static bool AecDebug(const GlobalObject& aGlobal);
  static void GetAecDebugLogDir(const GlobalObject& aGlobal, nsAString& aDir);

  static void StashStats(const RTCStatsReportInternal& aReport);

  WebrtcGlobalInformation() = delete;
  WebrtcGlobalInformation(const WebrtcGlobalInformation& aOrig) = delete;
  WebrtcGlobalInformation& operator=(const WebrtcGlobalInformation& aRhs) =
      delete;

  struct PcTrackingUpdate {
    static PcTrackingUpdate Add(const nsString& aPcid,
                                const bool& aLongTermStatsDisabled) {
      return PcTrackingUpdate{aPcid, Some(aLongTermStatsDisabled)};
    }
    static PcTrackingUpdate Remove(const nsString& aPcid) {
      return PcTrackingUpdate{aPcid, Nothing()};
    }
    nsString mPcid;
    Maybe<bool> mLongTermStatsDisabled;
    enum class Type {
      Add,
      Remove,
    };
    Type Type() const {
      return mLongTermStatsDisabled ? Type::Add : Type::Remove;
    }
  };
  static void PeerConnectionTracking(PcTrackingUpdate& aUpdate) {
    AdjustTimerReferences(std::move(aUpdate));
  }

 private:
  static void AdjustTimerReferences(PcTrackingUpdate&& aUpdate);
};

}  // namespace dom
}  // namespace mozilla

#endif  // _WEBRTC_GLOBAL_INFORMATION_H_
