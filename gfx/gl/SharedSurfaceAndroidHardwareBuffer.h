/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 4; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SHARED_SURFACE_ANDROID_HARDWARE_BUFFER_H_
#define SHARED_SURFACE_ANDROID_HARDWARE_BUFFER_H_

#include "mozilla/Attributes.h"
#include "mozilla/Mutex.h"
#include "CompositorTypes.h"
#include "SharedSurface.h"

namespace mozilla {

namespace layers {
class AndroidHardwareBuffer;
}  // namespace layers

namespace gl {

class SharedSurface_AndroidHardwareBuffer final : public SharedSurface {
 public:
  const UniquePtr<Texture> mTex;
  const RefPtr<layers::AndroidHardwareBuffer> mAndroidHardwareBuffer;
  EGLSync mSync = 0;

  static UniquePtr<SharedSurface_AndroidHardwareBuffer> Create(
      const SharedSurfaceDesc&);

 protected:
  SharedSurface_AndroidHardwareBuffer(
      const SharedSurfaceDesc&, UniquePtr<MozFramebuffer>, UniquePtr<Texture>,
      RefPtr<layers::AndroidHardwareBuffer> buffer);

 public:
  virtual ~SharedSurface_AndroidHardwareBuffer();

  void LockProdImpl() override {}
  void UnlockProdImpl() override {}

  bool ProducerAcquireImpl() override { return true; }
  void ProducerReleaseImpl() override;

  Maybe<layers::SurfaceDescriptor> ToSurfaceDescriptor() override;

  void WaitForBufferOwnership() override;
};

class SurfaceFactory_AndroidHardwareBuffer final : public SurfaceFactory {
 public:
  static UniquePtr<SurfaceFactory_AndroidHardwareBuffer> Create(GLContext&);

 private:
  explicit SurfaceFactory_AndroidHardwareBuffer(
      const PartialSharedSurfaceDesc& desc)
      : SurfaceFactory(desc) {}

 public:
  virtual UniquePtr<SharedSurface> CreateSharedImpl(
      const SharedSurfaceDesc& desc) override {
    return SharedSurface_AndroidHardwareBuffer::Create(desc);
  }
};

}  // namespace gl

} /* namespace mozilla */

#endif /* SHARED_SURFACE_ANDROID_HARDWARE_BUFFER_H_ */
