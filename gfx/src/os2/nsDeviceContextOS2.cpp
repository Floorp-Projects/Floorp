/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s):
 *   Henry Sobotka <sobotka@axess.com> 2OOO/O2 update
 *   IBM Corp.
 */

#include "nsDeviceContextOS2.h"
#include "nsRenderingContextOS2.h"
#include "nsDeviceContextSpecOS2.h"
#include "il_util.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"
#include "nsIScreenManager.h"
#include "nsIScreen.h"

#include "nsHashTable.h" // For CreateFontAliasTable()

#include "nsGfxDefs.h"

// Size of the color cube
#define COLOR_CUBE_SIZE       216

#define NOT_SETUP 0x33
static PRBool gIsWarp4 = NOT_SETUP;

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

PRBool nsDeviceContextOS2::gRound = PR_FALSE;
PRUint32 nsDeviceContextOS2::sNumberOfScreens = 0;

static char* nav4rounding = "font.size.nav4rounding";

nsDeviceContextOS2 :: nsDeviceContextOS2()
  : DeviceContextImpl()
{
  mSurface = nsnull;
  mPaletteInfo.isPaletteDevice = PR_FALSE;
  mPaletteInfo.sizePalette = 0;
  mPaletteInfo.numReserved = 0;
  mPaletteInfo.palette = nsnull;
  mDC = nsnull;
  mPixelScale = 1.0f;
  mWidth = -1;
  mHeight = -1;
  mSpec = nsnull;
  mCachedClientRect = PR_FALSE;
  mCachedFullRect = PR_FALSE;
#ifdef XP_OS2
  mPrintState = nsPrintState_ePreBeginDoc;
#endif

#ifdef XP_OS2 // OS2TODO - GET RID OF THIS!
   // Init module if necessary
   if( !gModuleData.lDisplayDepth)
      gModuleData.Init();
#endif

  // The first time in we initialize gIsWarp4 flag
  if (NOT_SETUP == gIsWarp4) {
    unsigned long ulValues[2];
    DosQuerySysInfo( QSV_VERSION_MAJOR, QSV_VERSION_MINOR,
                     ulValues, sizeof(ulValues));
    gIsWarp4 = (ulValues[0] >= 20) && (ulValues[1] >= 40);
  }

#ifndef XP_OS2
  nsresult res = NS_ERROR_FAILURE;
  NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &res);
  if (NS_SUCCEEDED(res)) {
    static PRBool roundingInitialized = PR_FALSE;
    if (!roundingInitialized) {
      roundingInitialized = PR_TRUE;
      PrefChanged(nav4rounding, this);
    }
    prefs->RegisterCallback(nav4rounding, PrefChanged, this);
  }
#endif
}

nsDeviceContextOS2::~nsDeviceContextOS2()
{
  if(mDC)
  {
     GpiAssociate(mPS, 0);
     GpiDestroyPS(mPS);
     PrnCloseDC(mDC);
  }

  NS_IF_RELEASE(mSpec);
}

nsresult nsDeviceContextOS2::Init( nsNativeWidget aWidget)
{
  nsresult retval = DeviceContextImpl::Init(aWidget);

  HWND  hwnd = (HWND)aWidget;
  HDC   hdc = WinOpenWindowDC(hwnd);

  CommonInit(hdc);

#ifdef XP_OS2 /* OS2TODO - Why are we doing this? These were set in CommonInit */
  // Get size - don't know if this is needed for non-print DCs
  long lCaps[2];
  DevQueryCaps( hdc, CAPS_WIDTH, 2, lCaps);
  mWidth = lCaps[0];  // these are in device units...
  mHeight = lCaps[1];
#endif

  return retval;
}

// This version of Init() is called when creating a print DC
nsresult nsDeviceContextOS2::Init( nsNativeDeviceContext aContext,
                                   nsIDeviceContext *aOrigContext)
{
  float origscale, newscale;
  float t2d, a2d;

  mDC = (HDC)aContext;

#ifdef XP_OS2
  // Create a print PS now.  This is necessary 'cos we need it from
  // odd places to do font-y things, where the only common reference
  // point is this DC.  We can't just create a new PS because only one
  // PS can be associated with a given DC, and we can't get that PS from
  // the DC (really?).  And it would be slow :-)
  SIZEL sizel = { 0 , 0 };
  mPS = GpiCreatePS( 0/*hab*/, mDC, &sizel,
                     PU_PELS | GPIT_MICRO | GPIA_ASSOC);
#endif

  CommonInit( mDC);

  GetTwipsToDevUnits( newscale);
  aOrigContext->GetTwipsToDevUnits( origscale);

  mPixelScale = newscale / origscale;

  aOrigContext->GetTwipsToDevUnits( t2d);
  aOrigContext->GetAppUnitsToDevUnits( a2d);

  mAppUnitsToDevUnits = (a2d / t2d) * mTwipsToPixels;
  mDevUnitsToAppUnits = 1.0f / mAppUnitsToDevUnits;

#ifdef XP_OS2
  HCINFO hcinfo;
  PrnQueryHardcopyCaps( mDC, &hcinfo);
  mWidth = hcinfo.xPels;
  mHeight = hcinfo.yPels;
  // XXX hsb says there are margin problems, must be from here...
  printf( "Got surface of size %d x %d pixels\n", mWidth, mHeight);
  printf( "mPixelScale = %f\n", mPixelScale);

  // We need to begin a document now, because the client is entitled at
  // this point to do stuff like create fonts, which required the PS to
  // be associated with a DC which has been DEVESC_STARTDOC'd.
  BeginDocument();
#endif

  return NS_OK;
}

void nsDeviceContextOS2 :: CommonInit(HDC aDC)
{
  LONG alArray[CAPS_DEVICE_POLYSET_POINTS];

  DevQueryCaps(aDC, CAPS_FAMILY, CAPS_DEVICE_POLYSET_POINTS, alArray);

  mTwipsToPixels = ((float)alArray[CAPS_VERTICAL_FONT_RES]) / (float)NSIntPointsToTwips(72);
  mPixelsToTwips = 1.0f / mTwipsToPixels;

  mDepth = alArray[CAPS_COLOR_BITCOUNT];
  mPaletteInfo.isPaletteDevice = !!(alArray[CAPS_ADDITIONAL_GRAPHICS] & CAPS_PALETTE_MANAGER);

  /* OS2TODO - pref to turn off palette management should set isPaletteDevice to false */

  if (mPaletteInfo.isPaletteDevice)
    mPaletteInfo.sizePalette = 256;

  if (alArray[CAPS_COLORS] >= 20) {
    mPaletteInfo.numReserved = 20;
  } else {
    mPaletteInfo.numReserved = alArray[CAPS_COLORS];
  } /* endif */

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
   NS_ASSERTION( mDC, "CreateRenderingContext for non-print DC");

   nsIRenderingContext *pContext = new nsRenderingContextOS2;
   if (!pContext)
     return NS_ERROR_OUT_OF_MEMORY;
   NS_ADDREF(pContext);

   nsPrintSurface *surf = new nsPrintSurface;
   if (!surf)
     return NS_ERROR_OUT_OF_MEMORY;
   NS_ADDREF(surf);

   surf->Init( mPS, mWidth, mHeight);

   nsresult rc = pContext->Init( this, (void*)((nsDrawingSurfaceOS2 *) surf));

   if( NS_OK != rc)
   {
      NS_IF_RELEASE(surf);
      NS_IF_RELEASE(pContext);
   }

   aContext = pContext;

   return rc;
}

NS_IMETHODIMP nsDeviceContextOS2 :: SupportsNativeWidgets(PRBool &aSupportsWidgets)
{
  if (nsnull == mDC)
    aSupportsWidgets = PR_TRUE;
  else
    aSupportsWidgets = PR_FALSE;

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextOS2 :: GetCanonicalPixelScale(float &aScale) const
{
  aScale = mPixelScale;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextOS2 :: GetScrollBarDimensions(float &aWidth, float &aHeight) const
{
  aWidth = ::WinQuerySysValue( HWND_DESKTOP, SV_CXVSCROLL) * mDevUnitsToAppUnits;
  aHeight = ::WinQuerySysValue( HWND_DESKTOP, SV_CYHSCROLL) * mDevUnitsToAppUnits;
  return NS_OK;
}

nscolor GetSysColorInfo(int iSysColor) 
{
  long lColor = WinQuerySysColor( HWND_DESKTOP, iSysColor, 0);
  RGB2 *pRGB2 = (RGB2*) &lColor;
  return NS_RGB( pRGB2->bRed, pRGB2->bGreen, pRGB2->bBlue);
}

nsresult GetSysFontInfo(nsSystemAttrID anID, nsFont* aFont) 
{
  char szFontNameSize[MAXNAMEL];

  switch (anID)
  {
    case eSystemAttr_Font_Caption: 
      if (!IsDBCS) {
        PrfQueryProfileString(HINI_USER, "PM_SystemFonts", "WindowText",
                              gIsWarp4 ? "9.WarpSans" : "8.Helv",
                              szFontNameSize, MAXNAMEL);
      } else {
        PrfQueryProfileString(HINI_USER, "PM_SystemFonts", "WindowText",
                              gIsWarp4 ? "9.WarpSans Combined" : "8.Helv Combined",
                              szFontNameSize, MAXNAMEL);
      } /* endif */
      break;

    case eSystemAttr_Font_Icon: 
      if (!IsDBCS) {
        PrfQueryProfileString(HINI_USER, "PM_SystemFonts", "IconText",
                              gIsWarp4 ? "9.WarpSans" : "8.Helv",
                              szFontNameSize, MAXNAMEL);
      } else {
        PrfQueryProfileString(HINI_USER, "PM_SystemFonts", "IconText",
                              gIsWarp4 ? "9.WarpSans Combined" : "8.Helv Combined",
                              szFontNameSize, MAXNAMEL);
      } /* endif */
      break;

    case eSystemAttr_Font_Menu: 
      if (!IsDBCS) {
        PrfQueryProfileString(HINI_USER, "PM_SystemFonts", "Menus",
                              gIsWarp4 ? "9.WarpSans Bold" : "10.Helv",
                              szFontNameSize, MAXNAMEL);
      } else {
        PrfQueryProfileString(HINI_USER, "PM_SystemFonts", "Menus", 
                              gIsWarp4 ? "9.WarpSans Combined" : "10.Helv Combined",
                              szFontNameSize, MAXNAMEL);
      } /* endif */
      break;

    case eSystemAttr_Font_MessageBox: 
      if (!IsDBCS) {
        PrfQueryProfileString(HINI_USER, "PM_SystemFonts", "WindowText",
                              gIsWarp4 ? "9.WarpSans" : "8.Helv",
                              szFontNameSize, MAXNAMEL);
      } else {
        PrfQueryProfileString(HINI_USER, "PM_SystemFonts", "WindowText",
                              gIsWarp4 ? "9.WarpSans Combined" : "8.Helv Combined",
                              szFontNameSize, MAXNAMEL);
      } /* endif */
      break;

    case eSystemAttr_Font_SmallCaption: 
      if (!IsDBCS) {
        PrfQueryProfileString(HINI_USER, "PM_SystemFonts", "WindowText",
                              gIsWarp4 ? "9.WarpSans" : "8.Helv",
                              szFontNameSize, MAXNAMEL);
      } else {
        PrfQueryProfileString(HINI_USER, "PM_SystemFonts", "WindowText", 
                              gIsWarp4 ? "9.WarpSans Combined" : "8.Helv Combined",
                              szFontNameSize, MAXNAMEL);
      } /* endif */
      break;

    case eSystemAttr_Font_StatusBar: 
    case eSystemAttr_Font_Tooltips: 
      if (!IsDBCS) {
        PrfQueryProfileString(HINI_USER, "PM_SystemFonts", "WindowText",
                              gIsWarp4 ? "9.WarpSans" : "8.Helv",
                              szFontNameSize, MAXNAMEL);
      } else {
        PrfQueryProfileString(HINI_USER, "PM_SystemFonts", "WindowText",
                              gIsWarp4 ? "9.WarpSans Combined" : "8.Helv Combined",
                              szFontNameSize, MAXNAMEL);
      } /* endif */
      break;

    case eSystemAttr_Font_Widget:

    case eSystemAttr_Font_Window:      // css3
    case eSystemAttr_Font_Document:
    case eSystemAttr_Font_Workspace:
    case eSystemAttr_Font_Desktop:
    case eSystemAttr_Font_Info:
    case eSystemAttr_Font_Dialog:
    case eSystemAttr_Font_Button:
    case eSystemAttr_Font_PullDownMenu:
    case eSystemAttr_Font_List:
    case eSystemAttr_Font_Field:
      if (!IsDBCS) {
        PrfQueryProfileString(HINI_USER, "PM_SystemFonts", "WindowText",
                              gIsWarp4 ? "9.WarpSans" : "8.Helv",
                              szFontNameSize, MAXNAMEL);
      } else {
        PrfQueryProfileString(HINI_USER, "PM_SystemFonts", "WindowText",
                              gIsWarp4 ? "9.WarpSans Combined" : "8.Helv Combined",
                              szFontNameSize, MAXNAMEL);
      } /* endif */
      break;
  } // switch 

  int pointSize;
  char szFacename[FACESIZE];

  sscanf( szFontNameSize, "%d.%s", &pointSize, szFacename);

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
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextOS2 :: GetSystemAttribute(nsSystemAttrID anID, SystemAttrStruct * aInfo) const
{
  nsresult status = NS_OK;

  switch (anID) {
    //---------
    // Colors
    //---------
    case eSystemAttr_Color_WindowBackground:
        *aInfo->mColor = GetSysColorInfo(SYSCLR_WINDOW);
        break;
    case eSystemAttr_Color_WindowForeground:
        *aInfo->mColor = GetSysColorInfo(SYSCLR_WINDOWTEXT);
        break;
    case eSystemAttr_Color_WidgetBackground:
        *aInfo->mColor = GetSysColorInfo(SYSCLR_BUTTONMIDDLE);
        break;
    case eSystemAttr_Color_WidgetForeground:
        *aInfo->mColor = GetSysColorInfo(SYSCLR_WINDOWTEXT);
        break;
    case eSystemAttr_Color_WidgetSelectBackground:
        *aInfo->mColor = GetSysColorInfo(SYSCLR_HILITEBACKGROUND);
        break;
    case eSystemAttr_Color_WidgetSelectForeground:
        *aInfo->mColor = GetSysColorInfo(SYSCLR_HILITEFOREGROUND);
        break;
    case eSystemAttr_Color_Widget3DHighlight:
        *aInfo->mColor = GetSysColorInfo(SYSCLR_BUTTONLIGHT);
        break;
    case eSystemAttr_Color_Widget3DShadow:
        *aInfo->mColor = GetSysColorInfo(SYSCLR_BUTTONDARK);
        break;
    case eSystemAttr_Color_TextBackground:
        *aInfo->mColor = GetSysColorInfo(SYSCLR_ENTRYFIELD);
        break;
    case eSystemAttr_Color_TextForeground:
        *aInfo->mColor = GetSysColorInfo(SYSCLR_WINDOWTEXT);
        break;
    case eSystemAttr_Color_TextSelectBackground:
        *aInfo->mColor = GetSysColorInfo(SYSCLR_HILITEBACKGROUND);
        break;
    case eSystemAttr_Color_TextSelectForeground:
        *aInfo->mColor = GetSysColorInfo(SYSCLR_HILITEFOREGROUND);
        break;
    //---------
    // Size
    //---------
    case eSystemAttr_Size_ScrollbarHeight : 
        aInfo->mSize = ::WinQuerySysValue(HWND_DESKTOP, SV_CYHSCROLL);
        break;
    case eSystemAttr_Size_ScrollbarWidth : 
        aInfo->mSize = ::WinQuerySysValue(HWND_DESKTOP, SV_CXVSCROLL);
        break;
    case eSystemAttr_Size_WindowTitleHeight:
        aInfo->mSize = ::WinQuerySysValue(HWND_DESKTOP, SV_CYTITLEBAR);
        break;
    case eSystemAttr_Size_WindowBorderWidth:
        aInfo->mSize = ::WinQuerySysValue(HWND_DESKTOP, SV_CXSIZEBORDER);
        break;
    case eSystemAttr_Size_WindowBorderHeight:
        aInfo->mSize = ::WinQuerySysValue(HWND_DESKTOP, SV_CYSIZEBORDER);
        break;
    case eSystemAttr_Size_Widget3DBorder:
        aInfo->mSize = ::WinQuerySysValue(HWND_DESKTOP, SV_CXBORDER);
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

    case eSystemAttr_Font_Window:      // css3
    case eSystemAttr_Font_Document:
    case eSystemAttr_Font_Workspace:
    case eSystemAttr_Font_Desktop:
    case eSystemAttr_Font_Info:
    case eSystemAttr_Font_Dialog:
    case eSystemAttr_Font_Button:
    case eSystemAttr_Font_PullDownMenu:
    case eSystemAttr_Font_List:
    case eSystemAttr_Font_Field:
    {
      status = GetSysFontInfo(anID, aInfo->mFont);
      break;
    }
  } // switch 

  return NS_OK;
}

nsresult nsDeviceContextOS2::GetDrawingSurface( nsIRenderingContext &aContext, nsDrawingSurface &aSurface)
{
  if (NULL == mSurface) {
    aContext.CreateDrawingSurface(nsnull, 0, mSurface);
  }

  aSurface = mSurface;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextOS2 :: CheckFontExistence(const nsString& aFontName)
{
  HWND    hwnd = (HWND)mWidget;
  HPS     hps = ::WinGetPS(hwnd);
  PRBool  isthere = PR_FALSE;

  char fontName[FACESIZE];
  aFontName.ToCString( fontName, FACESIZE);

  long lWant = 0;
  long lFonts = GpiQueryFonts( hps, QF_PUBLIC | QF_PRIVATE,
                               fontName, &lWant, 0, 0);

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

NS_IMETHODIMP nsDeviceContextOS2::GetILColorSpace(IL_ColorSpace*& aColorSpace)
{
  if (nsnull == mColorSpace) {
    // See if we're dealing with an 8-bit palette device
    if (8 == mDepth) {
      // Create a color cube. We want to use DIB_PAL_COLORS because it's faster
      // than DIB_RGB_COLORS, so make sure the indexes match that of the
      // GDI physical palette
      //
      // Note: the image library doesn't use the reserved colors, so it doesn't
      // matter what they're set to...
#ifdef XP_OS2
      IL_ColorMap* colorMap = IL_NewCubeColorMap(0, 0, COLOR_CUBE_SIZE + 10);
#else
      IL_RGB  reserved[10];
      memset(reserved, 0, sizeof(reserved));
      IL_ColorMap* colorMap = IL_NewCubeColorMap(reserved, 10, COLOR_CUBE_SIZE + 10);
#endif
      if (nsnull == colorMap) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
  
      // Create a pseudo color space
      mColorSpace = IL_CreatePseudoColorSpace(colorMap, 8, 8);
  
    } else {
      IL_RGBBits colorRGBBits;
    
      // Create a 24-bit color space
      colorRGBBits.red_shift = 16;  
      colorRGBBits.red_bits = 8;
      colorRGBBits.green_shift = 8;
      colorRGBBits.green_bits = 8; 
      colorRGBBits.blue_shift = 0; 
      colorRGBBits.blue_bits = 8;  
    
      mColorSpace = IL_CreateTrueColorSpace(&colorRGBBits, 24);
    }

    if (nsnull == mColorSpace) {
      aColorSpace = nsnull;
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  // Return the color space
  aColorSpace = mColorSpace;
  IL_AddRefToColorSpace(aColorSpace);
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextOS2::GetPaletteInfo(nsPaletteInfo& aPaletteInfo)
{
  aPaletteInfo.isPaletteDevice = mPaletteInfo.isPaletteDevice;
  aPaletteInfo.numReserved = mPaletteInfo.numReserved;

  if (NULL == mPaletteInfo.palette) {
    IL_ColorSpace*  colorSpace;
    GetILColorSpace(colorSpace);

    if (NI_PseudoColor == colorSpace->type) {
      // Create a logical palette
#ifdef XP_OS2
      PULONG aulTable;
      ULONG ulCount = COLOR_CUBE_SIZE;
      aulTable = (PULONG)malloc(ulCount*sizeof(ULONG));
#else
      ULONG aulTable[COLOR_CUBE_SIZE+20];
      ULONG ulCount = COLOR_CUBE_SIZE + 20;
#endif

#ifndef XP_OS2
      // First ten system colors
      ::GetPaletteEntries(hDefaultPalette, 0, 10, logPal->palPalEntry);

      // Last ten system colors
      ::GetPaletteEntries(hDefaultPalette, 10, 10, &logPal->palPalEntry[COLOR_CUBE_SIZE + 10]);
#endif
  
      // Now set the color cube entries.
#ifdef XP_OS2
      NI_RGB*       map = colorSpace->cmap.map;
#else
      NI_RGB*       map = colorSpace->cmap.map + 10;
#endif
#ifdef XP_OS2
      for (PRInt32 i = 0; i < COLOR_CUBE_SIZE; i++, map++) {
#else
      for (PRInt32 i = 10; i < COLOR_CUBE_SIZE; i++, map++) {
#endif
        aulTable[i] = MK_RGB( map->red, map->green, map->blue);
      }
  
      if (mPaletteInfo.isPaletteDevice) {
        // Create a GPI palette
        mPaletteInfo.palette = (void*)::GpiCreatePalette( (HAB)0, NULL, LCOLF_CONSECRGB, ulCount, aulTable );
        free(aulTable);
      } else {
        mPaletteInfo.palette = (void*)aulTable;
        mPaletteInfo.sizePalette = ulCount;
      }
    }

    IL_ReleaseColorSpace(colorSpace);
  }

  aPaletteInfo.palette = mPaletteInfo.palette;
  aPaletteInfo.sizePalette = mPaletteInfo.sizePalette;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextOS2 :: ConvertPixel(nscolor aColor, PRUint32 & aPixel)
{
  aPixel = aColor;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextOS2 :: GetDeviceSurfaceDimensions(PRInt32 &aWidth, PRInt32 &aHeight)
{
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

  ((nsDeviceContextOS2 *)aContext)->mSpec = aDevice;
  NS_ADDREF(aDevice);

  ((nsDeviceContextSpecOS2 *)aDevice)->GetPRTQUEUE(pq);

  HDC dc = PrnOpenDC(pq, "Mozilla");

  return ((nsDeviceContextOS2 *)aContext)->Init((nsNativeDeviceContext)dc, this);
}

nsresult nsDeviceContextOS2::CreateFontAliasTable()
{
   nsresult result = NS_OK;

   if( !mFontAliasTable)
   {
      mFontAliasTable = new nsHashtable;

      nsAutoString  times;              times.AssignWithConversion("Times");
      nsAutoString  timesNewRoman;      timesNewRoman.AssignWithConversion("Times New Roman");
      nsAutoString  timesRoman;         timesRoman.AssignWithConversion("Tms Rmn");
      nsAutoString  arial;              arial.AssignWithConversion("Arial");
      nsAutoString  helv;               helv.AssignWithConversion("Helv");
      nsAutoString  helvetica;          helvetica.AssignWithConversion("Helvetica");
      nsAutoString  courier;            courier.AssignWithConversion("Courier");
      nsAutoString  courierNew;         courierNew.AssignWithConversion("Courier New");
      nsAutoString  sans;               sans.AssignWithConversion("Sans");
      nsAutoString  unicode;            unicode.AssignWithConversion("Unicode");
      nsAutoString  timesNewRomanMT30;  timesNewRomanMT30.AssignWithConversion("Times New Roman MT 30");
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
nsresult nsDeviceContextOS2::BeginDocument()
{
   NS_ASSERTION(mDC, "BeginDocument for non-print DC");
   if( mPrintState == nsPrintState_ePreBeginDoc)
   {
      PrnStartJob( mDC, "Warpzilla NGLayout job");
      printf( "BeginDoc\n");
      mPrintState = nsPrintState_eBegunDoc;
   }
   return NS_OK;
}

nsresult nsDeviceContextOS2::EndDocument()
{
   PrnEndJob( mDC);
   mPrintState = nsPrintState_ePreBeginDoc;
   printf("EndDoc\n");
   return NS_OK;
}

nsresult nsDeviceContextOS2::BeginPage()
{
   if( mPrintState == nsPrintState_eBegunDoc)
      mPrintState = nsPrintState_eBegunFirstPage;
   else
   {
      PrnNewPage( mDC);
      printf("NewPage");
   }
   return NS_OK;
}

nsresult nsDeviceContextOS2::EndPage()
{
   /* nop */
   return NS_OK;
}

BOOL nsDeviceContextOS2::isPrintDC()
{
   if ( mDC == nsnull )
      return 0;

   else
      return 1;
}
