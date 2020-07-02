/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "prenv.h"

#include "GLContextProvider.h"

namespace mozilla::gl {

using namespace mozilla::gfx;
using namespace mozilla::widget;

static class GLContextProviderGLX sGLContextProviderGLX;
static class GLContextProviderEGL sGLContextProviderEGL;

static bool UseGLX() {
  static bool useGLX = !PR_GetEnv("MOZ_X11_EGL");
  return useGLX;
}

already_AddRefed<GLContext> GLContextProviderX11::CreateWrappingExisting(
    void* aContext, void* aSurface) {
  if (UseGLX()) {
    return sGLContextProviderGLX.CreateWrappingExisting(aContext, aSurface);
  } else {
    return sGLContextProviderEGL.CreateWrappingExisting(aContext, aSurface);
  }
}

already_AddRefed<GLContext> GLContextProviderX11::CreateForCompositorWidget(
    CompositorWidget* aCompositorWidget, bool aWebRender,
    bool aForceAccelerated) {
  if (UseGLX()) {
    return sGLContextProviderGLX.CreateForCompositorWidget(
        aCompositorWidget, aWebRender, aForceAccelerated);
  } else {
    return sGLContextProviderEGL.CreateForCompositorWidget(
        aCompositorWidget, aWebRender, aForceAccelerated);
  }
}

/*static*/
already_AddRefed<GLContext> GLContextProviderX11::CreateHeadless(
    const GLContextCreateDesc& desc, nsACString* const out_failureId) {
  if (UseGLX()) {
    return sGLContextProviderGLX.CreateHeadless(desc, out_failureId);
  } else {
    return sGLContextProviderEGL.CreateHeadless(desc, out_failureId);
  }
}

/*static*/
GLContext* GLContextProviderX11::GetGlobalContext() {
  if (UseGLX()) {
    return sGLContextProviderGLX.GetGlobalContext();
  } else {
    return sGLContextProviderEGL.GetGlobalContext();
  }
}

/*static*/
void GLContextProviderX11::Shutdown() {
  if (UseGLX()) {
    sGLContextProviderGLX.Shutdown();
  } else {
    sGLContextProviderEGL.Shutdown();
  }
}

}  // namespace mozilla::gl
