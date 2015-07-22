/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/Preferences.h"
#include "nsPrintfCString.h"
#include "nsSize.h"
#include "ImageContainer.h"
#include "Layers.h"
#include "MediaData.h"
#include "MediaInfo.h"
#include "MediaFormatReader.h"
#include "MediaResource.h"
#include "SharedDecoderManager.h"
#include "SharedThreadPool.h"
#include "VideoUtils.h"

#include <algorithm>

#ifdef MOZ_EME
#include "mozilla/CDMProxy.h"
#endif

using namespace mozilla::media;

using mozilla::layers::Image;
using mozilla::layers::LayerManager;
using mozilla::layers::LayersBackend;

PRLogModuleInfo* GetFormatDecoderLog() {
  static PRLogModuleInfo* log = nullptr;
  if (!log) {
    log = PR_NewLogModule("MediaFormatReader");
  }
  return log;
}
#define LOG(arg, ...) MOZ_LOG(GetFormatDecoderLog(), mozilla::LogLevel::Debug, ("MediaFormatReader(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))
#define LOGV(arg, ...) MOZ_LOG(GetFormatDecoderLog(), mozilla::LogLevel::Verbose, ("MediaFormatReader(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))

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
                                     TaskQueue* aBorrowedTaskQueue)
  : MediaDecoderReader(aDecoder, aBorrowedTaskQueue)
  , mDemuxer(aDemuxer)
  , mAudio(this, MediaData::AUDIO_DATA, Preferences::GetUint("media.audio-decode-ahead", 2))
  , mVideo(this, MediaData::VIDEO_DATA, Preferences::GetUint("media.video-decode-ahead", 2))
  , mLastReportedNumDecodedFrames(0)
  , mLayersBackendType(layers::LayersBackend::LAYERS_NONE)
  , mInitDone(false)
  , mSeekable(false)
  , mIsEncrypted(false)
  , mTrackDemuxersMayBlock(false)
{
  MOZ_ASSERT(aDemuxer);
  MOZ_COUNT_CTOR(MediaFormatReader);
}

MediaFormatReader::~MediaFormatReader()
{
  MOZ_COUNT_DTOR(MediaFormatReader);
}

nsRefPtr<ShutdownPromise>
MediaFormatReader::Shutdown()
{
  MOZ_ASSERT(OnTaskQueue());

  mDemuxerInitRequest.DisconnectIfExists();
  mMetadataPromise.RejectIfExists(ReadMetadataFailureReason::METADATA_ERROR, __func__);
  mSeekPromise.RejectIfExists(NS_ERROR_FAILURE, __func__);
  mSkipRequest.DisconnectIfExists();

  if (mAudio.mDecoder) {
    Flush(TrackInfo::kAudioTrack);
    if (mAudio.HasPromise()) {
      mAudio.RejectPromise(CANCELED, __func__);
    }
    mAudio.mDecoder->Shutdown();
    mAudio.mDecoder = nullptr;
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
  MOZ_ASSERT(mAudio.mPromise.IsEmpty());

  if (mVideo.mDecoder) {
    Flush(TrackInfo::kVideoTrack);
    if (mVideo.HasPromise()) {
      mVideo.RejectPromise(CANCELED, __func__);
    }
    mVideo.mDecoder->Shutdown();
    mVideo.mDecoder = nullptr;
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
  MOZ_ASSERT(mVideo.mPromise.IsEmpty());

  mDemuxer = nullptr;

  // shutdown main thread demuxer and track demuxers.
  if (mAudioTrackDemuxer) {
    mAudioTrackDemuxer->BreakCycles();
    mAudioTrackDemuxer = nullptr;
  }
  if (mVideoTrackDemuxer) {
    mVideoTrackDemuxer->BreakCycles();
    mVideoTrackDemuxer = nullptr;
  }
  mMainThreadDemuxer = nullptr;

  mPlatform = nullptr;

  return MediaDecoderReader::Shutdown();
}

void
MediaFormatReader::InitLayersBackendType()
{
  if (!IsVideoContentType(mDecoder->GetResource()->GetContentType())) {
    // Not playing video, we don't care about the layers backend type.
    return;
  }
  // Extract the layer manager backend type so that platform decoders
  // can determine whether it's worthwhile using hardware accelerated
  // video decoding.
  MediaDecoderOwner* owner = mDecoder->GetOwner();
  if (!owner) {
    NS_WARNING("MediaFormatReader without a decoder owner, can't get HWAccel");
    return;
  }

  dom::HTMLMediaElement* element = owner->GetMediaElement();
  NS_ENSURE_TRUE_VOID(element);

  nsRefPtr<LayerManager> layerManager =
    nsContentUtils::LayerManagerForDocument(element->OwnerDoc());
  NS_ENSURE_TRUE_VOID(layerManager);

  mLayersBackendType = layerManager->GetCompositorBackendType();
}

static bool sIsEMEEnabled = false;

nsresult
MediaFormatReader::Init(MediaDecoderReader* aCloneDonor)
{
  MOZ_ASSERT(NS_IsMainThread(), "Must be on main thread.");
  PlatformDecoderModule::Init();

  InitLayersBackendType();

  mAudio.mTaskQueue =
    new FlushableTaskQueue(GetMediaThreadPool(MediaThreadType::PLATFORM_DECODER));
  mVideo.mTaskQueue =
    new FlushableTaskQueue(GetMediaThreadPool(MediaThreadType::PLATFORM_DECODER));

  static bool sSetupPrefCache = false;
  if (!sSetupPrefCache) {
    sSetupPrefCache = true;
    Preferences::AddBoolVarCache(&sIsEMEEnabled, "media.eme.enabled", false);
  }

  return NS_OK;
}

#ifdef MOZ_EME
class DispatchKeyNeededEvent : public nsRunnable {
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
  nsRefPtr<AbstractMediaDecoder> mDecoder;
  nsTArray<uint8_t> mInitData;
  nsString mInitDataType;
};
#endif // MOZ_EME

bool MediaFormatReader::IsWaitingOnCDMResource() {
#ifdef MOZ_EME
  nsRefPtr<CDMProxy> proxy;
  {
    if (!IsEncrypted()) {
      // Not encrypted, no need to wait for CDMProxy.
      return false;
    }
    ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
    proxy = mDecoder->GetCDMProxy();
    if (!proxy) {
      // We're encrypted, we need a CDMProxy to decrypt file.
      return true;
    }
  }
  // We'll keep waiting if the CDM hasn't informed Gecko of its capabilities.
  {
    CDMCaps::AutoLock caps(proxy->Capabilites());
    LOG("capsKnown=%d", caps.AreCapsKnown());
    return !caps.AreCapsKnown();
  }
#else
  return false;
#endif
}

bool
MediaFormatReader::IsSupportedAudioMimeType(const nsACString& aMimeType)
{
  return mPlatform && (mPlatform->SupportsMimeType(aMimeType) ||
    PlatformDecoderModule::AgnosticMimeType(aMimeType));
}

bool
MediaFormatReader::IsSupportedVideoMimeType(const nsACString& aMimeType)
{
  return mPlatform && (mPlatform->SupportsMimeType(aMimeType) ||
    PlatformDecoderModule::AgnosticMimeType(aMimeType));
}

nsRefPtr<MediaDecoderReader::MetadataPromise>
MediaFormatReader::AsyncReadMetadata()
{
  MOZ_ASSERT(OnTaskQueue());

  if (mInitDone) {
    // We are returning from dormant.
    if (!EnsureDecodersSetup()) {
      return MetadataPromise::CreateAndReject(ReadMetadataFailureReason::METADATA_ERROR, __func__);
    }
    nsRefPtr<MetadataHolder> metadata = new MetadataHolder();
    metadata->mInfo = mInfo;
    metadata->mTags = nullptr;
    return MetadataPromise::CreateAndResolve(metadata, __func__);
  }

  nsRefPtr<MetadataPromise> p = mMetadataPromise.Ensure(__func__);

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

  // To decode, we need valid video and a place to put it.
  bool videoActive = !!mDemuxer->GetNumberTracks(TrackInfo::kVideoTrack) &&
    mDecoder->GetImageContainer();

  if (videoActive) {
    // We currently only handle the first video track.
    mVideo.mTrackDemuxer = mDemuxer->GetTrackDemuxer(TrackInfo::kVideoTrack, 0);
    MOZ_ASSERT(mVideo.mTrackDemuxer);
    mInfo.mVideo = *mVideo.mTrackDemuxer->GetInfo()->GetAsVideoInfo();
    mVideo.mCallback = new DecoderCallback(this, TrackInfo::kVideoTrack);
    mVideo.mTimeRanges = mVideo.mTrackDemuxer->GetBuffered();
    mTrackDemuxersMayBlock |= mVideo.mTrackDemuxer->GetSamplesMayBlock();
  }

  bool audioActive = !!mDemuxer->GetNumberTracks(TrackInfo::kAudioTrack);
  if (audioActive) {
    mAudio.mTrackDemuxer = mDemuxer->GetTrackDemuxer(TrackInfo::kAudioTrack, 0);
    MOZ_ASSERT(mAudio.mTrackDemuxer);
    mInfo.mAudio = *mAudio.mTrackDemuxer->GetInfo()->GetAsAudioInfo();
    mAudio.mCallback = new DecoderCallback(this, TrackInfo::kAudioTrack);
    mAudio.mTimeRanges = mAudio.mTrackDemuxer->GetBuffered();
    mTrackDemuxersMayBlock |= mAudio.mTrackDemuxer->GetSamplesMayBlock();
  }

  UniquePtr<EncryptionInfo> crypto = mDemuxer->GetCrypto();

  mIsEncrypted = crypto && crypto->IsEncrypted();

  if (crypto && crypto->IsEncrypted()) {
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

  mSeekable = mDemuxer->IsSeekable();

  // Create demuxer object for main thread.
  if (mDemuxer->IsThreadSafe()) {
    mMainThreadDemuxer = mDemuxer;
  } else {
    mMainThreadDemuxer = mDemuxer->Clone();
  }
  if (!mMainThreadDemuxer) {
    mMetadataPromise.Reject(ReadMetadataFailureReason::METADATA_ERROR, __func__);
    NS_WARNING("Unable to clone current MediaDataDemuxer");
    return;
  }

  if (!videoActive && !audioActive) {
    mMetadataPromise.Reject(ReadMetadataFailureReason::METADATA_ERROR, __func__);
    return;
  }
  if (videoActive) {
    mVideoTrackDemuxer =
      mMainThreadDemuxer->GetTrackDemuxer(TrackInfo::kVideoTrack, 0);
    MOZ_ASSERT(mVideoTrackDemuxer);
  }
  if (audioActive) {
    mAudioTrackDemuxer =
      mMainThreadDemuxer->GetTrackDemuxer(TrackInfo::kAudioTrack, 0);
    MOZ_ASSERT(mAudioTrackDemuxer);
  }

  mInitDone = true;

  if (!IsWaitingOnCDMResource() && !EnsureDecodersSetup()) {
    mMetadataPromise.Reject(ReadMetadataFailureReason::METADATA_ERROR, __func__);
  } else {
    nsRefPtr<MetadataHolder> metadata = new MetadataHolder();
    metadata->mInfo = mInfo;
    metadata->mTags = nullptr;
    mMetadataPromise.Resolve(metadata, __func__);
  }
}

void
MediaFormatReader::OnDemuxerInitFailed(DemuxerFailureReason aFailure)
{
  mDemuxerInitRequest.Complete();
  if (aFailure == DemuxerFailureReason::WAITING_FOR_DATA) {
    mMetadataPromise.Reject(ReadMetadataFailureReason::WAITING_FOR_RESOURCES, __func__);
  } else {
    mMetadataPromise.Reject(ReadMetadataFailureReason::METADATA_ERROR, __func__);
  }
}

bool
MediaFormatReader::EnsureDecodersSetup()
{
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(mInitDone);

  if (!mPlatform) {
    if (IsEncrypted()) {
#ifdef MOZ_EME
      // We have encrypted audio or video. We'll need a CDM to decrypt and
      // possibly decode this. Wait until we've received a CDM from the
      // JavaScript player app. Note: we still go through the motions here
      // even if EME is disabled, so that if script tries and fails to create
      // a CDM, we can detect that and notify chrome and show some UI
      // explaining that we failed due to EME being disabled.
      nsRefPtr<CDMProxy> proxy;
      {
        ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
        proxy = mDecoder->GetCDMProxy();
      }
      MOZ_ASSERT(proxy);

      mPlatform = PlatformDecoderModule::CreateCDMWrapper(proxy);
      NS_ENSURE_TRUE(mPlatform, false);
#else
      // EME not supported.
      return false;
#endif
    } else {
      mPlatform = PlatformDecoderModule::Create();
      NS_ENSURE_TRUE(mPlatform, false);
    }
  }

  MOZ_ASSERT(mPlatform);

  if (HasAudio() && !mAudio.mDecoder) {
    NS_ENSURE_TRUE(IsSupportedAudioMimeType(mInfo.mAudio.mMimeType),
                   false);

    mAudio.mDecoder =
      mPlatform->CreateDecoder(mAudio.mInfo ?
                                 *mAudio.mInfo->GetAsAudioInfo() :
                                 mInfo.mAudio,
                               mAudio.mTaskQueue,
                               mAudio.mCallback);
    NS_ENSURE_TRUE(mAudio.mDecoder != nullptr, false);
    nsresult rv = mAudio.mDecoder->Init();
    NS_ENSURE_SUCCESS(rv, false);
  }

  if (HasVideo() && !mVideo.mDecoder) {
    NS_ENSURE_TRUE(IsSupportedVideoMimeType(mInfo.mVideo.mMimeType),
                   false);

    if (mSharedDecoderManager &&
        mPlatform->SupportsSharedDecoders(mInfo.mVideo)) {
      mVideo.mDecoder =
        mSharedDecoderManager->CreateVideoDecoder(mPlatform,
                                                  mVideo.mInfo ?
                                                    *mVideo.mInfo->GetAsVideoInfo() :
                                                    mInfo.mVideo,
                                                  mLayersBackendType,
                                                  mDecoder->GetImageContainer(),
                                                  mVideo.mTaskQueue,
                                                  mVideo.mCallback);
    } else {
      mVideo.mDecoder =
        mPlatform->CreateDecoder(mVideo.mInfo ?
                                   *mVideo.mInfo->GetAsVideoInfo() :
                                   mInfo.mVideo,
                                 mVideo.mTaskQueue,
                                 mVideo.mCallback,
                                 mLayersBackendType,
                                 mDecoder->GetImageContainer());
    }
    NS_ENSURE_TRUE(mVideo.mDecoder != nullptr, false);
    nsresult rv = mVideo.mDecoder->Init();
    NS_ENSURE_SUCCESS(rv, false);
  }

  return true;
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

void
MediaFormatReader::DisableHardwareAcceleration()
{
  MOZ_ASSERT(OnTaskQueue());
  if (HasVideo() && mSharedDecoderManager) {
    mSharedDecoderManager->DisableHardwareAcceleration();

    if (!mSharedDecoderManager->Recreate(mInfo.mVideo)) {
      mVideo.mError = true;
    }
    ScheduleUpdate(TrackInfo::kVideoTrack);
  }
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
  return nextKeyframe < aTimeThreshold && nextKeyframe.ToMicroseconds() >= 0;
}

nsRefPtr<MediaDecoderReader::VideoDataPromise>
MediaFormatReader::RequestVideoData(bool aSkipToNextKeyframe,
                                    int64_t aTimeThreshold,
                                    bool aForceDecodeAhead)
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
    return VideoDataPromise::CreateAndReject(DECODE_ERROR, __func__);
  }

  if (IsSeeking()) {
    LOG("called mid-seek. Rejecting.");
    return VideoDataPromise::CreateAndReject(CANCELED, __func__);
  }

  if (mShutdown) {
    NS_WARNING("RequestVideoData on shutdown MediaFormatReader!");
    return VideoDataPromise::CreateAndReject(CANCELED, __func__);
  }

  if (!EnsureDecodersSetup()) {
    NS_WARNING("Error constructing decoders");
    return VideoDataPromise::CreateAndReject(DECODE_ERROR, __func__);
  }

  MOZ_ASSERT(HasVideo() && mPlatform && mVideo.mDecoder);

  mVideo.mForceDecodeAhead = aForceDecodeAhead;
  media::TimeUnit timeThreshold{media::TimeUnit::FromMicroseconds(aTimeThreshold)};
  if (ShouldSkip(aSkipToNextKeyframe, timeThreshold)) {
    Flush(TrackInfo::kVideoTrack);
    nsRefPtr<VideoDataPromise> p = mVideo.mPromise.Ensure(__func__);
    SkipVideoDemuxToNextKeyFrame(timeThreshold);
    return p;
  }

  nsRefPtr<VideoDataPromise> p = mVideo.mPromise.Ensure(__func__);
  ScheduleUpdate(TrackInfo::kVideoTrack);
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
MediaFormatReader::OnVideoDemuxCompleted(nsRefPtr<MediaTrackDemuxer::SamplesHolder> aSamples)
{
  LOGV("%d video samples demuxed (sid:%d)",
       aSamples->mSamples.Length(),
       aSamples->mSamples[0]->mTrackInfo ? aSamples->mSamples[0]->mTrackInfo->GetID() : 0);
  mVideo.mDemuxRequest.Complete();
  mVideo.mQueuedSamples.AppendElements(aSamples->mSamples);
  ScheduleUpdate(TrackInfo::kVideoTrack);
}

nsRefPtr<MediaDecoderReader::AudioDataPromise>
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
    return AudioDataPromise::CreateAndReject(DECODE_ERROR, __func__);
  }

  if (IsSeeking()) {
    LOG("called mid-seek. Rejecting.");
    return AudioDataPromise::CreateAndReject(CANCELED, __func__);
  }

  if (mShutdown) {
    NS_WARNING("RequestAudioData on shutdown MediaFormatReader!");
    return AudioDataPromise::CreateAndReject(CANCELED, __func__);
  }

  if (!EnsureDecodersSetup()) {
    NS_WARNING("Error constructing decoders");
    return AudioDataPromise::CreateAndReject(DECODE_ERROR, __func__);
  }

  nsRefPtr<AudioDataPromise> p = mAudio.mPromise.Ensure(__func__);
  ScheduleUpdate(TrackInfo::kAudioTrack);
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
MediaFormatReader::OnAudioDemuxCompleted(nsRefPtr<MediaTrackDemuxer::SamplesHolder> aSamples)
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
  LOGV("Received new sample time:%lld duration:%lld",
       aSample->mTime, aSample->mDuration);
  auto& decoder = GetDecoderData(aTrack);
  if (!decoder.mOutputRequested) {
    LOG("MediaFormatReader produced output while flushing, discarding.");
    return;
  }
  decoder.mOutput.AppendElement(aSample);
  decoder.mNumSamplesOutput++;
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
  decoder.mNeedDraining = true;
  ScheduleUpdate(aTrack);
}

void
MediaFormatReader::NotifyWaitingForData(TrackType aTrack)
{
  MOZ_ASSERT(OnTaskQueue());
  auto& decoder = GetDecoderData(aTrack);
  decoder.mWaitingForData = true;
  decoder.mNeedDraining = true;
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

bool
MediaFormatReader::NeedInput(DecoderData& aDecoder)
{
  MOZ_ASSERT(OnTaskQueue());
  // We try to keep a few more compressed samples input than decoded samples
  // have been output, provided the state machine has requested we send it a
  // decoded sample. To account for H.264 streams which may require a longer
  // run of input than we input, decoders fire an "input exhausted" callback,
  // which overrides our "few more samples" threshold.
  return
    !aDecoder.mDraining &&
    !aDecoder.mError &&
    (aDecoder.HasPromise() || aDecoder.mForceDecodeAhead) &&
    !aDecoder.mDemuxRequest.Exists() &&
    aDecoder.mOutput.IsEmpty() &&
    (aDecoder.mInputExhausted || !aDecoder.mQueuedSamples.IsEmpty() ||
     aDecoder.mTimeThreshold.isSome() ||
     aDecoder.mForceDecodeAhead ||
     aDecoder.mNumSamplesInput - aDecoder.mNumSamplesOutput < aDecoder.mDecodeAhead);
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
    NS_NewRunnableMethodWithArg<TrackType>(this, &MediaFormatReader::Update, aTrack));
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
  decoder.mReceivedNewData = false;
  decoder.mWaitingForData = false;
  bool hasLastEnd;
  media::TimeUnit lastEnd = decoder.mTimeRanges.GetEnd(&hasLastEnd);
  // Update our cached TimeRange.
  decoder.mTimeRanges = decoder.mTrackDemuxer->GetBuffered();
  if (decoder.mTimeRanges.Length() &&
      (!hasLastEnd || decoder.mTimeRanges.GetEnd() > lastEnd)) {
    // New data was added after our previous end, we can clear the EOS flag.
    decoder.mDemuxEOS = false;
  }

  if (decoder.mError) {
    return false;
  }
  if (decoder.HasWaitingPromise()) {
    MOZ_ASSERT(!decoder.HasPromise());
    LOG("We have new data. Resolving WaitingPromise");
    decoder.mWaitingPromise.Resolve(decoder.mType, __func__);
    return true;
  }
  if (!mSeekPromise.IsEmpty()) {
    MOZ_ASSERT(!decoder.HasPromise());
    if (mVideo.mSeekRequest.Exists() || mAudio.mSeekRequest.Exists()) {
      // Already waiting for a seek to complete. Nothing more to do.
      return true;
    }
    LOG("Attempting Seek");
    AttemptSeek();
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
    return;
  }

  LOGV("Requesting extra demux %s", TrackTypeToStr(aTrack));
  if (aTrack == TrackInfo::kVideoTrack) {
    DoDemuxVideo();
  } else {
    DoDemuxAudio();
  }
}

void
MediaFormatReader::DecodeDemuxedSamples(TrackType aTrack,
                                        AbstractMediaDecoder::AutoNotifyDecoded& aA)
{
  MOZ_ASSERT(OnTaskQueue());
  auto& decoder = GetDecoderData(aTrack);

  if (decoder.mQueuedSamples.IsEmpty()) {
    return;
  }

  LOGV("Giving %s input to decoder", TrackTypeToStr(aTrack));

  // Decode all our demuxed frames.
  bool samplesPending = false;
  while (decoder.mQueuedSamples.Length()) {
    nsRefPtr<MediaRawData> sample = decoder.mQueuedSamples[0];
    nsRefPtr<SharedTrackInfo> info = sample->mTrackInfo;

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
        decoder.mNeedDraining = true;
        decoder.mNextStreamSourceID = Some(info->GetID());
        DrainDecoder(aTrack);
        return;
      }

      LOG("%s stream id has changed from:%d to:%d, recreating decoder.",
          TrackTypeToStr(aTrack), decoder.mLastStreamSourceID,
          info->GetID());
      decoder.mInfo = info;
      decoder.mLastStreamSourceID = info->GetID();
      // Flush will clear our array of queued samples. So make a copy now.
      nsTArray<nsRefPtr<MediaRawData>> samples{decoder.mQueuedSamples};
      Flush(aTrack);
      decoder.mDecoder->Shutdown();
      decoder.mDecoder = nullptr;
      if (!EnsureDecodersSetup()) {
        LOG("Unable to re-create decoder, aborting");
        NotifyError(aTrack);
        return;
      }
      LOGV("%s decoder:%p created for sid:%u",
           TrackTypeToStr(aTrack), decoder.mDecoder.get(), info->GetID());
      if (sample->mKeyframe) {
        decoder.mQueuedSamples.MoveElementsFrom(samples);
      } else {
        MOZ_ASSERT(decoder.mTimeThreshold.isNothing());
        LOG("Stream change occurred on a non-keyframe. Seeking to:%lld",
            sample->mTime);
        decoder.mTimeThreshold = Some(TimeUnit::FromMicroseconds(sample->mTime));
        nsRefPtr<MediaFormatReader> self = this;
        decoder.mSeekRequest.Begin(decoder.mTrackDemuxer->Seek(decoder.mTimeThreshold.ref())
                   ->Then(OwnerThread(), __func__,
                          [self, aTrack] (media::TimeUnit aTime) {
                            auto& decoder = self->GetDecoderData(aTrack);
                            decoder.mSeekRequest.Complete();
                            self->ScheduleUpdate(aTrack);
                          },
                          [self, aTrack] (DemuxerFailureReason aResult) {
                            auto& decoder = self->GetDecoderData(aTrack);
                            decoder.mSeekRequest.Complete();
                            switch (aResult) {
                              case DemuxerFailureReason::WAITING_FOR_DATA:
                                self->NotifyWaitingForData(aTrack);
                                break;
                              case DemuxerFailureReason::END_OF_STREAM:
                                self->NotifyEndOfStream(aTrack);
                                break;
                              case DemuxerFailureReason::CANCELED:
                              case DemuxerFailureReason::SHUTDOWN:
                                break;
                              default:
                                self->NotifyError(aTrack);
                                break;
                            }
                            decoder.mTimeThreshold.reset();
                          }));
        return;
      }
    }

    LOGV("Input:%lld (dts:%lld kf:%d)",
         sample->mTime, sample->mTimecode, sample->mKeyframe);
    decoder.mOutputRequested = true;
    decoder.mNumSamplesInput++;
    decoder.mSizeOfQueue++;
    if (aTrack == TrackInfo::kVideoTrack) {
      aA.mParsed++;
    }
    decoder.mDecoder->Input(sample);
    decoder.mQueuedSamples.RemoveElementAt(0);
    samplesPending = true;
  }

  // We have serviced the decoder's request for more data.
  decoder.mInputExhausted = false;
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
  if (!decoder.mDecoder) {
    return;
  }
  decoder.mOutputRequested = true;
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

  bool needInput = false;
  bool needOutput = false;
  auto& decoder = GetDecoderData(aTrack);
  decoder.mUpdateScheduled = false;

  if (UpdateReceivedNewData(aTrack)) {
    LOGV("Nothing more to do");
    return;
  }

  // Record number of frames decoded and parsed. Automatically update the
  // stats counters using the AutoNotifyDecoded stack-based class.
  AbstractMediaDecoder::AutoNotifyDecoded a(mDecoder);

  if (aTrack == TrackInfo::kVideoTrack) {
    uint64_t delta =
      decoder.mNumSamplesOutput - mLastReportedNumDecodedFrames;
    a.mDecoded = static_cast<uint32_t>(delta);
    mLastReportedNumDecodedFrames = decoder.mNumSamplesOutput;
  }

  if (decoder.HasPromise()) {
    needOutput = true;
    if (!decoder.mOutput.IsEmpty()) {
      // We have a decoded sample ready to be returned.
      if (aTrack == TrackType::kVideoTrack) {
        mVideo.mIsHardwareAccelerated =
          mVideo.mDecoder && mVideo.mDecoder->IsHardwareAccelerated();
      }
      nsRefPtr<MediaData> output = decoder.mOutput[0];
      decoder.mOutput.RemoveElementAt(0);
      decoder.mSizeOfQueue -= 1;
      if (decoder.mTimeThreshold.isNothing() ||
          media::TimeUnit::FromMicroseconds(output->mTime) >= decoder.mTimeThreshold.ref()) {
        ReturnOutput(output, aTrack);
        decoder.mTimeThreshold.reset();
      } else {
        LOGV("Internal Seeking: Dropping frame time:%f wanted:%f (kf:%d)",
             media::TimeUnit::FromMicroseconds(output->mTime).ToSeconds(),
             decoder.mTimeThreshold.ref().ToSeconds(),
             output->mKeyframe);
      }
    } else if (decoder.mDrainComplete) {
      decoder.mDrainComplete = false;
      decoder.mDraining = false;
      if (decoder.mError) {
        LOG("Decoding Error");
        decoder.RejectPromise(DECODE_ERROR, __func__);
        return;
      } else if (decoder.mDemuxEOS) {
        decoder.RejectPromise(END_OF_STREAM, __func__);
      } else if (decoder.mWaitingForData) {
        LOG("Waiting For Data");
        decoder.RejectPromise(WAITING_FOR_DATA, __func__);
      }
    } else if (decoder.mError && !decoder.mDecoder) {
      decoder.RejectPromise(DECODE_ERROR, __func__);
      return;
    }
  }

  if (decoder.mError || decoder.mDemuxEOS || decoder.mWaitingForData) {
    DrainDecoder(aTrack);
    return;
  }

  if (!NeedInput(decoder)) {
    LOGV("No need for additional input");
    return;
  }

  needInput = true;

  LOGV("Update(%s) ni=%d no=%d ie=%d, in:%d out:%d qs=%d sid:%d",
       TrackTypeToStr(aTrack), needInput, needOutput, decoder.mInputExhausted,
       decoder.mNumSamplesInput, decoder.mNumSamplesOutput,
       size_t(decoder.mSizeOfQueue), decoder.mLastStreamSourceID);

  // Demux samples if we don't have some.
  RequestDemuxSamples(aTrack);
  // Decode all pending demuxed samples.
  DecodeDemuxedSamples(aTrack, a);
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

  if (aTrack == TrackInfo::kAudioTrack) {
    AudioData* audioData = static_cast<AudioData*>(aData);

    if (audioData->mChannels != mInfo.mAudio.mChannels ||
        audioData->mRate != mInfo.mAudio.mRate) {
      LOG("change of audio format (rate:%d->%d). "
          "This is an unsupported configuration",
          mInfo.mAudio.mRate, audioData->mRate);
      mInfo.mAudio.mRate = audioData->mRate;
      mInfo.mAudio.mChannels = audioData->mChannels;
    }
    mAudio.mPromise.Resolve(audioData, __func__);
  } else if (aTrack == TrackInfo::kVideoTrack) {
    mVideo.mPromise.Resolve(static_cast<VideoData*>(aData), __func__);
  }
  LOG("Resolved data promise for %s", TrackTypeToStr(aTrack));
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

nsRefPtr<MediaDecoderReader::WaitForDataPromise>
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
  nsRefPtr<WaitForDataPromise> p = decoder.mWaitingPromise.Ensure(__func__);
  ScheduleUpdate(trackType);
  return p;
}

nsresult
MediaFormatReader::ResetDecode()
{
  MOZ_ASSERT(OnTaskQueue());
  LOGV("");

  mAudio.mSeekRequest.DisconnectIfExists();
  mVideo.mSeekRequest.DisconnectIfExists();
  mSeekPromise.RejectIfExists(NS_OK, __func__);
  mSkipRequest.DisconnectIfExists();

  // Do the same for any data wait promises.
  mAudio.mWaitingPromise.RejectIfExists(WaitForDataRejectValue(MediaData::AUDIO_DATA, WaitForDataRejectValue::CANCELED), __func__);
  mVideo.mWaitingPromise.RejectIfExists(WaitForDataRejectValue(MediaData::VIDEO_DATA, WaitForDataRejectValue::CANCELED), __func__);

  // Reset miscellaneous seeking state.
  mPendingSeekTime.reset();

  if (HasVideo()) {
    mVideo.ResetDemuxer();
    Flush(TrackInfo::kVideoTrack);
    if (mVideo.HasPromise()) {
      mVideo.RejectPromise(CANCELED, __func__);
    }
  }
  if (HasAudio()) {
    mAudio.ResetDemuxer();
    Flush(TrackInfo::kAudioTrack);
    if (mAudio.HasPromise()) {
      mAudio.RejectPromise(CANCELED, __func__);
    }
  }
  return MediaDecoderReader::ResetDecode();
}

void
MediaFormatReader::Output(TrackType aTrack, MediaData* aSample)
{
  LOGV("Decoded %s sample time=%lld dur=%lld",
       TrackTypeToStr(aTrack), aSample->mTime, aSample->mDuration);

  if (!aSample) {
    NS_WARNING("MediaFormatReader::Output() passed a null sample");
    Error(aTrack);
    return;
  }

  RefPtr<nsIRunnable> task =
    NS_NewRunnableMethodWithArgs<TrackType, MediaData*>(
      this, &MediaFormatReader::NotifyNewOutput, aTrack, aSample);
  OwnerThread()->Dispatch(task.forget());
}

void
MediaFormatReader::DrainComplete(TrackType aTrack)
{
  RefPtr<nsIRunnable> task =
    NS_NewRunnableMethodWithArg<TrackType>(
      this, &MediaFormatReader::NotifyDrainComplete, aTrack);
  OwnerThread()->Dispatch(task.forget());
}

void
MediaFormatReader::InputExhausted(TrackType aTrack)
{
  RefPtr<nsIRunnable> task =
    NS_NewRunnableMethodWithArg<TrackType>(
      this, &MediaFormatReader::NotifyInputExhausted, aTrack);
  OwnerThread()->Dispatch(task.forget());
}

void
MediaFormatReader::Error(TrackType aTrack)
{
  RefPtr<nsIRunnable> task =
    NS_NewRunnableMethodWithArg<TrackType>(
      this, &MediaFormatReader::NotifyError, aTrack);
  OwnerThread()->Dispatch(task.forget());
}

void
MediaFormatReader::Flush(TrackType aTrack)
{
  MOZ_ASSERT(OnTaskQueue());
  LOG("Flush(%s) BEGIN", TrackTypeToStr(aTrack));

  auto& decoder = GetDecoderData(aTrack);
  if (!decoder.mDecoder) {
    return;
  }

  decoder.mDecoder->Flush();
  // Purge the current decoder's state.
  // ResetState clears mOutputRequested flag so that we ignore all output until
  // the next request for more data.
  decoder.ResetState();
  LOG("Flush(%s) END", TrackTypeToStr(aTrack));
}

void
MediaFormatReader::SkipVideoDemuxToNextKeyFrame(media::TimeUnit aTimeThreshold)
{
  MOZ_ASSERT(OnTaskQueue());

  MOZ_ASSERT(mVideo.mDecoder);
  MOZ_ASSERT(mVideo.HasPromise());
  LOG("Skipping up to %lld", aTimeThreshold.ToMicroseconds());

  if (mVideo.mError) {
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
  mDecoder->NotifyDecodedFrames(aSkipped, 0, aSkipped);
  MOZ_ASSERT(!mVideo.mError); // We have flushed the decoder, no frame could
                              // have been decoded (and as such errored)
  ScheduleUpdate(TrackInfo::kVideoTrack);
}

void
MediaFormatReader::OnVideoSkipFailed(MediaTrackDemuxer::SkipFailureHolder aFailure)
{
  MOZ_ASSERT(OnTaskQueue());
  LOG("Skipping failed, skipped %u frames", aFailure.mSkipped);
  mSkipRequest.Complete();
  mDecoder->NotifyDecodedFrames(aFailure.mSkipped, 0, aFailure.mSkipped);
  MOZ_ASSERT(mVideo.HasPromise());
  switch (aFailure.mFailure) {
    case DemuxerFailureReason::END_OF_STREAM:
      NotifyEndOfStream(TrackType::kVideoTrack);
      mVideo.RejectPromise(END_OF_STREAM, __func__);
      break;
    case DemuxerFailureReason::WAITING_FOR_DATA:
      NotifyWaitingForData(TrackType::kVideoTrack);
      mVideo.RejectPromise(WAITING_FOR_DATA, __func__);
      break;
    case DemuxerFailureReason::CANCELED:
    case DemuxerFailureReason::SHUTDOWN:
      break;
    default:
      NotifyError(TrackType::kVideoTrack);
      mVideo.RejectPromise(DECODE_ERROR, __func__);
      break;
  }
}

nsRefPtr<MediaDecoderReader::SeekPromise>
MediaFormatReader::Seek(int64_t aTime, int64_t aUnused)
{
  MOZ_ASSERT(OnTaskQueue());

  LOG("aTime=(%lld)", aTime);

  MOZ_DIAGNOSTIC_ASSERT(mSeekPromise.IsEmpty());
  MOZ_DIAGNOSTIC_ASSERT(!mVideo.HasPromise());
  MOZ_DIAGNOSTIC_ASSERT(!mAudio.HasPromise());
  MOZ_DIAGNOSTIC_ASSERT(mPendingSeekTime.isNothing());
  MOZ_DIAGNOSTIC_ASSERT(mVideo.mTimeThreshold.isNothing());
  MOZ_DIAGNOSTIC_ASSERT(mAudio.mTimeThreshold.isNothing());

  if (!mSeekable) {
    LOG("Seek() END (Unseekable)");
    return SeekPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  if (mShutdown) {
    return SeekPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  mPendingSeekTime.emplace(media::TimeUnit::FromMicroseconds(aTime));

  nsRefPtr<SeekPromise> p = mSeekPromise.Ensure(__func__);

  AttemptSeek();

  return p;
}

void
MediaFormatReader::AttemptSeek()
{
  MOZ_ASSERT(OnTaskQueue());
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

  if (HasAudio()) {
    MOZ_ASSERT(mPendingSeekTime.isSome());
    DoAudioSeek();
  } else {
    mPendingSeekTime.reset();
    mSeekPromise.Resolve(aTime.ToMicroseconds(), __func__);
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
  mSeekPromise.Resolve(aTime.ToMicroseconds(), __func__);
}

int64_t
MediaFormatReader::GetEvictionOffset(double aTime)
{
  int64_t audioOffset;
  int64_t videoOffset;
  if (NS_IsMainThread()) {
    audioOffset = HasAudio() ? mAudioTrackDemuxer->GetEvictionOffset(media::TimeUnit::FromSeconds(aTime)) : INT64_MAX;
    videoOffset = HasVideo() ? mVideoTrackDemuxer->GetEvictionOffset(media::TimeUnit::FromSeconds(aTime)) : INT64_MAX;
  } else {
    MOZ_ASSERT(OnTaskQueue());
    audioOffset = HasAudio() ? mAudio.mTrackDemuxer->GetEvictionOffset(media::TimeUnit::FromSeconds(aTime)) : INT64_MAX;
    videoOffset = HasVideo() ? mVideo.mTrackDemuxer->GetEvictionOffset(media::TimeUnit::FromSeconds(aTime)) : INT64_MAX;
  }
  return std::min(audioOffset, videoOffset);
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
  int64_t startTime;
  if (!ForceZeroStartTime()) {
    if (!HaveStartTime()) {
      return intervals;
    }
    startTime = StartTime();
  } else {
    // MSE, start time is assumed to be 0 we can proceeed with what we have.
    startTime = 0;
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

  return intervals.Shift(media::TimeUnit::FromMicroseconds(-startTime));
}

void MediaFormatReader::ReleaseMediaResources()
{
  // Before freeing a video codec, all video buffers needed to be released
  // even from graphics pipeline.
  VideoFrameContainer* container =
    mDecoder ? mDecoder->GetVideoFrameContainer() : nullptr;
  if (container) {
    container->ClearCurrentFrame();
  }
  if (mVideo.mDecoder) {
    mVideo.mDecoder->Shutdown();
    mVideo.mDecoder = nullptr;
  }
}

void
MediaFormatReader::SetIdle()
{
  if (mSharedDecoderManager && mVideo.mDecoder) {
    mSharedDecoderManager->SetIdle(mVideo.mDecoder);
  }
}

void
MediaFormatReader::SetSharedDecoderManager(SharedDecoderManager* aManager)
{
#if !defined(MOZ_WIDGET_ANDROID)
  mSharedDecoderManager = aManager;
#endif
}

bool
MediaFormatReader::VideoIsHardwareAccelerated() const
{
  return mVideo.mIsHardwareAccelerated;
}

void
MediaFormatReader::NotifyDemuxer(uint32_t aLength, int64_t aOffset)
{
  MOZ_ASSERT(OnTaskQueue());

  LOGV("aLength=%u, aOffset=%lld", aLength, aOffset);
  if (mShutdown) {
    return;
  }

  if (aLength || aOffset) {
    mDemuxer->NotifyDataArrived(aLength, aOffset);
  } else {
    mDemuxer->NotifyDataRemoved();
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
MediaFormatReader::NotifyDataArrivedInternal(uint32_t aLength, int64_t aOffset)
{
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(aLength);

  if (!mInitDone || mShutdown) {
    return;
  }

  MOZ_ASSERT(mMainThreadDemuxer);

  // Queue a task to notify our main thread demuxer.
  nsCOMPtr<nsIRunnable> task =
    NS_NewRunnableMethodWithArgs<uint32_t, int64_t>(
      mMainThreadDemuxer, &MediaDataDemuxer::NotifyDataArrived,
      aLength, aOffset);
  AbstractThread::MainThread()->Dispatch(task.forget());

  NotifyDemuxer(aLength, aOffset);
}

void
MediaFormatReader::NotifyDataRemoved()
{
  MOZ_ASSERT(OnTaskQueue());

  if (!mInitDone || mShutdown) {
    return;
  }

  MOZ_ASSERT(mMainThreadDemuxer);

  // Queue a task to notify our main thread demuxer.
  nsCOMPtr<nsIRunnable> task =
    NS_NewRunnableMethod(
      mMainThreadDemuxer, &MediaDataDemuxer::NotifyDataRemoved);
  AbstractThread::MainThread()->Dispatch(task.forget());

  NotifyDemuxer(0, 0);
}

bool
MediaFormatReader::ForceZeroStartTime() const
{
  return !mDemuxer->ShouldComputeStartTime();
}

} // namespace mozilla
