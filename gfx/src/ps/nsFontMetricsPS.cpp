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

#include "nsFontMetricsPS.h"

static NS_DEFINE_IID(kIFontMetricsIID, NS_IFONT_METRICS_IID);

nsFontMetricsPS :: nsFontMetricsPS()
{
  NS_INIT_REFCNT();
}
  
nsFontMetricsPS :: ~nsFontMetricsPS()
{
  if (nsnull != mFont){
    delete mFont;
    mFont = nsnull;
  }

  if (NULL != mFontHandle){
    ::DeleteObject(mFontHandle);
    mFontHandle = NULL;
  }

  mDeviceContext = nsnull;
}

#ifdef LEAK_DEBUG
nsrefcnt
nsFontMetricsPS :: AddRef()
{
  NS_PRECONDITION(mRefCnt != 0, "resurrecting a dead object");
  return ++mRefCnt;
}

nsrefcnt
nsFontMetricsPS :: Release()
{
  NS_PRECONDITION(mRefCnt != 0, "too many release's");
  if (--mRefCnt == 0) {
    delete this;
  }
  return mRefCnt;
}

nsresult
nsFontMetricsPS :: QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  *aInstancePtr = NULL;

  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
  static NS_DEFINE_IID(kClassIID, kIFontMetricsIID);
  if (aIID.Equals(kClassIID)) {
    *aInstancePtr = (void*) this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*) ((nsISupports*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}
#else
NS_IMPL_ISUPPORTS(nsFontMetricsPS, kIFontMetricsIID)
#endif

NS_IMETHODIMP
nsFontMetricsPS :: Init(const nsFont& aFont, nsIDeviceContext *aContext)
{
  mFont = new nsFont(aFont);
  //don't addref this to avoid circular refs
  mDeviceContext = (nsDeviceContextPS *)aContext;
  RealizeFont();
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsPS :: Destroy()
{
  mDeviceContext = nsnull;
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
nsFontMetricsPS::RealizeFont()
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
  logFont.lfWeight = ((400 < mFont->weight) ? FW_BOLD : FW_NORMAL);  // XXX could be smarter
  logFont.lfItalic = (mFont->style & (NS_FONT_STYLE_ITALIC | NS_FONT_STYLE_OBLIQUE))
    ? TRUE : FALSE;   // XXX need better oblique support
  float app2dev, app2twip, scale;
  mDeviceContext->GetAppUnitsToDevUnits(app2dev);
  mDeviceContext->GetDevUnitsToTwips(app2twip);
  mDeviceContext->GetCanonicalPixelScale(scale);
  app2twip *= app2dev * scale;

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

  HWND win = NULL;
  HDC dc = NULL;

  if (NULL != mDeviceContext->mDC)
    dc = mDeviceContext->mDC;
  else
  {
    // Find font metrics and character widths
    win = (HWND)mDeviceContext->mWidget;
    dc = ::GetDC(win);
  }

  HFONT oldfont = (HFONT)::SelectObject(dc, (HGDIOBJ) mFontHandle);

  // Get font metrics
  float dev2app;
  mDeviceContext->GetDevUnitsToAppUnits(dev2app);
  OUTLINETEXTMETRIC oMetrics;
  TEXTMETRIC&  metrics = oMetrics.otmTextMetrics;
  nscoord onePixel = NSToCoordRound(1 * dev2app);

  if (0 < ::GetOutlineTextMetrics(dc, sizeof(oMetrics), &oMetrics)) {
//    mXHeight = NSToCoordRound(oMetrics.otmsXHeight * dev2app);  XXX not really supported on windows
    mXHeight = NSToCoordRound((float)metrics.tmAscent * dev2app * 0.50f); // 50% of ascent, best guess for true type
    mSuperscriptOffset = NSToCoordRound(oMetrics.otmptSuperscriptOffset.y * dev2app);
    mSubscriptOffset = NSToCoordRound(oMetrics.otmptSubscriptOffset.y * dev2app);

    mStrikeoutSize = MAX(onePixel, NSToCoordRound(oMetrics.otmsStrikeoutSize * dev2app));
    mStrikeoutOffset = NSToCoordRound(oMetrics.otmsStrikeoutPosition * dev2app);
    mUnderlineSize = MAX(onePixel, NSToCoordRound(oMetrics.otmsUnderscoreSize * dev2app));
    mUnderlineOffset = NSToCoordRound(oMetrics.otmsUnderscorePosition * dev2app);
  }
  else {
    // Make a best-effort guess at extended metrics
    // this is based on general typographic guidelines
    ::GetTextMetrics(dc, &metrics);
    mXHeight = NSToCoordRound((float)metrics.tmAscent * dev2app * 0.56f); // 56% of ascent, best guess for non-true type
    mSuperscriptOffset = mXHeight;     // XXX temporary code!
    mSubscriptOffset = mXHeight;     // XXX temporary code!

    mStrikeoutSize = onePixel; // XXX this is a guess
    mStrikeoutOffset = NSToCoordRound(mXHeight / 2.0f); // 50% of xHeight
    mUnderlineSize = onePixel; // XXX this is a guess
    mUnderlineOffset = -NSToCoordRound((float)metrics.tmDescent * dev2app * 0.30f); // 30% of descent
  }

  mHeight = NSToCoordRound(metrics.tmHeight * dev2app);
  mAscent = NSToCoordRound(metrics.tmAscent * dev2app);
  mDescent = NSToCoordRound(metrics.tmDescent * dev2app);
  mLeading = NSToCoordRound(metrics.tmInternalLeading * dev2app);
  mMaxAscent = NSToCoordRound(metrics.tmAscent * dev2app);
  mMaxDescent = NSToCoordRound(metrics.tmDescent * dev2app);
  mMaxAdvance = NSToCoordRound(metrics.tmMaxCharWidth * dev2app);

  ::SelectObject(dc, oldfont);

  if (NULL == mDeviceContext->mDC)
    ::ReleaseDC(win, dc);
}

NS_IMETHODIMP
nsFontMetricsPS :: GetXHeight(nscoord& aResult)
{
  aResult = mXHeight;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsPS :: GetSuperscriptOffset(nscoord& aResult)
{
  aResult = mSuperscriptOffset;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsPS :: GetSubscriptOffset(nscoord& aResult)
{
  aResult = mSubscriptOffset;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsPS :: GetStrikeout(nscoord& aOffset, nscoord& aSize)
{
  aOffset = mStrikeoutOffset;
  aSize = mStrikeoutSize;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsPS :: GetUnderline(nscoord& aOffset, nscoord& aSize)
{
  aOffset = mUnderlineOffset;
  aSize = mUnderlineSize;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsPS :: GetHeight(nscoord &aHeight)
{
  aHeight = mHeight;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsPS :: GetLeading(nscoord &aLeading)
{
  aLeading = mLeading;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsPS :: GetMaxAscent(nscoord &aAscent)
{
  aAscent = mMaxAscent;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsPS :: GetMaxDescent(nscoord &aDescent)
{
  aDescent = mMaxDescent;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsPS :: GetMaxAdvance(nscoord &aAdvance)
{
  aAdvance = mMaxAdvance;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsPS :: GetFont(const nsFont *&aFont)
{
  aFont = mFont;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsPS::GetFontHandle(nsFontHandle &aHandle)
{
  aHandle = mFontHandle;
  return NS_OK;
}
