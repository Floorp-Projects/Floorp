/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ADTS_DEMUXER_H_
#define ADTS_DEMUXER_H_

#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"
#include "MediaDataDemuxer.h"
#include "MediaResource.h"

namespace mozilla {

namespace adts {
class Frame;
class FrameParser;
}  // namespace adts

class ADTSTrackDemuxer;

DDLoggedTypeDeclNameAndBase(ADTSDemuxer, MediaDataDemuxer);

class ADTSDemuxer : public MediaDataDemuxer,
                    public DecoderDoctorLifeLogger<ADTSDemuxer> {
 public:
  // MediaDataDemuxer interface.
  explicit ADTSDemuxer(MediaResource* aSource);
  RefPtr<InitPromise> Init() override;
  uint32_t GetNumberTracks(TrackInfo::TrackType aType) const override;
  already_AddRefed<MediaTrackDemuxer> GetTrackDemuxer(
      TrackInfo::TrackType aType, uint32_t aTrackNumber) override;
  bool IsSeekable() const override;

  // Return true if a valid ADTS frame header could be found.
  static bool ADTSSniffer(const uint8_t* aData, const uint32_t aLength);

 private:
  bool InitInternal();

  RefPtr<MediaResource> mSource;
  RefPtr<ADTSTrackDemuxer> mTrackDemuxer;
};

DDLoggedTypeNameAndBase(ADTSTrackDemuxer, MediaTrackDemuxer);

class ADTSTrackDemuxer : public MediaTrackDemuxer,
                         public DecoderDoctorLifeLogger<ADTSTrackDemuxer> {
 public:
  explicit ADTSTrackDemuxer(MediaResource* aSource);

  // Initializes the track demuxer by reading the first frame for meta data.
  // Returns initialization success state.
  bool Init();

  // Returns the total stream length if known, -1 otherwise.
  int64_t StreamLength() const;

  // Returns the estimated stream duration, or a 0-duration if unknown.
  media::TimeUnit Duration() const;

  // Returns the estimated duration up to the given frame number,
  // or a 0-duration if unknown.
  media::TimeUnit Duration(int64_t aNumFrames) const;

  // MediaTrackDemuxer interface.
  UniquePtr<TrackInfo> GetInfo() const override;
  RefPtr<SeekPromise> Seek(const media::TimeUnit& aTime) override;
  RefPtr<SamplesPromise> GetSamples(int32_t aNumSamples = 1) override;
  void Reset() override;
  RefPtr<SkipAccessPointPromise> SkipToNextRandomAccessPoint(
      const media::TimeUnit& aTimeThreshold) override;
  int64_t GetResourceOffset() const override;
  media::TimeIntervals GetBuffered() override;

 private:
  // Destructor.
  ~ADTSTrackDemuxer();

  // Fast approximate seeking to given time.
  media::TimeUnit FastSeek(const media::TimeUnit& aTime);

  // Seeks by scanning the stream up to the given time for more accurate
  // results.
  media::TimeUnit ScanUntil(const media::TimeUnit& aTime);

  // Finds the next valid frame and returns its byte range.
  const adts::Frame& FindNextFrame(bool findFirstFrame = false);

  // Skips the next frame given the provided byte range.
  bool SkipNextFrame(const adts::Frame& aFrame);

  // Returns the next ADTS frame, if available.
  already_AddRefed<MediaRawData> GetNextFrame(const adts::Frame& aFrame);

  // Updates post-read meta data.
  void UpdateState(const adts::Frame& aFrame);

  // Returns the frame index for the given offset.
  int64_t FrameIndexFromOffset(int64_t aOffset) const;

  // Returns the frame index for the given time.
  int64_t FrameIndexFromTime(const media::TimeUnit& aTime) const;

  // Reads aSize bytes into aBuffer from the source starting at aOffset.
  // Returns the actual size read.
  int32_t Read(uint8_t* aBuffer, int64_t aOffset, int32_t aSize);

  // Returns the average frame length derived from the previously parsed frames.
  double AverageFrameLength() const;

  // The (hopefully) ADTS resource.
  MediaResourceIndex mSource;

  // ADTS frame parser used to detect frames and extract side info.
  adts::FrameParser* mParser;

  // Current byte offset in the source stream.
  int64_t mOffset;

  // Total parsed frames.
  uint64_t mNumParsedFrames;

  // Current frame index.
  int64_t mFrameIndex;

  // Sum of parsed frames' lengths in bytes.
  uint64_t mTotalFrameLen;

  // Samples per frame metric derived from frame headers or 0 if none available.
  uint32_t mSamplesPerFrame;

  // Samples per second metric derived from frame headers or 0 if none
  // available.
  uint32_t mSamplesPerSecond;

  // Channel count derived from frame headers or 0 if none available.
  uint32_t mChannels;

  // Audio track config info.
  UniquePtr<AudioInfo> mInfo;

  // Amount of pre-roll time when seeking.
  // AAC encoder delay is by default 2112 audio frames.
  media::TimeUnit mPreRoll;
};

}  // namespace mozilla

#endif  // !ADTS_DEMUXER_H_
