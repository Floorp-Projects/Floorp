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

#include "nsDeviceContextWin.h"
#include "nsRenderingContextWin.h"
#include "il_util.h"

// Size of the color cube
#define COLOR_CUBE_SIZE       216

HPALETTE  nsDeviceContextWin::gPalette = NULL;

nsDeviceContextWin :: nsDeviceContextWin()
  : DeviceContextImpl()
{
  HDC hdc = ::GetDC(NULL);

  mTwipsToPixels = ((float)::GetDeviceCaps(hdc, LOGPIXELSX)) / (float)NSIntPointsToTwips(72); // XXX shouldn't be LOGPIXELSY ??
  mPixelsToTwips = 1.0f / mTwipsToPixels;

  ::ReleaseDC(NULL, hdc);

  mSurface = NULL;
}

nsDeviceContextWin :: ~nsDeviceContextWin()
{
  nsDrawingSurfaceWin *surf = (nsDrawingSurfaceWin *)mSurface;

  NS_IF_RELEASE(surf);    //this clears the surf pointer...
  mSurface = nsnull;
  mIsPaletteDevice = PR_FALSE;
}

nsresult nsDeviceContextWin::Init(nsNativeWidget aWidget)
{
  HWND  hwnd = (HWND)aWidget;
  HDC   hdc = ::GetDC(hwnd);
  int   rasterCaps = ::GetDeviceCaps(hdc, RASTERCAPS);

  mDepth = (PRUint32)::GetDeviceCaps(hdc, BITSPIXEL);
  mIsPaletteDevice = RC_PALETTE == (rasterCaps & RC_PALETTE);
  ::ReleaseDC(hwnd, hdc);

  return DeviceContextImpl::Init(aWidget);
}

float nsDeviceContextWin :: GetScrollBarWidth() const
{
  return ::GetSystemMetrics(SM_CXVSCROLL) * mDevUnitsToAppUnits;
}

float nsDeviceContextWin :: GetScrollBarHeight() const
{
  return ::GetSystemMetrics(SM_CXHSCROLL) * mDevUnitsToAppUnits;
}

nsDrawingSurface nsDeviceContextWin :: GetDrawingSurface(nsIRenderingContext &aContext)
{
  if (NULL == mSurface)
    mSurface = aContext.CreateDrawingSurface(nsnull);

  return mSurface;
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
  HWND    hwnd = (HWND)GetNativeWidget();
  HDC     hdc = ::GetDC(hwnd);
  PRBool  isthere = PR_FALSE;

  char    fontName[LF_FACESIZE];
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

NS_IMETHODIMP nsDeviceContextWin::GetILColorSpace(IL_ColorSpace*& aColorSpace)
{
  if (nsnull == mColorSpace) {
    // See if we're dealing with an 8-bit palette device
    if ((8 == mDepth) && mIsPaletteDevice) {
      // Create a color cube. We want to use DIB_PAL_COLORS because it's faster
      // than DIB_RGB_COLORS, so make sure the indexes match that of the
      // GDI physical palette
      //
      // Note: the image library doesn't use the reserved colors, so it doesn't
      // matter what they're set to...
      IL_RGB  reserved[10];
      memset(reserved, 0, sizeof(reserved));
      IL_ColorMap* colorMap = IL_NewCubeColorMap(reserved, 10, COLOR_CUBE_SIZE + 10);
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

HPALETTE nsDeviceContextWin::GetLogicalPalette()
{
  if (NULL == gPalette) {
    IL_ColorSpace*  colorSpace;
    GetILColorSpace(colorSpace);

    if (NI_PseudoColor == colorSpace->type) {
      // Create a logical palette
      BYTE         tmp[sizeof(LOGPALETTE) + ((COLOR_CUBE_SIZE + 20) * sizeof(PALETTEENTRY))];
      LPLOGPALETTE logPal = (LPLOGPALETTE)tmp;

      logPal->palVersion = 0x300;
      logPal->palNumEntries = COLOR_CUBE_SIZE + 20;
  
      // Initialize it from the default Windows palette
      HPALETTE  hDefaultPalette = ::GetStockObject(DEFAULT_PALETTE);
  
      // First ten system colors
      ::GetPaletteEntries(hDefaultPalette, 0, 10, logPal->palPalEntry);

      // Last ten system colors
      ::GetPaletteEntries(hDefaultPalette, 10, 10, &logPal->palPalEntry[COLOR_CUBE_SIZE + 10]);
  
      // Now set the color cube entries
      PALETTEENTRY* entry = &logPal->palPalEntry[10];
      NI_RGB*       map = colorSpace->cmap.map + 10;
      for (PRInt32 i = 0; i < COLOR_CUBE_SIZE; i++) {
        entry->peRed = map->red;
        entry->peGreen = map->green;
        entry->peBlue = map->blue; 
        entry->peFlags = 0;
  
        entry++;
        map++;
      }
  
      // Create a GDI palette
      gPalette = ::CreatePalette(logPal);
    }

    IL_ReleaseColorSpace(colorSpace);
  }

  return gPalette;
}
