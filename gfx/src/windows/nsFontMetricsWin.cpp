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
#include "prmem.h"

static NS_DEFINE_IID(kIFontMetricsIID, NS_IFONT_METRICS_IID);

typedef struct UnicodeRange
{
#undef UNICODE_RANGE_DESCRIPTION
#ifdef UNICODE_RANGE_DESCRIPTION
  PRUint8  bit;
  char*    description;
#endif
  PRUint16 begin;
  PRUint16 end;
} UnicodeRange;

static UnicodeRange unicodeRanges[128] =
{
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    0, "Basic Latin",
#endif
    0x0020, 0x007E
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    1, "Latin-1 Supplement",
#endif
    0x00A0, 0x00FF
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    2, "Latin Extended-A",
#endif
    0x0100, 0x017F
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    3, "Latin Extended-B",
#endif
    0x0180, 0x024F
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    4, "IPA Extensions",
#endif
    0x0250, 0x02AF
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    5, "Spacing Modifier Letters",
#endif
    0x02B0, 0x02FF
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    6, "Combining Diacritical Marks",
#endif
    0x0300, 0x036F
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    7, "Basic Greek",
#endif
    0x0370, 0x03CF
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    8, "Greek Symbols and Coptic",
#endif
    0x03D0, 0x03FF
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    9, "Cyrillic",
#endif
    0x0400, 0x04FF
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    10, "Armenian",
#endif
    0x0530, 0x058F
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    11, "Basic Hebrew",
#endif
    0x05D0, 0x05FF
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    12, "Hebrew Extended",
#endif
    0x0590, 0x05CF
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    13, "Basic Arabic",
#endif
    0x0600, 0x0652
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    14, "Arabic Extended",
#endif
    0x0653, 0x06FF
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    15, "Devanagari",
#endif
    0x0900, 0x097F
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    16, "Bengali",
#endif
    0x0980, 0x09FF
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    17, "Gurmukhi",
#endif
    0x0A00, 0x0A7F
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    18, "Gujarati",
#endif
    0x0A80, 0x0AFF
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    19, "Oriya",
#endif
    0x0B00, 0x0B7F
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    20, "Tamil",
#endif
    0x0B80, 0x0BFF
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    21, "Telugu",
#endif
    0x0C00, 0x0C7F
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    22, "Kannada",
#endif
    0x0C80, 0x0CFF
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    23, "Malayalam",
#endif
    0x0D00, 0x0D7F
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    24, "Thai",
#endif
    0x0E00, 0x0E7F
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    25, "Lao",
#endif
    0x0E80, 0x0EFF
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    26, "Basic Georgian",
#endif
    0x10D0, 0x10FF
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    27, "Georgian Extended",
#endif
    0x10A0, 0x10CF
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    28, "Hangul Jamo",
#endif
    0x1100, 0x11FF
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    29, "Latin Extended Additional",
#endif
    0x1E00, 0x1EFF
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    30, "Greek Extended",
#endif
    0x1F00, 0x1FFF
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    31, "General Punctuation",
#endif
    0x2000, 0x206F
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    32, "Subscripts and Superscripts",
#endif
    0x2070, 0x209F
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    33, "Currency Symbols",
#endif
    0x20A0, 0x20CF
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    34, "Combining Diacritical Marks for Symbols",
#endif
    0x20D0, 0x20FF
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    35, "Letter-like Symbols",
#endif
    0x2100, 0x214F
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    36, "Number Forms",
#endif
    0x2150, 0x218F
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    37, "Arrows",
#endif
    0x2190, 0x21FF
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    38, "Mathematical Operators",
#endif
    0x2200, 0x22FF
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    39, "Miscellaneous Technical",
#endif
    0x2300, 0x23FF
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    40, "Control Pictures",
#endif
    0x2400, 0x243F
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    41, "Optical Character Recognition",
#endif
    0x2440, 0x245F
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    42, "Enclosed Alphanumerics",
#endif
    0x2460, 0x24FF
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    43, "Box Drawing",
#endif
    0x2500, 0x257F
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    44, "Block Elements",
#endif
    0x2580, 0x259F
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    45, "Geometric Shapes",
#endif
    0x25A0, 0x25FF
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    46, "Miscellaneous Symbols",
#endif
    0x2600, 0x26FF
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    47, "Dingbats",
#endif
    0x2700, 0x27BF
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    48, "Chinese, Japanese, and Korean (CJK) Symbols and Punctuation",
#endif
    0x3000, 0x303F
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    49, "Hiragana",
#endif
    0x3040, 0x309F
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    50, "Katakana",
#endif
    0x30A0, 0x30FF
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    51, "Bopomofo",
#endif
    0x3100, 0x312F
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    52, "Hangul Compatibility Jamo",
#endif
    0x3130, 0x318F
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    53, "CJK Miscellaneous",
#endif
    0x3190, 0x319F
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    54, "Enclosed CJK",
#endif
    0x3200, 0x32FF
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    55, "CJK Compatibility",
#endif
    0x3300, 0x33FF
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    56, "Hangul",
#endif
    0xAC00, 0xD7A3
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    57, "Reserved for Unicode Subranges",
#endif
    0x3D2E, 0x44B7
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    58, "Reserved for Unicode Subranges",
#endif
    0x44B8, 0x4DFF
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    59, "CJK Unified Ideographs",
#endif
    0x4E00, 0x9FFF
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    60, "Private Use Area",
#endif
    0xE000, 0xF8FF
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    61, "CJK Compatibility Ideographs",
#endif
    0xF900, 0xFAFF
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    62, "Alphabetic Presentation Forms",
#endif
    0xFB00, 0xFB4F
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    63, "Arabic Presentation Forms-A",
#endif
    0xFB50, 0xFDFF
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    64, "Combining Half Marks",
#endif
    0xFE20, 0xFE2F
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    65, "CJK Compatibility Forms",
#endif
    0xFE30, 0xFE4F
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    66, "Small Form Variants",
#endif
    0xFE50, 0xFE6F
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    67, "Arabic Presentation Forms-B",
#endif
    0xFE70, 0xFEFE
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    68, "Halfwidth and Fullwidth Forms",
#endif
    0xFF00, 0xFFEF
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    69, "Specials",
#endif
    0xFFF0, 0xFFFD
  },
  {
#ifdef UNICODE_RANGE_DESCRIPTION
    70-127, "Reserved for Unicode Subranges",
#endif
    0x0000, 0x0000
  }
};

typedef struct nsGlobalFont
{
  nsString*     name;
  FONTSIGNATURE signature;
  PRUint8*      map;
} nsGlobalFont;

static nsGlobalFont* gGlobalFonts = nsnull;
static int gGlobalFontsAlloc = 0;
static int gGlobalFontsCount = 0;

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

#ifdef FONT_SWITCHING

  if (mFonts) {
    delete [] mFonts;
    mFonts = nsnull;
  }

  if (mLoadedFonts) {
    nsFontWin* font = mLoadedFonts;
    nsFontWin* end = &mLoadedFonts[mLoadedFontsCount];
    while (font < end) {
      if (font->font) {
        ::DeleteObject(font->font);
      }
      font++;
    }
    PR_Free(mLoadedFonts);
    mLoadedFonts = nsnull;
  }

#endif /* FONT_SWITCHING */

  mDeviceContext = nsnull;
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
NS_IMPL_ISUPPORTS(nsFontMetricsWin, kIFontMetricsIID)
#endif

NS_IMETHODIMP
nsFontMetricsWin :: Init(const nsFont& aFont, nsIDeviceContext *aContext)
{
  mFont = new nsFont(aFont);
  //don't addref this to avoid circular refs
  mDeviceContext = (nsDeviceContextWin *)aContext;
  RealizeFont();
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin :: Destroy()
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

void
nsFontMetricsWin::FillLogFont(LOGFONT* logFont)
{
  // Fill in logFont structure; stolen from awt
  logFont->lfWidth          = 0; 
  logFont->lfEscapement     = 0;
  logFont->lfOrientation    = 0;
  logFont->lfUnderline      = 
    (mFont->decorations & NS_FONT_DECORATION_UNDERLINE)
    ? TRUE : FALSE; 
  logFont->lfStrikeOut      =
    (mFont->decorations & NS_FONT_DECORATION_LINE_THROUGH)
    ? TRUE : FALSE; 
  logFont->lfCharSet        = DEFAULT_CHARSET;
  logFont->lfOutPrecision   = OUT_DEFAULT_PRECIS;
  logFont->lfClipPrecision  = CLIP_DEFAULT_PRECIS;
  logFont->lfQuality        = DEFAULT_QUALITY;
  logFont->lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
  logFont->lfWeight = ((400 < mFont->weight) ? FW_BOLD : FW_NORMAL);  // XXX could be smarter
  logFont->lfItalic = (mFont->style & (NS_FONT_STYLE_ITALIC | NS_FONT_STYLE_OBLIQUE))
    ? TRUE : FALSE;   // XXX need better oblique support
  float app2dev, app2twip, scale;
  mDeviceContext->GetAppUnitsToDevUnits(app2dev);
  mDeviceContext->GetDevUnitsToTwips(app2twip);
  mDeviceContext->GetCanonicalPixelScale(scale);
  app2twip *= app2dev * scale;

  // this interesting bit of code rounds the font size off to the floor point value
  // this is necessary for proper font scaling under windows
  PRInt32 sizePoints = NSTwipsToFloorIntPoints(nscoord(mFont->size * app2twip));
  float rounded = ((float)NSIntPointsToTwips(sizePoints)) / app2twip;

  // round font size off to floor point size to be windows compatible
  logFont->lfHeight = - NSToIntRound(rounded * app2dev);  // this is proper (windows) rounding
//  logFont->lfHeight = - LONG(rounded * app2dev);  // this floor rounding is to make ours compatible with Nav 4.0

#ifdef NS_DEBUG
  // Make Purify happy
  memset(logFont->lfFaceName, 0, sizeof(logFont->lfFaceName));
#endif
}

#ifdef FONT_SWITCHING

static void
FillBitMap(FONTSIGNATURE* aSignature, PRUint8* aMap)
{
  DWORD* array = aSignature->fsUsb;
  int i = 0;
  int dword;
#ifdef UNICODE_RANGE_DESCRIPTION
  printf("  FONTSIGNATURE.fsUsb:");
  for (dword = 0; dword < 4; dword++) {
    printf(" 0x%08X", array[dword]);
  }
  printf("\n");
#endif
  for (dword = 0; dword < 4; dword++) {
    for (int bit = 0; bit < sizeof(DWORD) * 8; bit++) {
      if ((array[dword] >> bit) & 1) {
        int end = unicodeRanges[i].end;
        for (int c = unicodeRanges[i].begin; c <= end; c++) {
          ADD_GLYPH(aMap, c);
        }
#ifdef UNICODE_RANGE_DESCRIPTION
        printf("    %s\n",
          unicodeRanges[i].description?unicodeRanges[i].description:"NULL");
#endif
      }
      i++;
    }
  }
}

nsFontWin*
nsFontMetricsWin::LoadFont(HDC aDC, nsString* aName)
{
  LOGFONT logFont;
  FillLogFont(&logFont);

  // XXX need to preserve Unicode chars in face name (use LOGFONTW) -- erik
  aName->ToCString(logFont.lfFaceName, LF_FACESIZE);

  HFONT hfont = ::CreateFontIndirect(&logFont);

  if (hfont) {
    HFONT oldFont = (HFONT) ::SelectObject(aDC, (HGDIOBJ) hfont);
    if (mLoadedFontsCount == mLoadedFontsAlloc) {
      int newSize = 2 * (mLoadedFontsAlloc ? mLoadedFontsAlloc : 1);
      nsFontWin* newPointer = (nsFontWin*) PR_Realloc(mLoadedFonts,
        newSize * sizeof(nsFontWin));
      if (newPointer) {
        mLoadedFonts = newPointer;
        mLoadedFontsAlloc = newSize;
      }
      else {
        return nsnull;
      }
    }
    nsFontWin* font = &mLoadedFonts[mLoadedFontsCount++];
    font->font = hfont;
    memset(font->map, 0, sizeof(font->map));
    FONTSIGNATURE signature;
    GetTextCharsetInfo(aDC, &signature, 0);
#ifdef UNICODE_RANGE_DESCRIPTION
    printf("%s\n", logFont.lfFaceName);
#endif
    FillBitMap(&signature, font->map);
    ::SelectObject(aDC, (HGDIOBJ) oldFont);

    return font;
  }

  return nsnull;
}

static int CALLBACK enumProc(const LOGFONT* logFont, const TEXTMETRIC* metrics,
  DWORD fontType, LPARAM closure)
{
  // XXX make this smarter: don't add font to list if we already have a font
  // with the same font signature -- erik
  if (gGlobalFontsCount == gGlobalFontsAlloc) {
    int newSize = 2 * (gGlobalFontsAlloc ? gGlobalFontsAlloc : 1);
    nsGlobalFont* newPointer = (nsGlobalFont*) PR_Realloc(gGlobalFonts,
      newSize * sizeof(nsGlobalFont));
    if (newPointer) {
      gGlobalFonts = newPointer;
      gGlobalFontsAlloc = newSize;
    }
    else {
      return 0;
    }
  }
  nsGlobalFont* font = &gGlobalFonts[gGlobalFontsCount++];

  // XXX do correct character encoding conversion here
  font->name = new nsString(logFont->lfFaceName);

// XXX use the fast way on NT only?
#if 0
  NEWTEXTMETRICEX* metricsEx = (NEWTEXTMETRICEX*) metrics;
  font->signature = metricsEx->ntmFontSig;
#else
  HDC dc = (HDC) closure;
  HFONT hfont = ::CreateFontIndirect(logFont);
  if (hfont) {
    HFONT oldFont = (HFONT) ::SelectObject(dc, (HGDIOBJ) hfont);
    GetTextCharsetInfo(dc, &font->signature, 0);
    ::SelectObject(dc, (HGDIOBJ) oldFont);
    ::DeleteObject(hfont);
  }
#endif

  font->map = nsnull;

  return 1;
}

nsFontWin*
nsFontMetricsWin::FindGlobalFont(HDC aDC, PRUnichar c)
{
  if (!gGlobalFonts) {
    LOGFONT logFont;
    logFont.lfCharSet = DEFAULT_CHARSET;
    logFont.lfFaceName[0] = 0;
    logFont.lfPitchAndFamily = 0;
    EnumFontFamiliesEx(aDC, &logFont, enumProc, (LPARAM) aDC, 0);
  }
  for (int i = 0; i < gGlobalFontsCount; i++) {
    if (!gGlobalFonts[i].map) {
      gGlobalFonts[i].map = (PRUint8*) PR_Calloc(8192, 1);
      if (!gGlobalFonts[i].map) {
        return nsnull;
      }
#ifdef UNICODE_RANGE_DESCRIPTION
      char name[LF_FACESIZE];
      gGlobalFonts[i].name->ToCString(name, sizeof(name));
      printf("%s\n", name);
#endif
      FillBitMap(&gGlobalFonts[i].signature, gGlobalFonts[i].map);
    }
    if (FONT_HAS_GLYPH(gGlobalFonts[i].map, c)) {
      return LoadFont(aDC, gGlobalFonts[i].name);
    }
  }

  return nsnull;
}

nsFontWin*
nsFontMetricsWin::FindLocalFont(HDC aDC, PRUnichar aChar)
{
  while (mFontsIndex < mFontsCount) {
    nsFontWin* font = LoadFont(aDC, &mFonts[mFontsIndex++]);
    if (font && FONT_HAS_GLYPH(font->map, aChar)) {
      return font;
    }
  }

  return nsnull;
}

#endif /* FONT_SWITCHING */

struct FontEnumData {
  FontEnumData(nsIDeviceContext* aContext, TCHAR* aFaceName
#ifdef FONT_SWITCHING
    , nsFontMetricsWin* aMetrics, HDC aDC
#endif /* FONT_SWITCHING */
  )
  {
    mContext = aContext;
    mFaceName = aFaceName;
#ifdef FONT_SWITCHING
    mMetrics = aMetrics;
    mDC = aDC;
    mFoundASCIIFont = 0;
#endif /* FONT_SWITCHING */
  }
  nsIDeviceContext* mContext;
  TCHAR* mFaceName;
#ifdef FONT_SWITCHING
  nsFontMetricsWin* mMetrics;
  HDC mDC;
  char mFoundASCIIFont;
#endif /* FONT_SWITCHING */
};

#ifndef FONT_SWITCHING

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

#else /* FONT_SWITCHING */

static void
FontEnumHelper(FontEnumData* data, nsString* realFace)
{
  nsFontMetricsWin* metrics = data->mMetrics;
  if (metrics->mFontsCount == metrics->mFontsAlloc) {
    int newSize = 2 * (metrics->mFontsAlloc ? metrics->mFontsAlloc : 1);
    nsString* newPointer = new nsString[newSize];
    if (newPointer) {
      for (int i = metrics->mFontsCount - 1; i >= 0; i--) {
        newPointer[i].SetString(metrics->mFonts[i].GetUnicode());
      }
      delete [] metrics->mFonts;
      metrics->mFonts = newPointer;
      metrics->mFontsAlloc = newSize;
    }
    else {
      return;
    }
  }
  metrics->mFonts[metrics->mFontsCount++].SetString(realFace->GetUnicode());
  if (!data->mFoundASCIIFont) {
    nsFontWin* font =
      metrics->LoadFont(data->mDC, &metrics->mFonts[metrics->mFontsIndex++]);
    if (font && FONT_HAS_GLYPH(font->map, 'a')) {
      data->mFoundASCIIFont = 1;
      realFace->ToCString(data->mFaceName, LF_FACESIZE);
    }
  }
}

static PRBool
FontEnumCallback(const nsString& aFamily, PRBool aGeneric, void *aData)
{
  FontEnumData* data = (FontEnumData*)aData;
  if (aGeneric) {
    nsAutoString realFace;
    MapGenericFamilyToFont(aFamily, data->mContext, realFace);
    FontEnumHelper(data, &realFace);
    return PR_TRUE;  // don't stop
  }
  else {
    nsAutoString realFace;
    PRBool  aliased;
    data->mContext->GetLocalFontName(aFamily, realFace, aliased);
    if (aliased || (NS_OK == data->mContext->CheckFontExistence(realFace))) {
      FontEnumHelper(data, &realFace);
      return PR_TRUE;  // don't stop
    }
  }
  return PR_TRUE;
}

#endif /* FONT_SWITCHING */

void
nsFontMetricsWin::RealizeFont()
{
  LOGFONT logFont;
  FillLogFont(&logFont);
  logFont.lfFaceName[0] = '\0';

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

#ifdef FONT_SWITCHING
  FontEnumData  data(mDeviceContext, logFont.lfFaceName, this, dc);
#else
  FontEnumData  data(mDeviceContext, logFont.lfFaceName);
#endif
  mFont->EnumerateFamilies(FontEnumCallback, &data); 

  // Create font handle from font spec
  mFontHandle = ::CreateFontIndirect(&logFont);

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
nsFontMetricsWin :: GetXHeight(nscoord& aResult)
{
  aResult = mXHeight;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin :: GetSuperscriptOffset(nscoord& aResult)
{
  aResult = mSuperscriptOffset;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin :: GetSubscriptOffset(nscoord& aResult)
{
  aResult = mSubscriptOffset;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin :: GetStrikeout(nscoord& aOffset, nscoord& aSize)
{
  aOffset = mStrikeoutOffset;
  aSize = mStrikeoutSize;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin :: GetUnderline(nscoord& aOffset, nscoord& aSize)
{
  aOffset = mUnderlineOffset;
  aSize = mUnderlineSize;
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
