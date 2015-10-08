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

#include "MediaDataDemuxer.h"
#include "MediaDecoderReader.h"
#include "PDMFactory.h"

namespace mozilla {

class CDMProxy;

class MediaFormatReader final : public MediaDecoderReader
{
  typedef TrackInfo::TrackType TrackType;
  typedef media::Interval<int64_t> ByteInterval;

public:
  explicit MediaFormatReader(AbstractMediaDecoder* aDecoder,
                             MediaDataDemuxer* aDemuxer,
                             TaskQueue* aBorrowedTaskQueue = nullptr);

  virtual ~MediaFormatReader();

  nsresult Init(MediaDecoderReader* aCloneDonor) override;

  size_t SizeOfVideoQueueInFrames() override;
  size_t SizeOfAudioQueueInFrames() override;

  nsRefPtr<VideoDataPromise>
  RequestVideoData(bool aSkipToNextKeyframe, int64_t aTimeThreshold) override;

  nsRefPtr<AudioDataPromise> RequestAudioData() override;

  bool HasVideo() override
  {
    return mVideo.mTrackDemuxer;
  }

  bool HasAudio() override
  {
    return mAudio.mTrackDemuxer;
  }

  nsRefPtr<MetadataPromise> AsyncReadMetadata() override;

  void ReadUpdatedMetadata(MediaInfo* aInfo) override;

  nsRefPtr<SeekPromise>
  Seek(int64_t aTime, int64_t aUnused) override;

  bool IsMediaSeekable() override
  {
    return mSeekable;
  }

protected:
  void NotifyDataArrivedInternal(uint32_t aLength, int64_t aOffset) override;
public:
  void NotifyDataRemoved() override;

  media::TimeIntervals GetBuffered() override;

  virtual bool ForceZeroStartTime() const override;

  // For Media Resource Management
  void ReleaseMediaResources() override;

  nsresult ResetDecode() override;

  nsRefPtr<ShutdownPromise> Shutdown() override;

  bool IsAsync() const override { return true; }

  bool VideoIsHardwareAccelerated() const override;

  void DisableHardwareAcceleration() override;

  bool IsWaitForDataSupported() override { return true; }
  nsRefPtr<WaitForDataPromise> WaitForData(MediaData::Type aType) override;

  bool UseBufferingHeuristics() override
  {
    return mTrackDemuxersMayBlock;
  }

#ifdef MOZ_EME
  void SetCDMProxy(CDMProxy* aProxy) override;
#endif

private:
  bool IsWaitingOnCDMResource();

  bool InitDemuxer();
  // Notify the demuxer that new data has been received.
  // The next queued task calling GetBuffered() is guaranteed to have up to date
  // buffered ranges.
  void NotifyDemuxer(uint32_t aLength, int64_t aOffset);
  void ReturnOutput(MediaData* aData, TrackType aTrack);

  bool EnsureDecodersCreated();
  // It returns true when all decoders are initialized. False when there is pending
  // initialization.
  bool EnsureDecodersInitialized();
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
  // Decode any pending already demuxed samples.
  void DecodeDemuxedSamples(TrackType aTrack,
                            AbstractMediaDecoder::AutoNotifyDecoded& aA);
  // Drain the current decoder.
  void DrainDecoder(TrackType aTrack);
  void NotifyNewOutput(TrackType aTrack, MediaData* aSample);
  void NotifyInputExhausted(TrackType aTrack);
  void NotifyDrainComplete(TrackType aTrack);
  void NotifyError(TrackType aTrack);
  void NotifyWaitingForData(TrackType aTrack);
  void NotifyEndOfStream(TrackType aTrack);
  void NotifyDecodingRequested(TrackType aTrack);

  void ExtractCryptoInitData(nsTArray<uint8_t>& aInitData);

  // Initializes mLayersBackendType if possible.
  void InitLayersBackendType();

  // DecoderCallback proxies the MediaDataDecoderCallback calls to these
  // functions.
  void Output(TrackType aType, MediaData* aSample);
  void InputExhausted(TrackType aTrack);
  void Error(TrackType aTrack);
  void Flush(TrackType aTrack);
  void DrainComplete(TrackType aTrack);

  bool ShouldSkip(bool aSkipToNextKeyframe, media::TimeUnit aTimeThreshold);

  size_t SizeOfQueue(TrackType aTrack);

  nsRefPtr<PDMFactory> mPlatform;

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
    void Error() override {
      mReader->Error(mType);
    }
    void DrainComplete() override {
      mReader->DrainComplete(mType);
    }
    void ReleaseMediaResources() override {
      mReader->ReleaseMediaResources();
    }
    bool OnReaderTaskQueue() override {
      return mReader->OnTaskQueue();
    }

  private:
    MediaFormatReader* mReader;
    TrackType mType;
  };

  struct DecoderData {
    DecoderData(MediaFormatReader* aOwner,
                MediaData::Type aType,
                uint32_t aDecodeAhead)
      : mOwner(aOwner)
      , mType(aType)
      , mDecodeAhead(aDecodeAhead)
      , mUpdateScheduled(false)
      , mDemuxEOS(false)
      , mWaitingForData(false)
      , mReceivedNewData(false)
      , mDiscontinuity(true)
      , mDecoderInitialized(false)
      , mDecodingRequested(false)
      , mOutputRequested(false)
      , mInputExhausted(false)
      , mError(false)
      , mNeedDraining(false)
      , mDraining(false)
      , mDrainComplete(false)
      , mNumSamplesInput(0)
      , mNumSamplesOutput(0)
      , mNumSamplesOutputTotal(0)
      , mSizeOfQueue(0)
      , mIsHardwareAccelerated(false)
      , mLastStreamSourceID(UINT32_MAX)
    {}

    MediaFormatReader* mOwner;
    // Disambiguate Audio vs Video.
    MediaData::Type mType;
    nsRefPtr<MediaTrackDemuxer> mTrackDemuxer;
    // The platform decoder.
    nsRefPtr<MediaDataDecoder> mDecoder;
    // TaskQueue on which decoder can choose to decode.
    // Only non-null up until the decoder is created.
    nsRefPtr<FlushableTaskQueue> mTaskQueue;
    // Callback that receives output and error notifications from the decoder.
    nsAutoPtr<DecoderCallback> mCallback;

    // Only accessed from reader's task queue.
    uint32_t mDecodeAhead;
    bool mUpdateScheduled;
    bool mDemuxEOS;
    bool mWaitingForData;
    bool mReceivedNewData;
    bool mDiscontinuity;

    // Pending seek.
    MozPromiseRequestHolder<MediaTrackDemuxer::SeekPromise> mSeekRequest;

    // Queued demux samples waiting to be decoded.
    nsTArray<nsRefPtr<MediaRawData>> mQueuedSamples;
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
    bool mDecodingRequested;
    bool mOutputRequested;
    bool mInputExhausted;
    bool mError;
    bool mNeedDraining;
    bool mDraining;
    bool mDrainComplete;
    // If set, all decoded samples prior mTimeThreshold will be dropped.
    // Used for internal seeking when a change of stream is detected.
    Maybe<media::TimeUnit> mTimeThreshold;

    // Decoded samples returned my mDecoder awaiting being returned to
    // state machine upon request.
    nsTArray<nsRefPtr<MediaData>> mOutput;
    uint64_t mNumSamplesInput;
    uint64_t mNumSamplesOutput;
    uint64_t mNumSamplesOutputTotal;

    // These get overriden in the templated concrete class.
    // Indicate if we have a pending promise for decoded frame.
    // Rejecting the promise will stop the reader from decoding ahead.
    virtual bool HasPromise() = 0;
    virtual void RejectPromise(MediaDecoderReader::NotDecodedReason aReason,
                               const char* aMethodName) = 0;

    void ResetDemuxer()
    {
      // Clear demuxer related data.
      mDemuxRequest.DisconnectIfExists();
      mTrackDemuxer->Reset();
    }

    void ResetState()
    {
      MOZ_ASSERT(mOwner->OnTaskQueue());
      mDemuxEOS = false;
      mWaitingForData = false;
      mReceivedNewData = false;
      mDiscontinuity = true;
      mQueuedSamples.Clear();
      mDecodingRequested = false;
      mOutputRequested = false;
      mInputExhausted = false;
      mNeedDraining = false;
      mDraining = false;
      mDrainComplete = false;
      mTimeThreshold.reset();
      mOutput.Clear();
      mNumSamplesInput = 0;
      mNumSamplesOutput = 0;
      mSizeOfQueue = 0;
      mNextStreamSourceID.reset();
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
    nsRefPtr<SharedTrackInfo> mInfo;
  };

  template<typename PromiseType>
  struct DecoderDataWithPromise : public DecoderData {
    DecoderDataWithPromise(MediaFormatReader* aOwner,
                           MediaData::Type aType,
                           uint32_t aDecodeAhead) :
      DecoderData(aOwner, aType, aDecodeAhead)
    {}

    MozPromiseHolder<PromiseType> mPromise;

    bool HasPromise() override
    {
      MOZ_ASSERT(mOwner->OnTaskQueue());
      return !mPromise.IsEmpty();
    }

    void RejectPromise(MediaDecoderReader::NotDecodedReason aReason,
                       const char* aMethodName) override
    {
      MOZ_ASSERT(mOwner->OnTaskQueue());
      mPromise.Reject(aReason, aMethodName);
      mDecodingRequested = false;
    }
  };

  DecoderDataWithPromise<AudioDataPromise> mAudio;
  DecoderDataWithPromise<VideoDataPromise> mVideo;

  // Returns true when the decoder for this track needs input.
  bool NeedInput(DecoderData& aDecoder);

  DecoderData& GetDecoderData(TrackType aTrack);

  void OnDecoderInitDone(const nsTArray<TrackType>& aTrackTypes);
  void OnDecoderInitFailed(MediaDataDecoder::DecoderFailureReason aReason);

  // Demuxer objects.
  nsRefPtr<MediaDataDemuxer> mDemuxer;
  bool mDemuxerInitDone;
  void OnDemuxerInitDone(nsresult);
  void OnDemuxerInitFailed(DemuxerFailureReason aFailure);
  MozPromiseRequestHolder<MediaDataDemuxer::InitPromise> mDemuxerInitRequest;
  void OnDemuxFailed(TrackType aTrack, DemuxerFailureReason aFailure);

  void DoDemuxVideo();
  void OnVideoDemuxCompleted(nsRefPtr<MediaTrackDemuxer::SamplesHolder> aSamples);
  void OnVideoDemuxFailed(DemuxerFailureReason aFailure)
  {
    OnDemuxFailed(TrackType::kVideoTrack, aFailure);
  }

  void DoDemuxAudio();
  void OnAudioDemuxCompleted(nsRefPtr<MediaTrackDemuxer::SamplesHolder> aSamples);
  void OnAudioDemuxFailed(DemuxerFailureReason aFailure)
  {
    OnDemuxFailed(TrackType::kAudioTrack, aFailure);
  }

  void SkipVideoDemuxToNextKeyFrame(media::TimeUnit aTimeThreshold);
  MozPromiseRequestHolder<MediaTrackDemuxer::SkipAccessPointPromise> mSkipRequest;
  void OnVideoSkipCompleted(uint32_t aSkipped);
  void OnVideoSkipFailed(MediaTrackDemuxer::SkipFailureHolder aFailure);

  // The last number of decoded output frames that we've reported to
  // MediaDecoder::NotifyDecoded(). We diff the number of output video
  // frames every time that DecodeVideoData() is called, and report the
  // delta there.
  uint64_t mLastReportedNumDecodedFrames;

  layers::LayersBackend mLayersBackendType;

  // Metadata objects
  // True if we've read the streams' metadata.
  bool mInitDone;
  MozPromiseHolder<MetadataPromise> mMetadataPromise;
  // Accessed from multiple thread, in particular the MediaDecoderStateMachine,
  // however the value doesn't change after reading the metadata.
  bool mSeekable;
  bool IsEncrypted()
  {
    return mIsEncrypted;
  }
  bool mIsEncrypted;

  // Set to true if any of our track buffers may be blocking.
  bool mTrackDemuxersMayBlock;

  bool mHardwareAccelerationDisabled;

  // Seeking objects.
  bool IsSeeking() const { return mPendingSeekTime.isSome(); }
  void AttemptSeek();
  void OnSeekFailed(TrackType aTrack, DemuxerFailureReason aFailure);
  void DoVideoSeek();
  void OnVideoSeekCompleted(media::TimeUnit aTime);
  void OnVideoSeekFailed(DemuxerFailureReason aFailure)
  {
    OnSeekFailed(TrackType::kVideoTrack, aFailure);
  }

  void DoAudioSeek();
  void OnAudioSeekCompleted(media::TimeUnit aTime);
  void OnAudioSeekFailed(DemuxerFailureReason aFailure)
  {
    OnSeekFailed(TrackType::kAudioTrack, aFailure);
  }
  // Temporary seek information while we wait for the data
  Maybe<media::TimeUnit> mOriginalSeekTime;
  Maybe<media::TimeUnit> mPendingSeekTime;
  MozPromiseHolder<SeekPromise> mSeekPromise;

  // Pending decoders initialization.
  MozPromiseRequestHolder<MediaDataDecoder::InitPromise::AllPromiseType> mDecodersInitRequest;

#ifdef MOZ_EME
  nsRefPtr<CDMProxy> mCDMProxy;
#endif

#if defined(READER_DORMANT_HEURISTIC)
  const bool mDormantEnabled;
#endif
};

} // namespace mozilla

#endif
