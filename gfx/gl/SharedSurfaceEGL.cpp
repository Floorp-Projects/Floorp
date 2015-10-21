/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedSurfaceEGL.h"

#include "GLBlitHelper.h"
#include "GLContextEGL.h"
#include "GLLibraryEGL.h"
#include "GLReadTexImageHelper.h"
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor, etc
#include "SharedSurface.h"
#include "TextureGarbageBin.h"

namespace mozilla {
namespace gl {

/*static*/ UniquePtr<SharedSurface_EGLImage>
SharedSurface_EGLImage::Create(GLContext* prodGL,
                               const GLFormats& formats,
                               const gfx::IntSize& size,
                               bool hasAlpha,
                               EGLContext context)
{
    GLLibraryEGL* egl = &sEGLLibrary;
    MOZ_ASSERT(egl);
    MOZ_ASSERT(context);

    UniquePtr<SharedSurface_EGLImage> ret;

    if (!HasExtensions(egl, prodGL)) {
        return Move(ret);
    }

    MOZ_ALWAYS_TRUE(prodGL->MakeCurrent());
    GLuint prodTex = CreateTextureForOffscreen(prodGL, formats, size);
    if (!prodTex) {
        return Move(ret);
    }

    EGLClientBuffer buffer = reinterpret_cast<EGLClientBuffer>(prodTex);
    EGLImage image = egl->fCreateImage(egl->Display(), context,
                                       LOCAL_EGL_GL_TEXTURE_2D, buffer,
                                       nullptr);
    if (!image) {
        prodGL->fDeleteTextures(1, &prodTex);
        return Move(ret);
    }

    ret.reset( new SharedSurface_EGLImage(prodGL, egl, size, hasAlpha,
                                          formats, prodTex, image) );
    return Move(ret);
}

bool
SharedSurface_EGLImage::HasExtensions(GLLibraryEGL* egl, GLContext* gl)
{
    return egl->HasKHRImageBase() &&
           egl->IsExtensionSupported(GLLibraryEGL::KHR_gl_texture_2D_image) &&
           gl->IsExtensionSupported(GLContext::OES_EGL_image_external);
}

SharedSurface_EGLImage::SharedSurface_EGLImage(GLContext* gl,
                                               GLLibraryEGL* egl,
                                               const gfx::IntSize& size,
                                               bool hasAlpha,
                                               const GLFormats& formats,
                                               GLuint prodTex,
                                               EGLImage image)
    : SharedSurface(SharedSurfaceType::EGLImageShare,
                    AttachmentType::GLTexture,
                    gl,
                    size,
                    hasAlpha,
                    false) // Can't recycle, as mSync changes never update TextureHost.
    , mMutex("SharedSurface_EGLImage mutex")
    , mEGL(egl)
    , mFormats(formats)
    , mProdTex(prodTex)
    , mImage(image)
    , mCurConsGL(nullptr)
    , mConsTex(0)
    , mSync(0)
{}

SharedSurface_EGLImage::~SharedSurface_EGLImage()
{
    mEGL->fDestroyImage(Display(), mImage);

    mGL->MakeCurrent();
    mGL->fDeleteTextures(1, &mProdTex);
    mProdTex = 0;

    if (mConsTex) {
        MOZ_ASSERT(mGarbageBin);
        mGarbageBin->Trash(mConsTex);
        mConsTex = 0;
    }

    if (mSync) {
        // We can't call this unless we have the ext, but we will always have
        // the ext if we have something to destroy.
        mEGL->fDestroySync(Display(), mSync);
        mSync = 0;
    }
}

layers::TextureFlags
SharedSurface_EGLImage::GetTextureFlags() const
{
    return layers::TextureFlags::DEALLOCATE_CLIENT;
}

void
SharedSurface_EGLImage::Fence()
{
    MutexAutoLock lock(mMutex);
    mGL->MakeCurrent();

    if (mEGL->IsExtensionSupported(GLLibraryEGL::KHR_fence_sync) &&
        mGL->IsExtensionSupported(GLContext::OES_EGL_sync))
    {
        if (mSync) {
            MOZ_RELEASE_ASSERT(false, "Non-recycleable should not Fence twice.");
            MOZ_ALWAYS_TRUE( mEGL->fDestroySync(Display(), mSync) );
            mSync = 0;
        }

        mSync = mEGL->fCreateSync(Display(),
                                  LOCAL_EGL_SYNC_FENCE,
                                  nullptr);
        if (mSync) {
            mGL->fFlush();
            return;
        }
    }

    MOZ_ASSERT(!mSync);
    mGL->fFinish();
}

bool
SharedSurface_EGLImage::WaitSync()
{
    MutexAutoLock lock(mMutex);
    if (!mSync) {
        // We must not be needed.
        return true;
    }
    MOZ_ASSERT(mEGL->IsExtensionSupported(GLLibraryEGL::KHR_fence_sync));

    // Wait FOREVER, primarily because some NVIDIA (at least Tegra) drivers
    // have ClientWaitSync returning immediately if the timeout delay is anything
    // else than FOREVER.
    //
    // FIXME: should we try to use a finite timeout delay where possible?
    EGLint status = mEGL->fClientWaitSync(Display(),
                                          mSync,
                                          0,
                                          LOCAL_EGL_FOREVER);

    return status == LOCAL_EGL_CONDITION_SATISFIED;
}

bool
SharedSurface_EGLImage::PollSync()
{
    MutexAutoLock lock(mMutex);
    if (!mSync) {
        // We must not be needed.
        return true;
    }
    MOZ_ASSERT(mEGL->IsExtensionSupported(GLLibraryEGL::KHR_fence_sync));

    EGLint status = 0;
    MOZ_ALWAYS_TRUE( mEGL->fGetSyncAttrib(mEGL->Display(),
                                         mSync,
                                         LOCAL_EGL_SYNC_STATUS_KHR,
                                         &status) );

    return status == LOCAL_EGL_SIGNALED_KHR;
}

EGLDisplay
SharedSurface_EGLImage::Display() const
{
    return mEGL->Display();
}

void
SharedSurface_EGLImage::AcquireConsumerTexture(GLContext* consGL, GLuint* out_texture, GLuint* out_target)
{
    MutexAutoLock lock(mMutex);
    MOZ_ASSERT(!mCurConsGL || consGL == mCurConsGL);

    if (!mConsTex) {
        consGL->fGenTextures(1, &mConsTex);
        MOZ_ASSERT(mConsTex);

        ScopedBindTexture autoTex(consGL, mConsTex, LOCAL_GL_TEXTURE_EXTERNAL);
        consGL->fEGLImageTargetTexture2D(LOCAL_GL_TEXTURE_EXTERNAL, mImage);

        mCurConsGL = consGL;
        mGarbageBin = consGL->TexGarbageBin();
    }

    MOZ_ASSERT(consGL == mCurConsGL);
    *out_texture = mConsTex;
    *out_target = LOCAL_GL_TEXTURE_EXTERNAL;
}

bool
SharedSurface_EGLImage::ToSurfaceDescriptor(layers::SurfaceDescriptor* const out_descriptor)
{
    *out_descriptor = layers::EGLImageDescriptor((uintptr_t)mImage, (uintptr_t)mSync,
                                                 mSize, mHasAlpha);
    return true;
}

bool
SharedSurface_EGLImage::ReadbackBySharedHandle(gfx::DataSourceSurface* out_surface)
{
    MOZ_ASSERT(out_surface);
    MOZ_ASSERT(NS_IsMainThread());
    return sEGLLibrary.ReadbackEGLImage(mImage, out_surface);
}

////////////////////////////////////////////////////////////////////////

/*static*/ UniquePtr<SurfaceFactory_EGLImage>
SurfaceFactory_EGLImage::Create(GLContext* prodGL, const SurfaceCaps& caps,
                                const RefPtr<layers::ISurfaceAllocator>& allocator,
                                const layers::TextureFlags& flags)
{
    EGLContext context = GLContextEGL::Cast(prodGL)->mContext;

    typedef SurfaceFactory_EGLImage ptrT;
    UniquePtr<ptrT> ret;

    GLLibraryEGL* egl = &sEGLLibrary;
    if (SharedSurface_EGLImage::HasExtensions(egl, prodGL)) {
        ret.reset( new ptrT(prodGL, caps, allocator, flags, context) );
    }

    return Move(ret);
}

} // namespace gl

} /* namespace mozilla */
