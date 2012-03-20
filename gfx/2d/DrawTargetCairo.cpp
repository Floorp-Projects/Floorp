/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "DrawTargetCairo.h"

#include "SourceSurfaceCairo.h"
#include "PathCairo.h"
#include "HelpersCairo.h"
#include "ScaledFontBase.h"

#include "cairo.h"

#include "Blur.h"

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
      cairo_surface_t* surf;

      // After this block, |surf| always has an extra cairo reference to be
      // destroyed. This makes creating new surfaces or reusing old ones more
      // uniform.
      if (pattern.mSurface->GetType() == SURFACE_CAIRO) {
        const SourceSurfaceCairo* source = static_cast<const SourceSurfaceCairo*>(pattern.mSurface.get());
        surf = source->GetSurface();
        cairo_surface_reference(surf);
      } else if (pattern.mSurface->GetType() == SURFACE_CAIRO_IMAGE) {
        const DataSourceSurfaceCairo* source =
          static_cast<const DataSourceSurfaceCairo*>(pattern.mSurface.get());
        surf = source->GetSurface();
        cairo_surface_reference(surf);
      } else {
        RefPtr<DataSourceSurface> source = pattern.mSurface->GetDataSurface();
        surf = cairo_image_surface_create_for_data(source->GetData(),
                                                   GfxFormatToCairoFormat(source->GetFormat()),
                                                   source->GetSize().width,
                                                   source->GetSize().height,
                                                   source->Stride());
      }

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
}

IntSize
DrawTargetCairo::GetSize()
{
  return IntSize();
}

TemporaryRef<SourceSurface>
DrawTargetCairo::Snapshot()
{
  cairo_surface_t* csurf = cairo_get_target(mContext);
  IntSize size;
  if (GetCairoSurfaceSize(csurf, size)) {
    cairo_content_t content = cairo_surface_get_content(csurf);
    RefPtr<SourceSurfaceCairo> surf = new SourceSurfaceCairo(csurf, size,
                                                             CairoContentToGfxFormat(content),
                                                             this);
    AppendSnapshot(surf);
    return surf;
  }

  return NULL;
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
  cairo_matrix_init_scale(&src_mat, sx, sy);
  cairo_matrix_translate(&src_mat, aSource.X(), aSource.Y());

  cairo_surface_t* surf = NULL;
  if (aSurface->GetType() == SURFACE_CAIRO) {
    surf = static_cast<SourceSurfaceCairo*>(aSurface)->GetSurface();
  }

  cairo_pattern_t* pat = cairo_pattern_create_for_surface(surf);
  cairo_pattern_set_matrix(pat, &src_mat);
  cairo_pattern_set_filter(pat, GfxFilterToCairoFilter(aSurfOptions.mFilter));

  cairo_save(mContext);

  cairo_set_operator(mContext, GfxOpToCairoOp(aOptions.mCompositionOp));

  cairo_translate(mContext, aDest.X(), aDest.Y());

  cairo_set_source(mContext, pat);

  cairo_new_path(mContext);
  cairo_rectangle(mContext, 0, 0, aDest.Width(), aDest.Height());
  cairo_clip(mContext);
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
  WillChange();

  if (aSurface->GetType() != SURFACE_CAIRO) {
    return;
  }

  SourceSurfaceCairo* source = static_cast<SourceSurfaceCairo*>(aSurface);

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

  // Draw the source surface into the surface we're going to blur.
  cairo_surface_t* surf = source->GetSurface();
  cairo_pattern_t* pat = cairo_pattern_create_for_surface(surf);

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

  cairo_set_operator(mContext, CAIRO_OPERATOR_OVER);
  cairo_set_source_rgba(mContext, aColor.r, aColor.g, aColor.b, aColor.a);

  cairo_identity_matrix(mContext);
  cairo_translate(mContext, aDest.x, aDest.y);

  cairo_mask_surface(mContext, blursurf, aOffset.x, aOffset.y);

  // Now that the shadow has been drawn, we can draw the surface on top.

  cairo_set_operator(mContext, GfxOpToCairoOp(aOperator));

  cairo_set_source(mContext, pat);

  cairo_new_path(mContext);
  cairo_rectangle(mContext, 0, 0, width, height);
  cairo_clip(mContext);

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

  if (NeedIntermediateSurface(aPattern, aOptions)) {
    cairo_push_group_with_content(mContext, CAIRO_CONTENT_COLOR_ALPHA);

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
                             const IntRect &aSourceRect,
                             const IntPoint &aDestination)
{
  AutoPrepareForDrawing prep(this, mContext);
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
}

void
DrawTargetCairo::PushClip(const Path *aPath)
{
}

void
DrawTargetCairo::PushClipRect(const Rect& aRect)
{
}

void
DrawTargetCairo::PopClip()
{
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

TemporaryRef<GradientStops>
DrawTargetCairo::CreateGradientStops(GradientStop *aStops, uint32_t aNumStops, ExtendMode aExtendMode) const
{
  RefPtr<GradientStopsCairo> stops = new GradientStopsCairo(aStops, aNumStops);
  return stops;
}

TemporaryRef<SourceSurface>
DrawTargetCairo::CreateSourceSurfaceFromData(unsigned char *aData,
                                             const IntSize &aSize,
                                             int32_t aStride,
                                             SurfaceFormat aFormat) const
{
  cairo_surface_t* surf = cairo_image_surface_create_for_data(aData,
                                                              GfxFormatToCairoFormat(aFormat),
                                                              aSize.width,
                                                              aSize.height,
                                                              aStride);
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
    target->Init(similar);
    return target;
  }

  return NULL;
}

bool
DrawTargetCairo::Init(cairo_surface_t* aSurface)
{
  mContext = cairo_create(aSurface);

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

  if (aPath && mPathObserver && !mPathObserver->ContainsPath(aPath)) {
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
