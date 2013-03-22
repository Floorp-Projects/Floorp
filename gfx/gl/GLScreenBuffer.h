/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* GLScreenBuffer is the abstraction for the "default framebuffer" used
 * by an offscreen GLContext. Since it's only for offscreen GLContext's,
 * it's only useful for things like WebGL, and is NOT used by the
 * compositor's GLContext. Remember that GLContext provides an abstraction
 * so that even if you want to draw to the 'screen', even if that's not
 * actually the screen, just draw to 0. This GLScreenBuffer class takes the
 * logic handling out of GLContext.
*/

#ifndef SCREEN_BUFFER_H_
#define SCREEN_BUFFER_H_

#include "SurfaceTypes.h"
#include "GLContextTypes.h"
#include "GLDefs.h"
#include "gfxPoint.h"

// Forwards:
class gfxImageSurface;

namespace mozilla {
    namespace gfx {
        class SurfaceStream;
        class SharedSurface;
    }
    namespace gl {
        class GLContext;
        class SharedSurface_GL;
        class SurfaceFactory_GL;
    }
}

namespace mozilla {
namespace gl {

class DrawBuffer
{
protected:
    typedef struct gfx::SurfaceCaps SurfaceCaps;

public:
    // Infallible, may return null if unneeded.
    static DrawBuffer* Create(GLContext* const gl,
                              const SurfaceCaps& caps,
                              const GLFormats& formats,
                              const gfxIntSize& size);

protected:
    GLContext* const mGL;
    const gfxIntSize mSize;
    const GLuint mFB;
    const GLuint mColorMSRB;
    const GLuint mDepthRB;
    const GLuint mStencilRB;

    DrawBuffer(GLContext* gl,
               const gfxIntSize& size,
               GLuint fb,
               GLuint colorMSRB,
               GLuint depthRB,
               GLuint stencilRB)
        : mGL(gl)
        , mSize(size)
        , mFB(fb)
        , mColorMSRB(colorMSRB)
        , mDepthRB(depthRB)
        , mStencilRB(stencilRB)
    {}

public:
    virtual ~DrawBuffer();

    const gfxIntSize& Size() const {
        return mSize;
    }

    GLuint FB() const {
        return mFB;
    }
};

class ReadBuffer
{
protected:
    typedef struct gfx::SurfaceCaps SurfaceCaps;

public:
    // Infallible, always non-null.
    static ReadBuffer* Create(GLContext* gl,
                              const SurfaceCaps& caps,
                              const GLFormats& formats,
                              SharedSurface_GL* surf);

protected:
    GLContext* const mGL;

    const GLuint mFB;
    // mFB has the following attachments:
    const GLuint mDepthRB;
    const GLuint mStencilRB;
    // note no mColorRB here: this is provided by mSurf.
    SharedSurface_GL* mSurf; // Owned by GLScreenBuffer's SurfaceStream.

    ReadBuffer(GLContext* gl,
               GLuint fb,
               GLuint depthRB,
               GLuint stencilRB,
               SharedSurface_GL* surf)
        : mGL(gl)
        , mFB(fb)
        , mDepthRB(depthRB)
        , mStencilRB(stencilRB)
        , mSurf(surf)
    {}

public:
    virtual ~ReadBuffer();

    // Cannot attach a surf of a different AttachType or Size than before.
    void Attach(SharedSurface_GL* surf);

    const gfxIntSize& Size() const;

    GLuint FB() const {
        return mFB;
    }

    SharedSurface_GL* SharedSurf() const {
        return mSurf;
    }
};


class GLScreenBuffer
{
protected:
    typedef class gfx::SurfaceStream SurfaceStream;
    typedef class gfx::SharedSurface SharedSurface;
    typedef gfx::SurfaceStreamType SurfaceStreamType;
    typedef gfx::SharedSurfaceType SharedSurfaceType;
    typedef struct gfx::SurfaceCaps SurfaceCaps;

public:
    // Infallible.
    static GLScreenBuffer* Create(GLContext* gl,
                                  const gfxIntSize& size,
                                  const SurfaceCaps& caps);

protected:
    GLContext* const mGL;         // Owns us.
    SurfaceCaps mCaps;
    SurfaceFactory_GL* mFactory;  // Owned by us.
    SurfaceStream* mStream;       // Owned by us.

    DrawBuffer* mDraw;            // Owned by us.
    ReadBuffer* mRead;            // Owned by us.

    bool mNeedsBlit;

    // Below are the parts that help us pretend to be framebuffer 0:
    GLuint mUserDrawFB;
    GLuint mUserReadFB;
    GLuint mInternalDrawFB;
    GLuint mInternalReadFB;

#ifdef DEBUG
    bool mInInternalMode_DrawFB;
    bool mInInternalMode_ReadFB;
#endif

    GLScreenBuffer(GLContext* gl,
                   const SurfaceCaps& caps,
                   SurfaceFactory_GL* factory,
                   SurfaceStream* stream)
        : mGL(gl)
        , mCaps(caps)
        , mFactory(factory)
        , mStream(stream)
        , mDraw(nullptr)
        , mRead(nullptr)
        , mNeedsBlit(true)
        , mUserDrawFB(0)
        , mUserReadFB(0)
        , mInternalDrawFB(0)
        , mInternalReadFB(0)
#ifdef DEBUG
        , mInInternalMode_DrawFB(true)
        , mInInternalMode_ReadFB(true)
#endif
    {}

public:
    virtual ~GLScreenBuffer();

    SurfaceStream* Stream() const {
        return mStream;
    }

    SurfaceFactory_GL* Factory() const {
        return mFactory;
    }

    SharedSurface_GL* SharedSurf() const {
        MOZ_ASSERT(mRead);
        return mRead->SharedSurf();
    }

    bool PreserveBuffer() const {
        return mCaps.preserve;
    }

    const SurfaceCaps& Caps() const {
        return mCaps;
    }

    GLuint DrawFB() const {
        if (!mDraw)
            return ReadFB();

        return mDraw->FB();
    }

    GLuint ReadFB() const {
        return mRead->FB();
    }

    void DeletingFB(GLuint fb);

    const gfxIntSize& Size() const {
        MOZ_ASSERT(mRead);
        MOZ_ASSERT(!mDraw || mDraw->Size() == mRead->Size());
        return mRead->Size();
    }

    void BindAsFramebuffer(GLContext* const gl, GLenum target) const;

    void RequireBlit();
    void AssureBlitted();
    void AfterDrawCall();
    void BeforeReadCall();

    /* Morph swaps out our SurfaceStream mechanism and replaces it with
     * one best suited to our platform and compositor configuration.
     *
     * Must be called on the producing thread.
     * We haven't made any guarantee that rendering is actually
     * done when Morph is run, just that it can't run concurrently
     * with rendering. This means that we can't just drop the contents
     * of the buffer, since we may only be partially done rendering.
     *
     * Once you pass newFactory into Morph, newFactory will be owned by
     * GLScreenBuffer, so `forget` any references to it that still exist.
     */
    void Morph(SurfaceFactory_GL* newFactory, SurfaceStreamType streamType);

protected:
    // Returns false on error or inability to resize.
    bool Swap(const gfxIntSize& size);

public:
    bool PublishFrame(const gfxIntSize& size);

    bool Resize(const gfxIntSize& size);

    void Readback(SharedSurface_GL* src, gfxImageSurface* dest);

protected:
    void Attach(SharedSurface* surface, const gfxIntSize& size);

    DrawBuffer* CreateDraw(const gfxIntSize& size);
    ReadBuffer* CreateRead(SharedSurface_GL* surf);

public:
    /* `fb` in these functions is the framebuffer the GLContext is hoping to
     * bind. When this is 0, we intercept the call and bind our own
     * framebuffers. As a client of these functions, just bind 0 when you want
     * to draw to the default framebuffer/'screen'.
     */
    void BindFB(GLuint fb);
    void BindDrawFB(GLuint fb);
    void BindReadFB(GLuint fb);
    GLuint GetFB() const;
    GLuint GetDrawFB() const;
    GLuint GetReadFB() const;

    // Here `fb` is the actual framebuffer you want bound. Binding 0 will
    // bind the (generally useless) default framebuffer.
    void BindDrawFB_Internal(GLuint fb);
    void BindReadFB_Internal(GLuint fb);
};

}   // namespace gl
}   // namespace mozilla

#endif  // SCREEN_BUFFER_H_
