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

  mPixmap = nsnull;
  mGC = nsnull;
  mWidth = mHeight = 0;
  mFlags = 0;

}

nsDrawingSurfacePh :: ~nsDrawingSurfacePh()
{
   PgShmemDestroy( mPixmap->image );
   PmMemReleaseMC( (PmMemoryContext_t *) mGC);
   free (mPixmap);
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

NS_IMETHODIMP nsDrawingSurfacePh :: Lock(PRInt32 aX, PRInt32 aY,
                                          PRUint32 aWidth, PRUint32 aHeight,
                                          void **aBits, PRInt32 *aStride,
                                          PRInt32 *aWidthBytes, PRUint32 aFlags)
{
printf ("kedl: drawingsurface lock\n");
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDrawingSurfacePh::Lock - Not Implemented\n"));
  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfacePh :: Unlock(void)
{
printf ("kedl: drawingsurface unlock\n");
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDrawingSurfacePh::Unlock - Not Implemented\n"));
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
  *aAddressable = PR_FALSE;
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDrawingSurfacePh::IsPixelAddressable - Not Implemented\n"));

  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfacePh :: GetPixelFormat(nsPixelFormat *aFormat)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDrawingSurfacePh::GetPixelFormat - Not Implemented\n"));
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

  PhImage_t *image;
  PhDim_t dim;
  PhArea_t    area;
  PtArg_t     arg[3];

  image = malloc(sizeof(PhImage_t));
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

  // DVS
  PgSetRegion( mholdGC->rid );
//  ApplyClipping();

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
  if (mholdGC==nsnull) mholdGC = mGC;

  if (mIsOffscreen)
  {
//printf ("going offscreen\n");
    PmMemStart( (PmMemoryContext_t *) mGC);
    PgSetRegion(mGC->rid);
  }
  else
  {
//printf ("going onscreen\n");
	PgSetGC(mGC);
	PgSetRegion(mGC->rid);
  }

  return NS_OK;
}

void nsDrawingSurfacePh::Stop(void)
{
  PmMemFlush( (PmMemoryContext_t *) mGC, mPixmap ); // get the image
  PmMemStop( (PmMemoryContext_t *) mGC );
}

PhGC_t *nsDrawingSurfacePh::GetGC(void)
{
	return mGC;
}
