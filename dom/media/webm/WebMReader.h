/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(WebMReader_h_)
#define WebMReader_h_

#include <stdint.h>

#include "MediaDecoderReader.h"
#include "nsAutoRef.h"
#include "nestegg/nestegg.h"

#define VPX_DONT_DEFINE_STDINT_TYPES
#include "vpx/vpx_codec.h"

#include "mozilla/layers/LayersTypes.h"

#ifdef MOZ_TREMOR
#include "tremor/ivorbiscodec.h"
#else
#include "vorbis/codec.h"
#endif

#include "OpusParser.h"

namespace mozilla {
static const unsigned NS_PER_USEC = 1000;
static const double NS_PER_S = 1e9;

// Holds a nestegg_packet, and its file offset. This is needed so we
// know the offset in the file we've played up to, in order to calculate
// whether it's likely we can play through to the end without needing
// to stop to buffer, given the current download rate.
class NesteggPacketHolder {
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(NesteggPacketHolder)
  NesteggPacketHolder() : mPacket(nullptr), mOffset(-1), mTimestamp(-1), mIsKeyframe(false) {}

  bool Init(nestegg_packet* aPacket, int64_t aOffset, unsigned aTrack, bool aIsKeyframe)
  {
    uint64_t timestamp_ns;
    if (nestegg_packet_tstamp(aPacket, &timestamp_ns) == -1) {
      return false;
    }

    // We store the timestamp as signed microseconds so that it's easily
    // comparable to other timestamps we have in the system.
    mTimestamp = timestamp_ns / 1000;
    mPacket = aPacket;
    mOffset = aOffset;
    mTrack = aTrack;
    mIsKeyframe = aIsKeyframe;

    return true;
  }

  nestegg_packet* Packet() { MOZ_ASSERT(IsInitialized()); return mPacket; }
  int64_t Offset() { MOZ_ASSERT(IsInitialized()); return mOffset; }
  int64_t Timestamp() { MOZ_ASSERT(IsInitialized()); return mTimestamp; }
  unsigned Track() { MOZ_ASSERT(IsInitialized()); return mTrack; }
  bool IsKeyframe() { MOZ_ASSERT(IsInitialized()); return mIsKeyframe; }

private:
  ~NesteggPacketHolder()
  {
    nestegg_free_packet(mPacket);
  }

  bool IsInitialized() { return mOffset >= 0; }

  nestegg_packet* mPacket;

  // Offset in bytes. This is the offset of the end of the Block
  // which contains the packet.
  int64_t mOffset;

  // Packet presentation timestamp in microseconds.
  int64_t mTimestamp;

  // Track ID.
  unsigned mTrack;

  // Does this packet contain a keyframe?
  bool mIsKeyframe;

  // Copy constructor and assignment operator not implemented. Don't use them!
  NesteggPacketHolder(const NesteggPacketHolder &aOther);
  NesteggPacketHolder& operator= (NesteggPacketHolder const& aOther);
};

class WebMBufferedState;

// Queue for holding nestegg packets.
class WebMPacketQueue {
 public:
  int32_t GetSize() {
    return mQueue.size();
  }

  void Push(already_AddRefed<NesteggPacketHolder> aItem) {
    mQueue.push_back(Move(aItem));
  }

  void PushFront(already_AddRefed<NesteggPacketHolder> aItem) {
    mQueue.push_front(Move(aItem));
  }

  already_AddRefed<NesteggPacketHolder> PopFront() {
    nsRefPtr<NesteggPacketHolder> result = mQueue.front().forget();
    mQueue.pop_front();
    return result.forget();
  }

  void Reset() {
    while (!mQueue.empty()) {
      mQueue.pop_front();
    }
  }

private:
  std::deque<nsRefPtr<NesteggPacketHolder>> mQueue;
};

class WebMReader;

// Class to handle various video decode paths
class WebMVideoDecoder
{
public:
  virtual nsresult Init(unsigned int aWidth = 0, unsigned int aHeight = 0) = 0;
  virtual nsresult Flush() { return NS_OK; }
  virtual void Shutdown() = 0;
  virtual bool DecodeVideoFrame(bool &aKeyframeSkip,
                                int64_t aTimeThreshold) = 0;
  WebMVideoDecoder() {}
  virtual ~WebMVideoDecoder() {}
};

class WebMReader : public MediaDecoderReader
{
public:
  explicit WebMReader(AbstractMediaDecoder* aDecoder);

protected:
  ~WebMReader();

public:
  virtual nsRefPtr<ShutdownPromise> Shutdown() override;
  virtual nsresult Init(MediaDecoderReader* aCloneDonor) override;
  virtual nsresult ResetDecode() override;
  virtual bool DecodeAudioData() override;

  virtual bool DecodeVideoFrame(bool &aKeyframeSkip,
                                int64_t aTimeThreshold) override;

  virtual bool HasAudio() override
  {
    MOZ_ASSERT(OnTaskQueue());
    return mHasAudio;
  }

  virtual bool HasVideo() override
  {
    MOZ_ASSERT(OnTaskQueue());
    return mHasVideo;
  }

  virtual nsresult ReadMetadata(MediaInfo* aInfo,
                                MetadataTags** aTags) override;
  virtual nsRefPtr<SeekPromise>
  Seek(int64_t aTime, int64_t aEndTime) override;

  virtual nsresult GetBuffered(dom::TimeRanges* aBuffered) override;
  virtual void NotifyDataArrived(const char* aBuffer, uint32_t aLength,
                                 int64_t aOffset) override;
  virtual int64_t GetEvictionOffset(double aTime) override;

  virtual bool IsMediaSeekable() override;

  // Value passed to NextPacket to determine if we are reading a video or an
  // audio packet.
  enum TrackType {
    VIDEO = 0,
    AUDIO = 1
  };

  // Read a packet from the nestegg file. Returns nullptr if all packets for
  // the particular track have been read. Pass VIDEO or AUDIO to indicate the
  // type of the packet we want to read.
  already_AddRefed<NesteggPacketHolder> NextPacket(TrackType aTrackType);

  // Pushes a packet to the front of the video packet queue.
  virtual void PushVideoPacket(already_AddRefed<NesteggPacketHolder> aItem);

  int GetVideoCodec();
  nsIntRect GetPicture();
  nsIntSize GetInitialFrame();
  int64_t GetLastVideoFrameTime();
  void SetLastVideoFrameTime(int64_t aFrameTime);
  layers::LayersBackend GetLayersBackendType() { return mLayersBackendType; }
  FlushableMediaTaskQueue* GetVideoTaskQueue() { return mVideoTaskQueue; }

protected:
  // Setup opus decoder
  bool InitOpusDecoder();

  // Decode a nestegg packet of audio data. Push the audio data on the
  // audio queue. Returns true when there's more audio to decode,
  // false if the audio is finished, end of file has been reached,
  // or an un-recoverable read error has occured. The reader's monitor
  // must be held during this call. The caller is responsible for freeing
  // aPacket.
  bool DecodeAudioPacket(NesteggPacketHolder* aHolder);
  bool DecodeVorbis(const unsigned char* aData, size_t aLength,
                    int64_t aOffset, uint64_t aTstampUsecs,
                    int32_t* aTotalFrames);
  bool DecodeOpus(const unsigned char* aData, size_t aLength,
                  int64_t aOffset, uint64_t aTstampUsecs,
                  nestegg_packet* aPacket);

  // Release context and set to null. Called when an error occurs during
  // reading metadata or destruction of the reader itself.
  void Cleanup();

  virtual nsresult SeekInternal(int64_t aTime);

  // Initializes mLayersBackendType if possible.
  void InitLayersBackendType();

  bool ShouldSkipVideoFrame(int64_t aTimeThreshold);

private:
  // Get the timestamp of keyframe greater than aTimeThreshold.
  int64_t GetNextKeyframeTime(int64_t aTimeThreshold);
  // Push the packets into aOutput which's timestamp is less than aEndTime.
  // Return false if we reach the end of stream or something wrong.
  bool FilterPacketByTime(int64_t aEndTime, WebMPacketQueue& aOutput);

  // Internal method that demuxes the next packet from the stream. The caller
  // is responsible for making sure it doesn't get lost.
  already_AddRefed<NesteggPacketHolder> DemuxPacket();

  // libnestegg context for webm container. Access on state machine thread
  // or decoder thread only.
  nestegg* mContext;

  // The video decoder
  nsAutoPtr<WebMVideoDecoder> mVideoDecoder;

  // Vorbis decoder state
  vorbis_info mVorbisInfo;
  vorbis_comment mVorbisComment;
  vorbis_dsp_state mVorbisDsp;
  vorbis_block mVorbisBlock;
  int64_t mPacketCount;

  // Opus decoder state
  nsAutoPtr<OpusParser> mOpusParser;
  OpusMSDecoder *mOpusDecoder;
  uint16_t mSkip;        // Samples left to trim before playback.
  uint64_t mSeekPreroll; // Nanoseconds to discard after seeking.

  // Queue of video and audio packets that have been read but not decoded. These
  // must only be accessed from the decode thread.
  WebMPacketQueue mVideoPackets;
  WebMPacketQueue mAudioPackets;

  // Index of video and audio track to play
  uint32_t mVideoTrack;
  uint32_t mAudioTrack;

  // Time in microseconds of the start of the first audio frame we've decoded.
  int64_t mAudioStartUsec;

  // Number of audio frames we've decoded since decoding began at mAudioStartMs.
  uint64_t mAudioFrames;

  // Number of microseconds that must be discarded from the start of the Stream.
  uint64_t mCodecDelay;

  // Calculate the frame duration from the last decodeable frame using the
  // previous frame's timestamp.  In NS.
  int64_t mLastVideoFrameTime;

  // Parser state and computed offset-time mappings.  Shared by multiple
  // readers when decoder has been cloned.  Main thread only.
  nsRefPtr<WebMBufferedState> mBufferedState;

  // Size of the frame initially present in the stream. The picture region
  // is defined as a ratio relative to this.
  nsIntSize mInitialFrame;

  // Picture region, as relative to the initial frame size.
  nsIntRect mPicture;

  // Codec ID of audio track
  int mAudioCodec;
  // Codec ID of video track
  int mVideoCodec;

  layers::LayersBackend mLayersBackendType;

  // For hardware video decoding.
  nsRefPtr<FlushableMediaTaskQueue> mVideoTaskQueue;

  // Booleans to indicate if we have audio and/or video data
  bool mHasVideo;
  bool mHasAudio;

  // Opus padding should only be discarded on the final packet.  Once this
  // is set to true, if the reader attempts to decode any further packets it
  // will raise an error so we can indicate that the file is invalid.
  bool mPaddingDiscarded;
};

} // namespace mozilla

#endif
