/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *   Vladimir Vukicevic <vladimir@pobox.com>
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
