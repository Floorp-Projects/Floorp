/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLContextProvider.h"
#include "GLContext.h"
#include "TextureImageCGL.h"
#include "nsDebug.h"
#include "nsIWidget.h"
#include "OpenGL/OpenGL.h"
#include <OpenGL/gl.h>
#include <AppKit/NSOpenGL.h>
#include "gfxASurface.h"
#include "gfxImageSurface.h"
#include "gfxQuartzSurface.h"
#include "gfxPlatform.h"
#include "gfxFailure.h"
#include "prenv.h"
#include "mozilla/Preferences.h"
#include "GeckoProfiler.h"
#include "mozilla/gfx/MacIOSurface.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace gl {

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

class GLContextCGL : public GLContext
{
    friend class GLContextProviderCGL;

public:
    GLContextCGL(const SurfaceCaps& caps,
                 GLContext *shareContext,
                 NSOpenGLContext *context,
                 bool isOffscreen = false)
        : GLContext(caps, shareContext, isOffscreen),
          mContext(context),
          mTempTextureName(0)
    {
        SetProfileVersion(ContextProfile::OpenGLCompatibility, 210);
    }

    ~GLContextCGL()
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

    GLContextType GetContextType() {
        return ContextTypeCGL;
    }

    bool Init()
    {
        if (!InitWithPrefix("gl", true))
            return false;

        return true;
    }

    void *GetNativeData(NativeDataType aType)
    { 
        switch (aType) {
        case NativeGLContext:
            return mContext;
        case NativeCGLContext:
            return [mContext CGLContextObj];
        default:
            return nullptr;
        }
    }

    bool MakeCurrentImpl(bool aForce = false)
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
            GLint swapInt = gfxPlatform::GetPrefLayoutFrameRate() == 0 ? 0 : 1;
            [mContext setValues:&swapInt forParameter:NSOpenGLCPSwapInterval];
        }
        return true;
    }

    virtual bool IsCurrent() {
        return [NSOpenGLContext currentContext] == mContext;
    }

    virtual GLenum GetPreferredARGB32Format() MOZ_OVERRIDE { return LOCAL_GL_BGRA; }

    bool SetupLookupFunction()
    {
        return false;
    }

    bool IsDoubleBuffered() 
    { 
      return gUseDoubleBufferedWindows; 
    }

    bool SupportsRobustness()
    {
        return false;
    }

    bool SwapBuffers()
    {
      PROFILER_LABEL("GLContext", "SwapBuffers");
      [mContext flushBuffer];
      return true;
    }

    bool ResizeOffscreen(const gfxIntSize& aNewSize);

    NSOpenGLContext *mContext;
    GLuint mTempTextureName;
};

bool
GLContextCGL::ResizeOffscreen(const gfxIntSize& aNewSize)
{
    return ResizeScreenBuffer(aNewSize);
}

static GLContextCGL *
GetGlobalContextCGL()
{
    return static_cast<GLContextCGL*>(GLContextProviderCGL::GetGlobalContext());
}

already_AddRefed<GLContext>
GLContextProviderCGL::CreateForWindow(nsIWidget *aWidget)
{
    if (!sCGLLibrary.EnsureInitialized()) {
        return nullptr;
    }

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
                                shareContext:shareContext ? shareContext->mContext : NULL];
    if (!context) {
        return nullptr;
    }

    SurfaceCaps dummyCaps = SurfaceCaps::Any();
    nsRefPtr<GLContextCGL> glContext = new GLContextCGL(dummyCaps, shareContext, context, true);

    return glContext.forget();
}

already_AddRefed<GLContext>
GLContextProviderCGL::CreateOffscreen(const gfxIntSize& size,
                                      const SurfaceCaps& caps,
                                      const ContextFlags flags)
{
    nsRefPtr<GLContextCGL> glContext = CreateOffscreenFBOContext();
    if (glContext &&
        glContext->Init() &&
        glContext->InitOffscreen(size, caps))
    {
        return glContext.forget();
    }

    // everything failed
    return nullptr;
}

static nsRefPtr<GLContext> gGlobalContext;

GLContext *
GLContextProviderCGL::GetGlobalContext(const ContextFlags)
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

        gGlobalContext->SetIsGlobalSharedContext(true);
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
