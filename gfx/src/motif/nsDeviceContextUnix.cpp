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

#include "nsDeviceContextUnix.h"
#include "nsRenderingContextUnix.h"
#include "../nsGfxCIID.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kDeviceContextIID, NS_IDEVICE_CONTEXT_IID);

nsDeviceContextUnix :: nsDeviceContextUnix()
{
  NS_INIT_REFCNT();

  mFontCache = nsnull;
}

nsDeviceContextUnix :: ~nsDeviceContextUnix()
{
  NS_IF_RELEASE(mFontCache);
}

NS_IMPL_QUERY_INTERFACE(nsDeviceContextUnix, kDeviceContextIID)
NS_IMPL_ADDREF(nsDeviceContextUnix)
NS_IMPL_RELEASE(nsDeviceContextUnix)

nsresult nsDeviceContextUnix :: Init()
{
  return NS_OK;
}

float nsDeviceContextUnix :: GetTwipsToDevUnits() const
{
  return 0.0;
}

float nsDeviceContextUnix :: GetDevUnitsToTwips() const
{
  return 0.0;
}

void nsDeviceContextUnix :: SetAppUnitsToDevUnits(float aAppUnits)
{
}

void nsDeviceContextUnix :: SetDevUnitsToAppUnits(float aDevUnits)
{
}

float nsDeviceContextUnix :: GetAppUnitsToDevUnits() const
{
  return 0.0;
}

float nsDeviceContextUnix :: GetDevUnitsToAppUnits() const
{
  return 0.0;
}

float nsDeviceContextUnix :: GetScrollBarWidth() const
{
  return 0.0;
}

float nsDeviceContextUnix :: GetScrollBarHeight() const
{
  return 0.0;
}

nsIRenderingContext * nsDeviceContextUnix :: CreateRenderingContext(nsIView *aView)
{
  nsIRenderingContext *pContext = nsnull;
  nsresult            rv;

  static NS_DEFINE_IID(kRCCID, NS_RENDERING_CONTEXT_CID);
  static NS_DEFINE_IID(kRCIID, NS_IRENDERING_CONTEXT_IID);

  rv = NSRepository::CreateInstance(kRCCID, nsnull, kRCIID, (void **)&pContext);

  if (NS_OK == rv)
    InitRenderingContext(pContext, mWidget);

  return pContext;
}

void nsDeviceContextUnix :: InitRenderingContext(nsIRenderingContext *aContext, nsIWidget *aWin)
{
  aContext->Init(this, aWin);
  mWidget = aWin;
}

nsIFontCache* nsDeviceContextUnix::GetFontCache()
{
  if (nsnull == mFontCache) {
    if (NS_OK != CreateFontCache()) {
      return nsnull;
    }
  }
  NS_ADDREF(mFontCache);
  return mFontCache;
}

nsresult nsDeviceContextUnix::CreateFontCache()
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

void nsDeviceContextUnix::FlushFontCache()
{
}


nsIFontMetrics* nsDeviceContextUnix::GetMetricsFor(const nsFont& aFont)
{
  return nsnull;
}

void nsDeviceContextUnix :: SetZoom(float aZoom)
{
}

float nsDeviceContextUnix :: GetZoom() const
{
  return 0.0;
}

nsDrawingSurface nsDeviceContextUnix :: GetDrawingSurface(nsIRenderingContext &aContext)
{
  return ( aContext.CreateDrawingSurface(nsnull));
}
