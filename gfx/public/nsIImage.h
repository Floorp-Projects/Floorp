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

#ifndef nsIImage_h___
#define nsIImage_h___

#include "nsISupports.h"
#include "nsRect.h"
#include "nsIRenderingContext.h"

typedef struct
{
  //I lifted this from the image lib. The difference is that
  //this uses nscolor instead of NI_RGB. Multiple color pollution
  //is a bad thing. MMP
  PRInt32 NumColors;  // Number of colors in the colormap.
                      // A negative value can be used to denote a
                      // possibly non-unique set.
  //nscolor *Map;       // Colormap colors.
  PRUint8 *Index;     // NULL, if map is in index order, otherwise
                      // specifies the indices of the map entries. */
} nsColorMap;

typedef enum {
    nsMaskRequirements_kNoMask,
    nsMaskRequirements_kNeeds1Bit,
    nsMaskRequirements_kNeeds8Bit
} nsMaskRequirements;


#define  nsImageUpdateFlags_kColorMapChanged 0x1
#define  nsImageUpdateFlags_kBitsChanged     0x2
 
// IID for the nsIImage interface
#define NS_IIMAGE_IID          \
{ 0x0b4faaa0, 0xaa3a, 0x11d1, \
{ 0xa8, 0x24, 0x00, 0x40, 0x95, 0x9a, 0x28, 0xc9 } }

// Interface to Images
class nsIImage : public nsISupports
{

public:

  /**
   * Build and initialize the nsIImage
   * @param aWidth The width in pixels of the desired pixelmap
   * @param aHeight The height in pixels of the desired pixelmap
   * @param aDepth The number of bits per pixel for the pixelmap
   * @param aMaskRequirements A flag indicating if a alpha mask should be allocated 
   */
  virtual nsresult Init(PRInt32 aWidth, PRInt32 aHeight, PRInt32 aDepth, nsMaskRequirements aMaskRequirements) = 0;

  /**
   * Get the number of bytes per pixel for this image
   * @update - dwc 2/3/99
   * @return - the number of bytes per pixel
   */
  virtual PRInt32 GetBytesPix() = 0;

  /**
   * Get whether rows are organized top to bottom, or bottom to top 
   * @update - syd 3/29/99 
   * @return PR_TRUE if top to bottom, else PR_FALSE 
   */
  virtual PRBool GetIsRowOrderTopToBottom() = 0;

  /**
   * Get the width for the pixelmap
   * @update - dwc 2/1/99
   * @return The width in pixels for the pixelmap
   */
  virtual PRInt32 GetWidth() = 0;

  /**
   * Get the height for the pixelmap
   * @update - dwc 2/1/99
   * @return The height in pixels for the pixelmap
   */
  virtual PRInt32 GetHeight() = 0;

  /**
   * Get a pointer to the bits for the pixelmap, only if it is not optimized
   * @update - dwc 2/1/99
   * @return address of the DIB pixel array
   */
  virtual PRUint8 * GetBits() = 0;

  /**
   * Get the number of bytes needed to get to the next scanline for the pixelmap
   * @update - dwc 2/1/99
   @return The number of bytes in each scanline
   */
  virtual PRInt32 GetLineStride() = 0;

  /**
   * Get a pointer to the bits for the alpha mask
   * @update - dwc 2/1/99
   * @return address of the alpha mask pixel array
   */
  virtual PRUint8 * GetAlphaBits() = 0;

  /**
   * Get the width of the alpha mask
   * @update - dwc 2/1/99
   * @return The width in pixels
   */
  virtual PRInt32     GetAlphaWidth() = 0;

  /**
   * Get the height of the alpha mask
   * @update - dwc 2/1/99
   * @return The width in pixels
   */
  virtual PRInt32     GetAlphaHeight()  = 0;

  /**
   * Get the number of bytes per scanline for the alpha mask
   * @update - dwc 2/1/99
   * @return The number of bytes in each scanline
   */
  virtual PRInt32 GetAlphaLineStride() = 0;

  /**
   * Update the nsIImage color table
   * @update - dwc 2/1/99
   * @param aFlags Used to pass in parameters for the update
   * @param aUpdateRect The rectangle to update
   */
  virtual void ImageUpdated(nsIDeviceContext *aContext, PRUint8 aFlags, nsRect *aUpdateRect) = 0;

  /**
   * Returns if the pixelmap has been converted to an optimized pixelmap
   * @update - dwc 2/1/99
   * @return If true, it is optimized
   */
  virtual PRBool IsOptimized() = 0;

  /**
   * Converted this pixelmap to an optimized pixelmap for the device
   * @update - dwc 2/1/99
   * @param aContext The device to optimize for
   * @return the result of the operation, if NS_OK, then the pixelmap is optimized
   */
  virtual nsresult Optimize(nsIDeviceContext* aContext) = 0;

  /**
   * Get the colormap for the nsIImage
   * @update - dwc 2/1/99
   * @return if non null, the colormap for the pixelmap,otherwise the image is not color mapped
   */
  virtual nsColorMap * GetColorMap() = 0;

  /**
   * BitBlit the nsIImage to a device, the source can be scaled to the dest
   * @update - dwc 2/1/99
   * @param aSurface  the surface to blit to
   * @param aX The destination horizontal location
   * @param aY The destination vertical location
   * @param aWidth The destination width of the pixelmap
   * @param aHeight The destination height of the pixelmap
   * @return if TRUE, no errors
   */
  NS_IMETHOD Draw(nsIRenderingContext &aContext, nsDrawingSurface aSurface, PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight) = 0;

  /**
   * BitBlit the nsIImage to a device, the source and dest can be scaled
   * @update - dwc 2/1/99
   * @param aSurface  the surface to blit to
   * @param aSX The source width of the pixelmap
   * @param aSY The source vertical location
   * @param aSWidth The source width of the pixelmap
   * @param aSHeight The source height of the pixelmap
   * @param aDX The destination horizontal location
   * @param aDY The destination vertical location
   * @param aDWidth The destination width of the pixelmap
   * @param aDHeight The destination height of the pixelmap
   * @return if TRUE, no errors
   */
  NS_IMETHOD Draw(nsIRenderingContext &aContext, nsDrawingSurface aSurface, PRInt32 aSX, PRInt32 aSY, PRInt32 aSWidth, PRInt32 aSHeight,
                  PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight) = 0;

  /**
   * Set the alpha level for the image
   * @update - dwc 2/1/99
   * @param  the alpha level to set for the image, from 0 to 100%
   */
  virtual void  SetAlphaLevel(PRInt32 aAlphaLevel) = 0;

  /**
   * Get the alpha level for the image
   * @update - dwc 2/1/99
   * @return  the alpha level for the image, from 0 to 100%
   */
  virtual PRInt32 GetAlphaLevel() = 0;

  /**
   * Return information about the bits for this structure
   * @update - dwc 2/1/99
   * @return a bitmap info structure for the Device Dependent Bits
   */
  virtual void* GetBitInfo() = 0;

  //get the color space metrics for this image
  //virtual NI_ColorSpec * GetColorSpec() = 0;                       fix

  //get the color which should be considered transparent.
  //if this image is color mapped, this value will be an
  //index into the color map. hokey? yes, but it avoids
  //another silly api or struct.
  //virtual nscolor GetTransparentColor() = 0;                              fix.
};

//change notification API flag bits
#define NS_IMAGE_UPDATE_COLORMAP  1
#define NS_IMAGE_UPDATE_PIXELS    2
#define NS_IMAGE_UPDATE_ALPHA     4

#endif
