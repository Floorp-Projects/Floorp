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
#include "FilterNodeSoftware.h"
#include "mozilla/Scoped.h"

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
#include "cairo-xlib-xrender.h"
#endif

#ifdef CAIRO_HAS_WIN32_SURFACE
#include "cairo-win32.h"
#endif

#include <algorithm>

namespace mozilla {

MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedCairoSurface, cairo_surface_t, cairo_surface_destroy);

namespace gfx {

cairo_surface_t *DrawTargetCairo::mDummySurface;

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

  ~AutoPrepareForDrawing()
  {
    cairo_restore(mCtx);
    cairo_status_t status = cairo_status(mCtx);
    if (status) {
      gfxWarning() << "DrawTargetCairo context in error state: " << cairo_status_to_string(status) << "(" << status << ")";
    }
  }

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
SupportsSelfCopy(cairo_surface_t* surface)
{
  switch (cairo_surface_get_type(surface))
  {
#ifdef CAIRO_HAS_QUARTZ_SURFACE
    case CAIRO_SURFACE_TYPE_QUARTZ:
      return true;
#endif
#ifdef CAIRO_HAS_WIN32_SURFACE
    case CAIRO_SURFACE_TYPE_WIN32:
    case CAIRO_SURFACE_TYPE_WIN32_PRINTING:
      return true;
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
    case PatternType::LINEAR_GRADIENT:
    {
      const LinearGradientPattern& pattern = static_cast<const LinearGradientPattern&>(aPattern);
      return pattern.mStops->GetBackendType() == BackendType::CAIRO;
    }
    case PatternType::RADIAL_GRADIENT:
    {
      const RadialGradientPattern& pattern = static_cast<const RadialGradientPattern&>(aPattern);
      return pattern.mStops->GetBackendType() == BackendType::CAIRO;
    }
    default:
      return true;
  }
}

static cairo_user_data_key_t surfaceDataKey;

void
ReleaseData(void* aData)
{
  DataSourceSurface *data = static_cast<DataSourceSurface*>(aData);
  data->Unmap();
  data->Release();
}

cairo_surface_t*
CopyToImageSurface(unsigned char *aData,
                   const IntRect &aRect,
                   int32_t aStride,
                   SurfaceFormat aFormat)
{
  cairo_surface_t* surf = cairo_image_surface_create(GfxFormatToCairoFormat(aFormat),
                                                     aRect.width,
                                                     aRect.height);
  // In certain scenarios, requesting larger than 8k image fails.  Bug 803568
  // covers the details of how to run into it, but the full detailed
  // investigation hasn't been done to determine the underlying cause.  We
  // will just handle the failure to allocate the surface to avoid a crash.
  if (cairo_surface_status(surf)) {
    return nullptr;
  }

  unsigned char* surfData = cairo_image_surface_get_data(surf);
  int surfStride = cairo_image_surface_get_stride(surf);
  int32_t pixelWidth = BytesPerPixel(aFormat);

  unsigned char* source = aData +
                          aRect.y * aStride +
                          aRect.x * pixelWidth;

  MOZ_ASSERT(aStride >= aRect.width * pixelWidth);
  for (int32_t y = 0; y < aRect.height; ++y) {
    memcpy(surfData + y * surfStride,
           source + y * aStride,
           aRect.width * pixelWidth);
  }
  cairo_surface_mark_dirty(surf);
  return surf;
}

/**
 * If aSurface can be represented as a surface of type
 * CAIRO_SURFACE_TYPE_IMAGE then returns that surface. Does
 * not add a reference.
 */
cairo_surface_t* GetAsImageSurface(cairo_surface_t* aSurface)
{
  if (cairo_surface_get_type(aSurface) == CAIRO_SURFACE_TYPE_IMAGE) {
    return aSurface;
#ifdef CAIRO_HAS_WIN32_SURFACE
  } else if (cairo_surface_get_type(aSurface) == CAIRO_SURFACE_TYPE_WIN32) {
    return cairo_win32_surface_get_image(aSurface);
#endif
  }

  return nullptr;
}

cairo_surface_t* CreateSubImageForData(unsigned char* aData,
                                       const IntRect& aRect,
                                       int aStride,
                                       SurfaceFormat aFormat)
{
  unsigned char *data = aData +
                        aRect.y * aStride +
                        aRect.x * BytesPerPixel(aFormat);

  cairo_surface_t *image =
    cairo_image_surface_create_for_data(data,
                                        GfxFormatToCairoFormat(aFormat),
                                        aRect.width,
                                        aRect.height,
                                        aStride);
  cairo_surface_set_device_offset(image, -aRect.x, -aRect.y);
  return image;
}

/**
 * Returns a referenced cairo_surface_t representing the
 * sub-image specified by aSubImage.
 */
cairo_surface_t* ExtractSubImage(cairo_surface_t* aSurface,
                                 const IntRect& aSubImage,
                                 SurfaceFormat aFormat)
{
  // No need to worry about retaining a reference to the original
  // surface since the only caller of this function guarantees
  // that aSurface will stay alive as long as the result

  cairo_surface_t* image = GetAsImageSurface(aSurface);
  if (image) {
    image = CreateSubImageForData(cairo_image_surface_get_data(image),
                                  aSubImage,
                                  cairo_image_surface_get_stride(image),
                                  aFormat);
    return image;
  }

  cairo_surface_t* similar =
    cairo_surface_create_similar(aSurface,
                                 cairo_surface_get_content(aSurface),
                                 aSubImage.width, aSubImage.height);

  cairo_t* ctx = cairo_create(similar);
  cairo_set_operator(ctx, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_surface(ctx, aSurface, -aSubImage.x, -aSubImage.y);
  cairo_paint(ctx);
  cairo_destroy(ctx);

  cairo_surface_set_device_offset(similar, -aSubImage.x, -aSubImage.y);
  return similar;
}

/**
 * Returns cairo surface for the given SourceSurface.
 * If possible, it will use the cairo_surface associated with aSurface,
 * otherwise, it will create a new cairo_surface.
 * In either case, the caller must call cairo_surface_destroy on the
 * result when it is done with it.
 */
cairo_surface_t*
GetCairoSurfaceForSourceSurface(SourceSurface *aSurface,
                                bool aExistingOnly = false,
                                const IntRect& aSubImage = IntRect())
{
  IntRect subimage = IntRect(IntPoint(), aSurface->GetSize());
  if (!aSubImage.IsEmpty()) {
    MOZ_ASSERT(!aExistingOnly);
    MOZ_ASSERT(subimage.Contains(aSubImage));
    subimage = aSubImage;
  }

  if (aSurface->GetType() == SurfaceType::CAIRO) {
    cairo_surface_t* surf = static_cast<SourceSurfaceCairo*>(aSurface)->GetSurface();
    if (aSubImage.IsEmpty()) {
      cairo_surface_reference(surf);
    } else {
      surf = ExtractSubImage(surf, subimage, aSurface->GetFormat());
    }
    return surf;
  }

  if (aSurface->GetType() == SurfaceType::CAIRO_IMAGE) {
    cairo_surface_t* surf =
      static_cast<const DataSourceSurfaceCairo*>(aSurface)->GetSurface();
    if (aSubImage.IsEmpty()) {
      cairo_surface_reference(surf);
    } else {
      surf = ExtractSubImage(surf, subimage, aSurface->GetFormat());
    }
    return surf;
  }

  if (aExistingOnly) {
    return nullptr;
  }

  RefPtr<DataSourceSurface> data = aSurface->GetDataSurface();
  if (!data) {
    return nullptr;
  }

  DataSourceSurface::MappedSurface map;
  if (!data->Map(DataSourceSurface::READ, &map)) {
    return nullptr;
  }

  cairo_surface_t* surf =
    CreateSubImageForData(map.mData, subimage,
                          map.mStride, data->GetFormat());

  // In certain scenarios, requesting larger than 8k image fails.  Bug 803568
  // covers the details of how to run into it, but the full detailed
  // investigation hasn't been done to determine the underlying cause.  We
  // will just handle the failure to allocate the surface to avoid a crash.
  if (cairo_surface_status(surf)) {
    if (cairo_surface_status(surf) == CAIRO_STATUS_INVALID_STRIDE) {
      // If we failed because of an invalid stride then copy into
      // a new surface with a stride that cairo chooses. No need to
      // set user data since we're not dependent on the original
      // data.
      cairo_surface_t* result =
        CopyToImageSurface(map.mData,
                           subimage,
                           map.mStride,
                           data->GetFormat());
      data->Unmap();
      return result;
    }
    data->Unmap();
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
  explicit AutoClearDeviceOffset(SourceSurface* aSurface)
    : mSurface(nullptr)
  {
    Init(aSurface);
  }

  explicit AutoClearDeviceOffset(const Pattern& aPattern)
    : mSurface(nullptr)
  {
    if (aPattern.GetType() == PatternType::SURFACE) {
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
    case PatternType::COLOR:
    {
      Color color = static_cast<const ColorPattern&>(aPattern).mColor;
      pat = cairo_pattern_create_rgba(color.r, color.g, color.b, color.a * aAlpha);
      break;
    }

    case PatternType::SURFACE:
    {
      const SurfacePattern& pattern = static_cast<const SurfacePattern&>(aPattern);
      cairo_surface_t* surf = GetCairoSurfaceForSourceSurface(pattern.mSurface,
                                                              false,
                                                              pattern.mSamplingRect);
      if (!surf)
        return nullptr;

      pat = cairo_pattern_create_for_surface(surf);

      matrix = &pattern.mMatrix;

      cairo_pattern_set_filter(pat, GfxFilterToCairoFilter(pattern.mFilter));
      cairo_pattern_set_extend(pat, GfxExtendToCairoExtend(pattern.mExtendMode));

      cairo_surface_destroy(surf);
      break;
    }
    case PatternType::LINEAR_GRADIENT:
    {
      const LinearGradientPattern& pattern = static_cast<const LinearGradientPattern&>(aPattern);

      pat = cairo_pattern_create_linear(pattern.mBegin.x, pattern.mBegin.y,
                                        pattern.mEnd.x, pattern.mEnd.y);

      MOZ_ASSERT(pattern.mStops->GetBackendType() == BackendType::CAIRO);
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
    case PatternType::RADIAL_GRADIENT:
    {
      const RadialGradientPattern& pattern = static_cast<const RadialGradientPattern&>(aPattern);

      pat = cairo_pattern_create_radial(pattern.mCenter1.x, pattern.mCenter1.y, pattern.mRadius1,
                                        pattern.mCenter2.x, pattern.mCenter2.y, pattern.mRadius2);

      MOZ_ASSERT(pattern.mStops->GetBackendType() == BackendType::CAIRO);
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
  if (aPattern.GetType() == PatternType::COLOR)
    return false;

  if (aOptions.mAlpha == 1.0)
    return false;

  return true;
}

DrawTargetCairo::DrawTargetCairo()
  : mContext(nullptr)
  , mLockedBits(nullptr)
{
}

DrawTargetCairo::~DrawTargetCairo()
{
  cairo_destroy(mContext);
  if (mSurface) {
    cairo_surface_destroy(mSurface);
  }
  MOZ_ASSERT(!mLockedBits);
}

DrawTargetType
DrawTargetCairo::GetType() const
{
  if (mContext) {
    cairo_surface_type_t type = cairo_surface_get_type(mSurface);
    if (type == CAIRO_SURFACE_TYPE_TEE) {
      type = cairo_surface_get_type(cairo_tee_surface_index(mSurface, 0));
      MOZ_ASSERT(type != CAIRO_SURFACE_TYPE_TEE, "C'mon!");
      MOZ_ASSERT(type == cairo_surface_get_type(cairo_tee_surface_index(mSurface, 1)),
                 "What should we do here?");
    }
    switch (type) {
    case CAIRO_SURFACE_TYPE_PDF:
    case CAIRO_SURFACE_TYPE_PS:
    case CAIRO_SURFACE_TYPE_SVG:
    case CAIRO_SURFACE_TYPE_WIN32_PRINTING:
    case CAIRO_SURFACE_TYPE_XML:
      return DrawTargetType::VECTOR;

    case CAIRO_SURFACE_TYPE_VG:
    case CAIRO_SURFACE_TYPE_GL:
    case CAIRO_SURFACE_TYPE_GLITZ:
    case CAIRO_SURFACE_TYPE_QUARTZ:
    case CAIRO_SURFACE_TYPE_DIRECTFB:
      return DrawTargetType::HARDWARE_RASTER;

    case CAIRO_SURFACE_TYPE_SKIA:
    case CAIRO_SURFACE_TYPE_QT:
      MOZ_ASSERT(false, "Can't determine actual DrawTargetType for DrawTargetCairo - assuming SOFTWARE_RASTER");
      // fallthrough
    case CAIRO_SURFACE_TYPE_IMAGE:
    case CAIRO_SURFACE_TYPE_XLIB:
    case CAIRO_SURFACE_TYPE_XCB:
    case CAIRO_SURFACE_TYPE_WIN32:
    case CAIRO_SURFACE_TYPE_BEOS:
    case CAIRO_SURFACE_TYPE_OS2:
    case CAIRO_SURFACE_TYPE_QUARTZ_IMAGE:
    case CAIRO_SURFACE_TYPE_SCRIPT:
    case CAIRO_SURFACE_TYPE_RECORDING:
    case CAIRO_SURFACE_TYPE_DRM:
    case CAIRO_SURFACE_TYPE_SUBSURFACE:
#ifdef CAIRO_HAS_D2D_SURFACE
    case CAIRO_SURFACE_TYPE_D2D:
#endif
    case CAIRO_SURFACE_TYPE_TEE: // included to silence warning about unhandled enum value
      return DrawTargetType::SOFTWARE_RASTER;
    default:
      MOZ_CRASH("Unsupported cairo surface type");
    }
  }
  MOZ_ASSERT(false, "Could not determine DrawTargetType for DrawTargetCairo");
  return DrawTargetType::SOFTWARE_RASTER;
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

bool
DrawTargetCairo::LockBits(uint8_t** aData, IntSize* aSize,
                          int32_t* aStride, SurfaceFormat* aFormat)
{
  if (cairo_surface_get_type(mSurface) == CAIRO_SURFACE_TYPE_IMAGE) {
    WillChange();

    mLockedBits = cairo_image_surface_get_data(mSurface);
    *aData = mLockedBits;
    *aSize = GetSize();
    *aStride = cairo_image_surface_get_stride(mSurface);
    *aFormat = GetFormat();
    return true;
  }

  return false;
}

void
DrawTargetCairo::ReleaseBits(uint8_t* aData)
{
  MOZ_ASSERT(mLockedBits == aData);
  mLockedBits = nullptr;
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

cairo_surface_t*
DrawTargetCairo::GetDummySurface()
{
  if (mDummySurface) {
    return mDummySurface;
  }

  mDummySurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);

  return mDummySurface;
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

  // If the destination rect covers the entire clipped area, then unbounded and bounded
  // operations are identical, and we don't need to push a group.
  bool needsGroup = !IsOperatorBoundByMask(aOptions.mCompositionOp) &&
                    !aDest.Contains(GetUserSpaceClip());

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
DrawTargetCairo::DrawFilter(FilterNode *aNode,
                            const Rect &aSourceRect,
                            const Point &aDestPoint,
                            const DrawOptions &aOptions)
{
  FilterNodeSoftware* filter = static_cast<FilterNodeSoftware*>(aNode);
  filter->Draw(this, aSourceRect, aDestPoint, aOptions);
}

void
DrawTargetCairo::DrawSurfaceWithShadow(SourceSurface *aSurface,
                                       const Point &aDest,
                                       const Color &aColor,
                                       const Point &aOffset,
                                       Float aSigma,
                                       CompositionOp aOperator)
{
  if (aSurface->GetType() != SurfaceType::CAIRO) {
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
                      aSigma, aSigma);
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
                             DrawPatternType aDrawType,
                             bool aPathBoundsClip)
{
  if (!PatternIsCompatible(aPattern)) {
    return;
  }

  AutoClearDeviceOffset clear(aPattern);

  cairo_pattern_t* pat = GfxPatternToCairoPattern(aPattern, aOptions.mAlpha);
  if (!pat) {
    return;
  }
  if (cairo_pattern_status(pat)) {
    cairo_pattern_destroy(pat);
    gfxWarning() << "Invalid pattern";
    return;
  }

  cairo_set_source(mContext, pat);

  cairo_set_antialias(mContext, GfxAntialiasToCairoAntialias(aOptions.mAntialiasMode));

  if (NeedIntermediateSurface(aPattern, aOptions) ||
      (!IsOperatorBoundByMask(aOptions.mCompositionOp) && !aPathBoundsClip)) {
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

  bool pathBoundsClip = false;

  if (aRect.Contains(GetUserSpaceClip())) {
    pathBoundsClip = true;
  }

  DrawPattern(aPattern, StrokeOptions(), aOptions, DRAW_FILL, pathBoundsClip);
}

void
DrawTargetCairo::CopySurfaceInternal(cairo_surface_t* aSurface,
                                     const IntRect &aSource,
                                     const IntPoint &aDest)
{
  if (cairo_surface_status(aSurface)) {
    gfxWarning() << "Invalid surface";
    return;
  }

  cairo_identity_matrix(mContext);

  cairo_set_source_surface(mContext, aSurface, aDest.x - aSource.x, aDest.y - aSource.y);
  cairo_set_operator(mContext, CAIRO_OPERATOR_SOURCE);
  cairo_set_antialias(mContext, CAIRO_ANTIALIAS_NONE);

  cairo_reset_clip(mContext);
  cairo_new_path(mContext);
  cairo_rectangle(mContext, aDest.x, aDest.y, aSource.width, aSource.height);
  cairo_fill(mContext);
}

void
DrawTargetCairo::CopySurface(SourceSurface *aSurface,
                             const IntRect &aSource,
                             const IntPoint &aDest)
{
  AutoPrepareForDrawing prep(this, mContext);
  AutoClearDeviceOffset clear(aSurface);

  if (!aSurface) {
    gfxWarning() << "Unsupported surface type specified";
    return;
  }

  cairo_surface_t* surf = GetCairoSurfaceForSourceSurface(aSurface);
  if (!surf) {
    gfxWarning() << "Unsupported surface type specified";
    return;
  }

  CopySurfaceInternal(surf, aSource, aDest);
  cairo_surface_destroy(surf);
}

void
DrawTargetCairo::CopyRect(const IntRect &aSource,
                          const IntPoint &aDest)
{
  AutoPrepareForDrawing prep(this, mContext);

  IntRect source = aSource;
  cairo_surface_t* surf = mSurface;

  if (!SupportsSelfCopy(mSurface) &&
      aDest.y >= aSource.y &&
      aDest.y < aSource.YMost()) {
    cairo_surface_t* similar = cairo_surface_create_similar(mSurface,
                                                            GfxFormatToCairoContent(GetFormat()),
                                                            aSource.width, aSource.height);
    cairo_t* ctx = cairo_create(similar);
    cairo_set_operator(ctx, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_surface(ctx, surf, -aSource.x, -aSource.y);
    cairo_paint(ctx);
    cairo_destroy(ctx);

    source.x = 0;
    source.y = 0;
    surf = similar;
  }

  CopySurfaceInternal(surf, source, aDest);

  if (surf != mSurface) {
    cairo_surface_destroy(surf);
  }
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

  if (aPath->GetBackendType() != BackendType::CAIRO)
    return;

  PathCairo* path = const_cast<PathCairo*>(static_cast<const PathCairo*>(aPath));
  path->SetPathOnContext(mContext);

  DrawPattern(aPattern, aStrokeOptions, aOptions, DRAW_STROKE);
}

void
DrawTargetCairo::Fill(const Path *aPath,
                      const Pattern &aPattern,
                      const DrawOptions &aOptions /* = DrawOptions() */)
{
  AutoPrepareForDrawing prep(this, mContext, aPath);

  if (aPath->GetBackendType() != BackendType::CAIRO)
    return;

  PathCairo* path = const_cast<PathCairo*>(static_cast<const PathCairo*>(aPath));
  path->SetPathOnContext(mContext);

  DrawPattern(aPattern, StrokeOptions(), aOptions, DRAW_FILL);
}

void
DrawTargetCairo::SetPermitSubpixelAA(bool aPermitSubpixelAA)
{
  DrawTarget::SetPermitSubpixelAA(aPermitSubpixelAA);
#ifdef MOZ_TREE_CAIRO
  cairo_surface_set_subpixel_antialiasing(mSurface,
    aPermitSubpixelAA ? CAIRO_SUBPIXEL_ANTIALIASING_ENABLED : CAIRO_SUBPIXEL_ANTIALIASING_DISABLED);
#endif
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
  if (!pat)
    return;

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
  if (!source) {
    return;
  }

  cairo_pattern_t* mask = GfxPatternToCairoPattern(aMask, aOptions.mAlpha);
  if (!mask) {
    cairo_pattern_destroy(source);
    return;
  }

  if (cairo_pattern_status(source) || cairo_pattern_status(mask)) {
    cairo_pattern_destroy(source);
    cairo_pattern_destroy(mask);
    gfxWarning() << "Invalid pattern";
    return;
  }

  cairo_set_source(mContext, source);
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
  if (!pat) {
    return;
  }

  if (cairo_pattern_status(pat)) {
    cairo_pattern_destroy(pat);
    gfxWarning() << "Invalid pattern";
    return;
  }

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
  if (!surf) {
    cairo_pattern_destroy(pat);
    return;
  }
  cairo_pattern_t* mask = cairo_pattern_create_for_surface(surf);
  cairo_matrix_t matrix;

  cairo_matrix_init_translate (&matrix, -aOffset.x, -aOffset.y);
  cairo_pattern_set_matrix (mask, &matrix);

  cairo_set_operator(mContext, GfxOpToCairoOp(aOptions.mCompositionOp));

  cairo_mask(mContext, mask);

  cairo_surface_destroy(surf);
  cairo_pattern_destroy(mask);
  cairo_pattern_destroy(pat);
}

void
DrawTargetCairo::PushClip(const Path *aPath)
{
  if (aPath->GetBackendType() != BackendType::CAIRO) {
    return;
  }

  WillChange(aPath);
  cairo_save(mContext);

  PathCairo* path = const_cast<PathCairo*>(static_cast<const PathCairo*>(aPath));
  path->SetPathOnContext(mContext);
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

  // cairo_restore will restore the transform too and we don't want to do that
  // so we'll save it now and restore it after the cairo_restore
  cairo_matrix_t mat;
  cairo_get_matrix(mContext, &mat);

  cairo_restore(mContext);

  cairo_set_matrix(mContext, &mat);

  MOZ_ASSERT(cairo_status(mContext) || GetTransform() == Matrix(mat.xx, mat.yx, mat.xy, mat.yy, mat.x0, mat.y0),
             "Transforms are out of sync");
}

TemporaryRef<PathBuilder>
DrawTargetCairo::CreatePathBuilder(FillRule aFillRule /* = FillRule::FILL_WINDING */) const
{
  return new PathBuilderCairo(aFillRule);
}

void
DrawTargetCairo::ClearSurfaceForUnboundedSource(const CompositionOp &aOperator)
{
  if (aOperator != CompositionOp::OP_SOURCE)
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
  return new GradientStopsCairo(aStops, aNumStops, aExtendMode);
}

TemporaryRef<FilterNode>
DrawTargetCairo::CreateFilter(FilterType aType)
{
  return FilterNodeSoftware::Create(aType);
}

TemporaryRef<SourceSurface>
DrawTargetCairo::CreateSourceSurfaceFromData(unsigned char *aData,
                                             const IntSize &aSize,
                                             int32_t aStride,
                                             SurfaceFormat aFormat) const
{
  cairo_surface_t* surf = CopyToImageSurface(aData, IntRect(IntPoint(), aSize),
                                             aStride, aFormat);

  RefPtr<SourceSurfaceCairo> source_surf = new SourceSurfaceCairo(surf, aSize, aFormat);
  cairo_surface_destroy(surf);

  return source_surf.forget();
}

#ifdef CAIRO_HAS_XLIB_SURFACE
static cairo_user_data_key_t gDestroyPixmapKey;

struct DestroyPixmapClosure {
  DestroyPixmapClosure(Drawable d, Screen *s) : mPixmap(d), mScreen(s) {}
  ~DestroyPixmapClosure() {
    XFreePixmap(DisplayOfScreen(mScreen), mPixmap);
  }
  Drawable mPixmap;
  Screen *mScreen;
};

static void
DestroyPixmap(void *data)
{
  delete static_cast<DestroyPixmapClosure*>(data);
}
#endif

TemporaryRef<SourceSurface>
DrawTargetCairo::OptimizeSourceSurface(SourceSurface *aSurface) const
{
#ifdef CAIRO_HAS_XLIB_SURFACE
  cairo_surface_type_t ctype = cairo_surface_get_type(mSurface);
  if (aSurface->GetType() == SurfaceType::CAIRO &&
      cairo_surface_get_type(
        static_cast<SourceSurfaceCairo*>(aSurface)->GetSurface()) == ctype) {
    return aSurface;
  }

  if (ctype != CAIRO_SURFACE_TYPE_XLIB) {
    return aSurface;
  }

  IntSize size = aSurface->GetSize();
  if (!size.width || !size.height) {
    return aSurface;
  }

  // Although the dimension parameters in the xCreatePixmapReq wire protocol are
  // 16-bit unsigned integers, the server's CreatePixmap returns BadAlloc if
  // either dimension cannot be represented by a 16-bit *signed* integer.
  #define XLIB_IMAGE_SIDE_SIZE_LIMIT 0x7fff

  if (size.width > XLIB_IMAGE_SIDE_SIZE_LIMIT ||
      size.height > XLIB_IMAGE_SIDE_SIZE_LIMIT) {
    return aSurface;
  }

  SurfaceFormat format = aSurface->GetFormat();
  Screen *screen = cairo_xlib_surface_get_screen(mSurface);
  Display *dpy = DisplayOfScreen(screen);
  XRenderPictFormat* xrenderFormat = nullptr;
  switch (format) {
  case SurfaceFormat::B8G8R8A8:
    xrenderFormat = XRenderFindStandardFormat(dpy, PictStandardARGB32);
    break;
  case SurfaceFormat::B8G8R8X8:
    xrenderFormat = XRenderFindStandardFormat(dpy, PictStandardRGB24);
    break;
  case SurfaceFormat::A8:
    xrenderFormat = XRenderFindStandardFormat(dpy, PictStandardA8);
    break;
  default:
    return aSurface;
  }
  if (!xrenderFormat) {
    return aSurface;
  }

  Drawable pixmap = XCreatePixmap(dpy, RootWindowOfScreen(screen),
                                  size.width, size.height,
                                  xrenderFormat->depth);
  if (!pixmap) {
    return aSurface;
  }

  ScopedDeletePtr<DestroyPixmapClosure> closure(
    new DestroyPixmapClosure(pixmap, screen));

  ScopedCairoSurface csurf(
    cairo_xlib_surface_create_with_xrender_format(dpy, pixmap,
                                                  screen, xrenderFormat,
                                                  size.width, size.height));
  if (!csurf || cairo_surface_status(csurf)) {
    return aSurface;
  }

  cairo_surface_set_user_data(csurf, &gDestroyPixmapKey,
                              closure.forget(), DestroyPixmap);

  RefPtr<DrawTargetCairo> dt = new DrawTargetCairo();
  if (!dt->Init(csurf, size, &format)) {
    return aSurface;
  }

  dt->CopySurface(aSurface,
                  IntRect(0, 0, size.width, size.height),
                  IntPoint(0, 0));
  dt->Flush();

  return new SourceSurfaceCairo(csurf, size, format);
#endif

  return aSurface;
}

TemporaryRef<SourceSurface>
DrawTargetCairo::CreateSourceSurfaceFromNativeSurface(const NativeSurface &aSurface) const
{
  if (aSurface.mType == NativeSurfaceType::CAIRO_SURFACE) {
    if (aSurface.mSize.width <= 0 ||
        aSurface.mSize.height <= 0) {
      gfxWarning() << "Can't create a SourceSurface without a valid size";
      return nullptr;
    }
    cairo_surface_t* surf = static_cast<cairo_surface_t*>(aSurface.mSurface);
    return new SourceSurfaceCairo(surf, aSurface.mSize, aSurface.mFormat);
  }

  return nullptr;
}

TemporaryRef<DrawTarget>
DrawTargetCairo::CreateSimilarDrawTarget(const IntSize &aSize, SurfaceFormat aFormat) const
{
  cairo_surface_t* similar = cairo_surface_create_similar(mSurface,
                                                          GfxFormatToCairoContent(aFormat),
                                                          aSize.width, aSize.height);

  if (!cairo_surface_status(similar)) {
    RefPtr<DrawTargetCairo> target = new DrawTargetCairo();
    target->InitAlreadyReferenced(similar, aSize);
    return target.forget();
  }

  gfxCriticalError() << "Failed to create similar cairo surface! Size: " << aSize << " Status: " << cairo_surface_status(similar);

  return nullptr;
}

bool
DrawTargetCairo::InitAlreadyReferenced(cairo_surface_t* aSurface, const IntSize& aSize, SurfaceFormat* aFormat)
{
  if (cairo_surface_status(aSurface)) {
    gfxCriticalError() << "Attempt to create DrawTarget for invalid surface. "
                       << aSize << " Cairo Status: " << cairo_surface_status(aSurface);
    cairo_surface_destroy(aSurface);
    return false;
  }

  mContext = cairo_create(aSurface);
  mSurface = aSurface;
  mSize = aSize;
  mFormat = aFormat ? *aFormat : CairoContentToGfxFormat(cairo_surface_get_content(aSurface));

  // Cairo image surface have a bug where they will allocate a mask surface (for clipping)
  // the size of the clip extents, and don't take the surface extents into account.
  // Add a manual clip to the surface extents to prevent this.
  cairo_new_path(mContext);
  cairo_rectangle(mContext, 0, 0, mSize.width, mSize.height);
  cairo_clip(mContext);

  if (mFormat == SurfaceFormat::B8G8R8A8 ||
      mFormat == SurfaceFormat::R8G8B8A8) {
    SetPermitSubpixelAA(false);
  } else {
    SetPermitSubpixelAA(true);
  }

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
    return target.forget();
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
  return target.forget();
}

bool
DrawTargetCairo::Init(cairo_surface_t* aSurface, const IntSize& aSize, SurfaceFormat* aFormat)
{
  cairo_surface_reference(aSurface);
  return InitAlreadyReferenced(aSurface, aSize, aFormat);
}

bool
DrawTargetCairo::Init(const IntSize& aSize, SurfaceFormat aFormat)
{
  cairo_surface_t *surf = cairo_image_surface_create(GfxFormatToCairoFormat(aFormat), aSize.width, aSize.height);
  return InitAlreadyReferenced(surf, aSize);
}

bool
DrawTargetCairo::Init(unsigned char* aData, const IntSize &aSize, int32_t aStride, SurfaceFormat aFormat)
{
  cairo_surface_t* surf =
    cairo_image_surface_create_for_data(aData,
                                        GfxFormatToCairoFormat(aFormat),
                                        aSize.width,
                                        aSize.height,
                                        aStride);
  return InitAlreadyReferenced(surf, aSize);
}

void *
DrawTargetCairo::GetNativeSurface(NativeSurfaceType aType)
{
  if (aType == NativeSurfaceType::CAIRO_SURFACE) {
    return cairo_get_target(mContext);
  }
  if (aType == NativeSurfaceType::CAIRO_CONTEXT) {
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
  MOZ_ASSERT(!mLockedBits);
}

void
DrawTargetCairo::SetTransform(const Matrix& aTransform)
{
  mTransform = aTransform;

  cairo_matrix_t mat;
  GfxMatrixToCairoMatrix(mTransform, mat);
  cairo_set_matrix(mContext, &mat);
}

Rect
DrawTargetCairo::GetUserSpaceClip()
{
  double clipX1, clipY1, clipX2, clipY2;
  cairo_clip_extents(mContext, &clipX1, &clipY1, &clipX2, &clipY2);
  return Rect(clipX1, clipY1, clipX2 - clipX1, clipY2 - clipY1); // Narrowing of doubles to floats
}

cairo_t*
BorrowedCairoContext::BorrowCairoContextFromDrawTarget(DrawTarget* aDT)
{
  if (aDT->GetBackendType() != BackendType::CAIRO ||
      aDT->IsDualDrawTarget() ||
      aDT->IsTiledDrawTarget()) {
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
  if (aDT->GetBackendType() != BackendType::CAIRO ||
      aDT->IsDualDrawTarget() ||
      aDT->IsTiledDrawTarget()) {
    return;
  }
  DrawTargetCairo* cairoDT = static_cast<DrawTargetCairo*>(aDT);

  cairo_restore(aCairo);
  cairoDT->mContext = aCairo;
}

}
}
