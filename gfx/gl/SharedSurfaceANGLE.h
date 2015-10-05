/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SHARED_SURFACE_ANGLE_H_
#define SHARED_SURFACE_ANGLE_H_

#include <windows.h>
#include "SharedSurface.h"

struct IDXGIKeyedMutex;
struct ID3D11Texture2D;

namespace mozilla {
namespace gl {

class GLContext;
class GLLibraryEGL;

class SharedSurface_ANGLEShareHandle
    : public SharedSurface
{
public:
    static UniquePtr<SharedSurface_ANGLEShareHandle> Create(GLContext* gl,
                                                            EGLConfig config,
                                                            const gfx::IntSize& size,
                                                            bool hasAlpha);

    static SharedSurface_ANGLEShareHandle* Cast(SharedSurface* surf) {
        MOZ_ASSERT(surf->mType == SharedSurfaceType::EGLSurfaceANGLE);

        return (SharedSurface_ANGLEShareHandle*)surf;
    }

protected:
    GLLibraryEGL* const mEGL;
    const EGLSurface mPBuffer;
public:
    const HANDLE mShareHandle;
protected:
    RefPtr<IDXGIKeyedMutex> mKeyedMutex;
    RefPtr<IDXGIKeyedMutex> mConsumerKeyedMutex;
    RefPtr<ID3D11Texture2D> mConsumerTexture;

    const GLuint mFence;

    SharedSurface_ANGLEShareHandle(GLContext* gl,
                                   GLLibraryEGL* egl,
                                   const gfx::IntSize& size,
                                   bool hasAlpha,
                                   EGLSurface pbuffer,
                                   HANDLE shareHandle,
                                   const RefPtr<IDXGIKeyedMutex>& keyedMutex,
                                   GLuint fence);

    EGLDisplay Display();

public:
    virtual ~SharedSurface_ANGLEShareHandle();

    virtual void LockProdImpl() override;
    virtual void UnlockProdImpl() override;

    virtual void Fence() override;
    virtual void ProducerAcquireImpl() override;
    virtual void ProducerReleaseImpl() override;
    virtual void ProducerReadAcquireImpl() override;
    virtual void ProducerReadReleaseImpl() override;
    virtual void ConsumerAcquireImpl() override;
    virtual void ConsumerReleaseImpl() override;
    virtual bool WaitSync() override;
    virtual bool PollSync() override;

    virtual void Fence_ContentThread_Impl() override;
    virtual bool WaitSync_ContentThread_Impl() override;
    virtual bool PollSync_ContentThread_Impl() override;

    const RefPtr<ID3D11Texture2D>& GetConsumerTexture() const {
        return mConsumerTexture;
    }

    virtual bool ToSurfaceDescriptor(layers::SurfaceDescriptor* const out_descriptor) override;
};



class SurfaceFactory_ANGLEShareHandle
    : public SurfaceFactory
{
protected:
    GLContext* const mProdGL;
    GLLibraryEGL* const mEGL;
    const EGLConfig mConfig;

public:
    static UniquePtr<SurfaceFactory_ANGLEShareHandle> Create(GLContext* gl,
                                                             const SurfaceCaps& caps,
                                                             const RefPtr<layers::ISurfaceAllocator>& allocator,
                                                             const layers::TextureFlags& flags);

protected:
    SurfaceFactory_ANGLEShareHandle(GLContext* gl, const SurfaceCaps& caps,
                                    const RefPtr<layers::ISurfaceAllocator>& allocator,
                                    const layers::TextureFlags& flags, GLLibraryEGL* egl,
                                    EGLConfig config);

    virtual UniquePtr<SharedSurface> CreateShared(const gfx::IntSize& size) override {
        bool hasAlpha = mReadCaps.alpha;
        return SharedSurface_ANGLEShareHandle::Create(mProdGL, mConfig, size, hasAlpha);
    }
};

} /* namespace gfx */
} /* namespace mozilla */

#endif /* SHARED_SURFACE_ANGLE_H_ */
