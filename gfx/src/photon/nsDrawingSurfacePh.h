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

#ifndef nsDrawingSurfacePh_h___
#define nsDrawingSurfacePh_h___

#include "nsIDrawingSurface.h"
#include "nsIDrawingSurfacePh.h"


class nsDrawingSurfacePh : public nsIDrawingSurface,
                           public nsIDrawingSurfacePh
{
public:
  nsDrawingSurfacePh();
  virtual ~nsDrawingSurfacePh();
  
  NS_DECL_ISUPPORTS

  //nsIDrawingSurface interface

  NS_IMETHOD Lock(PRInt32 aX, PRInt32 aY, PRUint32 aWidth, PRUint32 aHeight,
                  void **aBits, PRInt32 *aStride, PRInt32 *aWidthBytes,
                  PRUint32 aFlags);
  NS_IMETHOD Unlock(void);

	inline
  NS_IMETHODIMP GetDimensions(PRUint32 *aWidth, PRUint32 *aHeight)
		{
		*aWidth = mWidth;
		*aHeight = mHeight;
		return NS_OK;
		}

  inline NS_IMETHODIMP IsOffscreen( PRBool *aOffScreen ) { *aOffScreen = mIsOffscreen; return NS_OK; }
  inline NS_IMETHODIMP IsPixelAddressable( PRBool *aAddressable ) { *aAddressable = PR_FALSE; return NS_OK; }

	inline
  NS_IMETHODIMP GetPixelFormat(nsPixelFormat *aFormat)
		{ 
		*aFormat = mPixFormat;
		return NS_OK;
		}

  //nsIDrawingSurfacePh interface

  /* Initialize a On-Screen Drawing Surface */
	inline
  NS_IMETHODIMP Init( PhDrawContext_t *aDC, PhGC_t *aGC )
		{
		mGC = aGC;
		mIsOffscreen = PR_FALSE;
		mDrawContext =  aDC;
		return NS_OK;
		}

  /* Initizlize a Off-Screen Drawing Surface */
  NS_IMETHOD Init( PRUint32 aWidth, PRUint32 aHeight, PRUint32 aFlags );

  /* Make this DrawingSurface active */
  inline PhDrawContext_t* Select( )
		{
		PhDCSetCurrent( mDrawContext );
		PgSetGCCx( mDrawContext, mGC );
		return mDrawContext;
		}

  /* Flush the Off-Screen draw buffer to the pixmap or PgFlush the On-Screen */
  inline PhGC_t								*GetGC( void ) { return mGC; }
  inline PhDrawContext_t			*GetDC( void ) { return mDrawContext; }
  inline void									GetSize( PRUint32 *aWidth, PRUint32 *aHeight ) { *aWidth = mWidth; *aHeight = mHeight;}

private:
  PRUint32					mWidth;
  PRUint32					mHeight;
  PRBool						mIsOffscreen;
  PhGC_t        		*mGC;
  PhDrawContext_t		*mDrawContext;
  
  PRUint32					mFlags;

  /* for lock & unlock */
  PhDrawContext_t		*mLockDrawContext;
  PRInt32						mLockX;
  PRInt32						mLockY;
  PRUint32					mLockWidth;
  PRUint32					mLockHeight;
  PRUint32					mLockFlags;
  PRBool						mLocked;
  static nsPixelFormat	mPixFormat;
  static int    				prefChanged(const char* aPref, void* aClosure);
};

#endif
