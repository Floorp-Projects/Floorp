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

#ifndef nsImageGTK_h___
#define nsImageGTK_h___

#include "nsIImage.h"

#include "X11/Xlib.h"
#include "X11/Xutil.h"
#include <gdk/gdk.h>
#include "nsRegion.h"

#undef Bool

class nsImageGTK : public nsIImage
{
public:
  nsImageGTK();
  virtual ~nsImageGTK();

  NS_DECL_ISUPPORTS

  /**
  @see nsIImage.h
  */
  virtual PRInt32     GetBytesPix()       { return mNumBytesPixel; }
  virtual PRInt32     GetHeight();
  virtual PRInt32     GetWidth();
  virtual PRUint8*    GetBits();
  virtual void*       GetBitInfo();
  virtual PRBool      GetIsRowOrderTopToBottom() { return mIsTopToBottom; }
  virtual PRInt32     GetLineStride();

  NS_IMETHOD     SetNaturalWidth(PRInt32 naturalwidth) { mNaturalWidth= naturalwidth; return NS_OK;}
  NS_IMETHOD     SetNaturalHeight(PRInt32 naturalheight) { mNaturalHeight= naturalheight; return NS_OK;}
  virtual PRInt32     GetNaturalWidth() {return mNaturalWidth; }
  virtual PRInt32     GetNaturalHeight() {return mNaturalHeight; }

  NS_IMETHOD          SetDecodedRect(PRInt32 x1, PRInt32 y1, PRInt32 x2, PRInt32 y2);
  virtual PRInt32     GetDecodedX1() { return mDecodedX1;}
  virtual PRInt32     GetDecodedY1() { return mDecodedY1;}
  virtual PRInt32     GetDecodedX2() { return mDecodedX2;}
  virtual PRInt32     GetDecodedY2() { return mDecodedY2;}

  virtual nsColorMap* GetColorMap();

  NS_IMETHOD Draw(nsIRenderingContext &aContext,
                  nsDrawingSurface aSurface,
                  PRInt32 aX, PRInt32 aY,
                  PRInt32 aWidth, PRInt32 aHeight);
  NS_IMETHOD Draw(nsIRenderingContext &aContext,
                  nsDrawingSurface aSurface,
                  PRInt32 aSX, PRInt32 aSY, PRInt32 aSWidth, PRInt32 aSHeight,
                  PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight);

#ifdef USE_IMG2
  NS_IMETHOD DrawToImage(nsIImage* aDstImage, nscoord aDX, nscoord aDY,
                         nscoord aDWidth, nscoord aDHeight);
#endif

  NS_IMETHOD DrawTile(nsIRenderingContext &aContext,
                      nsDrawingSurface aSurface,
                      PRInt32 aSXOffset, PRInt32 aSYOffset,
                      const nsRect &aTileRect);

  void UpdateCachedImage();
  virtual void ImageUpdated(nsIDeviceContext *aContext,
                            PRUint8 aFlags, nsRect *aUpdateRect);
  virtual nsresult    Init(PRInt32 aWidth, PRInt32 aHeight,
                           PRInt32 aDepth,
                           nsMaskRequirements aMaskRequirements);
  virtual PRBool      IsOptimized();

  virtual nsresult    Optimize(nsIDeviceContext* aContext);

  virtual PRBool      GetHasAlphaMask()     { return mAlphaBits != nsnull; }        
  virtual PRUint8*    GetAlphaBits();
  virtual PRInt32     GetAlphaWidth();
  virtual PRInt32     GetAlphaHeight();
  virtual PRInt32     GetAlphaLineStride();
  virtual nsIImage*   DuplicateImage();

  virtual void  SetAlphaLevel(PRInt32 aAlphaLevel);
  virtual PRInt32 GetAlphaLevel();
  /**
   * Get the alpha depth for the image mask
   * @update - lordpixel 2001/05/16
   * @return  the alpha mask depth for the image, ie, 0, 1 or 8
   */
  virtual PRInt8 GetAlphaDepth() {return(mAlphaDepth);}  
  virtual void  MoveAlphaMask(PRInt32 aX, PRInt32 aY);

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

  NS_IMETHOD DrawScaled(nsIRenderingContext &aContext, nsDrawingSurface aSurface,
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
                             nsDrawingSurface aSurface,
                             PRInt32 aSX, PRInt32 aSY,
                             PRInt32 aSWidth, PRInt32 aSHeight,
                             PRInt32 aDX, PRInt32 aDY,
                             PRInt32 aDWidth, PRInt32 aDHeight);

  inline void TilePixmap(GdkPixmap *src, GdkPixmap *dest, PRInt32 aSXOffset, PRInt32 aSYOffset, 
                         const nsRect &destRect, const nsRect &clipRect, PRBool useClip);
  inline void CreateAlphaBitmap(PRInt32 aWidth, PRInt32 aHeight);
  inline void CreateOffscreenPixmap(PRInt32 aWidth, PRInt32 aHeight);
  inline void SetupGCForAlpha(GdkGC *aGC, PRInt32 aX, PRInt32 aY);

  PRUint8      *mImageBits;
  GdkPixmap    *mImagePixmap;
  PRUint8      *mAlphaBits;
  GdkPixmap    *mAlphaPixmap;

  PRInt32       mWidth;
  PRInt32       mHeight;
  PRInt32       mRowBytes;
  PRInt32       mSizeImage;

  PRInt32       mNaturalWidth;
  PRInt32       mNaturalHeight;

  PRInt32       mDecodedX1;         // Keeps track of what part of image
  PRInt32       mDecodedY1;         // has been decoded.
  PRInt32       mDecodedX2;
  PRInt32       mDecodedY2;

  nsRegion      mUpdateRegion;

  // alpha layer members
  PRInt16       mAlphaRowBytes;     // alpha bytes per row
  PRInt16       mAlphaWidth;        // alpha layer width
  PRInt16       mAlphaHeight;       // alpha layer height
  PRInt8        mAlphaDepth;        // alpha layer depth
  PRPackedBool  mAlphaValid;
  PRPackedBool  mIsSpacer;
  PRPackedBool  mPendingUpdate;

  PRPackedBool  mIsTopToBottom;
  PRInt8        mNumBytesPixel;
  PRUint8       mFlags;             // flags set by ImageUpdated
  PRInt8        mDepth;             // bits per pixel
};

#endif
