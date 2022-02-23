/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteDataDecoder.h"

#include <jni.h>

#include "AndroidBridge.h"
#include "AndroidDecoderModule.h"
#include "EMEDecoderModule.h"
#include "GLImages.h"
#include "JavaCallbacksSupport.h"
#include "MediaData.h"
#include "MediaInfo.h"
#include "SimpleMap.h"
#include "VPXDecoder.h"
#include "VideoUtils.h"
#include "mozilla/java/CodecProxyWrappers.h"
#include "mozilla/java/GeckoSurfaceWrappers.h"
#include "mozilla/java/SampleBufferWrappers.h"
#include "mozilla/java/SampleWrappers.h"
#include "mozilla/java/SurfaceAllocatorWrappers.h"
#include "nsPromiseFlatString.h"
#include "nsThreadUtils.h"
#include "prlog.h"

#undef LOG
#define LOG(arg, ...)                                         \
  MOZ_LOG(sAndroidDecoderModuleLog, mozilla::LogLevel::Debug, \
          ("RemoteDataDecoder(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))

using namespace mozilla;
using namespace mozilla::gl;
using media::TimeUnit;

namespace mozilla {

// Hold a reference to the output buffer until we're ready to release it back to
// the Java codec (for rendering or not).
class RenderOrReleaseOutput {
 public:
  RenderOrReleaseOutput(java::CodecProxy::Param aCodec,
                        java::Sample::Param aSample)
      : mCodec(aCodec), mSample(aSample) {}

  virtual ~RenderOrReleaseOutput() { ReleaseOutput(false); }

 protected:
  void ReleaseOutput(bool aToRender) {
    if (mCodec && mSample) {
      mCodec->ReleaseOutput(mSample, aToRender);
      mCodec = nullptr;
      mSample = nullptr;
    }
  }

 private:
  java::CodecProxy::GlobalRef mCodec;
  java::Sample::GlobalRef mSample;
};

class RemoteVideoDecoder : public RemoteDataDecoder {
 public:
  // Render the output to the surface when the frame is sent
  // to compositor, or release it if not presented.
  class CompositeListener
      : private RenderOrReleaseOutput,
        public layers::SurfaceTextureImage::SetCurrentCallback {
   public:
    CompositeListener(java::CodecProxy::Param aCodec,
                      java::Sample::Param aSample)
        : RenderOrReleaseOutput(aCodec, aSample) {}

    void operator()(void) override { ReleaseOutput(true); }
  };

  class InputInfo {
   public:
    InputInfo() {}

    InputInfo(const int64_t aDurationUs, const gfx::IntSize& aImageSize,
              const gfx::IntSize& aDisplaySize)
        : mDurationUs(aDurationUs),
          mImageSize(aImageSize),
          mDisplaySize(aDisplaySize) {}

    int64_t mDurationUs;
    gfx::IntSize mImageSize;
    gfx::IntSize mDisplaySize;
  };

  class CallbacksSupport final : public JavaCallbacksSupport {
   public:
    explicit CallbacksSupport(RemoteVideoDecoder* aDecoder)
        : mDecoder(aDecoder) {}

    void HandleInput(int64_t aTimestamp, bool aProcessed) override {
      mDecoder->UpdateInputStatus(aTimestamp, aProcessed);
    }

    void HandleOutput(java::Sample::Param aSample,
                      java::SampleBuffer::Param aBuffer) override {
      MOZ_ASSERT(!aBuffer, "Video sample should be bufferless");
      // aSample will be implicitly converted into a GlobalRef.
      mDecoder->ProcessOutput(std::move(aSample));
    }

    void HandleError(const MediaResult& aError) override {
      mDecoder->Error(aError);
    }

    friend class RemoteDataDecoder;

   private:
    RemoteVideoDecoder* mDecoder;
  };

  RemoteVideoDecoder(const VideoInfo& aConfig,
                     java::sdk::MediaFormat::Param aFormat,
                     const nsString& aDrmStubId)
      : RemoteDataDecoder(MediaData::Type::VIDEO_DATA, aConfig.mMimeType,
                          aFormat, aDrmStubId),
        mConfig(aConfig) {}

  ~RemoteVideoDecoder() {
    if (mSurface) {
      java::SurfaceAllocator::DisposeSurface(mSurface);
    }
  }

  RefPtr<InitPromise> Init() override {
    mThread = GetCurrentSerialEventTarget();
    java::sdk::BufferInfo::LocalRef bufferInfo;
    if (NS_FAILED(java::sdk::BufferInfo::New(&bufferInfo)) || !bufferInfo) {
      return InitPromise::CreateAndReject(NS_ERROR_OUT_OF_MEMORY, __func__);
    }
    mInputBufferInfo = bufferInfo;

    mSurface =
        java::GeckoSurface::LocalRef(java::SurfaceAllocator::AcquireSurface(
            mConfig.mImage.width, mConfig.mImage.height, false));
    if (!mSurface) {
      return InitPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                                          __func__);
    }

    mSurfaceHandle = mSurface->GetHandle();

    // Register native methods.
    JavaCallbacksSupport::Init();

    mJavaCallbacks = java::CodecProxy::NativeCallbacks::New();
    if (!mJavaCallbacks) {
      return InitPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                                          __func__);
    }
    JavaCallbacksSupport::AttachNative(
        mJavaCallbacks, mozilla::MakeUnique<CallbacksSupport>(this));

    mJavaDecoder = java::CodecProxy::Create(
        false,  // false indicates to create a decoder and true denotes encoder
        mFormat, mSurface, mJavaCallbacks, mDrmStubId);
    if (mJavaDecoder == nullptr) {
      return InitPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                                          __func__);
    }
    mIsCodecSupportAdaptivePlayback =
        mJavaDecoder->IsAdaptivePlaybackSupported();
    mIsHardwareAccelerated = mJavaDecoder->IsHardwareAccelerated();
    return InitPromise::CreateAndResolve(TrackInfo::kVideoTrack, __func__);
  }

  RefPtr<MediaDataDecoder::FlushPromise> Flush() override {
    AssertOnThread();
    mInputInfos.Clear();
    mSeekTarget.reset();
    mLatestOutputTime.reset();
    return RemoteDataDecoder::Flush();
  }

  RefPtr<MediaDataDecoder::DecodePromise> Decode(
      MediaRawData* aSample) override {
    AssertOnThread();

    const VideoInfo* config =
        aSample->mTrackInfo ? aSample->mTrackInfo->GetAsVideoInfo() : &mConfig;
    MOZ_ASSERT(config);

    InputInfo info(aSample->mDuration.ToMicroseconds(), config->mImage,
                   config->mDisplay);
    mInputInfos.Insert(aSample->mTime.ToMicroseconds(), info);
    return RemoteDataDecoder::Decode(aSample);
  }

  bool SupportDecoderRecycling() const override {
    return mIsCodecSupportAdaptivePlayback;
  }

  void SetSeekThreshold(const TimeUnit& aTime) override {
    auto setter = [self = RefPtr{this}, aTime] {
      if (aTime.IsValid()) {
        self->mSeekTarget = Some(aTime);
      } else {
        self->mSeekTarget.reset();
      }
    };
    if (mThread->IsOnCurrentThread()) {
      setter();
    } else {
      nsCOMPtr<nsIRunnable> runnable = NS_NewRunnableFunction(
          "RemoteVideoDecoder::SetSeekThreshold", std::move(setter));
      nsresult rv = mThread->Dispatch(runnable.forget());
      MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
      Unused << rv;
    }
  }

  bool IsUsefulData(const RefPtr<MediaData>& aSample) override {
    AssertOnThread();

    if (mLatestOutputTime && aSample->mTime < mLatestOutputTime.value()) {
      return false;
    }

    const TimeUnit endTime = aSample->GetEndTime();
    if (mSeekTarget && endTime <= mSeekTarget.value()) {
      return false;
    }

    mSeekTarget.reset();
    mLatestOutputTime = Some(endTime);
    return true;
  }

  bool IsHardwareAccelerated(nsACString& aFailureReason) const override {
    return mIsHardwareAccelerated;
  }

  ConversionRequired NeedsConversion() const override {
    return ConversionRequired::kNeedAnnexB;
  }

 private:
  // Param and LocalRef are only valid for the duration of a JNI method call.
  // Use GlobalRef as the parameter type to keep the Java object referenced
  // until running.
  void ProcessOutput(java::Sample::GlobalRef&& aSample) {
    if (!mThread->IsOnCurrentThread()) {
      nsresult rv =
          mThread->Dispatch(NewRunnableMethod<java::Sample::GlobalRef&&>(
              "RemoteVideoDecoder::ProcessOutput", this,
              &RemoteVideoDecoder::ProcessOutput, std::move(aSample)));
      MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
      Unused << rv;
      return;
    }

    AssertOnThread();
    if (GetState() == State::SHUTDOWN) {
      aSample->Dispose();
      return;
    }

    UniquePtr<layers::SurfaceTextureImage::SetCurrentCallback> releaseSample(
        new CompositeListener(mJavaDecoder, aSample));

    java::sdk::BufferInfo::LocalRef info = aSample->Info();
    MOZ_ASSERT(info);

    int32_t flags;
    bool ok = NS_SUCCEEDED(info->Flags(&flags));

    int32_t offset;
    ok &= NS_SUCCEEDED(info->Offset(&offset));

    int32_t size;
    ok &= NS_SUCCEEDED(info->Size(&size));

    int64_t presentationTimeUs;
    ok &= NS_SUCCEEDED(info->PresentationTimeUs(&presentationTimeUs));

    if (!ok) {
      Error(MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                        RESULT_DETAIL("VideoCallBack::HandleOutput")));
      return;
    }

    InputInfo inputInfo;
    ok = mInputInfos.Find(presentationTimeUs, inputInfo);
    bool isEOS = !!(flags & java::sdk::MediaCodec::BUFFER_FLAG_END_OF_STREAM);
    if (!ok && !isEOS) {
      // Ignore output with no corresponding input.
      return;
    }

    if (ok && (size > 0 || presentationTimeUs >= 0)) {
      RefPtr<layers::Image> img = new layers::SurfaceTextureImage(
          mSurfaceHandle, inputInfo.mImageSize, false /* NOT continuous */,
          gl::OriginPos::BottomLeft, mConfig.HasAlpha());
      img->AsSurfaceTextureImage()->RegisterSetCurrentCallback(
          std::move(releaseSample));

      RefPtr<VideoData> v = VideoData::CreateFromImage(
          inputInfo.mDisplaySize, offset,
          TimeUnit::FromMicroseconds(presentationTimeUs),
          TimeUnit::FromMicroseconds(inputInfo.mDurationUs), img.forget(),
          !!(flags & java::sdk::MediaCodec::BUFFER_FLAG_SYNC_FRAME),
          TimeUnit::FromMicroseconds(presentationTimeUs));

      RemoteDataDecoder::UpdateOutputStatus(std::move(v));
    }

    if (isEOS) {
      DrainComplete();
    }
  }

  const VideoInfo mConfig;
  java::GeckoSurface::GlobalRef mSurface;
  AndroidSurfaceTextureHandle mSurfaceHandle;
  // Only accessed on reader's task queue.
  bool mIsCodecSupportAdaptivePlayback = false;
  // Can be accessed on any thread, but only written on during init.
  bool mIsHardwareAccelerated = false;
  // Accessed on mThread and reader's thread. SimpleMap however is
  // thread-safe, so it's okay to do so.
  SimpleMap<InputInfo> mInputInfos;
  // Only accessed on mThread.
  Maybe<TimeUnit> mSeekTarget;
  Maybe<TimeUnit> mLatestOutputTime;
};

class RemoteAudioDecoder : public RemoteDataDecoder {
 public:
  RemoteAudioDecoder(const AudioInfo& aConfig,
                     java::sdk::MediaFormat::Param aFormat,
                     const nsString& aDrmStubId)
      : RemoteDataDecoder(MediaData::Type::AUDIO_DATA, aConfig.mMimeType,
                          aFormat, aDrmStubId) {
    JNIEnv* const env = jni::GetEnvForThread();

    bool formatHasCSD = false;
    NS_ENSURE_SUCCESS_VOID(aFormat->ContainsKey(u"csd-0"_ns, &formatHasCSD));

    if (!formatHasCSD && aConfig.mCodecSpecificConfig->Length() >= 2) {
      jni::ByteBuffer::LocalRef buffer(env);
      buffer = jni::ByteBuffer::New(aConfig.mCodecSpecificConfig->Elements(),
                                    aConfig.mCodecSpecificConfig->Length());
      NS_ENSURE_SUCCESS_VOID(aFormat->SetByteBuffer(u"csd-0"_ns, buffer));
    }
  }

  RefPtr<InitPromise> Init() override {
    mThread = GetCurrentSerialEventTarget();
    java::sdk::BufferInfo::LocalRef bufferInfo;
    if (NS_FAILED(java::sdk::BufferInfo::New(&bufferInfo)) || !bufferInfo) {
      return InitPromise::CreateAndReject(NS_ERROR_OUT_OF_MEMORY, __func__);
    }
    mInputBufferInfo = bufferInfo;

    // Register native methods.
    JavaCallbacksSupport::Init();

    mJavaCallbacks = java::CodecProxy::NativeCallbacks::New();
    if (!mJavaCallbacks) {
      return InitPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                                          __func__);
    }
    JavaCallbacksSupport::AttachNative(
        mJavaCallbacks, mozilla::MakeUnique<CallbacksSupport>(this));

    mJavaDecoder = java::CodecProxy::Create(false, mFormat, nullptr,
                                            mJavaCallbacks, mDrmStubId);
    if (mJavaDecoder == nullptr) {
      return InitPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                                          __func__);
    }

    return InitPromise::CreateAndResolve(TrackInfo::kAudioTrack, __func__);
  }

  RefPtr<FlushPromise> Flush() override {
    AssertOnThread();
    mFirstDemuxedSampleTime.reset();
    return RemoteDataDecoder::Flush();
  }

  RefPtr<DecodePromise> Decode(MediaRawData* aSample) override {
    AssertOnThread();
    if (!mFirstDemuxedSampleTime) {
      MOZ_ASSERT(aSample->mTime.IsValid());
      mFirstDemuxedSampleTime.emplace(aSample->mTime);
    }
    return RemoteDataDecoder::Decode(aSample);
  }

 private:
  class CallbacksSupport final : public JavaCallbacksSupport {
   public:
    explicit CallbacksSupport(RemoteAudioDecoder* aDecoder)
        : mDecoder(aDecoder) {}

    void HandleInput(int64_t aTimestamp, bool aProcessed) override {
      mDecoder->UpdateInputStatus(aTimestamp, aProcessed);
    }

    void HandleOutput(java::Sample::Param aSample,
                      java::SampleBuffer::Param aBuffer) override {
      MOZ_ASSERT(aBuffer, "Audio sample should have buffer");
      // aSample will be implicitly converted into a GlobalRef.
      mDecoder->ProcessOutput(std::move(aSample), std::move(aBuffer));
    }

    void HandleOutputFormatChanged(
        java::sdk::MediaFormat::Param aFormat) override {
      int32_t outputChannels = 0;
      aFormat->GetInteger(u"channel-count"_ns, &outputChannels);
      AudioConfig::ChannelLayout layout(outputChannels);
      if (!layout.IsValid()) {
        mDecoder->Error(MediaResult(
            NS_ERROR_DOM_MEDIA_FATAL_ERR,
            RESULT_DETAIL("Invalid channel layout:%d", outputChannels)));
        return;
      }

      int32_t sampleRate = 0;
      aFormat->GetInteger(u"sample-rate"_ns, &sampleRate);
      LOG("Audio output format changed: channels:%d sample rate:%d",
          outputChannels, sampleRate);

      mDecoder->ProcessOutputFormatChange(outputChannels, sampleRate);
    }

    void HandleError(const MediaResult& aError) override {
      mDecoder->Error(aError);
    }

   private:
    RemoteAudioDecoder* mDecoder;
  };

  bool IsSampleTimeSmallerThanFirstDemuxedSampleTime(int64_t aTime) const {
    return mFirstDemuxedSampleTime->ToMicroseconds() > aTime;
  }

  bool ShouldDiscardSample(int64_t aSession) const {
    AssertOnThread();
    // HandleOutput() runs on Android binder thread pool and could be preempted
    // by RemoteDateDecoder task queue. That means ProcessOutput() could be
    // scheduled after Shutdown() or Flush(). We won't need the
    // sample which is returned after calling Shutdown() and Flush(). We can
    // check mFirstDemuxedSampleTime to know whether the Flush() has been
    // called, becasue it would be reset in Flush().
    return GetState() == State::SHUTDOWN || !mFirstDemuxedSampleTime ||
           mSession != aSession;
  }

  // Param and LocalRef are only valid for the duration of a JNI method call.
  // Use GlobalRef as the parameter type to keep the Java object referenced
  // until running.
  void ProcessOutput(java::Sample::GlobalRef&& aSample,
                     java::SampleBuffer::GlobalRef&& aBuffer) {
    if (!mThread->IsOnCurrentThread()) {
      nsresult rv =
          mThread->Dispatch(NewRunnableMethod<java::Sample::GlobalRef&&,
                                              java::SampleBuffer::GlobalRef&&>(
              "RemoteAudioDecoder::ProcessOutput", this,
              &RemoteAudioDecoder::ProcessOutput, std::move(aSample),
              std::move(aBuffer)));
      MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
      Unused << rv;
      return;
    }

    AssertOnThread();

    if (ShouldDiscardSample(aSample->Session()) || !aBuffer->IsValid()) {
      aSample->Dispose();
      return;
    }

    RenderOrReleaseOutput autoRelease(mJavaDecoder, aSample);

    java::sdk::BufferInfo::LocalRef info = aSample->Info();
    MOZ_ASSERT(info);

    int32_t flags = 0;
    bool ok = NS_SUCCEEDED(info->Flags(&flags));
    bool isEOS = !!(flags & java::sdk::MediaCodec::BUFFER_FLAG_END_OF_STREAM);

    int32_t offset;
    ok &= NS_SUCCEEDED(info->Offset(&offset));

    int64_t presentationTimeUs;
    ok &= NS_SUCCEEDED(info->PresentationTimeUs(&presentationTimeUs));

    int32_t size;
    ok &= NS_SUCCEEDED(info->Size(&size));

    if (!ok ||
        (IsSampleTimeSmallerThanFirstDemuxedSampleTime(presentationTimeUs) &&
         !isEOS)) {
      Error(MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__));
      return;
    }

    if (size > 0) {
#ifdef MOZ_SAMPLE_TYPE_S16
      const int32_t numSamples = size / 2;
#else
#  error We only support 16-bit integer PCM
#endif

      AlignedAudioBuffer audio(numSamples);
      if (!audio) {
        Error(MediaResult(NS_ERROR_OUT_OF_MEMORY, __func__));
        return;
      }

      jni::ByteBuffer::LocalRef dest = jni::ByteBuffer::New(audio.get(), size);
      aBuffer->WriteToByteBuffer(dest, offset, size);

      RefPtr<AudioData> data =
          new AudioData(0, TimeUnit::FromMicroseconds(presentationTimeUs),
                        std::move(audio), mOutputChannels, mOutputSampleRate);

      UpdateOutputStatus(std::move(data));
    }

    if (isEOS) {
      DrainComplete();
    }
  }

  void ProcessOutputFormatChange(int32_t aChannels, int32_t aSampleRate) {
    if (!mThread->IsOnCurrentThread()) {
      nsresult rv = mThread->Dispatch(NewRunnableMethod<int32_t, int32_t>(
          "RemoteAudioDecoder::ProcessOutputFormatChange", this,
          &RemoteAudioDecoder::ProcessOutputFormatChange, aChannels,
          aSampleRate));
      MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
      Unused << rv;
      return;
    }

    AssertOnThread();

    mOutputChannels = aChannels;
    mOutputSampleRate = aSampleRate;
  }

  int32_t mOutputChannels;
  int32_t mOutputSampleRate;
  Maybe<TimeUnit> mFirstDemuxedSampleTime;
};

already_AddRefed<MediaDataDecoder> RemoteDataDecoder::CreateAudioDecoder(
    const CreateDecoderParams& aParams, const nsString& aDrmStubId,
    CDMProxy* aProxy) {
  const AudioInfo& config = aParams.AudioConfig();
  java::sdk::MediaFormat::LocalRef format;
  NS_ENSURE_SUCCESS(
      java::sdk::MediaFormat::CreateAudioFormat(config.mMimeType, config.mRate,
                                                config.mChannels, &format),
      nullptr);

  RefPtr<MediaDataDecoder> decoder =
      new RemoteAudioDecoder(config, format, aDrmStubId);
  if (aProxy) {
    decoder = new EMEMediaDataDecoderProxy(aParams, decoder.forget(), aProxy);
  }
  return decoder.forget();
}

already_AddRefed<MediaDataDecoder> RemoteDataDecoder::CreateVideoDecoder(
    const CreateDecoderParams& aParams, const nsString& aDrmStubId,
    CDMProxy* aProxy) {
  const VideoInfo& config = aParams.VideoConfig();
  java::sdk::MediaFormat::LocalRef format;
  NS_ENSURE_SUCCESS(java::sdk::MediaFormat::CreateVideoFormat(
                        TranslateMimeType(config.mMimeType),
                        config.mImage.width, config.mImage.height, &format),
                    nullptr);

  RefPtr<MediaDataDecoder> decoder =
      new RemoteVideoDecoder(config, format, aDrmStubId);
  if (aProxy) {
    decoder = new EMEMediaDataDecoderProxy(aParams, decoder.forget(), aProxy);
  }
  return decoder.forget();
}

RemoteDataDecoder::RemoteDataDecoder(MediaData::Type aType,
                                     const nsACString& aMimeType,
                                     java::sdk::MediaFormat::Param aFormat,
                                     const nsString& aDrmStubId)
    : mType(aType),
      mMimeType(aMimeType),
      mFormat(aFormat),
      mDrmStubId(aDrmStubId),
      mSession(0),
      mNumPendingInputs(0) {}

RefPtr<MediaDataDecoder::FlushPromise> RemoteDataDecoder::Flush() {
  AssertOnThread();
  MOZ_ASSERT(GetState() != State::SHUTDOWN);

  mDecodedData = DecodedData();
  UpdatePendingInputStatus(PendingOp::CLEAR);
  mDecodePromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
  mDrainPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
  SetState(State::DRAINED);
  mJavaDecoder->Flush();
  return FlushPromise::CreateAndResolve(true, __func__);
}

RefPtr<MediaDataDecoder::DecodePromise> RemoteDataDecoder::Drain() {
  AssertOnThread();
  if (GetState() == State::SHUTDOWN) {
    return DecodePromise::CreateAndReject(NS_ERROR_DOM_MEDIA_CANCELED,
                                          __func__);
  }
  RefPtr<DecodePromise> p = mDrainPromise.Ensure(__func__);
  if (GetState() == State::DRAINED) {
    // There's no operation to perform other than returning any already
    // decoded data.
    ReturnDecodedData();
    return p;
  }

  if (GetState() == State::DRAINING) {
    // Draining operation already pending, let it complete its course.
    return p;
  }

  SetState(State::DRAINING);
  mInputBufferInfo->Set(0, 0, -1,
                        java::sdk::MediaCodec::BUFFER_FLAG_END_OF_STREAM);
  mSession = mJavaDecoder->Input(nullptr, mInputBufferInfo, nullptr);
  return p;
}

RefPtr<ShutdownPromise> RemoteDataDecoder::Shutdown() {
  LOG("");
  AssertOnThread();
  SetState(State::SHUTDOWN);
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

  return ShutdownPromise::CreateAndResolve(true, __func__);
}

using CryptoInfoResult = Result<java::sdk::CryptoInfo::LocalRef, nsresult>;

static CryptoInfoResult GetCryptoInfoFromSample(const MediaRawData* aSample) {
  auto& cryptoObj = aSample->mCrypto;
  java::sdk::CryptoInfo::LocalRef cryptoInfo;

  if (!cryptoObj.IsEncrypted()) {
    return CryptoInfoResult(cryptoInfo);
  }

  static bool supportsCBCS = java::CodecProxy::SupportsCBCS();
  if (cryptoObj.mCryptoScheme == CryptoScheme::Cbcs && !supportsCBCS) {
    return CryptoInfoResult(NS_ERROR_DOM_MEDIA_NOT_SUPPORTED_ERR);
  }

  nsresult rv = java::sdk::CryptoInfo::New(&cryptoInfo);
  NS_ENSURE_SUCCESS(rv, CryptoInfoResult(rv));

  uint32_t numSubSamples = std::min<uint32_t>(
      cryptoObj.mPlainSizes.Length(), cryptoObj.mEncryptedSizes.Length());

  uint32_t totalSubSamplesSize = 0;
  for (auto& size : cryptoObj.mPlainSizes) {
    totalSubSamplesSize += size;
  }
  for (auto& size : cryptoObj.mEncryptedSizes) {
    totalSubSamplesSize += size;
  }

  // Deep copy the plain sizes so we can modify them.
  nsTArray<uint32_t> plainSizes = cryptoObj.mPlainSizes.Clone();
  uint32_t codecSpecificDataSize = aSample->Size() - totalSubSamplesSize;
  // Size of codec specific data("CSD") for Android java::sdk::MediaCodec usage
  // should be included in the 1st plain size if it exists.
  if (codecSpecificDataSize > 0 && !plainSizes.IsEmpty()) {
    // This shouldn't overflow as the the plain size should be UINT16_MAX at
    // most, and the CSD should never be that large. Checked int acts like a
    // diagnostic assert here to help catch if we ever have insane inputs.
    CheckedUint32 newLeadingPlainSize{plainSizes[0]};
    newLeadingPlainSize += codecSpecificDataSize;
    plainSizes[0] = newLeadingPlainSize.value();
  }

  static const int kExpectedIVLength = 16;
  nsTArray<uint8_t> tempIV(kExpectedIVLength);
  jint mode;
  switch (cryptoObj.mCryptoScheme) {
    case CryptoScheme::None:
      mode = java::sdk::MediaCodec::CRYPTO_MODE_UNENCRYPTED;
      MOZ_ASSERT(cryptoObj.mIV.Length() <= kExpectedIVLength);
      tempIV.AppendElements(cryptoObj.mIV);
      break;
    case CryptoScheme::Cenc:
      mode = java::sdk::MediaCodec::CRYPTO_MODE_AES_CTR;
      MOZ_ASSERT(cryptoObj.mIV.Length() <= kExpectedIVLength);
      tempIV.AppendElements(cryptoObj.mIV);
      break;
    case CryptoScheme::Cbcs:
      mode = java::sdk::MediaCodec::CRYPTO_MODE_AES_CBC;
      MOZ_ASSERT(cryptoObj.mConstantIV.Length() <= kExpectedIVLength);
      tempIV.AppendElements(cryptoObj.mConstantIV);
      break;
  }
  auto tempIVLength = tempIV.Length();
  for (size_t i = tempIVLength; i < kExpectedIVLength; i++) {
    // Padding with 0
    tempIV.AppendElement(0);
  }

  cryptoInfo->Set(numSubSamples, mozilla::jni::IntArray::From(plainSizes),
                  mozilla::jni::IntArray::From(cryptoObj.mEncryptedSizes),
                  mozilla::jni::ByteArray::From(cryptoObj.mKeyId),
                  mozilla::jni::ByteArray::From(tempIV), mode);
  if (mode == java::sdk::MediaCodec::CRYPTO_MODE_AES_CBC) {
    java::CodecProxy::SetCryptoPatternIfNeeded(
        cryptoInfo, cryptoObj.mCryptByteBlock, cryptoObj.mSkipByteBlock);
  }

  return CryptoInfoResult(cryptoInfo);
}

RefPtr<MediaDataDecoder::DecodePromise> RemoteDataDecoder::Decode(
    MediaRawData* aSample) {
  AssertOnThread();
  MOZ_ASSERT(GetState() != State::SHUTDOWN);
  MOZ_ASSERT(aSample != nullptr);
  jni::ByteBuffer::LocalRef bytes = jni::ByteBuffer::New(
      const_cast<uint8_t*>(aSample->Data()), aSample->Size());

  SetState(State::DRAINABLE);
  mInputBufferInfo->Set(0, aSample->Size(), aSample->mTime.ToMicroseconds(), 0);
  CryptoInfoResult crypto = GetCryptoInfoFromSample(aSample);
  if (crypto.isErr()) {
    return DecodePromise::CreateAndReject(
        MediaResult(crypto.unwrapErr(), __func__), __func__);
  }
  int64_t session =
      mJavaDecoder->Input(bytes, mInputBufferInfo, crypto.unwrap());
  if (session == java::CodecProxy::INVALID_SESSION) {
    return DecodePromise::CreateAndReject(
        MediaResult(NS_ERROR_OUT_OF_MEMORY, __func__), __func__);
  }
  mSession = session;
  return mDecodePromise.Ensure(__func__);
}

void RemoteDataDecoder::UpdatePendingInputStatus(PendingOp aOp) {
  AssertOnThread();
  switch (aOp) {
    case PendingOp::INCREASE:
      mNumPendingInputs++;
      break;
    case PendingOp::DECREASE:
      mNumPendingInputs--;
      break;
    case PendingOp::CLEAR:
      mNumPendingInputs = 0;
      break;
  }
}

void RemoteDataDecoder::UpdateInputStatus(int64_t aTimestamp, bool aProcessed) {
  if (!mThread->IsOnCurrentThread()) {
    nsresult rv = mThread->Dispatch(NewRunnableMethod<int64_t, bool>(
        "RemoteDataDecoder::UpdateInputStatus", this,
        &RemoteDataDecoder::UpdateInputStatus, aTimestamp, aProcessed));
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    Unused << rv;
    return;
  }
  AssertOnThread();
  if (GetState() == State::SHUTDOWN) {
    return;
  }

  if (!aProcessed) {
    UpdatePendingInputStatus(PendingOp::INCREASE);
  } else if (HasPendingInputs()) {
    UpdatePendingInputStatus(PendingOp::DECREASE);
  }

  if (!HasPendingInputs() ||  // Input has been processed, request the next one.
      !mDecodedData.IsEmpty()) {  // Previous output arrived before Decode().
    ReturnDecodedData();
  }
}

void RemoteDataDecoder::UpdateOutputStatus(RefPtr<MediaData>&& aSample) {
  AssertOnThread();
  if (GetState() == State::SHUTDOWN) {
    return;
  }
  if (IsUsefulData(aSample)) {
    mDecodedData.AppendElement(std::move(aSample));
  }
  ReturnDecodedData();
}

void RemoteDataDecoder::ReturnDecodedData() {
  AssertOnThread();
  MOZ_ASSERT(GetState() != State::SHUTDOWN);

  // We only want to clear mDecodedData when we have resolved the promises.
  if (!mDecodePromise.IsEmpty()) {
    mDecodePromise.Resolve(std::move(mDecodedData), __func__);
    mDecodedData = DecodedData();
  } else if (!mDrainPromise.IsEmpty() &&
             (!mDecodedData.IsEmpty() || GetState() == State::DRAINED)) {
    mDrainPromise.Resolve(std::move(mDecodedData), __func__);
    mDecodedData = DecodedData();
  }
}

void RemoteDataDecoder::DrainComplete() {
  if (!mThread->IsOnCurrentThread()) {
    nsresult rv = mThread->Dispatch(
        NewRunnableMethod("RemoteDataDecoder::DrainComplete", this,
                          &RemoteDataDecoder::DrainComplete));
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    Unused << rv;
    return;
  }
  AssertOnThread();
  if (GetState() == State::SHUTDOWN) {
    return;
  }
  SetState(State::DRAINED);
  ReturnDecodedData();
}

void RemoteDataDecoder::Error(const MediaResult& aError) {
  if (!mThread->IsOnCurrentThread()) {
    nsresult rv = mThread->Dispatch(NewRunnableMethod<MediaResult>(
        "RemoteDataDecoder::Error", this, &RemoteDataDecoder::Error, aError));
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    Unused << rv;
    return;
  }
  AssertOnThread();
  if (GetState() == State::SHUTDOWN) {
    return;
  }
  mDecodePromise.RejectIfExists(aError, __func__);
  mDrainPromise.RejectIfExists(aError, __func__);
}

}  // namespace mozilla
#undef LOG
