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

#include "nsDeviceContextWin.h"
#include "nsRenderingContextWin.h"
#include "nsDeviceContextSpecWin.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"
#include "nsIScreenManager.h"
#include "nsIScreen.h"
#include "nsGfxCIID.h"
#include "nsReadableutils.h"

// Print Options
#include "nsIPrintOptions.h"
#include "nsString.h"
static NS_DEFINE_CID(kPrintOptionsCID, NS_PRINTOPTIONS_CID);

// Size of the color cube
#define COLOR_CUBE_SIZE       216

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

PRBool nsDeviceContextWin::gRound = PR_FALSE;
PRUint32 nsDeviceContextWin::sNumberOfScreens = 0;

static char* nav4rounding = "font.size.nav4rounding";

nsDeviceContextWin :: nsDeviceContextWin()
  : DeviceContextImpl()
{
  mSurface = NULL;
  mPaletteInfo.isPaletteDevice = PR_FALSE;
  mPaletteInfo.sizePalette = 0;
  mPaletteInfo.numReserved = 0;
  mPaletteInfo.palette = NULL;
  mDC = NULL;
  mPixelScale = 1.0f;
  mWidth = -1;
  mHeight = -1;
  mSpec = nsnull;
  mCachedClientRect = PR_FALSE;
  mCachedFullRect = PR_FALSE;

  nsresult res = NS_ERROR_FAILURE;
  nsCOMPtr<nsIPref> prefs(do_GetService(kPrefCID, &res));
  if (NS_SUCCEEDED(res)) {
    static PRBool roundingInitialized = PR_FALSE;
    if (!roundingInitialized) {
      roundingInitialized = PR_TRUE;
      PrefChanged(nav4rounding, this);
    }
    prefs->RegisterCallback(nav4rounding, PrefChanged, this);
  }
}

nsDeviceContextWin :: ~nsDeviceContextWin()
{
  nsDrawingSurfaceWin *surf = (nsDrawingSurfaceWin *)mSurface;

  NS_IF_RELEASE(surf);    //this clears the surf pointer...
  mSurface = nsnull;

  if (NULL != mPaletteInfo.palette)
    ::DeleteObject((HPALETTE)mPaletteInfo.palette);

  if (NULL != mDC)
  {
    ::DeleteDC(mDC);
    mDC = NULL;
  }

  NS_IF_RELEASE(mSpec);

  nsresult res = NS_ERROR_FAILURE;
  nsCOMPtr<nsIPref> prefs(do_GetService(kPrefCID, &res));
  if (NS_SUCCEEDED(res)) {
    prefs->UnregisterCallback(nav4rounding, PrefChanged, this);
  }
}

NS_IMETHODIMP nsDeviceContextWin :: Init(nsNativeWidget aWidget)
{
  nsresult retval = DeviceContextImpl::Init(aWidget);

  HWND  hwnd = (HWND)aWidget;
  HDC   hdc = ::GetDC(hwnd);

  CommonInit(hdc);

  ::ReleaseDC(hwnd, hdc);

  return retval;
}

//local method...

nsresult nsDeviceContextWin :: Init(nsNativeDeviceContext aContext, nsIDeviceContext *aOrigContext)
{
  float origscale, newscale;
  float t2d, a2d;

  mDC = (HDC)aContext;

  CommonInit(mDC);


  GetTwipsToDevUnits(newscale);
  aOrigContext->GetTwipsToDevUnits(origscale);

  mPixelScale = newscale / origscale;

  aOrigContext->GetTwipsToDevUnits(t2d);
  aOrigContext->GetAppUnitsToDevUnits(a2d);

  mAppUnitsToDevUnits = (a2d / t2d) * mTwipsToPixels;
  mDevUnitsToAppUnits = 1.0f / mAppUnitsToDevUnits;

  return NS_OK;
}

void nsDeviceContextWin :: CommonInit(HDC aDC)
{
  int   rasterCaps = ::GetDeviceCaps(aDC, RASTERCAPS);

  mTwipsToPixels = ((float)::GetDeviceCaps(aDC, LOGPIXELSY)) / (float)NSIntPointsToTwips(72);
  mPixelsToTwips = 1.0f / mTwipsToPixels;

  mDepth = (PRUint32)::GetDeviceCaps(aDC, BITSPIXEL);
  mPaletteInfo.isPaletteDevice = RC_PALETTE == (rasterCaps & RC_PALETTE);
  mPaletteInfo.sizePalette = (PRUint16)::GetDeviceCaps(aDC, SIZEPALETTE);
  mPaletteInfo.numReserved = (PRUint16)::GetDeviceCaps(aDC, NUMRESERVED);

  mWidth = ::GetDeviceCaps(aDC, HORZRES);
  mHeight = ::GetDeviceCaps(aDC, VERTRES);

  if (::GetDeviceCaps(aDC, TECHNOLOGY) == DT_RASDISPLAY)
  {
    // init the screen manager and compute our client rect based on the
    // screen objects. We'll save the result 
    nsresult ignore;
    mScreenManager = do_GetService("@mozilla.org/gfx/screenmanager;1", &ignore);   
    if ( !sNumberOfScreens )
      mScreenManager->GetNumberOfScreens(&sNumberOfScreens);
  } // if this dc is not a print device

  DeviceContextImpl::CommonInit();
}


void
nsDeviceContextWin :: ComputeClientRectUsingScreen ( nsRect* outRect )
{
  // if we have more than one screen, we always need to recompute the clientRect
  // because the window may have moved onto a different screen. In the single
  // monitor case, we only need to do the computation if we haven't done it
  // once already, and remember that we have because we're assured it won't change.
  if ( sNumberOfScreens > 1 || !mCachedClientRect ) {
    nsCOMPtr<nsIScreen> screen;
    FindScreen ( getter_AddRefs(screen) );
    if ( screen ) {
      PRInt32 x, y, width, height;
      screen->GetAvailRect ( &x, &y, &width, &height );
    
      // convert to device units
      outRect->y = NSToIntRound(y * mDevUnitsToAppUnits);
      outRect->x = NSToIntRound(x * mDevUnitsToAppUnits);
      outRect->width = NSToIntRound(width * mDevUnitsToAppUnits);
      outRect->height = NSToIntRound(height * mDevUnitsToAppUnits);

      mCachedClientRect = PR_TRUE;
      mClientRect = *outRect;
    }
  } // if we need to recompute the client rect
  else
    *outRect = mClientRect;

} // ComputeClientRectUsingScreen


void
nsDeviceContextWin :: ComputeFullAreaUsingScreen ( nsRect* outRect )
{
  // if we have more than one screen, we always need to recompute the clientRect
  // because the window may have moved onto a different screen. In the single
  // monitor case, we only need to do the computation if we haven't done it
  // once already, and remember that we have because we're assured it won't change.
  if ( sNumberOfScreens > 1 || !mCachedFullRect ) {
    nsCOMPtr<nsIScreen> screen;
    FindScreen ( getter_AddRefs(screen) );
    if ( screen ) {
      PRInt32 x, y, width, height;
      screen->GetRect ( &x, &y, &width, &height );
    
      // convert to device units
      outRect->y = NSToIntRound(y * mDevUnitsToAppUnits);
      outRect->x = NSToIntRound(x * mDevUnitsToAppUnits);
      outRect->width = NSToIntRound(width * mDevUnitsToAppUnits);
      outRect->height = NSToIntRound(height * mDevUnitsToAppUnits);

      mWidth = width;
      mHeight = height;
      mCachedFullRect = PR_TRUE;
    }
  } // if we need to recompute the client rect
  else {
      outRect->y = 0;
      outRect->x = 0;
      outRect->width = NSToIntRound(mWidth * mDevUnitsToAppUnits);
      outRect->height = NSToIntRound(mHeight * mDevUnitsToAppUnits);
  }
 
} // ComputeFullRectUsingScreen


//
// FindScreen
//
// Determines which screen intersects the largest area of the given surface.
//
void
nsDeviceContextWin :: FindScreen ( nsIScreen** outScreen )
{
  // optimize for the case where we only have one monitor.
  if ( !mPrimaryScreen && mScreenManager )
    mScreenManager->GetPrimaryScreen ( getter_AddRefs(mPrimaryScreen) );  
  if ( sNumberOfScreens == 1 ) {
    NS_IF_ADDREF(*outScreen = mPrimaryScreen.get());
    return;
  }
  
  // now then, if we have more than one screen, we need to find which screen this
  // window is on.
  HWND window = NS_REINTERPRET_CAST(HWND, mWidget);
  if ( window ) {
    RECT globalPosition;
    ::GetWindowRect ( window, &globalPosition ); 
    if ( mScreenManager )
      mScreenManager->ScreenForRect ( globalPosition.left, globalPosition.top, 
                                       globalPosition.right - globalPosition.left,
                                       globalPosition.bottom - globalPosition.top, outScreen );
  }

} // FindScreen


static NS_DEFINE_CID(kRCCID,NS_RENDERING_CONTEXT_CID);

NS_IMETHODIMP nsDeviceContextWin :: CreateRenderingContext(nsIRenderingContext *&aContext)
{
#ifdef NS_PRINT_PREVIEW
  // Defer to Alt when there is one
  if (mAltDC && (mUseAltDC & kUseAltDCFor_CREATE_RC)) {
    return mAltDC->CreateRenderingContext(aContext);
  }
#endif

  nsIRenderingContext *pContext;
  nsresult             rv;
  nsDrawingSurfaceWin  *surf;

  rv = nsComponentManager::CreateInstance(kRCCID,nsnull,NS_GET_IID(nsIRenderingContext),(void**)&pContext);

  if ( (NS_SUCCEEDED(rv)) && (nsnull != pContext))
  {
    surf = new nsDrawingSurfaceWin();

    if (nsnull != surf)
    {
      rv = surf->Init(mDC);

      if (NS_OK == rv)
        rv = pContext->Init(this, surf);
    }
    else
      rv = NS_ERROR_OUT_OF_MEMORY;
  }
  else
    rv = NS_ERROR_OUT_OF_MEMORY;

  if (NS_OK != rv)
  {
    NS_IF_RELEASE(pContext);
  }

  aContext = pContext;

  return rv;
}

NS_IMETHODIMP nsDeviceContextWin :: SupportsNativeWidgets(PRBool &aSupportsWidgets)
{
  if (nsnull == mDC)
    aSupportsWidgets = PR_TRUE;
  else
    aSupportsWidgets = PR_FALSE;

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextWin :: GetCanonicalPixelScale(float &aScale) const
{
  aScale = mPixelScale;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextWin :: GetScrollBarDimensions(float &aWidth, float &aHeight) const
{
  aWidth = ::GetSystemMetrics(SM_CXVSCROLL) * mDevUnitsToAppUnits;
  aHeight = ::GetSystemMetrics(SM_CXHSCROLL) * mDevUnitsToAppUnits;
  return NS_OK;
}

nsresult nsDeviceContextWin :: GetSysFontInfo(HDC aHDC, nsSystemFontID anID, nsFont* aFont) const
{
  NONCLIENTMETRICS ncm;
  HGDIOBJ hGDI;

  LOGFONT logFont;
  LOGFONT* ptrLogFont = NULL;

  BOOL status;
  if (anID == eSystemFont_Icon) 
  {
    status = ::SystemParametersInfo(SPI_GETICONTITLELOGFONT,
                                  sizeof(logFont),
                                  (PVOID)&logFont,
                                  0);
  }
  else
  {
    ncm.cbSize = sizeof(NONCLIENTMETRICS);
    status = ::SystemParametersInfo(SPI_GETNONCLIENTMETRICS, 
                                     sizeof(ncm),  
                                     (PVOID)&ncm, 
                                     0);
  }

  if (!status)
  {
    return NS_ERROR_FAILURE;
  }

  switch (anID)
  {
    // Caption in CSS is NOT the same as Caption on Windows
    //case eSystemFont_Caption: 
    //  ptrLogFont = &ncm.lfCaptionFont;
    //  break;

    case eSystemFont_Icon: 
      ptrLogFont = &logFont;
      break;

    case eSystemFont_Menu: 
      ptrLogFont = &ncm.lfMenuFont;
      break;

    case eSystemFont_MessageBox: 
      ptrLogFont = &ncm.lfMessageFont;
      break;

    case eSystemFont_SmallCaption: 
      ptrLogFont = &ncm.lfSmCaptionFont;
      break;

    case eSystemFont_StatusBar: 
    case eSystemFont_Tooltips: 
      ptrLogFont = &ncm.lfStatusFont;
      break;

    case eSystemFont_Widget:

    case eSystemFont_Window:      // css3
    case eSystemFont_Document:
    case eSystemFont_Workspace:
    case eSystemFont_Desktop:
    case eSystemFont_Info:
    case eSystemFont_Dialog:
    case eSystemFont_Button:
    case eSystemFont_PullDownMenu:
    case eSystemFont_List:
    case eSystemFont_Field:
    case eSystemFont_Caption: 
      hGDI = ::GetStockObject(DEFAULT_GUI_FONT);
      if (hGDI != NULL)
      {
        if (::GetObject(hGDI, sizeof(logFont), &logFont) > 0)
        { 
          ptrLogFont = &logFont;
        }
      }
      break;
  } // switch 

  if (nsnull == ptrLogFont)
  {
    return NS_ERROR_FAILURE;
  }

  PRUnichar name[LF_FACESIZE];
  name[0] = 0;
  MultiByteToWideChar(CP_ACP, 0, ptrLogFont->lfFaceName,
    strlen(ptrLogFont->lfFaceName) + 1, name, sizeof(name)/sizeof(name[0]));
  aFont->name = name;

  // Do Style
  aFont->style = NS_FONT_STYLE_NORMAL;
  if (ptrLogFont->lfItalic)
  {
    aFont->style = NS_FONT_STYLE_ITALIC;
  }
  // XXX What about oblique?

  aFont->variant = NS_FONT_VARIANT_NORMAL;

  // Do Weight
  aFont->weight = (ptrLogFont->lfWeight == FW_BOLD ? 
            NS_FONT_WEIGHT_BOLD : NS_FONT_WEIGHT_NORMAL);

  // Do decorations
  aFont->decorations = NS_FONT_DECORATION_NONE;
  if (ptrLogFont->lfUnderline)
  {
    aFont->decorations |= NS_FONT_DECORATION_UNDERLINE;
  }
  if (ptrLogFont->lfStrikeOut)
  {
    aFont->decorations |= NS_FONT_DECORATION_LINE_THROUGH;
  }

  // Do Point Size
  //
  // The lfHeight is in pixel and it needs to be adjusted for the
  // device it will be "displayed" on
  // Screens and Printers will differe in DPI
  //
  // So this accounts for the difference in the DeviceContexts
  // The mPixelScale will be a "1" for the screen and could be
  // any value when going to a printer, for example mPixleScale is
  // 6.25 when going to a 600dpi printer.
  // round, but take into account whether it is negative
  LONG logHeight = LONG((float(ptrLogFont->lfHeight) * mPixelScale) + (ptrLogFont->lfHeight < 0 ? -0.5 : 0.5)); // round up
  int pointSize = -MulDiv(logHeight, 72, ::GetDeviceCaps(aHDC, LOGPIXELSY));

  aFont->size = NSIntPointsToTwips(pointSize);

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextWin :: GetSystemFont(nsSystemFontID anID, nsFont *aFont) const
{
  nsresult status = NS_OK;

  switch (anID) {
    case eSystemFont_Caption: 
    case eSystemFont_Icon: 
    case eSystemFont_Menu: 
    case eSystemFont_MessageBox: 
    case eSystemFont_SmallCaption: 
    case eSystemFont_StatusBar: 
    case eSystemFont_Tooltips: 
    case eSystemFont_Widget:

    case eSystemFont_Window:      // css3
    case eSystemFont_Document:
    case eSystemFont_Workspace:
    case eSystemFont_Desktop:
    case eSystemFont_Info:
    case eSystemFont_Dialog:
    case eSystemFont_Button:
    case eSystemFont_PullDownMenu:
    case eSystemFont_List:
    case eSystemFont_Field:
    {
      HWND  hwnd;
      HDC   tdc;

      if (nsnull == mDC)
      {
        hwnd = (HWND)mWidget;
        tdc = ::GetDC(hwnd);
      }
      else
        tdc = mDC;

      status = GetSysFontInfo(tdc, anID, aFont);

      if (nsnull == mDC)
        ::ReleaseDC(hwnd, tdc);

      break;
    }
  }

  return status;
}

NS_IMETHODIMP nsDeviceContextWin :: GetDrawingSurface(nsIRenderingContext &aContext, nsDrawingSurface &aSurface)
{
  if (NULL == mSurface) {
    aContext.CreateDrawingSurface(nsnull, 0, mSurface);
  }

  aSurface = mSurface;
  return NS_OK;
}

int CALLBACK fontcallback(ENUMLOGFONT FAR *lpelf, NEWTEXTMETRIC FAR *lpntm,
                          int FontType, LPARAM lParam)  
{
  if (NULL != lpelf)
    *((PRBool *)lParam) = PR_TRUE;

  return 0;
}

NS_IMETHODIMP nsDeviceContextWin :: CheckFontExistence(const nsString& aFontName)
{
  HWND    hwnd = (HWND)mWidget;
  HDC     hdc = ::GetDC(hwnd);
  PRBool  isthere = PR_FALSE;

  char    fontName[LF_FACESIZE];

  const PRUnichar* unicodefontname = aFontName.get();

  int outlen = ::WideCharToMultiByte(CP_ACP, 0, aFontName.get(), aFontName.Length(), 
                                   fontName, LF_FACESIZE, NULL, NULL);
  if(outlen > 0)
    fontName[outlen] = '\0'; // null terminate

  // somehow the WideCharToMultiByte failed, let's try the old code
  if(0 == outlen) 
    aFontName.ToCString(fontName, LF_FACESIZE);

  ::EnumFontFamilies(hdc, fontName, (FONTENUMPROC)fontcallback, (LPARAM)&isthere);

  ::ReleaseDC(hwnd, hdc);

  if (PR_TRUE == isthere)
    return NS_OK;
  else
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDeviceContextWin::GetDepth(PRUint32& aDepth)
{
  aDepth = mDepth;
  return NS_OK;
}

#define COLOR_CUBE_SIZE 216

nscolor map[COLOR_CUBE_SIZE] = {
NS_RGB(0x0,  0x0,  0x0 ),
NS_RGB(0x0,  0x0,  0x33),
NS_RGB(0x0,  0x0,  0x66),
NS_RGB(0x0,  0x0,  0x99),
NS_RGB(0x0,  0x0,  0xcc),
NS_RGB(0x0,  0x0,  0xff),
NS_RGB(0x0,  0x33, 0x0 ),
NS_RGB(0x0,  0x33, 0x33),
NS_RGB(0x0,  0x33, 0x66),
NS_RGB(0x0,  0x33, 0x99),
NS_RGB(0x0,  0x33, 0xcc),
NS_RGB(0x0,  0x33, 0xff),
NS_RGB(0x0,  0x66, 0x0 ),
NS_RGB(0x0,  0x66, 0x33),
NS_RGB(0x0,  0x66, 0x66),
NS_RGB(0x0,  0x66, 0x99),
NS_RGB(0x0,  0x66, 0xcc),
NS_RGB(0x0,  0x66, 0xff),
NS_RGB(0x0,  0x99, 0x0 ),
NS_RGB(0x0,  0x99, 0x33),
NS_RGB(0x0,  0x99, 0x66),
NS_RGB(0x0,  0x99, 0x99),
NS_RGB(0x0,  0x99, 0xcc),
NS_RGB(0x0,  0x99, 0xff),
NS_RGB(0x0,  0xcc, 0x0 ),
NS_RGB(0x0,  0xcc, 0x33),
NS_RGB(0x0,  0xcc, 0x66),
NS_RGB(0x0,  0xcc, 0x99),
NS_RGB(0x0,  0xcc, 0xcc),
NS_RGB(0x0,  0xcc, 0xff),
NS_RGB(0x0,  0xff, 0x0 ),
NS_RGB(0x0,  0xff, 0x33),
NS_RGB(0x0,  0xff, 0x66),
NS_RGB(0x0,  0xff, 0x99),
NS_RGB(0x0,  0xff, 0xcc),
NS_RGB(0x0,  0xff, 0xff),
NS_RGB(0x33, 0x0,  0x0 ),
NS_RGB(0x33, 0x0,  0x33),
NS_RGB(0x33, 0x0,  0x66),
NS_RGB(0x33, 0x0,  0x99),
NS_RGB(0x33, 0x0,  0xcc),
NS_RGB(0x33, 0x0,  0xff),
NS_RGB(0x33, 0x33, 0x0 ),
NS_RGB(0x33, 0x33, 0x33),
NS_RGB(0x33, 0x33, 0x66),
NS_RGB(0x33, 0x33, 0x99),
NS_RGB(0x33, 0x33, 0xcc),
NS_RGB(0x33, 0x33, 0xff),
NS_RGB(0x33, 0x66, 0x0 ),
NS_RGB(0x33, 0x66, 0x33),
NS_RGB(0x33, 0x66, 0x66),
NS_RGB(0x33, 0x66, 0x99),
NS_RGB(0x33, 0x66, 0xcc),
NS_RGB(0x33, 0x66, 0xff),
NS_RGB(0x33, 0x99, 0x0 ),
NS_RGB(0x33, 0x99, 0x33),
NS_RGB(0x33, 0x99, 0x66),
NS_RGB(0x33, 0x99, 0x99),
NS_RGB(0x33, 0x99, 0xcc),
NS_RGB(0x33, 0x99, 0xff),
NS_RGB(0x33, 0xcc, 0x0 ),
NS_RGB(0x33, 0xcc, 0x33),
NS_RGB(0x33, 0xcc, 0x66),
NS_RGB(0x33, 0xcc, 0x99),
NS_RGB(0x33, 0xcc, 0xcc),
NS_RGB(0x33, 0xcc, 0xff),
NS_RGB(0x33, 0xff, 0x0 ),
NS_RGB(0x33, 0xff, 0x33),
NS_RGB(0x33, 0xff, 0x66),
NS_RGB(0x33, 0xff, 0x99),
NS_RGB(0x33, 0xff, 0xcc),
NS_RGB(0x33, 0xff, 0xff),
NS_RGB(0x66, 0x0,  0x0 ),
NS_RGB(0x66, 0x0,  0x33),
NS_RGB(0x66, 0x0,  0x66),
NS_RGB(0x66, 0x0,  0x99),
NS_RGB(0x66, 0x0,  0xcc),
NS_RGB(0x66, 0x0,  0xff),
NS_RGB(0x66, 0x33, 0x0 ),
NS_RGB(0x66, 0x33, 0x33),
NS_RGB(0x66, 0x33, 0x66),
NS_RGB(0x66, 0x33, 0x99),
NS_RGB(0x66, 0x33, 0xcc),
NS_RGB(0x66, 0x33, 0xff),
NS_RGB(0x66, 0x66, 0x0 ),
NS_RGB(0x66, 0x66, 0x33),
NS_RGB(0x66, 0x66, 0x66),
NS_RGB(0x66, 0x66, 0x99),
NS_RGB(0x66, 0x66, 0xcc),
NS_RGB(0x66, 0x66, 0xff),
NS_RGB(0x66, 0x99, 0x0 ),
NS_RGB(0x66, 0x99, 0x33),
NS_RGB(0x66, 0x99, 0x66),
NS_RGB(0x66, 0x99, 0x99),
NS_RGB(0x66, 0x99, 0xcc),
NS_RGB(0x66, 0x99, 0xff),
NS_RGB(0x66, 0xcc, 0x0 ),
NS_RGB(0x66, 0xcc, 0x33),
NS_RGB(0x66, 0xcc, 0x66),
NS_RGB(0x66, 0xcc, 0x99),
NS_RGB(0x66, 0xcc, 0xcc),
NS_RGB(0x66, 0xcc, 0xff),
NS_RGB(0x66, 0xff, 0x0 ),
NS_RGB(0x66, 0xff, 0x33),
NS_RGB(0x66, 0xff, 0x66),
NS_RGB(0x66, 0xff, 0x99),
NS_RGB(0x66, 0xff, 0xcc),
NS_RGB(0x66, 0xff, 0xff),
NS_RGB(0x99, 0x0,  0x0 ),
NS_RGB(0x99, 0x0,  0x33),
NS_RGB(0x99, 0x0,  0x66),
NS_RGB(0x99, 0x0,  0x99),
NS_RGB(0x99, 0x0,  0xcc),
NS_RGB(0x99, 0x0,  0xff),
NS_RGB(0x99, 0x33, 0x0 ),
NS_RGB(0x99, 0x33, 0x33),
NS_RGB(0x99, 0x33, 0x66),
NS_RGB(0x99, 0x33, 0x99),
NS_RGB(0x99, 0x33, 0xcc),
NS_RGB(0x99, 0x33, 0xff),
NS_RGB(0x99, 0x66, 0x0 ),
NS_RGB(0x99, 0x66, 0x33),
NS_RGB(0x99, 0x66, 0x66),
NS_RGB(0x99, 0x66, 0x99),
NS_RGB(0x99, 0x66, 0xcc),
NS_RGB(0x99, 0x66, 0xff),
NS_RGB(0x99, 0x99, 0x0 ),
NS_RGB(0x99, 0x99, 0x33),
NS_RGB(0x99, 0x99, 0x66),
NS_RGB(0x99, 0x99, 0x99),
NS_RGB(0x99, 0x99, 0xcc),
NS_RGB(0x99, 0x99, 0xff),
NS_RGB(0x99, 0xcc, 0x0 ),
NS_RGB(0x99, 0xcc, 0x33),
NS_RGB(0x99, 0xcc, 0x66),
NS_RGB(0x99, 0xcc, 0x99),
NS_RGB(0x99, 0xcc, 0xcc),
NS_RGB(0x99, 0xcc, 0xff),
NS_RGB(0x99, 0xff, 0x0 ),
NS_RGB(0x99, 0xff, 0x33),
NS_RGB(0x99, 0xff, 0x66),
NS_RGB(0x99, 0xff, 0x99),
NS_RGB(0x99, 0xff, 0xcc),
NS_RGB(0x99, 0xff, 0xff),
NS_RGB(0xcc, 0x0,  0x0 ),
NS_RGB(0xcc, 0x0,  0x33),
NS_RGB(0xcc, 0x0,  0x66),
NS_RGB(0xcc, 0x0,  0x99),
NS_RGB(0xcc, 0x0,  0xcc),
NS_RGB(0xcc, 0x0,  0xff),
NS_RGB(0xcc, 0x33, 0x0 ),
NS_RGB(0xcc, 0x33, 0x33),
NS_RGB(0xcc, 0x33, 0x66),
NS_RGB(0xcc, 0x33, 0x99),
NS_RGB(0xcc, 0x33, 0xcc),
NS_RGB(0xcc, 0x33, 0xff),
NS_RGB(0xcc, 0x66, 0x0 ),
NS_RGB(0xcc, 0x66, 0x33),
NS_RGB(0xcc, 0x66, 0x66),
NS_RGB(0xcc, 0x66, 0x99),
NS_RGB(0xcc, 0x66, 0xcc),
NS_RGB(0xcc, 0x66, 0xff),
NS_RGB(0xcc, 0x99, 0x0 ),
NS_RGB(0xcc, 0x99, 0x33),
NS_RGB(0xcc, 0x99, 0x66),
NS_RGB(0xcc, 0x99, 0x99),
NS_RGB(0xcc, 0x99, 0xcc),
NS_RGB(0xcc, 0x99, 0xff),
NS_RGB(0xcc, 0xcc, 0x0 ),
NS_RGB(0xcc, 0xcc, 0x33),
NS_RGB(0xcc, 0xcc, 0x66),
NS_RGB(0xcc, 0xcc, 0x99),
NS_RGB(0xcc, 0xcc, 0xcc),
NS_RGB(0xcc, 0xcc, 0xff),
NS_RGB(0xcc, 0xff, 0x0 ),
NS_RGB(0xcc, 0xff, 0x33),
NS_RGB(0xcc, 0xff, 0x66),
NS_RGB(0xcc, 0xff, 0x99),
NS_RGB(0xcc, 0xff, 0xcc),
NS_RGB(0xcc, 0xff, 0xff),
NS_RGB(0xff, 0x0,  0x0 ),
NS_RGB(0xff, 0x0,  0x33),
NS_RGB(0xff, 0x0,  0x66),
NS_RGB(0xff, 0x0,  0x99),
NS_RGB(0xff, 0x0,  0xcc),
NS_RGB(0xff, 0x0,  0xff),
NS_RGB(0xff, 0x33, 0x0 ),
NS_RGB(0xff, 0x33, 0x33),
NS_RGB(0xff, 0x33, 0x66),
NS_RGB(0xff, 0x33, 0x99),
NS_RGB(0xff, 0x33, 0xcc),
NS_RGB(0xff, 0x33, 0xff),
NS_RGB(0xff, 0x66, 0x0 ),
NS_RGB(0xff, 0x66, 0x33),
NS_RGB(0xff, 0x66, 0x66),
NS_RGB(0xff, 0x66, 0x99),
NS_RGB(0xff, 0x66, 0xcc),
NS_RGB(0xff, 0x66, 0xff),
NS_RGB(0xff, 0x99, 0x0 ),
NS_RGB(0xff, 0x99, 0x33),
NS_RGB(0xff, 0x99, 0x66),
NS_RGB(0xff, 0x99, 0x99),
NS_RGB(0xff, 0x99, 0xcc),
NS_RGB(0xff, 0x99, 0xff),
NS_RGB(0xff, 0xcc, 0x0 ),
NS_RGB(0xff, 0xcc, 0x33),
NS_RGB(0xff, 0xcc, 0x66),
NS_RGB(0xff, 0xcc, 0x99),
NS_RGB(0xff, 0xcc, 0xcc),
NS_RGB(0xff, 0xcc, 0xff),
NS_RGB(0xff, 0xff, 0x0 ),
NS_RGB(0xff, 0xff, 0x33),
NS_RGB(0xff, 0xff, 0x66),
NS_RGB(0xff, 0xff, 0x99),
NS_RGB(0xff, 0xff, 0xcc),
NS_RGB(0xff, 0xff, 0xff)};

NS_IMETHODIMP nsDeviceContextWin::GetPaletteInfo(nsPaletteInfo& aPaletteInfo)
{
  aPaletteInfo.isPaletteDevice = mPaletteInfo.isPaletteDevice;
  aPaletteInfo.sizePalette = mPaletteInfo.sizePalette;
  aPaletteInfo.numReserved = mPaletteInfo.numReserved;

  if (NULL == mPaletteInfo.palette) {
    // Create a logical palette
    BYTE         tmp[sizeof(LOGPALETTE) + ((COLOR_CUBE_SIZE + 20) * sizeof(PALETTEENTRY))];
    LPLOGPALETTE logPal = (LPLOGPALETTE)tmp;

    logPal->palVersion = 0x300;
    logPal->palNumEntries = COLOR_CUBE_SIZE + 20;
  
    // Initialize it from the default Windows palette
    HPALETTE  hDefaultPalette = (HPALETTE)::GetStockObject(DEFAULT_PALETTE);
  
    // First ten system colors
    ::GetPaletteEntries(hDefaultPalette, 0, 10, logPal->palPalEntry);

    // Last ten system colors
    ::GetPaletteEntries(hDefaultPalette, 10, 10, &logPal->palPalEntry[COLOR_CUBE_SIZE + 10]);
  
    // Now set the color cube entries.
    PALETTEENTRY* entry = &logPal->palPalEntry[10];
    for (PRInt32 i = 0; i < COLOR_CUBE_SIZE; i++) {
      entry->peRed = NS_GET_R(map[i]);
      entry->peGreen = NS_GET_G(map[i]);
      entry->peBlue = NS_GET_B(map[i]);
      entry->peFlags = 0;
  
      entry++;
    }
  
    // Create a GDI palette
    mPaletteInfo.palette = ::CreatePalette(logPal);
  }

  aPaletteInfo.palette = mPaletteInfo.palette;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextWin :: ConvertPixel(nscolor aColor, PRUint32 & aPixel)
{
  aPixel = aColor;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextWin :: GetDeviceSurfaceDimensions(PRInt32 &aWidth, PRInt32 &aHeight)
{
#ifdef NS_PRINT_PREVIEW
  // Defer to Alt when there is one
  if (mAltDC && (mUseAltDC & kUseAltDCFor_SURFACE_DIM)) {
    return mAltDC->GetDeviceSurfaceDimensions(aWidth, aHeight);
  }
#endif

  if ( mSpec )
  {
    // we have a printer device
    aWidth = NSToIntRound(mWidth * mDevUnitsToAppUnits);
    aHeight = NSToIntRound(mHeight * mDevUnitsToAppUnits);
  } 
  else 
  {
    nsRect area;
    ComputeFullAreaUsingScreen ( &area );
    aWidth = area.width;
    aHeight = area.height;
  }

  return NS_OK;
}


NS_IMETHODIMP nsDeviceContextWin :: GetRect(nsRect &aRect)
{
  if ( mSpec )
  {
    // we have a printer device
    aRect.x = 0;
    aRect.y = 0;
    aRect.width = NSToIntRound(mWidth * mDevUnitsToAppUnits);
    aRect.height = NSToIntRound(mHeight * mDevUnitsToAppUnits);
  }
  else
    ComputeFullAreaUsingScreen ( &aRect );

  return NS_OK;
}


NS_IMETHODIMP nsDeviceContextWin :: GetClientRect(nsRect &aRect)
{
  if ( mSpec )
  {
    // we have a printer device
    aRect.x = 0;
    aRect.y = 0;
    aRect.width = NSToIntRound(mWidth * mDevUnitsToAppUnits);
    aRect.height = NSToIntRound(mHeight * mDevUnitsToAppUnits);
  }
  else
    ComputeClientRectUsingScreen ( &aRect );

  return NS_OK;
}

BOOL CALLBACK abortproc( HDC hdc, int iError )
{
  return TRUE;
} 
 


NS_IMETHODIMP nsDeviceContextWin :: GetDeviceContextFor(nsIDeviceContextSpec *aDevice,
                                                        nsIDeviceContext *&aContext)
{
  char *devicename;
  char *drivername;
  HGLOBAL hdevmode;
  DEVMODE *devmode;

  //XXX this API should take an CID, use the repository and
  //then QI for the real object rather than casting... MMP

  aContext = new nsDeviceContextWin();
  if(nsnull != aContext){
    NS_ADDREF(aContext);
  } else {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  ((nsDeviceContextWin *)aContext)->mSpec = aDevice;
  NS_ADDREF(aDevice);
 
  ((nsDeviceContextSpecWin *)aDevice)->GetDeviceName(devicename);
  ((nsDeviceContextSpecWin *)aDevice)->GetDriverName(drivername);
  ((nsDeviceContextSpecWin *)aDevice)->GetDEVMODE(hdevmode);

  devmode = (DEVMODE *)::GlobalLock(hdevmode);
  HDC dc = ::CreateDC(drivername, devicename, NULL, devmode);

//  ::SetAbortProc(dc, (ABORTPROC)abortproc);

  ::GlobalUnlock(hdevmode);

  return ((nsDeviceContextWin *)aContext)->Init(dc, this);
}

NS_IMETHODIMP nsDeviceContextWin :: BeginDocument(PRUnichar * aTitle)
{
  nsresult rv = NS_OK;

  if (NULL != mDC){
    DOCINFO docinfo;

    nsString titleStr;
    titleStr = aTitle;
    char *title = GetACPString(titleStr);

    docinfo.cbSize = sizeof(docinfo);
    docinfo.lpszDocName = title != nsnull?title:"Mozilla Document";
#ifdef DEBUG_rods
    docinfo.lpszOutput = "p.ps";
#else
    docinfo.lpszOutput = NULL;
#endif
    docinfo.lpszDatatype = NULL;
    docinfo.fwType = 0;

    if (::StartDoc(mDC, &docinfo) > 0)
      rv = NS_OK;
    else
      rv = NS_ERROR_FAILURE;

    if (title != nsnull) {
      delete [] title;
    }
  }

  return rv;
}

NS_IMETHODIMP nsDeviceContextWin :: EndDocument(void)
{
  if (NULL != mDC)
  {
    if (::EndDoc(mDC) > 0)
      return NS_OK;
    else
      return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextWin :: BeginPage(void)
{
  if (NULL != mDC)
  {
    if (::StartPage(mDC) > 0)
      return NS_OK;
    else
      return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextWin :: EndPage(void)
{
  if (NULL != mDC)
  {
    if (::EndPage(mDC) > 0)
      return NS_OK;
    else
      return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

int
nsDeviceContextWin :: PrefChanged(const char* aPref, void* aClosure)
{
  nsresult res = NS_ERROR_FAILURE;
  nsCOMPtr<nsIPref> prefs(do_GetService(kPrefCID, &res));
  if (NS_SUCCEEDED(res)) {
    prefs->GetBoolPref(nav4rounding, &gRound);
    nsDeviceContextWin* deviceContext = (nsDeviceContextWin*) aClosure;
    deviceContext->FlushFontCache();
  }

  return 0;
}

char* 
nsDeviceContextWin :: GetACPString(const nsString& aStr)
{
   int acplen = aStr.Length() * 2 + 1;
   char * acp = new char[acplen];
   if(acp)
   {
      int outlen = ::WideCharToMultiByte( CP_ACP, 0, 
                      aStr.get(), aStr.Length(),
                      acp, acplen, NULL, NULL);
      if ( outlen > 0)
         acp[outlen] = '\0';  // null terminate
   }
   return acp;
}
