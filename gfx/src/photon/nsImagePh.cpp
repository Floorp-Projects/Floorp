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


#include "nsImagePh.h"
#include "nsRenderingContextPh.h"
#include "nsPhGfxLog.h"

static NS_DEFINE_IID(kIImageIID, NS_IIMAGE_IID);


/** ----------------------------------------------------------------
  * Constructor for nsImagePh
  * @update dc - 11/20/98
  */
nsImagePh :: nsImagePh()
{
  NS_INIT_REFCNT();

  mWidth = 0;
  mHeight = 0;
  mDepth = 0;
  mNumBytesPixel = 0;
  mNumPaletteColors = 0;
  mSizeImage = 0;
  mRowBytes = 0;
  mImageBits = nsnull;
  mIsOptimized = PR_FALSE;
  mColorMap = nsnull;
  mAlphaBits = nsnull;
  mAlphaDepth = 0;
  mARowBytes = 0;
  mAlphaWidth = 0;
  mAlphaHeight = 0;
  mImageCache = 0;
  mAlphaLevel = 0;
  mImage.palette = nsnull;
  mImage.image = nsnull;
}

/** ----------------------------------------------------------------
  * destructor for nsImagePh
  * @update dc - 11/20/98
  */
nsImagePh :: ~nsImagePh()
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::~nsImagePh Destructor called\n"));

  /* from windows */
  CleanUp(PR_TRUE);
}

NS_IMPL_ISUPPORTS(nsImagePh, kIImageIID);

/** ----------------------------------------------------------------
 * Initialize the nsImagePh object
 * @update dc - 11/20/98
 * @param aWidth - Width of the image
 * @param aHeight - Height of the image
 * @param aDepth - Depth of the image
 * @param aMaskRequirements - A mask used to specify if alpha is needed.
 * @result NS_OK if the image was initied ok
 */
nsresult nsImagePh :: Init(PRInt32 aWidth, PRInt32 aHeight, PRInt32 aDepth,nsMaskRequirements aMaskRequirements)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::Init (%p) - aWidth=%d aHeight=%d aDepth=%d\n", this,
  	aWidth, aHeight, aDepth));


//  mHBitmap = nsnull;
//  mAlphaHBitmap = nsnull;
  CleanUp(PR_TRUE);

  if (8 == aDepth)
  {
    PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::Init - Palette image."));

    mNumPaletteColors = 256;
    mNumBytesPixel = 1;
    mImage.type = Pg_IMAGE_PALETTE_BYTE;
    mImage.colors = 256;

    mColorMap = new nsColorMap;
    if( mColorMap != nsnull )
    {
      mColorMap->NumColors = mNumPaletteColors;
      mColorMap->Index = new PRUint8[3 * mColorMap->NumColors];
      memset( mColorMap->Index, 0, sizeof(PRUint8) * ( 3 * mColorMap->NumColors ));
    }
  }
  else if (24 == aDepth)
  {
    mNumPaletteColors = 0;
    mNumBytesPixel = 3;
    mImage.type = Pg_IMAGE_DIRECT_888;
    mImage.colors = 0;
  }
  else
  {
    NS_ASSERTION(PR_FALSE, "unexpected image depth");
    return NS_ERROR_UNEXPECTED;
  }

  /* From GTK */
  mWidth = aWidth;
  mHeight = aHeight;
  mDepth = aDepth;

  /* Computer the Image Metrics */
  mRowBytes = CalcBytesSpan(mWidth);
  mSizeImage = mRowBytes * mHeight;

  /* Allocate the Image Data */
  mImageBits = (PRUint8*) new PRUint8[mSizeImage];

  switch(aMaskRequirements)
  {
  case nsMaskRequirements_kNoMask:
    mAlphaBits = nsnull;
    mAlphaWidth = 0;
    mAlphaHeight = 0;
    break;
								
  case nsMaskRequirements_kNeeds1Bit:
    mARowBytes = (aWidth + 7) / 8;
    mAlphaDepth = 1;

    // 32-bit align each row
    mARowBytes = (mARowBytes + 3) & ~0x3;
    mAlphaBits = new unsigned char[mARowBytes * aHeight];
    mAlphaWidth = aWidth;
    mAlphaHeight = aHeight;
    break;

  case nsMaskRequirements_kNeeds8Bit:
    mAlphaBits = nsnull;
    mAlphaWidth = 0;
    mAlphaHeight = 0;
    printf("TODO: want an 8bit mask for an image..\n");
    PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::Init - 8 bit mask not implemented.\n" ));
    break;

  default:
    printf("TODO: want a mask for an image.\n");
    break;
  }

  mImage.image_tag     = 0; // REVISIT - A CRC value for PhRelay ???
  mImage.bpl           = mRowBytes;
  mImage.size.w        = mWidth;
  mImage.size.h        = mHeight;
  mImage.palette_tag   = 0; // REVISIT - A CRC value for PhRelay ???
  mImage.xscale        = 1; // scaling is integral only
  mImage.yscale        = 1;
  mImage.format        = 0; // not used
  mImage.flags         = 0;
  mImage.ghost_bpl     = 0;
  mImage.spare1        = 0;
  mImage.ghost_bitmap  = nsnull;
  mImage.mask_bpl      = mARowBytes;
  mImage.mask_bm       = (char *)mAlphaBits;
//  mImage.palette       = nsnull;   // REVISIT - not handling palette yet
  if( mColorMap )
    mImage.palette     = (PgColor_t *) mColorMap->Index;
  else
    mImage.palette     = nsnull;
  mImage.image         = (char *)mImageBits;

  return NS_OK;
}

/** ----------------------------------------------------------------
  * This routine started out to set up a color table, which as been 
  * outdated, and seemed to have been used for other purposes.  This 
  * routine should be deleted soon -- dc
  * @update dc - 11/20/98
  * @param aContext - a device context to use
  * @param aFlags - flags used for the color manipulation
  * @param aUpdateRect - The update rectangle to use
  * @result void
  */
void nsImagePh :: ImageUpdated(nsIDeviceContext *aContext, PRUint8 aFlags, nsRect *aUpdateRect)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::ImageUpdated (%p)\n", this ));

#if 0
  // REVISIT - we only need to fix images for the screen device (not printers, etc)
  // Fix the RGB ordering for 24-bit images (swap red and blue values)...

  if( mImageBits && ( mNumBytesPixel == 3 ) && ( aFlags & NS_IMAGE_UPDATE_PIXELS ))
  {
    int x,y,off,off2,xm,ym;
    char c;

    if( aUpdateRect )
    {
      xm = aUpdateRect->width*3;
      ym = aUpdateRect->height;
      off=mRowBytes*aUpdateRect->y + aUpdateRect->x;

      PR_LOG(PhGfxLog, PR_LOG_DEBUG,("  Pixels have changed in (%ld,%ld,%ld,%ld).\n", aUpdateRect->x,
        aUpdateRect->y, aUpdateRect->width, aUpdateRect->height ));
    }
    else
    {
      xm = mWidth*3;
      ym = mHeight;
      off=0;

      PR_LOG(PhGfxLog, PR_LOG_DEBUG,("  All pixels have changed.\n" ));
    }

    for(y=0;y<ym;y++,off+=mRowBytes)
    {
      off2 = off+2;
      for(x=0;x<xm;x+=3)
      {
        c = mImageBits[off+x];
        mImageBits[off+x] = mImageBits[off2+x];
        mImageBits[off2+x] = c;
      }
    }
  }
  else
    PR_LOG(PhGfxLog, PR_LOG_DEBUG,("  skipped: mImageBits=%p  mNumBytesPixel=%d  flags=%ld.\n", mImageBits, mNumBytesPixel, aFlags ));
#endif

}

//------------------------------------------------------------


// Raster op used with MaskBlt(). Assumes our transparency mask has 0 for the
// opaque pixels and 1 for the transparent pixels. That means we want the
// background raster op (value of 0) to be SRCCOPY, and the foreground raster
// (value of 1) to just use the destination bits
#define MASKBLT_ROP MAKEROP4((DWORD)0x00AA0029, SRCCOPY)


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
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::Draw1 (%p)\n", this ));

  if( !mImage.image )
  {
    PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::Draw1 - mImageBits is NULL!\n" ));
    return NS_OK;
  }

  PhPoint_t pos = { aDX, aDY };

//  PgDrawPhImagemx( &pos, &mImage, 0 );

  if( mColorMap )
  {
    PgSetPalette( (PgColor_t*) mColorMap->Index, 0, 0, mColorMap->NumColors, Pg_PALSET_SOFT, 0 );
  }

  if( mAlphaBits )
    PgDrawTImage( mImage.image, mImage.type, &pos, &mImage.size, mImage.bpl, 0, mAlphaBits, mARowBytes );
  else
    PgDrawImage( mImage.image, mImage.type, &pos, &mImage.size, mImage.bpl, 0 );

//  PgFlush();

  return NS_OK;
}

/** ----------------------------------------------------------------
  * Draw the bitmap, defaulting the source to 0,0,source 
  * and destination have the same width and height.
  * @update dc - 11/20/98
  * @param aContext - the rendering context to draw with
  * @param aSurface - The HDC in a nsDrawingsurfacePh to copy the bits to.
  * @param aX - destination location
  * @param aY - destination location
  * @param aWidth - image width
  * @param aHeight - image height
  * @result NS_OK if the draw worked
  */
NS_IMETHODIMP nsImagePh :: Draw(nsIRenderingContext &aContext, nsDrawingSurface aSurface,
				 PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::Draw2 (%p) (%ld,%ld,%ld,%ld)\n", this, aX, aY, aWidth, aHeight ));

//printf ("kedl: draw2\n");
  // REVISIT - this is a brute-force implementation. We currently have no h/w blit
  // capabilities.

  if( !mImage.image )
  {
    PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::Draw2 - mImageBits is NULL!\n" ));
    return NS_OK;
  }

  PhPoint_t pos = { aX, aY };

  if( mAlphaBits )
    PgDrawTImage( mImage.image, mImage.type, &pos, &mImage.size, mImage.bpl, 0, mAlphaBits, mARowBytes );
  else
    PgDrawImage( mImage.image, mImage.type, &pos, &mImage.size, mImage.bpl, 0 );

//  PgDrawPhImagemx( &pos, &mImage, 0 );
//  PgFlush();

  return NS_OK;
}

/** ----------------------------------------------------------------
 * Create an optimezed bitmap, -- this routine may need to be deleted, not really used now
 * @update dc - 11/20/98
 * @param aContext - The device context to use for the optimization
 */
nsresult nsImagePh :: Optimize(nsIDeviceContext* aContext)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::Optimize\n" ));

  return NS_OK;
}

/** ----------------------------------------------------------------
 * Calculate the number of bytes in a span for this image
 * @update dc - 11/20/98
 * @param aWidth - The width to calulate the number of bytes from
 * @return Number of bytes for the span
 */
PRInt32  nsImagePh :: CalcBytesSpan(PRUint32  aWidth)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::CalcBytesSpan aWidth=<%d>\n",aWidth));

  /* Stolen from GTK */
  PRInt32 spanbytes;
  
  spanbytes = (aWidth * mDepth) >> 5;

  if (((PRUint32)aWidth * mDepth) & 0x1F)
     spanbytes++;
  spanbytes <<= 2;

  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::CalcBytesSpan spanbytes returns <%d>\n",spanbytes));

  return(spanbytes);
}

/** ----------------------------------------------------------------
 * Clean up the memory associted with this object.  Has two flavors, 
 * one mode is to clean up everything, the other is to clean up only the DIB information
 * @update dc - 11/20/98
 * @param aWidth - The width to calulate the number of bytes from
 * @return Number of bytes for the span
 */
void nsImagePh :: CleanUp(PRBool aCleanUpAll)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::CleanUp\n" ));

#if 0
  /* from windows */
  // this only happens when we need to clean up everything
  if (aCleanUpAll == PR_TRUE)
  {
    if (mHBitmap != nsnull)
	  ::DeleteObject(mHBitmap);
	if (mAlphaHBitmap != nsnull)
	  ::DeleteObject(mAlphaHBitmap);
	if(mBHead)
	{
	  delete[] mBHead;
	  mBHead = nsnull;
	}
															  
	mHBitmap = nsnull;
	mAlphaHBitmap = nsnull;
	mIsOptimized = PR_FALSE;
  }
#endif

  if (mImageBits != nsnull)
  {
    delete [] mImageBits;
    mImageBits = nsnull;
    mImage.image = nsnull;
  }

  if (mAlphaBits != nsnull)
  {
    delete [] mAlphaBits;
    mAlphaBits = nsnull;
  }

  // Should be an ISupports, so we can release
  if (mColorMap != nsnull)
  {
    delete [] mColorMap->Index;
    delete mColorMap;
  }

  mNumPaletteColors = -1;
  mNumBytesPixel = 0;
  mSizeImage = 0;
  mImageBits = nsnull;
  mAlphaBits = nsnull;
  mColorMap = nsnull;
}

PRInt32 nsImagePh::GetBytesPix()
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::GetBytesPix\n" ));

  return mNumBytesPixel;
}

PRInt32 nsImagePh::GetHeight()
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::GetHeight\n" ));
  return mHeight;
}

PRInt32 nsImagePh::GetWidth()
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::GetWidth\n" ));
  return mWidth;
}

PRUint8* nsImagePh::GetBits()
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::GetBits\n" ));
  return mImageBits;
}

PRInt32 nsImagePh::GetLineStride()
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::GetLineStride\n" ));
  return mRowBytes;
}

nsColorMap* nsImagePh::GetColorMap()
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::GetColorMap\n" ));
  return mColorMap;
}

PRBool nsImagePh::IsOptimized()
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::IsOptimized\n" ));
  return mIsOptimized;
}

PRUint8* nsImagePh::GetAlphaBits()
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::GetAlphaBits\n" ));
  return mAlphaBits;
}

PRInt32 nsImagePh::GetAlphaWidth()
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::GetAlphaWidth\n" ));
  return mAlphaWidth;
}

PRInt32 nsImagePh::GetAlphaHeight()
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::GetAlphaHeight\n" ));
  return mAlphaHeight;
}

PRInt32 nsImagePh::GetAlphaLineStride()
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::GetAlphaLineStride\n" ));
  return mARowBytes;
}

PRBool nsImagePh::GetIsRowOrderTopToBottom()
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::GetIsRowOrderTopToBottom\n" ));
  return PR_TRUE;
}

PRIntn nsImagePh::GetSizeHeader()
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::GetSizeHeader\n" ));
  return 0;
}

PRIntn nsImagePh::GetSizeImage()
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::GetSizeImage\n" ));
  return mSizeImage;
}

void nsImagePh::SetAlphaLevel(PRInt32 aAlphaLevel)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::SetAlphaLevel\n" ));
  mAlphaLevel=aAlphaLevel;
}

PRInt32 nsImagePh::GetAlphaLevel()
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::GetAlphaLevel\n" ));
  return(mAlphaLevel);
}

void* nsImagePh::GetBitInfo()
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh:: - Not implemented\n" ));
  return nsnull;
}

