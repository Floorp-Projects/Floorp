/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxTypes.h"
#include "gfxPattern.h"
#include "gfxASurface.h"
#include "gfxPlatform.h"
#include "gfx2DGlue.h"
#include "gfxGradientCache.h"

#include "cairo.h"

#include <vector>

using namespace mozilla::gfx;

gfxPattern::gfxPattern(cairo_pattern_t *aPattern)
  : mGfxPattern(nullptr)
{
    mPattern = cairo_pattern_reference(aPattern);
}

gfxPattern::gfxPattern(const gfxRGBA& aColor)
  : mGfxPattern(nullptr)
{
    mPattern = cairo_pattern_create_rgba(aColor.r, aColor.g, aColor.b, aColor.a);
}

// from another surface
gfxPattern::gfxPattern(gfxASurface *surface)
  : mGfxPattern(nullptr)
{
    mPattern = cairo_pattern_create_for_surface(surface->CairoSurface());
}

// linear
gfxPattern::gfxPattern(gfxFloat x0, gfxFloat y0, gfxFloat x1, gfxFloat y1)
  : mGfxPattern(nullptr)
{
    mPattern = cairo_pattern_create_linear(x0, y0, x1, y1);
}

// radial
gfxPattern::gfxPattern(gfxFloat cx0, gfxFloat cy0, gfxFloat radius0,
                       gfxFloat cx1, gfxFloat cy1, gfxFloat radius1)
  : mGfxPattern(nullptr)
{
    mPattern = cairo_pattern_create_radial(cx0, cy0, radius0,
                                           cx1, cy1, radius1);
}

// Azure
gfxPattern::gfxPattern(SourceSurface *aSurface, const Matrix &aTransform)
  : mPattern(nullptr)
  , mGfxPattern(nullptr)
  , mSourceSurface(aSurface)
  , mTransform(aTransform)
  , mExtend(EXTEND_NONE)
{
}

gfxPattern::~gfxPattern()
{
    cairo_pattern_destroy(mPattern);

    if (mGfxPattern) {
      mGfxPattern->~Pattern();
    }
}

cairo_pattern_t *
gfxPattern::CairoPattern()
{
    return mPattern;
}

void
gfxPattern::AddColorStop(gfxFloat offset, const gfxRGBA& c)
{
  if (mPattern) {
    mStops = nullptr;
    if (gfxPlatform::GetCMSMode() == eCMSMode_All) {
        gfxRGBA cms;
        qcms_transform *transform = gfxPlatform::GetCMSRGBTransform();
        if (transform)
          gfxPlatform::TransformPixel(c, cms, transform);

        // Use the original alpha to avoid unnecessary float->byte->float
        // conversion errors
        cairo_pattern_add_color_stop_rgba(mPattern, offset,
                                          cms.r, cms.g, cms.b, c.a);
    }
    else
        cairo_pattern_add_color_stop_rgba(mPattern, offset, c.r, c.g, c.b, c.a);
  }
}

void
gfxPattern::SetColorStops(mozilla::RefPtr<mozilla::gfx::GradientStops> aStops)
{
  mStops = aStops;
}

void
gfxPattern::CacheColorStops(mozilla::gfx::DrawTarget *aDT)
{
  if (mPattern) {
    mStops = nullptr;
    nsTArray<GradientStop> stops;
    int count = 0;
    cairo_pattern_get_color_stop_count(mPattern, &count);
    stops.SetLength(count);
    for (int n = 0; n < count; ++n) {
      double offset, r, g, b, a;
      cairo_pattern_get_color_stop_rgba(mPattern, n, &offset, &r, &g, &b, &a);
      stops[n].color = mozilla::gfx::Color(r, g, b, a);
      stops[n].offset = offset;
    }
    mStops = gfxGradientCache::GetOrCreateGradientStops(aDT,
                                                        stops,
                                                        (cairo_pattern_get_extend(mPattern) == CAIRO_EXTEND_REPEAT)
                                                        ? mozilla::gfx::ExtendMode::REPEAT
                                                        : mozilla::gfx::ExtendMode::CLAMP);
  }
}

void
gfxPattern::SetMatrix(const gfxMatrix& matrix)
{
  if (mPattern) {
    cairo_matrix_t mat = *reinterpret_cast<const cairo_matrix_t*>(&matrix);
    cairo_pattern_set_matrix(mPattern, &mat);
  } else {
    mTransform = ToMatrix(matrix);
    // Cairo-pattern matrices specify the conversion from DrawTarget to pattern
    // space. Azure pattern matrices specify the conversion from pattern to
    // DrawTarget space.
    mTransform.Invert();
  }
}

gfxMatrix
gfxPattern::GetMatrix() const
{
  if (mPattern) {
    cairo_matrix_t mat;
    cairo_pattern_get_matrix(mPattern, &mat);
    return gfxMatrix(*reinterpret_cast<gfxMatrix*>(&mat));
  } else {
    // invert at the higher precision of gfxMatrix
    // cause we need to convert at some point anyways
    gfxMatrix mat = ThebesMatrix(mTransform);
    mat.Invert();
    return mat;
  }
}

gfxMatrix
gfxPattern::GetInverseMatrix() const
{
  if (mPattern) {
    cairo_matrix_t mat;
    cairo_pattern_get_matrix(mPattern, &mat);
    cairo_matrix_invert(&mat);
    return gfxMatrix(*reinterpret_cast<gfxMatrix*>(&mat));
  } else {
    return ThebesMatrix(mTransform);
  }
}

Pattern*
gfxPattern::GetPattern(DrawTarget *aTarget, Matrix *aPatternTransform)
{
  if (mGfxPattern) {
    mGfxPattern->~Pattern();
    mGfxPattern = nullptr;
  }

  if (!mPattern) {
    Matrix adjustedMatrix = mTransform;
    if (aPatternTransform)
      AdjustTransformForPattern(adjustedMatrix, aTarget->GetTransform(), aPatternTransform);
    mGfxPattern = new (mSurfacePattern.addr())
      SurfacePattern(mSourceSurface, ToExtendMode(mExtend), adjustedMatrix, mFilter);
    return mGfxPattern;
  }

  GraphicsExtend extend = (GraphicsExtend)cairo_pattern_get_extend(mPattern);

  switch (cairo_pattern_get_type(mPattern)) {
  case CAIRO_PATTERN_TYPE_SOLID:
    {
      double r, g, b, a;
      cairo_pattern_get_rgba(mPattern, &r, &g, &b, &a);

      new (mColorPattern.addr()) ColorPattern(Color(r, g, b, a));
      return mColorPattern.addr();
    }
  case CAIRO_PATTERN_TYPE_SURFACE:
    {
      GraphicsFilter filter = (GraphicsFilter)cairo_pattern_get_filter(mPattern);
      cairo_matrix_t mat;
      cairo_pattern_get_matrix(mPattern, &mat);
      gfxMatrix matrix(*reinterpret_cast<gfxMatrix*>(&mat));

      cairo_surface_t *surf = nullptr;
      cairo_pattern_get_surface(mPattern, &surf);

      if (!mSourceSurface) {
        nsRefPtr<gfxASurface> gfxSurf = gfxASurface::Wrap(surf);
        // The underlying surface here will be kept around by the gfxPattern.
        // This function is intended to be used right away.
        mSourceSurface =
          gfxPlatform::GetPlatform()->GetSourceSurfaceForSurface(aTarget, gfxSurf);
      }

      if (mSourceSurface) {
        Matrix newMat = ToMatrix(matrix);

        AdjustTransformForPattern(newMat, aTarget->GetTransform(), aPatternTransform);

        double x, y;
        cairo_surface_get_device_offset(surf, &x, &y);
        newMat.Translate(-x, -y);
        mGfxPattern = new (mSurfacePattern.addr())
          SurfacePattern(mSourceSurface, ToExtendMode(extend), newMat, ToFilter(filter));
        return mGfxPattern;
      }
      break;
    }
  case CAIRO_PATTERN_TYPE_LINEAR:
    {
      double x1, y1, x2, y2;
      cairo_pattern_get_linear_points(mPattern, &x1, &y1, &x2, &y2);
      if (!mStops) {
        int count = 0;
        cairo_pattern_get_color_stop_count(mPattern, &count);

        std::vector<GradientStop> stops;

        for (int i = 0; i < count; i++) {
          GradientStop stop;
          double r, g, b, a, offset;
          cairo_pattern_get_color_stop_rgba(mPattern, i, &offset, &r, &g, &b, &a);

          stop.offset = offset;
          stop.color = Color(Float(r), Float(g), Float(b), Float(a));
          stops.push_back(stop);
        }

        mStops = aTarget->CreateGradientStops(&stops.front(), count, ToExtendMode(extend));
      }

      if (mStops) {
        cairo_matrix_t mat;
        cairo_pattern_get_matrix(mPattern, &mat);
        gfxMatrix matrix(*reinterpret_cast<gfxMatrix*>(&mat));

        Matrix newMat = ToMatrix(matrix);

        AdjustTransformForPattern(newMat, aTarget->GetTransform(), aPatternTransform);

        mGfxPattern = new (mLinearGradientPattern.addr())
          LinearGradientPattern(Point(x1, y1), Point(x2, y2), mStops, newMat);

        return mGfxPattern;
      }
      break;
    }
  case CAIRO_PATTERN_TYPE_RADIAL:
    {
      if (!mStops) {
        int count = 0;
        cairo_pattern_get_color_stop_count(mPattern, &count);

        std::vector<GradientStop> stops;

        for (int i = 0; i < count; i++) {
          GradientStop stop;
          double r, g, b, a, offset;
          cairo_pattern_get_color_stop_rgba(mPattern, i, &offset, &r, &g, &b, &a);

          stop.offset = offset;
          stop.color = Color(Float(r), Float(g), Float(b), Float(a));
          stops.push_back(stop);
        }

        mStops = aTarget->CreateGradientStops(&stops.front(), count, ToExtendMode(extend));
      }

      if (mStops) {
        cairo_matrix_t mat;
        cairo_pattern_get_matrix(mPattern, &mat);
        gfxMatrix matrix(*reinterpret_cast<gfxMatrix*>(&mat));

        Matrix newMat = ToMatrix(matrix);

        AdjustTransformForPattern(newMat, aTarget->GetTransform(), aPatternTransform);

        double x1, y1, x2, y2, r1, r2;
        cairo_pattern_get_radial_circles(mPattern, &x1, &y1, &r1, &x2, &y2, &r2);
        mGfxPattern = new (mRadialGradientPattern.addr())
          RadialGradientPattern(Point(x1, y1), Point(x2, y2), r1, r2, mStops, newMat);

        return mGfxPattern;
      }
      break;
    }
  default:
    /* Reassure the compiler we are handling all the enum values.  */
    break;
  }

  new (mColorPattern.addr()) ColorPattern(Color(0, 0, 0, 0));
  return mColorPattern.addr();
}

void
gfxPattern::SetExtend(GraphicsExtend extend)
{
  if (mPattern) {
    mStops = nullptr;
    if (extend == EXTEND_PAD_EDGE) {
        if (cairo_pattern_get_type(mPattern) == CAIRO_PATTERN_TYPE_SURFACE) {
            cairo_surface_t *surf = nullptr;

            cairo_pattern_get_surface (mPattern, &surf);
            if (surf) {
                switch (cairo_surface_get_type(surf)) {
                    case CAIRO_SURFACE_TYPE_WIN32_PRINTING:
                    case CAIRO_SURFACE_TYPE_QUARTZ:
                        extend = EXTEND_NONE;
                        break;

                    case CAIRO_SURFACE_TYPE_WIN32:
                    case CAIRO_SURFACE_TYPE_XLIB:
                    default:
                        extend = EXTEND_PAD;
                        break;
                }
            }
        }

        // if something went wrong, or not a surface pattern, use PAD
        if (extend == EXTEND_PAD_EDGE)
            extend = EXTEND_PAD;
    }

    cairo_pattern_set_extend(mPattern, (cairo_extend_t)extend);
  } else {
    // This is always a surface pattern and will default to EXTEND_PAD
    // for EXTEND_PAD_EDGE.
    mExtend = extend;
  }
}

bool
gfxPattern::IsOpaque()
{
  if (mPattern) {
    switch (cairo_pattern_get_type(mPattern)) {
    case CAIRO_PATTERN_TYPE_SURFACE:
      {
        cairo_surface_t *surf = nullptr;
        cairo_pattern_get_surface(mPattern, &surf);

        if (cairo_surface_get_content(surf) == CAIRO_CONTENT_COLOR) {
          return true;
        }
      }
    default:
      return false;
    }
  }

  if (mSourceSurface->GetFormat() == SurfaceFormat::B8G8R8X8) {
    return true;
  }
  return false;
}

gfxPattern::GraphicsExtend
gfxPattern::Extend() const
{
  if (mPattern) {
    return (GraphicsExtend)cairo_pattern_get_extend(mPattern);
  } else {
    return mExtend;
  }
}

void
gfxPattern::SetFilter(GraphicsFilter filter)
{
  if (mPattern) {
    cairo_pattern_set_filter(mPattern, (cairo_filter_t)(int)filter);
  } else {
    mFilter = ToFilter(filter);
  }
}

GraphicsFilter
gfxPattern::Filter() const
{
  if (mPattern) {
    return (GraphicsFilter)cairo_pattern_get_filter(mPattern);
  } else {
    return ThebesFilter(mFilter);
  }
}

bool
gfxPattern::GetSolidColor(gfxRGBA& aColor)
{
    return cairo_pattern_get_rgba(mPattern,
                                  &aColor.r,
                                  &aColor.g,
                                  &aColor.b,
                                  &aColor.a) == CAIRO_STATUS_SUCCESS;
}

already_AddRefed<gfxASurface>
gfxPattern::GetSurface()
{
  if (mPattern) {
    cairo_surface_t *surf = nullptr;

    if (cairo_pattern_get_surface (mPattern, &surf) != CAIRO_STATUS_SUCCESS)
        return nullptr;

    return gfxASurface::Wrap(surf);
  } else {
    // We should never be trying to get the surface off an Azure gfx Pattern.
    NS_ERROR("Attempt to get surface off an Azure gfxPattern!");
    return nullptr;
  }
}

gfxPattern::GraphicsPatternType
gfxPattern::GetType() const
{
  if (mPattern) {
    return (GraphicsPatternType) cairo_pattern_get_type(mPattern);
  } else {
    // We should never be trying to get the type off an Azure gfx Pattern.
    MOZ_ASSERT(0);
    return PATTERN_SURFACE;
  }
}

int
gfxPattern::CairoStatus()
{
  if (mPattern) {
    return cairo_pattern_status(mPattern);
  } else {
    // An Azure pattern as this point is never in error status.
    return CAIRO_STATUS_SUCCESS;
  }
}

void
gfxPattern::AdjustTransformForPattern(Matrix &aPatternTransform,
                                      const Matrix &aCurrentTransform,
                                      const Matrix *aOriginalTransform)
{
  aPatternTransform.Invert();
  if (!aOriginalTransform) {
    // User space is unchanged, so to get from pattern space to user space,
    // just invert the cairo matrix.
    aPatternTransform.NudgeToIntegers();
    return;
  }
  // aPatternTransform now maps from pattern space to the user space defined
  // by *aOriginalTransform.

  Matrix mat = aCurrentTransform;
  mat.Invert();
  // mat maps from device space to current user space

  // First, transform from pattern space to original user space. Then transform
  // from original user space to device space. Then transform from
  // device space to current user space.
  aPatternTransform = aPatternTransform * *aOriginalTransform * mat;
  aPatternTransform.NudgeToIntegers();
}
