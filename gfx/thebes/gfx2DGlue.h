/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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

inline Size ToSize(const gfxSize &aSize)
{
  return Size(Float(aSize.width), Float(aSize.height));
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

inline gfxRect ThebesRect(const IntRect &aRect)
{
  return gfxRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

inline gfxRect ThebesRect(const RectDouble &aRect)
{
  return gfxRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

inline gfxImageFormat SurfaceFormatToImageFormat(SurfaceFormat aFormat)
{
  switch (aFormat) {
  case SurfaceFormat::B8G8R8A8:
    return SurfaceFormat::A8R8G8B8_UINT32;
  case SurfaceFormat::B8G8R8X8:
    return SurfaceFormat::X8R8G8B8_UINT32;
  case SurfaceFormat::R5G6B5_UINT16:
    return SurfaceFormat::R5G6B5_UINT16;
  case SurfaceFormat::A8:
    return SurfaceFormat::A8;
  default:
    return SurfaceFormat::UNKNOWN;
  }
}

inline SurfaceFormat ImageFormatToSurfaceFormat(gfxImageFormat aFormat)
{
  switch (aFormat) {
  case SurfaceFormat::A8R8G8B8_UINT32:
    return SurfaceFormat::B8G8R8A8;
  case SurfaceFormat::X8R8G8B8_UINT32:
    return SurfaceFormat::B8G8R8X8;
  case SurfaceFormat::R5G6B5_UINT16:
    return SurfaceFormat::R5G6B5_UINT16;
  case SurfaceFormat::A8:
    return SurfaceFormat::A8;
  default:
  case SurfaceFormat::UNKNOWN:
    return SurfaceFormat::B8G8R8A8;
  }
}

inline gfxContentType ContentForFormat(const SurfaceFormat &aFormat)
{
  switch (aFormat) {
  case SurfaceFormat::R5G6B5_UINT16:
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
