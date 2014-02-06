/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(MediaDecoderReader_h_)
#define MediaDecoderReader_h_

#include "AbstractMediaDecoder.h"
#include "MediaInfo.h"
#include "MediaData.h"
#include "MediaQueue.h"
#include "AudioCompactor.h"

namespace mozilla {

namespace dom {
class TimeRanges;
}

// Encapsulates the decoding and reading of media data. Reading can only be
// done on the decode thread. Never hold the decoder monitor when
// calling into this class. Unless otherwise specified, methods and fields of
// this class can only be accessed on the decode thread.
class MediaDecoderReader {
public:
  MediaDecoderReader(AbstractMediaDecoder* aDecoder);
  virtual ~MediaDecoderReader();

  // Initializes the reader, returns NS_OK on success, or NS_ERROR_FAILURE
  // on failure.
  virtual nsresult Init(MediaDecoderReader* aCloneDonor) = 0;

  // True if this reader is waiting media resource allocation
  virtual bool IsWaitingMediaResources() { return false; }
  // True when this reader need to become dormant state
  virtual bool IsDormantNeeded() { return false; }
  // Release media resources they should be released in dormant state
  virtual void ReleaseMediaResources() {};
  // Release the decoder during shutdown
  virtual void ReleaseDecoder() {};

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
                                int64_t aTimeThreshold) = 0;

  virtual bool HasAudio() = 0;
  virtual bool HasVideo() = 0;

  // Read header data for all bitstreams in the file. Fills aInfo with
  // the data required to present the media, and optionally fills *aTags
  // with tag metadata from the file.
  // Returns NS_OK on success, or NS_ERROR_FAILURE on failure.
  virtual nsresult ReadMetadata(MediaInfo* aInfo,
                                MetadataTags** aTags) = 0;

  // Stores the presentation time of the first frame we'd be able to play if
  // we started playback at the current position. Returns the first video
  // frame, if we have video.
  virtual VideoData* FindStartTime(int64_t& aOutStartTime);

  // Moves the decode head to aTime microseconds. aStartTime and aEndTime
  // denote the start and end times of the media in usecs, and aCurrentTime
  // is the current playback position in microseconds.
  virtual nsresult Seek(int64_t aTime,
                        int64_t aStartTime,
                        int64_t aEndTime,
                        int64_t aCurrentTime) = 0;

  // Called when the decode thread is started, before calling any other
  // decode, read metadata, or seek functions. Do any thread local setup
  // in this function.
  virtual void OnDecodeThreadStart() {}

  // Called when the decode thread is about to finish, after all calls to
  // any other decode, read metadata, or seek functions. Any backend specific
  // thread local tear down must be done in this function. Note that another
  // decode thread could start up and run in future.
  virtual void OnDecodeThreadFinish() {}

  // Tell the reader that the data decoded are not for direct playback, so it
  // can accept more files, in particular those which have more channels than
  // available in the audio output.
  void SetIgnoreAudioOutputFormat()
  {
    mIgnoreAudioOutputFormat = true;
  }

protected:
  // Queue of audio frames. This queue is threadsafe, and is accessed from
  // the audio, decoder, state machine, and main threads.
  MediaQueue<AudioData> mAudioQueue;

  // Queue of video frames. This queue is threadsafe, and is accessed from
  // the decoder, state machine, and main threads.
  MediaQueue<VideoData> mVideoQueue;

  // An adapter to the audio queue which first copies data to buffers with
  // minimal allocation slop and then pushes them to the queue.  This is
  // useful for decoders working with formats that give awkward numbers of
  // frames such as mp3.
  AudioCompactor mAudioCompactor;

public:
  // Populates aBuffered with the time ranges which are buffered. aStartTime
  // must be the presentation time of the first frame in the media, e.g.
  // the media time corresponding to playback time/position 0. This function
  // is called on the main, decode, and state machine threads.
  //
  // This base implementation in MediaDecoderReader estimates the time ranges
  // buffered by interpolating the cached byte ranges with the duration
  // of the media. Reader subclasses should override this method if they
  // can quickly calculate the buffered ranges more accurately.
  //
  // The primary advantage of this implementation in the reader base class
  // is that it's a fast approximation, which does not perform any I/O.
  //
  // The OggReader relies on this base implementation not performing I/O,
  // since in FirefoxOS we can't do I/O on the main thread, where this is
  // called.
  virtual nsresult GetBuffered(dom::TimeRanges* aBuffered,
                               int64_t aStartTime);

  class VideoQueueMemoryFunctor : public nsDequeFunctor {
  public:
    VideoQueueMemoryFunctor() : mResult(0) {}

    virtual void* operator()(void* anObject);

    int64_t mResult;
  };

  virtual int64_t VideoQueueMemoryInUse() {
    VideoQueueMemoryFunctor functor;
    mVideoQueue.LockedForEach(functor);
    return functor.mResult;
  }

  class AudioQueueMemoryFunctor : public nsDequeFunctor {
  public:
    AudioQueueMemoryFunctor() : mSize(0) {}

    MOZ_DEFINE_MALLOC_SIZE_OF(MallocSizeOf);

    virtual void* operator()(void* anObject) {
      const AudioData* audioData = static_cast<const AudioData*>(anObject);
      mSize += audioData->SizeOfIncludingThis(MallocSizeOf);
      return nullptr;
    }

    size_t mSize;
  };

  size_t SizeOfAudioQueue() {
    AudioQueueMemoryFunctor functor;
    mAudioQueue.LockedForEach(functor);
    return functor.mSize;
  }

  // Only used by WebMReader and MediaOmxReader for now, so stub here rather
  // than in every reader than inherits from MediaDecoderReader.
  virtual void NotifyDataArrived(const char* aBuffer, uint32_t aLength, int64_t aOffset) {}

  virtual MediaQueue<AudioData>& AudioQueue() { return mAudioQueue; }
  virtual MediaQueue<VideoData>& VideoQueue() { return mVideoQueue; }

  // Returns a pointer to the decoder.
  AbstractMediaDecoder* GetDecoder() {
    return mDecoder;
  }

  AudioData* DecodeToFirstAudioData();
  VideoData* DecodeToFirstVideoData();

protected:
  // Pumps the decode until we reach frames required to play at time aTarget
  // (usecs).
  nsresult DecodeToTarget(int64_t aTarget);

  // Reference to the owning decoder object.
  AbstractMediaDecoder* mDecoder;

  // Stores presentation info required for playback.
  MediaInfo mInfo;

  // Whether we should accept media that we know we can't play
  // directly, because they have a number of channel higher than
  // what we support.
  bool mIgnoreAudioOutputFormat;
};

} // namespace mozilla

#endif
