/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(PlatformEncoderModule_h_)
#  define PlatformEncoderModule_h_

#  include "MP4Decoder.h"
#  include "MediaData.h"
#  include "MediaInfo.h"
#  include "MediaResult.h"
#  include "VPXDecoder.h"
#  include "mozilla/Attributes.h"
#  include "mozilla/Maybe.h"
#  include "mozilla/MozPromise.h"
#  include "mozilla/RefPtr.h"
#  include "mozilla/TaskQueue.h"
#  include "mozilla/dom/ImageBitmapBinding.h"
#  include "nsISupportsImpl.h"
#  include "VideoUtils.h"

namespace mozilla {

class MediaDataEncoder;
class EncoderConfig;
struct EncoderConfigurationChangeList;

enum class CodecType {
  _BeginVideo_,
  H264,
  VP8,
  VP9,
  AV1,
  _EndVideo_,
  _BeginAudio_ = _EndVideo_,
  Opus,
  G722,
  _EndAudio_,
  Unknown,
};

// TODO: Automatically generate this (Bug 1865896)
const char* GetCodecTypeString(const CodecType& aCodecType);

enum class H264BitStreamFormat { AVC, ANNEXB };

struct H264Specific final {
  const H264_PROFILE mProfile;
  const H264_LEVEL mLevel;
  const H264BitStreamFormat mFormat;

  H264Specific(H264_PROFILE aProfile, H264_LEVEL aLevel,
               H264BitStreamFormat aFormat)
      : mProfile(aProfile), mLevel(aLevel), mFormat(aFormat) {}
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

enum class VPXComplexity { Normal, High, Higher, Max };
struct VP8Specific {
  VP8Specific() = default;
  // Ignore webrtc::VideoCodecVP8::errorConcealmentOn,
  // for it's always false in the codebase (except libwebrtc test cases).
  VP8Specific(const VPXComplexity aComplexity, const bool aResilience,
              const uint8_t aNumTemporalLayers, const bool aDenoising,
              const bool aAutoResize, const bool aFrameDropping)
      : mComplexity(aComplexity),
        mResilience(aResilience),
        mNumTemporalLayers(aNumTemporalLayers),
        mDenoising(aDenoising),
        mAutoResize(aAutoResize),
        mFrameDropping(aFrameDropping) {}
  const VPXComplexity mComplexity{VPXComplexity::Normal};
  const bool mResilience{true};
  const uint8_t mNumTemporalLayers{1};
  const bool mDenoising{true};
  const bool mAutoResize{false};
  const bool mFrameDropping{false};
};

struct VP9Specific : public VP8Specific {
  VP9Specific() = default;
  VP9Specific(const VPXComplexity aComplexity, const bool aResilience,
              const uint8_t aNumTemporalLayers, const bool aDenoising,
              const bool aAutoResize, const bool aFrameDropping,
              const bool aAdaptiveQp, const uint8_t aNumSpatialLayers,
              const bool aFlexible)
      : VP8Specific(aComplexity, aResilience, aNumTemporalLayers, aDenoising,
                    aAutoResize, aFrameDropping),
        mAdaptiveQp(aAdaptiveQp),
        mNumSpatialLayers(aNumSpatialLayers),
        mFlexible(aFlexible) {}
  const bool mAdaptiveQp{true};
  const uint8_t mNumSpatialLayers{1};
  const bool mFlexible{false};
};

class PlatformEncoderModule {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(PlatformEncoderModule)

  virtual already_AddRefed<MediaDataEncoder> CreateVideoEncoder(
      const EncoderConfig& aConfig, const RefPtr<TaskQueue>& aTaskQueue) const {
    return nullptr;
  };

  virtual already_AddRefed<MediaDataEncoder> CreateAudioEncoder(
      const EncoderConfig& aConfig, const RefPtr<TaskQueue>& aTaskQueue) const {
    return nullptr;
  };

  using CreateEncoderPromise = MozPromise<RefPtr<MediaDataEncoder>, MediaResult,
                                          /* IsExclusive = */ true>;

  // Indicates if the PlatformDecoderModule supports encoding of a codec.
  virtual bool Supports(const EncoderConfig& aConfig) const = 0;
  virtual bool SupportsCodec(CodecType aCodecType) const = 0;

  // Returns a readable name for this Platform Encoder Module
  virtual const char* GetName() const = 0;

  // Asychronously create an encoder
  RefPtr<PlatformEncoderModule::CreateEncoderPromise> AsyncCreateEncoder(
      const EncoderConfig& aEncoderConfig, const RefPtr<TaskQueue>& aTaskQueue);

 protected:
  PlatformEncoderModule() = default;
  virtual ~PlatformEncoderModule() = default;
};

class MediaDataEncoder {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaDataEncoder)

  enum class Usage {
    Realtime,  // Low latency prefered
    Record
  };
  using PixelFormat = dom::ImageBitmapFormat;
  enum class BitrateMode { Constant, Variable };
  // Scalable Video Coding (SVC) settings for WebCodecs:
  // https://www.w3.org/TR/webrtc-svc/
  enum class ScalabilityMode { None, L1T2, L1T3 };

  enum class HardwarePreference { RequireHardware, RequireSoftware, None };

  static bool IsVideo(const CodecType aCodec) {
    return aCodec > CodecType::_BeginVideo_ && aCodec < CodecType::_EndVideo_;
  }
  static bool IsAudio(const CodecType aCodec) {
    return aCodec > CodecType::_BeginAudio_ && aCodec < CodecType::_EndAudio_;
  }

  using InitPromise =
      MozPromise<TrackInfo::TrackType, MediaResult, /* IsExclusive = */ true>;
  using EncodedData = nsTArray<RefPtr<MediaRawData>>;
  using EncodePromise =
      MozPromise<EncodedData, MediaResult, /* IsExclusive = */ true>;
  using ReconfigurationPromise =
      MozPromise<bool, MediaResult, /* IsExclusive = */ true>;

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

  // Attempt to reconfigure the encoder on the fly. This can fail if the
  // underlying PEM doesn't support this type of reconfiguration.
  virtual RefPtr<ReconfigurationPromise> Reconfigure(
      const RefPtr<const EncoderConfigurationChangeList>&
          aConfigurationChanges) = 0;

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

  virtual RefPtr<GenericPromise> SetBitrate(uint32_t aBitsPerSec) {
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
  virtual ~MediaDataEncoder() = default;
};

class EncoderConfig final {
 public:
  using CodecSpecific =
      Variant<H264Specific, OpusSpecific, VP8Specific, VP9Specific>;

  EncoderConfig(const EncoderConfig& aConfig)
      : mCodec(aConfig.mCodec),
        mSize(aConfig.mSize),
        mUsage(aConfig.mUsage),
        mHardwarePreference(aConfig.mHardwarePreference),
        mPixelFormat(aConfig.mPixelFormat),
        mSourcePixelFormat(aConfig.mSourcePixelFormat),
        mScalabilityMode(aConfig.mScalabilityMode),
        mFramerate(aConfig.mFramerate),
        mKeyframeInterval(aConfig.mKeyframeInterval),
        mBitrate(aConfig.mBitrate),
        mBitrateMode(aConfig.mBitrateMode),
        mCodecSpecific(aConfig.mCodecSpecific) {}

  template <typename... Ts>
  EncoderConfig(const CodecType aCodecType, gfx::IntSize aSize,
                const MediaDataEncoder::Usage aUsage,
                const MediaDataEncoder::PixelFormat aPixelFormat,
                const MediaDataEncoder::PixelFormat aSourcePixelFormat,
                const uint8_t aFramerate, const size_t aKeyframeInterval,
                const uint32_t aBitrate,
                const MediaDataEncoder::BitrateMode aBitrateMode,
                const MediaDataEncoder::HardwarePreference aHardwarePreference,
                const MediaDataEncoder::ScalabilityMode aScalabilityMode,
                const Maybe<CodecSpecific>& aCodecSpecific)
      : mCodec(aCodecType),
        mSize(aSize),
        mUsage(aUsage),
        mHardwarePreference(aHardwarePreference),
        mPixelFormat(aPixelFormat),
        mSourcePixelFormat(aSourcePixelFormat),
        mScalabilityMode(aScalabilityMode),
        mFramerate(aFramerate),
        mKeyframeInterval(aKeyframeInterval),
        mBitrate(aBitrate),
        mBitrateMode(aBitrateMode),
        mCodecSpecific(aCodecSpecific) {}

  static CodecType CodecTypeForMime(const nsACString& aMimeType) {
    if (MP4Decoder::IsH264(aMimeType)) {
      return CodecType::H264;
    }
    if (VPXDecoder::IsVPX(aMimeType, VPXDecoder::VP8)) {
      return CodecType::VP8;
    }
    if (VPXDecoder::IsVPX(aMimeType, VPXDecoder::VP9)) {
      return CodecType::VP9;
    }
    MOZ_ASSERT_UNREACHABLE("Unsupported Mimetype");
    return CodecType::Unknown;
  }

  bool IsVideo() const {
    return mCodec > CodecType::_BeginVideo_ && mCodec < CodecType::_EndVideo_;
  }

  bool IsAudio() const {
    return mCodec > CodecType::_BeginAudio_ && mCodec < CodecType::_EndAudio_;
  }

  CodecType mCodec;
  gfx::IntSize mSize;
  MediaDataEncoder::Usage mUsage;
  MediaDataEncoder::HardwarePreference mHardwarePreference;
  MediaDataEncoder::PixelFormat mPixelFormat;
  MediaDataEncoder::PixelFormat mSourcePixelFormat;
  MediaDataEncoder::ScalabilityMode mScalabilityMode;
  uint8_t mFramerate{};
  size_t mKeyframeInterval{};
  uint32_t mBitrate{};
  MediaDataEncoder::BitrateMode mBitrateMode{};
  Maybe<CodecSpecific> mCodecSpecific;
};

// Wrap a type to make it unique. This allows using ergonomically in the Variant
// below. Simply aliasing with `using` isn't enough, because typedefs in C++
// don't produce strong types, so two integer variants result in
// the same type, making it ambiguous to the Variant code.
// T is the type to be wrapped. Phantom is a type that is only used to
// disambiguate and should be unique in the program.
template <typename T, typename Phantom>
class StrongTypedef {
 public:
  explicit StrongTypedef(T const& value) : mValue(value) {}
  explicit StrongTypedef(T&& value) : mValue(std::move(value)) {}
  T& get() { return mValue; }
  T const& get() const { return mValue; }

 private:
  T mValue;
};

// Dimensions of the video frames
using DimensionsChange =
    StrongTypedef<gfx::IntSize, struct DimensionsChangeType>;
// Expected display size of the encoded frames, can influence encoding
using DisplayDimensionsChange =
    StrongTypedef<Maybe<gfx::IntSize>, struct DisplayDimensionsChangeType>;
// If present, the bitrate in kbps of the encoded stream. If absent, let the
// platform decide.
using BitrateChange = StrongTypedef<Maybe<uint32_t>, struct BitrateChangeType>;
// If present, the expected framerate of the output video stream. If absent,
// infer from the input frames timestamp.
using FramerateChange =
    StrongTypedef<Maybe<double>, struct FramerateChangeType>;
// The bitrate mode (variable, constant) of the encoding
using BitrateModeChange =
    StrongTypedef<MediaDataEncoder::BitrateMode, struct BitrateModeChangeType>;
// The usage for the encoded stream, this influence latency, ordering, etc.
using UsageChange =
    StrongTypedef<MediaDataEncoder::Usage, struct UsageChangeType>;
// If present, the expected content of the video frames (screen, movie, etc.).
// The value the string can have isn't decided just yet. When absent, the
// encoder uses generic settings.
using ContentHintChange =
    StrongTypedef<Maybe<nsString>, struct ContentHintTypeType>;

// A change to a parameter of an encoder instance.
using EncoderConfigurationItem =
    Variant<DimensionsChange, DisplayDimensionsChange, BitrateModeChange,
            BitrateChange, FramerateChange, UsageChange, ContentHintChange>;

// A list of changes to an encoder configuration, that _might_ be able to change
// on the fly. Not all encoder modules can adjust their configuration on the
// fly.
struct EncoderConfigurationChangeList {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(EncoderConfigurationChangeList)
  bool Empty() const { return mChanges.IsEmpty(); }
  template <typename T>
  void Push(const T& aItem) {
    mChanges.AppendElement(aItem);
  }
  nsString ToString() const;

  nsTArray<EncoderConfigurationItem> mChanges;

 private:
  ~EncoderConfigurationChangeList() = default;
};

// Just by inspecting the configuration and before asking the PEM, it's
// sometimes possible to know that a media won't be able to be encoded. For
// example, VP8 encodes the frame size on 14 bits, so a resolution of more than
// 16383x16383 pixels cannot work.
bool CanLikelyEncode(const EncoderConfig& aConfig);

}  // namespace mozilla

#endif /* PlatformEncoderModule_h_ */
