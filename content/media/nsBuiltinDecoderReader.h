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
#if !defined(nsBuiltinDecoderReader_h_)
#define nsBuiltinDecoderReader_h_

#include <nsDeque.h>
#include "Layers.h"
#include "ImageLayers.h"
#include "nsAutoLock.h"
#include "nsClassHashtable.h"
#include "mozilla/TimeStamp.h"
#include "nsSize.h"
#include "nsRect.h"
#include "mozilla/Monitor.h"

class nsBuiltinDecoderStateMachine;

// Stores info relevant to presenting media samples.
class nsVideoInfo {
public:
  nsVideoInfo()
    : mPixelAspectRatio(1.0),
      mAudioRate(0),
      mAudioChannels(0),
      mFrame(0,0),
      mDisplay(0,0),
      mStereoMode(mozilla::layers::STEREO_MODE_MONO),
      mHasAudio(PR_FALSE),
      mHasVideo(PR_FALSE)
  {}

  // Returns PR_TRUE if it's safe to use aPicture as the picture to be
  // extracted inside a frame of size aFrame, and scaled up to and displayed
  // at a size of aDisplay. You should validate the frame, picture, and
  // display regions before setting them into the mFrame, mPicture and
  // mDisplay fields of nsVideoInfo.
  static PRBool ValidateVideoRegion(const nsIntSize& aFrame,
                                    const nsIntRect& aPicture,
                                    const nsIntSize& aDisplay);

  // Pixel aspect ratio, as stored in the metadata.
  float mPixelAspectRatio;

  // Samples per second.
  PRUint32 mAudioRate;

  // Number of audio channels.
  PRUint32 mAudioChannels;

  // Dimensions of the video frame.
  nsIntSize mFrame;

  // The picture region inside the video frame to be displayed.
  nsIntRect mPicture;

  // Display size of the video frame. The picture region will be scaled
  // to and displayed at this size.
  nsIntSize mDisplay;

  // The offset of the first non-header page in the file, in bytes.
  // Used to seek to the start of the media.
  PRInt64 mDataOffset;

  // Indicates the frame layout for single track stereo videos.
  mozilla::layers::StereoMode mStereoMode;

  // PR_TRUE if we have an active audio bitstream.
  PRPackedBool mHasAudio;

  // PR_TRUE if we have an active video bitstream.
  PRPackedBool mHasVideo;
};

#ifdef MOZ_TREMOR
#include <ogg/os_types.h>
typedef ogg_int32_t VorbisPCMValue;
typedef short SoundDataValue;

#define MOZ_SOUND_DATA_FORMAT (nsAudioStream::FORMAT_S16_LE)
#define MOZ_CLIP_TO_15(x) ((x)<-32768?-32768:(x)<=32767?(x):32767)
// Convert the output of vorbis_synthesis_pcmout to a SoundDataValue
#define MOZ_CONVERT_VORBIS_SAMPLE(x) \
 (static_cast<SoundDataValue>(MOZ_CLIP_TO_15((x)>>9)))
// Convert a SoundDataValue to a float for the Audio API
#define MOZ_CONVERT_SOUND_SAMPLE(x) ((x)*(1.F/32768))

#else /*MOZ_VORBIS*/

typedef float VorbisPCMValue;
typedef float SoundDataValue;

#define MOZ_SOUND_DATA_FORMAT (nsAudioStream::FORMAT_FLOAT32)
#define MOZ_CONVERT_VORBIS_SAMPLE(x) (x)
#define MOZ_CONVERT_SOUND_SAMPLE(x) (x)

#endif

// Holds chunk a decoded sound samples.
class SoundData {
public:
  SoundData(PRInt64 aOffset,
            PRInt64 aTime,
            PRInt64 aDuration,
            PRUint32 aSamples,
            SoundDataValue* aData,
            PRUint32 aChannels)
  : mOffset(aOffset),
    mTime(aTime),
    mDuration(aDuration),
    mSamples(aSamples),
    mChannels(aChannels),
    mAudioData(aData)
  {
    MOZ_COUNT_CTOR(SoundData);
  }

  SoundData(PRInt64 aOffset,
            PRInt64 aDuration,
            PRUint32 aSamples,
            SoundDataValue* aData,
            PRUint32 aChannels)
  : mOffset(aOffset),
    mTime(-1),
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

  // Approximate byte offset of the end of the page on which this sample
  // chunk ends.
  const PRInt64 mOffset;

  PRInt64 mTime; // Start time of samples in ms.
  const PRInt64 mDuration; // In ms.
  const PRUint32 mSamples;
  const PRUint32 mChannels;
  nsAutoArrayPtr<SoundDataValue> mAudioData;
};

// Holds a decoded video frame, in YCbCr format. These are queued in the reader.
class VideoData {
public:
  typedef mozilla::layers::ImageContainer ImageContainer;
  typedef mozilla::layers::Image Image;

  // YCbCr data obtained from decoding the video. The index's are:
  //   0 = Y
  //   1 = Cb
  //   2 = Cr
  struct YCbCrBuffer {
    struct Plane {
      PRUint8* mData;
      PRUint32 mWidth;
      PRUint32 mHeight;
      PRUint32 mStride;
    };

    Plane mPlanes[3];
  };

  // Constructs a VideoData object. Makes a copy of YCbCr data in aBuffer.
  // aTimecode is a codec specific number representing the timestamp of
  // the frame of video data. Returns nsnull if an error occurs. This may
  // indicate that memory couldn't be allocated to create the VideoData
  // object, or it may indicate some problem with the input data (e.g.
  // negative stride).
  static VideoData* Create(nsVideoInfo& aInfo,
                           ImageContainer* aContainer,
                           PRInt64 aOffset,
                           PRInt64 aTime,
                           PRInt64 aEndTime,
                           const YCbCrBuffer &aBuffer,
                           PRBool aKeyframe,
                           PRInt64 aTimecode);

  // Constructs a duplicate VideoData object. This intrinsically tells the
  // player that it does not need to update the displayed frame when this
  // frame is played; this frame is identical to the previous.
  static VideoData* CreateDuplicate(PRInt64 aOffset,
                                    PRInt64 aTime,
                                    PRInt64 aEndTime,
                                    PRInt64 aTimecode)
  {
    return new VideoData(aOffset, aTime, aEndTime, aTimecode);
  }

  ~VideoData()
  {
    MOZ_COUNT_DTOR(VideoData);
  }

  // Approximate byte offset of the end of the frame in the media.
  PRInt64 mOffset;

  // Start time of frame in milliseconds.
  PRInt64 mTime;

  // End time of frame in milliseconds;
  PRInt64 mEndTime;

  // Codec specific internal time code. For Ogg based codecs this is the
  // granulepos.
  PRInt64 mTimecode;

  // This frame's image.
  nsRefPtr<Image> mImage;

  // When PR_TRUE, denotes that this frame is identical to the frame that
  // came before; it's a duplicate. mBuffer will be empty.
  PRPackedBool mDuplicate;
  PRPackedBool mKeyframe;

public:
  VideoData(PRInt64 aOffset, PRInt64 aTime, PRInt64 aEndTime, PRInt64 aTimecode)
    : mOffset(aOffset),
      mTime(aTime),
      mEndTime(aEndTime),
      mTimecode(aTimecode),
      mDuplicate(PR_TRUE),
      mKeyframe(PR_FALSE)
  {
    MOZ_COUNT_CTOR(VideoData);
    NS_ASSERTION(aEndTime >= aTime, "Frame must start before it ends.");
  }

  VideoData(PRInt64 aOffset,
            PRInt64 aTime,
            PRInt64 aEndTime,
            PRBool aKeyframe,
            PRInt64 aTimecode)
    : mOffset(aOffset),
      mTime(aTime),
      mEndTime(aEndTime),
      mTimecode(aTimecode),
      mDuplicate(PR_FALSE),
      mKeyframe(aKeyframe)
  {
    MOZ_COUNT_CTOR(VideoData);
    NS_ASSERTION(aEndTime >= aTime, "Frame must start before it ends.");
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
   typedef mozilla::MonitorAutoEnter MonitorAutoEnter;
   typedef mozilla::Monitor Monitor;

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

  // Returns PR_TRUE if the media queue has had it last sample added to it.
  // This happens when the media stream has been completely decoded. Note this
  // does not mean that the corresponding stream has finished playback.
  PRBool IsFinished() {
    MonitorAutoEnter mon(mMonitor);
    return mEndOfStream;    
  }

  // Informs the media queue that it won't be receiving any more samples.
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

  // PR_TRUE when we've decoded the last frame of data in the
  // bitstream for which we're queueing sample-data.
  PRBool mEndOfStream;
};

// Encapsulates the decoding and reading of media data. Reading can be done
// on either the state machine thread (when loading and seeking) or on
// the reader thread (when it's reading and decoding). The reader encapsulates
// the reading state and maintains it's own monitor to ensure thread safety
// and correctness. Never hold the nsBuiltinDecoder's monitor when calling into
// this class.
class nsBuiltinDecoderReader : public nsRunnable {
public:
  typedef mozilla::Monitor Monitor;
  typedef mozilla::MonitorAutoEnter MonitorAutoEnter;

  nsBuiltinDecoderReader(nsBuiltinDecoder* aDecoder);
  ~nsBuiltinDecoderReader();

  // Initializes the reader, returns NS_OK on success, or NS_ERROR_FAILURE
  // on failure.
  virtual nsresult Init(nsBuiltinDecoderReader* aCloneDonor) = 0;

  // Resets all state related to decoding, emptying all buffers etc.
  virtual nsresult ResetDecode();

  // Decodes an unspecified amount of audio data, enqueuing the audio data
  // in mAudioQueue. Returns PR_TRUE when there's more audio to decode,
  // PR_FALSE if the audio is finished, end of file has been reached,
  // or an un-recoverable read error has occured.
  virtual PRBool DecodeAudioData() = 0;

  // Reads and decodes one video frame. Packets with a timestamp less
  // than aTimeThreshold will be decoded (unless they're not keyframes
  // and aKeyframeSkip is PR_TRUE), but will not be added to the queue.
  virtual PRBool DecodeVideoFrame(PRBool &aKeyframeSkip,
                                  PRInt64 aTimeThreshold) = 0;

  virtual PRBool HasAudio() = 0;
  virtual PRBool HasVideo() = 0;

  // Read header data for all bitstreams in the file. Fills mInfo with
  // the data required to present the media. Returns NS_OK on success,
  // or NS_ERROR_FAILURE on failure.
  virtual nsresult ReadMetadata(nsVideoInfo* aInfo) = 0;

  // Stores the presentation time of the first frame/sample we'd be
  // able to play if we started playback at aOffset, and returns the
  // first video sample, if we have video.
  virtual VideoData* FindStartTime(PRInt64 aOffset,
                                   PRInt64& aOutStartTime);

  // Returns the end time of the last page which occurs before aEndOffset.
  // This will not read past aEndOffset. Returns -1 on failure. 
  virtual PRInt64 FindEndTime(PRInt64 aEndOffset);

  // Moves the decode head to aTime milliseconds. aStartTime and aEndTime
  // denote the start and end times of the media in ms, and aCurrentTime
  // is the current playback position in ms.
  virtual nsresult Seek(PRInt64 aTime,
                        PRInt64 aStartTime,
                        PRInt64 aEndTime,
                        PRInt64 aCurrentTime) = 0;

  // Queue of audio samples. This queue is threadsafe.
  MediaQueue<SoundData> mAudioQueue;

  // Queue of video samples. This queue is threadsafe.
  MediaQueue<VideoData> mVideoQueue;

  // Populates aBuffered with the time ranges which are buffered. aStartTime
  // must be the presentation time of the first sample/frame in the media, e.g.
  // the media time corresponding to playback time/position 0. This function
  // should only be called on the main thread.
  virtual nsresult GetBuffered(nsTimeRanges* aBuffered,
                               PRInt64 aStartTime) = 0;

  // Only used by nsWebMReader for now, so stub here rather than in every
  // reader than inherits from nsBuiltinDecoderReader.
  virtual void NotifyDataArrived(const char* aBuffer, PRUint32 aLength, PRUint32 aOffset) {}

protected:

  // Pumps the decode until we reach frames/samples required to play at
  // time aTarget (ms).
  nsresult DecodeToTarget(PRInt64 aTarget);

  // Reader decode function. Matches DecodeVideoFrame() and
  // DecodeAudioData().
  typedef PRBool (nsBuiltinDecoderReader::*DecodeFn)();

  // Calls aDecodeFn on *this until aQueue has a sample, whereupon
  // we return the first sample.
  template<class Data>
  Data* DecodeToFirstData(DecodeFn aDecodeFn,
                          MediaQueue<Data>& aQueue);

  // Wrapper so that DecodeVideoFrame(PRBool&,PRInt64) can be called from
  // DecodeToFirstData().
  PRBool DecodeVideoFrame() {
    PRBool f = PR_FALSE;
    return DecodeVideoFrame(f, 0);
  }

  // The lock which we hold whenever we read or decode. This ensures the thread
  // safety of the reader and its data fields.
  Monitor mMonitor;

  // Reference to the owning decoder object. Do not hold the
  // reader's monitor when accessing this.
  nsBuiltinDecoder* mDecoder;

  // The offset of the start of the first non-header page in the file.
  // Used to seek to media start time.
  PRInt64 mDataOffset;

  // Stores presentation info required for playback. The reader's monitor
  // must be held when accessing this.
  nsVideoInfo mInfo;
};

#endif
