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
#include "nspr.h"
#include "xlibrgb.h"

#include "nsGfxPSCID.h"
#include "nsIDeviceContextPS.h"

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_IID(kIPrefIID, NS_IPREF_IID);
static NS_DEFINE_IID(kDeviceContextIID, NS_IDEVICE_CONTEXT_IID);

static PRLogModuleInfo *DeviceContextXlibLM = PR_NewLogModule("DeviceContextXlib");

static PRUint8 _ConvertMaskToCount(unsigned long val);
static PRUint8 _GetShiftForMask(unsigned long val);
static void _EvilInitilizeGlobals();

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
}

nsDeviceContextXlib::~nsDeviceContextXlib()
{
  nsDrawingSurfaceXlib *surf = (nsDrawingSurfaceXlib *)mSurface;
  NS_IF_RELEASE(surf);
  mSurface = nsnull;
}

// EVIL
extern Display *  gDisplay;
extern Screen *   gScreen;
extern Visual *   gVisual;
extern int        gDepth;
extern XVisualInfo     *gVisualInfo;

extern PRUint32  gRedZeroMask;     //red color mask in zero position
extern PRUint32  gGreenZeroMask;   //green color mask in zero position
extern PRUint32  gBlueZeroMask;    //blue color mask in zero position
extern PRUint32  gAlphaZeroMask;   //alpha data mask in zero position
extern PRUint32  gRedMask;         //red color mask
extern PRUint32  gGreenMask;       //green color mask
extern PRUint32  gBlueMask;        //blue color mask
extern PRUint32  gAlphaMask;       //alpha data mask
extern PRUint8   gRedCount;        //number of red color bits
extern PRUint8   gGreenCount;      //number of green color bits
extern PRUint8   gBlueCount;       //number of blue color bits
extern PRUint8   gAlphaCount;      //number of alpha data bits
extern PRUint8   gRedShift;        //number to shift value into red position
extern PRUint8   gGreenShift;      //number to shift value into green position
extern PRUint8   gBlueShift;       //number to shift value into blue position
extern PRUint8   gAlphaShift;      //number to shift value into alpha position


NS_IMETHODIMP nsDeviceContextXlib::Init(nsNativeWidget aNativeWidget)
{
  PR_LOG(DeviceContextXlibLM, PR_LOG_DEBUG, ("nsDeviceContextXlib::Init()\n"));

  mWidget = aNativeWidget;

  // This is EVIL and has to be fixed by inventing an interface for it.
  mDisplay = gDisplay;
  mScreen = gScreen;
  mVisual = gVisual;
  mDepth = gDepth;

  static PRBool firstTime = TRUE;

  if (firstTime)
  {
    firstTime = PR_FALSE;

    _EvilInitilizeGlobals();

    printf("nsDeviceContextXlib::Init(dpy=%p  screen=%p  visual=%p  depth=%d)\n",
           mDisplay,
           mScreen,
           mVisual,
           mDepth);
  }


  CommonInit();

  return NS_OK;
}

void
nsDeviceContextXlib::CommonInit(void)
{
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
          float screenWidth = float(WidthOfScreen(mScreen));
          float screenWidthIn = float(WidthMMOfScreen(mScreen)) / 25.4f;
          dpi = nscoord(screenWidth / screenWidthIn);
        }
      }
      nsServiceManager::ReleaseService(kPrefCID, prefs);
    }
  }

  mTwipsToPixels = float(dpi) / float(NSIntPointsToTwips(72));
  mPixelsToTwips = 1.0f / mTwipsToPixels;

  PR_LOG(DeviceContextXlibLM, PR_LOG_DEBUG, ("GFX: dpi=%d t2p=%g p2t=%g\n", dpi, mTwipsToPixels, mPixelsToTwips));

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
  PR_LOG(DeviceContextXlibLM, PR_LOG_DEBUG, ("nsDeviceContextXlib::CheckFontExistence()\n"));
  char        **fnames = nsnull;
  PRInt32     namelen = aFontName.Length() + 1;
  char        *wildstring = (char *)PR_Malloc(namelen + 200);
  float       t2d;
  GetTwipsToDevUnits(t2d);
  PRInt32     dpi = NSToIntRound(t2d * 1440);
  int         numnames = 0;
  XFontStruct *fonts;
  nsresult    rv = NS_ERROR_FAILURE;
  
  if (nsnull == wildstring)
    return NS_ERROR_UNEXPECTED;
  
  if (abs(dpi - 75) < abs(dpi - 100))
    dpi = 75;
  else
    dpi = 100;
  
  char* fontName = aFontName.ToNewCString();
  PR_snprintf(wildstring, namelen + 200,
              "*-%s-*-*-normal--*-*-%d-%d-*-*-*",
              fontName, dpi, dpi);
  delete [] fontName;
  
  fnames = ::XListFontsWithInfo(mDisplay, wildstring, 1, &numnames, &fonts);
  
  if (numnames > 0)
  {
    ::XFreeFontInfo(fnames, fonts, numnames);
    rv = NS_OK;
  }
  
  PR_Free(wildstring);
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextXlib::GetDeviceSurfaceDimensions(PRInt32 &aWidth, PRInt32 &aHeight)
{
  PR_LOG(DeviceContextXlibLM, PR_LOG_DEBUG, ("nsDeviceContextXlib::GetDeviceSurfaceDimensions()\n"));
  aWidth = 1;
  aHeight = 1;
  return NS_OK;
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
                                          nsIDeviceContextPS::GetIID(),
                                          (void **)&dcps);

  NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't create PS Device context");
  
  dcps->SetSpec(aDevice);
  dcps->InitDeviceContextPS((nsIDeviceContext*)aContext,
                            (nsIDeviceContext*)this);

  rv = dcps->QueryInterface(nsIDeviceContext::GetIID(),
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

static void _EvilInitilizeGlobals()
{
  // set up the color info for this display
  // set up the masks
  gRedMask = gVisualInfo->red_mask;
  gGreenMask = gVisualInfo->green_mask;
  gBlueMask = gVisualInfo->blue_mask;
  gAlphaMask = 0;
  // set up the number of bits in each
  gRedCount = _ConvertMaskToCount(gVisualInfo->red_mask);
  gGreenCount = _ConvertMaskToCount(gVisualInfo->green_mask);
  gBlueCount = _ConvertMaskToCount(gVisualInfo->blue_mask);
  gAlphaCount = 0;
  // set up the number of bits that you need to shift to get to
  // a specific mask
  gRedShift = _GetShiftForMask(gVisualInfo->red_mask);
  gGreenShift = _GetShiftForMask(gVisualInfo->green_mask);
  gBlueShift = _GetShiftForMask(gVisualInfo->blue_mask);
  gAlphaShift = 0;
}

static PRUint8 _ConvertMaskToCount(unsigned long val)
{
  PRUint8 retval = 0;
  PRUint8 cur_bit = 0;
  // walk through the number, incrementing the value if
  // the bit in question is set.
  while (cur_bit < (sizeof(unsigned long) * 8)) {
    if ((val >> cur_bit) & 0x1) {
      retval++;
    }
    cur_bit++;
  }
  return retval;
}

static PRUint8 _GetShiftForMask(unsigned long val)
{
  PRUint8 cur_bit = 0;
  // walk through the number, looking for the first 1
  while (cur_bit < (sizeof(unsigned long) * 8)) {
    if ((val >> cur_bit) & 0x1) {
      return cur_bit;
    }
    cur_bit++;
  }
  return cur_bit;
}
