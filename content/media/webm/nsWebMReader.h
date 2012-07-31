/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(nsWebMReader_h_)
#define nsWebMReader_h_

#include "mozilla/StandardInteger.h"

#include "nsDeque.h"
#include "nsBuiltinDecoderReader.h"
#include "nsAutoRef.h"
#include "nestegg/nestegg.h"

#define VPX_DONT_DEFINE_STDINT_TYPES
#include "vpx/vpx_codec.h"

#ifdef MOZ_TREMOR
#include "tremor/ivorbiscodec.h"
#else
#include "vorbis/codec.h"
#endif

class nsWebMBufferedState;

// Holds a nestegg_packet, and its file offset. This is needed so we
// know the offset in the file we've played up to, in order to calculate
// whether it's likely we can play through to the end without needing
// to stop to buffer, given the current download rate.
class NesteggPacketHolder {
public:
  NesteggPacketHolder(nestegg_packet* aPacket, PRInt64 aOffset)
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
  PRInt64 mOffset;
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
class PacketQueue : private nsDeque {
 public:
   PacketQueue()
     : nsDeque(new PacketQueueDeallocator())
   {}
  
  ~PacketQueue() {
    Reset();
  }

  inline PRInt32 GetSize() { 
    return nsDeque::GetSize();
  }
  
  inline void Push(NesteggPacketHolder* aItem) {
    NS_ASSERTION(aItem, "NULL pushed to PacketQueue");
    nsDeque::Push(aItem);
  }
  
  inline void PushFront(NesteggPacketHolder* aItem) {
    NS_ASSERTION(aItem, "NULL pushed to PacketQueue");
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

class nsWebMReader : public nsBuiltinDecoderReader
{
public:
  nsWebMReader(nsBuiltinDecoder* aDecoder);
  ~nsWebMReader();

  virtual nsresult Init(nsBuiltinDecoderReader* aCloneDonor);
  virtual nsresult ResetDecode();
  virtual bool DecodeAudioData();

  // If the Theora granulepos has not been captured, it may read several packets
  // until one with a granulepos has been captured, to ensure that all packets
  // read have valid time info.  
  virtual bool DecodeVideoFrame(bool &aKeyframeSkip,
                                  PRInt64 aTimeThreshold);

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

  // Bug 575140, cannot seek in webm if no cue is present.
  bool IsSeekableInBufferedRanges() {
    return false;
  }

  virtual nsresult ReadMetadata(nsVideoInfo* aInfo,
                                nsHTMLMediaElement::MetadataTags** aTags);
  virtual nsresult Seek(PRInt64 aTime, PRInt64 aStartTime, PRInt64 aEndTime, PRInt64 aCurrentTime);
  virtual nsresult GetBuffered(nsTimeRanges* aBuffered, PRInt64 aStartTime);
  virtual void NotifyDataArrived(const char* aBuffer, PRUint32 aLength, PRInt64 aOffset);

private:
  // Value passed to NextPacket to determine if we are reading a video or an
  // audio packet.
  enum TrackType {
    VIDEO = 0,
    AUDIO = 1
  };

  // Read a packet from the nestegg file. Returns NULL if all packets for
  // the particular track have been read. Pass VIDEO or AUDIO to indicate the
  // type of the packet we want to read.
  nsReturnRef<NesteggPacketHolder> NextPacket(TrackType aTrackType);

  // Returns an initialized ogg packet with data obtained from the WebM container.
  ogg_packet InitOggPacket(unsigned char* aData,
                           size_t aLength,
                           bool aBOS,
                           bool aEOS,
                           PRInt64 aGranulepos);

  // Decode a nestegg packet of audio data. Push the audio data on the
  // audio queue. Returns true when there's more audio to decode,
  // false if the audio is finished, end of file has been reached,
  // or an un-recoverable read error has occured. The reader's monitor
  // must be held during this call. This function will free the packet
  // so the caller must not use the packet after calling.
  bool DecodeAudioPacket(nestegg_packet* aPacket, PRInt64 aOffset);

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
  PRUint32 mPacketCount;
  PRUint32 mChannels;

  // Queue of video and audio packets that have been read but not decoded. These
  // must only be accessed from the state machine thread.
  PacketQueue mVideoPackets;
  PacketQueue mAudioPackets;

  // Index of video and audio track to play
  PRUint32 mVideoTrack;
  PRUint32 mAudioTrack;

  // Time in microseconds of the start of the first audio frame we've decoded.
  PRInt64 mAudioStartUsec;

  // Number of audio frames we've decoded since decoding began at mAudioStartMs.
  PRUint64 mAudioFrames;

  // Parser state and computed offset-time mappings.  Shared by multiple
  // readers when decoder has been cloned.  Main thread only.
  nsRefPtr<nsWebMBufferedState> mBufferedState;

  // Size of the frame initially present in the stream. The picture region
  // is defined as a ratio relative to this.
  nsIntSize mInitialFrame;

  // Picture region, as relative to the initial frame size.
  nsIntRect mPicture;

  // Booleans to indicate if we have audio and/or video data
  bool mHasVideo;
  bool mHasAudio;
};

#endif
