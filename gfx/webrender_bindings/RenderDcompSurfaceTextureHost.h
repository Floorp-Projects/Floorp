/* -*- Mode: C++; tab-width: ; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_RENDERDCOMPSURFACETEXTUREHOST_H
#define MOZILLA_GFX_RENDERDCOMPSURFACETEXTUREHOST_H

#include "GLTypes.h"
#include "RenderTextureHostSWGL.h"
#include "mozilla/webrender/RenderThread.h"

struct IDCompositionDevice;
struct IDCompositionSurface;

inline mozilla::LazyLogModule gDcompSurface("DcompSurface");

namespace mozilla::wr {

/**
 * A render texture host is responsible to create a dcomp surface from an
 * existing dcomp handle. Currently usage is that MF media engine will create
 * a surface handle from another remote process, and we reconstruct the surface
 * and use it in the DCLayerTree in the GPU process.
 */
class RenderDcompSurfaceTextureHost final : public RenderTextureHostSWGL {
 public:
  RenderDcompSurfaceTextureHost(HANDLE aHandle, gfx::IntSize aSize,
                                gfx::SurfaceFormat aFormat);

  // RenderTextureHost
  RenderDcompSurfaceTextureHost* AsRenderDcompSurfaceTextureHost() override {
    return this;
  }

  // RenderTextureHostSWGL
  gfx::SurfaceFormat GetFormat() const override { return mFormat; }
  gfx::ColorDepth GetColorDepth() const override {
    return gfx::ColorDepth::COLOR_8;
  }
  size_t GetPlaneCount() const override { return 1; }
  bool MapPlane(RenderCompositor* aCompositor, uint8_t aChannelIndex,
                PlaneInfo& aPlaneInfo) override {
    return false;
  }
  void UnmapPlanes() override {}
  gfx::YUVRangedColorSpace GetYUVColorSpace() const override {
    return gfx::YUVRangedColorSpace::GbrIdentity;
  }
  size_t Bytes() override { return 0; }

  gfx::IntSize GetSize() const { return mSize; };

  HANDLE GetDcompSurfaceHandle() const { return mHandle; }

  // Not thread-safe. They should only be called on the renderer thread on the
  // GPU process.
  IDCompositionSurface* CreateSurfaceFromDevice(IDCompositionDevice* aDevice);
  IDCompositionSurface* GetSurface() const { return mDcompSurface; };

 private:
  const HANDLE mHandle;
  const gfx::IntSize mSize;
  const gfx::SurfaceFormat mFormat;
  RefPtr<IDCompositionSurface> mDcompSurface;
};

}  // namespace mozilla::wr

#endif  // MOZILLA_GFX_RENDERDCOMPSURFACETEXTUREHOST_H
