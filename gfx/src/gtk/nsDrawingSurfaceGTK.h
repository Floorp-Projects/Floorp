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

#ifndef nsDrawingSurfaceGTK_h___
#define nsDrawingSurfaceGTK_h___

#include "nsIDrawingSurface.h"
#include "nsIDrawingSurfaceGTK.h"

#include <gtk/gtk.h>

class nsDrawingSurfaceGTK : public nsIDrawingSurface,
                            nsIDrawingSurfaceGTK
{
public:
  nsDrawingSurfaceGTK();
  virtual ~nsDrawingSurfaceGTK();

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

  //nsIDrawingSurfaceGTK interface

  NS_IMETHOD Init(GdkDrawable *aDrawable, GdkGC *aGC);
  NS_IMETHOD Init(GdkGC *aGC, PRUint32 aWidth, PRUint32 aHeight, PRUint32 aFlags);

/* get the GC and manage the GdkGC's refcount */
  NS_IMETHOD GetGC(GdkGC *aGC);
  NS_IMETHOD ReleaseGC(void);

/* below are utility functions used mostly for nsRenderingContext and nsImage
 * to plug into gdk_* functions for drawing.  You should not set a pointer
 * that might hang around with the return from these.  instead use the ones
 * above.  pav
 */
  GdkGC *GetGC(void);
  GdkDrawable *GetDrawable(void);

private:
  /* general */
  GdkPixmap	*mPixmap;
  GdkGC		*mGC;
  gint		mDepth;
  nsPixelFormat	mPixFormat;
  PRUint32      mWidth;
  PRUint32      mHeight;
  PRUint32	mFlags;
  PRBool	mIsOffscreen;

  /* for locks */
  GdkImage	*mImage;
  PRInt32	mLockX;
  PRInt32	mLockY;
  PRUint32	mLockWidth;
  PRUint32	mLockHeight;
  PRUint32	mLockFlags;
  PRBool	mLocked;
};

#endif
