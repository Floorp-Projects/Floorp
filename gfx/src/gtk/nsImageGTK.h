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

#ifndef nsImageGTK_h___
#define nsImageGTK_h___

#include "nsIImage.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <gdk/gdk.h>
#include "nsRegion.h"

class nsDrawingSurfaceGTK;

class nsImageGTK : public nsIImage
{
public:
  nsImageGTK();
  virtual ~nsImageGTK();

  static void Startup();
  static void Shutdown();

  NS_DECL_ISUPPORTS

  /**
  @see nsIImage.h
  */
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
  virtual PRInt8 GetAlphaDepth() { 
    if (mTrueAlphaBits)
      return mTrueAlphaDepth;
    else
      return mAlphaDepth;
  }

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

  static unsigned scaled6[1<<6];
  static unsigned scaled5[1<<5];

  void DrawComposited32(PRBool isLSB, PRBool flipBytes,
                        PRUint8 *imageOrigin, PRUint32 imageStride,
                        PRUint8 *alphaOrigin, PRUint32 alphaStride,
                        unsigned width, unsigned height,
                        XImage *ximage, unsigned char *readData, unsigned char *srcData);
  void DrawComposited24(PRBool isLSB, PRBool flipBytes,
                        PRUint8 *imageOrigin, PRUint32 imageStride,
                        PRUint8 *alphaOrigin, PRUint32 alphaStride,
                        unsigned width, unsigned height,
                        XImage *ximage, unsigned char *readData, unsigned char *srcData);
  void DrawComposited16(PRBool isLSB, PRBool flipBytes,
                        PRUint8 *imageOrigin, PRUint32 imageStride,
                        PRUint8 *alphaOrigin, PRUint32 alphaStride,
                        unsigned width, unsigned height,
                        XImage *ximage, unsigned char *readData, unsigned char *srcData);
  void DrawCompositedGeneral(PRBool isLSB, PRBool flipBytes,
                             PRUint8 *imageOrigin, PRUint32 imageStride,
                             PRUint8 *alphaOrigin, PRUint32 alphaStride,
                             unsigned width, unsigned height,
                             XImage *ximage, unsigned char *readData, unsigned char *srcData);
  inline void DrawComposited(nsIRenderingContext &aContext,
                             nsIDrawingSurface* aSurface,
                             PRInt32 srcWidth, PRInt32 srcHeight,
                             PRInt32 dstWidth, PRInt32 dstHeight,
                             PRInt32 dstOrigX, PRInt32 dstOrigY,
                             PRInt32 aDX, PRInt32 aDY,
                             PRInt32 aDWidth, PRInt32 aDHeight);
  inline void DrawCompositeTile(nsIRenderingContext &aContext,
                                nsIDrawingSurface* aSurface,
                                PRInt32 aSX, PRInt32 aSY,
                                PRInt32 aSWidth, PRInt32 aSHeight,
                                PRInt32 aDX, PRInt32 aDY,
                                PRInt32 aDWidth, PRInt32 aDHeight);

  inline void TilePixmap(GdkPixmap *src, GdkPixmap *dest, PRInt32 aSXOffset, PRInt32 aSYOffset, 
                         const nsRect &destRect, const nsRect &clipRect, PRBool useClip);
  inline void CreateOffscreenPixmap(PRInt32 aWidth, PRInt32 aHeight);
  inline void SetupGCForAlpha(GdkGC *aGC, PRInt32 aX, PRInt32 aY);

  void SlowTile(nsDrawingSurfaceGTK *aSurface, const nsRect &aTileRect,
                PRInt32 aSXOffset, PRInt32 aSYOffset);

  PRUint8      *mImageBits;
  GdkPixmap    *mImagePixmap;
  PRUint8      *mTrueAlphaBits;
  PRUint8      *mAlphaBits;
  GdkPixmap    *mAlphaPixmap;
  XImage       *mAlphaXImage;

  PRInt32       mWidth;
  PRInt32       mHeight;
  PRInt32       mRowBytes;
  PRInt32       mSizeImage;

  PRInt32       mDecodedX1;         // Keeps track of what part of image
  PRInt32       mDecodedY1;         // has been decoded.
  PRInt32       mDecodedX2;
  PRInt32       mDecodedY2;

  nsRegion      mUpdateRegion;

  // alpha layer members
  PRInt32       mAlphaRowBytes;     // alpha bytes per row
  PRInt32       mTrueAlphaRowBytes; // alpha bytes per row
  PRInt8        mAlphaDepth;        // alpha layer depth
  PRInt8        mTrueAlphaDepth;    // alpha layer depth
  PRPackedBool  mIsSpacer;
  PRPackedBool  mPendingUpdate;

  PRInt8        mNumBytesPixel;
  PRUint8       mFlags;             // flags set by ImageUpdated
  PRInt8        mDepth;             // bits per pixel

  PRPackedBool  mOptimized;
};

#endif
