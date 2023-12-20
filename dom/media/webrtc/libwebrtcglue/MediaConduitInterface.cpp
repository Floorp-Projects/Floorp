/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaConduitInterface.h"

#include "nsTArray.h"
#include "mozilla/Assertions.h"
#include "MainThreadUtils.h"
#include "SystemTime.h"

#include "system_wrappers/include/clock.h"

namespace mozilla {

void MediaSessionConduit::GetRtpSources(
    nsTArray<dom::RTCRtpSourceEntry>& outSources) const {
  MOZ_ASSERT(NS_IsMainThread());
  if (mSourcesUpdateNeeded) {
    UpdateRtpSources(GetUpstreamRtpSources());
    OnSourcesUpdated();
  }
  outSources.Clear();
  for (auto& [key, entry] : mSourcesCache) {
    (void)key;
    outSources.AppendElement(entry);
  }

  struct TimestampComparator {
    bool LessThan(const dom::RTCRtpSourceEntry& aLhs,
                  const dom::RTCRtpSourceEntry& aRhs) const {
      // Sort descending!
      return aLhs.mTimestamp > aRhs.mTimestamp;
    }

    bool Equals(const dom::RTCRtpSourceEntry& aLhs,
                const dom::RTCRtpSourceEntry& aRhs) const {
      return aLhs.mTimestamp == aRhs.mTimestamp;
    }
  };

  // *sigh* We have to re-sort this by JS timestamp; we can run into cases
  // where the libwebrtc timestamps are not in exactly the same order as JS
  // timestamps due to clock differences (wibbly-wobbly, timey-wimey stuff)
  outSources.Sort(TimestampComparator());
}

static double rtpToDomAudioLevel(uint8_t aAudioLevel) {
  if (aAudioLevel == 127) {
    // Spec indicates that a value of 127 should be set to 0
    return 0;
  }

  // All other values are calculated as 10^(-rfc_level/20)
  return std::pow(10, -aAudioLevel / 20.0);
}

void MediaSessionConduit::UpdateRtpSources(
    const std::vector<webrtc::RtpSource>& aSources) const {
  MOZ_ASSERT(NS_IsMainThread());
  // Empty out the cache; we'll copy things back as needed
  auto cache = std::move(mSourcesCache);

  for (const auto& source : aSources) {
    SourceKey key(source);
    auto it = cache.find(key);
    if (it != cache.end()) {
      // This source entry was already in the cache, and should continue to be
      // present in exactly the same form as before. This means we do _not_
      // want to perform the timestamp adjustment again, since it might yield a
      // slightly different result. This is why we copy this entry from the old
      // cache instead of simply rebuilding it, and is also why we key the
      // cache based on timestamp (keying the cache based on timestamp also
      // gets us the ordering we want, conveniently).
      mSourcesCache[key] = it->second;
      continue;
    }

    // This is something we did not already have in the cache.
    dom::RTCRtpSourceEntry domEntry;
    domEntry.mSource = source.source_id();
    switch (source.source_type()) {
      case webrtc::RtpSourceType::SSRC:
        domEntry.mSourceType = dom::RTCRtpSourceEntryType::Synchronization;
        break;
      case webrtc::RtpSourceType::CSRC:
        domEntry.mSourceType = dom::RTCRtpSourceEntryType::Contributing;
        break;
      default:
        MOZ_CRASH("Unexpected RTCRtpSourceEntryType");
    }

    if (source.audio_level()) {
      domEntry.mAudioLevel.Construct(rtpToDomAudioLevel(*source.audio_level()));
    }

    // These timestamps are always **rounded** to milliseconds. That means they
    // can jump up to half a millisecond into the future. We compensate for that
    // here so that things seem consistent to js.
    domEntry.mTimestamp =
        dom::RTCStatsTimestamp::FromRealtime(
            GetTimestampMaker(),
            webrtc::Timestamp::Millis(source.timestamp().ms()) -
                webrtc::TimeDelta::Micros(500))
            .ToDom();
    domEntry.mRtpTimestamp = source.rtp_timestamp();
    mSourcesCache[key] = domEntry;
  }
}

void MediaSessionConduit::OnSourcesUpdated() const {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mSourcesUpdateNeeded);
  mSourcesUpdateNeeded = false;
  // Reset the updateNeeded flag and clear the cache in a direct task, i.e.,
  // as soon as the current task has finished.
  AbstractThread::GetCurrent()->TailDispatcher().AddDirectTask(
      NS_NewRunnableFunction(
          __func__, [this, self = RefPtr<const MediaSessionConduit>(this)] {
            mSourcesUpdateNeeded = true;
            mSourcesCache.clear();
          }));
}

void MediaSessionConduit::InsertAudioLevelForContributingSource(
    const uint32_t aCsrcSource, const int64_t aTimestamp,
    const uint32_t aRtpTimestamp, const bool aHasAudioLevel,
    const uint8_t aAudioLevel) {
  MOZ_ASSERT(NS_IsMainThread());

  if (mSourcesUpdateNeeded) {
    OnSourcesUpdated();
  }

  dom::RTCRtpSourceEntry domEntry;
  domEntry.mSource = aCsrcSource;
  domEntry.mSourceType = dom::RTCRtpSourceEntryType::Contributing;
  domEntry.mTimestamp = aTimestamp;
  domEntry.mRtpTimestamp = aRtpTimestamp;
  if (aHasAudioLevel) {
    domEntry.mAudioLevel.Construct(rtpToDomAudioLevel(aAudioLevel));
  }

  auto now = GetTimestampMaker().GetNow();
  webrtc::Timestamp convertedTimestamp =
      now.ToRealtime() - webrtc::TimeDelta::Millis(now.ToDom() - aTimestamp);

  SourceKey key(convertedTimestamp.ms<uint32_t>(), aCsrcSource);
  mSourcesCache[key] = domEntry;
}

}  // namespace mozilla
