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
 * Contributor(s): 
 *
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
   mWidth = 0;
   mHeight = 0;
   mDC = 0;
   mPrintState = nsPrintState_ePreBeginDoc;

   // Init module if necessary (XXX find a better way of doing this!)
   if( !gModuleData.lDisplayDepth)
      gModuleData.Init();
}

nsDeviceContextOS2::~nsDeviceContextOS2()
{
   NS_IF_RELEASE( mSurface);
   NS_IF_RELEASE( mPalette);

   if( mDC)
   {
      GpiAssociate( mPS, 0);
      GpiDestroyPS( mPS);
      PrnCloseDC( mDC);
   }
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
 
   mDC = (HDC)aContext; // we are print dc...

   // Create a print PS now.  This is necessary 'cos we need it from
   // odd places to do font-y things, where the only common reference
   // point is this DC.  We can't just create a new PS because only one
   // PS can be associated with a given DC, and we can't get that PS from
   // the DC (really?).  And it would be slow :-)
   SIZEL sizel = { 0 , 0 };
   mPS = GpiCreatePS( 0/*hab*/, mDC, &sizel,
                      PU_PELS | GPIT_MICRO | GPIA_ASSOC);
 
   CommonInit( mDC);
   // yech: we can't call nsDeviceContextImpl::Init() 'cos no widget.
   DeviceContextImpl::CommonInit(); 
 
   GetTwipsToDevUnits( newscale);
   aOrigContext->GetTwipsToDevUnits( origscale);
 
   mPixelScale = newscale / origscale;
 
   aOrigContext->GetTwipsToDevUnits( t2d);
   aOrigContext->GetAppUnitsToDevUnits( a2d);
 
   mAppUnitsToDevUnits = (a2d / t2d) * mTwipsToPixels;
   mDevUnitsToAppUnits = 1.0f / mAppUnitsToDevUnits;

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
   
   return NS_OK;
}

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
}

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
   NS_ASSERTION( mDC, "CreateRenderingContext for non-print DC");

   nsIRenderingContext *pContext = new nsRenderingContextOS2;
   NS_ADDREF(pContext);

   nsPrintSurface *surf = new nsPrintSurface;
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

   if( mDC == 0)
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
   if( mDC == 0)
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

// Utility function to query a logical font, from the ini file if possible
// but with os-dependent defaults.  If there's no appropriate inikey, just
// supply a null for that parameter.
static void FindSystemFont( nsFont *aFont, const char *aIniKey,
                            const char *aWarp3Default,
                            const char *aWarp4Default)
{
   static int bIsWarp4 = -1; // XXX MT
   char fontNameSize[100];

   NS_ASSERTION( aWarp3Default && aWarp4Default, "Bad params to FindSystemFont");
   if( !aWarp3Default || !aWarp4Default) return;

   if( bIsWarp4 == -1)
   {
      ULONG ulValues[2];
      DosQuerySysInfo( QSV_VERSION_MAJOR, QSV_VERSION_MINOR,
                       ulValues, sizeof ulValues);
      bIsWarp4 = (ulValues[0] >= 20) && (ulValues[1] >= 40);
   }

   if( aIniKey)
      PrfQueryProfileString( HINI_USERPROFILE, "PM_SystemFonts", aIniKey,
                             bIsWarp4 ? aWarp4Default : aWarp3Default,
                             fontNameSize, 100);
   else
      strcpy( fontNameSize, bIsWarp4 ? aWarp4Default : aWarp3Default);

   // Parse font description into face and pointage
   int iPoints = 0;
   char facename[ 100];
   if( 2 == sscanf( fontNameSize, "%d.%s", &iPoints, facename))
   {
      // XXX This isn't quite `right', but I can't be bothered to go
      // XXX through the rigamarole of working out precisely what's going
      // XXX on.  Just ensure that the right thing happens when this font
      // XXX gets passed to nsFontMetricsOS2 or nsWindow.
      aFont->name = facename;
      aFont->style = NS_FONT_STYLE_NORMAL;
      aFont->variant = NS_FONT_VARIANT_NORMAL;
      aFont->weight = NS_FONT_WEIGHT_NORMAL;
      aFont->decorations = NS_FONT_DECORATION_NONE;

      // XXX Windows sets the size in points...
#if 1
      aFont->size = iPoints;
#else
      // XXX ..but it should probably be in app units
      float twip2dev, twip2app;
      nscoord nsTwips = NSIntPointsToTwips( iPoints);
      GetDevUnitsToAppUnits( twip2app);
      GetTwipsToDevUnits( twip2dev);
      twip2app *= twip2dev;

      aFont->size = NSToCoordFloor( nsTwips * twip2app);
#endif
   }
   else
      NS_ASSERTION( 0, "Malformed fontnamesize string");
}

nsresult nsDeviceContextOS2::GetSystemAttribute( nsSystemAttrID anID,
                                                 SystemAttrStruct *aInfo) const
{
   int sysclr = 0, sysval = 0;

   switch( anID)
   {
      // Colors
      // !! Any of these could be wrong...
      case eSystemAttr_Color_WindowBackground:
         sysclr = SYSCLR_BACKGROUND;
         break;
      case eSystemAttr_Color_WindowForeground:
         sysclr = SYSCLR_WINDOWTEXT;
         break;
      case eSystemAttr_Color_WidgetBackground:
         sysclr = SYSCLR_BUTTONMIDDLE;
         break;
      case eSystemAttr_Color_WidgetForeground:
         sysclr = SYSCLR_WINDOWTEXT;
         break;
      case eSystemAttr_Color_WidgetSelectBackground:
         sysclr = SYSCLR_HILITEBACKGROUND;
         break;
      case eSystemAttr_Color_WidgetSelectForeground:
         sysclr = SYSCLR_HILITEFOREGROUND;
         break;
      case eSystemAttr_Color_Widget3DHighlight:
         sysclr = SYSCLR_BUTTONLIGHT;
         break;
      case eSystemAttr_Color_Widget3DShadow:
         sysclr = SYSCLR_BUTTONDARK;
         break;
      case eSystemAttr_Color_TextBackground:
         sysclr = SYSCLR_ENTRYFIELD;
         break;
      case eSystemAttr_Color_TextForeground:
         sysclr = SYSCLR_WINDOWTEXT;
         break;
      case eSystemAttr_Color_TextSelectBackground:
         sysclr = SYSCLR_HILITEBACKGROUND;
         break;
      case eSystemAttr_Color_TextSelectForeground:
         sysclr = SYSCLR_HILITEFOREGROUND;
         break;
      // Size (these seem to be required in device units)
      // !! These could well be wrong & need to be arbitrarily changed...
      case eSystemAttr_Size_WindowTitleHeight:
         sysval = SV_CYTITLEBAR;
         break;
      case eSystemAttr_Size_WindowBorderWidth:
         sysval = SV_CXSIZEBORDER;
         break;
      case eSystemAttr_Size_WindowBorderHeight:
         sysval = SV_CYSIZEBORDER;
         break;
      case eSystemAttr_Size_Widget3DBorder:
         sysval = SV_CXBORDER;
         break;
      case eSystemAttr_Size_ScrollbarHeight:
         sysval = SV_CYHSCROLL;
         break;
      case eSystemAttr_Size_ScrollbarWidth:
         sysval = SV_CXVSCROLL;
         break;
      // Fonts
      //
      // !! Again, these are up for grabs & spec-less.
      // !! If it looks crazy, change it here.
      //
      case eSystemAttr_Font_Caption:
      case eSystemAttr_Font_SmallCaption:
         FindSystemFont( aInfo->mFont, "WindowTitles",
                        "10.Helv", "9.WarpSans Bold");
         break;
      case eSystemAttr_Font_Icon:
         FindSystemFont( aInfo->mFont, "IconText",
                         "8.Helv", "9.WarpSans");
         break;
      case eSystemAttr_Font_Menu:
         FindSystemFont( aInfo->mFont, "Menus",
                         "10.Helv", "9.WarpSans Bold");
         break;
      case eSystemAttr_Font_MessageBox:
      case eSystemAttr_Font_StatusBar:
      case eSystemAttr_Font_Tooltips:
      case eSystemAttr_Font_Widget:
         FindSystemFont( aInfo->mFont, "WindowText",
                         "8.Helv", "9.Warpsans");
         break;
      // don't want to be here
      default: 
         NS_ASSERTION(0,"Bad eSystemAttr value");
         break;
   }

   // Load colour if appropriate
   if( sysclr != 0)
   {
      long lColor = WinQuerySysColor( HWND_DESKTOP, sysclr, 0);
   
      RGB2 *pRGB2 = (RGB2*) &lColor;
   
      *(aInfo->mColor) = NS_RGB( pRGB2->bRed, pRGB2->bGreen, pRGB2->bBlue);
   }
   else if( sysval != 0)
   {
      aInfo->mSize = WinQuerySysValue( HWND_DESKTOP, sysval);
   }

   return NS_OK;
}

//
// (XXX) the screen object thinks these are in pels, duh.
//
nsresult nsDeviceContextOS2::GetDeviceSurfaceDimensions( PRInt32 &aWidth, PRInt32 &aHeight)
{
   aWidth = NSToIntRound( mWidth * mDevUnitsToAppUnits);
   aHeight = NSToIntRound( mHeight * mDevUnitsToAppUnits);

   return NS_OK;
}

nsresult nsDeviceContextOS2::GetClientRect(nsRect &aRect)
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

nsresult nsDeviceContextOS2::GetDepth( PRUint32& aDepth)
{
   aDepth = mDepth;
   return NS_OK;
}

// Yuck...
nsresult nsDeviceContextOS2::SupportsNativeWidgets( PRBool &aSupportsWidgets)
{
   aSupportsWidgets = (mDC == 0) ? PR_TRUE : PR_FALSE;
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

      AliasFont( "Times", "Tms Rmn", "Times New Roman", PR_FALSE);
      AliasFont( "Times Roman", "Tms Rmn", "Times New Roman", PR_FALSE);
      AliasFont( "Arial", "Helv", "Helvetica", PR_FALSE);
      AliasFont( "Helvetica", "Helv", "Arial", PR_FALSE);
      AliasFont( "Courier", "Courier New", "", PR_FALSE); // why does base force this alias?
      AliasFont( "Courier New", "Courier", "", PR_FALSE);
      AliasFont( "Sans", "Helv", "Arial", PR_FALSE);
      // Is this right?
      AliasFont( "Unicode", "Times New Roman MT 30", "", PR_FALSE);
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
      if( mDC) rc = NS_CreatePalette( this, mPalette);
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
