/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(WebMDemuxer_h_)
#define WebMDemuxer_h_

#include "nsTArray.h"
#include "MediaDataDemuxer.h"

typedef struct nestegg nestegg;

namespace mozilla {

class NesteggPacketHolder;
class WebMBufferedState;
class WebMPacketQueue;

// Queue for holding MediaRawData samples
class MediaRawDataQueue {
 public:
  uint32_t GetSize() {
    return mQueue.size();
  }

  void Push(MediaRawData* aItem) {
    mQueue.push_back(aItem);
  }

  void PushFront(MediaRawData* aItem) {
    mQueue.push_front(aItem);
  }

  already_AddRefed<MediaRawData> PopFront() {
    nsRefPtr<MediaRawData> result = mQueue.front();
    mQueue.pop_front();
    return result.forget();
  }

  void Reset() {
    while (!mQueue.empty()) {
      mQueue.pop_front();
    }
  }

private:
  std::deque<nsRefPtr<MediaRawData>> mQueue;
};

class WebMTrackDemuxer;

class WebMDemuxer : public MediaDataDemuxer
{
public:
  explicit WebMDemuxer(MediaResource* aResource);
  // Indicate if the WebMDemuxer is to be used with MediaSource. In which
  // case the demuxer will stop reads to the last known complete block.
  WebMDemuxer(MediaResource* aResource, bool aIsMediaSource);
  
  nsRefPtr<InitPromise> Init() override;

  already_AddRefed<MediaDataDemuxer> Clone() const override;

  bool HasTrackType(TrackInfo::TrackType aType) const override;

  uint32_t GetNumberTracks(TrackInfo::TrackType aType) const override;

  UniquePtr<TrackInfo> GetTrackInfo(TrackInfo::TrackType aType, size_t aTrackNumber) const;

  already_AddRefed<MediaTrackDemuxer> GetTrackDemuxer(TrackInfo::TrackType aType,
                                                      uint32_t aTrackNumber) override;

  bool IsSeekable() const override;

  UniquePtr<EncryptionInfo> GetCrypto() override;

  bool GetOffsetForTime(uint64_t aTime, int64_t* aOffset);

  // Demux next WebM packet and append samples to MediaRawDataQueue
  bool GetNextPacket(TrackInfo::TrackType aType, MediaRawDataQueue *aSamples);

  nsresult Reset();

  // Pushes a packet to the front of the audio packet queue.
  virtual void PushAudioPacket(NesteggPacketHolder* aItem);

  // Pushes a packet to the front of the video packet queue.
  virtual void PushVideoPacket(NesteggPacketHolder* aItem);

  // Public accessor for nestegg callbacks
  MediaResourceIndex* GetResource()
  {
    return &mResource;
  }

  int64_t GetEndDataOffset() const
  {
    return (!mIsMediaSource || mLastWebMBlockOffset < 0)
      ? mResource.GetLength() : mLastWebMBlockOffset;
  }
  int64_t IsMediaSource() const
  {
    return mIsMediaSource;
  }

private:
  friend class WebMTrackDemuxer;

  ~WebMDemuxer();
  void Cleanup();
  nsresult InitBufferedState();
  nsresult ReadMetadata();
  void NotifyDataArrived(uint32_t aLength, int64_t aOffset) override;
  void NotifyDataRemoved() override;
  void EnsureUpToDateIndex();
  media::TimeIntervals GetBuffered();
  virtual nsresult SeekInternal(const media::TimeUnit& aTarget);
  // Get the timestamp of the next keyframe
  int64_t GetNextKeyframeTime();

  // Read a packet from the nestegg file. Returns nullptr if all packets for
  // the particular track have been read. Pass TrackInfo::kVideoTrack or
  // TrackInfo::kVideoTrack to indicate the type of the packet we want to read.
  nsRefPtr<NesteggPacketHolder> NextPacket(TrackInfo::TrackType aType);

  // Internal method that demuxes the next packet from the stream. The caller
  // is responsible for making sure it doesn't get lost.
  nsRefPtr<NesteggPacketHolder> DemuxPacket();

  MediaResourceIndex mResource;
  MediaInfo mInfo;
  nsTArray<nsRefPtr<WebMTrackDemuxer>> mDemuxers;

  // Parser state and computed offset-time mappings.  Shared by multiple
  // readers when decoder has been cloned.  Main thread only.
  nsRefPtr<WebMBufferedState> mBufferedState;
  nsRefPtr<MediaByteBuffer> mInitData;

  // libnestegg context for webm container.
  // Access on reader's thread for main demuxer,
  // or main thread for cloned demuxer
  nestegg* mContext;

  // Queue of video and audio packets that have been read but not decoded.
  WebMPacketQueue mVideoPackets;
  WebMPacketQueue mAudioPackets;

  // Index of video and audio track to play
  uint32_t mVideoTrack;
  uint32_t mAudioTrack;

  // Number of microseconds that must be discarded from the start of the Stream.
  uint64_t mCodecDelay;

  // Nanoseconds to discard after seeking.
  uint64_t mSeekPreroll;

  int64_t mLastAudioFrameTime;

  // Calculate the frame duration from the last decodeable frame using the
  // previous frame's timestamp.  In NS.
  int64_t mLastVideoFrameTime;

  // Codec ID of audio track
  int mAudioCodec;
  // Codec ID of video track
  int mVideoCodec;

  // Booleans to indicate if we have audio and/or video data
  bool mHasVideo;
  bool mHasAudio;
  bool mNeedReIndex;

  // The last complete block parsed by the WebMBufferedState. -1 if not set.
  // We cache those values rather than retrieving them for performance reasons
  // as nestegg only performs 1-byte read at a time.
  int64_t mLastWebMBlockOffset;
  const bool mIsMediaSource;
};

class WebMTrackDemuxer : public MediaTrackDemuxer
{
public:
  WebMTrackDemuxer(WebMDemuxer* aParent,
                  TrackInfo::TrackType aType,
                  uint32_t aTrackNumber);

  UniquePtr<TrackInfo> GetInfo() const override;

  nsRefPtr<SeekPromise> Seek(media::TimeUnit aTime) override;

  nsRefPtr<SamplesPromise> GetSamples(int32_t aNumSamples = 1) override;

  void Reset() override;

  nsresult GetNextRandomAccessPoint(media::TimeUnit* aTime) override;

  nsRefPtr<SkipAccessPointPromise> SkipToNextRandomAccessPoint(media::TimeUnit aTimeThreshold) override;

  media::TimeIntervals GetBuffered() override;

  int64_t GetEvictionOffset(media::TimeUnit aTime) override;

  void BreakCycles() override;

private:
  friend class WebMDemuxer;
  ~WebMTrackDemuxer();
  void UpdateSamples(nsTArray<nsRefPtr<MediaRawData>>& aSamples);
  void SetNextKeyFrameTime();
  nsRefPtr<MediaRawData> NextSample ();
  nsRefPtr<WebMDemuxer> mParent;
  TrackInfo::TrackType mType;
  UniquePtr<TrackInfo> mInfo;
  Maybe<media::TimeUnit> mNextKeyframeTime;

  // Queued samples extracted by the demuxer, but not yet returned.
  MediaRawDataQueue mSamples;
};

} // namespace mozilla

#endif
