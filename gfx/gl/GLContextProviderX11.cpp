/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "prenv.h"

#include "GLContextProvider.h"
#include "mozilla/gfx/gfxVars.h"

namespace mozilla::gl {

using namespace mozilla::gfx;
using namespace mozilla::widget;

static class GLContextProviderGLX sGLContextProviderGLX;
static class GLContextProviderEGL sGLContextProviderEGL;

already_AddRefed<GLContext> GLContextProviderX11::CreateForCompositorWidget(
    CompositorWidget* aCompositorWidget, bool aHardwareWebRender,
    bool aForceAccelerated) {
  if (!gfxVars::UseEGL()) {
    return sGLContextProviderGLX.CreateForCompositorWidget(
        aCompositorWidget, aHardwareWebRender, aForceAccelerated);
  } else {
    return sGLContextProviderEGL.CreateForCompositorWidget(
        aCompositorWidget, aHardwareWebRender, aForceAccelerated);
  }
}

/*static*/
already_AddRefed<GLContext> GLContextProviderX11::CreateHeadless(
    const GLContextCreateDesc& desc, nsACString* const out_failureId) {
  if (!gfxVars::UseEGL()) {
    return sGLContextProviderGLX.CreateHeadless(desc, out_failureId);
  } else {
    return sGLContextProviderEGL.CreateHeadless(desc, out_failureId);
  }
}

/*static*/
GLContext* GLContextProviderX11::GetGlobalContext() {
  if (!gfxVars::UseEGL()) {
    return sGLContextProviderGLX.GetGlobalContext();
  } else {
    return sGLContextProviderEGL.GetGlobalContext();
  }
}

/*static*/
void GLContextProviderX11::Shutdown() {
  if (!gfxVars::UseEGL()) {
    sGLContextProviderGLX.Shutdown();
  } else {
    sGLContextProviderEGL.Shutdown();
  }
}

}  // namespace mozilla::gl
