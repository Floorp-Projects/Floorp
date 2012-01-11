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
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bas Schouten <bschouten@mozilla.com>
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

#include "SourceSurfaceCairo.h"
#include "DrawTargetCairo.h"

#include "cairo.h"

namespace mozilla {
namespace gfx {

static cairo_format_t
GfxFormatToCairoFormat(SurfaceFormat format)
{
  switch (format)
  {
    case FORMAT_B8G8R8A8:
      return CAIRO_FORMAT_ARGB32;
    case FORMAT_B8G8R8X8:
      return CAIRO_FORMAT_RGB24;
    case FORMAT_A8:
      return CAIRO_FORMAT_A8;
  }

  return CAIRO_FORMAT_ARGB32;
}

static SurfaceFormat
CairoFormatToSurfaceFormat(cairo_format_t format)
{
  switch (format)
  {
    case CAIRO_FORMAT_ARGB32:
      return FORMAT_B8G8R8A8;
    case CAIRO_FORMAT_RGB24:
      return FORMAT_B8G8R8X8;
    case CAIRO_FORMAT_A8:
      return FORMAT_A8;
  }

  return FORMAT_B8G8R8A8;
}

static cairo_content_t
GfxFormatToCairoContent(SurfaceFormat format)
{
  switch(format)
  {
    case FORMAT_B8G8R8A8:
      return CAIRO_CONTENT_COLOR_ALPHA;
    case FORMAT_B8G8R8X8:
      return CAIRO_CONTENT_COLOR;
    case FORMAT_A8:
      return CAIRO_CONTENT_ALPHA;
  }

  return CAIRO_CONTENT_COLOR_ALPHA;
}

SourceSurfaceCairo::SourceSurfaceCairo(cairo_surface_t* aSurface,
                                       const IntSize& aSize,
                                       const SurfaceFormat& aFormat,
                                       DrawTargetCairo* aDrawTarget /* = NULL */)
 : mSize(aSize)
 , mFormat(aFormat)
 , mSurface(aSurface)
 , mDrawTarget(aDrawTarget)
{
  cairo_surface_reference(mSurface);
}

SourceSurfaceCairo::~SourceSurfaceCairo()
{
  MarkIndependent();
  cairo_surface_destroy(mSurface);
}

IntSize
SourceSurfaceCairo::GetSize() const
{
  return mSize;
}

SurfaceFormat
SourceSurfaceCairo::GetFormat() const
{
  return mFormat;
}

TemporaryRef<DataSourceSurface>
SourceSurfaceCairo::GetDataSurface()
{
  RefPtr<DataSourceSurfaceCairo> dataSurf;

  if (cairo_surface_get_type(mSurface) == CAIRO_SURFACE_TYPE_IMAGE) {
    dataSurf = new DataSourceSurfaceCairo(mSurface);
  } else {
    cairo_surface_t* imageSurf = cairo_image_surface_create(GfxFormatToCairoFormat(mFormat),
                                                            mSize.width, mSize.height);

    // Fill the new image surface with the contents of our surface.
    cairo_t* ctx = cairo_create(imageSurf);
    cairo_set_source_surface(ctx, mSurface, 0, 0);
    cairo_paint(ctx);
    cairo_destroy(ctx);

    dataSurf = new DataSourceSurfaceCairo(imageSurf);
    cairo_surface_destroy(imageSurf);
  }

  return dataSurf;
}

cairo_surface_t*
SourceSurfaceCairo::GetSurface() const
{
  return mSurface;
}

void
SourceSurfaceCairo::DrawTargetWillChange()
{
  if (mDrawTarget) {
    mDrawTarget = NULL;

    // We're about to lose our version of the surface, so make a copy of it.
    cairo_surface_t* surface = cairo_surface_create_similar(mSurface,
                                                            GfxFormatToCairoContent(mFormat),
                                                            mSize.width, mSize.height);
    cairo_t* ctx = cairo_create(surface);
    cairo_pattern_t* pat = cairo_pattern_create_for_surface(mSurface);
    cairo_set_source(ctx, pat);
    cairo_paint(ctx);
    cairo_destroy(ctx);

    // Swap in this new surface.
    cairo_surface_destroy(mSurface);
    mSurface = surface;
  }
}

void
SourceSurfaceCairo::MarkIndependent()
{
  if (mDrawTarget) {
    mDrawTarget->RemoveSnapshot(this);
    mDrawTarget = NULL;
  }
}

DataSourceSurfaceCairo::DataSourceSurfaceCairo(cairo_surface_t* imageSurf)
 : mImageSurface(imageSurf)
{
  cairo_surface_reference(mImageSurface);
}

DataSourceSurfaceCairo::~DataSourceSurfaceCairo()
{
  cairo_surface_destroy(mImageSurface);
}

unsigned char *
DataSourceSurfaceCairo::GetData()
{
  return cairo_image_surface_get_data(mImageSurface);
}

int32_t
DataSourceSurfaceCairo::Stride()
{
  return cairo_image_surface_get_stride(mImageSurface);
}

IntSize
DataSourceSurfaceCairo::GetSize() const
{
  IntSize size;
  size.width = cairo_image_surface_get_width(mImageSurface);
  size.height = cairo_image_surface_get_height(mImageSurface);

  return size;
}

SurfaceFormat
DataSourceSurfaceCairo::GetFormat() const
{
  return CairoFormatToSurfaceFormat(cairo_image_surface_get_format(mImageSurface));
}

cairo_surface_t*
DataSourceSurfaceCairo::GetSurface() const
{
  return mImageSurface;
}

}
}
