/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SHARED_SURFACE_EGL_H_
#define SHARED_SURFACE_EGL_H_

#include "SharedSurfaceGL.h"
#include "SurfaceFactory.h"
#include "GLLibraryEGL.h"
#include "SurfaceTypes.h"
#include "mozilla/Attributes.h"
#include "mozilla/Mutex.h"

namespace mozilla {
namespace gl {

class GLContext;
class TextureGarbageBin;

class SharedSurface_EGLImage
    : public SharedSurface_GL
{
public:
    static SharedSurface_EGLImage* Create(GLContext* prodGL,
                                                  const GLFormats& formats,
                                                  const gfxIntSize& size,
                                                  bool hasAlpha,
                                                  EGLContext context);

    static SharedSurface_EGLImage* Cast(SharedSurface* surf) {
        MOZ_ASSERT(surf->Type() == SharedSurfaceType::EGLImageShare);

        return (SharedSurface_EGLImage*)surf;
    }

protected:
    mutable Mutex mMutex;
    GLLibraryEGL* const mEGL;
    const GLFormats mFormats;
    GLuint mProdTex;
    nsRefPtr<gfxImageSurface> mPixels;
    GLuint mProdTexForPipe; // Moves to mProdTex when mPipeActive becomes true.
    EGLImage mImage;
    GLContext* mCurConsGL;
    GLuint mConsTex;
    nsRefPtr<TextureGarbageBin> mGarbageBin;
    EGLSync mSync;
    bool mPipeFailed;   // Pipe creation failed, and has been abandoned.
    bool mPipeComplete; // Pipe connects (mPipeActive ? mProdTex : mProdTexForPipe) to mConsTex.
    bool mPipeActive;   // Pipe is complete and in use for production.

    SharedSurface_EGLImage(GLContext* gl,
                           GLLibraryEGL* egl,
                           const gfxIntSize& size,
                           bool hasAlpha,
                           const GLFormats& formats,
                           GLuint prodTex);

    EGLDisplay Display() const;

    static bool HasExtensions(GLLibraryEGL* egl, GLContext* gl);

public:
    virtual ~SharedSurface_EGLImage();

    virtual void LockProdImpl();
    virtual void UnlockProdImpl() {}


    virtual void Fence();
    virtual bool WaitSync();


    virtual GLuint Texture() const {
        return mProdTex;
    }

    // Implementation-specific functions below:
    // Returns 0 if the pipe isn't ready. If 0, use GetPixels below.
    GLuint AcquireConsumerTexture(GLContext* consGL);

    // Will be void if AcquireConsumerTexture returns non-zero.
    gfxImageSurface* GetPixels() const;
};



class SurfaceFactory_EGLImage
    : public SurfaceFactory_GL
{
public:
    // Infallible:
    static SurfaceFactory_EGLImage* Create(GLContext* prodGL,
                                           const SurfaceCaps& caps);

protected:
    const EGLContext mContext;

    SurfaceFactory_EGLImage(GLContext* prodGL,
                            EGLContext context,
                            const SurfaceCaps& caps)
        : SurfaceFactory_GL(prodGL, SharedSurfaceType::EGLImageShare, caps)
        , mContext(context)
    {}

public:
    virtual SharedSurface* CreateShared(const gfxIntSize& size) {
        bool hasAlpha = mReadCaps.alpha;
        return SharedSurface_EGLImage::Create(mGL, mFormats, size, hasAlpha, mContext);
    }
};

} /* namespace gfx */
} /* namespace mozilla */

#endif /* SHARED_SURFACE_EGL_H_ */
