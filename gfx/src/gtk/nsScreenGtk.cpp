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
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsScreenGtk.h"

#include <gdk/gdk.h>
#include <gdk/gdkx.h>


nsScreenGtk :: nsScreenGtk (  )
{
  NS_INIT_REFCNT();

  mScreenNum = 0;
  mXOrg = 0;
  mYOrg = 0;
  // these always default to the full screen size
  mWidth = gdk_screen_width();
  mHeight = gdk_screen_height();
}


nsScreenGtk :: ~nsScreenGtk()
{
  // nothing to see here.
}


// addref, release, QI
NS_IMPL_ISUPPORTS(nsScreenGtk, NS_GET_IID(nsIScreen))


NS_IMETHODIMP
nsScreenGtk :: GetRect(PRInt32 *outLeft, PRInt32 *outTop, PRInt32 *outWidth, PRInt32 *outHeight)
{
  *outLeft = mXOrg;
  *outTop = mYOrg;
  *outWidth = mWidth;
  *outHeight = mHeight;

  return NS_OK;
  
} // GetRect


NS_IMETHODIMP
nsScreenGtk :: GetAvailRect(PRInt32 *outLeft, PRInt32 *outTop, PRInt32 *outWidth, PRInt32 *outHeight)
{
  *outTop = mXOrg;
  *outLeft = mYOrg;
  *outWidth = mWidth;
  *outHeight = mHeight;

  return NS_OK;
  
} // GetAvailRect


NS_IMETHODIMP 
nsScreenGtk :: GetPixelDepth(PRInt32 *aPixelDepth)
{
  GdkVisual * rgb_visual = gdk_rgb_get_visual();
  *aPixelDepth = rgb_visual->depth;

  return NS_OK;

} // GetPixelDepth


NS_IMETHODIMP 
nsScreenGtk :: GetColorDepth(PRInt32 *aColorDepth)
{
  return GetPixelDepth ( aColorDepth );

} // GetColorDepth


