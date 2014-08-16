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

/*static*/ UniquePtr<SharedSurface_IOSurface>
SharedSurface_IOSurface::Create(const RefPtr<MacIOSurface>& ioSurf,
                                GLContext* gl,
                                bool hasAlpha)
{
    MOZ_ASSERT(ioSurf);
    MOZ_ASSERT(gl);

    gfx::IntSize size(ioSurf->GetWidth(), ioSurf->GetHeight());

    typedef SharedSurface_IOSurface ptrT;
    UniquePtr<ptrT> ret( new ptrT(ioSurf, gl, size, hasAlpha) );
    return Move(ret);
}

void
SharedSurface_IOSurface::Fence()
{
    mGL->MakeCurrent();
    mGL->fFlush();
}

bool
SharedSurface_IOSurface::ReadPixels(GLint x, GLint y, GLsizei width, GLsizei height,
                                    GLenum format, GLenum type, GLvoid* pixels)
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
                             mHasAlpha ? LOCAL_GL_RGBA : LOCAL_GL_RGB,
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

SharedSurface_IOSurface::SharedSurface_IOSurface(const RefPtr<MacIOSurface>& ioSurf,
                                                 GLContext* gl,
                                                 const gfx::IntSize& size,
                                                 bool hasAlpha)
  : SharedSurface(SharedSurfaceType::IOSurface,
                  AttachmentType::GLTexture,
                  gl,
                  size,
                  hasAlpha)
  , mIOSurf(ioSurf)
  , mCurConsGL(nullptr)
  , mConsTex(0)
{
    gl->MakeCurrent();
    mProdTex = 0;
    gl->fGenTextures(1, &mProdTex);
    BackTextureWithIOSurf(gl, mProdTex, mIOSurf);
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
        BackTextureWithIOSurf(consGL, mConsTex, mIOSurf);
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

////////////////////////////////////////////////////////////////////////
// SurfaceFactory_IOSurface

/*static*/ UniquePtr<SurfaceFactory_IOSurface>
SurfaceFactory_IOSurface::Create(GLContext* gl,
                                 const SurfaceCaps& caps)
{
    gfx::IntSize maxDims(MacIOSurface::GetMaxWidth(),
                         MacIOSurface::GetMaxHeight());

    typedef SurfaceFactory_IOSurface ptrT;
    UniquePtr<ptrT> ret( new ptrT(gl, caps, maxDims) );
    return Move(ret);
}

UniquePtr<SharedSurface>
SurfaceFactory_IOSurface::CreateShared(const gfx::IntSize& size)
{
    if (size.width > mMaxDims.width ||
        size.height > mMaxDims.height)
    {
        return nullptr;
    }

    bool hasAlpha = mReadCaps.alpha;
    RefPtr<MacIOSurface> ioSurf;
    ioSurf = MacIOSurface::CreateIOSurface(size.width, size.height, 1.0,
                                           hasAlpha);

    if (!ioSurf) {
        NS_WARNING("Failed to create MacIOSurface.");
        return nullptr;
    }

    return SharedSurface_IOSurface::Create(ioSurf, mGL, hasAlpha);
}

}
}
