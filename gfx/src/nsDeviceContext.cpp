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

#include "nsDeviceContext.h"
#include "nsIFontCache.h"
#include "nsIView.h"
#include "nsGfxCIID.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kDeviceContextIID, NS_IDEVICE_CONTEXT_IID);

DeviceContextImpl :: DeviceContextImpl()
{
  NS_INIT_REFCNT();
  mFontCache = nsnull;
  mDevUnitsToAppUnits = 1.0f;
  mAppUnitsToDevUnits = 1.0f;
  mGammaValue = 1.0f;
  mGammaTable = new PRUint8[256];
  mZoom = 1.0f;
}

DeviceContextImpl :: ~DeviceContextImpl()
{
  NS_IF_RELEASE(mFontCache);

  if (nsnull != mGammaTable)
  {
    delete mGammaTable;
    mGammaTable = nsnull;
  }
}

NS_IMPL_QUERY_INTERFACE(DeviceContextImpl, kDeviceContextIID)
NS_IMPL_ADDREF(DeviceContextImpl)
NS_IMPL_RELEASE(DeviceContextImpl)

nsresult DeviceContextImpl :: Init(nsNativeWidget aWidget)
{
  for (PRInt32 cnt = 0; cnt < 256; cnt++)
    mGammaTable[cnt] = cnt;

  mWidget = aWidget;

  return NS_OK;
}

float DeviceContextImpl :: GetTwipsToDevUnits() const
{
  return mTwipsToPixels;
}

float DeviceContextImpl :: GetDevUnitsToTwips() const
{
  return mPixelsToTwips;
}

void DeviceContextImpl :: SetAppUnitsToDevUnits(float aAppUnits)
{
  mAppUnitsToDevUnits = aAppUnits;
}

void DeviceContextImpl :: SetDevUnitsToAppUnits(float aDevUnits)
{
  mDevUnitsToAppUnits = aDevUnits;
}

float DeviceContextImpl :: GetAppUnitsToDevUnits() const
{
  return mAppUnitsToDevUnits;
}

float DeviceContextImpl :: GetDevUnitsToAppUnits() const
{
  return mDevUnitsToAppUnits;
}

nsIRenderingContext * DeviceContextImpl :: CreateRenderingContext(nsIView *aView)
{
  nsIRenderingContext *pContext = nsnull;
  nsIWidget           *win = aView->GetWidget();
  nsresult            rv;

  static NS_DEFINE_IID(kRCCID, NS_RENDERING_CONTEXT_CID);
  static NS_DEFINE_IID(kRCIID, NS_IRENDERING_CONTEXT_IID);

  rv = NSRepository::CreateInstance(kRCCID, nsnull, kRCIID, (void **)&pContext);

  if (NS_OK == rv) {
    rv = InitRenderingContext(pContext, win);
    if (NS_OK != rv) {
      NS_RELEASE(pContext);
    }
  }

  NS_IF_RELEASE(win);
  return pContext;
}

nsresult DeviceContextImpl :: InitRenderingContext(nsIRenderingContext *aContext, nsIWidget *aWin)
{
  return (aContext->Init(this, aWin));
}

nsIFontCache* DeviceContextImpl::GetFontCache()
{
  if (nsnull == mFontCache) {
    if (NS_OK != CreateFontCache()) {
      return nsnull;
    }
  }
  NS_ADDREF(mFontCache);
  return mFontCache;
}

void DeviceContextImpl::FlushFontCache()
{
  NS_IF_RELEASE(mFontCache);
}

nsresult DeviceContextImpl::CreateFontCache()
{
  nsresult rv = NS_NewFontCache(&mFontCache);
  if (NS_OK != rv) {
    return rv;
  }
  mFontCache->Init(this);
  return NS_OK;
}

nsIFontMetrics* DeviceContextImpl::GetMetricsFor(const nsFont& aFont)
{
  if (nsnull == mFontCache) {
    if (NS_OK != CreateFontCache()) {
      return nsnull;
    }
  }
  return mFontCache->GetMetricsFor(aFont);
}

void DeviceContextImpl :: SetZoom(float aZoom)
{
  mZoom = aZoom;
}

float DeviceContextImpl :: GetZoom() const
{
  return mZoom;
}

float DeviceContextImpl :: GetGamma(void)
{
  return mGammaValue;
}

void DeviceContextImpl :: SetGamma(float aGamma)
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

PRUint8 * DeviceContextImpl :: GetGammaTable(void)
{
  //XXX we really need to ref count this somehow. MMP

  return mGammaTable;
}

void DeviceContextImpl :: SetGammaTable(PRUint8 * aTable, float aCurrentGamma, float aNewGamma)
{
  double fgval = (1.0f / aCurrentGamma) * (1.0f / aNewGamma);

  for (PRInt32 cnt = 0; cnt < 256; cnt++)
    aTable[cnt] = (PRUint8)(pow((double)cnt * (1. / 256.), fgval) * 255.99999999);
}

nsNativeWidget DeviceContextImpl :: GetNativeWidget(void)
{
  return mWidget;
}

