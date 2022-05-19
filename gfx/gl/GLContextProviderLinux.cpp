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

already_AddRefed<GLContext> GLContextProviderLinux::CreateForCompositorWidget(
    CompositorWidget* aCompositorWidget, bool aHardwareWebRender,
    bool aForceAccelerated) {
  if (gfxVars::UseEGL()) {
    return sGLContextProviderEGL.CreateForCompositorWidget(
        aCompositorWidget, aHardwareWebRender, aForceAccelerated);
  } else {
    return sGLContextProviderGLX.CreateForCompositorWidget(
        aCompositorWidget, aHardwareWebRender, aForceAccelerated);
  }
}

/*static*/
already_AddRefed<GLContext> GLContextProviderLinux::CreateHeadless(
    const GLContextCreateDesc& desc, nsACString* const out_failureId) {
  if (gfxVars::UseEGL()) {
    return sGLContextProviderEGL.CreateHeadless(desc, out_failureId);
  } else {
    return sGLContextProviderGLX.CreateHeadless(desc, out_failureId);
  }
}

/*static*/
GLContext* GLContextProviderLinux::GetGlobalContext() {
  if (gfxVars::UseEGL()) {
    return sGLContextProviderEGL.GetGlobalContext();
  } else {
    return sGLContextProviderGLX.GetGlobalContext();
  }
}

/*static*/
void GLContextProviderLinux::Shutdown() {
  if (gfxVars::UseEGL()) {
    sGLContextProviderEGL.Shutdown();
  } else {
    sGLContextProviderGLX.Shutdown();
  }
}

}  // namespace mozilla::gl
