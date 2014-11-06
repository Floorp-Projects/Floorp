/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLContextEGL.h"
#include "GLSharedHandleHelpers.h"
#ifdef MOZ_WIDGET_ANDROID
#include "nsSurfaceTexture.h"
#endif

namespace mozilla {
namespace gl {

enum SharedHandleType {
    SharedHandleType_Image
#ifdef MOZ_WIDGET_ANDROID
    , SharedHandleType_SurfaceTexture
#endif
};

class SharedTextureHandleWrapper
{
public:
    explicit SharedTextureHandleWrapper(SharedHandleType aHandleType) : mHandleType(aHandleType)
    {
    }

    virtual ~SharedTextureHandleWrapper()
    {
    }

    SharedHandleType Type() { return mHandleType; }

    SharedHandleType mHandleType;
};

#ifdef MOZ_WIDGET_ANDROID

class SurfaceTextureWrapper: public SharedTextureHandleWrapper
{
public:
    SurfaceTextureWrapper(nsSurfaceTexture* aSurfaceTexture) :
        SharedTextureHandleWrapper(SharedHandleType_SurfaceTexture)
        , mSurfaceTexture(aSurfaceTexture)
    {
    }

    virtual ~SurfaceTextureWrapper() {
        mSurfaceTexture = nullptr;
    }

    nsSurfaceTexture* SurfaceTexture() { return mSurfaceTexture; }

    nsRefPtr<nsSurfaceTexture> mSurfaceTexture;
};

#endif // MOZ_WIDGET_ANDROID

class EGLTextureWrapper : public SharedTextureHandleWrapper
{
public:
    EGLTextureWrapper() :
        SharedTextureHandleWrapper(SharedHandleType_Image)
        , mEGLImage(nullptr)
        , mSyncObject(nullptr)
    {
    }

    // Args are the active GL context, and a texture in that GL
    // context for which to create an EGLImage.  After the EGLImage
    // is created, the texture is unused by EGLTextureWrapper.
    bool CreateEGLImage(GLContext *ctx, uintptr_t texture) {
        MOZ_ASSERT(!mEGLImage && texture && sEGLLibrary.HasKHRImageBase());
        static const EGLint eglAttributes[] = {
            LOCAL_EGL_NONE
        };
        EGLContext eglContext = GLContextEGL::Cast(ctx)->GetEGLContext();
        mEGLImage = sEGLLibrary.fCreateImage(EGL_DISPLAY(), eglContext, LOCAL_EGL_GL_TEXTURE_2D,
                                             (EGLClientBuffer)texture, eglAttributes);
        if (!mEGLImage) {
#ifdef DEBUG
            printf_stderr("Could not create EGL images: ERROR (0x%04x)\n", sEGLLibrary.fGetError());
#endif
            return false;
        }
        return true;
    }

    virtual ~EGLTextureWrapper() {
        if (mEGLImage) {
            sEGLLibrary.fDestroyImage(EGL_DISPLAY(), mEGLImage);
            mEGLImage = nullptr;
        }
    }

    const EGLImage GetEGLImage() {
        return mEGLImage;
    }

    // Insert a sync point on the given context, which should be the current active
    // context.
    bool MakeSync(GLContext *ctx) {
        MOZ_ASSERT(mSyncObject == nullptr);

        if (sEGLLibrary.IsExtensionSupported(GLLibraryEGL::KHR_fence_sync)) {
            mSyncObject = sEGLLibrary.fCreateSync(EGL_DISPLAY(), LOCAL_EGL_SYNC_FENCE, nullptr);
            // We need to flush to make sure the sync object enters the command stream;
            // we can't use EGL_SYNC_FLUSH_COMMANDS_BIT at wait time, because the wait
            // happens on a different thread/context.
            ctx->fFlush();
        }

        if (mSyncObject == EGL_NO_SYNC) {
            // we failed to create one, so just do a finish
            ctx->fFinish();
        }

        return true;
    }

    bool WaitSync() {
        if (!mSyncObject) {
            // if we have no sync object, then we did a Finish() earlier
            return true;
        }

        // wait at most 1 second; this should really be never/rarely hit
        const uint64_t ns_per_ms = 1000 * 1000;
        EGLTime timeout = 1000 * ns_per_ms;

        EGLint result = sEGLLibrary.fClientWaitSync(EGL_DISPLAY(), mSyncObject, 0, timeout);
        sEGLLibrary.fDestroySync(EGL_DISPLAY(), mSyncObject);
        mSyncObject = nullptr;

        return result == LOCAL_EGL_CONDITION_SATISFIED;
    }

private:
    EGLImage mEGLImage;
    EGLSync mSyncObject;
};

static bool DoesEGLContextSupportSharingWithEGLImage(GLContext *gl)
{
    return sEGLLibrary.HasKHRImageBase() &&
           sEGLLibrary.HasKHRImageTexture2D() &&
           gl->IsExtensionSupported(GLContext::OES_EGL_image);
}

SharedTextureHandle CreateSharedHandle(GLContext* gl,
                                       SharedTextureShareType shareType,
                                       void* buffer,
                                       SharedTextureBufferType bufferType)
{
    // unimplemented outside of EGL
    if (gl->GetContextType() != GLContextType::EGL)
        return 0;

    // Both EGLImage and SurfaceTexture only support same-process currently, but
    // it's possible to make SurfaceTexture work across processes. We should do that.
    if (shareType != SharedTextureShareType::SameProcess)
        return 0;

    switch (bufferType) {
#ifdef MOZ_WIDGET_ANDROID
    case SharedTextureBufferType::SurfaceTexture:
        if (!gl->IsExtensionSupported(GLContext::OES_EGL_image_external)) {
            NS_WARNING("Missing GL_OES_EGL_image_external");
            return 0;
        }

        return (SharedTextureHandle) new SurfaceTextureWrapper(reinterpret_cast<nsSurfaceTexture*>(buffer));
#endif
    case SharedTextureBufferType::TextureID: {
        if (!DoesEGLContextSupportSharingWithEGLImage(gl))
            return 0;

        GLuint texture = (uintptr_t)buffer;
        EGLTextureWrapper* tex = new EGLTextureWrapper();
        if (!tex->CreateEGLImage(gl, texture)) {
            NS_ERROR("EGLImage creation for EGLTextureWrapper failed");
            delete tex;
            return 0;
        }

        return (SharedTextureHandle)tex;
    }
    default:
        NS_ERROR("Unknown shared texture buffer type");
        return 0;
    }
}

void ReleaseSharedHandle(GLContext* gl,
                         SharedTextureShareType shareType,
                         SharedTextureHandle sharedHandle)
{
    // unimplemented outside of EGL
    if (gl->GetContextType() != GLContextType::EGL)
        return;

    if (shareType != SharedTextureShareType::SameProcess) {
        NS_ERROR("Implementation not available for this sharing type");
        return;
    }

    SharedTextureHandleWrapper* wrapper = reinterpret_cast<SharedTextureHandleWrapper*>(sharedHandle);

    switch (wrapper->Type()) {
#ifdef MOZ_WIDGET_ANDROID
    case SharedHandleType_SurfaceTexture:
        delete wrapper;
        break;
#endif

    case SharedHandleType_Image: {
        NS_ASSERTION(DoesEGLContextSupportSharingWithEGLImage(gl), "EGLImage not supported or disabled in runtime");

        EGLTextureWrapper* wrap = (EGLTextureWrapper*)sharedHandle;
        delete wrap;
        break;
    }

    default:
        NS_ERROR("Unknown shared handle type");
        return;
    }
}

bool GetSharedHandleDetails(GLContext* gl,
                            SharedTextureShareType shareType,
                            SharedTextureHandle sharedHandle,
                            SharedHandleDetails& details)
{
    // unimplemented outside of EGL
    if (gl->GetContextType() != GLContextType::EGL)
        return false;

    if (shareType != SharedTextureShareType::SameProcess)
        return false;

    SharedTextureHandleWrapper* wrapper = reinterpret_cast<SharedTextureHandleWrapper*>(sharedHandle);

    switch (wrapper->Type()) {
#ifdef MOZ_WIDGET_ANDROID
    case SharedHandleType_SurfaceTexture: {
        SurfaceTextureWrapper* surfaceWrapper = reinterpret_cast<SurfaceTextureWrapper*>(wrapper);

        details.mTarget = LOCAL_GL_TEXTURE_EXTERNAL;
        details.mTextureFormat = gfx::SurfaceFormat::R8G8B8A8;
        surfaceWrapper->SurfaceTexture()->GetTransformMatrix(details.mTextureTransform);
        break;
    }
#endif

    case SharedHandleType_Image:
        details.mTarget = LOCAL_GL_TEXTURE_2D;
        details.mTextureFormat = gfx::SurfaceFormat::R8G8B8A8;
        break;

    default:
        NS_ERROR("Unknown shared handle type");
        return false;
    }

    return true;
}

bool AttachSharedHandle(GLContext* gl,
                        SharedTextureShareType shareType,
                        SharedTextureHandle sharedHandle)
{
    // unimplemented outside of EGL
    if (gl->GetContextType() != GLContextType::EGL)
        return false;

    if (shareType != SharedTextureShareType::SameProcess)
        return false;

    SharedTextureHandleWrapper* wrapper = reinterpret_cast<SharedTextureHandleWrapper*>(sharedHandle);

    switch (wrapper->Type()) {
#ifdef MOZ_WIDGET_ANDROID
    case SharedHandleType_SurfaceTexture: {
#ifndef DEBUG
        /*
         * NOTE: SurfaceTexture spams us if there are any existing GL errors, so we'll clear
         * them here in order to avoid that.
         */
        gl->GetAndClearError();
#endif
        SurfaceTextureWrapper* surfaceTextureWrapper = reinterpret_cast<SurfaceTextureWrapper*>(wrapper);

        // FIXME: SurfaceTexture provides a transform matrix which is supposed to
        // be applied to the texture coordinates. We should return that here
        // so we can render correctly. Bug 775083
        surfaceTextureWrapper->SurfaceTexture()->UpdateTexImage();
        break;
    }
#endif // MOZ_WIDGET_ANDROID

    case SharedHandleType_Image: {
        NS_ASSERTION(DoesEGLContextSupportSharingWithEGLImage(gl), "EGLImage not supported or disabled in runtime");

        EGLTextureWrapper* wrap = (EGLTextureWrapper*)sharedHandle;
        wrap->WaitSync();
        gl->fEGLImageTargetTexture2D(LOCAL_GL_TEXTURE_2D, wrap->GetEGLImage());
        break;
    }

    default:
        NS_ERROR("Unknown shared handle type");
        return false;
    }

    return true;
}

/**
  * Detach Shared GL Handle from GL_TEXTURE_2D target
  */
void DetachSharedHandle(GLContext*,
                        SharedTextureShareType,
                        SharedTextureHandle)
{
  // currently a no-operation
}

}
}
