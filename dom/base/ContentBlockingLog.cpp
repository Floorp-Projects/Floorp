/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ContentBlockingLog.h"

#include "mozilla/dom/ContentChild.h"
#include "mozilla/RandomNum.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Unused.h"
#include "mozilla/XorShift128PlusRNG.h"

static LazyLogModule gContentBlockingLog("ContentBlockingLog");
#define LOG(fmt, ...) \
  MOZ_LOG(gContentBlockingLog, mozilla::LogLevel::Debug, (fmt, ##__VA_ARGS__))

typedef mozilla::Telemetry::OriginMetricID OriginMetricID;

namespace mozilla {
namespace dom {

// randomly choose 1% users included in the content blocking measurement
// based on their client id.
static constexpr double kRatioReportUser = 0.01;

// randomly choose 0.14% documents when the page is unload.
static constexpr double kRatioReportDocument = 0.0014;

static bool IsReportingPerUserEnabled() {
  MOZ_ASSERT(NS_IsMainThread());

  static Maybe<bool> sIsReportingEnabled;

  if (sIsReportingEnabled.isSome()) {
    return sIsReportingEnabled.value();
  }

  nsAutoCString cachedClientId;
  if (NS_FAILED(Preferences::GetCString("toolkit.telemetry.cachedClientID",
                                        cachedClientId))) {
    return false;
  }

  nsID clientId;
  if (!clientId.Parse(cachedClientId.get())) {
    return false;
  }

  /**
   * UUID might not be uniform-distributed (although some part of it could be).
   * In order to generate more random result, usually we use a hash function,
   * but here we hope it's fast and doesn't have to be cryptographic-safe.
   * |XorShift128PlusRNG| looks like a good alternative because it takes a
   * 128-bit data as its seed and always generate identical sequence if the
   * initial seed is the same.
   */
  static_assert(sizeof(nsID) == 16, "nsID is 128-bit");
  uint64_t* init = reinterpret_cast<uint64_t*>(&clientId);
  non_crypto::XorShift128PlusRNG rng(init[0], init[1]);
  sIsReportingEnabled.emplace(rng.nextDouble() <= kRatioReportUser);

  return sIsReportingEnabled.value();
}

static bool IsReportingPerDocumentEnabled() {
  constexpr double boundary =
      kRatioReportDocument * std::numeric_limits<uint64_t>::max();
  Maybe<uint64_t> randomNum = RandomUint64();
  return randomNum.isSome() && randomNum.value() <= boundary;
}

static bool IsReportingEnabled() {
  if (StaticPrefs::telemetry_origin_telemetry_test_mode_enabled()) {
    return true;
  } else if (!StaticPrefs::
                 privacy_trackingprotection_origin_telemetry_enabled()) {
    return false;
  }

  return IsReportingPerUserEnabled() && IsReportingPerDocumentEnabled();
}

static void ReportOriginSingleHash(OriginMetricID aId,
                                   const nsACString& aOrigin) {
  MOZ_ASSERT(NS_IsMainThread());

  LOG("ReportOriginSingleHash metric=%s",
      Telemetry::MetricIDToString[static_cast<uint32_t>(aId)]);
  LOG("ReportOriginSingleHash origin=%s", PromiseFlatCString(aOrigin).get());

  if (XRE_IsParentProcess()) {
    Telemetry::RecordOrigin(aId, aOrigin);
    return;
  }

  dom::ContentChild* contentChild = dom::ContentChild::GetSingleton();
  if (NS_WARN_IF(!contentChild)) {
    return;
  }

  Unused << contentChild->SendRecordOrigin(static_cast<uint32_t>(aId),
                                           nsCString(aOrigin));
}

void ContentBlockingLog::ReportLog() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!IsReportingEnabled()) {
    return;
  }
  LOG("ContentBlockingLog::ReportLog [this=%p]", this);

  for (const auto& originEntry : mLog) {
    if (!originEntry.mData) {
      continue;
    }

    for (const auto& logEntry : Reversed(originEntry.mData->mLogs)) {
      if (logEntry.mType !=
              nsIWebProgressListener::STATE_COOKIES_BLOCKED_TRACKER ||
          logEntry.mTrackingFullHashes.IsEmpty()) {
        continue;
      }

      const bool isBlocked = logEntry.mBlocked;
      Maybe<StorageAccessGrantedReason> reason = logEntry.mReason;
      const bool testMode =
          StaticPrefs::telemetry_origin_telemetry_test_mode_enabled();

      OriginMetricID metricId =
          testMode ? OriginMetricID::ContentBlocking_Blocked_TestOnly
                   : OriginMetricID::ContentBlocking_Blocked;
      if (!isBlocked) {
        MOZ_ASSERT(reason.isSome());
        switch (reason.value()) {
          case StorageAccessGrantedReason::eStorageAccessAPI:
            metricId =
                testMode
                    ? OriginMetricID::
                          ContentBlocking_StorageAccessAPIExempt_TestOnly
                    : OriginMetricID::ContentBlocking_StorageAccessAPIExempt;
            break;
          case StorageAccessGrantedReason::eOpenerAfterUserInteraction:
            metricId =
                testMode
                    ? OriginMetricID::
                          ContentBlocking_OpenerAfterUserInteractionExempt_TestOnly
                    : OriginMetricID::
                          ContentBlocking_OpenerAfterUserInteractionExempt;
            break;
          case StorageAccessGrantedReason::eOpener:
            metricId =
                testMode ? OriginMetricID::ContentBlocking_OpenerExempt_TestOnly
                         : OriginMetricID::ContentBlocking_OpenerExempt;
            break;
          default:
            MOZ_ASSERT_UNREACHABLE("Unknown StorageAccessGrantedReason");
        }
      }

      for (const nsCString& hash : logEntry.mTrackingFullHashes) {
        ReportOriginSingleHash(metricId, hash);
      }
      break;
    }
  }
}

}  // namespace dom
}  // namespace mozilla
