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

#ifndef nsImagePh_h___
#define nsImagePh_h___

#include <Pt.h>
#include "nsIImage.h"

class nsImagePh : public nsIImage {
public:
  nsImagePh();
  virtual ~nsImagePh();

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

  NS_IMETHOD          SetDecodedRect(PRInt32 x1, PRInt32 y1, PRInt32 x2, PRInt32 y2);        
  virtual PRInt32     GetDecodedX1() { return mDecodedX1;}
  virtual PRInt32     GetDecodedY1() { return mDecodedY1;}
  virtual PRInt32     GetDecodedX2() { return mDecodedX2;}
  virtual PRInt32     GetDecodedY2() { return mDecodedY2;}

  NS_IMETHOD     SetNaturalWidth(PRInt32 naturalwidth) { mNaturalWidth= naturalwidth; return NS_OK;}
  NS_IMETHOD     SetNaturalHeight(PRInt32 naturalheight) { mNaturalHeight= naturalheight; return NS_OK;}
  virtual PRInt32     GetNaturalWidth() {return mNaturalWidth; }
  virtual PRInt32     GetNaturalHeight() {return mNaturalHeight; }


  virtual nsColorMap* GetColorMap();
  NS_IMETHOD          Draw(nsIRenderingContext &aContext, nsDrawingSurface aSurface, PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight);
  NS_IMETHOD          Draw(nsIRenderingContext &aContext, nsDrawingSurface aSurface, PRInt32 aSX, PRInt32 aSY, PRInt32 aSWidth,  \
  						PRInt32 aSHeight, PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight);
#ifdef USE_IMG2
  NS_IMETHOD DrawToImage(nsIImage* aDstImage, nscoord aDX, nscoord aDY,
             nscoord aDWidth, nscoord aDHeight);
#endif
  NS_IMETHOD 		DrawTile(nsIRenderingContext &aContext, nsDrawingSurface aSurface, 
  						nsRect &aSrcRect, nsRect &aTileRect);

  NS_IMETHOD 		  DrawTile(nsIRenderingContext &aContext, nsDrawingSurface aSurface,
                        PRInt32 aSXOffset, PRInt32 aSYOffset, const nsRect &aTileRect);

  virtual void        ImageUpdated(nsIDeviceContext *aContext, PRUint8 aFlags, nsRect *aUpdateRect);
  virtual nsresult    Init(PRInt32 aWidth, PRInt32 aHeight, PRInt32 aDepth, nsMaskRequirements aMaskRequirements);
  virtual PRBool      IsOptimized();

  virtual nsresult    Optimize(nsIDeviceContext* aContext);

  virtual PRBool      GetHasAlphaMask()     { return mAlphaBits != nsnull; } 
  virtual PRUint8*    GetAlphaBits();
  virtual PRInt32     GetAlphaWidth();
  virtual PRInt32     GetAlphaHeight();
  virtual PRInt32     GetAlphaLineStride();
  virtual nsIImage*   DuplicateImage();
  
  virtual void  	  SetAlphaLevel(PRInt32 aAlphaLevel);
  virtual PRInt32 	  GetAlphaLevel();
  /**
   * Get the alpha depth for the image mask
   * @update - lordpixel 2001/05/16
   * @return  the alpha mask depth for the image, ie, 0, 1 or 8
   */
  virtual PRInt8 GetAlphaDepth() {return(mAlphaDepth);}  
  virtual void  	  MoveAlphaMask(PRInt32 aX, PRInt32 aY);

  NS_IMETHOD   		  LockImagePixels(PRBool aMaskPixels);
  NS_IMETHOD   		  UnlockImagePixels(PRBool aMaskPixels);    

private:
  void ComputePaletteSize(PRIntn nBitCount);
  PRUint8 * CreateSRamImage(PRUint32 size);
  PRBool DestroySRamImage(PRUint8 *ptr);
  

private:
  PRInt32             mWidth;
  PRInt32             mHeight;
  PRInt32             mDepth;
  PRInt32             mRowBytes;          // number of bytes per row
  PRUint8*            mImageBits;         // starting address of DIB bits
  PRUint8           *mConvertedBits;      // NEW
  PRInt32             mSizeImage;         // number of bytes
  PRBool              mIsTopToBottom;  
  PRBool			  mIsOptimized;
  PRInt8              mNumBytesPixel;     // number of bytes per pixel
  PRInt16             mNumPaletteColors;  // either 8 or 0

  PRInt32             mDecodedX1;       //Keeps track of what part of image
  PRInt32             mDecodedY1;       // has been decoded.
  PRInt32             mDecodedX2; 
  PRInt32             mDecodedY2;    

  // alpha layer members
  PRUint8             *mAlphaBits;        // alpha layer if we made one
  PRInt8              mAlphaDepth;        // alpha layer depth
  PRInt16             mAlphaRowBytes;         // number of bytes per row in the image for tha alpha
  PRInt16             mAlphaWidth;        // alpha layer width
  PRInt16             mAlphaHeight;       // alpha layer height
  PRInt8              mImageCache;        // place to save off the old image for fast animation
  PRInt16             mAlphaLevel;        // an alpha level every pixel uses
  PhImage_t           mPhImage;
  PhImage_t           *mPhImageZoom;			// the zoomed version of mPhImage
  PdOffscreenContext_t *mPhImageCache;	  // Cache for the image offscreen

  PRUint8             mFlags;             // flags set by ImageUpdated
  PRUint8             mImageFlags;             // flags set by ImageUpdated
  PRInt32 mNaturalWidth;
  PRInt32 mNaturalHeight;
};
#endif
