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

#ifndef nsImagePh_h___
#define nsImagePh_h___

#include <Pt.h>
#include "nsIImage.h"
#include "nsRect.h"

class nsImagePh : public nsIImage {
public:
  nsImagePh();
  virtual ~nsImagePh();

  NS_DECL_ISUPPORTS

  /**
  @see nsIImage.h
  */
  virtual PRInt32     GetBytesPix()       { return mNumBytesPixel; }
  virtual PRInt32     GetHeight() { return mHeight; }
  virtual PRInt32     GetWidth() { return mWidth; }

  virtual PRUint8*    GetBits() {  return mImageBits; }

  virtual void*       GetBitInfo() { return nsnull; }

  virtual PRBool      GetIsRowOrderTopToBottom() { return PR_TRUE; }
  virtual PRInt32     GetLineStride() { return mRowBytes; }

	inline
  NS_IMETHODIMP       SetDecodedRect(PRInt32 x1, PRInt32 y1, PRInt32 x2, PRInt32 y2)
		{
		mDecodedX1 = x1;
		mDecodedY1 = y1;
		mDecodedX2 = x2;
		mDecodedY2 = y2;
		return NS_OK;
		}

  virtual PRInt32     GetDecodedX1() { return mDecodedX1;}
  virtual PRInt32     GetDecodedY1() { return mDecodedY1;}
  virtual PRInt32     GetDecodedX2() { return mDecodedX2;}
  virtual PRInt32     GetDecodedY2() { return mDecodedY2;}

  NS_IMETHOD     SetNaturalWidth(PRInt32 naturalwidth) { mNaturalWidth= naturalwidth; return NS_OK;}
  NS_IMETHOD     SetNaturalHeight(PRInt32 naturalheight) { mNaturalHeight= naturalheight; return NS_OK;}
  virtual PRInt32     GetNaturalWidth() {return mNaturalWidth; }
  virtual PRInt32     GetNaturalHeight() {return mNaturalHeight; }


  virtual nsColorMap* GetColorMap() { return nsnull; }

  NS_IMETHOD          Draw(nsIRenderingContext &aContext, nsIDrawingSurface* aSurface, PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight);
  NS_IMETHOD          Draw(nsIRenderingContext &aContext, nsIDrawingSurface* aSurface, PRInt32 aSX, PRInt32 aSY, PRInt32 aSWidth,  \
  						PRInt32 aSHeight, PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight);

  NS_IMETHOD DrawToImage(nsIImage* aDstImage, nscoord aDX, nscoord aDY,
             nscoord aDWidth, nscoord aDHeight);

  NS_IMETHOD 		  DrawTile(nsIRenderingContext &aContext, nsIDrawingSurface* aSurface,
                        PRInt32 aSXOffset, PRInt32 aSYOffset, PRInt32 aPadX, PRInt32 aPadY,
												const nsRect &aTileRect);

  virtual void        ImageUpdated(nsIDeviceContext *aContext, PRUint8 aFlags, nsRect *aUpdateRect)
		{
		PRInt32 y = aUpdateRect->YMost();
		PRInt32 x = aUpdateRect->XMost();
		if( y > mDecodedY2 ) mDecodedY2 = y;
		if( x > mDecodedX2 ) mDecodedX2 = x;
		mDirtyFlags = aFlags;
		mPhImage.size.h = mDecodedY2;
		}

  virtual nsresult    Init(PRInt32 aWidth, PRInt32 aHeight, PRInt32 aDepth, nsMaskRequirements aMaskRequirements);
//  virtual PRBool      IsOptimized() { return mIsOptimized; }

  virtual nsresult    Optimize(nsIDeviceContext* aContext);

  virtual PRBool      GetHasAlphaMask() { return mAlphaBits != nsnull; } 
  virtual PRUint8*    GetAlphaBits() { return mAlphaBits; }
  virtual PRInt32     GetAlphaWidth() { return mAlphaWidth; }
  virtual PRInt32     GetAlphaHeight() { return mAlphaHeight; }
  virtual PRInt32     GetAlphaLineStride() { return mAlphaRowBytes; }
  virtual nsIImage*   DuplicateImage() { return nsnull; }
  
  /**
   * Get the alpha depth for the image mask
   * @update - lordpixel 2001/05/16
   * @return  the alpha mask depth for the image, ie, 0, 1 or 8
   */
  virtual PRInt8 GetAlphaDepth() { return mAlphaDepth; }  
  virtual void  	  MoveAlphaMask(PRInt32 aX, PRInt32 aY) { }

  inline
	NS_IMETHODIMP 	  LockImagePixels(PRBool aMaskPixels) { return NS_OK; }

	inline
  NS_IMETHODIMP 	  UnlockImagePixels(PRBool aMaskPixels) { return NS_OK; }

private:
  void ComputePaletteSize(PRIntn nBitCount);

private:
  PRInt32             mWidth;
  PRInt32             mHeight;
  PRInt32             mDepth;
  PRInt32             mRowBytes;          // number of bytes per row
  PRUint8*            mImageBits;         // starting address of DIB bits

  PRInt8              mNumBytesPixel;     // number of bytes per pixel
  PRUint8             mImageFlags;
	PRUint8							mDirtyFlags;

  PRInt32             mDecodedX1;       //Keeps track of what part of image
  PRInt32             mDecodedY1;       // has been decoded.
  PRInt32             mDecodedX2; 
  PRInt32             mDecodedY2;    

  // alpha layer members
  PRUint8             *mAlphaBits;        // alpha layer if we made one
  PRInt8              mAlphaDepth;        // alpha layer depth
  PRInt16             mAlphaRowBytes;         // number of bytes per row in the image for the alpha
  PRInt16             mAlphaWidth;        // alpha layer width
  PRInt16             mAlphaHeight;       // alpha layer height
  PhImage_t           mPhImage;
  PhImage_t           *mPhImageZoom;			// the zoomed version of mPhImage
	PRInt32							mDecodedY2_when_scaled;

  PRInt32 mNaturalWidth;
  PRInt32 mNaturalHeight;
};
#endif
