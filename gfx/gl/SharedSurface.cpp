/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedSurface.h"

#include "GLBlitHelper.h"
#include "GLContext.h"
#include "nsThreadUtils.h"
#include "ScopedGLHelpers.h"
#include "SharedSurfaceGL.h"

namespace mozilla {
namespace gl {

/*static*/ void
SharedSurface::ProdCopy(SharedSurface* src, SharedSurface* dest,
                        SurfaceFactory* factory)
{
    GLContext* gl = src->mGL;

    // If `src` begins locked, it must end locked, though we may
    // temporarily unlock it if we need to.
    MOZ_ASSERT((src == gl->GetLockedSurface()) == src->IsLocked());

    gl->MakeCurrent();

    if (src->mAttachType  == AttachmentType::Screen &&
        dest->mAttachType == AttachmentType::Screen)
    {
        // Here, we actually need to blit through a temp surface, so let's make one.
        UniquePtr<SharedSurface_GLTexture> tempSurf;
        tempSurf = SharedSurface_GLTexture::Create(gl,
                                                   gl,
                                                   factory->mFormats,
                                                   src->mSize,
                                                   factory->mCaps.alpha);

        ProdCopy(src, tempSurf.get(), factory);
        ProdCopy(tempSurf.get(), dest, factory);
        return;
    }

    if (src->mAttachType == AttachmentType::Screen) {
        SharedSurface* origLocked = gl->GetLockedSurface();
        bool srcNeedsUnlock = false;
        bool origNeedsRelock = false;
        if (origLocked != src) {
            if (origLocked) {
                origLocked->UnlockProd();
                origNeedsRelock = true;
            }

            src->LockProd();
            srcNeedsUnlock = true;
        }

        if (dest->mAttachType == AttachmentType::GLTexture) {
            GLuint destTex = dest->ProdTexture();
            GLenum destTarget = dest->ProdTextureTarget();

            gl->BlitHelper()->BlitFramebufferToTexture(0, destTex,
                                                       src->mSize,
                                                       dest->mSize,
                                                       destTarget,
                                                       true);
        } else if (dest->mAttachType == AttachmentType::GLRenderbuffer) {
            GLuint destRB = dest->ProdRenderbuffer();
            ScopedFramebufferForRenderbuffer destWrapper(gl, destRB);

            gl->BlitHelper()->BlitFramebufferToFramebuffer(0,
                                                           destWrapper.FB(),
                                                           src->mSize,
                                                           dest->mSize,
                                                           true);
        } else {
            MOZ_CRASH("Unhandled dest->mAttachType.");
        }

        if (srcNeedsUnlock)
            src->UnlockProd();

        if (origNeedsRelock)
            origLocked->LockProd();

        return;
    }

    if (dest->mAttachType == AttachmentType::Screen) {
        SharedSurface* origLocked = gl->GetLockedSurface();
        bool destNeedsUnlock = false;
        bool origNeedsRelock = false;
        if (origLocked != dest) {
            if (origLocked) {
                origLocked->UnlockProd();
                origNeedsRelock = true;
            }

            dest->LockProd();
            destNeedsUnlock = true;
        }

        if (src->mAttachType == AttachmentType::GLTexture) {
            GLuint srcTex = src->ProdTexture();
            GLenum srcTarget = src->ProdTextureTarget();

            gl->BlitHelper()->BlitTextureToFramebuffer(srcTex, 0,
                                                       src->mSize,
                                                       dest->mSize,
                                                       srcTarget,
                                                       true);
        } else if (src->mAttachType == AttachmentType::GLRenderbuffer) {
            GLuint srcRB = src->ProdRenderbuffer();
            ScopedFramebufferForRenderbuffer srcWrapper(gl, srcRB);

            gl->BlitHelper()->BlitFramebufferToFramebuffer(srcWrapper.FB(),
                                                           0,
                                                           src->mSize,
                                                           dest->mSize,
                                                           true);
        } else {
            MOZ_CRASH("Unhandled src->mAttachType.");
        }

        if (destNeedsUnlock)
            dest->UnlockProd();

        if (origNeedsRelock)
            origLocked->LockProd();

        return;
    }

    // Alright, done with cases involving Screen types.
    // Only {src,dest}x{texture,renderbuffer} left.

    if (src->mAttachType == AttachmentType::GLTexture) {
        GLuint srcTex = src->ProdTexture();
        GLenum srcTarget = src->ProdTextureTarget();

        if (dest->mAttachType == AttachmentType::GLTexture) {
            GLuint destTex = dest->ProdTexture();
            GLenum destTarget = dest->ProdTextureTarget();

            gl->BlitHelper()->BlitTextureToTexture(srcTex, destTex,
                                                   src->mSize, dest->mSize,
                                                   srcTarget, destTarget);

            return;
        }

        if (dest->mAttachType == AttachmentType::GLRenderbuffer) {
            GLuint destRB = dest->ProdRenderbuffer();
            ScopedFramebufferForRenderbuffer destWrapper(gl, destRB);

            gl->BlitHelper()->BlitTextureToFramebuffer(srcTex, destWrapper.FB(),
                                                       src->mSize, dest->mSize, srcTarget);

            return;
        }

        MOZ_CRASH("Unhandled dest->mAttachType.");
    }

    if (src->mAttachType == AttachmentType::GLRenderbuffer) {
        GLuint srcRB = src->ProdRenderbuffer();
        ScopedFramebufferForRenderbuffer srcWrapper(gl, srcRB);

        if (dest->mAttachType == AttachmentType::GLTexture) {
            GLuint destTex = dest->ProdTexture();
            GLenum destTarget = dest->ProdTextureTarget();

            gl->BlitHelper()->BlitFramebufferToTexture(srcWrapper.FB(), destTex,
                                                       src->mSize, dest->mSize, destTarget);

            return;
        }

        if (dest->mAttachType == AttachmentType::GLRenderbuffer) {
            GLuint destRB = dest->ProdRenderbuffer();
            ScopedFramebufferForRenderbuffer destWrapper(gl, destRB);

            gl->BlitHelper()->BlitFramebufferToFramebuffer(srcWrapper.FB(), destWrapper.FB(),
                                                           src->mSize, dest->mSize);

            return;
        }

        MOZ_CRASH("Unhandled dest->mAttachType.");
    }

    MOZ_CRASH("Unhandled src->mAttachType.");
}

////////////////////////////////////////////////////////////////////////
// SharedSurface


SharedSurface::SharedSurface(SharedSurfaceType type,
                             AttachmentType attachType,
                             GLContext* gl,
                             const gfx::IntSize& size,
                             bool hasAlpha)
    : mType(type)
    , mAttachType(attachType)
    , mGL(gl)
    , mSize(size)
    , mHasAlpha(hasAlpha)
    , mIsLocked(false)
#ifdef DEBUG
    , mOwningThread(NS_GetCurrentThread())
#endif
{
}

void
SharedSurface::LockProd()
{
    MOZ_ASSERT(!mIsLocked);

    LockProdImpl();

    mGL->LockSurface(this);
    mIsLocked = true;
}

void
SharedSurface::UnlockProd()
{
    if (!mIsLocked)
        return;

    UnlockProdImpl();

    mGL->UnlockSurface(this);
    mIsLocked = false;
}

void
SharedSurface::Fence_ContentThread()
{
    MOZ_ASSERT(NS_GetCurrentThread() == mOwningThread);
    Fence_ContentThread_Impl();
}

bool
SharedSurface::WaitSync_ContentThread()
{
    MOZ_ASSERT(NS_GetCurrentThread() == mOwningThread);
    return WaitSync_ContentThread_Impl();
}

bool
SharedSurface::PollSync_ContentThread()
{
    MOZ_ASSERT(NS_GetCurrentThread() == mOwningThread);
    return PollSync_ContentThread_Impl();
}



////////////////////////////////////////////////////////////////////////
// SurfaceFactory

static void
ChooseBufferBits(const SurfaceCaps& caps,
                 SurfaceCaps* const out_drawCaps,
                 SurfaceCaps* const out_readCaps)
{
    MOZ_ASSERT(out_drawCaps);
    MOZ_ASSERT(out_readCaps);

    SurfaceCaps screenCaps;

    screenCaps.color = caps.color;
    screenCaps.alpha = caps.alpha;
    screenCaps.bpp16 = caps.bpp16;

    screenCaps.depth = caps.depth;
    screenCaps.stencil = caps.stencil;

    screenCaps.antialias = caps.antialias;
    screenCaps.preserve = caps.preserve;

    if (caps.antialias) {
        *out_drawCaps = screenCaps;
        out_readCaps->Clear();

        // Color caps need to be duplicated in readCaps.
        out_readCaps->color = caps.color;
        out_readCaps->alpha = caps.alpha;
        out_readCaps->bpp16 = caps.bpp16;
    } else {
        out_drawCaps->Clear();
        *out_readCaps = screenCaps;
    }
}

SurfaceFactory::SurfaceFactory(GLContext* gl,
                               SharedSurfaceType type,
                               const SurfaceCaps& caps)
    : mGL(gl)
    , mCaps(caps)
    , mType(type)
    , mFormats(gl->ChooseGLFormats(caps))
{
    ChooseBufferBits(mCaps, &mDrawCaps, &mReadCaps);
}

SurfaceFactory::~SurfaceFactory()
{
    while (!mScraps.Empty()) {
        mScraps.Pop();
    }
}

UniquePtr<SharedSurface>
SurfaceFactory::NewSharedSurface(const gfx::IntSize& size)
{
    // Attempt to reuse an old surface.
    while (!mScraps.Empty()) {
        UniquePtr<SharedSurface> cur = mScraps.Pop();

        if (cur->mSize == size)
            return Move(cur);

        // Let `cur` be destroyed as it falls out of scope, if it wasn't
        // moved.
    }

    return CreateShared(size);
}

TemporaryRef<ShSurfHandle>
SurfaceFactory::NewShSurfHandle(const gfx::IntSize& size)
{
    auto surf = NewSharedSurface(size);
    if (!surf)
        return nullptr;

    return new ShSurfHandle(this, Move(surf));
}

// Auto-deletes surfs of the wrong type.
void
SurfaceFactory::Recycle(UniquePtr<SharedSurface> surf)
{
    MOZ_ASSERT(surf);

    if (surf->mType == mType) {
        mScraps.Push(Move(surf));
    }
}

////////////////////////////////////////////////////////////////////////////////
// ScopedReadbackFB

ScopedReadbackFB::ScopedReadbackFB(SharedSurface* src)
    : mGL(src->mGL)
    , mAutoFB(mGL)
    , mTempFB(0)
    , mTempTex(0)
    , mSurfToUnlock(nullptr)
    , mSurfToLock(nullptr)
{
    switch (src->mAttachType) {
    case AttachmentType::GLRenderbuffer:
        {
            mGL->fGenFramebuffers(1, &mTempFB);
            mGL->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, mTempFB);

            GLuint rb = src->ProdRenderbuffer();
            mGL->fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER,
                                          LOCAL_GL_COLOR_ATTACHMENT0,
                                          LOCAL_GL_RENDERBUFFER, rb);
            break;
        }
    case AttachmentType::GLTexture:
        {
            mGL->fGenFramebuffers(1, &mTempFB);
            mGL->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, mTempFB);

            GLuint tex = src->ProdTexture();
            GLenum texImageTarget = src->ProdTextureTarget();
            mGL->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER,
                                       LOCAL_GL_COLOR_ATTACHMENT0,
                                       texImageTarget, tex, 0);
            break;
        }
    case AttachmentType::Screen:
        {
            SharedSurface* origLocked = mGL->GetLockedSurface();
            if (origLocked != src) {
                if (origLocked) {
                    mSurfToLock = origLocked;
                    mSurfToLock->UnlockProd();
                }

                mSurfToUnlock = src;
                mSurfToUnlock->LockProd();
            }

            // TODO: This should just be BindFB, but we don't have
            // the patch for this yet. (bug 1045955)
            MOZ_ASSERT(mGL->Screen());
            mGL->Screen()->BindReadFB_Internal(0);
            break;
        }
    default:
        MOZ_CRASH("Unhandled `mAttachType`.");
    }

    if (src->NeedsIndirectReads()) {
        mGL->fGenTextures(1, &mTempTex);

        {
            ScopedBindTexture autoTex(mGL, mTempTex);

            GLenum format = src->mHasAlpha ? LOCAL_GL_RGBA
                                           : LOCAL_GL_RGB;
            auto width = src->mSize.width;
            auto height = src->mSize.height;
            mGL->fCopyTexImage2D(LOCAL_GL_TEXTURE_2D, 0, format, 0, 0, width,
                                 height, 0);
        }

        mGL->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER,
                                   LOCAL_GL_COLOR_ATTACHMENT0,
                                   LOCAL_GL_TEXTURE_2D, mTempTex, 0);
    }
}

ScopedReadbackFB::~ScopedReadbackFB()
{
    if (mTempFB) {
        mGL->fDeleteFramebuffers(1, &mTempFB);
    }
    if (mTempTex) {
        mGL->fDeleteTextures(1, &mTempTex);
    }
    if (mSurfToUnlock) {
        mSurfToUnlock->UnlockProd();
    }
    if (mSurfToLock) {
        mSurfToLock->LockProd();
    }
}

} /* namespace gfx */
} /* namespace mozilla */
