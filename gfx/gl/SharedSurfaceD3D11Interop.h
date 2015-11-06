/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SHARED_SURFACE_D3D11_INTEROP_H_
#define SHARED_SURFACE_D3D11_INTEROP_H_

#include <windows.h>
#include "SharedSurface.h"

struct ID3D11Device;
struct ID3D11ShaderResourceView;

namespace mozilla {
namespace gl {

class DXGLDevice;
class GLContext;
class WGLLibrary;

class SharedSurface_D3D11Interop
    : public SharedSurface
{
    const GLuint mProdRB;
    const RefPtr<DXGLDevice> mDXGL;
    const HANDLE mObjectWGL;
    const HANDLE mSharedHandle;
    const RefPtr<ID3D11Texture2D> mTextureD3D;
    RefPtr<IDXGIKeyedMutex> mKeyedMutex;
    RefPtr<IDXGIKeyedMutex> mConsumerKeyedMutex;
    RefPtr<ID3D11Texture2D> mConsumerTexture;
    const GLuint mFence;

protected:
    bool mLockedForGL;

public:
    static UniquePtr<SharedSurface_D3D11Interop> Create(const RefPtr<DXGLDevice>& dxgl,
                                                        GLContext* gl,
                                                        const gfx::IntSize& size,
                                                        bool hasAlpha);

    static SharedSurface_D3D11Interop* Cast(SharedSurface* surf) {
        MOZ_ASSERT(surf->mType == SharedSurfaceType::DXGLInterop2);

        return (SharedSurface_D3D11Interop*)surf;
    }

protected:
    SharedSurface_D3D11Interop(GLContext* gl,
                               const gfx::IntSize& size,
                               bool hasAlpha,
                               GLuint renderbufferGL,
                               const RefPtr<DXGLDevice>& dxgl,
                               HANDLE objectWGL,
                               const RefPtr<ID3D11Texture2D>& textureD3D,
                               HANDLE sharedHandle,
                               const RefPtr<IDXGIKeyedMutex>& keyedMutex,
                               GLuint fence);

public:
    virtual ~SharedSurface_D3D11Interop();

    virtual void LockProdImpl() override;
    virtual void UnlockProdImpl() override;

    virtual void Fence() override;
    virtual void ProducerAcquireImpl() override;
    virtual void ProducerReleaseImpl() override;
    virtual void ConsumerAcquireImpl() override;
    virtual void ConsumerReleaseImpl() override;
    virtual bool WaitSync() override;
    virtual bool PollSync() override;

    virtual void Fence_ContentThread_Impl() override;
    virtual bool WaitSync_ContentThread_Impl() override;
    virtual bool PollSync_ContentThread_Impl() override;

    virtual GLuint ProdRenderbuffer() override {
        return mProdRB;
    }

    virtual bool ToSurfaceDescriptor(layers::SurfaceDescriptor* const out_descriptor) override;

    // Implementation-specific functions below:
    HANDLE GetSharedHandle() const {
        return mSharedHandle;
    }
};


class SurfaceFactory_D3D11Interop
    : public SurfaceFactory
{
public:
    const RefPtr<DXGLDevice> mDXGL;

    static UniquePtr<SurfaceFactory_D3D11Interop> Create(GLContext* gl,
                                                         const SurfaceCaps& caps,
                                                         const RefPtr<layers::ISurfaceAllocator>& allocator,
                                                         const layers::TextureFlags& flags);

protected:
    SurfaceFactory_D3D11Interop(GLContext* gl, const SurfaceCaps& caps,
                                const RefPtr<layers::ISurfaceAllocator>& allocator,
                                const layers::TextureFlags& flags,
                                const RefPtr<DXGLDevice>& dxgl);

public:
    virtual ~SurfaceFactory_D3D11Interop();

protected:
    virtual UniquePtr<SharedSurface> CreateShared(const gfx::IntSize& size) override {
        bool hasAlpha = mReadCaps.alpha;
        return SharedSurface_D3D11Interop::Create(mDXGL, mGL, size, hasAlpha);
    }
};

} /* namespace gl */
} /* namespace mozilla */

#endif /* SHARED_SURFACE_D3D11_INTEROP_H_ */
