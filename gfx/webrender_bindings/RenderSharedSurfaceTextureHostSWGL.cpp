/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderSharedSurfaceTextureHostSWGL.h"

#include "mozilla/layers/SourceSurfaceSharedData.h"

namespace mozilla {
namespace wr {

RenderSharedSurfaceTextureHostSWGL::RenderSharedSurfaceTextureHostSWGL(
    gfx::SourceSurfaceSharedDataWrapper* aSurface)
    : mSurface(aSurface) {
  MOZ_COUNT_CTOR_INHERITED(RenderSharedSurfaceTextureHostSWGL,
                           RenderTextureHostSWGL);
  MOZ_ASSERT(aSurface);
}

RenderSharedSurfaceTextureHostSWGL::~RenderSharedSurfaceTextureHostSWGL() {
  MOZ_COUNT_DTOR_INHERITED(RenderSharedSurfaceTextureHostSWGL,
                           RenderTextureHostSWGL);
}

size_t RenderSharedSurfaceTextureHostSWGL::GetPlaneCount() const { return 1; }

gfx::SurfaceFormat RenderSharedSurfaceTextureHostSWGL::GetFormat() const {
  return mSurface->GetFormat();
}

gfx::ColorDepth RenderSharedSurfaceTextureHostSWGL::GetColorDepth() const {
  return gfx::ColorDepth::COLOR_8;
}

bool RenderSharedSurfaceTextureHostSWGL::MapPlane(uint8_t aChannelIndex,
                                                  PlaneInfo& aPlaneInfo) {
  if (NS_WARN_IF(
          !mSurface->Map(gfx::DataSourceSurface::MapType::READ, &mMap))) {
    return false;
  }
  aPlaneInfo.mData = mMap.mData;
  aPlaneInfo.mStride = mMap.mStride;
  aPlaneInfo.mSize = mSurface->GetSize();
  return true;
}

void RenderSharedSurfaceTextureHostSWGL::UnmapPlanes() { mSurface->Unmap(); }

}  // namespace wr
}  // namespace mozilla
