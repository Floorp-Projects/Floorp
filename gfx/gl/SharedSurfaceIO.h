/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 4; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SHARED_SURFACEIO_H_
#define SHARED_SURFACEIO_H_

#include "mozilla/RefPtr.h"
#include "SharedSurface.h"

class MacIOSurface;

namespace mozilla {
namespace gl {

class Texture;

class SharedSurface_IOSurface final : public SharedSurface {
 public:
  const UniquePtr<Texture> mTex;
  const RefPtr<MacIOSurface> mIOSurf;

  static UniquePtr<SharedSurface_IOSurface> Create(const SharedSurfaceDesc&);

 private:
  SharedSurface_IOSurface(const SharedSurfaceDesc&, UniquePtr<MozFramebuffer>,
                          UniquePtr<Texture>, const RefPtr<MacIOSurface>&);

 public:
  ~SharedSurface_IOSurface();

  virtual void LockProdImpl() override {}
  virtual void UnlockProdImpl() override {}

  virtual bool ProducerAcquireImpl() override { return true; }
  virtual void ProducerReleaseImpl() override;

  virtual bool NeedsIndirectReads() const override { return true; }

  Maybe<layers::SurfaceDescriptor> ToSurfaceDescriptor() override;
};

class SurfaceFactory_IOSurface : public SurfaceFactory {
 public:
  const gfx::IntSize mMaxDims;

  explicit SurfaceFactory_IOSurface(GLContext& gl);

  virtual UniquePtr<SharedSurface> CreateSharedImpl(
      const SharedSurfaceDesc& desc) override {
    if (desc.size.width > mMaxDims.width ||
        desc.size.height > mMaxDims.height) {
      return nullptr;
    }
    return SharedSurface_IOSurface::Create(desc);
  }
};

}  // namespace gl

} /* namespace mozilla */

#endif /* SHARED_SURFACEIO_H_ */
