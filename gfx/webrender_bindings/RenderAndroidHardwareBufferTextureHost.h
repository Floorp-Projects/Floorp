/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_RenderAndroidHardwareBufferTextureHost_H
#define MOZILLA_GFX_RenderAndroidHardwareBufferTextureHost_H

#include "GLContextTypes.h"
#include "GLTypes.h"
#include "RenderTextureHostSWGL.h"

namespace mozilla {

namespace layers {
class AndroidHardwareBuffer;
}

namespace wr {

class RenderAndroidHardwareBufferTextureHost final
    : public RenderTextureHostSWGL {
 public:
  explicit RenderAndroidHardwareBufferTextureHost(
      layers::AndroidHardwareBuffer* aAndroidHardwareBuffer);

  wr::WrExternalImage Lock(uint8_t aChannelIndex, gl::GLContext* aGL) override;
  void Unlock() override;

  size_t Bytes() override;

  RenderAndroidHardwareBufferTextureHost*
  AsRenderAndroidHardwareBufferTextureHost() override {
    return this;
  }

  // RenderTextureHostSWGL
  gfx::SurfaceFormat GetFormat() const override;
  gfx::ColorDepth GetColorDepth() const override {
    return gfx::ColorDepth::COLOR_8;
  }
  size_t GetPlaneCount() const override { return 1; }
  bool MapPlane(RenderCompositor* aCompositor, uint8_t aChannelIndex,
                PlaneInfo& aPlaneInfo) override;
  void UnmapPlanes() override;

  layers::AndroidHardwareBuffer* GetAndroidHardwareBuffer() {
    return mAndroidHardwareBuffer;
  }

  gfx::IntSize GetSize() const;

 private:
  virtual ~RenderAndroidHardwareBufferTextureHost();
  bool EnsureLockable();
  void DestroyEGLImage();
  void DeleteTextureHandle();
  already_AddRefed<gfx::DataSourceSurface> ReadTexImage();

  const RefPtr<layers::AndroidHardwareBuffer> mAndroidHardwareBuffer;

  RefPtr<gl::GLContext> mGL;
  EGLImage mEGLImage;
  GLuint mTextureHandle;

  RefPtr<gfx::DataSourceSurface> mReadback;
};

}  // namespace wr
}  // namespace mozilla

#endif  // MOZILLA_GFX_RenderAndroidHardwareBufferTextureHost_H
