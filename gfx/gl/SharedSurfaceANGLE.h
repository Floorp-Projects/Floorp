/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SHARED_SURFACE_ANGLE_H_
#define SHARED_SURFACE_ANGLE_H_

#include <windows.h>

#include "SharedSurface.h"

namespace mozilla {
namespace gl {

class GLContext;
class GLLibraryEGL;

class SharedSurface_ANGLEShareHandle
    : public SharedSurface
{
public:
    static SharedSurface_ANGLEShareHandle* Create(GLContext* gl,
                                                  EGLContext context, EGLConfig config,
                                                  const gfx::IntSize& size,
                                                  bool hasAlpha);

    static SharedSurface_ANGLEShareHandle* Cast(SharedSurface* surf) {
        MOZ_ASSERT(surf->mType == SharedSurfaceType::EGLSurfaceANGLE);

        return (SharedSurface_ANGLEShareHandle*)surf;
    }

protected:
    GLLibraryEGL* const mEGL;
    const EGLContext mContext;
    const EGLSurface mPBuffer;
    const HANDLE mShareHandle;

    SharedSurface_ANGLEShareHandle(GLContext* gl,
                                   GLLibraryEGL* egl,
                                   const gfx::IntSize& size,
                                   bool hasAlpha,
                                   EGLContext context,
                                   EGLSurface pbuffer,
                                   HANDLE shareHandle)
        : SharedSurface(SharedSurfaceType::EGLSurfaceANGLE,
                        AttachmentType::Screen,
                        gl,
                        size,
                        hasAlpha)
        , mEGL(egl)
        , mContext(context)
        , mPBuffer(pbuffer)
        , mShareHandle(shareHandle)
    {}

    EGLDisplay Display();

public:
    virtual ~SharedSurface_ANGLEShareHandle();

    virtual void LockProdImpl() MOZ_OVERRIDE;
    virtual void UnlockProdImpl() MOZ_OVERRIDE;

    virtual void Fence() MOZ_OVERRIDE;
    virtual bool WaitSync() MOZ_OVERRIDE;

    // Implementation-specific functions below:
    HANDLE GetShareHandle() {
        return mShareHandle;
    }
};



class SurfaceFactory_ANGLEShareHandle
    : public SurfaceFactory
{
protected:
    GLContext* const mProdGL;
    GLLibraryEGL* const mEGL;
    EGLContext mContext;
    EGLConfig mConfig;

public:
    static SurfaceFactory_ANGLEShareHandle* Create(GLContext* gl,
                                                   const SurfaceCaps& caps);

protected:
    SurfaceFactory_ANGLEShareHandle(GLContext* gl,
                                    GLLibraryEGL* egl,
                                    const SurfaceCaps& caps);

    virtual SharedSurface* CreateShared(const gfx::IntSize& size) MOZ_OVERRIDE {
        bool hasAlpha = mReadCaps.alpha;
        return SharedSurface_ANGLEShareHandle::Create(mProdGL,
                                                      mContext, mConfig,
                                                      size, hasAlpha);
    }
};

} /* namespace gfx */
} /* namespace mozilla */

#endif /* SHARED_SURFACE_ANGLE_H_ */
