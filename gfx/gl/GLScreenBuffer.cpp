/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLScreenBuffer.h"

#include <cstring>
#include "gfxImageSurface.h"
#include "GLContext.h"
#include "SharedSurfaceGL.h"
#include "SurfaceStream.h"
#ifdef MOZ_WIDGET_GONK
#include "SharedSurfaceGralloc.h"
#include "nsXULAppAPI.h"
#endif
#ifdef XP_MACOSX
#include "SharedSurfaceIO.h"
#endif

using namespace mozilla::gfx;

namespace mozilla {
namespace gl {

GLScreenBuffer*
GLScreenBuffer::Create(GLContext* gl,
                     const gfxIntSize& size,
                     const SurfaceCaps& caps)
{
    if (caps.antialias &&
        !gl->IsSupported(GLFeature::framebuffer_multisample))
    {
        return nullptr;
    }

    SurfaceFactory_GL* factory = nullptr;

#ifdef MOZ_WIDGET_GONK
    /* On B2G, we want a Gralloc factory, and we want one right at the start */
    if (!factory &&
        XRE_GetProcessType() != GeckoProcessType_Default)
    {
        factory = new SurfaceFactory_Gralloc(gl, caps);
    }
#endif
#ifdef XP_MACOSX
    /* On OSX, we want an IOSurface factory, and we want one right at the start */
    if (!factory)
    {
        factory = new SurfaceFactory_IOSurface(gl, caps);
    }
#endif

    if (!factory)
        factory = new SurfaceFactory_Basic(gl, caps);

    SurfaceStream* stream = SurfaceStream::CreateForType(
        SurfaceStream::ChooseGLStreamType(SurfaceStream::MainThread,
                                          caps.preserve),
        gl,
        nullptr);

    return new GLScreenBuffer(gl, caps, factory, stream);
}

GLScreenBuffer::~GLScreenBuffer()
{
    delete mStream;
    delete mDraw;
    delete mRead;

    // bug 914823: it is crucial to destroy the Factory _after_ we destroy
    // the SharedSurfaces around here! Reason: the shared surfaces will want
    // to ask the Allocator (e.g. the ClientLayerManager) to destroy their
    // buffers, but that Allocator may be kept alive by the Factory,
    // as it currently the case in SurfaceFactory_Gralloc holding a nsRefPtr
    // to the Allocator!
    delete mFactory;
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
    if (!mGL->IsSupported(GLFeature::framebuffer_blit)) {
        NS_WARNING("DRAW_FRAMEBUFFER requested, but unsupported.");

        mGL->raw_fBindFramebuffer(LOCAL_GL_DRAW_FRAMEBUFFER_EXT, fb);
    } else {
        GLuint drawFB = DrawFB();
        mUserDrawFB = fb;
        mInternalDrawFB = (fb == 0) ? drawFB : fb;

        mGL->raw_fBindFramebuffer(LOCAL_GL_DRAW_FRAMEBUFFER_EXT, mInternalDrawFB);
    }

#ifdef DEBUG
    mInInternalMode_DrawFB = false;
#endif
}

void
GLScreenBuffer::BindReadFB(GLuint fb)
{
    if (!mGL->IsSupported(GLFeature::framebuffer_blit)) {
        NS_WARNING("READ_FRAMEBUFFER requested, but unsupported.");

        mGL->raw_fBindFramebuffer(LOCAL_GL_READ_FRAMEBUFFER_EXT, fb);
    } else {
        GLuint readFB = ReadFB();
        mUserReadFB = fb;
        mInternalReadFB = (fb == 0) ? readFB : fb;

        mGL->raw_fBindFramebuffer(LOCAL_GL_READ_FRAMEBUFFER_EXT, mInternalReadFB);
    }

#ifdef DEBUG
    mInInternalMode_ReadFB = false;
#endif
}

void
GLScreenBuffer::BindDrawFB_Internal(GLuint fb)
{
  mInternalDrawFB = mUserDrawFB = fb;
  mGL->raw_fBindFramebuffer(LOCAL_GL_DRAW_FRAMEBUFFER_EXT, mInternalDrawFB);

#ifdef DEBUG
    mInInternalMode_DrawFB = true;
#endif
}

void
GLScreenBuffer::BindReadFB_Internal(GLuint fb)
{
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
GLScreenBuffer::ReadPixels(GLint x, GLint y, GLsizei width, GLsizei height,
                           GLenum format, GLenum type, GLvoid *pixels)
{
    // If the currently bound framebuffer is backed by a SharedSurface_GL
    // then it might want to override how we read pixel data from it.
    // This is normally only the default framebuffer, but we can also
    // have SharedSurfaces bound to other framebuffers when doing
    // readback for BasicLayers.
    SharedSurface_GL* surf;
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
        MOZ_ASSERT(mGL->IsExtensionSupported(GLContext::EXT_framebuffer_blit) ||
                   mGL->IsExtensionSupported(GLContext::ANGLE_framebuffer_blit));
        MOZ_ASSERT(mDraw->Size() == mRead->Size());

        ScopedBindFramebuffer boundFB(mGL);
        ScopedGLState scissor(mGL, LOCAL_GL_SCISSOR_TEST, false);

        BindReadFB_Internal(drawFB);
        BindDrawFB_Internal(readFB);

        const gfxIntSize&  srcSize = mDraw->Size();
        const gfxIntSize& destSize = mRead->Size();

        mGL->raw_fBlitFramebuffer(0, 0,  srcSize.width,  srcSize.height,
                                  0, 0, destSize.width, destSize.height,
                                  LOCAL_GL_COLOR_BUFFER_BIT,
                                  LOCAL_GL_NEAREST);
        // Done!
    }

    mNeedsBlit = false;
}

void
GLScreenBuffer::Morph(SurfaceFactory_GL* newFactory, SurfaceStreamType streamType)
{
    MOZ_ASSERT(mStream);

    if (newFactory) {
        delete mFactory;
        mFactory = newFactory;
    }

    if (mStream->mType == streamType)
        return;

    SurfaceStream* newStream = SurfaceStream::CreateForType(streamType, mGL, mStream);
    MOZ_ASSERT(newStream);

    delete mStream;
    mStream = newStream;
}

void
GLScreenBuffer::Attach(SharedSurface* surface, const gfxIntSize& size)
{
    ScopedBindFramebuffer autoFB(mGL);

    SharedSurface_GL* surf = SharedSurface_GL::Cast(surface);
    if (mRead && SharedSurf())
        SharedSurf()->UnlockProd();

    surf->LockProd();

    if (mRead &&
        surf->AttachType() == SharedSurf()->AttachType() &&
        size == Size())
    {
        // Same size, same type, ready for reuse!
        mRead->Attach(surf);
    } else {
        // Else something changed, so resize:
        DrawBuffer* draw = CreateDraw(size);  // Can be null.
        ReadBuffer* read = CreateRead(surf);
        MOZ_ASSERT(read); // Should never fail if SwapProd succeeded.

        delete mDraw;
        delete mRead;

        mDraw = draw;
        mRead = read;
    }

    // Check that we're all set up.
    MOZ_ASSERT(SharedSurf() == surf);

    if (!PreserveBuffer()) {
        // DiscardFramebuffer here could help perf on some mobile platforms.
    }
}

bool
GLScreenBuffer::Swap(const gfxIntSize& size)
{
    SharedSurface* nextSurf = mStream->SwapProducer(mFactory, size);
    if (!nextSurf) {
        SurfaceFactory_Basic basicFactory(mGL, mFactory->Caps());
        nextSurf = mStream->SwapProducer(&basicFactory, size);
        if (!nextSurf)
          return false;

        NS_WARNING("SwapProd failed for sophisticated Factory type, fell back to Basic.");
    }
    MOZ_ASSERT(nextSurf);

    Attach(nextSurf, size);

    return true;
}

bool
GLScreenBuffer::PublishFrame(const gfxIntSize& size)
{
    AssureBlitted();

    bool good = Swap(size);
    return good;
}

bool
GLScreenBuffer::Resize(const gfxIntSize& size)
{
    SharedSurface* surface = mStream->Resize(mFactory, size);
    if (!surface)
        return false;

    Attach(surface, size);
    return true;
}

DrawBuffer*
GLScreenBuffer::CreateDraw(const gfxIntSize& size)
{
    GLContext* gl = mFactory->GL();
    const GLFormats& formats = mFactory->Formats();
    const SurfaceCaps& caps = mFactory->DrawCaps();

    return DrawBuffer::Create(gl, caps, formats, size);
}

ReadBuffer*
GLScreenBuffer::CreateRead(SharedSurface_GL* surf)
{
    GLContext* gl = mFactory->GL();
    const GLFormats& formats = mFactory->Formats();
    const SurfaceCaps& caps = mFactory->ReadCaps();

    return ReadBuffer::Create(gl, caps, formats, surf);
}


void
GLScreenBuffer::Readback(SharedSurface_GL* src, gfxImageSurface* dest)
{
    MOZ_ASSERT(src && dest);
    MOZ_ASSERT(dest->GetSize() == src->Size());
    MOZ_ASSERT(dest->Format() == (src->HasAlpha() ? gfxImageSurface::ImageFormatARGB32
                                                  : gfxImageSurface::ImageFormatRGB24));

    mGL->MakeCurrent();

    bool needsSwap = src != SharedSurf();
    if (needsSwap) {
        SharedSurf()->UnlockProd();
        src->LockProd();
    }

    ReadBuffer* buffer = CreateRead(src);
    MOZ_ASSERT(buffer);

    ScopedBindFramebuffer autoFB(mGL, buffer->FB());
    mGL->ReadPixelsIntoImageSurface(dest);

    delete buffer;

    if (needsSwap) {
        src->UnlockProd();
        SharedSurf()->LockProd();
    }
}



DrawBuffer*
DrawBuffer::Create(GLContext* const gl,
                   const SurfaceCaps& caps,
                   const GLFormats& formats,
                   const gfxIntSize& size)
{
    if (!caps.color) {
        MOZ_ASSERT(!caps.alpha && !caps.depth && !caps.stencil);

        // Nothing is needed.
        return nullptr;
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

    gl->CreateRenderbuffersForOffscreen(formats, size, caps.antialias,
                                        pColorMSRB, pDepthRB, pStencilRB);

    GLuint fb = 0;
    gl->fGenFramebuffers(1, &fb);
    gl->AttachBuffersToFB(0, colorMSRB, depthRB, stencilRB, fb);
    MOZ_ASSERT(gl->IsFramebufferComplete(fb));

    return new DrawBuffer(gl, size, fb, colorMSRB, depthRB, stencilRB);
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






ReadBuffer*
ReadBuffer::Create(GLContext* gl,
                   const SurfaceCaps& caps,
                   const GLFormats& formats,
                   SharedSurface_GL* surf)
{
    MOZ_ASSERT(surf);

    if (surf->AttachType() == AttachmentType::Screen) {
        // Don't need anything. Our read buffer will be the 'screen'.

        return new ReadBuffer(gl,
                              0, 0, 0,
                              surf);
    }

    GLuint depthRB = 0;
    GLuint stencilRB = 0;

    GLuint* pDepthRB   = caps.depth   ? &depthRB   : nullptr;
    GLuint* pStencilRB = caps.stencil ? &stencilRB : nullptr;

    gl->CreateRenderbuffersForOffscreen(formats, surf->Size(), caps.antialias,
                                        nullptr, pDepthRB, pStencilRB);

    GLuint colorTex = 0;
    GLuint colorRB = 0;
    GLenum target = 0;

    switch (surf->AttachType()) {
    case AttachmentType::GLTexture:
        colorTex = surf->Texture();
        target = surf->TextureTarget();
        break;
    case AttachmentType::GLRenderbuffer:
        colorRB = surf->Renderbuffer();
        break;
    default:
        MOZ_CRASH("Unknown attachment type?");
    }
    MOZ_ASSERT(colorTex || colorRB);

    GLuint fb = 0;
    gl->fGenFramebuffers(1, &fb);
    gl->AttachBuffersToFB(colorTex, colorRB, depthRB, stencilRB, fb, target);
    gl->mFBOMapping[fb] = surf;

    MOZ_ASSERT(gl->IsFramebufferComplete(fb));

    return new ReadBuffer(gl,
                          fb, depthRB, stencilRB,
                          surf);
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
ReadBuffer::Attach(SharedSurface_GL* surf)
{
    MOZ_ASSERT(surf && mSurf);
    MOZ_ASSERT(surf->AttachType() == mSurf->AttachType());
    MOZ_ASSERT(surf->Size() == mSurf->Size());

    // Nothing else is needed for AttachType Screen.
    if (surf->AttachType() != AttachmentType::Screen) {
        GLuint colorTex = 0;
        GLuint colorRB = 0;
        GLenum target = 0;

        switch (surf->AttachType()) {
        case AttachmentType::GLTexture:
            colorTex = surf->Texture();
            target = surf->TextureTarget();
            break;
        case AttachmentType::GLRenderbuffer:
            colorRB = surf->Renderbuffer();
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

const gfxIntSize&
ReadBuffer::Size() const
{
    return mSurf->Size();
}

} /* namespace gl */
} /* namespace mozilla */
