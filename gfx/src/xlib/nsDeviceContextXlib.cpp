/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Peter Hartshorn <peter@igelaus.com.au>
 *   Ken Faulkner <faulkner@igelaus.com.au>
 *   Tony Tsui <tony@igelaus.com.au>
 *   Tim Copperfield <timecop@network.email.ne.jp>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de> 
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsRenderingContextXlib.h"
#include "nsDrawingSurfaceXlib.h"
#include "nsDeviceContextXlib.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "nsGfxCIID.h"
#include "nspr.h"

#include "nsFontMetricsXlib.h"

#include "xlibrgb.h"
#include "X11/Xatom.h"

#include "nsDeviceContextSpecXlib.h"

#include "nsGfxPSCID.h"
#include "nsIDeviceContextPS.h"
#ifdef USE_XPRINT
#include "nsGfxXPrintCID.h"
#include "nsIDeviceContextXPrint.h"
#endif /* USE_XPRINT */

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_IID(kDeviceContextIID, NS_IDEVICE_CONTEXT_IID);

#define XLIB_DEFAULT_FONT1 "-*-helvetica-medium-r-*--*-120-*-*-*-*-iso8859-1"
#define XLIB_DEFAULT_FONT2 "-*-fixed-medium-r-*-*-*-120-*-*-*-*-*-*"

#ifdef PR_LOGGING 
static PRLogModuleInfo *DeviceContextXlibLM = PR_NewLogModule("DeviceContextXlib");
#endif /* PR_LOGGING */ 

/* global default font handle */
static XFontStruct *mDefaultFont = nsnull;

nsDeviceContextXlib::nsDeviceContextXlib()
  : nsDeviceContextX()
{
  PR_LOG(DeviceContextXlibLM, PR_LOG_DEBUG, ("nsDeviceContextXlib::nsDeviceContextXlib()\n"));
  mTwipsToPixels = 1.0;
  mPixelsToTwips = 1.0;
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
  mXlibRgbHandle = xxlib_find_handle(XXLIBRGB_DEFAULT_HANDLE);
  if (!mXlibRgbHandle)
    abort();
}

nsDeviceContextXlib::~nsDeviceContextXlib()
{
  nsIDrawingSurfaceXlib *surf = NS_STATIC_CAST(nsIDrawingSurfaceXlib *, mSurface);
  NS_IF_RELEASE(surf);
  mSurface = nsnull;
}

NS_IMETHODIMP nsDeviceContextXlib::Init(nsNativeWidget aNativeWidget)
{
  PR_LOG(DeviceContextXlibLM, PR_LOG_DEBUG, ("nsDeviceContextXlib::Init()\n"));

  mWidget = aNativeWidget;

  mDisplay = xxlib_rgb_get_display(mXlibRgbHandle);
  mScreen  = xxlib_rgb_get_screen(mXlibRgbHandle);
  mVisual  = xxlib_rgb_get_visual(mXlibRgbHandle);
  mDepth   = xxlib_rgb_get_depth(mXlibRgbHandle);

  if (!mDefaultFont)
    mDefaultFont = XLoadQueryFont(mDisplay, XLIB_DEFAULT_FONT1);

  if (!mDefaultFont)
    mDefaultFont = XLoadQueryFont(mDisplay, XLIB_DEFAULT_FONT2);

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
    nsCOMPtr<nsIPref> prefs(do_GetService(kPrefCID, &res));
    if (NS_SUCCEEDED(res) && prefs) {
      PRInt32 intVal = 96;
      res = prefs->GetIntPref("browser.display.screen_resolution", &intVal);
      if (NS_SUCCEEDED(res)) {
        if (intVal) {
          dpi = intVal;
        }
        else {
          // Compute dpi of display
          float screenWidth = float(XWidthOfScreen(mScreen));
          float screenWidthIn = float(XWidthMMOfScreen(mScreen)) / 25.4f;
          dpi = nscoord(screenWidth / screenWidthIn);
        }
      }
    }
  }

  // Do extra rounding (based on GTK). KenF
  mPixelsToTwips = float(NSToIntRound(float(NSIntPointsToTwips(72)) / float(dpi)));
  mTwipsToPixels = 1.0f / mPixelsToTwips;

  PR_LOG(DeviceContextXlibLM, PR_LOG_DEBUG, ("GFX: dpi=%d t2p=%g p2t=%g\n", dpi, mTwipsToPixels, mPixelsToTwips));

  mWidthFloat  = (float) XWidthOfScreen(mScreen);
  mHeightFloat = (float) XHeightOfScreen(mScreen);

  DeviceContextImpl::CommonInit();
}

NS_IMETHODIMP nsDeviceContextXlib::CreateRenderingContext(nsIRenderingContext *&aContext)
{
  PR_LOG(DeviceContextXlibLM, PR_LOG_DEBUG, ("nsDeviceContextXlib::CreateRenderingContext()\n"));

  nsIRenderingContext      *context;
  nsDrawingSurfaceXlibImpl *surface = nsnull;
  nsresult                  rv;

  context = new nsRenderingContextXlib();

  if (nsnull != context) {
    NS_ADDREF(context);
    surface = new nsDrawingSurfaceXlibImpl();
    if (nsnull != surface) {
      xGC *gc = new xGC(mDisplay, (Drawable)mWidget, 0, nsnull);
      rv = surface->Init(mXlibRgbHandle,
                         (Drawable)mWidget, 
                         gc);

      if (NS_SUCCEEDED(rv)) {
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
  
  if (NS_FAILED(rv)) {
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

NS_IMETHODIMP nsDeviceContextXlib::GetSystemFont(nsSystemFontID anID, nsFont *aFont) const
{
  PR_LOG(DeviceContextXlibLM, PR_LOG_DEBUG, ("nsDeviceContextXlib::GetSystemFont()\n"));

  switch (anID) {
    case eSystemFont_Caption:              // css2
    case eSystemFont_Icon:
    case eSystemFont_Menu:
    case eSystemFont_MessageBox:
    case eSystemFont_SmallCaption:
    case eSystemFont_StatusBar:
    case eSystemFont_Window:                       // css3
    case eSystemFont_Document:
    case eSystemFont_Workspace:
    case eSystemFont_Desktop:
    case eSystemFont_Info:
    case eSystemFont_Dialog:
    case eSystemFont_Button:
    case eSystemFont_PullDownMenu:
    case eSystemFont_List:
    case eSystemFont_Field:
    case eSystemFont_Tooltips:             // moz
    case eSystemFont_Widget:
      aFont->style       = NS_FONT_STYLE_NORMAL;
      aFont->weight      = NS_FONT_WEIGHT_NORMAL;
      aFont->decorations = NS_FONT_DECORATION_NONE;

      if (!mDefaultFont)
        return NS_ERROR_FAILURE;
      else
      {
        char *fontName = nsnull;
        unsigned long pr = 0;

        ::XGetFontProperty(mDefaultFont, XA_FULL_NAME, &pr);
        if(pr)
          {
            fontName = XGetAtomName(mDisplay, pr);
            aFont->name.AssignWithConversion(fontName);
            ::XFree(fontName);
          }
  
        pr = 0;
        ::XGetFontProperty(mDefaultFont, XA_WEIGHT, &pr);
        if ( pr > 10 )
          aFont->weight = NS_FONT_WEIGHT_BOLD;
    
        pr = 0;
        Atom pixelSizeAtom = ::XInternAtom(mDisplay, "PIXEL_SIZE", 0);
        ::XGetFontProperty(mDefaultFont, pixelSizeAtom, &pr);
        if( pr )
          aFont->size = NSIntPixelsToTwips(pr, mPixelsToTwips);

        pr = 0;
        ::XGetFontProperty(mDefaultFont, XA_ITALIC_ANGLE, &pr );
        if( pr )
          aFont->style = NS_FONT_STYLE_ITALIC;
    
        pr = 0;
        ::XGetFontProperty(mDefaultFont, XA_UNDERLINE_THICKNESS, &pr);
        if( pr )
          aFont->decorations = NS_FONT_DECORATION_UNDERLINE;
      }
      break;
  }

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextXlib::GetDrawingSurface(nsIRenderingContext &aContext, nsDrawingSurface &aSurface)
{
  PR_LOG(DeviceContextXlibLM, PR_LOG_DEBUG, ("nsDeviceContextXlib::GetDrawingSurface()\n"));
  if (nsnull == mSurface) {
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
  aPixel = xxlib_rgb_xpixel_from_rgb(mXlibRgbHandle,
                                     NS_RGB(NS_GET_B(aColor),
                                            NS_GET_G(aColor),
                                            NS_GET_R(aColor)));
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextXlib::CheckFontExistence(const nsString& aFontName)
{
  return nsFontMetricsXlib::FamilyExists(this, aFontName);
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
  nsresult                 rv;
  PrintMethod              method;
  nsDeviceContextSpecXlib *spec = NS_STATIC_CAST(nsDeviceContextSpecXlib *, aDevice);
  
  rv = spec->GetPrintMethod(method);
  if (NS_FAILED(rv)) 
    return rv;

#ifdef USE_XPRINT
  if (method == pmXprint) { // XPRINT
    static NS_DEFINE_CID(kCDeviceContextXp, NS_DEVICECONTEXTXP_CID);
    nsCOMPtr<nsIDeviceContextXp> dcxp(do_CreateInstance(kCDeviceContextXp, &rv));
    NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't create Xp Device context.");    
    if (NS_FAILED(rv)) 
      return rv;
    
    rv = dcxp->SetSpec(aDevice);
    if (NS_FAILED(rv)) 
      return rv;
    
    rv = dcxp->InitDeviceContextXP((nsIDeviceContext*)aContext,
                                   (nsIDeviceContext*)this);
    if (NS_FAILED(rv)) 
      return rv;
      
    rv = dcxp->QueryInterface(NS_GET_IID(nsIDeviceContext),
                              (void **)&aContext);
    return rv;
  }
  else
#endif /* USE_XPRINT */
  if (method == pmPostScript) { // PostScript
    // default/PS
    static NS_DEFINE_CID(kCDeviceContextPS, NS_DEVICECONTEXTPS_CID);
  
    // Create a Postscript device context 
    nsCOMPtr<nsIDeviceContextPS> dcps(do_CreateInstance(kCDeviceContextPS, &rv));
    NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't create PS Device context.");
    if (NS_FAILED(rv)) 
      return rv;
  
    rv = dcps->SetSpec(aDevice);
    if (NS_FAILED(rv)) 
      return rv;
      
    rv = dcps->InitDeviceContextPS((nsIDeviceContext*)aContext,
                                   (nsIDeviceContext*)this);
    if (NS_FAILED(rv)) 
      return rv;

    rv = dcps->QueryInterface(NS_GET_IID(nsIDeviceContext),
                              (void **)&aContext);
    return rv;
  }
  
  NS_WARNING("no print module created.");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP nsDeviceContextXlib::BeginDocument(PRUnichar * aTitle)
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

class nsFontCacheXlib : public nsFontCache
{
public:
  /* override DeviceContextImpl::CreateFontCache() */
  NS_IMETHODIMP CreateFontMetricsInstance(nsIFontMetrics** aResult);
};


NS_IMETHODIMP nsFontCacheXlib::CreateFontMetricsInstance(nsIFontMetrics** aResult)
{
  NS_PRECONDITION(aResult, "null out param");
  nsIFontMetrics *fm = new nsFontMetricsXlib();
  if (!fm)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(fm);
  *aResult = fm;
  return NS_OK;
}

/* override DeviceContextImpl::CreateFontCache() */
NS_IMETHODIMP nsDeviceContextXlib::CreateFontCache()
{
  mFontCache = new nsFontCacheXlib();
  if (nsnull == mFontCache) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mFontCache->Init(this);
  return NS_OK;
}


