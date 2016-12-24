/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaCodecDataDecoder.h"

#include "AndroidBridge.h"
#include "AndroidSurfaceTexture.h"
#include "GeneratedJNINatives.h"
#include "GLImages.h"

#include "MediaData.h"
#include "MediaInfo.h"
#include "VPXDecoder.h"

#include "nsThreadUtils.h"
#include "nsPromiseFlatString.h"
#include "nsIGfxInfo.h"

#include "prlog.h"

#include <jni.h>

#undef LOG
#define LOG(arg, ...) MOZ_LOG(sAndroidDecoderModuleLog, \
    mozilla::LogLevel::Debug, ("MediaCodecDataDecoder(%p)::%s: " arg, \
      this, __func__, ##__VA_ARGS__))

using namespace mozilla;
using namespace mozilla::gl;
using namespace mozilla::java;
using namespace mozilla::java::sdk;
using media::TimeUnit;

namespace mozilla {

#define INVOKE_CALLBACK(Func, ...) \
  if (mCallback) { \
    mCallback->Func(__VA_ARGS__); \
  } else { \
    NS_WARNING("Callback not set"); \
  }

static MediaCodec::LocalRef
CreateDecoder(const nsACString& aMimeType)
{
  MediaCodec::LocalRef codec;
  NS_ENSURE_SUCCESS(MediaCodec::CreateDecoderByType(TranslateMimeType(aMimeType),
                    &codec), nullptr);
  return codec;
}

class VideoDataDecoder : public MediaCodecDataDecoder
{
public:
  VideoDataDecoder(const VideoInfo& aConfig,
                   MediaFormat::Param aFormat,
                   MediaDataDecoderCallback* aCallback,
                   layers::ImageContainer* aImageContainer,
                   const nsString& aDrmStubId)
    : MediaCodecDataDecoder(MediaData::Type::VIDEO_DATA, aConfig.mMimeType,
                            aFormat, aCallback, aDrmStubId)
    , mImageContainer(aImageContainer)
    , mConfig(aConfig)
  {

  }

  const char* GetDescriptionName() const override
  {
    return "Android MediaCodec video decoder";
  }

  RefPtr<InitPromise> Init() override
  {
    mSurfaceTexture = AndroidSurfaceTexture::Create();
    if (!mSurfaceTexture) {
      NS_WARNING("Failed to create SurfaceTexture for video decode\n");
      return InitPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
    }

    if (NS_FAILED(InitDecoder(mSurfaceTexture->JavaSurface()))) {
      return InitPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
    }
    return InitPromise::CreateAndResolve(TrackInfo::kVideoTrack, __func__);
  }

  void Cleanup() override
  {
  }

  nsresult PostOutput(BufferInfo::Param aInfo, MediaFormat::Param aFormat,
                      const TimeUnit& aDuration) override
  {
    RefPtr<layers::Image> img =
      new SurfaceTextureImage(mSurfaceTexture.get(), mConfig.mDisplay,
                              gl::OriginPos::BottomLeft);

    nsresult rv;
    int32_t flags;
    NS_ENSURE_SUCCESS(rv = aInfo->Flags(&flags), rv);

    bool isSync = !!(flags & MediaCodec::BUFFER_FLAG_SYNC_FRAME);

    int32_t offset;
    NS_ENSURE_SUCCESS(rv = aInfo->Offset(&offset), rv);

    int64_t presentationTimeUs;
    NS_ENSURE_SUCCESS(rv = aInfo->PresentationTimeUs(&presentationTimeUs), rv);

    RefPtr<VideoData> v =
      VideoData::CreateFromImage(mConfig,
                                 offset,
                                 presentationTimeUs,
                                 aDuration.ToMicroseconds(),
                                 img,
                                 isSync,
                                 presentationTimeUs,
                                 gfx::IntRect(0, 0,
                                              mConfig.mDisplay.width,
                                              mConfig.mDisplay.height));
    INVOKE_CALLBACK(Output, v);
    return NS_OK;
  }

  bool SupportDecoderRecycling() const override { return mIsCodecSupportAdaptivePlayback; }

protected:
  layers::ImageContainer* mImageContainer;
  const VideoInfo& mConfig;
  RefPtr<AndroidSurfaceTexture> mSurfaceTexture;
};



class EMEVideoDataDecoder : public VideoDataDecoder {
public:
  EMEVideoDataDecoder(const VideoInfo& aConfig,
                      MediaFormat::Param aFormat,
                      MediaDataDecoderCallback* aCallback,
                      layers::ImageContainer* aImageContainer,
                      const nsString& aDrmStubId,
                      CDMProxy* aProxy,
                      TaskQueue* aTaskQueue)
   : VideoDataDecoder(aConfig, aFormat, aCallback, aImageContainer, aDrmStubId)
   , mSamplesWaitingForKey(new SamplesWaitingForKey(this, aCallback,
                                                    aTaskQueue, aProxy))
  {
  }

  void Input(MediaRawData* aSample) override;
  void Shutdown() override;

private:
  RefPtr<SamplesWaitingForKey> mSamplesWaitingForKey;
};

void
EMEVideoDataDecoder::Input(MediaRawData* aSample)
{
  if (mSamplesWaitingForKey->WaitIfKeyNotUsable(aSample)) {
    return;
  }
  VideoDataDecoder::Input(aSample);
}

void
EMEVideoDataDecoder::Shutdown()
{
  VideoDataDecoder::Shutdown();

  mSamplesWaitingForKey->BreakCycles();
  mSamplesWaitingForKey = nullptr;
}

class AudioDataDecoder : public MediaCodecDataDecoder
{
public:
  AudioDataDecoder(const AudioInfo& aConfig, MediaFormat::Param aFormat,
                   MediaDataDecoderCallback* aCallback,
                   const nsString& aDrmStubId)
    : MediaCodecDataDecoder(MediaData::Type::AUDIO_DATA, aConfig.mMimeType,
                            aFormat, aCallback, aDrmStubId)
  {
    JNIEnv* const env = jni::GetEnvForThread();

    jni::ByteBuffer::LocalRef buffer(env);
    NS_ENSURE_SUCCESS_VOID(aFormat->GetByteBuffer(NS_LITERAL_STRING("csd-0"),
                                                  &buffer));

    if (!buffer && aConfig.mCodecSpecificConfig->Length() >= 2) {
      buffer = jni::ByteBuffer::New(
          aConfig.mCodecSpecificConfig->Elements(),
          aConfig.mCodecSpecificConfig->Length());
      NS_ENSURE_SUCCESS_VOID(aFormat->SetByteBuffer(NS_LITERAL_STRING("csd-0"),
                                                    buffer));
    }
  }

  const char* GetDescriptionName() const override
  {
    return "android audio decoder";
  }

  nsresult Output(BufferInfo::Param aInfo, void* aBuffer,
                  MediaFormat::Param aFormat, const TimeUnit& aDuration)
  {
    // The output on Android is always 16-bit signed
    nsresult rv;
    int32_t numChannels;
    NS_ENSURE_SUCCESS(rv =
        aFormat->GetInteger(NS_LITERAL_STRING("channel-count"), &numChannels), rv);
    AudioConfig::ChannelLayout layout(numChannels);
    if (!layout.IsValid()) {
      return NS_ERROR_FAILURE;
    }

    int32_t sampleRate;
    NS_ENSURE_SUCCESS(rv =
        aFormat->GetInteger(NS_LITERAL_STRING("sample-rate"), &sampleRate), rv);

    int32_t size;
    NS_ENSURE_SUCCESS(rv = aInfo->Size(&size), rv);

    int32_t offset;
    NS_ENSURE_SUCCESS(rv = aInfo->Offset(&offset), rv);

#ifdef MOZ_SAMPLE_TYPE_S16
    const int32_t numSamples = size / 2;
#else
#error We only support 16-bit integer PCM
#endif

    const int32_t numFrames = numSamples / numChannels;
    AlignedAudioBuffer audio(numSamples);
    if (!audio) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    const uint8_t* bufferStart = static_cast<uint8_t*>(aBuffer) + offset;
    PodCopy(audio.get(), reinterpret_cast<const AudioDataValue*>(bufferStart),
            numSamples);

    int64_t presentationTimeUs;
    NS_ENSURE_SUCCESS(rv = aInfo->PresentationTimeUs(&presentationTimeUs), rv);

    RefPtr<AudioData> data = new AudioData(0, presentationTimeUs,
                                           aDuration.ToMicroseconds(),
                                           numFrames,
                                           Move(audio),
                                           numChannels,
                                           sampleRate);
    INVOKE_CALLBACK(Output, data);
    return NS_OK;
  }
};

class EMEAudioDataDecoder : public AudioDataDecoder {
public:
  EMEAudioDataDecoder(const AudioInfo& aConfig, MediaFormat::Param aFormat,
                      MediaDataDecoderCallback* aCallback, const nsString& aDrmStubId,
                      CDMProxy* aProxy, TaskQueue* aTaskQueue)
   : AudioDataDecoder(aConfig, aFormat, aCallback, aDrmStubId)
   , mSamplesWaitingForKey(new SamplesWaitingForKey(this, aCallback,
                                                    aTaskQueue, aProxy))
  {
  }

  void Input(MediaRawData* aSample) override;
  void Shutdown() override;

private:
  RefPtr<SamplesWaitingForKey> mSamplesWaitingForKey;
};

void
EMEAudioDataDecoder::Input(MediaRawData* aSample)
{
  if (mSamplesWaitingForKey->WaitIfKeyNotUsable(aSample)) {
    return;
  }
  AudioDataDecoder::Input(aSample);
}

void
EMEAudioDataDecoder::Shutdown()
{
  AudioDataDecoder::Shutdown();

  mSamplesWaitingForKey->BreakCycles();
  mSamplesWaitingForKey = nullptr;
}

MediaDataDecoder*
MediaCodecDataDecoder::CreateAudioDecoder(const AudioInfo& aConfig,
                                          java::sdk::MediaFormat::Param aFormat,
                                          MediaDataDecoderCallback* aCallback,
                                          const nsString& aDrmStubId,
                                          CDMProxy* aProxy,
                                          TaskQueue* aTaskQueue)
{
  if (!aProxy) {
    return new AudioDataDecoder(aConfig, aFormat, aCallback, aDrmStubId);
  } else {
    return new EMEAudioDataDecoder(aConfig,
                                   aFormat,
                                   aCallback,
                                   aDrmStubId,
                                   aProxy,
                                   aTaskQueue);
  }
}

MediaDataDecoder*
MediaCodecDataDecoder::CreateVideoDecoder(const VideoInfo& aConfig,
                                          java::sdk::MediaFormat::Param aFormat,
                                          MediaDataDecoderCallback* aCallback,
                                          layers::ImageContainer* aImageContainer,
                                          const nsString& aDrmStubId,
                                          CDMProxy* aProxy,
                                          TaskQueue* aTaskQueue)
{
  if (!aProxy) {
    return new VideoDataDecoder(aConfig, aFormat, aCallback, aImageContainer, aDrmStubId);
  } else {
    return new EMEVideoDataDecoder(aConfig,
                                   aFormat,
                                   aCallback,
                                   aImageContainer,
                                   aDrmStubId,
                                   aProxy,
                                   aTaskQueue);
  }
}

MediaCodecDataDecoder::MediaCodecDataDecoder(MediaData::Type aType,
                                             const nsACString& aMimeType,
                                             MediaFormat::Param aFormat,
                                             MediaDataDecoderCallback* aCallback,
                                             const nsString& aDrmStubId)
  : mType(aType)
  , mMimeType(aMimeType)
  , mFormat(aFormat)
  , mCallback(aCallback)
  , mInputBuffers(nullptr)
  , mOutputBuffers(nullptr)
  , mMonitor("MediaCodecDataDecoder::mMonitor")
  , mState(ModuleState::kDecoding)
  , mDrmStubId(aDrmStubId)
{
}

MediaCodecDataDecoder::~MediaCodecDataDecoder()
{
  Shutdown();
}

RefPtr<MediaDataDecoder::InitPromise>
MediaCodecDataDecoder::Init()
{
  nsresult rv = InitDecoder(nullptr);

  TrackInfo::TrackType type =
    (mType == MediaData::AUDIO_DATA ? TrackInfo::TrackType::kAudioTrack
                                    : TrackInfo::TrackType::kVideoTrack);

  return NS_SUCCEEDED(rv) ?
           InitPromise::CreateAndResolve(type, __func__) :
           InitPromise::CreateAndReject(
               NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
}

nsresult
MediaCodecDataDecoder::InitDecoder(Surface::Param aSurface)
{
  mDecoder = CreateDecoder(mMimeType);

  if (!mDecoder) {
    INVOKE_CALLBACK(Error,
                    MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__));
    return NS_ERROR_FAILURE;
  }

  // Check if the video codec supports adaptive playback or not.
  if (aSurface) {
    mIsCodecSupportAdaptivePlayback =
      java::HardwareCodecCapabilityUtils::CheckSupportsAdaptivePlayback(mDecoder,
        nsCString(TranslateMimeType(mMimeType)));
    if (mIsCodecSupportAdaptivePlayback) {
        // TODO: may need to find a way to not use hard code to decide the max w/h.
        mFormat->SetInteger(MediaFormat::KEY_MAX_WIDTH, 1920);
        mFormat->SetInteger(MediaFormat::KEY_MAX_HEIGHT, 1080);
    }
  }

  MediaCrypto::LocalRef crypto = MediaDrmProxy::GetMediaCrypto(mDrmStubId);
  bool hascrypto = !!crypto;
  LOG("Has(%d) MediaCrypto (%s)", hascrypto, NS_ConvertUTF16toUTF8(mDrmStubId).get());
  nsresult rv;
  NS_ENSURE_SUCCESS(rv = mDecoder->Configure(mFormat, aSurface, crypto, 0), rv);
  NS_ENSURE_SUCCESS(rv = mDecoder->Start(), rv);

  NS_ENSURE_SUCCESS(rv = ResetInputBuffers(), rv);
  NS_ENSURE_SUCCESS(rv = ResetOutputBuffers(), rv);

  nsCOMPtr<nsIRunnable> r = NewRunnableMethod(this, &MediaCodecDataDecoder::DecoderLoop);
  rv = NS_NewNamedThread("MC Decoder", getter_AddRefs(mThread), r);

  return rv;
}

// This is in usec, so that's 10ms.
static const int64_t kDecoderTimeout = 10000;

#define BREAK_ON_DECODER_ERROR() \
  if (NS_FAILED(res)) { \
    NS_WARNING("Exiting decoder loop due to exception"); \
    if (mState == ModuleState::kDrainDecoder) { \
      INVOKE_CALLBACK(DrainComplete); \
      SetState(ModuleState::kDecoding); \
    } \
    INVOKE_CALLBACK(Error, MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__)); \
    break; \
  }

nsresult
MediaCodecDataDecoder::GetInputBuffer(
    JNIEnv* aEnv, int aIndex, jni::Object::LocalRef* aBuffer)
{
  MOZ_ASSERT(aEnv);
  MOZ_ASSERT(!*aBuffer);

  int numTries = 2;

  while (numTries--) {
    *aBuffer = jni::Object::LocalRef::Adopt(
        aEnv->GetObjectArrayElement(mInputBuffers.Get(), aIndex));
    if (*aBuffer) {
      return NS_OK;
    }
    nsresult res = ResetInputBuffers();
    if (NS_FAILED(res)) {
      return res;
    }
  }
  return NS_ERROR_FAILURE;
}

bool
MediaCodecDataDecoder::WaitForInput()
{
  MonitorAutoLock lock(mMonitor);

  while (mState ==  ModuleState::kDecoding && mQueue.empty()) {
    // Signal that we require more input.
    INVOKE_CALLBACK(InputExhausted);
    lock.Wait();
  }

  return mState != ModuleState::kStopping;
}


already_AddRefed<MediaRawData>
MediaCodecDataDecoder::PeekNextSample()
{
  MonitorAutoLock lock(mMonitor);

  if (mState == ModuleState::kFlushing) {
    mDecoder->Flush();
    ClearQueue();
    SetState(ModuleState::kDecoding);
    lock.Notify();
    return nullptr;
  }

  if (mQueue.empty()) {
    if (mState == ModuleState::kDrainQueue) {
      SetState(ModuleState::kDrainDecoder);
    }
    return nullptr;
  }

  // We're not stopping or flushing, so try to get a sample.
  return RefPtr<MediaRawData>(mQueue.front()).forget();
}

nsresult
MediaCodecDataDecoder::QueueSample(const MediaRawData* aSample)
{
  MOZ_ASSERT(aSample);
  AutoLocalJNIFrame frame(jni::GetEnvForThread(), 1);

  // We have a sample, try to feed it to the decoder.
  int32_t inputIndex = -1;
  nsresult res = mDecoder->DequeueInputBuffer(kDecoderTimeout, &inputIndex);
  if (NS_FAILED(res)) {
    return res;
  }

  if (inputIndex < 0) {
    // There is no valid input buffer available.
    return NS_ERROR_FAILURE;
  }

  jni::Object::LocalRef buffer(frame.GetEnv());
  res = GetInputBuffer(frame.GetEnv(), inputIndex, &buffer);
  if (NS_FAILED(res)) {
    return res;
  }

  void* directBuffer = frame.GetEnv()->GetDirectBufferAddress(buffer.Get());

  MOZ_ASSERT(frame.GetEnv()->GetDirectBufferCapacity(buffer.Get()) >=
             aSample->Size(),
             "Decoder buffer is not large enough for sample");

  PodCopy(static_cast<uint8_t*>(directBuffer), aSample->Data(), aSample->Size());

  CryptoInfo::LocalRef cryptoInfo = GetCryptoInfoFromSample(aSample);
  if (cryptoInfo) {
    res = mDecoder->QueueSecureInputBuffer(inputIndex, 0, cryptoInfo,
                                           aSample->mTime, 0);
  } else {
    res = mDecoder->QueueInputBuffer(inputIndex, 0, aSample->Size(),
                                     aSample->mTime, 0);
  }

  if (NS_FAILED(res)) {
    return res;
  }

  mDurations.push_back(TimeUnit::FromMicroseconds(aSample->mDuration));
  return NS_OK;
}

nsresult
MediaCodecDataDecoder::QueueEOS()
{
  mMonitor.AssertCurrentThreadOwns();

  nsresult res = NS_OK;
  int32_t inputIndex = -1;
  res = mDecoder->DequeueInputBuffer(kDecoderTimeout, &inputIndex);
  if (NS_FAILED(res) || inputIndex < 0) {
    return res;
  }

  res = mDecoder->QueueInputBuffer(inputIndex, 0, 0, 0,
                                   MediaCodec::BUFFER_FLAG_END_OF_STREAM);
  if (NS_SUCCEEDED(res)) {
    SetState(ModuleState::kDrainWaitEOS);
    mMonitor.Notify();
  }
  return res;
}

void
MediaCodecDataDecoder::HandleEOS(int32_t aOutputStatus)
{
  MonitorAutoLock lock(mMonitor);

  if (mState ==  ModuleState::kDrainWaitEOS) {
    SetState(ModuleState::kDecoding);
    mMonitor.Notify();

    INVOKE_CALLBACK(DrainComplete);
  }

  mDecoder->ReleaseOutputBuffer(aOutputStatus, false);
}

Maybe<TimeUnit>
MediaCodecDataDecoder::GetOutputDuration()
{
  if (mDurations.empty()) {
    return Nothing();
  }
  const Maybe<TimeUnit> duration = Some(mDurations.front());
  mDurations.pop_front();
  return duration;
}

nsresult
MediaCodecDataDecoder::ProcessOutput(
    BufferInfo::Param aInfo, MediaFormat::Param aFormat, int32_t aStatus)
{
  AutoLocalJNIFrame frame(jni::GetEnvForThread(), 1);

  const Maybe<TimeUnit> duration = GetOutputDuration();
  if (!duration) {
    // Some devices report failure in QueueSample while actually succeeding at
    // it, in which case we get an output buffer without having a cached duration
    // (bug 1273523).
    return NS_OK;
  }

  const auto buffer = jni::Object::LocalRef::Adopt(
      frame.GetEnv()->GetObjectArrayElement(mOutputBuffers.Get(), aStatus));

  if (buffer) {
    // The buffer will be null on Android L if we are decoding to a Surface.
    void* directBuffer = frame.GetEnv()->GetDirectBufferAddress(buffer.Get());
    Output(aInfo, directBuffer, aFormat, duration.value());
  }

  // The Surface will be updated at this point (for video).
  mDecoder->ReleaseOutputBuffer(aStatus, true);
  PostOutput(aInfo, aFormat, duration.value());

  return NS_OK;
}

void
MediaCodecDataDecoder::DecoderLoop()
{
  bool isOutputDone = false;
  AutoLocalJNIFrame frame(jni::GetEnvForThread(), 1);
  MediaFormat::LocalRef outputFormat(frame.GetEnv());
  nsresult res = NS_OK;

  while (WaitForInput()) {
    RefPtr<MediaRawData> sample = PeekNextSample();

    {
      MonitorAutoLock lock(mMonitor);
      if (mState == ModuleState::kDrainDecoder) {
        MOZ_ASSERT(!sample, "Shouldn't have a sample when pushing EOF frame");
        res = QueueEOS();
        BREAK_ON_DECODER_ERROR();
      }
    }

    if (sample) {
      res = QueueSample(sample);
      if (NS_SUCCEEDED(res)) {
        // We've fed this into the decoder, so remove it from the queue.
        MonitorAutoLock lock(mMonitor);
        MOZ_RELEASE_ASSERT(mQueue.size(), "Queue may not be empty");
        mQueue.pop_front();
        isOutputDone = false;
      }
    }

    if (isOutputDone) {
      continue;
    }

    BufferInfo::LocalRef bufferInfo;
    nsresult res = BufferInfo::New(&bufferInfo);
    BREAK_ON_DECODER_ERROR();

    int32_t outputStatus = -1;
    res = mDecoder->DequeueOutputBuffer(bufferInfo, kDecoderTimeout,
                                        &outputStatus);
    BREAK_ON_DECODER_ERROR();

    if (outputStatus == MediaCodec::INFO_TRY_AGAIN_LATER) {
      // We might want to call mCallback->InputExhausted() here, but there seems
      // to be some possible bad interactions here with the threading.
    } else if (outputStatus == MediaCodec::INFO_OUTPUT_BUFFERS_CHANGED) {
      res = ResetOutputBuffers();
      BREAK_ON_DECODER_ERROR();
    } else if (outputStatus == MediaCodec::INFO_OUTPUT_FORMAT_CHANGED) {
      res = mDecoder->GetOutputFormat(ReturnTo(&outputFormat));
      BREAK_ON_DECODER_ERROR();
    } else if (outputStatus < 0) {
      NS_WARNING("Unknown error from decoder!");
      INVOKE_CALLBACK(Error,
                      MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                                       __func__));
      // Don't break here just in case it's recoverable. If it's not, other
      // stuff will fail later and we'll bail out.
    } else {
      // We have a valid buffer index >= 0 here.
      int32_t flags;
      nsresult res = bufferInfo->Flags(&flags);
      BREAK_ON_DECODER_ERROR();

      if (flags & MediaCodec::BUFFER_FLAG_END_OF_STREAM) {
        HandleEOS(outputStatus);
        isOutputDone = true;
        // We only queue empty EOF frames, so we're done for now.
        continue;
      }

      res = ProcessOutput(bufferInfo, outputFormat, outputStatus);
      BREAK_ON_DECODER_ERROR();
    }
  }

  Cleanup();

  // We're done.
  MonitorAutoLock lock(mMonitor);
  SetState(ModuleState::kShutdown);
  mMonitor.Notify();
}

const char*
MediaCodecDataDecoder::ModuleStateStr(ModuleState aState) {
    switch (aState) {
      case ModuleState::kDecoding:     return "Decoding";
      case ModuleState::kFlushing:     return "Flushing";
      case ModuleState::kDrainQueue:   return "DrainQueue";
      case ModuleState::kDrainDecoder: return "DrainDecoder";
      case ModuleState::kDrainWaitEOS: return "DrainWaitEOS";
      case ModuleState::kStopping:     return "Stopping";
      case ModuleState::kShutdown:     return "Shutdown";
      default: MOZ_ASSERT_UNREACHABLE("Invalid state.");
    }
    return "Unknown";
}

bool
MediaCodecDataDecoder::SetState(ModuleState aState)
{
  bool ok = true;

  if (mState == ModuleState::kShutdown) {
    ok = false;
  } else if (mState == ModuleState::kStopping) {
    ok = aState == ModuleState::kShutdown;
  } else if (aState == ModuleState::kDrainDecoder) {
    ok = mState == ModuleState::kDrainQueue;
  } else if (aState == ModuleState::kDrainWaitEOS) {
    ok = mState == ModuleState::kDrainDecoder;
  }

  if (ok) {
    LOG("%s -> %s", ModuleStateStr(mState), ModuleStateStr(aState));
    mState = aState;
  } else {
    LOG("Fail to transit from %s to %s state", ModuleStateStr(mState), ModuleStateStr(aState));
  }

  return ok;
}

void
MediaCodecDataDecoder::ClearQueue()
{
  mMonitor.AssertCurrentThreadOwns();

  mQueue.clear();
  mDurations.clear();
}

void
MediaCodecDataDecoder::Input(MediaRawData* aSample)
{
  MonitorAutoLock lock(mMonitor);
  mQueue.push_back(aSample);
  lock.NotifyAll();
}

nsresult
MediaCodecDataDecoder::ResetInputBuffers()
{
  return mDecoder->GetInputBuffers(ReturnTo(&mInputBuffers));
}

nsresult
MediaCodecDataDecoder::ResetOutputBuffers()
{
  return mDecoder->GetOutputBuffers(ReturnTo(&mOutputBuffers));
}

void
MediaCodecDataDecoder::Flush()
{
  MonitorAutoLock lock(mMonitor);
  if (!SetState(ModuleState::kFlushing)) {
    return;
  }
  lock.Notify();

  while (mState == ModuleState::kFlushing) {
    lock.Wait();
  }
}

void
MediaCodecDataDecoder::Drain()
{
  MonitorAutoLock lock(mMonitor);
  if (mState == ModuleState::kDrainDecoder ||
      mState == ModuleState::kDrainQueue) {
    return;
  }

  SetState(ModuleState::kDrainQueue);
  lock.Notify();
}


void
MediaCodecDataDecoder::Shutdown()
{
  MonitorAutoLock lock(mMonitor);

  SetState(ModuleState::kStopping);
  lock.Notify();

  while (mThread && mState != ModuleState::kShutdown) {
    lock.Wait();
  }

  if (mThread) {
    mThread->Shutdown();
    mThread = nullptr;
  }

  if (mDecoder) {
    mDecoder->Stop();
    mDecoder->Release();
    mDecoder = nullptr;
  }
}

} // mozilla
