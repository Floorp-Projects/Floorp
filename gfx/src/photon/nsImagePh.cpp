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

#include "nsImagePh.h"
#include "nsRenderingContextPh.h"
#include "nsPhGfxLog.h"
#include "nsDeviceContextPh.h"
#include "nspr.h"
#include <errno.h>
#include <Pt.h>
#include <photon/PxImage.h>
#include "photon/PhRender.h"

/*******************************************************************/
#define GR_FPMUL(a,b)	((((uint64_t)(a)) * ((uint64_t)(b))) >> 16)
#define GR_FPDIV(a,b)	((((uint64_t)(a)) << 16) / ((uint64_t)(b)))

PhImage_t *MyPiResizeImage(PhImage_t *image,PhRect_t const *bounds,short w,short h,int flags)
{
	PhRect_t oldRect,newRect,srcRect;
	PhPoint_t dst,src;
	PhImage_t *newImage;
	unsigned long value,xScale,yScale,xScaleInv = 0,yScaleInv = 0;
	char *ptr,*new_mask_ptr;
	char const *old_mask_ptr;
	struct
	{
		unsigned long x,y;
	} next;

	if(bounds)
		srcRect = *bounds;
	else
	{
		srcRect.ul.x = srcRect.ul.y = 0;
		srcRect.lr.x = image->size.w - 1;
		srcRect.lr.y = image->size.h - 1;
	}
	
	oldRect.ul.x = oldRect.ul.y = 0;
	oldRect.lr.x = w - 1;
	oldRect.lr.y = h - 1;
			
	if(!(newImage = PiInitImage(image,(PhRect_t const*)(&oldRect),&newRect,image->type,flags | Pi_USE_COLORS,image->colors)))
		return(NULL);

	memset(newImage->image, 0x22, newImage->bpl*newImage->size.h);
	
	if((xScale = GR_FPDIV(newImage->size.w << 16,(srcRect.lr.x - srcRect.ul.x + 1) << 16)) <= 0x10000)
		xScaleInv = GR_FPDIV((srcRect.lr.x - srcRect.ul.x + 1) << 16,newImage->size.w << 16);
	if((yScale = GR_FPDIV(newImage->size.h << 16,(srcRect.lr.y - srcRect.ul.y + 1) << 16)) <= 0x10000)
		yScaleInv = GR_FPDIV((srcRect.lr.y - srcRect.ul.y + 1) << 16,newImage->size.h << 16);

	for(dst.y = 0,next.y = srcRect.ul.y << 16,src.y = srcRect.ul.y,ptr = newImage->image,old_mask_ptr = image->mask_bm,new_mask_ptr = newImage->mask_bm;
	  dst.y < newImage->size.h;
	  dst.y++,ptr += newImage->bpl,new_mask_ptr += newImage->mask_bpl)
	{
		if(yScaleInv)
		{
			src.y = next.y >> 16;
			next.y += yScaleInv;
		}
		else
		{
			if(dst.y < (next.y >> 16) || src.y > srcRect.lr.y)
			{
				memcpy(ptr,ptr - newImage->bpl,newImage->bpl);
				if(newImage->mask_bm)
					memcpy(new_mask_ptr,new_mask_ptr - newImage->mask_bpl,newImage->mask_bpl);

				continue;
			}
			else
				next.y += yScale;
		}

		if(newImage->mask_bm)
			old_mask_ptr = image->mask_bm + (src.y * image->mask_bpl);

		if(xScaleInv)
		{
			for(dst.x = 0,next.x = srcRect.ul.x << 16,src.x = srcRect.ul.x;
			  dst.x < newImage->size.w;dst.x++,next.x += xScaleInv)
			{
				src.x = next.x >> 16;
				PiGetPixel((const PhImage_t *)image,src.x,src.y,&value);
				PiSetPixel(newImage,dst.x,dst.y,value);
				
				if(newImage->mask_bm && !PiGetPixelFromData((const uint8_t *)old_mask_ptr,Pg_BITMAP_TRANSPARENT,src.x,&value))
				{
					if(!value)
						PiSetPixelInData((uint8_t *)new_mask_ptr,Pg_BITMAP_TRANSPARENT,dst.x,0);
				}
			}
		}
		else
		{
			unsigned long color = 0;
			
			for(dst.x = 0,next.x = srcRect.ul.x << 16,src.x = srcRect.ul.x;
			  dst.x < newImage->size.w;dst.x++)
			{
				if(dst.x == (next.x >> 16) && src.y <= srcRect.lr.y)
				{
					if(!PiGetPixel((const PhImage_t *)image,src.x,src.y,&color) && (!newImage->mask_bm || PiGetPixelFromData((const uint8_t *)old_mask_ptr,Pg_BITMAP_TRANSPARENT,src.x,&value)))
						value = 1;

					next.x += xScale;
					
					src.x++;
				}
					
				PiSetPixel(newImage,dst.x,dst.y,color);
				
				if(!value)
					PiSetPixelInData((uint8_t *)new_mask_ptr,Pg_BITMAP_TRANSPARENT,dst.x,0);
			}
		}
		
		if(!yScaleInv)
			src.y++;
	}

	/* Calculate CRC tags for new image */

	if(!(flags & Pi_SUPPRESS_CRC))
		newImage->image_tag = PtCRC(newImage->image,newImage->bpl * newImage->size.h);

	if(flags & Pi_FREE)
	{
		image->flags |= Ph_RELEASE_IMAGE | Ph_RELEASE_PALETTE | Ph_RELEASE_TRANSPARENCY_MASK | Ph_RELEASE_GHOST_BITMAP;
		PhReleaseImage(image);
	}

	return(newImage);	
}
/*******************************************************************/

//#define ALLOW_PHIMAGE_CACHEING
#define IsFlagSet(a,b) (a & b)

// static NS_DEFINE_IID(kIImageIID, NS_IIMAGE_IID);

NS_IMPL_ISUPPORTS1(nsImagePh, nsIImage)

#define IMAGE_SHMEM				0x1
#define IMAGE_SHMEM_THRESHOLD	4096

// ----------------------------------------------------------------
nsImagePh::nsImagePh()
: mImageBits(nsnull)
, mWidth(0)
, mHeight(0)
, mDepth(0)
, mAlphaBits(nsnull)
, mAlphaDepth(0)
, mRowBytes(0)
, mSizeImage(0)
, mDecodedX1(0)
, mDecodedY1(0)
, mDecodedX2(0)
, mDecodedY2(0)
, mConvertedBits(nsnull)
, mImageFlags(0)
, mAlphaRowBytes(0)
, mIsOptimized(PR_FALSE)
, mPhImageCache(nsnull)
, mPhImageZoom(nsnull)
{
	memset(&mPhImage, 0, sizeof(PhImage_t));
}

// ----------------------------------------------------------------
nsImagePh :: ~nsImagePh()
{
  if (mImageBits != nsnull)
  {
  	if (mImageFlags & IMAGE_SHMEM)
  		DestroySRamImage(mImageBits);
  	else 
		delete [] mImageBits;
    mImageBits = nsnull;
  }

  if (mConvertedBits != nsnull)
	DestroySRamImage(mConvertedBits);

  if (mPhImageCache)
  {
	PhDCRelease(mPhImageCache);
	mPhImageCache=NULL;
  }

  if (mAlphaBits != nsnull)
  {
    delete [] mAlphaBits;
    mAlphaBits = nsnull;
  }

	if( mPhImageZoom ) {
		if ( PgShmemDestroy( mPhImageZoom->image ) == -1)
			free(mPhImageZoom->image);
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
  // gfxIImageFrame protects nsImagePh from multiple Init calls
	int type = -1;

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
	mSizeImage = mNumBytesPixel * mWidth * mHeight;

	/* TODO: don't allow shared memory contexts if the graphics driver isn't a local device */

  	if (mSizeImage >= IMAGE_SHMEM_THRESHOLD)
  	{
		mImageBits = CreateSRamImage(mSizeImage);
		mImageFlags |= IMAGE_SHMEM;
//	  	mImageBits = (PRUint8*) PgShmemCreate(mSizeImage,0);
  	}
  	else
  	{
	  	//mImageBits = (PRUint8*) new PRUint8[mSizeImage];
	  	mImageBits = new PRUint8[mSizeImage];
	  	memset(mImageBits, 0, mSizeImage);
	  	mImageFlags &= ~IMAGE_SHMEM;
	}

//	mImageCache = PdCreateOffscreenContext(0, aWidth, aHeight, 0);

	switch(aMaskRequirements)
	{
		case nsMaskRequirements_kNeeds1Bit:
			mAlphaRowBytes = (aWidth + 7) / 8;
			mAlphaDepth = 1;

			// 32-bit align each row
			mAlphaRowBytes = (mAlphaRowBytes + 3) & ~0x3;

			mAlphaBits = new PRUint8[mAlphaRowBytes * aHeight];
			break;

		case nsMaskRequirements_kNeeds8Bit:
			mAlphaRowBytes = aWidth;
			mAlphaDepth = 8;

			// 32-bit align each row
			mAlphaRowBytes = (mAlphaRowBytes + 3) & ~0x3;
			mAlphaBits = new PRUint8[mAlphaRowBytes * aHeight];
			break;

    default:
      break; // avoid compiler warning
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

/* Creates a shared memory image if allowed...otherwise mallocs it... */
PRUint8 * nsImagePh::CreateSRamImage (PRUint32 size)
{
	/* TODO: add code to check for remote drivers (no shmem then) */
	return (PRUint8 *) PgShmemCreate(size,NULL);
}

PRBool nsImagePh::DestroySRamImage (PRUint8 * ptr)
{
	PgShmemDestroy(ptr);
	return PR_TRUE;
}

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
  	return mIsOptimized;
}

PRUint8* nsImagePh::GetAlphaBits()
{
  	return mAlphaBits;
}

PRInt32 nsImagePh::GetAlphaLineStride()
{
  	return mAlphaRowBytes;
}

void nsImagePh :: ImageUpdated(nsIDeviceContext *aContext, PRUint8 aFlags, nsRect *aUpdateRect)
{
	/* does this mean it's dirty? */
  	mFlags = aFlags; // this should be 0'd out by Draw()
  if (aUpdateRect->YMost() > mDecodedY2)
    mDecodedY2 = aUpdateRect->YMost();
  if (aUpdateRect->XMost() > mDecodedX2)
    mDecodedX2 = aUpdateRect->XMost();
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
	PhRect_t clip = { {aDX, aDY}, {aDX + aDWidth-1, aDY + aDHeight-1} };
	PhPoint_t pos = { aDX - aSX, aDY - aSY};

	if( !aSWidth || !aSHeight || !aDWidth || !aDHeight ) return NS_OK;

#ifdef ALLOW_PHIMAGE_CACHEING
  	PRBool 	aOffScreen;
	nsDrawingSurfacePh* drawing = (nsDrawingSurfacePh*) aSurface;

	drawing->IsOffscreen(&aOffScreen);
	if (!mPhImage.mask_bm && mIsOptimized && mPhImageCache && aOffScreen)
	{
		PhArea_t sarea, darea;

		sarea.pos.x = aSX;
		sarea.pos.y = aSY;
		darea.pos.x = aDX;
		darea.pos.y = aDY;
		darea.size.w=sarea.size.w=aDWidth;
		darea.size.h=sarea.size.h=aDHeight;
		
		PgContextBlitArea(mPhImageCache,&sarea,(PdOffscreenContext_t *)drawing->GetDC(),&darea);
		PgFlush();	
	}
	else
#endif
	{
	#if 0
		if( (aSWidth != aDWidth || aSHeight != aDHeight) && 
		   (mPhImageZoom == NULL || mPhImageZoom->size.w != aDWidth || mPhImageZoom->size.h != aDHeight)) 
		{
			if ( mPhImageZoom ) {
				if ( PgShmemDestroy( mPhImageZoom->image ) == -1 )
					free(mPhImageZoom->image);
				free( mPhImageZoom );
			}
			PhRect_t bounds = {{aSX, aSY}, {aSX + aSWidth - 1, aSY + aSHeight - 1}};

			if ((aDHeight * mPhImage.bpl) < IMAGE_SHMEM_THRESHOLD)
				mPhImageZoom = MyPiResizeImage(&mPhImage, &bounds, aDWidth, aDHeight, Pi_USE_COLORS);
			else
				mPhImageZoom = MyPiResizeImage(&mPhImage, &bounds, aDWidth, aDHeight, Pi_USE_COLORS|Pi_SHMEM);
			if ( mPhImageZoom == NULL )
				return NS_OK;
			pos.x = aDX;
			pos.y = aDY;
		}
	#endif

		PgSetMultiClip( 1, &clip );
		if ((mAlphaDepth == 1) || (mAlphaDepth == 0))
			PgDrawPhImagemx( &pos, mPhImageZoom ? mPhImageZoom : &mPhImage, 0 );
		else
		{
			PgMap_t map;

			map.dim.w = mWidth;
			map.dim.h = mHeight;
			map.bpl = mAlphaRowBytes;
			map.bpp = mAlphaDepth;
			map.map = (char *)mAlphaBits;
			PgSetAlphaBlend(&map, 0);
			PgAlphaOn();
			PgDrawPhImagemx( &pos, mPhImageZoom ? mPhImageZoom : &mPhImage, 0 );
			PgAlphaOff();
			
		}
		PgSetMultiClip( 0, NULL );
	}


  	return NS_OK;
}

NS_IMETHODIMP nsImagePh :: Draw(nsIRenderingContext &aContext, nsDrawingSurface aSurface,
				 PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  	PhPoint_t pos = { aX, aY };

	if (!aSurface || !mImageBits)
		return (NS_ERROR_FAILURE);

#ifdef ALLOW_PHIMAGE_CACHEING
  	PRBool 	aOffScreen;
  	nsDrawingSurfacePh* drawing = (nsDrawingSurfacePh*) aSurface;

	drawing->IsOffscreen(&aOffScreen);
	if (!mPhImage.mask_bm && mIsOptimized && mPhImageCache && aOffScreen)
	{
		PhArea_t sarea, darea;

		sarea.pos.x=sarea.pos.y=0;
		darea.pos=pos;
		darea.size.w=sarea.size.w=aWidth;
		darea.size.h=sarea.size.h=aHeight;
		
		PgContextBlitArea(mPhImageCache,&sarea,(PdOffscreenContext_t *)drawing->GetDC(),&darea);
		PgFlush();	
	}
	else
#endif
	{
		if ((mAlphaDepth == 1) || (mAlphaDepth == 0))
		{
			PgDrawPhImagemx(&pos, &mPhImage, 0);
		}
		else if (mAlphaDepth == 8)
		{
			PgMap_t map;

			map.dim.w = mWidth;
			map.dim.h = mHeight;
			map.bpl = mAlphaRowBytes;
			map.bpp = mAlphaDepth;
			map.map = (char *)mAlphaBits;
			PgSetAlphaBlend(&map, 0);
			PgAlphaOn();
			PgDrawPhImagemx( &pos, &mPhImage, 0 );
			PgAlphaOff();
		}
	}

  	return NS_OK;
}

/* New Tile code *********************************************************************/
NS_IMETHODIMP nsImagePh::DrawTile(nsIRenderingContext &aContext, nsDrawingSurface aSurface, nsRect &aSrcRect, nsRect &aTileRect ) {
	PhPoint_t pos, space, rep;

	pos.x = aTileRect.x;
	pos.y = aTileRect.y;

	space.x = aSrcRect.width;
	space.y = aSrcRect.height;
	rep.x = ( aTileRect.width + space.x - 1 ) / space.x;
	rep.y = ( aTileRect.height + space.y - 1 ) / space.y;
	PhRect_t clip = { {aTileRect.x, aTileRect.y}, {aTileRect.x + aTileRect.width, aTileRect.y + aTileRect.height} };
	PgSetMultiClip( 1, &clip );

#ifdef ALLOW_PHIMAGE_CACHEING
  	PRBool 	aOffScreen;
 	nsDrawingSurfacePh* drawing = (nsDrawingSurfacePh*) aSurface;

	drawing->IsOffscreen(&aOffScreen);
	if (!mPhImage.mask_bm && mIsOptimized && mPhImageCache && aOffScreen)
	{
		PhArea_t sarea, darea;
		int x,y;
		PdOffscreenContext_t *dc = (PdOffscreenContext_t *) drawing->GetDC(),*odc= (PdOffscreenContext_t *) PhDCSetCurrent(NULL);

		sarea.pos.x=sarea.pos.y=0;
		darea.pos=pos;
		darea.size.w=sarea.size.w=mPhImage.size.w;
		darea.size.h=sarea.size.h=mPhImage.size.h;

		for (y=0; y<rep.y; y++)
		{
			for (x=0; x<rep.x; x++)
			{
				PgContextBlitArea(mPhImageCache,&sarea,dc,&darea);
				darea.pos.x+=darea.size.w;
			}
			darea.pos.x=pos.x;
			darea.pos.y+=darea.size.h;
		}

		PgFlush();	
		PhDCSetCurrent(odc);
	}
	else
#endif
	{
		PgDrawRepPhImagemx( &mPhImage, 0, &pos, &rep, &space );
	}

	PgSetMultiClip( 0, NULL );
	return NS_OK;
}

NS_IMETHODIMP nsImagePh::DrawTile( nsIRenderingContext &aContext, nsDrawingSurface aSurface, PRInt32 aSXOffset, PRInt32 aSYOffset, const nsRect &aTileRect ) 
{
	PhPoint_t pos, space, rep;

	// since there is an offset into the image and I only want to draw full
	// images, shift the position back and set clipping so that it looks right
	pos.x = aTileRect.x - aSXOffset;
	pos.y = aTileRect.y - aSYOffset;

	space.x = mPhImage.size.w;
	space.y = mPhImage.size.h;
	rep.x = ( aTileRect.width + aSXOffset + space.x - 1 ) / space.x;
	rep.y = ( aTileRect.height + aSYOffset + space.y - 1 ) / space.y;

	/* not sure if cliping is necessary */
	PhRect_t clip = { {aTileRect.x, aTileRect.y}, {aTileRect.x + aTileRect.width-1, aTileRect.y + aTileRect.height-1} };
	PgSetMultiClip( 1, &clip );

#ifdef ALLOW_PHIMAGE_CACHEING
  	PRBool 	aOffScreen;
	nsDrawingSurfacePh* drawing = (nsDrawingSurfacePh*) aSurface;

	drawing->IsOffscreen(&aOffScreen);
	if (!mPhImage.mask_bm && mIsOptimized && mPhImageCache && aOffScreen)
	{
		PhArea_t sarea, darea;
		int x,y;
		PdOffscreenContext_t *dc = (PdOffscreenContext_t *) drawing->GetDC(),*odc= (PdOffscreenContext_t *) PhDCSetCurrent(NULL);

		sarea.pos.x=sarea.pos.y=0;
		darea.pos=pos;
		darea.size.w=sarea.size.w=mPhImage.size.w;
		darea.size.h=sarea.size.h=mPhImage.size.h;

		for (y=0; y<rep.y; y++)
		{
			for (x=0; x<rep.x; x++)
			{
				PgContextBlitArea(mPhImageCache,&sarea,dc,&darea);
				darea.pos.x+=darea.size.w;
			}
			darea.pos.x=pos.x;
			darea.pos.y+=darea.size.h;
		}

		PgFlush();	
		PhDCSetCurrent(odc);
	}
	else
#endif
	{
		PgDrawRepPhImagemx( &mPhImage, 0, &pos, &rep, &space );
	}

	PgSetMultiClip( 0, NULL );

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

#ifdef ALLOW_PHIMAGE_CACHEING
	PhPoint_t pos={0,0};
	PhDrawContext_t *odc;

	if ((mSizeImage > IMAGE_SHMEM_THRESHOLD) && (mPhImage.mask_bm == NULL))
	{
		if (mPhImageCache == NULL)
		{
			/* good candidate for an offscreen cached image */
			if((mPhImageCache = PdCreateOffscreenContext(0,mPhImage.size.w,mPhImage.size.h,0)) != NULL)
			{	
				odc = PhDCSetCurrent (mPhImageCache);
				PgDrawPhImagemx (&pos, &mPhImage, 0);
				PgFlush();
				PhDCSetCurrent(odc);
				mIsOptimized = PR_TRUE;
				return NS_OK;
			}
		}
	}
#endif // ALLOW_PHIMAGE_CACHEING

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

#ifdef ALLOW_PHIMAGE_CACHEING
    if (!mPhImage.mask_bm && mIsOptimized && mPhImageCache && dest->mPhImageCache)
    {
        PhArea_t sarea, darea;

        sarea.pos.x = sarea.pos.y = 0;
        darea.pos.x = aDX;
        darea.pos.y = aDY;
        darea.size.w = sarea.size.w = aDWidth;
        darea.size.h = sarea.size.h = aDHeight;

        PgContextBlitArea(mPhImageCache,&sarea,dest->mPhImageCache,&darea);
        PgFlush();
    }
	else
#endif
	{
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
		if ((aDWidth != mPhImage.size.w) || (aDHeight != mPhImage.size.h))
		{
			release = 1;
			if ((aDHeight * mPhImage.bpl) < IMAGE_SHMEM_THRESHOLD)
				pimage = MyPiResizeImage(&mPhImage, NULL, aDWidth, aDHeight, Pi_USE_COLORS);
			else
				pimage = MyPiResizeImage(&mPhImage, NULL, aDWidth, aDHeight, Pi_USE_COLORS|Pi_SHMEM);
		}
		else
			pimage = &mPhImage;
		if ( pimage == NULL )
			return NS_OK;
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
		if (release)
			PhReleaseImage(pimage);
	}
	return NS_OK;
}



