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

#include "nsDrawingSurfacePh.h"
#include "prmem.h"

#include "nsPhGfxLog.h"
#include <photon/PhRender.h>
#include <Pt.h>
#include <errno.h>


NS_IMPL_ISUPPORTS2( nsDrawingSurfacePh, nsIDrawingSurface, nsIDrawingSurfacePh )

nsDrawingSurfacePh :: nsDrawingSurfacePh( ) {
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

nsDrawingSurfacePh :: ~nsDrawingSurfacePh( ) {

	if(mDrawContext) {
		mDrawContext->gc = nsnull; /* because we do not own mGC and mDrawContext->gc, do not allow PhDCRelease to release this, as it belongs to upper classes */
		PhDCRelease( mDrawContext ); /* the mDrawContext->gc will be free by the upper classes */
		mDrawContext = nsnull;
		}

	if( mLockDrawContext ) {
		PhDCRelease(mLockDrawContext);
		mLockDrawContext = nsnull;
		}

	mGC = nsnull; /* don't release the GC - it has not been allocated, it has only been instantiated to the current GC */
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
NS_IMETHODIMP nsDrawingSurfacePh :: Lock( PRInt32 aX, PRInt32 aY,
                                          PRUint32 aWidth, PRUint32 aHeight,
                                          void **aBits, PRInt32 *aStride,
                                          PRInt32 *aWidthBytes, PRUint32 aFlags ) {
	PhArea_t    dst_area, src_area;
	int         format = 0, bpl;

	if( mLocked ) return NS_ERROR_FAILURE;

	mLocked = PR_TRUE;

	mLockX = aX;
	mLockY = aY;
	mLockWidth = aWidth;
	mLockHeight = aHeight;
	mLockFlags = aFlags;

	// create a offscreen context to save the locked rectangle into
	PdOffscreenContext_t *odc = (PdOffscreenContext_t *) mDrawContext;
	format = odc->format;
	mLockDrawContext = ( PhDrawContext_t * )PdCreateOffscreenContext( format, aWidth, aHeight, Pg_OSC_MEM_PAGE_ALIGN );
	if( !mLockDrawContext ) return NS_ERROR_FAILURE;

	dst_area.pos.x = dst_area.pos.y = 0;
	dst_area.size.w = aWidth;
	dst_area.size.h = aHeight;
	src_area.pos.x = aX;
	src_area.pos.y = aY;
	src_area.size.w = aWidth;
	src_area.size.h = aHeight;
	PgContextBlitArea( (PdOffscreenContext_t *)mDrawContext, &src_area, (PdOffscreenContext_t *) mLockDrawContext, &dst_area );

	*aBits = PdGetOffscreenContextPtr( (PdOffscreenContext_t *) mLockDrawContext );
	switch( format ) {
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

	return NS_OK;
	}

NS_IMETHODIMP nsDrawingSurfacePh :: Unlock( void ) {
	PhArea_t    dst_area, src_area;

	if( !mLocked ) return NS_ERROR_FAILURE;

	// If the lock was not read only, put the bits back on the draw context
	if( !( mLockFlags & NS_LOCK_SURFACE_READ_ONLY ) ) {
		dst_area.pos.x = mLockX;
		dst_area.pos.y = mLockY;
		dst_area.size.w = mLockWidth;
		dst_area.size.h = mLockHeight;
		src_area.pos.x = src_area.pos.y = 0;
		src_area.size.w = mLockWidth;
		src_area.size.h = mLockHeight;
		PgContextBlitArea( (PdOffscreenContext_t *) mLockDrawContext, &src_area, (PdOffscreenContext_t *) mDrawContext, &dst_area );

		// release the mLockDrawContext somehow !!
		PhDCRelease( (PdDirectContext_t *) mLockDrawContext );
		mLockDrawContext = nsnull;
		}

	mLocked = PR_FALSE;
	return NS_OK;
	}

NS_IMETHODIMP nsDrawingSurfacePh :: GetDimensions( PRUint32 *aWidth, PRUint32 *aHeight ) {
  *aWidth = mWidth;
  *aHeight = mHeight;
  return NS_OK;
	}

NS_IMETHODIMP nsDrawingSurfacePh :: IsOffscreen( PRBool *aOffScreen ) {
  *aOffScreen = mIsOffscreen;
  return NS_OK;
	}

NS_IMETHODIMP nsDrawingSurfacePh :: IsPixelAddressable( PRBool *aAddressable ) {
  *aAddressable = PR_FALSE;
  return NS_OK;
	}

NS_IMETHODIMP nsDrawingSurfacePh :: GetPixelFormat( nsPixelFormat *aFormat ) {
  *aFormat = mPixFormat;
  return NS_OK;
	}


NS_IMETHODIMP nsDrawingSurfacePh :: Init( PhGC_t * &aGC ) {

	mGC = aGC;

	// this is definatly going to be on the screen, as it will be the window of a widget or something
	mIsOffscreen = PR_FALSE;
	mDrawContext = nsnull;
	return NS_OK;
	}

NS_IMETHODIMP nsDrawingSurfacePh :: Init( PhGC_t * &aGC, PRUint32 aWidth, PRUint32 aHeight, PRUint32 aFlags ) {
	mGC = aGC;
	mWidth = aWidth;
	mHeight = aHeight;
	mFlags = aFlags;

	// we can draw on this offscreen because it has no parent
	mIsOffscreen = PR_TRUE;

	// create an offscreen context with the current video modes image depth
	mDrawContext = (PhDrawContext_t *)PdCreateOffscreenContext(0, mWidth, mHeight, 0);
	if( !mDrawContext ) return NS_ERROR_FAILURE;

	/* use the gc provided */
	PgDestroyGC( mDrawContext->gc );
	mDrawContext->gc = mGC;

 	return NS_OK;
	}

NS_IMETHODIMP nsDrawingSurfacePh :: Select( void ) {
	PhDCSetCurrent( mDrawContext );
	PgSetGC( mGC );
	return NS_OK;
	}

PhGC_t *nsDrawingSurfacePh::GetGC( void ) { return mGC; }
PhDrawContext_t *nsDrawingSurfacePh::GetDC( void ) { return mDrawContext; }
NS_IMETHODIMP nsDrawingSurfacePh::Flush(void) { return NS_OK; }
PRBool nsDrawingSurfacePh::IsActive( void ) { return mDrawContext == PhDCGetCurrent() ? PR_TRUE : PR_FALSE; }
