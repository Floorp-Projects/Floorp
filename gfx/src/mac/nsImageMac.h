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

#ifndef nsImageMac_h___
#define nsImageMac_h___

#include "nsIImage.h"
#include <QDOffscreen.h>

class nsImageMac : public nsIImage
{
public:
  nsImageMac();
  ~nsImageMac();

  NS_DECL_ISUPPORTS

  /**
  @see nsIImage.h
  */
  virtual PRInt32     GetBytesPix()       { return mThePixelmap.cmpCount; }
  virtual PRInt32     GetHeight()         { return mHeight;}
  virtual PRInt32     GetWidth()          { return mWidth; }
  virtual PRUint8*    GetBits() ;          //{ return mImageBits; }
  virtual void*       GetBitInfo()        { return nsnull; }
  virtual PRBool      GetIsRowOrderTopToBottom() { return mIsTopToBottom; }
  virtual PRInt32     GetLineStride()     {return mRowBytes; }
  NS_IMETHOD Draw(nsIRenderingContext &aContext, nsDrawingSurface aSurface, PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight);
  NS_IMETHOD Draw(nsIRenderingContext &aContext, nsDrawingSurface aSurface, PRInt32 aSX, PRInt32 aSY, PRInt32 aSWidth, PRInt32 aSHeight,
                  PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight);
  virtual nsColorMap* GetColorMap() {return nsnull;}
  virtual void ImageUpdated(nsIDeviceContext *aContext, PRUint8 aFlags, nsRect *aUpdateRect);
  virtual nsresult    Init(PRInt32 aWidth, PRInt32 aHeight, PRInt32 aDepth, nsMaskRequirements aMaskRequirements);
  virtual PRBool      IsOptimized()       { return false; }
  virtual nsresult    Optimize(nsIDeviceContext* aContext);
  virtual PRUint8*    GetAlphaBits()      { return mAlphaBits; }
  virtual PRInt32     GetAlphaWidth()   { return mAlphaWidth;}
  virtual PRInt32     GetAlphaHeight()   {return mAlphaHeight;}
  virtual PRInt32     GetAlphaLineStride(){ return mARowBytes; }
  virtual nsIImage*   DuplicateImage() {return(nsnull);}

  void AllocConvertedBits(PRUint32 aSize);

  /** 
    * Return the image size of the Device Independent Bitmap(DIB).
    * @return size of image in bytes
    */
  PRIntn      GetSizeImage(){ return mSizeImage; }

  /** 
   * Make a palette for the DIB.
   * @return true or false if the palette was created
   */
  PRBool      MakePalette();

  /** 
   * Calculate the number of bytes spaned for this image for a given width
   * @param aWidth is the width to calculate the number of bytes for
   * @return the number of bytes in this span
   */
  PRInt32  					CalcBytesSpan(PRUint32  aWidth,PRUint32	aDepth);
  virtual void  		SetAlphaLevel(PRInt32 aAlphaLevel) {}
  virtual PRInt32 	GetAlphaLevel() {return(0);}
  virtual void  		MoveAlphaMask(PRInt32 aX, PRInt32 aY) {}

private:
  PixMap			mThePixelmap;
  PRInt32			mWidth;
  PRInt32			mHeight;
  PRInt32			mSizeImage;
  PRInt32           mRowBytes;          // number of bytes per row
  PRUint8*          mImageBits;         // starting address of the bits

	
  nsColorMap*       mColorMap;          // Redundant with mColorTable, but necessary
    
  // alpha layer members
  BitMap			*mAlphaPix;			// the alpha level pixel map
  PRBool 			mIsTopToBottom;
  PRUint8			*mAlphaBits;        // the bits to set for the mask
  PRInt32			mARowBytes;			// rowbytes for the alpha layer
  PRInt8            mAlphaDepth;        // alpha layer depth
  PRInt16			mAlphaWidth;        // alpha layer width
  PRInt16			mAlphaHeight;       // alpha layer height
  nsPoint			mLocation;          // alpha mask location
  PRInt8			mImageCache;        // place to save off the old image for fast animation
  PRInt16			mAlphaLevel;        // an alpha level every pixel uses
};

#endif
