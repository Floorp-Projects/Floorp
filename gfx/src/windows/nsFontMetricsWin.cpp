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

#include "nsFontMetricsWin.h"

static NS_DEFINE_IID(kIFontMetricsIID, NS_IFONT_METRICS_IID);

nsFontMetricsWin :: nsFontMetricsWin()
{
  NS_INIT_REFCNT();
}
  
nsFontMetricsWin :: ~nsFontMetricsWin()
{
  if (nsnull != mFont)
  {
    delete mFont;
    mFont = nsnull;
  }

  if (NULL != mFontHandle)
  {
    ::DeleteObject(mFontHandle);
    mFontHandle = NULL;
  }

  NS_IF_RELEASE(mDeviceContext);
}

#ifdef LEAK_DEBUG
nsrefcnt
nsFontMetricsWin :: AddRef()
{
  NS_PRECONDITION(mRefCnt != 0, "resurrecting a dead object");
  return ++mRefCnt;
}

nsrefcnt
nsFontMetricsWin :: Release()
{
  NS_PRECONDITION(mRefCnt != 0, "too many release's");
  if (--mRefCnt == 0) {
    delete this;
  }
  return mRefCnt;
}

nsresult
nsFontMetricsWin :: QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  *aInstancePtr = NULL;

  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
  static NS_DEFINE_IID(kClassIID, kIFontMetricsIID);
  if (aIID.Equals(kClassIID)) {
    *aInstancePtr = (void*) this;
    mRefCnt++;
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*) ((nsISupports*)this);
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}
#else
NS_IMPL_ISUPPORTS(nsFontMetricsWin, kIFontMetricsIID)
#endif

NS_IMETHODIMP
nsFontMetricsWin :: Init(const nsFont& aFont, nsIDeviceContext *aContext)
{
  mFont = new nsFont(aFont);
  mDeviceContext = aContext;
  NS_ADDREF(aContext);
  RealizeFont();
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin :: Destroy()
{
  NS_IF_RELEASE(mDeviceContext);
  return NS_OK;
}

static void
MapGenericFamilyToFont(const nsString& aGenericFamily,
                       nsIDeviceContext* aDC,
                       nsString& aFontFace)
{
  // the CSS generic names (conversions from Nav for now)
  // XXX this  need to check availability with the dc
  PRBool  aliased;
  if (aGenericFamily.EqualsIgnoreCase("serif")) {
    aDC->GetLocalFontName(nsString("Times New Roman"), aFontFace, aliased);
  }
  else if (aGenericFamily.EqualsIgnoreCase("sans-serif")) {
    aDC->GetLocalFontName(nsString("Arial"), aFontFace, aliased);
  }
  else if (aGenericFamily.EqualsIgnoreCase("cursive")) {
    aDC->GetLocalFontName(nsString("Script"), aFontFace, aliased);
  }
  else if (aGenericFamily.EqualsIgnoreCase("fantasy")) {
    aDC->GetLocalFontName(nsString("Arial"), aFontFace, aliased);
  }
  else if (aGenericFamily.EqualsIgnoreCase("monospace")) {
    aDC->GetLocalFontName(nsString("Courier New"), aFontFace, aliased);
  }
  else {
    aFontFace.Truncate();
  }
}

struct FontEnumData {
  FontEnumData(nsIDeviceContext* aContext, TCHAR* aFaceName)
  {
    mContext = aContext;
    mFaceName = aFaceName;
  }
  nsIDeviceContext* mContext;
  TCHAR* mFaceName;
};

static PRBool
FontEnumCallback(const nsString& aFamily, PRBool aGeneric, void *aData)
{
  FontEnumData* data = (FontEnumData*)aData;
  if (aGeneric) {
    nsAutoString realFace;
    MapGenericFamilyToFont(aFamily, data->mContext, realFace);
    realFace.ToCString(data->mFaceName, LF_FACESIZE);
    return PR_FALSE;  // stop
  }
  else {
    nsAutoString realFace;
    PRBool  aliased;
    data->mContext->GetLocalFontName(aFamily, realFace, aliased);
    if (aliased || (NS_OK == data->mContext->CheckFontExistence(realFace))) {
      realFace.ToCString(data->mFaceName, LF_FACESIZE);
      return PR_FALSE;  // stop
    }
  }
  return PR_TRUE;
}

void
nsFontMetricsWin::RealizeFont()
{
  // Fill in logFont structure; stolen from awt
  LOGFONT logFont;
  logFont.lfWidth          = 0;
  logFont.lfEscapement     = 0;
  logFont.lfOrientation    = 0;
  logFont.lfUnderline      =
    (mFont->decorations & NS_FONT_DECORATION_UNDERLINE)
    ? TRUE : FALSE;
  logFont.lfStrikeOut      =
    (mFont->decorations & NS_FONT_DECORATION_LINE_THROUGH)
    ? TRUE : FALSE;
  logFont.lfCharSet        = DEFAULT_CHARSET;
  logFont.lfOutPrecision   = OUT_DEFAULT_PRECIS;
  logFont.lfClipPrecision  = CLIP_DEFAULT_PRECIS;
  logFont.lfQuality        = DEFAULT_QUALITY;
  logFont.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
  logFont.lfWeight = (mFont->weight > NS_FONT_WEIGHT_NORMAL)  // XXX ??? this should be smarter
    ? FW_BOLD : FW_NORMAL;
  logFont.lfItalic = (mFont->style & NS_FONT_STYLE_ITALIC)
    ? TRUE : FALSE;
  float app2dev, app2twip;
  mDeviceContext->GetAppUnitsToDevUnits(app2dev);
  mDeviceContext->GetDevUnitsToTwips(app2twip);
  app2twip *= app2dev;

  float rounded = ((float)NSIntPointsToTwips(NSTwipsToFloorIntPoints(nscoord(mFont->size * app2twip)))) / app2twip;

  // round font size off to floor point size to be windows compatible
//  logFont.lfHeight = - NSToIntRound(rounded * app2dev);  // this is proper (windows) rounding
  logFont.lfHeight = - LONG(rounded * app2dev);  // this floor rounding is to make ours compatible with Nav 4.0

#ifdef NS_DEBUG
  // Make Purify happy
  memset(logFont.lfFaceName, 0, sizeof(logFont.lfFaceName));
#endif
  logFont.lfFaceName[0] = '\0';

  FontEnumData  data(mDeviceContext, logFont.lfFaceName);
  mFont->EnumerateFamilies(FontEnumCallback, &data); 

  // Create font handle from font spec
  mFontHandle = ::CreateFontIndirect(&logFont);

  // Find font metrics and character widths
  nsNativeWidget  widget;
  mDeviceContext->GetNativeWidget(widget);
  HWND win = (HWND)widget;
  HDC dc = ::GetDC(win);
  HFONT oldfont = (HFONT)::SelectObject(dc, (HGDIOBJ) mFontHandle);

  // Get font metrics
  float dev2app;
  mDeviceContext->GetDevUnitsToAppUnits(dev2app);
  TEXTMETRIC metrics;
  ::GetTextMetrics(dc, &metrics);
  mHeight = nscoord(metrics.tmHeight * dev2app);
  mAscent = nscoord(metrics.tmAscent * dev2app);
  mDescent = nscoord(metrics.tmDescent * dev2app);
  mLeading = nscoord(metrics.tmInternalLeading * dev2app);
  mMaxAscent = nscoord(metrics.tmAscent * dev2app);
  mMaxDescent = nscoord(metrics.tmDescent * dev2app);
  mMaxAdvance = nscoord(metrics.tmMaxCharWidth * dev2app);

  ::ReleaseDC(win, dc);
}

NS_IMETHODIMP
nsFontMetricsWin :: GetWidth(char ch, nscoord& aWidth)
{
  char buf[1];
  buf[0] = ch;
  return GetWidth(buf, 1, aWidth);
}

NS_IMETHODIMP
nsFontMetricsWin :: GetWidth(PRUnichar ch, nscoord &aWidth)
{
  PRUnichar buf[1];
  buf[0] = ch;
  return GetWidth(buf, 1, aWidth);
}

NS_IMETHODIMP
nsFontMetricsWin :: GetWidth(const char* aString, nscoord& aWidth)
{
  return GetWidth(aString, strlen(aString), aWidth);
}

NS_IMETHODIMP
nsFontMetricsWin :: GetWidth(const char* aString,
                             PRUint32 aLength,
                             nscoord& aWidth)
{
  if (nsnull == mDeviceContext) {
    aWidth = 0;
    return NS_ERROR_NULL_POINTER;
  }

  nsNativeWidget widget;
  mDeviceContext->GetNativeWidget(widget);
  HWND win = (HWND)widget;
  HDC  hdc = ::GetDC(win);
  HFONT oldfont = (HFONT)::SelectObject(hdc, (HGDIOBJ) mFontHandle);
  SIZE size;
  BOOL status = GetTextExtentPoint32(hdc, aString, aLength, &size);
  ::ReleaseDC(win, hdc);

  float app2dev, dev2twip;
  mDeviceContext->GetAppUnitsToDevUnits(app2dev);
  mDeviceContext->GetDevUnitsToTwips(dev2twip);

  float app2twip = dev2twip * app2dev;
  aWidth = nscoord(float(size.cx) * dev2twip);
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin :: GetWidth(const nsString& aString, nscoord& aWidth)
{
  return GetWidth(aString.GetUnicode(), aString.Length(), aWidth);
}

NS_IMETHODIMP
nsFontMetricsWin :: GetWidth(const PRUnichar *aString,
                             PRUint32 aLength,
                             nscoord &aWidth)
{
  if (nsnull == mDeviceContext) {
    aWidth = 0;
    return NS_ERROR_NULL_POINTER;
  }

  nsNativeWidget widget;
  mDeviceContext->GetNativeWidget(widget);
  HWND win = (HWND)widget;
  HDC  hdc = ::GetDC(win);
  HFONT oldfont = (HFONT)::SelectObject(hdc, (HGDIOBJ) mFontHandle);
  SIZE size;
  BOOL status = GetTextExtentPoint32W(hdc, aString, aLength, &size);
  ::ReleaseDC(win, hdc);

  float app2dev, dev2twip;
  mDeviceContext->GetAppUnitsToDevUnits(app2dev);
  mDeviceContext->GetDevUnitsToTwips(dev2twip);

  float app2twip = dev2twip * app2dev;
  aWidth = nscoord(float(size.cx) * dev2twip);
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin :: GetHeight(nscoord &aHeight)
{
  aHeight = mHeight;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin :: GetLeading(nscoord &aLeading)
{
  aLeading = mLeading;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin :: GetMaxAscent(nscoord &aAscent)
{
  aAscent = mMaxAscent;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin :: GetMaxDescent(nscoord &aDescent)
{
  aDescent = mMaxDescent;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin :: GetMaxAdvance(nscoord &aAdvance)
{
  aAdvance = mMaxAdvance;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin :: GetFont(const nsFont *&aFont)
{
  aFont = mFont;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin::GetFontHandle(nsFontHandle &aHandle)
{
  aHandle = mFontHandle;
  return NS_OK;
}
