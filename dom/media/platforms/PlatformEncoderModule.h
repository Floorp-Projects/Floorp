/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(PlatformEncoderModule_h_)
#  define PlatformEncoderModule_h_

#  include "MediaData.h"
#  include "MediaInfo.h"
#  include "MediaResult.h"
#  include "mozilla/Attributes.h"
#  include "mozilla/Maybe.h"
#  include "mozilla/MozPromise.h"
#  include "mozilla/RefPtr.h"
#  include "mozilla/TaskQueue.h"
#  include "mozilla/dom/ImageBitmapBinding.h"
#  include "nsISupportsImpl.h"

namespace mozilla {

class MediaDataEncoder;
struct CreateEncoderParams;

class PlatformEncoderModule {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(PlatformEncoderModule)

  virtual already_AddRefed<MediaDataEncoder> CreateVideoEncoder(
      const CreateEncoderParams& aParams) const {
    return nullptr;
  };

  virtual already_AddRefed<MediaDataEncoder> CreateAudioEncoder(
      const CreateEncoderParams& aParams) const {
    return nullptr;
  };

  // Indicates if the PlatformDecoderModule supports encoding of aMimeType.
  virtual bool SupportsMimeType(const nsACString& aMimeType) const = 0;

 protected:
  PlatformEncoderModule() = default;
  virtual ~PlatformEncoderModule() = default;
};

class MediaDataEncoder {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaDataEncoder)

  enum class Usage {
    Realtime,  // For WebRTC
    Record     // For MediaRecoder
  };

  enum class CodecType {
    _BeginVideo_,
    H264,
    VP8,
    VP9,
    _EndVideo_,
    _BeginAudio_ = _EndVideo_,
    Opus,
    G722,
    _EndAudio_,
    Unknown,
  };

  struct H264Specific final {
    enum class ProfileLevel { BaselineAutoLevel, MainAutoLevel };

    const size_t mKeyframeInterval;
    const ProfileLevel mProfileLevel;

    H264Specific(const size_t aKeyframeInterval,
                 const ProfileLevel aProfileLevel)
        : mKeyframeInterval(aKeyframeInterval), mProfileLevel(aProfileLevel) {}
  };

  struct OpusSpecific final {
    enum class Application { Voip, Audio, RestricedLowDelay };

    const Application mApplication;
    const uint8_t mComplexity;  // from 0-10

    OpusSpecific(const Application aApplication, const uint8_t aComplexity)
        : mApplication(aApplication), mComplexity(aComplexity) {
      MOZ_ASSERT(mComplexity <= 10);
    }
  };

  static bool IsVideo(const CodecType aCodec) {
    return aCodec > CodecType::_BeginVideo_ && aCodec < CodecType::_EndVideo_;
  }
  static bool IsAudio(const CodecType aCodec) {
    return aCodec > CodecType::_BeginAudio_ && aCodec < CodecType::_EndAudio_;
  }

  using PixelFormat = dom::ImageBitmapFormat;
  // Sample rate for audio, framerate for video, and bitrate for both.
  using Rate = uint32_t;

  using InitPromise =
      MozPromise<TrackInfo::TrackType, MediaResult, /* IsExclusive = */ true>;
  using EncodedData = nsTArray<RefPtr<MediaRawData>>;
  using EncodePromise =
      MozPromise<EncodedData, MediaResult, /* IsExclusive = */ true>;

  // Initialize the encoder. It should be ready to encode once the returned
  // promise resolves. The encoder should do any initialization here, rather
  // than in its constructor or PlatformEncoderModule::Create*Encoder(),
  // so that if the client needs to shutdown during initialization,
  // it can call Shutdown() to cancel this operation. Any initialization
  // that requires blocking the calling thread in this function *must*
  // be done here so that it can be canceled by calling Shutdown()!
  virtual RefPtr<InitPromise> Init() = 0;

  // Inserts a sample into the encoder's encode pipeline. The EncodePromise it
  // returns will be resolved with already encoded MediaRawData at the moment,
  // or empty when there is none available yet.
  virtual RefPtr<EncodePromise> Encode(const MediaData* aSample) = 0;

  // Causes all complete samples in the pipeline that can be encoded to be
  // output. It indicates that there is no more input sample to insert.
  // This function is asynchronous.
  // The MediaDataEncoder shall resolve the pending EncodePromise with drained
  // samples. Drain will be called multiple times until the resolved
  // EncodePromise is empty which indicates that there are no more samples to
  // drain.
  virtual RefPtr<EncodePromise> Drain() = 0;

  // Cancels all init/encode/drain operations, and shuts down the encoder. The
  // platform encoder should clean up any resources it's using and release
  // memory etc. The shutdown promise will be resolved once the encoder has
  // completed shutdown. The client will delete the decoder once the promise is
  // resolved.
  // The ShutdownPromise must only ever be resolved.
  virtual RefPtr<ShutdownPromise> Shutdown() = 0;

  virtual RefPtr<GenericPromise> SetBitrate(Rate aBitsPerSec) {
    return GenericPromise::CreateAndResolve(true, __func__);
  }

  // Decoder needs to decide whether or not hardware acceleration is supported
  // after creating. It doesn't need to call Init() before calling this
  // function.
  virtual bool IsHardwareAccelerated(nsACString& aFailureReason) const {
    return false;
  }

  // Return the name of the MediaDataEncoder, only used for encoding.
  // May be accessed in a non thread-safe fashion.
  virtual nsCString GetDescriptionName() const = 0;

  friend class PlatformEncoderModule;

 protected:
  template <typename T>
  struct BaseConfig {
    const CodecType mCodecType;
    const Usage mUsage;
    const Rate mBitsPerSec;
    Maybe<T> mCodecSpecific;

    void SetCodecSpecific(const T& aCodecSpecific) {
      mCodecSpecific.emplace(aCodecSpecific);
    }

   protected:
    BaseConfig(const CodecType aCodecType, const Usage aUsage,
               const Rate aBitsPerSec)
        : mCodecType(aCodecType), mUsage(aUsage), mBitsPerSec(aBitsPerSec) {}

    virtual ~BaseConfig() = default;
  };

  template <typename T>
  struct VideoConfig final : public BaseConfig<T> {
    const gfx::IntSize mSize;
    const PixelFormat mSourcePixelFormat;
    const uint8_t mFramerate;
    VideoConfig(const CodecType aCodecType, const Usage aUsage,
                const gfx::IntSize& aSize, const PixelFormat aSourcePixelFormat,
                const uint8_t aFramerate, const Rate aBitrate)
        : BaseConfig<T>(aCodecType, aUsage, aBitrate),
          mSize(aSize),
          mSourcePixelFormat(aSourcePixelFormat),
          mFramerate(aFramerate) {}
  };

  template <typename T>
  struct AudioConfig final : public BaseConfig<T> {
    const uint8_t mNumChannels;
    const Rate mSampleRate;

    AudioConfig(const CodecType aCodecType, const Usage aUsage,
                const Rate aBitrate, const Rate aSampleRate,
                const uint8_t aNumChannels)
        : BaseConfig<T>(aCodecType, aUsage, aBitrate),
          mNumChannels(aNumChannels),
          mSampleRate(aSampleRate) {}
  };

  virtual ~MediaDataEncoder() = default;

 public:
  using H264Config = VideoConfig<H264Specific>;
};

struct MOZ_STACK_CLASS CreateEncoderParams final {
  union CodecSpecific {
    MediaDataEncoder::H264Specific mH264;
    MediaDataEncoder::OpusSpecific mOpus;

    explicit CodecSpecific(const MediaDataEncoder::H264Specific&& aH264)
        : mH264(aH264) {}
    explicit CodecSpecific(const MediaDataEncoder::OpusSpecific&& aOpus)
        : mOpus(aOpus) {}
  };

  CreateEncoderParams(const TrackInfo& aConfig,
                      const MediaDataEncoder::Usage aUsage,
                      const RefPtr<TaskQueue> aTaskQueue,
                      const MediaDataEncoder::PixelFormat aPixelFormat,
                      const uint8_t aFramerate,
                      const MediaDataEncoder::Rate aBitrate)
      : mConfig(aConfig),
        mUsage(aUsage),
        mTaskQueue(aTaskQueue),
        mPixelFormat(aPixelFormat),
        mFramerate(aFramerate),
        mBitrate(aBitrate) {
    MOZ_ASSERT(mTaskQueue);
  }

  template <typename... Ts>
  CreateEncoderParams(const TrackInfo& aConfig,
                      const MediaDataEncoder::Usage aUsage,
                      const RefPtr<TaskQueue> aTaskQueue,
                      const MediaDataEncoder::PixelFormat aPixelFormat,
                      const uint8_t aFramerate,
                      const MediaDataEncoder::Rate aBitrate,
                      const Ts&&... aCodecSpecific)
      : mConfig(aConfig),
        mUsage(aUsage),
        mTaskQueue(aTaskQueue),
        mPixelFormat(aPixelFormat),
        mFramerate(aFramerate),
        mBitrate(aBitrate) {
    MOZ_ASSERT(mTaskQueue);
    Set(std::forward<const Ts>(aCodecSpecific)...);
  }

  const MediaDataEncoder::H264Config ToH264Config() const {
    const VideoInfo* info = mConfig.GetAsVideoInfo();
    MOZ_ASSERT(info);

    auto config = MediaDataEncoder::H264Config(
        MediaDataEncoder::CodecType::H264, mUsage, info->mImage, mPixelFormat,
        mFramerate, mBitrate);
    if (mCodecSpecific) {
      config.SetCodecSpecific(mCodecSpecific.ref().mH264);
    }

    return config;
  }

  const TrackInfo& mConfig;
  const MediaDataEncoder::Usage mUsage;
  const RefPtr<TaskQueue> mTaskQueue;
  const MediaDataEncoder::PixelFormat mPixelFormat;
  const uint8_t mFramerate;
  const MediaDataEncoder::Rate mBitrate;
  Maybe<CodecSpecific> mCodecSpecific;

 private:
  template <typename T>
  void Set(const T&& aCodecSpecific) {
    mCodecSpecific.emplace(std::forward<const T>(aCodecSpecific));
  }
};

}  // namespace mozilla

#endif /* PlatformEncoderModule_h_ */
