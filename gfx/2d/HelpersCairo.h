/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_HELPERSCAIRO_H_
#define MOZILLA_GFX_HELPERSCAIRO_H_

#include "2D.h"
#include "cairo.h"
#include "Logging.h"

namespace mozilla {
namespace gfx {

static inline cairo_operator_t
GfxOpToCairoOp(CompositionOp op)
{
  switch (op)
  {
    case CompositionOp::OP_OVER:
      return CAIRO_OPERATOR_OVER;
    case CompositionOp::OP_ADD:
      return CAIRO_OPERATOR_ADD;
    case CompositionOp::OP_ATOP:
      return CAIRO_OPERATOR_ATOP;
    case CompositionOp::OP_OUT:
      return CAIRO_OPERATOR_OUT;
    case CompositionOp::OP_IN:
      return CAIRO_OPERATOR_IN;
    case CompositionOp::OP_SOURCE:
      return CAIRO_OPERATOR_SOURCE;
    case CompositionOp::OP_DEST_IN:
      return CAIRO_OPERATOR_DEST_IN;
    case CompositionOp::OP_DEST_OUT:
      return CAIRO_OPERATOR_DEST_OUT;
    case CompositionOp::OP_DEST_OVER:
      return CAIRO_OPERATOR_DEST_OVER;
    case CompositionOp::OP_DEST_ATOP:
      return CAIRO_OPERATOR_DEST_ATOP;
    case CompositionOp::OP_XOR:
      return CAIRO_OPERATOR_XOR;
    case CompositionOp::OP_MULTIPLY:
      return CAIRO_OPERATOR_MULTIPLY;
    case CompositionOp::OP_SCREEN:
      return CAIRO_OPERATOR_SCREEN;
    case CompositionOp::OP_OVERLAY:
      return CAIRO_OPERATOR_OVERLAY;
    case CompositionOp::OP_DARKEN:
      return CAIRO_OPERATOR_DARKEN;
    case CompositionOp::OP_LIGHTEN:
      return CAIRO_OPERATOR_LIGHTEN;
    case CompositionOp::OP_COLOR_DODGE:
      return CAIRO_OPERATOR_COLOR_DODGE;
    case CompositionOp::OP_COLOR_BURN:
      return CAIRO_OPERATOR_COLOR_BURN;
    case CompositionOp::OP_HARD_LIGHT:
      return CAIRO_OPERATOR_HARD_LIGHT;
    case CompositionOp::OP_SOFT_LIGHT:
      return CAIRO_OPERATOR_SOFT_LIGHT;
    case CompositionOp::OP_DIFFERENCE:
      return CAIRO_OPERATOR_DIFFERENCE;
    case CompositionOp::OP_EXCLUSION:
      return CAIRO_OPERATOR_EXCLUSION;
    case CompositionOp::OP_HUE:
      return CAIRO_OPERATOR_HSL_HUE;
    case CompositionOp::OP_SATURATION:
      return CAIRO_OPERATOR_HSL_SATURATION;
    case CompositionOp::OP_COLOR:
      return CAIRO_OPERATOR_HSL_COLOR;
    case CompositionOp::OP_LUMINOSITY:
      return CAIRO_OPERATOR_HSL_LUMINOSITY;
    case CompositionOp::OP_COUNT:
      break;
  }

  return CAIRO_OPERATOR_OVER;
}

static inline cairo_antialias_t
GfxAntialiasToCairoAntialias(AntialiasMode antialias)
{
  switch (antialias)
  {
    case AntialiasMode::NONE:
      return CAIRO_ANTIALIAS_NONE;
    case AntialiasMode::GRAY:
      return CAIRO_ANTIALIAS_GRAY;
    case AntialiasMode::SUBPIXEL:
      return CAIRO_ANTIALIAS_SUBPIXEL;
    default:
      return CAIRO_ANTIALIAS_DEFAULT;
  }
}

static inline AntialiasMode
CairoAntialiasToGfxAntialias(cairo_antialias_t aAntialias)
{
  switch(aAntialias) {
    case CAIRO_ANTIALIAS_NONE:
      return AntialiasMode::NONE;
    case CAIRO_ANTIALIAS_GRAY:
      return AntialiasMode::GRAY;
    case CAIRO_ANTIALIAS_SUBPIXEL:
      return AntialiasMode::SUBPIXEL;
    default:
      return AntialiasMode::DEFAULT;
  }
}

static inline cairo_filter_t
GfxSamplingFilterToCairoFilter(SamplingFilter filter)
{
  switch (filter)
  {
    case SamplingFilter::GOOD:
      return CAIRO_FILTER_GOOD;
    case SamplingFilter::LINEAR:
      return CAIRO_FILTER_BILINEAR;
    case SamplingFilter::POINT:
      return CAIRO_FILTER_NEAREST;
    default:
      MOZ_CRASH("GFX: bad Cairo filter");
  }

  return CAIRO_FILTER_BILINEAR;
}

static inline cairo_extend_t
GfxExtendToCairoExtend(ExtendMode extend)
{
  switch (extend)
  {
    case ExtendMode::CLAMP:
      return CAIRO_EXTEND_PAD;
    // Cairo doesn't support tiling in only 1 direction,
    // So we have to fallback and tile in both.
    case ExtendMode::REPEAT_X:
    case ExtendMode::REPEAT_Y:
    case ExtendMode::REPEAT:
      return CAIRO_EXTEND_REPEAT;
    case ExtendMode::REFLECT:
      return CAIRO_EXTEND_REFLECT;
  }

  return CAIRO_EXTEND_PAD;
}

static inline cairo_format_t
GfxFormatToCairoFormat(SurfaceFormat format)
{
  switch (format)
  {
    case SurfaceFormat::A8R8G8B8_UINT32:
      return CAIRO_FORMAT_ARGB32;
    case SurfaceFormat::X8R8G8B8_UINT32:
      return CAIRO_FORMAT_RGB24;
    case SurfaceFormat::A8:
      return CAIRO_FORMAT_A8;
    case SurfaceFormat::R5G6B5_UINT16:
      return CAIRO_FORMAT_RGB16_565;
    default:
      gfxCriticalError() << "Unknown image format " << (int)format;
      return CAIRO_FORMAT_ARGB32;
  }
}

static inline cairo_content_t
GfxFormatToCairoContent(SurfaceFormat format)
{
  switch (format)
  {
    case SurfaceFormat::A8R8G8B8_UINT32:
      return CAIRO_CONTENT_COLOR_ALPHA;
    case SurfaceFormat::X8R8G8B8_UINT32:
    case SurfaceFormat::R5G6B5_UINT16:  //fall through
      return CAIRO_CONTENT_COLOR;
    case SurfaceFormat::A8:
      return CAIRO_CONTENT_ALPHA;
    default:
      gfxCriticalError() << "Unknown image content format " << (int)format;
      return CAIRO_CONTENT_COLOR_ALPHA;
  }
}

static inline cairo_line_join_t
GfxLineJoinToCairoLineJoin(JoinStyle style)
{
  switch (style)
  {
    case JoinStyle::BEVEL:
      return CAIRO_LINE_JOIN_BEVEL;
    case JoinStyle::ROUND:
      return CAIRO_LINE_JOIN_ROUND;
    case JoinStyle::MITER:
      return CAIRO_LINE_JOIN_MITER;
    case JoinStyle::MITER_OR_BEVEL:
      return CAIRO_LINE_JOIN_MITER;
  }

  return CAIRO_LINE_JOIN_MITER;
}

static inline cairo_line_cap_t
GfxLineCapToCairoLineCap(CapStyle style)
{
  switch (style)
  {
    case CapStyle::BUTT:
      return CAIRO_LINE_CAP_BUTT;
    case CapStyle::ROUND:
      return CAIRO_LINE_CAP_ROUND;
    case CapStyle::SQUARE:
      return CAIRO_LINE_CAP_SQUARE;
  }

  return CAIRO_LINE_CAP_BUTT;
}

static inline SurfaceFormat
CairoContentToGfxFormat(cairo_content_t content)
{
  switch (content)
  {
    case CAIRO_CONTENT_COLOR_ALPHA:
      return SurfaceFormat::A8R8G8B8_UINT32;
    case CAIRO_CONTENT_COLOR:
      // BEWARE! format may be 565
      return SurfaceFormat::X8R8G8B8_UINT32;
    case CAIRO_CONTENT_ALPHA:
      return SurfaceFormat::A8;
  }

  return SurfaceFormat::B8G8R8A8;
}

static inline SurfaceFormat
CairoFormatToGfxFormat(cairo_format_t format)
{
  switch (format) {
    case CAIRO_FORMAT_ARGB32:
      return SurfaceFormat::A8R8G8B8_UINT32;
    case CAIRO_FORMAT_RGB24:
      return SurfaceFormat::X8R8G8B8_UINT32;
    case CAIRO_FORMAT_A8:
      return SurfaceFormat::A8;
    case CAIRO_FORMAT_RGB16_565:
      return SurfaceFormat::R5G6B5_UINT16;
    default:
      gfxCriticalError() << "Unknown cairo format " << format;
      return SurfaceFormat::UNKNOWN;
  }
}

static inline FontHinting
CairoHintingToGfxHinting(cairo_hint_style_t aHintStyle)
{
  switch (aHintStyle) {
    case CAIRO_HINT_STYLE_NONE:
      return FontHinting::NONE;
    case CAIRO_HINT_STYLE_SLIGHT:
      return FontHinting::LIGHT;
    case CAIRO_HINT_STYLE_MEDIUM:
      return FontHinting::NORMAL;
    case CAIRO_HINT_STYLE_FULL:
      return FontHinting::FULL;
    default:
      return FontHinting::NORMAL;
  }
}

SurfaceFormat GfxFormatForCairoSurface(cairo_surface_t* surface);

static inline void
GfxMatrixToCairoMatrix(const Matrix& mat, cairo_matrix_t& retval)
{
  cairo_matrix_init(&retval, mat._11, mat._12, mat._21, mat._22, mat._31, mat._32);
}

static inline void
SetCairoStrokeOptions(cairo_t* aCtx, const StrokeOptions& aStrokeOptions)
{
  cairo_set_line_width(aCtx, aStrokeOptions.mLineWidth);

  cairo_set_miter_limit(aCtx, aStrokeOptions.mMiterLimit);

  if (aStrokeOptions.mDashPattern) {
    // Convert array of floats to array of doubles
    std::vector<double> dashes(aStrokeOptions.mDashLength);
    bool nonZero = false;
    for (size_t i = 0; i < aStrokeOptions.mDashLength; ++i) {
      if (aStrokeOptions.mDashPattern[i] != 0) {
        nonZero = true;
      }
      dashes[i] = aStrokeOptions.mDashPattern[i];
    }
    // Avoid all-zero patterns that would trigger the CAIRO_STATUS_INVALID_DASH context error state.
    if (nonZero) {
      cairo_set_dash(aCtx, &dashes[0], aStrokeOptions.mDashLength,
                     aStrokeOptions.mDashOffset);
    }
  }

  cairo_set_line_join(aCtx, GfxLineJoinToCairoLineJoin(aStrokeOptions.mLineJoin));

  cairo_set_line_cap(aCtx, GfxLineCapToCairoLineCap(aStrokeOptions.mLineCap));
}

static inline cairo_fill_rule_t
GfxFillRuleToCairoFillRule(FillRule rule)
{
  switch (rule)
  {
    case FillRule::FILL_WINDING:
      return CAIRO_FILL_RULE_WINDING;
    case FillRule::FILL_EVEN_ODD:
      return CAIRO_FILL_RULE_EVEN_ODD;
  }

  return CAIRO_FILL_RULE_WINDING;
}

// RAII class for temporarily changing the cairo matrix transform. It will use
// the given matrix transform while it is in scope. When it goes out of scope
// it will put the cairo context back the way it was.

class CairoTempMatrix
{
public:
  CairoTempMatrix(cairo_t* aCtx, const Matrix& aMatrix)
    : mCtx(aCtx)
  {
    cairo_get_matrix(aCtx, &mSaveMatrix);
    cairo_matrix_t matrix;
    GfxMatrixToCairoMatrix(aMatrix, matrix);
    cairo_set_matrix(aCtx, &matrix);
  }

  ~CairoTempMatrix()
  {
    cairo_set_matrix(mCtx, &mSaveMatrix);
  }

private:
  cairo_t* mCtx;
  cairo_matrix_t mSaveMatrix;
};

} // namespace gfx
} // namespace mozilla

#endif /* MOZILLA_GFX_HELPERSCAIRO_H_ */
