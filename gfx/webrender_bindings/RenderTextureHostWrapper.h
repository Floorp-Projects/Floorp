/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_RENDERTEXTUREHOSTWRAPPER_H
#define MOZILLA_GFX_RENDERTEXTUREHOSTWRAPPER_H

#include "RenderTextureHostSWGL.h"

namespace mozilla {

namespace wr {

/**
 * RenderTextureHost of GPUVideoTextureHost.
 *
 * GPUVideoTextureHost wraps TextureHost. This class wraps RenderTextureHost of
 * the wrapped TextureHost. Lifetime of the wrapped TextureHost is usually
 * longer than GPUVideoTextureHost and the wrapped TextureHost is used by
 * multiple GPUVideoTextureHosts. This class is used to reduce recreations of
 * the wrappded RenderTextureHost. Initializations of some
 * RenderTextureHosts(RenderDXGITextureHost and
 * RenderDXGIYCbCrTextureHost) have overhead.
 */
class RenderTextureHostWrapper final : public RenderTextureHostSWGL {
 public:
  explicit RenderTextureHostWrapper(ExternalImageId aExternalImageId);

  // RenderTextureHost
  wr::WrExternalImage Lock(uint8_t aChannelIndex, gl::GLContext* aGL,
                           wr::ImageRendering aRendering) override;
  void Unlock() override;
  void ClearCachedResources() override;
  void PrepareForUse() override;
  void NotifyForUse() override;
  void NotifyNotUsed() override;
  bool SyncObjectNeeded() override;
  RenderMacIOSurfaceTextureHost* AsRenderMacIOSurfaceTextureHost() override;
  RenderDXGITextureHost* AsRenderDXGITextureHost() override;
  RenderDXGIYCbCrTextureHost* AsRenderDXGIYCbCrTextureHost() override;
  RenderDcompSurfaceTextureHost* AsRenderDcompSurfaceTextureHost() override;

  // RenderTextureHostSWGL
  size_t GetPlaneCount() const override;
  gfx::SurfaceFormat GetFormat() const override;
  gfx::ColorDepth GetColorDepth() const override;
  gfx::YUVRangedColorSpace GetYUVColorSpace() const override;
  bool MapPlane(RenderCompositor* aCompositor, uint8_t aChannelIndex,
                PlaneInfo& aPlaneInfo) override;
  void UnmapPlanes() override;

  // This is just a wrapper, so doesn't need to report the
  // size of the wrapped object (which reports itself).
  size_t Bytes() override { return 0; }

 private:
  ~RenderTextureHostWrapper() override;

  void EnsureTextureHost() const;
  RenderTextureHostSWGL* EnsureRenderTextureHostSWGL() const;

  const ExternalImageId mExternalImageId;
  mutable RefPtr<RenderTextureHost> mTextureHost;
};

}  // namespace wr
}  // namespace mozilla

#endif  // MOZILLA_GFX_RENDERTEXTUREHOSTWRAPPER_H
