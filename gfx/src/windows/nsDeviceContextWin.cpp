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
#include "nsDeviceContextSpecWin.h"
#include "il_util.h"

// Size of the color cube
#define COLOR_CUBE_SIZE       216

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
  mWidthFloat = 0.0f;
  mHeightFloat = 0.0f;
  mWidth = -1;
  mHeight = -1;
  mSpec = nsnull;
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
}

NS_IMETHODIMP nsDeviceContextWin :: Init(nsNativeWidget aWidget)
{
  HWND  hwnd = (HWND)aWidget;
  HDC   hdc = ::GetDC(hwnd);

  CommonInit(hdc);

  ::ReleaseDC(hwnd, hdc);

  return DeviceContextImpl::Init(aWidget);
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
  mPaletteInfo.sizePalette = (PRUint8)::GetDeviceCaps(aDC, SIZEPALETTE);
  mPaletteInfo.numReserved = (PRUint8)::GetDeviceCaps(aDC, NUMRESERVED);

  mClientRect.width = ::GetDeviceCaps(aDC, HORZRES);
  mClientRect.height = ::GetDeviceCaps(aDC, VERTRES);
  mWidthFloat = (float)mClientRect.width;
  mHeightFloat = (float)mClientRect.height;
  if (::GetDeviceCaps(aDC, TECHNOLOGY) == DT_RASDISPLAY)
  {
    RECT workArea;
    ::SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
    mClientRect.x = workArea.left;
    mClientRect.y = workArea.top;
    mClientRect.width = workArea.right - workArea.left;
    mClientRect.height = workArea.bottom - workArea.top;
  }

  DeviceContextImpl::CommonInit();
}

NS_IMETHODIMP nsDeviceContextWin :: CreateRenderingContext(nsIRenderingContext *&aContext)
{
  nsIRenderingContext *pContext;
  nsresult             rv;
  nsDrawingSurfaceWin  *surf;

  pContext = new nsRenderingContextWin();

  if (nsnull != pContext)
  {
    NS_ADDREF(pContext);

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

nsresult GetSysFontInfo(HDC aHDC, nsSystemAttrID anID, nsFont * aFont) 
{
  NONCLIENTMETRICS ncm;

  memset(&ncm, sizeof(NONCLIENTMETRICS), 0);
  ncm.cbSize = sizeof(NONCLIENTMETRICS);

  BOOL status = SystemParametersInfo(SPI_GETNONCLIENTMETRICS, 
                                     sizeof(NONCLIENTMETRICS),  
                                     (PVOID)&ncm, 
                                     0);
  if (!status) {
    return NS_ERROR_FAILURE;
  }

  LOGFONT * logFont = NULL;
  switch (anID) {
    case eSystemAttr_Font_Caption : 
      logFont = &ncm.lfCaptionFont;
      break;

    case eSystemAttr_Font_Icon : 
      logFont = &ncm.lfSmCaptionFont; // XXX this may not be right
      break;

    case eSystemAttr_Font_Menu : 
      logFont = &ncm.lfMenuFont;
      break;

    case eSystemAttr_Font_MessageBox : 
      logFont = &ncm.lfMessageFont; // XXX this may not be right
      break;

    case eSystemAttr_Font_SmallCaption : 
      logFont = &ncm.lfSmCaptionFont; // XXX this may not be right
      break;

    case eSystemAttr_Font_StatusBar : 
    case eSystemAttr_Font_Tooltips : 
      logFont = &ncm.lfStatusFont;
      break;

    case eSystemAttr_Font_Widget:
      break;
  } // switch 

  if (nsnull == logFont) {
    return NS_ERROR_FAILURE;
  }

  
  aFont->name = logFont->lfFaceName;

  // Do Style
  aFont->style = NS_FONT_STYLE_NORMAL;
  if (logFont->lfItalic) {
    aFont->decorations &= NS_FONT_STYLE_ITALIC;
  }
  // XXX What about oblique?


  aFont->variant = 0;

  // Do Weight
  aFont->weight = (logFont->lfWeight == FW_BOLD?NS_FONT_WEIGHT_BOLD:NS_FONT_WEIGHT_NORMAL);

  // Do decorations
  aFont->decorations = NS_FONT_DECORATION_NONE;
  if (logFont->lfUnderline) {
    aFont->decorations &= NS_FONT_DECORATION_UNDERLINE;
  }
  if (logFont->lfStrikeOut) {
    aFont->decorations &= NS_FONT_DECORATION_LINE_THROUGH;
  }

  // Do Size
  aFont->size = logFont->lfHeight * -1;

  return NS_OK;

}

NS_IMETHODIMP nsDeviceContextWin :: GetSystemAttribute(nsSystemAttrID anID, SystemAttrStruct * aInfo) const
{
  nsresult status = NS_OK;

  switch (anID) {
    //---------
    // Colors
    //---------
    case eSystemAttr_Color_WindowBackground:
        *aInfo->mColor = ::GetSysColor(COLOR_WINDOW);
        break;
    case eSystemAttr_Color_WindowForeground:
        *aInfo->mColor = ::GetSysColor(COLOR_WINDOWTEXT);
        break;
    case eSystemAttr_Color_WidgetBackground:
        *aInfo->mColor = ::GetSysColor(COLOR_BTNFACE);
        break;
    case eSystemAttr_Color_WidgetForeground:
        *aInfo->mColor = ::GetSysColor(COLOR_BTNTEXT);
        break;
    case eSystemAttr_Color_WidgetSelectBackground:
        *aInfo->mColor = ::GetSysColor(COLOR_HIGHLIGHT);
        break;
    case eSystemAttr_Color_WidgetSelectForeground:
        *aInfo->mColor = ::GetSysColor(COLOR_HIGHLIGHTTEXT);
        break;
    case eSystemAttr_Color_Widget3DHighlight:
        *aInfo->mColor = ::GetSysColor(COLOR_BTNHIGHLIGHT);
        break;
    case eSystemAttr_Color_Widget3DShadow:
        *aInfo->mColor = ::GetSysColor(COLOR_BTNSHADOW);
        break;
    case eSystemAttr_Color_TextBackground:
        *aInfo->mColor = ::GetSysColor(COLOR_WINDOW);
        break;
    case eSystemAttr_Color_TextForeground:
        *aInfo->mColor = ::GetSysColor(COLOR_WINDOWTEXT);
        break;
    case eSystemAttr_Color_TextSelectBackground:
        *aInfo->mColor = ::GetSysColor(COLOR_HIGHLIGHT);
        break;
    case eSystemAttr_Color_TextSelectForeground:
        *aInfo->mColor = ::GetSysColor(COLOR_HIGHLIGHTTEXT);
        break;
    //---------
    // Size
    //---------
    case eSystemAttr_Size_ScrollbarHeight : 
        aInfo->mSize = ::GetSystemMetrics(SM_CXHSCROLL);
        break;
    case eSystemAttr_Size_ScrollbarWidth : 
        aInfo->mSize = ::GetSystemMetrics(SM_CXVSCROLL);
        break;
    case eSystemAttr_Size_WindowTitleHeight:
        aInfo->mSize = ::GetSystemMetrics(SM_CYCAPTION);
        break;
    case eSystemAttr_Size_WindowBorderWidth:
        aInfo->mSize = ::GetSystemMetrics(SM_CXFRAME);
        break;
    case eSystemAttr_Size_WindowBorderHeight:
        aInfo->mSize = ::GetSystemMetrics(SM_CYFRAME);
        break;
    case eSystemAttr_Size_Widget3DBorder:
        aInfo->mSize = ::GetSystemMetrics(SM_CXEDGE);
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

      status = GetSysFontInfo(tdc, anID, aInfo->mFont);

      if (nsnull == mDC)
        ::ReleaseDC(hwnd, tdc);

      break;
    }
    case eSystemAttr_Font_Widget:

      aInfo->mFont->name        = "Arial";
      aInfo->mFont->style       = NS_FONT_STYLE_NORMAL;
      aInfo->mFont->weight      = NS_FONT_WEIGHT_NORMAL;
      aInfo->mFont->decorations = NS_FONT_DECORATION_NONE;
      aInfo->mFont->size        = 204;
      break;
  } // switch 

  return NS_OK;
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

  const PRUnichar* unicodefontname = aFontName.GetUnicode();

  int outlen = ::WideCharToMultiByte(CP_ACP, 0, aFontName.GetUnicode(), aFontName.Length(), 
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

NS_IMETHODIMP nsDeviceContextWin::GetILColorSpace(IL_ColorSpace*& aColorSpace)
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

NS_IMETHODIMP nsDeviceContextWin::GetPaletteInfo(nsPaletteInfo& aPaletteInfo)
{
  aPaletteInfo.isPaletteDevice = mPaletteInfo.isPaletteDevice;
  aPaletteInfo.sizePalette = mPaletteInfo.sizePalette;
  aPaletteInfo.numReserved = mPaletteInfo.numReserved;

  if (NULL == mPaletteInfo.palette) {
    IL_ColorSpace*  colorSpace;
    GetILColorSpace(colorSpace);

    if (NI_PseudoColor == colorSpace->type) {
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
      mPaletteInfo.palette = ::CreatePalette(logPal);
    }

    IL_ReleaseColorSpace(colorSpace);
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
  if (mWidth == -1)
    mWidth = NSToIntRound(mWidthFloat * mDevUnitsToAppUnits);

  if (mHeight == -1)
    mHeight = NSToIntRound(mHeightFloat * mDevUnitsToAppUnits);

  aWidth = mWidth;
  aHeight = mHeight;

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextWin :: GetClientRect(nsRect &aRect)
{
  aRect = mClientRect;
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

NS_IMETHODIMP nsDeviceContextWin :: BeginDocument(void)
{
  if (NULL != mDC)
  {
    DOCINFO docinfo;

    docinfo.cbSize = sizeof(docinfo);
    docinfo.lpszDocName = "New Layout Document";
    docinfo.lpszOutput = NULL;
    docinfo.lpszDatatype = NULL;
    docinfo.fwType = 0;

    HGLOBAL hdevmode;
    DEVMODE *devmode;

    //XXX need to QI rather than cast... MMP

    ((nsDeviceContextSpecWin *)mSpec)->GetDEVMODE(hdevmode);

    devmode = (DEVMODE *)::GlobalLock(hdevmode);

//  ::ResetDC(mDC, devmode);

    ::GlobalUnlock(hdevmode);

    if (::StartDoc(mDC, &docinfo) > 0)
      return NS_OK;
    else
      return NS_ERROR_FAILURE;
  }

  return NS_OK;
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
