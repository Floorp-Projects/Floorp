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

#include "nsDrawingSurfacePh.h"
#include "prmem.h"

#include "nsPhGfxLog.h"
#include <photon/PhRender.h>
#include <Pt.h>

/* The Transparency Mask of the last image Draw'd in nsRenderingContextPh */
/* This is needed for locking and unlocking */
extern void *Mask;


NS_IMPL_ISUPPORTS2(nsDrawingSurfacePh, nsIDrawingSurface, nsIDrawingSurfacePh)

nsDrawingSurfacePh :: nsDrawingSurfacePh()
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDrawingSurfacePh::nsDrawingSurfacePh this=<%p>\n", this));

  NS_INIT_REFCNT();

  mIsOffscreen = PR_FALSE;
  mPixmap      = nsnull;
  mGC          = nsnull;
  mMC          = nsnull;
  mDrawContext = nsnull;
  mWidth       = 0;
  mHeight      = 0;
  mFlags       = 0;

  mImage       = nsnull;
  mLockWidth   = 0;
  mLockHeight  = 0;
  mLockFlags   = 0;
  mLocked      = PR_FALSE;

  mPixFormat.mRedMask = 0xff0000;
  mPixFormat.mGreenMask = 0x00ff00;
  mPixFormat.mBlueMask = 0x0000ff;
  mPixFormat.mAlphaMask = 0;

  mPixFormat.mRedShift = 16;
  mPixFormat.mGreenShift = 8;
  mPixFormat.mBlueShift = 0;
  mPixFormat.mAlphaShift = 0;
}

nsDrawingSurfacePh :: ~nsDrawingSurfacePh()
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDrawingSurfacePh::~nsDrawingSurfacePh this=<%p> mIsOffscreen=<%d> mGC=<%p> mImage=<%p>\n", this, mIsOffscreen, mGC, mImage));

  if (mImage)
  {
    /* This will be defined if the surface is still locked... */
	/* maybe I should unlock it? */
    if (mImage)
    {
	  PR_Free(mImage->image);
      mImage->image = nsnull;
	  PR_Free(mImage);
	  mImage = nsnull;
    }
  }

    Stop();
	
  if (mIsOffscreen)
  {
    //PmMemReleaseMC( mMC);				/* this function has an error! */
      free(mMC);
    mMC = nsnull;
    PgShmemDestroy( mPixmap->image );
	mPixmap->image = nsnull;
    PR_Free (mPixmap);
	mPixmap = nsnull;
  }
}

  /**
   * Lock a rect of a drawing surface and return a
   * pointer to the upper left hand corner of the
   * bitmap.
   * @param  aX x position of subrect of bitmap
   * @param  aY y position of subrect of bitmap
   * @param  aWidth width of subrect of bitmap
   * @param  aHeight height of subrect of bitmap
   * @param  aBits out parameter for upper left hand
   *         corner of bitmap
   * @param  aStride out parameter for number of bytes
   *         to add to aBits to go from scanline to scanline
   * @param  aWidthBytes out parameter for number of
   *         bytes per line in aBits to process aWidth pixels
   * @return error status
   *
   **/
NS_IMETHODIMP nsDrawingSurfacePh :: Lock(PRInt32 aX, PRInt32 aY,
                                          PRUint32 aWidth, PRUint32 aHeight,
                                          void **aBits, PRInt32 *aStride,
                                          PRInt32 *aWidthBytes, PRUint32 aFlags)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDrawingSurfacePh::Lock this=<%p> mLocked=<%d>\n", this, mLocked));

  if (mLocked)
  {
    NS_ASSERTION(0, "nested lock attempt");
    return NS_ERROR_FAILURE;
  }

  mLocked = PR_TRUE;
  mLockX = aX;
  mLockY = aY;
  mLockWidth = aWidth;
  mLockHeight = aHeight;
  mLockFlags = aFlags;

  PhImage_t  *image;
  PhDim_t    dim;
  short      bytes_per_pixel = 3;

  image = (PhImage_t  *) PR_CALLOC( sizeof(PhImage_t) );
  if (image == NULL)
  {
	abort();
    return NS_ERROR_FAILURE;
  }
  
  mImage = image;

  dim.w = mLockWidth;
  dim.h = mLockHeight;
  
  /* Force all the Draw Events out to the image */
  Flush();
  
  image->type = Pg_IMAGE_DIRECT_888; // 3 bytes per pixel with this type
  image->size = dim;
  image->image = (char *) PR_Malloc( dim.w * dim.h * bytes_per_pixel);
  if (image->image == NULL)
  {
	abort();
    return NS_ERROR_FAILURE;
  }
  	  
  image->bpl = bytes_per_pixel*dim.w;

  for (int y=0; y<dim.h; y++)
  {
	memcpy( image->image+y*dim.w*3,
	        mPixmap->image+3*mLockX+(mLockY+y)*mPixmap->bpl,
			dim.w*3);
  }
  
  *aBits = mImage->image;
  *aWidthBytes = aWidth*3;
  *aStride = mImage->bpl;    /* kirkj: I think this is wrong... */

  return NS_OK;
}


NS_IMETHODIMP nsDrawingSurfacePh :: Unlock(void)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDrawingSurfacePh::Unlock this=<%p> mLocked=<%d> \n", this, mLocked));

  if (!mLocked)
  {
    NS_ASSERTION(0, "nsDrawingSurfacePh::Unlock - attempting to unlock an DS that isn't locked");
    return NS_ERROR_FAILURE;
  }

  // If the lock was not read only, put the bits back on the pixmap
  if (!(mLockFlags & NS_LOCK_SURFACE_READ_ONLY))
  {
    PhPoint_t pos = { mLockX, mLockY };
    int err;

    Select();

    if (Mask)
    {
      int bpl;
      bpl = (mLockWidth+7)/8;
      bpl = (bpl + 3) & ~0x3;
      err=PgDrawTImage( mImage->image, mImage->type, &pos, &mImage->size, mImage->bpl, 0 ,Mask,bpl);
      if (err == -1)
	  {
	    NS_ASSERTION(0, "nsDrawingSurfacePh::Unlock PgDrawTImage failed");
		return NS_ERROR_FAILURE;
	  }
    }
    else
	{
      err=PgDrawImage( mImage->image, mImage->type, &pos, &mImage->size, mImage->bpl, 0 );
      if (err == -1)
	  {
	    NS_ASSERTION(0, "nsDrawingSurfacePh::Unlock PgDrawImage failed");
		return NS_ERROR_FAILURE;
	  }
    }
  }

  PR_Free(mImage->image);
  PR_Free(mImage);
  mImage = nsnull;
  mLocked = PR_FALSE;

  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfacePh :: GetDimensions(PRUint32 *aWidth, PRUint32 *aHeight)
{
  *aWidth = mWidth;
  *aHeight = mHeight;

  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDrawingSurfacePh::GetDimensions this=<%p> w,h=(%ld,%ld)\n", this, mWidth, mHeight));
  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfacePh :: IsOffscreen(PRBool *aOffScreen)
{
  *aOffScreen = mIsOffscreen;
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDrawingSurfacePh::IsOffScreen mIsOffscreen=<%d>\n", mIsOffscreen));

  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfacePh :: IsPixelAddressable(PRBool *aAddressable)
{
  *aAddressable = PR_FALSE;
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDrawingSurfacePh::IsPixelAddressable - Not Implemented\n"));
  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfacePh :: GetPixelFormat(nsPixelFormat *aFormat)
{
  *aFormat = mPixFormat;
  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfacePh :: Init( PhGC_t * &aGC )
{
  /* Init a OnScreen Drawing Surface */
  mGC          = aGC;
  mIsOffscreen = PR_FALSE;
  mPixmap      = NULL;
  mDrawContext = PhDCGetCurrent();

  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDrawingSurfacePh::Init with PhGC_t=<%p> for onscreen drawing this=<%p> mDrawContext=<%p>\n",aGC, this, mDrawContext));
  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfacePh :: Init( PhGC_t * &aGC, PRUint32 aWidth,
                                          PRUint32 aHeight, PRUint32 aFlags)
{
  /* Init a Off-Screen Drawing Surface */
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDrawingSurfacePh::Init with PhGC_t + width/height this=<%p> w,h=(%ld,%ld) aGC=<%p>\n", this, aWidth, aHeight, aGC));

  mGC = aGC;
  mWidth = aWidth;
  mHeight = aHeight;
  mFlags = aFlags;

  mIsOffscreen = PR_TRUE;

  PhDim_t     dim;
  PhArea_t    area;
  PtArg_t     arg[3];

  if (mPixmap)
  {
    NS_ASSERTION(NULL, "nsDrawingSurfacePh::Init with mPixmap already created");
	abort();
  }

  mPixmap = (PhImage_t  *) PR_CALLOC(sizeof(PhImage_t) );
  if (mPixmap == NULL)
  {
    NS_ASSERTION(NULL, "nsDrawingSurfacePh::Init can't CALLOC pixmap, out of memory");
    abort();
    return NS_ERROR_FAILURE;
  }
  	
  area.pos.x  = 0;
  area.pos.y  = 0;
  area.size.w = aWidth;
  area.size.h = aHeight;
  dim.w       = area.size.w;
  dim.h       = area.size.h;

  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDrawingSurfacePh::Init create drawing surface: area=<%d,%d,%d,%d> mPixmap=<%p>\n",
    area.pos.x,area.pos.y,area.size.w,area.size.h,mPixmap));

  PhPoint_t           translation = { 0, 0 };
  short               bytes_per_pixel = 3;

  mPixmap->type = Pg_IMAGE_DIRECT_888; // 3 bytes per pixel with this type
  mPixmap->size = dim;
  mPixmap->image = (char *) PgShmemCreate( dim.w * dim.h * bytes_per_pixel, NULL);

  if (mPixmap->image == NULL)
  {
    NS_ASSERTION(0,"nsDrawingSurfacePh::Init Out of Memory calling PgShmemCreate");
	abort();
    return NS_ERROR_FAILURE;
  }
  
  mPixmap->bpl = bytes_per_pixel * dim.w;

  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDrawingSurfacePh::Init  Before calling PmMemCreateMC\n"));

  mMC = PmMemCreateMC( mPixmap, &dim, &translation );
  if (mMC == NULL)
  {
    NS_ASSERTION(0, "nsDrawingSurfacePh::Init Out of Memory calling PmMemCreateMC");
	abort();
    return NS_ERROR_FAILURE;
  }
  	
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDrawingSurfacePh::Init  mMC=<%p> mDrawContext=<%p> CurrentMC=<%p>\n", mMC, mDrawContext, PhDCGetCurrent()));

  PhDrawContext_t *oldDC;

  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDrawingSurfacePh::Init  Before calling PmMemStart this=<%p>\n", this));

  // after the Start all drawing goes into the memory context
  oldDC = PmMemStart( mMC );
  if (oldDC == NULL)
  {
	NS_ASSERTION(0, "nsDrawingSurfacePh::Init - Error calling PmMemStart");
    PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDrawingSurfacePh::Init - Error calling PmMemStart"));
	abort();
	return NS_ERROR_FAILURE;
  }

  /* Hack to see if this helps! */
  //PmMemSetType(mMC, Pm_IMAGE_CONTEXT);

  /* Save away the new DrawContext */
  mDrawContext = PhDCGetCurrent();

  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDrawingSurfacePh::Init  Finished calling PmMemStart oldDC=<%p> new mc=<%p> CurrentDC=<%p>\n", oldDC, mMC, PhDCGetCurrent()));
	    
  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfacePh :: Select( void )
{
  PhGC_t *gc = PgGetGC();
  PhDrawContext_t *dc = PhDCGetCurrent();
  
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDrawingSurfacePh::Select this=<%p> CurrentGC=<%p> mIsOffscreen=<%d> CurrentDC=<%p> mDrawContext=<%p>\n", this, gc, mIsOffscreen, dc, mDrawContext));

  if (mDrawContext != dc)
  {
    PhDrawContext_t *old_dc;
	/* Reset the Draw Context */
    old_dc = PhDCSetCurrent(mDrawContext);
    if (old_dc == NULL)
	{
	  NS_ASSERTION(old_dc, "nsDrawingSurfacePh::Select new DC can not be made active");
	  abort();	
	}

    gc = PgGetGC();
    dc = PhDCGetCurrent();
  
    PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDrawingSurfacePh::Select after set DrawContext CurrentGC=<%p> CurrentDC=<%p>\n", gc, dc));
  }
  else
  {
    PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDrawingSurfacePh::Select mDrawContext== CurrentDC\n"));
  }

  
  gc = PgGetGC();
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDrawingSurfacePh::Select mGC=<%p> mMC=<%p> gc=<%p>\n", mGC, mMC, gc));
  
  if (gc != mGC)
  {
    PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDrawingSurfacePh::Select Setting GC to mGC=<%p>\n", mGC));
    PgSetGC(mGC);  
  }

  /* Clear out the Multi-clip, it will be reset if needed */
  /* This fixed the toolbar drawing */
  PgSetMultiClip(0,NULL);


#ifdef DEBUG
  PhRect_t  *rect;
  int       rect_count;
  gc = PgGetGC();
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDrawingSurfacePh::Select   GC Information: rid=<%d> target_rid=<%d>\n", gc->rid, gc->target_rid));
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

  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfacePh::Stop(void)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDrawingSurfacePh::Stop this=<%p> mIsOffscreen=<%d> mGC=<%p>\n", this, mIsOffscreen, mGC));

  Flush();
  
  if (mIsOffscreen)
  {
    PhDrawContext_t *theMC;
    PhDrawContext_t *dc = PhDCGetCurrent();
  
    /* Is my Memory Context even active? */
    if (mDrawContext == dc)
    {
      theMC = PmMemStop( (PmMemoryContext_t *) mGC );
	  if (theMC == NULL)
	  {
	    NS_WARNING("nsDrawingSurfacePh::Stop - Error calling PmMemStop");
	    PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDrawingSurfacePh::Stop - Error calling PmMemStop this=<%p>\n", this));
 	  }
	  else
	  {
	    PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDrawingSurfacePh::Stop - PmMemStop Successful\n"));
	  }
    }
  }

  return NS_OK;
}

PhGC_t *nsDrawingSurfacePh::GetGC(void)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDrawingSurfacePh::GetGC this=<%p> mGC=<%p> mMC=<%p> mIsOffscreen=<%d>", this, mGC, mMC, mIsOffscreen));
  return mGC;
}

NS_IMETHODIMP nsDrawingSurfacePh::Flush(void)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDrawingSurfacePh::Flush this=<%p> mIsOffscreen=<%d> mGC=<%p> mMC=<%p> mPixmap=<%p>\n", this, mIsOffscreen, mGC, mMC, mPixmap));
  int err;


  if (mIsOffscreen)
  {
    NS_ASSERTION(mMC, "nsDrawingSurfacePh::Flush  mMC is NULL\n");
    NS_ASSERTION(mPixmap, "nsDrawingSurfacePh::Flush  mPixmap is NULL\n");

    err = PmMemFlush( mMC, mPixmap ); 
    if (err == -1)
	{
	  NS_ASSERTION(0,"nsDrawingSurfacePh::Flush  Invalid Memory Context");
	  abort();
	}

    PgFlush();		/* not sure if I need this or not */
  }
  else
  {
	err=PgFlush();  
    if (err == -1)
	{
	  NS_ASSERTION(0,"nsDrawingSurfacePh::Flush  Error with PgFlush of Offscreen DrawingSurface");
	  abort();	
	}
  }
}

PRBool nsDrawingSurfacePh::IsActive(void)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDrawingSurfacePh::IsActive this=<%p> mIsOffscreen=<%d> mGC=<%p> mMC=<%p> mPixmap=<%p>\n", this, mIsOffscreen, mGC, mMC, mPixmap));

  PhDrawContext_t *dc = PhDCGetCurrent();
  
  /* Is my Memory Context even active? */
  if (mDrawContext == dc)
  {
    return PR_TRUE;
  }
  else
  {
    return PR_FALSE;
  }
}


