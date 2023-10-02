/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebCodecsUtils.h"

#include "VideoUtils.h"
#include "js/experimental/TypedData.h"
#include "mozilla/Assertions.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/dom/ImageBitmapBinding.h"
#include "mozilla/dom/VideoColorSpaceBinding.h"
#include "mozilla/dom/VideoFrameBinding.h"
#include "mozilla/gfx/Types.h"
#include "nsDebug.h"

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
    case gfx::ColorSpace2::SRGB:
      return Nothing();
    case gfx::ColorSpace2::DISPLAY_P3:
      return Some(VideoColorPrimaries::Smpte432);
    case gfx::ColorSpace2::BT601_525:
      return Some(VideoColorPrimaries::Smpte170m);
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

}  // namespace mozilla::dom
