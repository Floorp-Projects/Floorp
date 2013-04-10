/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_LAYERSLOGGING_H
#define GFX_LAYERSLOGGING_H

#include "Layers.h"
#include "nsPoint.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/layers/Compositor.h"
#include "FrameMetrics.h"
#include "gfxPattern.h"
#include "gfxColor.h"
#include "gfx3DMatrix.h"
#include "nsRegion.h"

namespace mozilla {
namespace layers {

nsACString&
AppendToString(nsACString& s, const void* p,
               const char* pfx="", const char* sfx="");

nsACString&
AppendToString(nsACString& s, const gfxPattern::GraphicsFilter& f,
               const char* pfx="", const char* sfx="");

nsACString&
AppendToString(nsACString& s, FrameMetrics::ViewID n,
               const char* pfx="", const char* sfx="");

nsACString&
AppendToString(nsACString& s, const gfxRGBA& c,
               const char* pfx="", const char* sfx="");

nsACString&
AppendToString(nsACString& s, const gfx3DMatrix& m,
               const char* pfx="", const char* sfx="");

nsACString&
AppendToString(nsACString& s, const nsIntPoint& p,
               const char* pfx="", const char* sfx="");

nsACString&
AppendToString(nsACString& s, const mozilla::gfx::Point& p,
               const char* pfx="", const char* sfx="");

nsACString&
AppendToString(nsACString& s, const nsIntRect& r,
               const char* pfx="", const char* sfx="");

nsACString&
AppendToString(nsACString& s, const mozilla::gfx::Rect& r,
               const char* pfx="", const char* sfx="");

nsACString&
AppendToString(nsACString& s, const nsIntRegion& r,
               const char* pfx="", const char* sfx="");

nsACString&
AppendToString(nsACString& s, const nsIntSize& sz,
               const char* pfx="", const char* sfx="");

nsACString&
AppendToString(nsACString& s, const FrameMetrics& m,
               const char* pfx="", const char* sfx="");

nsACString&
AppendToString(nsACString& s, const mozilla::gfx::IntSize& size,
               const char* pfx="", const char* sfx="");

nsACString&
AppendToString(nsACString& s, const mozilla::gfx::Matrix4x4& m,
               const char* pfx="", const char* sfx="");

nsACString&
AppendToString(nsACString& s, const mozilla::gfx::Filter filter,
               const char* pfx="", const char* sfx="");

nsACString&
AppendToString(nsACString& s, mozilla::layers::TextureFlags flags,
               const char* pfx="", const char* sfx="");

nsACString&
AppendToString(nsACString& s, mozilla::gfx::SurfaceFormat format,
               const char* pfx="", const char* sfx="");

} // namespace
} // namespace

#endif /* GFX_LAYERSLOGGING_H */
