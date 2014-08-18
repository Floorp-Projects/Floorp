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

class RequestSampleCallback;
class MediaDecoderReader;

// Encapsulates the decoding and reading of media data. Reading can either
// synchronous and done on the calling "decode" thread, or asynchronous and
// performed on a background thread, with the result being returned by
// callback. Never hold the decoder monitor when calling into this class.
// Unless otherwise specified, methods and fields of this class can only
// be accessed on the decode task queue.
class MediaDecoderReader {
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaDecoderReader)

  MediaDecoderReader(AbstractMediaDecoder* aDecoder);

  // Initializes the reader, returns NS_OK on success, or NS_ERROR_FAILURE
  // on failure.
  virtual nsresult Init(MediaDecoderReader* aCloneDonor) = 0;

  // True if this reader is waiting media resource allocation
  virtual bool IsWaitingMediaResources() { return false; }
  // True when this reader need to become dormant state
  virtual bool IsDormantNeeded() { return false; }
  // Release media resources they should be released in dormant state
  // The reader can be made usable again by calling ReadMetadata().
  virtual void ReleaseMediaResources() {};
  // Breaks reference-counted cycles. Called during shutdown.
  // WARNING: If you override this, you must call the base implementation
  // in your override.
  virtual void BreakCycles();

  // Destroys the decoding state. The reader cannot be made usable again.
  // This is different from ReleaseMediaResources() as it is irreversable,
  // whereas ReleaseMediaResources() is.
  virtual void Shutdown();

  virtual void SetCallback(RequestSampleCallback* aDecodedSampleCallback);
  virtual void SetTaskQueue(MediaTaskQueue* aTaskQueue);

  // Resets all state related to decoding, emptying all buffers etc.
  // Cancels all pending Request*Data() request callbacks, and flushes the
  // decode pipeline. The decoder must not call any of the callbacks for
  // outstanding Request*Data() calls after this is called. Calls to
  // Request*Data() made after this should be processed as usual.
  // Normally this call preceedes a Seek() call, or shutdown.
  // The first samples of every stream produced after a ResetDecode() call
  // *must* be marked as "discontinuities". If it's not, seeking work won't
  // properly!
  virtual nsresult ResetDecode();

  // Requests the Reader to call OnAudioDecoded() on aCallback with one
  // audio sample. The decode should be performed asynchronously, and
  // the callback can be performed on any thread. Don't hold the decoder
  // monitor while calling this, as the implementation may try to wait
  // on something that needs the monitor and deadlock.
  virtual void RequestAudioData();

  // Requests the Reader to call OnVideoDecoded() on aCallback with one
  // video sample. The decode should be performed asynchronously, and
  // the callback can be performed on any thread. Don't hold the decoder
  // monitor while calling this, as the implementation may try to wait
  // on something that needs the monitor and deadlock.
  // If aSkipToKeyframe is true, the decode should skip ahead to the
  // the next keyframe at or after aTimeThreshold microseconds.
  virtual void RequestVideoData(bool aSkipToNextKeyframe,
                                int64_t aTimeThreshold);

  virtual bool HasAudio() = 0;
  virtual bool HasVideo() = 0;

  // Read header data for all bitstreams in the file. Fills aInfo with
  // the data required to present the media, and optionally fills *aTags
  // with tag metadata from the file.
  // Returns NS_OK on success, or NS_ERROR_FAILURE on failure.
  virtual nsresult ReadMetadata(MediaInfo* aInfo,
                                MetadataTags** aTags) = 0;

  // TODO: DEPRECATED. This uses synchronous decoding.
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

  // Called to move the reader into idle state. When the reader is
  // created it is assumed to be active (i.e. not idle). When the media
  // element is paused and we don't need to decode any more data, the state
  // machine calls SetIdle() to inform the reader that its decoder won't be
  // needed for a while. The reader can use these notifications to enter
  // a low power state when the decoder isn't needed, if desired.
  // This is most useful on mobile.
  // Note: DecodeVideoFrame, DecodeAudioData, ReadMetadata and Seek should
  // activate the decoder if necessary. The state machine only needs to know
  // when to call SetIdle().
  virtual void SetIdle() { }

  // Tell the reader that the data decoded are not for direct playback, so it
  // can accept more files, in particular those which have more channels than
  // available in the audio output.
  void SetIgnoreAudioOutputFormat()
  {
    mIgnoreAudioOutputFormat = true;
  }

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

  // Returns the number of bytes of memory allocated by structures/frames in
  // the video queue.
  size_t SizeOfVideoQueueInBytes() const;

  // Returns the number of bytes of memory allocated by structures/frames in
  // the audio queue.
  size_t SizeOfAudioQueueInBytes() const;

  // Only used by WebMReader and MediaOmxReader for now, so stub here rather
  // than in every reader than inherits from MediaDecoderReader.
  virtual void NotifyDataArrived(const char* aBuffer, uint32_t aLength, int64_t aOffset) {}
  virtual int64_t GetEvictionOffset(double aTime) { return -1; }

  virtual MediaQueue<AudioData>& AudioQueue() { return mAudioQueue; }
  virtual MediaQueue<VideoData>& VideoQueue() { return mVideoQueue; }

  // Returns a pointer to the decoder.
  AbstractMediaDecoder* GetDecoder() {
    return mDecoder;
  }

  AudioData* DecodeToFirstAudioData();
  VideoData* DecodeToFirstVideoData();

  MediaInfo GetMediaInfo() { return mInfo; }

  // Indicates if the media is seekable.
  // ReadMetada should be called before calling this method.
  virtual bool IsMediaSeekable() = 0;

protected:
  virtual ~MediaDecoderReader();

  // Overrides of this function should decodes an unspecified amount of
  // audio data, enqueuing the audio data in mAudioQueue. Returns true
  // when there's more audio to decode, false if the audio is finished,
  // end of file has been reached, or an un-recoverable read error has
  // occured. This function blocks until the decode is complete.
  virtual bool DecodeAudioData() {
    return false;
  }

  // Overrides of this function should read and decodes one video frame.
  // Packets with a timestamp less than aTimeThreshold will be decoded
  // (unless they're not keyframes and aKeyframeSkip is true), but will
  // not be added to the queue. This function blocks until the decode
  // is complete.
  virtual bool DecodeVideoFrame(bool &aKeyframeSkip, int64_t aTimeThreshold) {
    return false;
  }

  RequestSampleCallback* GetCallback() {
    MOZ_ASSERT(mSampleDecodedCallback);
    return mSampleDecodedCallback;
  }

  virtual MediaTaskQueue* GetTaskQueue() {
    return mTaskQueue;
  }

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

  // Reference to the owning decoder object.
  AbstractMediaDecoder* mDecoder;

  // Stores presentation info required for playback.
  MediaInfo mInfo;

  // Whether we should accept media that we know we can't play
  // directly, because they have a number of channel higher than
  // what we support.
  bool mIgnoreAudioOutputFormat;

private:

  nsRefPtr<RequestSampleCallback> mSampleDecodedCallback;

  nsRefPtr<MediaTaskQueue> mTaskQueue;

  // Flags whether a the next audio/video sample comes after a "gap" or
  // "discontinuity" in the stream. For example after a seek.
  bool mAudioDiscontinuity;
  bool mVideoDiscontinuity;
};

// Interface that callers to MediaDecoderReader::Request{Audio,Video}Data()
// must implement to receive the requested samples asynchronously.
// This object is refcounted, and cycles must be broken by calling
// BreakCycles() during shutdown.
class RequestSampleCallback {
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RequestSampleCallback)

  // Receives the result of a RequestAudioData() call.
  virtual void OnAudioDecoded(AudioData* aSample) = 0;

  // Called when a RequestAudioData() call can't be fulfiled as we've
  // reached the end of stream.
  virtual void OnAudioEOS() = 0;

  // Receives the result of a RequestVideoData() call.
  virtual void OnVideoDecoded(VideoData* aSample) = 0;

  // Called when a RequestVideoData() call can't be fulfiled as we've
  // reached the end of stream.
  virtual void OnVideoEOS() = 0;

  // Called when there's a decode error. No more sample requests
  // will succeed.
  virtual void OnDecodeError() = 0;

  // Called during shutdown to break any reference cycles.
  virtual void BreakCycles() = 0;

protected:
  virtual ~RequestSampleCallback() {}
};

// A RequestSampleCallback implementation that can be passed to the
// MediaDecoderReader to block the thread requesting an audio sample until
// the audio decode is complete. This is used to adapt the asynchronous
// model of the MediaDecoderReader to a synchronous model.
class AudioDecodeRendezvous : public RequestSampleCallback {
public:
  AudioDecodeRendezvous();
  ~AudioDecodeRendezvous();

  // RequestSampleCallback implementation. Called when decode is complete.
  // Note: aSample is null at end of stream.
  virtual void OnAudioDecoded(AudioData* aSample) MOZ_OVERRIDE;
  virtual void OnAudioEOS() MOZ_OVERRIDE;
  virtual void OnVideoDecoded(VideoData* aSample) MOZ_OVERRIDE {}
  virtual void OnVideoEOS() MOZ_OVERRIDE {}
  virtual void OnDecodeError() MOZ_OVERRIDE;
  virtual void BreakCycles() MOZ_OVERRIDE {};
  void Reset();

  // Returns failure on error, or NS_OK.
  // If *aSample is null, EOS has been reached.
  nsresult Await(nsAutoPtr<AudioData>& aSample);

  // Interrupts a call to Wait().
  void Cancel();

private:
  Monitor mMonitor;
  nsresult mStatus;
  nsAutoPtr<AudioData> mSample;
  bool mHaveResult;
};

} // namespace mozilla

#endif
