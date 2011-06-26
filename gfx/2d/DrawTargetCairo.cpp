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

namespace mozilla {
namespace gfx {

cairo_operator_t
GfxOpToCairoOp(CompositionOp op)
{
  switch (op)
  {
    case OP_OVER:
      return CAIRO_OPERATOR_OVER;
    case OP_SOURCE:
      return CAIRO_OPERATOR_SOURCE;
    case OP_ADD:
      return CAIRO_OPERATOR_ADD;
    case OP_ATOP:
      return CAIRO_OPERATOR_ATOP;
    case OP_COUNT:
      break;
  }

  return CAIRO_OPERATOR_OVER;
}

cairo_filter_t
GfxFilterToCairoFilter(Filter filter)
{
  switch (filter)
  {
    case FILTER_LINEAR:
      return CAIRO_FILTER_BILINEAR;
    case FILTER_POINT:
      return CAIRO_FILTER_NEAREST;
  }

  return CAIRO_FILTER_BILINEAR;
}

cairo_format_t
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

void
GfxMatrixToCairoMatrix(const Matrix& mat, cairo_matrix_t& retval)
{
  cairo_matrix_init(&retval, mat._11, mat._12, mat._21, mat._22, mat._31, mat._32);
}

DrawTargetCairo::DrawTargetCairo()
  : mContext(NULL)
{
}

DrawTargetCairo::~DrawTargetCairo()
{
  cairo_destroy(mContext);
}

TemporaryRef<SourceSurface>
DrawTargetCairo::Snapshot()
{
  return NULL;
}

void
DrawTargetCairo::Flush()
{
  cairo_surface_t* surf = cairo_get_target(mContext);
  cairo_surface_flush(surf);
}

void
DrawTargetCairo::DrawSurface(SourceSurface *aSurface,
                             const Rect &aDest,
                             const Rect &aSource,
                             const DrawSurfaceOptions &aSurfOptions,
                             const DrawOptions &aOptions)
{
  float sx = aSource.Width() / aDest.Width();
  float sy = aSource.Height() / aDest.Height();

  cairo_matrix_t src_mat;
  cairo_matrix_init_scale(&src_mat, sx, sy);
  cairo_matrix_translate(&src_mat, -aSource.X(), -aSource.Y());

  cairo_surface_t* surf = NULL;
  if (aSurface->GetType() == SURFACE_CAIRO) {
    surf = static_cast<SourceSurfaceCairo*>(aSurface)->GetSurface();
  }

  cairo_pattern_t* pat = cairo_pattern_create_for_surface(surf);
  cairo_pattern_set_matrix(pat, &src_mat);
  cairo_pattern_set_filter(pat, GfxFilterToCairoFilter(aSurfOptions.mFilter));

  cairo_set_operator(mContext, GfxOpToCairoOp(aOptions.mCompositionOp));
  cairo_rectangle(mContext, aDest.X(), aDest.Y(),
                  aDest.Width(), aDest.Height());
  cairo_fill(mContext);

  cairo_pattern_destroy(pat);
}

void
DrawTargetCairo::FillRect(const Rect &aRect,
                          const Pattern &aPattern,
                          const DrawOptions &aOptions)
{
  cairo_new_path(mContext);
  cairo_rectangle(mContext, aRect.x, aRect.y, aRect.Width(), aRect.Height());

  cairo_set_operator(mContext, GfxOpToCairoOp(aOptions.mCompositionOp));

  if (aPattern.GetType() == PATTERN_COLOR) {
    Color color = static_cast<const ColorPattern&>(aPattern).mColor;
    cairo_set_source_rgba(mContext, color.r, color.g,
                                    color.b, color.a);
  }

  cairo_fill(mContext);
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
  RefPtr<SourceSurfaceCairo> source_surf = new SourceSurfaceCairo();
  source_surf->InitFromSurface(surf, aSize, aFormat);
  cairo_surface_destroy(surf);
  return source_surf;
}

TemporaryRef<SourceSurface>
DrawTargetCairo::OptimizeSourceSurface(SourceSurface *aSurface) const
{
  return NULL;
}

TemporaryRef<SourceSurface>
DrawTargetCairo::CreateSourceSurfaceFromNativeSurface(const NativeSurface &aSurface) const
{
  return NULL;
}

bool
DrawTargetCairo::Init(cairo_surface_t* aSurface)
{
  mContext = cairo_create(aSurface);

  return true;
}

void
DrawTargetCairo::SetTransform(const Matrix& aTransform)
{
  cairo_matrix_t mat;
  GfxMatrixToCairoMatrix(aTransform, mat);
  cairo_set_matrix(mContext, &mat);
  mTransform = aTransform;
}

}
}
