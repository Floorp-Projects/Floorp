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

#include "nsDrawingSurfacePh.h"
#include "prmem.h"

#include "nsPhGfxLog.h"
#include <photon/PhRender.h>
#include <Pt.h>

NS_IMPL_ISUPPORTS2(nsDrawingSurfacePh, nsIDrawingSurface, nsIDrawingSurfacePh)

nsDrawingSurfacePh :: nsDrawingSurfacePh()
{
  NS_INIT_REFCNT();

  mIsOffscreen=0;
  mPixmap = nsnull;
  mGC = nsnull;
  mWidth = mHeight = 0;
  mFlags = 0;

  mImage = nsnull;
  mLockWidth = mLockHeight = 0;
  mLockFlags = 0;
  mLocked = PR_FALSE;

  mPixFormat.mRedMask = 0xff0000;
  mPixFormat.mGreenMask = 0x00ff00;
  mPixFormat.mBlueMask = 0x0000ff;
  // FIXME
  mPixFormat.mAlphaMask = 0;

  mPixFormat.mRedShift = 16;
  mPixFormat.mGreenShift = 8;
  mPixFormat.mBlueShift = 0;
  // FIXME
  mPixFormat.mAlphaShift = 0;
}

nsDrawingSurfacePh :: ~nsDrawingSurfacePh()
{
  if( mIsOffscreen )
  {
    /* putting this back in */
    PmMemFlush( (PmMemoryContext_t *) mGC, mPixmap );
    PmMemReleaseMC( (PmMemoryContext_t *) mGC);
    PgShmemDestroy( mPixmap->image );
    PR_Free (mPixmap);
    PgSetGC(mholdGC);
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
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDrawingSurfacePh::Lock mLocked=<%d>\n"));

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

// kirk - replaced Malloc with Calloc and got rid of memset further down
//  image = PR_Malloc(sizeof(PhImage_t));
  image = PR_CALLOC(sizeof(PhImage_t) );
  if (image == NULL)
    return NS_ERROR_FAILURE;

  mImage = image;

  dim.w = mLockWidth;
  dim.h = mLockHeight;

//  memset( image, 0, sizeof(PhImage_t) );
  image->type = Pg_IMAGE_DIRECT_888; // 3 bytes per pixel with this type
  image->size = dim;
  image->image = (char *) PR_Malloc( dim.w * dim.h * bytes_per_pixel);
  if (image->image == NULL)
    return NS_ERROR_FAILURE;
	  
  image->bpl = bytes_per_pixel*dim.w;

  int y;
  for (y=0;y<dim.h;y++)
  {
	memcpy( image->image+y*dim.w*3,
	        mPixmap->image+3*mLockX+(mLockY+y)*mPixmap->bpl,
			dim.w*3);
  }
  
  *aBits = mImage->image;
  *aWidthBytes = aWidth*3;
  *aStride = mImage->bpl;

  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfacePh :: XOR(PRInt32 aX, PRInt32 aY,
                                          PRUint32 aWidth, PRUint32 aHeight)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDrawingSurfacePh::XOR this=<%p> mIsOffscreen=<%d> area=(%ld,%ld,%ld,%ld)\n",
    this, mIsOffscreen, aX, aY, aWidth, aHeight));

  int x, y, err;
  unsigned char *ptr;

    err=PmMemFlush( (PmMemoryContext_t *) mGC, mPixmap ); // get the image
    NS_ASSERTION(err != -1, "nsDrawingSurfacePh::XOR  PmMemFlush failed");
    if (err == -1)
	  return NS_ERROR_FAILURE;
	  	
	ptr = mPixmap->image;
	for (y=aY;y<aY+aHeight;y++)
	{
	  for (x=aX;x<aX+aWidth;x++)
	  {
		ptr[3*x+0+(y*mPixmap->bpl)]^=255;
		ptr[3*x+1+(y*mPixmap->bpl)]^=255;
		ptr[3*x+2+(y*mPixmap->bpl)]^=255;
	  }
	}

	return NS_OK;
}

extern void *Mask;

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

  PgFlush();
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
  mGC = aGC;
  mIsOffscreen = PR_FALSE;	// is onscreen
  mPixmap = NULL; // is onscreen

  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDrawingSurfacePh::Init with PhGC_t=<%p> this=<%p>\n",aGC, this));

  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfacePh :: Init( PhGC_t * &aGC, PRUint32 aWidth,
                                          PRUint32 aHeight, PRUint32 aFlags)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDrawingSurfacePh::Init with PhGC_t + width/height this=<%p> w,h=(%ld,%ld) aGC=<%p>\n", this, aWidth, aHeight, aGC));

  mholdGC = aGC;
  mWidth = aWidth;
  mHeight = aHeight;
  mFlags = aFlags;

  // we can draw on this offscreen because it has no parent
  mIsOffscreen = PR_TRUE;

  PhImage_t   *image = NULL;
  PhDim_t     dim;
  PhArea_t    area;
  PtArg_t     arg[3];

// kirk - replaced Malloc with Calloc and got rid of memset further down
//  image = PR_Malloc(sizeof(PhImage_t));
  image = PR_CALLOC(sizeof(PhImage_t) );
  if (image == NULL)
    return NS_ERROR_FAILURE;
	
  mPixmap     = image;
  area.pos.x  = 0;
  area.pos.y  = 0;
  area.size.w = aWidth;
  area.size.h = aHeight;
  dim.w       = area.size.w;
  dim.h       = area.size.h;

#if 1
  // kedl, hack! weird font not drawing unless
  // the surface is somewhat bigger??
    dim.h += 100;
    dim.w ++;
#endif

printf ("nsDrawingSurfacePh::Init create drawing surface: area=<%d,%d,%d,%d> %p\n",area.pos.x,area.pos.y,area.size.w,area.size.h,image);

  PhPoint_t           translation = { 0, 0 };
  PmMemoryContext_t   *mc;
  short               bytes_per_pixel = 3;

//  memset( image, 0, sizeof(PhImage_t) );
  image->type = Pg_IMAGE_DIRECT_888; // 3 bytes per pixel with this type
  image->size = dim;
  image->image = (char *) PgShmemCreate( dim.w * dim.h * bytes_per_pixel, NULL);

  if (image->image == NULL)
  {
    NS_ASSERTION(0,"nsDrawingSurfacePh::Init Out of Memory calling PgShmemCreate");
    return NS_ERROR_FAILURE;
  }
  
  image->bpl = bytes_per_pixel * dim.w;

  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDrawingSurfacePh::Init  Before calling PmMemCreateMC\n"));

  mc = PmMemCreateMC( image, &dim, &translation );
  if (mc == NULL)
  {
    NS_ASSERTION(0, "nsDrawingSurfacePh::Init Out of Memory calling PmMemCreateMC");
    return NS_ERROR_FAILURE;
  }
  	
  /* Kirk: 10/18/99 Set the type and see if it helps... */
  PmMemSetType(mc, Pm_IMAGE_CONTEXT);

  mGC = (PhGC_t *) mc;

  // now all drawing goes into the memory context
  PhDrawContext_t *oldDC;

  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDrawingSurfacePh::Init  Before calling PmMemStart\n"));

  oldDC = PmMemStart( mc );
  if (oldDC == NULL)
  {
	NS_ASSERTION(0, "nsDrawingSurfacePh::Init - Error calling PmMemStart");
    PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDrawingSurfacePh::Init - Error calling PmMemStart"));
	return NS_ERROR_FAILURE;
  }

  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDrawingSurfacePh::Init  Finished calling PmMemStart\n"));
	    
  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfacePh :: GetGC( PhGC_t **aGC )
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDrawingSurfacePh::GetGC - Not Implemented\n"));
  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfacePh :: ReleaseGC( void )
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDrawingSurfacePh::ReleaseGC - Not Implemented\n"));
  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfacePh :: Select( void )
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDrawingSurfacePh::Select mIsOffscreen=<%d>\n", mIsOffscreen));

  PhGC_t *gc = PgGetGC();

  if (mholdGC==nsnull)
    mholdGC = mGC;


  if (gc == mGC)
  {
    //printf ("don't set gc\n");
    return NS_OK;	// kirk - this was a 0, I think its the same?
  }
  else
  {
    if (mIsOffscreen)
    {
      int err;
	  PhDrawContext_t *oldDC;
	  
      err=PmMemFlush( (PmMemoryContext_t *) mGC, mPixmap ); // get the image
      if (err == -1)
	  {
		NS_ASSERTION(0, "nsDrawingSurfacePh::Select - Error calling PmMemFlush");
		return NS_ERROR_FAILURE;
	  }

      oldDC=PmMemStart( (PmMemoryContext_t *) mGC);
      if (oldDC == NULL)
	  {
		NS_ASSERTION(0, "nsDrawingSurfacePh::Select - Error calling PmMemStart");
		PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDrawingSurfacePh::Select - Error calling PmMemStart"));

		return NS_ERROR_FAILURE;
	  }

      PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDrawingSurfacePh::Select  Finished calling PmMemStart\n"));

    }
    else
    {
      //printf ("going onscreen: %p\n",mGC); fflush(stdout);
      PgSetGC(mGC);
    }
  }

  return NS_OK;
}

void nsDrawingSurfacePh::Stop(void)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDrawingSurfacePh::Stop mIsOffscreen=<%d>\n", mIsOffscreen));
  PgFlush();

  if (mIsOffscreen)
  {
    int err;
    PhDrawContext_t *theMC;

    err=PmMemFlush( (PmMemoryContext_t *) mGC, mPixmap ); // get the image
    if (err == -1)
    {
      NS_ASSERTION(0, "nsDrawingSurfacePh::Stop - Error calling PmMemFlush");
	  return;
	}

    theMC = PmMemStop( (PmMemoryContext_t *) mGC );
	if (theMC == NULL)
	{
	  NS_ASSERTION(0, "nsDrawingSurfacePh::Stop - Error calling PmMemStop");
	  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDrawingSurfacePh::Stop - Error calling PmMemStop\n"));
	}
	else
	{
	  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDrawingSurfacePh::Stop - PmMemStop Successful\n"));
	}
  }
}

PhGC_t *nsDrawingSurfacePh::GetGC(void)
{
	return mGC;
}
