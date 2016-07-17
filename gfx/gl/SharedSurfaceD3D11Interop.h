/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SHARED_SURFACE_D3D11_INTEROP_H_
#define SHARED_SURFACE_D3D11_INTEROP_H_

#include <windows.h>
#include "SharedSurface.h"

namespace mozilla {
namespace gl {

class DXInterop2Device;
class GLContext;
class WGLLibrary;

class SharedSurface_D3D11Interop
    : public SharedSurface
{
public:
    const GLuint mProdRB;
    const RefPtr<DXInterop2Device> mInterop;
    const HANDLE mLockHandle;
    const RefPtr<ID3D11Texture2D> mTexD3D;
    const HANDLE mDXGIHandle;
    const bool mNeedsFinish;

protected:
    bool mLockedForGL;

public:
    static UniquePtr<SharedSurface_D3D11Interop> Create(DXInterop2Device* interop,
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
                               DXInterop2Device* interop,
                               HANDLE lockHandle,
                               ID3D11Texture2D* texD3D,
                               HANDLE dxgiHandle);

public:
    virtual ~SharedSurface_D3D11Interop() override;

    virtual void LockProdImpl() override { }
    virtual void UnlockProdImpl() override { }

    virtual void ProducerAcquireImpl() override;
    virtual void ProducerReleaseImpl() override;

    virtual GLuint ProdRenderbuffer() override {
        return mProdRB;
    }

    virtual bool ToSurfaceDescriptor(layers::SurfaceDescriptor* const out_descriptor) override;
};

class SurfaceFactory_D3D11Interop
    : public SurfaceFactory
{
public:
    const RefPtr<DXInterop2Device> mInterop;

    static UniquePtr<SurfaceFactory_D3D11Interop> Create(GLContext* gl,
                                                         const SurfaceCaps& caps,
                                                         layers::ClientIPCAllocator* allocator,
                                                         const layers::TextureFlags& flags);

protected:
    SurfaceFactory_D3D11Interop(GLContext* gl, const SurfaceCaps& caps,
                                layers::ClientIPCAllocator* allocator,
                                const layers::TextureFlags& flags,
                                DXInterop2Device* interop);

public:
    virtual ~SurfaceFactory_D3D11Interop() override;

protected:
    virtual UniquePtr<SharedSurface> CreateShared(const gfx::IntSize& size) override {
        bool hasAlpha = mReadCaps.alpha;
        return SharedSurface_D3D11Interop::Create(mInterop, mGL, size, hasAlpha);
    }
};

} /* namespace gl */
} /* namespace mozilla */

#endif /* SHARED_SURFACE_D3D11_INTEROP_H_ */
