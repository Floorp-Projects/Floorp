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
#include "mozilla/Monitor.h"

#include "MediaDataDemuxer.h"
#include "MediaDecoderReader.h"
#include "nsAutoPtr.h"
#include "PDMFactory.h"

namespace mozilla {

class CDMProxy;

class MediaFormatReader final : public MediaDecoderReader
{
  typedef TrackInfo::TrackType TrackType;

public:
  MediaFormatReader(AbstractMediaDecoder* aDecoder,
                    MediaDataDemuxer* aDemuxer,
                    VideoFrameContainer* aVideoFrameContainer = nullptr,
                    layers::LayersBackend aLayersBackend = layers::LayersBackend::LAYERS_NONE);

  virtual ~MediaFormatReader();

  nsresult Init() override;

  size_t SizeOfVideoQueueInFrames() override;
  size_t SizeOfAudioQueueInFrames() override;

  RefPtr<MediaDataPromise>
  RequestVideoData(bool aSkipToNextKeyframe, int64_t aTimeThreshold) override;

  RefPtr<MediaDataPromise> RequestAudioData() override;

  RefPtr<MetadataPromise> AsyncReadMetadata() override;

  void ReadUpdatedMetadata(MediaInfo* aInfo) override;

  RefPtr<SeekPromise>
  Seek(SeekTarget aTarget, int64_t aUnused) override;

protected:
  void NotifyDataArrivedInternal() override;

public:
  media::TimeIntervals GetBuffered() override;

  RefPtr<BufferedUpdatePromise> UpdateBufferedWithPromise() override;

  bool ForceZeroStartTime() const override;

  // For Media Resource Management
  void ReleaseResources() override;

  nsresult ResetDecode(TrackSet aTracks) override;

  RefPtr<ShutdownPromise> Shutdown() override;

  bool IsAsync() const override { return true; }

  bool VideoIsHardwareAccelerated() const override;

  bool IsWaitForDataSupported() const override { return true; }
  RefPtr<WaitForDataPromise> WaitForData(MediaData::Type aType) override;

  // MediaFormatReader supports demuxed-only mode.
  bool IsDemuxOnlySupported() const override { return true; }

  void SetDemuxOnly(bool aDemuxedOnly) override
  {
    if (OnTaskQueue()) {
      mDemuxOnly = aDemuxedOnly;
      return;
    }
    nsCOMPtr<nsIRunnable> r = NewRunnableMethod<bool>(
      this, &MediaDecoderReader::SetDemuxOnly, aDemuxedOnly);
    OwnerThread()->Dispatch(r.forget());
  }

  bool UseBufferingHeuristics() const override
  {
    return mTrackDemuxersMayBlock;
  }

#ifdef MOZ_EME
  void SetCDMProxy(CDMProxy* aProxy) override;
#endif

  // Returns a string describing the state of the decoder data.
  // Used for debugging purposes.
  void GetMozDebugReaderData(nsAString& aString);

  void SetVideoBlankDecode(bool aIsBlankDecode) override;

private:

  bool HasVideo() const { return mVideo.mTrackDemuxer; }
  bool HasAudio() const { return mAudio.mTrackDemuxer; }

  bool IsWaitingOnCDMResource();

  bool InitDemuxer();
  // Notify the demuxer that new data has been received.
  // The next queued task calling GetBuffered() is guaranteed to have up to date
  // buffered ranges.
  void NotifyDemuxer();
  void ReturnOutput(MediaData* aData, TrackType aTrack);

  MediaResult EnsureDecoderCreated(TrackType aTrack);
  bool EnsureDecoderInitialized(TrackType aTrack);

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

  struct InternalSeekTarget {
    InternalSeekTarget(const media::TimeInterval& aTime, bool aDropTarget)
      : mTime(aTime)
      , mDropTarget(aDropTarget)
      , mWaiting(false)
      , mHasSeeked(false)
    {}

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
  void NotifyNewOutput(TrackType aTrack, MediaData* aSample);
  void NotifyInputExhausted(TrackType aTrack);
  void NotifyDrainComplete(TrackType aTrack);
  void NotifyError(TrackType aTrack, const MediaResult& aError);
  void NotifyWaitingForData(TrackType aTrack);
  void NotifyEndOfStream(TrackType aTrack);

  void ExtractCryptoInitData(nsTArray<uint8_t>& aInitData);

  // Initializes mLayersBackendType if possible.
  void InitLayersBackendType();

  // DecoderCallback proxies the MediaDataDecoderCallback calls to these
  // functions.
  void Output(TrackType aType, MediaData* aSample);
  void InputExhausted(TrackType aTrack);
  void Error(TrackType aTrack, const MediaResult& aError);
  void Reset(TrackType aTrack);
  void DrainComplete(TrackType aTrack);
  void DropDecodedSamples(TrackType aTrack);
  void WaitingForKey(TrackType aTrack);

  bool ShouldSkip(bool aSkipToNextKeyframe, media::TimeUnit aTimeThreshold);

  void SetVideoDecodeThreshold();

  size_t SizeOfQueue(TrackType aTrack);

  RefPtr<PDMFactory> mPlatform;

  class DecoderCallback : public MediaDataDecoderCallback {
  public:
    DecoderCallback(MediaFormatReader* aReader, TrackType aType)
      : mReader(aReader)
      , mType(aType)
    {
    }
    void Output(MediaData* aSample) override {
      mReader->Output(mType, aSample);
    }
    void InputExhausted() override {
      mReader->InputExhausted(mType);
    }
    void Error(const MediaResult& aError) override {
      mReader->Error(mType, aError);
    }
    void DrainComplete() override {
      mReader->DrainComplete(mType);
    }
    void ReleaseMediaResources() override {
      mReader->ReleaseResources();
    }
    bool OnReaderTaskQueue() override {
      return mReader->OnTaskQueue();
    }
    void WaitingForKey() override {
      mReader->WaitingForKey(mType);
    }

  private:
    MediaFormatReader* mReader;
    TrackType mType;
  };

  struct DecoderData {
    DecoderData(MediaFormatReader* aOwner,
                MediaData::Type aType,
                uint32_t aNumOfMaxError)
      : mOwner(aOwner)
      , mType(aType)
      , mMonitor("DecoderData")
      , mDescription("shutdown")
      , mUpdateScheduled(false)
      , mDemuxEOS(false)
      , mWaitingForData(false)
      , mReceivedNewData(false)
      , mDecoderInitialized(false)
      , mOutputRequested(false)
      , mDecodePending(false)
      , mNeedDraining(false)
      , mDraining(false)
      , mDrainComplete(false)
      , mNumOfConsecutiveError(0)
      , mMaxConsecutiveError(aNumOfMaxError)
      , mNumSamplesInput(0)
      , mNumSamplesOutput(0)
      , mNumSamplesOutputTotal(0)
      , mNumSamplesSkippedTotal(0)
      , mSizeOfQueue(0)
      , mIsHardwareAccelerated(false)
      , mLastStreamSourceID(UINT32_MAX)
      , mIsBlankDecode(false)
    {}

    MediaFormatReader* mOwner;
    // Disambiguate Audio vs Video.
    MediaData::Type mType;
    RefPtr<MediaTrackDemuxer> mTrackDemuxer;
    // TaskQueue on which decoder can choose to decode.
    // Only non-null up until the decoder is created.
    RefPtr<TaskQueue> mTaskQueue;
    // Callback that receives output and error notifications from the decoder.
    nsAutoPtr<DecoderCallback> mCallback;

    // Monitor protecting mDescription and mDecoder.
    Monitor mMonitor;
    // The platform decoder.
    RefPtr<MediaDataDecoder> mDecoder;
    const char* mDescription;
    void ShutdownDecoder()
    {
      mInitPromise.DisconnectIfExists();
      MonitorAutoLock mon(mMonitor);
      if (mDecoder) {
        mDecoder->Shutdown();
      }
      mDescription = "shutdown";
      mDecoder = nullptr;
    }

    // Only accessed from reader's task queue.
    bool mUpdateScheduled;
    bool mDemuxEOS;
    bool mWaitingForData;
    bool mReceivedNewData;

    // Pending seek.
    MozPromiseRequestHolder<MediaTrackDemuxer::SeekPromise> mSeekRequest;

    // Queued demux samples waiting to be decoded.
    nsTArray<RefPtr<MediaRawData>> mQueuedSamples;
    MozPromiseRequestHolder<MediaTrackDemuxer::SamplesPromise> mDemuxRequest;
    MozPromiseHolder<WaitForDataPromise> mWaitingPromise;
    bool HasWaitingPromise()
    {
      MOZ_ASSERT(mOwner->OnTaskQueue());
      return !mWaitingPromise.IsEmpty();
    }

    // MediaDataDecoder handler's variables.
    // Decoder initialization promise holder.
    MozPromiseRequestHolder<MediaDataDecoder::InitPromise> mInitPromise;
    // False when decoder is created. True when decoder Init() promise is resolved.
    bool mDecoderInitialized;
    bool mOutputRequested;
    // Set to true once the MediaDataDecoder has been fed a compressed sample.
    // No more sample will be passed to the decoder while true.
    // mDecodePending is reset when:
    // 1- The decoder returns a sample
    // 2- The decoder calls InputExhausted
    // 3- The decoder is Flushed or Reset.
    bool mDecodePending;
    bool mNeedDraining;
    bool mDraining;
    bool mDrainComplete;

    bool HasPendingDrain() const
    {
      return mDraining || mDrainComplete;
    }

    uint32_t mNumOfConsecutiveError;
    uint32_t mMaxConsecutiveError;

    Maybe<MediaResult> mError;
    bool HasFatalError() const
    {
      return mError.isSome() && mError.ref() != NS_ERROR_DOM_MEDIA_DECODE_ERR;
    }

    // If set, all decoded samples prior mTimeThreshold will be dropped.
    // Used for internal seeking when a change of stream is detected or when
    // encountering data discontinuity.
    Maybe<InternalSeekTarget> mTimeThreshold;
    // Time of last sample returned.
    Maybe<media::TimeInterval> mLastSampleTime;

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
    virtual RefPtr<MediaDataPromise> EnsurePromise(const char* aMethodName) = 0;
    virtual void ResolvePromise(MediaData* aData, const char* aMethodName) = 0;
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
    // Decoding will be suspended until mInputRequested is set again.
    // Following a flush, the decoder is ready to accept any new data.
    void Flush()
    {
      if (mDecoder) {
        mDecoder->Flush();
      }
      mOutputRequested = false;
      mDecodePending = false;
      mOutput.Clear();
      mNumSamplesInput = 0;
      mNumSamplesOutput = 0;
      mSizeOfQueue = 0;
      mDraining = false;
      mDrainComplete = false;
    }

    // Reset the state of the DecoderData, clearing all queued frames
    // (pending demuxed and decoded).
    // Decoding will be suspended until mInputRequested is set again.
    // The track demuxer is *not* reset.
    void ResetState()
    {
      MOZ_ASSERT(mOwner->OnTaskQueue());
      mDemuxEOS = false;
      mWaitingForData = false;
      mQueuedSamples.Clear();
      mOutputRequested = false;
      mNeedDraining = false;
      mDecodePending = false;
      mDraining = false;
      mDrainComplete = false;
      mTimeThreshold.reset();
      mLastSampleTime.reset();
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
    RefPtr<SharedTrackInfo> mInfo;
    Maybe<media::TimeUnit> mFirstDemuxedSampleTime;
    // Use BlankDecoderModule or not.
    bool mIsBlankDecode;

  };

  class DecoderDataWithPromise : public DecoderData {
  public:
    DecoderDataWithPromise(MediaFormatReader* aOwner,
                           MediaData::Type aType,
                           uint32_t aNumOfMaxError)
      : DecoderData(aOwner, aType, aNumOfMaxError)
      , mHasPromise(false)

    {}

    bool HasPromise() const override
    {
      return mHasPromise;
    }

    RefPtr<MediaDataPromise> EnsurePromise(const char* aMethodName) override
    {
      MOZ_ASSERT(mOwner->OnTaskQueue());
      mHasPromise = true;
      return mPromise.Ensure(aMethodName);
    }

    void ResolvePromise(MediaData* aData, const char* aMethodName) override
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
    MozPromiseHolder<MediaDataPromise> mPromise;
    Atomic<bool> mHasPromise;
  };

  DecoderDataWithPromise mAudio;
  DecoderDataWithPromise mVideo;

  // Returns true when the decoder for this track needs input.
  bool NeedInput(DecoderData& aDecoder);

  DecoderData& GetDecoderData(TrackType aTrack);

  // Demuxer objects.
  RefPtr<MediaDataDemuxer> mDemuxer;
  bool mDemuxerInitDone;
  void OnDemuxerInitDone(nsresult);
  void OnDemuxerInitFailed(const MediaResult& aError);
  MozPromiseRequestHolder<MediaDataDemuxer::InitPromise> mDemuxerInitRequest;
  void OnDemuxFailed(TrackType aTrack, const MediaResult& aError);

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

  layers::LayersBackend mLayersBackendType;

  // Metadata objects
  // True if we've read the streams' metadata.
  bool mInitDone;
  MozPromiseHolder<MetadataPromise> mMetadataPromise;
  bool IsEncrypted() const;

  // Set to true if any of our track buffers may be blocking.
  bool mTrackDemuxersMayBlock;

  // Set the demuxed-only flag.
  Atomic<bool> mDemuxOnly;

  // Seeking objects.
  void SetSeekTarget(const SeekTarget& aTarget);
  media::TimeUnit DemuxStartTime();
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

#ifdef MOZ_EME
  RefPtr<CDMProxy> mCDMProxy;
#endif
  RefPtr<GMPCrashHelper> mCrashHelper;

  void SetBlankDecode(TrackType aTrack, bool aIsBlankDecode);
};

} // namespace mozilla

#endif
