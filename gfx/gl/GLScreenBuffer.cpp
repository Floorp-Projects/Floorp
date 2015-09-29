/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLScreenBuffer.h"

#include <cstring>
#include "CompositorTypes.h"
#include "GLContext.h"
#include "GLBlitHelper.h"
#include "GLReadTexImageHelper.h"
#include "SharedSurfaceGL.h"
#ifdef MOZ_WIDGET_GONK
#include "SharedSurfaceGralloc.h"
#include "nsXULAppAPI.h"
#endif
#ifdef XP_MACOSX
#include "SharedSurfaceIO.h"
#endif
#include "ScopedGLHelpers.h"
#include "gfx2DGlue.h"
#include "../layers/ipc/ShadowLayers.h"
#include "mozilla/layers/TextureClientSharedSurface.h"

namespace mozilla {
namespace gl {

using gfx::SurfaceFormat;

UniquePtr<GLScreenBuffer>
GLScreenBuffer::Create(GLContext* gl,
                       const gfx::IntSize& size,
                       const SurfaceCaps& caps)
{
    UniquePtr<GLScreenBuffer> ret;
    if (caps.antialias &&
        !gl->IsSupported(GLFeature::framebuffer_multisample))
    {
        return Move(ret);
    }

    layers::TextureFlags flags = layers::TextureFlags::ORIGIN_BOTTOM_LEFT;
    if (!caps.premultAlpha) {
        flags |= layers::TextureFlags::NON_PREMULTIPLIED;
    }

    UniquePtr<SurfaceFactory> factory = MakeUnique<SurfaceFactory_Basic>(gl, caps, flags);

    ret.reset( new GLScreenBuffer(gl, caps, Move(factory)) );
    return Move(ret);
}

GLScreenBuffer::GLScreenBuffer(GLContext* gl,
                               const SurfaceCaps& caps,
                               UniquePtr<SurfaceFactory> factory)
    : mGL(gl)
    , mCaps(caps)
    , mFactory(Move(factory))
    , mNeedsBlit(true)
    , mUserReadBufferMode(LOCAL_GL_BACK)
    , mUserDrawBufferMode(LOCAL_GL_BACK)
    , mUserDrawFB(0)
    , mUserReadFB(0)
    , mInternalDrawFB(0)
    , mInternalReadFB(0)
#ifdef DEBUG
    , mInInternalMode_DrawFB(true)
    , mInInternalMode_ReadFB(true)
#endif
{ }

GLScreenBuffer::~GLScreenBuffer()
{
    mFactory = nullptr;
    mDraw = nullptr;
    mRead = nullptr;
}

void
GLScreenBuffer::BindAsFramebuffer(GLContext* const gl, GLenum target) const
{
    GLuint drawFB = DrawFB();
    GLuint readFB = ReadFB();

    if (!gl->IsSupported(GLFeature::framebuffer_blit)) {
        MOZ_ASSERT(drawFB == readFB);
        gl->raw_fBindFramebuffer(target, readFB);
        return;
    }

    switch (target) {
    case LOCAL_GL_FRAMEBUFFER:
        gl->raw_fBindFramebuffer(LOCAL_GL_DRAW_FRAMEBUFFER_EXT, drawFB);
        gl->raw_fBindFramebuffer(LOCAL_GL_READ_FRAMEBUFFER_EXT, readFB);
        break;

    case LOCAL_GL_DRAW_FRAMEBUFFER_EXT:
        if (!gl->IsSupported(GLFeature::framebuffer_blit))
            NS_WARNING("DRAW_FRAMEBUFFER requested but unavailable.");

        gl->raw_fBindFramebuffer(LOCAL_GL_DRAW_FRAMEBUFFER_EXT, drawFB);
        break;

    case LOCAL_GL_READ_FRAMEBUFFER_EXT:
        if (!gl->IsSupported(GLFeature::framebuffer_blit))
            NS_WARNING("READ_FRAMEBUFFER requested but unavailable.");

        gl->raw_fBindFramebuffer(LOCAL_GL_READ_FRAMEBUFFER_EXT, readFB);
        break;

    default:
        MOZ_CRASH("Bad `target` for BindFramebuffer.");
    }
}

void
GLScreenBuffer::BindFB(GLuint fb)
{
    GLuint drawFB = DrawFB();
    GLuint readFB = ReadFB();

    mUserDrawFB = fb;
    mUserReadFB = fb;
    mInternalDrawFB = (fb == 0) ? drawFB : fb;
    mInternalReadFB = (fb == 0) ? readFB : fb;

    if (mInternalDrawFB == mInternalReadFB) {
        mGL->raw_fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, mInternalDrawFB);
    } else {
        MOZ_ASSERT(mGL->IsSupported(GLFeature::framebuffer_blit));
        mGL->raw_fBindFramebuffer(LOCAL_GL_DRAW_FRAMEBUFFER_EXT, mInternalDrawFB);
        mGL->raw_fBindFramebuffer(LOCAL_GL_READ_FRAMEBUFFER_EXT, mInternalReadFB);
    }

#ifdef DEBUG
    mInInternalMode_DrawFB = false;
    mInInternalMode_ReadFB = false;
#endif
}

void
GLScreenBuffer::BindDrawFB(GLuint fb)
{
    MOZ_ASSERT(mGL->IsSupported(GLFeature::framebuffer_blit));

    GLuint drawFB = DrawFB();
    mUserDrawFB = fb;
    mInternalDrawFB = (fb == 0) ? drawFB : fb;

    mGL->raw_fBindFramebuffer(LOCAL_GL_DRAW_FRAMEBUFFER_EXT, mInternalDrawFB);

#ifdef DEBUG
    mInInternalMode_DrawFB = false;
#endif
}

void
GLScreenBuffer::BindReadFB(GLuint fb)
{
    MOZ_ASSERT(mGL->IsSupported(GLFeature::framebuffer_blit));

    GLuint readFB = ReadFB();
    mUserReadFB = fb;
    mInternalReadFB = (fb == 0) ? readFB : fb;

    mGL->raw_fBindFramebuffer(LOCAL_GL_READ_FRAMEBUFFER_EXT, mInternalReadFB);

#ifdef DEBUG
    mInInternalMode_ReadFB = false;
#endif
}

void
GLScreenBuffer::BindFB_Internal(GLuint fb)
{
    mInternalDrawFB = mUserDrawFB = fb;
    mInternalReadFB = mUserReadFB = fb;
    mGL->raw_fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, mInternalDrawFB);

#ifdef DEBUG
    mInInternalMode_DrawFB = true;
    mInInternalMode_ReadFB = true;
#endif
}

void
GLScreenBuffer::BindDrawFB_Internal(GLuint fb)
{
    MOZ_ASSERT(mGL->IsSupported(GLFeature::framebuffer_blit));

    mInternalDrawFB = mUserDrawFB = fb;
    mGL->raw_fBindFramebuffer(LOCAL_GL_DRAW_FRAMEBUFFER_EXT, mInternalDrawFB);

#ifdef DEBUG
    mInInternalMode_DrawFB = true;
#endif
}

void
GLScreenBuffer::BindReadFB_Internal(GLuint fb)
{
    MOZ_ASSERT(mGL->IsSupported(GLFeature::framebuffer_blit));

    mInternalReadFB = mUserReadFB = fb;
    mGL->raw_fBindFramebuffer(LOCAL_GL_READ_FRAMEBUFFER_EXT, mInternalReadFB);

#ifdef DEBUG
    mInInternalMode_ReadFB = true;
#endif
}


GLuint
GLScreenBuffer::GetDrawFB() const
{
#ifdef DEBUG
    MOZ_ASSERT(mGL->IsCurrent());
    MOZ_ASSERT(!mInInternalMode_DrawFB);

    // Don't need a branch here, because:
    // LOCAL_GL_DRAW_FRAMEBUFFER_BINDING_EXT == LOCAL_GL_FRAMEBUFFER_BINDING == 0x8CA6
    // We use raw_ here because this is debug code and we need to see what
    // the driver thinks.
    GLuint actual = 0;
    mGL->raw_fGetIntegerv(LOCAL_GL_DRAW_FRAMEBUFFER_BINDING_EXT, (GLint*)&actual);

    GLuint predicted = mInternalDrawFB;
    if (predicted != actual) {
        printf_stderr("Misprediction: Bound draw FB predicted: %d. Was: %d.\n",
                      predicted, actual);
        MOZ_ASSERT(false, "Draw FB binding misprediction!");
    }
#endif

    return mUserDrawFB;
}

GLuint
GLScreenBuffer::GetReadFB() const
{
#ifdef DEBUG
    MOZ_ASSERT(mGL->IsCurrent());
    MOZ_ASSERT(!mInInternalMode_ReadFB);

    // We use raw_ here because this is debug code and we need to see what
    // the driver thinks.
    GLuint actual = 0;
    if (mGL->IsSupported(GLFeature::framebuffer_blit))
        mGL->raw_fGetIntegerv(LOCAL_GL_READ_FRAMEBUFFER_BINDING_EXT, (GLint*)&actual);
    else
        mGL->raw_fGetIntegerv(LOCAL_GL_FRAMEBUFFER_BINDING, (GLint*)&actual);

    GLuint predicted = mInternalReadFB;
    if (predicted != actual) {
        printf_stderr("Misprediction: Bound read FB predicted: %d. Was: %d.\n",
                      predicted, actual);
        MOZ_ASSERT(false, "Read FB binding misprediction!");
    }
#endif

    return mUserReadFB;
}

GLuint
GLScreenBuffer::GetFB() const
{
    MOZ_ASSERT(GetDrawFB() == GetReadFB());
    return GetDrawFB();
}


void
GLScreenBuffer::DeletingFB(GLuint fb)
{
    if (fb == mInternalDrawFB) {
        mInternalDrawFB = 0;
        mUserDrawFB = 0;
    }
    if (fb == mInternalReadFB) {
        mInternalReadFB = 0;
        mUserReadFB = 0;
    }
}


void
GLScreenBuffer::AfterDrawCall()
{
    if (mUserDrawFB != 0)
        return;

    RequireBlit();
}

void
GLScreenBuffer::BeforeReadCall()
{
    if (mUserReadFB != 0)
        return;

    AssureBlitted();
}

bool
GLScreenBuffer::CopyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x,
                               GLint y, GLsizei width, GLsizei height, GLint border)
{
    SharedSurface* surf;
    if (GetReadFB() == 0) {
        surf = SharedSurf();
    } else {
        surf = mGL->mFBOMapping[GetReadFB()];
    }
    if (surf) {
        return surf->CopyTexImage2D(target, level, internalformat,  x, y, width, height, border);
    }

    return false;
}

bool
GLScreenBuffer::ReadPixels(GLint x, GLint y,
                           GLsizei width, GLsizei height,
                           GLenum format, GLenum type,
                           GLvoid* pixels)
{
    // If the currently bound framebuffer is backed by a SharedSurface
    // then it might want to override how we read pixel data from it.
    // This is normally only the default framebuffer, but we can also
    // have SharedSurfaces bound to other framebuffers when doing
    // readback for BasicLayers.
    SharedSurface* surf;
    if (GetReadFB() == 0) {
        surf = SharedSurf();
    } else {
        surf = mGL->mFBOMapping[GetReadFB()];
    }
    if (surf) {
        return surf->ReadPixels(x, y, width, height, format, type, pixels);
    }

    return false;
}

void
GLScreenBuffer::RequireBlit()
{
    mNeedsBlit = true;
}

void
GLScreenBuffer::AssureBlitted()
{
    if (!mNeedsBlit)
        return;

    if (mDraw) {
        GLuint drawFB = DrawFB();
        GLuint readFB = ReadFB();

        MOZ_ASSERT(drawFB != 0);
        MOZ_ASSERT(drawFB != readFB);
        MOZ_ASSERT(mGL->IsSupported(GLFeature::framebuffer_blit));
        MOZ_ASSERT(mDraw->mSize == mRead->Size());

        ScopedBindFramebuffer boundFB(mGL);
        ScopedGLState scissor(mGL, LOCAL_GL_SCISSOR_TEST, false);

        BindReadFB_Internal(drawFB);
        BindDrawFB_Internal(readFB);

        const gfx::IntSize&  srcSize = mDraw->mSize;
        const gfx::IntSize& destSize = mRead->Size();

        mGL->raw_fBlitFramebuffer(0, 0,  srcSize.width,  srcSize.height,
                                  0, 0, destSize.width, destSize.height,
                                  LOCAL_GL_COLOR_BUFFER_BIT,
                                  LOCAL_GL_NEAREST);
        // Done!
    }

    mNeedsBlit = false;
}

void
GLScreenBuffer::Morph(UniquePtr<SurfaceFactory> newFactory)
{
    MOZ_ASSERT(newFactory);
    mFactory = Move(newFactory);
}

bool
GLScreenBuffer::Attach(SharedSurface* surf, const gfx::IntSize& size)
{
    ScopedBindFramebuffer autoFB(mGL);

    if (mRead && SharedSurf())
        SharedSurf()->UnlockProd();

    surf->LockProd();

    if (mRead &&
        surf->mAttachType == SharedSurf()->mAttachType &&
        size == Size())
    {
        // Same size, same type, ready for reuse!
        mRead->Attach(surf);
    } else {
        // Else something changed, so resize:
        UniquePtr<DrawBuffer> draw;
        bool drawOk = true;

        /* Don't change out the draw buffer unless we resize. In the
         * preserveDrawingBuffer:true case, prior contents of the buffer must
         * be retained. If we're using a draw buffer, it's an MSAA buffer, so
         * even if we copy the previous frame into the (single-sampled) read
         * buffer, if we need to re-resolve from draw to read (as triggered by
         * drawing), we'll need the old MSAA content to still be in the draw
         * buffer.
         */
        if (!mDraw || size != Size())
            drawOk = CreateDraw(size, &draw);  // Can be null.

        UniquePtr<ReadBuffer> read = CreateRead(surf);
        bool readOk = !!read;

        if (!drawOk || !readOk) {
            surf->UnlockProd();

            return false;
        }

        if (draw)
            mDraw = Move(draw);

        mRead = Move(read);
    }

    // Check that we're all set up.
    MOZ_ASSERT(SharedSurf() == surf);

    // Update the ReadBuffer mode.
    if (mGL->IsSupported(gl::GLFeature::read_buffer)) {
        BindFB(0);
        mRead->SetReadBuffer(mUserReadBufferMode);
    }

    // Update the DrawBuffer mode.
    if (mGL->IsSupported(gl::GLFeature::draw_buffers)) {
        BindFB(0);
        SetDrawBuffer(mUserDrawBufferMode);
    }

    RequireBlit();

    return true;
}

bool
GLScreenBuffer::Swap(const gfx::IntSize& size)
{
    RefPtr<SharedSurfaceTextureClient> newBack = mFactory->NewTexClient(size);
    if (!newBack)
        return false;

    if (!Attach(newBack->Surf(), size))
        return false;
    // Attach was successful.

    mFront = mBack;
    mBack = newBack;

    if (mBack) {
        mBack->Surf()->ProducerAcquire();
    }

    if (ShouldPreserveBuffer() &&
        mFront &&
        mBack &&
        !mDraw)
    {
        auto src  = mFront->Surf();
        auto dest = mBack->Surf();

        //uint32_t srcPixel = ReadPixel(src);
        //uint32_t destPixel = ReadPixel(dest);
        //printf_stderr("Before: src: 0x%08x, dest: 0x%08x\n", srcPixel, destPixel);
#ifdef DEBUG
        GLContext::LocalErrorScope errorScope(*mGL);
#endif

        SharedSurface::ProdCopy(src, dest, mFactory.get());

#ifdef DEBUG
        MOZ_ASSERT(!errorScope.GetError());
#endif

        //srcPixel = ReadPixel(src);
        //destPixel = ReadPixel(dest);
        //printf_stderr("After: src: 0x%08x, dest: 0x%08x\n", srcPixel, destPixel);
    }

    // XXX: We would prefer to fence earlier on platforms that don't need
    // the full ProducerAcquire/ProducerRelease semantics, so that the fence
    // doesn't include the copy operation. Unfortunately, the current API
    // doesn't expose a good way to do that.
    if (mFront) {
        mFront->Surf()->ProducerRelease();
    }

    return true;
}

bool
GLScreenBuffer::PublishFrame(const gfx::IntSize& size)
{
    AssureBlitted();

    bool good = Swap(size);
    return good;
}

bool
GLScreenBuffer::Resize(const gfx::IntSize& size)
{
    RefPtr<SharedSurfaceTextureClient> newBack = mFactory->NewTexClient(size);
    if (!newBack)
        return false;

    if (!Attach(newBack->Surf(), size))
        return false;

    if (mBack)
        mBack->Surf()->ProducerRelease();

    mBack = newBack;

    mBack->Surf()->ProducerAcquire();

    return true;
}

bool
GLScreenBuffer::CreateDraw(const gfx::IntSize& size,
                           UniquePtr<DrawBuffer>* out_buffer)
{
    GLContext* gl = mFactory->mGL;
    const GLFormats& formats = mFactory->mFormats;
    const SurfaceCaps& caps = mFactory->DrawCaps();

    return DrawBuffer::Create(gl, caps, formats, size, out_buffer);
}

UniquePtr<ReadBuffer>
GLScreenBuffer::CreateRead(SharedSurface* surf)
{
    GLContext* gl = mFactory->mGL;
    const GLFormats& formats = mFactory->mFormats;
    const SurfaceCaps& caps = mFactory->ReadCaps();

    return ReadBuffer::Create(gl, caps, formats, surf);
}

void
GLScreenBuffer::SetDrawBuffer(GLenum mode)
{
    MOZ_ASSERT(mGL->IsSupported(gl::GLFeature::draw_buffers));
    MOZ_ASSERT(GetDrawFB() == 0);

    if (!mGL->IsSupported(GLFeature::draw_buffers))
        return;

    mUserDrawBufferMode = mode;

    GLuint fb = mDraw ? mDraw->mFB : mRead->mFB;
    GLenum internalMode;

    switch (mode) {
    case LOCAL_GL_BACK:
        internalMode = (fb == 0) ? LOCAL_GL_BACK
                                 : LOCAL_GL_COLOR_ATTACHMENT0;
        break;

    case LOCAL_GL_NONE:
        internalMode = LOCAL_GL_NONE;
        break;

    default:
        MOZ_CRASH("Bad value.");
    }

    mGL->MakeCurrent();
    mGL->fDrawBuffers(1, &internalMode);
}

void
GLScreenBuffer::SetReadBuffer(GLenum mode)
{
    MOZ_ASSERT(mGL->IsSupported(gl::GLFeature::read_buffer));
    MOZ_ASSERT(GetReadFB() == 0);

    mUserReadBufferMode = mode;
    mRead->SetReadBuffer(mUserReadBufferMode);
}

bool
GLScreenBuffer::IsDrawFramebufferDefault() const
{
    if (!mDraw)
        return IsReadFramebufferDefault();
    return mDraw->mFB == 0;
}

bool
GLScreenBuffer::IsReadFramebufferDefault() const
{
    return SharedSurf()->mAttachType == AttachmentType::Screen;
}

////////////////////////////////////////////////////////////////////////
// DrawBuffer

bool
DrawBuffer::Create(GLContext* const gl,
                   const SurfaceCaps& caps,
                   const GLFormats& formats,
                   const gfx::IntSize& size,
                   UniquePtr<DrawBuffer>* out_buffer)
{
    MOZ_ASSERT(out_buffer);
    *out_buffer = nullptr;

    if (!caps.color) {
        MOZ_ASSERT(!caps.alpha && !caps.depth && !caps.stencil);

        // Nothing is needed.
        return true;
    }

    if (caps.antialias) {
        if (formats.samples == 0)
            return false; // Can't create it.

        MOZ_ASSERT(formats.samples <= gl->MaxSamples());
    }

    GLuint colorMSRB = 0;
    GLuint depthRB   = 0;
    GLuint stencilRB = 0;

    GLuint* pColorMSRB = caps.antialias ? &colorMSRB : nullptr;
    GLuint* pDepthRB   = caps.depth     ? &depthRB   : nullptr;
    GLuint* pStencilRB = caps.stencil   ? &stencilRB : nullptr;

    if (!formats.color_rbFormat)
        pColorMSRB = nullptr;

    if (pDepthRB && pStencilRB) {
        if (!formats.depth && !formats.depthStencil)
            pDepthRB = nullptr;

        if (!formats.stencil && !formats.depthStencil)
            pStencilRB = nullptr;
    } else {
        if (!formats.depth)
            pDepthRB = nullptr;

        if (!formats.stencil)
            pStencilRB = nullptr;
    }

    GLContext::LocalErrorScope localError(*gl);

    CreateRenderbuffersForOffscreen(gl, formats, size, caps.antialias,
                                    pColorMSRB, pDepthRB, pStencilRB);

    GLuint fb = 0;
    gl->fGenFramebuffers(1, &fb);
    gl->AttachBuffersToFB(0, colorMSRB, depthRB, stencilRB, fb);

    GLsizei samples = formats.samples;
    if (!samples)
        samples = 1;

    UniquePtr<DrawBuffer> ret( new DrawBuffer(gl, size, samples, fb, colorMSRB,
                                              depthRB, stencilRB) );

    GLenum err = localError.GetError();
    MOZ_ASSERT_IF(err != LOCAL_GL_NO_ERROR, err == LOCAL_GL_OUT_OF_MEMORY);
    if (err || !gl->IsFramebufferComplete(fb))
        return false;

    *out_buffer = Move(ret);
    return true;
}

DrawBuffer::~DrawBuffer()
{
    mGL->MakeCurrent();

    GLuint fb = mFB;
    GLuint rbs[] = {
        mColorMSRB,
        mDepthRB,
        mStencilRB
    };

    mGL->fDeleteFramebuffers(1, &fb);
    mGL->fDeleteRenderbuffers(3, rbs);
}

////////////////////////////////////////////////////////////////////////
// ReadBuffer

UniquePtr<ReadBuffer>
ReadBuffer::Create(GLContext* gl,
                   const SurfaceCaps& caps,
                   const GLFormats& formats,
                   SharedSurface* surf)
{
    MOZ_ASSERT(surf);

    if (surf->mAttachType == AttachmentType::Screen) {
        // Don't need anything. Our read buffer will be the 'screen'.
        return UniquePtr<ReadBuffer>( new ReadBuffer(gl, 0, 0, 0,
                                                     surf) );
    }

    GLuint depthRB = 0;
    GLuint stencilRB = 0;

    GLuint* pDepthRB   = caps.depth   ? &depthRB   : nullptr;
    GLuint* pStencilRB = caps.stencil ? &stencilRB : nullptr;

    GLContext::LocalErrorScope localError(*gl);

    CreateRenderbuffersForOffscreen(gl, formats, surf->mSize, caps.antialias,
                                    nullptr, pDepthRB, pStencilRB);

    GLuint colorTex = 0;
    GLuint colorRB = 0;
    GLenum target = 0;

    switch (surf->mAttachType) {
    case AttachmentType::GLTexture:
        colorTex = surf->ProdTexture();
        target = surf->ProdTextureTarget();
        break;
    case AttachmentType::GLRenderbuffer:
        colorRB = surf->ProdRenderbuffer();
        break;
    default:
        MOZ_CRASH("Unknown attachment type?");
    }
    MOZ_ASSERT(colorTex || colorRB);

    GLuint fb = 0;
    gl->fGenFramebuffers(1, &fb);
    gl->AttachBuffersToFB(colorTex, colorRB, depthRB, stencilRB, fb, target);
    gl->mFBOMapping[fb] = surf;

    UniquePtr<ReadBuffer> ret( new ReadBuffer(gl, fb, depthRB,
                                              stencilRB, surf) );

    GLenum err = localError.GetError();
    MOZ_ASSERT_IF(err != LOCAL_GL_NO_ERROR, err == LOCAL_GL_OUT_OF_MEMORY);
    if (err || !gl->IsFramebufferComplete(fb)) {
        ret = nullptr;
    }

    return Move(ret);
}

ReadBuffer::~ReadBuffer()
{
    mGL->MakeCurrent();

    GLuint fb = mFB;
    GLuint rbs[] = {
        mDepthRB,
        mStencilRB
    };

    mGL->fDeleteFramebuffers(1, &fb);
    mGL->fDeleteRenderbuffers(2, rbs);
    mGL->mFBOMapping.erase(mFB);
}

void
ReadBuffer::Attach(SharedSurface* surf)
{
    MOZ_ASSERT(surf && mSurf);
    MOZ_ASSERT(surf->mAttachType == mSurf->mAttachType);
    MOZ_ASSERT(surf->mSize == mSurf->mSize);

    // Nothing else is needed for AttachType Screen.
    if (surf->mAttachType != AttachmentType::Screen) {
        GLuint colorTex = 0;
        GLuint colorRB = 0;
        GLenum target = 0;

        switch (surf->mAttachType) {
        case AttachmentType::GLTexture:
            colorTex = surf->ProdTexture();
            target = surf->ProdTextureTarget();
            break;
        case AttachmentType::GLRenderbuffer:
            colorRB = surf->ProdRenderbuffer();
            break;
        default:
            MOZ_CRASH("Unknown attachment type?");
        }

        mGL->AttachBuffersToFB(colorTex, colorRB, 0, 0, mFB, target);
        mGL->mFBOMapping[mFB] = surf;
        MOZ_ASSERT(mGL->IsFramebufferComplete(mFB));
    }

    mSurf = surf;
}

const gfx::IntSize&
ReadBuffer::Size() const
{
    return mSurf->mSize;
}

void
ReadBuffer::SetReadBuffer(GLenum userMode) const
{
    if (!mGL->IsSupported(GLFeature::read_buffer))
        return;

    GLenum internalMode;

    switch (userMode) {
    case LOCAL_GL_BACK:
    case LOCAL_GL_FRONT:
        internalMode = (mFB == 0) ? userMode
                                  : LOCAL_GL_COLOR_ATTACHMENT0;
        break;

    case LOCAL_GL_NONE:
        internalMode = LOCAL_GL_NONE;
        break;

    default:
        MOZ_CRASH("Bad value.");
    }

    mGL->MakeCurrent();
    mGL->fReadBuffer(internalMode);
}

} /* namespace gl */
} /* namespace mozilla */
