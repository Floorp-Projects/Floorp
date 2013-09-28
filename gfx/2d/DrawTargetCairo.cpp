/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DrawTargetCairo.h"

#include "SourceSurfaceCairo.h"
#include "PathCairo.h"
#include "HelpersCairo.h"
#include "ScaledFontBase.h"
#include "BorrowedContext.h"

#include "cairo.h"
#include "cairo-tee.h"
#include <string.h>

#include "Blur.h"
#include "Logging.h"
#include "Tools.h"

#ifdef CAIRO_HAS_QUARTZ_SURFACE
#include "cairo-quartz.h"
#include <ApplicationServices/ApplicationServices.h>
#endif

#ifdef CAIRO_HAS_XLIB_SURFACE
#include "cairo-xlib.h"
#endif

#ifdef CAIRO_HAS_WIN32_SURFACE
#include "cairo-win32.h"
#endif

#include <algorithm>

namespace mozilla {
namespace gfx {

namespace {

// An RAII class to prepare to draw a context and optional path. Saves and
// restores the context on construction/destruction.
class AutoPrepareForDrawing
{
public:
  AutoPrepareForDrawing(DrawTargetCairo* dt, cairo_t* ctx)
    : mCtx(ctx)
  {
    dt->PrepareForDrawing(ctx);
    cairo_save(mCtx);
    MOZ_ASSERT(cairo_status(mCtx) || dt->GetTransform() == GetTransform());
  }

  AutoPrepareForDrawing(DrawTargetCairo* dt, cairo_t* ctx, const Path* path)
    : mCtx(ctx)
  {
    dt->PrepareForDrawing(ctx, path);
    cairo_save(mCtx);
    MOZ_ASSERT(cairo_status(mCtx) || dt->GetTransform() == GetTransform());
  }

  ~AutoPrepareForDrawing() { cairo_restore(mCtx); }

private:
#ifdef DEBUG
  Matrix GetTransform()
  {
    cairo_matrix_t mat;
    cairo_get_matrix(mCtx, &mat);
    return Matrix(mat.xx, mat.yx, mat.xy, mat.yy, mat.x0, mat.y0);
  }
#endif

  cairo_t* mCtx;
};


} // end anonymous namespace

static bool
GetCairoSurfaceSize(cairo_surface_t* surface, IntSize& size)
{
  switch (cairo_surface_get_type(surface))
  {
    case CAIRO_SURFACE_TYPE_IMAGE:
    {
      size.width = cairo_image_surface_get_width(surface);
      size.height = cairo_image_surface_get_height(surface);
      return true;
    }

#ifdef CAIRO_HAS_XLIB_SURFACE
    case CAIRO_SURFACE_TYPE_XLIB:
    {
      size.width = cairo_xlib_surface_get_width(surface);
      size.height = cairo_xlib_surface_get_height(surface);
      return true;
    }
#endif

#ifdef CAIRO_HAS_QUARTZ_SURFACE
    case CAIRO_SURFACE_TYPE_QUARTZ:
    {
      CGContextRef cgc = cairo_quartz_surface_get_cg_context(surface);

      // It's valid to call these CGBitmapContext functions on non-bitmap
      // contexts; they'll just return 0 in that case.
      size.width = CGBitmapContextGetWidth(cgc);
      size.height = CGBitmapContextGetHeight(cgc);
      return true;
    }
#endif
#ifdef CAIRO_HAS_WIN32_SURFACE
    case CAIRO_SURFACE_TYPE_WIN32:
    case CAIRO_SURFACE_TYPE_WIN32_PRINTING:
    {
      size.width = cairo_win32_surface_get_width(surface);
      size.height = cairo_win32_surface_get_height(surface);
      return true;
    }
#endif

    default:
      return false;
  }
}

static bool
PatternIsCompatible(const Pattern& aPattern)
{
  switch (aPattern.GetType())
  {
    case PATTERN_LINEAR_GRADIENT:
    {
      const LinearGradientPattern& pattern = static_cast<const LinearGradientPattern&>(aPattern);
      return pattern.mStops->GetBackendType() == BACKEND_CAIRO;
    }
    case PATTERN_RADIAL_GRADIENT:
    {
      const RadialGradientPattern& pattern = static_cast<const RadialGradientPattern&>(aPattern);
      return pattern.mStops->GetBackendType() == BACKEND_CAIRO;
    }
    default:
      return true;
  }
}

static cairo_user_data_key_t surfaceDataKey;

void
ReleaseData(void* aData)
{
  static_cast<DataSourceSurface*>(aData)->Release();
}

/**
 * Returns cairo surface for the given SourceSurface.
 * If possible, it will use the cairo_surface associated with aSurface,
 * otherwise, it will create a new cairo_surface.
 * In either case, the caller must call cairo_surface_destroy on the
 * result when it is done with it.
 */
cairo_surface_t*
GetCairoSurfaceForSourceSurface(SourceSurface *aSurface, bool aExistingOnly = false)
{
  if (aSurface->GetType() == SURFACE_CAIRO) {
    cairo_surface_t* surf = static_cast<SourceSurfaceCairo*>(aSurface)->GetSurface();
    cairo_surface_reference(surf);
    return surf;
  }

  if (aSurface->GetType() == SURFACE_CAIRO_IMAGE) {
    cairo_surface_t* surf =
      static_cast<const DataSourceSurfaceCairo*>(aSurface)->GetSurface();
    cairo_surface_reference(surf);
    return surf;
  }

  if (aExistingOnly) {
    return nullptr;
  }

  RefPtr<DataSourceSurface> data = aSurface->GetDataSurface();
  if (!data) {
    return nullptr;
  }

  cairo_surface_t* surf =
    cairo_image_surface_create_for_data(data->GetData(),
                                        GfxFormatToCairoFormat(data->GetFormat()),
                                        data->GetSize().width,
                                        data->GetSize().height,
                                        data->Stride());

  // In certain scenarios, requesting larger than 8k image fails.  Bug 803568
  // covers the details of how to run into it, but the full detailed
  // investigation hasn't been done to determine the underlying cause.  We
  // will just handle the failure to allocate the surface to avoid a crash.
  if (cairo_surface_status(surf)) {
    return nullptr;
  }

  cairo_surface_set_user_data(surf,
 				                      &surfaceDataKey,
 				                      data.forget().drop(),
 				                      ReleaseData);
  return surf;
}

// An RAII class to temporarily clear any device offset set
// on a surface. Note that this does not take a reference to the
// surface.
class AutoClearDeviceOffset
{
public:
  AutoClearDeviceOffset(SourceSurface* aSurface)
    : mSurface(nullptr)
  {
    Init(aSurface);
  }

  AutoClearDeviceOffset(const Pattern& aPattern)
    : mSurface(nullptr)
  {
    if (aPattern.GetType() == PATTERN_SURFACE) {
      const SurfacePattern& pattern = static_cast<const SurfacePattern&>(aPattern);
      Init(pattern.mSurface);
    }
  }

  ~AutoClearDeviceOffset()
  {
    if (mSurface) {
      cairo_surface_set_device_offset(mSurface, mX, mY);
    }
  }

private:
  void Init(SourceSurface* aSurface)
  {
    cairo_surface_t* surface = GetCairoSurfaceForSourceSurface(aSurface, true);
    if (surface) {
      Init(surface);
      cairo_surface_destroy(surface);
    }
  }

  void Init(cairo_surface_t *aSurface)
  {
    mSurface = aSurface;
    cairo_surface_get_device_offset(mSurface, &mX, &mY);
    cairo_surface_set_device_offset(mSurface, 0, 0);
  }

  cairo_surface_t* mSurface;
  double mX;
  double mY;
};

// Never returns nullptr. As such, you must always pass in Cairo-compatible
// patterns, most notably gradients with a GradientStopCairo.
// The pattern returned must have cairo_pattern_destroy() called on it by the
// caller.
// As the cairo_pattern_t returned may depend on the Pattern passed in, the
// lifetime of the cairo_pattern_t returned must not exceed the lifetime of the
// Pattern passed in.
static cairo_pattern_t*
GfxPatternToCairoPattern(const Pattern& aPattern, Float aAlpha)
{
  cairo_pattern_t* pat;
  const Matrix* matrix = nullptr;

  switch (aPattern.GetType())
  {
    case PATTERN_COLOR:
    {
      Color color = static_cast<const ColorPattern&>(aPattern).mColor;
      pat = cairo_pattern_create_rgba(color.r, color.g, color.b, color.a * aAlpha);
      break;
    }

    case PATTERN_SURFACE:
    {
      const SurfacePattern& pattern = static_cast<const SurfacePattern&>(aPattern);
      cairo_surface_t* surf = GetCairoSurfaceForSourceSurface(pattern.mSurface);

      pat = cairo_pattern_create_for_surface(surf);

      matrix = &pattern.mMatrix;

      cairo_pattern_set_filter(pat, GfxFilterToCairoFilter(pattern.mFilter));
      cairo_pattern_set_extend(pat, GfxExtendToCairoExtend(pattern.mExtendMode));

      cairo_surface_destroy(surf);

      break;
    }
    case PATTERN_LINEAR_GRADIENT:
    {
      const LinearGradientPattern& pattern = static_cast<const LinearGradientPattern&>(aPattern);

      pat = cairo_pattern_create_linear(pattern.mBegin.x, pattern.mBegin.y,
                                        pattern.mEnd.x, pattern.mEnd.y);

      MOZ_ASSERT(pattern.mStops->GetBackendType() == BACKEND_CAIRO);
      GradientStopsCairo* cairoStops = static_cast<GradientStopsCairo*>(pattern.mStops.get());
      cairo_pattern_set_extend(pat, GfxExtendToCairoExtend(cairoStops->GetExtendMode()));

      matrix = &pattern.mMatrix;

      const std::vector<GradientStop>& stops = cairoStops->GetStops();
      for (size_t i = 0; i < stops.size(); ++i) {
        const GradientStop& stop = stops[i];
        cairo_pattern_add_color_stop_rgba(pat, stop.offset, stop.color.r,
                                          stop.color.g, stop.color.b,
                                          stop.color.a);
      }

      break;
    }
    case PATTERN_RADIAL_GRADIENT:
    {
      const RadialGradientPattern& pattern = static_cast<const RadialGradientPattern&>(aPattern);

      pat = cairo_pattern_create_radial(pattern.mCenter1.x, pattern.mCenter1.y, pattern.mRadius1,
                                        pattern.mCenter2.x, pattern.mCenter2.y, pattern.mRadius2);

      MOZ_ASSERT(pattern.mStops->GetBackendType() == BACKEND_CAIRO);
      GradientStopsCairo* cairoStops = static_cast<GradientStopsCairo*>(pattern.mStops.get());
      cairo_pattern_set_extend(pat, GfxExtendToCairoExtend(cairoStops->GetExtendMode()));

      matrix = &pattern.mMatrix;

      const std::vector<GradientStop>& stops = cairoStops->GetStops();
      for (size_t i = 0; i < stops.size(); ++i) {
        const GradientStop& stop = stops[i];
        cairo_pattern_add_color_stop_rgba(pat, stop.offset, stop.color.r,
                                          stop.color.g, stop.color.b,
                                          stop.color.a);
      }

      break;
    }
    default:
    {
      // We should support all pattern types!
      MOZ_ASSERT(false);
    }
  }

  // The pattern matrix is a matrix that transforms the pattern into user
  // space. Cairo takes a matrix that converts from user space to pattern
  // space. Cairo therefore needs the inverse.
  if (matrix) {
    cairo_matrix_t mat;
    GfxMatrixToCairoMatrix(*matrix, mat);
    cairo_matrix_invert(&mat);
    cairo_pattern_set_matrix(pat, &mat);
  }

  return pat;
}

static bool
NeedIntermediateSurface(const Pattern& aPattern, const DrawOptions& aOptions)
{
  // We pre-multiply colours' alpha by the global alpha, so we don't need to
  // use an intermediate surface for them.
  if (aPattern.GetType() == PATTERN_COLOR)
    return false;

  if (aOptions.mAlpha == 1.0)
    return false;

  return true;
}

DrawTargetCairo::DrawTargetCairo()
  : mContext(nullptr)
  , mPathObserver(nullptr)
{
}

DrawTargetCairo::~DrawTargetCairo()
{
  if (mPathObserver) {
    mPathObserver->ForgetDrawTarget();
  }
  cairo_destroy(mContext);
  if (mSurface) {
    cairo_surface_destroy(mSurface);
  }
}

IntSize
DrawTargetCairo::GetSize()
{
  return mSize;
}

TemporaryRef<SourceSurface>
DrawTargetCairo::Snapshot()
{
  if (mSnapshot) {
    return mSnapshot;
  }

  IntSize size = GetSize();

  cairo_content_t content = cairo_surface_get_content(mSurface);
  mSnapshot = new SourceSurfaceCairo(mSurface,
                                     size,
                                     CairoContentToGfxFormat(content),
                                     this);
  return mSnapshot;
}

void
DrawTargetCairo::Flush()
{
  cairo_surface_t* surf = cairo_get_target(mContext);
  cairo_surface_flush(surf);
}

void
DrawTargetCairo::PrepareForDrawing(cairo_t* aContext, const Path* aPath /* = nullptr */)
{
  WillChange(aPath);
}

void
DrawTargetCairo::DrawSurface(SourceSurface *aSurface,
                             const Rect &aDest,
                             const Rect &aSource,
                             const DrawSurfaceOptions &aSurfOptions,
                             const DrawOptions &aOptions)
{
  AutoPrepareForDrawing prep(this, mContext);
  AutoClearDeviceOffset clear(aSurface);

  float sx = aSource.Width() / aDest.Width();
  float sy = aSource.Height() / aDest.Height();

  cairo_matrix_t src_mat;
  cairo_matrix_init_translate(&src_mat, aSource.X(), aSource.Y());
  cairo_matrix_scale(&src_mat, sx, sy);

  cairo_surface_t* surf = GetCairoSurfaceForSourceSurface(aSurface);
  cairo_pattern_t* pat = cairo_pattern_create_for_surface(surf);
  cairo_surface_destroy(surf);

  cairo_pattern_set_matrix(pat, &src_mat);
  cairo_pattern_set_filter(pat, GfxFilterToCairoFilter(aSurfOptions.mFilter));
  cairo_pattern_set_extend(pat, CAIRO_EXTEND_PAD);

  cairo_set_antialias(mContext, GfxAntialiasToCairoAntialias(aOptions.mAntialiasMode));

  double clipX1, clipY1, clipX2, clipY2;
  cairo_clip_extents(mContext, &clipX1, &clipY1, &clipX2, &clipY2);
  Rect clip(clipX1, clipY1, clipX2 - clipX1, clipY2 - clipY1); // Narrowing of doubles to floats
  // If the destination rect covers the entire clipped area, then unbounded and bounded
  // operations are identical, and we don't need to push a group.
  bool needsGroup = !IsOperatorBoundByMask(aOptions.mCompositionOp) &&
                    !aDest.Contains(clip);

  cairo_translate(mContext, aDest.X(), aDest.Y());

  if (needsGroup) {
    cairo_push_group(mContext);
      cairo_new_path(mContext);
      cairo_rectangle(mContext, 0, 0, aDest.Width(), aDest.Height());
      cairo_set_source(mContext, pat);
      cairo_fill(mContext);
    cairo_pop_group_to_source(mContext);
  } else {
    cairo_new_path(mContext);
    cairo_rectangle(mContext, 0, 0, aDest.Width(), aDest.Height());
    cairo_clip(mContext);
    cairo_set_source(mContext, pat);
  }

  cairo_set_operator(mContext, GfxOpToCairoOp(aOptions.mCompositionOp));

  cairo_paint_with_alpha(mContext, aOptions.mAlpha);

  cairo_pattern_destroy(pat);
}

void
DrawTargetCairo::DrawSurfaceWithShadow(SourceSurface *aSurface,
                                       const Point &aDest,
                                       const Color &aColor,
                                       const Point &aOffset,
                                       Float aSigma,
                                       CompositionOp aOperator)
{
  if (aSurface->GetType() != SURFACE_CAIRO) {
    return;
  }
  
  AutoClearDeviceOffset clear(aSurface);

  Float width = Float(aSurface->GetSize().width);
  Float height = Float(aSurface->GetSize().height);

  SourceSurfaceCairo* source = static_cast<SourceSurfaceCairo*>(aSurface);
  cairo_surface_t* sourcesurf = source->GetSurface();
  cairo_surface_t* blursurf;
  cairo_surface_t* surf;

  // We only use the A8 surface for blurred shadows. Unblurred shadows can just
  // use the RGBA surface directly.
  if (cairo_surface_get_type(sourcesurf) == CAIRO_SURFACE_TYPE_TEE) {
    blursurf = cairo_tee_surface_index(sourcesurf, 0);
    surf = cairo_tee_surface_index(sourcesurf, 1);

    MOZ_ASSERT(cairo_surface_get_type(blursurf) == CAIRO_SURFACE_TYPE_IMAGE);
    Rect extents(0, 0, width, height);
    AlphaBoxBlur blur(extents,
                      cairo_image_surface_get_stride(blursurf),
                      aSigma);
    blur.Blur(cairo_image_surface_get_data(blursurf));
  } else {
    blursurf = sourcesurf;
    surf = sourcesurf;
  }

  WillChange();
  ClearSurfaceForUnboundedSource(aOperator);

  cairo_save(mContext);
  cairo_set_operator(mContext, GfxOpToCairoOp(aOperator));
  cairo_identity_matrix(mContext);
  cairo_translate(mContext, aDest.x, aDest.y);

  if (IsOperatorBoundByMask(aOperator)){
    cairo_set_source_rgba(mContext, aColor.r, aColor.g, aColor.b, aColor.a);
    cairo_mask_surface(mContext, blursurf, aOffset.x, aOffset.y);

    // Now that the shadow has been drawn, we can draw the surface on top.
    cairo_set_source_surface(mContext, surf, 0, 0);
    cairo_new_path(mContext);
    cairo_rectangle(mContext, 0, 0, width, height);
    cairo_fill(mContext);
  } else {
    cairo_push_group(mContext);
      cairo_set_source_rgba(mContext, aColor.r, aColor.g, aColor.b, aColor.a);
      cairo_mask_surface(mContext, blursurf, aOffset.x, aOffset.y);

      // Now that the shadow has been drawn, we can draw the surface on top.
      cairo_set_source_surface(mContext, surf, 0, 0);
      cairo_new_path(mContext);
      cairo_rectangle(mContext, 0, 0, width, height);
      cairo_fill(mContext);
    cairo_pop_group_to_source(mContext);
    cairo_paint(mContext);
  }

  cairo_restore(mContext);
}

void
DrawTargetCairo::DrawPattern(const Pattern& aPattern,
                             const StrokeOptions& aStrokeOptions,
                             const DrawOptions& aOptions,
                             DrawPatternType aDrawType)
{
  if (!PatternIsCompatible(aPattern)) {
    return;
  }
  
  AutoClearDeviceOffset clear(aPattern);

  cairo_pattern_t* pat = GfxPatternToCairoPattern(aPattern, aOptions.mAlpha);
  cairo_set_source(mContext, pat);

  cairo_set_antialias(mContext, GfxAntialiasToCairoAntialias(aOptions.mAntialiasMode));

  if (NeedIntermediateSurface(aPattern, aOptions) ||
      !IsOperatorBoundByMask(aOptions.mCompositionOp)) {
    cairo_push_group_with_content(mContext, CAIRO_CONTENT_COLOR_ALPHA);

    ClearSurfaceForUnboundedSource(aOptions.mCompositionOp);

    // Don't want operators to be applied twice
    cairo_set_operator(mContext, CAIRO_OPERATOR_OVER);

    if (aDrawType == DRAW_STROKE) {
      SetCairoStrokeOptions(mContext, aStrokeOptions);
      cairo_stroke_preserve(mContext);
    } else {
      cairo_fill_preserve(mContext);
    }

    cairo_pop_group_to_source(mContext);

    // Now draw the content using the desired operator
    cairo_set_operator(mContext, GfxOpToCairoOp(aOptions.mCompositionOp));
    cairo_paint_with_alpha(mContext, aOptions.mAlpha);
  } else {
    ClearSurfaceForUnboundedSource(aOptions.mCompositionOp);
    cairo_set_operator(mContext, GfxOpToCairoOp(aOptions.mCompositionOp));

    if (aDrawType == DRAW_STROKE) {
      SetCairoStrokeOptions(mContext, aStrokeOptions);
      cairo_stroke_preserve(mContext);
    } else {
      cairo_fill_preserve(mContext);
    }
  }

  cairo_pattern_destroy(pat);
}

void
DrawTargetCairo::FillRect(const Rect &aRect,
                          const Pattern &aPattern,
                          const DrawOptions &aOptions)
{
  AutoPrepareForDrawing prep(this, mContext);

  cairo_new_path(mContext);
  cairo_rectangle(mContext, aRect.x, aRect.y, aRect.Width(), aRect.Height());

  DrawPattern(aPattern, StrokeOptions(), aOptions, DRAW_FILL);
}

void
DrawTargetCairo::CopySurface(SourceSurface *aSurface,
                             const IntRect &aSource,
                             const IntPoint &aDest)
{
  AutoPrepareForDrawing prep(this, mContext);
  AutoClearDeviceOffset clear(aSurface);

  if (!aSurface || aSurface->GetType() != SURFACE_CAIRO) {
    gfxWarning() << "Unsupported surface type specified";
    return;
  }

  cairo_surface_t* surf = static_cast<SourceSurfaceCairo*>(aSurface)->GetSurface();

  cairo_identity_matrix(mContext);

  cairo_set_source_surface(mContext, surf, aDest.x - aSource.x, aDest.y - aSource.y);
  cairo_set_operator(mContext, CAIRO_OPERATOR_SOURCE);
  cairo_set_antialias(mContext, CAIRO_ANTIALIAS_NONE);

  cairo_reset_clip(mContext);
  cairo_new_path(mContext);
  cairo_rectangle(mContext, aDest.x, aDest.y, aSource.width, aSource.height);
  cairo_fill(mContext);
}

void
DrawTargetCairo::ClearRect(const Rect& aRect)
{
  AutoPrepareForDrawing prep(this, mContext);

  cairo_set_antialias(mContext, CAIRO_ANTIALIAS_NONE);
  cairo_new_path(mContext);
  cairo_set_operator(mContext, CAIRO_OPERATOR_CLEAR);
  cairo_rectangle(mContext, aRect.X(), aRect.Y(),
                  aRect.Width(), aRect.Height());
  cairo_fill(mContext);
}

void
DrawTargetCairo::StrokeRect(const Rect &aRect,
                            const Pattern &aPattern,
                            const StrokeOptions &aStrokeOptions /* = StrokeOptions() */,
                            const DrawOptions &aOptions /* = DrawOptions() */)
{
  AutoPrepareForDrawing prep(this, mContext);

  cairo_new_path(mContext);
  cairo_rectangle(mContext, aRect.x, aRect.y, aRect.Width(), aRect.Height());

  DrawPattern(aPattern, aStrokeOptions, aOptions, DRAW_STROKE);
}

void
DrawTargetCairo::StrokeLine(const Point &aStart,
                            const Point &aEnd,
                            const Pattern &aPattern,
                            const StrokeOptions &aStrokeOptions /* = StrokeOptions() */,
                            const DrawOptions &aOptions /* = DrawOptions() */)
{
  AutoPrepareForDrawing prep(this, mContext);

  cairo_new_path(mContext);
  cairo_move_to(mContext, aStart.x, aStart.y);
  cairo_line_to(mContext, aEnd.x, aEnd.y);

  DrawPattern(aPattern, aStrokeOptions, aOptions, DRAW_STROKE);
}

void
DrawTargetCairo::Stroke(const Path *aPath,
                        const Pattern &aPattern,
                        const StrokeOptions &aStrokeOptions /* = StrokeOptions() */,
                        const DrawOptions &aOptions /* = DrawOptions() */)
{
  AutoPrepareForDrawing prep(this, mContext, aPath);

  if (aPath->GetBackendType() != BACKEND_CAIRO)
    return;

  PathCairo* path = const_cast<PathCairo*>(static_cast<const PathCairo*>(aPath));
  path->CopyPathTo(mContext, this);

  DrawPattern(aPattern, aStrokeOptions, aOptions, DRAW_STROKE);
}

void
DrawTargetCairo::Fill(const Path *aPath,
                      const Pattern &aPattern,
                      const DrawOptions &aOptions /* = DrawOptions() */)
{
  AutoPrepareForDrawing prep(this, mContext, aPath);

  if (aPath->GetBackendType() != BACKEND_CAIRO)
    return;

  PathCairo* path = const_cast<PathCairo*>(static_cast<const PathCairo*>(aPath));
  path->CopyPathTo(mContext, this);

  DrawPattern(aPattern, StrokeOptions(), aOptions, DRAW_FILL);
}

void
DrawTargetCairo::FillGlyphs(ScaledFont *aFont,
                            const GlyphBuffer &aBuffer,
                            const Pattern &aPattern,
                            const DrawOptions &aOptions,
                            const GlyphRenderingOptions*)
{
  AutoPrepareForDrawing prep(this, mContext);
  AutoClearDeviceOffset clear(aPattern);

  ScaledFontBase* scaledFont = static_cast<ScaledFontBase*>(aFont);
  cairo_set_scaled_font(mContext, scaledFont->GetCairoScaledFont());

  cairo_pattern_t* pat = GfxPatternToCairoPattern(aPattern, aOptions.mAlpha);
  cairo_set_source(mContext, pat);
  cairo_pattern_destroy(pat);

  cairo_set_antialias(mContext, GfxAntialiasToCairoAntialias(aOptions.mAntialiasMode));

  // Convert our GlyphBuffer into an array of Cairo glyphs.
  std::vector<cairo_glyph_t> glyphs(aBuffer.mNumGlyphs);
  for (uint32_t i = 0; i < aBuffer.mNumGlyphs; ++i) {
    glyphs[i].index = aBuffer.mGlyphs[i].mIndex;
    glyphs[i].x = aBuffer.mGlyphs[i].mPosition.x;
    glyphs[i].y = aBuffer.mGlyphs[i].mPosition.y;
  }

  cairo_show_glyphs(mContext, &glyphs[0], aBuffer.mNumGlyphs);
}

void
DrawTargetCairo::Mask(const Pattern &aSource,
                      const Pattern &aMask,
                      const DrawOptions &aOptions /* = DrawOptions() */)
{
  AutoPrepareForDrawing prep(this, mContext);
  AutoClearDeviceOffset clearSource(aSource);
  AutoClearDeviceOffset clearMask(aMask);

  cairo_set_antialias(mContext, GfxAntialiasToCairoAntialias(aOptions.mAntialiasMode));

  cairo_pattern_t* source = GfxPatternToCairoPattern(aSource, aOptions.mAlpha);
  cairo_set_source(mContext, source);

  cairo_pattern_t* mask = GfxPatternToCairoPattern(aMask, aOptions.mAlpha);
  cairo_mask(mContext, mask);

  cairo_pattern_destroy(mask);
  cairo_pattern_destroy(source);
}

void
DrawTargetCairo::MaskSurface(const Pattern &aSource,
                             SourceSurface *aMask,
                             Point aOffset,
                             const DrawOptions &aOptions)
{
  AutoPrepareForDrawing prep(this, mContext);
  AutoClearDeviceOffset clearSource(aSource);
  AutoClearDeviceOffset clearMask(aMask);

  if (!PatternIsCompatible(aSource)) {
    return;
  }

  cairo_set_antialias(mContext, GfxAntialiasToCairoAntialias(aOptions.mAntialiasMode));

  cairo_pattern_t* pat = GfxPatternToCairoPattern(aSource, aOptions.mAlpha);
  cairo_set_source(mContext, pat);

  if (NeedIntermediateSurface(aSource, aOptions)) {
    cairo_push_group_with_content(mContext, CAIRO_CONTENT_COLOR_ALPHA);

    // Don't want operators to be applied twice
    cairo_set_operator(mContext, CAIRO_OPERATOR_OVER);

    // Now draw the content using the desired operator
    cairo_paint_with_alpha(mContext, aOptions.mAlpha);

    cairo_pop_group_to_source(mContext);
  }

  cairo_surface_t* surf = GetCairoSurfaceForSourceSurface(aMask);
  cairo_pattern_t* mask = cairo_pattern_create_for_surface(surf);
  cairo_matrix_t matrix;

  cairo_matrix_init_translate (&matrix, -aOffset.x, -aOffset.y);
  cairo_pattern_set_matrix (mask, &matrix);

  cairo_mask(mContext, mask);

  cairo_surface_destroy(surf);
  cairo_pattern_destroy(mask);
  cairo_pattern_destroy(pat);
}

void
DrawTargetCairo::PushClip(const Path *aPath)
{
  if (aPath->GetBackendType() != BACKEND_CAIRO) {
    return;
  }

  WillChange(aPath);
  cairo_save(mContext);

  PathCairo* path = const_cast<PathCairo*>(static_cast<const PathCairo*>(aPath));
  path->CopyPathTo(mContext, this);
  cairo_clip_preserve(mContext);
}

void
DrawTargetCairo::PushClipRect(const Rect& aRect)
{
  WillChange();
  cairo_save(mContext);

  cairo_new_path(mContext);
  cairo_rectangle(mContext, aRect.X(), aRect.Y(), aRect.Width(), aRect.Height());
  cairo_clip_preserve(mContext);
}

void
DrawTargetCairo::PopClip()
{
  // save/restore does not affect the path, so no need to call WillChange()
  cairo_restore(mContext);
}

TemporaryRef<PathBuilder>
DrawTargetCairo::CreatePathBuilder(FillRule aFillRule /* = FILL_WINDING */) const
{
  RefPtr<PathBuilderCairo> builder = new PathBuilderCairo(mContext,
                                                          const_cast<DrawTargetCairo*>(this),
                                                          aFillRule);

  return builder;
}

void
DrawTargetCairo::ClearSurfaceForUnboundedSource(const CompositionOp &aOperator)
{
  if (aOperator != OP_SOURCE)
    return;
  cairo_set_operator(mContext, CAIRO_OPERATOR_CLEAR);
  // It doesn't really matter what the source is here, since Paint
  // isn't bounded by the source and the mask covers the entire clip
  // region.
  cairo_paint(mContext);
}


TemporaryRef<GradientStops>
DrawTargetCairo::CreateGradientStops(GradientStop *aStops, uint32_t aNumStops,
                                     ExtendMode aExtendMode) const
{
  RefPtr<GradientStopsCairo> stops = new GradientStopsCairo(aStops, aNumStops,
                                                            aExtendMode);
  return stops;
}

/**
 * Copies pixel data from aData into aSurface; aData must have the dimensions
 * given in aSize, with a stride of aStride bytes and aPixelWidth bytes per pixel
 */
static void
CopyDataToCairoSurface(cairo_surface_t* aSurface,
                       unsigned char *aData,
                       const IntSize &aSize,
                       int32_t aStride,
                       int32_t aPixelWidth)
{
  unsigned char* surfData = cairo_image_surface_get_data(aSurface);
  int surfStride = cairo_image_surface_get_stride(aSurface);
  // In certain scenarios, requesting larger than 8k image fails.  Bug 803568
  // covers the details of how to run into it, but the full detailed
  // investigation hasn't been done to determine the underlying cause.  We
  // will just handle the failure to allocate the surface to avoid a crash.
  if (!surfData) {
    return;
  }
  for (int32_t y = 0; y < aSize.height; ++y) {
    memcpy(surfData + y * surfStride,
           aData + y * aStride,
           aSize.width * aPixelWidth);
  }
  cairo_surface_mark_dirty(aSurface);
}

TemporaryRef<SourceSurface>
DrawTargetCairo::CreateSourceSurfaceFromData(unsigned char *aData,
                                             const IntSize &aSize,
                                             int32_t aStride,
                                             SurfaceFormat aFormat) const
{
  cairo_surface_t* surf = cairo_image_surface_create(GfxFormatToCairoFormat(aFormat),
                                                     aSize.width,
                                                     aSize.height);
  // In certain scenarios, requesting larger than 8k image fails.  Bug 803568
  // covers the details of how to run into it, but the full detailed
  // investigation hasn't been done to determine the underlying cause.  We
  // will just handle the failure to allocate the surface to avoid a crash.
  if (cairo_surface_status(surf)) {
    return nullptr;
  }

  CopyDataToCairoSurface(surf, aData, aSize, aStride, BytesPerPixel(aFormat));

  RefPtr<SourceSurfaceCairo> source_surf = new SourceSurfaceCairo(surf, aSize, aFormat);
  cairo_surface_destroy(surf);

  return source_surf;
}

TemporaryRef<SourceSurface>
DrawTargetCairo::OptimizeSourceSurface(SourceSurface *aSurface) const
{
  return aSurface;
}

TemporaryRef<SourceSurface>
DrawTargetCairo::CreateSourceSurfaceFromNativeSurface(const NativeSurface &aSurface) const
{
  if (aSurface.mType == NATIVE_SURFACE_CAIRO_SURFACE) {
    IntSize size;
    cairo_surface_t* surf = static_cast<cairo_surface_t*>(aSurface.mSurface);
    if (GetCairoSurfaceSize(surf, size)) {
      RefPtr<SourceSurfaceCairo> source =
        new SourceSurfaceCairo(surf, size, aSurface.mFormat);
      return source;
    }
  }

  return nullptr;
}

TemporaryRef<DrawTarget>
DrawTargetCairo::CreateSimilarDrawTarget(const IntSize &aSize, SurfaceFormat aFormat) const
{
  cairo_surface_t* similar = cairo_surface_create_similar(cairo_get_target(mContext),
                                                          GfxFormatToCairoContent(aFormat),
                                                          aSize.width, aSize.height);

  if (!cairo_surface_status(similar)) {
    RefPtr<DrawTargetCairo> target = new DrawTargetCairo();
    target->InitAlreadyReferenced(similar, aSize);
    return target;
  }

  return nullptr;
}

bool
DrawTargetCairo::InitAlreadyReferenced(cairo_surface_t* aSurface, const IntSize& aSize)
{
  mContext = cairo_create(aSurface);
  mSurface = aSurface;
  mSize = aSize;
  mFormat = CairoContentToGfxFormat(cairo_surface_get_content(aSurface));

  return true;
}

TemporaryRef<DrawTarget>
DrawTargetCairo::CreateShadowDrawTarget(const IntSize &aSize, SurfaceFormat aFormat,
                                        float aSigma) const
{
  cairo_surface_t* similar = cairo_surface_create_similar(cairo_get_target(mContext),
                                                          GfxFormatToCairoContent(aFormat),
                                                          aSize.width, aSize.height);

  if (cairo_surface_status(similar)) {
    return nullptr;
  }

  // If we don't have a blur then we can use the RGBA mask and keep all the
  // operations in graphics memory.
  if (aSigma == 0.0F) {
    RefPtr<DrawTargetCairo> target = new DrawTargetCairo();
    target->InitAlreadyReferenced(similar, aSize);
    return target;
  }

  cairo_surface_t* blursurf = cairo_image_surface_create(CAIRO_FORMAT_A8,
                                                         aSize.width,
                                                         aSize.height);

  if (cairo_surface_status(blursurf)) {
    return nullptr;
  }

  cairo_surface_t* tee = cairo_tee_surface_create(blursurf);
  cairo_surface_destroy(blursurf);
  if (cairo_surface_status(tee)) {
    cairo_surface_destroy(similar);
    return nullptr;
  }

  cairo_tee_surface_add(tee, similar);
  cairo_surface_destroy(similar);

  RefPtr<DrawTargetCairo> target = new DrawTargetCairo();
  target->InitAlreadyReferenced(tee, aSize);
  return target;
}

bool
DrawTargetCairo::Init(cairo_surface_t* aSurface, const IntSize& aSize)
{
  cairo_surface_reference(aSurface);
  return InitAlreadyReferenced(aSurface, aSize);
}

void *
DrawTargetCairo::GetNativeSurface(NativeSurfaceType aType)
{
  if (aType == NATIVE_SURFACE_CAIRO_SURFACE) {
    return cairo_get_target(mContext);
  }
  if (aType == NATIVE_SURFACE_CAIRO_CONTEXT) {
    return mContext;
  }

  return nullptr;
}

void
DrawTargetCairo::MarkSnapshotIndependent()
{
  if (mSnapshot) {
    if (mSnapshot->refCount() > 1) {
      // We only need to worry about snapshots that someone else knows about
      mSnapshot->DrawTargetWillChange();
    }
    mSnapshot = nullptr;
  }
}

void
DrawTargetCairo::WillChange(const Path* aPath /* = nullptr */)
{
  MarkSnapshotIndependent();

  if (mPathObserver &&
      (!aPath || !mPathObserver->ContainsPath(aPath))) {
    mPathObserver->PathWillChange();
    mPathObserver = nullptr;
  }
}

void
DrawTargetCairo::SetPathObserver(CairoPathContext* aPathObserver)
{
  if (mPathObserver && mPathObserver != aPathObserver) {
    mPathObserver->PathWillChange();
  }
  mPathObserver = aPathObserver;
}

void
DrawTargetCairo::SetTransform(const Matrix& aTransform)
{
  mTransform = aTransform;

  cairo_matrix_t mat;
  GfxMatrixToCairoMatrix(mTransform, mat);
  cairo_set_matrix(mContext, &mat);
}

cairo_t*
BorrowedCairoContext::BorrowCairoContextFromDrawTarget(DrawTarget* aDT)
{
  if (aDT->GetType() != BACKEND_CAIRO || aDT->IsDualDrawTarget()) {
    return nullptr;
  }
  DrawTargetCairo* cairoDT = static_cast<DrawTargetCairo*>(aDT);

  cairoDT->WillChange();

  // save the state to make it easier for callers to avoid mucking with things
  cairo_save(cairoDT->mContext);

  // Neuter the DrawTarget while the context is being borrowed
  cairo_t* cairo = cairoDT->mContext;
  cairoDT->mContext = nullptr;

  return cairo;
}

void
BorrowedCairoContext::ReturnCairoContextToDrawTarget(DrawTarget* aDT,
                                                     cairo_t* aCairo)
{
  if (aDT->GetType() != BACKEND_CAIRO || aDT->IsDualDrawTarget()) {
    return;
  }
  DrawTargetCairo* cairoDT = static_cast<DrawTargetCairo*>(aDT);

  cairo_restore(aCairo);
  cairoDT->mContext = aCairo;
}

}
}
