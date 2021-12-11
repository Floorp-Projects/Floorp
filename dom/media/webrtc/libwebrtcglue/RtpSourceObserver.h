/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AUDIOLEVELOBSERVER_H
#define AUDIOLEVELOBSERVER_H

#include <vector>
#include <map>

#include "nsISupportsImpl.h"
#include "mozilla/dom/RTCRtpSourcesBinding.h"
#include "webrtc/common_types.h"
#include "jsapi/RTCStatsReport.h"

// Unit Test class
namespace test {
class RtpSourcesTest;
}

namespace mozilla {

/* Observes reception of RTP packets and tabulates data about the
 * most recent arival times by source (csrc or ssrc) and audio level information
 *  * csrc-audio-level RTP header extension
 *  * ssrc-audio-level RTP header extension
 */
class RtpSourceObserver {
 public:
  explicit RtpSourceObserver(
      const dom::RTCStatsTimestampMaker& aTimestampMaker);

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RtpSourceObserver)

  void OnRtpPacket(const webrtc::RTPHeader& aHeader, const uint32_t aJitter);

  /*
   * Get the most recent 10 second window of CSRC and SSRC sources.
   * @param outLevels will be popluted with source entries
   * Note: this takes jitter into account when calculating the window so
   * the window is actually [time - jitter - 10 sec .. time - jitter]
   */
  void GetRtpSources(nsTArray<dom::RTCRtpSourceEntry>& outSources) const;

 private:
  virtual ~RtpSourceObserver() = default;

  struct RtpSourceEntry {
    RtpSourceEntry() = default;
    void Update(const int64_t aTimestamp, const uint32_t aRtpTimestamp,
                const bool aHasAudioLevel, const uint8_t aAudioLevel) {
      predictedPlayoutTime = aTimestamp;
      rtpTimestamp = aRtpTimestamp;
      // Audio level range is 0 - 127 inclusive
      hasAudioLevel = aHasAudioLevel && !(aAudioLevel & 0x80);
      audioLevel = aAudioLevel;
    }
    // Sets the audio level nullable according to the linear scale
    // outlined in the webrtc-pc spec.
    double ToLinearAudioLevel() const;
    // Time this information was received + jitter
    int64_t predictedPlayoutTime = 0;
    // The original RTP timestamp in the received packet
    uint32_t rtpTimestamp = 0;
    bool hasAudioLevel = false;
    uint8_t audioLevel = 0;
  };

  /* Why this is needed:
   * We are supposed to only report source stats for packets that have already
   * been rendered. Unfortunately, we only know when these packets are
   * _received_ right now. So, we need to make a guess at when each packet will
   * be rendered, and hide its statistics until the clock reaches that estimate.
   */
  /* Maintains a history of packets for reporting with getContributingSources
   * and getSynchronizationSources. It is expected that entries will not always
   * be observed in chronological order, and that the correct entry for a query
   * not be the most recently added item. Many times the query time is expected
   * to fall within [now - Jitter window .. now + Jitter Window]. A full history
   * is kept within the jitter window, and only the most recent to fall out of
   * the window is stored for the full 10 seconds. This value is only likely to
   * be returned when the stream is stopped or paused.
   *  x-axis: time (non-linear scale)
   *  let J = now + Jitter Window
   *  let T = now - Jitter Window
   *  now - 10 seconds                             T      now       J
   *  |-----------------Z--------------------------|-AB--CDEFG-HI--J|
   *                    ^Latest evicted               ^Jitter buffer entries
   *  Ex Query Time  ^Q0                  ^Q1          ^Q2 ^Q3     ^Q4
   *  Query result:
   *  Q0: Nothing
   *  Q1: Z
   *  Q2: B
   *  Q3: E
   *  Q4: I
   */
  class RtpSourceHistory {
   public:
    RtpSourceHistory() = default;
    // Finds the closest entry to a time, and passes that value to a closure
    // Note: the pointer is invalidated by any operation on the history
    // Note: the pointer is owned by the RtpSourceHistory
    const RtpSourceEntry* FindClosestNotAfter(int64_t aTime) const;
    // Inserts data into the history, may silently drop data if it is too old
    void Insert(const int64_t aTimeNow, const int64_t aTimestamp,
                const uint32_t aRtpTimestamp, const bool aHasAudioLevel,
                const uint8_t aAudioLevel);
    // Removes aged out from the jitter window
    void Prune(const int64_t aTimeNow);
    // Set Source
    void SetSource(uint32_t aSource, dom::RTCRtpSourceEntryType aType);

   private:
    // Finds a place to insert data and returns a reference to it
    RtpSourceObserver::RtpSourceEntry& Insert(const int64_t aTimeNow,
                                              const int64_t aTimestamp);
    // Is the history buffer empty?
    bool Empty() const { return !mDetailedHistory.size(); }
    // Is there an evicted entry
    bool HasEvicted() const { return mHasEvictedEntry; }

    // Minimum amount of time (ms) to store a complete packet history
    constexpr static int64_t kMinJitterWindow = 1000;
    // Size of the history window (ms)
    constexpr static int64_t kHistoryWindow = 10000;
    // This is 2 x the maximum observed jitter or the min which ever is higher
    int64_t mMaxJitterWindow = kMinJitterWindow;
    // The least old entry to be kicked from the buffer.
    RtpSourceEntry mLatestEviction;
    // Is there an evicted entry?
    bool mHasEvictedEntry = false;
    std::map<int64_t, RtpSourceEntry> mDetailedHistory;
    // Entry before history
    RtpSourceEntry mPrehistory;
    // Unit test
    friend test::RtpSourcesTest;
  };

  // Do not copy or assign
  RtpSourceObserver(const RtpSourceObserver&) = delete;
  RtpSourceObserver& operator=(RtpSourceObserver const&) = delete;
  // Returns a key for a source and a type
  static uint64_t GetKey(const uint32_t id,
                         const dom::RTCRtpSourceEntryType aType) {
    return (aType == dom::RTCRtpSourceEntryType::Synchronization)
               ? (static_cast<uint64_t>(id) |
                  (static_cast<uint64_t>(0x1) << 32))
               : (static_cast<uint64_t>(id));
  }
  // Returns the source from a key
  static uint32_t GetSourceFromKey(const uint64_t aKey) {
    return static_cast<uint32_t>(aKey & ~(static_cast<uint64_t>(0x1) << 32));
  }
  // Returns the type from a key
  static dom::RTCRtpSourceEntryType GetTypeFromKey(const uint64_t aKey) {
    return (aKey & (static_cast<uint64_t>(0x1) << 32))
               ? dom::RTCRtpSourceEntryType::Synchronization
               : dom::RTCRtpSourceEntryType::Contributing;
  }
  // Map CSRC to RtpSourceEntry
  std::map<uint64_t, RtpSourceHistory> mRtpSources;
  // 2 x the largest observed
  int64_t mMaxJitterWindow;
  dom::RTCStatsTimestampMaker mTimestampMaker;

  // Unit test
  friend test::RtpSourcesTest;

  // Testing only
  // Inserts additional csrc audio levels for mochitests
  friend void InsertAudioLevelForContributingSource(
      RtpSourceObserver& observer, const uint32_t aCsrcSource,
      const int64_t aTimestamp, const uint32_t aRtpTimestamp,
      const bool aHasAudioLevel, const uint8_t aAudioLevel);
};
}  // namespace mozilla
#undef NG
#endif  // AUDIOLEVELOBSERVER_H
