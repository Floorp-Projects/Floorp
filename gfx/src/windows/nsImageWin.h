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

#ifndef nsImageWin_h___
#define nsImageWin_h___

#include <windows.h>
#include "nsIImage.h"

class nsImageWin : public nsIImage
{
public:
  nsImageWin();
  ~nsImageWin();

  NS_DECL_ISUPPORTS

  /**
  @see nsIImage.h
  */
  virtual PRInt32     GetHeight()         { return mBHead->biHeight; }
  virtual PRInt32     GetWidth()          { return mBHead->biWidth; }
  virtual PRUint8*    GetAlphaBits()      { return mAlphaBits; }
  virtual PRInt32     GetAlphaLineStride(){ return mARowBytes; }
  virtual PRUint8*    GetBits()           { return mImageBits; }
  virtual PRInt32     GetLineStride()     {return mRowBytes; }
  virtual PRBool      Draw(nsDrawingSurface aSurface, PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight);
  virtual PRBool      Draw(nsDrawingSurface aSurface, PRInt32 aSX, PRInt32 aSY, PRInt32 aSWidth, PRInt32 aSHeight,
                                  PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight);
  virtual void CompositeImage(nsIImage *aTheImage,nsPoint *aULLocation);
  virtual nsColorMap* GetColorMap() {return mColorMap;}
  virtual void        ImageUpdated(PRUint8 aFlags, nsRect *aUpdateRect);
  virtual nsresult    Init(PRInt32 aWidth, PRInt32 aHeight, PRInt32 aDepth, nsMaskRequirements aMaskRequirements);
  virtual PRBool      IsOptimized()       { return mIsOptimized; }
  virtual nsresult    Optimize(nsDrawingSurface aSurface);

  /** 
   * Return the header size of the Device Independent Bitmap(DIB).
   * @return size of header in bytes
  */
  PRIntn      GetSizeHeader(){return sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * mNumPalleteColors;}

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
   * Set the for this pixelmap to the system palette.
   * @param  aHdc is the DC to get the palette from to use
   * @return true or false if the palette was set
   */
  PRBool      SetSystemPalette(HDC* aHdc);


  /** 
   * Set the palette for an HDC.
   * @param aHDC is the DC to set the palette to
   * @param bBackround tells if the DC is in the background
   * @return true or false if the palette was set
   */
  PRUintn     UsePalette(HDC* aHdc, PRBool bBackground = PR_FALSE);


  /** 
   * Calculate the number of bytes spaned for this image for a given width
   * @param aWidth is the width to calculate the number of bytes for
   * @return the number of bytes in this span
   */
  PRInt32  CalcBytesSpan(PRUint32  aWidth);

  PRBool  SetAlphaMask(nsIImage *aTheMask);

  void MoveAlphaMask(PRInt32 aX, PRInt32 aY){mLocation.x=aX;mLocation.y=aY;}

private:
  /** 
   * Clean up the memory used nsImageWin.
   * @param aCleanUpAll if True, all the memory used will be released otherwise just clean up the DIB memory
   */
  void CleanUp(PRBool aCleanUpAll);

  /** 
   * Calculate the amount of memory needed for the palette
   * @param  aBitCount is the number of bits per pixel
   */
  void ComputePaletteSize(PRIntn aBitCount);

  /**
   * Composite a 24 bit image into another 24 bit image
   * @param aTheImage The image to blend into this image
   * @param aULLocation The upper left coordinate to place the passed in image
   */
  void Comp24to24(nsImageWin *aTheImage,nsPoint *aULLocation);


  /** 
   * Calculate the amount of memory needed for the initialization of the pixelmap
   */
  void ComputeMetrics();

  PRInt8              mNumBytesPixel;     // number of bytes per pixel
  PRInt16             mNumPalleteColors;  // either 8 or 0
  PRInt32             mSizeImage;         // number of bytes
  PRInt32             mRowBytes;          // number of bytes per row
  PRUint8             *mColorTable;       // color table for the bitmap
  LPBYTE              mImageBits;         // starting address of DIB bits
  LPBYTE              mAlphaBits;         // alpha layer if we made one
  PRInt8              mlphaDepth;         // alpha layer depth
  PRInt16             mARowBytes;
  PRInt16             mAlphaWidth;        // alpha layer width
  PRInt16             mAlphaHeight;       // alpha layer height
  nsPoint             mLocation;          // alpha mask location
  PRBool              mIsOptimized;       // Have we turned our DIB into a GDI?
  nsColorMap          *mColorMap;         // Redundant with mColorTable, but necessary
                                          // for Set/GetColorMap
  HPALETTE            mHPalette;
  HBITMAP             mHBitmap;           // the GDI bitmap
  static HDC          mOptimizeDC;        // optimized DC for hbitmap
  LPBITMAPINFOHEADER  mBHead;             // BITMAPINFOHEADER
};

#endif
