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

#ifndef nsICanvasElement_h___
#define nsICanvasElement_h___

#include "nsISupports.h"
#include "nsRect.h"

class gfxContext;
class nsIFrame;

// {C234660C-BD06-493e-8583-939A5A158B37}
#define NS_ICANVASELEMENT_IID \
    { 0xc234660c, 0xbd06, 0x493e, { 0x85, 0x83, 0x93, 0x9a, 0x5a, 0x15, 0x8b, 0x37 } }

class nsIRenderingContext;

struct _cairo_surface;

class nsICanvasElement : public nsISupports {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ICANVASELEMENT_IID)

  /**
   * Ask the canvas Element to return the primary frame, if any
   */
  NS_IMETHOD GetPrimaryCanvasFrame (nsIFrame **aFrame) = 0;

  /**
   * Get the size in pixels of this canvas element
   */
  NS_IMETHOD GetSize (PRUint32 *width, PRUint32 *height) = 0;

  /*
   * Ask the canvas element to tell the contexts to render themselves
   * to the given gfxContext at the origin of its coordinate space.
   */
  NS_IMETHOD RenderContexts (gfxContext *ctx) = 0;

  /**
   * Determine whether the canvas is write-only.
   */
  virtual PRBool IsWriteOnly() = 0;

  /**
   * Force the canvas to be write-only.
   */
  virtual void SetWriteOnly() = 0;

  /*
   * Ask the canvas frame to invalidate itself
   */
  NS_IMETHOD InvalidateFrame () = 0;

  /*
   * Ask the canvas frame to invalidate a portion of the frame; damageRect
   * is relative to the origin of the canvas frame.
   */
  NS_IMETHOD InvalidateFrameSubrect (const nsRect& damageRect) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsICanvasElement, NS_ICANVASELEMENT_IID)

#endif /* nsICanvasElement_h___ */
