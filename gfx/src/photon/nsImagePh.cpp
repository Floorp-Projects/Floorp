/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsImagePh.h"
#include "nsRenderingContextPh.h"
#include "nsPhGfxLog.h"

#include "nspr.h"

#define IsFlagSet(a,b) (a & b)

// static NS_DEFINE_IID(kIImageIID, NS_IIMAGE_IID);

NS_IMPL_ISUPPORTS1(nsImagePh, nsIImage)

#define IMAGE_SHMEM				0x1
#define IMAGE_SHMEM_THRESHOLD	1024

// ----------------------------------------------------------------
nsImagePh :: nsImagePh()
{
	NS_INIT_REFCNT();
	mImageBits = nsnull;
	mWidth = 0;
	mHeight = 0;
	mDepth = 0;
	mAlphaBits = nsnull;
	mAlphaDepth = 0;
	mRowBytes = 0;
	mSizeImage = 0;
	mAlphaHeight = 0;
	mAlphaWidth = 0;
	mConvertedBits = nsnull;
	mImageFlags = 0;
	mAlphaRowBytes = 0;
	mNaturalWidth = 0;
	mNaturalHeight = 0;
	memset(&mPhImage, 0, sizeof(PhImage_t));
}

// ----------------------------------------------------------------
nsImagePh :: ~nsImagePh()
{
  if (mImageBits != nsnull)
  {
  	if (mImageFlags & IMAGE_SHMEM)
  		PgShmemDestroy(mImageBits);
  	else
		delete [] mImageBits;
    mImageBits = nsnull;
  }

  if (mAlphaBits != nsnull)
  {
    delete [] mAlphaBits;
    mAlphaBits = nsnull;
  }

  memset(&mPhImage, 0, sizeof(PhImage_t));
}

/** ----------------------------------------------------------------
 * Initialize the nsImagePh object
 * @param aWidth - Width of the image
 * @param aHeight - Height of the image
 * @param aDepth - Depth of the image
 * @param aMaskRequirements - A mask used to specify if alpha is needed.
 * @result NS_OK if the image was initied ok
 */
nsresult nsImagePh :: Init(PRInt32 aWidth, PRInt32 aHeight, PRInt32 aDepth,nsMaskRequirements aMaskRequirements)
{
	int type = -1;

	if (mImageBits != nsnull)
	{
		if (mImageFlags & IMAGE_SHMEM)
			PgShmemDestroy(mImageBits);
		else
			delete [] mImageBits;
		mImageBits = nsnull;
	}

	if (mAlphaBits != nsnull)
	{
		delete [] mAlphaBits;
		mAlphaBits = nsnull;
	}
  
  	SetDecodedRect(0,0,0,0);  //init
 
    switch (aDepth)
    {
        case 24:
            type = Pg_IMAGE_DIRECT_888;
            mNumBytesPixel = 3;
            break;
//      case 16:
//          type = Pg_IMAGE_DIRECT_555;
//          mNumBytesPixel = 2;
//          break;
      case 8:
//          type = Pg_IMAGE_PALETTE_BYTE;
//          mNumBytesPixel = 1;
//          break;
        default:
            NS_ASSERTION(PR_FALSE, "unexpected image depth");
            return NS_ERROR_UNEXPECTED;
            break;
    }
 
	mWidth = aWidth;
	mHeight = aHeight;
	mDepth = aDepth;
	mIsTopToBottom = PR_TRUE;

  	/* Allocate the Image Data */
	mSizeImage = mNumBytesPixel * mWidth * mHeight;
  	if (mWidth >= IMAGE_SHMEM_THRESHOLD)
	  	mImageBits = (PRUint8*) PgShmemCreate(mSizeImage,0);
  	else
  	{
	  	mImageBits = (PRUint8*) new PRUint8[mSizeImage];
	  	memset(mImageBits, 0, mSizeImage);
	}

	switch(aMaskRequirements)
	{
		default:
		case nsMaskRequirements_kNoMask:
			mAlphaBits = nsnull;
			mAlphaWidth = 0;
			mAlphaHeight = 0;
			mAlphaRowBytes = 0;
			break;

		case nsMaskRequirements_kNeeds1Bit:
			mAlphaRowBytes = (aWidth + 7) / 8;
			mAlphaDepth = 1;

			// 32-bit align each row
			mAlphaRowBytes = (mAlphaRowBytes + 3) & ~0x3;

			mAlphaBits = new PRUint8[mAlphaRowBytes * aHeight];
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

	mPhImage.image_tag = PtCRC((char *)mImageBits, mSizeImage);
	mPhImage.image = (char *)mImageBits;
	mPhImage.size.w = mWidth;
	mPhImage.size.h = mHeight;
	mRowBytes = mPhImage.bpl = mNumBytesPixel * mWidth;
	mPhImage.type = type;
	if (aMaskRequirements == nsMaskRequirements_kNeeds1Bit)
	{
		mPhImage.mask_bm = (char *)mAlphaBits;
		mPhImage.mask_bpl = mAlphaRowBytes;
	}

  	return NS_OK;
}

//------------------------------------------------------------

PRInt32 nsImagePh::GetHeight()
{
  	return mHeight;
}

PRInt32 nsImagePh::GetWidth()
{
  	return mWidth;
}

PRUint8* nsImagePh::GetBits()
{
  	return mImageBits;
}

void* nsImagePh::GetBitInfo()
{
  	return nsnull;
}

PRInt32 nsImagePh::GetLineStride()
{
  	return mRowBytes;
}

nsColorMap* nsImagePh::GetColorMap()
{
  	return nsnull;
}

PRBool nsImagePh::IsOptimized()
{
  	return PR_TRUE;
}

PRUint8* nsImagePh::GetAlphaBits()
{
  	return mAlphaBits;
}

PRInt32 nsImagePh::GetAlphaWidth()
{
  	return mAlphaWidth;
}

PRInt32 nsImagePh::GetAlphaHeight()
{
  	return mAlphaHeight;
}

PRInt32 nsImagePh::GetAlphaLineStride()
{
  	return mAlphaRowBytes;
}

nsIImage *nsImagePh::DuplicateImage()
{
  	return nsnull;
}

void nsImagePh::SetAlphaLevel(PRInt32 aAlphaLevel)
{
  	mAlphaLevel=aAlphaLevel;
}

PRInt32 nsImagePh::GetAlphaLevel()
{
  	return(mAlphaLevel);
}

void nsImagePh::MoveAlphaMask(PRInt32 aX, PRInt32 aY)
{
}

void nsImagePh :: ImageUpdated(nsIDeviceContext *aContext, PRUint8 aFlags, nsRect *aUpdateRect)
{
  	mFlags = aFlags; // this should be 0'd out by Draw()
}

/** ----------------------------------------------------------------
  * Draw the bitmap, this method has a source and destination coordinates
  * @update dc - 11/20/98
  * @param aContext - the rendering context to draw with
  * @param aSurface - The HDC in a nsDrawingsurfacePh to copy the bits to.
  * @param aSX - source horizontal location
  * @param aSY - source vertical location
  * @param aSWidth - source width
  * @param aSHeight - source height
  * @param aDX - destination location
  * @param aDY - destination location
  * @param aDWidth - destination width
  * @param aDHeight - destination height
  * @result NS_OK if the draw worked
  */
NS_IMETHODIMP nsImagePh :: Draw(nsIRenderingContext &aContext, nsDrawingSurface aSurface,
				 PRInt32 aSX, PRInt32 aSY, PRInt32 aSWidth, PRInt32 aSHeight,
				 PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight)
{
	return (Draw(aContext, aSurface, aDX, aDY, aDWidth,  aDHeight));
}

NS_IMETHODIMP nsImagePh :: Draw(nsIRenderingContext &aContext, nsDrawingSurface aSurface,
				 PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  	PhPoint_t pos = { aX, aY };
  	int       err;

	if (!aSurface || !mImageBits)
		return (NS_ERROR_FAILURE);
  
  	nsDrawingSurfacePh* drawing = (nsDrawingSurfacePh*) aSurface;

	if ((mAlphaDepth == 1) || (mAlphaDepth == 0))
	{
		PgDrawPhImagemx(&pos, &mPhImage, 0);
	}
	else if (mAlphaDepth == 8)
	{
	//	memset(&pg_alpha, 0, sizeof(PgAlpha_t));
	//	pg_alpha.dest_alpha_map = mAlphaBits;
	//	mPhImage.alpha = &pg_alpha;
	//	PgDrawPhImagemx(&pos, &mPhImage, 0);
	//	mPhImage.alpha = nsnull;
		printf("DRAW IMAGE: with 8 bit alpha!!\n");
	}

  	return NS_OK;
}

/* New Tile code *********************************************************************/
// this should be changed to use PhDrawRepImagemx when that function supports transparency masks

NS_IMETHODIMP nsImagePh::DrawTile(nsIRenderingContext &aContext,
					  nsDrawingSurface aSurface,
					  nsRect &aSrcRect,
					  nsRect &aTileRect)
{
	PhPoint_t pos;
	int       err;
	PhPoint_t space, rep;
	int x, y;
	PhRect_t clip;

	pos.x = aTileRect.x;
	pos.y = aTileRect.y;
	space.x = aSrcRect.width;
	space.y = aSrcRect.height;

	// clip to the area we want tiled
	clip.ul.x = aTileRect.x;
	clip.ul.y = aTileRect.y;
	clip.lr.x = aTileRect.x + aTileRect.width;
	clip.lr.y = aTileRect.y + aTileRect.height;
	PgSetMultiClip(1, &clip);

	for (; pos.y < clip.lr.y; pos.y += space.y)
	{
		for (pos.x = aTileRect.x; pos.x < clip.lr.x; pos.x += space.x)
			Draw(aContext, aSurface, pos.x, pos.y, aSrcRect.width, aSrcRect.height);
	}

	// remove my clipping
	PgSetMultiClip(0, NULL);
}


NS_IMETHODIMP nsImagePh::DrawTile(nsIRenderingContext &aContext,
                                   nsDrawingSurface aSurface,
                                   PRInt32 aSXOffset, PRInt32 aSYOffset,
                                   const nsRect &aTileRect)
{
  PhPoint_t pos;
  int       err;
  PhPoint_t space, rep;
  int x, y;

	// since there is an offset into the image and I only want to draw full
	// images, shift the position back and set clipping so that it looks right
	pos.x = aTileRect.x - aSXOffset;
	pos.y = aTileRect.y - aSYOffset;

	space.x = mPhImage.size.w;
	space.y = mPhImage.size.h;
	rep.x = (aTileRect.width + aSXOffset + space.x - 1)/space.x;
	rep.y = (aTileRect.height + aSYOffset + space.y - 1)/space.y;

  PhRect_t clip;
  clip.ul.x = aTileRect.x;
  clip.ul.y = aTileRect.y;
  clip.lr.x = aTileRect.x + aTileRect.width;
  clip.lr.y = aTileRect.y + aTileRect.height;
  PgSetMultiClip(1, &clip);
    //for (y = 0; y < rep.y; y++, pos.y += space.y)
    for (; pos.y < clip.lr.y; pos.y += space.y)
    {
		//for (x = 0, pos.x = aTileRect.x/* - aSXOffset*/; x < rep.x; x++, pos.x += space.x)
		for (pos.x = aTileRect.x - aSXOffset; pos.x < clip.lr.x; pos.x += space.x)
		{
			Draw(aContext, aSurface, pos.x, pos.y, mPhImage.size.w, mPhImage.size.h);
		}
	}
  PgSetMultiClip(0, NULL);

  return NS_OK;
}
/* End New Tile code *****************************************************************/


//------------------------------------------------------------
nsresult nsImagePh :: Optimize(nsIDeviceContext* aContext)
{
  return NS_OK;
}


//------------------------------------------------------------
// lock the image pixels. Implement this if you need it.
NS_IMETHODIMP
nsImagePh::LockImagePixels(PRBool aMaskPixels)
{
  	return NS_OK;
}

//------------------------------------------------------------
// unlock the image pixels.  Implement this if you need it.
NS_IMETHODIMP
nsImagePh::UnlockImagePixels(PRBool aMaskPixels)
{
  	return NS_OK;
}

/** ---------------------------------------------------
 *	Set the decoded dimens of the image
 */
NS_IMETHODIMP
nsImagePh::SetDecodedRect(PRInt32 x1, PRInt32 y1, PRInt32 x2, PRInt32 y2 )
{
  	mDecodedX1 = x1; 
  	mDecodedY1 = y1; 
  	mDecodedX2 = x2; 
  	mDecodedY2 = y2; 
  	return NS_OK;
}
