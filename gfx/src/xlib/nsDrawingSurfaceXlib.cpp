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

#include "nsDrawingSurfaceXlib.h"

static NS_DEFINE_IID(kIDrawingSurfaceIID, NS_IDRAWING_SURFACE_IID);

extern Display         *gDisplay;
extern Screen          *gScreen;
extern int              gScreenNum;
extern int              gDepth;
extern Visual          *gVisual;
extern XVisualInfo     *gVisualInfo;

nsDrawingSurfaceXlib::nsDrawingSurfaceXlib()
{
  NS_INIT_REFCNT();
  mPixmap = 0;
  mGC = 0;
  // set up the masks for the pix formats
  mPixFormat.mRedMask = gVisualInfo->red_mask;
  mPixFormat.mGreenMask = gVisualInfo->green_mask;
  mPixFormat.mBlueMask = gVisualInfo->blue_mask;
}

NS_IMPL_QUERY_INTERFACE(nsDrawingSurfaceXlib, kIDrawingSurfaceIID)
NS_IMPL_ADDREF(nsDrawingSurfaceXlib)
NS_IMPL_RELEASE(nsDrawingSurfaceXlib)


NS_IMETHODIMP
nsDrawingSurfaceXlib::Lock(PRInt32 aX, PRInt32 aY,
                           PRUint32 aWidth, PRUint32 aHeight,
                           void **aBits, PRInt32 *aStride,
                           PRInt32 *aWidthBytes, PRUint32 aFlags)
{
  return NS_OK;
}

NS_IMETHODIMP
nsDrawingSurfaceXlib::Unlock(void)
{
  return NS_OK;
}

NS_IMETHODIMP
nsDrawingSurfaceXlib::GetDimensions(PRUint32 *aWidth, PRUint32 *aHeight)
{
  return NS_OK;
}

NS_IMETHODIMP
nsDrawingSurfaceXlib::IsOffscreen(PRBool *aOffScreen)
{
  return NS_OK;
}

NS_IMETHODIMP
nsDrawingSurfaceXlib::IsPixelAddressable(PRBool *aAddressable)
{
  return NS_OK;
}

NS_IMETHODIMP
nsDrawingSurfaceXlib::GetPixelFormat(nsPixelFormat *aFormat)
{
  return NS_OK;
}
