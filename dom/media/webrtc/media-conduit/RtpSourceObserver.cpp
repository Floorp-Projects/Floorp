/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RtpSourceObserver.h"
#include "nsThreadUtils.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/modules/include/module_common_types.h"

namespace mozilla {

using EntryType = dom::RTCRtpSourceEntryType;

double RtpSourceObserver::RtpSourceEntry::ToLinearAudioLevel() const {
  // Spec indicates that a value of 127 should be set to 0
  if (audioLevel == 127) {
    return 0;
  }
  // All other values are calculated as 10^(-rfc_level/20)
  return std::pow(10, -static_cast<double>(audioLevel) / 20);
}

RtpSourceObserver::RtpSourceObserver(
    const dom::RTCStatsTimestampMaker& aTimestampMaker)
    : mMaxJitterWindow(0), mTimestampMaker(aTimestampMaker) {}

void RtpSourceObserver::OnRtpPacket(const webrtc::RTPHeader& aHeader,
                                    const uint32_t aJitter) {
  DOMHighResTimeStamp jsNow = mTimestampMaker.GetNow();

  RefPtr<Runnable> runnable = NS_NewRunnableFunction(
      "RtpSourceObserver::OnRtpPacket",
      [this, self = RefPtr<RtpSourceObserver>(this), aHeader, aJitter,
       jsNow]() {
        mMaxJitterWindow =
            std::max(mMaxJitterWindow, static_cast<int64_t>(aJitter) * 2);
        // We are supposed to report the time at which this packet was played
        // out, but we have only just received the packet. We try to guess when
        // it will be played out.
        // TODO: We need to move where we update these stats to MediaPipeline,
        // where we send frames to the media track graph. In order to do that,
        // we will need to have the ssrc (and csrc) for decoded frames, but we
        // don't have that right now. Once we move this to the correct place, we
        // will no longer need to keep anything but the most recent data.
        const auto predictedPlayoutTime = jsNow + aJitter;
        auto& hist =
            mRtpSources[GetKey(aHeader.ssrc, EntryType::Synchronization)];
        hist.Prune(jsNow);
        // ssrc-audio-level handling
        hist.Insert(jsNow, predictedPlayoutTime, aHeader.timestamp,
                    aHeader.extension.hasAudioLevel,
                    aHeader.extension.audioLevel);

        // csrc-audio-level handling
        const auto& list = aHeader.extension.csrcAudioLevels;
        for (uint8_t i = 0; i < aHeader.numCSRCs; i++) {
          const uint32_t& csrc = aHeader.arrOfCSRCs[i];
          auto& hist = mRtpSources[GetKey(csrc, EntryType::Contributing)];
          hist.Prune(jsNow);
          bool hasLevel = i < list.numAudioLevels;
          uint8_t level = hasLevel ? list.arrOfAudioLevels[i] : 0;
          hist.Insert(jsNow, predictedPlayoutTime, aHeader.timestamp, hasLevel,
                      level);
        }
      });

  if (NS_IsMainThread()) {
    // Code-path for gtests; everything happens on main, and there's no event
    // loop.
    runnable->Run();
  } else {
    NS_DispatchToMainThread(runnable);
  }
}

void RtpSourceObserver::GetRtpSources(
    nsTArray<dom::RTCRtpSourceEntry>& outSources) const {
  MOZ_ASSERT(NS_IsMainThread());
  outSources.Clear();
  for (const auto& it : mRtpSources) {
    const RtpSourceEntry* entry =
        it.second.FindClosestNotAfter(mTimestampMaker.GetNow());
    if (entry) {
      dom::RTCRtpSourceEntry domEntry;
      domEntry.mSource = GetSourceFromKey(it.first);
      domEntry.mSourceType = GetTypeFromKey(it.first);
      domEntry.mTimestamp = entry->predictedPlayoutTime;
      domEntry.mRtpTimestamp = entry->rtpTimestamp;
      if (entry->hasAudioLevel) {
        domEntry.mAudioLevel.Construct(entry->ToLinearAudioLevel());
      }
      outSources.AppendElement(std::move(domEntry));
    }
  }
}

const RtpSourceObserver::RtpSourceEntry*
RtpSourceObserver::RtpSourceHistory::FindClosestNotAfter(int64_t aTime) const {
  MOZ_ASSERT(NS_IsMainThread());
  // This method scans the history for the entry whose timestamp is closest to a
  // given timestamp but no greater. Because it is scanning forward, it keeps
  // track of the closest entry it has found so far in case it overshoots.
  // There is no before map.begin() which complicates things, so found tracks
  // if something was really found.
  auto lastFound = mDetailedHistory.cbegin();
  bool found = false;
  for (const auto& it : mDetailedHistory) {
    if (it.second.predictedPlayoutTime > aTime) {
      break;
    }
    // lastFound can't start before begin, so the first inc must be skipped
    if (found) {
      lastFound++;
    }
    found = true;
  }
  if (found) {
    return &lastFound->second;
  }
  if (HasEvicted() && aTime >= mLatestEviction.predictedPlayoutTime) {
    return &mLatestEviction;
  }
  return nullptr;
}

void RtpSourceObserver::RtpSourceHistory::Prune(const int64_t aTimeNow) {
  MOZ_ASSERT(NS_IsMainThread());
  const auto aTimeT = aTimeNow - mMaxJitterWindow;
  const auto aTimePrehistory = aTimeNow - kHistoryWindow;
  bool found = false;
  // New lower bound of the map
  auto lower = mDetailedHistory.begin();
  for (auto& it : mDetailedHistory) {
    if (it.second.predictedPlayoutTime > aTimeT) {
      found = true;
      break;
    }
    if (found) {
      lower++;
    }
    found = true;
  }
  if (found) {
    if (lower->second.predictedPlayoutTime > aTimePrehistory) {
      mLatestEviction = lower->second;
      mHasEvictedEntry = true;
    }
    lower++;
    mDetailedHistory.erase(mDetailedHistory.begin(), lower);
  }
  if (HasEvicted() &&
      (mLatestEviction.predictedPlayoutTime + kHistoryWindow) < aTimeNow) {
    mHasEvictedEntry = false;
  }
}

void RtpSourceObserver::RtpSourceHistory::Insert(const int64_t aTimeNow,
                                                 const int64_t aTimestamp,
                                                 const uint32_t aRtpTimestamp,
                                                 const bool aHasAudioLevel,
                                                 const uint8_t aAudioLevel) {
  MOZ_ASSERT(NS_IsMainThread());
  Insert(aTimeNow, aTimestamp)
      .Update(aTimestamp, aRtpTimestamp, aHasAudioLevel, aAudioLevel);
}

RtpSourceObserver::RtpSourceEntry& RtpSourceObserver::RtpSourceHistory::Insert(
    const int64_t aTimeNow, const int64_t aTimestamp) {
  MOZ_ASSERT(NS_IsMainThread());
  // Time T is the oldest time inside the jitter window (now - jitter)
  // Time J is the newest time inside the jitter window (now + jitter)
  // Time x is the jitter adjusted entry time
  // Time Z is the time of the long term storage element
  // Times A, B, C are times of entries in the jitter window buffer
  // x-axis: time
  // x or x        T   J
  //  |------Z-----|ABC| -> |------Z-----|ABC|
  if ((aTimestamp + kHistoryWindow) < aTimeNow ||
      aTimestamp < mLatestEviction.predictedPlayoutTime) {
    return mPrehistory;  // A.K.A. /dev/null
  }
  mMaxJitterWindow = std::max(mMaxJitterWindow, (aTimestamp - aTimeNow) * 2);
  const int64_t aTimeT = aTimeNow - mMaxJitterWindow;
  //           x  T   J
  // |------Z-----|ABC| -> |--------x---|ABC|
  if (aTimestamp < aTimeT) {
    mHasEvictedEntry = true;
    return mLatestEviction;
  }
  //              T  X J
  // |------Z-----|AB-C| -> |--------x---|ABXC|
  return mDetailedHistory[aTimestamp];
}

}  // namespace mozilla
