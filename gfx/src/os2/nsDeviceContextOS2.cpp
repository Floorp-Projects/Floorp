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
#ifdef COLOR_256
#include "il_util.h"
#endif
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
  mSurface = NULL;
#ifdef COLOR_256
  mPaletteInfo.isPaletteDevice = PR_FALSE;
  mPaletteInfo.sizePalette = 0;
  mPaletteInfo.numReserved = 0;
  mPaletteInfo.palette = NULL;
#endif
  mPrintDC = NULL;
  mPixelScale = 1.0f;
  mWidth = -1;
  mHeight = -1;
  mSpec = nsnull;
  mCachedClientRect = PR_FALSE;
  mCachedFullRect = PR_FALSE;
  mSupportsRasterFonts = PR_FALSE;
  mPelsPerMeter = 0;
#ifdef XP_OS2
  mPrintState = nsPrintState_ePreBeginDoc;
#endif

#ifdef XP_OS2 // OS2TODO - GET RID OF THIS!
   // Init module if necessary
   if( !gGfxModuleData.hpsScreen)
      gGfxModuleData.Init();
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
  nsCOMPtr<nsIPref> prefs(do_GetService(kPrefCID, &res));
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
  if(mPrintDC)
  {
    GFX (::GpiDestroyPS (mPrintPS), FALSE);
    ::DevCloseDC(mPrintDC);
  }

  NS_IF_RELEASE(mSpec);
}

nsresult nsDeviceContextOS2::Init( nsNativeWidget aWidget)
{
  nsresult retval = DeviceContextImpl::Init(aWidget);

  CommonInit(::WinOpenWindowDC((HWND)aWidget));

  return retval;
}

// This version of Init() is called when creating a print DC
nsresult nsDeviceContextOS2::Init( nsNativeDeviceContext aContext,
                                   nsIDeviceContext *aOrigContext)
{
//  float origscale, newscale;
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

//  GetTwipsToDevUnits( newscale);
//  aOrigContext->GetTwipsToDevUnits( origscale);

//  mPixelScale = newscale / origscale;

  mPixelScale = (float)mPelsPerMeter / 3622.0f; /* This is ugly - it prevents different size printing on different */
                                                /* Resolutions */

  aOrigContext->GetTwipsToDevUnits( t2d);
  aOrigContext->GetAppUnitsToDevUnits( a2d);

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
  printf( "mPixelScale = %f\n", mPixelScale);
#endif

  // We need to begin a document now, because the client is entitled at
  // this point to do stuff like create fonts, which required the PS to
  // be associated with a DC which has been DEVESC_STARTDOC'd.

  // Commenting out for now - we'll see what people say about the effects
  // This line causes us not to print on Lexmark printers
//  BeginDocument(nsnull);
#endif

  return NS_OK;
}

void nsDeviceContextOS2 :: CommonInit(HDC aDC)
{
  LONG alArray[CAPS_DEVICE_POLYSET_POINTS];

  GFX (::DevQueryCaps(aDC, CAPS_FAMILY, CAPS_DEVICE_POLYSET_POINTS, alArray), FALSE);

  mTwipsToPixels = ((float)alArray [CAPS_VERTICAL_FONT_RES]) / (float)NSIntPointsToTwips(72);

  mPixelsToTwips = 1.0f / mTwipsToPixels;

  mPelsPerMeter = alArray[CAPS_VERTICAL_RESOLUTION];

  mDepth = alArray[CAPS_COLOR_BITCOUNT];
#ifdef COLOR_256
  mPaletteInfo.isPaletteDevice = !!(alArray[CAPS_ADDITIONAL_GRAPHICS] & CAPS_PALETTE_MANAGER);

  /* OS2TODO - pref to turn off palette management should set isPaletteDevice to false */

  if (mPaletteInfo.isPaletteDevice)
    mPaletteInfo.sizePalette = 256;

  if (alArray[CAPS_COLORS] >= 20) {
    mPaletteInfo.numReserved = 20;
  } else {
    mPaletteInfo.numReserved = alArray[CAPS_COLORS];
  } /* endif */
#endif

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
  long lColor = ::WinQuerySysColor( HWND_DESKTOP, iSysColor, 0);
  RGB2 *pRGB2 = (RGB2*) &lColor;
  return NS_RGB( pRGB2->bRed, pRGB2->bGreen, pRGB2->bBlue);
}

nsresult GetSysFontInfo(nsSystemFontID aID, nsFont* aFont) 
{
  char szFontNameSize[MAXNAMEL];

  switch (aID)
  {
    case eSystemFont_Caption: 
      if (!IsDBCS()) {
        PrfQueryProfileString(HINI_USER, "PM_SystemFonts", "WindowText",
                              gIsWarp4 ? "9.WarpSans" : "8.Helv",
                              szFontNameSize, MAXNAMEL);
      } else {
        PrfQueryProfileString(HINI_USER, "PM_SystemFonts", "WindowText",
                              gIsWarp4 ? "9.WarpSans Combined" : "8.Helv Combined",
                              szFontNameSize, MAXNAMEL);
      } /* endif */
      break;

    case eSystemFont_Icon: 
      if (!IsDBCS()) {
        PrfQueryProfileString(HINI_USER, "PM_SystemFonts", "IconText",
                              gIsWarp4 ? "9.WarpSans" : "8.Helv",
                              szFontNameSize, MAXNAMEL);
      } else {
        PrfQueryProfileString(HINI_USER, "PM_SystemFonts", "IconText",
                              gIsWarp4 ? "9.WarpSans Combined" : "8.Helv Combined",
                              szFontNameSize, MAXNAMEL);
      } /* endif */
      break;

    case eSystemFont_Menu: 
      if (!IsDBCS()) {
        PrfQueryProfileString(HINI_USER, "PM_SystemFonts", "Menus",
                              gIsWarp4 ? "9.WarpSans Bold" : "10.Helv",
                              szFontNameSize, MAXNAMEL);
      } else {
        PrfQueryProfileString(HINI_USER, "PM_SystemFonts", "Menus", 
                              gIsWarp4 ? "9.WarpSans Combined" : "10.Helv Combined",
                              szFontNameSize, MAXNAMEL);
      } /* endif */
      break;

    case eSystemFont_MessageBox: 
      if (!IsDBCS()) {
        PrfQueryProfileString(HINI_USER, "PM_SystemFonts", "WindowText",
                              gIsWarp4 ? "9.WarpSans" : "8.Helv",
                              szFontNameSize, MAXNAMEL);
      } else {
        PrfQueryProfileString(HINI_USER, "PM_SystemFonts", "WindowText",
                              gIsWarp4 ? "9.WarpSans Combined" : "8.Helv Combined",
                              szFontNameSize, MAXNAMEL);
      } /* endif */
      break;

    case eSystemFont_SmallCaption: 
      if (!IsDBCS()) {
        PrfQueryProfileString(HINI_USER, "PM_SystemFonts", "WindowText",
                              gIsWarp4 ? "9.WarpSans" : "8.Helv",
                              szFontNameSize, MAXNAMEL);
      } else {
        PrfQueryProfileString(HINI_USER, "PM_SystemFonts", "WindowText", 
                              gIsWarp4 ? "9.WarpSans Combined" : "8.Helv Combined",
                              szFontNameSize, MAXNAMEL);
      } /* endif */
      break;

    case eSystemFont_StatusBar: 
    case eSystemFont_Tooltips: 
      if (!IsDBCS()) {
        PrfQueryProfileString(HINI_USER, "PM_SystemFonts", "WindowText",
                              gIsWarp4 ? "9.WarpSans" : "8.Helv",
                              szFontNameSize, MAXNAMEL);
      } else {
        PrfQueryProfileString(HINI_USER, "PM_SystemFonts", "WindowText",
                              gIsWarp4 ? "9.WarpSans Combined" : "8.Helv Combined",
                              szFontNameSize, MAXNAMEL);
      } /* endif */
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
      if (!IsDBCS()) {
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
  char *szFacename;

  pointSize = atoi(szFontNameSize);

  szFacename = strchr(szFontNameSize, '.');
  szFacename++;

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
  HPS     hps = NULL;
  PRBool  isthere = PR_FALSE;

  if (NULL != mPrintDC){
    hps = mPrintPS;
  } else {
    hps = ::WinGetPS((HWND)mWidget);
  }

  char fontName[FACESIZE];

  WideCharToMultiByte(0, aFontName.get(), aFontName.Length() + 1,
    fontName, sizeof(fontName));

  long lWant = 0;
  long lFonts = GFX (::GpiQueryFonts (hps, QF_PUBLIC | QF_PRIVATE,
                     fontName, &lWant, 0, 0), GPI_ALTERROR);

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

#ifdef COLOR_256
NS_IMETHODIMP nsDeviceContextOS2::GetILColorSpace(IL_ColorSpace*& aColorSpace)
{
  if (nsnull == mColorSpace) {
    // See if we're dealing with an 8-bit palette device
    if ((8 == mDepth) && mPaletteInfo.isPaletteDevice) {
      // Create a color cube. We want to use DIB_PAL_COLORS because it's faster
      // than DIB_RGB_COLORS, so make sure the indexes match that of the
      // GDI physical palette
      //
      // Note: the image library doesn't use the reserved colors, so it doesn't
      // matter what they're set to...
#ifdef XP_OS2
      IL_ColorMap* colorMap = IL_NewCubeColorMap(0, 0, COLOR_CUBE_SIZE);
#else /* This code causes traps on 256 color */
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

#define NUM_SYS_COLORS 22

typedef struct _MYRGB
{
  BYTE red;
  BYTE green;
  BYTE blue;
} MYRGB;

MYRGB sysColors[NUM_SYS_COLORS] =
{
    0x00, 0x00, 0x00,   // CLR_BLACK
    0x00, 0x00, 0x80,   // CLR_DARKBLUE
    0x00, 0x80, 0x00,   // CLR_DARKGREEN
    0x00, 0x80, 0x80,   // CLR_DARKCYAN
    0x80, 0x00, 0x00,   // CLR_DARKRED
    0x80, 0x00, 0x80,   // CLR_DARKPINK
    0x80, 0x80, 0x00,   // CLR_BROWN
    0x80, 0x80, 0x80,   // CLR_DARKGRAY
    0xCC, 0xCC, 0xCC,   // CLR_PALEGRAY
    0x00, 0x00, 0xFF,   // CLR_BLUE
    0x00, 0xFF, 0x00,   // CLR_GREEN
    0x00, 0xFF, 0xFF,   // CLR_CYAN
    0xFF, 0x00, 0x00,   // CLR_RED
    0xFF, 0x00, 0xFF,   // CLR_PINK
    0xFF, 0xFF, 0x00,   // CLR_YELLOW
    0xFE, 0xFE, 0xFE,   // CLR_OFFWHITE - can only use white at index 255

    0xC0, 0xC0, 0xC0,   // Gray (Windows)
    0xFF, 0xFB, 0xF0,   // Pale Yellow (Windows)
    0xC0, 0xDC, 0xC0,   // Pale Green (Windows)
    0xA4, 0xC8, 0xF0,   // Light Blue (Windows)
    0xA4, 0xA0, 0xA4,   // Medium Gray (Windows)

    0xFF, 0xFF, 0xE4    // Tooltip color - see nsLookAndFeel.cpp
};

NS_IMETHODIMP nsDeviceContextOS2::GetPaletteInfo(nsPaletteInfo& aPaletteInfo)
{
  static PRBool fPaletteInitialized = PR_FALSE;
  static ULONG aulTable[256];

  aPaletteInfo.isPaletteDevice = mPaletteInfo.isPaletteDevice;
  aPaletteInfo.sizePalette = mPaletteInfo.sizePalette;
  aPaletteInfo.numReserved = mPaletteInfo.numReserved;

  if ((mPaletteInfo.isPaletteDevice) && (NULL == mPaletteInfo.palette)) {
    if (!fPaletteInitialized) {
      IL_ColorSpace*  colorSpace;
      GetILColorSpace(colorSpace);

      // Create a logical palette
      ULONG ulCount;
  
      PRInt32 i,j;
      // system colors
      for (i = 0; i < NUM_SYS_COLORS; i++) {
        aulTable[i] = MK_RGB(sysColors[i].red, sysColors[i].green, sysColors[i].blue);
      }
  
      // Now set the color cube entries.
#ifdef XP_OS2
      NI_RGB*       map = colorSpace->cmap.map;
#else /* Combined with changes in GetILColor Space, this traps */
      NI_RGB*       map = colorSpace->cmap.map + 10;
#endif

      PRInt32 k = NUM_SYS_COLORS;
      for (i = 0; i < COLOR_CUBE_SIZE; i++, map++) {
        aulTable[k] = MK_RGB(map->red, map->green, map->blue);
        for (j = 0;j < NUM_SYS_COLORS; j++) {
           if (aulTable[k] == aulTable[j]) {
              aulTable[k] = 0;
              break;
           } /* endif */
        } /* endfor */
        if (j == NUM_SYS_COLORS) {
           k++;
        } /* endif */
      } /* endfor */
  
      ulCount = (k-1);
  
      // This overwrites the last entry in the cube (white)
      for (i=ulCount;i<256 ;i++ ) {
         aulTable[i] = MK_RGB(254,254,254);
      } /* endfor */
  
      aulTable[255] = MK_RGB(255, 255, 255); // Entry 255 must be white
      fPaletteInitialized = PR_TRUE;

      IL_ReleaseColorSpace(colorSpace);

#ifdef DEBUG
      for (i=0;i<256 ;i++ )
         printf("Entry[%d] in table is %x\n", i, aulTable[i]);
#endif

    } /* endif */
    // Create a GPI palette
    mPaletteInfo.palette =
      (void*)GFX (::GpiCreatePalette ((HAB)0, NULL,
                                      LCOLF_CONSECRGB, 256, aulTable),
                                      GPI_ERROR);
  } /* endif */

  aPaletteInfo.palette = mPaletteInfo.palette;
  return NS_OK;
}
#endif

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
  if(nsnull != aContext){
    NS_ADDREF(aContext);
  } else {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  ((nsDeviceContextOS2 *)aContext)->mSpec = aDevice;
  NS_ADDREF(aDevice);

  ((nsDeviceContextSpecOS2 *)aDevice)->GetPRTQUEUE(pq);

  HDC dc = PrnOpenDC(pq, "Mozilla");

  if (!dc) {
     PMERROR("DevOpenDC");
  } /* endif */

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
nsresult nsDeviceContextOS2::BeginDocument(PRUnichar * aTitle)
{
  nsresult rv = NS_OK;

  if (NULL != mPrintDC){
    nsString titleStr;
    titleStr = aTitle;
    char *title = GetACPString(titleStr);

    PSZ pszDocName = title != nsnull?title:"Mozilla Document";

    long lDummy = 0;
    long lResult = ::DevEscape(mPrintDC, DEVESC_STARTDOC,
                               strlen(pszDocName) + 1, pszDocName,
                               &lDummy, NULL);

    if (lResult == DEV_OK)
      rv = NS_OK;
    else
      rv = NS_ERROR_FAILURE;

    if (title != nsnull) {
      delete [] title;
    }
  }

  return rv;
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
      return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult nsDeviceContextOS2::BeginPage()
{
  return NS_OK;
}

nsresult nsDeviceContextOS2::EndPage()
{
  if (NULL != mPrintDC)
  {

    long lDummy = 0;
    long lResult = ::DevEscape(mPrintDC, DEVESC_NEWFRAME, 0, NULL,
                               &lDummy, NULL);

    if (lResult == DEV_OK)
      return NS_OK;
    else
      return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

char* 
nsDeviceContextOS2::GetACPString(const nsString& aStr)
{
   int acplen = aStr.Length() * 2 + 1;
   if (acplen == 1) {
      return nsnull;
   } /* endif */
   char * acp = new char[acplen];
   if(acp)
   {
      int outlen = ::WideCharToMultiByte( 0,
                      aStr.get(), aStr.Length(),
                      acp, acplen);
      if ( outlen > 0)
         acp[outlen] = '\0';  // null terminate
   }
   return acp;
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
