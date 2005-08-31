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
#include "nsPresContext.h"
#include "gfxIImageFrame.h"
#include "nsICanvasElement.h"
#include "nsIInputStream.h"

// {0be74436-51a3-4be3-8357-ede741750080}
#define NS_ICANVASRENDERINGCONTEXTINTERNAL_IID \
  { 0x0be74436, 0x51a3, 0x4be3, { 0x83, 0x57, 0xed, 0xe7, 0x41, 0x75, 0x00, 0x80 } }

class nsICanvasRenderingContextInternal : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ICANVASRENDERINGCONTEXTINTERNAL_IID)

  // This method should NOT hold a ref to aParentCanvas; it will be called
  // with nsnull when the element is going away.
  NS_IMETHOD SetCanvasElement(nsICanvasElement* aParentCanvas) = 0;

  // Will be called whenever the target image frame changes
  NS_IMETHOD SetTargetImageFrame(gfxIImageFrame* aImageFrame) = 0;

  // Gives you a stream containing the image represented by this context.
  // The format is given in aMimeTime, for example "image/png".
  //
  // If the image format does not support transparency or aIncludeTransparency
  // is false, alpha will be discarded and the result will be the image
  // composited on black.
  NS_IMETHOD GetInputStream(const nsACString& aMimeType,
                            const nsAString& aEncoderOptions,
                            nsIInputStream **aStream) = 0;

  // Will be called whenever the element needs to be redrawn,
  // e.g. due to a Redraw on the frame.
  NS_IMETHOD UpdateImageFrame() = 0;
};

#endif /* nsICanvasRenderingContextInternal_h___ */
