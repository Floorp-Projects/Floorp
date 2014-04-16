/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedSurfaceIO.h"
#include "GLContextCGL.h"
#include "mozilla/gfx/MacIOSurface.h"
#include "mozilla/DebugOnly.h"
#include "ScopedGLHelpers.h"

namespace mozilla {
namespace gl {

using namespace gfx;

/* static */ SharedSurface_IOSurface*
SharedSurface_IOSurface::Create(MacIOSurface* surface, GLContext *gl, bool hasAlpha)
{
    MOZ_ASSERT(surface);
    MOZ_ASSERT(gl);

    gfx::IntSize size(surface->GetWidth(), surface->GetHeight());
    return new SharedSurface_IOSurface(surface, gl, size, hasAlpha);
}

void
SharedSurface_IOSurface::Fence()
{
    mGL->MakeCurrent();
    mGL->fFlush();
}

bool
SharedSurface_IOSurface::ReadPixels(GLint x, GLint y, GLsizei width, GLsizei height,
                                    GLenum format, GLenum type, GLvoid *pixels)
{
    // Calling glReadPixels when an IOSurface is bound to the current framebuffer
    // can cause corruption in following glReadPixel calls (even if they aren't
    // reading from an IOSurface).
    // We workaround this by copying to a temporary texture, and doing the readback
    // from that.
    MOZ_ASSERT(mGL->IsCurrent());


    ScopedTexture destTex(mGL);
    {
        ScopedFramebufferForTexture srcFB(mGL, ProdTexture(), ProdTextureTarget());

        ScopedBindFramebuffer bindFB(mGL, srcFB.FB());
        ScopedBindTexture bindTex(mGL, destTex.Texture());
        mGL->fCopyTexImage2D(LOCAL_GL_TEXTURE_2D, 0,
                             HasAlpha() ? LOCAL_GL_RGBA : LOCAL_GL_RGB,
                             x, y,
                             width, height, 0);
    }

    ScopedFramebufferForTexture destFB(mGL, destTex.Texture());

    ScopedBindFramebuffer bindFB(mGL, destFB.FB());
    mGL->fReadPixels(0, 0, width, height, format, type, pixels);
    return true;
}

static void
BackTextureWithIOSurf(GLContext* gl, GLuint tex, MacIOSurface* ioSurf)
{
    MOZ_ASSERT(gl->IsCurrent());

    ScopedBindTexture texture(gl, tex, LOCAL_GL_TEXTURE_RECTANGLE_ARB);

    gl->fTexParameteri(LOCAL_GL_TEXTURE_RECTANGLE_ARB,
                        LOCAL_GL_TEXTURE_MIN_FILTER,
                        LOCAL_GL_LINEAR);
    gl->fTexParameteri(LOCAL_GL_TEXTURE_RECTANGLE_ARB,
                        LOCAL_GL_TEXTURE_MAG_FILTER,
                        LOCAL_GL_LINEAR);
    gl->fTexParameteri(LOCAL_GL_TEXTURE_RECTANGLE_ARB,
                        LOCAL_GL_TEXTURE_WRAP_S,
                        LOCAL_GL_CLAMP_TO_EDGE);
    gl->fTexParameteri(LOCAL_GL_TEXTURE_RECTANGLE_ARB,
                        LOCAL_GL_TEXTURE_WRAP_T,
                        LOCAL_GL_CLAMP_TO_EDGE);

    CGLContextObj cgl = GLContextCGL::Cast(gl)->GetCGLContext();
    MOZ_ASSERT(cgl);

    ioSurf->CGLTexImageIOSurface2D(cgl);
}

SharedSurface_IOSurface::SharedSurface_IOSurface(MacIOSurface* surface,
                                                 GLContext* gl,
                                                 const gfx::IntSize& size,
                                                 bool hasAlpha)
  : SharedSurface_GL(SharedSurfaceType::IOSurface, AttachmentType::GLTexture, gl, size, hasAlpha)
  , mSurface(surface)
  , mCurConsGL(nullptr)
  , mConsTex(0)
{
    gl->MakeCurrent();
    mProdTex = 0;
    gl->fGenTextures(1, &mProdTex);
    BackTextureWithIOSurf(gl, mProdTex, surface);
}

GLuint
SharedSurface_IOSurface::ConsTexture(GLContext* consGL)
{
    if (!mCurConsGL) {
        mCurConsGL = consGL;
    }
    MOZ_ASSERT(consGL == mCurConsGL);

    if (!mConsTex) {
        consGL->MakeCurrent();
        mConsTex = 0;
        consGL->fGenTextures(1, &mConsTex);
        BackTextureWithIOSurf(consGL, mConsTex, mSurface);
    }

    return mConsTex;
}

SharedSurface_IOSurface::~SharedSurface_IOSurface()
{
    if (mProdTex) {
        DebugOnly<bool> success = mGL->MakeCurrent();
        MOZ_ASSERT(success);
        mGL->fDeleteTextures(1, &mProdTex);
        mGL->fDeleteTextures(1, &mConsTex); // This will work if we're shared.
    }
}

SharedSurface*
SurfaceFactory_IOSurface::CreateShared(const gfx::IntSize& size)
{
    bool hasAlpha = mReadCaps.alpha;
    RefPtr<MacIOSurface> surf =
        MacIOSurface::CreateIOSurface(size.width, size.height, 1.0, hasAlpha);

    if (!surf) {
        NS_WARNING("Failed to create MacIOSurface.");
        return nullptr;
    }

    return SharedSurface_IOSurface::Create(surf, mGL, hasAlpha);
}

}
}
