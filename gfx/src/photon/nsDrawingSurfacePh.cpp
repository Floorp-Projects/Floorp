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
#include <errno.h>

NS_IMPL_ISUPPORTS2(nsDrawingSurfacePh, nsIDrawingSurface, nsIDrawingSurfacePh)

nsDrawingSurfacePh :: nsDrawingSurfacePh()
{
	PhSysInfo_t sysinfo;
	PhRect_t    rect = {{0, 0}, {SHRT_MAX, SHRT_MAX}};
	char        *p;
	int         inp_grp;
	PhRegion_t  rid;

	NS_INIT_REFCNT();

	mDrawContext = nsnull;
	mGC = nsnull;
	mWidth = 0;
	mHeight = 0;
	mFlags = 0;

  	mIsOffscreen = PR_FALSE;
	mLockDrawContext = nsnull;
	mLockWidth = 0;
	mLockHeight = 0;
	mLockFlags = 0;
	mLockX = 0;
	mLockY = 0;
	mLocked = PR_FALSE;

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
	if (mDrawContext)
		 PhDCRelease(mDrawContext);
	mDrawContext = nsnull;

	if (mLockDrawContext)
		 PhDCRelease(mLockDrawContext);
	mLockDrawContext = nsnull;

//	if (mGC)
//		PgDestroyGC(mGC);
	mGC = nsnull;
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
	printf("Lock()\n");
	PhArea_t    dst_area, src_area;
	int         format = 0, bpl;

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

	// create a offscreen context to save the locked rectangle into
//	if (mIsOffscreen)
//	{
		PdOffscreenContext_t *odc = (PdOffscreenContext_t *) mDrawContext;
		format = odc->format;
//	}
	mLockDrawContext = (PhDrawContext_t *)PdCreateOffscreenContext(format, aWidth, aHeight, \
		Pg_OSC_MEM_PAGE_ALIGN);
	if (!mLockDrawContext)
	{
		NS_ASSERTION(0, "Failed to create Offscreen area for lock.");
		return NS_ERROR_FAILURE;
	}
	dst_area.pos.x = dst_area.pos.y = 0;
	dst_area.size.w = aWidth;
	dst_area.size.h = aHeight;
	src_area.pos.x = aX;
	src_area.pos.y = aY;
	src_area.size.w = aWidth;
	src_area.size.h = aHeight;
	PgContextBlitArea((PdOffscreenContext_t *)mDrawContext, &src_area, (PdOffscreenContext_t *)mLockDrawContext, &dst_area);

	*aBits = PdGetOffscreenContextPtr((PdOffscreenContext_t *)mLockDrawContext);
	switch (format)
	{
		case Pg_IMAGE_PALETTE_BYTE:
			bpl = 1;
			break;
		case Pg_IMAGE_DIRECT_8888:
			bpl = 4;
			break;
		case Pg_IMAGE_DIRECT_888:
			bpl = 3;
			break;
		case Pg_IMAGE_DIRECT_565:
		case Pg_IMAGE_DIRECT_555:
			bpl = 2;
			break;
	}
	*aStride = *aWidthBytes = bpl * dst_area.size.w;

	printf("end Lock()\n");
	return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfacePh :: Unlock(void)
{
	printf("UnLock\n");
	PdOffscreenContext_t *off_dc = (PdOffscreenContext_t *) mDrawContext;
	PhArea_t    dst_area, src_area;

	if (!mLocked)
	{
		NS_ASSERTION(0, "attempting to unlock an DS that isn't locked");
		return NS_ERROR_FAILURE;
	}

	// If the lock was not read only, put the bits back on the draw context
	if (!(mLockFlags & NS_LOCK_SURFACE_READ_ONLY))
	{
		dst_area.pos.x = mLockX;
		dst_area.pos.y = mLockY;
		dst_area.size.w = mLockWidth;
		dst_area.size.h = mLockHeight;
		src_area.pos.x = src_area.pos.y = 0;
		src_area.size.w = mLockWidth;
		src_area.size.h = mLockHeight;
		PgContextBlitArea((PdOffscreenContext_t *)mLockDrawContext, &src_area, (PdOffscreenContext_t *)mDrawContext, &dst_area);
		// release the mLockDrawContext somehow !!
		PhDCRelease((PdDirectContext_t *)mLockDrawContext);
		mLockDrawContext = nsnull;
	}

	mLocked = PR_FALSE;

	printf("end UnLock\n");
  	return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfacePh :: GetDimensions(PRUint32 *aWidth, PRUint32 *aHeight)
{
  *aWidth = mWidth;
  *aHeight = mHeight;

  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfacePh :: IsOffscreen(PRBool *aOffScreen)
{
  *aOffScreen = mIsOffscreen;

  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfacePh :: IsPixelAddressable(PRBool *aAddressable)
{
  *aAddressable = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfacePh :: GetPixelFormat(nsPixelFormat *aFormat)
{
  *aFormat = mPixFormat;
  return NS_OK;
}


NS_IMETHODIMP nsDrawingSurfacePh :: Init( PhGC_t * &aGC )
{
//	if (mGC)
  //  	PgDestroyGC(mGC);

    mGC = aGC;

    // this is definatly going to be on the screen, as it will be the window of a
    // widget or something.
    mIsOffscreen = PR_FALSE;

    // create an onscreen context with the current video modes image depth
    //mDrawContext = (PhDrawContext_t *)PdCreateOffscreenContext(0, 0, 0, Pg_OSC_MAIN_DISPLAY);
    mDrawContext = nsnull;

  	return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfacePh :: Init( PhGC_t * &aGC, PRUint32 aWidth,
                                          PRUint32 aHeight, PRUint32 aFlags)
{
//	if (mGC)
//		PgDestroyGC(mGC);

	mGC = aGC;
	mWidth = aWidth;
	mHeight = aHeight;
	mFlags = aFlags;

	// we can draw on this offscreen because it has no parent
	mIsOffscreen = PR_TRUE;

	// create an offscreen context with the current video modes image depth
	mDrawContext = (PhDrawContext_t *)PdCreateOffscreenContext(0, mWidth, mHeight, 0);
	if (!mDrawContext)
	{
		NS_ASSERTION(0, "Failed to create Offscreen Context for DrawingSurface");
		return NS_ERROR_FAILURE;
	}

  	return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfacePh :: Select( void )
{
  PhGC_t *gc = PgGetGC();
  PhDrawContext_t *dc = PhDCGetCurrent();
  
  if (mDrawContext != dc)
  {
    PhDrawContext_t *old_dc;
    old_dc = PhDCSetCurrent(mDrawContext);

    gc = PgGetGC();
	if (gc != mGC)
	{
		PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDrawingSurfacePh::Select Setting GC to mGC=<%p>\n", mGC));
		PgSetGC(mGC);  
	}
  }

  return NS_OK;
}

PhGC_t *nsDrawingSurfacePh::GetGC(void)
{
  return mGC;
}

PhDrawContext_t *nsDrawingSurfacePh::GetDC(void)
{
  return mDrawContext;	// xyz
}

NS_IMETHODIMP nsDrawingSurfacePh::Flush(void)
{
  int err;

  return NS_OK;
}

PRBool nsDrawingSurfacePh::IsActive(void)
{
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
