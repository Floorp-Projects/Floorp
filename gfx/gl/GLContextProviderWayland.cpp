/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_WIDGET_GTK
#  include "mozilla/WidgetUtilsGtk.h"
#endif

#include "GLContextProvider.h"

namespace mozilla::gl {

using namespace mozilla::gfx;
using namespace mozilla::widget;

static class GLContextProviderX11 sGLContextProviderX11;
static class GLContextProviderEGL sGLContextProviderEGL;

// Note that if there is no GTK display, `GdkIsX11Display` and
// `GdkIsWaylandDisplay` will both return false.  That case can
// currently happen only in X11 mode if the pref `dom.ipc.avoid-gtk`
// is set (and applicable to this process).  Thus, these conditionals
// check for the presence of Wayland rather than the absence of X11.
//
// In the future we'll want `dom.ipc.avoid-gtk` to also apply to
// Wayland; at that time we'll need another way to communicate the
// choice of window system.

already_AddRefed<GLContext> GLContextProviderWayland::CreateForCompositorWidget(
    CompositorWidget* aCompositorWidget, bool aHardwareWebRender,
    bool aForceAccelerated) {
  if (GdkIsWaylandDisplay()) {
    return sGLContextProviderEGL.CreateForCompositorWidget(
        aCompositorWidget, aHardwareWebRender, aForceAccelerated);
  } else {
    return sGLContextProviderX11.CreateForCompositorWidget(
        aCompositorWidget, aHardwareWebRender, aForceAccelerated);
  }
}

/*static*/
already_AddRefed<GLContext> GLContextProviderWayland::CreateHeadless(
    const GLContextCreateDesc& desc, nsACString* const out_failureId) {
  if (GdkIsWaylandDisplay()) {
    return sGLContextProviderEGL.CreateHeadless(desc, out_failureId);
  } else {
    return sGLContextProviderX11.CreateHeadless(desc, out_failureId);
  }
}

/*static*/
GLContext* GLContextProviderWayland::GetGlobalContext() {
  if (GdkIsWaylandDisplay()) {
    return sGLContextProviderEGL.GetGlobalContext();
  } else {
    return sGLContextProviderX11.GetGlobalContext();
  }
}

/*static*/
void GLContextProviderWayland::Shutdown() {
  if (GdkIsWaylandDisplay()) {
    sGLContextProviderEGL.Shutdown();
  } else {
    sGLContextProviderX11.Shutdown();
  }
}

}  // namespace mozilla::gl
