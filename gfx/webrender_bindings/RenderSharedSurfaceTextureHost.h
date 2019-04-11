/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_RENDERSHAREDSURFACETEXTUREHOST_H
#define MOZILLA_GFX_RENDERSHAREDSURFACETEXTUREHOST_H

#include "RenderTextureHost.h"

namespace mozilla {
namespace gfx {
class SourceSurfaceSharedDataWrapper;
}

namespace wr {

/**
 * This class allows for surfaces managed by SharedSurfacesParent to be inserted
 * into the render texture cache by wrapping an existing surface wrapper. These
 * surfaces are backed by BGRA/X shared memory buffers.
 */
class RenderSharedSurfaceTextureHost final : public RenderTextureHost {
 public:
  explicit RenderSharedSurfaceTextureHost(
      gfx::SourceSurfaceSharedDataWrapper* aSurface);

  wr::WrExternalImage Lock(uint8_t aChannelIndex, gl::GLContext* aGL,
                           wr::ImageRendering aRendering) override;
  void Unlock() override;

 private:
  virtual ~RenderSharedSurfaceTextureHost();

  RefPtr<gfx::SourceSurfaceSharedDataWrapper> mSurface;
  gfx::DataSourceSurface::MappedSurface mMap;
  bool mLocked;
};

}  // namespace wr
}  // namespace mozilla

#endif  // MOZILLA_GFX_RENDERSHAREDSURFACETEXTUREHOST_H
