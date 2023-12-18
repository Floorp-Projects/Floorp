/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 4; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SHARED_SURFACE_EGL_H_
#define SHARED_SURFACE_EGL_H_

#include "mozilla/Attributes.h"
#include "mozilla/Mutex.h"
#include "CompositorTypes.h"
#include "SharedSurface.h"

#ifdef MOZ_WIDGET_ANDROID
#  include "AndroidNativeWindow.h"
#  include "GLLibraryEGL.h"
#endif

namespace mozilla {
namespace gl {

class GLLibraryEGL;

// -
// EGLImage

class SharedSurface_EGLImage final : public SharedSurface {
  mutable Mutex mMutex MOZ_UNANNOTATED;
  EGLSync mSync = 0;

 public:
  const EGLImage mImage;

  static UniquePtr<SharedSurface_EGLImage> Create(const SharedSurfaceDesc&);

 protected:
  SharedSurface_EGLImage(const SharedSurfaceDesc&,
                         UniquePtr<MozFramebuffer>&& fb, EGLImage);

  void UpdateProdTexture(const MutexAutoLock& curAutoLock);

 public:
  virtual ~SharedSurface_EGLImage();

  virtual void LockProdImpl() override {}
  virtual void UnlockProdImpl() override {}

  virtual bool ProducerAcquireImpl() override { return true; }
  virtual void ProducerReleaseImpl() override;

  virtual bool ProducerReadAcquireImpl() override;
  virtual void ProducerReadReleaseImpl() override{};

  Maybe<layers::SurfaceDescriptor> ToSurfaceDescriptor() override;
};

class SurfaceFactory_EGLImage final : public SurfaceFactory {
 public:
  static UniquePtr<SurfaceFactory_EGLImage> Create(GLContext&);

 private:
  explicit SurfaceFactory_EGLImage(const PartialSharedSurfaceDesc& desc)
      : SurfaceFactory(desc) {}

 public:
  virtual UniquePtr<SharedSurface> CreateSharedImpl(
      const SharedSurfaceDesc& desc) override {
    return SharedSurface_EGLImage::Create(desc);
  }
};

// -
// SurfaceTexture

#ifdef MOZ_WIDGET_ANDROID

class SharedSurface_SurfaceTexture final : public SharedSurface {
  const java::GeckoSurface::GlobalRef mSurface;
  const EGLSurface mEglSurface;
  const std::weak_ptr<EglDisplay> mEglDisplay;
  EGLSurface mOrigEglSurface = 0;

 public:
  static UniquePtr<SharedSurface_SurfaceTexture> Create(
      const SharedSurfaceDesc&);

  java::GeckoSurface::Param JavaSurface() { return mSurface; }

 protected:
  SharedSurface_SurfaceTexture(const SharedSurfaceDesc&,
                               java::GeckoSurface::Param surface, EGLSurface);

 public:
  virtual ~SharedSurface_SurfaceTexture();

  virtual void LockProdImpl() override;
  virtual void UnlockProdImpl() override;

  virtual bool ProducerAcquireImpl() override { return true; }
  virtual void ProducerReleaseImpl() override {}
  virtual void ProducerReadReleaseImpl() override;

  Maybe<layers::SurfaceDescriptor> ToSurfaceDescriptor() override;

  virtual void Commit() override;

  virtual void WaitForBufferOwnership() override;

  virtual bool IsBufferAvailable() const override;

  bool IsValid() const override;
};

class SurfaceFactory_SurfaceTexture final : public SurfaceFactory {
 public:
  explicit SurfaceFactory_SurfaceTexture(GLContext&);

  virtual UniquePtr<SharedSurface> CreateSharedImpl(
      const SharedSurfaceDesc& desc) override {
    return SharedSurface_SurfaceTexture::Create(desc);
  }
};

#endif  // MOZ_WIDGET_ANDROID

}  // namespace gl

} /* namespace mozilla */

#endif /* SHARED_SURFACE_EGL_H_ */
