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
#include "mozilla/dom/VideoColorSpaceBinding.h"
#include "mozilla/gfx/Types.h"
#include "nsDebug.h"

namespace mozilla::dom {

/*
 * The followings are helpers for VideoDecoder methods
 */

nsTArray<nsCString> GuessContainers(const nsString& aCodec) {
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

/*
 * The below are helpers to operate ArrayBuffer or ArrayBufferView.
 */
template <class T>
Result<Span<uint8_t>, nsresult> GetArrayBufferData(const T& aBuffer) {
  // Get buffer's data and length before using it.
  aBuffer.ComputeState();

  CheckedInt<size_t> byteLength(sizeof(typename T::element_type));
  byteLength *= aBuffer.Length();
  if (NS_WARN_IF(!byteLength.isValid())) {
    return Err(NS_ERROR_INVALID_ARG);
  }

  return Span<uint8_t>(aBuffer.Data(), byteLength.value());
}

Result<Span<uint8_t>, nsresult> GetSharedArrayBufferData(
    const MaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer& aBuffer) {
  if (aBuffer.IsArrayBufferView()) {
    return GetArrayBufferData(aBuffer.GetAsArrayBufferView());
  }

  MOZ_ASSERT(aBuffer.IsArrayBuffer());
  return GetArrayBufferData(aBuffer.GetAsArrayBuffer());
}

Result<Span<uint8_t>, nsresult> GetSharedArrayBufferData(
    const OwningMaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer& aBuffer) {
  if (aBuffer.IsArrayBufferView()) {
    return GetArrayBufferData(aBuffer.GetAsArrayBufferView());
  }

  MOZ_ASSERT(aBuffer.IsArrayBuffer());
  return GetArrayBufferData(aBuffer.GetAsArrayBuffer());
}

static std::tuple<JS::ArrayBufferOrView, size_t, size_t> GetArrayBufferInfo(
    JSContext* aCx,
    const OwningMaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer& aBuffer) {
  if (aBuffer.IsArrayBuffer()) {
    const ArrayBuffer& buffer = aBuffer.GetAsArrayBuffer();
    buffer.ComputeState();
    CheckedInt<size_t> byteLength(buffer.Length());
    byteLength *= sizeof(JS::ArrayBuffer::DataType);
    return byteLength.isValid()
               ? std::make_tuple(
                     JS::ArrayBufferOrView::fromObject(buffer.Obj()), (size_t)0,
                     byteLength.value())
               : std::make_tuple(JS::ArrayBufferOrView::fromObject(nullptr),
                                 (size_t)0, (size_t)0);
  }

  MOZ_ASSERT(aBuffer.IsArrayBufferView());
  const ArrayBufferView& view = aBuffer.GetAsArrayBufferView();
  bool isSharedMemory;
  JS::Rooted<JSObject*> obj(aCx, view.Obj());
  return std::make_tuple(
      JS::ArrayBufferOrView::fromObject(
          JS_GetArrayBufferViewBuffer(aCx, obj, &isSharedMemory)),
      JS_GetTypedArrayByteOffset(obj), JS_GetTypedArrayByteLength(obj));
}

Result<OwningMaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer, nsresult>
CloneBuffer(
    JSContext* aCx,
    const OwningMaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer& aBuffer) {
  std::tuple<JS::ArrayBufferOrView, size_t, size_t> info =
      GetArrayBufferInfo(aCx, aBuffer);
  const JS::ArrayBufferOrView& bufOrView = std::get<0>(info);
  size_t offset = std::get<1>(info);
  size_t len = std::get<2>(info);
  if (NS_WARN_IF(!bufOrView)) {
    return Err(NS_ERROR_UNEXPECTED);
  }

  JS::Rooted<JSObject*> obj(aCx, bufOrView.asObject());
  JS::Rooted<JSObject*> cloned(aCx,
                               JS::ArrayBufferClone(aCx, obj, offset, len));
  if (NS_WARN_IF(!cloned)) {
    return Err(NS_ERROR_OUT_OF_MEMORY);
  }

  JS::Rooted<JS::Value> value(aCx, JS::ObjectValue(*cloned));
  OwningMaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer clonedBufOrView;
  if (NS_WARN_IF(!clonedBufOrView.Init(aCx, value))) {
    return Err(NS_ERROR_UNEXPECTED);
  }
  return clonedBufOrView;
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

}  // namespace mozilla::dom
