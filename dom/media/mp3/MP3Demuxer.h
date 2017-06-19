/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MP3_DEMUXER_H_
#define MP3_DEMUXER_H_

#include "MediaDataDemuxer.h"
#include "MediaResource.h"
#include "MP3FrameParser.h"

namespace mozilla {

class MP3TrackDemuxer;

class MP3Demuxer : public MediaDataDemuxer
{
public:
  // MediaDataDemuxer interface.
  explicit MP3Demuxer(MediaResource* aSource);
  RefPtr<InitPromise> Init() override;
  bool HasTrackType(TrackInfo::TrackType aType) const override;
  uint32_t GetNumberTracks(TrackInfo::TrackType aType) const override;
  already_AddRefed<MediaTrackDemuxer> GetTrackDemuxer(
      TrackInfo::TrackType aType, uint32_t aTrackNumber) override;
  bool IsSeekable() const override;
  void NotifyDataArrived() override;
  void NotifyDataRemoved() override;

private:
  // Synchronous initialization.
  bool InitInternal();

  RefPtr<MediaResource> mSource;
  RefPtr<MP3TrackDemuxer> mTrackDemuxer;
};

// The MP3 demuxer used to extract MPEG frames and side information out of
// MPEG streams.
class MP3TrackDemuxer : public MediaTrackDemuxer
{
public:
  // Constructor, expecting a valid media resource.
  explicit MP3TrackDemuxer(MediaResource* aSource);

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

  // Returns the estimated current seek position time.
  media::TimeUnit SeekPosition() const;

  const FrameParser::Frame& LastFrame() const;
  RefPtr<MediaRawData> DemuxSample();

  const ID3Parser::ID3Header& ID3Header() const;
  const FrameParser::VBRHeader& VBRInfo() const;

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
  ~MP3TrackDemuxer() {}

  // Fast approximate seeking to given time.
  media::TimeUnit FastSeek(const media::TimeUnit& aTime);

  // Seeks by scanning the stream up to the given time for more accurate results.
  media::TimeUnit ScanUntil(const media::TimeUnit& aTime);

  // Finds the first valid frame and returns its byte range if found
  // or a null-byte range otherwise.
  MediaByteRange FindFirstFrame();

  // Finds the next valid frame and returns its byte range if found
  // or a null-byte range otherwise.
  MediaByteRange FindNextFrame();

  // Skips the next frame given the provided byte range.
  bool SkipNextFrame(const MediaByteRange& aRange);

  // Returns the next MPEG frame, if available.
  already_AddRefed<MediaRawData> GetNextFrame(const MediaByteRange& aRange);

  // Updates post-read meta data.
  void UpdateState(const MediaByteRange& aRange);

  // Returns the estimated offset for the given frame index.
  int64_t OffsetFromFrameIndex(int64_t aFrameIndex) const;

  // Returns the estimated frame index for the given offset.
  int64_t FrameIndexFromOffset(int64_t aOffset) const;

  // Returns the estimated frame index for the given time.
  int64_t FrameIndexFromTime(const media::TimeUnit& aTime) const;

  // Reads aSize bytes into aBuffer from the source starting at aOffset.
  // Returns the actual size read.
  int32_t Read(uint8_t* aBuffer, int64_t aOffset, int32_t aSize);

  // Returns the average frame length derived from the previously parsed frames.
  double AverageFrameLength() const;

  // The (hopefully) MPEG resource.
  MediaResourceIndex mSource;

  // MPEG frame parser used to detect frames and extract side info.
  FrameParser mParser;

  // Whether we've locked onto a valid sequence of frames or not.
  bool mFrameLock;

  // Current byte offset in the source stream.
  int64_t mOffset;

  // Byte offset of the begin of the first frame, or 0 if none parsed yet.
  int64_t mFirstFrameOffset;

  // Total parsed frames.
  uint64_t mNumParsedFrames;

  // Current frame index.
  int64_t mFrameIndex;

  // Sum of parsed frames' lengths in bytes.
  uint64_t mTotalFrameLen;

  // Samples per frame metric derived from frame headers or 0 if none available.
  int32_t mSamplesPerFrame;

  // Samples per second metric derived from frame headers or 0 if none available.
  int32_t mSamplesPerSecond;

  // Channel count derived from frame headers or 0 if none available.
  int32_t mChannels;

  // Audio track config info.
  UniquePtr<AudioInfo> mInfo;
};

} // namespace mozilla

#endif
