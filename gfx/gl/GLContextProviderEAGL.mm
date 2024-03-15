/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLContextProvider.h"
#include "GLContextEAGL.h"
#include "GLLibraryLoader.h"
#include "nsDebug.h"
#include "nsIWidget.h"
#include "gfxFailure.h"
#include "prenv.h"
#include "mozilla/Preferences.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/layers/CompositorOptions.h"
#include "mozilla/widget/CompositorWidget.h"

#import <UIKit/UIKit.h>

namespace mozilla {
namespace gl {

using namespace mozilla::widget;

GLContextEAGL::GLContextEAGL(const GLContextDesc& desc, EAGLContext* context,
                             GLContext* sharedContext)
    : GLContext(desc, sharedContext), mContext(context) {}

GLContextEAGL::~GLContextEAGL() {
  MakeCurrent();

  if (mBackbufferFB) {
    fDeleteFramebuffers(1, &mBackbufferFB);
  }

  if (mBackbufferRB) {
    fDeleteRenderbuffers(1, &mBackbufferRB);
  }

  MarkDestroyed();

  if (mContext) {
    [EAGLContext setCurrentContext:nil];
    [mContext release];
  }
}

bool GLContextEAGL::MakeCurrentImpl() const {
  if (mContext) {
    GLContext::ResetTLSCurrentContext();

    if (![EAGLContext setCurrentContext:mContext]) {
      return false;
    }
  }
  return true;
}

bool GLContextEAGL::IsCurrentImpl() const {
  return [EAGLContext currentContext] == mContext;
}

static PRFuncPtr GLAPIENTRY GetLoadedProcAddress(const char* const name) {
  PRLibrary* lib = nullptr;
  const auto& ret = PR_FindFunctionSymbolAndLibrary(name, &lib);
  if (lib) {
    PR_UnloadLibrary(lib);
  }
  return ret;
}

Maybe<SymbolLoader> GLContextEAGL::GetSymbolLoader() const {
  return Some(SymbolLoader(&GetLoadedProcAddress));
}

bool GLContextEAGL::IsDoubleBuffered() const { return true; }

bool GLContextEAGL::SwapBuffers() {
  AUTO_PROFILER_LABEL("GLContextEAGL::SwapBuffers", GRAPHICS);

  [mContext presentRenderbuffer:LOCAL_GL_RENDERBUFFER];
  return true;
}

void GLContextEAGL::GetWSIInfo(nsCString* const out) const {
  out->AppendLiteral("EAGL");
}

static GLContextEAGL* GetGlobalContextEAGL() {
  return static_cast<GLContextEAGL*>(GLContextProviderEAGL::GetGlobalContext());
}

static RefPtr<GLContext> CreateEAGLContext(const GLContextDesc& desc,
                                           GLContextEAGL* sharedContext) {
  EAGLRenderingAPI apis[] = {kEAGLRenderingAPIOpenGLES3,
                             kEAGLRenderingAPIOpenGLES2};

  // Try to create a GLES3 context if we can, otherwise fall back to GLES2
  EAGLContext* context = nullptr;
  for (EAGLRenderingAPI api : apis) {
    if (sharedContext) {
      context = [[EAGLContext alloc]
          initWithAPI:api
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

  RefPtr<GLContextEAGL> glContext =
      new GLContextEAGL(desc, context, sharedContext);
  if (!glContext->Init()) {
    glContext = nullptr;
    return nullptr;
  }

  return glContext;
}

already_AddRefed<GLContext> GLContextProviderEAGL::CreateForCompositorWidget(
    CompositorWidget* aCompositorWidget, bool aHardwareWebRender,
    bool aForceAccelerated) {
  CreateContextFlags flags = CreateContextFlags::ALLOW_OFFLINE_RENDERER;
  if (aForceAccelerated) {
    flags |= CreateContextFlags::FORBID_SOFTWARE;
  }
  nsCString failureUnused;
  return CreateHeadless({flags}, &failureUnused);
}

already_AddRefed<GLContext> GLContextProviderEAGL::CreateHeadless(
    const GLContextCreateDesc& createDesc, nsACString* const out_failureId) {
  auto desc = GLContextDesc{createDesc};
  return CreateEAGLContext(desc, GetGlobalContextEAGL()).forget();
}

static RefPtr<GLContext> gGlobalContext;

GLContext* GLContextProviderEAGL::GetGlobalContext() {
  static bool triedToCreateContext = false;
  if (!triedToCreateContext) {
    triedToCreateContext = true;

    MOZ_RELEASE_ASSERT(!gGlobalContext,
                       "GFX: Global GL context already initialized.");
    nsCString discardFailureId;
    RefPtr<GLContext> temp = CreateHeadless({}, &discardFailureId);
    gGlobalContext = temp;

    if (!gGlobalContext) {
      MOZ_CRASH("Failed to create global context");
    }
  }

  return gGlobalContext;
}

void GLContextProviderEAGL::Shutdown() { gGlobalContext = nullptr; }

} /* namespace gl */
} /* namespace mozilla */
