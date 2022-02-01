/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */

#include "RTCStatsIdGenerator.h"

#include <iostream>

#include "mozilla/RandomNum.h"
#include "RTCStatsReport.h"

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

  auto rewriteIds = [&](auto& aList) {
    for (auto& stat : aList) {
      rewriteId(stat.mId);
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

  auto rewriteCodecIds = [&](auto& aList) {
    for (auto& stat : aList) {
      rewriteId(stat.mCodecId);
    }
  };

  // Involves a lot of copying, since webidl dictionaries don't have
  // move semantics. Oh well.

  // Create a temporary to avoid double-rewriting any stats already in
  // aIntoReport.
  auto stats = MakeUnique<dom::RTCStatsCollection>();
  dom::FlattenStats(std::move(aFromStats), stats.get());

  for (auto& stat : stats->mIceCandidatePairStats) {
    rewriteId(stat.mLocalCandidateId);
    rewriteId(stat.mRemoteCandidateId);
  };
  rewriteIds(stats->mIceCandidatePairStats);

  rewriteIds(stats->mIceCandidateStats);

  rewriteRemoteIds(stats->mInboundRtpStreamStats);
  rewriteCodecIds(stats->mInboundRtpStreamStats);
  rewriteIds(stats->mInboundRtpStreamStats);

  rewriteRemoteIds(stats->mOutboundRtpStreamStats);
  rewriteCodecIds(stats->mOutboundRtpStreamStats);
  rewriteIds(stats->mOutboundRtpStreamStats);

  rewriteLocalIds(stats->mRemoteInboundRtpStreamStats);
  rewriteCodecIds(stats->mRemoteInboundRtpStreamStats);
  rewriteIds(stats->mRemoteInboundRtpStreamStats);

  rewriteLocalIds(stats->mRemoteOutboundRtpStreamStats);
  rewriteCodecIds(stats->mRemoteOutboundRtpStreamStats);
  rewriteIds(stats->mRemoteOutboundRtpStreamStats);

  rewriteIds(stats->mCodecStats);
  rewriteIds(stats->mRtpContributingSourceStats);
  rewriteIds(stats->mTrickledIceCandidateStats);
  rewriteIds(stats->mDataChannelStats);

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
