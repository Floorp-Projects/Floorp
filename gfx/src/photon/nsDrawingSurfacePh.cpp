/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsDrawingSurfacePh.h"
#include "nsCOMPtr.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "prmem.h"

#include "nsPhGfxLog.h"
#include <photon/PhRender.h>
#include <Pt.h>
#include <errno.h>

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

nsPixelFormat nsDrawingSurfacePh::mPixFormat = {
	0, // mRedZeroMask;     //red color mask in zero position
	0, // mGreenZeroMask;   //green color mask in zero position
	0, // mBlueZeroMask;    //blue color mask in zero position
	0, // mAlphaZeroMask;   //alpha data mask in zero position
	0xff0000, // mRedMask;         //red color mask
	0x00ff00, // mGreenMask;       //green color mask
	0x0000ff, // mBlueMask;        //blue color mask
	0, // mAlphaMask;       //alpha data mask
	0, // mRedCount;        //number of red color bits
	0, // mGreenCount;      //number of green color bits
	0, // mBlueCount;       //number of blue color bits
	0, // mAlphaCount;      //number of alpha data bits
	16, // mRedShift;        //number to shift value into red position
	8, // mGreenShift;      //number to shift value into green position
	0, // mBlueShift;       //number to shift value into blue position
	0 // mAlphaShift;      //number to shift value into alpha position
	};

NS_IMPL_ISUPPORTS2( nsDrawingSurfacePh, nsIDrawingSurface, nsIDrawingSurfacePh )

nsDrawingSurfacePh :: nsDrawingSurfacePh( ) 
{
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
}

nsDrawingSurfacePh :: ~nsDrawingSurfacePh( ) 
{
	if( mIsOffscreen ) {
		mDrawContext->gc = NULL;
		PhDCRelease( mDrawContext ); /* the mDrawContext->gc will be freed by the upper classes */
	}
	
	if( mLockDrawContext ) {
		PhDCRelease(mLockDrawContext);
	}

	if( mIsOffscreen ) {
		nsresult rv;
		nsCOMPtr<nsIPref> prefs = do_GetService(kPrefCID, &rv);
		if (NS_SUCCEEDED(rv)) {
			prefs->UnregisterCallback("browser.display.internaluse.graphics_changed", prefChanged, (void *)this);
		}

		if( mGC ) PgDestroyGC( mGC );
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
NS_IMETHODIMP nsDrawingSurfacePh :: Lock( PRInt32 aX, PRInt32 aY,
                                          PRUint32 aWidth, PRUint32 aHeight,
                                          void **aBits, PRInt32 *aStride,
                                          PRInt32 *aWidthBytes, PRUint32 aFlags ) {
	PhArea_t    dst_area, src_area;
	int         format = 0, bpl;

	if( mLocked ) return NS_ERROR_FAILURE;

	// create an offscreen context to save the locked rectangle into
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
	if( *aBits == nsnull ) return NS_ERROR_FAILURE; /* on certain hardware this may fail */

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
		default:
		    return NS_ERROR_FAILURE;
		}

	*aStride = *aWidthBytes = bpl * dst_area.size.w;

	mLocked = PR_TRUE;
	mLockX = aX;
	mLockY = aY;
	mLockWidth = aWidth;
	mLockHeight = aHeight;
	mLockFlags = aFlags;

	return NS_OK;
	}

NS_IMETHODIMP nsDrawingSurfacePh :: Unlock( void ) {
	PhArea_t dst_area, src_area;

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
		}

	// release the mLockDrawContext somehow !!
	PhDCRelease( (PdDirectContext_t *) mLockDrawContext );
	mLockDrawContext = nsnull;

	mLocked = PR_FALSE;
	return NS_OK;
	}

NS_IMETHODIMP nsDrawingSurfacePh :: Init( PRUint32 aWidth, PRUint32 aHeight, PRUint32 aFlags ) {
	mWidth = aWidth;
	mHeight = aHeight;
	mFlags = aFlags;
	
	mIsOffscreen = PR_TRUE;

	/* an offscreen surface owns its own PhGC_t */
	mGC = PgCreateGC( 0 );

	mDrawContext = (PhDrawContext_t *)PdCreateOffscreenContext(0, mWidth, mHeight, 0);
	if( !mDrawContext ) return NS_ERROR_FAILURE;

	PgSetDrawBufferSizeCx( mDrawContext, 0xffff );

	nsresult rv;
	nsCOMPtr<nsIPref> prefs(do_GetService(kPrefCID, &rv));
	if (NS_SUCCEEDED(rv)) {
		prefs->RegisterCallback("browser.display.internaluse.graphics_changed", prefChanged, (void *)this);
		}

 	return NS_OK;
	}

int nsDrawingSurfacePh::prefChanged(const char *aPref, void *aClosure)
{
	nsDrawingSurfacePh *surface = (nsDrawingSurfacePh*)aClosure;
	
	if( surface->mLockDrawContext ) {
		PhDCRelease(surface->mLockDrawContext);
		surface->mLockDrawContext = nsnull;
		}

	if(surface->mIsOffscreen) {
		surface->mDrawContext->gc = nsnull; /* because we do not want to destroy the one we have since other have it */
		PhDCRelease( surface->mDrawContext ); 
		surface->mDrawContext = (PhDrawContext_t *)PdCreateOffscreenContext(0, surface->mWidth, surface->mHeight, 0);
		if( !surface->mDrawContext ) return NS_ERROR_FAILURE;

		PgSetDrawBufferSizeCx( surface->mDrawContext, 0xffff );

		PgDestroyGC(surface->mDrawContext->gc);
		surface->mDrawContext->gc = surface->mGC;
		/* use the gc provided */
	}
	return 0;
}
