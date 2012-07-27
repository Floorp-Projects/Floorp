/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DrawTargetCairo.h"

#include "SourceSurfaceCairo.h"
#include "PathCairo.h"
#include "HelpersCairo.h"
#include "ScaledFontBase.h"

#include "cairo.h"

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
  }

  AutoPrepareForDrawing(DrawTargetCairo* dt, cairo_t* ctx, const Path* path)
    : mCtx(ctx)
  {
    dt->PrepareForDrawing(ctx, path);
    cairo_save(mCtx);
  }

  ~AutoPrepareForDrawing() { cairo_restore(mCtx); }

private:
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
      size.height = CGBitmapContextGetWidth(cgc);
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
GetCairoSurfaceForSourceSurface(SourceSurface *aSurface)
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

  RefPtr<DataSourceSurface> data = aSurface->GetDataSurface();
  cairo_surface_t* surf =
    cairo_image_surface_create_for_data(data->GetData(),
                                        GfxFormatToCairoFormat(data->GetFormat()),
                                        data->GetSize().width,
                                        data->GetSize().height,
                                        data->Stride());
  cairo_surface_set_user_data(surf,
 				                      &surfaceDataKey,
 				                      data.forget().drop(),
 				                      ReleaseData);
  return surf;
}

// Never returns NULL. As such, you must always pass in Cairo-compatible
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
      const std::vector<GradientStop>& stops =
        static_cast<GradientStopsCairo*>(pattern.mStops.get())->GetStops();
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

      const std::vector<GradientStop>& stops =
        static_cast<GradientStopsCairo*>(pattern.mStops.get())->GetStops();
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

  return pat;
}

/**
 * Returns true iff the the given operator should affect areas of the
 * destination where the source is transparent. Among other things, this
 * implies that a fully transparent source would still affect the canvas.
 */
static bool
OperatorAffectsUncoveredAreas(CompositionOp op)
{
  return op == OP_IN ||
         op == OP_OUT ||
         op == OP_DEST_IN ||
         op == OP_DEST_ATOP ||
         op == OP_DEST_OUT;
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
  : mContext(NULL)
{
}

DrawTargetCairo::~DrawTargetCairo()
{
  MarkSnapshotsIndependent();
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
  IntSize size = GetSize();

  cairo_content_t content = cairo_surface_get_content(mSurface);
  RefPtr<SourceSurfaceCairo> surf = new SourceSurfaceCairo(mSurface, size,
                                                           CairoContentToGfxFormat(content),
                                                           this);
  AppendSnapshot(surf);
  return surf;
}

void
DrawTargetCairo::Flush()
{
  cairo_surface_t* surf = cairo_get_target(mContext);
  cairo_surface_flush(surf);
}

void
DrawTargetCairo::PrepareForDrawing(cairo_t* aContext, const Path* aPath /* = NULL */)
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

  cairo_save(mContext);
  cairo_translate(mContext, aDest.X(), aDest.Y());

  if (OperatorAffectsUncoveredAreas(aOptions.mCompositionOp) ||
      aOptions.mCompositionOp == OP_SOURCE) {
    cairo_push_group(mContext);
      cairo_new_path(mContext);
      cairo_rectangle(mContext, 0, 0, aDest.Width(), aDest.Height());
      //TODO[nrc] remove comments if test ok
      //cairo_clip(mContext);
      cairo_set_source(mContext, pat);
      //cairo_paint(mContext);
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

  cairo_restore(mContext);

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

  WillChange();

  Float width = aSurface->GetSize().width;
  Float height = aSurface->GetSize().height;
  Rect extents(0, 0, width, height);

  AlphaBoxBlur blur(extents, IntSize(0, 0),
                    AlphaBoxBlur::CalculateBlurRadius(Point(aSigma, aSigma)),
                    NULL, NULL);
  if (!blur.GetData()) {
    return;
  }

  IntSize blursize = blur.GetSize();
  cairo_surface_t* blursurf = cairo_image_surface_create_for_data(blur.GetData(),
                                                                  CAIRO_FORMAT_A8,
                                                                  blursize.width,
                                                                  blursize.height,
                                                                  blur.GetStride());

  ClearSurfaceForUnboundedSource(aOperator);
  
  // Draw the source surface into the surface we're going to blur.
  SourceSurfaceCairo* source = static_cast<SourceSurfaceCairo*>(aSurface);
  cairo_surface_t* surf = source->GetSurface();
  cairo_pattern_t* pat = cairo_pattern_create_for_surface(surf);
  cairo_pattern_set_extend(pat, CAIRO_EXTEND_PAD);

  cairo_t* ctx = cairo_create(blursurf);

  cairo_set_source(ctx, pat);

  IntRect blurrect = blur.GetRect();
  cairo_new_path(ctx);
  cairo_rectangle(ctx, blurrect.x, blurrect.y, blurrect.width, blurrect.height);
  cairo_clip(ctx);
  cairo_paint(ctx);

  cairo_destroy(ctx);

  // Blur the result, then use that blurred result as a mask to draw the shadow
  // colour to the surface.
  blur.Blur();
  cairo_save(mContext);
  cairo_set_operator(mContext, GfxOpToCairoOp(aOperator));
  cairo_identity_matrix(mContext);
  cairo_translate(mContext, aDest.x, aDest.y);

  if (OperatorAffectsUncoveredAreas(aOperator) ||
      aOperator == OP_SOURCE){
    cairo_push_group(mContext);
      cairo_set_source_rgba(mContext, aColor.r, aColor.g, aColor.b, aColor.a);
      cairo_mask_surface(mContext, blursurf, aOffset.x, aOffset.y);
    cairo_pop_group_to_source(mContext);
    cairo_paint(mContext);

    // Now that the shadow has been drawn, we can draw the surface on top.
    cairo_push_group(mContext);
      cairo_new_path(mContext);
      cairo_rectangle(mContext, 0, 0, width, height);
      //TODO[nrc] remove comments if test ok
      //cairo_clip(mContext);
      cairo_set_source(mContext, pat);
      //cairo_paint(mContext);
      cairo_fill(mContext);
    cairo_pop_group_to_source(mContext);
  } else {
    cairo_set_source_rgba(mContext, aColor.r, aColor.g, aColor.b, aColor.a);
    cairo_mask_surface(mContext, blursurf, aOffset.x, aOffset.y);

    // Now that the shadow has been drawn, we can draw the surface on top.
    cairo_set_source(mContext, pat);
    cairo_new_path(mContext);
    cairo_rectangle(mContext, 0, 0, width, height);
    cairo_clip(mContext);
  }

  cairo_paint(mContext);

  cairo_restore(mContext);

  cairo_pattern_destroy(pat);
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

  cairo_pattern_t* pat = GfxPatternToCairoPattern(aPattern, aOptions.mAlpha);
  cairo_set_source(mContext, pat);

  if (NeedIntermediateSurface(aPattern, aOptions) ||
      OperatorAffectsUncoveredAreas(aOptions.mCompositionOp)) {
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

  if (!aSurface || aSurface->GetType() != SURFACE_CAIRO) {
    gfxWarning() << "Unsupported surface type specified";
    return;
  }

  cairo_surface_t* surf = static_cast<SourceSurfaceCairo*>(aSurface)->GetSurface();

  cairo_save(mContext);

  cairo_identity_matrix(mContext);

  cairo_set_source_surface(mContext, surf, aDest.x - aSource.x, aDest.y - aSource.y);
  cairo_set_operator(mContext, CAIRO_OPERATOR_SOURCE);

  cairo_reset_clip(mContext);
  cairo_new_path(mContext);
  cairo_rectangle(mContext, aDest.x, aDest.y, aSource.width, aSource.height);
  cairo_fill(mContext);

  cairo_restore(mContext);
}

void
DrawTargetCairo::ClearRect(const Rect& aRect)
{
  AutoPrepareForDrawing prep(this, mContext);

  cairo_save(mContext);

  cairo_new_path(mContext);
  cairo_set_operator(mContext, CAIRO_OPERATOR_CLEAR);
  cairo_rectangle(mContext, aRect.X(), aRect.Y(),
                  aRect.Width(), aRect.Height());
  cairo_fill(mContext);

  cairo_restore(mContext);
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

  ScaledFontBase* scaledFont = static_cast<ScaledFontBase*>(aFont);
  cairo_set_scaled_font(mContext, scaledFont->GetCairoScaledFont());

  cairo_pattern_t* pat = GfxPatternToCairoPattern(aPattern, aOptions.mAlpha);
  cairo_set_source(mContext, pat);
  cairo_pattern_destroy(pat);

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
  // TODO
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

  // Creating a PathBuilder implicitly resets our mPathObserver, as it calls
  // SetPathObserver() on us. Since this guarantees our old path is saved off,
  // it's safe to reset the path here.
  cairo_new_path(mContext);

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
DrawTargetCairo::CreateGradientStops(GradientStop *aStops, uint32_t aNumStops, ExtendMode aExtendMode) const
{
  RefPtr<GradientStopsCairo> stops = new GradientStopsCairo(aStops, aNumStops);
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
  for (int32_t y = 0; y < aSize.height; ++y) {
    memcpy(surfData + y * aSize.width * aPixelWidth,
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

  return NULL;
}

TemporaryRef<DrawTarget>
DrawTargetCairo::CreateSimilarDrawTarget(const IntSize &aSize, SurfaceFormat aFormat) const
{
  cairo_surface_t* similar = cairo_surface_create_similar(cairo_get_target(mContext),
                                                          GfxFormatToCairoContent(aFormat),
                                                          aSize.width, aSize.height);

  if (!cairo_surface_status(similar)) {
    RefPtr<DrawTargetCairo> target = new DrawTargetCairo();
    target->Init(similar, aSize);
    return target;
  }

  return NULL;
}

bool
DrawTargetCairo::Init(cairo_surface_t* aSurface, const IntSize& aSize)
{
  mContext = cairo_create(aSurface);
  mSurface = aSurface;
  cairo_surface_reference(mSurface);
  mSize = aSize;
  mFormat = CairoContentToGfxFormat(cairo_surface_get_content(aSurface));

  return true;
}

void *
DrawTargetCairo::GetNativeSurface(NativeSurfaceType aType)
{
  if (aType == NATIVE_SURFACE_CAIRO_SURFACE) {
    return cairo_get_target(mContext);
  }

  return NULL;
}

void
DrawTargetCairo::MarkSnapshotsIndependent()
{
  // Make a copy of the vector, since MarkIndependent implicitly modifies mSnapshots.
  std::vector<SourceSurfaceCairo*> snapshots = mSnapshots;
  for (std::vector<SourceSurfaceCairo*>::iterator iter = snapshots.begin();
       iter != snapshots.end();
       ++iter) {
    (*iter)->MarkIndependent();
  }
}

void
DrawTargetCairo::AppendSnapshot(SourceSurfaceCairo* aSnapshot)
{
  mSnapshots.push_back(aSnapshot);
}

void
DrawTargetCairo::RemoveSnapshot(SourceSurfaceCairo* aSnapshot)
{
  std::vector<SourceSurfaceCairo*>::iterator iter = std::find(mSnapshots.begin(),
                                                              mSnapshots.end(),
                                                              aSnapshot);
  if (iter != mSnapshots.end()) {
    mSnapshots.erase(iter);
  }
}

void
DrawTargetCairo::WillChange(const Path* aPath /* = NULL */)
{
  if (!mSnapshots.empty()) {
    for (std::vector<SourceSurfaceCairo*>::iterator iter = mSnapshots.begin();
         iter != mSnapshots.end(); ++iter) {
      (*iter)->DrawTargetWillChange();
    }
    // All snapshots will now have copied data.
    mSnapshots.clear();
  }

  if (mPathObserver &&
      (!aPath || !mPathObserver->ContainsPath(aPath))) {
    mPathObserver->PathWillChange();
    mPathObserver = NULL;
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
  // We're about to logically change our transformation. Our current path will
  // need to change, because Cairo stores paths in device space.
  if (mPathObserver) {
    mPathObserver->MatrixWillChange(aTransform);
  }

  mTransform = aTransform;

  cairo_matrix_t mat;
  GfxMatrixToCairoMatrix(mTransform, mat);
  cairo_set_matrix(mContext, &mat);
}

}
}
