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

#ifndef nsImageMotif_h___
#define nsImageMotif_h___

#include "nsIImage.h"

#include "X11/Xlib.h"
#include "X11/Xutil.h"

/* taken from ../nsBlender.h */
typedef enum
{
  nsLowQual = 0,
  nsLowMedQual,
  nsMedQual,
  nsHighMedQual,
  nsHighQual
} nsBlendQuality;

class nsImageMotif : public nsIImage
{
public:
  nsImageMotif();
  virtual ~nsImageMotif();

  NS_DECL_ISUPPORTS

  /**
  @see nsIImage.h
  */
  virtual PRInt32     GetBytesPix()       { return mNumBytesPixel; }
  virtual PRInt32     GetHeight()         { return mHeight; }
  virtual PRInt32     GetWidth()          { return mWidth; }
  virtual PRUint8*    GetBits()           { return mImageBits; }
  virtual void*       GetBitInfo()        { return nsnull; }
  virtual PRBool      GetIsRowOrderTopToBottom() { return mIsTopToBottom; }
  virtual PRInt32     GetLineStride()     {return mRowBytes; }

  NS_IMETHOD     SetNaturalWidth(PRInt32 naturalwidth) { mNaturalWidth= naturalwidth; return NS_OK;}
  NS_IMETHOD     SetNaturalHeight(PRInt32 naturalheight) { mNaturalHeight= naturalheight; return NS_OK;}
  virtual PRInt32     GetNaturalWidth() {return mNaturalWidth; }
  virtual PRInt32     GetNaturalHeight() {return mNaturalHeight; }

  NS_IMETHOD          SetDecodedRect(PRInt32 x1, PRInt32 y1, PRInt32 x2, PRInt32 y2);        
  virtual PRInt32     GetDecodedX1() { return mDecodedX1;}
  virtual PRInt32     GetDecodedY1() { return mDecodedY1;}
  virtual PRInt32     GetDecodedX2() { return mDecodedX2;}
  virtual PRInt32     GetDecodedY2() { return mDecodedY2;}

  NS_IMETHOD Draw(nsIRenderingContext &aContext, nsDrawingSurface aSurface, PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight);
  NS_IMETHOD Draw(nsIRenderingContext &aContext, nsDrawingSurface aSurface, PRInt32 aSX, PRInt32 aSY, PRInt32 aSWidth, PRInt32 aSHeight,
                  PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight);
  virtual nsColorMap* GetColorMap() {return mColorMap;}
  virtual void ImageUpdated(nsIDeviceContext *aContext, PRUint8 aFlags, nsRect *aUpdateRect);
  virtual nsresult    Init(PRInt32 aWidth, PRInt32 aHeight, PRInt32 aDepth, nsMaskRequirements aMaskRequirements);
  virtual PRBool      IsOptimized()       { return (mImage!=nsnull); }

  virtual  nsresult   BuildImage(nsDrawingSurface aDrawingSurface);
  virtual nsresult    Optimize(nsIDeviceContext* aContext);
  virtual PRBool      GetHasAlphaMask()   { return mAlphaBits != nsnull; }        
  virtual PRUint8*    GetAlphaBits()      { return mAlphaBits; }
  virtual PRInt32     GetAlphaWidth()   { return 0;}
  virtual PRInt32     GetAlphaHeight()   {return 0;}
  virtual PRInt32     GetAlphaXLoc() {return 0;}
  virtual PRInt32     GetAlphaYLoc() {return 0;}
  virtual PRInt32     GetAlphaLineStride(){ return 0; }
  virtual void        CompositeImage(nsIImage *aTheImage,nsPoint *aULLocation,nsBlendQuality aQuality);
  virtual nsIImage*   DuplicateImage() {return(nsnull);}

  void AllocConvertedBits(PRUint32 aSize);

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
  virtual void  SetAlphaLevel(PRInt32 /* aAlphaLevel */) {}
  virtual PRInt32 GetAlphaLevel() {return(0);}
  /**
   * Get the alpha depth for the image mask
   * @update - lordpixel 2001/05/16
   * @return  the alpha mask depth for the image, ie, 0, 1 or 8
   */
  virtual PRInt8 GetAlphaDepth() {return(mAlphaDepth);}  
  virtual void  MoveAlphaMask(PRInt32 /* aX */, PRInt32 /* aY */) {}

  NS_IMETHOD   LockImagePixels(PRBool aMaskPixels);
  NS_IMETHOD   UnlockImagePixels(PRBool aMaskPixels);    

private:
  void CreateImage(nsDrawingSurface aSurface);
  void ConvertImage(nsDrawingSurface aSurface);

  /** 
   * Calculate the amount of memory needed for the initialization of the image
   */
  void ComputMetrics();
  void ComputePaletteSize(PRIntn nBitCount);


private:
  PRBool     mStaticImage;
  PRInt32    mWidth;
  PRInt32    mHeight;
  PRInt32    mDepth;       // bits per pixel
  PRInt32    mOriginalDepth;       // bits per pixel
  PRInt32    mRowBytes;
  PRInt32    mOriginalRowBytes;
  Pixmap     mThePixMap;
  PRUint8    *mImageBits;
  PRUint8    *mConvertedBits;
  PRBool     mConverted;
  PRUint8    *mBitsForCreate;
  PRInt32    mSizeImage;
  PRBool     mIsTopToBottom;
  XImage     *mImage ;
  nsColorMap *mColorMap;
  PRInt16     mNumPalleteColors;
  PRInt8      mNumBytesPixel;

  PRInt32	  mNaturalWidth;
  PRInt32	  mNaturalHeight;

  PRInt32             mDecodedX1;       //Keeps track of what part of image
  PRInt32             mDecodedY1;       // has been decoded.
  PRInt32             mDecodedX2; 
  PRInt32             mDecodedY2;    

 // alpha layer members
  PRUint8    *mAlphaBits;
  PRInt8     mAlphaDepth;        // alpha layer depth
  PRInt16    mARowBytes;
  PRInt16    mAlphaWidth;        // alpha layer width
  PRInt16    mAlphaHeight;       // alpha layer height
  nsPoint    mLocation;          // alpha mask location


};

#endif // nsImageMotif_h___
