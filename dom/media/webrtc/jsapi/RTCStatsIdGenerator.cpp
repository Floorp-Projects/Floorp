/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */

#include "RTCStatsIdGenerator.h"

#include <iostream>

#include "mozilla/dom/RTCStatsReportBinding.h"
#include "mozilla/RandomNum.h"

namespace mozilla {

RTCStatsIdGenerator::RTCStatsIdGenerator()
    : mSalt(RandomUint64().valueOr(0xa5a5a5a5)), mCounter(0) {}

void RTCStatsIdGenerator::RewriteIds(
    const nsTArray<UniquePtr<dom::RTCStatsCollection>>& aFromStats,
    dom::RTCStatsCollection* aIntoReport) {
  // Rewrite an Optional id
  auto rewriteId = [&](dom::Optional<nsString>& id) {
    if (id.WasPassed()) {
      id.Value() = Id(id.Value());
    }
  };

  auto assignWithOpaqueIds = [&](auto& aSource, auto& aDest) {
    for (auto& stat : aSource) {
      rewriteId(stat.mId);
    }
    if (!aDest.AppendElements(aSource, fallible)) {
      mozalloc_handle_oom(0);
    }
  };

  auto rewriteRemoteIds = [&](auto& aList) {
    for (auto& stat : aList) {
      rewriteId(stat.mRemoteId);
    }
  };

  auto rewriteLocalIds = [&](auto& aList) {
    for (auto& stat : aList) {
      rewriteId(stat.mLocalId);
    }
  };

  // Involves a lot of copying, since webidl dictionaries don't have
  // move semantics. Oh well.
  for (const auto& stats : aFromStats) {
    for (auto& stat : stats->mIceCandidatePairStats) {
      rewriteId(stat.mLocalCandidateId);
      rewriteId(stat.mRemoteCandidateId);
    };
    assignWithOpaqueIds(stats->mIceCandidatePairStats,
                        aIntoReport->mIceCandidatePairStats);

    assignWithOpaqueIds(stats->mIceCandidateStats,
                        aIntoReport->mIceCandidateStats);

    rewriteRemoteIds(stats->mInboundRtpStreamStats);
    assignWithOpaqueIds(stats->mInboundRtpStreamStats,
                        aIntoReport->mInboundRtpStreamStats);

    rewriteRemoteIds(stats->mOutboundRtpStreamStats);
    assignWithOpaqueIds(stats->mOutboundRtpStreamStats,
                        aIntoReport->mOutboundRtpStreamStats);

    rewriteLocalIds(stats->mRemoteInboundRtpStreamStats);
    assignWithOpaqueIds(stats->mRemoteInboundRtpStreamStats,
                        aIntoReport->mRemoteInboundRtpStreamStats);

    rewriteLocalIds(stats->mRemoteOutboundRtpStreamStats);
    assignWithOpaqueIds(stats->mRemoteOutboundRtpStreamStats,
                        aIntoReport->mRemoteOutboundRtpStreamStats);

    assignWithOpaqueIds(stats->mRtpContributingSourceStats,
                        aIntoReport->mRtpContributingSourceStats);
    assignWithOpaqueIds(stats->mTrickledIceCandidateStats,
                        aIntoReport->mTrickledIceCandidateStats);
    assignWithOpaqueIds(stats->mDataChannelStats,
                        aIntoReport->mDataChannelStats);
    if (!aIntoReport->mRawLocalCandidates.AppendElements(
            stats->mRawLocalCandidates, fallible) ||
        !aIntoReport->mRawRemoteCandidates.AppendElements(
            stats->mRawRemoteCandidates, fallible) ||
        !aIntoReport->mVideoFrameHistories.AppendElements(
            stats->mVideoFrameHistories, fallible) ||
        !aIntoReport->mBandwidthEstimations.AppendElements(
            stats->mBandwidthEstimations, fallible)) {
      // XXX(Bug 1632090) Instead of extending the array 1-by-1
      // (which might involve multiple reallocations) and
      // potentially crashing here, SetCapacity could be called
      // outside the loop once.
      mozalloc_handle_oom(0);
    }
  }
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
