/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_2D_GLUE_H
#define GFX_2D_GLUE_H


#include "gfxPlatform.h"
#include "gfxRect.h"
#include "gfxMatrix.h"
#include "gfx3DMatrix.h"
#include "gfxContext.h"
#include "mozilla/gfx/Matrix.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/gfx/2D.h"
#include "gfxColor.h"

namespace mozilla {
namespace gfx {
class DrawTarget;
class SourceSurface;
class ScaledFont;
}
}

namespace mozilla {
namespace gfx {

inline Rect ToRect(const gfxRect &aRect)
{
  return Rect(Float(aRect.x), Float(aRect.y),
              Float(aRect.width), Float(aRect.height));
}

inline Rect ToRect(const nsIntRect &aRect)
{
  return Rect(aRect.x, aRect.y, aRect.width, aRect.height);
}

inline IntRect ToIntRect(const nsIntRect &aRect)
{
  return IntRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

inline Color ToColor(const gfxRGBA &aRGBA)
{
  return Color(Float(aRGBA.r), Float(aRGBA.g),
               Float(aRGBA.b), Float(aRGBA.a));
}

inline gfxRGBA ThebesColor(Color &aColor)
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

inline IntPoint ToIntPoint(const nsIntPoint &aPoint)
{
  return IntPoint(aPoint.x, aPoint.y);
}

inline IntMargin ToIntMargin(const nsIntMargin& aMargin)
{
  return IntMargin(aMargin.top, aMargin.right, aMargin.bottom, aMargin.left);
}

inline nsIntPoint ThebesIntPoint(const IntPoint &aPoint)
{
  return nsIntPoint(aPoint.x, aPoint.y);
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

inline gfxPattern::GraphicsPatternType
ThebesPatternType(PatternType aType)
{
  switch (aType) {
  case PatternType::SURFACE:
    return gfxPattern::PATTERN_SURFACE;
  case PatternType::LINEAR_GRADIENT:
    return gfxPattern::PATTERN_LINEAR;
  case PatternType::RADIAL_GRADIENT:
    return gfxPattern::PATTERN_RADIAL;
  default:
    return gfxPattern::PATTERN_SOLID;
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

inline nsIntRect ThebesIntRect(const IntRect &aRect)
{
  return nsIntRect(aRect.x, aRect.y, aRect.width, aRect.height);
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

inline CompositionOp CompositionOpForOp(gfxContext::GraphicsOperator aOp)
{
  switch (aOp) {
  case gfxContext::OPERATOR_ADD:
    return CompositionOp::OP_ADD;
  case gfxContext::OPERATOR_ATOP:
    return CompositionOp::OP_ATOP;
  case gfxContext::OPERATOR_IN:
    return CompositionOp::OP_IN;
  case gfxContext::OPERATOR_OUT:
    return CompositionOp::OP_OUT;
  case gfxContext::OPERATOR_SOURCE:
    return CompositionOp::OP_SOURCE;
  case gfxContext::OPERATOR_DEST_IN:
    return CompositionOp::OP_DEST_IN;
  case gfxContext::OPERATOR_DEST_OUT:
    return CompositionOp::OP_DEST_OUT;
  case gfxContext::OPERATOR_DEST_ATOP:
    return CompositionOp::OP_DEST_ATOP;
  case gfxContext::OPERATOR_XOR:
    return CompositionOp::OP_XOR;
  case gfxContext::OPERATOR_MULTIPLY:
    return CompositionOp::OP_MULTIPLY;
  case gfxContext::OPERATOR_SCREEN:
    return CompositionOp::OP_SCREEN;
  case gfxContext::OPERATOR_OVERLAY:
    return CompositionOp::OP_OVERLAY;
  case gfxContext::OPERATOR_DARKEN:
    return CompositionOp::OP_DARKEN;
  case gfxContext::OPERATOR_LIGHTEN:
    return CompositionOp::OP_LIGHTEN;
  case gfxContext::OPERATOR_COLOR_DODGE:
    return CompositionOp::OP_COLOR_DODGE;
  case gfxContext::OPERATOR_COLOR_BURN:
    return CompositionOp::OP_COLOR_BURN;
  case gfxContext::OPERATOR_HARD_LIGHT:
    return CompositionOp::OP_HARD_LIGHT;
  case gfxContext::OPERATOR_SOFT_LIGHT:
    return CompositionOp::OP_SOFT_LIGHT;
  case gfxContext::OPERATOR_DIFFERENCE:
    return CompositionOp::OP_DIFFERENCE;
  case gfxContext::OPERATOR_EXCLUSION:
    return CompositionOp::OP_EXCLUSION;
  case gfxContext::OPERATOR_HUE:
    return CompositionOp::OP_HUE;
  case gfxContext::OPERATOR_SATURATION:
    return CompositionOp::OP_SATURATION;
  case gfxContext::OPERATOR_COLOR:
    return CompositionOp::OP_COLOR;
  case gfxContext::OPERATOR_LUMINOSITY:
    return CompositionOp::OP_LUMINOSITY;
  default:
    return CompositionOp::OP_OVER;
  }
}

inline gfxContext::GraphicsOperator ThebesOp(CompositionOp aOp)
{
  switch (aOp) {
  case CompositionOp::OP_ADD:
    return gfxContext::OPERATOR_ADD;
  case CompositionOp::OP_ATOP:
    return gfxContext::OPERATOR_ATOP;
  case CompositionOp::OP_IN:
    return gfxContext::OPERATOR_IN;
  case CompositionOp::OP_OUT:
    return gfxContext::OPERATOR_OUT;
  case CompositionOp::OP_SOURCE:
    return gfxContext::OPERATOR_SOURCE;
  case CompositionOp::OP_DEST_IN:
    return gfxContext::OPERATOR_DEST_IN;
  case CompositionOp::OP_DEST_OUT:
    return gfxContext::OPERATOR_DEST_OUT;
  case CompositionOp::OP_DEST_ATOP:
    return gfxContext::OPERATOR_DEST_ATOP;
  case CompositionOp::OP_XOR:
    return gfxContext::OPERATOR_XOR;
  case CompositionOp::OP_MULTIPLY:
    return gfxContext::OPERATOR_MULTIPLY;
  case CompositionOp::OP_SCREEN:
    return gfxContext::OPERATOR_SCREEN;
  case CompositionOp::OP_OVERLAY:
    return gfxContext::OPERATOR_OVERLAY;
  case CompositionOp::OP_DARKEN:
    return gfxContext::OPERATOR_DARKEN;
  case CompositionOp::OP_LIGHTEN:
    return gfxContext::OPERATOR_LIGHTEN;
  case CompositionOp::OP_COLOR_DODGE:
    return gfxContext::OPERATOR_COLOR_DODGE;
  case CompositionOp::OP_COLOR_BURN:
    return gfxContext::OPERATOR_COLOR_BURN;
  case CompositionOp::OP_HARD_LIGHT:
    return gfxContext::OPERATOR_HARD_LIGHT;
  case CompositionOp::OP_SOFT_LIGHT:
    return gfxContext::OPERATOR_SOFT_LIGHT;
  case CompositionOp::OP_DIFFERENCE:
    return gfxContext::OPERATOR_DIFFERENCE;
  case CompositionOp::OP_EXCLUSION:
    return gfxContext::OPERATOR_EXCLUSION;
  case CompositionOp::OP_HUE:
    return gfxContext::OPERATOR_HUE;
  case CompositionOp::OP_SATURATION:
    return gfxContext::OPERATOR_SATURATION;
  case CompositionOp::OP_COLOR:
    return gfxContext::OPERATOR_COLOR;
  case CompositionOp::OP_LUMINOSITY:
    return gfxContext::OPERATOR_LUMINOSITY;
  default:
    return gfxContext::OPERATOR_OVER;
  }
}

inline Matrix4x4
ToMatrix4x4(const gfx3DMatrix& aIn)
{
  Matrix4x4 m;
  m._11 = aIn._11;
  m._12 = aIn._12;
  m._13 = aIn._13;
  m._14 = aIn._14;
  m._21 = aIn._21;
  m._22 = aIn._22;
  m._23 = aIn._23;
  m._24 = aIn._24;
  m._31 = aIn._31;
  m._32 = aIn._32;
  m._33 = aIn._33;
  m._34 = aIn._34;
  m._41 = aIn._41;
  m._42 = aIn._42;
  m._43 = aIn._43;
  m._44 = aIn._44;
  return m;
}

inline gfx3DMatrix
To3DMatrix(const Matrix4x4& aIn)
{
  gfx3DMatrix m;
  m._11 = aIn._11;
  m._12 = aIn._12;
  m._13 = aIn._13;
  m._14 = aIn._14;
  m._21 = aIn._21;
  m._22 = aIn._22;
  m._23 = aIn._23;
  m._24 = aIn._24;
  m._31 = aIn._31;
  m._32 = aIn._32;
  m._33 = aIn._33;
  m._34 = aIn._34;
  m._41 = aIn._41;
  m._42 = aIn._42;
  m._43 = aIn._43;
  m._44 = aIn._44;
  return m;
}

}
}

#endif
