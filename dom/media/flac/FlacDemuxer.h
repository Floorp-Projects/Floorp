/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef FLAC_DEMUXER_H_
#define FLAC_DEMUXER_H_

#include "mozilla/Attributes.h"
#include "MediaDataDemuxer.h"
#include "MediaResource.h"
namespace mozilla {

namespace flac {
class Frame;
class FrameParser;
}
class FlacTrackDemuxer;


class FlacDemuxer : public MediaDataDemuxer {
public:
  // MediaDataDemuxer interface.
  explicit FlacDemuxer(MediaResource* aSource);
  RefPtr<InitPromise> Init() override;
  bool HasTrackType(TrackInfo::TrackType aType) const override;
  uint32_t GetNumberTracks(TrackInfo::TrackType aType) const override;
  already_AddRefed<MediaTrackDemuxer> GetTrackDemuxer(
    TrackInfo::TrackType aType, uint32_t aTrackNumber) override;
  bool IsSeekable() const override;

private:
  bool InitInternal();

  RefPtr<MediaResource> mSource;
  RefPtr<FlacTrackDemuxer> mTrackDemuxer;
};

class FlacTrackDemuxer : public MediaTrackDemuxer {
public:
  explicit FlacTrackDemuxer(MediaResource* aSource);

  // Initializes the track demuxer by reading the first frame for meta data.
  // Returns initialization success state.
  bool Init();

  // MediaTrackDemuxer interface.
  UniquePtr<TrackInfo> GetInfo() const override;
  RefPtr<SeekPromise> Seek(media::TimeUnit aTime) override;
  RefPtr<SamplesPromise> GetSamples(int32_t aNumSamples = 1) override;
  void Reset() override;
  int64_t GetResourceOffset() const override;
  media::TimeIntervals GetBuffered() override;
  RefPtr<SkipAccessPointPromise> SkipToNextRandomAccessPoint(
    media::TimeUnit aTimeThreshold) override;

  bool IsSeekable() const;

private:
  // Destructor.
  ~FlacTrackDemuxer();

  // Returns the estimated stream duration, or a 0-duration if unknown.
  media::TimeUnit Duration() const;
  media::TimeUnit TimeAtEnd();

  // Fast approximate seeking to given time.
  media::TimeUnit FastSeek(const media::TimeUnit& aTime);

  // Seeks by scanning the stream up to the given time for more accurate results.
  media::TimeUnit ScanUntil(const media::TimeUnit& aTime);

  // Finds the next valid frame and return it.
  const flac::Frame& FindNextFrame();

  // Returns the next ADTS frame, if available.
  already_AddRefed<MediaRawData> GetNextFrame(const flac::Frame& aFrame);

  // Reads aSize bytes into aBuffer from the source starting at aOffset.
  // Returns the actual size read.
  int32_t Read(uint8_t* aBuffer, int64_t aOffset, int32_t aSize);

  // Returns the average frame length derived from the previously parsed frames.
  double AverageFrameLength() const;

  // The (hopefully) Flac resource.
  MediaResourceIndex mSource;

  // Flac frame parser used to detect frames and extract side info.
  nsAutoPtr<flac::FrameParser> mParser;

  // Total duration of parsed frames.
  media::TimeUnit mParsedFramesDuration;

  // Sum of parsed frames' lengths in bytes.
  uint64_t mTotalFrameLen;

  // Audio track config info.
  UniquePtr<AudioInfo> mInfo;
};

} // mozilla

#endif // !FLAC_DEMUXER_H_
