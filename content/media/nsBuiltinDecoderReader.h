/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(nsBuiltinDecoderReader_h_)
#define nsBuiltinDecoderReader_h_

#include <nsDeque.h>
#include "nsSize.h"
#include "mozilla/ReentrantMonitor.h"
#include "MediaStreamGraph.h"
#include "SharedBuffer.h"

// Stores info relevant to presenting media frames.
class nsVideoInfo {
public:
  nsVideoInfo()
    : mAudioRate(44100),
      mAudioChannels(2),
      mDisplay(0,0),
      mStereoMode(mozilla::layers::STEREO_MODE_MONO),
      mHasAudio(false),
      mHasVideo(false)
  {}

  // Returns true if it's safe to use aPicture as the picture to be
  // extracted inside a frame of size aFrame, and scaled up to and displayed
  // at a size of aDisplay. You should validate the frame, picture, and
  // display regions before using them to display video frames.
  static bool ValidateVideoRegion(const nsIntSize& aFrame,
                                    const nsIntRect& aPicture,
                                    const nsIntSize& aDisplay);

  // Sample rate.
  PRUint32 mAudioRate;

  // Number of audio channels.
  PRUint32 mAudioChannels;

  // Size in pixels at which the video is rendered. This is after it has
  // been scaled by its aspect ratio.
  nsIntSize mDisplay;

  // Indicates the frame layout for single track stereo videos.
  mozilla::layers::StereoMode mStereoMode;

  // True if we have an active audio bitstream.
  bool mHasAudio;

  // True if we have an active video bitstream.
  bool mHasVideo;
};

#ifdef MOZ_TREMOR
#include <ogg/os_types.h>
typedef ogg_int32_t VorbisPCMValue;
typedef short AudioDataValue;

#define MOZ_AUDIO_DATA_FORMAT (nsAudioStream::FORMAT_S16_LE)
#define MOZ_CLIP_TO_15(x) ((x)<-32768?-32768:(x)<=32767?(x):32767)
// Convert the output of vorbis_synthesis_pcmout to a AudioDataValue
#define MOZ_CONVERT_VORBIS_SAMPLE(x) \
 (static_cast<AudioDataValue>(MOZ_CLIP_TO_15((x)>>9)))
// Convert a AudioDataValue to a float for the Audio API
#define MOZ_CONVERT_AUDIO_SAMPLE(x) ((x)*(1.F/32768))
#define MOZ_SAMPLE_TYPE_S16LE 1

#else /*MOZ_VORBIS*/

typedef float VorbisPCMValue;
typedef float AudioDataValue;

#define MOZ_AUDIO_DATA_FORMAT (nsAudioStream::FORMAT_FLOAT32)
#define MOZ_CONVERT_VORBIS_SAMPLE(x) (x)
#define MOZ_CONVERT_AUDIO_SAMPLE(x) (x)
#define MOZ_SAMPLE_TYPE_FLOAT32 1

#endif

// Holds chunk a decoded audio frames.
class AudioData {
public:
  typedef mozilla::SharedBuffer SharedBuffer;

  AudioData(PRInt64 aOffset,
            PRInt64 aTime,
            PRInt64 aDuration,
            PRUint32 aFrames,
            AudioDataValue* aData,
            PRUint32 aChannels)
  : mOffset(aOffset),
    mTime(aTime),
    mDuration(aDuration),
    mFrames(aFrames),
    mChannels(aChannels),
    mAudioData(aData)
  {
    MOZ_COUNT_CTOR(AudioData);
  }

  ~AudioData()
  {
    MOZ_COUNT_DTOR(AudioData);
  }

  // If mAudioBuffer is null, creates it from mAudioData.
  void EnsureAudioBuffer();

  PRInt64 GetEnd() { return mTime + mDuration; }

  // Approximate byte offset of the end of the page on which this chunk
  // ends.
  const PRInt64 mOffset;

  PRInt64 mTime; // Start time of data in usecs.
  const PRInt64 mDuration; // In usecs.
  const PRUint32 mFrames;
  const PRUint32 mChannels;
  // At least one of mAudioBuffer/mAudioData must be non-null.
  // mChannels channels, each with mFrames frames
  nsRefPtr<SharedBuffer> mAudioBuffer;
  // mFrames frames, each with mChannels values
  nsAutoArrayPtr<AudioDataValue> mAudioData;
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
      PRUint32 mOffset;
      PRUint32 mSkip;
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
                           bool aKeyframe,
                           PRInt64 aTimecode,
                           nsIntRect aPicture);

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

  PRInt64 GetEnd() { return mEndTime; }

  // Dimensions at which to display the video frame. The picture region
  // will be scaled to this size. This is should be the picture region's
  // dimensions scaled with respect to its aspect ratio.
  nsIntSize mDisplay;

  // Approximate byte offset of the end of the frame in the media.
  PRInt64 mOffset;

  // Start time of frame in microseconds.
  PRInt64 mTime;

  // End time of frame in microseconds.
  PRInt64 mEndTime;

  // Codec specific internal time code. For Ogg based codecs this is the
  // granulepos.
  PRInt64 mTimecode;

  // This frame's image.
  nsRefPtr<Image> mImage;

  // When true, denotes that this frame is identical to the frame that
  // came before; it's a duplicate. mBuffer will be empty.
  bool mDuplicate;
  bool mKeyframe;

public:
  VideoData(PRInt64 aOffset, PRInt64 aTime, PRInt64 aEndTime, PRInt64 aTimecode)
    : mOffset(aOffset),
      mTime(aTime),
      mEndTime(aEndTime),
      mTimecode(aTimecode),
      mDuplicate(true),
      mKeyframe(false)
  {
    MOZ_COUNT_CTOR(VideoData);
    NS_ASSERTION(aEndTime >= aTime, "Frame must start before it ends.");
  }

  VideoData(PRInt64 aOffset,
            PRInt64 aTime,
            PRInt64 aEndTime,
            bool aKeyframe,
            PRInt64 aTimecode,
            nsIntSize aDisplay)
    : mDisplay(aDisplay),
      mOffset(aOffset),
      mTime(aTime),
      mEndTime(aEndTime),
      mTimecode(aTimecode),
      mDuplicate(false),
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
   typedef mozilla::ReentrantMonitorAutoEnter ReentrantMonitorAutoEnter;
   typedef mozilla::ReentrantMonitor ReentrantMonitor;

   MediaQueue()
     : nsDeque(new MediaQueueDeallocator<T>()),
       mReentrantMonitor("mediaqueue"),
       mEndOfStream(0)
   {}
  
  ~MediaQueue() {
    Reset();
  }

  inline PRInt32 GetSize() { 
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    return nsDeque::GetSize();
  }

  inline void Push(T* aItem) {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    nsDeque::Push(aItem);
  }

  inline void PushFront(T* aItem) {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    nsDeque::PushFront(aItem);
  }

  inline T* Pop() {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    return static_cast<T*>(nsDeque::Pop());
  }

  inline T* PopFront() {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    return static_cast<T*>(nsDeque::PopFront());
  }
  
  inline T* Peek() {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    return static_cast<T*>(nsDeque::Peek());
  }
  
  inline T* PeekFront() {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    return static_cast<T*>(nsDeque::PeekFront());
  }

  inline void Empty() {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    nsDeque::Empty();
  }

  inline void Erase() {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    nsDeque::Erase();
  }

  void Reset() {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    while (GetSize() > 0) {
      T* x = PopFront();
      delete x;
    }
    mEndOfStream = false;
  }

  bool AtEndOfStream() {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    return GetSize() == 0 && mEndOfStream;
  }

  // Returns true if the media queue has had it last item added to it.
  // This happens when the media stream has been completely decoded. Note this
  // does not mean that the corresponding stream has finished playback.
  bool IsFinished() {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    return mEndOfStream;
  }

  // Informs the media queue that it won't be receiving any more items.
  void Finish() {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    mEndOfStream = true;
  }

  // Returns the approximate number of microseconds of items in the queue.
  PRInt64 Duration() {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    if (GetSize() < 2) {
      return 0;
    }
    T* last = Peek();
    T* first = PeekFront();
    return last->mTime - first->mTime;
  }

  void LockedForEach(nsDequeFunctor& aFunctor) const {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    ForEach(aFunctor);
  }

  // Extracts elements from the queue into aResult, in order.
  // Elements whose start time is before aTime are ignored.
  void GetElementsAfter(PRInt64 aTime, nsTArray<T*>* aResult) {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    if (!GetSize())
      return;
    PRInt32 i;
    for (i = GetSize() - 1; i > 0; --i) {
      T* v = static_cast<T*>(ObjectAt(i));
      if (v->GetEnd() < aTime)
        break;
    }
    // Elements less than i have a end time before aTime. It's also possible
    // that the element at i has a end time before aTime, but that's OK.
    for (; i < GetSize(); ++i) {
      aResult->AppendElement(static_cast<T*>(ObjectAt(i)));
    }
  }

private:
  mutable ReentrantMonitor mReentrantMonitor;

  // True when we've decoded the last frame of data in the
  // bitstream for which we're queueing frame data.
  bool mEndOfStream;
};

// Encapsulates the decoding and reading of media data. Reading can only be
// done on the decode thread. Never hold the decoder monitor when
// calling into this class. Unless otherwise specified, methods and fields of
// this class can only be accessed on the decode thread.
class nsBuiltinDecoderReader : public nsRunnable {
public:
  typedef mozilla::ReentrantMonitor ReentrantMonitor;
  typedef mozilla::ReentrantMonitorAutoEnter ReentrantMonitorAutoEnter;
  typedef mozilla::VideoFrameContainer VideoFrameContainer;

  nsBuiltinDecoderReader(nsBuiltinDecoder* aDecoder);
  virtual ~nsBuiltinDecoderReader();

  // Initializes the reader, returns NS_OK on success, or NS_ERROR_FAILURE
  // on failure.
  virtual nsresult Init(nsBuiltinDecoderReader* aCloneDonor) = 0;

  // Resets all state related to decoding, emptying all buffers etc.
  virtual nsresult ResetDecode();

  // Decodes an unspecified amount of audio data, enqueuing the audio data
  // in mAudioQueue. Returns true when there's more audio to decode,
  // false if the audio is finished, end of file has been reached,
  // or an un-recoverable read error has occured.
  virtual bool DecodeAudioData() = 0;

  // Reads and decodes one video frame. Packets with a timestamp less
  // than aTimeThreshold will be decoded (unless they're not keyframes
  // and aKeyframeSkip is true), but will not be added to the queue.
  virtual bool DecodeVideoFrame(bool &aKeyframeSkip,
                                PRInt64 aTimeThreshold) = 0;

  virtual bool HasAudio() = 0;
  virtual bool HasVideo() = 0;

  // Read header data for all bitstreams in the file. Fills mInfo with
  // the data required to present the media. Returns NS_OK on success,
  // or NS_ERROR_FAILURE on failure.
  virtual nsresult ReadMetadata(nsVideoInfo* aInfo) = 0;

  // Stores the presentation time of the first frame we'd be able to play if
  // we started playback at the current position. Returns the first video
  // frame, if we have video.
  VideoData* FindStartTime(PRInt64& aOutStartTime);

  // Moves the decode head to aTime microseconds. aStartTime and aEndTime
  // denote the start and end times of the media in usecs, and aCurrentTime
  // is the current playback position in microseconds.
  virtual nsresult Seek(PRInt64 aTime,
                        PRInt64 aStartTime,
                        PRInt64 aEndTime,
                        PRInt64 aCurrentTime) = 0;

  // Queue of audio frames. This queue is threadsafe, and is accessed from
  // the audio, decoder, state machine, and main threads.
  MediaQueue<AudioData> mAudioQueue;

  // Queue of video frames. This queue is threadsafe, and is accessed from
  // the decoder, state machine, and main threads.
  MediaQueue<VideoData> mVideoQueue;

  // Populates aBuffered with the time ranges which are buffered. aStartTime
  // must be the presentation time of the first frame in the media, e.g.
  // the media time corresponding to playback time/position 0. This function
  // should only be called on the main thread.
  virtual nsresult GetBuffered(nsTimeRanges* aBuffered,
                               PRInt64 aStartTime) = 0;

  // True if we can seek using only buffered ranges. This is backend dependant.
  virtual bool IsSeekableInBufferedRanges() = 0;

  class VideoQueueMemoryFunctor : public nsDequeFunctor {
  public:
    VideoQueueMemoryFunctor() : mResult(0) {}

    virtual void* operator()(void* anObject) {
      const VideoData* v = static_cast<const VideoData*>(anObject);
      if (!v->mImage) {
        return nsnull;
      }
      NS_ASSERTION(v->mImage->GetFormat() == mozilla::layers::Image::PLANAR_YCBCR,
                   "Wrong format?");
      mozilla::layers::PlanarYCbCrImage* vi = static_cast<mozilla::layers::PlanarYCbCrImage*>(v->mImage.get());

      mResult += vi->GetDataSize();
      return nsnull;
    }

    PRInt64 mResult;
  };

  PRInt64 VideoQueueMemoryInUse() {
    VideoQueueMemoryFunctor functor;
    mVideoQueue.LockedForEach(functor);
    return functor.mResult;
  }

  class AudioQueueMemoryFunctor : public nsDequeFunctor {
  public:
    AudioQueueMemoryFunctor() : mResult(0) {}

    virtual void* operator()(void* anObject) {
      const AudioData* audioData = static_cast<const AudioData*>(anObject);
      mResult += audioData->mFrames * audioData->mChannels * sizeof(AudioDataValue);
      return nsnull;
    }

    PRInt64 mResult;
  };

  PRInt64 AudioQueueMemoryInUse() {
    AudioQueueMemoryFunctor functor;
    mAudioQueue.LockedForEach(functor);
    return functor.mResult;
  }

  // Only used by nsWebMReader for now, so stub here rather than in every
  // reader than inherits from nsBuiltinDecoderReader.
  virtual void NotifyDataArrived(const char* aBuffer, PRUint32 aLength, PRInt64 aOffset) {}

protected:

  // Pumps the decode until we reach frames required to play at time aTarget
  // (usecs).
  nsresult DecodeToTarget(PRInt64 aTarget);

  // Reader decode function. Matches DecodeVideoFrame() and
  // DecodeAudioData().
  typedef bool (nsBuiltinDecoderReader::*DecodeFn)();

  // Calls aDecodeFn on *this until aQueue has an item, whereupon
  // we return the first item.
  template<class Data>
  Data* DecodeToFirstData(DecodeFn aDecodeFn,
                          MediaQueue<Data>& aQueue);

  // Wrapper so that DecodeVideoFrame(bool&,PRInt64) can be called from
  // DecodeToFirstData().
  bool DecodeVideoFrame() {
    bool f = false;
    return DecodeVideoFrame(f, 0);
  }

  // Reference to the owning decoder object.
  nsBuiltinDecoder* mDecoder;

  // Stores presentation info required for playback.
  nsVideoInfo mInfo;
};

#endif
