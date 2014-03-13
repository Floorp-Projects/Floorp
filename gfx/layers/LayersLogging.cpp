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

nsACString&
AppendToString(nsACString& s, const void* p,
               const char* pfx, const char* sfx)
{
  s += pfx;
  s += nsPrintfCString("%p", p);
  return s += sfx;
}

nsACString&
AppendToString(nsACString& s, const GraphicsFilter& f,
               const char* pfx, const char* sfx)
{
  s += pfx;
  switch (f) {
  case GraphicsFilter::FILTER_FAST:      s += "fast"; break;
  case GraphicsFilter::FILTER_GOOD:      s += "good"; break;
  case GraphicsFilter::FILTER_BEST:      s += "best"; break;
  case GraphicsFilter::FILTER_NEAREST:   s += "nearest"; break;
  case GraphicsFilter::FILTER_BILINEAR:  s += "bilinear"; break;
  case GraphicsFilter::FILTER_GAUSSIAN:  s += "gaussian"; break;
  default:
    NS_ERROR("unknown filter type");
    s += "???";
  }
  return s += sfx;
}

nsACString&
AppendToString(nsACString& s, FrameMetrics::ViewID n,
               const char* pfx, const char* sfx)
{
  s += pfx;
  s.AppendInt(n);
  return s += sfx;
}

nsACString&
AppendToString(nsACString& s, const gfxRGBA& c,
               const char* pfx, const char* sfx)
{
  s += pfx;
  s += nsPrintfCString(
    "rgba(%d, %d, %d, %g)",
    uint8_t(c.r*255.0), uint8_t(c.g*255.0), uint8_t(c.b*255.0), c.a);
  return s += sfx;
}

nsACString&
AppendToString(nsACString& s, const nsIntPoint& p,
               const char* pfx, const char* sfx)
{
  s += pfx;
  s += nsPrintfCString("(x=%d, y=%d)", p.x, p.y);
  return s += sfx;
}

nsACString&
AppendToString(nsACString& s, const nsIntRect& r,
               const char* pfx, const char* sfx)
{
  s += pfx;
  s += nsPrintfCString(
    "(x=%d, y=%d, w=%d, h=%d)",
    r.x, r.y, r.width, r.height);
  return s += sfx;
}

nsACString&
AppendToString(nsACString& s, const nsIntRegion& r,
               const char* pfx, const char* sfx)
{
  s += pfx;

  nsIntRegionRectIterator it(r);
  s += "< ";
  while (const nsIntRect* sr = it.Next())
    AppendToString(s, *sr) += "; ";
  s += ">";

  return s += sfx;
}

nsACString&
AppendToString(nsACString& s, const nsIntSize& sz,
               const char* pfx, const char* sfx)
{
  s += pfx;
  s += nsPrintfCString("(w=%d, h=%d)", sz.width, sz.height);
  return s += sfx;
}

nsACString&
AppendToString(nsACString& s, const FrameMetrics& m,
               const char* pfx, const char* sfx)
{
  s += pfx;
  AppendToString(s, m.mViewport, "{ viewport=");
  AppendToString(s, m.GetScrollOffset(), " viewportScroll=");
  AppendToString(s, m.mDisplayPort, " displayport=");
  AppendToString(s, m.mScrollableRect, " scrollableRect=");
  AppendToString(s, m.mScrollId, " scrollId=", " }");
  return s += sfx;
}

nsACString&
AppendToString(nsACString& s, const IntSize& size,
               const char* pfx, const char* sfx)
{
  s += pfx;
  s += nsPrintfCString(
    "(width=%d, height=%d)",
    size.width, size.height);
  return s += sfx;
}

nsACString&
AppendToString(nsACString& s, const Matrix4x4& m,
               const char* pfx, const char* sfx)
{
  s += pfx;
  if (m.Is2D()) {
    Matrix matrix = m.As2D();
    if (matrix.IsIdentity()) {
      s += "[ I ]";
      return s += sfx;
    }
    s += nsPrintfCString(
      "[ %g %g; %g %g; %g %g; ]",
      matrix._11, matrix._12, matrix._21, matrix._22, matrix._31, matrix._32);
  } else {
    s += nsPrintfCString(
      "[ %g %g %g %g; %g %g %g %g; %g %g %g %g; %g %g %g %g; ]",
      m._11, m._12, m._13, m._14,
      m._21, m._22, m._23, m._24,
      m._31, m._32, m._33, m._34,
      m._41, m._42, m._43, m._44);
  }
  return s += sfx;
}

nsACString&
AppendToString(nsACString& s, const Filter filter,
               const char* pfx, const char* sfx)
{
  s += pfx;

  switch (filter) {
    case Filter::GOOD: s += "Filter::GOOD"; break;
    case Filter::LINEAR: s += "Filter::LINEAR"; break;
    case Filter::POINT: s += "Filter::POINT"; break;
  }
  return s += sfx;
}

nsACString&
AppendToString(nsACString& s, TextureFlags flags,
               const char* pfx, const char* sfx)
{
  s += pfx;
  if (!flags) {
    s += "NoFlags";
  } else {

#define AppendFlag(test) \
{ \
  if (flags & test) { \
    if (previous) { \
      s += "|"; \
    } \
    s += #test; \
    previous = true; \
  } \
}
    bool previous = false;
    AppendFlag(TEXTURE_USE_NEAREST_FILTER);
    AppendFlag(TEXTURE_NEEDS_Y_FLIP);
    AppendFlag(TEXTURE_DISALLOW_BIGIMAGE);
    AppendFlag(TEXTURE_ALLOW_REPEAT);
    AppendFlag(TEXTURE_NEW_TILE);

#undef AppendFlag
  }
  return s += sfx;
}

nsACString&
AppendToString(nsACString& s, mozilla::gfx::SurfaceFormat format,
               const char* pfx, const char* sfx)
{
  s += pfx;
  switch (format) {
  case SurfaceFormat::B8G8R8A8:  s += "SurfaceFormat::B8G8R8A8"; break;
  case SurfaceFormat::B8G8R8X8:  s += "SurfaceFormat::B8G8R8X8"; break;
  case SurfaceFormat::R8G8B8A8:  s += "SurfaceFormat::R8G8B8A8"; break;
  case SurfaceFormat::R8G8B8X8:  s += "SurfaceFormat::R8G8B8X8"; break;
  case SurfaceFormat::R5G6B5:    s += "SurfaceFormat::R5G6B5"; break;
  case SurfaceFormat::A8:        s += "SurfaceFormat::A8"; break;
  case SurfaceFormat::YUV:       s += "SurfaceFormat::YUV"; break;
  case SurfaceFormat::UNKNOWN:   s += "SurfaceFormat::UNKNOWN"; break;
  }

  return s += sfx;
}

} // namespace
} // namespace
