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
 *   Daniel Switkin, Mathias Agopian and Sergei Dolgov
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

#include "nsImageBeOS.h"
#include "nsRenderingContextBeOS.h"
#include "nspr.h"
#include <Looper.h>
#include <Bitmap.h>
#include <View.h>

NS_IMPL_ISUPPORTS1(nsImageBeOS, nsIImage)

nsImageBeOS::nsImageBeOS()
  : mImage(nsnull)
  , mImageBits(nsnull)
  , mWidth(0)
  , mHeight(0)
  , mDepth(0)
  , mRowBytes(0)
  , mSizeImage(0)
  , mDecodedX1(PR_INT32_MAX)
  , mDecodedY1(PR_INT32_MAX)
  , mDecodedX2(0)
  , mDecodedY2(0)
  , mAlphaBits(nsnull)
  , mAlphaRowBytes(0)
  , mAlphaDepth(0)
  , mFlags(0)
  , mNumBytesPixel(0)
  , mImageCurrent(PR_FALSE)
  , mOptimized(PR_FALSE)
{
}

nsImageBeOS::~nsImageBeOS() 
{
	if (nsnull != mImage) 
	{
		delete mImage;
		mImage = nsnull;
	}
	if (nsnull != mImageBits) 
	{
		delete [] mImageBits;
		mImageBits = nsnull;
	}
	if (nsnull != mAlphaBits) 
	{
		delete [] mAlphaBits;
		mAlphaBits = nsnull;
	}
}

nsresult nsImageBeOS::Init(PRInt32 aWidth, PRInt32 aHeight, PRInt32 aDepth,
							nsMaskRequirements aMaskRequirements) 
{
	// Assumed: Init only gets called once by gfxIImageFrame
	// Only 24 bit depths are supported for the platform independent bits
	if (24 == aDepth) 
	{
		mNumBytesPixel = 3;
	}
	else 
	{
		NS_ASSERTION(PR_FALSE, "unexpected image depth");
		return NS_ERROR_UNEXPECTED;
	}
	
	mWidth = aWidth;
	mHeight = aHeight;
	mDepth = aDepth;
	mRowBytes = (mWidth * mDepth) >> 5;
	if (((PRUint32)mWidth * mDepth) & 0x1F) 
		mRowBytes++;
	mRowBytes <<= 2;
	mSizeImage = mRowBytes * mHeight;

	mImageBits = new PRUint8[mSizeImage];

	switch (aMaskRequirements) 
	{
		case nsMaskRequirements_kNeeds1Bit:
			mAlphaRowBytes = (aWidth + 7) / 8;
			mAlphaDepth = 1;
			// 32-bit align each row
			mAlphaRowBytes = (mAlphaRowBytes + 3) & ~0x3;
			mAlphaBits = new PRUint8[mAlphaRowBytes * aHeight];
			memset(mAlphaBits, 255, mAlphaRowBytes * aHeight);
			break;
		case nsMaskRequirements_kNeeds8Bit:
			mAlphaRowBytes = aWidth;
			mAlphaDepth = 8;
			// 32-bit align each row
			mAlphaRowBytes = (mAlphaRowBytes + 3) & ~0x3;
			mAlphaBits = new PRUint8[mAlphaRowBytes * aHeight];
			break;
	}
	
	return NS_OK;
}

// This is a notification that the platform independent bits have changed. Therefore,
// the contents of the BBitmap are no longer up to date. By setting the mImageCurrent
// flag to false, we can be sure that the BBitmap will be updated before it gets blit.
// TO DO: It would be better to cache the updated rectangle here, and only copy the
// area that has changed in CreateImage().
void nsImageBeOS::ImageUpdated(nsIDeviceContext *aContext, PRUint8 aFlags, nsRect *aUpdateRect) 
{
	// This should be 0'd out by Draw()
	mFlags = aFlags;
	mImageCurrent = PR_FALSE;

	mDecodedX1 = PR_MIN(mDecodedX1, aUpdateRect->x);
	mDecodedY1 = PR_MIN(mDecodedY1, aUpdateRect->y);

	if (aUpdateRect->YMost() > mDecodedY2)
		mDecodedY2 = aUpdateRect->YMost();
	if (aUpdateRect->XMost() > mDecodedX2)
		mDecodedX2 = aUpdateRect->XMost();
} 

// Draw the bitmap, this method has a source and destination coordinates
NS_IMETHODIMP nsImageBeOS::Draw(nsIRenderingContext &aContext, nsIDrawingSurface* aSurface,
	PRInt32 aSX, PRInt32 aSY, PRInt32 aSWidth, PRInt32 aSHeight,
	PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight) 
{
	// Don't bother to draw nothing		
	if (!aSWidth || !aSHeight || !aDWidth || !aDHeight)
		return NS_OK;
	if (mDecodedX2 < mDecodedX1 || mDecodedY2 < mDecodedY1)
		return NS_OK;

	// As we do all scaling with Be API (DrawBitmap), using float is safe and convinient	
	float srcX = aSX, srcY = aSY, srcMostX = aSX + aSWidth, srcMostY = aSY + aSHeight;
	float dstX = aDX, dstY = aDY, dstMostX = aDX + aDWidth, dstMostY = aDY + aDHeight;
	float  scaleX = float(aDWidth)/float(aSWidth), scaleY = float(aDHeight)/float(aSHeight);

	if (!mImageCurrent || (nsnull == mImage)) 
		BuildImage(aSurface);
	if (nsnull == mImage || mImage->BitsLength() == 0) 
		return NS_ERROR_FAILURE;

	// Limit the image rectangle to the size of the image data which
	// has been validated. 
	if ((mDecodedY1 > 0)) 
	{
		srcY = float(PR_MAX(mDecodedY1, aSY));
	}
	if ((mDecodedX1 > 0))
	{
		srcX = float(PR_MAX(mDecodedX1, aSX));
	}
	// When targeting BRects, MostX/Y is more natural than Width/Heigh
	if ((mDecodedY2 < mHeight)) 
		srcMostY = float(PR_MIN(mDecodedY2, aSY + aSHeight));

	if ((mDecodedX2 < mWidth)) 
		srcMostX = float(PR_MIN(mDecodedX2, aSX + aSWidth));

	dstX = float(srcX - aSX)*scaleX + float(aDX);
	dstY = float(srcY - aSY)*scaleY + float(aDY);
	dstMostX = dstMostX - (float(aSWidth + aSX) - srcMostX)*scaleX;
	dstMostY =	dstMostY - (float(aSHeight + aSY) - srcMostY)*scaleY;

	nsDrawingSurfaceBeOS *beosdrawing = (nsDrawingSurfaceBeOS *)aSurface;
	BView *view;

	// LockAndUpdateView() sets proper clipping region here and elsewhere in nsImageBeOS.
	if (((nsRenderingContextBeOS&)aContext).LockAndUpdateView()) 
	{
		beosdrawing->AcquireView(&view);
		if (view) 
		{
			// TODO: With future locking update check if restricting clipping region 
			// to decoded values fastens things up - in this case clipping must be reset to 0
			// at unlocking or before getting native region in LockAndUpdateView().

			// Only use B_OP_ALPHA when there is an alpha channel present, as it is much slower
			if (0 != mAlphaDepth) 
			{
				view->SetDrawingMode(B_OP_ALPHA);
				view->DrawBitmap(mImage, BRect(srcX, srcY, srcMostX - 1, srcMostY - 1),
					BRect(dstX, dstY, dstMostX - 1, dstMostY - 1));
				view->SetDrawingMode(B_OP_COPY);
			}
			else 
			{
				view->DrawBitmap(mImage, BRect(srcX, srcY, srcMostX - 1, srcMostY - 1),
					BRect(dstX, dstY, dstMostX - 1, dstMostY - 1));
			}
			// view was locked by LockAndUpdateView() before it was aquired. So unlock.
			view->UnlockLooper();
		}
		beosdrawing->ReleaseView();
	}
	
	mFlags = 0;
	return NS_OK;
}

// Draw the bitmap, this draw just has destination coordinates
NS_IMETHODIMP nsImageBeOS::Draw(nsIRenderingContext &aContext, nsIDrawingSurface* aSurface,
	PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight) 
{
	//Don't bother to draw nothing		
	if (!aWidth || !aHeight)
		return NS_OK;
	if (mDecodedX2 < mDecodedX1 || mDecodedY2 < mDecodedY1)
		return NS_OK;

	if (!mImageCurrent || (nsnull == mImage)) 
		BuildImage(aSurface);
	if (nsnull == mImage) 
		return NS_ERROR_FAILURE;

	PRInt32 validX = 0, validY = 0, validMostX = mWidth, validMostY = mHeight;

	// XXX kipp: this is temporary code until we eliminate the
	// width/height arguments from the draw method.
	aWidth = PR_MIN(aWidth, mWidth);
	aHeight = PR_MIN(aHeight, mHeight);

	if ((mDecodedY2 < aHeight)) 
		validMostY = mDecodedY2;

	if ((mDecodedX2 < aWidth)) 
		validMostX = mDecodedX2;

	if ((mDecodedY1 > 0)) 
		validY = mDecodedY1;
	if ((mDecodedX1 > 0)) 
		validX = mDecodedX1;
			
	nsDrawingSurfaceBeOS *beosdrawing = (nsDrawingSurfaceBeOS *)aSurface;
	BView *view;

	if (((nsRenderingContextBeOS&)aContext).LockAndUpdateView()) 
	{
		beosdrawing->AcquireView(&view);
		if (view) 
		{
			// See TODO clipping comment above - code ready for use:
			// BRegion tmpreg(BRect(aX + validX, aY + validY, aX + validMostX - 1, aY + validMostY - 1));
			// view->ConstrainClippingRegion(&tmpreg);
			
			// Only use B_OP_ALPHA when there is an alpha channel present, as it is much slower
			if (0 != mAlphaDepth) 
			{
				view->SetDrawingMode(B_OP_ALPHA);
				view->DrawBitmap(mImage, BRect(validX, validY, validMostX - 1, validMostY - 1), 
								BRect(aX + validX, aY + validY, aX + validMostX - 1, aY + validMostY - 1));
				view->SetDrawingMode(B_OP_COPY);
			} 
			else 
			{
				view->DrawBitmap(mImage, BRect(validX, validY, validMostX - 1, validMostY - 1), 
								BRect(aX + validX, aY + validY, aX + validMostX - 1, aY + validMostY - 1));
			}
			view->UnlockLooper();
		}
		beosdrawing->ReleaseView();
	}

	mFlags = 0;
	return NS_OK;
}

NS_IMETHODIMP nsImageBeOS::DrawTile(nsIRenderingContext &aContext, nsIDrawingSurface* aSurface,
	PRInt32 aSXOffset, PRInt32 aSYOffset, PRInt32 aPadX, PRInt32 aPadY, const nsRect &aTileRect) 
{
	// Don't bother to draw nothing	
	if (!aTileRect.width || !aTileRect.height)
		return NS_OK;
	if (mDecodedX2 < mDecodedX1 || mDecodedY2 < mDecodedY1)
		return NS_OK;

	if (!mImageCurrent || (nsnull == mImage)) 
		BuildImage(aSurface);
	if (nsnull == mImage || mImage->BitsLength() == 0) 
		return NS_ERROR_FAILURE;

	PRInt32 validX = 0, validY = 0, validMostX = mWidth, validMostY = mHeight;

	// Limit the image rectangle to the size of the image data which
	// has been validated.
	if ((mDecodedY2 < mHeight)) 
		validMostY = mDecodedY2;

	if ((mDecodedX2 < mWidth)) 
		validMostX = mDecodedX2;

	if ((mDecodedY1 > 0)) 
		validY = mDecodedY1;

	if ((mDecodedX1 > 0)) 
		validX = mDecodedX1;

	nsDrawingSurfaceBeOS *beosdrawing = (nsDrawingSurfaceBeOS *)aSurface;
	BView *view = 0;
	if (((nsRenderingContextBeOS&)aContext).LockAndUpdateView()) 
	{
		beosdrawing->AcquireView(&view);
		if (view) 
		{
	        BRegion rgn(BRect(aTileRect.x, aTileRect.y,
	        			aTileRect.x + aTileRect.width - 1, aTileRect.y + aTileRect.height - 1));
    	    view->ConstrainClippingRegion(&rgn);

			BBitmap *tmpbmp = 0;			
			// Force transparency for bitmap blitting in case of padding even if mAlphaDepth == 0
			if (0 != mAlphaDepth || aPadX || aPadY) 
				view->SetDrawingMode(B_OP_ALPHA);
			// Creating temporary bitmap, compatible with mImage and  with size of area to be filled with tiles
			tmpbmp = new BBitmap(BRect(0, 0, aTileRect.width - 1, aTileRect.height -1), mImage->ColorSpace(), false);
			int32 tmpbitlength = tmpbmp->BitsLength();

			if (!tmpbmp || tmpbitlength == 0)
			{
				// Failed. Cleaning things a bit.
				view->UnlockLooper();
				if(tmpbmp)
					delete tmpbmp;
				beosdrawing->ReleaseView();
				return NS_ERROR_FAILURE;
			}

			uint32 *dst0 = (uint32 *)tmpbmp->Bits();
			uint32 *src0 = (uint32 *)mImage->Bits();
			uint32 *dst = dst0;
			uint32 dstRowLength = tmpbmp->BytesPerRow()/4;
			uint32 dstColHeight = tmpbitlength/tmpbmp->BytesPerRow();

			// Filling tmpbmp with transparent color to preserve padding areas on destination 
			uint32 filllength = tmpbitlength/4;
			if (0 != mAlphaDepth  || aPadX || aPadY) 
			{
				for (uint32 i=0, *dst = dst0; i < filllength; ++i)
					*(dst++) = B_TRANSPARENT_MAGIC_RGBA32;
			}

			// Rendering mImage tile to temporary bitmap
			uint32 *src = src0; dst = dst0;
			for (uint32 y = 0, yy = aSYOffset; y < dstColHeight; ++y) 
			{					
				src = src0 + yy*mWidth;
				dst = dst0 + y*dstRowLength;
				// Avoid unneccessary job outside update rect
				if (yy >= validY && yy <= validMostY)
				{
					for (uint32 x = 0, xx = aSXOffset; x < dstRowLength; ++x) 
					{
						// Avoid memwrite if outside update rect
						if(xx >= validX && xx <= validMostX)
							dst[x] = src[xx];
						if(++xx == mWidth)
						{
							// Width of source reached. Adding horizontal paddding.
							xx = 0;
							x += aPadX;
						}
					}
				}
				if (++yy == mHeight)
				{
					// Height of source reached. Adding vertical paddding.
					yy = 0;
					y += aPadY;
				}
			}
			// Flushing temporary bitmap to proper area in drawable BView	
			view->DrawBitmap(tmpbmp, BPoint(aTileRect.x , aTileRect.y ));
			view->SetDrawingMode(B_OP_COPY);
			view->Sync(); // useful in multilayered pages with animation
			view->UnlockLooper();
			if (tmpbmp) 
			{
				delete tmpbmp;
				tmpbmp = 0;
			}
		}
		beosdrawing->ReleaseView();
	}
	mFlags = 0;
	return NS_OK;
}

// If not yet optimized, delete mImageBits and mAlphaBits here to save memory,
// since the image will never change again and no one else will need to get the
// platform independent bits.
nsresult nsImageBeOS::Optimize(nsIDeviceContext *aContext) 
{
	if (!mOptimized) 
	{
		// Make sure the BBitmap is up to date
		CreateImage(NULL);
		
		// Release Mozilla-specific data
		if (nsnull != mImageBits) 
		{
			delete [] mImageBits;
			mImageBits = nsnull;
		}
		if (nsnull != mAlphaBits) 
		{
			delete [] mAlphaBits;
			mAlphaBits = nsnull;
		}
		
		mOptimized = PR_TRUE;
	}
	return NS_OK;
}

// Not implemented at the moment. It's unclear whether this is necessary for
// the BeOS port or not. BBitmap::Lock/UnlockBits()  may be used if neccessary
NS_IMETHODIMP nsImageBeOS::LockImagePixels(PRBool aMaskPixels) 
{
	return NS_OK;
}

// Same as above.
NS_IMETHODIMP nsImageBeOS::UnlockImagePixels(PRBool aMaskPixels) 
{
	return NS_OK;
}

nsresult nsImageBeOS::BuildImage(nsIDrawingSurface* aDrawingSurface) 
{
	CreateImage(aDrawingSurface);
	return NS_OK;
}

// This function is responsible for copying the platform independent bits (mImageBits
// and mAlphaBits) into a BBitmap so it can be drawn using the Be APIs. Since it is
// expensive to create and destroy a BBitmap for this purpose, we will keep this bitmap
// around, which also prevents the need to copy the bits if they have not changed.
void nsImageBeOS::CreateImage(nsIDrawingSurface* aSurface) 
{
	PRInt32 validX = 0, validY = 0, validMostX = mWidth, validMostY = mHeight;

	if (mImageBits) 
	{
		if (24 != mDepth) 
		{
			NS_ASSERTION(PR_FALSE, "unexpected image depth");
			return;
		}
		
		// If the previous BBitmap is the right dimensions and colorspace, then reuse it.
		const color_space cs = B_RGBA32;
		if (nsnull != mImage) 
		{
			BRect bounds = mImage->Bounds();
			if (bounds.IntegerWidth() != validMostX - 1 || bounds.IntegerHeight() != validMostY - 1 ||
				mImage->ColorSpace() != cs) 
			{
				
				delete mImage;
				mImage = new BBitmap(BRect(0, 0, validMostX - 1, validMostY - 1), cs, false);
			} 
			else 
			{
				// Don't copy the data twice if the BBitmap is up to date
				if (mImageCurrent) return;
			}
		} 
		else 
		{
			// No BBitmap exists, so create one and update it
			mImage = new BBitmap(BRect(0, 0, mWidth - 1, mHeight - 1), cs, false);
		}
		
		// Only the data that has changed (the rectangle supplied to ImageUpdated())
		// needs to be copied to the BBitmap. 
		
		if ((mDecodedY2 < mHeight)) 
			validMostY = mDecodedY2;

		if ((mDecodedX2 < mWidth)) 
			validMostX = mDecodedX2;

		if ((mDecodedY1 > 0)) 
			validY = mDecodedY1;
		if ((mDecodedX1 > 0)) 
			validX = mDecodedX1;
		
		if (mDecodedX2 < mDecodedX1 || mDecodedY2 < mDecodedY1)
			return;

		// Making up a 32 bit double word for the destination is much more efficient
		// than writing each byte separately on some CPUs. For example, BeOS does not
		// support write-combining on the Athlon/Duron family.
		
		// Using mRowBytes as source stride due alignment of mImageBits array to 4.
		// Destination stride is mWidth, as mImage always uses RGB(A)32 
		if (mImage && mImage->IsValid()) 
		{
			uint32 *dest, *dst0 = (uint32 *)mImage->Bits() + validX;
			uint8 *src, *src0 = mImageBits + 3*validX; 
			if (mAlphaBits) 
			{
				uint8 a, *alpha = mAlphaBits + validY*mAlphaRowBytes;;
				for (int y = validY; y < validMostY; ++y) 
				{
					dest = dst0 + y*mWidth;
					src = src0 + y*mRowBytes;
					for (int x = validX; x < validMostX; ++x) 
					{
						if(1 == mAlphaDepth)
							a = (alpha[x / 8] & (1 << (7 - (x % 8)))) ? 255 : 0;
						else
							a = alpha[x];
						*dest++ = (a << 24) | (src[2] << 16) | (src[1] << 8) | src[0];
						src += 3;
					}
					alpha += mAlphaRowBytes;
				}
			} 
			else 
			{
				// Fixed 255 in the alpha channel to mean completely opaque
				for (int y = validY; y < validMostY; ++y) 
				{
					dest = dst0 + y*mWidth;
					src = src0 + y*mRowBytes;
					for (int x = validX; x < validMostX; ++x) 
					{
						*dest++ = 0xff000000 | (src[2] << 16) | (src[1] << 8) | src[0];
						src += 3;
					}
				}
			}
			
			// The contents of the BBitmap now match mImageBits (and mAlphaBits),
			// so mark the flag up to date to prevent extra copies in the future.
			mImageCurrent = PR_TRUE;
		}
	}
}

// Since this function does not touch either the source or destination BBitmap,
// there is no need to call CreateImage(). The platform independent bits will get
// copied to the BBitmap if and when it gets blit. Code is derived from GTK version,
// but some wordy constant math is moved out of loops for performance
NS_IMETHODIMP nsImageBeOS::DrawToImage(nsIImage* aDstImage, 
										nscoord aDX, nscoord aDY, 
										nscoord aDWidth, nscoord aDHeight)
{
	nsImageBeOS *dest = NS_STATIC_CAST(nsImageBeOS *, aDstImage);

	if (!dest)
		return NS_ERROR_FAILURE;

	if (aDX >= dest->mWidth || aDY >= dest->mHeight)
		return NS_OK;

	PRUint8 *rgbPtr=0, *alphaPtr=0;
	PRUint32 rgbStride, alphaStride;

	rgbPtr = mImageBits;
	rgbStride = mRowBytes;
	alphaPtr = mAlphaBits;
	alphaStride = mAlphaRowBytes;

	PRInt32 y;
	PRInt32 ValidWidth = ( aDWidth < ( dest->mWidth - aDX ) ) ? aDWidth : ( dest->mWidth - aDX ); 
	PRInt32 ValidHeight = ( aDHeight < ( dest->mHeight - aDY ) ) ? aDHeight : ( dest->mHeight - aDY );

	// now composite the two images together
	switch (mAlphaDepth)
	{
		case 1:
		{
			PRUint8 *dst = dest->mImageBits + aDY*dest->mRowBytes + 3*aDX;
			PRUint8 *dstAlpha = dest->mAlphaBits + aDY*dest->mAlphaRowBytes;
			PRUint8 *src = rgbPtr;
			PRUint8 *alpha = alphaPtr;
			PRUint8 offset = aDX & 0x7; // x starts at 0
			PRUint8 offset_8U = 8U - offset;
			int iterations = (ValidWidth+7)/8; // round up
			PRUint32  dst_it_stride = dest->mRowBytes - 3*8*iterations;
			PRUint32  src_it_stride = rgbStride - 3*8*iterations;
			PRUint32  alpha_it_stride = alphaStride - iterations;

			for (y=0; y < ValidHeight; ++y)
			{
				for (int x=0; x < ValidWidth; x += 8, dst += 24, src += 24)
				{
					PRUint8 alphaPixels = *alpha++;
					PRInt32  VW_x = ValidWidth-x;
					if (alphaPixels == 0)
						continue; // all 8 transparent; jump forward

					// 1 or more bits are set, handle dstAlpha now - may not be aligned.
					// Are all 8 of these alpha pixels used?
					if (x+7 >= ValidWidth)
					{
						alphaPixels &= 0xff << (8 - VW_x); // no, mask off unused
						if (alphaPixels == 0)
							continue;  // no 1 alpha pixels left
					}
					if (offset == 0)
					{
						dstAlpha[(aDX+x)>>3] |= alphaPixels; // the cheap aligned case
					}
					else
					{
						dstAlpha[(aDX+x)>>3] |= alphaPixels >> offset;
						// avoid write if no 1's to write - also avoids going past end of array
						// compiler should merge the common sub-expressions
						if (alphaPixels << offset_8U)
							dstAlpha[((aDX+x)>>3) + 1] |= alphaPixels << offset_8U;
					}
          
					if (alphaPixels == 0xff)
					{
						// fix - could speed up by gathering a run of 0xff's and doing 1 memcpy
						// all 8 pixels set; copy and jump forward
						memcpy(dst,src,24);
						continue;
					}
					else
					{
						// else mix of 1's and 0's in alphaPixels, do 1 bit at a time
						// Don't go past end of line!
						PRUint8 *d = dst, *s = src;
						for (PRUint8 aMask = 1<<7, j = 0; aMask && j < VW_x; aMask >>= 1, ++j)
						{
							// if this pixel is opaque then copy into the destination image
							if (alphaPixels & aMask)
							{
								// might be faster with *d++ = *s++ 3 times?
								d[0] = s[0];
								d[1] = s[1];
								d[2] = s[2];
								// dstAlpha bit already set
							}
							d += 3;
							s += 3;
						}
					}
				}
				// at end of each line, bump pointers. 
				dst = dst + dst_it_stride;
				src = src + src_it_stride;
				alpha = alpha + alpha_it_stride;
				dstAlpha += dest->mAlphaRowBytes;
			}
		}
		break;
		case 0:
		default:
			for (y=0; y < ValidHeight; ++y)
				memcpy(dest->mImageBits + (y+aDY)*dest->mRowBytes + 3*aDX, 
						rgbPtr + y*rgbStride, 3*ValidWidth);
	}
	// ImageUpdated() won't be called in this case, so we need to mark the destination
	// image as changed. This will cause its data to be copied in the BBitmap when it
	// tries to blit. The source has not been modified, so its status has not changed.
	nsRect rect(aDX, aDY, ValidWidth, ValidHeight);
	dest->ImageUpdated(nsnull, 0, &rect);
	mImageCurrent = PR_TRUE;

	return NS_OK;
}
