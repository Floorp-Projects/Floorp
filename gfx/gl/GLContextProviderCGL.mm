/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLContextProvider.h"
#include "GLContextCGL.h"
#include "TextureImageCGL.h"
#include "nsDebug.h"
#include "nsIWidget.h"
#include <OpenGL/gl.h>
#include "gfxPrefs.h"
#include "gfxFailure.h"
#include "prenv.h"
#include "mozilla/Preferences.h"
#include "GeckoProfiler.h"
#include "mozilla/gfx/MacIOSurface.h"

namespace mozilla {
namespace gl {

using namespace mozilla::gfx;

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
    PRLibrary *mOGLLibrary;
};

CGLLibrary sCGLLibrary;

GLContextCGL::GLContextCGL(const SurfaceCaps& caps, NSOpenGLContext* context,
                           bool isOffscreen, ContextProfile profile)
    : GLContext(caps, nullptr, isOffscreen)
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
GLContextCGL::SupportsRobustness() const
{
    return false;
}

bool
GLContextCGL::SwapBuffers()
{
  PROFILER_LABEL("GLContextCGL", "SwapBuffers",
    js::ProfileEntry::Category::GRAPHICS);

  [mContext flushBuffer];
  return true;
}


already_AddRefed<GLContext>
GLContextProviderCGL::CreateWrappingExisting(void*, void*)
{
    return nullptr;
}

static const NSOpenGLPixelFormatAttribute kAttribs_singleBuffered[] = {
    NSOpenGLPFAAccelerated,
    NSOpenGLPFAAllowOfflineRenderers,
    0
};

static const NSOpenGLPixelFormatAttribute kAttribs_doubleBuffered[] = {
    NSOpenGLPFAAccelerated,
    NSOpenGLPFAAllowOfflineRenderers,
    NSOpenGLPFADoubleBuffer,
    0
};

static const NSOpenGLPixelFormatAttribute kAttribs_offscreen[] = {
    NSOpenGLPFAPixelBuffer,
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
    if (!format)
        return nullptr;

    NSOpenGLContext* context = [[NSOpenGLContext alloc] initWithFormat:format
                                shareContext:nullptr];

    [format release];

    return context;
}

already_AddRefed<GLContext>
GLContextProviderCGL::CreateForWindow(nsIWidget *aWidget)
{
    if (!sCGLLibrary.EnsureInitialized()) {
        return nullptr;
    }

    const NSOpenGLPixelFormatAttribute* attribs;
    if (sCGLLibrary.UseDoubleBufferedWindows()) {
        attribs = kAttribs_doubleBuffered;
    } else {
        attribs = kAttribs_singleBuffered;
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
    nsRefPtr<GLContextCGL> glContext = new GLContextCGL(caps, context, false,
                                                        profile);

    if (!glContext->Init()) {
        glContext = nullptr;
        [context release];
        return nullptr;
    }

    return glContext.forget();
}

static already_AddRefed<GLContextCGL>
CreateOffscreenFBOContext(bool requireCompatProfile)
{
    if (!sCGLLibrary.EnsureInitialized()) {
        return nullptr;
    }

    ContextProfile profile;
    NSOpenGLContext* context = nullptr;

    if (!requireCompatProfile) {
        profile = ContextProfile::OpenGLCore;
        context = CreateWithFormat(kAttribs_offscreen_coreProfile);
    }
    if (!context) {
        profile = ContextProfile::OpenGLCompatibility;
        context = CreateWithFormat(kAttribs_offscreen);
    }
    if (!context) {
        return nullptr;
    }

    SurfaceCaps dummyCaps = SurfaceCaps::Any();
    nsRefPtr<GLContextCGL> glContext = new GLContextCGL(dummyCaps, context,
                                                        true, profile);

    return glContext.forget();
}

already_AddRefed<GLContext>
GLContextProviderCGL::CreateHeadless(bool requireCompatProfile)
{
    nsRefPtr<GLContextCGL> gl;
    gl = CreateOffscreenFBOContext(requireCompatProfile);
    if (!gl)
        return nullptr;

    if (!gl->Init())
        return nullptr;

    return gl.forget();
}

already_AddRefed<GLContext>
GLContextProviderCGL::CreateOffscreen(const gfxIntSize& size,
                                      const SurfaceCaps& caps,
                                      bool requireCompatProfile)
{
    nsRefPtr<GLContext> glContext = CreateHeadless(requireCompatProfile);
    if (!glContext->InitOffscreen(ToIntSize(size), caps))
        return nullptr;

    return glContext.forget();
}

static nsRefPtr<GLContext> gGlobalContext;

GLContext*
GLContextProviderCGL::GetGlobalContext()
{
    if (!sCGLLibrary.EnsureInitialized()) {
        return nullptr;
    }

    if (!gGlobalContext) {
        // There are bugs in some older drivers with pbuffers less
        // than 16x16 in size; also 16x16 is POT so that we can do
        // a FBO with it on older video cards.  A FBO context for
        // sharing is preferred since it has no associated target.
        gGlobalContext = CreateOffscreenFBOContext(false);
        if (!gGlobalContext || !static_cast<GLContextCGL*>(gGlobalContext.get())->Init()) {
            NS_WARNING("Couldn't init gGlobalContext.");
            gGlobalContext = nullptr;
            return nullptr;
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
