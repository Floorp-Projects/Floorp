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
 *   Peter Hartshorn <peter@igelaus.com.au>
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

#ifndef nsImageXlib_h__
#define nsImageXlib_h__

#include "nsIImage.h"
#include "nsPoint.h"
#include "nsGCCache.h"
#include "nsRegion.h"
#include "xlibrgb.h"

class nsImageXlib : public nsIImage {
public:
  nsImageXlib();
  virtual ~nsImageXlib();

  NS_DECL_ISUPPORTS

  virtual PRInt32     GetBytesPix()       { return mNumBytesPixel; }
  virtual PRInt32     GetHeight();
  virtual PRInt32     GetWidth();
  virtual PRUint8*    GetBits();
  virtual void*       GetBitInfo();
  virtual PRBool      GetIsRowOrderTopToBottom() { return PR_TRUE; }
  virtual PRInt32     GetLineStride();

  virtual nsColorMap* GetColorMap();

  NS_IMETHOD Draw(nsIRenderingContext &aContext,
                  nsIDrawingSurface* aSurface,
                  PRInt32 aX, PRInt32 aY,
                  PRInt32 aWidth, PRInt32 aHeight);
  NS_IMETHOD Draw(nsIRenderingContext &aContext,
                  nsIDrawingSurface* aSurface,
                  PRInt32 aSX, PRInt32 aSY, PRInt32 aSWidth, PRInt32 aSHeight,
                  PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight);

  NS_IMETHOD DrawToImage(nsIImage* aDstImage, nscoord aDX, nscoord aDY,
                         nscoord aDWidth, nscoord aDHeight);

  NS_IMETHOD DrawTile(nsIRenderingContext &aContext,
                      nsIDrawingSurface* aSurface,
                      PRInt32 aSXOffset, PRInt32 aSYOffset,
                      PRInt32 aPadX, PRInt32 aPadY,
                      const nsRect &aTileRect);

  void UpdateCachedImage();
  virtual void ImageUpdated(nsIDeviceContext *aContext,
                            PRUint8 aFlags, nsRect *aUpdateRect);
  virtual nsresult    Init(PRInt32 aWidth, PRInt32 aHeight,
                           PRInt32 aDepth,
                           nsMaskRequirements aMaskRequirements);

  virtual nsresult    Optimize(nsIDeviceContext* aContext);

  virtual PRBool      GetHasAlphaMask()     { return mAlphaBits != nsnull; }     
  virtual PRUint8*    GetAlphaBits();
  virtual PRInt32     GetAlphaLineStride();
  /**
   * Get the alpha depth for the image mask
   * @update - lordpixel 2001/05/16
   * @return  the alpha mask depth for the image, ie, 0, 1 or 8
   */
  virtual PRInt8 GetAlphaDepth() {return(mAlphaDepth);}  

  NS_IMETHOD   LockImagePixels(PRBool aMaskPixels);
  NS_IMETHOD   UnlockImagePixels(PRBool aMaskPixels);

private:
  /**
   * Calculate the amount of memory needed for the initialization of the image
   */
  void ComputeMetrics() {
    mRowBytes = (mWidth * mDepth) >> 5;

    if (((PRUint32)mWidth * mDepth) & 0x1F)
      mRowBytes++;
    mRowBytes <<= 2;

    mSizeImage = mRowBytes * mHeight;
  };
  void ComputePaletteSize(PRIntn nBitCount);

private:
  NS_IMETHODIMP DrawScaled(nsIRenderingContext &aContext, nsIDrawingSurface* aSurface,
                           PRInt32 aSX, PRInt32 aSY, PRInt32 aSWidth, PRInt32 aSHeight,
                           PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight);

  static unsigned scaled6[1<<6];
  static unsigned scaled5[1<<5];

  void DrawComposited32(PRBool isLSB, PRBool flipBytes,
                        PRUint8 *imageOrigin, PRUint32 imageStride,
                        PRUint8 *alphaOrigin, PRUint32 alphaStride,
                        unsigned width, unsigned height,
                        XImage *ximage, unsigned char *readData);
  void DrawComposited24(PRBool isLSB, PRBool flipBytes,
                        PRUint8 *imageOrigin, PRUint32 imageStride,
                        PRUint8 *alphaOrigin, PRUint32 alphaStride,
                        unsigned width, unsigned height,
                        XImage *ximage, unsigned char *readData);
  void DrawComposited16(PRBool isLSB, PRBool flipBytes,
                        PRUint8 *imageOrigin, PRUint32 imageStride,
                        PRUint8 *alphaOrigin, PRUint32 alphaStride,
                        unsigned width, unsigned height,
                        XImage *ximage, unsigned char *readData);
  void DrawCompositedGeneral(PRBool isLSB, PRBool flipBytes,
                             PRUint8 *imageOrigin, PRUint32 imageStride,
                             PRUint8 *alphaOrigin, PRUint32 alphaStride,
                             unsigned width, unsigned height,
                             XImage *ximage, unsigned char *readData);
  inline void DrawComposited(nsIRenderingContext &aContext,
                             nsIDrawingSurface* aSurface,
                             PRInt32 aSX, PRInt32 aSY,
                             PRInt32 aSWidth, PRInt32 aSHeight,
                             PRInt32 aDX, PRInt32 aDY,
                             PRInt32 aDWidth, PRInt32 aDHeight);

  inline void TilePixmap(Pixmap src, Pixmap dest, PRInt32 aSXOffset, PRInt32 aSYOffset,
                         const nsRect &destRect, const nsRect &clipRect, PRBool useClip);
  inline void CreateAlphaBitmap(PRInt32 aWidth, PRInt32 aHeight);
  inline void CreateOffscreenPixmap(PRInt32 aWidth, PRInt32 aHeight);
  inline void DrawImageOffscreen(PRInt32 aSX, PRInt32 aSY,
                                 PRInt32 aWidth, PRInt32 aHeight);
  inline void SetupGCForAlpha(GC aGC, PRInt32 aX, PRInt32 aY);

  // image bits
  PRUint8      *mImageBits;
  PRUint8      *mAlphaBits;
  Pixmap        mImagePixmap;
  Pixmap        mAlphaPixmap;

  PRInt32       mWidth;
  PRInt32       mHeight;
  PRInt32       mDepth;       // bits per pixel
  PRInt32       mRowBytes;
  GC            mGC;

  PRInt32       mSizeImage;
  PRInt8        mNumBytesPixel;

  PRInt32       mDecodedX1;       //Keeps track of what part of image
  PRInt32       mDecodedY1;       // has been decoded.
  PRInt32       mDecodedX2;
  PRInt32       mDecodedY2;

  nsRegion      mUpdateRegion;

  static XlibRgbHandle *mXlibRgbHandle;
  static Display       *mDisplay;

  // alpha layer members
  PRInt8        mAlphaDepth;        // alpha layer depth
  PRInt16       mAlphaRowBytes;     // alpha bytes per row
  PRPackedBool  mAlphaValid;
  PRPackedBool  mIsSpacer;
  PRPackedBool  mPendingUpdate;

  PRUint8       mFlags;             // flags set by ImageUpdated
};

#endif /* !nsImageXlib_h__ */
