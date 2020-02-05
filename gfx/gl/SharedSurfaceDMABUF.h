/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 4; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SHARED_SURFACE_DMABUF_H_
#define SHARED_SURFACE_DMABUF_H_

#include "SharedSurface.h"
#include "mozilla/widget/WaylandDMABufSurface.h"

namespace mozilla {
namespace gl {

class GLContext;
class GLLibraryEGL;

class SharedSurface_DMABUF final : public SharedSurface {
 public:
  static UniquePtr<SharedSurface_DMABUF> Create(GLContext* prodGL,
                                                const GLFormats& formats,
                                                const gfx::IntSize& size,
                                                bool hasAlpha);

  static SharedSurface_DMABUF* Cast(SharedSurface* surf) {
    MOZ_ASSERT(surf->mType == SharedSurfaceType::EGLSurfaceDMABUF);

    return (SharedSurface_DMABUF*)surf;
  }

 protected:
  RefPtr<WaylandDMABufSurface> mSurface;

  SharedSurface_DMABUF(GLContext* gl, const gfx::IntSize& size, bool hasAlpha,
                       RefPtr<WaylandDMABufSurface> aSurface);

  void UpdateProdTexture(const MutexAutoLock& curAutoLock);

 public:
  virtual ~SharedSurface_DMABUF();

  // Exclusive Content/WebGL lock/unlock of surface for write
  virtual void LockProdImpl() override {}
  virtual void UnlockProdImpl() override {}

  // Non-exclusive Content/WebGL lock/unlock of surface for write
  virtual void ProducerAcquireImpl() override {}
  virtual void ProducerReleaseImpl() override;

  // Non-exclusive Content/WebGL lock/unlock for read from surface
  virtual void ProducerReadAcquireImpl() override {}
  virtual void ProducerReadReleaseImpl() override {}

  virtual GLuint ProdTexture() override { return mSurface->GetGLTexture(); }

  virtual bool ToSurfaceDescriptor(
      layers::SurfaceDescriptor* const out_descriptor) override;
};

class SurfaceFactory_DMABUF : public SurfaceFactory {
 public:
  SurfaceFactory_DMABUF(GLContext* prodGL, const SurfaceCaps& caps,
                        const RefPtr<layers::LayersIPCChannel>& allocator,
                        const layers::TextureFlags& flags)
      : SurfaceFactory(SharedSurfaceType::EGLSurfaceDMABUF, prodGL, caps,
                       allocator, flags){};

 public:
  virtual UniquePtr<SharedSurface> CreateShared(
      const gfx::IntSize& size) override {
    bool hasAlpha = mReadCaps.alpha;
    return SharedSurface_DMABUF::Create(mGL, mFormats, size, hasAlpha);
  }
};

}  // namespace gl

}  // namespace mozilla

#endif /* SHARED_SURFACE_DMABUF_H_ */
