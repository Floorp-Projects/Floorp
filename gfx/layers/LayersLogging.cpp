/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LayersLogging.h"
#include <stdint.h>                     // for uint8_t
#include "gfx3DMatrix.h"                // for gfx3DMatrix
#include "gfxColor.h"                   // for gfxRGBA
#include "mozilla/gfx/Matrix.h"         // for Matrix4x4, Matrix
#include "nsDebug.h"                    // for NS_ERROR
#include "nsPoint.h"                    // for nsIntPoint
#include "nsRect.h"                     // for nsIntRect
#include "nsSize.h"                     // for nsIntSize

using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

void
AppendToString(std::stringstream& aStream, const void* p,
               const char* pfx, const char* sfx)
{
  aStream << pfx;
  aStream << nsPrintfCString("%p", p).get();
  aStream << sfx;
}

void
AppendToString(std::stringstream& aStream, const GraphicsFilter& f,
               const char* pfx, const char* sfx)
{
  aStream << pfx;
  switch (f) {
  case GraphicsFilter::FILTER_FAST:      aStream << "fast"; break;
  case GraphicsFilter::FILTER_GOOD:      aStream << "good"; break;
  case GraphicsFilter::FILTER_BEST:      aStream << "best"; break;
  case GraphicsFilter::FILTER_NEAREST:   aStream << "nearest"; break;
  case GraphicsFilter::FILTER_BILINEAR:  aStream << "bilinear"; break;
  case GraphicsFilter::FILTER_GAUSSIAN:  aStream << "gaussian"; break;
  default:
    NS_ERROR("unknown filter type");
    aStream << "???";
  }
  aStream << sfx;
}

void
AppendToString(std::stringstream& aStream, FrameMetrics::ViewID n,
               const char* pfx, const char* sfx)
{
  aStream << pfx;
  aStream << n;
  aStream << sfx;
}

void
AppendToString(std::stringstream& aStream, const gfxRGBA& c,
               const char* pfx, const char* sfx)
{
  aStream << pfx;
  aStream << nsPrintfCString(
    "rgba(%d, %d, %d, %g)",
    uint8_t(c.r*255.0), uint8_t(c.g*255.0), uint8_t(c.b*255.0), c.a).get();
  aStream << sfx;
}

void
AppendToString(std::stringstream& aStream, const nsIntPoint& p,
               const char* pfx, const char* sfx)
{
  aStream << pfx;
  aStream << nsPrintfCString("(x=%d, y=%d)", p.x, p.y).get();
  aStream << sfx;
}

void
AppendToString(std::stringstream& aStream, const nsIntRect& r,
               const char* pfx, const char* sfx)
{
  aStream << pfx;
  aStream << nsPrintfCString(
    "(x=%d, y=%d, w=%d, h=%d)",
    r.x, r.y, r.width, r.height).get();
  aStream << sfx;
}

void
AppendToString(std::stringstream& aStream, const nsIntRegion& r,
               const char* pfx, const char* sfx)
{
  aStream << pfx;

  nsIntRegionRectIterator it(r);
  aStream << "< ";
  while (const nsIntRect* sr = it.Next()) {
    AppendToString(aStream, *sr);
    aStream << "; ";
  }
  aStream << ">";

  aStream << sfx;
}

void
AppendToString(std::stringstream& aStream, const nsIntSize& sz,
               const char* pfx, const char* sfx)
{
  aStream << pfx;
  aStream << nsPrintfCString("(w=%d, h=%d)", sz.width, sz.height).get();
  aStream << sfx;
}

void
AppendToString(std::stringstream& aStream, const FrameMetrics& m,
               const char* pfx, const char* sfx, bool detailed)
{
  aStream << pfx;
  AppendToString(aStream, m.mCompositionBounds, "{ cb=");
  AppendToString(aStream, m.mScrollableRect, " sr=");
  AppendToString(aStream, m.GetScrollOffset(), " s=");
  AppendToString(aStream, m.mDisplayPort, " dp=");
  AppendToString(aStream, m.mCriticalDisplayPort, " cdp=");
  if (!detailed) {
    aStream << nsPrintfCString(" z=%.3f }", m.GetZoom().scale).get();
  } else {
    AppendToString(aStream, m.GetDisplayPortMargins(), " dpm=");
    aStream << nsPrintfCString(" um=%d", m.GetUseDisplayPortMargins()).get();
    AppendToString(aStream, m.GetRootCompositionSize(), " rcs=");
    AppendToString(aStream, m.mViewport, " v=");
    aStream << nsPrintfCString(" z=(ld=%.3f r=%.3f cr=%.3f z=%.3f ts=%.3f)",
            m.mDevPixelsPerCSSPixel.scale, m.mResolution.scale,
            m.mCumulativeResolution.scale, m.GetZoom().scale,
            m.mTransformScale.scale).get();
    aStream << nsPrintfCString(" u=(%d %lu)",
            m.GetScrollOffsetUpdated(), m.GetScrollGeneration()).get();
    aStream << nsPrintfCString(" i=(%ld %lld) }",
            m.GetPresShellId(), m.GetScrollId()).get();
  }
  aStream << sfx;
}

void
AppendToString(std::stringstream& aStream, const Matrix4x4& m,
               const char* pfx, const char* sfx)
{
  aStream << pfx;
  if (m.Is2D()) {
    Matrix matrix = m.As2D();
    if (matrix.IsIdentity()) {
      aStream << "[ I ]";
      aStream << sfx;
      return;
    }
    aStream << nsPrintfCString(
      "[ %g %g; %g %g; %g %g; ]",
      matrix._11, matrix._12, matrix._21, matrix._22, matrix._31, matrix._32).get();
  } else {
    aStream << nsPrintfCString(
      "[ %g %g %g %g; %g %g %g %g; %g %g %g %g; %g %g %g %g; ]",
      m._11, m._12, m._13, m._14,
      m._21, m._22, m._23, m._24,
      m._31, m._32, m._33, m._34,
      m._41, m._42, m._43, m._44).get();
  }
  aStream << sfx;
}

void
AppendToString(std::stringstream& aStream, const Filter filter,
               const char* pfx, const char* sfx)
{
  aStream << pfx;

  switch (filter) {
    case Filter::GOOD: aStream << "Filter::GOOD"; break;
    case Filter::LINEAR: aStream << "Filter::LINEAR"; break;
    case Filter::POINT: aStream << "Filter::POINT"; break;
  }
  aStream << sfx;
}

void
AppendToString(std::stringstream& aStream, TextureFlags flags,
               const char* pfx, const char* sfx)
{
  aStream << pfx;
  if (flags == TextureFlags::NO_FLAGS) {
    aStream << "NoFlags";
  } else {

#define AppendFlag(test) \
{ \
  if (!!(flags & test)) { \
    if (previous) { \
      aStream << "|"; \
    } \
    aStream << #test; \
    previous = true; \
  } \
}
    bool previous = false;
    AppendFlag(TextureFlags::USE_NEAREST_FILTER);
    AppendFlag(TextureFlags::NEEDS_Y_FLIP);
    AppendFlag(TextureFlags::DISALLOW_BIGIMAGE);
    AppendFlag(TextureFlags::ALLOW_REPEAT);
    AppendFlag(TextureFlags::NEW_TILE);

#undef AppendFlag
  }
  aStream << sfx;
}

void
AppendToString(std::stringstream& aStream, mozilla::gfx::SurfaceFormat format,
               const char* pfx, const char* sfx)
{
  aStream << pfx;
  switch (format) {
  case SurfaceFormat::B8G8R8A8:  aStream << "SurfaceFormat::B8G8R8A8"; break;
  case SurfaceFormat::B8G8R8X8:  aStream << "SurfaceFormat::B8G8R8X8"; break;
  case SurfaceFormat::R8G8B8A8:  aStream << "SurfaceFormat::R8G8B8A8"; break;
  case SurfaceFormat::R8G8B8X8:  aStream << "SurfaceFormat::R8G8B8X8"; break;
  case SurfaceFormat::R5G6B5:    aStream << "SurfaceFormat::R5G6B5"; break;
  case SurfaceFormat::A8:        aStream << "SurfaceFormat::A8"; break;
  case SurfaceFormat::YUV:       aStream << "SurfaceFormat::YUV"; break;
  case SurfaceFormat::UNKNOWN:   aStream << "SurfaceFormat::UNKNOWN"; break;
  }

  aStream << sfx;
}

} // namespace
} // namespace
