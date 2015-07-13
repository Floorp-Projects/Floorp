/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_HELPERSSKIA_H_
#define MOZILLA_GFX_HELPERSSKIA_H_

#include "2D.h"
#include "skia/SkCanvas.h"
#include "skia/SkDashPathEffect.h"
#include "skia/SkShader.h"
#ifdef USE_SKIA_GPU
#include "skia/GrTypes.h"
#endif
#include "mozilla/Assertions.h"
#include <vector>

namespace mozilla {
namespace gfx {

static inline SkColorType
GfxFormatToSkiaColorType(SurfaceFormat format)
{
  switch (format)
  {
    case SurfaceFormat::B8G8R8A8:
      return kBGRA_8888_SkColorType;
    case SurfaceFormat::B8G8R8X8:
      // We probably need to do something here.
      return kBGRA_8888_SkColorType;
    case SurfaceFormat::R5G6B5:
      return kRGB_565_SkColorType;
    case SurfaceFormat::A8:
      return kAlpha_8_SkColorType;
    default:
      return kRGBA_8888_SkColorType;
  }
}

static inline SurfaceFormat
SkiaColorTypeToGfxFormat(SkColorType type)
{
  switch (type)
  {
    case kBGRA_8888_SkColorType:
      return SurfaceFormat::B8G8R8A8;
    case kRGB_565_SkColorType:
      return SurfaceFormat::R5G6B5;
    case kAlpha_8_SkColorType:
      return SurfaceFormat::A8;
    default:
      return SurfaceFormat::B8G8R8A8;
  }
}

#ifdef USE_SKIA_GPU
static inline GrPixelConfig
GfxFormatToGrConfig(SurfaceFormat format)
{
  switch (format)
  {
    case SurfaceFormat::B8G8R8A8:
      return kBGRA_8888_GrPixelConfig;
    case SurfaceFormat::B8G8R8X8:
      // We probably need to do something here.
      return kBGRA_8888_GrPixelConfig;
    case SurfaceFormat::R5G6B5:
      return kRGB_565_GrPixelConfig;
    case SurfaceFormat::A8:
      return kAlpha_8_GrPixelConfig;
    default:
      return kRGBA_8888_GrPixelConfig;
  }

}
#endif
static inline void
GfxMatrixToSkiaMatrix(const Matrix& mat, SkMatrix& retval)
{
    retval.setAll(SkFloatToScalar(mat._11), SkFloatToScalar(mat._21), SkFloatToScalar(mat._31),
                  SkFloatToScalar(mat._12), SkFloatToScalar(mat._22), SkFloatToScalar(mat._32),
                  0, 0, SK_Scalar1);
}

static inline SkPaint::Cap
CapStyleToSkiaCap(CapStyle aCap)
{
  switch (aCap)
  {
    case CapStyle::BUTT:
      return SkPaint::kButt_Cap;
    case CapStyle::ROUND:
      return SkPaint::kRound_Cap;
    case CapStyle::SQUARE:
      return SkPaint::kSquare_Cap;
  }
  return SkPaint::kDefault_Cap;
}

static inline SkPaint::Join
JoinStyleToSkiaJoin(JoinStyle aJoin)
{
  switch (aJoin)
  {
    case JoinStyle::BEVEL:
      return SkPaint::kBevel_Join;
    case JoinStyle::ROUND:
      return SkPaint::kRound_Join;
    case JoinStyle::MITER:
    case JoinStyle::MITER_OR_BEVEL:
      return SkPaint::kMiter_Join;
  }
  return SkPaint::kDefault_Join;
}

static inline bool
StrokeOptionsToPaint(SkPaint& aPaint, const StrokeOptions &aOptions)
{
  // Skia renders 0 width strokes with a width of 1 (and in black),
  // so we should just skip the draw call entirely.
  if (!aOptions.mLineWidth) {
    return false;
  }
  aPaint.setStrokeWidth(SkFloatToScalar(aOptions.mLineWidth));
  aPaint.setStrokeMiter(SkFloatToScalar(aOptions.mMiterLimit));
  aPaint.setStrokeCap(CapStyleToSkiaCap(aOptions.mLineCap));
  aPaint.setStrokeJoin(JoinStyleToSkiaJoin(aOptions.mLineJoin));

  if (aOptions.mDashLength > 0) {
    // Skia only supports dash arrays that are multiples of 2.
    uint32_t dashCount;

    if (aOptions.mDashLength % 2 == 0) {
      dashCount = aOptions.mDashLength;
    } else {
      dashCount = aOptions.mDashLength * 2;
    }

    std::vector<SkScalar> pattern;
    pattern.resize(dashCount);

    for (uint32_t i = 0; i < dashCount; i++) {
      pattern[i] = SkFloatToScalar(aOptions.mDashPattern[i % aOptions.mDashLength]);
      if (!pattern[i])
          pattern[i] = SK_ScalarNearlyZero;
    }

    SkDashPathEffect* dash = SkDashPathEffect::Create(&pattern.front(),
                                                      dashCount, 
                                                      SkFloatToScalar(aOptions.mDashOffset));
    SkSafeUnref(aPaint.setPathEffect(dash));
  }

  aPaint.setStyle(SkPaint::kStroke_Style);
  return true;
}

static inline SkXfermode::Mode
GfxOpToSkiaOp(CompositionOp op)
{
  switch (op)
  {
    case CompositionOp::OP_OVER:
      return SkXfermode::kSrcOver_Mode;
    case CompositionOp::OP_ADD:
      return SkXfermode::kPlus_Mode;
    case CompositionOp::OP_ATOP:
      return SkXfermode::kSrcATop_Mode;
    case CompositionOp::OP_OUT:
      return SkXfermode::kSrcOut_Mode;
    case CompositionOp::OP_IN:
      return SkXfermode::kSrcIn_Mode;
    case CompositionOp::OP_SOURCE:
      return SkXfermode::kSrc_Mode;
    case CompositionOp::OP_DEST_IN:
      return SkXfermode::kDstIn_Mode;
    case CompositionOp::OP_DEST_OUT:
      return SkXfermode::kDstOut_Mode;
    case CompositionOp::OP_DEST_OVER:
      return SkXfermode::kDstOver_Mode;
    case CompositionOp::OP_DEST_ATOP:
      return SkXfermode::kDstATop_Mode;
    case CompositionOp::OP_XOR:
      return SkXfermode::kXor_Mode;
    case CompositionOp::OP_MULTIPLY:
      return SkXfermode::kMultiply_Mode;
    case CompositionOp::OP_SCREEN:
      return SkXfermode::kScreen_Mode;
    case CompositionOp::OP_OVERLAY:
      return SkXfermode::kOverlay_Mode;
    case CompositionOp::OP_DARKEN:
      return SkXfermode::kDarken_Mode;
    case CompositionOp::OP_LIGHTEN:
      return SkXfermode::kLighten_Mode;
    case CompositionOp::OP_COLOR_DODGE:
      return SkXfermode::kColorDodge_Mode;
    case CompositionOp::OP_COLOR_BURN:
      return SkXfermode::kColorBurn_Mode;
    case CompositionOp::OP_HARD_LIGHT:
      return SkXfermode::kHardLight_Mode;
    case CompositionOp::OP_SOFT_LIGHT:
      return SkXfermode::kSoftLight_Mode;
    case CompositionOp::OP_DIFFERENCE:
      return SkXfermode::kDifference_Mode;
    case CompositionOp::OP_EXCLUSION:
      return SkXfermode::kExclusion_Mode;
    case CompositionOp::OP_HUE:
      return SkXfermode::kHue_Mode;
    case CompositionOp::OP_SATURATION:
      return SkXfermode::kSaturation_Mode;
    case CompositionOp::OP_COLOR:
      return SkXfermode::kColor_Mode;
    case CompositionOp::OP_LUMINOSITY:
      return SkXfermode::kLuminosity_Mode;
    default:
      return SkXfermode::kSrcOver_Mode;
  }
}

/* There's quite a bit of inconsistency about
 * whether float colors should be rounded with .5f.
 * We choose to do it to match cairo which also
 * happens to match the Direct3D specs */
static inline U8CPU ColorFloatToByte(Float color)
{
  //XXX: do a better job converting to int
  return U8CPU(color*255.f + .5f);
};

static inline SkColor ColorToSkColor(const Color &color, Float aAlpha)
{
  return SkColorSetARGB(ColorFloatToByte(color.a*aAlpha), ColorFloatToByte(color.r),
                        ColorFloatToByte(color.g), ColorFloatToByte(color.b));
}

static inline SkRect
RectToSkRect(const Rect& aRect)
{
  return SkRect::MakeXYWH(SkFloatToScalar(aRect.x), SkFloatToScalar(aRect.y), 
                          SkFloatToScalar(aRect.width), SkFloatToScalar(aRect.height));
}

static inline SkRect
IntRectToSkRect(const IntRect& aRect)
{
  return SkRect::MakeXYWH(SkIntToScalar(aRect.x), SkIntToScalar(aRect.y), 
                          SkIntToScalar(aRect.width), SkIntToScalar(aRect.height));
}

static inline SkIRect
RectToSkIRect(const Rect& aRect)
{
  return SkIRect::MakeXYWH(int32_t(aRect.x), int32_t(aRect.y),
                           int32_t(aRect.width), int32_t(aRect.height));
}

static inline SkIRect
IntRectToSkIRect(const IntRect& aRect)
{
  return SkIRect::MakeXYWH(aRect.x, aRect.y, aRect.width, aRect.height);
}

static inline Point
SkPointToPoint(const SkPoint &aPoint)
{
  return Point(SkScalarToFloat(aPoint.x()), SkScalarToFloat(aPoint.y()));
}

static inline Rect
SkRectToRect(const SkRect &aRect)
{
  return Rect(SkScalarToFloat(aRect.x()), SkScalarToFloat(aRect.y()),
              SkScalarToFloat(aRect.width()), SkScalarToFloat(aRect.height()));
}

static inline SkShader::TileMode
ExtendModeToTileMode(ExtendMode aMode)
{
  switch (aMode)
  {
    case ExtendMode::CLAMP:
      return SkShader::kClamp_TileMode;
    case ExtendMode::REPEAT:
      return SkShader::kRepeat_TileMode;
    case ExtendMode::REFLECT:
      return SkShader::kMirror_TileMode;
  }
  return SkShader::kClamp_TileMode;
}

// The following class was imported from Skia, which is under the 
// following licence:
//
// Copyright (c) 2011 Google Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
// * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

template <typename T> class RefPtrSkia {
public:
  RefPtrSkia() : fObj(NULL) {}
  explicit RefPtrSkia(T* obj) : fObj(obj) { SkSafeRef(fObj); }
  RefPtrSkia(const RefPtrSkia& o) : fObj(o.fObj) { SkSafeRef(fObj); }
  ~RefPtrSkia() { SkSafeUnref(fObj); }

  RefPtrSkia& operator=(const RefPtrSkia& rp) {
    SkRefCnt_SafeAssign(fObj, rp.fObj);
    return *this;
  }
  RefPtrSkia& operator=(T* obj) {
    SkRefCnt_SafeAssign(fObj, obj);
    return *this;
  }

  T* get() const { return fObj; }
  T& operator*() const { return *fObj; }
  T* operator->() const { return fObj; }

  RefPtrSkia& adopt(T* obj) {
    SkSafeUnref(fObj);
    fObj = obj;
    return *this;
  }

  typedef T* RefPtrSkia::*unspecified_bool_type;
  operator unspecified_bool_type() const {
    return fObj ? &RefPtrSkia::fObj : NULL;
  }

private:
  T* fObj;
};

// End of code imported from Skia.

} // namespace gfx
} // namespace mozilla

#endif /* MOZILLA_GFX_HELPERSSKIA_H_ */
