/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SHARED_SURFACE_EGL_H_
#define SHARED_SURFACE_EGL_H_

#include "mozilla/Attributes.h"
#include "mozilla/Mutex.h"
#include "SharedSurface.h"

#ifdef MOZ_WIDGET_ANDROID
#include "GeneratedJNIWrappers.h"
#include "AndroidNativeWindow.h"
#endif

namespace mozilla {
namespace gl {

class GLContext;
class GLLibraryEGL;

class SharedSurface_EGLImage
    : public SharedSurface
{
public:
    static UniquePtr<SharedSurface_EGLImage> Create(GLContext* prodGL,
                                                    const GLFormats& formats,
                                                    const gfx::IntSize& size,
                                                    bool hasAlpha,
                                                    EGLContext context);

    static SharedSurface_EGLImage* Cast(SharedSurface* surf) {
        MOZ_ASSERT(surf->mType == SharedSurfaceType::EGLImageShare);

        return (SharedSurface_EGLImage*)surf;
    }

    static bool HasExtensions(GLLibraryEGL* egl, GLContext* gl);

protected:
    mutable Mutex mMutex;
    GLLibraryEGL* const mEGL;
    const GLFormats mFormats;
    GLuint mProdTex;
public:
    const EGLImage mImage;
protected:
    EGLSync mSync;

    SharedSurface_EGLImage(GLContext* gl,
                           GLLibraryEGL* egl,
                           const gfx::IntSize& size,
                           bool hasAlpha,
                           const GLFormats& formats,
                           GLuint prodTex,
                           EGLImage image);

    EGLDisplay Display() const;
    void UpdateProdTexture(const MutexAutoLock& curAutoLock);

public:
    virtual ~SharedSurface_EGLImage();

    virtual layers::TextureFlags GetTextureFlags() const override {
      return layers::TextureFlags::DEALLOCATE_CLIENT;
    }

    virtual void LockProdImpl() override {}
    virtual void UnlockProdImpl() override {}

    virtual void ProducerAcquireImpl() override {}
    virtual void ProducerReleaseImpl() override;

    virtual void ProducerReadAcquireImpl() override;
    virtual void ProducerReadReleaseImpl() override {};

    virtual GLuint ProdTexture() override {
      return mProdTex;
    }

    // Implementation-specific functions below:
    // Returns texture and target
    virtual bool ToSurfaceDescriptor(layers::SurfaceDescriptor* const out_descriptor) override;

    virtual bool ReadbackBySharedHandle(gfx::DataSourceSurface* out_surface) override;
};



class SurfaceFactory_EGLImage
    : public SurfaceFactory
{
public:
    // Fallible:
    static UniquePtr<SurfaceFactory_EGLImage> Create(GLContext* prodGL,
                                                     const SurfaceCaps& caps,
                                                     const RefPtr<layers::LayersIPCChannel>& allocator,
                                                     const layers::TextureFlags& flags);

protected:
    const EGLContext mContext;

    SurfaceFactory_EGLImage(GLContext* prodGL, const SurfaceCaps& caps,
                            const RefPtr<layers::LayersIPCChannel>& allocator,
                            const layers::TextureFlags& flags,
                            EGLContext context)
        : SurfaceFactory(SharedSurfaceType::EGLImageShare, prodGL, caps, allocator, flags)
        , mContext(context)
    { }

public:
    virtual UniquePtr<SharedSurface> CreateShared(const gfx::IntSize& size) override {
        bool hasAlpha = mReadCaps.alpha;
        return SharedSurface_EGLImage::Create(mGL, mFormats, size, hasAlpha, mContext);
    }
};

#ifdef MOZ_WIDGET_ANDROID

class SharedSurface_SurfaceTexture
    : public SharedSurface
{
public:
    static UniquePtr<SharedSurface_SurfaceTexture> Create(GLContext* prodGL,
                                                          const GLFormats& formats,
                                                          const gfx::IntSize& size,
                                                          bool hasAlpha,
                                                          java::GeckoSurface::Param surface);

    static SharedSurface_SurfaceTexture* Cast(SharedSurface* surf) {
        MOZ_ASSERT(surf->mType == SharedSurfaceType::AndroidSurfaceTexture);

        return (SharedSurface_SurfaceTexture*)surf;
    }

    java::GeckoSurface::Param JavaSurface() { return mSurface; }

protected:
    java::GeckoSurface::GlobalRef mSurface;
    EGLSurface mEglSurface;
    EGLSurface mOrigEglSurface;

    SharedSurface_SurfaceTexture(GLContext* gl,
                                 const gfx::IntSize& size,
                                 bool hasAlpha,
                                 const GLFormats& formats,
                                 java::GeckoSurface::Param surface,
                                 EGLSurface eglSurface);

public:
    virtual ~SharedSurface_SurfaceTexture();

    virtual layers::TextureFlags GetTextureFlags() const override {
      return layers::TextureFlags::DEALLOCATE_CLIENT;
    }

    virtual void LockProdImpl() override;
    virtual void UnlockProdImpl() override;

    virtual void ProducerAcquireImpl() override {}
    virtual void ProducerReleaseImpl() override {}

    virtual void ProducerReadAcquireImpl() override {}
    virtual void ProducerReadReleaseImpl() override {}

    // Implementation-specific functions below:
    // Returns texture and target
    virtual bool ToSurfaceDescriptor(layers::SurfaceDescriptor* const out_descriptor) override;

    virtual bool ReadbackBySharedHandle(gfx::DataSourceSurface* out_surface) override { return false; }

    virtual void Commit() override;

    virtual void WaitForBufferOwnership() override;
};



class SurfaceFactory_SurfaceTexture
    : public SurfaceFactory
{
public:
    // Fallible:
    static UniquePtr<SurfaceFactory_SurfaceTexture> Create(GLContext* prodGL,
                                                           const SurfaceCaps& caps,
                                                           const RefPtr<layers::LayersIPCChannel>& allocator,
                                                           const layers::TextureFlags& flags);

protected:
    SurfaceFactory_SurfaceTexture(GLContext* prodGL, const SurfaceCaps& caps,
                            const RefPtr<layers::LayersIPCChannel>& allocator,
                            const layers::TextureFlags& flags)
        : SurfaceFactory(SharedSurfaceType::AndroidSurfaceTexture, prodGL, caps, allocator, flags)
    { }

public:
    virtual UniquePtr<SharedSurface> CreateShared(const gfx::IntSize& size) override;
};

#endif // MOZ_WIDGET_ANDROID

} // namespace gl

} /* namespace mozilla */

#endif /* SHARED_SURFACE_EGL_H_ */
