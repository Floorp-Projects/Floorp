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
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is
 * John Fairhurst, <john_fairhurst@iname.com>.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
 * ***** END LICENSE BLOCK *****
 *
 * This Original Code has been modified by IBM Corporation. Modifications made by IBM 
 * described herein are Copyright (c) International Business Machines Corporation, 2000.
 * Modifications to Mozilla code or documentation identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 05/11/2000       IBM Corp.      Make it look more like Windows.
 */

#ifndef _nsImageOS2_h_
#define _nsImageOS2_h_

#include "nsIImage.h"
#include "nsRect.h"

struct nsDrawingSurfaceOS2;

class nsImageOS2 : public nsIImage{
public:
  nsImageOS2();
  ~nsImageOS2();

  NS_DECL_ISUPPORTS

  /**
  @see nsIImage.h
  */
  virtual PRInt32     GetBytesPix()       { return mInfo ? (mInfo->cBitCount <= 8 ? 1 : mInfo->cBitCount / 8) : 0; }
  virtual PRInt32     GetHeight()         { return mInfo ? mInfo->cy : 0; }
  virtual PRBool      GetIsRowOrderTopToBottom() { return PR_FALSE; }
  virtual PRInt32     GetWidth()          { return mInfo ? mInfo->cx : 0; }
  virtual PRUint8*    GetBits()           { return mImageBits; }
  virtual PRInt32     GetLineStride()     { return mRowBytes; }

  virtual PRBool      GetHasAlphaMask()   { return mAlphaBits != nsnull; }

  NS_IMETHOD          Draw(nsIRenderingContext &aContext, nsIDrawingSurface* aSurface, PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight);
  NS_IMETHOD          Draw(nsIRenderingContext &aContext, nsIDrawingSurface* aSurface, PRInt32 aSX, PRInt32 aSY, PRInt32 aSWidth, PRInt32 aSHeight,
                      PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight);
  virtual nsColorMap* GetColorMap() {return mColorMap;}
  virtual void        ImageUpdated(nsIDeviceContext *aContext, PRUint8 aFlags, nsRect *aUpdateRect);
  virtual nsresult    Init(PRInt32 aWidth, PRInt32 aHeight, PRInt32 aDepth, nsMaskRequirements aMaskRequirements);
  virtual nsresult    Optimize(nsIDeviceContext* aContext);
  virtual PRUint8*    GetAlphaBits()      { return mAlphaBits; }
  virtual PRInt32     GetAlphaLineStride(){ return mARowBytes; }

  /** 
   * Draw a tiled version of the bitmap
   * @param aSurface  the surface to blit to
   * @param aSXOffset Offset from image's upper right corner that will be located at 
   * @param aSYOffset   upper right corner of filled area
   * @param aTileRect Area which must be filled with tiled image
   * @return if TRUE, no errors
   */
  NS_IMETHOD DrawTile(nsIRenderingContext &aContext,
                      nsIDrawingSurface* aSurface,
                      PRInt32 aSXOffset, PRInt32 aSYOffset,
		      PRInt32 aPadX, PRInt32 aPadY,
                      const nsRect &aTileRect);

  NS_IMETHOD DrawToImage(nsIImage* aDstImage, nscoord aDX, nscoord aDY,
                         nscoord aDWidth, nscoord aDHeight);

  /** 
   * Return the header size of the Device Independent Bitmap(DIB).
   * @return size of header in bytes
   */
#if 0 // OS2TODO
  PRIntn      GetSizeHeader(){return sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * mNumPaletteColors;}
#endif

  /** 
   * Return the image size of the Device Independent Bitmap(DIB).
   * @update dc - 10/29/98
   * @return size of image in bytes
   */
#if 0 // OS2TODO
  PRIntn      GetSizeImage(){ return mSizeImage; }
#endif

  /** 
   * Calculate the number of bytes spaned for this image for a given width
   * @param aWidth is the width to calculate the number of bytes for
   * @return the number of bytes in this span
   */
#if 0 // OS2TODO
  PRInt32  CalcBytesSpan(PRUint32  aWidth);
#endif

  /**
   * Get the alpha depth for the image mask
   * @update - lordpixel 2001/05/16
   * @return  the alpha mask depth for the image, ie, 0, 1 or 8
   */
  virtual PRInt8 GetAlphaDepth() {return(mAlphaDepth);}

  /** 
   * Get the DIB specific informations for this bitmap.
   * @update dc - 10/29/98
   * @return VOID
   */
  void* GetBitInfo()  { return mInfo; }

  NS_IMETHOD   LockImagePixels(PRBool aMaskPixels);
  NS_IMETHOD   UnlockImagePixels(PRBool aMaskPixels);


 private:
  /** 
   * Clean up the memory used nsImageWin.
   * @update dc - 10/29/98
   * @param aCleanUpAll - if True, all the memory used will be released otherwise just clean up the DIB memory
   */
  void CleanUp(PRBool aCleanUpAll);

  void DrawComposited24(unsigned char *aBits,
                        PRUint8 *aImageRGB, PRUint32 aStrideRGB,
                        PRUint8 *aImageAlpha, PRUint32 aStrideAlpha,
                        int aWidth, int aHeight);

#ifdef OS2TODO
  /** 
   * Create a Device Dependent bitmap from a drawing surface
   * @update dc - 10/29/98
   * @param aSurface - The drawingsurface to create the DDB from.
   */
  void CreateDDB(nsIDrawingSurface* aSurface);


  /** 
   * Create a Device Dependent bitmap from a drawing surface
   * @update dc - 05/20/99
   * @param aSurface - The drawingsurface to create the DIB from.
   * @param aWidth - width of DIB
   * @param aHeight - height of DIB
   */
  nsresult ConvertDDBtoDIB(PRInt32 aWidth,PRInt32 aHeight);


  /** 
   * Print a DDB
   * @update dc - 05/20/99
   * @param aSurface - The drawingsurface to create the DIB from.
   * @param aX - x location to place image
   * @param aX - y location to place image
   * @param aWidth - width of DIB
   * @param aHeight - height of DIB
   */
  nsresult PrintDDB(nsIDrawingSurface* aSurface,PRInt32 aX,PRInt32 aY,PRInt32 aWidth,PRInt32 aHeight);


  /** 
   * Get an index in the palette that matches as closly as possible the passed in RGB colors
   * @update dc - 10/29/98
   * @param aR - Red component of the color to match
   * @param aG - Green component of the color to match
   * @param aB - Blue component of the color to match
   * @return - The closest palette match
   */
  PRUint8 PaletteMatch(PRUint8 r, PRUint8 g, PRUint8 b);
#endif
  BITMAPINFO2*        mInfo;
  PRUint32            mDeviceDepth;
  PRInt32             mRowBytes;          // number of bytes per row
  PRUint8*            mImageBits;         // starting address of DIB bits
  PRBool              mIsOptimized;       // Did we convert our DIB to a HBITMAP
  nsColorMap*         mColorMap;          // Redundant with mColorTable, but necessary

  nsRect              mDecodedRect;       // Keeps track of what part of image has been decoded.
    
  // alpha layer members
  PRUint8             *mAlphaBits;        // alpha layer if we made one
  PRInt8              mAlphaDepth;        // alpha layer depth
  PRInt16             mARowBytes;         // number of bytes per row in the image for tha alpha

  static PRUint8      gBlenderLookup [65536];    // Lookup table for fast alpha blending
  static PRBool       gBlenderReady;
  static void         BuildBlenderLookup (void);
  static PRUint8      FAST_BLEND (PRUint8 Source, PRUint8 Dest, PRUint8 Alpha) { return gBlenderLookup [(Alpha << 8) + Source] + 
                                                                                        gBlenderLookup [((255 - Alpha) << 8) + Dest]; }

  NS_IMETHODIMP UpdateImageBits( HPS mPS );
  void NS2PM_ININ( const nsRect &in, RECTL &rcl);
  void CreateBitmaps( nsDrawingSurfaceOS2 *surf);

  void     BuildTile (HPS hpsTile, PRUint8* pImageBits, PBITMAPINFO2 pBitmapInfo, nscoord aTileWidth, nscoord aTileHeight, float scale);
};

#endif
