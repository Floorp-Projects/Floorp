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

#ifndef nsDrawingSurfaceBeOS_h___
#define nsDrawingSurfaceBeOS_h___

#include "nsIDrawingSurface.h"

#include <Bitmap.h>
#include <View.h>

class nsDrawingSurfaceBeOS : public nsIDrawingSurface
{
public:
  nsDrawingSurfaceBeOS();

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

  // local methods
  NS_IMETHOD Init(BView *aView);
  NS_IMETHOD Init(BView *aView, PRUint32 aWidth, PRUint32 aHeight,
                  PRUint32 aFlags);
  NS_IMETHOD GetView(BView **aView);
  NS_IMETHOD ReleaseView(void);
  NS_IMETHOD GetBitmap(BBitmap **aBitmap);
  NS_IMETHOD ReleaseBitmap(void);

private:
  virtual ~nsDrawingSurfaceBeOS();

  BView			*mView;
  BBitmap		*mBitmap;
  nsPixelFormat mPixFormat;
  PRUint32      mWidth;
  PRUint32      mHeight;
  PRInt32       mLockOffset;
  PRInt32       mLockHeight;
  PRUint32      mLockFlags;
};

#endif
