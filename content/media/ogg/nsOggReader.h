/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: ML 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Chris Double <chris.double@double.co.nz>
 *  Chris Pearce <chris@pearce.org.nz>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#if !defined(nsOggReader_h_)
#define nsOggReader_h_

#include <nsDeque.h>
#include "nsOggCodecState.h"
#include <ogg/ogg.h>
#include <theora/theoradec.h>
#include <vorbis/codec.h>
#include "nsAutoLock.h"
#include "nsClassHashtable.h"
#include "mozilla/TimeStamp.h"
#include "nsSize.h"
#include "nsRect.h"
#include "mozilla/Monitor.h"

class nsOggPlayStateMachine;

using mozilla::Monitor;
using mozilla::MonitorAutoEnter;
using mozilla::TimeDuration;
using mozilla::TimeStamp;

// Holds chunk a decoded sound samples.
class SoundData {
public:
  SoundData(PRInt64 aTime,
            PRInt64 aDuration,
            PRUint32 aSamples,
            float* aData,
            PRUint32 aChannels)
  : mTime(aTime),
    mDuration(aDuration),
    mSamples(aSamples),
    mChannels(aChannels),
    mAudioData(aData)
  {
    MOZ_COUNT_CTOR(SoundData);
  }

  SoundData(PRInt64 aDuration,
            PRUint32 aSamples,
            float* aData,
            PRUint32 aChannels)
  : mTime(-1),
    mDuration(aDuration),
    mSamples(aSamples),
    mChannels(aChannels),
    mAudioData(aData)
  {
    MOZ_COUNT_CTOR(SoundData);
  }

  ~SoundData()
  {
    MOZ_COUNT_DTOR(SoundData);
  }

  PRUint32 AudioDataLength() {
    return mChannels * mSamples;
  }

  PRInt64 mTime; // Start time of samples in ms.
  const PRInt64 mDuration; // In ms.
  const PRUint32 mSamples;
  const PRUint32 mChannels;
  nsAutoArrayPtr<float> mAudioData;
};

// Holds a decoded Theora frame, in YCbCr format. These are queued in the reader.
class VideoData {
public:

  // Constructs a VideoData object. Makes a copy of YCbCr data in aBuffer.
  // This may return nsnull if we run out of memory when allocating buffers
  // to store the frame.
  static VideoData* Create(PRInt64 aTime,
                           th_ycbcr_buffer aBuffer,
                           PRBool aKeyframe,
                           PRInt64 aGranulepos);

  // Constructs a duplicate VideoData object. This intrinsically tells the
  // player that it does not need to update the displayed frame when this
  // frame is played; this frame is identical to the previous.
  static VideoData* CreateDuplicate(PRInt64 aTime,
                                    PRInt64 aGranulepos)
  {
    return new VideoData(aTime, aGranulepos);
  }

  ~VideoData()
  {
    MOZ_COUNT_DTOR(VideoData);
    for (PRUint32 i = 0; i < 3; ++i) {
      delete mBuffer[i].data;
    }
  }

  // Start time of frame in milliseconds.
  PRInt64 mTime;
  PRInt64 mGranulepos;

  th_ycbcr_buffer mBuffer;

  // When PR_TRUE, denotes that this frame is identical to the frame that
  // came before; it's a duplicate. mBuffer will be empty.
  PRPackedBool mDuplicate;
  PRPackedBool mKeyframe;

private:
  VideoData(PRInt64 aTime, PRInt64 aGranulepos) :
    mTime(aTime),
    mGranulepos(aGranulepos),
    mDuplicate(PR_TRUE),
    mKeyframe(PR_FALSE)
  {
    MOZ_COUNT_CTOR(VideoData);
    memset(&mBuffer, 0, sizeof(th_ycbcr_buffer));
  }

  VideoData(PRInt64 aTime,
            PRBool aKeyframe,
            PRInt64 aGranulepos)
  : mTime(aTime),
    mGranulepos(aGranulepos),
    mDuplicate(PR_FALSE),
    mKeyframe(aKeyframe)
  {
    MOZ_COUNT_CTOR(VideoData);
  }

};

// Thread and type safe wrapper around nsDeque.
template <class T>
class MediaQueueDeallocator : public nsDequeFunctor {
  virtual void* operator() (void* anObject) {
    delete static_cast<T*>(anObject);
    return nsnull;
  }
};

template <class T> class MediaQueue : private nsDeque {
 public:
  
   MediaQueue()
     : nsDeque(new MediaQueueDeallocator<T>()),
       mMonitor("mediaqueue"),
       mEndOfStream(0)
   {}
  
  ~MediaQueue() {
    Reset();
  }

  inline PRInt32 GetSize() { 
    MonitorAutoEnter mon(mMonitor);
    return nsDeque::GetSize();
  }
  
  inline void Push(T* aItem) {
    MonitorAutoEnter mon(mMonitor);
    nsDeque::Push(aItem);
  }
  
  inline void PushFront(T* aItem) {
    MonitorAutoEnter mon(mMonitor);
    nsDeque::PushFront(aItem);
  }
  
  inline T* Pop() {
    MonitorAutoEnter mon(mMonitor);
    return static_cast<T*>(nsDeque::Pop());
  }

  inline T* PopFront() {
    MonitorAutoEnter mon(mMonitor);
    return static_cast<T*>(nsDeque::PopFront());
  }
  
  inline T* Peek() {
    MonitorAutoEnter mon(mMonitor);
    return static_cast<T*>(nsDeque::Peek());
  }
  
  inline T* PeekFront() {
    MonitorAutoEnter mon(mMonitor);
    return static_cast<T*>(nsDeque::PeekFront());
  }

  inline void Empty() {
    MonitorAutoEnter mon(mMonitor);
    nsDeque::Empty();
  }

  inline void Erase() {
    MonitorAutoEnter mon(mMonitor);
    nsDeque::Erase();
  }

  void Reset() {
    MonitorAutoEnter mon(mMonitor);
    while (GetSize() > 0) {
      T* x = PopFront();
      delete x;
    }
    mEndOfStream = PR_FALSE;
  }

  PRBool AtEndOfStream() {
    MonitorAutoEnter mon(mMonitor);
    return GetSize() == 0 && mEndOfStream;    
  }

  void Finish() {
    MonitorAutoEnter mon(mMonitor);
    mEndOfStream = PR_TRUE;    
  }

  // Returns the approximate number of milliseconds of samples in the queue.
  PRInt64 Duration() {
    MonitorAutoEnter mon(mMonitor);
    if (GetSize() < 2) {
      return 0;
    }
    T* last = Peek();
    T* first = PeekFront();
    return last->mTime - first->mTime;
  }

private:
  Monitor mMonitor;

  // PR_TRUE when we've decoded the last packet in the bitstream for which
  // we're queueing sample-data.
  PRBool mEndOfStream;
};

// Represents a section of contiguous media, with a start and end offset,
// and the timestamps of the start and end of that range. Used to denote the
// extremities of a range to seek in.
class ByteRange {
public:
  ByteRange() :
      mOffsetStart(0),
      mOffsetEnd(0),
      mTimeStart(0),
      mTimeEnd(0)
  {}

  ByteRange(PRInt64 aOffsetStart,
            PRInt64 aOffsetEnd,
            PRInt64 aTimeStart,
            PRInt64 aTimeEnd)
    : mOffsetStart(aOffsetStart),
      mOffsetEnd(aOffsetEnd),
      mTimeStart(aTimeStart),
      mTimeEnd(aTimeEnd)
  {}

  PRBool IsNull() {
    return mOffsetStart == 0 &&
           mOffsetEnd == 0 &&
           mTimeStart == 0 &&
           mTimeEnd == 0;
  }

  PRInt64 mOffsetStart, mOffsetEnd; // in bytes.
  PRInt64 mTimeStart, mTimeEnd; // in ms.
};

// Stores info relevant to presenting media samples.
class nsOggInfo {
public:
  nsOggInfo()
    : mFramerate(0.0),
      mAspectRatio(1.0),
      mCallbackPeriod(1),
      mAudioRate(0),
      mAudioChannels(0),
      mFrame(0,0),
      mHasAudio(PR_FALSE),
      mHasVideo(PR_FALSE)
  {}

  // Frames per second.
  float mFramerate;

  // Aspect ratio, as stored in the video header packet.
  float mAspectRatio;

  // Length of a video frame in milliseconds, or the callback period if
  // there's no audio.
  PRUint32 mCallbackPeriod;

  // Samples per second.
  PRUint32 mAudioRate;

  // Number of audio channels.
  PRUint32 mAudioChannels;

  // Dimensions of the video frame.
  nsIntSize mFrame;

  // The picture region inside the video frame to be displayed.
  nsIntRect mPicture;

  // The offset of the first non-header page in the file, in bytes.
  // Used to seek to the start of the media.
  PRInt64 mDataOffset;

  // PR_TRUE if we have an active audio bitstream.
  PRPackedBool mHasAudio;

  // PR_TRUE if we have an active video bitstream.
  PRPackedBool mHasVideo;
};

// Encapsulates the decoding and reading of Ogg data. Reading can be done
// on either the state machine thread (when loading and seeking) or on
// the reader thread (when it's reading and decoding). The reader encapsulates
// the reading state and maintains it's own monitor to ensure thread safety
// and correctness. Never hold the nsOggDecoder's monitor when calling into
// this class.
class nsOggReader : public nsRunnable {
public:
  nsOggReader(nsOggPlayStateMachine* aStateMachine);
  ~nsOggReader();

  PRBool HasAudio()
  {
    MonitorAutoEnter mon(mMonitor);
    return mVorbisState != 0 && mVorbisState->mActive;
  }

  PRBool HasVideo()
  {
    MonitorAutoEnter mon(mMonitor);
    return mTheoraState != 0 && mTheoraState->mActive;
  }

  // Read header data for all bitstreams in the Ogg file. Fills aInfo with
  // the data required to present the media. Returns NS_OK on success,
  // or NS_ERROR_FAILURE on failure.
  nsresult ReadOggHeaders(nsOggInfo& aInfo);

  // Stores the presentation time of the first sample in the stream in
  // aOutStartTime, and returns the first video sample, if we have video.
  VideoData* FindStartTime(PRInt64 aOffset,
                           PRInt64& aOutStartTime);

  // Returns the end time of the last page which occurs before aEndOffset.
  // This will not read past aEndOffset. Returns -1 on failure.
  PRInt64 FindEndTime(PRInt64 aEndOffset);

  // Decodes one Vorbis page, enqueuing the audio data in mAudioQueue.
  // Returns PR_TRUE when there's more audio to decode, PR_FALSE if the
  // audio is finished, end of file has been reached, or an un-recoverable
  // read error has occured.
  PRBool DecodeAudioPage();
  
  // Reads and decodes one video frame. If the Theora granulepos has not
  // been captured, it may read several packets until one with a granulepos
  // has been captured, to ensure that all packets read have valid time info.
  // Packets with a timestamp less than aTimeThreshold will be decoded (unless
  // they're not keyframes and aKeyframeSkip is PR_TRUE), but will not be
  // added to the queue.
  PRBool DecodeVideoPage(PRBool &aKeyframeSkip,
                         PRInt64 aTimeThreshold);

  // Moves the decode head to aTime milliseconds. aStartTime and aEndTime
  // denote the start and end times of the media.
  nsresult Seek(PRInt64 aTime, PRInt64 aStartTime, PRInt64 aEndTime);

  // Queue of audio samples. This queue is threadsafe.
  MediaQueue<SoundData> mAudioQueue;

  // Queue of video samples. This queue is threadsafe.
  MediaQueue<VideoData> mVideoQueue;

  // Initializes the reader, returns NS_OK on success, or NS_ERROR_FAILURE
  // on failure.
  nsresult Init();

private:

  // Ogg reader decode function. Matches DecodeVideoPage() and
  // DecodeAudioPage().
  typedef PRBool (nsOggReader::*DecodeFn)();

  // Calls aDecodeFn on *this until aQueue has a sample, whereupon
  // we return the first sample.
  template<class Data>
  Data* DecodeToFirstData(DecodeFn aDecodeFn,
                          MediaQueue<Data>& aQueue);

  // Wrapper so that DecodeVideoPage(PRBool&,PRInt64) can be called from
  // DecodeToFirstData().
  PRBool DecodeVideoPage() {
    PRBool f = PR_FALSE;
    return DecodeVideoPage(f, 0);
  }

  // Decodes one packet of Vorbis data, storing the resulting chunks of
  // PCM samples in aChunks.
  nsresult DecodeVorbis(nsTArray<SoundData*>& aChunks,
                        ogg_packet* aPacket);

  // May return NS_ERROR_OUT_OF_MEMORY.
  nsresult DecodeTheora(nsTArray<VideoData*>& aFrames,
                        ogg_packet* aPacket);

  // Resets all state related to decoding, emptying all buffers etc.
  nsresult ResetDecode();

  // Read a page of data from the Ogg file. Returns the offset of the start
  // of the page, or -1 if the page read failed.
  PRInt64 ReadOggPage(ogg_page* aPage);

  // Read a packet for an Ogg bitstream/codec state. Returns PR_TRUE on
  // success, or PR_FALSE if the read failed.
  PRBool ReadOggPacket(nsOggCodecState* aCodecState, ogg_packet* aPacket);

  // Performs a seek bisection to move the media stream's read cursor to the
  // last ogg page boundary which has end time before aTarget ms on both the
  // Theora and Vorbis bitstreams. Limits its search to data inside aRange;
  // i.e. it will only read inside of the aRange's start and end offsets.
  // aFuzz is the number of ms of leniency we'll allow; we'll terminate the
  // seek when we land in the range (aTime - aFuzz, aTime) ms.
  nsresult SeekBisection(PRInt64 aTarget,
                         const ByteRange& aRange,
                         PRUint32 aFuzz);

  // Fills aRanges with ByteRanges denoting the sections of the media which
  // have been downloaded and are stored in the media cache. The reader
  // monitor must must be held with exactly one lock count. The nsMediaStream
  // must be pinned while calling this.
  nsresult GetBufferedBytes(nsTArray<ByteRange>& aRanges);

  // Returns the range in which you should perform a seek bisection if
  // you wish to seek to aTarget ms, given the known (buffered) byte ranges
  // in aRanges. If aExact is PR_TRUE, we only return an exact copy of a
  // range in which aTarget lies, or a null range if aTarget isn't contained
  // in any of the (buffered) ranges. Otherwise, when aExact is PR_FALSE,
  // we'll construct the smallest possible range we can, based on the times
  // and byte offsets known in aRanges. We can then use this to minimize our
  // bisection's search space when the target isn't in a known buffered range.
  ByteRange GetSeekRange(const nsTArray<ByteRange>& aRanges,
                         PRInt64 aTarget,
                         PRInt64 aStartTime,
                         PRInt64 aEndTime,
                         PRBool aExact);

  // The lock which we hold whenever we read or decode. This ensures the thread
  // safety of the reader and its data fields.
  Monitor mMonitor;

  // Reference to the owning player state machine object. Do not hold the
  // reader's monitor when accessing the player.
  nsOggPlayStateMachine* mPlayer;

  // Maps Ogg serialnos to nsOggStreams.
  nsClassHashtable<nsUint32HashKey, nsOggCodecState> mCodecStates;

  // Decode state of the Theora bitstream we're decoding, if we have video.
  nsTheoraState* mTheoraState;

  // Decode state of the Vorbis bitstream we're decoding, if we have audio.
  nsVorbisState* mVorbisState;

  // Ogg decoding state.
  ogg_sync_state mOggState;

  // The offset of the end of the last page we've read, or the start of
  // the page we're about to read.
  PRInt64 mPageOffset;

  // The offset of the start of the first non-header page in the file.
  // Used to seek to media start time.
  PRInt64 mDataOffset;

  // The granulepos of the last decoded Theora frame.
  PRInt64 mTheoraGranulepos;

  // The granulepos of the last decoded Vorbis sample.
  PRInt64 mVorbisGranulepos;

  // Number of milliseconds of data video/audio data held in a frame.
  PRUint32 mCallbackPeriod;

};

#endif
