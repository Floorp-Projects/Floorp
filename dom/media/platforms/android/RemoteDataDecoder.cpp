/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteDataDecoder.h"

#include <jni.h>

#include "AndroidBridge.h"
#include "AndroidBuild.h"
#include "AndroidDecoderModule.h"
#include "EMEDecoderModule.h"
#include "GLImages.h"
#include "JavaCallbacksSupport.h"
#include "MediaCodec.h"
#include "MediaData.h"
#include "MediaInfo.h"
#include "PerformanceRecorder.h"
#include "SimpleMap.h"
#include "VPXDecoder.h"
#include "VideoUtils.h"
#include "mozilla/fallible.h"
#include "mozilla/gfx/Matrix.h"
#include "mozilla/gfx/Types.h"
#include "mozilla/java/CodecProxyWrappers.h"
#include "mozilla/java/GeckoSurfaceWrappers.h"
#include "mozilla/java/SampleBufferWrappers.h"
#include "mozilla/java/SampleWrappers.h"
#include "mozilla/java/SurfaceAllocatorWrappers.h"
#include "mozilla/Maybe.h"
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

class RemoteVideoDecoder final : public RemoteDataDecoder {
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
    InputInfo() = default;

    InputInfo(const int64_t aDurationUs, const gfx::IntSize& aImageSize,
              const gfx::IntSize& aDisplaySize)
        : mDurationUs(aDurationUs),
          mImageSize(aImageSize),
          mDisplaySize(aDisplaySize) {}

    int64_t mDurationUs = {};
    gfx::IntSize mImageSize = {};
    gfx::IntSize mDisplaySize = {};
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
      mDecoder->ProcessOutput(aSample);
    }

    void HandleOutputFormatChanged(
        java::sdk::MediaFormat::Param aFormat) override {
      int32_t colorFormat = 0;
      aFormat->GetInteger(java::sdk::MediaFormat::KEY_COLOR_FORMAT,
                          &colorFormat);
      if (colorFormat == 0) {
        mDecoder->Error(
            MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                        RESULT_DETAIL("Invalid color format:%d", colorFormat)));
        return;
      }

      Maybe<int32_t> colorRange;
      {
        int32_t range = 0;
        if (NS_SUCCEEDED(aFormat->GetInteger(
                java::sdk::MediaFormat::KEY_COLOR_RANGE, &range))) {
          colorRange.emplace(range);
        }
      }

      Maybe<int32_t> colorSpace;
      {
        int32_t space = 0;
        if (NS_SUCCEEDED(aFormat->GetInteger(
                java::sdk::MediaFormat::KEY_COLOR_STANDARD, &space))) {
          colorSpace.emplace(space);
        }
      }

      mDecoder->ProcessOutputFormatChange(colorFormat, colorRange, colorSpace);
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
                     const nsString& aDrmStubId, Maybe<TrackingId> aTrackingId)
      : RemoteDataDecoder(MediaData::Type::VIDEO_DATA, aConfig.mMimeType,
                          aFormat, aDrmStubId),
        mConfig(aConfig),
        mTrackingId(std::move(aTrackingId)) {}

  ~RemoteVideoDecoder() {
    if (mSurface) {
      java::SurfaceAllocator::DisposeSurface(mSurface);
    }
  }

  RefPtr<InitPromise> Init() override {
    mThread = GetCurrentSerialEventTarget();
    java::sdk::MediaCodec::BufferInfo::LocalRef bufferInfo;
    if (NS_FAILED(java::sdk::MediaCodec::BufferInfo::New(&bufferInfo)) ||
        !bufferInfo) {
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

    // On some devices we have observed that the transform obtained from
    // SurfaceTexture.getTransformMatrix() is incorrect for surfaces produced by
    // a MediaCodec. We therefore override the transform to be a simple y-flip
    // to ensure it is rendered correctly.
    const auto hardware = java::sdk::Build::HARDWARE()->ToString();
    if (hardware.EqualsASCII("mt6735") || hardware.EqualsASCII("kirin980")) {
      mTransformOverride = Some(
          gfx::Matrix4x4::Scaling(1.0, -1.0, 1.0).PostTranslate(0.0, 1.0, 0.0));
    }

    mMediaInfoFlag = MediaInfoFlag::None;
    mMediaInfoFlag |= mIsHardwareAccelerated ? MediaInfoFlag::HardwareDecoding
                                             : MediaInfoFlag::SoftwareDecoding;
    if (mMimeType.EqualsLiteral("video/mp4") ||
        mMimeType.EqualsLiteral("video/avc")) {
      mMediaInfoFlag |= MediaInfoFlag::VIDEO_H264;
    } else if (mMimeType.EqualsLiteral("video/vp8")) {
      mMediaInfoFlag |= MediaInfoFlag::VIDEO_VP8;
    } else if (mMimeType.EqualsLiteral("video/vp9")) {
      mMediaInfoFlag |= MediaInfoFlag::VIDEO_VP9;
    } else if (mMimeType.EqualsLiteral("video/av1")) {
      mMediaInfoFlag |= MediaInfoFlag::VIDEO_AV1;
    }
    return InitPromise::CreateAndResolve(TrackInfo::kVideoTrack, __func__);
  }

  RefPtr<MediaDataDecoder::FlushPromise> Flush() override {
    AssertOnThread();
    mInputInfos.Clear();
    mSeekTarget.reset();
    mLatestOutputTime.reset();
    mPerformanceRecorder.Record(std::numeric_limits<int64_t>::max());
    return RemoteDataDecoder::Flush();
  }

  nsCString GetCodecName() const override {
    if (mMediaInfoFlag & MediaInfoFlag::VIDEO_H264) {
      return "h264"_ns;
    }
    if (mMediaInfoFlag & MediaInfoFlag::VIDEO_VP8) {
      return "vp8"_ns;
    }
    if (mMediaInfoFlag & MediaInfoFlag::VIDEO_VP9) {
      return "vp9"_ns;
    }
    if (mMediaInfoFlag & MediaInfoFlag::VIDEO_AV1) {
      return "av1"_ns;
    }
    return "unknown"_ns;
  }

  RefPtr<MediaDataDecoder::DecodePromise> Decode(
      MediaRawData* aSample) override {
    AssertOnThread();

    if (NeedsNewDecoder()) {
      return DecodePromise::CreateAndReject(NS_ERROR_DOM_MEDIA_NEED_NEW_DECODER,
                                            __func__);
    }

    const VideoInfo* config =
        aSample->mTrackInfo ? aSample->mTrackInfo->GetAsVideoInfo() : &mConfig;
    MOZ_ASSERT(config);

    mTrackingId.apply([&](const auto& aId) {
      MediaInfoFlag flag = mMediaInfoFlag;
      flag |= (aSample->mKeyframe ? MediaInfoFlag::KeyFrame
                                  : MediaInfoFlag::NonKeyFrame);
      mPerformanceRecorder.Start(aSample->mTime.ToMicroseconds(),
                                 "AndroidDecoder"_ns, aId, flag);
    });

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

    // If our output surface has been released (due to the GPU process crashing)
    // then request a new decoder, which will in turn allocate a new
    // Surface. This is usually be handled by the Error() callback, but on some
    // devices (or at least on the emulator) the java decoder does not raise an
    // error when the Surface is released. So we raise this error here as well.
    if (NeedsNewDecoder()) {
      Error(MediaResult(NS_ERROR_DOM_MEDIA_NEED_NEW_DECODER,
                        RESULT_DETAIL("VideoCallBack::HandleOutput")));
      return;
    }

    java::sdk::MediaCodec::BufferInfo::LocalRef info = aSample->Info();
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
          gl::OriginPos::BottomLeft, mConfig.HasAlpha(), mTransformOverride);
      img->AsSurfaceTextureImage()->RegisterSetCurrentCallback(
          std::move(releaseSample));

      RefPtr<VideoData> v = VideoData::CreateFromImage(
          inputInfo.mDisplaySize, offset,
          TimeUnit::FromMicroseconds(presentationTimeUs),
          TimeUnit::FromMicroseconds(inputInfo.mDurationUs), img.forget(),
          !!(flags & java::sdk::MediaCodec::BUFFER_FLAG_SYNC_FRAME),
          TimeUnit::FromMicroseconds(presentationTimeUs));

      mPerformanceRecorder.Record(presentationTimeUs, [&](DecodeStage& aStage) {
        using Cap = java::sdk::MediaCodecInfo::CodecCapabilities;
        using Fmt = java::sdk::MediaFormat;
        mColorFormat.apply([&](int32_t aFormat) {
          switch (aFormat) {
            case Cap::COLOR_Format32bitABGR8888:
            case Cap::COLOR_Format32bitARGB8888:
            case Cap::COLOR_Format32bitBGRA8888:
            case Cap::COLOR_FormatRGBAFlexible:
              aStage.SetImageFormat(DecodeStage::RGBA32);
              break;
            case Cap::COLOR_Format24bitBGR888:
            case Cap::COLOR_Format24bitRGB888:
            case Cap::COLOR_FormatRGBFlexible:
              aStage.SetImageFormat(DecodeStage::RGB24);
              break;
            case Cap::COLOR_FormatYUV411Planar:
            case Cap::COLOR_FormatYUV411PackedPlanar:
            case Cap::COLOR_FormatYUV420Planar:
            case Cap::COLOR_FormatYUV420PackedPlanar:
            case Cap::COLOR_FormatYUV420Flexible:
              aStage.SetImageFormat(DecodeStage::YUV420P);
              break;
            case Cap::COLOR_FormatYUV420SemiPlanar:
            case Cap::COLOR_FormatYUV420PackedSemiPlanar:
            case Cap::COLOR_QCOM_FormatYUV420SemiPlanar:
            case Cap::COLOR_TI_FormatYUV420PackedSemiPlanar:
              aStage.SetImageFormat(DecodeStage::NV12);
              break;
            case Cap::COLOR_FormatYCbYCr:
            case Cap::COLOR_FormatYCrYCb:
            case Cap::COLOR_FormatCbYCrY:
            case Cap::COLOR_FormatCrYCbY:
            case Cap::COLOR_FormatYUV422Planar:
            case Cap::COLOR_FormatYUV422PackedPlanar:
            case Cap::COLOR_FormatYUV422Flexible:
              aStage.SetImageFormat(DecodeStage::YUV422P);
              break;
            case Cap::COLOR_FormatYUV444Interleaved:
            case Cap::COLOR_FormatYUV444Flexible:
              aStage.SetImageFormat(DecodeStage::YUV444P);
              break;
            case Cap::COLOR_FormatSurface:
              aStage.SetImageFormat(DecodeStage::ANDROID_SURFACE);
              break;
            /* Added in API level 33
            case Cap::COLOR_FormatYUVP010:
              aStage.SetImageFormat(DecodeStage::P010);
              break;
            */
            default:
              NS_WARNING(nsPrintfCString("Unhandled color format %d (0x%08x)",
                                         aFormat, aFormat)
                             .get());
          }
        });
        mColorRange.apply([&](int32_t aRange) {
          switch (aRange) {
            case Fmt::COLOR_RANGE_FULL:
              aStage.SetColorRange(gfx::ColorRange::FULL);
              break;
            case Fmt::COLOR_RANGE_LIMITED:
              aStage.SetColorRange(gfx::ColorRange::LIMITED);
              break;
            default:
              NS_WARNING(nsPrintfCString("Unhandled color range %d (0x%08x)",
                                         aRange, aRange)
                             .get());
          }
        });
        mColorSpace.apply([&](int32_t aSpace) {
          switch (aSpace) {
            case Fmt::COLOR_STANDARD_BT2020:
              aStage.SetYUVColorSpace(gfx::YUVColorSpace::BT2020);
              break;
            case Fmt::COLOR_STANDARD_BT601_NTSC:
            case Fmt::COLOR_STANDARD_BT601_PAL:
              aStage.SetYUVColorSpace(gfx::YUVColorSpace::BT601);
              break;
            case Fmt::COLOR_STANDARD_BT709:
              aStage.SetYUVColorSpace(gfx::YUVColorSpace::BT709);
              break;
            default:
              NS_WARNING(nsPrintfCString("Unhandled color space %d (0x%08x)",
                                         aSpace, aSpace)
                             .get());
          }
        });
        aStage.SetResolution(v->mImage->GetSize().Width(),
                             v->mImage->GetSize().Height());
      });

      RemoteDataDecoder::UpdateOutputStatus(std::move(v));
    }

    if (isEOS) {
      DrainComplete();
    }
  }

  void ProcessOutputFormatChange(int32_t aColorFormat,
                                 Maybe<int32_t> aColorRange,
                                 Maybe<int32_t> aColorSpace) {
    if (!mThread->IsOnCurrentThread()) {
      nsresult rv = mThread->Dispatch(
          NewRunnableMethod<int32_t, Maybe<int32_t>, Maybe<int32_t>>(
              "RemoteVideoDecoder::ProcessOutputFormatChange", this,
              &RemoteVideoDecoder::ProcessOutputFormatChange, aColorFormat,
              aColorRange, aColorSpace));
      MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
      Unused << rv;
      return;
    }

    AssertOnThread();

    mColorFormat = Some(aColorFormat);
    mColorRange = aColorRange;
    mColorSpace = aColorSpace;
  }

  bool NeedsNewDecoder() const override {
    return !mSurface || mSurface->IsReleased();
  }

  const VideoInfo mConfig;
  java::GeckoSurface::GlobalRef mSurface;
  AndroidSurfaceTextureHandle mSurfaceHandle{};
  // Used to override the SurfaceTexture transform on some devices where the
  // decoder provides a buggy value.
  Maybe<gfx::Matrix4x4> mTransformOverride;
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
  Maybe<int32_t> mColorFormat;
  Maybe<int32_t> mColorRange;
  Maybe<int32_t> mColorSpace;
  // Only accessed on mThread.
  // Tracking id for the performance recorder.
  const Maybe<TrackingId> mTrackingId;
  // Can be accessed on any thread, but only written during init.
  // Pre-filled decode info used by the performance recorder.
  MediaInfoFlag mMediaInfoFlag = {};
  // Only accessed on mThread.
  // Records decode performance to the profiler.
  PerformanceRecorderMulti<DecodeStage> mPerformanceRecorder;
};

class RemoteAudioDecoder final : public RemoteDataDecoder {
 public:
  RemoteAudioDecoder(const AudioInfo& aConfig,
                     java::sdk::MediaFormat::Param aFormat,
                     const nsString& aDrmStubId)
      : RemoteDataDecoder(MediaData::Type::AUDIO_DATA, aConfig.mMimeType,
                          aFormat, aDrmStubId),
        mOutputChannels(AssertedCast<int32_t>(aConfig.mChannels)),
        mOutputSampleRate(AssertedCast<int32_t>(aConfig.mRate)) {
    JNIEnv* const env = jni::GetEnvForThread();

    bool formatHasCSD = false;
    NS_ENSURE_SUCCESS_VOID(aFormat->ContainsKey(u"csd-0"_ns, &formatHasCSD));

    // It would be nice to instead use more specific information here, but
    // we force a byte buffer for now since this handles arbitrary codecs.
    // TODO(bug 1768564): implement further type checking for codec data.
    RefPtr<MediaByteBuffer> audioCodecSpecificBinaryBlob =
        ForceGetAudioCodecSpecificBlob(aConfig.mCodecSpecificConfig);
    if (!formatHasCSD && audioCodecSpecificBinaryBlob->Length() >= 2) {
      jni::ByteBuffer::LocalRef buffer(env);
      buffer = jni::ByteBuffer::New(audioCodecSpecificBinaryBlob->Elements(),
                                    audioCodecSpecificBinaryBlob->Length());
      NS_ENSURE_SUCCESS_VOID(aFormat->SetByteBuffer(u"csd-0"_ns, buffer));
    }
  }

  RefPtr<InitPromise> Init() override {
    mThread = GetCurrentSerialEventTarget();
    java::sdk::MediaCodec::BufferInfo::LocalRef bufferInfo;
    if (NS_FAILED(java::sdk::MediaCodec::BufferInfo::New(&bufferInfo)) ||
        !bufferInfo) {
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

  nsCString GetCodecName() const override {
    if (mMimeType.EqualsLiteral("audio/mp4a-latm")) {
      return "aac"_ns;
    }
    return "unknown"_ns;
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
      mDecoder->ProcessOutput(aSample, aBuffer);
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

    LOG("ProcessOutput");

    if (ShouldDiscardSample(aSample->Session()) || !aBuffer->IsValid()) {
      aSample->Dispose();
      LOG("Discarding sample");
      return;
    }

    RenderOrReleaseOutput autoRelease(mJavaDecoder, aSample);

    java::sdk::MediaCodec::BufferInfo::LocalRef info = aSample->Info();
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
      LOG("ProcessOutput: decoding error ok[%s], pts[%" PRId64 "], eos[%s]",
          ok ? "true" : "false", presentationTimeUs, isEOS ? "true" : "false");
      Error(MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__));
      return;
    }

    if (size > 0) {
      const int32_t sampleSize = sizeof(int16_t);
      const int32_t numSamples = size / sampleSize;

      InflatableShortBuffer audio(numSamples);
      if (!audio) {
        Error(MediaResult(NS_ERROR_OUT_OF_MEMORY, __func__));
        LOG("OOM while allocating temporary output buffer");
        return;
      }
      jni::ByteBuffer::LocalRef dest = jni::ByteBuffer::New(audio.get(), size);
      aBuffer->WriteToByteBuffer(dest, offset, size);
      AlignedFloatBuffer converted = audio.Inflate();

      TimeUnit pts = TimeUnit::FromMicroseconds(presentationTimeUs);

      LOG("Decoded: %u frames of %s audio, pts: %s, %d channels, %" PRId32
          " Hz",
          numSamples / mOutputChannels,
          sampleSize == sizeof(int16_t) ? "int16" : "f32", pts.ToString().get(),
          mOutputChannels, mOutputSampleRate);

      RefPtr<AudioData> data = new AudioData(
          0, pts, std::move(converted), mOutputChannels, mOutputSampleRate);

      UpdateOutputStatus(std::move(data));
    } else {
      LOG("ProcessOutput but size 0");
    }

    if (isEOS) {
      LOG("EOS: drain complete");
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

  int32_t mOutputChannels{};
  int32_t mOutputSampleRate{};
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
  // format->SetInteger(java::sdk::MediaFormat::KEY_PCM_ENCODING,
  //                    java::sdk::AudioFormat::ENCODING_PCM_FLOAT);

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
      new RemoteVideoDecoder(config, format, aDrmStubId, aParams.mTrackingId);
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
  LOG("Shutdown");
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

using CryptoInfoResult =
    Result<java::sdk::MediaCodec::CryptoInfo::LocalRef, nsresult>;

static CryptoInfoResult GetCryptoInfoFromSample(const MediaRawData* aSample) {
  const auto& cryptoObj = aSample->mCrypto;
  java::sdk::MediaCodec::CryptoInfo::LocalRef cryptoInfo;

  if (!cryptoObj.IsEncrypted()) {
    return CryptoInfoResult(cryptoInfo);
  }

  static bool supportsCBCS = java::CodecProxy::SupportsCBCS();
  if (cryptoObj.mCryptoScheme == CryptoScheme::Cbcs && !supportsCBCS) {
    return CryptoInfoResult(NS_ERROR_DOM_MEDIA_NOT_SUPPORTED_ERR);
  }

  nsresult rv = java::sdk::MediaCodec::CryptoInfo::New(&cryptoInfo);
  NS_ENSURE_SUCCESS(rv, CryptoInfoResult(rv));

  uint32_t numSubSamples = std::min<uint32_t>(
      cryptoObj.mPlainSizes.Length(), cryptoObj.mEncryptedSizes.Length());

  uint32_t totalSubSamplesSize = 0;
  for (const auto& size : cryptoObj.mPlainSizes) {
    totalSubSamplesSize += size;
  }
  for (const auto& size : cryptoObj.mEncryptedSizes) {
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

  MOZ_ASSERT(numSubSamples <= INT32_MAX);
  cryptoInfo->Set(static_cast<int32_t>(numSubSamples),
                  mozilla::jni::IntArray::From(plainSizes),
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
      const_cast<uint8_t*>(aSample->Data()), aSample->Size(), fallible);
  if (!bytes) {
    return DecodePromise::CreateAndReject(
        MediaResult(NS_ERROR_OUT_OF_MEMORY, __func__), __func__);
  }

  SetState(State::DRAINABLE);
  MOZ_ASSERT(aSample->Size() <= INT32_MAX);
  mInputBufferInfo->Set(0, static_cast<int32_t>(aSample->Size()),
                        aSample->mTime.ToMicroseconds(), 0);
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
    LOG("Update output status, but decoder has been shut down, dropping the "
        "decoded results");
    return;
  }
  if (IsUsefulData(aSample)) {
    mDecodedData.AppendElement(std::move(aSample));
  } else {
    LOG("Decoded data, but not considered useful");
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

  // If we know we need a new decoder (eg because RemoteVideoDecoder's mSurface
  // has been released due to a GPU process crash) then override the error to
  // request a new decoder.
  const MediaResult& error =
      NeedsNewDecoder()
          ? MediaResult(NS_ERROR_DOM_MEDIA_NEED_NEW_DECODER, __func__)
          : aError;

  mDecodePromise.RejectIfExists(error, __func__);
  mDrainPromise.RejectIfExists(error, __func__);
}

}  // namespace mozilla
#undef LOG
