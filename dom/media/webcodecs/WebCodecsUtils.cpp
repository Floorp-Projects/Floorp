/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebCodecsUtils.h"

#include "DecoderTypes.h"
#include "VideoUtils.h"
#include "js/experimental/TypedData.h"
#include "mozilla/Assertions.h"
#include "mozilla/dom/ImageBitmapBinding.h"
#include "mozilla/dom/VideoColorSpaceBinding.h"
#include "mozilla/dom/VideoFrameBinding.h"
#include "mozilla/gfx/Types.h"
#include "nsDebug.h"
#include "PlatformEncoderModule.h"
#include "PlatformEncoderModule.h"

extern mozilla::LazyLogModule gWebCodecsLog;

#ifdef LOG_INTERNAL
#  undef LOG_INTERNAL
#endif  // LOG_INTERNAL
#define LOG_INTERNAL(level, msg, ...) \
  MOZ_LOG(gWebCodecsLog, LogLevel::level, (msg, ##__VA_ARGS__))
#ifdef LOG
#  undef LOG
#endif  // LOG
#define LOG(msg, ...) LOG_INTERNAL(Debug, msg, ##__VA_ARGS__)

namespace mozilla {
std::atomic<WebCodecsId> sNextId = 0;
};

namespace mozilla::dom {

/*
 * The followings are helpers for VideoDecoder methods
 */

nsTArray<nsCString> GuessContainers(const nsAString& aCodec) {
  if (IsAV1CodecString(aCodec)) {
    return {"mp4"_ns, "webm"_ns};
  }

  if (IsVP9CodecString(aCodec)) {
    return {"mp4"_ns, "webm"_ns, "ogg"_ns};
  }

  if (IsVP8CodecString(aCodec)) {
    return {"webm"_ns, "ogg"_ns, "3gpp"_ns, "3gpp2"_ns, "3gp2"_ns};
  }

  if (IsH264CodecString(aCodec)) {
    return {"mp4"_ns, "3gpp"_ns, "3gpp2"_ns, "3gp2"_ns};
  }

  return {};
}

Maybe<nsString> ParseCodecString(const nsAString& aCodec) {
  // Trim the spaces on each end.
  nsString str(aCodec);
  str.Trim(" ");
  nsTArray<nsString> codecs;
  if (!ParseCodecsString(str, codecs) || codecs.Length() != 1 ||
      codecs[0] != str) {
    return Nothing();
  }
  return Some(codecs[0]);
}

/*
 * The below are helpers to operate ArrayBuffer or ArrayBufferView.
 */

static std::tuple<JS::ArrayBufferOrView, size_t, size_t> GetArrayBufferInfo(
    JSContext* aCx,
    const OwningMaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer& aBuffer) {
  if (aBuffer.IsArrayBuffer()) {
    const ArrayBuffer& buffer = aBuffer.GetAsArrayBuffer();
    size_t length;
    {
      bool isShared;
      uint8_t* data;
      JS::GetArrayBufferMaybeSharedLengthAndData(buffer.Obj(), &length,
                                                 &isShared, &data);
    }
    return std::make_tuple(JS::ArrayBufferOrView::fromObject(buffer.Obj()),
                           (size_t)0, length);
  }

  MOZ_ASSERT(aBuffer.IsArrayBufferView());
  const ArrayBufferView& view = aBuffer.GetAsArrayBufferView();
  bool isSharedMemory;
  JS::Rooted<JSObject*> obj(aCx, view.Obj());
  return std::make_tuple(
      JS::ArrayBufferOrView::fromObject(
          JS_GetArrayBufferViewBuffer(aCx, obj, &isSharedMemory)),
      JS_GetArrayBufferViewByteOffset(obj),
      JS_GetArrayBufferViewByteLength(obj));
}

Result<Ok, nsresult> CloneBuffer(
    JSContext* aCx,
    OwningMaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer& aDest,
    const OwningMaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer& aSrc) {
  std::tuple<JS::ArrayBufferOrView, size_t, size_t> info =
      GetArrayBufferInfo(aCx, aSrc);
  JS::Rooted<JS::ArrayBufferOrView> abov(aCx);
  abov.set(std::get<0>(info));
  size_t offset = std::get<1>(info);
  size_t len = std::get<2>(info);
  if (NS_WARN_IF(!abov)) {
    return Err(NS_ERROR_UNEXPECTED);
  }

  JS::Rooted<JSObject*> obj(aCx, abov.asObject());
  JS::Rooted<JSObject*> cloned(aCx,
                               JS::ArrayBufferClone(aCx, obj, offset, len));
  if (NS_WARN_IF(!cloned)) {
    return Err(NS_ERROR_OUT_OF_MEMORY);
  }

  JS::Rooted<JS::Value> value(aCx, JS::ObjectValue(*cloned));
  if (NS_WARN_IF(!aDest.Init(aCx, value))) {
    return Err(NS_ERROR_UNEXPECTED);
  }
  return Ok();
}

/*
 * The following are utilities to convert between VideoColorSpace values to
 * gfx's values.
 */

gfx::ColorRange ToColorRange(bool aIsFullRange) {
  return aIsFullRange ? gfx::ColorRange::FULL : gfx::ColorRange::LIMITED;
}

gfx::YUVColorSpace ToColorSpace(VideoMatrixCoefficients aMatrix) {
  switch (aMatrix) {
    case VideoMatrixCoefficients::Rgb:
      return gfx::YUVColorSpace::Identity;
    case VideoMatrixCoefficients::Bt709:
    case VideoMatrixCoefficients::Bt470bg:
      return gfx::YUVColorSpace::BT709;
    case VideoMatrixCoefficients::Smpte170m:
      return gfx::YUVColorSpace::BT601;
    case VideoMatrixCoefficients::Bt2020_ncl:
      return gfx::YUVColorSpace::BT2020;
    case VideoMatrixCoefficients::EndGuard_:
      break;
  }
  MOZ_ASSERT_UNREACHABLE("unsupported VideoMatrixCoefficients");
  return gfx::YUVColorSpace::Default;
}

gfx::TransferFunction ToTransferFunction(
    VideoTransferCharacteristics aTransfer) {
  switch (aTransfer) {
    case VideoTransferCharacteristics::Bt709:
    case VideoTransferCharacteristics::Smpte170m:
      return gfx::TransferFunction::BT709;
    case VideoTransferCharacteristics::Iec61966_2_1:
      return gfx::TransferFunction::SRGB;
    case VideoTransferCharacteristics::Pq:
      return gfx::TransferFunction::PQ;
    case VideoTransferCharacteristics::Hlg:
      return gfx::TransferFunction::HLG;
    case VideoTransferCharacteristics::Linear:
    case VideoTransferCharacteristics::EndGuard_:
      break;
  }
  MOZ_ASSERT_UNREACHABLE("unsupported VideoTransferCharacteristics");
  return gfx::TransferFunction::Default;
}

gfx::ColorSpace2 ToPrimaries(VideoColorPrimaries aPrimaries) {
  switch (aPrimaries) {
    case VideoColorPrimaries::Bt709:
      return gfx::ColorSpace2::BT709;
    case VideoColorPrimaries::Bt470bg:
      return gfx::ColorSpace2::BT601_625;
    case VideoColorPrimaries::Smpte170m:
      return gfx::ColorSpace2::BT601_525;
    case VideoColorPrimaries::Bt2020:
      return gfx::ColorSpace2::BT2020;
    case VideoColorPrimaries::Smpte432:
      return gfx::ColorSpace2::DISPLAY_P3;
    case VideoColorPrimaries::EndGuard_:
      break;
  }
  MOZ_ASSERT_UNREACHABLE("unsupported VideoTransferCharacteristics");
  return gfx::ColorSpace2::UNKNOWN;
}

bool ToFullRange(const gfx::ColorRange& aColorRange) {
  return aColorRange == gfx::ColorRange::FULL;
}

Maybe<VideoMatrixCoefficients> ToMatrixCoefficients(
    const gfx::YUVColorSpace& aColorSpace) {
  switch (aColorSpace) {
    case gfx::YUVColorSpace::BT601:
      return Some(VideoMatrixCoefficients::Smpte170m);
    case gfx::YUVColorSpace::BT709:
      return Some(VideoMatrixCoefficients::Bt709);
    case gfx::YUVColorSpace::BT2020:
      return Some(VideoMatrixCoefficients::Bt2020_ncl);
    case gfx::YUVColorSpace::Identity:
      return Some(VideoMatrixCoefficients::Rgb);
  }
  MOZ_ASSERT_UNREACHABLE("unsupported gfx::YUVColorSpace");
  return Nothing();
}

Maybe<VideoTransferCharacteristics> ToTransferCharacteristics(
    const gfx::TransferFunction& aTransferFunction) {
  switch (aTransferFunction) {
    case gfx::TransferFunction::BT709:
      return Some(VideoTransferCharacteristics::Bt709);
    case gfx::TransferFunction::SRGB:
      return Some(VideoTransferCharacteristics::Iec61966_2_1);
    case gfx::TransferFunction::PQ:
      return Some(VideoTransferCharacteristics::Pq);
    case gfx::TransferFunction::HLG:
      return Some(VideoTransferCharacteristics::Hlg);
  }
  MOZ_ASSERT_UNREACHABLE("unsupported gfx::TransferFunction");
  return Nothing();
}

Maybe<VideoColorPrimaries> ToPrimaries(const gfx::ColorSpace2& aColorSpace) {
  switch (aColorSpace) {
    case gfx::ColorSpace2::UNKNOWN:
      return Nothing();
    case gfx::ColorSpace2::DISPLAY_P3:
      return Some(VideoColorPrimaries::Smpte432);
    case gfx::ColorSpace2::BT601_525:
      return Some(VideoColorPrimaries::Smpte170m);
    case gfx::ColorSpace2::SRGB:
    case gfx::ColorSpace2::BT709:
      return Some(VideoColorPrimaries::Bt709);
    case gfx::ColorSpace2::BT2020:
      return Some(VideoColorPrimaries::Bt2020);
  }
  MOZ_ASSERT_UNREACHABLE("unsupported gfx::ColorSpace2");
  return Nothing();
}

/*
 * The following are utilities to convert from gfx's formats to
 * VideoPixelFormats.
 */

Maybe<VideoPixelFormat> SurfaceFormatToVideoPixelFormat(
    gfx::SurfaceFormat aFormat) {
  switch (aFormat) {
    case gfx::SurfaceFormat::B8G8R8A8:
      return Some(VideoPixelFormat::BGRA);
    case gfx::SurfaceFormat::B8G8R8X8:
      return Some(VideoPixelFormat::BGRX);
    case gfx::SurfaceFormat::R8G8B8A8:
      return Some(VideoPixelFormat::RGBA);
    case gfx::SurfaceFormat::R8G8B8X8:
      return Some(VideoPixelFormat::RGBX);
    case gfx::SurfaceFormat::YUV:
      return Some(VideoPixelFormat::I420);
    case gfx::SurfaceFormat::NV12:
      return Some(VideoPixelFormat::NV12);
    case gfx::SurfaceFormat::YUV422:
      return Some(VideoPixelFormat::I422);
    default:
      break;
  }
  return Nothing();
}

Maybe<VideoPixelFormat> ImageBitmapFormatToVideoPixelFormat(
    ImageBitmapFormat aFormat) {
  switch (aFormat) {
    case ImageBitmapFormat::RGBA32:
      return Some(VideoPixelFormat::RGBA);
    case ImageBitmapFormat::BGRA32:
      return Some(VideoPixelFormat::BGRA);
    case ImageBitmapFormat::YUV444P:
      return Some(VideoPixelFormat::I444);
    case ImageBitmapFormat::YUV422P:
      return Some(VideoPixelFormat::I422);
    case ImageBitmapFormat::YUV420P:
      return Some(VideoPixelFormat::I420);
    case ImageBitmapFormat::YUV420SP_NV12:
      return Some(VideoPixelFormat::NV12);
    default:
      break;
  }
  return Nothing();
}

Result<RefPtr<MediaByteBuffer>, nsresult> GetExtraDataFromArrayBuffer(
    const OwningMaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer& aBuffer) {
  RefPtr<MediaByteBuffer> data = MakeRefPtr<MediaByteBuffer>();
  Unused << AppendTypedArrayDataTo(aBuffer, *data);
  return data->Length() > 0 ? data : nullptr;
}

bool IsOnAndroid() {
#if defined(ANDROID)
  return true;
#else
  return false;
#endif
}

bool IsOnMacOS() {
#if defined(XP_MACOSX)
  return true;
#else
  return false;
#endif
}

bool IsOnLinux() {
#if defined(XP_LINUX)
  return true;
#else
  return false;
#endif
}

template <typename T>
nsCString MaybeToString(const Maybe<T>& aMaybe) {
  return nsPrintfCString(
      "%s", aMaybe.isSome() ? ToString(aMaybe.value()).c_str() : "nothing");
}

struct ConfigurationChangeToString {
  nsCString operator()(const CodecChange& aCodecChange) {
    return nsPrintfCString("Codec: %s",
                           NS_ConvertUTF16toUTF8(aCodecChange.get()).get());
  }
  nsCString operator()(const DimensionsChange& aDimensionChange) {
    return nsPrintfCString("Dimensions: %dx%d", aDimensionChange.get().width,
                           aDimensionChange.get().height);
  }
  nsCString operator()(const DisplayDimensionsChange& aDisplayDimensionChange) {
    if (aDisplayDimensionChange.get().isNothing()) {
      return nsPrintfCString("Display dimensions: nothing");
    }
    gfx::IntSize displayDimensions = aDisplayDimensionChange.get().value();
    return nsPrintfCString("Dimensions: %dx%d", displayDimensions.width,
                           displayDimensions.height);
  }
  nsCString operator()(const BitrateChange& aBitrateChange) {
    return nsPrintfCString("Bitrate: %skbps",
                           MaybeToString(aBitrateChange.get()).get());
  }
  nsCString operator()(const FramerateChange& aFramerateChange) {
    return nsPrintfCString("Framerate: %sHz",
                           MaybeToString(aFramerateChange.get()).get());
  }
  nsCString operator()(
      const HardwareAccelerationChange& aHardwareAccelerationChange) {
    return nsPrintfCString("HW acceleration: %s",
                           dom::HardwareAccelerationValues::GetString(
                               aHardwareAccelerationChange.get())
                               .data());
  }
  nsCString operator()(const AlphaChange& aAlphaChange) {
    return nsPrintfCString(
        "Alpha: %s",
        dom::AlphaOptionValues::GetString(aAlphaChange.get()).data());
  }
  nsCString operator()(const ScalabilityModeChange& aScalabilityModeChange) {
    if (aScalabilityModeChange.get().isNothing()) {
      return nsCString("Scalability mode: nothing");
    }
    return nsPrintfCString(
        "Scalability mode: %s",
        NS_ConvertUTF16toUTF8(aScalabilityModeChange.get().value()).get());
  }
  nsCString operator()(const BitrateModeChange& aBitrateModeChange) {
    return nsPrintfCString(
        "Bitrate mode: %s",
        dom::VideoEncoderBitrateModeValues::GetString(aBitrateModeChange.get())
            .data());
  }
  nsCString operator()(const LatencyModeChange& aLatencyModeChange) {
    return nsPrintfCString(
        "Latency mode: %s",
        dom::LatencyModeValues::GetString(aLatencyModeChange.get()).data());
  }
  nsCString operator()(const ContentHintChange& aContentHintChange) {
    return nsPrintfCString("Content hint: %s",
                           MaybeToString(aContentHintChange.get()).get());
  }
  template <typename T>
  nsCString operator()(const T& aNewBitrate) {
    return nsPrintfCString("Not implemented");
  }
};

nsString WebCodecsConfigurationChangeList::ToString() const {
  nsString rv;
  for (const WebCodecsEncoderConfigurationItem& change : mChanges) {
    nsCString str = change.match(ConfigurationChangeToString());
    rv.AppendPrintf("- %s\n", str.get());
  }
  return rv;
}

using CodecChange = StrongTypedef<nsString, struct CodecChangeTypeWebCodecs>;
using DimensionsChange =
    StrongTypedef<gfx::IntSize, struct DimensionsChangeTypeWebCodecs>;
using DisplayDimensionsChange =
    StrongTypedef<Maybe<gfx::IntSize>,
                  struct DisplayDimensionsChangeTypeWebCodecs>;
using BitrateChange =
    StrongTypedef<Maybe<uint32_t>, struct BitrateChangeTypeWebCodecs>;
using FramerateChange =
    StrongTypedef<Maybe<double>, struct FramerateChangeTypeWebCodecs>;
using HardwareAccelerationChange =
    StrongTypedef<dom::HardwareAcceleration,
                  struct HardwareAccelerationChangeTypeWebCodecs>;
using AlphaChange =
    StrongTypedef<dom::AlphaOption, struct AlphaChangeTypeWebCodecs>;
using ScalabilityModeChange =
    StrongTypedef<Maybe<nsString>, struct ScalabilityModeChangeTypeWebCodecs>;
using BitrateModeChange = StrongTypedef<dom::VideoEncoderBitrateMode,
                                        struct BitrateModeChangeTypeWebCodecs>;
using LatencyModeChange =
    StrongTypedef<dom::LatencyMode, struct LatencyModeTypeChangeTypeWebCodecs>;
using ContentHintChange =
    StrongTypedef<Maybe<nsString>, struct ContentHintTypeTypeWebCodecs>;

bool WebCodecsConfigurationChangeList::CanAttemptReconfigure() const {
  for (const auto& change : mChanges) {
    if (change.is<CodecChange>() || change.is<HardwareAccelerationChange>() ||
        change.is<AlphaChange>() || change.is<ScalabilityModeChange>()) {
      return false;
    }
  }
  return true;
}

RefPtr<EncoderConfigurationChangeList>
WebCodecsConfigurationChangeList::ToPEMChangeList() const {
  auto rv = MakeRefPtr<EncoderConfigurationChangeList>();
  MOZ_ASSERT(CanAttemptReconfigure());
  for (const auto& change : mChanges) {
    if (change.is<dom::DimensionsChange>()) {
      rv->Push(mozilla::DimensionsChange(change.as<DimensionsChange>().get()));
    } else if (change.is<dom::DisplayDimensionsChange>()) {
      rv->Push(mozilla::DisplayDimensionsChange(
          change.as<DisplayDimensionsChange>().get()));
    } else if (change.is<dom::BitrateChange>()) {
      rv->Push(mozilla::BitrateChange(change.as<BitrateChange>().get()));
    } else if (change.is<FramerateChange>()) {
      rv->Push(mozilla::FramerateChange(change.as<FramerateChange>().get()));
    } else if (change.is<dom::BitrateModeChange>()) {
      MediaDataEncoder::BitrateMode mode;
      if (change.as<dom::BitrateModeChange>().get() ==
          dom::VideoEncoderBitrateMode::Constant) {
        mode = MediaDataEncoder::BitrateMode::Constant;
      } else if (change.as<BitrateModeChange>().get() ==
                 dom::VideoEncoderBitrateMode::Variable) {
        mode = MediaDataEncoder::BitrateMode::Variable;
      } else {
        // Quantizer, not underlying support yet.
        mode = MediaDataEncoder::BitrateMode::Variable;
      }
      rv->Push(mozilla::BitrateModeChange(mode));
    } else if (change.is<LatencyModeChange>()) {
      MediaDataEncoder::Usage usage;
      if (change.as<LatencyModeChange>().get() == dom::LatencyMode::Quality) {
        usage = MediaDataEncoder::Usage::Record;
      } else {
        usage = MediaDataEncoder::Usage::Realtime;
      }
      rv->Push(UsageChange(usage));
    } else if (change.is<ContentHintChange>()) {
      rv->Push(
          mozilla::ContentHintChange(change.as<ContentHintChange>().get()));
    }
  }
  return rv.forget();
}

#define ENUM_TO_STRING(enumType, enumValue) \
  enumType##Values::GetString(enumValue).data()

nsCString ColorSpaceInitToString(
    const dom::VideoColorSpaceInit& aColorSpaceInit) {
  nsCString rv("VideoColorSpace");

  if (!aColorSpaceInit.mFullRange.IsNull()) {
    rv.AppendPrintf(" range: %s",
                    aColorSpaceInit.mFullRange.Value() ? "true" : "false");
  }
  if (!aColorSpaceInit.mMatrix.IsNull()) {
    rv.AppendPrintf(" matrix: %s",
                    ENUM_TO_STRING(dom::VideoMatrixCoefficients,
                                   aColorSpaceInit.mMatrix.Value()));
  }
  if (!aColorSpaceInit.mTransfer.IsNull()) {
    rv.AppendPrintf(" transfer: %s",
                    ENUM_TO_STRING(dom::VideoTransferCharacteristics,
                                   aColorSpaceInit.mTransfer.Value()));
  }
  if (!aColorSpaceInit.mPrimaries.IsNull()) {
    rv.AppendPrintf(" primaries: %s",
                    ENUM_TO_STRING(dom::VideoColorPrimaries,
                                   aColorSpaceInit.mPrimaries.Value()));
  }

  return rv;
}

RefPtr<TaskQueue> GetWebCodecsEncoderTaskQueue() {
  return TaskQueue::Create(
      GetMediaThreadPool(MediaThreadType::PLATFORM_ENCODER),
      "WebCodecs encoding", false);
}

VideoColorSpaceInit FallbackColorSpaceForVideoContent() {
  // If we're unable to determine the color space, but we think this is video
  // content (e.g. because it's in YUV or NV12 or something like that,
  // consider it's in BT709).
  // This is step 3 of
  // https://w3c.github.io/webcodecs/#videoframe-pick-color-space
  VideoColorSpaceInit colorSpace;
  colorSpace.mFullRange = false;
  colorSpace.mMatrix = VideoMatrixCoefficients::Bt709;
  colorSpace.mTransfer = VideoTransferCharacteristics::Bt709;
  colorSpace.mPrimaries = VideoColorPrimaries::Bt709;
  return colorSpace;
}
VideoColorSpaceInit FallbackColorSpaceForWebContent() {
  // If we're unable to determine the color space, but we think this is from
  // Web content (canvas, image, svg, etc.), consider it's in sRGB.
  // This is step 2 of
  // https://w3c.github.io/webcodecs/#videoframe-pick-color-space
  VideoColorSpaceInit colorSpace;
  colorSpace.mFullRange = true;
  colorSpace.mMatrix = VideoMatrixCoefficients::Rgb;
  colorSpace.mTransfer = VideoTransferCharacteristics::Iec61966_2_1;
  colorSpace.mPrimaries = VideoColorPrimaries::Bt709;
  return colorSpace;
}

Maybe<CodecType> CodecStringToCodecType(const nsAString& aCodecString) {
  if (StringBeginsWith(aCodecString, u"av01"_ns)) {
    return Some(CodecType::AV1);
  }
  if (StringBeginsWith(aCodecString, u"vp8"_ns)) {
    return Some(CodecType::VP8);
  }
  if (StringBeginsWith(aCodecString, u"vp09"_ns)) {
    return Some(CodecType::VP9);
  }
  if (StringBeginsWith(aCodecString, u"avc1"_ns)) {
    return Some(CodecType::H264);
  }
  return Nothing();
}

nsString ConfigToString(const VideoDecoderConfig& aConfig) {
  nsString rv;

  auto internal = VideoDecoderConfigInternal::Create(aConfig);

  return internal->ToString();
}

};  // namespace mozilla::dom
