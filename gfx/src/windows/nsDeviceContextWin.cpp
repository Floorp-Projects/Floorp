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
  if (NULL != mSurface)
  {
    // XXX We're leaking the HBITMAP...
    DeleteDC(mSurface);
    mSurface = NULL;
  }
}

nsresult nsDeviceContextWin::Init(nsNativeWidget aWidget)
{
  HWND      hwnd = (HWND)aWidget;
  HDC       hdc = ::GetDC(hwnd);

  mDepth = (PRUint32)::GetDeviceCaps(hdc, BITSPIXEL);
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

NS_IMETHODIMP nsDeviceContextWin::CreateILColorSpace(IL_ColorSpace*& aColorSpace)
{
  HWND      hwnd = (HWND)GetNativeWidget();
  HDC       hdc = ::GetDC(hwnd);
  nsresult  result = NS_OK;
  int       rasterCaps = ::GetDeviceCaps(hdc, RASTERCAPS);
  ::ReleaseDC(hwnd, hdc);

  // See if we're dealing with an 8-bit palette device
  if ((8 == mDepth) && (rasterCaps & RC_PALETTE)) {
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

    // Create a physical palette if we haven't already done so. Each HDC can
    // share the same physical palette
    //
    // XXX Device context should own the palette...
    if (nsnull == nsRenderingContextWin::gPalette) {
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
      NI_RGB*       map = colorMap->map + 10;
      for (PRInt32 i = 0; i < COLOR_CUBE_SIZE; i++) {
        entry->peRed = map->red;
        entry->peGreen = map->green;
        entry->peBlue = map->blue; 
        entry->peFlags = 0;
  
        entry++;
        map++;
      }
  
      // Create a GDI palette
      nsRenderingContextWin::gPalette = ::CreatePalette(logPal);
    }

    // Create an IL pseudo color space
    aColorSpace = IL_CreatePseudoColorSpace(colorMap, 8, 8);
    if (nsnull == aColorSpace) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

  } else {
    // Create a default color space.
    result = DeviceContextImpl::CreateILColorSpace(aColorSpace);
  }

  return result;
}

