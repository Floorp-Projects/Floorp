/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsIBlender_h___
#define nsIBlender_h___

#include "nscore.h"
#include "nsISupports.h"
#include "nsIRenderingContext.h"

// IID for the nsIBlender interface
#define NS_IBLENDER_IID    \
{ 0xbdb4b5b0, 0xf0db, 0x11d1, \
{ 0xa8, 0x2a, 0x00, 0x40, 0x95, 0x9a, 0x28, 0xc9 } }

//----------------------------------------------------------------------

// Blender interface
class nsIBlender : public nsISupports
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IBLENDER_IID)

  /**
   * Initialize the Blender
   * @update dc 11/4/98
   * @param  aContext is where the blender can get info about the device its blending on
   * @result The result of the initialization, NS_OK if no errors
   */
  NS_IMETHOD Init(nsIDeviceContext *aContext) = 0;

  /**
   * NOTE: if we can make this static, that would be great. I don't think we can.
   * Blend source and destination nsDrawingSurfaces. Both drawing surfaces
   * will have bitmaps associated with them.
   * @param aSX x offset into source drawing surface of blend area
   * @param aSY y offset into source drawing surface of blend area
   * @param aWidth width of blend area
   * @param aHeight width of blend area
   * @param aSrc  source for the blending
   * @param aDest destination for blending
   * @param aDX x offset into destination drawing surface of blend area
   * @param aDY y offset into destination drawing surface of blend area
   * @param aSrcOpacity 0.0f -> 1.0f opacity value of source area. 1.0f indicates
   *        complete opacity.
   * @param aSecondSrc an optional second source drawing surface which is used in
   *        conjunction with the background color parameters to determine
   *        which pixels to blend
   * @param aSrcBackColor color of pixels in aSrc that should be
   *        considered "background" color
   * @param aSecondSrcBackColor color of pixels in aSrc that should be
   *        considered "background" color
   */
  NS_IMETHOD Blend(PRInt32 aSX, PRInt32 aSY, PRInt32 aWidth, PRInt32 aHeight,nsDrawingSurface aSrc,
                   nsDrawingSurface aDest, PRInt32 aDX, PRInt32 aDY, float aSrcOpacity,
                   nsDrawingSurface aSecondSrc = nsnull, nscolor aSrcBackColor = NS_RGB(0, 0, 0),
                   nscolor aSecondSrcBackColor = NS_RGB(0, 0, 0)) = 0;

  NS_IMETHOD Blend(PRInt32 aSX, PRInt32 aSY, PRInt32 aWidth, PRInt32 aHeight, nsIRenderingContext *aSrc,
                   nsIRenderingContext *aDest, PRInt32 aDX, PRInt32 aDY, float aSrcOpacity,
                   nsIRenderingContext *aSecondSrc = nsnull, nscolor aSrcBackColor = NS_RGB(0, 0, 0),
                   nscolor aSecondSrcBackColor = NS_RGB(0, 0, 0)) = 0;
};

#endif
