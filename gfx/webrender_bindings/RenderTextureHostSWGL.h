/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_RENDERTEXTUREHOSTSWGL_H
#define MOZILLA_GFX_RENDERTEXTUREHOSTSWGL_H

#include "RenderTextureHost.h"

namespace mozilla {
namespace wr {

class RenderTextureHostSWGL : public RenderTextureHost {
 public:
  RenderTextureHostSWGL() {}

  wr::WrExternalImage LockSWGL(uint8_t aChannelIndex, void* aContext,
                               wr::ImageRendering aRendering) override;

  void UnlockSWGL() override;

  virtual size_t GetPlaneCount() = 0;

  struct PlaneInfo {
    explicit PlaneInfo(GLuint aTexture) : mTexture(aTexture) {}

    GLuint mTexture = 0;
    void* mData = nullptr;
    int32_t mStride = 0;
    gfx::IntSize mSize;
    gfx::SurfaceFormat mFormat = gfx::SurfaceFormat::UNKNOWN;
  };

  virtual bool MapPlane(uint8_t aChannelIndex, PlaneInfo& aPlaneInfo) = 0;

  virtual void UnmapPlanes() = 0;

 protected:
  bool mLocked = false;
  void* mContext = nullptr;
  std::vector<PlaneInfo> mPlanes;

  bool UpdatePlanes(wr::ImageRendering aRendering);

  void CleanupPlanes();

  virtual ~RenderTextureHostSWGL();
};

}  // namespace wr
}  // namespace mozilla

#endif  // MOZILLA_GFX_RENDERTEXTUREHOSTSWGL_H
