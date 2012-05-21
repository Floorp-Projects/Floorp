/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsICanvasElementExternal_h___
#define nsICanvasElementExternal_h___

#include "nsISupports.h"
#include "gfxPattern.h"

class gfxContext;
class nsIFrame;
struct gfxRect;

#define NS_ICANVASELEMENTEXTERNAL_IID \
  { 0x51870f54, 0x6c4c, 0x469a, {0xad, 0x46, 0xf0, 0xa9, 0x8e, 0x32, 0xa7, 0xe2 } }

class nsRenderingContext;
class nsICanvasRenderingContextInternal;

struct _cairo_surface;

/*
 * This interface contains methods that are needed outside of the content/layout
 * modules, specifically widget.  It should eventually go away when we support
 * libxul builds, and nsHTMLCanvasElement be used directly.
 *
 * Code internal to content/layout should /never/ use this interface; if the
 * same functionality is needed in both places, two separate methods should be
 * used.
 */

class nsICanvasElementExternal : public nsISupports {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ICANVASELEMENTEXTERNAL_IID)

  enum {
    RenderFlagPremultAlpha = 0x1
  };

  /**
   * Get the size in pixels of this canvas element
   */
  NS_IMETHOD_(nsIntSize) GetSizeExternal() = 0;

  /*
   * Ask the canvas element to tell the contexts to render themselves
   * to the given gfxContext at the origin of its coordinate space.
   */
  NS_IMETHOD RenderContextsExternal(gfxContext *ctx,
                                    gfxPattern::GraphicsFilter aFilter,
                                    PRUint32 aFlags = RenderFlagPremultAlpha) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsICanvasElementExternal, NS_ICANVASELEMENTEXTERNAL_IID)

#endif /* nsICanvasElementExternal_h___ */
