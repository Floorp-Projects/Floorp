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

static bool gUseDoubleBufferedWindows = true;

class CGLLibrary
{
public:
    CGLLibrary()
      : mInitialized(false),
        mOGLLibrary(nullptr),
        mPixelFormat(nullptr)
    { }

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
        gUseDoubleBufferedWindows = (!db || *db != '0');

        mInitialized = true;
        return true;
    }

    NSOpenGLPixelFormat *PixelFormat()
    {
        if (mPixelFormat == nullptr) {
            NSOpenGLPixelFormatAttribute attribs[] = {
                NSOpenGLPFAAccelerated,
                NSOpenGLPFAAllowOfflineRenderers,
                NSOpenGLPFADoubleBuffer,
                0
            };

            if (!gUseDoubleBufferedWindows) {
              attribs[2] = 0;
            }

            mPixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attribs];
        }

        return mPixelFormat;
    }
private:
    bool mInitialized;
    PRLibrary *mOGLLibrary;
    NSOpenGLPixelFormat *mPixelFormat;
};

CGLLibrary sCGLLibrary;

GLContextCGL::GLContextCGL(
                  const SurfaceCaps& caps,
                  GLContext *shareContext,
                  NSOpenGLContext *context,
                  bool isOffscreen)
    : GLContext(caps, shareContext, isOffscreen),
      mContext(context)
{
    SetProfileVersion(ContextProfile::OpenGLCompatibility, 210);
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
  return gUseDoubleBufferedWindows;
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


static GLContextCGL *
GetGlobalContextCGL()
{
    return static_cast<GLContextCGL*>(GLContextProviderCGL::GetGlobalContext());
}

already_AddRefed<GLContext>
GLContextProviderCGL::CreateWrappingExisting(void*, void*)
{
    return nullptr;
}

already_AddRefed<GLContext>
GLContextProviderCGL::CreateForWindow(nsIWidget *aWidget)
{
    GLContextCGL *shareContext = GetGlobalContextCGL();

    NSOpenGLContext *context = [[NSOpenGLContext alloc]
                                initWithFormat:sCGLLibrary.PixelFormat()
                                shareContext:(shareContext ? shareContext->mContext : NULL)];
    if (!context) {
        return nullptr;
    }

    // make the context transparent
    GLint opaque = 0;
    [context setValues:&opaque forParameter:NSOpenGLCPSurfaceOpacity];

    SurfaceCaps caps = SurfaceCaps::ForRGBA();
    nsRefPtr<GLContextCGL> glContext = new GLContextCGL(caps,
                                                        shareContext,
                                                        context);
    if (!glContext->Init()) {
        return nullptr;
    }

    return glContext.forget();
}

static already_AddRefed<GLContextCGL>
CreateOffscreenFBOContext(bool aShare = true)
{
    if (!sCGLLibrary.EnsureInitialized()) {
        return nullptr;
    }

    GLContextCGL *shareContext = aShare ? GetGlobalContextCGL() : nullptr;
    if (aShare && !shareContext) {
        // if there is no share context, then we can't use FBOs.
        return nullptr;
    }

    NSOpenGLContext *context = [[NSOpenGLContext alloc]
                                initWithFormat:sCGLLibrary.PixelFormat()
                                shareContext:shareContext ? shareContext->GetNSOpenGLContext() : NULL];
    if (!context) {
        return nullptr;
    }

    SurfaceCaps dummyCaps = SurfaceCaps::Any();
    nsRefPtr<GLContextCGL> glContext = new GLContextCGL(dummyCaps, shareContext, context, true);

    return glContext.forget();
}

already_AddRefed<GLContext>
GLContextProviderCGL::CreateOffscreen(const gfxIntSize& size,
                                      const SurfaceCaps& caps)
{
    nsRefPtr<GLContextCGL> glContext = CreateOffscreenFBOContext();
    if (glContext &&
        glContext->Init() &&
        glContext->InitOffscreen(ToIntSize(size), caps))
    {
        return glContext.forget();
    }

    // everything failed
    return nullptr;
}

static nsRefPtr<GLContext> gGlobalContext;

GLContext *
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
