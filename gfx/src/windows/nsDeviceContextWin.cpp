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

#include "nsDeviceContextWin.h"
#include "nsRenderingContextWin.h"

nsDeviceContextWin :: nsDeviceContextWin()
  : DeviceContextImpl()
{
  HDC hdc = ::GetDC(NULL);

  mTwipsToPixels = ((float)::GetDeviceCaps(hdc, LOGPIXELSX)) / (float)NSIntPointsToTwips(72); // XXX shouldn't be LOGPIXELSY ??
  mPixelsToTwips = 1.0f / mTwipsToPixels;

  ::ReleaseDC(NULL, hdc);

  mSurface = NULL;
}

nsDeviceContextWin :: ~nsDeviceContextWin()
{
  if (NULL != mSurface)
  {
    DeleteDC(mSurface);
    mSurface = NULL;
  }
}

float nsDeviceContextWin :: GetScrollBarWidth() const
{
  return ::GetSystemMetrics(SM_CXVSCROLL) * mDevUnitsToAppUnits;
}

float nsDeviceContextWin :: GetScrollBarHeight() const
{
  return ::GetSystemMetrics(SM_CXHSCROLL) * mDevUnitsToAppUnits;
}

nsDrawingSurface nsDeviceContextWin :: GetDrawingSurface(nsIRenderingContext &aContext)
{
  if (NULL == mSurface)
    mSurface = aContext.CreateDrawingSurface(nsnull);

  return mSurface;
}


