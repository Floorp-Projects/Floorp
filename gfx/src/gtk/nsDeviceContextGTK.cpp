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

#include "nsDeviceContextGTK.h"
#include "nsRenderingContextGTK.h"
#include "../nsGfxCIID.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kDeviceContextIID, NS_IDEVICE_CONTEXT_IID);

nsDeviceContextGTK :: nsDeviceContextGTK()
{
  NS_INIT_REFCNT();

  mFontCache = nsnull;
  mSurface = nsnull;
}

nsDeviceContextGTK :: ~nsDeviceContextGTK()
{
  NS_IF_RELEASE(mFontCache);

  if (mSurface) delete mSurface;
}

NS_IMPL_QUERY_INTERFACE(nsDeviceContextGTK, kDeviceContextIID)
NS_IMPL_ADDREF(nsDeviceContextGTK)
NS_IMPL_RELEASE(nsDeviceContextGTK)

nsresult nsDeviceContextGTK :: Init()
{
  return NS_OK;
}

float nsDeviceContextGTK :: GetTwipsToDevUnits() const
{
  return 0.0;
}

float nsDeviceContextGTK :: GetDevUnitsToTwips() const
{
  return 0.0;
}

void nsDeviceContextGTK :: SetAppUnitsToDevUnits(float aAppUnits)
{
}

void nsDeviceContextGTK :: SetDevUnitsToAppUnits(float aDevUnits)
{
}

float nsDeviceContextGTK :: GetAppUnitsToDevUnits() const
{
  return 0.0;
}

float nsDeviceContextGTK :: GetDevUnitsToAppUnits() const
{
  return 0.0;
}

float nsDeviceContextGTK :: GetScrollBarWidth() const
{
  return 0.0;
}

float nsDeviceContextGTK :: GetScrollBarHeight() const
{
  return 0.0;
}

nsIRenderingContext * nsDeviceContextGTK :: CreateRenderingContext(nsIView *aView)
{
  nsIRenderingContext *pContext = nsnull;
  nsIWidget *win = aView->GetWidget();
  nsresult            rv;

  mSurface = new nsDrawingSurfaceGTK();

  mSurface->drawable = (GdkDrawable *)win->GetNativeData(NS_NATIVE_WINDOW);
  mSurface->gc       = (GdkGC *)win->GetNativeData(NS_NATIVE_GRAPHIC);

  static NS_DEFINE_IID(kRCCID, NS_RENDERING_CONTEXT_CID);
  static NS_DEFINE_IID(kRCIID, NS_IRENDERING_CONTEXT_IID);

  rv = NSRepository::CreateInstance(kRCCID, nsnull, kRCIID, (void **)&pContext);

  if (NS_OK == rv)
    InitRenderingContext(pContext, win);

  return pContext;
}

void nsDeviceContextGTK :: InitRenderingContext(nsIRenderingContext *aContext, nsIWidget *aWin)
{
  aContext->Init(this, aWin);
}

nsIFontCache* nsDeviceContextGTK::GetFontCache()
{
  if (nsnull == mFontCache) {
    if (NS_OK != CreateFontCache()) {
      return nsnull;
    }
  }
  NS_ADDREF(mFontCache);
  return mFontCache;
}

nsresult nsDeviceContextGTK::CreateFontCache()
{
#if 0
  nsresult rv = NS_NewFontCache(&mFontCache);
  if (NS_OK != rv) {
    return rv;
  }
  mFontCache->Init(this);
#endif
  return NS_OK;
}

void nsDeviceContextGTK::FlushFontCache()
{
}


nsIFontMetrics* nsDeviceContextGTK::GetMetricsFor(const nsFont& aFont)
{
  return nsnull;
}

void nsDeviceContextGTK :: SetZoom(float aZoom)
{
}

float nsDeviceContextGTK :: GetZoom() const
{
  return 0.0;
}

nsDrawingSurface nsDeviceContextGTK :: GetDrawingSurface(nsIRenderingContext &aContext)
{
  return ( aContext.CreateDrawingSurface(nsnull));
}

float nsDeviceContextGTK :: GetGamma(void)
{
  return mGammaValue;
}

void nsDeviceContextGTK :: SetGamma(float aGamma)
{
  if (aGamma != mGammaValue)
  {
    //we don't need to-recorrect existing images for this case
    //so pass in 1.0 for the current gamma regardless of what it
    //really happens to be. existing images will get a one time
    //re-correction when they're rendered the next time. MMP

    mGammaValue = aGamma;
  }
}

PRUint8 * nsDeviceContextGTK :: GetGammaTable(void)
{
  //XXX we really need to ref count this somehow. MMP

  return nsnull;
}




