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


#include "nsImagePh.h"
#include "nsRenderingContextPh.h"

static NS_DEFINE_IID(kIImageIID, NS_IIMAGE_IID);


/** ----------------------------------------------------------------
  * Constructor for nsImagePh
  * @update dc - 11/20/98
  */
nsImagePh :: nsImagePh()
{
  NS_INIT_REFCNT();
}

/** ----------------------------------------------------------------
  * destructor for nsImagePh
  * @update dc - 11/20/98
  */
nsImagePh :: ~nsImagePh()
{
}

NS_IMPL_ISUPPORTS(nsImagePh, kIImageIID);

/** ----------------------------------------------------------------
 * Initialize the nsImagePh object
 * @update dc - 11/20/98
 * @param aWidth - Width of the image
 * @param aHeight - Height of the image
 * @param aDepth - Depth of the image
 * @param aMaskRequirements - A mask used to specify if alpha is needed.
 * @result NS_OK if the image was initied ok
 */
nsresult nsImagePh :: Init(PRInt32 aWidth, PRInt32 aHeight, PRInt32 aDepth,nsMaskRequirements aMaskRequirements)
{

  return NS_OK;
}

/** ----------------------------------------------------------------
  * This routine started out to set up a color table, which as been 
  * outdated, and seemed to have been used for other purposes.  This 
  * routine should be deleted soon -- dc
  * @update dc - 11/20/98
  * @param aContext - a device context to use
  * @param aFlags - flags used for the color manipulation
  * @param aUpdateRect - The update rectangle to use
  * @result void
  */
void nsImagePh :: ImageUpdated(nsIDeviceContext *aContext, PRUint8 aFlags, nsRect *aUpdateRect)
{
}

//------------------------------------------------------------


// Raster op used with MaskBlt(). Assumes our transparency mask has 0 for the
// opaque pixels and 1 for the transparent pixels. That means we want the
// background raster op (value of 0) to be SRCCOPY, and the foreground raster
// (value of 1) to just use the destination bits
#define MASKBLT_ROP MAKEROP4((DWORD)0x00AA0029, SRCCOPY)

/** ----------------------------------------------------------------
  * Create a device dependent windows bitmap
  * @update dc - 11/20/98
  * @param aSurface - The HDC in the form of a drawing surface used to create the DDB
  * @result void
  */
void nsImagePh :: CreateDDB(nsDrawingSurface aSurface)
{
}

/** ----------------------------------------------------------------
  * Draw the bitmap, this method has a source and destination coordinates
  * @update dc - 11/20/98
  * @param aContext - the rendering context to draw with
  * @param aSurface - The HDC in a nsDrawingsurfacePh to copy the bits to.
  * @param aSX - source horizontal location
  * @param aSY - source vertical location
  * @param aSWidth - source width
  * @param aSHeight - source height
  * @param aDX - destination location
  * @param aDY - destination location
  * @param aDWidth - destination width
  * @param aDHeight - destination height
  * @result NS_OK if the draw worked
  */
NS_IMETHODIMP nsImagePh :: Draw(nsIRenderingContext &aContext, nsDrawingSurface aSurface,
				 PRInt32 aSX, PRInt32 aSY, PRInt32 aSWidth, PRInt32 aSHeight,
				 PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight)
{

  return NS_OK;
}

/** ----------------------------------------------------------------
  * Draw the bitmap, defaulting the source to 0,0,source 
  * and destination have the same width and height.
  * @update dc - 11/20/98
  * @param aContext - the rendering context to draw with
  * @param aSurface - The HDC in a nsDrawingsurfacePh to copy the bits to.
  * @param aX - destination location
  * @param aY - destination location
  * @param aWidth - image width
  * @param aHeight - image height
  * @result NS_OK if the draw worked
  */
NS_IMETHODIMP nsImagePh :: Draw(nsIRenderingContext &aContext, nsDrawingSurface aSurface,
				 PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{

  return NS_OK;
}

/** ----------------------------------------------------------------
 * Create an optimezed bitmap, -- this routine may need to be deleted, not really used now
 * @update dc - 11/20/98
 * @param aContext - The device context to use for the optimization
 */
nsresult nsImagePh :: Optimize(nsIDeviceContext* aContext)
{
  return NS_OK;
}

/** ----------------------------------------------------------------
 * Calculate the number of bytes in a span for this image
 * @update dc - 11/20/98
 * @param aWidth - The width to calulate the number of bytes from
 * @return Number of bytes for the span
 */
PRInt32  nsImagePh :: CalcBytesSpan(PRUint32  aWidth)
{

  return(0);
}

/** ----------------------------------------------------------------
 * Clean up the memory associted with this object.  Has two flavors, 
 * one mode is to clean up everything, the other is to clean up only the DIB information
 * @update dc - 11/20/98
 * @param aWidth - The width to calulate the number of bytes from
 * @return Number of bytes for the span
 */
void nsImagePh :: CleanUp(PRBool aCleanUpAll)
{
}


