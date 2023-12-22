/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/VideoEncoder.h"
#include "mozilla/dom/VideoEncoderBinding.h"
#include "mozilla/dom/VideoColorSpaceBinding.h"
#include "mozilla/dom/VideoColorSpace.h"

#include "EncoderTraits.h"
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
      mContentHint(aConfig.mContentHint) {
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
      mContentHint(OptionalToMaybe(aConfig.mContentHint)) {
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

  return rv;
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

  // Not supported
  if (aConfig->mScalabilityMode.isSome() &&
      !aConfig->mScalabilityMode->IsEmpty()) {
    LOGE("Scalability mode not supported");
    return false;
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
  // Not implemented
  if (aConfig.mScalabilityMode.isSome() &&
      !aConfig.mScalabilityMode->IsEmpty()) {
    return false;
  }
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

nsTArray<RefPtr<EncodedVideoChunk>> VideoEncoder::EncodedDataToOutputType(
    nsIGlobalObject* aGlobalObject, nsTArray<RefPtr<MediaRawData>>&& aData) {
  AssertIsOnOwningThread();

  nsTArray<RefPtr<EncodedVideoChunk>> chunks;
  for (RefPtr<MediaRawData>& data : aData) {
    MOZ_RELEASE_ASSERT(data->mType == MediaData::Type::RAW_DATA);
    // Package into an EncodedVideoChunk
    auto buffer = MakeRefPtr<MediaAlignedByteBuffer>(data->Data(), data->Size());
    auto encodedVideoChunk = MakeRefPtr<EncodedVideoChunk>(
        aGlobalObject, buffer.forget(),
        data->mKeyframe ? EncodedVideoChunkType::Key
                        : EncodedVideoChunkType::Delta,
        data->mTime.ToMicroseconds(),
        data->mDuration.IsZero() ? Nothing()
                                 : Some(data->mDuration.ToMicroseconds()));
    chunks.AppendElement(encodedVideoChunk);
  }
  return chunks;
}

VideoDecoderConfig VideoEncoder::EncoderConfigToDecoderConfig(
    nsIGlobalObject* aGlobal, const VideoEncoderConfigInternal& mOutputConfig) const {
  VideoDecoderConfig decoderConfig;
  decoderConfig.mCodec = mOutputConfig.mCodec;
  decoderConfig.mCodedWidth.Construct(mOutputConfig.mWidth);
  decoderConfig.mCodedHeight.Construct(mOutputConfig.mHeight);
  if (mOutputConfig.mDisplayWidth.isSome()) {
    MOZ_ASSERT(mOutputConfig.mDisplayHeight.isSome());
    decoderConfig.mDisplayAspectWidth.Construct(
        mOutputConfig.mDisplayWidth.value());
    decoderConfig.mDisplayAspectHeight.Construct(
        mOutputConfig.mDisplayHeight.value());
  }

  // Colorspace is mandatory when outputing a decoder config after encode
  VideoColorSpaceInit init;
  init.mFullRange = false;
  init.mMatrix = VideoMatrixCoefficients::Bt709;
  init.mPrimaries = VideoColorPrimaries::Bt709;
  init.mTransfer = VideoTransferCharacteristics::Bt709;
  decoderConfig.mColorSpace.Construct(init);

  return decoderConfig;
}

#undef LOG
#undef LOGW
#undef LOGE
#undef LOGV
#undef LOG_INTERNAL

}  // namespace mozilla::dom
