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

#include "nsRenderingContextXlib.h"
#include "nsDrawingSurfaceXlib.h"
#include "nsDeviceContextXlib.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "nsGfxCIID.h"

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_IID(kIPrefIID, NS_IPREF_IID);
static NS_DEFINE_IID(kDeviceContextIID, NS_IDEVICE_CONTEXT_IID);

nsDeviceContextXlib::nsDeviceContextXlib()
  : DeviceContextImpl()
{
  printf("nsDeviceContextXlib::nsDeviceContextXlib()\n");
  NS_INIT_REFCNT();
  mTwipsToPixels = 1.0;
  mPixelsToTwips = 1.0;
  mPaletteInfo.isPaletteDevice = PR_FALSE;
  mPaletteInfo.sizePalette = 0;
  mPaletteInfo.numReserved = 0;
  mPaletteInfo.palette = NULL;
  mNumCells = 0;
}

nsDeviceContextXlib::~nsDeviceContextXlib()
{
}

NS_IMPL_QUERY_INTERFACE(nsDeviceContextXlib, kDeviceContextIID)
NS_IMPL_ADDREF(nsDeviceContextXlib)
NS_IMPL_RELEASE(nsDeviceContextXlib)

NS_IMETHODIMP nsDeviceContextXlib::Init(nsNativeWidget aNativeWidget)
{
  printf("nsDeviceContextXlib::Init()\n");

  mWidget = aNativeWidget;

  static nscoord dpi = 96;
  static int initialized = 0;

  if (!initialized) {
    initialized = 1;
    nsIPref* prefs = nsnull;
    nsresult res = nsServiceManager::GetService(kPrefCID, kIPrefIID,
                                                (nsISupports**) &prefs);
    if (NS_SUCCEEDED(res) && prefs) {
      PRInt32 intVal = 96;
      res = prefs->GetIntPref("browser.screen_resolution", &intVal);
      if (NS_SUCCEEDED(res)) {
        if (intVal) {
          dpi = intVal;
        }
        else {
          // Compute dpi of display
          float screenWidth = float(WidthOfScreen(gScreen));
          float screenWidthIn = float(WidthMMOfScreen(gScreen)) / 25.4f;
          dpi = nscoord(screenWidth / screenWidthIn);
        }
      }
      nsServiceManager::ReleaseService(kPrefCID, prefs);
    }
  }

  mTwipsToPixels = float(dpi) / float(NSIntPointsToTwips(72));
  mPixelsToTwips = 1.0f / mTwipsToPixels;

  CommonInit();

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextXlib::CreateRenderingContext(nsIRenderingContext *&aContext)
{
  printf("nsDeviceContextXlib::CreateRenderingContext()\n");

  nsIRenderingContext  *context = nsnull;
  nsDrawingSurfaceXlib *surface = nsnull;
  nsresult                  rv;
  Window                    win = (Window)aContext;
  // XXX umm...this isn't done.
  context = new nsRenderingContextXlib();

  if (nsnull != context) {
    NS_ADDREF(context);
    surface = new nsDrawingSurfaceXlib();
    if (nsnull != surface) {
      GC gc = XCreateGC(gDisplay, win, 0, NULL);
      rv = surface->Init((Drawable)win, gc);
      if (NS_OK == rv) {
        rv = context->Init(this, surface);
      }
    }
    else {
      rv = NS_ERROR_OUT_OF_MEMORY;
    }
  }
  else {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }
  
  if (NS_OK != rv) {
    NS_IF_RELEASE(context);
  }
  aContext = context;
  
  return rv;
}

NS_IMETHODIMP nsDeviceContextXlib::SupportsNativeWidgets(PRBool &aSupportsWidgets)
{
  printf("nsDeviceContextXlib::SupportsNativeWidgets()\n");
  aSupportsWidgets = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextXlib::GetScrollBarDimensions(float &aWidth, float &aHeight) const
{
  printf("nsDeviceContextXlib::GetScrollBarDimensions()\n");
  // XXX Oh, yeah.  These are hard coded.
  aWidth = 5 * mPixelsToTwips;
  aHeight = 5 * mPixelsToTwips;

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextXlib::GetSystemAttribute(nsSystemAttrID anID, SystemAttrStruct * aInfo) const
{
  printf("nsDeviceContextXlib::GetSystemAttribute()\n");
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextXlib::GetDrawingSurface(nsIRenderingContext &aContext, nsDrawingSurface &aSurface)
{
  printf("nsDeviceContextXlib::GetDrawingSurface()\n");
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextXlib::ConvertPixel(nscolor aColor, PRUint32 & aPixel)
{
  printf("nsDeviceContextXlib::ConvertPixel()\n");
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextXlib::CheckFontExistence(const nsString& aFontName)
{
  printf("nsDeviceContextXlib::CheckFontExistence()\n");
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextXlib::GetDeviceSurfaceDimensions(PRInt32 &aWidth, PRInt32 &aHeight)
{
  printf("nsDeviceContextXlib::GetDeviceSurfaceDimensions()\n");
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextXlib::GetDeviceContextFor(nsIDeviceContextSpec *aDevice,
                                                        nsIDeviceContext *&aContext)
{
  printf("nsDeviceContextXlib::GetDeviceContextFor()\n");
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextXlib::BeginDocument(void)
{
  printf("nsDeviceContextXlib::BeginDocument()\n");
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextXlib::EndDocument(void)
{
  printf("nsDeviceContextXlib::EndDocument()\n");
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextXlib::BeginPage(void)
{
  printf("nsDeviceContextXlib::BeginPage()\n");
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextXlib::EndPage(void)
{
  printf("nsDeviceContextXlib::EndPage()\n");
  return NS_OK;
}
