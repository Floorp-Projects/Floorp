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

#ifndef nsImage_h___
#define nsImage_h___

#include "nsIImage.h"

class nsImage : public nsIImage
{

 public:

 private:
  PRInt8              mNumBytesColor;     // number of bytes per color
  PRInt16             mNumPalleteColors;  // either 8 or 0
  PRInt32             mSizeImage;         // number of bits, not BITYMAPINFOHEADER
  PRInt32             mRowBytes;          // number of bytes per row
  PRUint8             *mColorTable;       // color table for the bitmap
  LPBYTE              mImageBits;         // starting address of DIB bits
  LPBYTE              mAlphaBits;         // alpha layer if we made one
  PRBool              mIsOptimized;       // Have we turned our DIB into a GDI?
  nsColorMap          *mColorMap;         // Redundant with mColorTable, but necessary
                                          // for Set/GetColorMap

#ifdef XP_PC
  HPALETTE            mHPalette;
  HBITMAP             mHBitmap;        // the GDI bitmap
  LPBITMAPINFOHEADER  mBHead;          // BITMAPINFOHEADER
#endif

public:
  nsImage();
  ~nsImage();


#ifdef XP_PC
  virtual   BOOL    Draw(void *aDrawPort,PRInt32 aX,PRInt32 aY,PRInt32 aWidth,PRInt32 aHeight); // until we implement CreateDibSection
  virtual PRInt32   GetHeight() { return mBHead->biHeight;}
          int       GetSizeHeader(){return sizeof(BITMAPINFOHEADER)+sizeof(RGBQUAD) * mNumPalleteColors;}
  virtual PRInt32   GetWidth() { return mBHead->biWidth;}
          BOOL      SetSystemPalette(HDC* aHdc);
          UINT      UsePalette(HDC* aHdc, BOOL bBackground = FALSE);
#endif

          void      CleanUp();
  virtual PRUint8*  GetAlphaBits(){return mAlphaBits;}
  virtual PRInt32   GetAlphaLineStride() {return mBHead->biWidth;}
  virtual PRUint8*  GetBits() {return mImageBits;}
  virtual PRInt32   GetLineStride() {return mRowBytes;}
          int       GetSizeImage() {return mSizeImage;}
  virtual void      Init(PRInt32 aWidth, PRInt32 aHeight, PRInt32 aDepth,nsMaskRequirements aMaskRequirements);
  virtual PRBool    IsOptimized() { return mIsOptimized;}
          BOOL      MakePalette();
  virtual nsresult  Optimize(void *aDrawPort);
          void      TestFillBits();

  // methods not complete
  virtual nsColorMap* GetColorMap();
  virtual void ImageUpdated(PRUint8 aFlags, nsRect *aUpdateRect);

  NS_DECL_ISUPPORTS

private:
#ifdef XP_PC
  void      ComputePaletteSize(int aBitCount);
  void      ComputeMetrics();
#endif

};

#endif
