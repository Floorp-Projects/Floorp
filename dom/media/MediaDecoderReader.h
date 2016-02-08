/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(MediaDecoderReader_h_)
#define MediaDecoderReader_h_

#include "mozilla/EnumSet.h"
#include "mozilla/MozPromise.h"

#include "AbstractMediaDecoder.h"
#include "MediaInfo.h"
#include "MediaData.h"
#include "MediaMetadataManager.h"
#include "MediaQueue.h"
#include "MediaTimer.h"
#include "AudioCompactor.h"
#include "Intervals.h"
#include "TimeUnits.h"
#include "SeekTarget.h"

namespace mozilla {

class CDMProxy;
class MediaDecoderReader;

struct WaitForDataRejectValue
{
  enum Reason {
    SHUTDOWN,
    CANCELED
  };

  WaitForDataRejectValue(MediaData::Type aType, Reason aReason)
    :mType(aType), mReason(aReason) {}
  MediaData::Type mType;
  Reason mReason;
};

class MetadataHolder
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MetadataHolder)
  MediaInfo mInfo;
  nsAutoPtr<MetadataTags> mTags;

private:
  virtual ~MetadataHolder() {}
};

enum class ReadMetadataFailureReason : int8_t
{
  METADATA_ERROR
};

// Encapsulates the decoding and reading of media data. Reading can either
// synchronous and done on the calling "decode" thread, or asynchronous and
// performed on a background thread, with the result being returned by
// callback. Never hold the decoder monitor when calling into this class.
// Unless otherwise specified, methods and fields of this class can only
// be accessed on the decode task queue.
class MediaDecoderReader {
  friend class ReRequestVideoWithSkipTask;
  friend class ReRequestAudioTask;

  static const bool IsExclusive = true;

public:
  enum NotDecodedReason {
    END_OF_STREAM,
    DECODE_ERROR,
    WAITING_FOR_DATA,
    CANCELED
  };

  using TrackSet = EnumSet<TrackInfo::TrackType>;

  using MetadataPromise =
    MozPromise<RefPtr<MetadataHolder>, ReadMetadataFailureReason, IsExclusive>;
  using MediaDataPromise =
    MozPromise<RefPtr<MediaData>, NotDecodedReason, IsExclusive>;
  using SeekPromise = MozPromise<media::TimeUnit, nsresult, IsExclusive>;

  // Note that, conceptually, WaitForData makes sense in a non-exclusive sense.
  // But in the current architecture it's only ever used exclusively (by MDSM),
  // so we mark it that way to verify our assumptions. If you have a use-case
  // for multiple WaitForData consumers, feel free to flip the exclusivity here.
  using WaitForDataPromise =
    MozPromise<MediaData::Type, WaitForDataRejectValue, IsExclusive>;

  using BufferedUpdatePromise = MozPromise<bool, bool, IsExclusive>;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaDecoderReader)

  // The caller must ensure that Shutdown() is called before aDecoder is
  // destroyed.
  explicit MediaDecoderReader(AbstractMediaDecoder* aDecoder);

  // Initializes the reader, returns NS_OK on success, or NS_ERROR_FAILURE
  // on failure.
  virtual nsresult Init() { return NS_OK; }

  // Release media resources they should be released in dormant state
  // The reader can be made usable again by calling ReadMetadata().
  virtual void ReleaseMediaResources() {}

  // Destroys the decoding state. The reader cannot be made usable again.
  // This is different from ReleaseMediaResources() as it is irreversable,
  // whereas ReleaseMediaResources() is.  Must be called on the decode
  // thread.
  virtual RefPtr<ShutdownPromise> Shutdown();

  virtual bool OnTaskQueue() const
  {
    return OwnerThread()->IsCurrentThreadIn();
  }

  // Resets all state related to decoding, emptying all buffers etc.
  // Cancels all pending Request*Data() request callbacks, rejects any
  // outstanding seek promises, and flushes the decode pipeline. The
  // decoder must not call any of the callbacks for outstanding
  // Request*Data() calls after this is called. Calls to Request*Data()
  // made after this should be processed as usual.
  //
  // Normally this call preceedes a Seek() call, or shutdown.
  //
  // The first samples of every stream produced after a ResetDecode() call
  // *must* be marked as "discontinuities". If it's not, seeking work won't
  // properly!
  //
  // aParam is a set of TrackInfo::TrackType enums specifying which
  // queues need to be reset, defaulting to both audio and video tracks.
  virtual nsresult ResetDecode(TrackSet aTracks = TrackSet(TrackInfo::kAudioTrack,
                                                           TrackInfo::kVideoTrack));

  // Requests one audio sample from the reader.
  //
  // The decode should be performed asynchronously, and the promise should
  // be resolved when it is complete. Don't hold the decoder
  // monitor while calling this, as the implementation may try to wait
  // on something that needs the monitor and deadlock.
  virtual RefPtr<MediaDataPromise> RequestAudioData();

  // Requests one video sample from the reader.
  //
  // Don't hold the decoder monitor while calling this, as the implementation
  // may try to wait on something that needs the monitor and deadlock.
  // If aSkipToKeyframe is true, the decode should skip ahead to the
  // the next keyframe at or after aTimeThreshold microseconds.
  virtual RefPtr<MediaDataPromise>
  RequestVideoData(bool aSkipToNextKeyframe, int64_t aTimeThreshold);

  // By default, the state machine polls the reader once per second when it's
  // in buffering mode. Some readers support a promise-based mechanism by which
  // they notify the state machine when the data arrives.
  virtual bool IsWaitForDataSupported() const { return false; }

  virtual RefPtr<WaitForDataPromise> WaitForData(MediaData::Type aType)
  {
    MOZ_CRASH();
  }

  // By default, the reader return the decoded data. Some readers support
  // retuning demuxed data.
  virtual bool IsDemuxOnlySupported() const { return false; }

  // Configure the reader to return demuxed or decoded data
  // upon calls to Request{Audio,Video}Data.
  virtual void SetDemuxOnly(bool /*aDemuxedOnly*/) {}

  // The default implementation of AsyncReadMetadata is implemented in terms of
  // synchronous ReadMetadata() calls. Implementations may also
  // override AsyncReadMetadata to create a more proper async implementation.
  virtual RefPtr<MetadataPromise> AsyncReadMetadata();

  // Fills aInfo with the latest cached data required to present the media,
  // ReadUpdatedMetadata will always be called once ReadMetadata has succeeded.
  virtual void ReadUpdatedMetadata(MediaInfo* aInfo) {}

  // Moves the decode head to aTime microseconds. aEndTime denotes the end
  // time of the media in usecs. This is only needed for OggReader, and should
  // probably be removed somehow.
  virtual RefPtr<SeekPromise> Seek(SeekTarget aTarget, int64_t aEndTime) = 0;

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
  virtual void SetIdle() {}

#ifdef MOZ_EME
  virtual void SetCDMProxy(CDMProxy* aProxy) {}
#endif

  // Tell the reader that the data decoded are not for direct playback, so it
  // can accept more files, in particular those which have more channels than
  // available in the audio output.
  void SetIgnoreAudioOutputFormat()
  {
    mIgnoreAudioOutputFormat = true;
  }

  // MediaSourceReader opts out of the start-time-guessing mechanism.
  virtual bool ForceZeroStartTime() const { return false; }

  // The MediaDecoderStateMachine uses various heuristics that assume that
  // raw media data is arriving sequentially from a network channel. This
  // makes sense in the <video src="foo"> case, but not for more advanced use
  // cases like MSE.
  virtual bool UseBufferingHeuristics() const { return true; }

  // Returns the number of bytes of memory allocated by structures/frames in
  // the video queue.
  size_t SizeOfVideoQueueInBytes() const;

  // Returns the number of bytes of memory allocated by structures/frames in
  // the audio queue.
  size_t SizeOfAudioQueueInBytes() const;

  virtual size_t SizeOfVideoQueueInFrames();
  virtual size_t SizeOfAudioQueueInFrames();

  void NotifyDataArrived()
  {
    MOZ_ASSERT(OnTaskQueue());
    NS_ENSURE_TRUE_VOID(!mShutdown);
    NotifyDataArrivedInternal();
    UpdateBuffered();
  }

  // Update the buffered ranges and upon doing so return a promise
  // to indicate success. Overrides may need to do extra work to ensure
  // buffered is up to date.
  virtual RefPtr<BufferedUpdatePromise> UpdateBufferedWithPromise()
  {
    MOZ_ASSERT(OnTaskQueue());
    UpdateBuffered();
    return BufferedUpdatePromise::CreateAndResolve(true, __func__);
  }

  virtual MediaQueue<AudioData>& AudioQueue() { return mAudioQueue; }
  virtual MediaQueue<VideoData>& VideoQueue() { return mVideoQueue; }

  AbstractCanonical<media::TimeIntervals>* CanonicalBuffered()
  {
    return &mBuffered;
  }

  void DispatchSetStartTime(int64_t aStartTime)
  {
    RefPtr<MediaDecoderReader> self = this;
    nsCOMPtr<nsIRunnable> r =
      NS_NewRunnableFunction([self, aStartTime] () -> void
    {
      MOZ_ASSERT(self->OnTaskQueue());
      MOZ_ASSERT(!self->HaveStartTime());
      self->mStartTime.emplace(aStartTime);
      self->UpdateBuffered();
    });
    OwnerThread()->Dispatch(r.forget());
  }

  TaskQueue* OwnerThread() const
  {
    return mTaskQueue;
  }

  // Returns true if the reader implements RequestAudioData()
  // and RequestVideoData() asynchronously, rather than using the
  // implementation in this class to adapt the old synchronous to
  // the newer async model.
  virtual bool IsAsync() const { return false; }

  // Returns true if this decoder reader uses hardware accelerated video
  // decoding.
  virtual bool VideoIsHardwareAccelerated() const { return false; }

  TimedMetadataEventSource& TimedMetadataEvent()
  {
    return mTimedMetadataEvent;
  }

  // Notified by the OggReader during playback when chained ogg is detected.
  MediaEventSource<void>& OnMediaNotSeekable() { return mOnMediaNotSeekable; }

  bool IsSuspended() const
  {
    MOZ_ASSERT(OnTaskQueue());
    return mIsSuspended;
  }

  void SetIsSuspended(bool aState)
  {
    MOZ_ASSERT(OnTaskQueue());
    mIsSuspended = aState;
  }

protected:
  virtual ~MediaDecoderReader();

  // Populates aBuffered with the time ranges which are buffered. This may only
  // be called on the decode task queue, and should only be used internally by
  // UpdateBuffered - mBuffered (or mirrors of it) should be used for everything
  // else.
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
  virtual media::TimeIntervals GetBuffered();

  RefPtr<MediaDataPromise> DecodeToFirstVideoData();

  bool HaveStartTime()
  {
    MOZ_ASSERT(OnTaskQueue());
    return mStartTime.isSome();
  }

  int64_t StartTime() { MOZ_ASSERT(HaveStartTime()); return mStartTime.ref(); }

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

  // Decode task queue.
  RefPtr<TaskQueue> mTaskQueue;

  // State-watching manager.
  WatchManager<MediaDecoderReader> mWatchManager;

  // Buffered range.
  Canonical<media::TimeIntervals> mBuffered;

  // Stores presentation info required for playback.
  MediaInfo mInfo;

  // Duration, mirrored from the state machine task queue.
  Mirror<media::NullableTimeUnit> mDuration;

  // Whether we should accept media that we know we can't play
  // directly, because they have a number of channel higher than
  // what we support.
  bool mIgnoreAudioOutputFormat;

  // The start time of the media, in microseconds. This is the presentation
  // time of the first frame decoded from the media. This is initialized to -1,
  // and then set to a value >= by MediaDecoderStateMachine::SetStartTime(),
  // after which point it never changes (though SetStartTime may be called
  // multiple times with the same value).
  //
  // This is an ugly breach of abstractions - it's currently necessary for the
  // readers to return the correct value of GetBuffered. We should refactor
  // things such that all GetBuffered calls go through the MDSM, which would
  // offset the range accordingly.
  Maybe<int64_t> mStartTime;

  // This is a quick-and-dirty way for DecodeAudioData implementations to
  // communicate the presence of a decoding error to RequestAudioData. We should
  // replace this with a promise-y mechanism as we make this stuff properly
  // async.
  bool mHitAudioDecodeError;
  bool mShutdown;

  // Used to send TimedMetadata to the listener.
  TimedMetadataEventProducer mTimedMetadataEvent;

  // Notify if this media is not seekable.
  MediaEventProducer<void> mOnMediaNotSeekable;

private:
  // Does any spinup that needs to happen on this task queue. This runs on a
  // different thread than Init, and there should not be ordering dependencies
  // between the two (even though in practice, Init will always run first right
  // now thanks to the tail dispatcher).
  void InitializationTask();

  // Read header data for all bitstreams in the file. Fills aInfo with
  // the data required to present the media, and optionally fills *aTags
  // with tag metadata from the file.
  // Returns NS_OK on success, or NS_ERROR_FAILURE on failure.
  virtual nsresult ReadMetadata(MediaInfo* aInfo, MetadataTags** aTags)
  {
    MOZ_CRASH();
  }

  // Recomputes mBuffered.
  virtual void UpdateBuffered();

  virtual void VisibilityChanged();

  virtual void NotifyDataArrivedInternal() {}

  // Overrides of this function should decodes an unspecified amount of
  // audio data, enqueuing the audio data in mAudioQueue. Returns true
  // when there's more audio to decode, false if the audio is finished,
  // end of file has been reached, or an un-recoverable read error has
  // occured. This function blocks until the decode is complete.
  virtual bool DecodeAudioData()
  {
    return false;
  }

  // Overrides of this function should read and decodes one video frame.
  // Packets with a timestamp less than aTimeThreshold will be decoded
  // (unless they're not keyframes and aKeyframeSkip is true), but will
  // not be added to the queue. This function blocks until the decode
  // is complete.
  virtual bool DecodeVideoFrame(bool &aKeyframeSkip, int64_t aTimeThreshold)
  {
    return false;
  }

  // Promises used only for the base-class (sync->async adapter) implementation
  // of Request{Audio,Video}Data.
  MozPromiseHolder<MediaDataPromise> mBaseAudioPromise;
  MozPromiseHolder<MediaDataPromise> mBaseVideoPromise;

  // Flags whether a the next audio/video sample comes after a "gap" or
  // "discontinuity" in the stream. For example after a seek.
  bool mAudioDiscontinuity;
  bool mVideoDiscontinuity;
  bool mIsSuspended;

  MediaEventListener mDataArrivedListener;
};

} // namespace mozilla

#endif
