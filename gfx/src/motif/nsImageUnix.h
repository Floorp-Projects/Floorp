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

#ifndef nsImageUnix_h___
#define nsImageUnix_h___

#include "nsIImage.h"

#include "X11/Xlib.h"
#include "X11/Xutil.h"

class nsImageUnix : public nsIImage
{
public:
  nsImageUnix();
  ~nsImageUnix();

  NS_DECL_ISUPPORTS

  /**
  @see nsIImage.h
  */
  virtual PRInt32     GetHeight()         { return mWidth; }
  virtual PRInt32     GetWidth()          { return mDepth; }
  virtual PRUint8*    GetBits()           { return nsnull; }
  virtual PRInt32     GetLineStride()     {return 0; }
  virtual PRBool      Draw(nsIRenderingContext &aContext, nsDrawingSurface aSurface, PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight);
  virtual PRBool      Draw(nsIRenderingContext &aContext, nsDrawingSurface aSurface, PRInt32 aSX, PRInt32 aSY, PRInt32 aSWidth, PRInt32 aSHeight,
                                  PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight);
  virtual nsColorMap* GetColorMap() {return nsnull;}
  virtual void ImageUpdated(nsIDeviceContext *aContext, PRUint8 aFlags, nsRect *aUpdateRect);
  virtual nsresult    Init(PRInt32 aWidth, PRInt32 aHeight, PRInt32 aDepth, nsMaskRequirements aMaskRequirements);
  virtual PRBool      IsOptimized()       { return PR_FALSE; }
  virtual nsresult    Optimize(nsDrawingSurface aSurface);
  virtual PRUint8*    GetAlphaBits()      { return nsnull; }
  virtual PRInt32     GetAlphaWidth()   { return 0;}
  virtual PRInt32     GetAlphaHeight()   {return 0;}
  virtual PRInt32     GetAlphaXLoc() {return 0;}
  virtual PRInt32     GetAlphaYLoc() {return 0;}
  virtual PRInt32     GetAlphaLineStride(){ return 0; }
  virtual void CompositeImage(nsIImage *aTheImage,nsPoint *aULLocation,nsBlendQuality aQuality);
  
  virtual nsIImage*   DuplicateImage();

  /** 
    * Return the image size of the Device Independent Bitmap(DIB).
    * @return size of image in bytes
    */
  PRIntn      GetSizeImage(){ return 0; }

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
  PRInt32  CalcBytesSpan(PRUint32  aWidth);

  PRBool  SetAlphaMask(nsIImage *aTheMask);

  virtual void  SetAlphaLevel(PRInt32 aAlphaLevel) {mAlphaLevel=aAlphaLevel;}

  virtual PRInt32 GetAlphaLevel() {return(mAlphaLevel);}

  void MoveAlphaMask(PRInt32 aX, PRInt32 aY){}

private:
  void CreateImage(nsIDeviceContext * aDeviceContext);

private:
  PRInt16             mAlphaLevel;        // an alpha level every pixel uses

  PRInt32 mWidth;
  PRInt32 mDepth;
  PRInt32 mHeight;
  nsMaskRequirements mMaskReq;

  XImage * mImage ;

};

#endif
