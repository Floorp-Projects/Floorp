/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/VideoEncoder.h"
#include "mozilla/dom/VideoEncoderBinding.h"
#include "mozilla/dom/VideoColorSpaceBinding.h"
#include "mozilla/dom/VideoColorSpace.h"
#include "mozilla/dom/VideoFrame.h"

#include "EncoderTraits.h"
#include "ImageContainer.h"
#include "VideoUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/Logging.h"
#include "mozilla/Maybe.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/EncodedVideoChunk.h"
#include "mozilla/dom/EncodedVideoChunkBinding.h"
#include "mozilla/dom/ImageUtils.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/VideoColorSpaceBinding.h"
#include "mozilla/dom/VideoFrameBinding.h"
#include "mozilla/dom/WebCodecsUtils.h"
#include "nsPrintfCString.h"
#include "nsReadableUtils.h"

extern mozilla::LazyLogModule gWebCodecsLog;

namespace mozilla::dom {

#ifdef LOG_INTERNAL
#  undef LOG_INTERNAL
#endif  // LOG_INTERNAL
#define LOG_INTERNAL(level, msg, ...) \
  MOZ_LOG(gWebCodecsLog, LogLevel::level, (msg, ##__VA_ARGS__))

#ifdef LOG
#  undef LOG
#endif  // LOG
#define LOG(msg, ...) LOG_INTERNAL(Debug, msg, ##__VA_ARGS__)

#ifdef LOGW
#  undef LOGW
#endif  // LOGW
#define LOGW(msg, ...) LOG_INTERNAL(Warning, msg, ##__VA_ARGS__)

#ifdef LOGE
#  undef LOGE
#endif  // LOGE
#define LOGE(msg, ...) LOG_INTERNAL(Error, msg, ##__VA_ARGS__)

#ifdef LOGV
#  undef LOGV
#endif  // LOGV
#define LOGV(msg, ...) LOG_INTERNAL(Verbose, msg, ##__VA_ARGS__)

NS_IMPL_CYCLE_COLLECTION_INHERITED(VideoEncoder, DOMEventTargetHelper,
                                   mErrorCallback, mOutputCallback)
NS_IMPL_ADDREF_INHERITED(VideoEncoder, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(VideoEncoder, DOMEventTargetHelper)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(VideoEncoder)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

VideoEncoderConfigInternal::VideoEncoderConfigInternal(
    const nsAString& aCodec, uint32_t aWidth, uint32_t aHeight,
    Maybe<uint32_t>&& aDisplayWidth, Maybe<uint32_t>&& aDisplayHeight,
    Maybe<uint32_t>&& aBitrate, Maybe<double>&& aFramerate,
    const HardwareAcceleration& aHardwareAcceleration,
    const AlphaOption& aAlpha, Maybe<nsString>&& aScalabilityMode,
    const VideoEncoderBitrateMode& aBitrateMode,
    const LatencyMode& aLatencyMode, Maybe<nsString>&& aContentHint)
    : mCodec(aCodec),
      mWidth(aWidth),
      mHeight(aHeight),
      mDisplayWidth(std::move(aDisplayWidth)),
      mDisplayHeight(std::move(aDisplayHeight)),
      mBitrate(std::move(aBitrate)),
      mFramerate(std::move(aFramerate)),
      mHardwareAcceleration(aHardwareAcceleration),
      mAlpha(aAlpha),
      mScalabilityMode(std::move(aScalabilityMode)),
      mBitrateMode(aBitrateMode),
      mLatencyMode(aLatencyMode),
      mContentHint(std::move(aContentHint)) {}

VideoEncoderConfigInternal::VideoEncoderConfigInternal(
    const VideoEncoderConfigInternal& aConfig)
    : mCodec(aConfig.mCodec),
      mWidth(aConfig.mWidth),
      mHeight(aConfig.mHeight),
      mDisplayWidth(aConfig.mDisplayWidth),
      mDisplayHeight(aConfig.mDisplayHeight),
      mBitrate(aConfig.mBitrate),
      mFramerate(aConfig.mFramerate),
      mHardwareAcceleration(aConfig.mHardwareAcceleration),
      mAlpha(aConfig.mAlpha),
      mScalabilityMode(aConfig.mScalabilityMode),
      mBitrateMode(aConfig.mBitrateMode),
      mLatencyMode(aConfig.mLatencyMode),
      mContentHint(aConfig.mContentHint),
      mAvc(aConfig.mAvc) {
}

VideoEncoderConfigInternal::VideoEncoderConfigInternal(
    const VideoEncoderConfig& aConfig)
    : mCodec(aConfig.mCodec),
      mWidth(aConfig.mWidth),
      mHeight(aConfig.mHeight),
      mDisplayWidth(OptionalToMaybe(aConfig.mDisplayWidth)),
      mDisplayHeight(OptionalToMaybe(aConfig.mDisplayHeight)),
      mBitrate(OptionalToMaybe(aConfig.mBitrate)),
      mFramerate(OptionalToMaybe(aConfig.mFramerate)),
      mHardwareAcceleration(aConfig.mHardwareAcceleration),
      mAlpha(aConfig.mAlpha),
      mScalabilityMode(OptionalToMaybe(aConfig.mScalabilityMode)),
      mBitrateMode(aConfig.mBitrateMode),
      mLatencyMode(aConfig.mLatencyMode),
      mContentHint(OptionalToMaybe(aConfig.mContentHint)),
      mAvc(OptionalToMaybe(aConfig.mAvc)) {
}

nsString VideoEncoderConfigInternal::ToString() const {
  nsString rv;

  rv.AppendPrintf("Codec: %s, [%" PRIu32 "x%" PRIu32 "],",
                  NS_ConvertUTF16toUTF8(mCodec).get(), mWidth, mHeight);
  if (mDisplayWidth.isSome()) {
    rv.AppendPrintf(", display[%" PRIu32 "x%" PRIu32 "]", mDisplayWidth.value(),
                    mDisplayHeight.value());
  }
  if (mBitrate.isSome()) {
    rv.AppendPrintf(", %" PRIu32 "kbps", mBitrate.value());
  }
  if (mFramerate.isSome()) {
    rv.AppendPrintf(", %lfHz", mFramerate.value());
  }
  rv.AppendPrintf(
      " hw: %s",
      HardwareAccelerationValues::GetString(mHardwareAcceleration).data());
  rv.AppendPrintf(", alpha: %s", AlphaOptionValues::GetString(mAlpha).data());
  if (mScalabilityMode.isSome()) {
    rv.AppendPrintf(", scalability mode: %s",
                    NS_ConvertUTF16toUTF8(mScalabilityMode.value()).get());
  }
  rv.AppendPrintf(
      ", bitrate mode: %s",
      VideoEncoderBitrateModeValues::GetString(mBitrateMode).data());
  rv.AppendPrintf(", latency mode: %s",
                  LatencyModeValues::GetString(mLatencyMode).data());
  if (mContentHint.isSome()) {
    rv.AppendPrintf(", content hint: %s",
                    NS_ConvertUTF16toUTF8(mContentHint.value()).get());
  }
  if (mAvc.isSome()) {
    rv.AppendPrintf(", avc-specific: %s",
                    AvcBitstreamFormatValues::GetString(mAvc->mFormat).data());
  }

  return rv;
}

template <typename T>
bool MaybeAreEqual(const Maybe<T>& aLHS, const Maybe<T> aRHS) {
  if (aLHS.isSome() && aRHS.isSome()) {
    return aLHS.value() == aRHS.value();
  }
  if (aLHS.isNothing() && aRHS.isNothing()) {
    return true;
  }
  return false;
}

bool VideoEncoderConfigInternal::Equals(
    const VideoEncoderConfigInternal& aOther) const {
  bool sameCodecSpecific = true;
  if ((mAvc.isSome() && aOther.mAvc.isSome() &&
       mAvc->mFormat != aOther.mAvc->mFormat) ||
      mAvc.isSome() != aOther.mAvc.isSome()) {
    sameCodecSpecific = false;
  }
  return mCodec.Equals(aOther.mCodec) && mWidth == aOther.mWidth &&
         mHeight == aOther.mHeight &&
         MaybeAreEqual(mDisplayWidth, aOther.mDisplayWidth) &&
         MaybeAreEqual(mDisplayHeight, aOther.mDisplayHeight) &&
         MaybeAreEqual(mBitrate, aOther.mBitrate) &&
         MaybeAreEqual(mFramerate, aOther.mFramerate) &&
         mHardwareAcceleration == aOther.mHardwareAcceleration &&
         mAlpha == aOther.mAlpha &&
         MaybeAreEqual(mScalabilityMode, aOther.mScalabilityMode) &&
         mBitrateMode == aOther.mBitrateMode &&
         mLatencyMode == aOther.mLatencyMode &&
         MaybeAreEqual(mContentHint, aOther.mContentHint) && sameCodecSpecific;
}

bool VideoEncoderConfigInternal::CanReconfigure(
    const VideoEncoderConfigInternal& aOther) const {
  return mCodec.Equals(aOther.mCodec) &&
         mHardwareAcceleration == aOther.mHardwareAcceleration;
}

EncoderConfig VideoEncoderConfigInternal::ToEncoderConfig() const {
  MediaDataEncoder::Usage usage;
  if (mLatencyMode == LatencyMode::Quality) {
    usage = MediaDataEncoder::Usage::Record;
  } else {
    usage = MediaDataEncoder::Usage::Realtime;
  }
  MediaDataEncoder::HardwarePreference hwPref =
      MediaDataEncoder::HardwarePreference::None;
  if (mHardwareAcceleration ==
      mozilla::dom::HardwareAcceleration::Prefer_hardware) {
    hwPref = MediaDataEncoder::HardwarePreference::RequireHardware;
  } else if (mHardwareAcceleration ==
             mozilla::dom::HardwareAcceleration::Prefer_software) {
    hwPref = MediaDataEncoder::HardwarePreference::RequireSoftware;
  }
  CodecType codecType;
  auto maybeCodecType = CodecStringToCodecType(mCodec);
  if (maybeCodecType.isSome()) {
    codecType = maybeCodecType.value();
  } else {
    MOZ_CRASH("The string should always contain a valid codec at this point.");
  }
  Maybe<EncoderConfig::CodecSpecific> specific;
  if (codecType == CodecType::H264) {
    uint8_t profile, constraints, level;
    H264BitStreamFormat format;
    if (mAvc) {
      format = mAvc->mFormat == AvcBitstreamFormat::Annexb
                   ? H264BitStreamFormat::ANNEXB
                   : H264BitStreamFormat::AVC;
    } else {
      format = H264BitStreamFormat::AVC;
    }
    if (ExtractH264CodecDetails(mCodec, profile, constraints, level)) {
      if (profile == H264_PROFILE_BASE || profile == H264_PROFILE_MAIN ||
          profile == H264_PROFILE_EXTENDED || profile == H264_PROFILE_HIGH) {
        specific.emplace(
            H264Specific(static_cast<H264_PROFILE>(profile), static_cast<H264_LEVEL>(level), format));
      }
    }
  }
  // Only for vp9, not vp8
  if (codecType == CodecType::VP9) {
    uint8_t profile, level, bitdepth, chromasubsampling;
    mozilla::VideoColorSpace colorspace;
    DebugOnly<bool> rv = ExtractVPXCodecDetails(
        mCodec, profile, level, bitdepth, chromasubsampling, colorspace);
#ifdef DEBUG
    if (!rv) {
      LOGE("Error extracting VPX codec details, non fatal");
    }
#endif
    specific.emplace(VP9Specific());
  }
  MediaDataEncoder::ScalabilityMode scalabilityMode;
  if (mScalabilityMode) {
    if (mScalabilityMode->EqualsLiteral("L1T2")) {
      scalabilityMode = MediaDataEncoder::ScalabilityMode::L1T2;
    } else if (mScalabilityMode->EqualsLiteral("L1T3")) {
      scalabilityMode = MediaDataEncoder::ScalabilityMode::L1T3;
    } else {
      scalabilityMode = MediaDataEncoder::ScalabilityMode::None;
    }
  } else {
    scalabilityMode = MediaDataEncoder::ScalabilityMode::None;
  }
  return EncoderConfig(
      codecType, {mWidth, mHeight}, usage, ImageBitmapFormat::RGBA32, ImageBitmapFormat::RGBA32,
      AssertedCast<uint8_t>(mFramerate.refOr(0.f)), 0, mBitrate.refOr(0),
      mBitrateMode == VideoEncoderBitrateMode::Constant
          ? MediaDataEncoder::BitrateMode::Constant
          : MediaDataEncoder::BitrateMode::Variable,
      hwPref, scalabilityMode, specific);
}
already_AddRefed<WebCodecsConfigurationChangeList>
VideoEncoderConfigInternal::Diff(
    const VideoEncoderConfigInternal& aOther) const {
  auto list = MakeRefPtr<WebCodecsConfigurationChangeList>();
  if (!mCodec.Equals(aOther.mCodec)) {
    list->Push(CodecChange{aOther.mCodec});
  }
  // Both must always be present, when a `VideoEncoderConfig` is passed to
  // `configure`.
  if (mWidth != aOther.mWidth || mHeight != aOther.mHeight) {
    list->Push(DimensionsChange{gfx::IntSize{aOther.mWidth, aOther.mHeight}});
  }
  // Similarly, both must always be present, when a `VideoEncoderConfig` is
  // passed to `configure`.
  if (!MaybeAreEqual(mDisplayWidth, aOther.mDisplayWidth) ||
      !MaybeAreEqual(mDisplayHeight, aOther.mDisplayHeight)) {
    Maybe<gfx::IntSize> displaySize =
        aOther.mDisplayWidth.isSome()
            ? Some(gfx::IntSize{aOther.mDisplayWidth.value(),
                                aOther.mDisplayHeight.value()})
            : Nothing();
    list->Push(DisplayDimensionsChange{displaySize});
  }
  if (!MaybeAreEqual(mBitrate, aOther.mBitrate)) {
    list->Push(BitrateChange{aOther.mBitrate});
  }
  if (!MaybeAreEqual(mFramerate, aOther.mFramerate)) {
    list->Push(FramerateChange{aOther.mFramerate});
  }
  if (mHardwareAcceleration != aOther.mHardwareAcceleration) {
    list->Push(HardwareAccelerationChange{aOther.mHardwareAcceleration});
  }
  if (mAlpha != aOther.mAlpha) {
    list->Push(AlphaChange{aOther.mAlpha});
  }
  if (!MaybeAreEqual(mScalabilityMode, aOther.mScalabilityMode)) {
    list->Push(ScalabilityModeChange{aOther.mScalabilityMode});
  }
  if (mBitrateMode != aOther.mBitrateMode) {
    list->Push(BitrateModeChange{aOther.mBitrateMode});
  }
  if (mLatencyMode != aOther.mLatencyMode) {
    list->Push(LatencyModeChange{aOther.mLatencyMode});
  }
  if (!MaybeAreEqual(mContentHint, aOther.mContentHint)) {
    list->Push(ContentHintChange{aOther.mContentHint});
  }
  return list.forget();
}

/*
 * The followings are helpers for VideoEncoder methods
 */
static bool IsEncodeSupportedCodec(const nsAString& aCodec) {
  LOG("IsEncodeSupported: %s", NS_ConvertUTF16toUTF8(aCodec).get());
  if (!IsVP9CodecString(aCodec) && !IsVP8CodecString(aCodec) &&
      !IsH264CodecString(aCodec) && !IsAV1CodecString(aCodec)) {
    return false;
  }

  // Gecko allows codec string starts with vp9 or av1 but Webcodecs requires to
  // starts with av01 and vp09.
  // https://www.w3.org/TR/webcodecs-codec-registry/#video-codec-registry
  if (StringBeginsWith(aCodec, u"vp9"_ns) ||
      StringBeginsWith(aCodec, u"av1"_ns)) {
    return false;
  }

  return true;
}

// https://w3c.github.io/webcodecs/#check-configuration-support
static bool CanEncode(const RefPtr<VideoEncoderConfigInternal>& aConfig) {
  auto parsedCodecString =
      ParseCodecString(aConfig->mCodec).valueOr(EmptyString());
  // TODO: Enable WebCodecs on Android (Bug 1840508)
  if (IsOnAndroid()) {
    return false;
  }
  if (!IsEncodeSupportedCodec(parsedCodecString)) {
    return false;
  }

  // TODO (bug 1872879, bug 1872880): Support this on Windows and Mac.
  if (aConfig->mScalabilityMode.isSome()) {
    // We only support L1T2 and L1T3 ScalabilityMode in VP8 and VP9 encoders on
    // Linux.
    bool supported = IsOnLinux() && (IsVP8CodecString(parsedCodecString) ||
                                     IsVP9CodecString(parsedCodecString))
                         ? aConfig->mScalabilityMode->EqualsLiteral("L1T2") ||
                               aConfig->mScalabilityMode->EqualsLiteral("L1T3")
                         : false;

    if (!supported) {
      LOGE("Scalability mode %s not supported for codec: %s",
           NS_ConvertUTF16toUTF8(aConfig->mScalabilityMode.value()).get(),
           NS_ConvertUTF16toUTF8(parsedCodecString).get());
      return false;
    }
  }

  return EncoderSupport::Supports(aConfig);
}

static Result<Ok, nsresult> CloneConfiguration(
    VideoEncoderConfig& aDest, JSContext* aCx,
    const VideoEncoderConfig& aConfig) {
  nsCString errorMessage;
  MOZ_ASSERT(VideoEncoderTraits::Validate(aConfig, errorMessage));

  aDest.mCodec = aConfig.mCodec;
  aDest.mWidth = aConfig.mWidth;
  aDest.mHeight = aConfig.mHeight;
  aDest.mAlpha = aConfig.mAlpha;
  if (aConfig.mBitrate.WasPassed()) {
    aDest.mBitrate.Construct(aConfig.mBitrate.Value());
  }
  aDest.mBitrateMode = aConfig.mBitrateMode;
  if (aConfig.mContentHint.WasPassed()) {
    aDest.mContentHint.Construct(aConfig.mContentHint.Value());
  }
  if (aConfig.mDisplayWidth.WasPassed()) {
    aDest.mDisplayWidth.Construct(aConfig.mDisplayWidth.Value());
  }
  if (aConfig.mDisplayHeight.WasPassed()) {
    aDest.mDisplayHeight.Construct(aConfig.mDisplayHeight.Value());
  }
  if (aConfig.mFramerate.WasPassed()) {
    aDest.mFramerate.Construct(aConfig.mFramerate.Value());
  }
  aDest.mHardwareAcceleration = aConfig.mHardwareAcceleration;
  aDest.mLatencyMode = aConfig.mLatencyMode;
  if (aConfig.mScalabilityMode.WasPassed()) {
    aDest.mScalabilityMode.Construct(aConfig.mScalabilityMode.Value());
  }

  // AVC specific
  if (aConfig.mAvc.WasPassed()) {
    aDest.mAvc.Construct(aConfig.mAvc.Value());
  }

  return Ok();
}

/* static */
bool VideoEncoderTraits::IsSupported(
    const VideoEncoderConfigInternal& aConfig) {
  return CanEncode(MakeRefPtr<VideoEncoderConfigInternal>(aConfig));
}

// https://w3c.github.io/webcodecs/#valid-videoencoderconfig
/* static */
bool VideoEncoderTraits::Validate(const VideoEncoderConfig& aConfig,
                                  nsCString& aErrorMessage) {
  Maybe<nsString> codec = ParseCodecString(aConfig.mCodec);
  // 1.
  if (!codec || codec->IsEmpty()) {
    LOGE("Invalid VideoEncoderConfig: invalid codec string");
    return false;
  }

  // 2.
  if (aConfig.mWidth == 0 || aConfig.mHeight == 0) {
    LOGE("Invalid VideoEncoderConfig: %s equal to 0",
         aConfig.mWidth == 0 ? "width" : "height");
    return false;
  }

  // 3.
  if ((aConfig.mDisplayWidth.WasPassed() &&
       aConfig.mDisplayWidth.Value() == 0)) {
    LOGE("Invalid VideoEncoderConfig: displayWidth equal to 0");
    return false;
  }
  if ((aConfig.mDisplayHeight.WasPassed() &&
       aConfig.mDisplayHeight.Value() == 0)) {
    LOGE("Invalid VideoEncoderConfig: displayHeight equal to 0");
    return false;
  }

  return true;
}

/* static */
RefPtr<VideoEncoderConfigInternal> VideoEncoderTraits::CreateConfigInternal(
    const VideoEncoderConfig& aConfig) {
  return MakeRefPtr<VideoEncoderConfigInternal>(aConfig);
}

/* static */
RefPtr<mozilla::VideoData> VideoEncoderTraits::CreateInputInternal(
    const dom::VideoFrame& aInput,
    const dom::VideoEncoderEncodeOptions& aOptions) {
  media::TimeUnit duration =
      aInput.GetDuration().IsNull()
          ? media::TimeUnit::Zero()
          : media::TimeUnit::FromMicroseconds(
                AssertedCast<int64_t>(aInput.GetDuration().Value()));
  media::TimeUnit pts = media::TimeUnit::FromMicroseconds(aInput.Timestamp());
  return VideoData::CreateFromImage(
      gfx::IntSize{aInput.DisplayWidth(), aInput.DisplayHeight()},
      0 /* bytestream offset */, pts, duration, aInput.GetImage(),
      aOptions.mKeyFrame, pts);
}

/*
 * Below are VideoEncoder implementation
 */

VideoEncoder::VideoEncoder(
    nsIGlobalObject* aParent, RefPtr<WebCodecsErrorCallback>&& aErrorCallback,
    RefPtr<EncodedVideoChunkOutputCallback>&& aOutputCallback)
    : EncoderTemplate(aParent, std::move(aErrorCallback),
                      std::move(aOutputCallback)) {
  MOZ_ASSERT(mErrorCallback);
  MOZ_ASSERT(mOutputCallback);
  LOG("VideoEncoder %p ctor", this);
}

VideoEncoder::~VideoEncoder() {
  LOG("VideoEncoder %p dtor", this);
  Unused << ResetInternal(NS_ERROR_DOM_ABORT_ERR);
}

JSObject* VideoEncoder::WrapObject(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  AssertIsOnOwningThread();

  return VideoEncoder_Binding::Wrap(aCx, this, aGivenProto);
}

// https://w3c.github.io/webcodecs/#dom-videoencoder-videoencoder
/* static */
already_AddRefed<VideoEncoder> VideoEncoder::Constructor(
    const GlobalObject& aGlobal, const VideoEncoderInit& aInit,
    ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return MakeAndAddRef<VideoEncoder>(
      global.get(), RefPtr<WebCodecsErrorCallback>(aInit.mError),
      RefPtr<EncodedVideoChunkOutputCallback>(aInit.mOutput));
}

// https://w3c.github.io/webcodecs/#dom-videoencoder-isconfigsupported
/* static */
already_AddRefed<Promise> VideoEncoder::IsConfigSupported(
    const GlobalObject& aGlobal, const VideoEncoderConfig& aConfig,
    ErrorResult& aRv) {
  LOG("VideoEncoder::IsConfigSupported, config: %s",
      NS_ConvertUTF16toUTF8(aConfig.mCodec).get());

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<Promise> p = Promise::Create(global.get(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return p.forget();
  }

  nsCString errorMessage;
  if (!VideoEncoderTraits::Validate(aConfig, errorMessage)) {
    p->MaybeRejectWithTypeError(nsPrintfCString(
        "IsConfigSupported: config is invalid: %s", errorMessage.get()));
    return p.forget();
  }

  // TODO: Move the following works to another thread to unblock the current
  // thread, as what spec suggests.

  VideoEncoderConfig config;
  auto r = CloneConfiguration(config, aGlobal.Context(), aConfig);
  if (r.isErr()) {
    nsresult e = r.unwrapErr();
    LOGE("Failed to clone VideoEncoderConfig. Error: 0x%08" PRIx32,
         static_cast<uint32_t>(e));
    p->MaybeRejectWithTypeError("Failed to clone VideoEncoderConfig");
    aRv.Throw(e);
    return p.forget();
  }

  bool canEncode = CanEncode(MakeRefPtr<VideoEncoderConfigInternal>(config));
  VideoEncoderSupport s;
  s.mConfig.Construct(std::move(config));
  s.mSupported.Construct(canEncode);

  p->MaybeResolve(s);
  return p.forget();
}

RefPtr<EncodedVideoChunk> VideoEncoder::EncodedDataToOutputType(
    nsIGlobalObject* aGlobalObject, RefPtr<MediaRawData>& aData) {
  AssertIsOnOwningThread();

  MOZ_RELEASE_ASSERT(aData->mType == MediaData::Type::RAW_DATA);
  // Package into an EncodedVideoChunk
  auto buffer =
      MakeRefPtr<MediaAlignedByteBuffer>(aData->Data(), aData->Size());
  auto encodedVideoChunk = MakeRefPtr<EncodedVideoChunk>(
      aGlobalObject, buffer.forget(),
      aData->mKeyframe ? EncodedVideoChunkType::Key
                       : EncodedVideoChunkType::Delta,
      aData->mTime.ToMicroseconds(),
      aData->mDuration.IsZero() ? Nothing()
                                : Some(aData->mDuration.ToMicroseconds()));
  return encodedVideoChunk;
}

VideoDecoderConfigInternal VideoEncoder::EncoderConfigToDecoderConfig(
    nsIGlobalObject* aGlobal, const RefPtr<MediaRawData>& aRawData,
    const VideoEncoderConfigInternal& mOutputConfig) const {
  // Colorspace is mandatory when outputing a decoder config after encode
  VideoColorSpaceInternal init;
  init.mFullRange.emplace(false);
  init.mMatrix.emplace(VideoMatrixCoefficients::Bt709);
  init.mPrimaries.emplace(VideoColorPrimaries::Bt709);
  init.mTransfer.emplace(VideoTransferCharacteristics::Bt709);

  return VideoDecoderConfigInternal(
      mOutputConfig.mCodec,        /* aCodec */
      Some(mOutputConfig.mHeight), /* aCodedHeight */
      Some(mOutputConfig.mWidth),  /* aCodedWidth */
      Some(init),                  /* aColorSpace */
      aRawData->mExtraData && !aRawData->mExtraData->IsEmpty()
          ? Some(aRawData->mExtraData)
          : Nothing(),                               /* aDescription*/
      Maybe<uint32_t>(mOutputConfig.mDisplayHeight), /* aDisplayAspectHeight*/
      Maybe<uint32_t>(mOutputConfig.mDisplayWidth),  /* aDisplayAspectWidth */
      mOutputConfig.mHardwareAcceleration,           /* aHardwareAcceleration */
      Nothing()                                      /*  aOptimizeForLatency */
  );
}

#undef LOG
#undef LOGW
#undef LOGE
#undef LOGV
#undef LOG_INTERNAL

}  // namespace mozilla::dom
