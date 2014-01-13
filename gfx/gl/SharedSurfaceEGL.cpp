/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedSurfaceEGL.h"
#include "GLContextEGL.h"
#include "GLBlitHelper.h"
#include "ScopedGLHelpers.h"
#include "SharedSurfaceGL.h"
#include "SurfaceFactory.h"
#include "GLLibraryEGL.h"
#include "TextureGarbageBin.h"
#include "GLReadTexImageHelper.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace gl {

SharedSurface_EGLImage*
SharedSurface_EGLImage::Create(GLContext* prodGL,
                               const GLFormats& formats,
                               const gfx::IntSize& size,
                               bool hasAlpha,
                               EGLContext context)
{
    GLLibraryEGL* egl = &sEGLLibrary;
    MOZ_ASSERT(egl);

    if (!HasExtensions(egl, prodGL))
        return nullptr;

    MOZ_ALWAYS_TRUE(prodGL->MakeCurrent());
    GLuint prodTex = CreateTextureForOffscreen(prodGL, formats, size);
    if (!prodTex)
        return nullptr;

    return new SharedSurface_EGLImage(prodGL, egl,
                                      size, hasAlpha,
                                      formats, prodTex);
}


bool
SharedSurface_EGLImage::HasExtensions(GLLibraryEGL* egl, GLContext* gl)
{
    return egl->HasKHRImageBase() &&
           egl->IsExtensionSupported(GLLibraryEGL::KHR_gl_texture_2D_image) &&
           gl->IsExtensionSupported(GLContext::OES_EGL_image);
}

SharedSurface_EGLImage::SharedSurface_EGLImage(GLContext* gl,
                                               GLLibraryEGL* egl,
                                               const gfx::IntSize& size,
                                               bool hasAlpha,
                                               const GLFormats& formats,
                                               GLuint prodTex)
    : SharedSurface_GL(SharedSurfaceType::EGLImageShare,
                        AttachmentType::GLTexture,
                        gl,
                        size,
                        hasAlpha)
    , mMutex("SharedSurface_EGLImage mutex")
    , mEGL(egl)
    , mFormats(formats)
    , mProdTex(prodTex)
    , mProdTexForPipe(0)
    , mImage(0)
    , mCurConsGL(nullptr)
    , mConsTex(0)
    , mSync(0)
    , mPipeFailed(false)
    , mPipeComplete(false)
    , mPipeActive(false)
{}

SharedSurface_EGLImage::~SharedSurface_EGLImage()
{
    mEGL->fDestroyImage(Display(), mImage);
    mImage = 0;

    mGL->MakeCurrent();
    mGL->fDeleteTextures(1, &mProdTex);
    mProdTex = 0;

    if (mProdTexForPipe) {
        mGL->fDeleteTextures(1, &mProdTexForPipe);
        mProdTexForPipe = 0;
    }

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

void
SharedSurface_EGLImage::LockProdImpl()
{
    MutexAutoLock lock(mMutex);

    if (!mPipeComplete)
        return;

    if (mPipeActive)
        return;

    mGL->BlitHelper()->BlitTextureToTexture(mProdTex, mProdTexForPipe, Size(), Size());
    mGL->fDeleteTextures(1, &mProdTex);
    mProdTex = mProdTexForPipe;
    mProdTexForPipe = 0;
    mPipeActive = true;
}

static bool
CreateTexturePipe(GLLibraryEGL* const egl, GLContext* const gl,
                  const GLFormats& formats, const gfx::IntSize& size,
                  GLuint* const out_tex, EGLImage* const out_image)
{
    MOZ_ASSERT(out_tex && out_image);
    *out_tex = 0;
    *out_image = 0;

    GLuint tex = CreateTextureForOffscreen(gl, formats, size);
    if (!tex)
        return false;

    EGLContext context = GLContextEGL::Cast(gl)->GetEGLContext();
    MOZ_ASSERT(context);
    EGLClientBuffer buffer = reinterpret_cast<EGLClientBuffer>(tex);
    EGLImage image = egl->fCreateImage(egl->Display(), context,
                                       LOCAL_EGL_GL_TEXTURE_2D, buffer,
                                       nullptr);
    if (!image) {
        gl->fDeleteTextures(1, &tex);
        return false;
    }

    // Success.
    *out_tex = tex;
    *out_image = image;
    return true;
}

void
SharedSurface_EGLImage::Fence()
{
    MutexAutoLock lock(mMutex);
    mGL->MakeCurrent();

    if (!mPipeActive) {
        MOZ_ASSERT(!mSync);
        MOZ_ASSERT(!mPipeComplete);

        if (!mPipeFailed) {
            if (!CreateTexturePipe(mEGL, mGL, mFormats, Size(),
                                   &mProdTexForPipe, &mImage))
            {
                mPipeFailed = true;
            }
        }

        if (!mPixels) {
            SurfaceFormat format =
                  HasAlpha() ? SurfaceFormat::B8G8R8A8
                             : SurfaceFormat::B8G8R8X8;
            mPixels = Factory::CreateDataSourceSurface(Size(), format);
        }

        nsRefPtr<gfxImageSurface> wrappedData =
            new gfxImageSurface(mPixels->GetData(),
                                ThebesIntSize(mPixels->GetSize()),
                                mPixels->Stride(),
                                SurfaceFormatToImageFormat(mPixels->GetFormat()));
        ReadScreenIntoImageSurface(mGL, wrappedData);
        mPixels->MarkDirty();
        return;
    }
    MOZ_ASSERT(mPipeActive);
    MOZ_ASSERT(mCurConsGL);

    if (mEGL->IsExtensionSupported(GLLibraryEGL::KHR_fence_sync) &&
        mGL->IsExtensionSupported(GLContext::OES_EGL_sync))
    {
        if (mSync) {
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

    if (status != LOCAL_EGL_CONDITION_SATISFIED) {
        return false;
    }

    MOZ_ALWAYS_TRUE( mEGL->fDestroySync(Display(), mSync) );
    mSync = 0;

    return true;
}


EGLDisplay
SharedSurface_EGLImage::Display() const
{
    return mEGL->Display();
}

GLuint
SharedSurface_EGLImage::AcquireConsumerTexture(GLContext* consGL)
{
    MutexAutoLock lock(mMutex);
    MOZ_ASSERT(!mCurConsGL || consGL == mCurConsGL);
    if (mPipeFailed)
        return 0;

    if (mPipeActive) {
        MOZ_ASSERT(mConsTex);

        return mConsTex;
    }

    if (!mConsTex) {
        consGL->fGenTextures(1, &mConsTex);
        ScopedBindTexture autoTex(consGL, mConsTex);
        consGL->fEGLImageTargetTexture2D(LOCAL_GL_TEXTURE_2D, mImage);

        mPipeComplete = true;
        mCurConsGL = consGL;
        mGarbageBin = consGL->TexGarbageBin();
    }

    MOZ_ASSERT(consGL == mCurConsGL);
    return 0;
}

DataSourceSurface*
SharedSurface_EGLImage::GetPixels() const
{
    MutexAutoLock lock(mMutex);
    return mPixels;
}



SurfaceFactory_EGLImage*
SurfaceFactory_EGLImage::Create(GLContext* prodGL,
                                        const SurfaceCaps& caps)
{
    EGLContext context = GLContextEGL::Cast(prodGL)->GetEGLContext();

    return new SurfaceFactory_EGLImage(prodGL, context, caps);
}

} /* namespace gfx */
} /* namespace mozilla */
