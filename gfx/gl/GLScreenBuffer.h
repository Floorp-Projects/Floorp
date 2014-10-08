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

#include "GLContextTypes.h"
#include "GLDefs.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/UniquePtr.h"
#include "SharedSurface.h"
#include "SurfaceTypes.h"

namespace mozilla {
namespace gl {

class GLContext;
class SharedSurface;
class ShSurfHandle;
class SurfaceFactory;

class DrawBuffer
{
public:
    // Fallible!
    // But it may return true with *out_buffer==nullptr if unneeded.
    static bool Create(GLContext* const gl,
                       const SurfaceCaps& caps,
                       const GLFormats& formats,
                       const gfx::IntSize& size,
                       UniquePtr<DrawBuffer>* out_buffer);

protected:
    GLContext* const mGL;
public:
    const gfx::IntSize mSize;
    const GLuint mFB;
protected:
    const GLuint mColorMSRB;
    const GLuint mDepthRB;
    const GLuint mStencilRB;

    DrawBuffer(GLContext* gl,
               const gfx::IntSize& size,
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
};

class ReadBuffer
{
public:
    // Infallible, always non-null.
    static UniquePtr<ReadBuffer> Create(GLContext* gl,
                                        const SurfaceCaps& caps,
                                        const GLFormats& formats,
                                        SharedSurface* surf);

protected:
    GLContext* const mGL;
public:
    const GLuint mFB;
protected:
    // mFB has the following attachments:
    const GLuint mDepthRB;
    const GLuint mStencilRB;
    // note no mColorRB here: this is provided by mSurf.
    SharedSurface* mSurf;

    ReadBuffer(GLContext* gl,
               GLuint fb,
               GLuint depthRB,
               GLuint stencilRB,
               SharedSurface* surf)
        : mGL(gl)
        , mFB(fb)
        , mDepthRB(depthRB)
        , mStencilRB(stencilRB)
        , mSurf(surf)
    {}

public:
    virtual ~ReadBuffer();

    // Cannot attach a surf of a different AttachType or Size than before.
    void Attach(SharedSurface* surf);

    const gfx::IntSize& Size() const;

    SharedSurface* SharedSurf() const {
        return mSurf;
    }
};


class GLScreenBuffer
{
public:
    // Infallible.
    static UniquePtr<GLScreenBuffer> Create(GLContext* gl,
                                            const gfx::IntSize& size,
                                            const SurfaceCaps& caps);

protected:
    GLContext* const mGL; // Owns us.
public:
    const SurfaceCaps mCaps;
protected:
    UniquePtr<SurfaceFactory> mFactory;

    RefPtr<ShSurfHandle> mBack;
    RefPtr<ShSurfHandle> mFront;

    UniquePtr<DrawBuffer> mDraw;
    UniquePtr<ReadBuffer> mRead;

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
                   UniquePtr<SurfaceFactory> factory)
        : mGL(gl)
        , mCaps(caps)
        , mFactory(Move(factory))
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

    SurfaceFactory* Factory() const {
        return mFactory.get();
    }

    ShSurfHandle* Front() const {
        return mFront;
    }

    SharedSurface* SharedSurf() const {
        MOZ_ASSERT(mRead);
        return mRead->SharedSurf();
    }

    bool ShouldPreserveBuffer() const {
        return mCaps.preserve;
    }

    GLuint DrawFB() const {
        if (!mDraw)
            return ReadFB();

        return mDraw->mFB;
    }

    GLuint ReadFB() const {
        return mRead->mFB;
    }

    void DeletingFB(GLuint fb);

    const gfx::IntSize& Size() const {
        MOZ_ASSERT(mRead);
        MOZ_ASSERT(!mDraw || mDraw->mSize == mRead->Size());
        return mRead->Size();
    }

    void BindAsFramebuffer(GLContext* const gl, GLenum target) const;

    void RequireBlit();
    void AssureBlitted();
    void AfterDrawCall();
    void BeforeReadCall();

    /**
     * Attempts to read pixels from the current bound framebuffer, if
     * it is backed by a SharedSurface.
     *
     * Returns true if the pixel data has been read back, false
     * otherwise.
     */
    bool ReadPixels(GLint x, GLint y, GLsizei width, GLsizei height,
                    GLenum format, GLenum type, GLvoid *pixels);

    // Morph changes the factory used to create surfaces.
    void Morph(UniquePtr<SurfaceFactory> newFactory);

protected:
    // Returns false on error or inability to resize.
    bool Swap(const gfx::IntSize& size);

public:
    bool PublishFrame(const gfx::IntSize& size);

    bool Resize(const gfx::IntSize& size);

    void Readback(SharedSurface* src, gfx::DataSourceSurface* dest);

protected:
    bool Attach(SharedSurface* surf, const gfx::IntSize& size);

    bool CreateDraw(const gfx::IntSize& size, UniquePtr<DrawBuffer>* out_buffer);
    UniquePtr<ReadBuffer> CreateRead(SharedSurface* surf);

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
    void BindFB_Internal(GLuint fb);
    void BindDrawFB_Internal(GLuint fb);
    void BindReadFB_Internal(GLuint fb);
};

}   // namespace gl
}   // namespace mozilla

#endif  // SCREEN_BUFFER_H_
