/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_HELPERSSKIA_H_
#define MOZILLA_GFX_HELPERSSKIA_H_

#include "2D.h"
#include "skia/include/core/SkCanvas.h"
#include "skia/include/core/SkPathEffect.h"
#include "skia/include/core/SkPathTypes.h"
#include "skia/include/core/SkShader.h"
#include "skia/include/effects/SkDashPathEffect.h"
#include "mozilla/Assertions.h"
#include <cmath>
#include <vector>
#include "nsDebug.h"

namespace mozilla {
namespace gfx {

static inline SkColorType GfxFormatToSkiaColorType(SurfaceFormat format) {
  switch (format) {
    case SurfaceFormat::B8G8R8A8:
      return kBGRA_8888_SkColorType;
    case SurfaceFormat::B8G8R8X8:
      // We probably need to do something here.
      return kBGRA_8888_SkColorType;
    case SurfaceFormat::R5G6B5_UINT16:
      return kRGB_565_SkColorType;
    case SurfaceFormat::A8:
      return kAlpha_8_SkColorType;
    case SurfaceFormat::R8G8B8A8:
      return kRGBA_8888_SkColorType;
    case SurfaceFormat::A8R8G8B8:
      MOZ_DIAGNOSTIC_ASSERT(false, "A8R8G8B8 unsupported by Skia");
      return kRGBA_8888_SkColorType;
    default:
      MOZ_DIAGNOSTIC_ASSERT(false, "Unknown surface format");
      return kRGBA_8888_SkColorType;
  }
}

static inline SurfaceFormat SkiaColorTypeToGfxFormat(
    SkColorType aColorType, SkAlphaType aAlphaType = kPremul_SkAlphaType) {
  switch (aColorType) {
    case kBGRA_8888_SkColorType:
      return aAlphaType == kOpaque_SkAlphaType ? SurfaceFormat::B8G8R8X8
                                               : SurfaceFormat::B8G8R8A8;
    case kRGB_565_SkColorType:
      return SurfaceFormat::R5G6B5_UINT16;
    case kAlpha_8_SkColorType:
      return SurfaceFormat::A8;
    default:
      return SurfaceFormat::B8G8R8A8;
  }
}

static inline SkAlphaType GfxFormatToSkiaAlphaType(SurfaceFormat format) {
  switch (format) {
    case SurfaceFormat::B8G8R8X8:
    case SurfaceFormat::R5G6B5_UINT16:
      return kOpaque_SkAlphaType;
    default:
      return kPremul_SkAlphaType;
  }
}

static inline SkImageInfo MakeSkiaImageInfo(const IntSize& aSize,
                                            SurfaceFormat aFormat) {
  return SkImageInfo::Make(aSize.width, aSize.height,
                           GfxFormatToSkiaColorType(aFormat),
                           GfxFormatToSkiaAlphaType(aFormat));
}

static inline void GfxMatrixToSkiaMatrix(const Matrix& mat, SkMatrix& retval) {
  retval.setAll(SkFloatToScalar(mat._11), SkFloatToScalar(mat._21),
                SkFloatToScalar(mat._31), SkFloatToScalar(mat._12),
                SkFloatToScalar(mat._22), SkFloatToScalar(mat._32), 0, 0,
                SK_Scalar1);
}

static inline void GfxMatrixToSkiaMatrix(const Matrix4x4& aMatrix,
                                         SkMatrix& aResult) {
  aResult.setAll(SkFloatToScalar(aMatrix._11), SkFloatToScalar(aMatrix._21),
                 SkFloatToScalar(aMatrix._41), SkFloatToScalar(aMatrix._12),
                 SkFloatToScalar(aMatrix._22), SkFloatToScalar(aMatrix._42),
                 SkFloatToScalar(aMatrix._14), SkFloatToScalar(aMatrix._24),
                 SkFloatToScalar(aMatrix._44));
}

static inline SkPaint::Cap CapStyleToSkiaCap(CapStyle aCap) {
  switch (aCap) {
    case CapStyle::BUTT:
      return SkPaint::kButt_Cap;
    case CapStyle::ROUND:
      return SkPaint::kRound_Cap;
    case CapStyle::SQUARE:
      return SkPaint::kSquare_Cap;
  }
  return SkPaint::kDefault_Cap;
}

static inline SkPaint::Join JoinStyleToSkiaJoin(JoinStyle aJoin) {
  switch (aJoin) {
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

static inline bool StrokeOptionsToPaint(SkPaint& aPaint,
                                        const StrokeOptions& aOptions,
                                        bool aUsePathEffects = true) {
  // Skia renders 0 width strokes with a width of 1 (and in black),
  // so we should just skip the draw call entirely.
  // Skia does not handle non-finite line widths.
  if (!aOptions.mLineWidth || !std::isfinite(aOptions.mLineWidth)) {
    return false;
  }
  aPaint.setStrokeWidth(SkFloatToScalar(aOptions.mLineWidth));
  aPaint.setStrokeMiter(SkFloatToScalar(aOptions.mMiterLimit));
  aPaint.setStrokeCap(CapStyleToSkiaCap(aOptions.mLineCap));
  aPaint.setStrokeJoin(JoinStyleToSkiaJoin(aOptions.mLineJoin));

  if (aOptions.mDashLength > 0 && aUsePathEffects) {
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
      pattern[i] =
          SkFloatToScalar(aOptions.mDashPattern[i % aOptions.mDashLength]);
    }

    auto dash = SkDashPathEffect::Make(&pattern.front(), dashCount,
                                       SkFloatToScalar(aOptions.mDashOffset));
    aPaint.setPathEffect(dash);
  }

  aPaint.setStyle(SkPaint::kStroke_Style);
  return true;
}

static inline SkBlendMode GfxOpToSkiaOp(CompositionOp op) {
  switch (op) {
    case CompositionOp::OP_CLEAR:
      return SkBlendMode::kClear;
    case CompositionOp::OP_OVER:
      return SkBlendMode::kSrcOver;
    case CompositionOp::OP_ADD:
      return SkBlendMode::kPlus;
    case CompositionOp::OP_ATOP:
      return SkBlendMode::kSrcATop;
    case CompositionOp::OP_OUT:
      return SkBlendMode::kSrcOut;
    case CompositionOp::OP_IN:
      return SkBlendMode::kSrcIn;
    case CompositionOp::OP_SOURCE:
      return SkBlendMode::kSrc;
    case CompositionOp::OP_DEST_IN:
      return SkBlendMode::kDstIn;
    case CompositionOp::OP_DEST_OUT:
      return SkBlendMode::kDstOut;
    case CompositionOp::OP_DEST_OVER:
      return SkBlendMode::kDstOver;
    case CompositionOp::OP_DEST_ATOP:
      return SkBlendMode::kDstATop;
    case CompositionOp::OP_XOR:
      return SkBlendMode::kXor;
    case CompositionOp::OP_MULTIPLY:
      return SkBlendMode::kMultiply;
    case CompositionOp::OP_SCREEN:
      return SkBlendMode::kScreen;
    case CompositionOp::OP_OVERLAY:
      return SkBlendMode::kOverlay;
    case CompositionOp::OP_DARKEN:
      return SkBlendMode::kDarken;
    case CompositionOp::OP_LIGHTEN:
      return SkBlendMode::kLighten;
    case CompositionOp::OP_COLOR_DODGE:
      return SkBlendMode::kColorDodge;
    case CompositionOp::OP_COLOR_BURN:
      return SkBlendMode::kColorBurn;
    case CompositionOp::OP_HARD_LIGHT:
      return SkBlendMode::kHardLight;
    case CompositionOp::OP_SOFT_LIGHT:
      return SkBlendMode::kSoftLight;
    case CompositionOp::OP_DIFFERENCE:
      return SkBlendMode::kDifference;
    case CompositionOp::OP_EXCLUSION:
      return SkBlendMode::kExclusion;
    case CompositionOp::OP_HUE:
      return SkBlendMode::kHue;
    case CompositionOp::OP_SATURATION:
      return SkBlendMode::kSaturation;
    case CompositionOp::OP_COLOR:
      return SkBlendMode::kColor;
    case CompositionOp::OP_LUMINOSITY:
      return SkBlendMode::kLuminosity;
    case CompositionOp::OP_COUNT:
      break;
  }

  return SkBlendMode::kSrcOver;
}

/* There's quite a bit of inconsistency about
 * whether float colors should be rounded with .5f.
 * We choose to do it to match cairo which also
 * happens to match the Direct3D specs */
static inline U8CPU ColorFloatToByte(Float color) {
  // XXX: do a better job converting to int
  return U8CPU(color * 255.f + .5f);
};

static inline SkColor ColorToSkColor(const DeviceColor& color, Float aAlpha) {
  return SkColorSetARGB(ColorFloatToByte(color.a * aAlpha),
                        ColorFloatToByte(color.r), ColorFloatToByte(color.g),
                        ColorFloatToByte(color.b));
}

static inline SkPoint PointToSkPoint(const Point& aPoint) {
  return SkPoint::Make(SkFloatToScalar(aPoint.x), SkFloatToScalar(aPoint.y));
}

static inline SkRect RectToSkRect(const Rect& aRect) {
  return SkRect::MakeXYWH(
      SkFloatToScalar(aRect.X()), SkFloatToScalar(aRect.Y()),
      SkFloatToScalar(aRect.Width()), SkFloatToScalar(aRect.Height()));
}

static inline SkRect IntRectToSkRect(const IntRect& aRect) {
  return SkRect::MakeXYWH(SkIntToScalar(aRect.X()), SkIntToScalar(aRect.Y()),
                          SkIntToScalar(aRect.Width()),
                          SkIntToScalar(aRect.Height()));
}

static inline SkIRect RectToSkIRect(const Rect& aRect) {
  return SkIRect::MakeXYWH(int32_t(aRect.X()), int32_t(aRect.Y()),
                           int32_t(aRect.Width()), int32_t(aRect.Height()));
}

static inline SkIRect IntRectToSkIRect(const IntRect& aRect) {
  return SkIRect::MakeXYWH(aRect.X(), aRect.Y(), aRect.Width(), aRect.Height());
}

static inline IntRect SkIRectToIntRect(const SkIRect& aRect) {
  return IntRect(aRect.x(), aRect.y(), aRect.width(), aRect.height());
}

static inline Point SkPointToPoint(const SkPoint& aPoint) {
  return Point(SkScalarToFloat(aPoint.x()), SkScalarToFloat(aPoint.y()));
}

static inline Rect SkRectToRect(const SkRect& aRect) {
  return Rect(SkScalarToFloat(aRect.x()), SkScalarToFloat(aRect.y()),
              SkScalarToFloat(aRect.width()), SkScalarToFloat(aRect.height()));
}

static inline SkTileMode ExtendModeToTileMode(ExtendMode aMode, Axis aAxis) {
  switch (aMode) {
    case ExtendMode::CLAMP:
      return SkTileMode::kClamp;
    case ExtendMode::REPEAT:
      return SkTileMode::kRepeat;
    case ExtendMode::REFLECT:
      return SkTileMode::kMirror;
    case ExtendMode::REPEAT_X: {
      return aAxis == Axis::X_AXIS ? SkTileMode::kRepeat : SkTileMode::kClamp;
    }
    case ExtendMode::REPEAT_Y: {
      return aAxis == Axis::Y_AXIS ? SkTileMode::kRepeat : SkTileMode::kClamp;
    }
  }
  return SkTileMode::kClamp;
}

static inline SkFontHinting GfxHintingToSkiaHinting(FontHinting aHinting) {
  switch (aHinting) {
    case FontHinting::NONE:
      return SkFontHinting::kNone;
    case FontHinting::LIGHT:
      return SkFontHinting::kSlight;
    case FontHinting::NORMAL:
      return SkFontHinting::kNormal;
    case FontHinting::FULL:
      return SkFontHinting::kFull;
  }
  return SkFontHinting::kNormal;
}

static inline FillRule GetFillRule(SkPathFillType aFillType) {
  switch (aFillType) {
    case SkPathFillType::kWinding:
      return FillRule::FILL_WINDING;
    case SkPathFillType::kEvenOdd:
      return FillRule::FILL_EVEN_ODD;
    case SkPathFillType::kInverseWinding:
    case SkPathFillType::kInverseEvenOdd:
    default:
      NS_WARNING("Unsupported fill type\n");
      break;
  }

  return FillRule::FILL_EVEN_ODD;
}

/**
 * Returns true if the canvas is backed by pixels.  Returns false if the canvas
 * wraps an SkPDFDocument, for example.
 *
 * Note: It is not clear whether the test used to implement this function may
 * result in it returning false in some circumstances even when the canvas
 * _is_ pixel backed.  In other words maybe it is possible for such a canvas to
 * have kUnknown_SkPixelGeometry?
 */
static inline bool IsBackedByPixels(const SkCanvas* aCanvas) {
  SkSurfaceProps props(0, kUnknown_SkPixelGeometry);
  if (!aCanvas->getProps(&props) ||
      props.pixelGeometry() == kUnknown_SkPixelGeometry) {
    return false;
  }
  return true;
}

/**
 * Computes appropriate resolution scale to be used with SkPath::getFillPath
 * based on the scaling of the supplied transform.
 */
float ComputeResScaleForStroking(const Matrix& aTransform);

/**
 * This is a wrapper around SkGeometry's SkConic that can be used to convert
 * conic sections in an SkPath to a sequence of quadratic curves. The quads
 * vector is organized such that for the Nth quad, it's control points are
 * 2*N, 2*N+1, 2*N+2. This function returns the resulting number of quads.
 */
int ConvertConicToQuads(const Point& aP0, const Point& aP1, const Point& aP2,
                        float aWeight, std::vector<Point>& aQuads);

}  // namespace gfx
}  // namespace mozilla

#endif /* MOZILLA_GFX_HELPERSSKIA_H_ */
