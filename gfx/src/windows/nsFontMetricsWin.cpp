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
}

NS_IMPL_ISUPPORTS(nsFontMetricsWin, kIFontMetricsIID)

// Note: The presentation context has a reference to this font
// metrics, therefore avoid circular references by not AddRef'ing the
// presentation context.
nsresult nsFontMetricsWin :: Init(const nsFont& aFont, nsIDeviceContext* aCX)
{
  mFont = new nsFont(aFont);
  mContext = aCX;

  RealizeFont();

  return NS_OK;
}

// XXX this function is a hack; the only logical font names we should
// support are the one used by css.
const char* nsFontMetricsWin::MapFamilyToFont(const nsString& aLogicalFontName)
{
  if (aLogicalFontName.EqualsIgnoreCase("Times Roman")) {
    return "Times New Roman";
  }
  if (aLogicalFontName.EqualsIgnoreCase("Times")) {
    return "Times New Roman";
  }
  if (aLogicalFontName.EqualsIgnoreCase("Unicode")) {
    return "Bitstream Cyberbit";
  }
  if (aLogicalFontName.EqualsIgnoreCase("Courier")) {
    return "Courier New";
  }
  if (aLogicalFontName.EqualsIgnoreCase("Courier New")) {
    return "Courier New";
  }

  // the CSS generic names
  if (aLogicalFontName.EqualsIgnoreCase("serif")) {
    return "Times New Roman";
  }
  if (aLogicalFontName.EqualsIgnoreCase("sans-serif")) {
    return "Arial";
  }
  if (aLogicalFontName.EqualsIgnoreCase("cursive")) {
//    return "XXX";
  }
  if (aLogicalFontName.EqualsIgnoreCase("fantasy")) {
//    return "XXX";
  }
  if (aLogicalFontName.EqualsIgnoreCase("monospace")) {
    return "Courier New";
  }
  return "Arial";/* XXX for now */
}

void nsFontMetricsWin::RealizeFont()
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
  float t2p = mContext->GetAppUnitsToDevUnits();
  logFont.lfHeight = (LONG)(-mFont->size * t2p);
  strncpy(logFont.lfFaceName,
          MapFamilyToFont(mFont->name),
          LF_FACESIZE);

  // Create font handle from font spec
  mFontHandle = ::CreateFontIndirect(&logFont);
//fprintf(stderr, "fFontHandle=%x\n", fFontHandle);

  // Find font metrics and character widths
  HWND win = ::GetDesktopWindow();
  HDC dc = ::GetDC(win);
  ::SelectObject(dc, (HGDIOBJ) mFontHandle);

  // Get font metrics
  float p2t = mContext->GetDevUnitsToAppUnits();
  TEXTMETRIC metrics;
  ::GetTextMetrics(dc, &metrics);
  mHeight = nscoord(metrics.tmHeight * p2t);
  mAscent = nscoord(metrics.tmAscent * p2t);
  mDescent = nscoord(metrics.tmDescent * p2t);
  mLeading = nscoord(metrics.tmInternalLeading * p2t);
  mMaxAscent = nscoord(metrics.tmAscent * p2t);
  mMaxDescent = nscoord(metrics.tmDescent * p2t);
  mMaxAdvance = nscoord(metrics.tmMaxCharWidth * p2t);
#if 0
  fprintf(stderr, "fontmetrics: height=%d ascent=%d descent=%d leading=%d\n",
          fHeight, fAscent, fDescent, fLeading);
#endif

  // Get character widths in twips
  int widths[256];
  ::GetCharWidth(dc, 0, 255, widths);
  nscoord* tp = mCharWidths;
  int* fp = widths;
  int* end = fp + 256;
  while (fp < end) {
    *tp++ = nscoord( *fp++ * p2t );
  }

  ::ReleaseDC(win, dc);
}

nscoord nsFontMetricsWin :: GetWidth(char ch)
{
  return mCharWidths[PRUint8(ch)];
}

nscoord nsFontMetricsWin :: GetWidth(PRUnichar ch)
{
  if (ch < 256) {
    return mCharWidths[ch];
  }
  return 0;/* XXX */
}

nscoord nsFontMetricsWin :: GetWidth(const nsString& aString)
{
  return GetWidth(aString.GetUnicode(), aString.Length());
}

nscoord nsFontMetricsWin :: GetWidth(const char *aString)
{
  // XXX use native text measurement routine
  nscoord sum = 0;
  PRUint8 ch;
  while ((ch = PRUint8(*aString++)) != 0) {
    sum += mCharWidths[ch];
  }
  return sum;
}

nscoord nsFontMetricsWin :: GetWidth(const PRUnichar *aString, PRUint32 aLength)
{
  // XXX use native text measurement routine
  nscoord sum = 0;
  while (aLength != 0) {
    PRUnichar ch = *aString++;
    if (ch < 256) {
      sum += mCharWidths[ch];
    } else {
      // XXX not yet
    }
    --aLength;
  }
  return sum;
}

nscoord nsFontMetricsWin :: GetHeight()
{
  return mHeight;
}

nscoord nsFontMetricsWin :: GetLeading()
{
  return mLeading;
}

nscoord nsFontMetricsWin :: GetMaxAscent()
{
  return mMaxAscent;
}

nscoord nsFontMetricsWin :: GetMaxDescent()
{
  return mMaxDescent;
}

nscoord nsFontMetricsWin :: GetMaxAdvance()
{
  return mMaxAdvance;
}

const nscoord * nsFontMetricsWin :: GetWidths()
{
  return mCharWidths;
}

const nsFont& nsFontMetricsWin :: GetFont()
{
  return *mFont;
}

nsFontHandle nsFontMetricsWin::GetFontHandle()
{
  return mFontHandle;
}
