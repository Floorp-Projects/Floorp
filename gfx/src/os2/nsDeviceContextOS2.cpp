/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is
 * John Fairhurst, <john_fairhurst@iname.com>.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Henry Sobotka <sobotka@axess.com> 2OOO/O2 update
 *   IBM Corp.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsDeviceContextOS2.h"
#include "nsRenderingContextOS2.h"
#include "nsDeviceContextSpecOS2.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"
#include "nsIScreenManager.h"
#include "nsIScreen.h"

#include "nsHashTable.h" // For CreateFontAliasTable()

#include "nsGfxDefs.h"
#include "nsIPref.h"

#include "nsOS2Uni.h"
#include "nsPaletteOS2.h"

#define NOT_SETUP 0x33
static PRBool gIsWarp4 = NOT_SETUP;

PRUint32 nsDeviceContextOS2::sNumberOfScreens = 0;
nscoord nsDeviceContextOS2::mDpi = 120;

nsDeviceContextOS2 :: nsDeviceContextOS2()
  : DeviceContextImpl()
{
  mSurface = NULL;
  mIsPaletteDevice = PR_FALSE;
  mPrintDC = NULL;
  mWidth = -1;
  mHeight = -1;
  mSpec = nsnull;
  mCachedClientRect = PR_FALSE;
  mCachedFullRect = PR_FALSE;
  mSupportsRasterFonts = PR_FALSE;
  mPrintingStarted = PR_FALSE;
#ifdef XP_OS2
  mPrintState = nsPrintState_ePreBeginDoc;
#endif

  // The first time in we initialize gIsWarp4 flag
  if (NOT_SETUP == gIsWarp4) {
    unsigned long ulValues[2];
    DosQuerySysInfo( QSV_VERSION_MAJOR, QSV_VERSION_MINOR,
                     ulValues, sizeof(ulValues));
    gIsWarp4 = (ulValues[0] >= 20) && (ulValues[1] >= 40);
  }
}

nsDeviceContextOS2::~nsDeviceContextOS2()
{
  if(mPrintDC)
  {
    GFX (::GpiDestroyPS (mPrintPS), FALSE);
    ::DevCloseDC(mPrintDC);
  } else {
    nsresult rv;
    nsCOMPtr<nsIPref> prefs = do_GetService(NS_PREF_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
      prefs->UnregisterCallback("browser.display.screen_resolution",
                                prefChanged, (void *)this);
    }
  }
  NS_IF_RELEASE(mSpec);
}

nsresult nsDeviceContextOS2::Init( nsNativeWidget aWidget)
{
  mWidget = aWidget;

  CommonInit(::WinOpenWindowDC((HWND)aWidget));

  static int initialized = 0;
  PRInt32 prefVal = -1;
  if (!initialized) {
    initialized = 1;

    // Set prefVal the value of the preference
    // "browser.display.screen_resolution"
    // or -1 if we can't get it.
    // If it's negative, we pretend it's not set.
    // If it's 0, it means force use of the operating system's logical
    // resolution.
    // If it's positive, we use it as the logical resolution
    nsresult res;

    nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &res));
    if (NS_SUCCEEDED(res) && prefs) {
      res = prefs->GetIntPref("browser.display.screen_resolution", &prefVal);
      if (NS_FAILED(res)) {
        prefVal = -1;
      }
      prefs->RegisterCallback("browser.display.screen_resolution", prefChanged,
                              (void *)this);
    }

    SetDPI(prefVal);
  } else {
    SetDPI(mDpi); // to setup p2t and t2p
  }

  return NS_OK;
}

// This version of Init() is called when creating a print DC
nsresult nsDeviceContextOS2::Init( nsNativeDeviceContext aContext,
                                   nsIDeviceContext *aOrigContext)
{
  float origscale, newscale;
  float t2d, a2d;

  mPrintDC = (HDC)aContext;

  NS_ASSERTION( mPrintDC, "!ERROR! - Received empty DC for printer");

#ifdef XP_OS2
  // Create a print PS now.  This is necessary 'cos we need it from
  // odd places to do font-y things, where the only common reference
  // point is this DC.  We can't just create a new PS because only one
  // PS can be associated with a given DC, and we can't get that PS from
  // the DC (really?).  And it would be slow :-)
  SIZEL sizel = { 0 , 0 };
  mPrintPS = GFX (::GpiCreatePS ( 0/*hab*/, mPrintDC, &sizel, 
                  PU_PELS | GPIT_MICRO | GPIA_ASSOC), GPI_ERROR);
#endif

  CommonInit( mPrintDC);

  long dpi;
  GFX (::DevQueryCaps(mPrintDC, CAPS_VERTICAL_FONT_RES, 1, &dpi), FALSE);

  mPixelsToTwips = ((float)NSIntPointsToTwips(72)) / ((float)dpi);
  mTwipsToPixels = 1.0 / mPixelsToTwips;

  newscale = TwipsToDevUnits();
// On OS/2, origscale can be different based on the video resolution.
// On 640x480, it's 1/15, on everything else it is 1/12.
// For consistent printing, 1/15 is the correct value to use.
// It is the closest to 4.x and to Windows.
//  origscale = aOrigContext->TwipsToDevUnits();
  origscale = 1.0/15.0;

  mCPixelScale = newscale / origscale;

  t2d = aOrigContext->TwipsToDevUnits();
  a2d = aOrigContext->AppUnitsToDevUnits();

  mAppUnitsToDevUnits = (a2d / t2d) * mTwipsToPixels;
  mDevUnitsToAppUnits = 1.0f / mAppUnitsToDevUnits;

#ifdef XP_OS2
  HCINFO hcinfo;
  PrnQueryHardcopyCaps( mPrintDC, &hcinfo);
  mWidth = hcinfo.xPels;
  mHeight = hcinfo.yPels;
  // XXX hsb says there are margin problems, must be from here...
#ifdef DEBUG
  printf( "Got surface of size %d x %d pixels (%d Kb)\n", mWidth, mHeight, mWidth * mHeight * mDepth / 8 / 1024);
  printf( "mCPixelScale = %f\n", mCPixelScale);
#endif

#endif

  return NS_OK;
}

void nsDeviceContextOS2 :: CommonInit(HDC aDC)
{
  LONG alArray[CAPS_DEVICE_POLYSET_POINTS];

  GFX (::DevQueryCaps(aDC, CAPS_FAMILY, CAPS_DEVICE_POLYSET_POINTS, alArray), FALSE);

  mDepth = alArray[CAPS_COLOR_BITCOUNT];
  mIsPaletteDevice = ((alArray[CAPS_ADDITIONAL_GRAPHICS] & CAPS_PALETTE_MANAGER) == CAPS_PALETTE_MANAGER);

  mWidth = alArray[CAPS_WIDTH];
  mHeight = alArray[CAPS_HEIGHT];

  if (alArray[CAPS_TECHNOLOGY] == CAPS_TECH_RASTER_DISPLAY)
  {
    // init the screen manager and compute our client rect based on the
    // screen objects. We'll save the result 
    nsresult ignore;
    mScreenManager = do_GetService("@mozilla.org/gfx/screenmanager;1", &ignore);   
    if ( !sNumberOfScreens )
      mScreenManager->GetNumberOfScreens(&sNumberOfScreens);
    mSupportsRasterFonts = !!(alArray[CAPS_RASTER_CAPS] & CAPS_RASTER_FONTS);
  } // if this dc is not a print device

  DeviceContextImpl::CommonInit();
}

void
nsDeviceContextOS2 :: ComputeClientRectUsingScreen ( nsRect* outRect )
{
  if ( !mCachedClientRect ) {
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
  }
  else
    *outRect = mClientRect;

} // ComputeClientRectUsingScreen


void
nsDeviceContextOS2 :: ComputeFullAreaUsingScreen ( nsRect* outRect )
{
  if ( !mCachedFullRect ) {
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
  }
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
nsDeviceContextOS2 :: FindScreen ( nsIScreen** outScreen )
{
  // optimize for the case where we only have one monitor.
  static nsCOMPtr<nsIScreen> sPrimaryScreen;
  if ( !sPrimaryScreen && mScreenManager )
    mScreenManager->GetPrimaryScreen ( getter_AddRefs(sPrimaryScreen) );  
  NS_IF_ADDREF(*outScreen = sPrimaryScreen.get());
  return;
} // FindScreen

/* OS2TODO - NOT PORTED */
// Create a rendering context against our hdc for a printer
nsresult nsDeviceContextOS2::CreateRenderingContext( nsIRenderingContext *&aContext)
{
#ifdef NS_PRINT_PREVIEW
   // Defer to Alt when there is one
  if (mAltDC && ((mUseAltDC & kUseAltDCFor_CREATERC_PAINT) || (mUseAltDC & kUseAltDCFor_CREATERC_REFLOW))) {
      return mAltDC->CreateRenderingContext(aContext);  
  }
#endif

   NS_ASSERTION( mPrintDC, "CreateRenderingContext for non-print DC");

   nsIRenderingContext *pContext = new nsRenderingContextOS2;
   if (!pContext)
     return NS_ERROR_OUT_OF_MEMORY;
   NS_ADDREF(pContext);

   nsPrintSurface *surf = new nsPrintSurface;
   if (!surf)
     return NS_ERROR_OUT_OF_MEMORY;

   surf->Init( mPrintPS, mWidth, mHeight, 0);

   nsresult rc = pContext->Init( this, (void*)((nsDrawingSurfaceOS2 *) surf));

   if( NS_OK != rc)
   {
      delete surf;
      NS_IF_RELEASE(pContext);
   }

   aContext = pContext;

   return rc;
}

NS_IMETHODIMP nsDeviceContextOS2 :: SupportsNativeWidgets(PRBool &aSupportsWidgets)
{
  if (nsnull == mPrintDC)
    aSupportsWidgets = PR_TRUE;
  else
    aSupportsWidgets = PR_FALSE;

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextOS2 :: GetScrollBarDimensions(float &aWidth, float &aHeight) const
{
  float scale = mCPixelScale;
  aWidth = ::WinQuerySysValue( HWND_DESKTOP, SV_CXVSCROLL) * mDevUnitsToAppUnits * scale;
  aHeight = ::WinQuerySysValue( HWND_DESKTOP, SV_CYHSCROLL) * mDevUnitsToAppUnits * scale;
  return NS_OK;
}

nscolor GetSysColorInfo(int iSysColor) 
{
  long lColor = ::WinQuerySysColor( HWND_DESKTOP, iSysColor, 0);
  RGB2 *pRGB2 = (RGB2*) &lColor;
  return NS_RGB( pRGB2->bRed, pRGB2->bGreen, pRGB2->bBlue);
}

/* Helper function to query font from INI file */

void QueryFontFromINI(char* fontType, char* fontName, ULONG ulLength)
{
  ULONG ulMaxNameL = ulLength;

  /* We had to switch to using PrfQueryProfileData because */
  /* some users have binary font data in their INI files */
  BOOL rc = PrfQueryProfileData(HINI_USER, "PM_SystemFonts", fontType,
                                fontName, &ulMaxNameL);
  /* If there was no entry in the INI, default to something */
  if (rc == FALSE) {
    /* Different values for DBCS vs. SBCS */
    if (!IsDBCS()) {
      /* WarpSans is only available on Warp 4 and above */
      if (gIsWarp4)
        strcpy(fontName, "9.WarpSans");
      else
        strcpy(fontName, "8.Helv");
    } else {
      /* WarpSans is only available on Warp 4 and above */
      if (gIsWarp4)
        strcpy(fontName, "9.WarpSans Combined");
      else
        strcpy(fontName, "10.Helv Combined");
    }
  } else {
    /* null terminate fontname */
    fontName[ulMaxNameL] = '\0';
  }
}


nsresult GetSysFontInfo(nsSystemFontID aID, nsFont* aFont) 
{
  char szFontNameSize[MAXNAMEL];

  switch (aID)
  {
    case eSystemFont_Icon: 
      QueryFontFromINI("IconText", szFontNameSize, MAXNAMEL);
      break;

    case eSystemFont_Menu: 
      QueryFontFromINI("Menus", szFontNameSize, MAXNAMEL);
      break;

    case eSystemFont_Caption: 

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
      QueryFontFromINI("WindowText", szFontNameSize, MAXNAMEL);
      break;
  } // switch 

  int pointSize;
  char *szFacename;

  pointSize = atoi(szFontNameSize);
  szFacename = strchr(szFontNameSize, '.');

  if ((pointSize == 0) || (!szFacename) || (*(szFacename++) == '\0')) {
     return NS_ERROR_FAILURE;
  }

#ifdef OLDCODE
  PRUnichar name[FACESIZE];
  name[0] = 0;
  MultiByteToWideChar(0, szFacename,
                      strlen(szFacename) + 1, name, sizeof(name)/sizeof(name[0]));
#endif
  aFont->name.AssignWithConversion(szFacename);

  // Do Style
  aFont->style = NS_FONT_STYLE_NORMAL;
#if 0
  if (ptrLogFont->lfItalic)
  {
    aFont->style = NS_FONT_STYLE_ITALIC;
  }
#endif
  // XXX What about oblique?

  aFont->variant = NS_FONT_VARIANT_NORMAL;

  // Do Weight
#if 0
  aFont->weight = (ptrLogFont->lfWeight == FW_BOLD ? 
            NS_FONT_WEIGHT_BOLD : NS_FONT_WEIGHT_NORMAL);
#else
  aFont->weight = NS_FONT_WEIGHT_NORMAL;
#endif

  // Do decorations
  aFont->decorations = NS_FONT_DECORATION_NONE;
#if 0
  if (ptrLogFont->lfUnderline)
  {
    aFont->decorations |= NS_FONT_DECORATION_UNDERLINE;
  }
  if (ptrLogFont->lfStrikeOut)
  {
    aFont->decorations |= NS_FONT_DECORATION_LINE_THROUGH;
  }
#endif

  // Do Size
  aFont->size = NSIntPointsToTwips(pointSize);

  aFont->systemFont = PR_TRUE;

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextOS2 :: GetSystemFont(nsSystemFontID aID, nsFont *aFont) const
{
  nsresult status = NS_OK;

  switch (aID) {
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
      status = GetSysFontInfo(aID, aFont);
      break;
    }
  }

  return status;
}

NS_IMETHODIMP nsDeviceContextOS2 :: CheckFontExistence(const nsString& aFontName)
{
  HPS     hps = NULL;

  if (NULL != mPrintDC){
    hps = mPrintPS;
  } else {
    hps = ::WinGetPS((HWND)mWidget);
  }

  nsAutoCharBuffer fontName;
  PRInt32 fontNameLength;
  WideCharToMultiByte(0, aFontName.get(), aFontName.Length(),
                      fontName, fontNameLength);

  long lWant = 0;
  long lFonts = GFX (::GpiQueryFonts(hps, QF_PUBLIC | QF_PRIVATE,
                                     fontName.get(), &lWant, 0, 0),
                     GPI_ALTERROR);

  if (NULL == mPrintDC)
    ::WinReleasePS(hps);

  if (lFonts > 0)
    return NS_OK;
  else
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDeviceContextOS2::GetDepth(PRUint32& aDepth)
{
  aDepth = mDepth;
  return NS_OK;
}

nsresult
nsDeviceContextOS2::SetDPI(PRInt32 aPrefDPI)
{
  // Set OSVal to what the operating system thinks the logical resolution is.
  long OSVal;
  HPS ps = ::WinGetScreenPS(HWND_DESKTOP);
  HDC hdc = GFX (::GpiQueryDevice (ps), HDC_ERROR);
  GFX (::DevQueryCaps(hdc, CAPS_HORIZONTAL_FONT_RES, 1, &OSVal), FALSE);
  ::WinReleasePS(ps);

  if (aPrefDPI == 0) {
    // If the pref is 0 use of OS value
    mDpi = OSVal;
  } else if (aPrefDPI > 0) {
    // If there's a valid pref value for the logical resolution,
    // use it.
    mDpi = aPrefDPI;
  } else {
    // if we couldn't get the pref or it's negative then use 120
    mDpi = 120;
  }

  int pt2t = 72;

  // make p2t a nice round number - this prevents rounding problems
  mPixelsToTwips = float(NSToIntRound(float(NSIntPointsToTwips(pt2t)) / float(mDpi)));
  mTwipsToPixels = 1.0f / mPixelsToTwips;

  // XXX need to reflow all documents
  return NS_OK;
}

int prefChanged(const char *aPref, void *aClosure)
{
  nsDeviceContextOS2 *context = (nsDeviceContextOS2*)aClosure;
  nsresult rv;
  
  if (nsCRT::strcmp(aPref, "browser.display.screen_resolution")==0) {
    PRInt32 dpi;
    nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &rv));
    rv = prefs->GetIntPref(aPref, &dpi);
    if (NS_SUCCEEDED(rv))
      context->SetDPI(dpi);

  }
  
  return 0;
}

NS_IMETHODIMP nsDeviceContextOS2 :: GetDeviceSurfaceDimensions(PRInt32 &aWidth, PRInt32 &aHeight)
{
#ifdef NS_PRINT_PREVIEW
  // Defer to Alt when there is one
  if (mAltDC && (mUseAltDC & kUseAltDCFor_SURFACE_DIM))
    return mAltDC->GetDeviceSurfaceDimensions(aWidth, aHeight);
#endif

  if ( mSpec )
  {
    // we have a printer device
    aWidth = NSToIntRound(mWidth * mDevUnitsToAppUnits);
    aHeight = NSToIntRound(mHeight * mDevUnitsToAppUnits);
  }
  else {
    nsRect area;
    ComputeFullAreaUsingScreen ( &area );
    aWidth = area.width;
    aHeight = area.height;
  }

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextOS2 :: GetRect(nsRect &aRect)
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


NS_IMETHODIMP nsDeviceContextOS2 :: GetClientRect(nsRect &aRect)
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

NS_IMETHODIMP nsDeviceContextOS2 :: GetDeviceContextFor(nsIDeviceContextSpec *aDevice,
                                                        nsIDeviceContext *&aContext)
{
  PRTQUEUE *pq;

  aContext = new nsDeviceContextOS2();
  if(nsnull != aContext){
    NS_ADDREF(aContext);
  } else {
    return NS_ERROR_OUT_OF_MEMORY;
  }
 
  ((nsDeviceContextOS2 *)aContext)->mSpec = aDevice;
  NS_ADDREF(aDevice);
  
  int numCopies = 0;
  int printerDest = 0;
  char *file = nsnull;

  ((nsDeviceContextSpecOS2 *)aDevice)->GetPRTQUEUE(pq);
  ((nsDeviceContextSpecOS2 *)aDevice)->GetCopies(numCopies);
  ((nsDeviceContextSpecOS2 *)aDevice)->GetDestination(printerDest);
  if (!printerDest) 
     ((nsDeviceContextSpecOS2 *)aDevice)->GetPath(&file);

  HDC dc = PrnOpenDC(pq, "Mozilla", numCopies, printerDest, file);

  if (!dc) {
     return NS_ERROR_FAILURE; //PMERROR("DevOpenDC");
  } /* endif */

  return ((nsDeviceContextOS2 *)aContext)->Init((nsNativeDeviceContext)dc, this);
}

nsresult nsDeviceContextOS2::CreateFontAliasTable()
{
   nsresult result = NS_OK;

   if( !mFontAliasTable)
   {
      mFontAliasTable = new nsHashtable;

      nsAutoString  times;              times.AssignLiteral("Times");
      nsAutoString  timesNewRoman;      timesNewRoman.AssignLiteral("Times New Roman");
      nsAutoString  timesRoman;         timesRoman.AssignLiteral("Tms Rmn");
      nsAutoString  arial;              arial.AssignLiteral("Arial");
      nsAutoString  helv;               helv.AssignLiteral("Helv");
      nsAutoString  helvetica;          helvetica.AssignLiteral("Helvetica");
      nsAutoString  courier;            courier.AssignLiteral("Courier");
      nsAutoString  courierNew;         courierNew.AssignLiteral("Courier New");
      nsAutoString  sans;               sans.AssignLiteral("Sans");
      nsAutoString  unicode;            unicode.AssignLiteral("Unicode");
      nsAutoString  timesNewRomanMT30;  timesNewRomanMT30.AssignLiteral("Times New Roman MT 30");
      nsAutoString  nullStr;

      AliasFont(times, timesNewRoman, timesRoman, PR_FALSE);
      AliasFont(timesRoman, timesNewRoman, times, PR_FALSE);
      AliasFont(timesNewRoman, timesRoman, times, PR_FALSE);
      AliasFont(arial, helv, helvetica, PR_FALSE);
      AliasFont(helvetica, helv, arial, PR_FALSE);
      AliasFont(courier, courierNew, nullStr, PR_TRUE);
      AliasFont(courierNew, courier, nullStr, PR_FALSE);
      AliasFont(sans, helv, arial, PR_FALSE);

      // Is this right?
      AliasFont(unicode, timesNewRomanMT30, nullStr, PR_FALSE);
   }
   return result;
}

// Printing ------------------------------------------------------------------
nsresult nsDeviceContextOS2::PrepareDocument(PRUnichar * aTitle, PRUnichar* aPrintToFileName)
{
  nsresult rv = NS_OK;

  if (NULL != mPrintDC){
    nsString titleStr;
    titleStr = aTitle;
    char *title = GetACPString(titleStr);

    PSZ pszDocName;
    const PSZ pszGenericDocName = "MozillaDocument";

    if (title) {
      pszDocName = title;
    } else {
      pszDocName = pszGenericDocName;
    } 

    long lDummy = 0;
    long lResult = ::DevEscape(mPrintDC, DEVESC_STARTDOC,
                               strlen(pszDocName) + 1, pszDocName,
                               &lDummy, NULL);

    mPrintingStarted = PR_TRUE;

    if (lResult == DEV_OK)
      rv = NS_OK;
    else
      rv = NS_ERROR_GFX_PRINTER_STARTDOC;

    if (title != nsnull) {
      nsMemory::Free(title);
    }
  }

  return rv;
}

nsresult nsDeviceContextOS2::BeginDocument(PRUnichar * aTitle, PRUnichar* aPrintToFileName, PRInt32 aStartPage, PRInt32 aEndPage)
{
  // Everything is done in PrepareDocument
  return NS_OK;
}

nsresult nsDeviceContextOS2::EndDocument()
{
  if (NULL != mPrintDC)
  {
    long   lOutCount = 2;
    USHORT usJobID = 0;
    long   lResult = ::DevEscape(mPrintDC, DEVESC_ENDDOC,
                                 0, NULL,
                                 &lOutCount, (PBYTE)&usJobID);
    if (lResult == DEV_OK)
      return NS_OK;
    else
      return NS_ERROR_GFX_PRINTER_ENDDOC;
  }

  return NS_OK;
}

nsresult nsDeviceContextOS2::AbortDocument()
{
  if (NULL != mPrintDC)
  {
    long ldummy = 0;
    long lResult = ::DevEscape(mPrintDC, DEVESC_ABORTDOC, 0, NULL,
                               &ldummy, NULL);
    if (lResult == DEV_OK)
      return NS_OK;
    else
      return NS_ERROR_ABORT;
  }

  return NS_OK;
}


nsresult nsDeviceContextOS2::BeginPage()
{
  if (mPrintingStarted) {
    mPrintingStarted = PR_FALSE;
    return NS_OK;
  }

  if (NULL != mPrintDC)
  {
    long lDummy = 0;
    long lResult = ::DevEscape(mPrintDC, DEVESC_NEWFRAME, 0, NULL,
                               &lDummy, NULL);

    if (lResult == DEV_OK)
      return NS_OK;
    else
      return NS_ERROR_GFX_PRINTER_STARTPAGE;
  }

  return NS_OK;

}

nsresult nsDeviceContextOS2::EndPage()
{
   return NS_OK;
}

char* 
nsDeviceContextOS2::GetACPString(const nsString& aStr)
{
   if (aStr.Length() == 0) {
      return nsnull;
   }

   nsAutoCharBuffer acp;
   PRInt32 acpLength;
   WideCharToMultiByte(0, aStr.get(), aStr.Length(), acp, acpLength);
   return ToNewCString(nsDependentCString(acp.get()));
}

BOOL nsDeviceContextOS2::isPrintDC()
{
   if ( mPrintDC == nsnull )
      return 0;

   else
      return 1;
}
PRBool nsDeviceContextOS2::SupportsRasterFonts()
{
   return mSupportsRasterFonts;
}
