/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */

#include "RTCStatsIdGenerator.h"

#include "mozilla/RandomNum.h"
#include "RTCStatsReport.h"
#include "WebrtcGlobal.h"

namespace mozilla {

RTCStatsIdGenerator::RTCStatsIdGenerator()
    : mSalt(RandomUint64().valueOr(0xa5a5a5a5)), mCounter(0) {}

void RTCStatsIdGenerator::RewriteIds(
    nsTArray<UniquePtr<dom::RTCStatsCollection>> aFromStats,
    dom::RTCStatsCollection* aIntoReport) {
  // Rewrite an Optional id
  auto rewriteId = [&](dom::Optional<nsString>& id) {
    if (id.WasPassed()) {
      id.Value() = Id(id.Value());
    }
  };

  auto rewriteIds = [&](auto& aList, auto... aParam) {
    for (auto& stat : aList) {
      (rewriteId(stat.*aParam), ...);
    }
  };

  // Involves a lot of copying, since webidl dictionaries don't have
  // move semantics. Oh well.

  // Create a temporary to avoid double-rewriting any stats already in
  // aIntoReport.
  auto stats = MakeUnique<dom::RTCStatsCollection>();
  dom::FlattenStats(std::move(aFromStats), stats.get());

  using S = dom::RTCStats;
  using ICPS = dom::RTCIceCandidatePairStats;
  using RSS = dom::RTCRtpStreamStats;
  using IRSS = dom::RTCInboundRtpStreamStats;
  using ORSS = dom::RTCOutboundRtpStreamStats;
  using RIRSS = dom::RTCRemoteInboundRtpStreamStats;
  using RORSS = dom::RTCRemoteOutboundRtpStreamStats;

  rewriteIds(stats->mIceCandidatePairStats, &S::mId, &ICPS::mLocalCandidateId,
             &ICPS::mRemoteCandidateId);
  rewriteIds(stats->mIceCandidateStats, &S::mId);
  rewriteIds(stats->mInboundRtpStreamStats, &S::mId, &IRSS::mRemoteId,
             &RSS::mCodecId);
  rewriteIds(stats->mOutboundRtpStreamStats, &S::mId, &ORSS::mRemoteId,
             &RSS::mCodecId);
  rewriteIds(stats->mRemoteInboundRtpStreamStats, &S::mId, &RIRSS::mLocalId,
             &RSS::mCodecId);
  rewriteIds(stats->mRemoteOutboundRtpStreamStats, &S::mId, &RORSS::mLocalId,
             &RSS::mCodecId);
  rewriteIds(stats->mCodecStats, &S::mId);
  rewriteIds(stats->mRtpContributingSourceStats, &S::mId);
  rewriteIds(stats->mTrickledIceCandidateStats, &S::mId);
  rewriteIds(stats->mDataChannelStats, &S::mId);

  dom::MergeStats(std::move(stats), aIntoReport);
}

nsString RTCStatsIdGenerator::Id(const nsString& aKey) {
  if (!aKey.Length()) {
    MOZ_ASSERT(aKey.Length(), "Stats IDs should never be empty.");
    return aKey;
  }
  if (mAllocated.find(aKey) == mAllocated.end()) {
    mAllocated[aKey] = Generate();
  }
  return mAllocated[aKey];
}

nsString RTCStatsIdGenerator::Generate() {
  auto random = RandomUint64().valueOr(0x1a22);
  auto idNum = static_cast<uint32_t>(mSalt ^ ((mCounter++ << 16) | random));
  nsString id;
  id.AppendInt(idNum, 16);  // Append as hex
  return id;
}

}  // namespace mozilla
