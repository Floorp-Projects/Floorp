/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedSurface.h"

#include "../2d/2D.h"
#include "GLBlitHelper.h"
#include "GLContext.h"
#include "GLReadTexImageHelper.h"
#include "GLScreenBuffer.h"
#include "nsThreadUtils.h"
#include "ScopedGLHelpers.h"
#include "SharedSurfaceGL.h"
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/TextureClientSharedSurface.h"
#include "mozilla/unused.h"

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
        UniquePtr<SharedSurface_Basic> tempSurf;
        tempSurf = SharedSurface_Basic::Create(gl, factory->mFormats, src->mSize,
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
            MOZ_CRASH("GFX: Unhandled dest->mAttachType 1.");
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
            MOZ_CRASH("GFX: Unhandled src->mAttachType 2.");
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

        MOZ_CRASH("GFX: Unhandled dest->mAttachType 3.");
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

        MOZ_CRASH("GFX: Unhandled dest->mAttachType 4.");
    }

    MOZ_CRASH("GFX: Unhandled src->mAttachType 5.");
}

////////////////////////////////////////////////////////////////////////
// SharedSurface


SharedSurface::SharedSurface(SharedSurfaceType type,
                             AttachmentType attachType,
                             GLContext* gl,
                             const gfx::IntSize& size,
                             bool hasAlpha,
                             bool canRecycle)
    : mType(type)
    , mAttachType(attachType)
    , mGL(gl)
    , mSize(size)
    , mHasAlpha(hasAlpha)
    , mCanRecycle(canRecycle)
    , mIsLocked(false)
    , mIsProducerAcquired(false)
    , mIsConsumerAcquired(false)
#ifdef DEBUG
    , mOwningThread(NS_GetCurrentThread())
#endif
{ }

layers::TextureFlags
SharedSurface::GetTextureFlags() const
{
    return layers::TextureFlags::NO_FLAGS;
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

SurfaceFactory::SurfaceFactory(SharedSurfaceType type, GLContext* gl,
                               const SurfaceCaps& caps,
                               const RefPtr<layers::ClientIPCAllocator>& allocator,
                               const layers::TextureFlags& flags)
    : mType(type)
    , mGL(gl)
    , mCaps(caps)
    , mAllocator(allocator)
    , mFlags(flags)
    , mFormats(gl->ChooseGLFormats(caps))
    , mMutex("SurfaceFactor::mMutex")
{
    ChooseBufferBits(mCaps, &mDrawCaps, &mReadCaps);
}

SurfaceFactory::~SurfaceFactory()
{
    while (!mRecycleTotalPool.empty()) {
        RefPtr<layers::SharedSurfaceTextureClient> tex = *mRecycleTotalPool.begin();
        StopRecycling(tex);
        tex->CancelWaitForCompositorRecycle();
    }

    MOZ_RELEASE_ASSERT(mRecycleTotalPool.empty(),"GFX: Surface recycle pool not empty.");

    // If we mRecycleFreePool.clear() before StopRecycling(), we may try to recycle it,
    // fail, call StopRecycling(), then return here and call it again.
    mRecycleFreePool.clear();
}

already_AddRefed<layers::SharedSurfaceTextureClient>
SurfaceFactory::NewTexClient(const gfx::IntSize& size)
{
    while (!mRecycleFreePool.empty()) {
        RefPtr<layers::SharedSurfaceTextureClient> cur = mRecycleFreePool.front();
        mRecycleFreePool.pop();

        if (cur->Surf()->mSize == size) {
            cur->Surf()->WaitForBufferOwnership();
            return cur.forget();
        }

        StopRecycling(cur);
    }

    UniquePtr<SharedSurface> surf = Move(CreateShared(size));
    if (!surf)
        return nullptr;

    RefPtr<layers::SharedSurfaceTextureClient> ret;
    ret = layers::SharedSurfaceTextureClient::Create(Move(surf), this, mAllocator, mFlags);

    StartRecycling(ret);

    return ret.forget();
}

void
SurfaceFactory::StartRecycling(layers::SharedSurfaceTextureClient* tc)
{
    tc->SetRecycleCallback(&SurfaceFactory::RecycleCallback, static_cast<void*>(this));

    bool didInsert = mRecycleTotalPool.insert(tc);
    MOZ_RELEASE_ASSERT(didInsert, "GFX: Shared surface texture client was not inserted to recycle.");
    mozilla::Unused << didInsert;
}

void
SurfaceFactory::StopRecycling(layers::SharedSurfaceTextureClient* tc)
{
    MutexAutoLock autoLock(mMutex);
    // Must clear before releasing ref.
    tc->ClearRecycleCallback();

    bool didErase = mRecycleTotalPool.erase(tc);
    MOZ_RELEASE_ASSERT(didErase, "GFX: Shared texture surface client was not erased.");
    mozilla::Unused << didErase;
}

/*static*/ void
SurfaceFactory::RecycleCallback(layers::TextureClient* rawTC, void* rawFactory)
{
    RefPtr<layers::SharedSurfaceTextureClient> tc;
    tc = static_cast<layers::SharedSurfaceTextureClient*>(rawTC);
    SurfaceFactory* factory = static_cast<SurfaceFactory*>(rawFactory);

    if (tc->Surf()->mCanRecycle) {
        if (factory->Recycle(tc))
            return;
    }

    // Did not recover the tex client. End the (re)cycle!
    factory->StopRecycling(tc);
}

bool
SurfaceFactory::Recycle(layers::SharedSurfaceTextureClient* texClient)
{
    MOZ_ASSERT(texClient);
    MutexAutoLock autoLock(mMutex);

    if (mRecycleFreePool.size() >= 2) {
        return false;
    }

    RefPtr<layers::SharedSurfaceTextureClient> texClientRef = texClient;
    mRecycleFreePool.push(texClientRef);
    return true;
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
        MOZ_CRASH("GFX: Unhandled `mAttachType`.");
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

////////////////////////////////////////////////////////////////////////////////

class AutoLockBits
{
    gfx::DrawTarget* mDT;
    uint8_t* mLockedBits;

public:
    explicit AutoLockBits(gfx::DrawTarget* dt)
        : mDT(dt)
        , mLockedBits(nullptr)
    {
        MOZ_ASSERT(mDT);
    }

    bool Lock(uint8_t** data, gfx::IntSize* size, int32_t* stride,
              gfx::SurfaceFormat* format)
    {
        if (!mDT->LockBits(data, size, stride, format))
            return false;

        mLockedBits = *data;
        return true;
    }

    ~AutoLockBits() {
        if (mLockedBits)
            mDT->ReleaseBits(mLockedBits);
    }
};

bool
ReadbackSharedSurface(SharedSurface* src, gfx::DrawTarget* dst)
{
    AutoLockBits lock(dst);

    uint8_t* dstBytes;
    gfx::IntSize dstSize;
    int32_t dstStride;
    gfx::SurfaceFormat dstFormat;
    if (!lock.Lock(&dstBytes, &dstSize, &dstStride, &dstFormat))
        return false;

    const bool isDstRGBA = (dstFormat == gfx::SurfaceFormat::R8G8B8A8 ||
                            dstFormat == gfx::SurfaceFormat::R8G8B8X8);
    MOZ_ASSERT_IF(!isDstRGBA, dstFormat == gfx::SurfaceFormat::B8G8R8A8 ||
                              dstFormat == gfx::SurfaceFormat::B8G8R8X8);

    size_t width = src->mSize.width;
    size_t height = src->mSize.height;
    MOZ_ASSERT(width == (size_t)dstSize.width);
    MOZ_ASSERT(height == (size_t)dstSize.height);

    GLenum readGLFormat;
    GLenum readType;

    {
        ScopedReadbackFB autoReadback(src);


        // We have a source FB, now we need a format.
        GLenum dstGLFormat = isDstRGBA ? LOCAL_GL_BGRA : LOCAL_GL_RGBA;
        GLenum dstType = LOCAL_GL_UNSIGNED_BYTE;

        // We actually don't care if they match, since we can handle
        // any read{Format,Type} we get.
        GLContext* gl = src->mGL;
        GetActualReadFormats(gl, dstGLFormat, dstType, &readGLFormat,
                             &readType);

        MOZ_ASSERT(readGLFormat == LOCAL_GL_RGBA ||
                   readGLFormat == LOCAL_GL_BGRA);
        MOZ_ASSERT(readType == LOCAL_GL_UNSIGNED_BYTE);

        // ReadPixels from the current FB into lockedBits.
        {
            size_t alignment = 8;
            if (dstStride % 4 == 0)
                alignment = 4;
            ScopedPackAlignment autoAlign(gl, alignment);

            gl->raw_fReadPixels(0, 0, width, height, readGLFormat, readType,
                                dstBytes);
        }
    }

    const bool isReadRGBA = readGLFormat == LOCAL_GL_RGBA;

    if (isReadRGBA != isDstRGBA) {
        for (size_t j = 0; j < height; ++j) {
            uint8_t* rowItr = dstBytes + j*dstStride;
            uint8_t* rowEnd = rowItr + 4*width;
            while (rowItr != rowEnd) {
                Swap(rowItr[0], rowItr[2]);
                rowItr += 4;
            }
        }
    }

    return true;
}

uint32_t
ReadPixel(SharedSurface* src)
{
    GLContext* gl = src->mGL;

    uint32_t pixel;

    ScopedReadbackFB a(src);
    {
        ScopedPackAlignment autoAlign(gl, 4);

        UniquePtr<uint8_t[]> bytes(new uint8_t[4]);
        gl->raw_fReadPixels(0, 0, 1, 1, LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_BYTE,
                            bytes.get());
        memcpy(&pixel, bytes.get(), 4);
    }

    return pixel;
}

} // namespace gl

} /* namespace mozilla */
