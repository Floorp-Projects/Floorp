/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

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
  /**
   * Initialize the Blender
   * @param  aSrc is the source drawing image to buffer
   * @param  aDst is the dest drawing image to buffer
   * @result The result of the initialization, NS_OK if no errors
   */
  virtual nsresult Init(nsDrawingSurface aSrc,nsDrawingSurface aDst) = 0;

  /**
   * Clean up the intialization stuff
   */
  virtual void CleanUp() = 0;

  /**
   * NOTE: if we can make this static, that would be great. I don't think we can.
   * Blend source and destination nsDrawingSurfaces. Both drawing surfaces
   * will have bitmaps associated with them.
   * @param aSrc source for blending
   * @param aSX x offset into source drawing surface of blend area
   * @param aSY y offset into source drawing surface of blend area
   * @param aWidth width of blend area
   * @param aHeight width of blend area
   * @param aDest destination for blending
   * @param aDX x offset into destination drawing surface of blend area
   * @param aDY y offset into destination drawing surface of blend area
   * @param aSrcOpacity 0.0f -> 1.0f opacity value of source area. 1.0f indicates
   *        complete opacity.
   */
  virtual void Blend(nsDrawingSurface aSrc,
                     PRInt32 aSX, PRInt32 aSY, PRInt32 aWidth, PRInt32 aHeight,
                     nsDrawingSurface aDest, PRInt32 aDX, PRInt32 aDY, float aSrcOpacity,PRBool aSaveBlendArea) = 0;

  /**
   * Return the source drawing surface that the blending it going to
   * @return The platform specific drawing surface
   */
  virtual nsDrawingSurface GetSrcDS()=0;

  /**
   * Return the destination drawing surface that the blending it going to
   * @return The platform specific drawing surface
   */
  virtual nsDrawingSurface GetDstDS()=0;

  /**
   * Restore the the blended area of the image if one was save on a previous blend.
   */
  virtual PRBool  RestoreImage(nsDrawingSurface aDst)=0;
};

#endif