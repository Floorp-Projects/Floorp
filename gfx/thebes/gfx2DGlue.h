/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_2D_GLUE_H
#define GFX_2D_GLUE_H


#include "gfxPlatform.h"
#include "gfxRect.h"
#include "gfxMatrix.h"
#include "gfxContext.h"
#include "mozilla/gfx/Matrix.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/gfx/2D.h"
#include "gfxColor.h"

namespace mozilla {
namespace gfx {

inline Rect ToRect(const gfxRect &aRect)
{
  return Rect(Float(aRect.x), Float(aRect.y),
              Float(aRect.width), Float(aRect.height));
}

inline RectDouble ToRectDouble(const gfxRect &aRect)
{
  return RectDouble(aRect.x, aRect.y, aRect.width, aRect.height);
}

inline Rect ToRect(const IntRect &aRect)
{
  return Rect(aRect.x, aRect.y, aRect.width, aRect.height);
}

inline Color ToColor(const gfxRGBA &aRGBA)
{
  return Color(Float(aRGBA.r), Float(aRGBA.g),
               Float(aRGBA.b), Float(aRGBA.a));
}

inline gfxRGBA ThebesColor(const Color &aColor)
{
  return gfxRGBA(aColor.r, aColor.g, aColor.b, aColor.a);
}

inline Matrix ToMatrix(const gfxMatrix &aMatrix)
{
  return Matrix(Float(aMatrix._11), Float(aMatrix._12), Float(aMatrix._21),
                Float(aMatrix._22), Float(aMatrix._31), Float(aMatrix._32));
}

inline gfxMatrix ThebesMatrix(const Matrix &aMatrix)
{
  return gfxMatrix(aMatrix._11, aMatrix._12, aMatrix._21,
                   aMatrix._22, aMatrix._31, aMatrix._32);
}

inline Point ToPoint(const gfxPoint &aPoint)
{
  return Point(Float(aPoint.x), Float(aPoint.y));
}

inline IntMargin ToIntMargin(const nsIntMargin& aMargin)
{
  return IntMargin(aMargin.top, aMargin.right, aMargin.bottom, aMargin.left);
}

inline Size ToSize(const gfxSize &aSize)
{
  return Size(Float(aSize.width), Float(aSize.height));
}

inline Filter ToFilter(GraphicsFilter aFilter)
{
  switch (aFilter) {
  case GraphicsFilter::FILTER_NEAREST:
    return Filter::POINT;
  case GraphicsFilter::FILTER_GOOD:
    return Filter::GOOD;
  default:
    return Filter::LINEAR;
  }
}

inline GraphicsFilter ThebesFilter(Filter aFilter)
{
  switch (aFilter) {
  case Filter::POINT:
    return GraphicsFilter::FILTER_NEAREST;
  default:
    return GraphicsFilter::FILTER_BEST;
  }
}

inline ExtendMode ToExtendMode(gfxPattern::GraphicsExtend aExtend)
{
  switch (aExtend) {
  case gfxPattern::EXTEND_REPEAT:
    return ExtendMode::REPEAT;
  case gfxPattern::EXTEND_REFLECT:
    return ExtendMode::REFLECT;
  default:
    return ExtendMode::CLAMP;
  }
}

inline gfxPattern::GraphicsExtend ThebesExtend(ExtendMode aExtend)
{
  switch (aExtend) {
  case ExtendMode::REPEAT:
    return gfxPattern::EXTEND_REPEAT;
  case ExtendMode::REFLECT:
    return gfxPattern::EXTEND_REFLECT;
  default:
    return gfxPattern::EXTEND_PAD;
  }
}

inline gfxPoint ThebesPoint(const Point &aPoint)
{
  return gfxPoint(aPoint.x, aPoint.y);
}

inline gfxSize ThebesSize(const Size &aSize)
{
  return gfxSize(aSize.width, aSize.height);
}

inline gfxRect ThebesRect(const Rect &aRect)
{
  return gfxRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

inline gfxRect ThebesRect(const RectDouble &aRect)
{
  return gfxRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

inline gfxRGBA ThebesRGBA(const Color &aColor)
{
  return gfxRGBA(aColor.r, aColor.g, aColor.b, aColor.a);
}

inline gfxImageFormat SurfaceFormatToImageFormat(SurfaceFormat aFormat)
{
  switch (aFormat) {
  case SurfaceFormat::B8G8R8A8:
    return gfxImageFormat::ARGB32;
  case SurfaceFormat::B8G8R8X8:
    return gfxImageFormat::RGB24;
  case SurfaceFormat::R5G6B5:
    return gfxImageFormat::RGB16_565;
  case SurfaceFormat::A8:
    return gfxImageFormat::A8;
  default:
    return gfxImageFormat::Unknown;
  }
}

inline SurfaceFormat ImageFormatToSurfaceFormat(gfxImageFormat aFormat)
{
  switch (aFormat) {
  case gfxImageFormat::ARGB32:
    return SurfaceFormat::B8G8R8A8;
  case gfxImageFormat::RGB24:
    return SurfaceFormat::B8G8R8X8;
  case gfxImageFormat::RGB16_565:
    return SurfaceFormat::R5G6B5;
  case gfxImageFormat::A8:
    return SurfaceFormat::A8;
  default:
  case gfxImageFormat::Unknown:
    return SurfaceFormat::B8G8R8A8;
  }
}

inline gfxContentType ContentForFormat(const SurfaceFormat &aFormat)
{
  switch (aFormat) {
  case SurfaceFormat::R5G6B5:
  case SurfaceFormat::B8G8R8X8:
  case SurfaceFormat::R8G8B8X8:
    return gfxContentType::COLOR;
  case SurfaceFormat::A8:
    return gfxContentType::ALPHA;
  case SurfaceFormat::B8G8R8A8:
  case SurfaceFormat::R8G8B8A8:
  default:
    return gfxContentType::COLOR_ALPHA;
  }
}

} // namespace gfx
} // namespace mozilla

#endif
