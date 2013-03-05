/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(WebMReader_h_)
#define WebMReader_h_

#include "mozilla/StandardInteger.h"

#include "nsDeque.h"
#include "MediaDecoderReader.h"
#include "nsAutoRef.h"
#include "nestegg/nestegg.h"

#define VPX_DONT_DEFINE_STDINT_TYPES
#include "vpx/vpx_codec.h"

#ifdef MOZ_TREMOR
#include "tremor/ivorbiscodec.h"
#else
#include "vorbis/codec.h"
#endif

#ifdef MOZ_DASH
#include "DASHRepReader.h"
#endif

namespace mozilla {

class WebMBufferedState;

// Holds a nestegg_packet, and its file offset. This is needed so we
// know the offset in the file we've played up to, in order to calculate
// whether it's likely we can play through to the end without needing
// to stop to buffer, given the current download rate.
class NesteggPacketHolder {
public:
  NesteggPacketHolder(nestegg_packet* aPacket, int64_t aOffset)
    : mPacket(aPacket), mOffset(aOffset)
  {
    MOZ_COUNT_CTOR(NesteggPacketHolder);
  }
  ~NesteggPacketHolder() {
    MOZ_COUNT_DTOR(NesteggPacketHolder);
    nestegg_free_packet(mPacket);
  }
  nestegg_packet* mPacket;
  // Offset in bytes. This is the offset of the end of the Block
  // which contains the packet.
  int64_t mOffset;
private:
  // Copy constructor and assignment operator not implemented. Don't use them!
  NesteggPacketHolder(const NesteggPacketHolder &aOther);
  NesteggPacketHolder& operator= (NesteggPacketHolder const& aOther);
};

// Thread and type safe wrapper around nsDeque.
class PacketQueueDeallocator : public nsDequeFunctor {
  virtual void* operator() (void* anObject) {
    delete static_cast<NesteggPacketHolder*>(anObject);
    return nullptr;
  }
};

// Typesafe queue for holding nestegg packets. It has
// ownership of the items in the queue and will free them
// when destroyed.
class WebMPacketQueue : private nsDeque {
 public:
   WebMPacketQueue()
     : nsDeque(new PacketQueueDeallocator())
   {}
  
  ~WebMPacketQueue() {
    Reset();
  }

  inline int32_t GetSize() { 
    return nsDeque::GetSize();
  }
  
  inline void Push(NesteggPacketHolder* aItem) {
    NS_ASSERTION(aItem, "NULL pushed to WebMPacketQueue");
    nsDeque::Push(aItem);
  }
  
  inline void PushFront(NesteggPacketHolder* aItem) {
    NS_ASSERTION(aItem, "NULL pushed to WebMPacketQueue");
    nsDeque::PushFront(aItem);
  }

  inline NesteggPacketHolder* PopFront() {
    return static_cast<NesteggPacketHolder*>(nsDeque::PopFront());
  }
  
  void Reset() {
    while (GetSize() > 0) {
      delete PopFront();
    }
  }
};

#ifdef MOZ_DASH
class WebMReader : public DASHRepReader
#else
class WebMReader : public MediaDecoderReader
#endif
{
public:
  WebMReader(AbstractMediaDecoder* aDecoder);
  ~WebMReader();

  virtual nsresult Init(MediaDecoderReader* aCloneDonor);
  virtual nsresult ResetDecode();
  virtual bool DecodeAudioData();

  // If the Theora granulepos has not been captured, it may read several packets
  // until one with a granulepos has been captured, to ensure that all packets
  // read have valid time info.  
  virtual bool DecodeVideoFrame(bool &aKeyframeSkip,
                                  int64_t aTimeThreshold);

  virtual bool HasAudio()
  {
    NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");
    return mHasAudio;
  }

  virtual bool HasVideo()
  {
    NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");
    return mHasVideo;
  }

  virtual nsresult ReadMetadata(VideoInfo* aInfo,
                                MetadataTags** aTags);
  virtual nsresult Seek(int64_t aTime, int64_t aStartTime, int64_t aEndTime, int64_t aCurrentTime);
  virtual nsresult GetBuffered(TimeRanges* aBuffered, int64_t aStartTime);
  virtual void NotifyDataArrived(const char* aBuffer, uint32_t aLength, int64_t aOffset);

#ifdef MOZ_DASH
  virtual void SetMainReader(DASHReader *aMainReader) MOZ_OVERRIDE {
    NS_ASSERTION(aMainReader, "aMainReader is null.");
    mMainReader = aMainReader;
  }

  // Called by |DASHReader| on the decode thread so that this reader will
  // start reading at the appropriate subsegment/cluster. If this is not the
  // current reader and a switch was previously requested, then it will seek to
  // starting offset of the subsegment at which it is supposed to switch.
  // Called on the decode thread, enters the decode monitor.
  void PrepareToDecode() MOZ_OVERRIDE;

  // Returns a reference to the audio/video queue of the main reader.
  // Allows for a single audio/video queue to be shared among multiple
  // |WebMReader|s.
  MediaQueue<AudioData>& AudioQueue() MOZ_OVERRIDE {
    if (mMainReader) {
      return mMainReader->AudioQueue();
    } else {
      return MediaDecoderReader::AudioQueue();
    }
  }

  MediaQueue<VideoData>& VideoQueue() MOZ_OVERRIDE {
    if (mMainReader) {
      return mMainReader->VideoQueue();
    } else {
      return MediaDecoderReader::VideoQueue();
    }
  }

  // Sets byte range for initialization (EBML); used by DASH.
  void SetInitByteRange(MediaByteRange &aByteRange) MOZ_OVERRIDE {
    mInitByteRange = aByteRange;
  }

  // Sets byte range for cue points, i.e. cluster offsets; used by DASH.
  void SetIndexByteRange(MediaByteRange &aByteRange) MOZ_OVERRIDE {
    mCuesByteRange = aByteRange;
  }

  // Returns the index of the subsegment which contains the seek time.
  int64_t GetSubsegmentForSeekTime(int64_t aSeekToTime) MOZ_OVERRIDE;

  // Returns list of ranges for cluster start and end offsets.
  nsresult GetSubsegmentByteRanges(nsTArray<MediaByteRange>& aByteRanges)
                                                                  MOZ_OVERRIDE;

  // Called by |DASHReader|::|PossiblySwitchVideoReaders| to check if this
  // reader has reached a switch access point and it's ok to switch readers.
  // Called on the decode thread.
  bool HasReachedSubsegment(uint32_t aSubsegmentIndex) MOZ_OVERRIDE;

  // Requests that this reader seek to the specified subsegment. Seek will
  // happen when |PrepareDecodeVideoFrame| is called on the decode
  // thread.
  // Called on the main thread or decoder thread. Decode monitor must be held.
  void RequestSeekToSubsegment(uint32_t aIdx) MOZ_OVERRIDE;

  // Requests that this reader switch to |aNextReader| at the start of the
  // specified subsegment. This is the reader to switch FROM.
  // Called on the main thread or decoder thread. Decode monitor must be held.
  void RequestSwitchAtSubsegment(int32_t aSubsegmentIdx,
                                 MediaDecoderReader* aNextReader) MOZ_OVERRIDE;

  // Seeks to the beginning of the specified cluster. Called on the decode
  // thread.
  void SeekToCluster(uint32_t aIdx);

  // Returns true if data at the end of the final subsegment has been cached.
  bool IsDataCachedAtEndOfSubsegments() MOZ_OVERRIDE;
#endif

protected:
  // Value passed to NextPacket to determine if we are reading a video or an
  // audio packet.
  enum TrackType {
    VIDEO = 0,
    AUDIO = 1
  };

  // Read a packet from the nestegg file. Returns NULL if all packets for
  // the particular track have been read. Pass VIDEO or AUDIO to indicate the
  // type of the packet we want to read.
#ifdef MOZ_DASH
  nsReturnRef<NesteggPacketHolder> NextPacketInternal(TrackType aTrackType);

  // Read a packet from the nestegg file. Returns NULL if all packets for
  // the particular track have been read. Pass VIDEO or AUDIO to indicate the
  // type of the packet we want to read. If the reader reaches a switch access
  // point, this function will get a packet from |mNextReader|.
#endif
  nsReturnRef<NesteggPacketHolder> NextPacket(TrackType aTrackType);

  // Pushes a packet to the front of the video packet queue.
  virtual void PushVideoPacket(NesteggPacketHolder* aItem);

  // Returns an initialized ogg packet with data obtained from the WebM container.
  ogg_packet InitOggPacket(unsigned char* aData,
                           size_t aLength,
                           bool aBOS,
                           bool aEOS,
                           int64_t aGranulepos);

  // Decode a nestegg packet of audio data. Push the audio data on the
  // audio queue. Returns true when there's more audio to decode,
  // false if the audio is finished, end of file has been reached,
  // or an un-recoverable read error has occured. The reader's monitor
  // must be held during this call. This function will free the packet
  // so the caller must not use the packet after calling.
  bool DecodeAudioPacket(nestegg_packet* aPacket, int64_t aOffset);

  // Release context and set to null. Called when an error occurs during
  // reading metadata or destruction of the reader itself.
  void Cleanup();

private:
  // libnestegg context for webm container. Access on state machine thread
  // or decoder thread only.
  nestegg* mContext;

  // VP8 decoder state
  vpx_codec_ctx_t mVP8;

  // Vorbis decoder state
  vorbis_info mVorbisInfo;
  vorbis_comment mVorbisComment;
  vorbis_dsp_state mVorbisDsp;
  vorbis_block mVorbisBlock;
  uint32_t mPacketCount;
  uint32_t mChannels;

  // Queue of video and audio packets that have been read but not decoded. These
  // must only be accessed from the state machine thread.
  WebMPacketQueue mVideoPackets;
  WebMPacketQueue mAudioPackets;

  // Index of video and audio track to play
  uint32_t mVideoTrack;
  uint32_t mAudioTrack;

  // Time in microseconds of the start of the first audio frame we've decoded.
  int64_t mAudioStartUsec;

  // Number of audio frames we've decoded since decoding began at mAudioStartMs.
  uint64_t mAudioFrames;

  // Parser state and computed offset-time mappings.  Shared by multiple
  // readers when decoder has been cloned.  Main thread only.
  nsRefPtr<WebMBufferedState> mBufferedState;

  // Size of the frame initially present in the stream. The picture region
  // is defined as a ratio relative to this.
  nsIntSize mInitialFrame;

  // Picture region, as relative to the initial frame size.
  nsIntRect mPicture;

  // Booleans to indicate if we have audio and/or video data
  bool mHasVideo;
  bool mHasAudio;

#ifdef MOZ_DASH
  // Byte range for initialisation data; e.g. specified in DASH manifest.
  MediaByteRange mInitByteRange;

  // Byte range for cues; e.g. specified in DASH manifest.
  MediaByteRange mCuesByteRange;

  // Byte ranges for clusters; set internally, derived from cues.
  nsTArray<TimestampedMediaByteRange> mClusterByteRanges;

  // Pointer to the main |DASHReader|. Set in the constructor.
  DASHReader* mMainReader;

  // Index of the cluster to switch to. Monitor must be entered for write
  // access on all threads, read access off the decode thread.
  int32_t mSwitchingCluster;

  // Pointer to the next reader. Used in |NextPacket| and |PushVideoPacket| at
  // switch access points. Monitor must be entered for write access on all
  // threads, read access off the decode thread.
  nsRefPtr<WebMReader> mNextReader;

  // Index of the cluster to seek to for a DASH stream request. Monitor must be
  // entered for write access on all threads, read access off the decode
  // thread.
  int32_t mSeekToCluster;

  // Current end offset of the last packet read in |NextPacket|. Used to check
  // if the reader reached the switch access point. Accessed on the decode
  // thread only.
  int64_t mCurrentOffset;

  // Index of next cluster to be read. Used to determine the starting offset of
  // the next cluster. Accessed on the decode thread only.
  uint32_t mNextCluster;

  // Set in |NextPacket| if we read a packet from the next reader. If true in
  // |PushVideoPacket|, we will push the packet onto the next reader's
  // video packet queue (not video data queue!). Accessed on decode thread
  // only.
  bool mPushVideoPacketToNextReader;

  // Indicates if the reader has reached a switch access point.  Set in
  // |NextPacket| and read in |HasReachedSubsegment|. Accessed on
  // decode thread only.
  bool mReachedSwitchAccessPoint;
#endif
};

} // namespace mozilla

#endif
