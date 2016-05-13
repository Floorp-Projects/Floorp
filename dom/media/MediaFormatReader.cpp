/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/Preferences.h"
#include "mozilla/Telemetry.h"
#include "nsContentUtils.h"
#include "nsPrintfCString.h"
#include "nsSize.h"
#include "Layers.h"
#include "MediaData.h"
#include "MediaInfo.h"
#include "MediaFormatReader.h"
#include "MediaResource.h"
#include "mozilla/SharedThreadPool.h"
#include "VideoUtils.h"

#include <algorithm>

#ifdef MOZ_EME
#include "mozilla/CDMProxy.h"
#endif

using namespace mozilla::media;

using mozilla::layers::Image;
using mozilla::layers::LayerManager;
using mozilla::layers::LayersBackend;

static mozilla::LazyLogModule sFormatDecoderLog("MediaFormatReader");

#define LOG(arg, ...) MOZ_LOG(sFormatDecoderLog, mozilla::LogLevel::Debug, ("MediaFormatReader(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))
#define LOGV(arg, ...) MOZ_LOG(sFormatDecoderLog, mozilla::LogLevel::Verbose, ("MediaFormatReader(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))

namespace mozilla {

static const char*
TrackTypeToStr(TrackInfo::TrackType aTrack)
{
  MOZ_ASSERT(aTrack == TrackInfo::kAudioTrack ||
             aTrack == TrackInfo::kVideoTrack ||
             aTrack == TrackInfo::kTextTrack);
  switch (aTrack) {
  case TrackInfo::kAudioTrack:
    return "Audio";
  case TrackInfo::kVideoTrack:
    return "Video";
  case TrackInfo::kTextTrack:
    return "Text";
  default:
    return "Unknown";
  }
}

MediaFormatReader::MediaFormatReader(AbstractMediaDecoder* aDecoder,
                                     MediaDataDemuxer* aDemuxer,
                                     VideoFrameContainer* aVideoFrameContainer,
                                     layers::LayersBackend aLayersBackend)
  : MediaDecoderReader(aDecoder)
  , mAudio(this, MediaData::AUDIO_DATA, Preferences::GetUint("media.audio-decode-ahead", 2))
  , mVideo(this, MediaData::VIDEO_DATA, Preferences::GetUint("media.video-decode-ahead", 2))
  , mDemuxer(aDemuxer)
  , mDemuxerInitDone(false)
  , mLastReportedNumDecodedFrames(0)
  , mLayersBackendType(aLayersBackend)
  , mInitDone(false)
  , mIsEncrypted(false)
  , mTrackDemuxersMayBlock(false)
  , mDemuxOnly(false)
  , mSeekScheduled(false)
  , mVideoFrameContainer(aVideoFrameContainer)
{
  MOZ_ASSERT(aDemuxer);
  MOZ_COUNT_CTOR(MediaFormatReader);
}

MediaFormatReader::~MediaFormatReader()
{
  MOZ_COUNT_DTOR(MediaFormatReader);
}

RefPtr<ShutdownPromise>
MediaFormatReader::Shutdown()
{
  MOZ_ASSERT(OnTaskQueue());

  if (HasVideo()) {
    ReportDroppedFramesTelemetry();
  }

  mDemuxerInitRequest.DisconnectIfExists();
  mMetadataPromise.RejectIfExists(ReadMetadataFailureReason::METADATA_ERROR, __func__);
  mSeekPromise.RejectIfExists(NS_ERROR_FAILURE, __func__);
  mSkipRequest.DisconnectIfExists();

  if (mAudio.mDecoder) {
    Reset(TrackInfo::kAudioTrack);
    if (mAudio.HasPromise()) {
      mAudio.RejectPromise(CANCELED, __func__);
    }
    mAudio.mInitPromise.DisconnectIfExists();
    mAudio.ShutdownDecoder();
  }
  if (mAudio.mTrackDemuxer) {
    mAudio.ResetDemuxer();
    mAudio.mTrackDemuxer->BreakCycles();
    mAudio.mTrackDemuxer = nullptr;
  }
  if (mAudio.mTaskQueue) {
    mAudio.mTaskQueue->BeginShutdown();
    mAudio.mTaskQueue->AwaitShutdownAndIdle();
    mAudio.mTaskQueue = nullptr;
  }
  MOZ_ASSERT(!mAudio.HasPromise());

  if (mVideo.mDecoder) {
    Reset(TrackInfo::kVideoTrack);
    if (mVideo.HasPromise()) {
      mVideo.RejectPromise(CANCELED, __func__);
    }
    mVideo.mInitPromise.DisconnectIfExists();
    mVideo.ShutdownDecoder();
  }
  if (mVideo.mTrackDemuxer) {
    mVideo.ResetDemuxer();
    mVideo.mTrackDemuxer->BreakCycles();
    mVideo.mTrackDemuxer = nullptr;
  }
  if (mVideo.mTaskQueue) {
    mVideo.mTaskQueue->BeginShutdown();
    mVideo.mTaskQueue->AwaitShutdownAndIdle();
    mVideo.mTaskQueue = nullptr;
  }
  MOZ_ASSERT(!mVideo.HasPromise());

  mDemuxer = nullptr;

  mPlatform = nullptr;

  return MediaDecoderReader::Shutdown();
}

void
MediaFormatReader::InitLayersBackendType()
{
  // Extract the layer manager backend type so that platform decoders
  // can determine whether it's worthwhile using hardware accelerated
  // video decoding.
  if (!mDecoder) {
    return;
  }
  MediaDecoderOwner* owner = mDecoder->GetOwner();
  if (!owner) {
    NS_WARNING("MediaFormatReader without a decoder owner, can't get HWAccel");
    return;
  }

  dom::HTMLMediaElement* element = owner->GetMediaElement();
  NS_ENSURE_TRUE_VOID(element);

  RefPtr<LayerManager> layerManager =
    nsContentUtils::LayerManagerForDocument(element->OwnerDoc());
  NS_ENSURE_TRUE_VOID(layerManager);

  mLayersBackendType = layerManager->GetCompositorBackendType();
}

nsresult
MediaFormatReader::Init()
{
  MOZ_ASSERT(NS_IsMainThread(), "Must be on main thread.");
  PDMFactory::Init();

  InitLayersBackendType();

  mAudio.mTaskQueue =
    new FlushableTaskQueue(GetMediaThreadPool(MediaThreadType::PLATFORM_DECODER));
  mVideo.mTaskQueue =
    new FlushableTaskQueue(GetMediaThreadPool(MediaThreadType::PLATFORM_DECODER));

  return NS_OK;
}

#ifdef MOZ_EME
class DispatchKeyNeededEvent : public Runnable {
public:
  DispatchKeyNeededEvent(AbstractMediaDecoder* aDecoder,
                         nsTArray<uint8_t>& aInitData,
                         const nsString& aInitDataType)
    : mDecoder(aDecoder)
    , mInitData(aInitData)
    , mInitDataType(aInitDataType)
  {
  }
  NS_IMETHOD Run() {
    // Note: Null check the owner, as the decoder could have been shutdown
    // since this event was dispatched.
    MediaDecoderOwner* owner = mDecoder->GetOwner();
    if (owner) {
      owner->DispatchEncrypted(mInitData, mInitDataType);
    }
    mDecoder = nullptr;
    return NS_OK;
  }
private:
  RefPtr<AbstractMediaDecoder> mDecoder;
  nsTArray<uint8_t> mInitData;
  nsString mInitDataType;
};

void
MediaFormatReader::SetCDMProxy(CDMProxy* aProxy)
{
  RefPtr<CDMProxy> proxy = aProxy;
  RefPtr<MediaFormatReader> self = this;
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction([=] () {
    MOZ_ASSERT(self->OnTaskQueue());
    self->mCDMProxy = proxy;
  });
  OwnerThread()->Dispatch(r.forget());
}
#endif // MOZ_EME

bool
MediaFormatReader::IsWaitingOnCDMResource() {
  MOZ_ASSERT(OnTaskQueue());
#ifdef MOZ_EME
  return IsEncrypted() && !mCDMProxy;
#else
  return false;
#endif
}

RefPtr<MediaDecoderReader::MetadataPromise>
MediaFormatReader::AsyncReadMetadata()
{
  MOZ_ASSERT(OnTaskQueue());

  MOZ_DIAGNOSTIC_ASSERT(mMetadataPromise.IsEmpty());

  if (mInitDone) {
    // We are returning from dormant.
    RefPtr<MetadataHolder> metadata = new MetadataHolder();
    metadata->mInfo = mInfo;
    metadata->mTags = nullptr;
    return MetadataPromise::CreateAndResolve(metadata, __func__);
  }

  RefPtr<MetadataPromise> p = mMetadataPromise.Ensure(__func__);

  mDemuxerInitRequest.Begin(mDemuxer->Init()
                       ->Then(OwnerThread(), __func__, this,
                              &MediaFormatReader::OnDemuxerInitDone,
                              &MediaFormatReader::OnDemuxerInitFailed));
  return p;
}

void
MediaFormatReader::OnDemuxerInitDone(nsresult)
{
  MOZ_ASSERT(OnTaskQueue());
  mDemuxerInitRequest.Complete();

  mDemuxerInitDone = true;

  UniquePtr<MetadataTags> tags(MakeUnique<MetadataTags>());

  RefPtr<PDMFactory> platform;
  if (!IsWaitingOnCDMResource()) {
    platform = new PDMFactory();
  }

  // To decode, we need valid video and a place to put it.
  bool videoActive = !!mDemuxer->GetNumberTracks(TrackInfo::kVideoTrack) &&
    GetImageContainer();

  if (videoActive) {
    // We currently only handle the first video track.
    mVideo.mTrackDemuxer = mDemuxer->GetTrackDemuxer(TrackInfo::kVideoTrack, 0);
    if (!mVideo.mTrackDemuxer) {
      mMetadataPromise.Reject(ReadMetadataFailureReason::METADATA_ERROR, __func__);
      return;
    }

    UniquePtr<TrackInfo> videoInfo = mVideo.mTrackDemuxer->GetInfo();
    videoActive = videoInfo && videoInfo->IsValid();
    if (videoActive) {
      if (platform && !platform->SupportsMimeType(videoInfo->mMimeType, nullptr)) {
        // We have no decoder for this track. Error.
        mMetadataPromise.Reject(ReadMetadataFailureReason::METADATA_ERROR, __func__);
        return;
      }
      mInfo.mVideo = *videoInfo->GetAsVideoInfo();
      for (const MetadataTag& tag : videoInfo->mTags) {
        tags->Put(tag.mKey, tag.mValue);
      }
      mVideo.mCallback = new DecoderCallback(this, TrackInfo::kVideoTrack);
      mVideo.mTimeRanges = mVideo.mTrackDemuxer->GetBuffered();
      mTrackDemuxersMayBlock |= mVideo.mTrackDemuxer->GetSamplesMayBlock();
    } else {
      mVideo.mTrackDemuxer->BreakCycles();
      mVideo.mTrackDemuxer = nullptr;
    }
  }

  bool audioActive = !!mDemuxer->GetNumberTracks(TrackInfo::kAudioTrack);
  if (audioActive) {
    mAudio.mTrackDemuxer = mDemuxer->GetTrackDemuxer(TrackInfo::kAudioTrack, 0);
    if (!mAudio.mTrackDemuxer) {
      mMetadataPromise.Reject(ReadMetadataFailureReason::METADATA_ERROR, __func__);
      return;
    }

    UniquePtr<TrackInfo> audioInfo = mAudio.mTrackDemuxer->GetInfo();
    // We actively ignore audio tracks that we know we can't play.
    audioActive = audioInfo && audioInfo->IsValid() &&
                  (!platform ||
                   platform->SupportsMimeType(audioInfo->mMimeType, nullptr));

    if (audioActive) {
      mInfo.mAudio = *audioInfo->GetAsAudioInfo();
      for (const MetadataTag& tag : audioInfo->mTags) {
        tags->Put(tag.mKey, tag.mValue);
      }
      mAudio.mCallback = new DecoderCallback(this, TrackInfo::kAudioTrack);
      mAudio.mTimeRanges = mAudio.mTrackDemuxer->GetBuffered();
      mTrackDemuxersMayBlock |= mAudio.mTrackDemuxer->GetSamplesMayBlock();
    } else {
      mAudio.mTrackDemuxer->BreakCycles();
      mAudio.mTrackDemuxer = nullptr;
    }
  }

  UniquePtr<EncryptionInfo> crypto = mDemuxer->GetCrypto();

  mIsEncrypted = crypto && crypto->IsEncrypted();

  if (mDecoder && crypto && crypto->IsEncrypted()) {
#ifdef MOZ_EME
    // Try and dispatch 'encrypted'. Won't go if ready state still HAVE_NOTHING.
    for (uint32_t i = 0; i < crypto->mInitDatas.Length(); i++) {
      NS_DispatchToMainThread(
        new DispatchKeyNeededEvent(mDecoder, crypto->mInitDatas[i].mInitData, NS_LITERAL_STRING("cenc")));
    }
#endif // MOZ_EME
    mInfo.mCrypto = *crypto;
  }

  int64_t videoDuration = HasVideo() ? mInfo.mVideo.mDuration : 0;
  int64_t audioDuration = HasAudio() ? mInfo.mAudio.mDuration : 0;

  int64_t duration = std::max(videoDuration, audioDuration);
  if (duration != -1) {
    mInfo.mMetadataDuration = Some(TimeUnit::FromMicroseconds(duration));
  }

  mInfo.mMediaSeekable = mDemuxer->IsSeekable();
  mInfo.mMediaSeekableOnlyInBufferedRanges =
    mDemuxer->IsSeekableOnlyInBufferedRanges();

  if (!videoActive && !audioActive) {
    mMetadataPromise.Reject(ReadMetadataFailureReason::METADATA_ERROR, __func__);
    return;
  }

  mInitDone = true;
  RefPtr<MetadataHolder> metadata = new MetadataHolder();
  metadata->mInfo = mInfo;
  metadata->mTags = tags->Count() ? tags.release() : nullptr;
  mMetadataPromise.Resolve(metadata, __func__);
}

void
MediaFormatReader::OnDemuxerInitFailed(DemuxerFailureReason aFailure)
{
  mDemuxerInitRequest.Complete();
  mMetadataPromise.Reject(ReadMetadataFailureReason::METADATA_ERROR, __func__);
}

bool
MediaFormatReader::EnsureDecoderCreated(TrackType aTrack)
{
  MOZ_ASSERT(OnTaskQueue());

  auto& decoder = GetDecoderData(aTrack);

  if (decoder.mDecoder) {
    return true;
  }

  if (!mPlatform) {
    mPlatform = new PDMFactory();
    NS_ENSURE_TRUE(mPlatform, false);
    if (IsEncrypted()) {
#ifdef MOZ_EME
      MOZ_ASSERT(mCDMProxy);
      mPlatform->SetCDMProxy(mCDMProxy);
#else
      // EME not supported.
      return false;
#endif
    }
  }

  decoder.mDecoderInitialized = false;

  MonitorAutoLock mon(decoder.mMonitor);

  switch (aTrack) {
    case TrackType::kAudioTrack:
      decoder.mDecoder =
        mPlatform->CreateDecoder(decoder.mInfo ?
                                   *decoder.mInfo->GetAsAudioInfo() :
                                   mInfo.mAudio,
                                 decoder.mTaskQueue,
                                 decoder.mCallback,
                                 /* DecoderDoctorDiagnostics* */ nullptr);
      break;
    case TrackType::kVideoTrack:
      // Decoders use the layers backend to decide if they can use hardware decoding,
      // so specify LAYERS_NONE if we want to forcibly disable it.
      decoder.mDecoder =
        mPlatform->CreateDecoder(mVideo.mInfo ?
                                   *mVideo.mInfo->GetAsVideoInfo() :
                                   mInfo.mVideo,
                                 decoder.mTaskQueue,
                                 decoder.mCallback,
                                 /* DecoderDoctorDiagnostics* */ nullptr,
                                 mLayersBackendType,
                                 GetImageContainer());
      break;
    default:
      break;
  }
  if (decoder.mDecoder ) {
    decoder.mDescription = decoder.mDecoder->GetDescriptionName();
  } else {
    decoder.mDescription = "error creating decoder";
  }
  return decoder.mDecoder != nullptr;
}

bool
MediaFormatReader::EnsureDecoderInitialized(TrackType aTrack)
{
  MOZ_ASSERT(OnTaskQueue());
  auto& decoder = GetDecoderData(aTrack);

  if (!decoder.mDecoder || decoder.mInitPromise.Exists()) {
    MOZ_ASSERT(decoder.mDecoder);
    return false;
  }
  if (decoder.mDecoderInitialized) {
    return true;
  }
  RefPtr<MediaFormatReader> self = this;
  decoder.mInitPromise.Begin(decoder.mDecoder->Init()
       ->Then(OwnerThread(), __func__,
              [self] (TrackType aTrack) {
                auto& decoder = self->GetDecoderData(aTrack);
                decoder.mInitPromise.Complete();
                decoder.mDecoderInitialized = true;
                MonitorAutoLock mon(decoder.mMonitor);
                decoder.mDescription = decoder.mDecoder->GetDescriptionName();
                self->ScheduleUpdate(aTrack);
              },
              [self, aTrack] (MediaDataDecoder::DecoderFailureReason aResult) {
                auto& decoder = self->GetDecoderData(aTrack);
                decoder.mInitPromise.Complete();
                decoder.ShutdownDecoder();
                self->NotifyError(aTrack);
              }));
  return false;
}

void
MediaFormatReader::ReadUpdatedMetadata(MediaInfo* aInfo)
{
  *aInfo = mInfo;
}

MediaFormatReader::DecoderData&
MediaFormatReader::GetDecoderData(TrackType aTrack)
{
  MOZ_ASSERT(aTrack == TrackInfo::kAudioTrack ||
             aTrack == TrackInfo::kVideoTrack);
  if (aTrack == TrackInfo::kAudioTrack) {
    return mAudio;
  }
  return mVideo;
}

bool
MediaFormatReader::ShouldSkip(bool aSkipToNextKeyframe, media::TimeUnit aTimeThreshold)
{
  MOZ_ASSERT(HasVideo());
  media::TimeUnit nextKeyframe;
  nsresult rv = mVideo.mTrackDemuxer->GetNextRandomAccessPoint(&nextKeyframe);
  if (NS_FAILED(rv)) {
    return aSkipToNextKeyframe;
  }
  return (nextKeyframe < aTimeThreshold ||
          (mVideo.mTimeThreshold &&
           mVideo.mTimeThreshold.ref().mTime < aTimeThreshold)) &&
         nextKeyframe.ToMicroseconds() >= 0;
}

RefPtr<MediaDecoderReader::MediaDataPromise>
MediaFormatReader::RequestVideoData(bool aSkipToNextKeyframe,
                                    int64_t aTimeThreshold)
{
  MOZ_ASSERT(OnTaskQueue());
  MOZ_DIAGNOSTIC_ASSERT(mSeekPromise.IsEmpty(), "No sample requests allowed while seeking");
  MOZ_DIAGNOSTIC_ASSERT(!mVideo.HasPromise(), "No duplicate sample requests");
  MOZ_DIAGNOSTIC_ASSERT(!mVideo.mSeekRequest.Exists() ||
                        mVideo.mTimeThreshold.isSome());
  MOZ_DIAGNOSTIC_ASSERT(!mSkipRequest.Exists(), "called mid-skipping");
  MOZ_DIAGNOSTIC_ASSERT(!IsSeeking(), "called mid-seek");
  LOGV("RequestVideoData(%d, %lld)", aSkipToNextKeyframe, aTimeThreshold);

  if (!HasVideo()) {
    LOG("called with no video track");
    return MediaDataPromise::CreateAndReject(DECODE_ERROR, __func__);
  }

  if (IsSeeking()) {
    LOG("called mid-seek. Rejecting.");
    return MediaDataPromise::CreateAndReject(CANCELED, __func__);
  }

  if (mShutdown) {
    NS_WARNING("RequestVideoData on shutdown MediaFormatReader!");
    return MediaDataPromise::CreateAndReject(CANCELED, __func__);
  }

  media::TimeUnit timeThreshold{media::TimeUnit::FromMicroseconds(aTimeThreshold)};
  // Ensure we have no pending seek going as ShouldSkip could return out of date
  // information.
  if (!mVideo.HasInternalSeekPending() &&
      ShouldSkip(aSkipToNextKeyframe, timeThreshold)) {
    RefPtr<MediaDataPromise> p = mVideo.EnsurePromise(__func__);
    SkipVideoDemuxToNextKeyFrame(timeThreshold);
    return p;
  }

  RefPtr<MediaDataPromise> p = mVideo.EnsurePromise(__func__);
  NotifyDecodingRequested(TrackInfo::kVideoTrack);

  return p;
}

void
MediaFormatReader::OnDemuxFailed(TrackType aTrack, DemuxerFailureReason aFailure)
{
  MOZ_ASSERT(OnTaskQueue());
  LOG("Failed to demux %s, failure:%d",
      aTrack == TrackType::kVideoTrack ? "video" : "audio", aFailure);
  auto& decoder = GetDecoderData(aTrack);
  decoder.mDemuxRequest.Complete();
  switch (aFailure) {
    case DemuxerFailureReason::END_OF_STREAM:
      NotifyEndOfStream(aTrack);
      break;
    case DemuxerFailureReason::DEMUXER_ERROR:
      NotifyError(aTrack);
      break;
    case DemuxerFailureReason::WAITING_FOR_DATA:
      if (!decoder.mWaitingForData) {
        decoder.mNeedDraining = true;
      }
      NotifyWaitingForData(aTrack);
      break;
    case DemuxerFailureReason::CANCELED:
    case DemuxerFailureReason::SHUTDOWN:
      if (decoder.HasPromise()) {
        decoder.RejectPromise(CANCELED, __func__);
      }
      break;
    default:
      MOZ_ASSERT(false);
      break;
  }
}

void
MediaFormatReader::DoDemuxVideo()
{
  // TODO Use DecodeAhead value rather than 1.
  mVideo.mDemuxRequest.Begin(mVideo.mTrackDemuxer->GetSamples(1)
                      ->Then(OwnerThread(), __func__, this,
                             &MediaFormatReader::OnVideoDemuxCompleted,
                             &MediaFormatReader::OnVideoDemuxFailed));
}

void
MediaFormatReader::OnVideoDemuxCompleted(RefPtr<MediaTrackDemuxer::SamplesHolder> aSamples)
{
  LOGV("%d video samples demuxed (sid:%d)",
       aSamples->mSamples.Length(),
       aSamples->mSamples[0]->mTrackInfo ? aSamples->mSamples[0]->mTrackInfo->GetID() : 0);
  mVideo.mDemuxRequest.Complete();
  mVideo.mQueuedSamples.AppendElements(aSamples->mSamples);
  ScheduleUpdate(TrackInfo::kVideoTrack);
}

RefPtr<MediaDecoderReader::MediaDataPromise>
MediaFormatReader::RequestAudioData()
{
  MOZ_ASSERT(OnTaskQueue());
  MOZ_DIAGNOSTIC_ASSERT(mSeekPromise.IsEmpty(), "No sample requests allowed while seeking");
  MOZ_DIAGNOSTIC_ASSERT(!mAudio.mSeekRequest.Exists() ||
                        mAudio.mTimeThreshold.isSome());
  MOZ_DIAGNOSTIC_ASSERT(!mAudio.HasPromise(), "No duplicate sample requests");
  MOZ_DIAGNOSTIC_ASSERT(!IsSeeking(), "called mid-seek");
  LOGV("");

  if (!HasAudio()) {
    LOG("called with no audio track");
    return MediaDataPromise::CreateAndReject(DECODE_ERROR, __func__);
  }

  if (IsSeeking()) {
    LOG("called mid-seek. Rejecting.");
    return MediaDataPromise::CreateAndReject(CANCELED, __func__);
  }

  if (mShutdown) {
    NS_WARNING("RequestAudioData on shutdown MediaFormatReader!");
    return MediaDataPromise::CreateAndReject(CANCELED, __func__);
  }

  RefPtr<MediaDataPromise> p = mAudio.EnsurePromise(__func__);
  NotifyDecodingRequested(TrackInfo::kAudioTrack);

  return p;
}

void
MediaFormatReader::DoDemuxAudio()
{
  // TODO Use DecodeAhead value rather than 1.
  mAudio.mDemuxRequest.Begin(mAudio.mTrackDemuxer->GetSamples(1)
                      ->Then(OwnerThread(), __func__, this,
                             &MediaFormatReader::OnAudioDemuxCompleted,
                             &MediaFormatReader::OnAudioDemuxFailed));
}

void
MediaFormatReader::OnAudioDemuxCompleted(RefPtr<MediaTrackDemuxer::SamplesHolder> aSamples)
{
  LOGV("%d audio samples demuxed (sid:%d)",
       aSamples->mSamples.Length(),
       aSamples->mSamples[0]->mTrackInfo ? aSamples->mSamples[0]->mTrackInfo->GetID() : 0);
  mAudio.mDemuxRequest.Complete();
  mAudio.mQueuedSamples.AppendElements(aSamples->mSamples);
  ScheduleUpdate(TrackInfo::kAudioTrack);
}

void
MediaFormatReader::NotifyNewOutput(TrackType aTrack, MediaData* aSample)
{
  MOZ_ASSERT(OnTaskQueue());
  LOGV("Received new %s sample time:%lld duration:%lld",
       TrackTypeToStr(aTrack), aSample->mTime, aSample->mDuration);
  auto& decoder = GetDecoderData(aTrack);
  if (!decoder.mOutputRequested) {
    LOG("MediaFormatReader produced output while flushing, discarding.");
    return;
  }
  decoder.mOutput.AppendElement(aSample);
  decoder.mNumSamplesOutput++;
  decoder.mNumSamplesOutputTotal++;
  decoder.mNumSamplesOutputTotalSinceTelemetry++;
  ScheduleUpdate(aTrack);
}

void
MediaFormatReader::NotifyInputExhausted(TrackType aTrack)
{
  MOZ_ASSERT(OnTaskQueue());
  LOGV("Decoder has requested more %s data", TrackTypeToStr(aTrack));
  auto& decoder = GetDecoderData(aTrack);
  decoder.mInputExhausted = true;
  ScheduleUpdate(aTrack);
}

void
MediaFormatReader::NotifyDrainComplete(TrackType aTrack)
{
  MOZ_ASSERT(OnTaskQueue());
  auto& decoder = GetDecoderData(aTrack);
  LOG("%s", TrackTypeToStr(aTrack));
  if (!decoder.mOutputRequested) {
    LOG("MediaFormatReader called DrainComplete() before flushing, ignoring.");
    return;
  }
  decoder.mDrainComplete = true;
  ScheduleUpdate(aTrack);
}

void
MediaFormatReader::NotifyError(TrackType aTrack)
{
  MOZ_ASSERT(OnTaskQueue());
  LOGV("%s Decoding error", TrackTypeToStr(aTrack));
  auto& decoder = GetDecoderData(aTrack);
  decoder.mError = true;
  ScheduleUpdate(aTrack);
}

void
MediaFormatReader::NotifyWaitingForData(TrackType aTrack)
{
  MOZ_ASSERT(OnTaskQueue());
  auto& decoder = GetDecoderData(aTrack);
  decoder.mWaitingForData = true;
  if (decoder.mTimeThreshold) {
    decoder.mTimeThreshold.ref().mWaiting = true;
  }
  ScheduleUpdate(aTrack);
}

void
MediaFormatReader::NotifyEndOfStream(TrackType aTrack)
{
  MOZ_ASSERT(OnTaskQueue());
  auto& decoder = GetDecoderData(aTrack);
  decoder.mDemuxEOS = true;
  decoder.mNeedDraining = true;
  ScheduleUpdate(aTrack);
}

void
MediaFormatReader::NotifyDecodingRequested(TrackType aTrack)
{
  MOZ_ASSERT(OnTaskQueue());
  auto& decoder = GetDecoderData(aTrack);
  decoder.mDecodingRequested = true;
  ScheduleUpdate(aTrack);
}

bool
MediaFormatReader::NeedInput(DecoderData& aDecoder)
{
  // We try to keep a few more compressed samples input than decoded samples
  // have been output, provided the state machine has requested we send it a
  // decoded sample. To account for H.264 streams which may require a longer
  // run of input than we input, decoders fire an "input exhausted" callback,
  // which overrides our "few more samples" threshold.
  return
    !aDecoder.mDraining &&
    !aDecoder.mError &&
    aDecoder.mDecodingRequested &&
    !aDecoder.mDemuxRequest.Exists() &&
    !aDecoder.HasInternalSeekPending() &&
    aDecoder.mOutput.Length() <= aDecoder.mDecodeAhead &&
    (aDecoder.mInputExhausted || !aDecoder.mQueuedSamples.IsEmpty() ||
     aDecoder.mTimeThreshold.isSome() ||
     aDecoder.mNumSamplesInput - aDecoder.mNumSamplesOutput <= aDecoder.mDecodeAhead);
}

void
MediaFormatReader::ScheduleUpdate(TrackType aTrack)
{
  MOZ_ASSERT(OnTaskQueue());
  if (mShutdown) {
    return;
  }
  auto& decoder = GetDecoderData(aTrack);
  if (decoder.mUpdateScheduled) {
    return;
  }
  LOGV("SchedulingUpdate(%s)", TrackTypeToStr(aTrack));
  decoder.mUpdateScheduled = true;
  RefPtr<nsIRunnable> task(
    NewRunnableMethod<TrackType>(this, &MediaFormatReader::Update, aTrack));
  OwnerThread()->Dispatch(task.forget());
}

bool
MediaFormatReader::UpdateReceivedNewData(TrackType aTrack)
{
  MOZ_ASSERT(OnTaskQueue());
  auto& decoder = GetDecoderData(aTrack);

  if (!decoder.mReceivedNewData) {
    return false;
  }

  // Update our cached TimeRange.
  decoder.mTimeRanges = decoder.mTrackDemuxer->GetBuffered();

  // We do not want to clear mWaitingForData while there are pending
  // demuxing or seeking operations that could affect the value of this flag.
  // This is in order to ensure that we will retry once they complete as we may
  // now have new data that could potentially allow those operations to
  // successfully complete if tried again.
  if (decoder.mSeekRequest.Exists()) {
    // Nothing more to do until this operation complete.
    return true;
  }
  if (decoder.mDemuxRequest.Exists()) {
    // We may have pending operations to process, so we want to continue
    // after UpdateReceivedNewData returns.
    return false;
  }

  if (decoder.mDrainComplete || decoder.mDraining) {
    // We do not want to clear mWaitingForData or mDemuxEOS while
    // a drain is in progress in order to properly complete the operation.
    return false;
  }

  bool hasLastEnd;
  media::TimeUnit lastEnd = decoder.mTimeRanges.GetEnd(&hasLastEnd);
  if (hasLastEnd) {
    if (decoder.mLastTimeRangesEnd && decoder.mLastTimeRangesEnd.ref() < lastEnd) {
      // New data was added after our previous end, we can clear the EOS flag.
      decoder.mDemuxEOS = false;
    }
    decoder.mLastTimeRangesEnd = Some(lastEnd);
  }

  decoder.mReceivedNewData = false;
  if (decoder.mTimeThreshold) {
    decoder.mTimeThreshold.ref().mWaiting = false;
  }
  decoder.mWaitingForData = false;

  if (decoder.mError) {
    return false;
  }

  if (!mSeekPromise.IsEmpty()) {
    MOZ_ASSERT(!decoder.HasPromise());
    MOZ_DIAGNOSTIC_ASSERT(!mAudio.mTimeThreshold && !mVideo.mTimeThreshold,
                          "InternalSeek must have been aborted when Seek was first called");
    MOZ_DIAGNOSTIC_ASSERT(!mAudio.HasWaitingPromise() && !mVideo.HasWaitingPromise(),
                          "Waiting promises must have been rejected when Seek was first called");
    if (mVideo.mSeekRequest.Exists() || mAudio.mSeekRequest.Exists()) {
      // Already waiting for a seek to complete. Nothing more to do.
      return true;
    }
    LOG("Attempting Seek");
    ScheduleSeek();
    return true;
  }
  if (decoder.HasInternalSeekPending() || decoder.HasWaitingPromise()) {
    if (decoder.HasInternalSeekPending()) {
      LOG("Attempting Internal Seek");
      InternalSeek(aTrack, decoder.mTimeThreshold.ref());
    }
    if (decoder.HasWaitingPromise()) {
      MOZ_ASSERT(!decoder.HasPromise());
      LOG("We have new data. Resolving WaitingPromise");
      decoder.mWaitingPromise.Resolve(decoder.mType, __func__);
    }
    return true;
  }
  return false;
}

void
MediaFormatReader::RequestDemuxSamples(TrackType aTrack)
{
  MOZ_ASSERT(OnTaskQueue());
  auto& decoder = GetDecoderData(aTrack);
  MOZ_ASSERT(!decoder.mDemuxRequest.Exists());

  if (!decoder.mQueuedSamples.IsEmpty()) {
    // No need to demux new samples.
    return;
  }

  if (decoder.mDemuxEOS) {
    // Nothing left to demux.
    // We do not want to attempt to demux while in waiting for data mode
    // as it would retrigger an unecessary drain.
    return;
  }

  LOGV("Requesting extra demux %s", TrackTypeToStr(aTrack));
  if (aTrack == TrackInfo::kVideoTrack) {
    DoDemuxVideo();
  } else {
    DoDemuxAudio();
  }
}

bool
MediaFormatReader::DecodeDemuxedSamples(TrackType aTrack,
                                        MediaRawData* aSample)
{
  MOZ_ASSERT(OnTaskQueue());
  auto& decoder = GetDecoderData(aTrack);
  if (NS_FAILED(decoder.mDecoder->Input(aSample))) {
      LOG("Unable to pass frame to decoder");
      return false;
  }
  return true;
}

void
MediaFormatReader::HandleDemuxedSamples(TrackType aTrack,
                                        AbstractMediaDecoder::AutoNotifyDecoded& aA)
{
  MOZ_ASSERT(OnTaskQueue());
  auto& decoder = GetDecoderData(aTrack);

  if (decoder.mQueuedSamples.IsEmpty()) {
    return;
  }

  if (!EnsureDecoderCreated(aTrack)) {
    NS_WARNING("Error constructing decoders");
    NotifyError(aTrack);
    return;
  }

  if (!EnsureDecoderInitialized(aTrack)) {
    return;
  }

  LOGV("Giving %s input to decoder", TrackTypeToStr(aTrack));

  // Decode all our demuxed frames.
  bool samplesPending = false;
  while (decoder.mQueuedSamples.Length()) {
    RefPtr<MediaRawData> sample = decoder.mQueuedSamples[0];
    RefPtr<SharedTrackInfo> info = sample->mTrackInfo;

    if (info && decoder.mLastStreamSourceID != info->GetID()) {
      if (samplesPending) {
        // Let existing samples complete their decoding. We'll resume later.
        return;
      }

      if (decoder.mNextStreamSourceID.isNothing() ||
          decoder.mNextStreamSourceID.ref() != info->GetID()) {
        LOG("%s stream id has changed from:%d to:%d, draining decoder.",
            TrackTypeToStr(aTrack), decoder.mLastStreamSourceID,
            info->GetID());
        if (aTrack == TrackType::kVideoTrack) {
          ReportDroppedFramesTelemetry();
        }
        decoder.mNeedDraining = true;
        decoder.mNextStreamSourceID = Some(info->GetID());
        ScheduleUpdate(aTrack);
        return;
      }

      LOG("%s stream id has changed from:%d to:%d, recreating decoder.",
          TrackTypeToStr(aTrack), decoder.mLastStreamSourceID,
          info->GetID());
      decoder.mInfo = info;
      decoder.mLastStreamSourceID = info->GetID();
      decoder.mNextStreamSourceID.reset();
      // Reset will clear our array of queued samples. So make a copy now.
      nsTArray<RefPtr<MediaRawData>> samples{decoder.mQueuedSamples};
      Reset(aTrack);
      decoder.ShutdownDecoder();
      if (sample->mKeyframe) {
        decoder.mQueuedSamples.AppendElements(Move(samples));
        NotifyDecodingRequested(aTrack);
      } else {
        InternalSeekTarget seekTarget =
          decoder.mTimeThreshold.refOr(InternalSeekTarget(TimeUnit::FromMicroseconds(sample->mTime), false));
        LOG("Stream change occurred on a non-keyframe. Seeking to:%lld",
            seekTarget.mTime.ToMicroseconds());
        InternalSeek(aTrack, seekTarget);
      }
      return;
    }

    LOGV("Input:%lld (dts:%lld kf:%d)",
         sample->mTime, sample->mTimecode, sample->mKeyframe);
    decoder.mOutputRequested = true;
    decoder.mNumSamplesInput++;
    decoder.mSizeOfQueue++;
    if (aTrack == TrackInfo::kVideoTrack) {
      aA.mParsed++;
    }

    if (mDemuxOnly) {
      ReturnOutput(sample, aTrack);
    } else if (!DecodeDemuxedSamples(aTrack, sample)) {
      NotifyError(aTrack);
      return;
    }

    decoder.mQueuedSamples.RemoveElementAt(0);
    if (mDemuxOnly) {
      // If demuxed-only case, ReturnOutput will resolve with one demuxed data.
      // Then we should stop doing the iteration.
      return;
    }
    samplesPending = true;
  }

  // We have serviced the decoder's request for more data.
  decoder.mInputExhausted = false;
}

void
MediaFormatReader::InternalSeek(TrackType aTrack, const InternalSeekTarget& aTarget)
{
  MOZ_ASSERT(OnTaskQueue());
  LOG("%s internal seek to %f",
      TrackTypeToStr(aTrack), aTarget.mTime.ToSeconds());

  auto& decoder = GetDecoderData(aTrack);
  decoder.Flush();
  decoder.ResetDemuxer();
  decoder.mTimeThreshold = Some(aTarget);
  RefPtr<MediaFormatReader> self = this;
  decoder.mSeekRequest.Begin(decoder.mTrackDemuxer->Seek(decoder.mTimeThreshold.ref().mTime)
             ->Then(OwnerThread(), __func__,
                    [self, aTrack] (media::TimeUnit aTime) {
                      auto& decoder = self->GetDecoderData(aTrack);
                      decoder.mSeekRequest.Complete();
                      MOZ_ASSERT(decoder.mTimeThreshold,
                                 "Seek promise must be disconnected when timethreshold is reset");
                      decoder.mTimeThreshold.ref().mHasSeeked = true;
                      self->NotifyDecodingRequested(aTrack);
                    },
                    [self, aTrack] (DemuxerFailureReason aResult) {
                      auto& decoder = self->GetDecoderData(aTrack);
                      decoder.mSeekRequest.Complete();
                      switch (aResult) {
                        case DemuxerFailureReason::WAITING_FOR_DATA:
                          self->NotifyWaitingForData(aTrack);
                          break;
                        case DemuxerFailureReason::END_OF_STREAM:
                          decoder.mTimeThreshold.reset();
                          self->NotifyEndOfStream(aTrack);
                          break;
                        case DemuxerFailureReason::CANCELED:
                        case DemuxerFailureReason::SHUTDOWN:
                          decoder.mTimeThreshold.reset();
                          break;
                        default:
                          decoder.mTimeThreshold.reset();
                          self->NotifyError(aTrack);
                          break;
                      }
                    }));
}

void
MediaFormatReader::DrainDecoder(TrackType aTrack)
{
  MOZ_ASSERT(OnTaskQueue());

  auto& decoder = GetDecoderData(aTrack);
  if (!decoder.mNeedDraining || decoder.mDraining) {
    return;
  }
  decoder.mNeedDraining = false;
  // mOutputRequest must be set, otherwise NotifyDrainComplete()
  // may reject the drain if a Flush recently occurred.
  decoder.mOutputRequested = true;
  if (!decoder.mDecoder ||
      decoder.mNumSamplesInput == decoder.mNumSamplesOutput) {
    // No frames to drain.
    NotifyDrainComplete(aTrack);
    return;
  }
  decoder.mDecoder->Drain();
  decoder.mDraining = true;
  LOG("Requesting %s decoder to drain", TrackTypeToStr(aTrack));
}

void
MediaFormatReader::Update(TrackType aTrack)
{
  MOZ_ASSERT(OnTaskQueue());

  if (mShutdown) {
    return;
  }

  LOGV("Processing update for %s", TrackTypeToStr(aTrack));

  bool needOutput = false;
  auto& decoder = GetDecoderData(aTrack);
  decoder.mUpdateScheduled = false;

  if (!mInitDone) {
    return;
  }

  if (UpdateReceivedNewData(aTrack)) {
    LOGV("Nothing more to do");
    return;
  }

  if (decoder.mSeekRequest.Exists()) {
    LOGV("Seeking hasn't completed, nothing more to do");
    return;
  }

  MOZ_DIAGNOSTIC_ASSERT(!decoder.HasInternalSeekPending() ||
                        (!decoder.mOutput.Length() &&
                         !decoder.mQueuedSamples.Length()),
                        "No frames can be demuxed or decoded while an internal seek is pending");

  // Record number of frames decoded and parsed. Automatically update the
  // stats counters using the AutoNotifyDecoded stack-based class.
  AbstractMediaDecoder::AutoNotifyDecoded a(mDecoder);

  // Drop any frames found prior our internal seek target.
  while (decoder.mTimeThreshold && decoder.mOutput.Length()) {
    RefPtr<MediaData>& output = decoder.mOutput[0];
    InternalSeekTarget target = decoder.mTimeThreshold.ref();
    media::TimeUnit time = media::TimeUnit::FromMicroseconds(output->mTime);
    if (time >= target.mTime) {
      // We have reached our internal seek target.
      decoder.mTimeThreshold.reset();
    }
    if (time < target.mTime || (target.mDropTarget && time == target.mTime)) {
      LOGV("Internal Seeking: Dropping %s frame time:%f wanted:%f (kf:%d)",
           TrackTypeToStr(aTrack),
           media::TimeUnit::FromMicroseconds(output->mTime).ToSeconds(),
           target.mTime.ToSeconds(),
           output->mKeyframe);
      decoder.mOutput.RemoveElementAt(0);
    }
  }

  if (decoder.HasPromise()) {
    needOutput = true;
    if (decoder.mOutput.Length()) {
      // We have a decoded sample ready to be returned.
      if (aTrack == TrackType::kVideoTrack) {
        uint64_t delta =
          decoder.mNumSamplesOutputTotal - mLastReportedNumDecodedFrames;
        a.mDecoded = static_cast<uint32_t>(delta);
        mLastReportedNumDecodedFrames = decoder.mNumSamplesOutputTotal;
        nsCString error;
        mVideo.mIsHardwareAccelerated =
          mVideo.mDecoder && mVideo.mDecoder->IsHardwareAccelerated(error);
      }
      RefPtr<MediaData> output = decoder.mOutput[0];
      decoder.mOutput.RemoveElementAt(0);
      decoder.mSizeOfQueue -= 1;
      decoder.mLastSampleTime =
        Some(media::TimeUnit::FromMicroseconds(output->mTime));
      ReturnOutput(output, aTrack);
    } else if (decoder.mError) {
      LOG("Rejecting %s promise: DECODE_ERROR", TrackTypeToStr(aTrack));
      decoder.RejectPromise(DECODE_ERROR, __func__);
      return;
    } else if (decoder.mDrainComplete) {
      bool wasDraining = decoder.mDraining;
      decoder.mDrainComplete = false;
      decoder.mDraining = false;
      if (decoder.mDemuxEOS) {
        LOG("Rejecting %s promise: EOS", TrackTypeToStr(aTrack));
        decoder.RejectPromise(END_OF_STREAM, __func__);
      } else if (decoder.mWaitingForData) {
        if (wasDraining && decoder.mLastSampleTime &&
            !decoder.mNextStreamSourceID) {
          // We have completed draining the decoder following WaitingForData.
          // Set up the internal seek machinery to be able to resume from the
          // last sample decoded.
          LOG("Seeking to last sample time: %lld",
              decoder.mLastSampleTime.ref().ToMicroseconds());
          InternalSeek(aTrack, InternalSeekTarget(decoder.mLastSampleTime.ref(), true));
        }
        if (!decoder.mReceivedNewData) {
          LOG("Rejecting %s promise: WAITING_FOR_DATA", TrackTypeToStr(aTrack));
          decoder.RejectPromise(WAITING_FOR_DATA, __func__);
        }
      }
      // Now that draining has completed, we check if we have received
      // new data again as the result may now be different from the earlier
      // run.
      if (UpdateReceivedNewData(aTrack) || decoder.mSeekRequest.Exists()) {
        LOGV("Nothing more to do");
        return;
      }
    }
  }

  if (decoder.mNeedDraining) {
    DrainDecoder(aTrack);
    return;
  }

  bool needInput = NeedInput(decoder);

  LOGV("Update(%s) ni=%d no=%d ie=%d, in:%llu out:%llu qs=%u pending:%u waiting:%d ahead:%d sid:%u",
       TrackTypeToStr(aTrack), needInput, needOutput, decoder.mInputExhausted,
       decoder.mNumSamplesInput, decoder.mNumSamplesOutput,
       uint32_t(size_t(decoder.mSizeOfQueue)), uint32_t(decoder.mOutput.Length()),
       decoder.mWaitingForData, !decoder.HasPromise(), decoder.mLastStreamSourceID);

  if (decoder.mWaitingForData &&
      (!decoder.mTimeThreshold || decoder.mTimeThreshold.ref().mWaiting)) {
    // Nothing more we can do at present.
    LOGV("Still waiting for data.");
    return;
  }

  if (!needInput) {
    LOGV("No need for additional input (pending:%u)",
         uint32_t(decoder.mOutput.Length()));
    return;
  }

  // Demux samples if we don't have some.
  RequestDemuxSamples(aTrack);

  HandleDemuxedSamples(aTrack, a);
}

void
MediaFormatReader::ReturnOutput(MediaData* aData, TrackType aTrack)
{
  auto& decoder = GetDecoderData(aTrack);
  MOZ_ASSERT(decoder.HasPromise());
  if (decoder.mDiscontinuity) {
    LOGV("Setting discontinuity flag");
    decoder.mDiscontinuity = false;
    aData->mDiscontinuity = true;
  }

  LOG("Resolved data promise for %s [%lld, %lld]", TrackTypeToStr(aTrack),
      aData->mTime, aData->GetEndTime());

  if (aTrack == TrackInfo::kAudioTrack) {
    if (aData->mType != MediaData::RAW_DATA) {
      AudioData* audioData = static_cast<AudioData*>(aData);

      if (audioData->mChannels != mInfo.mAudio.mChannels ||
          audioData->mRate != mInfo.mAudio.mRate) {
        LOG("change of audio format (rate:%d->%d). "
            "This is an unsupported configuration",
            mInfo.mAudio.mRate, audioData->mRate);
        mInfo.mAudio.mRate = audioData->mRate;
        mInfo.mAudio.mChannels = audioData->mChannels;
      }
    }
    mAudio.ResolvePromise(aData, __func__);
  } else if (aTrack == TrackInfo::kVideoTrack) {
    if (aData->mType != MediaData::RAW_DATA) {
      VideoData* videoData = static_cast<VideoData*>(aData);

      if (videoData->mDisplay != mInfo.mVideo.mDisplay) {
        LOG("change of video display size (%dx%d->%dx%d)",
            mInfo.mVideo.mDisplay.width, mInfo.mVideo.mDisplay.height,
            videoData->mDisplay.width, videoData->mDisplay.height);
        mInfo.mVideo.mDisplay = videoData->mDisplay;
      }
    }
    mVideo.ResolvePromise(aData, __func__);
  }
}

size_t
MediaFormatReader::SizeOfVideoQueueInFrames()
{
  return SizeOfQueue(TrackInfo::kVideoTrack);
}

size_t
MediaFormatReader::SizeOfAudioQueueInFrames()
{
  return SizeOfQueue(TrackInfo::kAudioTrack);
}

size_t
MediaFormatReader::SizeOfQueue(TrackType aTrack)
{
  auto& decoder = GetDecoderData(aTrack);
  return decoder.mSizeOfQueue;
}

RefPtr<MediaDecoderReader::WaitForDataPromise>
MediaFormatReader::WaitForData(MediaData::Type aType)
{
  MOZ_ASSERT(OnTaskQueue());
  TrackType trackType = aType == MediaData::VIDEO_DATA ?
    TrackType::kVideoTrack : TrackType::kAudioTrack;
  auto& decoder = GetDecoderData(trackType);
  if (!decoder.mWaitingForData) {
    // We aren't waiting for data any longer.
    return WaitForDataPromise::CreateAndResolve(decoder.mType, __func__);
  }
  RefPtr<WaitForDataPromise> p = decoder.mWaitingPromise.Ensure(__func__);
  ScheduleUpdate(trackType);
  return p;
}

nsresult
MediaFormatReader::ResetDecode(TargetQueues aQueues)
{
  MOZ_ASSERT(OnTaskQueue());
  LOGV("");

  mSeekPromise.RejectIfExists(NS_OK, __func__);
  mSkipRequest.DisconnectIfExists();

  // Do the same for any data wait promises.
  mAudio.mWaitingPromise.RejectIfExists(WaitForDataRejectValue(MediaData::AUDIO_DATA, WaitForDataRejectValue::CANCELED), __func__);
  mVideo.mWaitingPromise.RejectIfExists(WaitForDataRejectValue(MediaData::VIDEO_DATA, WaitForDataRejectValue::CANCELED), __func__);

  // Reset miscellaneous seeking state.
  mPendingSeekTime.reset();

  ResetDemuxers();

  if (HasVideo()) {
    Reset(TrackInfo::kVideoTrack);
    if (mVideo.HasPromise()) {
      mVideo.RejectPromise(CANCELED, __func__);
    }
  }

  if (HasAudio() && aQueues == AUDIO_VIDEO) {
    Reset(TrackInfo::kAudioTrack);
    if (mAudio.HasPromise()) {
      mAudio.RejectPromise(CANCELED, __func__);
    }
  }
  return MediaDecoderReader::ResetDecode(aQueues);
}

void
MediaFormatReader::ResetDemuxers()
{
  if (HasVideo()) {
    mVideo.ResetDemuxer();
    mVideo.ResetState();
  }
  if (HasAudio()) {
    mAudio.ResetDemuxer();
    mAudio.ResetState();
  }
}

void
MediaFormatReader::Output(TrackType aTrack, MediaData* aSample)
{
  LOGV("Decoded %s sample time=%lld timecode=%lld kf=%d dur=%lld",
       TrackTypeToStr(aTrack), aSample->mTime, aSample->mTimecode,
       aSample->mKeyframe, aSample->mDuration);

  if (!aSample) {
    NS_WARNING("MediaFormatReader::Output() passed a null sample");
    Error(aTrack);
    return;
  }

  RefPtr<nsIRunnable> task =
    NewRunnableMethod<TrackType, MediaData*>(
      this, &MediaFormatReader::NotifyNewOutput, aTrack, aSample);
  OwnerThread()->Dispatch(task.forget());
}

void
MediaFormatReader::DrainComplete(TrackType aTrack)
{
  RefPtr<nsIRunnable> task =
    NewRunnableMethod<TrackType>(
      this, &MediaFormatReader::NotifyDrainComplete, aTrack);
  OwnerThread()->Dispatch(task.forget());
}

void
MediaFormatReader::InputExhausted(TrackType aTrack)
{
  RefPtr<nsIRunnable> task =
    NewRunnableMethod<TrackType>(
      this, &MediaFormatReader::NotifyInputExhausted, aTrack);
  OwnerThread()->Dispatch(task.forget());
}

void
MediaFormatReader::Error(TrackType aTrack)
{
  RefPtr<nsIRunnable> task =
    NewRunnableMethod<TrackType>(
      this, &MediaFormatReader::NotifyError, aTrack);
  OwnerThread()->Dispatch(task.forget());
}

void
MediaFormatReader::Reset(TrackType aTrack)
{
  MOZ_ASSERT(OnTaskQueue());
  LOG("Reset(%s) BEGIN", TrackTypeToStr(aTrack));

  auto& decoder = GetDecoderData(aTrack);

  decoder.ResetState();
  decoder.Flush();

  LOG("Reset(%s) END", TrackTypeToStr(aTrack));
}

void
MediaFormatReader::SkipVideoDemuxToNextKeyFrame(media::TimeUnit aTimeThreshold)
{
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(mVideo.HasPromise());
  LOG("Skipping up to %lld", aTimeThreshold.ToMicroseconds());

  // Cancel any pending demux request and pending demuxed samples.
  mVideo.mDemuxRequest.DisconnectIfExists();

  // I think it's still possible for an output to have been sent from the decoder
  // and is currently sitting in our event queue waiting to be processed. The following
  // flush won't clear it, and when we return to the event loop it'll be added to our
  // output queue and be used.
  // This code will count that as dropped, which was the intent, but not quite true.
  mDecoder->NotifyDecodedFrames(0, 0, SizeOfVideoQueueInFrames());

  if (mVideo.mTimeThreshold) {
    LOGV("Internal Seek pending, cancelling it");
  }
  Reset(TrackInfo::kVideoTrack);

  if (mVideo.mError) {
    // We have flushed the decoder, and we are in error state, we can
    // immediately reject the promise as there is nothing more to do.
    mVideo.RejectPromise(DECODE_ERROR, __func__);
    return;
  }

  mSkipRequest.Begin(mVideo.mTrackDemuxer->SkipToNextRandomAccessPoint(aTimeThreshold)
                          ->Then(OwnerThread(), __func__, this,
                                 &MediaFormatReader::OnVideoSkipCompleted,
                                 &MediaFormatReader::OnVideoSkipFailed));
  return;
}

void
MediaFormatReader::OnVideoSkipCompleted(uint32_t aSkipped)
{
  MOZ_ASSERT(OnTaskQueue());
  LOG("Skipping succeeded, skipped %u frames", aSkipped);
  mSkipRequest.Complete();
  if (mDecoder) {
    mDecoder->NotifyDecodedFrames(aSkipped, 0, aSkipped);
  }
  mVideo.mNumSamplesSkippedTotal += aSkipped;
  mVideo.mNumSamplesSkippedTotalSinceTelemetry += aSkipped;
  MOZ_ASSERT(!mVideo.mError); // We have flushed the decoder, no frame could
                              // have been decoded (and as such errored)
  NotifyDecodingRequested(TrackInfo::kVideoTrack);
}

void
MediaFormatReader::OnVideoSkipFailed(MediaTrackDemuxer::SkipFailureHolder aFailure)
{
  MOZ_ASSERT(OnTaskQueue());
  LOG("Skipping failed, skipped %u frames", aFailure.mSkipped);
  mSkipRequest.Complete();
  if (mDecoder) {
    mDecoder->NotifyDecodedFrames(aFailure.mSkipped, 0, aFailure.mSkipped);
  }
  MOZ_ASSERT(mVideo.HasPromise());
  switch (aFailure.mFailure) {
    case DemuxerFailureReason::END_OF_STREAM:
      NotifyEndOfStream(TrackType::kVideoTrack);
      break;
    case DemuxerFailureReason::WAITING_FOR_DATA:
      // While there is nothing to drain considering the decoder has been
      // flushed in SkipVideoDemuxToNextKeyFrame, we need to set mNeedDraining
      // to true as the video MediaDataPromise will only be rejected once drain
      // has completed.
      MOZ_DIAGNOSTIC_ASSERT(!mVideo.mDecodingRequested,
                            "Reset must have been called");
      if (!mVideo.mWaitingForData) {
        mVideo.mNeedDraining = true;
      }
      NotifyWaitingForData(TrackType::kVideoTrack);
      break;
    case DemuxerFailureReason::CANCELED:
    case DemuxerFailureReason::SHUTDOWN:
      if (mVideo.HasPromise()) {
        mVideo.RejectPromise(CANCELED, __func__);
      }
      break;
    default:
      NotifyError(TrackType::kVideoTrack);
      break;
  }
}

RefPtr<MediaDecoderReader::SeekPromise>
MediaFormatReader::Seek(SeekTarget aTarget, int64_t aUnused)
{
  MOZ_ASSERT(OnTaskQueue());

  LOG("aTarget=(%lld)", aTarget.GetTime().ToMicroseconds());

  MOZ_DIAGNOSTIC_ASSERT(mSeekPromise.IsEmpty());
  MOZ_DIAGNOSTIC_ASSERT(!mVideo.HasPromise());
  MOZ_DIAGNOSTIC_ASSERT(!mAudio.HasPromise());
  MOZ_DIAGNOSTIC_ASSERT(mPendingSeekTime.isNothing());
  MOZ_DIAGNOSTIC_ASSERT(mVideo.mTimeThreshold.isNothing());
  MOZ_DIAGNOSTIC_ASSERT(mAudio.mTimeThreshold.isNothing());

  if (!mInfo.mMediaSeekable && !mInfo.mMediaSeekableOnlyInBufferedRanges) {
    LOG("Seek() END (Unseekable)");
    return SeekPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  if (mShutdown) {
    return SeekPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  mOriginalSeekTarget = Some(aTarget);
  mPendingSeekTime = Some(aTarget.GetTime());

  RefPtr<SeekPromise> p = mSeekPromise.Ensure(__func__);

  ScheduleSeek();

  return p;
}

void
MediaFormatReader::ScheduleSeek()
{
  if (mSeekScheduled) {
    return;
  }
  mSeekScheduled = true;
  OwnerThread()->Dispatch(NewRunnableMethod(this, &MediaFormatReader::AttemptSeek));
}

void
MediaFormatReader::AttemptSeek()
{
  MOZ_ASSERT(OnTaskQueue());

  mSeekScheduled = false;

  if (mPendingSeekTime.isNothing()) {
    return;
  }
  ResetDemuxers();
  if (HasVideo()) {
    DoVideoSeek();
  } else if (HasAudio()) {
    DoAudioSeek();
  } else {
    MOZ_CRASH();
  }
}

void
MediaFormatReader::OnSeekFailed(TrackType aTrack, DemuxerFailureReason aResult)
{
  MOZ_ASSERT(OnTaskQueue());
  LOGV("%s failure:%d", TrackTypeToStr(aTrack), aResult);
  if (aTrack == TrackType::kVideoTrack) {
    mVideo.mSeekRequest.Complete();
  } else {
    mAudio.mSeekRequest.Complete();
  }

  if (aResult == DemuxerFailureReason::WAITING_FOR_DATA) {
    if (HasVideo() && aTrack == TrackType::kAudioTrack &&
        mOriginalSeekTarget.isSome() &&
        mPendingSeekTime.ref() != mOriginalSeekTarget.ref().GetTime()) {
      // We have failed to seek audio where video seeked to earlier.
      // Attempt to seek instead to the closest point that we know we have in
      // order to limit A/V sync discrepency.

      // Ensure we have the most up to date buffered ranges.
      UpdateReceivedNewData(TrackType::kAudioTrack);
      Maybe<media::TimeUnit> nextSeekTime;
      // Find closest buffered time found after video seeked time.
      for (const auto& timeRange : mAudio.mTimeRanges) {
        if (timeRange.mStart >= mPendingSeekTime.ref()) {
          nextSeekTime.emplace(timeRange.mStart);
          break;
        }
      }
      if (nextSeekTime.isNothing() ||
          nextSeekTime.ref() > mOriginalSeekTarget.ref().GetTime()) {
        nextSeekTime = Some(mOriginalSeekTarget.ref().GetTime());
        LOG("Unable to seek audio to video seek time. A/V sync may be broken");
      } else {
        mOriginalSeekTarget.reset();
      }
      mPendingSeekTime = nextSeekTime;
      DoAudioSeek();
      return;
    }
    NotifyWaitingForData(aTrack);
    return;
  }
  MOZ_ASSERT(!mVideo.mSeekRequest.Exists() && !mAudio.mSeekRequest.Exists());
  mPendingSeekTime.reset();
  mSeekPromise.Reject(NS_ERROR_FAILURE, __func__);
}

void
MediaFormatReader::DoVideoSeek()
{
  MOZ_ASSERT(mPendingSeekTime.isSome());
  LOGV("Seeking video to %lld", mPendingSeekTime.ref().ToMicroseconds());
  media::TimeUnit seekTime = mPendingSeekTime.ref();
  mVideo.mSeekRequest.Begin(mVideo.mTrackDemuxer->Seek(seekTime)
                          ->Then(OwnerThread(), __func__, this,
                                 &MediaFormatReader::OnVideoSeekCompleted,
                                 &MediaFormatReader::OnVideoSeekFailed));
}

void
MediaFormatReader::OnVideoSeekCompleted(media::TimeUnit aTime)
{
  MOZ_ASSERT(OnTaskQueue());
  LOGV("Video seeked to %lld", aTime.ToMicroseconds());
  mVideo.mSeekRequest.Complete();

  MOZ_ASSERT(mOriginalSeekTarget.isSome());
  if (HasAudio() && !mOriginalSeekTarget->IsVideoOnly()) {
    MOZ_ASSERT(mPendingSeekTime.isSome());
    if (mOriginalSeekTarget->IsFast()) {
      // We are performing a fast seek. We need to seek audio to where the
      // video seeked to, to ensure proper A/V sync once playback resume.
      mPendingSeekTime = Some(aTime);
    }
    DoAudioSeek();
  } else {
    mPendingSeekTime.reset();
    mSeekPromise.Resolve(aTime, __func__);
  }
}

void
MediaFormatReader::DoAudioSeek()
{
  MOZ_ASSERT(mPendingSeekTime.isSome());
  LOGV("Seeking audio to %lld", mPendingSeekTime.ref().ToMicroseconds());
  media::TimeUnit seekTime = mPendingSeekTime.ref();
  mAudio.mSeekRequest.Begin(mAudio.mTrackDemuxer->Seek(seekTime)
                         ->Then(OwnerThread(), __func__, this,
                                &MediaFormatReader::OnAudioSeekCompleted,
                                &MediaFormatReader::OnAudioSeekFailed));
}

void
MediaFormatReader::OnAudioSeekCompleted(media::TimeUnit aTime)
{
  MOZ_ASSERT(OnTaskQueue());
  LOGV("Audio seeked to %lld", aTime.ToMicroseconds());
  mAudio.mSeekRequest.Complete();
  mPendingSeekTime.reset();
  mSeekPromise.Resolve(aTime, __func__);
}

media::TimeIntervals
MediaFormatReader::GetBuffered()
{
  MOZ_ASSERT(OnTaskQueue());
  media::TimeIntervals videoti;
  media::TimeIntervals audioti;
  media::TimeIntervals intervals;

  if (!mInitDone) {
    return intervals;
  }
  int64_t startTime = 0;
  if (!ForceZeroStartTime()) {
    if (!HaveStartTime()) {
      return intervals;
    }
    startTime = StartTime();
  }
  // Ensure we have up to date buffered time range.
  if (HasVideo()) {
    UpdateReceivedNewData(TrackType::kVideoTrack);
  }
  if (HasAudio()) {
    UpdateReceivedNewData(TrackType::kAudioTrack);
  }
  if (HasVideo()) {
    videoti = mVideo.mTimeRanges;
  }
  if (HasAudio()) {
    audioti = mAudio.mTimeRanges;
  }
  if (HasAudio() && HasVideo()) {
    intervals = media::Intersection(Move(videoti), Move(audioti));
  } else if (HasAudio()) {
    intervals = Move(audioti);
  } else if (HasVideo()) {
    intervals = Move(videoti);
  }

  if (!intervals.Length() ||
      intervals.GetStart() == media::TimeUnit::FromMicroseconds(0)) {
    // IntervalSet already starts at 0 or is empty, nothing to shift.
    return intervals;
  }
  return intervals.Shift(media::TimeUnit::FromMicroseconds(-startTime));
}

// For the MediaFormatReader override we need to force an update to the
// buffered ranges, so we call NotifyDataArrive
RefPtr<MediaDecoderReader::BufferedUpdatePromise>
MediaFormatReader::UpdateBufferedWithPromise() {
  MOZ_ASSERT(OnTaskQueue());
  // Call NotifyDataArrive to force a recalculation of the buffered
  // ranges. UpdateBuffered alone will not force a recalculation, so we
  // use NotifyDataArrived which sets flags to force this recalculation.
  // See MediaFormatReader::UpdateReceivedNewData for an example of where
  // the new data flag is used.
  NotifyDataArrived();
  return BufferedUpdatePromise::CreateAndResolve(true, __func__);
}

void MediaFormatReader::ReleaseMediaResources()
{
  // Before freeing a video codec, all video buffers needed to be released
  // even from graphics pipeline.
  if (mVideoFrameContainer) {
    mVideoFrameContainer->ClearCurrentFrame();
  }
  mVideo.mInitPromise.DisconnectIfExists();
  mVideo.ShutdownDecoder();
}

bool
MediaFormatReader::VideoIsHardwareAccelerated() const
{
  return mVideo.mIsHardwareAccelerated;
}

void
MediaFormatReader::NotifyDemuxer()
{
  MOZ_ASSERT(OnTaskQueue());

  if (mShutdown || !mDemuxer ||
      (!mDemuxerInitDone && !mDemuxerInitRequest.Exists())) {
    return;
  }

  LOGV("");

  mDemuxer->NotifyDataArrived();

  if (!mInitDone) {
    return;
  }
  if (HasVideo()) {
    mVideo.mReceivedNewData = true;
    ScheduleUpdate(TrackType::kVideoTrack);
  }
  if (HasAudio()) {
    mAudio.mReceivedNewData = true;
    ScheduleUpdate(TrackType::kAudioTrack);
  }
}

void
MediaFormatReader::NotifyDataArrivedInternal()
{
  MOZ_ASSERT(OnTaskQueue());
  NotifyDemuxer();
}

bool
MediaFormatReader::ForceZeroStartTime() const
{
  return !mDemuxer->ShouldComputeStartTime();
}

layers::ImageContainer*
MediaFormatReader::GetImageContainer()
{
  return mVideoFrameContainer
    ? mVideoFrameContainer->GetImageContainer() : nullptr;
}

void
MediaFormatReader::GetMozDebugReaderData(nsAString& aString)
{
  nsAutoCString result;
  const char* audioName = "unavailable";
  const char* videoName = audioName;

  if (HasAudio()) {
    MonitorAutoLock mon(mAudio.mMonitor);
    audioName = mAudio.mDescription;
  }
  if (HasVideo()) {
    MonitorAutoLock mon(mVideo.mMonitor);
    videoName = mVideo.mDescription;
  }

  result += nsPrintfCString("audio decoder: %s\n", audioName);
  result += nsPrintfCString("audio frames decoded: %lld\n",
                            mAudio.mNumSamplesOutputTotal);
  if (HasAudio()) {
    result += nsPrintfCString("audio state: ni=%d no=%d ie=%d demuxr:%d demuxq:%d decoder:%d tt:%f tths:%d in:%llu out:%llu qs=%u pending:%u waiting:%d sid:%u\n",
                              NeedInput(mAudio), mAudio.HasPromise(),
                              mAudio.mInputExhausted,
                              mAudio.mDemuxRequest.Exists(),
                              int(mAudio.mQueuedSamples.Length()),
                              mAudio.mDecodingRequested,
                              mAudio.mTimeThreshold
                              ? mAudio.mTimeThreshold.ref().mTime.ToSeconds()
                              : -1.0,
                              mAudio.mTimeThreshold
                              ? mAudio.mTimeThreshold.ref().mHasSeeked
                              : -1,
                              mAudio.mNumSamplesInput, mAudio.mNumSamplesOutput,
                              unsigned(size_t(mAudio.mSizeOfQueue)),
                              unsigned(mAudio.mOutput.Length()),
                              mAudio.mWaitingForData, mAudio.mLastStreamSourceID);
  }
  result += nsPrintfCString("video decoder: %s\n", videoName);
  result += nsPrintfCString("hardware video decoding: %s\n",
                            VideoIsHardwareAccelerated() ? "enabled" : "disabled");
  result += nsPrintfCString("video frames decoded: %lld (skipped:%lld)\n",
                            mVideo.mNumSamplesOutputTotal,
                            mVideo.mNumSamplesSkippedTotal);
  if (HasVideo()) {
    result += nsPrintfCString("video state: ni=%d no=%d ie=%d demuxr:%d demuxq:%d decoder:%d tt:%f tths:%d in:%llu out:%llu qs=%u pending:%u waiting:%d sid:%u\n",
                              NeedInput(mVideo), mVideo.HasPromise(),
                              mVideo.mInputExhausted,
                              mVideo.mDemuxRequest.Exists(),
                              int(mVideo.mQueuedSamples.Length()),
                              mVideo.mDecodingRequested,
                              mVideo.mTimeThreshold
                              ? mVideo.mTimeThreshold.ref().mTime.ToSeconds()
                              : -1.0,
                              mVideo.mTimeThreshold
                              ? mVideo.mTimeThreshold.ref().mHasSeeked
                              : -1,
                              mVideo.mNumSamplesInput, mVideo.mNumSamplesOutput,
                              unsigned(size_t(mVideo.mSizeOfQueue)),
                              unsigned(mVideo.mOutput.Length()),
                              mVideo.mWaitingForData, mVideo.mLastStreamSourceID);
  }
  aString += NS_ConvertUTF8toUTF16(result);
}

void
MediaFormatReader::ReportDroppedFramesTelemetry()
{
  MOZ_ASSERT(OnTaskQueue());

  const VideoInfo* info =
    mVideo.mInfo ? mVideo.mInfo->GetAsVideoInfo() : &mInfo.mVideo;

  if (!info || !mVideo.mDecoder) {
    return;
  }

  nsCString keyPhrase = nsCString("MimeType=");
  keyPhrase.Append(info->mMimeType);
  keyPhrase.Append("; ");

  keyPhrase.Append("Resolution=");
  keyPhrase.AppendInt(info->mDisplay.width);
  keyPhrase.Append('x');
  keyPhrase.AppendInt(info->mDisplay.height);
  keyPhrase.Append("; ");

  keyPhrase.Append("HardwareAcceleration=");
  if (VideoIsHardwareAccelerated()) {
    keyPhrase.Append(mVideo.mDecoder->GetDescriptionName());
    keyPhrase.Append("enabled");
  } else {
    keyPhrase.Append("disabled");
  }

  if (mVideo.mNumSamplesOutputTotalSinceTelemetry) {
    uint32_t percentage =
      100 * mVideo.mNumSamplesSkippedTotalSinceTelemetry /
            mVideo.mNumSamplesOutputTotalSinceTelemetry;
    nsCOMPtr<nsIRunnable> task = NS_NewRunnableFunction([=]() -> void {
      LOG("Reporting telemetry DROPPED_FRAMES_IN_VIDEO_PLAYBACK");
      Telemetry::Accumulate(Telemetry::VIDEO_DETAILED_DROPPED_FRAMES_PROPORTION,
                            keyPhrase,
                            percentage);
    });
    AbstractThread::MainThread()->Dispatch(task.forget());
  }

  mVideo.mNumSamplesSkippedTotalSinceTelemetry = 0;
  mVideo.mNumSamplesOutputTotalSinceTelemetry = 0;
}

} // namespace mozilla
