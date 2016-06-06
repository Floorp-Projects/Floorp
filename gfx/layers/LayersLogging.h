/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_LAYERSLOGGING_H
#define GFX_LAYERSLOGGING_H

#include "FrameMetrics.h"               // for FrameMetrics, etc
#include "mozilla/gfx/Matrix.h"         // for Matrix4x4
#include "mozilla/gfx/Point.h"          // for IntSize, etc
#include "mozilla/gfx/Types.h"          // for SamplingFilter, SurfaceFormat
#include "mozilla/layers/CompositorTypes.h"  // for TextureFlags
#include "nsAString.h"
#include "nsPrintfCString.h"            // for nsPrintfCString
#include "nsRegion.h"                   // for nsRegion, nsIntRegion
#include "nscore.h"                     // for nsACString, etc

namespace mozilla {
namespace gfx {
template <class units, class F> struct RectTyped;
} // namespace gfx

enum class ImageFormat;

namespace layers {

void
AppendToString(std::stringstream& aStream, const void* p,
               const char* pfx="", const char* sfx="");

void
AppendToString(std::stringstream& aStream, FrameMetrics::ViewID n,
               const char* pfx="", const char* sfx="");

void
AppendToString(std::stringstream& aStream, const gfx::Color& c,
               const char* pfx="", const char* sfx="");

void
AppendToString(std::stringstream& aStream, const nsPoint& p,
               const char* pfx="", const char* sfx="");

void
AppendToString(std::stringstream& aStream, const nsRect& r,
               const char* pfx="", const char* sfx="");

template<class T>
void
AppendToString(std::stringstream& aStream, const mozilla::gfx::PointTyped<T>& p,
               const char* pfx="", const char* sfx="")
{
  aStream << pfx << p << sfx;
}

template<class T>
void
AppendToString(std::stringstream& aStream, const mozilla::gfx::IntPointTyped<T>& p,
               const char* pfx="", const char* sfx="")
{
  aStream << pfx << p << sfx;
}

template<class T>
void
AppendToString(std::stringstream& aStream, const mozilla::gfx::RectTyped<T>& r,
               const char* pfx="", const char* sfx="")
{
  aStream << pfx;
  aStream << nsPrintfCString(
    "(x=%f, y=%f, w=%f, h=%f)",
    r.x, r.y, r.width, r.height).get();
  aStream << sfx;
}

template<class T>
void
AppendToString(std::stringstream& aStream, const mozilla::gfx::IntRectTyped<T>& r,
               const char* pfx="", const char* sfx="")
{
  aStream << pfx;
  aStream << nsPrintfCString(
    "(x=%d, y=%d, w=%d, h=%d)",
    r.x, r.y, r.width, r.height).get();
  aStream << sfx;
}

void
AppendToString(std::stringstream& aStream, const nsRegion& r,
               const char* pfx="", const char* sfx="");

void
AppendToString(std::stringstream& aStream, const nsIntRegion& r,
               const char* pfx="", const char* sfx="");

template <typename units>
void
AppendToString(std::stringstream& aStream, const mozilla::gfx::IntRegionTyped<units>& r,
               const char* pfx="", const char* sfx="")
{
  aStream << pfx;

  aStream << "< ";
  for (auto iter = r.RectIter(); !iter.Done(); iter.Next()) {
    AppendToString(aStream, iter.Get());
    aStream << "; ";
  }
  aStream << ">";

  aStream << sfx;
}

void
AppendToString(std::stringstream& aStream, const EventRegions& e,
               const char* pfx="", const char* sfx="");

void
AppendToString(std::stringstream& aStream, const ScrollMetadata& m,
               const char* pfx="", const char* sfx="");

void
AppendToString(std::stringstream& aStream, const FrameMetrics& m,
               const char* pfx="", const char* sfx="", bool detailed = false);

void
AppendToString(std::stringstream& aStream, const ScrollableLayerGuid& s,
               const char* pfx="", const char* sfx="");

void
AppendToString(std::stringstream& aStream, const ZoomConstraints& z,
               const char* pfx="", const char* sfx="");

template<class T>
void
AppendToString(std::stringstream& aStream, const mozilla::gfx::MarginTyped<T>& m,
               const char* pfx="", const char* sfx="")
{
  aStream << pfx;
  aStream << nsPrintfCString(
    "(l=%f, t=%f, r=%f, b=%f)",
    m.left, m.top, m.right, m.bottom).get();
  aStream << sfx;
}

template<class T>
void
AppendToString(std::stringstream& aStream, const mozilla::gfx::SizeTyped<T>& sz,
               const char* pfx="", const char* sfx="")
{
  aStream << pfx;
  aStream << nsPrintfCString(
    "(w=%f, h=%f)",
    sz.width, sz.height).get();
  aStream << sfx;
}

template<class T>
void
AppendToString(std::stringstream& aStream, const mozilla::gfx::IntSizeTyped<T>& sz,
               const char* pfx="", const char* sfx="")
{
  aStream << pfx;
  aStream << nsPrintfCString(
    "(w=%d, h=%d)",
    sz.width, sz.height).get();
  aStream << sfx;
}

template<class src, class dst>
void
AppendToString(std::stringstream& aStream, const mozilla::gfx::ScaleFactors2D<src, dst>& scale,
               const char* pfx="", const char* sfx="")
{
  aStream << pfx;
  std::streamsize oldPrecision = aStream.precision(3);
  if (scale.AreScalesSame()) {
    aStream << scale.xScale;
  } else {
    aStream << '(' << scale.xScale << ',' << scale.yScale << ')';
  }
  aStream.precision(oldPrecision);
  aStream << sfx;
}

void
AppendToString(std::stringstream& aStream, const mozilla::gfx::Matrix& m,
               const char* pfx="", const char* sfx="");

template<class SourceUnits, class TargetUnits>
void
AppendToString(std::stringstream& aStream, const mozilla::gfx::Matrix4x4Typed<SourceUnits, TargetUnits>& m,
               const char* pfx="", const char* sfx="")
{
  if (m.Is2D()) {
    mozilla::gfx::Matrix matrix = m.As2D();
    AppendToString(aStream, matrix, pfx, sfx);
    return;
  }

  aStream << pfx;
  aStream << nsPrintfCString(
    "[ %g %g %g %g; %g %g %g %g; %g %g %g %g; %g %g %g %g; ]",
    m._11, m._12, m._13, m._14,
    m._21, m._22, m._23, m._24,
    m._31, m._32, m._33, m._34,
    m._41, m._42, m._43, m._44).get();
  aStream << sfx;
}

void
AppendToString(std::stringstream& aStream, const mozilla::gfx::Matrix5x4& m,
               const char* pfx="", const char* sfx="");

void
AppendToString(std::stringstream& aStream,
               const mozilla::gfx::SamplingFilter samplingFilter,
               const char* pfx="", const char* sfx="");

void
AppendToString(std::stringstream& aStream, mozilla::layers::TextureFlags flags,
               const char* pfx="", const char* sfx="");

void
AppendToString(std::stringstream& aStream, mozilla::gfx::SurfaceFormat format,
               const char* pfx="", const char* sfx="");

void
AppendToString(std::stringstream& aStream, gfx::SurfaceType format,
               const char* pfx="", const char* sfx="");

void
AppendToString(std::stringstream& aStream, ImageFormat format,
               const char* pfx="", const char* sfx="");

// Sometimes, you just want a string from a single value.
template <typename T>
std::string
Stringify(const T& obj)
{
  std::stringstream ss;
  AppendToString(ss, obj);
  return ss.str();
}

} // namespace layers
} // namespace mozilla

// versions of printf_stderr and fprintf_stderr that deal with line
// truncation on android by printing individual lines out of the
// stringstream as separate calls to logcat.
void print_stderr(std::stringstream& aStr);
void fprint_stderr(FILE* aFile, std::stringstream& aStr);

#endif /* GFX_LAYERSLOGGING_H */
