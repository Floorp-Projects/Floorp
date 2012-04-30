/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <stuart@mozilla.com>
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

#include "gfxTypes.h"
#include "gfxPattern.h"
#include "gfxASurface.h"
#include "gfxPlatform.h"

#include "cairo.h"

#include <vector>

using namespace mozilla::gfx;

gfxPattern::gfxPattern(cairo_pattern_t *aPattern)
  : mGfxPattern(NULL)
{
    mPattern = cairo_pattern_reference(aPattern);
}

gfxPattern::gfxPattern(const gfxRGBA& aColor)
  : mGfxPattern(NULL)
{
    mPattern = cairo_pattern_create_rgba(aColor.r, aColor.g, aColor.b, aColor.a);
}

// from another surface
gfxPattern::gfxPattern(gfxASurface *surface)
  : mGfxPattern(NULL)
{
    mPattern = cairo_pattern_create_for_surface(surface->CairoSurface());
}

// linear
gfxPattern::gfxPattern(gfxFloat x0, gfxFloat y0, gfxFloat x1, gfxFloat y1)
  : mGfxPattern(NULL)
{
    mPattern = cairo_pattern_create_linear(x0, y0, x1, y1);
}

// radial
gfxPattern::gfxPattern(gfxFloat cx0, gfxFloat cy0, gfxFloat radius0,
                       gfxFloat cx1, gfxFloat cy1, gfxFloat radius1)
  : mGfxPattern(NULL)
{
    mPattern = cairo_pattern_create_radial(cx0, cy0, radius0,
                                           cx1, cy1, radius1);
}

// Azure
gfxPattern::gfxPattern(SourceSurface *aSurface, const Matrix &aTransform)
  : mPattern(NULL)
  , mGfxPattern(NULL)
  , mSourceSurface(aSurface)
  , mTransform(aTransform)
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
    mStops = NULL;
    if (gfxPlatform::GetCMSMode() == eCMSMode_All) {
        gfxRGBA cms;
        gfxPlatform::TransformPixel(c, cms, gfxPlatform::GetCMSRGBTransform());

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
    return ThebesMatrix(mTransform);
  }
}

Pattern*
gfxPattern::GetPattern(DrawTarget *aTarget, Matrix *aPatternTransform)
{
  if (!mPattern) {
    mGfxPattern = new (mSurfacePattern.addr())
      SurfacePattern(mSourceSurface, EXTEND_CLAMP, mTransform);
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

      cairo_surface_t *surf = NULL;
      cairo_pattern_get_surface(mPattern, &surf);

      if (!mSourceSurface) {
        nsRefPtr<gfxASurface> gfxSurf = gfxASurface::Wrap(surf);
        mSourceSurface =
          gfxPlatform::GetPlatform()->GetSourceSurfaceForSurface(aTarget, gfxSurf);
      }

      if (mSourceSurface) {
        Matrix newMat = ToMatrix(matrix);

        AdjustTransformForPattern(newMat, aTarget->GetTransform(), aPatternTransform);

        newMat.Invert();
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

        newMat.Invert();

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

        newMat.Invert();

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
    mStops = NULL;
    if (extend == EXTEND_PAD_EDGE) {
        if (cairo_pattern_get_type(mPattern) == CAIRO_PATTERN_TYPE_SURFACE) {
            cairo_surface_t *surf = NULL;

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
    mExtend = ToExtendMode(extend);
  }
}

bool
gfxPattern::IsOpaque()
{
  if (mPattern) {
    switch (cairo_pattern_get_type(mPattern)) {
    case CAIRO_PATTERN_TYPE_SURFACE:
      {
        cairo_surface_t *surf = NULL;
        cairo_pattern_get_surface(mPattern, &surf);

        if (cairo_surface_get_content(surf) == CAIRO_CONTENT_COLOR) {
          return true;
        }
      }
    default:
      return false;
    }
  }

  if (mSourceSurface->GetFormat() == FORMAT_B8G8R8X8) {
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
    return ThebesExtend(mExtend);
  }
}

void
gfxPattern::SetFilter(GraphicsFilter filter)
{
  if (mPattern) {
    cairo_pattern_set_filter(mPattern, (cairo_filter_t)filter);
  } else {
    mFilter = ToFilter(filter);
  }
}

gfxPattern::GraphicsFilter
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
    cairo_surface_t *surf = nsnull;

    if (cairo_pattern_get_surface (mPattern, &surf) != CAIRO_STATUS_SUCCESS)
        return nsnull;

    return gfxASurface::Wrap(surf);
  } else {
    // We should never be trying to get the surface off an Azure gfx Pattern.
    NS_ERROR("Attempt to get surface off an Azure gfxPattern!");
    return NULL;
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
  if (!aOriginalTransform) {
    return;
  }

  Matrix mat = *aOriginalTransform;
  mat.Invert();

  aPatternTransform = mat * aCurrentTransform * aPatternTransform;
}