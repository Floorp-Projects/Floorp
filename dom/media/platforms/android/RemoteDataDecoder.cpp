/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AndroidDecoderModule.h"
#include "AndroidBridge.h"
#include "AndroidSurfaceTexture.h"
#include "FennecJNINatives.h"
#include "GLImages.h"

#include "MediaData.h"
#include "MediaInfo.h"
#include "VideoUtils.h"
#include "VPXDecoder.h"

#include "nsThreadUtils.h"
#include "nsPromiseFlatString.h"
#include "nsIGfxInfo.h"

#include "prlog.h"

#include <jni.h>

#include <deque>

#undef LOG
#define LOG(arg, ...) MOZ_LOG(sAndroidDecoderModuleLog, \
    mozilla::LogLevel::Debug, ("RemoteDataDecoder(%p)::%s: " arg, \
      this, __func__, ##__VA_ARGS__))

using namespace mozilla;
using namespace mozilla::gl;
using namespace mozilla::java;
using namespace mozilla::java::sdk;
using media::TimeUnit;

namespace mozilla {

class JavaCallbacksSupport
  : public CodecProxy::NativeCallbacks::Natives<JavaCallbacksSupport>
{
public:
  typedef CodecProxy::NativeCallbacks::Natives<JavaCallbacksSupport> Base;
  using Base::AttachNative;

  JavaCallbacksSupport(MediaDataDecoderCallback* aDecoderCallback)
    : mDecoderCallback(aDecoderCallback)
  {
    MOZ_ASSERT(aDecoderCallback);
  }

  virtual ~JavaCallbacksSupport() {}

  void OnInputExhausted()
  {
    if (mDecoderCallback) {
      mDecoderCallback->InputExhausted();
    }
  }

  virtual void HandleOutput(Sample::Param aSample) = 0;

  void OnOutput(jni::Object::Param aSample)
  {
    if (mDecoderCallback) {
      HandleOutput(Sample::Ref::From(aSample));
    }
  }

  virtual void HandleOutputFormatChanged(MediaFormat::Param aFormat) {};

  void OnOutputFormatChanged(jni::Object::Param aFormat)
  {
    if (mDecoderCallback) {
      HandleOutputFormatChanged(MediaFormat::Ref::From(aFormat));
    }
  }

  void OnError(bool aIsFatal)
  {
    if (mDecoderCallback) {
      mDecoderCallback->Error(aIsFatal ?
        MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__) :
        MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR, __func__));
    }
  }

  void DisposeNative()
  {
    // TODO
  }

  void Cancel()
  {
    mDecoderCallback = nullptr;
  }

protected:
  MediaDataDecoderCallback* mDecoderCallback;
};

struct SampleTime final
{
  SampleTime(int64_t aStart, int64_t aDuration)
    : mStart(aStart)
    , mDuration(aDuration)
  {}

  int64_t mStart;
  int64_t mDuration;
};


class RemoteVideoDecoder final : public RemoteDataDecoder
{
public:
  class CallbacksSupport final : public JavaCallbacksSupport
  {
  public:
    CallbacksSupport(RemoteVideoDecoder* aDecoder, MediaDataDecoderCallback* aCallback)
      : JavaCallbacksSupport(aCallback)
      , mDecoder(aDecoder)
    {}

    virtual ~CallbacksSupport() {}

    void HandleOutput(Sample::Param aSample) override
    {
      Maybe<int64_t> durationUs = mDecoder->mInputDurations.Get();
      if (!durationUs) {
        return;
      }

      BufferInfo::LocalRef info = aSample->Info();

      int32_t flags;
      bool ok = NS_SUCCEEDED(info->Flags(&flags));
      MOZ_ASSERT(ok);

      int32_t offset;
      ok |= NS_SUCCEEDED(info->Offset(&offset));
      MOZ_ASSERT(ok);

      int64_t presentationTimeUs;
      ok |= NS_SUCCEEDED(info->PresentationTimeUs(&presentationTimeUs));
      MOZ_ASSERT(ok);

      int32_t size;
      ok |= NS_SUCCEEDED(info->Size(&size));
      MOZ_ASSERT(ok);

      NS_ENSURE_TRUE_VOID(ok);

      if (size > 0) {
        RefPtr<layers::Image> img =
          new SurfaceTextureImage(mDecoder->mSurfaceTexture.get(), mDecoder->mConfig.mDisplay,
                                  gl::OriginPos::BottomLeft);

        RefPtr<VideoData> v =
          VideoData::CreateFromImage(mDecoder->mConfig,
                                    offset,
                                    presentationTimeUs,
                                    durationUs.value(),
                                    img,
                                    !!(flags & MediaCodec::BUFFER_FLAG_SYNC_FRAME),
                                    presentationTimeUs,
                                    gfx::IntRect(0, 0,
                                                  mDecoder->mConfig.mDisplay.width,
                                                  mDecoder->mConfig.mDisplay.height));

        mDecoderCallback->Output(v);
      }

      if ((flags & MediaCodec::BUFFER_FLAG_END_OF_STREAM) != 0) {
        mDecoderCallback->DrainComplete();
      }
    }

    friend class RemoteDataDecoder;

  private:
    RemoteVideoDecoder* mDecoder;
  };

  RemoteVideoDecoder(const VideoInfo& aConfig,
                   MediaFormat::Param aFormat,
                   MediaDataDecoderCallback* aCallback,
                   layers::ImageContainer* aImageContainer,
                   const nsString& aDrmStubId)
    : RemoteDataDecoder(MediaData::Type::VIDEO_DATA, aConfig.mMimeType,
                        aFormat, aCallback, aDrmStubId)
    , mImageContainer(aImageContainer)
    , mConfig(aConfig)
  {
  }

  RefPtr<InitPromise> Init() override
  {
    mSurfaceTexture = AndroidSurfaceTexture::Create();
    if (!mSurfaceTexture) {
      NS_WARNING("Failed to create SurfaceTexture for video decode\n");
      return InitPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
    }

    if (!jni::IsFennec()) {
      NS_WARNING("Remote decoding not supported in non-Fennec environment\n");
      return InitPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
    }

    // Register native methods.
    JavaCallbacksSupport::Init();

    mJavaCallbacks = CodecProxy::NativeCallbacks::New();
    JavaCallbacksSupport::AttachNative(mJavaCallbacks,
                                       mozilla::MakeUnique<CallbacksSupport>(this, mCallback));

    mJavaDecoder = CodecProxy::Create(mFormat,
                                      mSurfaceTexture->JavaSurface(),
                                      mJavaCallbacks,
                                      mDrmStubId);
    if (mJavaDecoder == nullptr) {
      return InitPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
    }
    mIsCodecSupportAdaptivePlayback = mJavaDecoder->IsAdaptivePlaybackSupported();
    mInputDurations.Clear();

    return InitPromise::CreateAndResolve(TrackInfo::kVideoTrack, __func__);
  }

  void Flush() override
  {
    mInputDurations.Clear();
    RemoteDataDecoder::Flush();
  }

  void Drain() override
  {
    RemoteDataDecoder::Drain();
    mInputDurations.Put(0);
  }

  void Input(MediaRawData* aSample) override
  {
    RemoteDataDecoder::Input(aSample);
    mInputDurations.Put(aSample->mDuration);
  }

  bool SupportDecoderRecycling() const override { return mIsCodecSupportAdaptivePlayback; }

private:
  class DurationQueue {
  public:

    void Clear()
    {
      mValues.clear();
    }

    void Put(int64_t aDurationUs)
    {
      mValues.emplace_back(aDurationUs);
    }

    Maybe<int64_t> Get()
    {
      if (mValues.empty()) {
        return Nothing();
      }

      auto value = Some(mValues.front());
      mValues.pop_front();

      return value;
    }

  private:
    std::deque<int64_t> mValues;
  };

  layers::ImageContainer* mImageContainer;
  const VideoInfo& mConfig;
  RefPtr<AndroidSurfaceTexture> mSurfaceTexture;
  DurationQueue mInputDurations;
  bool mIsCodecSupportAdaptivePlayback = false;
};

class RemoteAudioDecoder final : public RemoteDataDecoder
{
public:
  RemoteAudioDecoder(const AudioInfo& aConfig,
                     MediaFormat::Param aFormat,
                     MediaDataDecoderCallback* aCallback,
                     const nsString& aDrmStubId)
    : RemoteDataDecoder(MediaData::Type::AUDIO_DATA, aConfig.mMimeType,
                        aFormat, aCallback, aDrmStubId)
    , mConfig(aConfig)
  {
    JNIEnv* const env = jni::GetEnvForThread();

    bool formatHasCSD = false;
    NS_ENSURE_SUCCESS_VOID(aFormat->ContainsKey(NS_LITERAL_STRING("csd-0"), &formatHasCSD));

    if (!formatHasCSD && aConfig.mCodecSpecificConfig->Length() >= 2) {
      jni::ByteBuffer::LocalRef buffer(env);
      buffer = jni::ByteBuffer::New(
          aConfig.mCodecSpecificConfig->Elements(),
          aConfig.mCodecSpecificConfig->Length());
      NS_ENSURE_SUCCESS_VOID(aFormat->SetByteBuffer(NS_LITERAL_STRING("csd-0"),
                                                    buffer));
    }
  }

  RefPtr<InitPromise> Init() override
  {
    // Register native methods.
    JavaCallbacksSupport::Init();

    mJavaCallbacks = CodecProxy::NativeCallbacks::New();
    JavaCallbacksSupport::AttachNative(mJavaCallbacks,
                                       mozilla::MakeUnique<CallbacksSupport>(this, mCallback));

    mJavaDecoder = CodecProxy::Create(mFormat, nullptr, mJavaCallbacks, mDrmStubId);
    if (mJavaDecoder == nullptr) {
      return InitPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
    }

    return InitPromise::CreateAndResolve(TrackInfo::kAudioTrack, __func__);
  }

private:
  class CallbacksSupport final : public JavaCallbacksSupport
  {
  public:
    CallbacksSupport(RemoteAudioDecoder* aDecoder, MediaDataDecoderCallback* aCallback)
      : JavaCallbacksSupport(aCallback)
      , mDecoder(aDecoder)
    {}

    virtual ~CallbacksSupport() {}

    void HandleOutput(Sample::Param aSample) override
    {
      BufferInfo::LocalRef info = aSample->Info();

      int32_t flags;
      bool ok = NS_SUCCEEDED(info->Flags(&flags));
      MOZ_ASSERT(ok);

      int32_t offset;
      ok |= NS_SUCCEEDED(info->Offset(&offset));
      MOZ_ASSERT(ok);

      int64_t presentationTimeUs;
      ok |= NS_SUCCEEDED(info->PresentationTimeUs(&presentationTimeUs));
      MOZ_ASSERT(ok);

      int32_t size;
      ok |= NS_SUCCEEDED(info->Size(&size));
      MOZ_ASSERT(ok);

      NS_ENSURE_TRUE_VOID(ok);

      if (size > 0) {
#ifdef MOZ_SAMPLE_TYPE_S16
        const int32_t numSamples = size / 2;
#else
#error We only support 16-bit integer PCM
#endif

        const int32_t numFrames = numSamples / mOutputChannels;
        AlignedAudioBuffer audio(numSamples);
        if (!audio) {
          return;
        }

        jni::ByteBuffer::LocalRef dest = jni::ByteBuffer::New(audio.get(), size);
        aSample->WriteToByteBuffer(dest);

        RefPtr<AudioData> data = new AudioData(0, presentationTimeUs,
                                              FramesToUsecs(numFrames, mOutputSampleRate).value(),
                                              numFrames,
                                              Move(audio),
                                              mOutputChannels,
                                              mOutputSampleRate);

        mDecoderCallback->Output(data);
      }

      if ((flags & MediaCodec::BUFFER_FLAG_END_OF_STREAM) != 0) {
        mDecoderCallback->DrainComplete();
        return;
      }
    }

    void HandleOutputFormatChanged(MediaFormat::Param aFormat) override
    {
      aFormat->GetInteger(NS_LITERAL_STRING("channel-count"), &mOutputChannels);
      AudioConfig::ChannelLayout layout(mOutputChannels);
      if (!layout.IsValid()) {
        mDecoderCallback->Error(MediaResult(
          NS_ERROR_DOM_MEDIA_FATAL_ERR,
          RESULT_DETAIL("Invalid channel layout:%d", mOutputChannels)));
        return;
      }
      aFormat->GetInteger(NS_LITERAL_STRING("sample-rate"), &mOutputSampleRate);
      LOG("Audio output format changed: channels:%d sample rate:%d", mOutputChannels, mOutputSampleRate);
    }

  private:
    RemoteAudioDecoder* mDecoder;
    int32_t mOutputChannels;
    int32_t mOutputSampleRate;
  };

  const AudioInfo& mConfig;
};

MediaDataDecoder*
RemoteDataDecoder::CreateAudioDecoder(const AudioInfo& aConfig,
                                          MediaFormat::Param aFormat,
                                          MediaDataDecoderCallback* aCallback,
                                          const nsString& aDrmStubId)
{
  return new RemoteAudioDecoder(aConfig, aFormat, aCallback, aDrmStubId);
}

MediaDataDecoder*
RemoteDataDecoder::CreateVideoDecoder(const VideoInfo& aConfig,
                                          MediaFormat::Param aFormat,
                                          MediaDataDecoderCallback* aCallback,
                                          layers::ImageContainer* aImageContainer,
                                          const nsString& aDrmStubId)
{
  return new RemoteVideoDecoder(aConfig, aFormat, aCallback, aImageContainer, aDrmStubId);
}

RemoteDataDecoder::RemoteDataDecoder(MediaData::Type aType,
                                     const nsACString& aMimeType,
                                     MediaFormat::Param aFormat,
                                     MediaDataDecoderCallback* aCallback,
                                     const nsString& aDrmStubId)
  : mType(aType)
  , mMimeType(aMimeType)
  , mFormat(aFormat)
  , mCallback(aCallback)
  , mDrmStubId(aDrmStubId)
{
}

void
RemoteDataDecoder::Flush()
{
  mJavaDecoder->Flush();
}

void
RemoteDataDecoder::Drain()
{
  BufferInfo::LocalRef bufferInfo;
  nsresult rv = BufferInfo::New(&bufferInfo);
  NS_ENSURE_SUCCESS_VOID(rv);
  bufferInfo->Set(0, 0, -1, MediaCodec::BUFFER_FLAG_END_OF_STREAM);

  mJavaDecoder->Input(nullptr, bufferInfo, nullptr);
}

void
RemoteDataDecoder::Shutdown()
{
  LOG("");
  MOZ_ASSERT(mJavaDecoder && mJavaCallbacks);

  mJavaDecoder->Release();
  mJavaDecoder = nullptr;

  JavaCallbacksSupport::GetNative(mJavaCallbacks)->Cancel();
  mJavaCallbacks = nullptr;

  mFormat = nullptr;
}

void
RemoteDataDecoder::Input(MediaRawData* aSample)
{
  MOZ_ASSERT(aSample != nullptr);

  jni::ByteBuffer::LocalRef bytes = jni::ByteBuffer::New(const_cast<uint8_t*>(aSample->Data()),
                                                         aSample->Size());

  BufferInfo::LocalRef bufferInfo;
  nsresult rv = BufferInfo::New(&bufferInfo);
  if (NS_FAILED(rv)) {
    mCallback->Error(MediaResult(NS_ERROR_OUT_OF_MEMORY, __func__));
    return;
  }
  bufferInfo->Set(0, aSample->Size(), aSample->mTime, 0);

  mJavaDecoder->Input(bytes, bufferInfo, GetCryptoInfoFromSample(aSample));
}

} // mozilla
