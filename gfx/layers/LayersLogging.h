/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_LAYERSLOGGING_H
#define GFX_LAYERSLOGGING_H

#include "FrameMetrics.h"             // for FrameMetrics
#include "mozilla/gfx/Matrix.h"       // for Matrix4x4
#include "mozilla/gfx/Point.h"        // for IntSize, etc
#include "mozilla/gfx/TiledRegion.h"  // for TiledRegion
#include "mozilla/gfx/Types.h"        // for SamplingFilter, SurfaceFormat
#include "mozilla/layers/CompositorTypes.h"  // for TextureFlags
#include "mozilla/layers/WebRenderLayersLogging.h"
#include "mozilla/layers/ZoomConstraints.h"
#include "nsAString.h"
#include "nsPrintfCString.h"  // for nsPrintfCString
#include "nsRegion.h"         // for nsRegion, nsIntRegion
#include "nscore.h"           // for nsACString, etc

struct nsRectAbsolute;

namespace mozilla {

namespace gfx {
template <class units, class F>
struct RectTyped;
}  // namespace gfx

enum class ImageFormat;

namespace layers {
struct ZoomConstraints;

void AppendToString(std::stringstream& aStream, const wr::ColorF& c,
                    const char* pfx = "", const char* sfx = "");

void AppendToString(std::stringstream& aStream, const wr::LayoutRect& r,
                    const char* pfx = "", const char* sfx = "");

void AppendToString(std::stringstream& aStream, const wr::LayoutSize& s,
                    const char* pfx = "", const char* sfx = "");

void AppendToString(std::stringstream& aStream, const wr::StickyOffsetBounds& s,
                    const char* pfx = "", const char* sfx = "");

void AppendToString(std::stringstream& aStream, const ScrollMetadata& m,
                    const char* pfx = "", const char* sfx = "");

void AppendToString(std::stringstream& aStream, const FrameMetrics& m,
                    const char* pfx = "", const char* sfx = "",
                    bool detailed = false);

void AppendToString(std::stringstream& aStream, const ZoomConstraints& z,
                    const char* pfx = "", const char* sfx = "");

void AppendToString(std::stringstream& aStream, const mozilla::gfx::Matrix& m,
                    const char* pfx = "", const char* sfx = "");

template <class SourceUnits, class TargetUnits>
void AppendToString(
    std::stringstream& aStream,
    const mozilla::gfx::Matrix4x4Typed<SourceUnits, TargetUnits>& m,
    const char* pfx = "", const char* sfx = "") {
  if (m.Is2D()) {
    mozilla::gfx::Matrix matrix = m.As2D();
    AppendToString(aStream, matrix, pfx, sfx);
    return;
  }

  aStream << pfx;
  aStream << nsPrintfCString(
                 "[ %g %g %g %g; %g %g %g %g; %g %g %g %g; %g %g %g %g; ]",
                 m._11, m._12, m._13, m._14, m._21, m._22, m._23, m._24, m._31,
                 m._32, m._33, m._34, m._41, m._42, m._43, m._44)
                 .get();
  aStream << sfx;
}

void AppendToString(std::stringstream& aStream,
                    const mozilla::gfx::Matrix5x4& m, const char* pfx = "",
                    const char* sfx = "");

void AppendToString(std::stringstream& aStream,
                    const mozilla::gfx::SamplingFilter samplingFilter,
                    const char* pfx = "", const char* sfx = "");

void AppendToString(std::stringstream& aStream,
                    mozilla::layers::TextureFlags flags, const char* pfx = "",
                    const char* sfx = "");

void AppendToString(std::stringstream& aStream,
                    mozilla::gfx::SurfaceFormat format, const char* pfx = "",
                    const char* sfx = "");

void AppendToString(std::stringstream& aStream, gfx::SurfaceType format,
                    const char* pfx = "", const char* sfx = "");

void AppendToString(std::stringstream& aStream, ImageFormat format,
                    const char* pfx = "", const char* sfx = "");

void AppendToString(std::stringstream& aStream,
                    const mozilla::ScrollPositionUpdate& aUpdate,
                    const char* pfx = "", const char* sfx = "");

// Sometimes, you just want a string from a single value.
template <typename T>
std::string Stringify(const T& obj) {
  std::stringstream ss;
  AppendToString(ss, obj);
  return ss.str();
}

}  // namespace layers
}  // namespace mozilla

// versions of printf_stderr and fprintf_stderr that deal with line
// truncation on android by printing individual lines out of the
// stringstream as separate calls to logcat.
void print_stderr(std::stringstream& aStr);
void fprint_stderr(FILE* aFile, std::stringstream& aStr);

#endif /* GFX_LAYERSLOGGING_H */
