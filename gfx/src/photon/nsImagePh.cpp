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

//#define PgFLUSH() PgFlush()
#define PgFLUSH()

extern void do_bmp(char *ptr,int bpl,int x,int y);

// ----------------------------------------------------------------
nsImagePh :: nsImagePh()
{
  NS_INIT_REFCNT();

  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::nsImagePh Constructor called this=<%p>\n", this));

  mImageBits = nsnull;
  mWidth = 0;
  mHeight = 0;
  mDepth = 0;
  mNumBytesPixel = 0;
  mNumPaletteColors = 0;
  mSizeImage = 0;
  mRowBytes = 0;
//  mIsOptimized = PR_FALSE;
//  mColorMap = nsnull;
  mAlphaBits = nsnull;
  mAlphaDepth = 0;
  mARowBytes = 0;
  mAlphaWidth = 0;
  mAlphaHeight = 0;
  mConvertedBits = nsnull;
  mImageCache = 0;
  mAlphaLevel = 0;
  mImage.palette = nsnull;
  mImage.image = nsnull;
}

// ----------------------------------------------------------------
nsImagePh :: ~nsImagePh()
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::~nsImagePh Destructor called this=<%p>\n", this));

 
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

  mNumPaletteColors = -1;
  mNumBytesPixel = 0;
  mSizeImage = 0;
  mImageBits = nsnull;
  mAlphaBits = nsnull;

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
  
  SetDecodedRect(0,0,0,0);  //init
  
  if (24 == aDepth)
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


  mWidth = aWidth;
  mHeight = aHeight;
  mDepth = aDepth;
  mIsTopToBottom = PR_TRUE;


 // create the memory for the image
  ComputeMetrics();


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
      mARowBytes = aWidth;
      mAlphaDepth = 8;

      // 32-bit align each row
      mARowBytes = (mARowBytes + 3) & ~0x3;
      mAlphaBits = new PRUint8[mARowBytes * aHeight];
      mAlphaWidth = aWidth;
      mAlphaHeight = aHeight;
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
  mImage.format        = 0; // not used
  mImage.flags         = 0;
  mImage.ghost_bpl     = 0;
  mImage.spare1        = 0;
  mImage.ghost_bitmap  = nsnull;
  mImage.mask_bpl      = mARowBytes;
#if 0
  mImage.mask_bm       = (char *)mAlphaBits;
  if( mColorMap )
    mImage.palette     = (PgColor_t *) mColorMap->Index;
  else
#endif
    mImage.palette     = nsnull;
  mImage.image         = (char *)mImageBits;

  return NS_OK;
}

//------------------------------------------------------------

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

void* nsImagePh::GetBitInfo()
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::GetBitInfo - Not implemented\n" ));
  return nsnull;
}

PRInt32 nsImagePh::GetLineStride()
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::GetLineStride\n" ));
  return mRowBytes;
}

nsColorMap* nsImagePh::GetColorMap()
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::GetColorMap\n" ));
  return nsnull;
}

PRBool nsImagePh::IsOptimized()
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::IsOptimized\n" ));
  return PR_TRUE;
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

nsIImage *nsImagePh::DuplicateImage()
{
  return nsnull;
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

void nsImagePh::MoveAlphaMask(PRInt32 aX, PRInt32 aY)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::MoveAlphaMask\n" ));
}


void nsImagePh :: ImageUpdated(nsIDeviceContext *aContext, PRUint8 aFlags, nsRect *aUpdateRect)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::ImageUpdated (%p)\n", this ));

  mFlags = aFlags; // this should be 0'd out by Draw()
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
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::Draw1 this=<%p> aS=(%d,%d,%d,%d) aD=(%d,%d,%d,%d) nsImageUpdateFlags_kBitsChanged Flag=<%d>\n", this,
    aSX, aSY, aSWidth, aSHeight, aDX, aDY, aDWidth, aDHeight,IsFlagSet(nsImageUpdateFlags_kBitsChanged, mFlags) ));

  if( !mImage.image )
  {
    PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::Draw1 - mImageBits is NULL!\n" ));
    return NS_OK;
  }

  PhPoint_t pos = { aDX, aDY };
  int       err;

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

  mFlags = 0;
  PgFLUSH();	// kedl
  return NS_OK;
}

NS_IMETHODIMP nsImagePh :: Draw(nsIRenderingContext &aContext, nsDrawingSurface aSurface,
				 PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::Draw2 this=(%p) dest=(%ld,%ld,%ld,%ld) aSurface=<%p> nsImageUpdateFlags_kBitsChanged flags=<%d>\n",
   this, aX, aY, aWidth, aHeight, aSurface, IsFlagSet(nsImageUpdateFlags_kBitsChanged, mFlags) ));

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

  nsDrawingSurfacePh* drawing = (nsDrawingSurfacePh*) aSurface;

  // XXX kipp: this is temporary code until we eliminate the
  // width/height arguments from the draw method.
  if ((aWidth != mWidth) || (aHeight != mHeight))
  {
	NS_ASSERTION(0, "nsImagePh::Draw2 - Width or Height don't match!");
	printf("nsImagePh::Draw2 - Width or Height don't match!\n");

    aWidth = mWidth;
    aHeight = mHeight;
  }

#if defined(DEBUG) && 0
{
  /* Print out all the clipping that applies */
  PhRect_t  *rect;
  int       rect_count;
  PhGC_t    *gc;

  gc = PgGetGC();
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsImagePh::Draw2   CurrentGC gc=<%p> Information: rid=<%d> target_rid=<%d>\n", gc, gc->rid, gc->target_rid));
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


/* drawing surface GC */

  gc = drawing->GetGC();
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsImagePh::Draw2   aSurface->GetGC gc=<%p> Information: rid=<%d> target_rid=<%d>\n", gc, gc->rid, gc->target_rid));
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
}
#endif

#if defined(DEBUG) && 0
{
  nsRect aRect;
  PRBool isValid;
  
  aContext.GetClipRect(aRect, isValid); 
  if (isValid)
  {
    PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsImagePh::Draw2  ClipRect=<%d,%d,%d,%d>\n",aRect.x,aRect.y, aRect.width, aRect.height));
  }
  else
  {
    PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsImagePh::Draw2  ClipRect=<not valid>\n"));
  }
}
#endif

//#define CREATE_NEW_GC

#ifdef CREATE_NEW_GC
  /* Create a new GC just for this image */
  PhGC_t *newGC = PgCreateGC(0);
  PgDefaultGC(newGC);
  PhGC_t *previousGC = PgSetGC(newGC);
  nsRect aRect;
  PRBool isValid;

  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsImagePh::Draw2   oldGC=<%p> newGC=<%p>\n", previousGC, newGC));

#if 0  
  aContext.GetClipRect(aRect, isValid); 
  if (isValid)
  {
    PhRect_t rect = { {aRect.x,aRect.y}, {aRect.x+aRect.width-1,aRect.y+aRect.height-1}};
    PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsImagePh::Draw2  ClipRect=<%d,%d,%d,%d>\n",aRect.x,aRect.y, aRect.width, aRect.height));

    PgSetMultiClip(1,&rect);  
  }
#else
  newGC->n_user_clip_rects = previousGC->n_user_clip_rects;
  newGC->user_clip_rects   = previousGC->user_clip_rects;
#endif

  newGC->translation = previousGC->translation;
  newGC->rid = previousGC->rid;
  newGC->target_rid = previousGC->target_rid;

{  /* Print out all the clipping that applies */
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
}
#endif


  PhPoint_t pos = { aX, aY };
  int       err;
  
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::Draw2 this=<%p> mImage.size=(%ld,%ld) mAlphaBits=<%p> mARowBytes=%d mImage.type=%d mImage.mask_bpl=%d\n", this, mImage.size.w, mImage.size.h, mAlphaBits, mARowBytes, mImage.type, mImage.mask_bpl));
  //printf("nsImagePh::Draw2 this=<%p> mImage.size=(%ld,%ld) mAlphaBits=<%p> mARowBytes=%d mImage.type=%d mImage.mask_bpl=%d\n", this, mImage.size.w, mImage.size.h, mAlphaBits, mARowBytes, mImage.type, mImage.mask_bpl);

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

#if defined(DEBUG) && 0
    /* Try to dump the image to a BMP file */
    PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::Draw2 Dump image to BMP\n"));

     char *ptr;
     ptr = (char *) mImage.image;  
 
     PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::Draw2 Dump image info w,h,d=(%d,%d,%d) mColorMap=<%p> \n",
	   mWidth, mHeight, mDepth, mColorMap));

    do_bmp(ptr, mImage.bpl/3, mImage.size.w, mImage.size.h);
#endif
  }

  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::Draw2 this=<%p> finished \n", this));

#ifdef CREATE_NEW_GC
  /* Restore the old GC */
  PgSetGC(previousGC);

  newGC->n_user_clip_rects = 0;
  newGC->user_clip_rects   = NULL;
  
  PgDestroyGC(newGC);
#endif

  PgFLUSH();	//kedl
  return NS_OK;
}

//------------------------------------------------------------

nsresult nsImagePh :: Optimize(nsIDeviceContext* aContext)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsImagePh::Optimize\n" ));

  return NS_OK;
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
