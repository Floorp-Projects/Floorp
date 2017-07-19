/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(MediaFormatReader_h_)
#define MediaFormatReader_h_

#include "mozilla/Atomics.h"
#include "mozilla/Maybe.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/Mutex.h"

#include "MediaEventSource.h"
#include "MediaDataDemuxer.h"
#include "MediaDecoderReader.h"
#include "MediaPrefs.h"
#include "nsAutoPtr.h"
#include "PDMFactory.h"

namespace mozilla {

class CDMProxy;

struct WaitForDataRejectValue
{
  enum Reason
  {
    SHUTDOWN,
    CANCELED
  };

  WaitForDataRejectValue(MediaData::Type aType, Reason aReason)
    : mType(aType)
    , mReason(aReason)
  {
  }
  MediaData::Type mType;
  Reason mReason;
};

struct SeekRejectValue
{
  MOZ_IMPLICIT SeekRejectValue(const MediaResult& aError)
    : mType(MediaData::NULL_DATA)
    , mError(aError)
  {
  }
  MOZ_IMPLICIT SeekRejectValue(nsresult aResult)
    : mType(MediaData::NULL_DATA)
    , mError(aResult)
  {
  }
  SeekRejectValue(MediaData::Type aType, const MediaResult& aError)
    : mType(aType)
    , mError(aError)
  {
  }
  MediaData::Type mType;
  MediaResult mError;
};

struct MetadataHolder
{
  UniquePtr<MediaInfo> mInfo;
  UniquePtr<MetadataTags> mTags;
};

class MediaFormatReader final : public MediaDecoderReader
{
  static const bool IsExclusive = true;
  typedef TrackInfo::TrackType TrackType;
  typedef MozPromise<bool, MediaResult, IsExclusive> NotifyDataArrivedPromise;

public:
  using TrackSet = EnumSet<TrackInfo::TrackType>;
  using MetadataPromise = MozPromise<MetadataHolder, MediaResult, IsExclusive>;

  template<typename Type>
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

  MediaFormatReader(MediaDecoderReaderInit& aInit, MediaDataDemuxer* aDemuxer);

  virtual ~MediaFormatReader();

  // Initializes the reader, returns NS_OK on success, or NS_ERROR_FAILURE
  // on failure.
  nsresult Init();

  size_t SizeOfVideoQueueInFrames();
  size_t SizeOfAudioQueueInFrames();

  // Requests one video sample from the reader.
  RefPtr<VideoDataPromise> RequestVideoData(
    const media::TimeUnit& aTimeThreshold);

  // Requests one audio sample from the reader.
  //
  // The decode should be performed asynchronously, and the promise should
  // be resolved when it is complete.
  RefPtr<AudioDataPromise> RequestAudioData();

  // The default implementation of AsyncReadMetadata is implemented in terms of
  // synchronous ReadMetadata() calls. Implementations may also
  // override AsyncReadMetadata to create a more proper async implementation.
  RefPtr<MetadataPromise> AsyncReadMetadata();

  // Fills aInfo with the latest cached data required to present the media,
  // ReadUpdatedMetadata will always be called once ReadMetadata has succeeded.
  void ReadUpdatedMetadata(MediaInfo* aInfo);

  RefPtr<SeekPromise> Seek(const SeekTarget& aTarget);

  // Called once new data has been cached by the MediaResource.
  // mBuffered should be recalculated and updated accordingly.
  void NotifyDataArrived();

protected:
  // Recomputes mBuffered.
  void UpdateBuffered();

public:
  // Called by MDSM in dormant state to release resources allocated by this
  // reader. The reader can resume decoding by calling Seek() to a specific
  // position.
  void ReleaseResources();

  bool OnTaskQueue() const { return OwnerThread()->IsCurrentThreadIn(); }

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
  nsresult ResetDecode(TrackSet aTracks);

  // Destroys the decoding state. The reader cannot be made usable again.
  // This is different from ReleaseMediaResources() as it is irreversable,
  // whereas ReleaseMediaResources() is.  Must be called on the decode
  // thread.
  RefPtr<ShutdownPromise> Shutdown();

  // Returns true if this decoder reader uses hardware accelerated video
  // decoding.
  bool VideoIsHardwareAccelerated() const;

  // By default, the state machine polls the reader once per second when it's
  // in buffering mode. Some readers support a promise-based mechanism by which
  // they notify the state machine when the data arrives.
  bool IsWaitForDataSupported() const { return true; }

  RefPtr<WaitForDataPromise> WaitForData(MediaData::Type aType);

  // The MediaDecoderStateMachine uses various heuristics that assume that
  // raw media data is arriving sequentially from a network channel. This
  // makes sense in the <video src="foo"> case, but not for more advanced use
  // cases like MSE.
  bool UseBufferingHeuristics() const { return mTrackDemuxersMayBlock; }

  void SetCDMProxy(CDMProxy* aProxy);

  // Returns a string describing the state of the decoder data.
  // Used for debugging purposes.
  void GetMozDebugReaderData(nsACString& aString);

  // Switch the video decoder to NullDecoderModule. It might takes effective
  // since a few samples later depends on how much demuxed samples are already
  // queued in the original video decoder.
  void SetVideoNullDecode(bool aIsNullDecode);

  void UpdateCompositor(already_AddRefed<layers::KnowsCompositor>);

  void UpdateDuration(const media::TimeUnit& aDuration)
  {
    MOZ_ASSERT(OnTaskQueue());
    UpdateBuffered();
  }

  AbstractCanonical<media::TimeIntervals>* CanonicalBuffered()
  {
    return &mBuffered;
  }

  TaskQueue* OwnerThread() const { return mTaskQueue; }

  TimedMetadataEventSource& TimedMetadataEvent() { return mTimedMetadataEvent; }

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

  MediaEventSource<void>& OnWaitingForKey() { return mOnWaitingForKey; }

  MediaEventSource<MediaResult>& OnDecodeWarning() { return mOnDecodeWarning; }

private:
  bool HasVideo() const { return mVideo.mTrackDemuxer; }
  bool HasAudio() const { return mAudio.mTrackDemuxer; }

  bool IsWaitingOnCDMResource();

  bool InitDemuxer();
  // Notify the track demuxers that new data has been received.
  void NotifyTrackDemuxers();
  void ReturnOutput(MediaData* aData, TrackType aTrack);

  // Enqueues a task to call Update(aTrack) on the decoder task queue.
  // Lock for corresponding track must be held.
  void ScheduleUpdate(TrackType aTrack);
  void Update(TrackType aTrack);
  // Handle actions should more data be received.
  // Returns true if no more action is required.
  bool UpdateReceivedNewData(TrackType aTrack);
  // Called when new samples need to be demuxed.
  void RequestDemuxSamples(TrackType aTrack);
  // Handle demuxed samples by the input behavior.
  void HandleDemuxedSamples(TrackType aTrack,
                            AbstractMediaDecoder::AutoNotifyDecoded& aA);
  // Decode any pending already demuxed samples.
  void DecodeDemuxedSamples(TrackType aTrack,
                            MediaRawData* aSample);

  struct InternalSeekTarget
  {
    InternalSeekTarget(const media::TimeInterval& aTime, bool aDropTarget)
      : mTime(aTime)
      , mDropTarget(aDropTarget)
      , mWaiting(false)
      , mHasSeeked(false)
    {
    }

    media::TimeUnit Time() const { return mTime.mStart; }
    media::TimeUnit EndTime() const { return mTime.mEnd; }
    bool Contains(const media::TimeUnit& aTime) const
    {
      return mTime.Contains(aTime);
    }

    media::TimeInterval mTime;
    bool mDropTarget;
    bool mWaiting;
    bool mHasSeeked;
  };

  // Perform an internal seek to aTime. If aDropTarget is true then
  // the first sample past the target will be dropped.
  void InternalSeek(TrackType aTrack, const InternalSeekTarget& aTarget);

  // Drain the current decoder.
  void DrainDecoder(TrackType aTrack);
  void NotifyNewOutput(TrackType aTrack,
                       const MediaDataDecoder::DecodedData& aResults);
  void NotifyError(TrackType aTrack, const MediaResult& aError);
  void NotifyWaitingForData(TrackType aTrack);
  void NotifyWaitingForKey(TrackType aTrack);
  void NotifyEndOfStream(TrackType aTrack);

  void ExtractCryptoInitData(nsTArray<uint8_t>& aInitData);

  // Initializes mLayersBackendType if possible.
  void InitLayersBackendType();

  void Reset(TrackType aTrack);
  void DropDecodedSamples(TrackType aTrack);

  bool ShouldSkip(media::TimeUnit aTimeThreshold);

  void SetVideoDecodeThreshold();

  size_t SizeOfQueue(TrackType aTrack);

  RefPtr<PDMFactory> mPlatform;

  enum class DrainState
  {
    None,
    DrainRequested,
    Draining,
    PartialDrainPending,
    DrainCompleted,
    DrainAborted,
  };

  class SharedShutdownPromiseHolder : public MozPromiseHolder<ShutdownPromise>
  {
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SharedShutdownPromiseHolder)
  private:
    ~SharedShutdownPromiseHolder() { }
  };

  struct DecoderData
  {
    DecoderData(MediaFormatReader* aOwner,
                MediaData::Type aType,
                uint32_t aNumOfMaxError)
      : mOwner(aOwner)
      , mType(aType)
      , mMutex("DecoderData")
      , mDescription("shutdown")
      , mUpdateScheduled(false)
      , mDemuxEOS(false)
      , mWaitingForData(false)
      , mWaitingForKey(false)
      , mReceivedNewData(false)
      , mFlushing(false)
      , mFlushed(true)
      , mDrainState(DrainState::None)
      , mNumOfConsecutiveError(0)
      , mMaxConsecutiveError(aNumOfMaxError)
      , mFirstFrameTime(Some(media::TimeUnit::Zero()))
      , mNumSamplesInput(0)
      , mNumSamplesOutput(0)
      , mNumSamplesOutputTotal(0)
      , mNumSamplesSkippedTotal(0)
      , mSizeOfQueue(0)
      , mIsHardwareAccelerated(false)
      , mLastStreamSourceID(UINT32_MAX)
      , mIsNullDecode(false)
    {
    }

    MediaFormatReader* mOwner;
    // Disambiguate Audio vs Video.
    MediaData::Type mType;
    RefPtr<MediaTrackDemuxer> mTrackDemuxer;
    // TaskQueue on which decoder can choose to decode.
    // Only non-null up until the decoder is created.
    RefPtr<TaskQueue> mTaskQueue;

    // Mutex protecting mDescription and mDecoder.
    Mutex mMutex;
    // The platform decoder.
    RefPtr<MediaDataDecoder> mDecoder;
    const char* mDescription;
    void ShutdownDecoder();

    // Only accessed from reader's task queue.
    bool mUpdateScheduled;
    bool mDemuxEOS;
    bool mWaitingForData;
    bool mWaitingForKey;
    bool mReceivedNewData;

    // Pending seek.
    MozPromiseRequestHolder<MediaTrackDemuxer::SeekPromise> mSeekRequest;

    // Queued demux samples waiting to be decoded.
    nsTArray<RefPtr<MediaRawData>> mQueuedSamples;
    MozPromiseRequestHolder<MediaTrackDemuxer::SamplesPromise> mDemuxRequest;
    // A WaitingPromise is pending if the demuxer is waiting for data or
    // if the decoder is waiting for a key.
    MozPromiseHolder<WaitForDataPromise> mWaitingPromise;
    bool HasWaitingPromise() const
    {
      MOZ_ASSERT(mOwner->OnTaskQueue());
      return !mWaitingPromise.IsEmpty();
    }
    bool IsWaiting() const
    {
      MOZ_ASSERT(mOwner->OnTaskQueue());
      return mWaitingForData || mWaitingForKey;
    }

    // MediaDataDecoder handler's variables.
    MozPromiseRequestHolder<MediaDataDecoder::DecodePromise> mDecodeRequest;
    bool mFlushing; // True if flush is in action.
    // Set to true if the last operation run on the decoder was a flush.
    bool mFlushed;
    RefPtr<SharedShutdownPromiseHolder> mShutdownPromise;

    MozPromiseRequestHolder<MediaDataDecoder::DecodePromise> mDrainRequest;
    DrainState mDrainState;
    bool HasPendingDrain() const
    {
      return mDrainState != DrainState::None;
    }
    bool HasCompletedDrain() const
    {
      return mDrainState == DrainState::DrainCompleted ||
             mDrainState == DrainState::DrainAborted;
    }
    void RequestDrain()
    {
        MOZ_RELEASE_ASSERT(mDrainState == DrainState::None);
        mDrainState = DrainState::DrainRequested;
    }

    uint32_t mNumOfConsecutiveError;
    uint32_t mMaxConsecutiveError;
    // Set when we haven't yet decoded the first frame.
    // Cleared once the first frame has been decoded.
    // This is used to determine, upon error, if we should try again to decode
    // the frame, or skip to the next keyframe.
    Maybe<media::TimeUnit> mFirstFrameTime;

    Maybe<MediaResult> mError;
    bool HasFatalError() const
    {
      if (!mError.isSome()) {
        return false;
      }
      if (mError.ref() == NS_ERROR_DOM_MEDIA_DECODE_ERR) {
        // Allow decode errors to be non-fatal, but give up
        // if we have too many, or if warnings should be treated as errors.
        return mNumOfConsecutiveError > mMaxConsecutiveError
               || MediaPrefs::MediaWarningsAsErrors();
      } else if (mError.ref() == NS_ERROR_DOM_MEDIA_NEED_NEW_DECODER) {
        // If the caller asked for a new decoder we shouldn't treat
        // it as fatal.
        return false;
      } else {
        // All other error types are fatal
        return true;
      }
    }

    // If set, all decoded samples prior mTimeThreshold will be dropped.
    // Used for internal seeking when a change of stream is detected or when
    // encountering data discontinuity.
    Maybe<InternalSeekTarget> mTimeThreshold;
    // Time of last decoded sample returned.
    Maybe<media::TimeInterval> mLastDecodedSampleTime;

    // Decoded samples returned my mDecoder awaiting being returned to
    // state machine upon request.
    nsTArray<RefPtr<MediaData>> mOutput;
    uint64_t mNumSamplesInput;
    uint64_t mNumSamplesOutput;
    uint64_t mNumSamplesOutputTotal;
    uint64_t mNumSamplesSkippedTotal;

    // These get overridden in the templated concrete class.
    // Indicate if we have a pending promise for decoded frame.
    // Rejecting the promise will stop the reader from decoding ahead.
    virtual bool HasPromise() const = 0;
    virtual void RejectPromise(const MediaResult& aError,
                               const char* aMethodName) = 0;

    // Clear track demuxer related data.
    void ResetDemuxer()
    {
      mDemuxRequest.DisconnectIfExists();
      mSeekRequest.DisconnectIfExists();
      mTrackDemuxer->Reset();
      mQueuedSamples.Clear();
    }

    // Flush the decoder if present and reset decoding related data.
    // Following a flush, the decoder is ready to accept any new data.
    void Flush();

    bool CancelWaitingForKey()
    {
      if (!mWaitingForKey) {
        return false;
      }
      mWaitingForKey = false;
      if (IsWaiting() || !HasWaitingPromise()) {
        return false;
      }
      mWaitingPromise.Resolve(mType, __func__);
      return true;
    }

    // Reset the state of the DecoderData, clearing all queued frames
    // (pending demuxed and decoded).
    // The track demuxer is *not* reset.
    void ResetState()
    {
      MOZ_ASSERT(mOwner->OnTaskQueue());
      mDemuxEOS = false;
      mWaitingForData = false;
      mQueuedSamples.Clear();
      mDecodeRequest.DisconnectIfExists();
      mDrainRequest.DisconnectIfExists();
      mDrainState = DrainState::None;
      CancelWaitingForKey();
      mTimeThreshold.reset();
      mLastDecodedSampleTime.reset();
      mOutput.Clear();
      mNumSamplesInput = 0;
      mNumSamplesOutput = 0;
      mSizeOfQueue = 0;
      mNextStreamSourceID.reset();
      if (!HasFatalError()) {
        mError.reset();
      }
    }

    bool HasInternalSeekPending() const
    {
      return mTimeThreshold && !mTimeThreshold.ref().mHasSeeked;
    }

    // Used by the MDSM for logging purposes.
    Atomic<size_t> mSizeOfQueue;
    // Used by the MDSM to determine if video decoding is hardware accelerated.
    // This value is updated after a frame is successfully decoded.
    Atomic<bool> mIsHardwareAccelerated;
    // Sample format monitoring.
    uint32_t mLastStreamSourceID;
    Maybe<uint32_t> mNextStreamSourceID;
    media::TimeIntervals mTimeRanges;
    Maybe<media::TimeUnit> mLastTimeRangesEnd;
    // TrackInfo as first discovered during ReadMetadata.
    UniquePtr<TrackInfo> mOriginalInfo;
    RefPtr<TrackInfoSharedPtr> mInfo;
    Maybe<media::TimeUnit> mFirstDemuxedSampleTime;
    // Use NullDecoderModule or not.
    bool mIsNullDecode;

  };

  template <typename Type>
  class DecoderDataWithPromise : public DecoderData
  {
  public:
    DecoderDataWithPromise(MediaFormatReader* aOwner,
                           MediaData::Type aType,
                           uint32_t aNumOfMaxError)
      : DecoderData(aOwner, aType, aNumOfMaxError)
      , mHasPromise(false)
    {
    }

    bool HasPromise() const override
    {
      return mHasPromise;
    }

    RefPtr<DataPromise<Type>> EnsurePromise(const char* aMethodName)
    {
      MOZ_ASSERT(mOwner->OnTaskQueue());
      mHasPromise = true;
      return mPromise.Ensure(aMethodName);
    }

    void ResolvePromise(Type* aData, const char* aMethodName)
    {
      MOZ_ASSERT(mOwner->OnTaskQueue());
      mPromise.Resolve(aData, aMethodName);
      mHasPromise = false;
    }

    void RejectPromise(const MediaResult& aError,
                       const char* aMethodName) override
    {
      MOZ_ASSERT(mOwner->OnTaskQueue());
      mPromise.Reject(aError, aMethodName);
      mHasPromise = false;
    }

  private:
    MozPromiseHolder<DataPromise<Type>> mPromise;
    Atomic<bool> mHasPromise;
  };

  // Decode task queue.
  RefPtr<TaskQueue> mTaskQueue;

  DecoderDataWithPromise<AudioData> mAudio;
  DecoderDataWithPromise<VideoData> mVideo;

  // Returns true when the decoder for this track needs input.
  bool NeedInput(DecoderData& aDecoder);

  DecoderData& GetDecoderData(TrackType aTrack);

  // Demuxer objects.
  class DemuxerProxy;
  UniquePtr<DemuxerProxy> mDemuxer;
  bool mDemuxerInitDone;
  void OnDemuxerInitDone(const MediaResult& aResult);
  void OnDemuxerInitFailed(const MediaResult& aError);
  MozPromiseRequestHolder<MediaDataDemuxer::InitPromise> mDemuxerInitRequest;
  MozPromiseRequestHolder<NotifyDataArrivedPromise> mNotifyDataArrivedPromise;
  bool mPendingNotifyDataArrived;
  void OnDemuxFailed(TrackType aTrack, const MediaResult &aError);

  void DoDemuxVideo();
  void OnVideoDemuxCompleted(RefPtr<MediaTrackDemuxer::SamplesHolder> aSamples);
  void OnVideoDemuxFailed(const MediaResult& aError)
  {
    OnDemuxFailed(TrackType::kVideoTrack, aError);
  }

  void DoDemuxAudio();
  void OnAudioDemuxCompleted(RefPtr<MediaTrackDemuxer::SamplesHolder> aSamples);
  void OnAudioDemuxFailed(const MediaResult& aError)
  {
    OnDemuxFailed(TrackType::kAudioTrack, aError);
  }

  void SkipVideoDemuxToNextKeyFrame(media::TimeUnit aTimeThreshold);
  MozPromiseRequestHolder<MediaTrackDemuxer::SkipAccessPointPromise> mSkipRequest;
  void VideoSkipReset(uint32_t aSkipped);
  void OnVideoSkipCompleted(uint32_t aSkipped);
  void OnVideoSkipFailed(MediaTrackDemuxer::SkipFailureHolder aFailure);

  // The last number of decoded output frames that we've reported to
  // MediaDecoder::NotifyDecoded(). We diff the number of output video
  // frames every time that DecodeVideoData() is called, and report the
  // delta there.
  uint64_t mLastReportedNumDecodedFrames;

  // Timestamp of the previous decoded keyframe, in microseconds.
  int64_t mPreviousDecodedKeyframeTime_us;
  // Default mLastDecodedKeyframeTime_us value, must be bigger than anything.
  static const int64_t sNoPreviousDecodedKeyframe = INT64_MAX;

  RefPtr<layers::KnowsCompositor> mKnowsCompositor;

  // Metadata objects
  // True if we've read the streams' metadata.
  bool mInitDone;
  MozPromiseHolder<MetadataPromise> mMetadataPromise;
  bool IsEncrypted() const;

  // Set to true if any of our track buffers may be blocking.
  bool mTrackDemuxersMayBlock;

  // Seeking objects.
  void SetSeekTarget(const SeekTarget& aTarget);
  bool IsSeeking() const { return mPendingSeekTime.isSome(); }
  bool IsVideoSeeking() const
  {
    return IsSeeking() && mOriginalSeekTarget.IsVideoOnly();
  }
  void ScheduleSeek();
  void AttemptSeek();
  void OnSeekFailed(TrackType aTrack, const MediaResult& aError);
  void DoVideoSeek();
  void OnVideoSeekCompleted(media::TimeUnit aTime);
  void OnVideoSeekFailed(const MediaResult& aError);
  bool mSeekScheduled;

  void DoAudioSeek();
  void OnAudioSeekCompleted(media::TimeUnit aTime);
  void OnAudioSeekFailed(const MediaResult& aError);
  // The SeekTarget that was last given to Seek()
  SeekTarget mOriginalSeekTarget;
  // Temporary seek information while we wait for the data
  Maybe<media::TimeUnit> mFallbackSeekTime;
  Maybe<media::TimeUnit> mPendingSeekTime;
  MozPromiseHolder<SeekPromise> mSeekPromise;

  RefPtr<VideoFrameContainer> mVideoFrameContainer;
  layers::ImageContainer* GetImageContainer();

  RefPtr<CDMProxy> mCDMProxy;

  RefPtr<GMPCrashHelper> mCrashHelper;

  void SetNullDecode(TrackType aTrack, bool aIsNullDecode);

  class DecoderFactory;
  UniquePtr<DecoderFactory> mDecoderFactory;

  class ShutdownPromisePool;
  UniquePtr<ShutdownPromisePool> mShutdownPromisePool;

  MediaEventListener mOnTrackWaitingForKeyListener;

  void OnFirstDemuxCompleted(TrackInfo::TrackType aType,
                             RefPtr<MediaTrackDemuxer::SamplesHolder> aSamples);

  void OnFirstDemuxFailed(TrackInfo::TrackType aType, const MediaResult& aError);

  void MaybeResolveMetadataPromise();

  // Stores presentation info required for playback.
  MediaInfo mInfo;

  UniquePtr<MetadataTags> mTags;

  // A flag indicating if the start time is known or not.
  bool mHasStartTime = false;

  void ShutdownDecoder(TrackType aTrack);
  RefPtr<ShutdownPromise> TearDownDecoders();

  // Reference to the owning decoder object.
  AbstractMediaDecoder* mDecoder;

  bool mShutdown = false;

  // Buffered range.
  Canonical<media::TimeIntervals> mBuffered;

  // Used to send TimedMetadata to the listener.
  TimedMetadataEventProducer mTimedMetadataEvent;

  // Notify if this media is not seekable.
  MediaEventProducer<void> mOnMediaNotSeekable;

  // Notify if we are waiting for a decryption key.
  MediaEventProducer<TrackInfo::TrackType> mOnTrackWaitingForKey;

  MediaEventProducer<nsTArray<uint8_t>, nsString> mOnEncrypted;

  MediaEventProducer<void> mOnWaitingForKey;

  MediaEventProducer<MediaResult> mOnDecodeWarning;
};

} // namespace mozilla

#endif
