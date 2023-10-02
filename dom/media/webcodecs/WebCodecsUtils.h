/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_DOM_WEBCODECS_WEBCODECSUTILS_H
#define MOZILLA_DOM_WEBCODECS_WEBCODECSUTILS_H

#include <tuple>

#include "ErrorList.h"
#include "js/TypeDecls.h"
#include "mozilla/Maybe.h"
#include "mozilla/Result.h"
#include "mozilla/Span.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/Nullable.h"
#include "mozilla/dom/UnionTypes.h"

namespace mozilla {

namespace gfx {
enum class ColorRange : uint8_t;
enum class ColorSpace2 : uint8_t;
enum class SurfaceFormat : int8_t;
enum class TransferFunction : uint8_t;
enum class YUVColorSpace : uint8_t;
}  // namespace gfx

namespace dom {

/*
 * The followings are helpers for WebCodecs methods.
 */

nsTArray<nsCString> GuessContainers(const nsAString& aCodec);

Maybe<nsString> ParseCodecString(const nsAString& aCodec);

/*
 * Below are helpers for conversion among Maybe, Optional, and Nullable.
 */

template <typename T>
Maybe<T> OptionalToMaybe(const Optional<T>& aOptional) {
  if (aOptional.WasPassed()) {
    return Some(aOptional.Value());
  }
  return Nothing();
}

template <typename T>
const T* OptionalToPointer(const Optional<T>& aOptional) {
  return aOptional.WasPassed() ? &aOptional.Value() : nullptr;
}

template <typename T>
Maybe<T> NullableToMaybe(const Nullable<T>& aNullable) {
  if (!aNullable.IsNull()) {
    return Some(aNullable.Value());
  }
  return Nothing();
}

template <typename T>
Nullable<T> MaybeToNullable(const Maybe<T>& aOptional) {
  if (aOptional.isSome()) {
    return Nullable<T>(aOptional.value());
  }
  return Nullable<T>();
}

/*
 * Below are helpers to operate ArrayBuffer or ArrayBufferView.
 */

Result<Ok, nsresult> CloneBuffer(
    JSContext* aCx,
    OwningMaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer& aDest,
    const OwningMaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer& aSrc);

/*
 * The following are utilities to convert between VideoColorSpace values to
 * gfx's values.
 */

enum class VideoColorPrimaries : uint8_t;
enum class VideoMatrixCoefficients : uint8_t;
enum class VideoTransferCharacteristics : uint8_t;

gfx::ColorRange ToColorRange(bool aIsFullRange);

gfx::YUVColorSpace ToColorSpace(VideoMatrixCoefficients aMatrix);

gfx::TransferFunction ToTransferFunction(
    VideoTransferCharacteristics aTransfer);

gfx::ColorSpace2 ToPrimaries(VideoColorPrimaries aPrimaries);

bool ToFullRange(const gfx::ColorRange& aColorRange);

Maybe<VideoMatrixCoefficients> ToMatrixCoefficients(
    const gfx::YUVColorSpace& aColorSpace);

Maybe<VideoTransferCharacteristics> ToTransferCharacteristics(
    const gfx::TransferFunction& aTransferFunction);

Maybe<VideoColorPrimaries> ToPrimaries(const gfx::ColorSpace2& aColorSpace);

/*
 * The following are utilities to convert from gfx's formats to
 * VideoPixelFormats.
 */

enum class ImageBitmapFormat : uint8_t;
enum class VideoPixelFormat : uint8_t;

Maybe<VideoPixelFormat> SurfaceFormatToVideoPixelFormat(
    gfx::SurfaceFormat aFormat);

Maybe<VideoPixelFormat> ImageBitmapFormatToVideoPixelFormat(
    ImageBitmapFormat aFormat);

}  // namespace dom
}  // namespace mozilla

#endif  // MOZILLA_DOM_WEBCODECS_WEBCODECSUTILS_H
