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
#include "../nsGfxCIID.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kDeviceContextIID, NS_IDEVICE_CONTEXT_IID);

nsDeviceContextWin :: nsDeviceContextWin()
{
  NS_INIT_REFCNT();

  HDC hdc = ::GetDC(NULL);

  mTwipsToPixels = ::GetDeviceCaps(hdc, LOGPIXELSX) / NS_POINTS_TO_TWIPS_FLOAT(72.0f);
  mPixelsToTwips = 1.0f / mTwipsToPixels;

  ::ReleaseDC(NULL, hdc);

  mFontCache = nsnull;

  mDevUnitsToAppUnits = 1.0f;
  mAppUnitsToDevUnits = 1.0f;

  mGammaValue = 1.0f;
  mGammaTable = new PRUint8[256];

  mZoom = 1.0f;

  mSurface = NULL;
}

nsDeviceContextWin :: ~nsDeviceContextWin()
{
  NS_IF_RELEASE(mFontCache);

  if (NULL != mSurface)
  {
    DeleteDC(mSurface);
    mSurface = NULL;
  }

  if (nsnull != mGammaTable)
  {
    delete mGammaTable;
    mGammaTable = nsnull;
  }
}

NS_IMPL_QUERY_INTERFACE(nsDeviceContextWin, kDeviceContextIID)
NS_IMPL_ADDREF(nsDeviceContextWin)
NS_IMPL_RELEASE(nsDeviceContextWin)

nsresult nsDeviceContextWin :: Init()
{
  for (PRInt32 cnt = 0; cnt < 256; cnt++)
    mGammaTable[cnt] = cnt;

  return NS_OK;
}

float nsDeviceContextWin :: GetTwipsToDevUnits() const
{
  return mTwipsToPixels;
}

float nsDeviceContextWin :: GetDevUnitsToTwips() const
{
  return mPixelsToTwips;
}

void nsDeviceContextWin :: SetAppUnitsToDevUnits(float aAppUnits)
{
  mAppUnitsToDevUnits = aAppUnits;
}

void nsDeviceContextWin :: SetDevUnitsToAppUnits(float aDevUnits)
{
  mDevUnitsToAppUnits = aDevUnits;
}

float nsDeviceContextWin :: GetAppUnitsToDevUnits() const
{
  return mAppUnitsToDevUnits;
}

float nsDeviceContextWin :: GetDevUnitsToAppUnits() const
{
  return mDevUnitsToAppUnits;
}

float nsDeviceContextWin :: GetScrollBarWidth() const
{
  return ::GetSystemMetrics(SM_CXVSCROLL) * mDevUnitsToAppUnits;
}

float nsDeviceContextWin :: GetScrollBarHeight() const
{
  return ::GetSystemMetrics(SM_CXHSCROLL) * mDevUnitsToAppUnits;
}

nsIRenderingContext * nsDeviceContextWin :: CreateRenderingContext(nsIView *aView)
{
  nsIRenderingContext *pContext = nsnull;
  nsIWidget           *win = aView->GetWidget();
  nsresult            rv;

  static NS_DEFINE_IID(kRCCID, NS_RENDERING_CONTEXT_CID);
  static NS_DEFINE_IID(kRCIID, NS_IRENDERING_CONTEXT_IID);

  rv = NSRepository::CreateInstance(kRCCID, nsnull, kRCIID, (void **)&pContext);

  if (NS_OK == rv)
    InitRenderingContext(pContext, win);

  NS_IF_RELEASE(win);
  return pContext;
}

void nsDeviceContextWin :: InitRenderingContext(nsIRenderingContext *aContext, nsIWidget *aWin)
{
  aContext->Init(this, aWin);
}

nsIFontCache* nsDeviceContextWin::GetFontCache()
{
  if (nsnull == mFontCache) {
    if (NS_OK != CreateFontCache()) {
      return nsnull;
    }
  }
  NS_ADDREF(mFontCache);
  return mFontCache;
}

void nsDeviceContextWin::FlushFontCache()
{
  NS_RELEASE(mFontCache);
}

nsresult nsDeviceContextWin::CreateFontCache()
{
  nsresult rv = NS_NewFontCache(&mFontCache);
  if (NS_OK != rv) {
    return rv;
  }
  mFontCache->Init(this);
  return NS_OK;
}

nsIFontMetrics* nsDeviceContextWin::GetMetricsFor(const nsFont& aFont)
{
  if (nsnull == mFontCache) {
    if (NS_OK != CreateFontCache()) {
      return nsnull;
    }
  }
  return mFontCache->GetMetricsFor(aFont);
}

void nsDeviceContextWin :: SetZoom(float aZoom)
{
  mZoom = aZoom;
}

float nsDeviceContextWin :: GetZoom() const
{
  return mZoom;
}

nsDrawingSurface nsDeviceContextWin :: GetDrawingSurface(nsIRenderingContext &aContext)
{
  if (NULL == mSurface)
    mSurface = aContext.CreateDrawingSurface(nsnull);

  return mSurface;
}

float nsDeviceContextWin :: GetGamma(void)
{
  return mGammaValue;
}

void nsDeviceContextWin :: SetGamma(float aGamma)
{
  if (aGamma != mGammaValue)
  {
    //we don't need to-recorrect existing images for this case
    //so pass in 1.0 for the current gamma regardless of what it
    //really happens to be. existing images will get a one time
    //re-correction when they're rendered the next time. MMP

    SetGammaTable(mGammaTable, 1.0f, aGamma);

    mGammaValue = aGamma;
  }
}

PRUint8 * nsDeviceContextWin :: GetGammaTable(void)
{
  //XXX we really need to ref count this somehow. MMP

  return mGammaTable;
}

void nsDeviceContextWin :: SetGammaTable(PRUint8 * aTable, float aCurrentGamma, float aNewGamma)
{
  float fgval = (1.0f / aCurrentGamma) * (1.0f / aNewGamma);

  for (PRInt32 cnt = 0; cnt < 256; cnt++)
    aTable[cnt] = (PRUint8)(::pow(cnt * (1. / 256.), fgval) * 255.99999999);
}
