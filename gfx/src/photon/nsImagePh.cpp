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

#include "nsImagePh.h"
#include "nsRenderingContextPh.h"
#include "nsPhGfxLog.h"
#include "nsDeviceContextPh.h"
#include "nspr.h"
#include <errno.h>
#include <Pt.h>
#include <photon/PxImage.h>
#include "photon/PhRender.h"


NS_IMPL_ISUPPORTS1(nsImagePh, nsIImage)

/* for mImageFlags */
#define IMAGE_SHMEM							0x1
#define ALPHA_SHMEM							0x2
#define ZOOM_SHMEM							0x4

#define IMAGE_SHMEM_THRESHOLD	4096

// ----------------------------------------------------------------
nsImagePh :: nsImagePh()
{
	mImageBits = nsnull;
	mWidth = 0;
	mHeight = 0;
	mDepth = 0;
	mAlphaBits = nsnull;
	mAlphaDepth = 0;
	mRowBytes = 0;
	mAlphaHeight = 0;
	mAlphaWidth = 0;
	mImageFlags = 0;
	mAlphaRowBytes = 0;
	mNaturalWidth = 0;
	mNaturalHeight = 0;
	memset(&mPhImage, 0, sizeof(PhImage_t));
	mPhImageZoom = NULL;
	mDecodedY2_when_scaled = 0;
	mDirtyFlags = 0;
}

// ----------------------------------------------------------------
nsImagePh :: ~nsImagePh()
{
  if (mImageBits != nsnull)
  {
  	if( mImageFlags & IMAGE_SHMEM ) PgShmemDestroy( mImageBits );
		else delete [] mImageBits;
    mImageBits = nsnull;
  }

  if (mAlphaBits != nsnull)
  {
		if( mImageFlags & ALPHA_SHMEM ) PgShmemDestroy( mAlphaBits );
    else delete [] mAlphaBits;
    mAlphaBits = nsnull;
  }

	if( mPhImageZoom ) {
		if( mImageFlags & ZOOM_SHMEM ) PgShmemDestroy( mPhImageZoom->image );
		else free( mPhImageZoom->image );
		if( mPhImageZoom->mask_bm )
			free( mPhImageZoom->mask_bm );
		free( mPhImageZoom );
		mPhImageZoom = NULL;
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

	mImageFlags = 0;

	if (mImageBits != nsnull)
	{
		if( mImageFlags & IMAGE_SHMEM ) PgShmemDestroy( mImageBits );
		else delete [] mImageBits;
		mImageBits = nsnull;
	}

	if (mAlphaBits != nsnull)
	{
		if( mImageFlags & ALPHA_SHMEM ) PgShmemDestroy( mAlphaBits );
		else delete [] mAlphaBits;
		mAlphaBits = nsnull;
	}

	if( mPhImageZoom ) {
		if( mImageFlags & ZOOM_SHMEM ) PgShmemDestroy( mPhImageZoom->image );
		else free( mPhImageZoom->image );
		if( mPhImageZoom->mask_bm )
			free( mPhImageZoom->mask_bm );
		free( mPhImageZoom );
		mPhImageZoom = NULL;
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

	/* Allocate the Image Data */
	PRInt32 image_size = mNumBytesPixel * mWidth * mHeight;

	/* TODO: don't allow shared memory contexts if the graphics driver isn't a local device */

  if (image_size >= IMAGE_SHMEM_THRESHOLD)
  {
		mImageBits = (PRUint8 *) PgShmemCreate( image_size, NULL );
		mImageFlags |= IMAGE_SHMEM;
  }
  else
  {
		mImageBits = new PRUint8[ image_size ];
	 	memset( mImageBits, 0, image_size );
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
			{
			mAlphaRowBytes = (aWidth + 7) / 8;
			mAlphaDepth = 1;

			int alphasize = mAlphaRowBytes * aHeight;
			mAlphaBits = new PRUint8[ alphasize ];
			memset( mAlphaBits, 0, alphasize );

			mAlphaWidth = aWidth;
			mAlphaHeight = aHeight;
			}
			break;

		case nsMaskRequirements_kNeeds8Bit:
			{
			mAlphaRowBytes = aWidth;
			mAlphaDepth = 8;

			int alphasize = mAlphaRowBytes * aHeight;
			if( alphasize > IMAGE_SHMEM_THRESHOLD ) {
				mAlphaBits = ( PRUint8 * ) PgShmemCreate( alphasize, NULL );
				mImageFlags |= ALPHA_SHMEM;
				}
			else mAlphaBits = new PRUint8[ alphasize ];
			memset( mAlphaBits, 0, alphasize );

			mAlphaWidth = aWidth;
			mAlphaHeight = aHeight;
			}
			break;
	}

	// mPhImage.image_tag = PtCRC( (char *)mImageBits, image_size );
	mPhImage.image = (char *)mImageBits;
	mPhImage.size.w = mWidth;
	mPhImage.size.h = 0;
	mRowBytes = mPhImage.bpl = mNumBytesPixel * mWidth;
	mPhImage.type = type;
	if (aMaskRequirements == nsMaskRequirements_kNeeds1Bit)
	{
		mPhImage.mask_bm = (char *)mAlphaBits;
		mPhImage.mask_bpl = mAlphaRowBytes;
	}

  	return NS_OK;
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
NS_IMETHODIMP nsImagePh :: Draw(nsIRenderingContext &aContext, nsIDrawingSurface* aSurface,
				 PRInt32 aSX, PRInt32 aSY, PRInt32 aSWidth, PRInt32 aSHeight,
				 PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight)
{
	PhRect_t clip = { {aDX, aDY}, {aDX + aDWidth-1, aDY + aDHeight-1} };
	PhPoint_t pos = { aDX - aSX, aDY - aSY};
	int use_zoom = 0;

	if( !aSWidth || !aSHeight || !aDWidth || !aDHeight ) return NS_OK;

///* ATENTIE */ printf( "this=%p size=%d,%d  src=(%d %d %d %d) dest=(%d %d %d %d)\n",
//this, mPhImage.size.w, mPhImage.size.h, aSX, aSY, aSWidth, aSHeight, aDX, aDY, aDWidth, aDHeight );

		PhDrawContext_t *dc = ((nsDrawingSurfacePh*)aSurface)->GetDC();
		PhGC_t *gc = PgGetGCCx( dc );
		if( (aSWidth != aDWidth || aSHeight != aDHeight) ) {

			/* the case below happens frequently - a 1x1 image needs to be stretched, or a line or a column */
			if( aSWidth == 1 && aSHeight == 1 || aSWidth == 1 && aSHeight == aDHeight || aDHeight == 1 && aSWidth == aDWidth ) {
				/* you can strech the image by drawing it repeateadly */

				PhPoint_t space = { 0, 0 };
				PhPoint_t rep = { aDWidth, aDHeight };

				/* is this a 1x1 transparent image used for spacing? */
				if( mWidth == 1 && mHeight == 1 && mAlphaDepth == 1 && mAlphaBits[0] == 0x0 ) return NS_OK;

    		PgSetMultiClipCx( gc, 1, &clip );
    		if ((mAlphaDepth == 1) || (mAlphaDepth == 0)) {
					if( mImageFlags & IMAGE_SHMEM )
    		  	PgDrawRepPhImageCxv( dc, &mPhImage, 0, &pos, &rep, &space );
					else PgDrawRepPhImageCx( dc, &mPhImage, 0, &pos, &rep, &space );
					}
    		else
    		{
    		  PgMap_t map;
    		  map.dim.w = mAlphaWidth;
    		  map.dim.h = mAlphaHeight;
    		  map.bpl = mAlphaRowBytes;
    		  map.bpp = mAlphaDepth;
    		  map.map = (unsigned char *)mAlphaBits;
    		  PgSetAlphaBlendCx( gc, &map, 0 );

    		  PgAlphaOnCx( gc );
					if( mImageFlags & IMAGE_SHMEM )
    		  	PgDrawRepPhImageCxv( dc, &mPhImage, 0, &pos, &rep, &space );
    		  else PgDrawRepPhImageCx( dc, &mPhImage, 0, &pos, &rep, &space );
    		  PgAlphaOffCx( gc );

					PgSetAlphaBlendCx( gc, NULL, 0 ); /* this shouldn't be necessary, but the ph lib's gc is holding onto our mAlphaBits */
    		}
    		PgSetMultiClipCx( gc, 0, NULL );

				return NS_OK;
				}

			else if( mPhImage.size.h > 0 ) {

				/* keeping the proportions, what is the size of mPhImageZoom that can give use the aDWidth and aDHeight? */
				PRInt32 scaled_w = aDWidth * mPhImage.size.w / aSWidth;
				PRInt32 scaled_h = aDHeight * mPhImage.size.h / aSHeight;
				use_zoom = 1;
				
				if( mPhImageZoom == NULL || mPhImageZoom->size.w != scaled_w || mPhImageZoom->size.h != scaled_h || mDecodedY2_when_scaled != mDecodedY2 || mDirtyFlags != 0 ) {

					/* we already had a scaled image, but the scaling factor was different from what we need now */
					mDirtyFlags = 0;

        	if ( mPhImageZoom ) {
						if( mImageFlags & ZOOM_SHMEM ) {
							PgFlushCx( dc );
							PgShmemDestroy( mPhImageZoom->image );
							}
						else free( mPhImageZoom->image );
						if( mPhImageZoom->mask_bm )
					    free( mPhImageZoom->mask_bm );
						free( mPhImageZoom );
						mPhImageZoom = NULL;
        	  }

					/* record the mDecodedY1 at the time of scaling */
					mDecodedY2_when_scaled = mDecodedY2;

					/* this is trying to estimate the image data size of the zoom image */
					if (( mPhImage.bpl * scaled_w * scaled_h / mPhImage.size.w ) < IMAGE_SHMEM_THRESHOLD) {
						mPhImageZoom = PiResizeImage( &mPhImage, NULL, scaled_w, scaled_h, Pi_USE_COLORS);
						mImageFlags &= ~ZOOM_SHMEM;
						}
					else {
						mPhImageZoom = PiResizeImage( &mPhImage, NULL, scaled_w, scaled_h, Pi_USE_COLORS|Pi_SHMEM);
						mImageFlags |= ZOOM_SHMEM;
						}

///* ATENTIE */ printf( "\t\t\tzoom from=%d,%d to=%d,%d\n", mPhImage.size.w, mPhImage.size.h, scaled_w, scaled_h );
					}
				}
			}

		PgSetMultiClipCx( gc, 1, &clip );
		if ((mAlphaDepth == 1) || (mAlphaDepth == 0)) {
			if( use_zoom ) {
				if( mImageFlags & ZOOM_SHMEM )
					PgDrawPhImageCxv( dc, &pos, mPhImageZoom, 0 );
				else PgDrawPhImageCx( dc, &pos, mPhImageZoom, 0 );
				}
			else {
				if( mImageFlags & IMAGE_SHMEM )
					PgDrawPhImageCxv( dc, &pos, &mPhImage, 0 );
				else PgDrawPhImageCx( dc, &pos, &mPhImage, 0 );
				}
			}
		else
		{
			PgMap_t map;

			map.dim.w = mAlphaWidth;
			map.dim.h = mAlphaHeight;
			map.bpl = mAlphaRowBytes;
			map.bpp = mAlphaDepth;
			map.map = (unsigned char *)mAlphaBits;
			PgSetAlphaBlendCx( gc, &map, 0 );

			PgAlphaOnCx( gc );
			if( use_zoom ) {
				if( mImageFlags & ZOOM_SHMEM )
					PgDrawPhImageCxv( dc, &pos, mPhImageZoom, 0 );
				else PgDrawPhImageCx( dc, &pos, mPhImageZoom, 0 );
				}
			else {
				if( mImageFlags & IMAGE_SHMEM )
					PgDrawPhImageCxv( dc, &pos, &mPhImage, 0 );
				else PgDrawPhImageCx( dc, &pos, &mPhImage, 0 );
				}
			PgAlphaOffCx( gc );

			PgSetAlphaBlendCx( gc, NULL, 0 ); /* this shouldn't be necessary, but the ph lib's gc is holding onto our mAlphaBits */
		}
		PgSetMultiClipCx( gc, 0, NULL );

  	return NS_OK;
}

NS_IMETHODIMP nsImagePh :: Draw(nsIRenderingContext &aContext, nsIDrawingSurface* aSurface,
				 PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  	PhPoint_t pos = { aX, aY };

	if (!aSurface || !mImageBits)
		return (NS_ERROR_FAILURE);

	PhDrawContext_t *dc = ((nsDrawingSurfacePh*)aSurface)->GetDC();

	if ((mAlphaDepth == 1) || (mAlphaDepth == 0))
	{
		if( mImageFlags & IMAGE_SHMEM )
			PgDrawPhImageCxv( dc, &pos, &mPhImage, 0 );
		else PgDrawPhImageCx( dc, &pos, &mPhImage, 0 );
	}
	else if (mAlphaDepth == 8)
	{
		PgMap_t map;
		PhGC_t *gc = PgGetGCCx( dc );

		map.dim.w = mAlphaWidth;
		map.dim.h = mAlphaHeight;
		map.bpl = mAlphaRowBytes;
		map.bpp = mAlphaDepth;
		map.map = (unsigned char *)mAlphaBits;
		PgSetAlphaBlendCx( gc, &map, 0 );

		PgAlphaOnCx( gc );
		if( mImageFlags & IMAGE_SHMEM )
			PgDrawPhImageCxv( dc, &pos, &mPhImage, 0 );
		else PgDrawPhImageCx( dc, &pos, &mPhImage, 0 );
		PgAlphaOffCx( gc );

		PgSetAlphaBlendCx( gc, NULL, 0 ); /* this shouldn't be necessary, but the ph lib's gc is holding onto our mAlphaBits */
	}

 	return NS_OK;
}

/* New Tile code *********************************************************************/
NS_IMETHODIMP nsImagePh::DrawTile( nsIRenderingContext &aContext, nsIDrawingSurface* aSurface,
		PRInt32 aSXOffset, PRInt32 aSYOffset, PRInt32 aPadX, PRInt32 aPadY, const nsRect &aTileRect ) 
{
	PhPoint_t pos, space, rep;
	PhDrawContext_t *dc ;
	PhGC_t *gc;

	/* is this a 1x1 transparent image used for spacing? */
	if( mWidth == 1 && mHeight == 1 && mAlphaDepth == 1 && mAlphaBits[0] == 0x0 ) return NS_OK;

	dc = ((nsDrawingSurfacePh*)aSurface)->GetDC();
	gc = PgGetGCCx( dc );

	// since there is an offset into the image and I only want to draw full
	// images, shift the position back and set clipping so that it looks right
	pos.x = aTileRect.x - aSXOffset;
	pos.y = aTileRect.y - aSYOffset;

	space.x = mPhImage.size.w + aPadX;
	space.y = mPhImage.size.h + aPadY;
	rep.x = ( aTileRect.width + aSXOffset + space.x - 1 ) / space.x;
	rep.y = ( aTileRect.height + aSYOffset + space.y - 1 ) / space.y;

	/* not sure if cliping is necessary */
	PhRect_t clip = { {aTileRect.x, aTileRect.y}, {aTileRect.x + aTileRect.width-1, aTileRect.y + aTileRect.height-1} };
	PgSetMultiClipCx( gc, 1, &clip );

	if ((mAlphaDepth == 1) || (mAlphaDepth == 0)) {
		if( mImageFlags & IMAGE_SHMEM )
 	  	PgDrawRepPhImageCxv( dc, &mPhImage, 0, &pos, &rep, &space );
 	  else PgDrawRepPhImageCx( dc, &mPhImage, 0, &pos, &rep, &space );
		}
 	else
 		{
    PgMap_t map;
    map.dim.w = mAlphaWidth;
    map.dim.h = mAlphaHeight;
    map.bpl = mAlphaRowBytes;
    map.bpp = mAlphaDepth;
    map.map = (unsigned char *)mAlphaBits;
    PgSetAlphaBlendCx( gc, &map, 0 );
	
    PgAlphaOnCx( gc );
		if( mImageFlags & IMAGE_SHMEM )
    	PgDrawRepPhImageCxv( dc, &mPhImage, 0, &pos, &rep, &space );
    else PgDrawRepPhImageCx( dc, &mPhImage, 0, &pos, &rep, &space );
    PgAlphaOffCx( gc );

		PgSetAlphaBlendCx( gc, NULL, 0 ); /* this shouldn't be necessary, but the ph lib's gc is holding onto our mAlphaBits */
 	 	}

	PgSetMultiClipCx( gc, 0, NULL );

	return NS_OK;
}
/* End New Tile code *****************************************************************/


//------------------------------------------------------------
nsresult nsImagePh :: Optimize(nsIDeviceContext* aContext)
{
	/* TODO: need to invalidate the caches if the graphics system has changed somehow (ie: mode switch, dragged to a remote
			display, etc... */

	/* TODO: remote graphics drivers should always cache the images offscreen regardless of wether there is a mask or not,
			the Draw code will need to be updated for this */

	return NS_OK;
}


 /**
  * BitBlit the entire (no cropping) nsIImage to another nsImage, the source and dest can be scaled
  * @update - saari 03/08/01
  * @param aDstImage  the nsImage to blit to
  * @param aDX The destination horizontal location
  * @param aDY The destination vertical location
  * @param aDWidth The destination width of the pixelmap
  * @param aDHeight The destination height of the pixelmap
  * @return if TRUE, no errors
  */
NS_IMETHODIMP nsImagePh::DrawToImage(nsIImage* aDstImage,
                                      nscoord aDX, nscoord aDY,
                                      nscoord aDWidth, nscoord aDHeight)
{
  nsImagePh *dest = NS_STATIC_CAST(nsImagePh *, aDstImage);

  if (!dest)
    return NS_ERROR_FAILURE;

  if (!dest->mPhImage.image)
    return NS_ERROR_FAILURE;

	PhArea_t sarea, darea;
	PhImage_t *pimage = NULL;
	int start, x, y;
	char mbit, mbyte;
	int release = 0;
		
	sarea.pos.x = sarea.pos.y = 0;
	darea.pos.x = aDX;
	darea.pos.y = aDY;
	darea.size.w = sarea.size.w = aDWidth;
	darea.size.h = sarea.size.h = aDHeight;

	if( mPhImage.size.h > 0 && ( aDWidth != mPhImage.size.w || aDHeight != mPhImage.size.h ) )
	{
		release = 1;
		if ((aDHeight * mPhImage.bpl) < IMAGE_SHMEM_THRESHOLD)
			pimage = PiResizeImage(&mPhImage, NULL, aDWidth, aDHeight, Pi_USE_COLORS);
		else
			pimage = PiResizeImage(&mPhImage, NULL, aDWidth, aDHeight, Pi_USE_COLORS|Pi_SHMEM);
	}
	else pimage = &mPhImage;

	if( pimage == NULL ) return NS_OK;

	start = (aDY * dest->mPhImage.bpl) + (aDX * mNumBytesPixel);
	for (y = 0; y < pimage->size.h; y++)
	{
		for (x = 0; x < pimage->size.w; x++)
		{
			if (pimage->mask_bm)
			{
				mbyte = *(pimage->mask_bm + (pimage->mask_bpl * y) + (x >> 3));
				mbit = x & 7;
				if (!(mbyte & (0x80 >> mbit)))
					continue;
			}
			memcpy(dest->mPhImage.image + start + (dest->mPhImage.bpl * y) + (x*mNumBytesPixel), 
				   pimage->image + (pimage->bpl * y) + (x*mNumBytesPixel), mNumBytesPixel);
		}
	}

	if( release ) {
		pimage->flags = Ph_RELEASE_IMAGE_ALL;
		PhReleaseImage(pimage);
		free( pimage );
		}

	return NS_OK;
}
