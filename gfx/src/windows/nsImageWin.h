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
  virtual PRUint8*    GetBits()           { return mImageBits; }
  virtual PRInt32     GetLineStride()     {return mRowBytes; }
  virtual PRBool      Draw(nsIRenderingContext &aContext, nsDrawingSurface aSurface, PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight);
  virtual PRBool      Draw(nsIRenderingContext &aContext, nsDrawingSurface aSurface, PRInt32 aSX, PRInt32 aSY, PRInt32 aSWidth, PRInt32 aSHeight,
                                  PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight);
  virtual nsColorMap* GetColorMap() {return mColorMap;}
  virtual void        ImageUpdated(nsIDeviceContext *aContext, PRUint8 aFlags, nsRect *aUpdateRect);
  virtual nsresult    Init(PRInt32 aWidth, PRInt32 aHeight, PRInt32 aDepth, nsMaskRequirements aMaskRequirements);
  virtual PRBool      IsOptimized()       { return mIsOptimized; }
  virtual nsresult    Optimize(nsDrawingSurface aSurface);
  virtual PRUint8*    GetAlphaBits()      { return mAlphaBits; }
  virtual PRInt32     GetAlphaWidth()   { return mAlphaWidth;}
  virtual PRInt32     GetAlphaHeight()   {return mAlphaHeight;}
  virtual PRInt32     GetAlphaXLoc() {return mLocation.x;}
  virtual PRInt32     GetAlphaYLoc() {return mLocation.y;}
  virtual PRInt32     GetAlphaLineStride(){ return mARowBytes; }
  virtual void        CompositeImage(nsIImage *aTheImage,nsPoint *aULLocation,nsBlendQuality aQuality);
  virtual nsIImage*   DuplicateImage();

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

  virtual void  SetAlphaLevel(PRInt32 aAlphaLevel) {mAlphaLevel=aAlphaLevel;}

  virtual PRInt32 GetAlphaLevel() {return(mAlphaLevel);}

  void MoveAlphaMask(PRInt32 aX, PRInt32 aY){mLocation.x=aX;mLocation.y=aY;}

private:

  /** 
   * Blend two 24 bit image arrays using an 8 bit alpha mask
   * @param aNumlines  Number of lines to blend
   * @param aNumberBytes Number of bytes per line to blend
   * @param aSImage Pointer to beginning of the source bytes
   * @param aDImage Pointer to beginning of the destination bytes
   * @param aMImage Pointer to beginning of the mask bytes
   * @param aSLSpan number of bytes per line for the source bytes
   * @param aDLSpan number of bytes per line for the destination bytes
   * @param aMLSpan number of bytes per line for the Mask bytes
   * @param aBlendQuality The quality of this blend, this is for tweening if neccesary
   */
  void Do24BlendWithMask(PRInt32 aNumlines,PRInt32 aNumbytes,PRUint8 *aSImage,PRUint8 *aDImage,
                PRUint8 *aMImage,PRInt32 aSLSpan,PRInt32 aDLSpan,PRInt32 aMLSpan,nsBlendQuality aBlendQuality);

  /** 
   * Blend two 24 bit image arrays using a passed in blend value
   * @param aNumlines  Number of lines to blend
   * @param aNumberBytes Number of bytes per line to blend
   * @param aSImage Pointer to beginning of the source bytes
   * @param aDImage Pointer to beginning of the destination bytes
   * @param aMImage Pointer to beginning of the mask bytes
   * @param aSLSpan number of bytes per line for the source bytes
   * @param aDLSpan number of bytes per line for the destination bytes
   * @param aMLSpan number of bytes per line for the Mask bytes
   * @param aBlendQuality The quality of this blend, this is for tweening if neccesary
   */
  void Do24Blend(PRUint8 aBlendVal,PRInt32 aNumlines,PRInt32 aNumbytes,PRUint8 *aSImage,PRUint8 *aDImage,
                PRInt32 aSLSpan,PRInt32 aDLSpan,nsBlendQuality aBlendQuality);


    /** 
   * Blend two 8 bit image arrays using an 8 bit alpha mask
   * @param aNumlines  Number of lines to blend
   * @param aNumberBytes Number of bytes per line to blend
   * @param aSImage Pointer to beginning of the source bytes
   * @param aDImage Pointer to beginning of the destination bytes
   * @param aMImage Pointer to beginning of the mask bytes
   * @param aSLSpan number of bytes per line for the source bytes
   * @param aDLSpan number of bytes per line for the destination bytes
   * @param aMLSpan number of bytes per line for the Mask bytes
   * @param aBlendQuality The quality of this blend, this is for tweening if neccesary
   */
  void Do8BlendWithMask(PRInt32 aNumlines,PRInt32 aNumbytes,PRUint8 *aSImage,PRUint8 *aDImage,
                PRUint8 *aMImage,PRInt32 aSLSpan,PRInt32 aDLSpan,PRInt32 aMLSpan,nsBlendQuality aBlendQuality);

  /** 
   * Blend two 8 bit image arrays using a passed in blend value
   * @param aNumlines  Number of lines to blend
   * @param aNumberBytes Number of bytes per line to blend
   * @param aSImage Pointer to beginning of the source bytes
   * @param aDImage Pointer to beginning of the destination bytes
   * @param aMImage Pointer to beginning of the mask bytes
   * @param aSLSpan number of bytes per line for the source bytes
   * @param aDLSpan number of bytes per line for the destination bytes
   * @param aMLSpan number of bytes per line for the Mask bytes
   * @param aBlendQuality The quality of this blend, this is for tweening if neccesary
   */
  void Do8Blend(PRUint8 aBlendVal,PRInt32 aNumlines,PRInt32 aNumbytes,PRUint8 *aSImage,PRUint8 *aDImage,
                PRInt32 aSLSpan,PRInt32 aDLSpan,nsBlendQuality aBlendQuality);


  /** 
   * Calculate the information we need to do a blend
   * @param aNumlines  Number of lines to blend
   * @param aNumberBytes Number of bytes per line to blend
   * @param aSImage Pointer to beginning of the source bytes
   * @param aDImage Pointer to beginning of the destination bytes
   * @param aMImage Pointer to beginning of the mask bytes
   * @param aSLSpan number of bytes per line for the source bytes
   * @param aDLSpan number of bytes per line for the destination bytes
   * @param aMLSpan number of bytes per line for the Mask bytes
   */
  PRBool CalcAlphaMetrics(nsIImage *aTheImage,nsPoint *aULLocation,PRInt32 *aNumlines,
                PRInt32 *aNumbytes,PRUint8 **aSImage,PRUint8 **aDImage,
                PRUint8 **aMImage,PRInt32 *SLSpan,PRInt32 *aDLSpan,PRInt32 *aMLSpan);


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
   * Calculate the amount of memory needed for the initialization of the pixelmap
   */
  void ComputeMetrics();

  PRUint8 PaletteMatch(PRUint8 r, PRUint8 g, PRUint8 b);

  PRInt8              mNumBytesPixel;     // number of bytes per pixel
  PRInt16             mNumPalleteColors;  // either 8 or 0
  PRInt32             mSizeImage;         // number of bytes
  PRInt32             mRowBytes;          // number of bytes per row
  PRUint8             *mColorTable;       // color table for the bitmap
  PRUint8             *mImageBits;         // starting address of DIB bits
  PRBool              mIsOptimized;       // Have we turned our DIB into a GDI?
  nsColorMap          *mColorMap;         // Redundant with mColorTable, but necessary
    
  // alpha layer members
  PRUint8             *mAlphaBits;         // alpha layer if we made one
  PRInt8              mAlphaDepth;         // alpha layer depth
  PRInt16             mARowBytes;
  PRInt16             mAlphaWidth;        // alpha layer width
  PRInt16             mAlphaHeight;       // alpha layer height
  nsPoint             mLocation;          // alpha mask location
  PRInt8              mImageCache;        // place to save off the old image for fast animation
  PRInt16             mAlphaLevel;        // an alpha level every pixel uses

  // for Set/GetColorMap
  HPALETTE            mHPalette;
  HBITMAP             mHBitmap;           // the GDI bitmap
  LPBITMAPINFOHEADER  mBHead;             // BITMAPINFOHEADER
};

#endif
