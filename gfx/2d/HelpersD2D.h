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

#ifndef MOZILLA_GFX_HELPERSD2D_H_
#define MOZILLA_GFX_HELPERSD2D_H_

#include <d2d1.h>
#include "2D.h"

namespace mozilla {
namespace gfx {

static inline D2D1_POINT_2F D2DPoint(const Point &aPoint)
{
  return D2D1::Point2F(aPoint.x, aPoint.y);
}

static inline D2D1_SIZE_U D2DIntSize(const IntSize &aSize)
{
  return D2D1::SizeU(aSize.width, aSize.height);
}

static inline D2D1_RECT_F D2DRect(const Rect &aRect)
{
  return D2D1::RectF(aRect.x, aRect.y, aRect.XMost(), aRect.YMost());
}

static inline D2D1_BITMAP_INTERPOLATION_MODE D2DFilter(const Filter &aFilter)
{
  switch (aFilter) {
  case FILTER_POINT:
    return D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR;
  }

  return D2D1_BITMAP_INTERPOLATION_MODE_LINEAR;
}

static inline D2D1_ANTIALIAS_MODE D2DAAMode(const AntialiasMode &aMode)
{
  switch (aMode) {
  case AA_NONE:
    D2D1_ANTIALIAS_MODE_ALIASED;
  }

  return D2D1_ANTIALIAS_MODE_PER_PRIMITIVE;
}

static inline D2D1_MATRIX_3X2_F D2DMatrix(const Matrix &aTransform)
{
  return D2D1::Matrix3x2F(aTransform._11, aTransform._12,
                          aTransform._21, aTransform._22,
                          aTransform._31, aTransform._32);
}

static inline D2D1_COLOR_F D2DColor(const Color &aColor)
{
  return D2D1::ColorF(aColor.r, aColor.g, aColor.b, aColor.a);
}

static inline IntSize ToIntSize(const D2D1_SIZE_U &aSize)
{
  return IntSize(aSize.width, aSize.height);
}

static inline SurfaceFormat ToPixelFormat(const D2D1_PIXEL_FORMAT &aFormat)
{
  switch(aFormat.format) {
  case DXGI_FORMAT_A8_UNORM:
    return FORMAT_A8;
  case DXGI_FORMAT_B8G8R8A8_UNORM:
    if (aFormat.alphaMode == D2D1_ALPHA_MODE_IGNORE) {
      return FORMAT_B8G8R8X8;
    } else {
      return FORMAT_B8G8R8A8;
    }
  }

  return FORMAT_B8G8R8A8;
}

static inline Rect ToRect(const D2D1_RECT_F &aRect)
{
  return Rect(aRect.left, aRect.top, aRect.right - aRect.left, aRect.bottom - aRect.top);
}

static inline DXGI_FORMAT DXGIFormat(SurfaceFormat aFormat)
{
  switch (aFormat) {
  case FORMAT_B8G8R8A8:
    return DXGI_FORMAT_B8G8R8A8_UNORM;
  case FORMAT_B8G8R8X8:
    return DXGI_FORMAT_B8G8R8A8_UNORM;
  case FORMAT_A8:
    return DXGI_FORMAT_A8_UNORM;
  }

  return DXGI_FORMAT_UNKNOWN;
}

static inline D2D1_ALPHA_MODE AlphaMode(SurfaceFormat aFormat)
{
  switch (aFormat) {
  case FORMAT_B8G8R8X8:
    return D2D1_ALPHA_MODE_IGNORE;
  }

  return D2D1_ALPHA_MODE_PREMULTIPLIED;
}

static inline int BytesPerPixel(SurfaceFormat aFormat)
{
  switch (aFormat) {
  case FORMAT_A8:
    return 1;
  default:
    return 4;
  }
}

static bool IsPatternSupportedByD2D(const Pattern &aPattern)
{
  if (aPattern.GetType() != PATTERN_RADIAL_GRADIENT) {
    return false;
  }

  const RadialGradientPattern *pat =
    static_cast<const RadialGradientPattern*>(&aPattern);
  
  if (pat->mRadius1 != 0) {
    return true;
  }

  Point diff = pat->mCenter2 - pat->mCenter1;

  if (sqrt(diff.x * diff.x + diff.y * diff.y) >= pat->mRadius2) {
    // Inner point lies outside the circle.
    return true;
  }

  return false;
}

/**
 * This structure is used to pass rectangles to our shader constant. We can use
 * this for passing rectangular areas to SetVertexShaderConstant. In the format
 * of a 4 component float(x,y,width,height). Our vertex shader can then use
 * this to construct rectangular positions from the 0,0-1,1 quad that we source
 * it with.
 */
struct ShaderConstantRectD3D10
{
  float mX, mY, mWidth, mHeight;
  ShaderConstantRectD3D10(float aX, float aY, float aWidth, float aHeight)
    : mX(aX), mY(aY), mWidth(aWidth), mHeight(aHeight)
  { }

  // For easy passing to SetVertexShaderConstantF.
  operator float* () { return &mX; }
};

}
}

#endif /* MOZILLA_GFX_HELPERSD2D_H_ */
