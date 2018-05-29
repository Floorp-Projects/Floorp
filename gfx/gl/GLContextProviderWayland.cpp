/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_WIDGET_GTK
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#endif

#include "GLContextProvider.h"

namespace mozilla {
namespace gl {

using namespace mozilla::gfx;
using namespace mozilla::widget;

static class GLContextProviderGLX sGLContextProviderGLX;
static class GLContextProviderEGL sGLContextProviderEGL;

already_AddRefed<GLContext>
GLContextProviderWayland::CreateWrappingExisting(void* aContext, void* aSurface)
{
    if (GDK_IS_X11_DISPLAY(gdk_display_get_default())) {
        return sGLContextProviderGLX.CreateWrappingExisting(aContext, aSurface);
    } else {
        return sGLContextProviderEGL.CreateWrappingExisting(aContext, aSurface);
    }
}

already_AddRefed<GLContext>
GLContextProviderWayland::CreateForCompositorWidget(CompositorWidget* aCompositorWidget, bool aForceAccelerated)
{
    if (GDK_IS_X11_DISPLAY(gdk_display_get_default())) {
        return sGLContextProviderGLX.CreateForCompositorWidget(aCompositorWidget, aForceAccelerated);
    } else {
        return sGLContextProviderEGL.CreateForCompositorWidget(aCompositorWidget, aForceAccelerated);
    }
}

already_AddRefed<GLContext>
GLContextProviderWayland::CreateForWindow(nsIWidget* aWidget,
                                      bool aWebRender,
                                      bool aForceAccelerated)
{
    if (GDK_IS_X11_DISPLAY(gdk_display_get_default())) {
        return sGLContextProviderGLX.CreateForWindow(aWidget, aWebRender, aForceAccelerated);
    } else {
        return sGLContextProviderEGL.CreateForWindow(aWidget, aWebRender, aForceAccelerated);
    }
}

/*static*/ already_AddRefed<GLContext>
GLContextProviderWayland::CreateHeadless(CreateContextFlags flags,
                                     nsACString* const out_failureId)
{
    if (GDK_IS_X11_DISPLAY(gdk_display_get_default())) {
        return sGLContextProviderGLX.CreateHeadless(flags, out_failureId);
    } else {
        return sGLContextProviderEGL.CreateHeadless(flags, out_failureId);
    }
}

/*static*/ already_AddRefed<GLContext>
GLContextProviderWayland::CreateOffscreen(const IntSize& size,
                                      const SurfaceCaps& minCaps,
                                      CreateContextFlags flags,
                                      nsACString* const out_failureId)
{
    if (GDK_IS_X11_DISPLAY(gdk_display_get_default())) {
        return sGLContextProviderGLX.CreateOffscreen(size, minCaps, flags, out_failureId);
    } else {
        return sGLContextProviderEGL.CreateOffscreen(size, minCaps, flags, out_failureId);
    }
}

/*static*/ GLContext*
GLContextProviderWayland::GetGlobalContext()
{
    if (GDK_IS_X11_DISPLAY(gdk_display_get_default())) {
        return sGLContextProviderGLX.GetGlobalContext();
    } else {
        return sGLContextProviderEGL.GetGlobalContext();
    }
}

/*static*/ void
GLContextProviderWayland::Shutdown()
{
    if (GDK_IS_X11_DISPLAY(gdk_display_get_default())) {
        sGLContextProviderGLX.Shutdown();
    } else {
        sGLContextProviderEGL.Shutdown();
    }
}

} /* namespace gl */
} /* namespace mozilla */
