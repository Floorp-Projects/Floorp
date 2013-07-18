/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* SharedSurface abstracts an actual surface (can be a GL texture, but
 * not necessarily) that handles sharing.
 * Its specializations are:
 *     SharedSurface_Basic (client-side bitmap, does readback)
 *     SharedSurface_GLTexture
 *     SharedSurface_EGLImage
 *     SharedSurface_ANGLEShareHandle
 */

#ifndef SHARED_SURFACE_H_
#define SHARED_SURFACE_H_

#include "mozilla/StandardInteger.h"
#include "mozilla/Attributes.h"
#include "GLDefs.h"
#include "gfxPoint.h"
#include "SurfaceTypes.h"

namespace mozilla {
namespace gfx {

class SurfaceFactory;

class SharedSurface
{
protected:
    const SharedSurfaceType mType;
    const APITypeT mAPI;
    const AttachmentType mAttachType;
    const gfxIntSize mSize;
    const bool mHasAlpha;
    bool mIsLocked;

    SharedSurface(SharedSurfaceType type,
                  APITypeT api,
                  AttachmentType attachType,
                  const gfxIntSize& size,
                  bool hasAlpha)
        : mType(type)
        , mAPI(api)
        , mAttachType(attachType)
        , mSize(size)
        , mHasAlpha(hasAlpha)
        , mIsLocked(false)
    {
    }

public:
    virtual ~SharedSurface() {
    }

    static void Copy(SharedSurface* src, SharedSurface* dest,
                     SurfaceFactory* factory);

    // This locks the SharedSurface as the production buffer for the context.
    // This is needed by backends which use PBuffers and/or EGLSurfaces.
    virtual void LockProd() {
        MOZ_ASSERT(!mIsLocked);
        LockProdImpl();
        mIsLocked = true;
    }

    // Unlocking is harmless if we're already unlocked.
    virtual void UnlockProd() {
        if (!mIsLocked)
            return;

        UnlockProdImpl();
        mIsLocked = false;
    }

    virtual void LockProdImpl() = 0;
    virtual void UnlockProdImpl() = 0;

    virtual void Fence() = 0;
    virtual bool WaitSync() = 0;


    SharedSurfaceType Type() const {
        return mType;
    }

    APITypeT APIType() const {
        return mAPI;
    }

    AttachmentType AttachType() const {
        return mAttachType;
    }

    const gfxIntSize& Size() const {
        return mSize;
    }

    bool HasAlpha() const {
        return mHasAlpha;
    }


    // For use when AttachType is correct.
    virtual GLuint Texture() const {
        MOZ_ASSERT(AttachType() == AttachmentType::GLTexture);
        MOZ_CRASH("Did you forget to override this function?");
    }

    virtual GLenum TextureTarget() const {
        return Texture() ? LOCAL_GL_TEXTURE_2D : 0;
    }

    virtual GLuint Renderbuffer() const {
        MOZ_ASSERT(AttachType() == AttachmentType::GLRenderbuffer);
        MOZ_CRASH("Did you forget to override this function?");
    }
};

} /* namespace gfx */
} /* namespace mozilla */

#endif /* SHARED_SURFACE_H_ */
