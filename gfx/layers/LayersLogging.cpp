/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LayersLogging.h"
#include "nsPrintfCString.h"

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
AppendToString(nsACString& s, const gfxPattern::GraphicsFilter& f,
               const char* pfx, const char* sfx)
{
  s += pfx;
  switch (f) {
  case gfxPattern::FILTER_FAST:      s += "fast"; break;
  case gfxPattern::FILTER_GOOD:      s += "good"; break;
  case gfxPattern::FILTER_BEST:      s += "best"; break;
  case gfxPattern::FILTER_NEAREST:   s += "nearest"; break;
  case gfxPattern::FILTER_BILINEAR:  s += "bilinear"; break;
  case gfxPattern::FILTER_GAUSSIAN:  s += "gaussian"; break;
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
AppendToString(nsACString& s, const gfx3DMatrix& m,
               const char* pfx, const char* sfx)
{
  s += pfx;
  if (m.IsIdentity())
    s += "[ I ]";
  else {
    gfxMatrix matrix;
    if (m.Is2D(&matrix)) {
      s += nsPrintfCString(
        "[ %g %g; %g %g; %g %g; ]",
        matrix.xx, matrix.yx, matrix.xy, matrix.yy, matrix.x0, matrix.y0);
    } else {
      s += nsPrintfCString(
        "[ %g %g %g %g; %g %g %g %g; %g %g %g %g; %g %g %g %g; ]",
        m._11, m._12, m._13, m._14,
        m._21, m._22, m._23, m._24,
        m._31, m._32, m._33, m._34,
        m._41, m._42, m._43, m._44);
    }
  }
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
AppendToString(nsACString& s, const Point& p,
               const char* pfx, const char* sfx)
{
  s += pfx;
  s += nsPrintfCString("(x=%f, y=%f)", p.x, p.y);
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
AppendToString(nsACString& s, const Rect& r,
               const char* pfx, const char* sfx)
{
  s += pfx;
  s.AppendPrintf(
    "(x=%f, y=%f, w=%f, h=%f)",
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
  AppendToString(s, m.mScrollOffset, " viewportScroll=");
  AppendToString(s, m.mDisplayPort, " displayport=");
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
    case FILTER_LINEAR: s += "FILTER_LINEAR"; break;
    case FILTER_POINT: s += "FILTER_POINT"; break;
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
    AppendFlag(UseNearestFilter);
    AppendFlag(NeedsYFlip);
    AppendFlag(ForceSingleTile);
    AppendFlag(AllowRepeat);
    AppendFlag(NewTile);
    AppendFlag(HostRelease);

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
  case FORMAT_B8G8R8A8:  s += "FORMAT_B8G8R8A8"; break;
  case FORMAT_B8G8R8X8:  s += "FORMAT_B8G8R8X8"; break;
  case FORMAT_R8G8B8A8:  s += "FORMAT_R8G8B8A8"; break;
  case FORMAT_R8G8B8X8:  s += "FORMAT_R8G8B8X8"; break;
  case FORMAT_R5G6B5:    s += "FORMAT_R5G6B5"; break;
  case FORMAT_A8:        s += "FORMAT_A8"; break;
  case FORMAT_YUV:       s += "FORMAT_YUV"; break;
  case FORMAT_UNKNOWN:   s += "FORMAT_UNKNOWN"; break;
  }

  return s += sfx;
}

} // namespace
} // namespace
