/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_WIDGET_GTK
#  include <gdk/gdk.h>
#  include <gdk/gdkx.h>
#endif

#include "GLContextProvider.h"

namespace mozilla::gl {

using namespace mozilla::gfx;
using namespace mozilla::widget;

static class GLContextProviderX11 sGLContextProviderX11;
static class GLContextProviderEGL sGLContextProviderEGL;

already_AddRefed<GLContext> GLContextProviderWayland::CreateForCompositorWidget(
    CompositorWidget* aCompositorWidget, bool aWebRender,
    bool aForceAccelerated) {
  if (GDK_IS_X11_DISPLAY(gdk_display_get_default())) {
    return sGLContextProviderX11.CreateForCompositorWidget(
        aCompositorWidget, aWebRender, aForceAccelerated);
  } else {
    return sGLContextProviderEGL.CreateForCompositorWidget(
        aCompositorWidget, aWebRender, aForceAccelerated);
  }
}

/*static*/
already_AddRefed<GLContext> GLContextProviderWayland::CreateHeadless(
    const GLContextCreateDesc& desc, nsACString* const out_failureId) {
  if (GDK_IS_X11_DISPLAY(gdk_display_get_default())) {
    return sGLContextProviderX11.CreateHeadless(desc, out_failureId);
  } else {
    return sGLContextProviderEGL.CreateHeadless(desc, out_failureId);
  }
}

/*static*/
GLContext* GLContextProviderWayland::GetGlobalContext() {
  if (GDK_IS_X11_DISPLAY(gdk_display_get_default())) {
    return sGLContextProviderX11.GetGlobalContext();
  } else {
    return sGLContextProviderEGL.GetGlobalContext();
  }
}

/*static*/
void GLContextProviderWayland::Shutdown() {
  if (GDK_IS_X11_DISPLAY(gdk_display_get_default())) {
    sGLContextProviderX11.Shutdown();
  } else {
    sGLContextProviderEGL.Shutdown();
  }
}

}  // namespace mozilla::gl
