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

#ifndef nsDrawingSurfaceGTK_h___
#define nsDrawingSurfaceGTK_h___

#include "nsIDrawingSurface.h"
#include "nsIDrawingSurfaceGTK.h"

#include "nsTimer.h"

#include <gtk/gtk.h>

class nsDrawingSurfaceGTK : public nsIDrawingSurface,
                            public nsIDrawingSurfaceGTK
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

/* below are utility functions used mostly for nsRenderingContext and nsImage
 * to plug into gdk_* functions for drawing.  You should not set a pointer
 * that might hang around with the return from these.  instead use the ones
 * above.  pav
 */
  GdkDrawable *GetDrawable(void) { return mPixmap; }

  void GetSize(PRUint32 *aWidth, PRUint32 *aHeight) { *aWidth = mWidth; *aHeight = mHeight; }

  PRInt32 GetDepth() { return mDepth; }

protected:
  inline PRUint8 ConvertMaskToCount(unsigned long val);

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

  //  MOZ_TIMER_DECLARE(mLockTime)
  //  MOZ_TIMER_DECLARE(mUnlockTime)
};

#endif
