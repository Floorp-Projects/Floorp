/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaConduitInterface.h"

#include "nsTArray.h"
#include "mozilla/Assertions.h"
#include "MainThreadUtils.h"

#include "system_wrappers/include/clock.h"

namespace mozilla {

void MediaSessionConduit::GetRtpSources(
    nsTArray<dom::RTCRtpSourceEntry>& outSources) const {
  MOZ_ASSERT(NS_IsMainThread());
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
    const std::vector<webrtc::RtpSource>& aSources) {
  MOZ_ASSERT(NS_IsMainThread());
  // Empty out the cache; we'll copy things back as needed
  auto cache = std::move(mSourcesCache);

  // Fix up timestamps to be consistent with JS time. We assume that
  // source.timestamp_ms() was not terribly long ago, and so clock drift
  // between the libwebrtc clock and our JS clock is not that significant.
  auto jsNow = GetNow();
  double libwebrtcNow = webrtc::Clock::GetRealTimeClock()->TimeInMilliseconds();

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

    double ago = libwebrtcNow - source.timestamp_ms();
    domEntry.mTimestamp = jsNow - ago;
    domEntry.mRtpTimestamp = source.rtp_timestamp();
    mSourcesCache[key] = domEntry;
  }
}

void MediaSessionConduit::InsertAudioLevelForContributingSource(
    const uint32_t aCsrcSource, const int64_t aTimestamp,
    const uint32_t aRtpTimestamp, const bool aHasAudioLevel,
    const uint8_t aAudioLevel) {
  MOZ_ASSERT(NS_IsMainThread());

  dom::RTCRtpSourceEntry domEntry;
  domEntry.mSource = aCsrcSource;
  domEntry.mSourceType = dom::RTCRtpSourceEntryType::Contributing;
  domEntry.mTimestamp = aTimestamp;
  domEntry.mRtpTimestamp = aRtpTimestamp;
  if (aHasAudioLevel) {
    domEntry.mAudioLevel.Construct(rtpToDomAudioLevel(aAudioLevel));
  }

  int64_t libwebrtcNow =
      webrtc::Clock::GetRealTimeClock()->TimeInMilliseconds();
  double jsNow = GetNow();
  double ago = jsNow - aTimestamp;
  uint64_t convertedTimestamp = libwebrtcNow - ago;

  // This will get stomped the next time UpdateRtpSources is called; the tests
  // that use this don't relinquish the event loop after calling this, so
  // that's ok.
  SourceKey key(convertedTimestamp, aCsrcSource);
  mSourcesCache[key] = domEntry;
}

}  // namespace mozilla
