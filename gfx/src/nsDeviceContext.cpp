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
#include "nsImageNet.h"
#include "nsImageRequest.h"
#include "nsIImageGroup.h"
#include "il_util.h"

static NS_DEFINE_IID(kDeviceContextIID, NS_IDEVICE_CONTEXT_IID);

NS_IMPL_QUERY_INTERFACE(DeviceContextImpl, kDeviceContextIID)
NS_IMPL_ADDREF(DeviceContextImpl)
NS_IMPL_RELEASE(DeviceContextImpl)

DeviceContextImpl :: DeviceContextImpl()
{
  NS_INIT_REFCNT();
  mFontCache = nsnull;
  mDevUnitsToAppUnits = 1.0f;
  mAppUnitsToDevUnits = 1.0f;
  mGammaValue = 1.0f;
  mGammaTable = new PRUint8[256];
  mZoom = 1.0f;
  mWidget = nsnull;
  mIconImageGroup = nsnull;
  for (PRInt32 i = 0; i < NS_NUMBER_OF_ICONS; i++) {
    mIcons[i] = nsnull;
  }
}

DeviceContextImpl :: ~DeviceContextImpl()
{
  NS_IF_RELEASE(mFontCache);

  if (nsnull != mGammaTable)
  {
    delete mGammaTable;
    mGammaTable = nsnull;
  }

  IL_DestroyGroupContext(mIconImageGroup);

  for (PRInt32 i = 0; i < NS_NUMBER_OF_ICONS; i++) {
    NS_IF_RELEASE(mIcons[i]);
  }
}

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

nsresult DeviceContextImpl::CreateIconILGroupContext()
{
  ilIImageRenderer* renderer;
  nsresult          result;
   
  // Create an image renderer
  result = NS_NewImageRenderer(&renderer);
  if (NS_FAILED(result)) {
    return result;
  }

  // Create an image group context. The image renderer code expects the
  // display_context to be a pointer to a device context
  mIconImageGroup = IL_NewGroupContext((void*)this, renderer);
  if (nsnull == mIconImageGroup) {
    NS_RELEASE(renderer);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Initialize the image group context.
  IL_ColorSpace* colorSpace;
  result = CreateILColorSpace(colorSpace);
  if (NS_FAILED(result)) {
    NS_RELEASE(renderer);
    IL_DestroyGroupContext(mIconImageGroup);
    return result;
  }

  // Icon loading is synchronous, so don't waste time with progressive display
  IL_DisplayData displayData;
  displayData.dither_mode = IL_Auto;
  displayData.color_space = colorSpace;
  displayData.progressive_display = PR_FALSE;
  IL_SetDisplayMode(mIconImageGroup, IL_COLOR_SPACE | IL_DITHER_MODE, &displayData);
  IL_ReleaseColorSpace(colorSpace);

  return NS_OK;
}

NS_IMETHODIMP DeviceContextImpl::LoadIconImage(PRInt32 aId, nsIImage*& aImage)
{
  nsresult  result;

  // Initialize out parameter
  aImage = nsnull;

  // Make sure the icon number is valid
  if ((aId < 0) || (aId >= NS_NUMBER_OF_ICONS)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  // See if the icon is already loaded
  if (nsnull != mIcons[aId]) {
    aImage = mIcons[aId]->GetImage();
    return NS_OK;
  }

  // Make sure we have an image group context
  if (nsnull == mIconImageGroup) {
    result = CreateIconILGroupContext();
    if (NS_FAILED(result)) {
      return result;
    }
  }

  // Build the URL string
  char  url[128];
  sprintf(url, "resource://res/gfx/icon_%d.gif", aId);

  // Use a sync net context
  ilINetContext* netContext;
  result = NS_NewImageNetContextSync(&netContext);
  if (NS_FAILED(result)) {
    return result;
  }

  // Create an image request object which will do the actual load
  ImageRequestImpl* imageReq = new ImageRequestImpl();
  if (nsnull == imageReq) {
    result = NS_ERROR_OUT_OF_MEMORY;

  } else {
    // Load the image
    result = imageReq->Init(mIconImageGroup, url, nsnull, nsnull, 0, 0,
                            nsImageLoadFlags_kHighPriority, netContext);
    aImage = imageReq->GetImage();

    // Keep the image request object around and avoid reloading the image
    NS_ADDREF(imageReq);
    mIcons[aId] = imageReq;
  }
  
  netContext->Release();
  return result;
}

NS_IMETHODIMP DeviceContextImpl::CreateILColorSpace(IL_ColorSpace*& aColorSpace)
{
  IL_RGBBits colorRGBBits;

  // Default is to create a 24-bit color space
  colorRGBBits.red_shift = 16;  
  colorRGBBits.red_bits = 8;
  colorRGBBits.green_shift = 8;
  colorRGBBits.green_bits = 8; 
  colorRGBBits.blue_shift = 0; 
  colorRGBBits.blue_bits = 8;  

  aColorSpace = IL_CreateTrueColorSpace(&colorRGBBits, 24);
  if (nsnull == aColorSpace) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

