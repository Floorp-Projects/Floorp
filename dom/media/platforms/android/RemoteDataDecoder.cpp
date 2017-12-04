/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AndroidBridge.h"
#include "AndroidDecoderModule.h"
#include "JavaCallbacksSupport.h"
#include "SimpleMap.h"
#include "GLImages.h"
#include "MediaData.h"
#include "MediaInfo.h"
#include "VideoUtils.h"
#include "VPXDecoder.h"

#include "mozilla/Telemetry.h"
#include "nsIGfxInfo.h"
#include "nsPromiseFlatString.h"
#include "nsThreadUtils.h"
#include "prlog.h"

#include <jni.h>

#ifdef NIGHTLY_BUILD
#define DEBUG_SHUTDOWN(fmt, ...) printf_stderr("[DEBUG SHUTDOWN] %s: " fmt "\n", __func__, ##__VA_ARGS__)
#else
#define DEBUG_SHUTDOWN(...) do { } while (0)
#endif

#undef LOG
#define LOG(arg, ...)                                                          \
  MOZ_LOG(sAndroidDecoderModuleLog,                                            \
          mozilla::LogLevel::Debug,                                            \
          ("RemoteDataDecoder(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))

using namespace mozilla;
using namespace mozilla::gl;
using namespace mozilla::java;
using namespace mozilla::java::sdk;
using media::TimeUnit;

namespace mozilla {

class RemoteVideoDecoder : public RemoteDataDecoder
{
public:
  // Hold an output buffer and render it to the surface when the frame is sent
  // to compositor, or release it if not presented.
  class RenderOrReleaseOutput : public VideoData::Listener
  {
  public:
    RenderOrReleaseOutput(java::CodecProxy::Param aCodec,
                          java::Sample::Param aSample)
      : mCodec(aCodec)
      , mSample(aSample)
    {
    }

    ~RenderOrReleaseOutput()
    {
      ReleaseOutput(false);
    }

    void OnSentToCompositor() override
    {
      ReleaseOutput(true);
      mCodec = nullptr;
      mSample = nullptr;
    }

  private:
    void ReleaseOutput(bool aToRender)
    {
      if (mCodec && mSample) {
        mCodec->ReleaseOutput(mSample, aToRender);
      }
    }

    java::CodecProxy::GlobalRef mCodec;
    java::Sample::GlobalRef mSample;
  };


  class InputInfo
  {
  public:
    InputInfo() { }

    InputInfo(const int64_t aDurationUs, const gfx::IntSize& aImageSize, const gfx::IntSize& aDisplaySize)
      : mDurationUs(aDurationUs)
      , mImageSize(aImageSize)
      , mDisplaySize(aDisplaySize)
    {
    }

    int64_t mDurationUs;
    gfx::IntSize mImageSize;
    gfx::IntSize mDisplaySize;
  };

  class CallbacksSupport final : public JavaCallbacksSupport
  {
  public:
    CallbacksSupport(RemoteVideoDecoder* aDecoder) : mDecoder(aDecoder) { }

    void HandleInput(int64_t aTimestamp, bool aProcessed) override
    {
      mDecoder->UpdateInputStatus(aTimestamp, aProcessed);
    }

    void HandleOutput(Sample::Param aSample) override
    {
      UniquePtr<VideoData::Listener> releaseSample(
        new RenderOrReleaseOutput(mDecoder->mJavaDecoder, aSample));

      BufferInfo::LocalRef info = aSample->Info();

      int32_t flags;
      bool ok = NS_SUCCEEDED(info->Flags(&flags));

      int32_t offset;
      ok &= NS_SUCCEEDED(info->Offset(&offset));

      int32_t size;
      ok &= NS_SUCCEEDED(info->Size(&size));

      int64_t presentationTimeUs;
      ok &= NS_SUCCEEDED(info->PresentationTimeUs(&presentationTimeUs));

      if (!ok) {
        HandleError(MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                                RESULT_DETAIL("VideoCallBack::HandleOutput")));
        return;
      }


      InputInfo inputInfo;
      ok = mDecoder->mInputInfos.Find(presentationTimeUs, inputInfo);
      bool isEOS = !!(flags & MediaCodec::BUFFER_FLAG_END_OF_STREAM);
      if (!ok && !isEOS) {
        // Ignore output with no corresponding input.
        return;
      }

      if (ok && (size > 0 || presentationTimeUs >= 0)) {
        RefPtr<layers::Image> img = new SurfaceTextureImage(
          mDecoder->mSurfaceHandle, inputInfo.mImageSize, false /* NOT continuous */,
          gl::OriginPos::BottomLeft);

        RefPtr<VideoData> v = VideoData::CreateFromImage(
          inputInfo.mDisplaySize, offset,
          TimeUnit::FromMicroseconds(presentationTimeUs),
          TimeUnit::FromMicroseconds(inputInfo.mDurationUs),
          img, !!(flags & MediaCodec::BUFFER_FLAG_SYNC_FRAME),
          TimeUnit::FromMicroseconds(presentationTimeUs));

        v->SetListener(Move(releaseSample));
        mDecoder->UpdateOutputStatus(Move(v));
      }

      if (isEOS) {
        mDecoder->DrainComplete();
      }
    }

    void HandleError(const MediaResult& aError) override
    {
      mDecoder->Error(aError);
    }

    friend class RemoteDataDecoder;

  private:
    RemoteVideoDecoder* mDecoder;
  };

  RemoteVideoDecoder(const VideoInfo& aConfig,
                     MediaFormat::Param aFormat,
                     const nsString& aDrmStubId, TaskQueue* aTaskQueue)
    : RemoteDataDecoder(MediaData::Type::VIDEO_DATA, aConfig.mMimeType,
                        aFormat, aDrmStubId, aTaskQueue)
    , mConfig(aConfig)
  {
  }

  ~RemoteVideoDecoder() {
    if (mSurface) {
      SurfaceAllocator::DisposeSurface(mSurface);
    }
  }

  RefPtr<InitPromise> Init() override
  {
    mSurface = GeckoSurface::LocalRef(SurfaceAllocator::AcquireSurface(mConfig.mImage.width, mConfig.mImage.height, false));
    if (!mSurface) {
      return InitPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
    }

    mSurfaceHandle = mSurface->GetHandle();

    // Register native methods.
    JavaCallbacksSupport::Init();

    mJavaCallbacks = CodecProxy::NativeCallbacks::New();
    JavaCallbacksSupport::AttachNative(
      mJavaCallbacks, mozilla::MakeUnique<CallbacksSupport>(this));

    mJavaDecoder = CodecProxy::Create(false, // false indicates to create a decoder and true denotes encoder
                                      mFormat,
                                      mSurface,
                                      mJavaCallbacks,
                                      mDrmStubId);
    if (mJavaDecoder == nullptr) {
      return InitPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                                          __func__);
    }
    mIsCodecSupportAdaptivePlayback =
      mJavaDecoder->IsAdaptivePlaybackSupported();
    Telemetry::Accumulate(Telemetry::MEDIA_ANDROID_VIDEO_TUNNELING_SUPPORT,
                          mJavaDecoder->IsTunneledPlaybackSupported());

    return InitPromise::CreateAndResolve(TrackInfo::kVideoTrack, __func__);
  }

  RefPtr<MediaDataDecoder::FlushPromise> Flush() override
  {
    mInputInfos.Clear();
    mSeekTarget.reset();
    return RemoteDataDecoder::Flush();
  }

  RefPtr<MediaDataDecoder::DecodePromise> Decode(MediaRawData* aSample) override
  {
    const VideoInfo* config = aSample->mTrackInfo
                              ? aSample->mTrackInfo->GetAsVideoInfo()
                              : &mConfig;
    MOZ_ASSERT(config);

    InputInfo info(
      aSample->mDuration.ToMicroseconds(), config->mImage, config->mDisplay);
    mInputInfos.Insert(aSample->mTime.ToMicroseconds(), info);
    return RemoteDataDecoder::Decode(aSample);
  }

  bool SupportDecoderRecycling() const override
  {
    return mIsCodecSupportAdaptivePlayback;
  }

  void SetSeekThreshold(const TimeUnit& aTime) override
  {
    mSeekTarget = Some(aTime);
  }

  bool IsUsefulData(const RefPtr<MediaData>& aSample) override
  {
    AssertOnTaskQueue();
    if (!mSeekTarget) {
      return true;
    }
    if (aSample->GetEndTime() > mSeekTarget.value()) {
      mSeekTarget.reset();
      return true;
    }
    return false;
  }

private:
  const VideoInfo mConfig;
  GeckoSurface::GlobalRef mSurface;
  AndroidSurfaceTextureHandle mSurfaceHandle;
  SimpleMap<InputInfo> mInputInfos;
  bool mIsCodecSupportAdaptivePlayback = false;
  Maybe<TimeUnit> mSeekTarget;
};

class RemoteAudioDecoder : public RemoteDataDecoder
{
public:
  RemoteAudioDecoder(const AudioInfo& aConfig,
                     MediaFormat::Param aFormat,
                     const nsString& aDrmStubId, TaskQueue* aTaskQueue)
    : RemoteDataDecoder(MediaData::Type::AUDIO_DATA, aConfig.mMimeType,
                        aFormat, aDrmStubId, aTaskQueue)
    , mConfig(aConfig)
  {
    JNIEnv* const env = jni::GetEnvForThread();

    bool formatHasCSD = false;
    NS_ENSURE_SUCCESS_VOID(
      aFormat->ContainsKey(NS_LITERAL_STRING("csd-0"), &formatHasCSD));

    if (!formatHasCSD && aConfig.mCodecSpecificConfig->Length() >= 2) {
      jni::ByteBuffer::LocalRef buffer(env);
      buffer = jni::ByteBuffer::New(aConfig.mCodecSpecificConfig->Elements(),
                                    aConfig.mCodecSpecificConfig->Length());
      NS_ENSURE_SUCCESS_VOID(
        aFormat->SetByteBuffer(NS_LITERAL_STRING("csd-0"), buffer));
    }
  }

  RefPtr<InitPromise> Init() override
  {
    // Register native methods.
    JavaCallbacksSupport::Init();

    mJavaCallbacks = CodecProxy::NativeCallbacks::New();
    JavaCallbacksSupport::AttachNative(
      mJavaCallbacks, mozilla::MakeUnique<CallbacksSupport>(this));

    mJavaDecoder =
      CodecProxy::Create(false, mFormat, nullptr, mJavaCallbacks, mDrmStubId);
    if (mJavaDecoder == nullptr) {
      return InitPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                                          __func__);
    }

    return InitPromise::CreateAndResolve(TrackInfo::kAudioTrack, __func__);
  }

  ConversionRequired NeedsConversion() const override
  {
    return ConversionRequired::kNeedAnnexB;
  }

private:
  class CallbacksSupport final : public JavaCallbacksSupport
  {
  public:
    CallbacksSupport(RemoteAudioDecoder* aDecoder) : mDecoder(aDecoder) { }

    void HandleInput(int64_t aTimestamp, bool aProcessed) override
    {
      mDecoder->UpdateInputStatus(aTimestamp, aProcessed);
    }

    void HandleOutput(Sample::Param aSample) override
    {
      BufferInfo::LocalRef info = aSample->Info();

      int32_t flags;
      bool ok = NS_SUCCEEDED(info->Flags(&flags));

      int32_t offset;
      ok &= NS_SUCCEEDED(info->Offset(&offset));

      int64_t presentationTimeUs;
      ok &= NS_SUCCEEDED(info->PresentationTimeUs(&presentationTimeUs));

      int32_t size;
      ok &= NS_SUCCEEDED(info->Size(&size));

      if (!ok) {
        HandleError(MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                                RESULT_DETAIL("AudioCallBack::HandleOutput")));
        return;
      }

      if (size > 0) {
#ifdef MOZ_SAMPLE_TYPE_S16
        const int32_t numSamples = size / 2;
#else
#error We only support 16-bit integer PCM
#endif

        const int32_t numFrames = numSamples / mOutputChannels;
        AlignedAudioBuffer audio(numSamples);
        if (!audio) {
          mDecoder->Error(MediaResult(NS_ERROR_OUT_OF_MEMORY, __func__));
          return;
        }

        jni::ByteBuffer::LocalRef dest =
          jni::ByteBuffer::New(audio.get(), size);
        aSample->WriteToByteBuffer(dest);

        RefPtr<AudioData> data = new AudioData(
          0, TimeUnit::FromMicroseconds(presentationTimeUs),
          FramesToTimeUnit(numFrames, mOutputSampleRate), numFrames,
          Move(audio), mOutputChannels, mOutputSampleRate);

        mDecoder->UpdateOutputStatus(Move(data));
      }

      if ((flags & MediaCodec::BUFFER_FLAG_END_OF_STREAM) != 0) {
        mDecoder->DrainComplete();
      }
    }

    void HandleOutputFormatChanged(MediaFormat::Param aFormat) override
    {
      aFormat->GetInteger(NS_LITERAL_STRING("channel-count"), &mOutputChannels);
      AudioConfig::ChannelLayout layout(mOutputChannels);
      if (!layout.IsValid()) {
        mDecoder->Error(MediaResult(
          NS_ERROR_DOM_MEDIA_FATAL_ERR,
          RESULT_DETAIL("Invalid channel layout:%d", mOutputChannels)));
        return;
      }
      aFormat->GetInteger(NS_LITERAL_STRING("sample-rate"), &mOutputSampleRate);
      LOG("Audio output format changed: channels:%d sample rate:%d",
          mOutputChannels, mOutputSampleRate);
    }

    void HandleError(const MediaResult& aError) override
    {
      mDecoder->Error(aError);
    }

  private:
    RemoteAudioDecoder* mDecoder;
    int32_t mOutputChannels;
    int32_t mOutputSampleRate;
  };

  const AudioInfo mConfig;
};

already_AddRefed<MediaDataDecoder>
RemoteDataDecoder::CreateAudioDecoder(const CreateDecoderParams& aParams,
                                      const nsString& aDrmStubId,
                                      CDMProxy* aProxy)
{
  const AudioInfo& config = aParams.AudioConfig();
  MediaFormat::LocalRef format;
  NS_ENSURE_SUCCESS(
    MediaFormat::CreateAudioFormat(
      config.mMimeType, config.mRate, config.mChannels, &format),
    nullptr);

  RefPtr<MediaDataDecoder> decoder =
    new RemoteAudioDecoder(config, format, aDrmStubId, aParams.mTaskQueue);
  if (aProxy) {
    decoder = new EMEMediaDataDecoderProxy(aParams, decoder.forget(), aProxy);
  }
  return decoder.forget();
}

already_AddRefed<MediaDataDecoder>
RemoteDataDecoder::CreateVideoDecoder(const CreateDecoderParams& aParams,
                                      const nsString& aDrmStubId,
                                      CDMProxy* aProxy)
{
  const VideoInfo& config = aParams.VideoConfig();
  MediaFormat::LocalRef format;
  NS_ENSURE_SUCCESS(
    MediaFormat::CreateVideoFormat(TranslateMimeType(config.mMimeType),
                                   config.mDisplay.width,
                                   config.mDisplay.height,
                                   &format),
    nullptr);

  RefPtr<MediaDataDecoder> decoder = new RemoteVideoDecoder(
    config, format, aDrmStubId, aParams.mTaskQueue);
  if (aProxy) {
    decoder = new EMEMediaDataDecoderProxy(aParams, decoder.forget(), aProxy);
  }
  return decoder.forget();
}

RemoteDataDecoder::RemoteDataDecoder(MediaData::Type aType,
                                     const nsACString& aMimeType,
                                     MediaFormat::Param aFormat,
                                     const nsString& aDrmStubId,
                                     TaskQueue* aTaskQueue)
  : mType(aType)
  , mMimeType(aMimeType)
  , mFormat(aFormat)
  , mDrmStubId(aDrmStubId)
  , mTaskQueue(aTaskQueue)
  , mNumPendingInputs(0)
{
}

RefPtr<MediaDataDecoder::FlushPromise>
RemoteDataDecoder::Flush()
{
  RefPtr<RemoteDataDecoder> self = this;
  return InvokeAsync(mTaskQueue, __func__, [self, this]() {
    mDecodedData.Clear();
    mNumPendingInputs = 0;
    mDecodePromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
    mDrainPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
    mDrainStatus = DrainStatus::DRAINED;
    mJavaDecoder->Flush();
    return FlushPromise::CreateAndResolve(true, __func__);
  });
}

RefPtr<MediaDataDecoder::DecodePromise>
RemoteDataDecoder::Drain()
{
  RefPtr<RemoteDataDecoder> self = this;
  return InvokeAsync(mTaskQueue, __func__, [self, this]() {
    if (mShutdown) {
      return DecodePromise::CreateAndReject(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
    }
    RefPtr<DecodePromise> p = mDrainPromise.Ensure(__func__);
    if (mDrainStatus == DrainStatus::DRAINED) {
      // There's no operation to perform other than returning any already
      // decoded data.
      ReturnDecodedData();
      return p;
    }

    if (mDrainStatus == DrainStatus::DRAINING) {
      // Draining operation already pending, let it complete its course.
      return p;
    }

    BufferInfo::LocalRef bufferInfo;
    nsresult rv = BufferInfo::New(&bufferInfo);
    if (NS_FAILED(rv)) {
      return DecodePromise::CreateAndReject(NS_ERROR_OUT_OF_MEMORY, __func__);
    }
    mDrainStatus = DrainStatus::DRAINING;
    bufferInfo->Set(0, 0, -1, MediaCodec::BUFFER_FLAG_END_OF_STREAM);
    mJavaDecoder->Input(nullptr, bufferInfo, nullptr);
    return p;
  });
}

RefPtr<ShutdownPromise>
RemoteDataDecoder::Shutdown()
{
  LOG("");
  RefPtr<RemoteDataDecoder> self = this;
  return InvokeAsync(mTaskQueue, this, __func__,
                     &RemoteDataDecoder::ProcessShutdown);
}

RefPtr<ShutdownPromise>
RemoteDataDecoder::ProcessShutdown()
{
  AssertOnTaskQueue();
  mShutdown = true;
  if (mJavaDecoder) {
    mJavaDecoder->Release();
    mJavaDecoder = nullptr;
  }

  if (mJavaCallbacks) {
    JavaCallbacksSupport::GetNative(mJavaCallbacks)->Cancel();
    JavaCallbacksSupport::DisposeNative(mJavaCallbacks);
    mJavaCallbacks = nullptr;
  }

  mFormat = nullptr;

  DEBUG_SHUTDOWN("decoder=%p mime=%s", this, mMimeType.get());
  return ShutdownPromise::CreateAndResolve(true, __func__);
}

static CryptoInfo::LocalRef
GetCryptoInfoFromSample(const MediaRawData* aSample)
{
  auto& cryptoObj = aSample->mCrypto;

  if (!cryptoObj.mValid) {
    return nullptr;
  }

  CryptoInfo::LocalRef cryptoInfo;
  nsresult rv = CryptoInfo::New(&cryptoInfo);
  NS_ENSURE_SUCCESS(rv, nullptr);

  uint32_t numSubSamples = std::min<uint32_t>(
    cryptoObj.mPlainSizes.Length(), cryptoObj.mEncryptedSizes.Length());

  uint32_t totalSubSamplesSize = 0;
  for (auto& size : cryptoObj.mEncryptedSizes) {
    totalSubSamplesSize += size;
  }

  // mPlainSizes is uint16_t, need to transform to uint32_t first.
  nsTArray<uint32_t> plainSizes;
  for (auto& size : cryptoObj.mPlainSizes) {
    totalSubSamplesSize += size;
    plainSizes.AppendElement(size);
  }

  uint32_t codecSpecificDataSize = aSample->Size() - totalSubSamplesSize;
  // Size of codec specific data("CSD") for Android MediaCodec usage should be
  // included in the 1st plain size.
  plainSizes[0] += codecSpecificDataSize;

  static const int kExpectedIVLength = 16;
  auto tempIV(cryptoObj.mIV);
  auto tempIVLength = tempIV.Length();
  MOZ_ASSERT(tempIVLength <= kExpectedIVLength);
  for (size_t i = tempIVLength; i < kExpectedIVLength; i++) {
    // Padding with 0
    tempIV.AppendElement(0);
  }

  auto numBytesOfPlainData = mozilla::jni::IntArray::New(
    reinterpret_cast<int32_t*>(&plainSizes[0]), plainSizes.Length());

  auto numBytesOfEncryptedData = mozilla::jni::IntArray::New(
    reinterpret_cast<const int32_t*>(&cryptoObj.mEncryptedSizes[0]),
    cryptoObj.mEncryptedSizes.Length());
  auto iv = mozilla::jni::ByteArray::New(reinterpret_cast<int8_t*>(&tempIV[0]),
                                         tempIV.Length());
  auto keyId = mozilla::jni::ByteArray::New(
    reinterpret_cast<const int8_t*>(&cryptoObj.mKeyId[0]),
    cryptoObj.mKeyId.Length());
  cryptoInfo->Set(numSubSamples,
                  numBytesOfPlainData,
                  numBytesOfEncryptedData,
                  keyId,
                  iv,
                  MediaCodec::CRYPTO_MODE_AES_CTR);

  return cryptoInfo;
}

RefPtr<MediaDataDecoder::DecodePromise>
RemoteDataDecoder::Decode(MediaRawData* aSample)
{
  MOZ_ASSERT(aSample != nullptr);

  RefPtr<RemoteDataDecoder> self = this;
  RefPtr<MediaRawData> sample = aSample;
  return InvokeAsync(mTaskQueue, __func__, [self, sample]() {
    jni::ByteBuffer::LocalRef bytes = jni::ByteBuffer::New(
      const_cast<uint8_t*>(sample->Data()), sample->Size());

    BufferInfo::LocalRef bufferInfo;
    nsresult rv = BufferInfo::New(&bufferInfo);
    if (NS_FAILED(rv)) {
      return DecodePromise::CreateAndReject(
        MediaResult(NS_ERROR_OUT_OF_MEMORY, __func__), __func__);
    }
    bufferInfo->Set(0, sample->Size(), sample->mTime.ToMicroseconds(), 0);

    self->mDrainStatus = DrainStatus::DRAINABLE;
    return self->mJavaDecoder->Input(bytes, bufferInfo, GetCryptoInfoFromSample(sample))
           ? self->mDecodePromise.Ensure(__func__)
           : DecodePromise::CreateAndReject(
               MediaResult(NS_ERROR_OUT_OF_MEMORY, __func__), __func__);

  });
}

void
RemoteDataDecoder::UpdateInputStatus(int64_t aTimestamp, bool aProcessed)
{
  if (!mTaskQueue->IsCurrentThreadIn()) {
    nsresult rv =
      mTaskQueue->Dispatch(
        NewRunnableMethod<int64_t, bool>("RemoteDataDecoder::UpdateInputStatus",
                                         this,
                                         &RemoteDataDecoder::UpdateInputStatus,
                                         aTimestamp,
                                         aProcessed));
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    return;
  }
  AssertOnTaskQueue();
  if (mShutdown) {
    return;
  }

  if (!aProcessed) {
    mNumPendingInputs++;
  } else if (mNumPendingInputs > 0) {
    mNumPendingInputs--;
  }

  if (mNumPendingInputs == 0 || // Input has been processed, request the next one.
      !mDecodedData.IsEmpty()) { // Previous output arrived before Decode().
    ReturnDecodedData();
  }
}

void
RemoteDataDecoder::UpdateOutputStatus(RefPtr<MediaData>&& aSample)
{
  if (!mTaskQueue->IsCurrentThreadIn()) {
    nsresult rv =
      mTaskQueue->Dispatch(
        NewRunnableMethod<const RefPtr<MediaData>>("RemoteDataDecoder::UpdateOutputStatus",
                                                   this,
                                                   &RemoteDataDecoder::UpdateOutputStatus,
                                                   Move(aSample)));
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    return;
  }
  AssertOnTaskQueue();
  if (mShutdown) {
    return;
  }
  if (IsUsefulData(aSample)) {
    mDecodedData.AppendElement(Move(aSample));
  }
  ReturnDecodedData();
}

void
RemoteDataDecoder::ReturnDecodedData()
{
  AssertOnTaskQueue();
  MOZ_ASSERT(!mShutdown);

  // We only want to clear mDecodedData when we have resolved the promises.
  if (!mDecodePromise.IsEmpty()) {
    mDecodePromise.Resolve(mDecodedData, __func__);
    mDecodedData.Clear();
  } else if (!mDrainPromise.IsEmpty() &&
             (!mDecodedData.IsEmpty() || mDrainStatus == DrainStatus::DRAINED)) {
    mDrainPromise.Resolve(mDecodedData, __func__);
    mDecodedData.Clear();
  }
}

void
RemoteDataDecoder::DrainComplete()
{
  if (!mTaskQueue->IsCurrentThreadIn()) {
    nsresult rv =
      mTaskQueue->Dispatch(
        NewRunnableMethod("RemoteDataDecoder::DrainComplete",
                          this, &RemoteDataDecoder::DrainComplete));
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    return;
  }
  AssertOnTaskQueue();
  if (mShutdown) {
    return;
  }
  mDrainStatus = DrainStatus::DRAINED;
  ReturnDecodedData();
  // Make decoder accept input again.
  mJavaDecoder->Flush();
}

void
RemoteDataDecoder::Error(const MediaResult& aError)
{
  if (!mTaskQueue->IsCurrentThreadIn()) {
    nsresult rv =
      mTaskQueue->Dispatch(
        NewRunnableMethod<MediaResult>("RemoteDataDecoder::Error",
                                       this, &RemoteDataDecoder::Error,
                                       aError));
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    return;
  }
  AssertOnTaskQueue();
  if (mShutdown) {
    return;
  }
  mDecodePromise.RejectIfExists(aError, __func__);
  mDrainPromise.RejectIfExists(aError, __func__);
}

} // mozilla
