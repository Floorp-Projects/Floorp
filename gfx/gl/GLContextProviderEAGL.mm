/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLContextProvider.h"
#include "GLContextEAGL.h"
#include "nsDebug.h"
#include "nsIWidget.h"
#include "gfxPrefs.h"
#include "gfxFailure.h"
#include "prenv.h"
#include "mozilla/Preferences.h"
#include "mozilla/layers/CompositorOptions.h"
#include "mozilla/widget/CompositorWidget.h"
#include "GeckoProfiler.h"

#import <UIKit/UIKit.h>

namespace mozilla {
namespace gl {

using namespace mozilla::widget;

GLContextEAGL::GLContextEAGL(CreateContextFlags flags, const SurfaceCaps& caps,
                             EAGLContext* context, GLContext* sharedContext,
                             bool isOffscreen)
    : GLContext(flags, caps, sharedContext, isOffscreen)
    , mContext(context)
    , mBackbufferRB(0)
    , mBackbufferFB(0)
    , mLayer(nil)
{
}

GLContextEAGL::~GLContextEAGL()
{
    MakeCurrent();

    if (mBackbufferFB) {
        fDeleteFramebuffers(1, &mBackbufferFB);
    }

    if (mBackbufferRB) {
        fDeleteRenderbuffers(1, &mBackbufferRB);
    }

    MarkDestroyed();

    if (mLayer) {
      mLayer = nil;
    }

    if (mContext) {
      [EAGLContext setCurrentContext:nil];
      [mContext release];
    }
}

bool
GLContextEAGL::Init()
{
    if (!InitWithPrefix("gl", true))
        return false;

    return true;
}

bool
GLContextEAGL::AttachToWindow(nsIWidget* aWidget)
{
    // This should only be called once
    MOZ_ASSERT(!mBackbufferFB && !mBackbufferRB);

    UIView* view =
        reinterpret_cast<UIView*>(aWidget->GetNativeData(NS_NATIVE_WIDGET));

    if (!view) {
      MOZ_CRASH("no view!");
    }

    mLayer = [view layer];

    fGenFramebuffers(1, &mBackbufferFB);
    return RecreateRB();
}

bool
GLContextEAGL::RecreateRB()
{
    MakeCurrent();

    CAEAGLLayer* layer = (CAEAGLLayer*)mLayer;

    if (mBackbufferRB) {
        // It doesn't seem to be enough to just call renderbufferStorage: below,
        // we apparently have to recreate the RB.
        fDeleteRenderbuffers(1, &mBackbufferRB);
        mBackbufferRB = 0;
    }

    fGenRenderbuffers(1, &mBackbufferRB);
    fBindRenderbuffer(LOCAL_GL_RENDERBUFFER, mBackbufferRB);

    [mContext renderbufferStorage:LOCAL_GL_RENDERBUFFER
              fromDrawable:layer];

    fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, mBackbufferFB);
    fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_COLOR_ATTACHMENT0,
                             LOCAL_GL_RENDERBUFFER, mBackbufferRB);

    return LOCAL_GL_FRAMEBUFFER_COMPLETE == fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER);
}

bool
GLContextEAGL::MakeCurrentImpl(bool aForce) const
{
    if (!aForce && [EAGLContext currentContext] == mContext) {
        return true;
    }

    if (mContext) {
        if(![EAGLContext setCurrentContext:mContext]) {
            return false;
        }
    }
    return true;
}

bool
GLContextEAGL::IsCurrent() const
{
    return [EAGLContext currentContext] == mContext;
}

bool
GLContextEAGL::SetupLookupFunction()
{
    return false;
}

bool
GLContextEAGL::IsDoubleBuffered() const
{
    return true;
}

bool
GLContextEAGL::SwapBuffers()
{
  AUTO_PROFILER_LABEL("GLContextEAGL::SwapBuffers", GRAPHICS);

  [mContext presentRenderbuffer:LOCAL_GL_RENDERBUFFER];
  return true;
}

void
GLContextEAGL::GetWSIInfo(nsCString* const out) const
{
    out->AppendLiteral("EAGL");
}

already_AddRefed<GLContext>
GLContextProviderEAGL::CreateWrappingExisting(void*, void*)
{
    return nullptr;
}

static GLContextEAGL*
GetGlobalContextEAGL()
{
    return static_cast<GLContextEAGL*>(GLContextProviderEAGL::GetGlobalContext());
}

static already_AddRefed<GLContext>
CreateEAGLContext(CreateContextFlags flags, bool aOffscreen, GLContextEAGL* sharedContext)
{
    EAGLRenderingAPI apis[] = { kEAGLRenderingAPIOpenGLES3, kEAGLRenderingAPIOpenGLES2 };

    // Try to create a GLES3 context if we can, otherwise fall back to GLES2
    EAGLContext* context = nullptr;
    for (EAGLRenderingAPI api : apis) {
        if (sharedContext) {
            context = [[EAGLContext alloc] initWithAPI:api
                       sharegroup:sharedContext->GetEAGLContext().sharegroup];
        } else {
            context = [[EAGLContext alloc] initWithAPI:api];
        }

        if (context) {
            break;
        }
    }

    if (!context) {
        return nullptr;
    }

    RefPtr<GLContextEAGL> glContext = new GLContextEAGL(flags, SurfaceCaps::ForRGBA(),
                                                        context, sharedContext,
                                                        aOffscreen);
    if (!glContext->Init()) {
        glContext = nullptr;
        return nullptr;
    }

    return glContext.forget();
}

already_AddRefed<GLContext>
GLContextProviderEAGL::CreateForCompositorWidget(CompositorWidget* aCompositorWidget, bool aForceAccelerated)
{
    return CreateForWindow(aCompositorWidget->RealWidget(),
                           aCompositorWidget->GetCompositorOptions().UseWebRender(),
                           aForceAccelerated);
}

already_AddRefed<GLContext>
GLContextProviderEAGL::CreateForWindow(nsIWidget* aWidget,
                                       bool aWebRender,
                                       bool aForceAccelerated)
{
    RefPtr<GLContext> glContext = CreateEAGLContext(CreateContextFlags::NONE, false,
                                                    GetGlobalContextEAGL());
    if (!glContext) {
        return nullptr;
    }

    if (!GLContextEAGL::Cast(glContext)->AttachToWindow(aWidget)) {
        return nullptr;
    }

    return glContext.forget();
}

already_AddRefed<GLContext>
GLContextProviderEAGL::CreateHeadless(CreateContextFlags flags,
                                      nsACString* const out_failureId)
{
    return CreateEAGLContext(flags, true, GetGlobalContextEAGL());
}

already_AddRefed<GLContext>
GLContextProviderEAGL::CreateOffscreen(const mozilla::gfx::IntSize& size,
                                       const SurfaceCaps& caps,
                                       CreateContextFlags flags,
                                       nsACString* const out_failureId)
{
    RefPtr<GLContext> glContext = CreateHeadless(flags, out_failureId);
    if (!glContext->InitOffscreen(size, caps)) {
        return nullptr;
    }

    return glContext.forget();
}

static RefPtr<GLContext> gGlobalContext;

GLContext*
GLContextProviderEAGL::GetGlobalContext()
{
    static bool triedToCreateContext = false;
    if (!triedToCreateContext) {
        triedToCreateContext = true;

        MOZ_RELEASE_ASSERT(!gGlobalContext, "GFX: Global GL context already initialized.");
        RefPtr<GLContext> temp = CreateHeadless(CreateContextFlags::NONE);
        gGlobalContext = temp;

        if (!gGlobalContext) {
            MOZ_CRASH("Failed to create global context");
        }
    }

    return gGlobalContext;
}

void
GLContextProviderEAGL::Shutdown()
{
    gGlobalContext = nullptr;
}

} /* namespace gl */
} /* namespace mozilla */
