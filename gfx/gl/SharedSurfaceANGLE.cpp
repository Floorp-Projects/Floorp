/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedSurfaceANGLE.h"

#include "GLContextEGL.h"
#include "GLLibraryEGL.h"

namespace mozilla {
namespace gl {

// Returns `EGL_NO_SURFACE` (`0`) on error.
static EGLSurface
CreatePBufferSurface(GLLibraryEGL* egl,
                     EGLDisplay display,
                     EGLConfig config,
                     const gfx::IntSize& size)
{
    auto width = size.width;
    auto height = size.height;

    EGLint attribs[] = {
        LOCAL_EGL_WIDTH, width,
        LOCAL_EGL_HEIGHT, height,
        LOCAL_EGL_NONE
    };

    DebugOnly<EGLint> preCallErr = egl->fGetError();
    MOZ_ASSERT(preCallErr == LOCAL_EGL_SUCCESS);
    EGLSurface surface = egl->fCreatePbufferSurface(display, config, attribs);
    EGLint err = egl->fGetError();
    if (err != LOCAL_EGL_SUCCESS)
        return 0;

    return surface;
}

/*static*/ UniquePtr<SharedSurface_ANGLEShareHandle>
SharedSurface_ANGLEShareHandle::Create(GLContext* gl,
                                       EGLContext context, EGLConfig config,
                                       const gfx::IntSize& size, bool hasAlpha)
{
    GLLibraryEGL* egl = &sEGLLibrary;
    MOZ_ASSERT(egl);
    MOZ_ASSERT(egl->IsExtensionSupported(
               GLLibraryEGL::ANGLE_surface_d3d_texture_2d_share_handle));

    if (!context || !config)
        return nullptr;

    EGLDisplay display = egl->Display();
    EGLSurface pbuffer = CreatePBufferSurface(egl, display, config, size);
    if (!pbuffer)
        return nullptr;

    // Declare everything before 'goto's.
    HANDLE shareHandle = nullptr;
    bool ok = egl->fQuerySurfacePointerANGLE(display,
                                             pbuffer,
                                             LOCAL_EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE,
                                             &shareHandle);
    if (!ok) {
        egl->fDestroySurface(egl->Display(), pbuffer);
        return nullptr;
    }

    GLuint fence = 0;
    if (gl->IsExtensionSupported(GLContext::NV_fence)) {
        gl->MakeCurrent();
        gl->fGenFences(1, &fence);
    }

    typedef SharedSurface_ANGLEShareHandle ptrT;
    UniquePtr<ptrT> ret( new ptrT(gl, egl, size, hasAlpha, context,
                                  pbuffer, shareHandle, fence) );
    return Move(ret);
}

EGLDisplay
SharedSurface_ANGLEShareHandle::Display()
{
    return mEGL->Display();
}

SharedSurface_ANGLEShareHandle::SharedSurface_ANGLEShareHandle(GLContext* gl,
                                                               GLLibraryEGL* egl,
                                                               const gfx::IntSize& size,
                                                               bool hasAlpha,
                                                               EGLContext context,
                                                               EGLSurface pbuffer,
                                                               HANDLE shareHandle,
                                                               GLuint fence)
    : SharedSurface(SharedSurfaceType::EGLSurfaceANGLE,
                    AttachmentType::Screen,
                    gl,
                    size,
                    hasAlpha)
    , mEGL(egl)
    , mContext(context)
    , mPBuffer(pbuffer)
    , mShareHandle(shareHandle)
    , mFence(fence)
{
}


SharedSurface_ANGLEShareHandle::~SharedSurface_ANGLEShareHandle()
{
    mEGL->fDestroySurface(Display(), mPBuffer);

    if (mFence) {
        mGL->MakeCurrent();
        mGL->fDeleteFences(1, &mFence);
    }
}

void
SharedSurface_ANGLEShareHandle::LockProdImpl()
{
    GLContextEGL::Cast(mGL)->SetEGLSurfaceOverride(mPBuffer);
}

void
SharedSurface_ANGLEShareHandle::UnlockProdImpl()
{
}

void
SharedSurface_ANGLEShareHandle::Fence()
{
    mGL->fFinish();
}

bool
SharedSurface_ANGLEShareHandle::WaitSync()
{
    return true;
}

bool
SharedSurface_ANGLEShareHandle::PollSync()
{
    return true;
}

void
SharedSurface_ANGLEShareHandle::Fence_ContentThread_Impl()
{
    if (mFence) {
        MOZ_ASSERT(mGL->IsExtensionSupported(GLContext::NV_fence));
        mGL->fSetFence(mFence, LOCAL_GL_ALL_COMPLETED_NV);
        mGL->fFlush();
        return;
    }

    Fence();
}

bool
SharedSurface_ANGLEShareHandle::WaitSync_ContentThread_Impl()
{
    if (mFence) {
        mGL->MakeCurrent();
        mGL->fFinishFence(mFence);
        return true;
    }

    return WaitSync();
}

bool
SharedSurface_ANGLEShareHandle::PollSync_ContentThread_Impl()
{
    if (mFence) {
        mGL->MakeCurrent();
        return mGL->fTestFence(mFence);
    }

    return PollSync();
}

////////////////////////////////////////////////////////////////////////////////
// Factory

static void
FillPBufferAttribs_ByBits(nsTArray<EGLint>& aAttrs,
                          int redBits, int greenBits,
                          int blueBits, int alphaBits,
                          int depthBits, int stencilBits)
{
    aAttrs.Clear();

#if defined(A1) || defined(A2)
#error The temp-macro names we want are already defined.
#endif

#define A1(_x)      do { aAttrs.AppendElement(_x); } while (0)
#define A2(_x,_y)   do { A1(_x); A1(_y); } while (0)

    A2(LOCAL_EGL_RENDERABLE_TYPE, LOCAL_EGL_OPENGL_ES2_BIT);
    A2(LOCAL_EGL_SURFACE_TYPE, LOCAL_EGL_PBUFFER_BIT);

    A2(LOCAL_EGL_RED_SIZE, redBits);
    A2(LOCAL_EGL_GREEN_SIZE, greenBits);
    A2(LOCAL_EGL_BLUE_SIZE, blueBits);
    A2(LOCAL_EGL_ALPHA_SIZE, alphaBits);

    A2(LOCAL_EGL_DEPTH_SIZE, depthBits);
    A2(LOCAL_EGL_STENCIL_SIZE, stencilBits);

    A1(LOCAL_EGL_NONE);
#undef A1
#undef A2
}

static void
FillPBufferAttribs_BySizes(nsTArray<EGLint>& attribs,
                           bool bpp16, bool hasAlpha,
                           int depthBits, int stencilBits)
{
    int red = 0;
    int green = 0;
    int blue = 0;
    int alpha = 0;

    if (bpp16) {
        if (hasAlpha) {
            red = green = blue = alpha = 4;
        } else {
            red = 5;
            green = 6;
            blue = 5;
        }
    } else {
        red = green = blue = 8;
        if (hasAlpha)
            alpha = 8;
    }

    FillPBufferAttribs_ByBits(attribs, red, green, blue, alpha, depthBits,
                              stencilBits);
}

static bool
DoesAttribBitsMatchCapBool(GLLibraryEGL* egl, EGLConfig config, EGLint attrib,
                           bool capBool)
{
    EGLint bits = 0;
    egl->fGetConfigAttrib(egl->Display(), config, attrib, &bits);
    MOZ_ASSERT(egl->fGetError() == LOCAL_EGL_SUCCESS);

    bool hasBits = !!bits;

    return hasBits == capBool;
}

static EGLConfig
ChooseConfig(GLContext* gl, GLLibraryEGL* egl, const SurfaceCaps& caps)
{
    MOZ_ASSERT(egl);
    MOZ_ASSERT(caps.color);

    // We might want 24-bit depth, but we're only (fairly) sure to get 16-bit.
    int depthBits = caps.depth ? 16 : 0;
    int stencilBits = caps.stencil ? 8 : 0;

    // Ok, now we have everything.
    nsTArray<EGLint> attribs(32);
    FillPBufferAttribs_BySizes(attribs, caps.bpp16, caps.alpha, depthBits,
                               stencilBits);

    // Time to try to get this config:
    EGLConfig configs[64];
    int numConfigs = sizeof(configs)/sizeof(EGLConfig);
    int foundConfigs = 0;

    if (!egl->fChooseConfig(egl->Display(), attribs.Elements(), configs,
                            numConfigs, &foundConfigs) ||
        !foundConfigs)
    {
        NS_WARNING("No configs found for the requested formats.");
        return EGL_NO_CONFIG;
    }

    // The requests passed to ChooseConfig are treated as minimums. If you ask
    // for 0 bits of alpha, we might still get 8 bits.
    EGLConfig config = EGL_NO_CONFIG;
    for (int i = 0; i < foundConfigs; i++) {
        EGLConfig cur = configs[i];
        if (!DoesAttribBitsMatchCapBool(egl, cur, LOCAL_EGL_ALPHA_SIZE,
                                        caps.alpha) ||
            !DoesAttribBitsMatchCapBool(egl, cur, LOCAL_EGL_DEPTH_SIZE,
                                        caps.depth) ||
            !DoesAttribBitsMatchCapBool(egl, cur, LOCAL_EGL_STENCIL_SIZE,
                                        caps.stencil))
        {
            continue;
        }

        config = cur;
        break;
    }

    if (config == EGL_NO_CONFIG) {
        NS_WARNING("No acceptable EGLConfig found.");
        return EGL_NO_CONFIG;
    }

    if (gl->DebugMode()) {
        egl->DumpEGLConfig(config);
    }

    return config;
}

/*static*/ UniquePtr<SurfaceFactory_ANGLEShareHandle>
SurfaceFactory_ANGLEShareHandle::Create(GLContext* gl,
                                        const SurfaceCaps& caps)
{
    GLLibraryEGL* egl = &sEGLLibrary;
    if (!egl)
        return nullptr;

    auto ext = GLLibraryEGL::ANGLE_surface_d3d_texture_2d_share_handle;
    if (!egl->IsExtensionSupported(ext))
        return nullptr;

    bool success;
    typedef SurfaceFactory_ANGLEShareHandle ptrT;
    UniquePtr<ptrT> ret( new ptrT(gl, egl, caps, &success) );

    if (!success)
        return nullptr;

    return Move(ret);
}

SurfaceFactory_ANGLEShareHandle::SurfaceFactory_ANGLEShareHandle(GLContext* gl,
                                                                 GLLibraryEGL* egl,
                                                                 const SurfaceCaps& caps,
                                                                 bool* const out_success)
    : SurfaceFactory(gl, SharedSurfaceType::EGLSurfaceANGLE, caps)
    , mProdGL(gl)
    , mEGL(egl)
{
    MOZ_ASSERT(out_success);
    *out_success = false;

    mContext = GLContextEGL::Cast(mProdGL)->GetEGLContext();
    mConfig = ChooseConfig(mProdGL, mEGL, mReadCaps);
    if (mConfig == EGL_NO_CONFIG)
        return;

    MOZ_ASSERT(mConfig && mContext);
    *out_success = true;
}

} /* namespace gl */
} /* namespace mozilla */
