/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsRenderingContextImpl_h___
#define nsRenderingContextImpl_h___

#include "nsIRenderingContext.h"


class nsRenderingContextImpl : public nsIRenderingContext
{
public:
  nsRenderingContextImpl();



  /** ---------------------------------------------------
   *  See documentation in nsIRenderingContext.h
   *	@update 03/29/00 dwc
   */
  NS_IMETHOD DrawTile(nsIImage *aImage,nscoord aX0,nscoord aY0,nscoord aX1,nscoord aY1,nscoord aWidth,nscoord aHeight);

  /** ---------------------------------------------------
   *  See documentation in nsIRenderingContext.h
   *	@update 03/29/00 dwc
   */
  NS_IMETHOD DrawPath(nsPoint aPointArray[],PRInt32 aNumPts);

protected:
  virtual ~nsRenderingContextImpl();

  /** ---------------------------------------------------
   *  Check to see if the given size of tile can be imaged by the RenderingContext
   *	@update 03/29/00 dwc
   *  @param aWidth The width of the tile
   *  @param aHeight The height of the tile
   *  @return PR_TRUE the RenderingContext can handle this tile
   */
  virtual PRBool CanTile(nscoord aWidth,nscoord aHeight) { return PR_FALSE; }

  
  /** ---------------------------------------------------
   *  A bit blitter to tile images to the background recursively
   *	@update 3/29/00 dwc
   *  @param aDS -- Target drawing surface for the rendering context
   *  @param aSrcRect -- Rectangle we are build with the image
   *  @param aHeight -- height of the tile
   *  @param aWidth -- width of the tile
   */
  void  TileImage(nsDrawingSurface  aDS,nsRect &aSrcRect,PRInt16 aWidth,PRInt16 aHeight);


public:

};

#endif /* nsRenderingContextImpl */
