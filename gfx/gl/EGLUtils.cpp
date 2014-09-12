/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EGLUtils.h"

#include "GLContextEGL.h"

namespace mozilla {
namespace gl {

bool
DoesEGLContextSupportSharingWithEGLImage(GLContext* gl)
{
    return sEGLLibrary.HasKHRImageBase() &&
           sEGLLibrary.HasKHRImageTexture2D() &&
           gl->IsExtensionSupported(GLContext::OES_EGL_image);
}

EGLImage
CreateEGLImage(GLContext* gl, GLuint tex)
{
    MOZ_ASSERT(DoesEGLContextSupportSharingWithEGLImage(gl));

    EGLContext eglContext = GLContextEGL::Cast(gl)->GetEGLContext();
    EGLImage image = sEGLLibrary.fCreateImage(EGL_DISPLAY(),
                                              eglContext,
                                              LOCAL_EGL_GL_TEXTURE_2D,
                                              (EGLClientBuffer)tex,
                                              nullptr);
    return image;
}

////////////////////////////////////////////////////////////////////////
// EGLImageWrapper

/*static*/ EGLImageWrapper*
EGLImageWrapper::Create(GLContext* gl, GLuint tex)
{
    MOZ_ASSERT(DoesEGLContextSupportSharingWithEGLImage(gl));

    GLLibraryEGL& library = sEGLLibrary;
    EGLDisplay display = EGL_DISPLAY();
    EGLContext eglContext = GLContextEGL::Cast(gl)->GetEGLContext();
    EGLImage image = library.fCreateImage(display,
                                          eglContext,
                                          LOCAL_EGL_GL_TEXTURE_2D,
                                          (EGLClientBuffer)tex,
                                          nullptr);
    if (!image) {
#ifdef DEBUG
        printf_stderr("Could not create EGL images: ERROR (0x%04x)\n",
                      sEGLLibrary.fGetError());
#endif
        return nullptr;
    }

    return new EGLImageWrapper(library, display, image);
}

EGLImageWrapper::~EGLImageWrapper()
{
    mLibrary.fDestroyImage(mDisplay, mImage);
}

bool
EGLImageWrapper::FenceSync(GLContext* gl)
{
    MOZ_ASSERT(!mSync);

    if (mLibrary.IsExtensionSupported(GLLibraryEGL::KHR_fence_sync)) {
        mSync = mLibrary.fCreateSync(mDisplay,
                                     LOCAL_EGL_SYNC_FENCE,
                                     nullptr);
        // We need to flush to make sure the sync object enters the command stream;
        // we can't use EGL_SYNC_FLUSH_COMMANDS_BIT at wait time, because the wait
        // happens on a different thread/context.
        gl->fFlush();
    }

    if (!mSync) {
        // we failed to create one, so just do a finish
        gl->fFinish();
    }

    return true;
}

bool
EGLImageWrapper::ClientWaitSync()
{
    if (!mSync) {
        // if we have no sync object, then we did a Finish() earlier
        return true;
    }

    // wait at most 1 second; this should really be never/rarely hit
    const uint64_t ns_per_ms = 1000 * 1000;
    EGLTime timeout = 1000 * ns_per_ms;

    EGLint result = mLibrary.fClientWaitSync(mDisplay,
                                             mSync,
                                             0,
                                             timeout);
    mLibrary.fDestroySync(mDisplay, mSync);
    mSync = nullptr;

    return result == LOCAL_EGL_CONDITION_SATISFIED;
}

} // namespace gl
} // namespace mozilla
