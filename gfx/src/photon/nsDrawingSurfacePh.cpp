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

#ifdef NGLAYOUT_DDRAW
#define INITGUID
#endif

#include "nsDrawingSurfacePh.h"
#include "prmem.h"
#include "nsCRT.h"

#include "nsPhGfxLog.h"
#include <photon/PhRender.h>
#include <Pt.h>

//#define GFX_DEBUG

#ifdef GFX_DEBUG
  #define BREAK_TO_DEBUGGER           DebugBreak()
#else   
  #define BREAK_TO_DEBUGGER
#endif  

#ifdef GFX_DEBUG
  #define VERIFY(exp)                 ((exp) ? 0: (GetLastError(), BREAK_TO_DEBUGGER))
#else   // !_DEBUG
  #define VERIFY(exp)                 (exp)
#endif  // !_DEBUG

static NS_DEFINE_IID(kIDrawingSurfaceIID, NS_IDRAWING_SURFACE_IID);
static NS_DEFINE_IID(kIDrawingSurfacePhIID, NS_IDRAWING_SURFACE_PH_IID);


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
    PmMemFlush( (PmMemoryContext_t *) mGC, mPixmap );
    PmMemReleaseMC( (PmMemoryContext_t *) mGC);
    PgShmemDestroy( mPixmap->image );
    PR_Free (mPixmap);
    PgSetGC(mholdGC);
  }
}

NS_IMETHODIMP nsDrawingSurfacePh :: QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr)
    return NS_ERROR_NULL_POINTER;

  if (aIID.Equals(kIDrawingSurfaceIID))
  {
    nsIDrawingSurface* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }

  if (aIID.Equals(kIDrawingSurfacePhIID))
  {
    nsIDrawingSurfacePh* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }

  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

  if (aIID.Equals(kISupportsIID))
  {
    nsIDrawingSurface* tmp = this;
    nsISupports* tmp2 = tmp;
    *aInstancePtr = (void*) tmp2;
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsDrawingSurfacePh)
NS_IMPL_RELEASE(nsDrawingSurfacePh)

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
//printf ("lock\n");
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

  PhImage_t *image;
  PhDim_t dim;
  short               bytes_per_pixel = 3;

  image = PR_Malloc(sizeof(PhImage_t));
  if (image == NULL)
    return NS_ERROR_FAILURE;

  mImage = image;

  dim.w = mLockWidth;
  dim.h = mLockHeight;

  memset( image, 0, sizeof(PhImage_t) );
  image->type = Pg_IMAGE_DIRECT_888; // 3 bytes per pixel with this type
  image->size = dim;
//  image->image = (char *) PgShmemCreate( dim.w * dim.h * bytes_per_pixel, NULL);
  image->image = (char *) PR_Malloc( dim.w * dim.h * bytes_per_pixel);
  if (image->image == NULL)
    return NS_ERROR_FAILURE;
	  
  image->bpl = bytes_per_pixel*dim.w;

int y;
for (y=0;y<dim.h;y++)
	memcpy(image->image+y*dim.w*3,mPixmap->image+3*mLockX+(mLockY+y)*mPixmap->bpl,dim.w*3);

//  *aBits = mImage->image+mLockX*3+mLockY*mImage->bpl;
  *aBits = mImage->image;
  *aWidthBytes = aWidth*3;
  *aStride = mImage->bpl;

  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfacePh :: XOR(PRInt32 aX, PRInt32 aY,
                                          PRUint32 aWidth, PRUint32 aHeight)
{
//printf ("XOR: %d, %d %d %d %d\n",mIsOffscreen,aX,aY,aWidth,aHeight);

int x;
int y;
unsigned char *ptr;

PmMemFlush( (PmMemoryContext_t *) mGC, mPixmap ); // get the image
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
//printf ("unlock: %p\n",Mask);
  if (!mLocked)
  {
    NS_ASSERTION(0, "attempting to unlock an DS that isn't locked");
    return NS_ERROR_FAILURE;
  }

  // If the lock was not read only, put the bits back on the pixmap
  if (!(mLockFlags & NS_LOCK_SURFACE_READ_ONLY))
  {
//printf ("really put data back: %d %d %d %d (%d %d)\n",mLockX,mLockY,mLockWidth,mLockHeight,mImage->size.w,mImage->size.h);
  PhPoint_t pos = { mLockX, mLockY };

    Select();
    if (Mask)
    {
//      printf ("use mask\n");
int bpl;
   bpl = (mLockWidth+7)/8;
   bpl = (bpl + 3) & ~0x3;
      PgDrawTImage( mImage->image, mImage->type, &pos, &mImage->size, mImage->bpl, 0 ,Mask,bpl);
    }
    else
      PgDrawImage( mImage->image, mImage->type, &pos, &mImage->size, mImage->bpl, 0 );

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

  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDrawingSurfacePh::GetDimensions - Not Implemented\n"));
  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfacePh :: IsOffscreen(PRBool *aOffScreen)
{
  *aOffScreen = mIsOffscreen;
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDrawingSurfacePh::IsOffScreen - Not Implemented\n"));
  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfacePh :: IsPixelAddressable(PRBool *aAddressable)
{
// FIXME
printf ("ispixeladdressable\n");
  *aAddressable = PR_FALSE;
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDrawingSurfacePh::IsPixelAddressable - Not Implemented\n"));

  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfacePh :: GetPixelFormat(nsPixelFormat *aFormat)
{
//printf ("getpixelformat\n");
  *aFormat = mPixFormat;
  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfacePh :: Init( PhGC_t * &aGC )
{
  mGC = aGC;
  mIsOffscreen = PR_FALSE;	// is onscreen
  mPixmap = NULL; // is onscreen

  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDrawingSurfacePh::Init with PhGC_t\n"));

  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfacePh :: Init( PhGC_t * &aGC, PRUint32 aWidth,
                                          PRUint32 aHeight, PRUint32 aFlags)
{
//printf ("kedl: init with width and height\n");
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDrawingSurfacePh::Init with PhGC_t + width/height\n"));

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

  image = PR_Malloc(sizeof(PhImage_t));
  if (image == NULL)
    return NS_ERROR_FAILURE;
	
  mPixmap = image;

  area.pos.x=0;
  area.pos.y=0;
  area.size.w=aWidth;
  area.size.h=aHeight;
  dim.w = area.size.w;
  dim.h = area.size.h;
  dim.h += 100;                 // kedl, uggggg hack! weird font not drawing unless
                                // the surface is somewhat bigger??
  dim.w ++;

//printf ("kedl: create drawing surface: %d %d %d %d, %lu\n",area.pos.x,area.pos.y,area.size.w,area.size.h,image);

  PhPoint_t           translation = { 0, 0 }, center, radii;
  PmMemoryContext_t   *mc;
  short               bytes_per_pixel = 3;

  memset( image, 0, sizeof(PhImage_t) );
  image->type = Pg_IMAGE_DIRECT_888; // 3 bytes per pixel with this type
  image->size = dim;
  image->image = (char *) PgShmemCreate( dim.w * dim.h * bytes_per_pixel, NULL);
  image->bpl = bytes_per_pixel*dim.w;

  mc = PmMemCreateMC( image, &dim, &translation );

  mGC = (PhGC_t *)mc;

  // now all drawing goes into the memory context
  PmMemStart( mc );
  
//  PgSetRegion( mholdGC->rid );

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
PhGC_t *gc=PgGetGC();

  if (mholdGC==nsnull) mholdGC = mGC;


  if (gc==mGC)
  {
    //printf ("don't set gc\n");
    return 0;
  }
  else
  {
    if (mIsOffscreen)
    {
      //printf ("going offscreen: %p\n",mGC); fflush(stdout);
      PmMemFlush( (PmMemoryContext_t *) mGC, mPixmap ); // get the image
      PmMemStart( (PmMemoryContext_t *) mGC);
//      PgSetRegion(mGC->rid);
    }
    else
    {
      //printf ("going onscreen: %p\n",mGC); fflush(stdout);
      PgSetGC(mGC);
//      PgSetRegion(mGC->rid);
    }
  }

  return 1;
}

void nsDrawingSurfacePh::Stop(void)
{
//  printf ("offscreen: %d\n",mIsOffscreen);
  if (mIsOffscreen)
  {
    PmMemFlush( (PmMemoryContext_t *) mGC, mPixmap ); // get the image
    PmMemStop( (PmMemoryContext_t *) mGC );
  }
}

PhGC_t *nsDrawingSurfacePh::GetGC(void)
{
	return mGC;
}
