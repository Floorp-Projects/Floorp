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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Peter Hartshorn <peter@igelaus.com.au>
 *   Ken Faulkner <faulkner@igelaus.com.au>
 *   Tony Tsui <tony@igelaus.com.au>
 */

#include "nsRenderingContextXlib.h"
#include "nsDrawingSurfaceXlib.h"
#include "nsDeviceContextXlib.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "nsGfxCIID.h"
#include "nspr.h"

#include "nsFontMetricsXlib.h"

#include "xlibrgb.h"

#include "nsGfxPSCID.h"
#include "nsIDeviceContextPS.h"

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_IID(kDeviceContextIID, NS_IDEVICE_CONTEXT_IID);

static PRLogModuleInfo *DeviceContextXlibLM = PR_NewLogModule("DeviceContextXlib");

nsDeviceContextXlib::nsDeviceContextXlib()
  : DeviceContextImpl()
{
  PR_LOG(DeviceContextXlibLM, PR_LOG_DEBUG, ("nsDeviceContextXlib::nsDeviceContextXlib()\n"));
  NS_INIT_REFCNT();
  mTwipsToPixels = 1.0;
  mPixelsToTwips = 1.0;
  mPaletteInfo.isPaletteDevice = PR_FALSE;
  mPaletteInfo.sizePalette = 0;
  mPaletteInfo.numReserved = 0;
  mPaletteInfo.palette = NULL;
  mNumCells = 0;
  mSurface = nsnull;
  mDisplay = nsnull;
  mScreen = nsnull;
  mVisual = nsnull;
  mDepth = 0;

  mWidthFloat = 0.0f;
  mHeightFloat = 0.0f;
  mWidth = -1;
  mHeight = -1;
}

nsDeviceContextXlib::~nsDeviceContextXlib()
{
  nsDrawingSurfaceXlib *surf = (nsDrawingSurfaceXlib *)mSurface;
  NS_IF_RELEASE(surf);
  mSurface = nsnull;
}

NS_IMETHODIMP nsDeviceContextXlib::Init(nsNativeWidget aNativeWidget)
{
  PR_LOG(DeviceContextXlibLM, PR_LOG_DEBUG, ("nsDeviceContextXlib::Init()\n"));

  mWidget = aNativeWidget;

  mDisplay = xlib_rgb_get_display();
  mScreen = xlib_rgb_get_screen();
  mVisual = xlib_rgb_get_visual();
  mDepth = xlib_rgb_get_depth();


#ifdef DEBUG
  static PRBool once = PR_TRUE;

  if (once)
  {
    once = PR_FALSE;

    printf("nsDeviceContextXlib::Init(dpy=%p  screen=%p  visual=%p  depth=%d)\n",
           mDisplay,
           mScreen,
           mVisual,
           mDepth);
  }
#endif /* DEBUG */

  CommonInit();

  return NS_OK;
}

void
nsDeviceContextXlib::CommonInit(void)
{
  // FIXME: PeterH
  // This was set to 100 dpi, then later on in the function is was changed
  // to a default of 96dpi IF we had a preference component. We need to 
  // find a way to get the actual server dpi for a comparison ala GTK.
  static nscoord dpi = 96;
  static int initialized = 0;

  if (!initialized) {
    initialized = 1;
    nsresult res;
    NS_WITH_SERVICE(nsIPref,prefs,kPrefCID,&res);
    if (NS_SUCCEEDED(res) && prefs) {
      PRInt32 intVal = 96;
      res = prefs->GetIntPref("browser.display.screen_resolution", &intVal);
      if (NS_SUCCEEDED(res)) {
        if (intVal) {
          dpi = intVal;
        }
        else {
          // Compute dpi of display
          float screenWidth = float(WidthOfScreen(mScreen));
          float screenWidthIn = float(WidthMMOfScreen(mScreen)) / 25.4f;
          dpi = nscoord(screenWidth / screenWidthIn);
        }
      }
    }
  }

	// Do extra rounding (based on GTK). KenF
	mPixelsToTwips = float(NSToIntRound(float(NSIntPointsToTwips(72)) / float(dpi)));
  	mTwipsToPixels = 1.0f / mPixelsToTwips;

  PR_LOG(DeviceContextXlibLM, PR_LOG_DEBUG, ("GFX: dpi=%d t2p=%g p2t=%g\n", dpi, mTwipsToPixels, mPixelsToTwips));


  mWidthFloat = (float) WidthOfScreen(mScreen);
  mHeightFloat = (float) HeightOfScreen(mScreen);

  DeviceContextImpl::CommonInit();
}

NS_IMETHODIMP nsDeviceContextXlib::CreateRenderingContext(nsIRenderingContext *&aContext)
{
  PR_LOG(DeviceContextXlibLM, PR_LOG_DEBUG, ("nsDeviceContextXlib::CreateRenderingContext()\n"));

  nsIRenderingContext  *context = nsnull;
  nsDrawingSurfaceXlib *surface = nsnull;
  nsresult                  rv;

  context = new nsRenderingContextXlib();

  if (nsnull != context) {
    NS_ADDREF(context);
    surface = new nsDrawingSurfaceXlib();
    if (nsnull != surface) {

      GC gc = XCreateGC(mDisplay, 
                        (Drawable) mWidget, 
                        0, 
                        NULL);

      rv = surface->Init(mDisplay, 
                         mScreen, 
                         mVisual, 
                         mDepth,
                         (Drawable) mWidget, 
                         gc);

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
  PR_LOG(DeviceContextXlibLM, PR_LOG_DEBUG, ("nsDeviceContextXlib::SupportsNativeWidgets()\n"));
  aSupportsWidgets = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextXlib::GetScrollBarDimensions(float &aWidth, float &aHeight) const
{
  PR_LOG(DeviceContextXlibLM, PR_LOG_DEBUG, ("nsDeviceContextXlib::GetScrollBarDimensions()\n"));
  // XXX Oh, yeah.  These are hard coded.
  aWidth = 15 * mPixelsToTwips;
  aHeight = 15 * mPixelsToTwips;

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextXlib::GetSystemAttribute(nsSystemAttrID anID, SystemAttrStruct * aInfo) const
{
  PR_LOG(DeviceContextXlibLM, PR_LOG_DEBUG, ("nsDeviceContextXlib::GetSystemAttribute()\n"));
  nsresult status = NS_OK;

  switch (anID) {
    //---------
    // Colors
    //---------
    case eSystemAttr_Color_WindowBackground:
        *aInfo->mColor = NS_RGB(255,255,255);
        break;
    case eSystemAttr_Color_WindowForeground:
        *aInfo->mColor = NS_RGB(0,0,0);
        break;
    case eSystemAttr_Color_WidgetBackground:
      *aInfo->mColor = NS_RGB(255,255,255);
        break;
    case eSystemAttr_Color_WidgetForeground:
        *aInfo->mColor = NS_RGB(0,0,0);
        break;
    case eSystemAttr_Color_WidgetSelectBackground:
      *aInfo->mColor = NS_RGB(255,255,255);
        break;
    case eSystemAttr_Color_WidgetSelectForeground:
        *aInfo->mColor = NS_RGB(0,0,0);
        break;
    case eSystemAttr_Color_Widget3DHighlight:
        *aInfo->mColor = NS_RGB(0xa0,0xa0,0xa0);
        break;
    case eSystemAttr_Color_Widget3DShadow:
        *aInfo->mColor = NS_RGB(0x40,0x40,0x40);
        break;
    case eSystemAttr_Color_TextBackground:
      *aInfo->mColor = NS_RGB(255,255,255);
        break;
    case eSystemAttr_Color_TextForeground: 
        *aInfo->mColor = NS_RGB(0,0,0);
        break;
    case eSystemAttr_Color_TextSelectBackground:
      *aInfo->mColor = NS_RGB(255,255,255);
        break;
    case eSystemAttr_Color_TextSelectForeground:
        *aInfo->mColor = NS_RGB(0,0,0);
        break;
    //---------
    // Size
    //---------
    case eSystemAttr_Size_ScrollbarHeight:
        aInfo->mSize = 15;
        break;
    case eSystemAttr_Size_ScrollbarWidth: 
        aInfo->mSize = 15;
        break;
    case eSystemAttr_Size_WindowTitleHeight:
        aInfo->mSize = 0;
        break;
    case eSystemAttr_Size_WindowBorderWidth:
      //      aInfo->mSize = style->klass->xthickness;
      aInfo->mSize = 1;
        break;
    case eSystemAttr_Size_WindowBorderHeight:
      //        aInfo->mSize = style->klass->ythickness;
        aInfo->mSize = 1;
        break;
    case eSystemAttr_Size_Widget3DBorder:
        aInfo->mSize = 4;
        break;
    //---------
    // Fonts
    //---------
    case eSystemAttr_Font_Caption:
    case eSystemAttr_Font_Icon:
    case eSystemAttr_Font_Menu:
    case eSystemAttr_Font_MessageBox:
    case eSystemAttr_Font_SmallCaption:
    case eSystemAttr_Font_StatusBar:
    case eSystemAttr_Font_Tooltips:
    case eSystemAttr_Font_Widget:
      status = NS_ERROR_FAILURE;
      break;
  } // switch

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextXlib::GetDrawingSurface(nsIRenderingContext &aContext, nsDrawingSurface &aSurface)
{
  PR_LOG(DeviceContextXlibLM, PR_LOG_DEBUG, ("nsDeviceContextXlib::GetDrawingSurface()\n"));
  if (NULL == mSurface) {
    aContext.CreateDrawingSurface(nsnull, 0, mSurface);
  }
  aSurface = mSurface;
  return NS_OK;
#if 0
  aContext.CreateDrawingSurface(nsnull, 0, aSurface);
  return nsnull == aSurface ? NS_ERROR_OUT_OF_MEMORY : NS_OK;
#endif
}

NS_IMETHODIMP nsDeviceContextXlib::ConvertPixel(nscolor aColor, PRUint32 & aPixel)
{
  PR_LOG(DeviceContextXlibLM, PR_LOG_DEBUG, ("nsDeviceContextXlib::ConvertPixel()\n"));
  aPixel = xlib_rgb_xpixel_from_rgb(NS_RGB(NS_GET_B(aPixel),
                                           NS_GET_G(aPixel),
                                           NS_GET_R(aPixel)));
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextXlib::CheckFontExistence(const nsString& aFontName)
{
  return nsFontMetricsXlib::FamilyExists(aFontName);
}

NS_IMETHODIMP nsDeviceContextXlib::GetDeviceSurfaceDimensions(PRInt32 &aWidth, PRInt32 &aHeight)
{
  if (mWidth == -1)
    mWidth = NSToIntRound(mWidthFloat * mDevUnitsToAppUnits);

  if (mHeight == -1)
    mHeight = NSToIntRound(mHeightFloat * mDevUnitsToAppUnits);

  aWidth = mWidth;
  aHeight = mHeight;

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextXlib::GetRect(nsRect &aRect)
{
  PRInt32 width, height;
  nsresult rv;
  rv = GetDeviceSurfaceDimensions(width, height);
  aRect.x = 0;
  aRect.y = 0;
  aRect.width = width;
  aRect.height = height;
  return rv;
}

NS_IMETHODIMP nsDeviceContextXlib::GetClientRect(nsRect &aRect)
{
  PRInt32 width, height;
  nsresult rv;
  rv = GetDeviceSurfaceDimensions(width, height);
  aRect.x = 0;
  aRect.y = 0;
  aRect.width = width;
  aRect.height = height;
  return rv;
}

NS_IMETHODIMP nsDeviceContextXlib::GetDeviceContextFor(nsIDeviceContextSpec *aDevice,
                                                        nsIDeviceContext *&aContext)
{
  static NS_DEFINE_CID(kCDeviceContextPS, NS_DEVICECONTEXTPS_CID);
  
  // Create a Postscript device context 
  nsresult rv;
  nsIDeviceContextPS *dcps;
  
  rv = nsComponentManager::CreateInstance(kCDeviceContextPS,
                                          nsnull,
                                          NS_GET_IID(nsIDeviceContextPS),
                                          (void **)&dcps);

  NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't create PS Device context");
  
  dcps->SetSpec(aDevice);
  dcps->InitDeviceContextPS((nsIDeviceContext*)aContext,
                            (nsIDeviceContext*)this);

  rv = dcps->QueryInterface(NS_GET_IID(nsIDeviceContext),
                            (void **)&aContext);

  NS_RELEASE(dcps);
  
  return rv;
}

NS_IMETHODIMP nsDeviceContextXlib::BeginDocument(void)
{
  PR_LOG(DeviceContextXlibLM, PR_LOG_DEBUG, ("nsDeviceContextXlib::BeginDocument()\n"));
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextXlib::EndDocument(void)
{
  PR_LOG(DeviceContextXlibLM, PR_LOG_DEBUG, ("nsDeviceContextXlib::EndDocument()\n"));
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextXlib::BeginPage(void)
{
  PR_LOG(DeviceContextXlibLM, PR_LOG_DEBUG, ("nsDeviceContextXlib::BeginPage()\n"));
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextXlib::EndPage(void)
{
  PR_LOG(DeviceContextXlibLM, PR_LOG_DEBUG, ("nsDeviceContextXlib::EndPage()\n"));
  return NS_OK;
}
