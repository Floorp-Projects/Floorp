/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s): Henry Sobotka <sobotka@axess.com> 2OOO/O2 update
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * ----             -----------     ---------------------------
 * 5/31/2000        IBM Corp.       Add method isPrintDC() to class nsDeviceContextOS2 to use in file
 *                                         nsRenderingContextOS2.cpp
 * 06/07/2000       IBM Corp.       Corrected querying of screen and client sizes
 */

// ToDo:
#include "nsGfxDefs.h"
#include "libprint.h"

#include "nsDeviceContextSpecOS2.h"
#include "nsDeviceContextOS2.h"
#include "nsDrawingSurfaceOS2.h"
#include "nsPaletteOS2.h"
#include "nsHashtable.h"
#include "nsString.h"
#include "nsFont.h"
#include "il_util.h"

// nsDeviceContextOS2 - The graphics state associated with a window.
// Keep the colour palette for all rendering contexts here; they share
// it and the widget can do palette-y things.
//
// Remember that this thing can be fairly long-lived, linked to a window.
//
// It's a bit schizophrenic - really there should be a nsIPrintDC <: nsIDC ...

// Init/Term -----------------------------------------------------------------

#define NOT_SETUP 0x33
static PRBool gIsWarp4 = NOT_SETUP;

nsDeviceContextOS2::nsDeviceContextOS2() : DeviceContextImpl()
{
   // Default for now
   mSurface = nsnull;
   mDepth = 0;
   mPaletteInfo.isPaletteDevice = PR_FALSE;
   mPaletteInfo.sizePalette = 0;
   mPaletteInfo.numReserved = 0;
   mPaletteInfo.palette = nsnull;
   mPalette = nsnull;
   mPixelsToTwips = 0;
   mTwipsToPixels = 0;
   mPixelScale = 1.0f;
   mWidth = -1;
   mHeight = -1;
   mPrintDC = nsnull;
   mSpec = nsnull;
   mCachedClientRect = PR_FALSE;
   mCachedFullRect = PR_FALSE;
   mPrintState = nsPrintState_ePreBeginDoc;

   // Init module if necessary (XXX find a better way of doing this!)
   if( !gModuleData.lDisplayDepth)
      gModuleData.Init();

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
   NS_IF_RELEASE( mSurface);
   NS_IF_RELEASE( mPalette);

   if( mPrintDC)
   {
      GpiAssociate( mPS, 0);
      GpiDestroyPS( mPS);
      PrnCloseDC( mPrintDC);
   }

   NS_IF_RELEASE(mSpec);
}

nsresult nsDeviceContextOS2::Init( nsNativeWidget aWidget)
{
   HWND hwnd = aWidget ? (HWND)aWidget : HWND_DESKTOP;
   HDC hdc = WinOpenWindowDC( hwnd);

   CommonInit( hdc);

   // Get size - don't know if this is needed for non-print DCs
   long lCaps[2];
   DevQueryCaps( hdc, CAPS_WIDTH, 2, lCaps);
   mWidth = lCaps[0];  // these are in device units...
   mHeight = lCaps[1];

   // Don't need to close HDCs obtained from WinOpenWindowDC

   return DeviceContextImpl::Init( aWidget);
}

// This version of Init() is called when creating a print DC
nsresult nsDeviceContextOS2::Init( nsNativeDeviceContext aContext,
                                   nsIDeviceContext *aOrigContext)
{
   // These calcs are from the windows version & I'm not 100%
   // sure what's going on...

   float origscale, newscale;
   float t2d, a2d;

   mPrintDC = (HDC)aContext; // we are print dc...

   // Create a print PS now.  This is necessary 'cos we need it from
   // odd places to do font-y things, where the only common reference
   // point is this DC.  We can't just create a new PS because only one
   // PS can be associated with a given DC, and we can't get that PS from
   // the DC (really?).  And it would be slow :-)
   SIZEL sizel = { 0 , 0 };
   mPS = GpiCreatePS( 0/*hab*/, mPrintDC, &sizel,
                      PU_PELS | GPIT_MICRO | GPIA_ASSOC);

   CommonInit( mPrintDC);

   GetTwipsToDevUnits( newscale);
   aOrigContext->GetTwipsToDevUnits( origscale);

   mPixelScale = newscale / origscale;

   aOrigContext->GetTwipsToDevUnits( t2d);
   aOrigContext->GetAppUnitsToDevUnits( a2d);

   mAppUnitsToDevUnits = (a2d / t2d) * mTwipsToPixels;
   mDevUnitsToAppUnits = 1.0f / mAppUnitsToDevUnits;

   HCINFO hcinfo;
   PrnQueryHardcopyCaps( mPrintDC, &hcinfo);
   mWidth = hcinfo.xPels;
   mHeight = hcinfo.yPels;
   // XXX hsb says there are margin problems, must be from here...
   printf( "Got surface of size %d x %d pixels\n", mWidth, mHeight);
   printf( "mPixelScale = %f\n", mPixelScale);

   // We need to begin a document now, because the client is entitled at
   // this point to do stuff like create fonts, which required the PS to
   // be associated with a DC which has been DEVESC_STARTDOC'd.
   BeginDocument();

   return NS_OK;
}

// XXXX Reduce to one call to DevQueryCaps???
void nsDeviceContextOS2::CommonInit( HDC aDC)
{
   // Record palette-potential of device even if it is >8bpp
   long lCap = 0;
   DevQueryCaps( aDC, CAPS_ADDITIONAL_GRAPHICS, 1, &lCap);
   mPaletteInfo.isPaletteDevice = !!(lCap & CAPS_PALETTE_MANAGER);

   // Work out the pels-to-twips conversion
   DevQueryCaps( aDC, CAPS_VERTICAL_FONT_RES, 1, &lCap);
   mTwipsToPixels = ((float) lCap) / (float)NSIntPointsToTwips(72);
   mPixelsToTwips = 1.0f / mTwipsToPixels;

   // max palette size: level out at COLOR_CUBE_SIZE
   // (or CAPS_COLORS ?)
   DevQueryCaps( aDC, CAPS_PHYS_COLORS, 1, &lCap);
   mPaletteInfo.sizePalette = (PRUint8) (min( lCap, COLOR_CUBE_SIZE));

   // erm?
   mPaletteInfo.numReserved = 0;

   DevQueryCaps( aDC, CAPS_COLOR_BITCOUNT, 1, &lCap);
   mDepth = lCap;

   mClientRect.x = mClientRect.y = 0;
   DevQueryCaps( aDC, CAPS_HORIZONTAL_RESOLUTION, 1, &lCap);
   mClientRect.width = lCap;
   DevQueryCaps( aDC, CAPS_VERTICAL_RESOLUTION, 1, &lCap);
   mClientRect.height = lCap;

   DevQueryCaps( aDC, CAPS_TECHNOLOGY, 1, &lCap);
   if (lCap & CAPS_TECH_RASTER_DISPLAY) {
     mClientRect.width = WinQuerySysValue(HWND_DESKTOP, SV_CXFULLSCREEN);
     mClientRect.height = WinQuerySysValue(HWND_DESKTOP, SV_CYFULLSCREEN);
   }

   DeviceContextImpl::CommonInit();
}


void
nsDeviceContextOS2::ComputeClientRectUsingScreen ( nsRect* outRect )
{
  if ( !mCachedClientRect ) {
    // convert to device units
    outRect->y = NSToIntRound(mClientRect.y * mDevUnitsToAppUnits);
    outRect->x = NSToIntRound(mClientRect.x * mDevUnitsToAppUnits);
    outRect->width = NSToIntRound(mClientRect.width * mDevUnitsToAppUnits);
    outRect->height = NSToIntRound(mClientRect.height * mDevUnitsToAppUnits);

    mCachedClientRect = PR_TRUE;
    mClientRect = *outRect;
  } // if we need to recompute the client rect
  else
    *outRect = mClientRect;

} // ComputeClientRectUsingScreen


void
nsDeviceContextOS2::ComputeFullAreaUsingScreen ( nsRect* outRect )
{
  outRect->y = 0;
  outRect->x = 0;
  outRect->width = NSToIntRound(mWidth * mDevUnitsToAppUnits);
  outRect->height = NSToIntRound(mHeight * mDevUnitsToAppUnits);

} // ComputeFullRectUsingScreen


// Creation of other gfx objects malarky -------------------------------------

nsresult nsDeviceContextOS2::GetDrawingSurface( nsIRenderingContext &aContext, nsDrawingSurface &aSurface)
{
   if( !mSurface)
   {
      nsDrawingSurface tSurface;
      aContext.CreateDrawingSurface( nsnull, 0, tSurface);
      mSurface = (nsDrawingSurfaceOS2 *) tSurface;
      NS_ADDREF(mSurface);
   }

   printf("Dodgy nsDeviceContext::GetDrawingContext() called\n");

   aSurface = mSurface;
   return NS_OK;
}

nsresult nsDeviceContextOS2::GetDeviceContextFor( nsIDeviceContextSpec *aDevice,
                                                  nsIDeviceContext *&aContext)
{
   // Prolly want to clean this up xpCom-wise...

   nsDeviceContextOS2 *newCX = new nsDeviceContextOS2;
   nsDeviceContextSpecOS2 *spec = (nsDeviceContextSpecOS2*) aDevice;

   if (!newCX)
     return NS_ERROR_OUT_OF_MEMORY;

   aContext = newCX;
   NS_ADDREF(aContext);

   PRTQUEUE *pq;
   spec->GetPRTQUEUE( pq);

   HDC hdcPrint = PrnOpenDC( pq, "Mozilla");

   return newCX->Init( (nsNativeDeviceContext) hdcPrint, this);
}

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

HPS nsDeviceContextOS2::GetRepresentativePS() const
{
   HPS hps;

   if( mPrintDC == 0)
   {
      HWND hwnd = mWidget ? (HWND)mWidget : HWND_DESKTOP;
      hps = WinGetPS( hwnd);
   }
   else
      hps = mPS;

   return hps;
}

void nsDeviceContextOS2::ReleaseRepresentativePS( HPS aPS)
{
   if( mPrintDC == 0)
      if( !WinReleasePS( aPS))
         PMERROR( "WinReleasePS (DC)");
   else
      /* nop */ ;
}

// Metrics -------------------------------------------------------------------

// Note that this returns values in app units, as opposed to GetSystemAttr(),
// which uses device units.
nsresult nsDeviceContextOS2::GetScrollBarDimensions( float &aWidth, float &aHeight) const
{
  aWidth = (float) WinQuerySysValue( HWND_DESKTOP, SV_CXVSCROLL) *
                   mDevUnitsToAppUnits;
  aHeight = (float) WinQuerySysValue( HWND_DESKTOP, SV_CYHSCROLL) *
                    mDevUnitsToAppUnits;
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
      PrfQueryProfileString(HINI_USER, "PM_SystemFonts", "WindowText", gIsWarp4 ? "9.WarpSans" : "8.Helv",
                            szFontNameSize, MAXNAMEL);
      break;

    case eSystemAttr_Font_Icon: 
      PrfQueryProfileString(HINI_USER, "PM_SystemFonts", "IconText", gIsWarp4 ? "9.WarpSans" : "8.Helv",
                            szFontNameSize, MAXNAMEL);
      break;

    case eSystemAttr_Font_Menu: 
      PrfQueryProfileString(HINI_USER, "PM_SystemFonts", "Menus", gIsWarp4 ? "9.WarpSans Bold" : "10.Helv",
                            szFontNameSize, MAXNAMEL);
      break;

    case eSystemAttr_Font_MessageBox: 
      PrfQueryProfileString(HINI_USER, "PM_SystemFonts", "WindowText", gIsWarp4 ? "9.WarpSans" : "8.Helv",
                            szFontNameSize, MAXNAMEL);
      break;

    case eSystemAttr_Font_SmallCaption: 
      PrfQueryProfileString(HINI_USER, "PM_SystemFonts", "WindowText", gIsWarp4 ? "9.WarpSans" : "8.Helv",
                            szFontNameSize, MAXNAMEL);
      break;

    case eSystemAttr_Font_StatusBar: 
    case eSystemAttr_Font_Tooltips: 
      PrfQueryProfileString(HINI_USER, "PM_SystemFonts", "WindowText", gIsWarp4 ? "9.WarpSans" : "8.Helv",
                            szFontNameSize, MAXNAMEL);
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
      PrfQueryProfileString(HINI_USER, "PM_SystemFonts", "WindowText", gIsWarp4 ? "9.WarpSans" : "8.Helv",
                            szFontNameSize, MAXNAMEL);
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

//
// (XXX) the screen object thinks these are in pels, duh. (jmf)
//
nsresult nsDeviceContextOS2::GetDeviceSurfaceDimensions( PRInt32 &aWidth, PRInt32 &aHeight)
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

NS_IMETHODIMP nsDeviceContextOS2::GetRect(nsRect &aRect)
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

nsresult nsDeviceContextOS2::GetClientRect(nsRect &aRect)
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

nsresult nsDeviceContextOS2::GetDepth( PRUint32& aDepth)
{
   aDepth = mDepth;
   return NS_OK;
}

// Yuck...
nsresult nsDeviceContextOS2::SupportsNativeWidgets( PRBool &aSupportsWidgets)
{
   aSupportsWidgets = (mPrintDC == 0) ? PR_TRUE : PR_FALSE;
   return NS_OK;
}

nsresult nsDeviceContextOS2::GetCanonicalPixelScale( float &aScale) const
{
   aScale = mPixelScale;
   return NS_OK;
}

// Fonts ---------------------------------------------------------------------

// Rather unfortunate place for this method...
nsresult nsDeviceContextOS2::CheckFontExistence( const nsString &aFontName)
{
   HPS hps = GetRepresentativePS();

   char fontName[ FACESIZE];
   aFontName.ToCString( fontName, FACESIZE);

   long lWant = 0;
   long lFonts = GpiQueryFonts( hps, QF_PUBLIC | QF_PRIVATE,
                                fontName, &lWant, 0, 0);

   ReleaseRepresentativePS( hps);

   return lFonts > 0 ? NS_OK : NS_ERROR_FAILURE;
}


// Font setup - defaults aren't terribly helpful; this may be no better!
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

// Colourspaces and palettes -------------------------------------------------

// Override these so we can use palette manager
nsresult nsDeviceContextOS2::GetILColorSpace( IL_ColorSpace *&aColorSpace)
{
   if( !mColorSpace)
   {
      // !! Might want to do something funky for 4bpp displays

      // See if we're dealing with an 8-bit palette device
      if( (8 == mDepth) && mPaletteInfo.isPaletteDevice)
      {
         IL_ColorMap *cMap = IL_NewCubeColorMap( 0, 0, COLOR_CUBE_SIZE);
         if( !cMap)
            return NS_ERROR_OUT_OF_MEMORY;

         // Create a pseudo color space
         mColorSpace = IL_CreatePseudoColorSpace( cMap, 8, 8);
      }
      else
      {
         IL_RGBBits colorRGBBits;

         // Create a 24-bit color space
         colorRGBBits.red_shift   = 16;
         colorRGBBits.red_bits    = 8;
         colorRGBBits.green_shift = 8;
         colorRGBBits.green_bits  = 8;
         colorRGBBits.blue_shift  = 0;
         colorRGBBits.blue_bits   = 8;

         mColorSpace = IL_CreateTrueColorSpace(&colorRGBBits, 24);
      }

      if( !mColorSpace)
      {
         aColorSpace = nsnull;
         return NS_ERROR_OUT_OF_MEMORY;
      }
   }

   // Return the color space
   aColorSpace = mColorSpace;
   IL_AddRefToColorSpace( aColorSpace);
   return NS_OK;
}

nsresult nsDeviceContextOS2::GetPaletteInfo( nsPaletteInfo& aPaletteInfo)
{
   aPaletteInfo.isPaletteDevice = mPaletteInfo.isPaletteDevice;
   aPaletteInfo.sizePalette = mPaletteInfo.sizePalette;
   aPaletteInfo.numReserved = mPaletteInfo.numReserved;

   nsresult rc = NS_OK;

   SetupColorMaps();

   aPaletteInfo.palette = mPaletteInfo.palette;
   return rc;
}

nsresult nsDeviceContextOS2::GetPalette( nsIPaletteOS2 *&aPalette)
{
   nsresult rc = SetupColorMaps();
   aPalette = mPalette;
   NS_ADDREF( aPalette);
   return rc;
}

nsresult nsDeviceContextOS2::SetupColorMaps()
{
   nsresult rc = NS_OK;
   if( !mPalette)
   {
      // Share a single palette between all screen instances; printers need
      // a separate palette for compatability reasons.
      if( mPrintDC) rc = NS_CreatePalette( this, mPalette);
      else mPalette = gModuleData.GetUIPalette( this);

      if( rc == NS_OK)
         rc = mPalette->GetNSPalette( mPaletteInfo.palette);
   }
   return rc;
}

NS_IMETHODIMP nsDeviceContextOS2::ConvertPixel(nscolor aColor, PRUint32 & aPixel)
{
   printf( "Alert: nsDeviceContext::ConvertPixel called\n");
   aPixel = aColor;
   return NS_OK;
}

// Printing ------------------------------------------------------------------
nsresult nsDeviceContextOS2::BeginDocument()
{
   NS_ASSERTION(mPrintDC, "BeginDocument for non-print DC");
   if( mPrintState == nsPrintState_ePreBeginDoc)
   {
      PrnStartJob( mPrintDC, "Warpzilla NGLayout job");
      printf( "BeginDoc\n");
      mPrintState = nsPrintState_eBegunDoc;
   }
   return NS_OK;
}

nsresult nsDeviceContextOS2::EndDocument()
{
   PrnEndJob( mPrintDC);
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
      PrnNewPage( mPrintDC);
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
   if ( mPrintDC == nsnull )
      return 0;

   else
      return 1;
}
