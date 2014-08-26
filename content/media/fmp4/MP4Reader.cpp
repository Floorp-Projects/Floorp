/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MP4Reader.h"
#include "MediaResource.h"
#include "nsSize.h"
#include "VideoUtils.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "ImageContainer.h"
#include "Layers.h"
#include "SharedThreadPool.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/TimeRanges.h"

#ifdef MOZ_EME
#include "mozilla/CDMProxy.h"
#endif

using mozilla::layers::Image;
using mozilla::layers::LayerManager;
using mozilla::layers::LayersBackend;

#ifdef PR_LOGGING
PRLogModuleInfo* GetDemuxerLog() {
  static PRLogModuleInfo* log = nullptr;
  if (!log) {
    log = PR_NewLogModule("MP4Demuxer");
  }
  return log;
}
#define LOG(...) PR_LOG(GetDemuxerLog(), PR_LOG_DEBUG, (__VA_ARGS__))
#else
#define LOG(...)
#endif

using namespace mp4_demuxer;

namespace mozilla {

// Uncomment to enable verbose per-sample logging.
//#define LOG_SAMPLE_DECODE 1

#ifdef PR_LOGGING
static const char*
TrackTypeToStr(TrackType aTrack)
{
  MOZ_ASSERT(aTrack == kAudio || aTrack == kVideo);
  switch (aTrack) {
  case kAudio:
    return "Audio";
  case kVideo:
    return "Video";
  default:
    return "Unknown";
  }
}
#endif

class MP4Stream : public Stream {
public:

  MP4Stream(MediaResource* aResource)
    : mResource(aResource)
  {
    MOZ_COUNT_CTOR(MP4Stream);
    MOZ_ASSERT(aResource);
  }
  virtual ~MP4Stream() {
    MOZ_COUNT_DTOR(MP4Stream);
  }

  virtual bool ReadAt(int64_t aOffset, void* aBuffer, size_t aCount,
                      size_t* aBytesRead) MOZ_OVERRIDE
  {
    uint32_t sum = 0;
    uint32_t bytesRead = 0;
    do {
      uint64_t offset = aOffset + sum;
      char* buffer = reinterpret_cast<char*>(aBuffer) + sum;
      uint32_t toRead = aCount - sum;
      nsresult rv = mResource->ReadAt(offset, buffer, toRead, &bytesRead);
      if (NS_FAILED(rv)) {
        return false;
      }
      sum += bytesRead;
    } while (sum < aCount && bytesRead > 0);
    *aBytesRead = sum;
    return true;
  }

  virtual bool Length(int64_t* aSize) MOZ_OVERRIDE
  {
    if (mResource->GetLength() < 0)
      return false;
    *aSize = mResource->GetLength();
    return true;
  }

private:
  RefPtr<MediaResource> mResource;
};

MP4Reader::MP4Reader(AbstractMediaDecoder* aDecoder)
  : MediaDecoderReader(aDecoder)
  , mAudio("MP4 audio decoder data", Preferences::GetUint("media.mp4-audio-decode-ahead", 2))
  , mVideo("MP4 video decoder data", Preferences::GetUint("media.mp4-video-decode-ahead", 2))
  , mLastReportedNumDecodedFrames(0)
  , mLayersBackendType(layers::LayersBackend::LAYERS_NONE)
  , mDemuxerInitialized(false)
  , mIsEncrypted(false)
  , mIndexReady(false)
  , mIndexMonitor("MP4 index")
{
  MOZ_ASSERT(NS_IsMainThread(), "Must be on main thread.");
  MOZ_COUNT_CTOR(MP4Reader);
}

MP4Reader::~MP4Reader()
{
  MOZ_ASSERT(NS_IsMainThread(), "Must be on main thread.");
  MOZ_COUNT_DTOR(MP4Reader);
  Shutdown();
}

void
MP4Reader::Shutdown()
{
  if (mAudio.mDecoder) {
    Flush(kAudio);
    mAudio.mDecoder->Shutdown();
    mAudio.mDecoder = nullptr;
  }
  if (mAudio.mTaskQueue) {
    mAudio.mTaskQueue->Shutdown();
    mAudio.mTaskQueue = nullptr;
  }
  if (mVideo.mDecoder) {
    Flush(kVideo);
    mVideo.mDecoder->Shutdown();
    mVideo.mDecoder = nullptr;
  }
  if (mVideo.mTaskQueue) {
    mVideo.mTaskQueue->Shutdown();
    mVideo.mTaskQueue = nullptr;
  }
  // Dispose of the queued sample before shutting down the demuxer
  mQueuedVideoSample = nullptr;

  if (mPlatform) {
    mPlatform->Shutdown();
    mPlatform = nullptr;
  }
}

void
MP4Reader::InitLayersBackendType()
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
    NS_WARNING("MP4Reader without a decoder owner, can't get HWAccel");
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
MP4Reader::Init(MediaDecoderReader* aCloneDonor)
{
  MOZ_ASSERT(NS_IsMainThread(), "Must be on main thread.");
  PlatformDecoderModule::Init();
  mDemuxer = new MP4Demuxer(new MP4Stream(mDecoder->GetResource()));

  InitLayersBackendType();

  mAudio.mTaskQueue = new MediaTaskQueue(
    SharedThreadPool::Get(NS_LITERAL_CSTRING("MP4 Audio Decode")));
  NS_ENSURE_TRUE(mAudio.mTaskQueue, NS_ERROR_FAILURE);

  mVideo.mTaskQueue = new MediaTaskQueue(
    SharedThreadPool::Get(NS_LITERAL_CSTRING("MP4 Video Decode")));
  NS_ENSURE_TRUE(mVideo.mTaskQueue, NS_ERROR_FAILURE);

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
      owner->DispatchNeedKey(mInitData, mInitDataType);
    }
    mDecoder = nullptr;
    return NS_OK;
  }
private:
  nsRefPtr<AbstractMediaDecoder> mDecoder;
  nsTArray<uint8_t> mInitData;
  nsString mInitDataType;
};
#endif

bool MP4Reader::IsWaitingOnCodecResource() {
#ifdef MOZ_GONK_MEDIACODEC
  return mVideo.mDecoder && mVideo.mDecoder->IsWaitingMediaResources();
#endif
  return false;
}

bool MP4Reader::IsWaitingOnCDMResource() {
#ifdef MOZ_EME
  nsRefPtr<CDMProxy> proxy;
  {
    ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
    if (!mIsEncrypted) {
      // Not encrypted, no need to wait for CDMProxy.
      return false;
    }
    proxy = mDecoder->GetCDMProxy();
    if (!proxy) {
      // We're encrypted, we need a CDMProxy to decrypt file.
      return true;
    }
  }
  // We'll keep waiting if the CDM hasn't informed Gecko of its capabilities.
  {
    CDMCaps::AutoLock caps(proxy->Capabilites());
    LOG("MP4Reader::IsWaitingMediaResources() capsKnown=%d", caps.AreCapsKnown());
    return !caps.AreCapsKnown();
  }
#else
  return false;
#endif
}

bool MP4Reader::IsWaitingMediaResources()
{
  // IsWaitingOnCDMResource() *must* come first, because we don't know whether
  // we can create a decoder until the CDM is initialized and it has told us
  // whether *it* will decode, or whether we need to create a PDM to do the
  // decoding
  return IsWaitingOnCDMResource() || IsWaitingOnCodecResource();
}

void
MP4Reader::ExtractCryptoInitData(nsTArray<uint8_t>& aInitData)
{
  MOZ_ASSERT(mDemuxer->Crypto().valid);
  const nsTArray<mp4_demuxer::PsshInfo>& psshs = mDemuxer->Crypto().pssh;
  for (uint32_t i = 0; i < psshs.Length(); i++) {
    aInitData.AppendElements(psshs[i].data);
  }
}

bool
MP4Reader::IsSupportedAudioMimeType(const char* aMimeType)
{
  return (!strcmp(aMimeType, "audio/mpeg") ||
          !strcmp(aMimeType, "audio/mp4a-latm")) &&
         mPlatform->SupportsAudioMimeType(aMimeType);
}

nsresult
MP4Reader::ReadMetadata(MediaInfo* aInfo,
                        MetadataTags** aTags)
{
  if (!mDemuxerInitialized) {
    bool ok = mDemuxer->Init();
    NS_ENSURE_TRUE(ok, NS_ERROR_FAILURE);

    {
      MonitorAutoLock mon(mIndexMonitor);
      mIndexReady = true;
    }

    mInfo.mVideo.mHasVideo = mVideo.mActive = mDemuxer->HasValidVideo();
    const VideoDecoderConfig& video = mDemuxer->VideoConfig();
    // If we have video, we *only* allow H.264 to be decoded.
    if (mInfo.mVideo.mHasVideo && strcmp(video.mime_type, "video/avc")) {
      return NS_ERROR_FAILURE;
    }

    mInfo.mAudio.mHasAudio = mAudio.mActive = mDemuxer->HasValidAudio();

    {
      ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
      mIsEncrypted = mDemuxer->Crypto().valid;
    }

    // Remember that we've initialized the demuxer, so that if we're decoding
    // an encrypted stream and we need to wait for a CDM to be set, we don't
    // need to reinit the demuxer.
    mDemuxerInitialized = true;
  } else if (mPlatform && !IsWaitingMediaResources()) {
    *aInfo = mInfo;
    *aTags = nullptr;
    return NS_OK;
  }

  if (mDemuxer->Crypto().valid) {
#ifdef MOZ_EME
    if (!sIsEMEEnabled) {
      // TODO: Need to signal DRM/EME required somehow...
      return NS_ERROR_FAILURE;
    }

    // We have encrypted audio or video. We'll need a CDM to decrypt and
    // possibly decode this. Wait until we've received a CDM from the
    // JavaScript player app.
    nsRefPtr<CDMProxy> proxy;
    nsTArray<uint8_t> initData;
    ExtractCryptoInitData(initData);
    if (initData.Length() == 0) {
      return NS_ERROR_FAILURE;
    }
    if (!mInitDataEncountered.Contains(initData)) {
      mInitDataEncountered.AppendElement(initData);
      NS_DispatchToMainThread(new DispatchKeyNeededEvent(mDecoder, initData, NS_LITERAL_STRING("cenc")));
    }
    if (IsWaitingMediaResources()) {
      return NS_OK;
    }
    MOZ_ASSERT(!IsWaitingMediaResources());

    {
      ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
      proxy = mDecoder->GetCDMProxy();
    }
    MOZ_ASSERT(proxy);

    mPlatform = PlatformDecoderModule::CreateCDMWrapper(proxy,
                                                        HasAudio(),
                                                        HasVideo(),
                                                        GetTaskQueue());
    NS_ENSURE_TRUE(mPlatform, NS_ERROR_FAILURE);
#else
    // EME not supported.
    return NS_ERROR_FAILURE;
#endif
  } else {
    mPlatform = PlatformDecoderModule::Create();
    NS_ENSURE_TRUE(mPlatform, NS_ERROR_FAILURE);
  }

  if (HasAudio()) {
    const AudioDecoderConfig& audio = mDemuxer->AudioConfig();
    if (mInfo.mAudio.mHasAudio && !IsSupportedAudioMimeType(audio.mime_type)) {
      return NS_ERROR_FAILURE;
    }
    mInfo.mAudio.mRate = audio.samples_per_second;
    mInfo.mAudio.mChannels = audio.channel_count;
    mAudio.mCallback = new DecoderCallback(this, kAudio);
    mAudio.mDecoder = mPlatform->CreateAudioDecoder(audio,
                                                    mAudio.mTaskQueue,
                                                    mAudio.mCallback);
    NS_ENSURE_TRUE(mAudio.mDecoder != nullptr, NS_ERROR_FAILURE);
    nsresult rv = mAudio.mDecoder->Init();
    NS_ENSURE_SUCCESS(rv, rv);

    // Decode one audio frame to detect potentially incorrect channels count or
    // sampling rate from demuxer.
    Decode(kAudio);
  }

  if (HasVideo()) {
    const VideoDecoderConfig& video = mDemuxer->VideoConfig();
    mInfo.mVideo.mDisplay =
      nsIntSize(video.display_width, video.display_height);
    mVideo.mCallback = new  DecoderCallback(this, kVideo);
    mVideo.mDecoder = mPlatform->CreateH264Decoder(video,
                                                   mLayersBackendType,
                                                   mDecoder->GetImageContainer(),
                                                   mVideo.mTaskQueue,
                                                   mVideo.mCallback);
    NS_ENSURE_TRUE(mVideo.mDecoder != nullptr, NS_ERROR_FAILURE);
    nsresult rv = mVideo.mDecoder->Init();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Get the duration, and report it to the decoder if we have it.
  Microseconds duration = mDemuxer->Duration();
  if (duration != -1) {
    ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
    mDecoder->SetMediaDuration(duration);
  }

  *aInfo = mInfo;
  *aTags = nullptr;

  UpdateIndex();

  return NS_OK;
}

bool
MP4Reader::IsMediaSeekable()
{
  // We can seek if we get a duration *and* the reader reports that it's
  // seekable.
  return mDecoder->GetResource()->IsTransportSeekable() && mDemuxer->CanSeek();
}

bool
MP4Reader::HasAudio()
{
  return mAudio.mActive;
}

bool
MP4Reader::HasVideo()
{
  return mVideo.mActive;
}

MP4Reader::DecoderData&
MP4Reader::GetDecoderData(TrackType aTrack)
{
  MOZ_ASSERT(aTrack == kAudio || aTrack == kVideo);
  return (aTrack == kAudio) ? mAudio : mVideo;
}

MediaDataDecoder*
MP4Reader::Decoder(TrackType aTrack)
{
  return GetDecoderData(aTrack).mDecoder;
}

MP4Sample*
MP4Reader::PopSample(TrackType aTrack)
{
  switch (aTrack) {
    case kAudio:
      return mDemuxer->DemuxAudioSample();

    case kVideo:
      if (mQueuedVideoSample) {
        return mQueuedVideoSample.forget();
      }
      return mDemuxer->DemuxVideoSample();

    default:
      return nullptr;
  }
}

// How async decoding works:
//
// When MP4Reader::Decode() is called:
// * Lock the DecoderData. We assume the state machine wants
//   output from the decoder (in future, we'll assume decoder wants input
//   when the output MediaQueue isn't "full").
// * Cache the value of mNumSamplesOutput, as prevFramesOutput.
// * While we've not output data (mNumSamplesOutput != prevNumFramesOutput)
//   and while we still require input, we demux and input data in the reader.
//   We assume we require input if
//   ((mNumSamplesInput - mNumSamplesOutput) < sDecodeAheadMargin) or
//   mInputExhausted is true. Before we send input, we reset mInputExhausted
//   and increment mNumFrameInput, and drop the lock on DecoderData.
// * Once we no longer require input, we wait on the DecoderData
//   lock for output, or for the input exhausted callback. If we receive the
//   input exhausted callback, we go back and input more data.
// * When our output callback is called, we take the DecoderData lock and
//   increment mNumSamplesOutput. We notify the DecoderData lock. This will
//   awaken the Decode thread, and unblock it, and it will return.
bool
MP4Reader::Decode(TrackType aTrack)
{
  DecoderData& data = GetDecoderData(aTrack);
  MOZ_ASSERT(data.mDecoder);

  data.mMonitor.Lock();
  uint64_t prevNumFramesOutput = data.mNumSamplesOutput;
  while (prevNumFramesOutput == data.mNumSamplesOutput) {
    data.mMonitor.AssertCurrentThreadOwns();
    if (data.mError) {
      // Decode error!
      data.mMonitor.Unlock();
      return false;
    }
    // Send input to the decoder, if we need to. We assume the decoder
    // needs input if it's told us it's out of input, or we're beneath the
    // "low water mark" for the amount of input we've sent it vs the amount
    // out output we've received. We always try to keep the decoder busy if
    // possible, so we try to maintain at least a few input samples ahead,
    // if we need output.
    while (prevNumFramesOutput == data.mNumSamplesOutput &&
           (data.mInputExhausted ||
           (data.mNumSamplesInput - data.mNumSamplesOutput) < data.mDecodeAhead) &&
           !data.mEOS) {
      data.mMonitor.AssertCurrentThreadOwns();
      data.mMonitor.Unlock();
      nsAutoPtr<MP4Sample> compressed(PopSample(aTrack));
      if (!compressed) {
        // EOS, or error. Send the decoder a signal to drain.
        LOG("Draining %s", TrackTypeToStr(aTrack));
        data.mMonitor.Lock();
        MOZ_ASSERT(!data.mEOS);
        data.mEOS = true;
        MOZ_ASSERT(!data.mDrainComplete);
        data.mDrainComplete = false;
        data.mMonitor.Unlock();
        data.mDecoder->Drain();
      } else {
#ifdef LOG_SAMPLE_DECODE
        LOG("PopSample %s time=%lld dur=%lld", TrackTypeToStr(aTrack),
            compressed->composition_timestamp, compressed->duration);
#endif
        data.mMonitor.Lock();
        data.mDrainComplete = false;
        data.mInputExhausted = false;
        data.mNumSamplesInput++;
        data.mMonitor.Unlock();

        if (NS_FAILED(data.mDecoder->Input(compressed))) {
          return false;
        }
        // If Input() failed, we let the auto pointer delete |compressed|.
        // Otherwise, we assume the decoder will delete it when it's finished
        // with it.
        compressed.forget();
      }
      data.mMonitor.Lock();
    }
    data.mMonitor.AssertCurrentThreadOwns();
    while (!data.mError &&
           prevNumFramesOutput == data.mNumSamplesOutput &&
           (!data.mInputExhausted || data.mEOS) &&
           !data.mDrainComplete) {
      data.mMonitor.Wait();
    }
    if (data.mError ||
        (data.mEOS && data.mDrainComplete)) {
      break;
    }
  }
  data.mMonitor.AssertCurrentThreadOwns();
  bool rv = !(data.mDrainComplete || data.mError);
  data.mMonitor.Unlock();
  return rv;
}

nsresult
MP4Reader::ResetDecode()
{
  Flush(kAudio);
  Flush(kVideo);
  return MediaDecoderReader::ResetDecode();
}

void
MP4Reader::Output(TrackType aTrack, MediaData* aSample)
{
#ifdef LOG_SAMPLE_DECODE
  LOG("Decoded %s sample time=%lld dur=%lld",
      TrackTypeToStr(aTrack), aSample->mTime, aSample->mDuration);
#endif

  DecoderData& data = GetDecoderData(aTrack);
  // Don't accept output while we're flushing.
  MonitorAutoLock mon(data.mMonitor);
  if (data.mIsFlushing) {
    delete aSample;
    LOG("MP4Reader produced output while flushing, discarding.");
    mon.NotifyAll();
    return;
  }

  switch (aTrack) {
    case kAudio: {
      MOZ_ASSERT(aSample->mType == MediaData::AUDIO_SAMPLES);
      AudioData* audioData = static_cast<AudioData*>(aSample);
      AudioQueue().Push(audioData);
      if (audioData->mChannels != mInfo.mAudio.mChannels ||
          audioData->mRate != mInfo.mAudio.mRate) {
        LOG("MP4Reader::Output change of sampling rate:%d->%d",
            mInfo.mAudio.mRate, audioData->mRate);
        mInfo.mAudio.mRate = audioData->mRate;
        mInfo.mAudio.mChannels = audioData->mChannels;
      }
      break;
    }
    case kVideo: {
      MOZ_ASSERT(aSample->mType == MediaData::VIDEO_FRAME);
      VideoQueue().Push(static_cast<VideoData*>(aSample));
      break;
    }
    default:
      break;
  }

  data.mNumSamplesOutput++;
  mon.NotifyAll();
}

void
MP4Reader::DrainComplete(TrackType aTrack)
{
  DecoderData& data = GetDecoderData(aTrack);
  MonitorAutoLock mon(data.mMonitor);
  data.mDrainComplete = true;
  mon.NotifyAll();
}

void
MP4Reader::InputExhausted(TrackType aTrack)
{
  DecoderData& data = GetDecoderData(aTrack);
  MonitorAutoLock mon(data.mMonitor);
  data.mInputExhausted = true;
  mon.NotifyAll();
}

void
MP4Reader::Error(TrackType aTrack)
{
  DecoderData& data = GetDecoderData(aTrack);
  MonitorAutoLock mon(data.mMonitor);
  data.mError = true;
  mon.NotifyAll();
}

bool
MP4Reader::DecodeAudioData()
{
  MOZ_ASSERT(HasAudio() && mPlatform && mAudio.mDecoder);
  return Decode(kAudio);
}

void
MP4Reader::Flush(TrackType aTrack)
{
  DecoderData& data = GetDecoderData(aTrack);
  if (!data.mDecoder) {
    return;
  }
  // Purge the current decoder's state.
  // Set a flag so that we ignore all output while we call
  // MediaDataDecoder::Flush().
  {
    MonitorAutoLock mon(data.mMonitor);
    data.mIsFlushing = true;
    data.mDrainComplete = false;
    data.mEOS = false;
  }
  data.mDecoder->Flush();
  {
    MonitorAutoLock mon(data.mMonitor);
    data.mIsFlushing = false;
  }
}

bool
MP4Reader::SkipVideoDemuxToNextKeyFrame(int64_t aTimeThreshold, uint32_t& parsed)
{
  MOZ_ASSERT(mVideo.mDecoder);

  Flush(kVideo);

  // Loop until we reach the next keyframe after the threshold.
  while (true) {
    nsAutoPtr<MP4Sample> compressed(PopSample(kVideo));
    if (!compressed) {
      // EOS, or error. Let the state machine know.
      return false;
    }
    parsed++;
    if (!compressed->is_sync_point ||
        compressed->composition_timestamp < aTimeThreshold) {
      continue;
    }
    mQueuedVideoSample = compressed;
    break;
  }

  return true;
}

bool
MP4Reader::DecodeVideoFrame(bool &aKeyframeSkip,
                            int64_t aTimeThreshold)
{
  // Record number of frames decoded and parsed. Automatically update the
  // stats counters using the AutoNotifyDecoded stack-based class.
  uint32_t parsed = 0, decoded = 0;
  AbstractMediaDecoder::AutoNotifyDecoded autoNotify(mDecoder, parsed, decoded);

  MOZ_ASSERT(HasVideo() && mPlatform && mVideo.mDecoder);

  if (aKeyframeSkip) {
    bool ok = SkipVideoDemuxToNextKeyFrame(aTimeThreshold, parsed);
    if (!ok) {
      NS_WARNING("Failed to skip demux up to next keyframe");
      return false;
    }
    aKeyframeSkip = false;
    nsresult rv = mVideo.mDecoder->Flush();
    NS_ENSURE_SUCCESS(rv, false);
  }

  bool rv = Decode(kVideo);
  {
    // Report the number of "decoded" frames as the difference in the
    // mNumSamplesOutput field since the last time we were called.
    MonitorAutoLock mon(mVideo.mMonitor);
    uint64_t delta = mVideo.mNumSamplesOutput - mLastReportedNumDecodedFrames;
    decoded = static_cast<uint32_t>(delta);
    mLastReportedNumDecodedFrames = mVideo.mNumSamplesOutput;
  }
  return rv;
}

nsresult
MP4Reader::Seek(int64_t aTime,
                int64_t aStartTime,
                int64_t aEndTime,
                int64_t aCurrentTime)
{
  if (!mDecoder->GetResource()->IsTransportSeekable() || !mDemuxer->CanSeek()) {
    return NS_ERROR_FAILURE;
  }

  mQueuedVideoSample = nullptr;
  if (mDemuxer->HasValidVideo()) {
    mDemuxer->SeekVideo(aTime);
    mQueuedVideoSample = PopSample(kVideo);
  }
  if (mDemuxer->HasValidAudio()) {
    mDemuxer->SeekAudio(
      mQueuedVideoSample ? mQueuedVideoSample->composition_timestamp : aTime);
  }

  return NS_OK;
}

void
MP4Reader::NotifyDataArrived(const char* aBuffer, uint32_t aLength,
                             int64_t aOffset)
{
  if (NS_IsMainThread()) {
    GetTaskQueue()->Dispatch(NS_NewRunnableMethod(this, &MP4Reader::UpdateIndex));
  } else {
    UpdateIndex();
  }
}

void
MP4Reader::UpdateIndex()
{
  MonitorAutoLock mon(mIndexMonitor);
  if (!mIndexReady) {
    return;
  }

  MediaResource* resource = mDecoder->GetResource();
  resource->Pin();
  nsTArray<MediaByteRange> ranges;
  if (NS_SUCCEEDED(resource->GetCachedRanges(ranges))) {
    mDemuxer->UpdateIndex(ranges);
  }
  resource->Unpin();
}

int64_t
MP4Reader::GetEvictionOffset(double aTime)
{
  MonitorAutoLock mon(mIndexMonitor);
  if (!mIndexReady) {
    return 0;
  }

  return mDemuxer->GetEvictionOffset(aTime * 1000000.0);
}

nsresult
MP4Reader::GetBuffered(dom::TimeRanges* aBuffered, int64_t aStartTime)
{
  MonitorAutoLock mon(mIndexMonitor);
  if (!mIndexReady) {
    return NS_OK;
  }

  MediaResource* resource = mDecoder->GetResource();
  nsTArray<MediaByteRange> ranges;
  resource->Pin();
  nsresult rv = resource->GetCachedRanges(ranges);
  resource->Unpin();

  if (NS_SUCCEEDED(rv)) {
    nsTArray<Interval<Microseconds>> timeRanges;
    mDemuxer->ConvertByteRangesToTime(ranges, &timeRanges);
    for (size_t i = 0; i < timeRanges.Length(); i++) {
      aBuffered->Add((timeRanges[i].start - aStartTime) / 1000000.0,
                     (timeRanges[i].end - aStartTime) / 1000000.0);
    }
  }

  return NS_OK;
}

bool MP4Reader::IsDormantNeeded()
{
#ifdef MOZ_GONK_MEDIACODEC
  return mVideo.mDecoder && mVideo.mDecoder->IsDormantNeeded();
#endif
  return false;
}

void MP4Reader::ReleaseMediaResources()
{
#ifdef MOZ_GONK_MEDIACODEC
  // Before freeing a video codec, all video buffers needed to be released
  // even from graphics pipeline.
  VideoFrameContainer* container = mDecoder->GetVideoFrameContainer();
  if (container) {
    container->ClearCurrentFrame();
  }
  if (mVideo.mDecoder) {
    mVideo.mDecoder->ReleaseMediaResources();
  }
#endif
}

void MP4Reader::NotifyResourcesStatusChanged()
{
#ifdef MOZ_GONK_MEDIACODEC
  if (mDecoder) {
    mDecoder->NotifyWaitingForResourcesStatusChanged();
  }
#endif
}

} // namespace mozilla
