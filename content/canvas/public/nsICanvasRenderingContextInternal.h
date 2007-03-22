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

#ifndef nsICanvasRenderingContextInternal_h___
#define nsICanvasRenderingContextInternal_h___

#include "nsISupports.h"
#include "nsICanvasElement.h"
#include "nsIInputStream.h"

// {05150761-22A3-4e8d-A03E-EC53CB731C70}
#define NS_ICANVASRENDERINGCONTEXTINTERNAL_IID \
  { 0x5150761, 0x22a3, 0x4e8d, { 0xa0, 0x3e, 0xec, 0x53, 0xcb, 0x73, 0x1c, 0x70 } }

class nsIRenderingContext;

struct _cairo_surface;

class nsICanvasRenderingContextInternal : public nsISupports {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ICANVASRENDERINGCONTEXTINTERNAL_IID)

  // This method should NOT hold a ref to aParentCanvas; it will be called
  // with nsnull when the element is going away.
  NS_IMETHOD SetCanvasElement(nsICanvasElement* aParentCanvas) = 0;

  // Sets the dimensions of the canvas, in pixels.  Called
  // whenever the size of the element changes.
  NS_IMETHOD SetDimensions(PRInt32 width, PRInt32 height) = 0;

  // Render the canvas at the origin of the given nsIRenderingContext
  NS_IMETHOD Render(nsIRenderingContext *rc) = 0;

  // Render the canvas at the origin of the given cairo surface
  NS_IMETHOD RenderToSurface(struct _cairo_surface *surf) = 0;

  // Gives you a stream containing the image represented by this context.
  // The format is given in aMimeTime, for example "image/png".
  //
  // If the image format does not support transparency or aIncludeTransparency
  // is false, alpha will be discarded and the result will be the image
  // composited on black.
  NS_IMETHOD GetInputStream(const nsACString& aMimeType,
                            const nsAString& aEncoderOptions,
                            nsIInputStream **aStream) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsICanvasRenderingContextInternal,
                              NS_ICANVASRENDERINGCONTEXTINTERNAL_IID)

#endif /* nsICanvasRenderingContextInternal_h___ */
