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
 * Contributor(s): Daniel Switkin and Mathias Agopian
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

#include "nsImageBeOS.h"
#include "nsRenderingContextBeOS.h"
#include "nspr.h"
#include "imgScaler.h"
#include <Looper.h>
#include <Bitmap.h>
#include <View.h>

NS_IMPL_ISUPPORTS1(nsImageBeOS, nsIImage)

nsImageBeOS::nsImageBeOS() {
	NS_INIT_ISUPPORTS();
	mImage = nsnull;
	mImageBits = nsnull;
	mWidth = 0;
	mHeight = 0;
	mDepth = 0;
	mRowBytes = 0;
	mSizeImage = 0;
	
	mNaturalWidth = 0;
	mNaturalHeight = 0;
	mDecodedX1 = 0;
	mDecodedY1 = 0;
	mDecodedX2 = 0;
	mDecodedY2 = 0;
	
	mAlphaBits = nsnull;
	mAlphaWidth = 0;
	mAlphaHeight = 0;
	mAlphaRowBytes = 0;
	mAlphaDepth = 0;

	mFlags = 0;
	mNumBytesPixel = 0;
	mImageCurrent = PR_FALSE;
	mOptimized = PR_FALSE;
}

nsImageBeOS::~nsImageBeOS() {
	if (nsnull != mImage) {
		delete mImage;
		mImage = nsnull;
	}
	if (nsnull != mImageBits) {
		delete [] mImageBits;
		mImageBits = nsnull;
	}
	if (nsnull != mAlphaBits) {
		delete [] mAlphaBits;
		mAlphaBits = nsnull;
	}
}

nsresult nsImageBeOS::Init(PRInt32 aWidth, PRInt32 aHeight, PRInt32 aDepth,
	nsMaskRequirements aMaskRequirements) {
	
	if (nsnull != mImageBits) {
		delete [] mImageBits;
		mImageBits = nsnull;
	}
	if (nsnull != mAlphaBits) {
		delete [] mAlphaBits;
		mAlphaBits = nsnull;
	}

	SetDecodedRect(0, 0, 0, 0);
	SetNaturalWidth(0); 
	SetNaturalHeight(0); 

	// Only 24 bit depths are supported for the platform independent bits
	if (24 == aDepth) {
		mNumBytesPixel = 3;
	} else {
		NS_ASSERTION(PR_FALSE, "unexpected image depth");
		return NS_ERROR_UNEXPECTED;
	}
	
	mWidth = aWidth;
	mHeight = aHeight;
	mDepth = aDepth;
	mRowBytes = (mWidth * mDepth) >> 5;
	if (((PRUint32)mWidth * mDepth) & 0x1F) mRowBytes++;
	mRowBytes <<= 2;
	mSizeImage = mRowBytes * mHeight;

	mImageBits = new PRUint8[mSizeImage];

	switch (aMaskRequirements) {
		case nsMaskRequirements_kNoMask:
			mAlphaBits = nsnull;
			mAlphaWidth = 0;
			mAlphaHeight = 0;
			break;
		case nsMaskRequirements_kNeeds1Bit:
			mAlphaRowBytes = (aWidth + 7) / 8;
			mAlphaDepth = 1;
			// 32-bit align each row
			mAlphaRowBytes = (mAlphaRowBytes + 3) & ~0x3;
			
			mAlphaBits = new PRUint8[mAlphaRowBytes * aHeight];
			memset(mAlphaBits, 255, mAlphaRowBytes * aHeight);
			mAlphaWidth = aWidth;
			mAlphaHeight = aHeight;
			break;
		case nsMaskRequirements_kNeeds8Bit:
			mAlphaRowBytes = aWidth;
			mAlphaDepth = 8;
			// 32-bit align each row
			mAlphaRowBytes = (mAlphaRowBytes + 3) & ~0x3;
			
			mAlphaBits = new PRUint8[mAlphaRowBytes * aHeight];
			mAlphaWidth = aWidth;
			mAlphaHeight = aHeight;
			break;
	}
	
	return NS_OK;
}

nsIImage *nsImageBeOS::DuplicateImage() {
	return nsnull;
}

void nsImageBeOS::SetAlphaLevel(PRInt32 aAlphaLevel) {

}

void nsImageBeOS::MoveAlphaMask(PRInt32 aX, PRInt32 aY) {

}

// This is a notification that the platform independent bits have changed. Therefore,
// the contents of the BBitmap are no longer up to date. By setting the mImageCurrent
// flag to false, we can be sure that the BBitmap will be updated before it gets blit.
// TO DO: It would be better to cache the updated rectangle here, and only copy the
// area that has changed in CreateImage().
void nsImageBeOS::ImageUpdated(nsIDeviceContext *aContext, PRUint8 aFlags, nsRect *aUpdateRect) {
	// This should be 0'd out by Draw()
	mFlags = aFlags;
	mImageCurrent = PR_FALSE;
} 

// Draw the bitmap, this method has a source and destination coordinates
NS_IMETHODIMP nsImageBeOS::Draw(nsIRenderingContext &aContext, nsDrawingSurface aSurface,
	PRInt32 aSX, PRInt32 aSY, PRInt32 aSWidth, PRInt32 aSHeight,
	PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight) {
	
	nsDrawingSurfaceBeOS *beosdrawing = (nsDrawingSurfaceBeOS *)aSurface;
	BView *view;
	beosdrawing->AcquireView(&view);
	
	if (!mImageCurrent || (nsnull == mImage)) BuildImage(aSurface);
	if (nsnull == mImage) return PR_FALSE;
	
	if (view && view->LockLooper()) {
		// Only use B_OP_ALPHA when there is an alpha channel present, as it is much slower
		if (0 != mAlphaDepth) {
			view->SetDrawingMode(B_OP_ALPHA);
			view->DrawBitmapAsync(mImage, BRect(aSX, aSY, aSX + aSWidth - 1, aSY + aSHeight - 1),
				BRect(aDX, aDY, aDX + aDWidth - 1, aDY + aDHeight - 1));
			view->SetDrawingMode(B_OP_COPY);
		} else {
			view->DrawBitmapAsync(mImage, BRect(aSX, aSY, aSX + aSWidth - 1, aSY + aSHeight - 1),
				BRect(aDX, aDY, aDX + aDWidth - 1, aDY + aDHeight - 1));
		}
		view->Sync();
		view->UnlockLooper();
	}
	beosdrawing->ReleaseView();
	
	mFlags = 0;
	return NS_OK;
}

// Draw the bitmap, this draw just has destination coordinates
NS_IMETHODIMP nsImageBeOS::Draw(nsIRenderingContext &aContext, nsDrawingSurface aSurface,
	PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight) {
	
	// XXX kipp: this is temporary code until we eliminate the
	// width/height arguments from the draw method.
	aWidth = PR_MIN(aWidth, mWidth);
	aHeight = PR_MIN(aHeight, mHeight);
	
	nsDrawingSurfaceBeOS *beosdrawing = (nsDrawingSurfaceBeOS *)aSurface;
	BView *view;
	beosdrawing->AcquireView(&view);

	if (!mImageCurrent || (nsnull == mImage)) BuildImage(aSurface);
	if (nsnull == mImage) return PR_FALSE;
	
	if (view && view->LockLooper()) {
		// Only use B_OP_ALPHA when there is an alpha channel present, as it is much slower
		if (0 != mAlphaDepth) {
			view->SetDrawingMode(B_OP_ALPHA);
			view->DrawBitmapAsync(mImage, BRect(0, 0, aWidth - 1, aHeight - 1),
				BRect(aX, aY, aX + aWidth - 1, aY + aHeight - 1));
			view->SetDrawingMode(B_OP_COPY);
		} else {
			view->DrawBitmapAsync(mImage, BRect(0, 0, aWidth - 1, aHeight - 1),
				BRect(aX, aY, aX + aWidth - 1, aY + aHeight - 1));
		}
		view->Sync();
		view->UnlockLooper();
	}
	beosdrawing->ReleaseView();
	
	mFlags = 0;
	return NS_OK;
}

// Private Draw() call used by DrawTile() routines. The view passed in is
// assumed to already be locked and the drawing is asynchronous. Calling Sync()
// on every iteration of tiling a small texture has a lot of overhead.
void nsImageBeOS::DrawNoLock(BView *aView, PRInt32 aX, PRInt32 aY, PRInt32 aWidth,
	PRInt32 aHeight) {
	
	// XXX kipp: this is temporary code until we eliminate the
	// width/height arguments from the draw method.
	aWidth = PR_MIN(aWidth, mWidth);
	aHeight = PR_MIN(aHeight, mHeight);
	
	if (aView) {
		if (0 != mAlphaDepth) {
			aView->SetDrawingMode(B_OP_ALPHA);
			aView->DrawBitmapAsync(mImage, BRect(0, 0, aWidth - 1, aHeight - 1),
				BRect(aX, aY, aX + aWidth - 1, aY + aHeight - 1));
			aView->SetDrawingMode(B_OP_COPY);
		} else {
			aView->DrawBitmapAsync(mImage, BRect(0, 0, aWidth - 1, aHeight - 1),
				BRect(aX, aY, aX + aWidth - 1, aY + aHeight - 1));
		}
	}
}

/** 
 * Draw a tiled version of the bitmap 
 * @update - dwc 3/30/00 
 * @param aSurface the surface to blit to 
 * @param aX0 starting x 
 * @param aY0 starting y 
 * @param aX1 ending x 
 * @param aY1 ending y 
 * @param aWidth The destination width of the pixelmap 
 * @param aHeight The destination height of the pixelmap 
 */ 
NS_IMETHODIMP nsImageBeOS::DrawTile(nsIRenderingContext &aContext, nsDrawingSurface aSurface,
	nsRect &aSrcRect, nsRect &aTileRect) {
	
	PRInt32 aY0 = aTileRect.y, aX0 = aTileRect.x, aY1 = aTileRect.y + aTileRect.height,
		aX1 = aTileRect.x + aTileRect.width;
	
	nsDrawingSurfaceBeOS *beosdrawing = (nsDrawingSurfaceBeOS *)aSurface;
	BView *view;
	beosdrawing->AcquireView(&view);
	
	if (!mImageCurrent || (nsnull == mImage)) BuildImage(aSurface);
	if (nsnull == mImage) return PR_FALSE;
	
	if (view && view->LockLooper()) {
		for (PRInt32 y = aY0; y < aY1; y += aSrcRect.height) {
			for (PRInt32 x = aX0; x < aX1; x += aSrcRect.width) {
				DrawNoLock(view, x, y, PR_MIN(aSrcRect.width, aX1 - x),
					PR_MIN(aSrcRect.height, aY1 - y));
			}
		}
		view->Sync();
		view->UnlockLooper();
	}

	beosdrawing->ReleaseView();
	return NS_OK;
}

NS_IMETHODIMP nsImageBeOS::DrawTile(nsIRenderingContext &aContext, nsDrawingSurface aSurface,
	PRInt32 aSXOffset, PRInt32 aSYOffset, const nsRect &aTileRect) {
	
	PRInt32 validX = 0, validY = 0, validWidth = mWidth, validHeight = mHeight;
	
	// Limit the image rectangle to the size of the image data which
	// has been validated.
	if ((mDecodedY2 < mHeight)) {
		validHeight = mDecodedY2 - mDecodedY1;
	}
	if ((mDecodedX2 < mWidth)) {
		validWidth = mDecodedX2 - mDecodedX1;
	}
	if ((mDecodedY1 > 0)) {
		validHeight -= mDecodedY1;
		validY = mDecodedY1;
	}
	if ((mDecodedX1 > 0)) {
		validWidth -= mDecodedX1;
		validX = mDecodedX1;
	}
	
	PRInt32 aY0 = aTileRect.y - aSYOffset, aX0 = aTileRect.x - aSXOffset,
		aY1 = aTileRect.y + aTileRect.height, aX1 = aTileRect.x + aTileRect.width;
	
	// Set up clipping and call Draw().
	PRBool clipState;
	aContext.PushState();
	aContext.SetClipRect(aTileRect, nsClipCombine_kIntersect, clipState);

	nsDrawingSurfaceBeOS *beosdrawing = (nsDrawingSurfaceBeOS *)aSurface;
	BView *view;
	beosdrawing->AcquireView(&view);
	
	if (!mImageCurrent || (nsnull == mImage)) BuildImage(aSurface);
	if (nsnull == mImage) return PR_FALSE;
	
	if (view && view->LockLooper()) {
		for (PRInt32 y = aY0; y < aY1; y += validHeight) {
			for (PRInt32 x = aX0; x < aX1; x += validWidth) {
				DrawNoLock(view, x, y, PR_MIN(validWidth, aX1 - x),
					PR_MIN(validHeight, aY1-y));
			}
		}
		view->Sync();
		view->UnlockLooper();
	}
	
	aContext.PopState(clipState);
	beosdrawing->ReleaseView();
	return NS_OK;
}

nsresult nsImageBeOS::BuildImage(nsDrawingSurface aDrawingSurface) {
	CreateImage(aDrawingSurface);
	return NS_OK;
}

// This function is responsible for copying the platform independent bits (mImageBits
// and mAlphaBits) into a BBitmap so it can be drawn using the Be APIs. Since it is
// expensive to create and destroy a BBitmap for this purpose, we will keep this bitmap
// around, which also prevents the need to copy the bits if they have not changed.
void nsImageBeOS::CreateImage(nsDrawingSurface aSurface) {
	if (mImageBits) {
		if (24 != mDepth) {
			NS_ASSERTION(PR_FALSE, "unexpected image depth");
			return;
		}
		
		// If the previous BBitmap is the right dimensions and colorspace, then reuse it.
		const color_space cs = B_RGB32;
		if (nsnull != mImage) {
			BRect bounds = mImage->Bounds();
			if (bounds.IntegerWidth() != mWidth - 1 || bounds.IntegerHeight() != mHeight - 1 ||
				mImage->ColorSpace() != cs) {
				
				delete mImage;
				mImage = new BBitmap(BRect(0, 0, mWidth - 1, mHeight - 1), cs);
			} else {
				// Don't copy the data twice if the BBitmap is up to date
				if (mImageCurrent) return;
			}
		} else {
			// No BBitmap exists, so create one and update it
			mImage = new BBitmap(BRect(0, 0, mWidth - 1, mHeight - 1), cs);
		}
		
		// Making up a 32 bit double word for the destination is much more efficient
		// than writing each byte separately on some CPUs. For example, BeOS does not
		// support write-combining on the Athlon/Duron family.
		// TO DO: Only the data that has changed (the rectangle supplied to ImageUpdated())
		// needs to be copied to the BBitmap, it is wasteful to copy the entire contents here.
		if (mImage && mImage->IsValid()) {
			uint32 *dest = (uint32 *)mImage->Bits();
			uint8 *src = mImageBits;

			if (mAlphaBits) {
				uint8 *alpha = mAlphaBits;
				if (1 == mAlphaDepth) {
					for (int y = 0; y < mHeight; y++) {
						for (int x = 0; x < mWidth; x++) {
							uint8 a = (alpha[x / 8] & (1 << (7 - (x % 8)))) ? 255 : 0;
							*dest++ = (a << 24) | (src[2] << 16) | (src[1] << 8) | src[0];
							src += 3;
						}
						src += mRowBytes - (mWidth * mNumBytesPixel);
						alpha += mAlphaRowBytes;
					}				
				} else {
					for (int y = 0; y < mHeight; y++) {
						for (int x = 0; x < mWidth; x++) {
							*dest++ = (alpha[x] << 24) | (src[2] << 16) | (src[1] << 8) | src[0];
							src += 3;
						}
						src += mRowBytes - (mWidth * mNumBytesPixel);
						alpha += mAlphaRowBytes;
					}
				}
			} else {
				// Fixed 255 in the alpha channel to mean completely opaque
				for (int y = 0; y < mHeight; y++) {
					for (int x = 0; x < mWidth; x++) {
						*dest++ = 0xff000000 | (src[2] << 16) | (src[1] << 8) | src[0];
						src += 3;
					}
					src += mRowBytes - (mWidth * mNumBytesPixel);
				}
			}
			
			// The contents of the BBitmap now match mImageBits (and mAlphaBits),
			// so mark the flag up to date to prevent extra copies in the future.
			mImageCurrent = PR_TRUE;
		}
	}
}

// If not yet optimized, delete mImageBits and mAlphaBits here to save memory,
// since the image will never change again and no one else will need to get the
// platform independent bits.
nsresult nsImageBeOS::Optimize(nsIDeviceContext *aContext) {
	if (!mOptimized) {
		// Make sure the BBitmap is up to date
		CreateImage(NULL);
		
		// Release Mozilla-specific data
		if (nsnull != mImageBits) {
			delete [] mImageBits;
			mImageBits = nsnull;
		}
		if (nsnull != mAlphaBits) {
			delete [] mAlphaBits;
			mAlphaBits = nsnull;
		}
		
		mOptimized = PR_TRUE;
	}
	return NS_OK;
}

// Not implemented at the moment. It's unclear whether this is necessary for
// the BeOS port or not.
NS_IMETHODIMP nsImageBeOS::LockImagePixels(PRBool aMaskPixels) {
	return NS_OK;
}

// Same as above.
NS_IMETHODIMP nsImageBeOS::UnlockImagePixels(PRBool aMaskPixels) {
	return NS_OK;
}

NS_IMETHODIMP nsImageBeOS::SetNaturalWidth(PRInt32 naturalwidth) {
	mNaturalWidth = naturalwidth;
	return NS_OK;
} 

NS_IMETHODIMP nsImageBeOS::SetNaturalHeight(PRInt32 naturalheight) {
	mNaturalHeight = naturalheight;
	return NS_OK;
}

// Set the decoded dimensions of the image
NS_IMETHODIMP nsImageBeOS::SetDecodedRect(PRInt32 x1, PRInt32 y1, PRInt32 x2, PRInt32 y2) {
	mDecodedX1 = x1;
	mDecodedY1 = y1;
	mDecodedX2 = x2;
	mDecodedY2 = y2;
	return NS_OK;
}

// Since this function does not touch either the source or destination BBitmap,
// there is no need to call CreateImage(). The platform independent bits will get
// copied to the BBitmap if and when it gets blit.
NS_IMETHODIMP nsImageBeOS::DrawToImage(nsIImage *aDstImage, nscoord aDX, nscoord aDY,
	nscoord aDWidth, nscoord aDHeight) {
	
	nsImageBeOS *dest = NS_STATIC_CAST(nsImageBeOS *, aDstImage);
	if (!dest) return NS_ERROR_FAILURE;
	if (!dest->mImageBits) return NS_ERROR_FAILURE;
	
	if (!mImageBits) return NS_ERROR_FAILURE;
	
	// The following part is derived from GTK version
	// 2001/6/21 Makoto Hamanaka < VTA04230@nifty.com >
	
	// Need to copy the mImageBits in case we're rendered scaled
	PRUint8 *scaledImage = nsnull, *scaledAlpha = nsnull;
	PRUint8 *rgbPtr = nsnull, *alphaPtr = nsnull;
	PRUint32 rgbStride = 0, alphaStride = 0;
	
	if ((aDWidth != mWidth) || (aDHeight != mHeight)) {
		// scale factor in DrawTo... start scaling
		scaledImage = (PRUint8 *)nsMemory::Alloc(3 * aDWidth * aDHeight);
		if (!scaledImage) return NS_ERROR_OUT_OF_MEMORY;
		
		RectStretch(0, 0, mWidth - 1, mHeight - 1, 0, 0, aDWidth - 1, aDHeight - 1,
			mImageBits, mRowBytes, scaledImage, 3 * aDWidth, 24);
		
		if (mAlphaDepth) {
			if (mAlphaDepth == 1) {
				// Round to next byte
				alphaStride = (aDWidth + 7) >> 3;
			} else {
				alphaStride = aDWidth;
			}
			
			scaledAlpha = (PRUint8 *)nsMemory::Alloc(alphaStride * aDHeight);
			if (!scaledAlpha) {
				nsMemory::Free(scaledImage);
				return NS_ERROR_OUT_OF_MEMORY;
			}
			
			RectStretch(0, 0, mWidth - 1, mHeight - 1, 0, 0, aDWidth - 1, aDHeight - 1,
				mAlphaBits, mAlphaRowBytes, scaledAlpha, alphaStride, mAlphaDepth);
		}
		rgbPtr = scaledImage;
		rgbStride = 3 * aDWidth;
		alphaPtr = scaledAlpha;
	} else {
		rgbPtr = mImageBits;
		rgbStride = mRowBytes;
		alphaPtr = mAlphaBits;
		alphaStride = mAlphaRowBytes;
	}

	// Now composite the two images together
	switch (mAlphaDepth) {
		case 1:
			for (int y = 0; y < aDHeight; y++) {
				PRUint8 *dst = dest->mImageBits + (y + aDY) * dest->mRowBytes + 3 * aDX;
				PRUint8 *dstAlpha = dest->mAlphaBits + (y + aDY) * dest->mAlphaRowBytes;
				PRUint8 *src = rgbPtr + y * rgbStride; 
				PRUint8 *alpha = alphaPtr + y * alphaStride;

				for (int x = 0; x < aDWidth; x++, dst += 3, src += 3) {
#define NS_GET_BIT(rowptr, x) (rowptr[(x) >> 3] & (1 << (7 - (x) & 0x7)))
#define NS_SET_BIT(rowptr, x) (rowptr[(x) >> 3] |= (1 << (7 - (x) & 0x7)))					
					// If this pixel is opaque then copy into the destination image
					if (NS_GET_BIT(alpha, x)) {
						dst[0] = src[0];
						dst[1] = src[1];
						dst[2] = src[2];
						NS_SET_BIT(dstAlpha, aDX + x);
					}
#undef NS_GET_BIT
#undef NS_SET_BIT
				}
			}
			break;
		case 8:
			for (int y = 0; y < aDHeight; y++) {
				PRUint8 *dst = dest->mImageBits + (y + aDY) * dest->mRowBytes + 3 * aDX;
				PRUint8 *dstAlpha = dest->mAlphaBits + (y + aDY) * dest->mAlphaRowBytes + aDX;
				PRUint8 *src = rgbPtr + y * rgbStride;
				PRUint8 *alpha = alphaPtr + y * alphaStride;
				for (int x = 0; x < aDWidth; x++, dst += 3, dstAlpha++, src += 3, alpha++) {
					// Blend this pixel over the destination image
					unsigned val = *alpha;
					MOZ_BLEND(dst[0], dst[0], src[0], val);
					MOZ_BLEND(dst[1], dst[1], src[1], val);
					MOZ_BLEND(dst[2], dst[2], src[2], val);
					MOZ_BLEND(*dstAlpha, *dstAlpha, val, val);
				}
			}
			break;
		case 0:
		default:
			// No alpha present, so just memcpy a scanline at a time
			for (int y = 0; y < aDHeight; y++) {
				memcpy(dest->mImageBits + (y + aDY) * dest->mRowBytes + 3 * aDX,
					rgbPtr + y * rgbStride, 3 * aDWidth);
			}
			break;
	}

	if (scaledAlpha) nsMemory::Free(scaledAlpha);
	if (scaledImage) nsMemory::Free(scaledImage);
	
	// ImageUpdated() won't be called in this case, so we need to mark the destination
	// image as changed. This will cause its data to be copied in the BBitmap when it
	// tries to blit. The source has not been modified, so its status has not changed.
	dest->mImageCurrent = PR_FALSE;
	return NS_OK;	
}
