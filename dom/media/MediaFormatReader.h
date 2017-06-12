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

class MediaFormatReader final : public MediaDecoderReader
{
  typedef TrackInfo::TrackType TrackType;
  typedef MozPromise<bool, MediaResult, /* IsExclusive = */ true> NotifyDataArrivedPromise;

public:
  MediaFormatReader(AbstractMediaDecoder* aDecoder,
                    MediaDataDemuxer* aDemuxer,
                    VideoFrameContainer* aVideoFrameContainer = nullptr);

  virtual ~MediaFormatReader();

  size_t SizeOfVideoQueueInFrames() override;
  size_t SizeOfAudioQueueInFrames() override;

  RefPtr<VideoDataPromise>
  RequestVideoData(const media::TimeUnit& aTimeThreshold) override;

  RefPtr<AudioDataPromise> RequestAudioData() override;

  RefPtr<MetadataPromise> AsyncReadMetadata() override;

  void ReadUpdatedMetadata(MediaInfo* aInfo) override;

  RefPtr<SeekPromise> Seek(const SeekTarget& aTarget) override;

protected:
  void NotifyDataArrived() override;
  void UpdateBuffered() override;

public:
  // For Media Resource Management
  void ReleaseResources() override;

  nsresult ResetDecode(TrackSet aTracks) override;

  RefPtr<ShutdownPromise> Shutdown() override;

  bool IsAsync() const override { return true; }

  bool VideoIsHardwareAccelerated() const override;

  bool IsWaitForDataSupported() const override { return true; }
  RefPtr<WaitForDataPromise> WaitForData(MediaData::Type aType) override;

  bool UseBufferingHeuristics() const override
  {
    return mTrackDemuxersMayBlock;
  }

  void SetCDMProxy(CDMProxy* aProxy) override;

  // Returns a string describing the state of the decoder data.
  // Used for debugging purposes.
  void GetMozDebugReaderData(nsACString& aString);

  void SetVideoNullDecode(bool aIsNullDecode) override;

private:
  nsresult InitInternal() override;

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

  void NotifyCompositorUpdated(RefPtr<layers::KnowsCompositor> aKnowsCompositor)
  {
    mKnowsCompositor = aKnowsCompositor.forget();
  }

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

  MediaEventListener mCompositorUpdatedListener;
  MediaEventListener mOnTrackWaitingForKeyListener;

  void OnFirstDemuxCompleted(TrackInfo::TrackType aType,
                             RefPtr<MediaTrackDemuxer::SamplesHolder> aSamples);

  void OnFirstDemuxFailed(TrackInfo::TrackType aType, const MediaResult& aError);

  void MaybeResolveMetadataPromise();

  UniquePtr<MetadataTags> mTags;

  // A flag indicating if the start time is known or not.
  bool mHasStartTime = false;

  void ShutdownDecoder(TrackType aTrack);
  RefPtr<ShutdownPromise> TearDownDecoders();
};

} // namespace mozilla

#endif
