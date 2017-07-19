/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(MediaDecoderReader_h_)
#define MediaDecoderReader_h_

#include "AbstractMediaDecoder.h"
#include "AudioCompactor.h"
#include "Intervals.h"
#include "MediaData.h"
#include "MediaInfo.h"
#include "MediaMetadataManager.h"
#include "MediaQueue.h"
#include "MediaResult.h"
#include "MediaTimer.h"
#include "SeekTarget.h"
#include "TimeUnits.h"
#include "mozilla/EnumSet.h"
#include "mozilla/MozPromise.h"
#include "nsAutoPtr.h"

namespace mozilla {

class CDMProxy;
class GMPCrashHelper;
class MediaDecoderReader;
class TaskQueue;
class VideoFrameContainer;

struct WaitForDataRejectValue
{
  enum Reason
  {
    SHUTDOWN,
    CANCELED
  };

  WaitForDataRejectValue(MediaData::Type aType, Reason aReason)
    :mType(aType), mReason(aReason)
  {
  }
  MediaData::Type mType;
  Reason mReason;
};

struct SeekRejectValue
{
  MOZ_IMPLICIT SeekRejectValue(const MediaResult& aError)
    : mType(MediaData::NULL_DATA), mError(aError) { }
  MOZ_IMPLICIT SeekRejectValue(nsresult aResult)
    : mType(MediaData::NULL_DATA), mError(aResult) { }
  SeekRejectValue(MediaData::Type aType, const MediaResult& aError)
    : mType(aType), mError(aError) { }
  MediaData::Type mType;
  MediaResult mError;
};

struct MetadataHolder
{
  UniquePtr<MediaInfo> mInfo;
  UniquePtr<MetadataTags> mTags;
};

struct MOZ_STACK_CLASS MediaDecoderReaderInit
{
  AbstractMediaDecoder* const mDecoder;
  MediaResource* mResource = nullptr;
  VideoFrameContainer* mVideoFrameContainer = nullptr;
  already_AddRefed<layers::KnowsCompositor> mKnowsCompositor;
  already_AddRefed<GMPCrashHelper> mCrashHelper;

  explicit MediaDecoderReaderInit(AbstractMediaDecoder* aDecoder)
    : mDecoder(aDecoder)
  {
  }
};

// Encapsulates the decoding and reading of media data. Reading can either
// synchronous and done on the calling "decode" thread, or asynchronous and
// performed on a background thread, with the result being returned by
// callback.
// Unless otherwise specified, methods and fields of this class can only
// be accessed on the decode task queue.
class MediaDecoderReader
{
  static const bool IsExclusive = true;

public:
  using TrackSet = EnumSet<TrackInfo::TrackType>;

  using MetadataPromise = MozPromise<MetadataHolder, MediaResult, IsExclusive>;

  template <typename Type>
  using DataPromise = MozPromise<RefPtr<Type>, MediaResult, IsExclusive>;
  using AudioDataPromise = DataPromise<AudioData>;
  using VideoDataPromise = DataPromise<VideoData>;

  using SeekPromise = MozPromise<media::TimeUnit, SeekRejectValue, IsExclusive>;

  // Note that, conceptually, WaitForData makes sense in a non-exclusive sense.
  // But in the current architecture it's only ever used exclusively (by MDSM),
  // so we mark it that way to verify our assumptions. If you have a use-case
  // for multiple WaitForData consumers, feel free to flip the exclusivity here.
  using WaitForDataPromise =
    MozPromise<MediaData::Type, WaitForDataRejectValue, IsExclusive>;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaDecoderReader)

  // The caller must ensure that Shutdown() is called before aDecoder is
  // destroyed.
  explicit MediaDecoderReader(MediaDecoderReaderInit& aInit);

  // Initializes the reader, returns NS_OK on success, or NS_ERROR_FAILURE
  // on failure.
  nsresult Init();

  // Called by MDSM in dormant state to release resources allocated by this
  // reader. The reader can resume decoding by calling Seek() to a specific
  // position.
  virtual void ReleaseResources() = 0;

  // Destroys the decoding state. The reader cannot be made usable again.
  // This is different from ReleaseMediaResources() as it is irreversable,
  // whereas ReleaseMediaResources() is.  Must be called on the decode
  // thread.
  virtual RefPtr<ShutdownPromise> Shutdown();

  virtual bool OnTaskQueue() const
  {
    return OwnerThread()->IsCurrentThreadIn();
  }

  void UpdateDuration(const media::TimeUnit& aDuration);

  virtual void UpdateCompositor(already_AddRefed<layers::KnowsCompositor>) = 0;

  // Resets all state related to decoding, emptying all buffers etc.
  // Cancels all pending Request*Data() request callbacks, rejects any
  // outstanding seek promises, and flushes the decode pipeline. The
  // decoder must not call any of the callbacks for outstanding
  // Request*Data() calls after this is called. Calls to Request*Data()
  // made after this should be processed as usual.
  //
  // Normally this call preceedes a Seek() call, or shutdown.
  //
  // aParam is a set of TrackInfo::TrackType enums specifying which
  // queues need to be reset, defaulting to both audio and video tracks.
  virtual nsresult ResetDecode(
    TrackSet aTracks = TrackSet(TrackInfo::kAudioTrack,
                                TrackInfo::kVideoTrack));

  // Requests one audio sample from the reader.
  //
  // The decode should be performed asynchronously, and the promise should
  // be resolved when it is complete.
  virtual RefPtr<AudioDataPromise> RequestAudioData() = 0;

  // Requests one video sample from the reader.
  virtual RefPtr<VideoDataPromise>
  RequestVideoData(const media::TimeUnit& aTimeThreshold) = 0;

  // By default, the state machine polls the reader once per second when it's
  // in buffering mode. Some readers support a promise-based mechanism by which
  // they notify the state machine when the data arrives.
  virtual bool IsWaitForDataSupported() const = 0;

  virtual RefPtr<WaitForDataPromise> WaitForData(MediaData::Type aType) = 0;

  // The default implementation of AsyncReadMetadata is implemented in terms of
  // synchronous ReadMetadata() calls. Implementations may also
  // override AsyncReadMetadata to create a more proper async implementation.
  virtual RefPtr<MetadataPromise> AsyncReadMetadata() = 0;

  // Fills aInfo with the latest cached data required to present the media,
  // ReadUpdatedMetadata will always be called once ReadMetadata has succeeded.
  virtual void ReadUpdatedMetadata(MediaInfo* aInfo) = 0;

  // Moves the decode head to aTime microseconds.
  virtual RefPtr<SeekPromise> Seek(const SeekTarget& aTarget) = 0;

  virtual void SetCDMProxy(CDMProxy* aProxy) = 0;

  // Tell the reader that the data decoded are not for direct playback, so it
  // can accept more files, in particular those which have more channels than
  // available in the audio output.
  void SetIgnoreAudioOutputFormat()
  {
  }

  // The MediaDecoderStateMachine uses various heuristics that assume that
  // raw media data is arriving sequentially from a network channel. This
  // makes sense in the <video src="foo"> case, but not for more advanced use
  // cases like MSE.
  virtual bool UseBufferingHeuristics() const = 0;

  // Returns the number of bytes of memory allocated by structures/frames in
  // the video queue.
  size_t SizeOfVideoQueueInBytes() const;

  // Returns the number of bytes of memory allocated by structures/frames in
  // the audio queue.
  size_t SizeOfAudioQueueInBytes() const;

  virtual size_t SizeOfVideoQueueInFrames();
  virtual size_t SizeOfAudioQueueInFrames();

  // Called once new data has been cached by the MediaResource.
  // mBuffered should be recalculated and updated accordingly.
  virtual void NotifyDataArrived() = 0;

  virtual MediaQueue<AudioData>& AudioQueue() { return mAudioQueue; }
  virtual MediaQueue<VideoData>& VideoQueue() { return mVideoQueue; }

  AbstractCanonical<media::TimeIntervals>* CanonicalBuffered()
  {
    return &mBuffered;
  }

  TaskQueue* OwnerThread() const
  {
    return mTaskQueue;
  }

  // Returns true if this decoder reader uses hardware accelerated video
  // decoding.
  virtual bool VideoIsHardwareAccelerated() const = 0;

  TimedMetadataEventSource& TimedMetadataEvent()
  {
    return mTimedMetadataEvent;
  }

  // Notified by the OggDemuxer during playback when chained ogg is detected.
  MediaEventSource<void>& OnMediaNotSeekable() { return mOnMediaNotSeekable; }

  TimedMetadataEventProducer& TimedMetadataProducer()
  {
    return mTimedMetadataEvent;
  }

  MediaEventProducer<void>& MediaNotSeekableProducer()
  {
    return mOnMediaNotSeekable;
  }

  // Notified if the reader can't decode a sample due to a missing decryption
  // key.
  MediaEventSource<TrackInfo::TrackType>& OnTrackWaitingForKey()
  {
    return mOnTrackWaitingForKey;
  }

  MediaEventProducer<TrackInfo::TrackType>& OnTrackWaitingForKeyProducer()
  {
    return mOnTrackWaitingForKey;
  }

  MediaEventSource<nsTArray<uint8_t>, nsString>& OnEncrypted()
  {
    return mOnEncrypted;
  }

  MediaEventSource<void>& OnWaitingForKey()
  {
    return mOnWaitingForKey;
  }

  MediaEventSource<MediaResult>& OnDecodeWarning()
  {
    return mOnDecodeWarning;
  }

  // Switch the video decoder to NullDecoderModule. It might takes effective
  // since a few samples later depends on how much demuxed samples are already
  // queued in the original video decoder.
  virtual void SetVideoNullDecode(bool aIsNullDecode) = 0;

protected:
  virtual ~MediaDecoderReader();

  // Recomputes mBuffered.
  virtual void UpdateBuffered() = 0;

  // Queue of audio frames. This queue is threadsafe, and is accessed from
  // the audio, decoder, state machine, and main threads.
  MediaQueue<AudioData> mAudioQueue;

  // Queue of video frames. This queue is threadsafe, and is accessed from
  // the decoder, state machine, and main threads.
  MediaQueue<VideoData> mVideoQueue;

  // Reference to the owning decoder object.
  AbstractMediaDecoder* mDecoder;

  // Decode task queue.
  RefPtr<TaskQueue> mTaskQueue;

  // Buffered range.
  Canonical<media::TimeIntervals> mBuffered;

  // Stores presentation info required for playback.
  MediaInfo mInfo;

  media::NullableTimeUnit mDuration;

  bool mShutdown;

  // Used to send TimedMetadata to the listener.
  TimedMetadataEventProducer mTimedMetadataEvent;

  // Notify if this media is not seekable.
  MediaEventProducer<void> mOnMediaNotSeekable;

  // Notify if we are waiting for a decryption key.
  MediaEventProducer<TrackInfo::TrackType> mOnTrackWaitingForKey;

  MediaEventProducer<nsTArray<uint8_t>, nsString> mOnEncrypted;

  MediaEventProducer<void> mOnWaitingForKey;

  MediaEventProducer<MediaResult> mOnDecodeWarning;

  RefPtr<MediaResource> mResource;

private:
  virtual nsresult InitInternal() = 0;
};

} // namespace mozilla

#endif
