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

static NS_DEFINE_IID(kIImageIID, NS_IIMAGE_IID);

NS_IMPL_ISUPPORTS1(nsImagePh, nsIImage)

//#define PgFLUSH() PgFlush()
#define PgFLUSH()

extern void do_bmp(char *ptr,int bpl,int x,int y);

// ----------------------------------------------------------------
nsImagePh :: nsImagePh()
{
  NS_INIT_REFCNT();

  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::nsImagePh Constructor called this=<%p>\n", this));

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

// ----------------------------------------------------------------
nsImagePh :: ~nsImagePh()
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::~nsImagePh Destructor called this=<%p>\n", this));

  /* from windows */
  CleanUp(PR_TRUE);
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
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::Init this=<%p> - aWidth=%d aHeight=%d aDepth=%d aMaskRequirements=%d\n", this, aWidth, aHeight, aDepth, aMaskRequirements));

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
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::Draw1 this=<%p> aS=(%d,%d,%d,%d) aD=(%d,%d,%d,%d)\n", this,
    aSX, aSY, aSWidth, aSHeight, aDX, aDY, aDWidth, aDHeight ));

  if( !mImage.image )
  {
    PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::Draw1 - mImageBits is NULL!\n" ));
    return NS_OK;
  }

  PhPoint_t pos = { aDX, aDY };
  int       err;

  if( mColorMap )
  {
    err=PgSetPalette( (PgColor_t*) mColorMap->Index, 0, 0, mColorMap->NumColors, Pg_PALSET_SOFT, 0 );
    if (err == -1)
    {
      NS_ASSERTION(0,"nsImagePh::Draw Error calling PgSetPalette");
      abort();
	  return NS_ERROR_FAILURE;
    }
  }

  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::Draw1 this=<%p> mImage.size=(%ld,%ld)\n", this, mImage.size.w, mImage.size.h));


  if( mAlphaBits )
  {
    err=PgDrawTImage( mImage.image, mImage.type, &pos, &mImage.size, mImage.bpl, 0, mAlphaBits, mARowBytes );
    if (err == -1)
    {
      NS_ASSERTION(0,"nsImagePh::Draw Error calling PgDrawTImage");
	  abort();
	  return NS_ERROR_FAILURE;
    }
  }
  else
  {
    err=PgDrawImage( mImage.image, mImage.type, &pos, &mImage.size, mImage.bpl, 0 );
    if (err == -1)
    {
      NS_ASSERTION(0,"nsImagePh::Draw Error calling PgDrawImage");
      abort();
	  return NS_ERROR_FAILURE;
    }
  }

  PgFLUSH();	// kedl
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
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::Draw2 this=(%p) dest=(%ld,%ld,%ld,%ld) aSurface=<%p>\n", this, aX, aY, aWidth, aHeight, aSurface ));
  //printf("nsImagePh::Draw2 this=(%p) dest=(%ld,%ld,%ld,%ld) aSurface=<%p>\n", this, aX, aY, aWidth, aHeight, aSurface);

  if( !mImage.image )
  {
    PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::Draw2 - mImage.image is NULL!\n" ));
	NS_ASSERTION(mImage.image, "nsImagePh::Draw2 - mImageBits is NULL!");
    return NS_ERROR_FAILURE;
  }

  if( !aSurface )
  {
    PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::Draw2 - aSurface is NULL!\n" ));
	NS_ASSERTION(mImage.image, "nsImagePh::Draw2 - aSurface is NULL!");
    return NS_ERROR_FAILURE;
  }

  // XXX kipp: this is temporary code until we eliminate the
  // width/height arguments from the draw method.
  if ((aWidth != mWidth) || (aHeight != mHeight))
  {
	NS_ASSERTION(0, "nsImagePh::Draw2 - Width or Height don't match!");
	printf("nsImagePh::Draw2 - Width or Height don't match!\n");

    aWidth = mWidth;
    aHeight = mHeight;
  }

#if 1
  /* Create a new GC just for this image */
  PhGC_t *newGC = PgCreateGC(0);
  PgDefaultGC(newGC);
  PhGC_t *previousGC = PgSetGC(newGC);
  nsRect aRect;
  PRBool isValid;
  
  aContext.GetClipRect(aRect, isValid); 
  if (isValid)
  {
    PhRect_t rect = { {aRect.x,aRect.y}, {aRect.x+aRect.width-1,aRect.y+aRect.height-1}};
    PgSetMultiClip(1,&rect);  
  }

  newGC->translation = previousGC->translation;
  newGC->rid = previousGC->rid;
  newGC->target_rid = previousGC->target_rid;
#endif
  
#if 1
  /* Print out all the clipping that applies */
  PhRect_t  *rect;
  int       rect_count;
  PhGC_t    *gc;

  gc = PgGetGC();
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsImagePh::Draw2   GC Information: rid=<%d> target_rid=<%d>\n", gc->rid, gc->target_rid));
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("\t n_clip_rects=<%d> max_clip_rects=<%d>\n", gc->n_clip_rects,gc->max_clip_rects));
  rect_count=gc->n_clip_rects;
  rect = gc->clip_rects;
  while(rect_count--)
  {
    PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("\t\t %d (%d,%d) to (%d,%d)\n", rect_count, rect->ul.x, rect->ul.y, rect->lr.x, rect->lr.y));
    rect++;
  }
  
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("\t n__user_clip_rects=<%d> max_user_clip_rects=<%d>\n", gc->n_user_clip_rects,gc->max_user_clip_rects));
  rect_count=gc->n_user_clip_rects;
  rect = gc->user_clip_rects;
  while(rect_count--)
  {
    PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("\t\t %d (%d,%d) to (%d,%d)\n", rect_count, rect->ul.x, rect->ul.y, rect->lr.x, rect->lr.y));
    rect++;
  }

  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("\t aux_clip_valid=<%d>\n", gc->aux_clip_valid));
#endif


  PhPoint_t pos = { aX, aY };
  int       err;
  
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::Draw2 this=<%p> mImage.size=(%ld,%ld) mAlphaBits=<%p> mARowBytes=%d mImage.type=%d mImage.mask_bpl=%d\n", this, mImage.size.w, mImage.size.h, mAlphaBits, mARowBytes, mImage.type, mImage.mask_bpl));
  //printf("nsImagePh::Draw2 this=<%p> mImage.size=(%ld,%ld) mAlphaBits=<%p> mARowBytes=%d mImage.type=%d mImage.mask_bpl=%d\n", this, mImage.size.w, mImage.size.h, mAlphaBits, mARowBytes, mImage.type, mImage.mask_bpl);

  if( mAlphaBits )
  {
#if 0
	int x,y;
	for(x=0; x < mHeight; x++)
	{
#if 1
 /* Use PR_LOG */
      unsigned char a = mAlphaBits[(x*4)+0];
      unsigned char b = mAlphaBits[(x*4)+1];
      unsigned char c = mAlphaBits[(x*4)+2];
      unsigned char d = mAlphaBits[(x*4)+3];

	  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::Draw2 %2d => %d%d%d%d%d%d%d%d %d%d%d%d%d%d%d%d %d%d%d%d%d%d%d%d %d%d%d%d%d%d%d%d ",x,
		  ((a&1)==1), ((a&2)==2), ((a&4)==4), ((a&8)==8),
		  ((a&16)==16), ((a&32)==32), ((a&64)==64), ((a&128)==128),

  		  ((b&1)==1), ((b&2)==2), ((b&4)==4), ((b&8)==8),
		  ((b&16)==16), ((b&32)==32), ((b&64)==64), ((b&128)==128),
		  
		  ((c&1)==1), ((c&2)==2), ((c&4)==4), ((c&8)==8),
		  ((c&16)==16), ((c&32)==32), ((c&64)==64), ((c&128)==128),	  

		  ((d&1)==1), ((d&2)==2), ((d&4)==4), ((d&8)==8),
		  ((d&16)==16), ((d&32)==32), ((d&64)==64), ((d&128)==128) ));
	}
#else
  /* use Printfs */
      printf("%2d => ",x);
	  for(y=0; 	y<mARowBytes; y++)
	  {
		//printf("(%d)", (x*mARowBytes)+y );
		unsigned char c = mAlphaBits[(x*mARowBytes)+y];
		printf("%d%d%d%d%d%d%d%d ",
		  ((c&1)==1), ((c&2)==2), ((c&4)==4), ((c&8)==8),
		  ((c&16)==16), ((c&32)==32), ((c&64)==64), ((c&128)==128) );
	  }
	  printf("\n");

	}
    printf("\n");
#endif
#endif    

    err=PgDrawTImage( mImage.image, mImage.type, &pos, &mImage.size, mImage.bpl, 0, mAlphaBits, mARowBytes );
    if (err == -1)
    {
      NS_ASSERTION(0,"nsImagePh::Draw Error calling PgDrawTImage");
	  abort();
      return NS_ERROR_FAILURE;
    }
  }
  else
  {
    err=PgDrawImage( mImage.image, mImage.type, &pos, &mImage.size, mImage.bpl, 0 );
    if (err == -1)
    {
      NS_ASSERTION(0,"nsImagePh::Draw Error calling PgDrawImage");
      abort();
	  return NS_ERROR_FAILURE;
    }

#if 1
    /* Try to dump the image to a BMP file */
    PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::Draw2 Dump image to BMP\n"));

     unsigned char *ptr;
     ptr = mImage.image;  
 
     PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::Draw2 Dump image info w,h,d=(%d,%d,%d) mColorMap=<%p> \n",
	   mWidth, mHeight, mDepth, mColorMap));


    do_bmp(ptr, mImage.bpl/3, mImage.size.w, mImage.size.h);
#endif

  }

  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::Draw2 this=<%p> finished \n", this));
  //printf("nsImagePh::Draw2 this=<%p> finished \n", this);

#if 1
  /* Restore the old GC */
  PgSetGC(previousGC);
  PgDestroyGC(newGC);
#endif

  PgFLUSH();	//kedl
  return NS_OK;
}

/** ----------------------------------------------------------------
 * Create an optimized bitmap, -- this routine may need to be deleted, not really used now
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
  //PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::GetBits\n" ));
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

//------------------------------------------------------------
// lock the image pixels. Implement this if you need it.
NS_IMETHODIMP
nsImagePh::LockImagePixels(PRBool aMaskPixels)
{
  //PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::LockImagePixels aMaskPixels=<%d>\n", aMaskPixels));

  return NS_OK;
}

//------------------------------------------------------------
// unlock the image pixels.  Implement this if you need it.
NS_IMETHODIMP
nsImagePh::UnlockImagePixels(PRBool aMaskPixels)
{
  //PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::UnlockImagePixels aMaskPixels=<%d>\n", aMaskPixels));

  return NS_OK;
}

