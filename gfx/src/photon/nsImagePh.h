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

#ifndef nsImagePh_h___
#define nsImagePh_h___

#include <Pt.h>
#include "nsIImage.h"

class nsImagePh : public nsIImage{
public:
  nsImagePh();
  ~nsImagePh();

  NS_DECL_ISUPPORTS

  /**
  @see nsIImage.h
  */
  virtual PRInt32     GetBytesPix()       { return mNumBytesPixel; }
  virtual PRInt32     GetHeight()         {  }
  virtual PRInt32     GetWidth()          {  }
  virtual PRUint8*    GetBits()           { return mImageBits; }
  virtual PRInt32     GetLineStride()     {return mRowBytes; }
  NS_IMETHOD          Draw(nsIRenderingContext &aContext, nsDrawingSurface aSurface, PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight);
  NS_IMETHOD          Draw(nsIRenderingContext &aContext, nsDrawingSurface aSurface, PRInt32 aSX, PRInt32 aSY, PRInt32 aSWidth, PRInt32 aSHeight,
                      PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight);
  virtual nsColorMap* GetColorMap() {return mColorMap;}
  virtual void        ImageUpdated(nsIDeviceContext *aContext, PRUint8 aFlags, nsRect *aUpdateRect);
  virtual nsresult    Init(PRInt32 aWidth, PRInt32 aHeight, PRInt32 aDepth, nsMaskRequirements aMaskRequirements);
  virtual PRBool      IsOptimized()       { return mIsOptimized; }
  virtual nsresult    Optimize(nsIDeviceContext* aContext);
  virtual PRUint8*    GetAlphaBits()      { return mAlphaBits; }
  virtual PRInt32     GetAlphaWidth()   { return mAlphaWidth;}
  virtual PRInt32     GetAlphaHeight()   {return mAlphaHeight;}
  virtual PRInt32     GetAlphaLineStride(){ return mARowBytes; }

  /** 
   * Return the header size of the Device Independent Bitmap(DIB).
   * @return size of header in bytes
   */
  PRIntn      GetSizeHeader(){return 0;}

  /** 
   * Return the image size of the Device Independent Bitmap(DIB).
   * @update dc - 10/29/98
   * @return size of image in bytes
   */
  PRIntn      GetSizeImage(){ return mSizeImage; }

  /** 
   * Calculate the number of bytes spaned for this image for a given width
   * @param aWidth is the width to calculate the number of bytes for
   * @return the number of bytes in this span
   */
  PRInt32  CalcBytesSpan(PRUint32  aWidth);

  virtual void  SetAlphaLevel(PRInt32 aAlphaLevel) {mAlphaLevel=aAlphaLevel;}

  /** 
   * Get the alpha level assigned.
   * @update dc - 10/29/98
   * @return The alpha level from 0 to 1
   */
  virtual PRInt32 GetAlphaLevel() {return(mAlphaLevel);}

  /** 
   * Get the DIB specific informations for this bitmap.
   * @update dc - 10/29/98
   * @return VOID
   */
  void* GetBitInfo() {return nsnull;}


private:
  /** 
   * Clean up the memory used nsImagePh.
   * @update dc - 10/29/98
   * @param aCleanUpAll - if True, all the memory used will be released otherwise just clean up the DIB memory
   */
  void CleanUp(PRBool aCleanUpAll);

  /** 
   * Create a Device Dependent bitmap from a drawing surface
   * @update dc - 10/29/98
   * @param aSurface - The drawingsurface to create the DDB from.
   */
  void CreateDDB(nsDrawingSurface aSurface);

  /** 
   * Get an index in the palette that matches as closly as possible the passed in RGB colors
   * @update dc - 10/29/98
   * @param aR - Red component of the color to match
   * @param aG - Green component of the color to match
   * @param aB - Blue component of the color to match
   * @return - The closest palette match
   */
  PRUint8 PaletteMatch(PRUint8 r, PRUint8 g, PRUint8 b);

  PRInt8              mNumBytesPixel;     // number of bytes per pixel
  PRInt16             mNumPaletteColors;  // either 8 or 0
  PRInt32             mSizeImage;         // number of bytes
  PRInt32             mRowBytes;          // number of bytes per row
  PRUint8*            mImageBits;         // starting address of DIB bits
  PRBool              mIsOptimized;       // Have we turned our DIB into a GDI?
  nsColorMap*         mColorMap;          // Redundant with mColorTable, but necessary
    
  // alpha layer members
  PRUint8             *mAlphaBits;        // alpha layer if we made one
  PRInt8              mAlphaDepth;        // alpha layer depth
  PRInt16             mARowBytes;         // number of bytes per row in the image for tha alpha
  PRInt16             mAlphaWidth;        // alpha layer width
  PRInt16             mAlphaHeight;       // alpha layer height
  //nsPoint             mLocation;          // alpha mask location
  PRInt8              mImageCache;        // place to save off the old image for fast animation
  PRInt16             mAlphaLevel;        // an alpha level every pixel uses

};

#endif
