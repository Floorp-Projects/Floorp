/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SHARED_SURFACE_ANGLE_H_
#define SHARED_SURFACE_ANGLE_H_

#include "SharedSurfaceGL.h"
#include "SurfaceFactory.h"
#include "GLLibraryEGL.h"
#include "SurfaceTypes.h"

#include <windows.h>
#include <d3d10_1.h>

namespace mozilla {
namespace gl {

class GLContext;

class SharedSurface_ANGLEShareHandle
    : public SharedSurface_GL
{
public:
    static SharedSurface_ANGLEShareHandle* Create(GLContext* gl, ID3D10Device1* d3d,
                                                  EGLContext context, EGLConfig config,
                                                  const gfx::IntSize& size,
                                                  bool hasAlpha);

    static SharedSurface_ANGLEShareHandle* Cast(SharedSurface* surf) {
        MOZ_ASSERT(surf->Type() == SharedSurfaceType::EGLSurfaceANGLE);

        return (SharedSurface_ANGLEShareHandle*)surf;
    }

protected:
    GLLibraryEGL* const mEGL;
    const EGLContext mContext;
    const EGLSurface mPBuffer;
    nsRefPtr<ID3D10Texture2D> mTexture;
    nsRefPtr<ID3D10ShaderResourceView> mSRV;

    SharedSurface_ANGLEShareHandle(GLContext* gl,
                                   GLLibraryEGL* egl,
                                   const gfx::IntSize& size,
                                   bool hasAlpha,
                                   EGLContext context,
                                   EGLSurface pbuffer,
                                   ID3D10Texture2D* texture,
                                   ID3D10ShaderResourceView* srv)
        : SharedSurface_GL(SharedSurfaceType::EGLSurfaceANGLE,
                           AttachmentType::Screen,
                           gl,
                           size,
                           hasAlpha)
        , mEGL(egl)
        , mContext(context)
        , mPBuffer(pbuffer)
        , mTexture(texture)
        , mSRV(srv)
    {}

    EGLDisplay Display();

public:
    virtual ~SharedSurface_ANGLEShareHandle();

    virtual void LockProdImpl() MOZ_OVERRIDE;
    virtual void UnlockProdImpl() MOZ_OVERRIDE;

    virtual void Fence() MOZ_OVERRIDE;
    virtual bool WaitSync() MOZ_OVERRIDE;

    // Implementation-specific functions below:
    ID3D10ShaderResourceView* GetSRV() {
        return mSRV;
    }
};



class SurfaceFactory_ANGLEShareHandle
    : public SurfaceFactory_GL
{
protected:
    GLContext* const mProdGL;
    GLLibraryEGL* const mEGL;
    nsRefPtr<ID3D10Device1> mConsD3D;
    EGLContext mContext;
    EGLConfig mConfig;

public:
    static SurfaceFactory_ANGLEShareHandle* Create(GLContext* gl,
                                                   ID3D10Device1* d3d,
                                                   const SurfaceCaps& caps);

protected:
    SurfaceFactory_ANGLEShareHandle(GLContext* gl,
                                    GLLibraryEGL* egl,
                                    ID3D10Device1* d3d,
                                    const SurfaceCaps& caps);

    virtual SharedSurface* CreateShared(const gfx::IntSize& size) MOZ_OVERRIDE {
        bool hasAlpha = mReadCaps.alpha;
        return SharedSurface_ANGLEShareHandle::Create(mProdGL, mConsD3D,
                                                      mContext, mConfig,
                                                      size, hasAlpha);
    }
};

} /* namespace gfx */
} /* namespace mozilla */

#endif /* SHARED_SURFACE_ANGLE_H_ */
