/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLContextProvider.h"
#include "GLContextCGL.h"
#include "nsDebug.h"
#include "nsIWidget.h"
#include <OpenGL/gl.h>
#include "gfxFailure.h"
#include "gfxPrefs.h"
#include "prenv.h"
#include "GeckoProfiler.h"
#include "mozilla/gfx/MacIOSurface.h"
#include "mozilla/layers/CompositorOptions.h"
#include "mozilla/widget/CompositorWidget.h"

#include <OpenGL/OpenGL.h>

// When running inside a VM, creating an accelerated OpenGL context usually
// fails. Uncomment this line to emulate that behavior.
// #define EMULATE_VM

namespace mozilla {
namespace gl {

using namespace mozilla::gfx;
using namespace mozilla::widget;

class CGLLibrary
{
public:
    CGLLibrary()
        : mInitialized(false)
        , mUseDoubleBufferedWindows(true)
        , mOGLLibrary(nullptr)
    {}

    bool EnsureInitialized()
    {
        if (mInitialized) {
            return true;
        }
        if (!mOGLLibrary) {
            mOGLLibrary = PR_LoadLibrary("/System/Library/Frameworks/OpenGL.framework/OpenGL");
            if (!mOGLLibrary) {
                NS_WARNING("Couldn't load OpenGL Framework.");
                return false;
            }
        }

        const char* db = PR_GetEnv("MOZ_CGL_DB");
        if (db) {
            mUseDoubleBufferedWindows = *db != '0';
        }

        mInitialized = true;
        return true;
    }

    bool UseDoubleBufferedWindows() const {
        MOZ_ASSERT(mInitialized);
        return mUseDoubleBufferedWindows;
    }

private:
    bool mInitialized;
    bool mUseDoubleBufferedWindows;
    PRLibrary* mOGLLibrary;
};

CGLLibrary sCGLLibrary;

GLContextCGL::GLContextCGL(CreateContextFlags flags, const SurfaceCaps& caps,
                           NSOpenGLContext* context, bool isOffscreen,
                           ContextProfile profile)
    : GLContext(flags, caps, nullptr, isOffscreen)
    , mContext(context)
{
    SetProfileVersion(profile, 210);
}

GLContextCGL::~GLContextCGL()
{
    MarkDestroyed();

    if (mContext) {
        if ([NSOpenGLContext currentContext] == mContext) {
            // Clear the current context before releasing. If we don't do
            // this, the next time we call [NSOpenGLContext currentContext],
            // "invalid context" will be printed to the console.
            [NSOpenGLContext clearCurrentContext];
        }
        [mContext release];
    }

}

bool
GLContextCGL::Init()
{
    if (!InitWithPrefix("gl", true))
        return false;

    return true;
}

CGLContextObj
GLContextCGL::GetCGLContext() const
{
    return static_cast<CGLContextObj>([mContext CGLContextObj]);
}

bool
GLContextCGL::MakeCurrentImpl(bool aForce)
{
    if (!aForce && [NSOpenGLContext currentContext] == mContext) {
        return true;
    }

    if (mContext) {
        [mContext makeCurrentContext];
        MOZ_ASSERT(IsCurrent());
        // Use non-blocking swap in "ASAP mode".
        // ASAP mode means that rendering is iterated as fast as possible.
        // ASAP mode is entered when layout.frame_rate=0 (requires restart).
        // If swapInt is 1, then glSwapBuffers will block and wait for a vblank signal.
        // When we're iterating as fast as possible, however, we want a non-blocking
        // glSwapBuffers, which will happen when swapInt==0.
        GLint swapInt = gfxPrefs::LayoutFrameRate() == 0 ? 0 : 1;
        [mContext setValues:&swapInt forParameter:NSOpenGLCPSwapInterval];
    }
    return true;
}

bool
GLContextCGL::IsCurrent() {
    return [NSOpenGLContext currentContext] == mContext;
}

GLenum
GLContextCGL::GetPreferredARGB32Format() const
{
    return LOCAL_GL_BGRA;
}

bool
GLContextCGL::SetupLookupFunction()
{
    return false;
}

bool
GLContextCGL::IsDoubleBuffered() const
{
  return sCGLLibrary.UseDoubleBufferedWindows();
}

bool
GLContextCGL::SwapBuffers()
{
  PROFILER_LABEL("GLContextCGL", "SwapBuffers",
    js::ProfileEntry::Category::GRAPHICS);

  [mContext flushBuffer];
  return true;
}

void
GLContextCGL::GetWSIInfo(nsCString* const out) const
{
    out->AppendLiteral("CGL");
}

already_AddRefed<GLContext>
GLContextProviderCGL::CreateWrappingExisting(void*, void*)
{
    return nullptr;
}

static const NSOpenGLPixelFormatAttribute kAttribs_singleBuffered[] = {
    NSOpenGLPFAAllowOfflineRenderers,
    0
};

static const NSOpenGLPixelFormatAttribute kAttribs_singleBuffered_accel[] = {
    NSOpenGLPFAAccelerated,
    NSOpenGLPFAAllowOfflineRenderers,
    0
};

static const NSOpenGLPixelFormatAttribute kAttribs_doubleBuffered[] = {
    NSOpenGLPFAAllowOfflineRenderers,
    NSOpenGLPFADoubleBuffer,
    0
};

static const NSOpenGLPixelFormatAttribute kAttribs_doubleBuffered_accel[] = {
    NSOpenGLPFAAccelerated,
    NSOpenGLPFAAllowOfflineRenderers,
    NSOpenGLPFADoubleBuffer,
    0
};

static const NSOpenGLPixelFormatAttribute kAttribs_doubleBuffered_accel_webrender[] = {
    NSOpenGLPFAAccelerated,
    NSOpenGLPFAAllowOfflineRenderers,
    NSOpenGLPFADoubleBuffer,
    NSOpenGLPFAOpenGLProfile,
    NSOpenGLProfileVersion3_2Core,
    NSOpenGLPFADepthSize, 24,
    0
};

static const NSOpenGLPixelFormatAttribute kAttribs_offscreen[] = {
    0
};

static const NSOpenGLPixelFormatAttribute kAttribs_offscreen_allow_offline[] = {
    NSOpenGLPFAAllowOfflineRenderers,
    0
};

static const NSOpenGLPixelFormatAttribute kAttribs_offscreen_accel[] = {
    NSOpenGLPFAAccelerated,
    0
};

static const NSOpenGLPixelFormatAttribute kAttribs_offscreen_coreProfile[] = {
    NSOpenGLPFAAccelerated,
    NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion3_2Core,
    0
};

static NSOpenGLContext*
CreateWithFormat(const NSOpenGLPixelFormatAttribute* attribs)
{
    NSOpenGLPixelFormat* format = [[NSOpenGLPixelFormat alloc]
                                   initWithAttributes:attribs];
    if (!format) {
        NS_WARNING("Failed to create NSOpenGLPixelFormat.");
        return nullptr;
    }

    NSOpenGLContext* context = [[NSOpenGLContext alloc] initWithFormat:format
                                shareContext:nullptr];

    [format release];

    return context;
}

already_AddRefed<GLContext>
GLContextProviderCGL::CreateForCompositorWidget(CompositorWidget* aCompositorWidget, bool aForceAccelerated)
{
    return CreateForWindow(aCompositorWidget->RealWidget(),
                           aCompositorWidget->GetCompositorOptions().UseWebRender(),
                           aForceAccelerated);
}

already_AddRefed<GLContext>
GLContextProviderCGL::CreateForWindow(nsIWidget* aWidget,
                                      bool aWebRender,
                                      bool aForceAccelerated)
{
    if (!sCGLLibrary.EnsureInitialized()) {
        return nullptr;
    }

#ifdef EMULATE_VM
    if (aForceAccelerated) {
        return nullptr;
    }
#endif

    const NSOpenGLPixelFormatAttribute* attribs;
    if (sCGLLibrary.UseDoubleBufferedWindows()) {
        if (aWebRender) {
            attribs = aForceAccelerated ? kAttribs_doubleBuffered_accel_webrender : kAttribs_doubleBuffered;
        } else {
            attribs = aForceAccelerated ? kAttribs_doubleBuffered_accel : kAttribs_doubleBuffered;
        }
    } else {
        attribs = aForceAccelerated ? kAttribs_singleBuffered_accel : kAttribs_singleBuffered;
    }
    NSOpenGLContext* context = CreateWithFormat(attribs);
    if (!context) {
        return nullptr;
    }

    // make the context transparent
    GLint opaque = 0;
    [context setValues:&opaque forParameter:NSOpenGLCPSurfaceOpacity];

    SurfaceCaps caps = SurfaceCaps::ForRGBA();
    ContextProfile profile = ContextProfile::OpenGLCompatibility;
    RefPtr<GLContextCGL> glContext = new GLContextCGL(CreateContextFlags::NONE, caps,
                                                      context, false, profile);

    if (!glContext->Init()) {
        glContext = nullptr;
        [context release];
        return nullptr;
    }

    return glContext.forget();
}

static already_AddRefed<GLContextCGL>
CreateOffscreenFBOContext(CreateContextFlags flags)
{
    if (!sCGLLibrary.EnsureInitialized()) {
        return nullptr;
    }

    ContextProfile profile;
    NSOpenGLContext* context = nullptr;

    if (!(flags & CreateContextFlags::REQUIRE_COMPAT_PROFILE)) {
        profile = ContextProfile::OpenGLCore;
        context = CreateWithFormat(kAttribs_offscreen_coreProfile);
    }
    if (!context) {
        profile = ContextProfile::OpenGLCompatibility;

        if (flags & CreateContextFlags::ALLOW_OFFLINE_RENDERER) {
          if (gfxPrefs::RequireHardwareGL())
              context = CreateWithFormat(kAttribs_singleBuffered);
          else
              context = CreateWithFormat(kAttribs_offscreen_allow_offline);

        } else {
          if (gfxPrefs::RequireHardwareGL())
              context = CreateWithFormat(kAttribs_offscreen_accel);
          else
              context = CreateWithFormat(kAttribs_offscreen);
        }
    }
    if (!context) {
        NS_WARNING("Failed to create NSOpenGLContext.");
        return nullptr;
    }

    SurfaceCaps dummyCaps = SurfaceCaps::Any();
    RefPtr<GLContextCGL> glContext = new GLContextCGL(flags, dummyCaps, context, true,
                                                      profile);

    if (gfxPrefs::GLMultithreaded()) {
        CGLEnable(glContext->GetCGLContext(), kCGLCEMPEngine);
    }
    return glContext.forget();
}

already_AddRefed<GLContext>
GLContextProviderCGL::CreateHeadless(CreateContextFlags flags,
                                     nsACString* const out_failureId)
{
    RefPtr<GLContextCGL> gl;
    gl = CreateOffscreenFBOContext(flags);
    if (!gl) {
        *out_failureId = NS_LITERAL_CSTRING("FEATURE_FAILURE_CGL_FBO");
        return nullptr;
    }

    if (!gl->Init()) {
        *out_failureId = NS_LITERAL_CSTRING("FEATURE_FAILURE_CGL_INIT");
        NS_WARNING("Failed during Init.");
        return nullptr;
    }

    return gl.forget();
}

already_AddRefed<GLContext>
GLContextProviderCGL::CreateOffscreen(const IntSize& size,
                                      const SurfaceCaps& minCaps,
                                      CreateContextFlags flags,
                                      nsACString* const out_failureId)
{
    RefPtr<GLContext> gl = CreateHeadless(flags, out_failureId);
    if (!gl) {
        return nullptr;
    }

    if (!gl->InitOffscreen(size, minCaps)) {
        *out_failureId = NS_LITERAL_CSTRING("FEATURE_FAILURE_CGL_INIT");
        return nullptr;
    }

    return gl.forget();
}

static RefPtr<GLContext> gGlobalContext;

GLContext*
GLContextProviderCGL::GetGlobalContext()
{
    static bool triedToCreateContext = false;
    if (!triedToCreateContext) {
        triedToCreateContext = true;

        MOZ_RELEASE_ASSERT(!gGlobalContext);
        nsCString discardFailureId;
        RefPtr<GLContext> temp = CreateHeadless(CreateContextFlags::NONE,
                                                &discardFailureId);
        gGlobalContext = temp;

        if (!gGlobalContext) {
            NS_WARNING("Couldn't init gGlobalContext.");
        }
    }

    return gGlobalContext;
}

void
GLContextProviderCGL::Shutdown()
{
    gGlobalContext = nullptr;
}

} /* namespace gl */
} /* namespace mozilla */
