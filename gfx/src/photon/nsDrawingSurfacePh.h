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
  NS_IMETHOD GetDimensions(PRUint32 *aWidth, PRUint32 *aHeight);
  NS_IMETHOD IsOffscreen(PRBool *aOffScreen);
  NS_IMETHOD IsPixelAddressable(PRBool *aAddressable);
  NS_IMETHOD GetPixelFormat(nsPixelFormat *aFormat);

  //nsIDrawingSurfacePh interface

  /* Initialize a On-Screen Drawing Surface */
  NS_IMETHOD Init(PhGC_t * &aGC);

  /* Initizlize a Off-Screen Drawing Surface */
  NS_IMETHOD Init(PhGC_t * &aGC, PRUint32 aWidth, PRUint32 aHeight, PRUint32 aFlags);

  /* Make this DrawingSurface active */
  NS_IMETHOD Select(void);

  /* Flush the Off-Screen draw buffer to the pixmap or PgFlush the On-Screen */
  NS_IMETHOD Flush(void);

  /* The GC is not ref counted, make sure you know what your doing */
  PhGC_t     *GetGC(void);
  PhDrawContext_t     *GetDC(void);

  /* Is this Drawing Surface Active? */
  PRBool      IsActive();
  void *GetDrawContext(void);

public:
  PRUint32			mWidth;
  PRUint32			mHeight;

private:

  PRBool			mIsOffscreen;
  PhGC_t        	*mGC;
  PhDrawContext_t	*mDrawContext;
  
  PRUint32			mFlags;
  nsPixelFormat 	mPixFormat;

  /* for lock & unlock */
  PhDrawContext_t	*mLockDrawContext;
  PRInt32			mLockX;
  PRInt32			mLockY;
  PRUint32			mLockWidth;
  PRUint32			mLockHeight;
  PRUint32			mLockFlags;
  PRBool			mLocked;
};

#endif
