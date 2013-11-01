/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef IN_GL_CONTEXT_PROVIDER_H
#error GLContextProviderImpl.h must only be included from GLContextProvider.h
#endif

#ifndef GL_CONTEXT_PROVIDER_NAME
#error GL_CONTEXT_PROVIDER_NAME not defined
#endif

class GL_CONTEXT_PROVIDER_NAME
{
public:
    typedef gfx::SurfaceCaps SurfaceCaps;
    /**
     * Create a context that renders to the surface of the widget that is
     * passed in.  The context is always created with an RGB pixel format,
     * with no alpha, depth or stencil.  If any of those features are needed,
     * either use a framebuffer, or use CreateOffscreen.
     *
     * This context will attempt to share resources with all other window
     * contexts.  As such, it's critical that resources allocated that are not
     * needed by other contexts be deleted before the context is destroyed.
     *
     * The GetSharedContext() method will return non-null if sharing
     * was successful.
     *
     * Note: a context created for a widget /must not/ hold a strong
     * reference to the widget; otherwise a cycle can be created through
     * a GL layer manager.
     *
     * @param aWidget Widget whose surface to create a context for
     *
     * @return Context to use for the window
     */
    static already_AddRefed<GLContext>
    CreateForWindow(nsIWidget* widget);

    /**
     * Create a context for offscreen rendering.  The target of this
     * context should be treated as opaque -- it might be a FBO, or a
     * pbuffer, or some other construct.  Users of this GLContext
     * should bind framebuffer 0 directly to use this offscreen buffer.
     *
     * The offscreen context returned by this method will always have
     * the ability to be rendered into a context created by a window.
     * It might or might not share resources with the global context;
     * query GetSharedContext() for a non-null result to check.  If
     * resource sharing can be avoided on the target platform, it will
     * be, in order to isolate the offscreen context.
     *
     * @param aSize The initial size of this offscreen context.
     * @param aFormat The ContextFormat for this offscreen context.
     *
     * @return Context to use for offscreen rendering
     */
    static already_AddRefed<GLContext>
    CreateOffscreen(const gfxIntSize& size,
                    const SurfaceCaps& caps,
                    ContextFlags flags = ContextFlagsNone);

    /**
     * Get a pointer to the global context, creating it if it doesn't exist.
     */
    static GLContext*
    GetGlobalContext(ContextFlags flags = ContextFlagsNone);
    
    /**
     * Free any resources held by this Context Provider.
     */
    static void
    Shutdown();
};
