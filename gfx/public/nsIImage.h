/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsIImage_h___
#define nsIImage_h___

#include "nsISupports.h"
#include "nsIRenderingContext.h"
#include "nsRect.h"
#include "gfxRect.h"

#ifdef MOZ_CAIRO_GFX
class gfxASurface;
#endif

class nsIDeviceContext;

struct nsColorMap
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
};

typedef enum {
    nsMaskRequirements_kNoMask,
    nsMaskRequirements_kNeeds1Bit,
    nsMaskRequirements_kNeeds8Bit
} nsMaskRequirements;


#define  nsImageUpdateFlags_kColorMapChanged 0x1
#define  nsImageUpdateFlags_kBitsChanged     0x2

#ifndef MOZ_CAIRO_GFX
// The following platforms store image data rows bottom-up.
#if defined(XP_WIN) || defined(XP_OS2) || defined(XP_MACOSX)
#define MOZ_PLATFORM_IMAGES_BOTTOM_TO_TOP
#endif
#endif

// IID for the nsIImage interface
// ab6af421-9552-4d18-a3f4-a2bf9d2e44f7
#define NS_IIMAGE_IID \
{ 0xab6af421, 0x9552, 0x4d18, \
  { 0xa3, 0xf4, 0xa2, 0xbf, 0x9d, 0x2e, 0x44, 0xf7 } }

// Interface to Images
class nsIImage : public nsISupports
{

public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IIMAGE_IID)

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
   * @return The number of bytes in each scanline
   */
  virtual PRInt32 GetLineStride() = 0;

  /**
   * Get whether this image has an alpha mask. Preferable to testing
   * if GetAlphaBits() is non-null.
   * @update - sfraser 10/19/99
   * @return PR_TRUE if the image has an alpha mask, PR_FALSE otherwise
   */
  virtual PRBool GetHasAlphaMask() = 0;

  /**
   * Get a pointer to the bits for the alpha mask
   * @update - dwc 2/1/99
   * @return address of the alpha mask pixel array
   */
  virtual PRUint8 * GetAlphaBits() = 0;

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
  virtual void ImageUpdated(nsIDeviceContext *aContext, PRUint8 aFlags, nsIntRect *aUpdateRect) = 0;
  
  /**
   * Get whether this image's region is completely filled with data.
   * @return PR_TRUE if image is complete, PR_FALSE if image is not yet 
   *         complete or broken
   */
  virtual PRBool GetIsImageComplete() = 0;

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
   * BitBlit the nsIImage to a device, the source and dest can be scaled
   * @param aSourceRect  source rectangle, in image pixels
   * @param aDestRect  destination rectangle, in device pixels
   */
  NS_IMETHOD Draw(nsIRenderingContext &aContext,
                  const gfxRect &aSourceRect,
                  const gfxRect &aDestRect) = 0;

  NS_IMETHOD DrawTile(nsIRenderingContext &aContext,
                      nsIDrawingSurface* aSurface,
                      PRInt32 aSXOffset, PRInt32 aSYOffset,
                      PRInt32 aPadX, PRInt32 aPadY,
                      const nsRect &aTileRect) = 0;

  /**
   * BitBlit the entire (no cropping) nsIImage to another nsImage, the source and dest can be scaled
   * @update - saari 03/08/01
   * @param aDstImage  the nsImage to blit to
   * @param aDX The destination horizontal location
   * @param aDY The destination vertical location
   * @param aDWidth The destination width of the pixelmap
   * @param aDHeight The destination height of the pixelmap
   * @return if TRUE, no errors
   */
  NS_IMETHOD DrawToImage(nsIImage* aDstImage, PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight) = 0;

  /**
   * Get the alpha depth for the image mask
   * @update - lordpixel 2001/05/16
   * @return  the alpha mask depth for the image, ie, 0, 1 or 8
   */
  virtual PRInt8 GetAlphaDepth() = 0;

  /**
   * Return information about the bits for this structure
   * @update - dwc 2/1/99
   * @return a bitmap info structure for the Device Dependent Bits
   */
  virtual void* GetBitInfo() = 0;


  /**
   * LockImagePixels
   * Lock the image pixels so that we can access them directly,
   * with safely. May be a noop on some platforms.
   *
   * aMaskPixels = PR_TRUE for the mask, PR_FALSE for the image
   *
   * Must be balanced by a call to UnlockImagePixels().
   *
   * @update - sfraser 10/18/99
   * @return error result
   */
  NS_IMETHOD LockImagePixels(PRBool aMaskPixels) = 0;
  
  /**
   * UnlockImagePixels
   * Unlock the image pixels. May be a noop on some platforms.
   *
   * Should balance an earlier call to LockImagePixels().
   *
   * aMaskPixels = PR_TRUE for the mask, PR_FALSE for the image
   *
   * @update - sfraser 10/18/99
   * @return error result
   */
  NS_IMETHOD UnlockImagePixels(PRBool aMaskPixels) = 0;

#ifdef MOZ_CAIRO_GFX
  NS_IMETHOD GetSurface(gfxASurface **aSurface) = 0;
#endif
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIImage, NS_IIMAGE_IID)

#endif
