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
#include "plhash.h"

static NS_DEFINE_IID(kIFontMetricsIID, NS_IFONT_METRICS_IID);

typedef struct nsGlobalFont
{
  nsString*     name;
  LOGFONT       logFont;
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
  if (nsnull != mFont) {
    delete mFont;
    mFont = nsnull;
  }

  mFontHandle = nsnull; // released below

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

#undef CMAP
#define CMAP (('c') | ('m' << 8) | ('a' << 16) | ('p' << 24))
#undef HEAD
#define HEAD (('h') | ('e' << 8) | ('a' << 16) | ('d' << 24))
#undef LOCA
#define LOCA (('l') | ('o' << 8) | ('c' << 16) | ('a' << 24))
#undef NAME
#define NAME (('n') | ('a' << 8) | ('m' << 16) | ('e' << 24))

#undef GET_SHORT
#define GET_SHORT(p) (((p)[0] << 8) | (p)[1])
#undef GET_LONG
#define GET_LONG(p) (((p)[0] << 24) | ((p)[1] << 16) | ((p)[2] << 8) | (p)[3])

static PRUint16
GetGlyph(PRUint16 segCount, PRUint16* endCode, PRUint16* startCode,
  PRUint16* idRangeOffset, PRUint16* idDelta, PRUint8* end, PRUint16 aChar)
{
  PRUint16 glyph = 0;
  PRUint16 i;
  for (i = 0; i < segCount; i++) {
    if (endCode[i] >= aChar) {
      break;
    }
  }
  PRUint16 startC = startCode[i];
  if (startC <= aChar) {
    if (idRangeOffset[i]) {
      PRUint16* p =
        (idRangeOffset[i]/2 + (aChar - startC) + &idRangeOffset[i]);
      if ((PRUint8*) p < end) {
        if (*p) {
          glyph = idDelta[i] + *p;
        }
      }
    }
    else {
      glyph = idDelta[i] + aChar;
    }
  }

  return glyph;
}

static int
GetNAME(HDC aDC, nsString* aName)
{
  DWORD len = GetFontData(aDC, NAME, 0, nsnull, 0);
  if ((len == GDI_ERROR) || (!len)) {
    return 0;
  }
  PRUint8* buf = (PRUint8*) PR_Malloc(len);
  if (!buf) {
    return 0;
  }
  DWORD newLen = GetFontData(aDC, NAME, 0, buf, len);
  if (newLen != len) {
    PR_Free(buf);
    return 0;
  }
  PRUint8* p = buf + 2;
  PRUint16 n = GET_SHORT(p);
  p += 2;
  PRUint16 offset = GET_SHORT(p);
  p += 2;
  PRUint16 i;
  PRUint16 idLength;
  PRUint16 idOffset;
  for (i = 0; i < n; i++) {
    PRUint16 platform = GET_SHORT(p);
    p += 2;
    PRUint16 encoding = GET_SHORT(p);
    p += 4;
    PRUint16 name = GET_SHORT(p);
    p += 2;
    idLength = GET_SHORT(p);
    p += 2;
    idOffset = GET_SHORT(p);
    p += 2;
    // XXX what about symbol? (platform 3, encoding 0)
    if ((platform == 3) && (encoding == 1) && (name == 3)) {
      break;
    }
  }
  if (i == n) {
    PR_Free(buf);
    return 0;
  }
  p = buf + offset + idOffset;
  idLength /= 2;
  for (i = 0; i < idLength; i++) {
    PRUnichar c = GET_SHORT(p);
    p += 2;
    aName->Append(c);
  }

  PR_Free(buf);

  return 1;
}

static PLHashNumber
HashKey(const void* aString)
{
  return (PLHashNumber)
    nsCRT::HashValue(((const nsString*) aString)->GetUnicode());
}

static PRIntn
CompareKeys(const void* aStr1, const void* aStr2)
{
  return nsCRT::strcmp(((const nsString*) aStr1)->GetUnicode(),
    ((const nsString*) aStr2)->GetUnicode()) == 0;
}

static int
GetIndexToLocFormat(HDC aDC)
{
  PRUint16 indexToLocFormat;
  if (GetFontData(aDC, HEAD, 50, &indexToLocFormat, 2) != 2) {
    return -1;
  }
  if (!indexToLocFormat) {
    return 0;
  }
  return 1;
}

static PRUint8*
GetSpaces(HDC aDC)
{
  int isLong = GetIndexToLocFormat(aDC);
  if (isLong < 0) {
    return nsnull;
  }
  DWORD len = GetFontData(aDC, LOCA, 0, nsnull, 0);
  if ((len == GDI_ERROR) || (!len)) {
    return nsnull;
  }
  PRUint8* buf = (PRUint8*) PR_Malloc(len);
  if (!buf) {
    return nsnull;
  }
  DWORD newLen = GetFontData(aDC, LOCA, 0, buf, len);
  if (newLen != len) {
    PR_Free(buf);
    return nsnull;
  }
  if (isLong) {
    DWORD longLen = ((len / 4) - 1);
    PRUint32* longBuf = (PRUint32*) buf;
    for (PRUint32 i = 0; i < longLen; i++) {
      if (longBuf[i] == longBuf[i+1]) {
        buf[i] = 1;
      }
      else {
        buf[i] = 0;
      }
    }
  }
  else {
    DWORD shortLen = ((len / 2) - 1);
    PRUint16* shortBuf = (PRUint16*) buf;
    for (PRUint16 i = 0; i < shortLen; i++) {
      if (shortBuf[i] == shortBuf[i+1]) {
        buf[i] = 1;
      }
      else {
        buf[i] = 0;
      }
    }
  }

  return buf;
}

#undef SET_SPACE
#define SET_SPACE(c) ADD_GLYPH(spaces, c)
#undef SHOULD_BE_SPACE
#define SHOULD_BE_SPACE(c) FONT_HAS_GLYPH(spaces, c)

static PRUint8*
GetCMAP(HDC aDC)
{
  static PLHashTable* fontMaps = nsnull;
  static int initialized = 0;
  if (!initialized) {
    initialized = 1;
    fontMaps = PL_NewHashTable(0, HashKey, CompareKeys, nsnull, nsnull,
      nsnull);
  }
  nsString* name = new nsString();
  if (!name) {
    return nsnull;
  }
  PRUint8* map;
  if (GetNAME(aDC, name)) {
    map = (PRUint8*) PL_HashTableLookup(fontMaps, name);
    if (map) {
      delete name;
      return map;
    }
    map = (PRUint8*) PR_Calloc(8192, 1);
    if (!map) {
      delete name;
      return nsnull;
    }
  }
  else {
    // return an empty map, so that we never try this font again
    static PRUint8* emptyMap = nsnull;
    if (!emptyMap) {
      emptyMap = (PRUint8*) PR_Calloc(8192, 1);
    }
    delete name;
    return emptyMap;
  }

  DWORD len = GetFontData(aDC, CMAP, 0, nsnull, 0);
  if ((len == GDI_ERROR) || (!len)) {
    delete name;
    PR_Free(map);
    return nsnull;
  }
  PRUint8* buf = (PRUint8*) PR_Malloc(len);
  if (!buf) {
    delete name;
    PR_Free(map);
    return nsnull;
  }
  DWORD newLen = GetFontData(aDC, CMAP, 0, buf, len);
  if (newLen != len) {
    PR_Free(buf);
    delete name;
    PR_Free(map);
    return nsnull;
  }
  PRUint8* p = buf + 2;
  PRUint16 n = GET_SHORT(p);
  p += 2;
  PRUint16 i;
  PRUint32 offset;
  for (i = 0; i < n; i++) {
    PRUint16 platformID = GET_SHORT(p);
    p += 2;
    PRUint16 encodingID = GET_SHORT(p);
    p += 2;
    offset = GET_LONG(p);
    p += 4;
    // XXX what about symbol fonts? (platform = 3, encoding = 0)
    if ((platformID == 3) && (encodingID == 1)) {
      break;
    }
  }
  if (i == n) {
    PR_Free(buf);
    delete name;
    PR_Free(map);
    return nsnull;
  }
  p = buf + offset;
  PRUint16 format = GET_SHORT(p);
  if (format != 4) {
    PR_Free(buf);
    delete name;
    PR_Free(map);
    return nsnull;
  }
  PRUint8* end = buf + len;

  // XXX byte swapping only required for little endian (ifdef?)
  while (p < end) {
    PRUint8 tmp = p[0];
    p[0] = p[1];
    p[1] = tmp;
    p += 2;
  }

  PRUint16* s = (PRUint16*) (buf + offset);
  PRUint16 segCount = s[3] / 2;
  PRUint16* endCode = &s[7];
  PRUint16* startCode = endCode + segCount + 1;
  PRUint16* idDelta = startCode + segCount;
  PRUint16* idRangeOffset = idDelta + segCount;
  PRUint16* glyphIdArray = idRangeOffset + segCount;

  static int spacesInitialized = 0;
  static PRUint8 spaces[8192];
  if (!spacesInitialized) {
    spacesInitialized = 1;
    SET_SPACE(0x0020);
    SET_SPACE(0x00A0);
    for (PRUint16 c = 0x2000; c <= 0x200B; c++) {
      SET_SPACE(c);
    }
    SET_SPACE(0x3000);
  }
  PRUint8* isSpace = GetSpaces(aDC);
  if (!isSpace) {
    PR_Free(buf);
    delete name;
    PR_Free(map);
    return nsnull;
  }

  for (i = 0; i < segCount; i++) {
    if (idRangeOffset[i]) {
      PRUint16 startC = startCode[i];
      PRUint16 endC = endCode[i];
      for (PRUint32 c = startC; c <= endC; c++) {
        PRUint16* g =
          (idRangeOffset[i]/2 + (c - startC) + &idRangeOffset[i]);
        if ((PRUint8*) g < end) {
          if (*g) {
            PRUint16 glyph = idDelta[i] + *g;
            if (isSpace[glyph]) {
              if (SHOULD_BE_SPACE(c)) {
                ADD_GLYPH(map, c);
              }
            }
            else {
              ADD_GLYPH(map, c);
            }
          }
        }
        else {
          // XXX should we trust this font at all if it does this?
        }
      }
      //printf("0x%04X-0x%04X ", startC, endC);
    }
    else {
      PRUint16 endC = endCode[i];
      for (PRUint32 c = startCode[i]; c <= endC; c++) {
        PRUint16 glyph = idDelta[i] + c;
        if (isSpace[glyph]) {
          if (SHOULD_BE_SPACE(c)) {
            ADD_GLYPH(map, c);
          }
        }
        else {
          ADD_GLYPH(map, c);
        }
      }
      //printf("0x%04X-0x%04X ", startCode[i], endC);
    }
  }
  //printf("\n");

  PR_Free(buf);
  PR_Free(isSpace);

  // XXX check to see if an identical map has already been added to table
  PL_HashTableAdd(fontMaps, name, map);

  return map;
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
    font->map = GetCMAP(aDC);
    if (!font->map) {
      mLoadedFontsCount--;
      ::SelectObject(aDC, (HGDIOBJ) oldFont);
      ::DeleteObject(hfont);
      return nsnull;
    }
    ::SelectObject(aDC, (HGDIOBJ) oldFont);

    return font;
  }

  return nsnull;
}

static int CALLBACK enumProc(const LOGFONT* logFont, const TEXTMETRIC* metrics,
  DWORD fontType, LPARAM closure)
{
  // XXX do we really want to ignore non-TrueType fonts?
  if (!(fontType & TRUETYPE_FONTTYPE)) {
    //printf("rejecting %s\n", logFont->lfFaceName);
    return 1;
  }

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
  if (!font->name) {
    gGlobalFontsCount--;
    return 0;
  }
  font->map = nsnull;
  font->logFont = *logFont;

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

    /*
     * msdn.microsoft.com/library states that
     * EnumFontFamiliesExW is only on NT/2000
     */
    EnumFontFamiliesEx(aDC, &logFont, enumProc, nsnull, 0);
  }
  for (int i = 0; i < gGlobalFontsCount; i++) {
    if (!gGlobalFonts[i].map) {
      HFONT font = ::CreateFontIndirect(&gGlobalFonts[i].logFont);
      if (!font) {
        continue;
      }
      HFONT oldFont = (HFONT) ::SelectObject(aDC, font);
      gGlobalFonts[i].map = GetCMAP(aDC);
      ::SelectObject(aDC, oldFont);
      ::DeleteObject(font);
      if (!gGlobalFonts[i].map) {
        continue;
      }
    }
    if (FONT_HAS_GLYPH(gGlobalFonts[i].map, c)) {
      return LoadFont(aDC, gGlobalFonts[i].name);
    }
  }

  return nsnull;
}

typedef struct nsFontFamilyName
{
  char* mName;
  char* mWinName;
} nsFontFamilyName;

static nsFontFamilyName gFamilyNameTable[] =
{
  { "times",           "Times New Roman" },
  { "times roman",     "Times New Roman" },
  { "times new roman", "Times New Roman" },
  { "arial",           "Arial" },
  { "helvetica",       "Arial" },
  { "courier",         "Courier New" },
  { "courier new",     "Courier New" },

  { "serif",      "Times New Roman" },
  { "sans-serif", "Arial" },
  { "fantasy",    "Arial" },
  { "cursive",    "Arial" },
  { "monospace",  "Courier New" },

  { nsnull, nsnull }
};

static PLHashTable* gFamilyNames = nsnull;

nsFontWin*
nsFontMetricsWin::FindLocalFont(HDC aDC, PRUnichar aChar)
{
  static int gInitialized = 0;
  if (!gInitialized) {
    gInitialized = 1;
    gFamilyNames = PL_NewHashTable(0, HashKey, CompareKeys, nsnull, nsnull,
      nsnull);
    nsFontFamilyName* f = gFamilyNameTable;
    while (f->mName) {
      nsString* name = new nsString(f->mName);
      nsString* winName = new nsString(f->mWinName);
      if (name && winName) {
        PL_HashTableAdd(gFamilyNames, name, (void*) winName);
      }
      f++;
    }
  }

  while (mFontsIndex < mFontsCount) {
    nsString* name = &mFonts[mFontsIndex++];
    nsString* low = new nsString(*name);
    if (low) {
      low->ToLowerCase();
      nsString* winName = (nsString*) PL_HashTableLookup(gFamilyNames, low);
      delete low;
      if (!winName) {
        winName = name;
      }
      nsFontWin* font = LoadFont(aDC, winName);
      if (font && FONT_HAS_GLYPH(font->map, aChar)) {
        return font;
      }
    }
  }

  return nsnull;
}

nsFontWin*
nsFontMetricsWin::FindFont(HDC aDC, PRUnichar aChar)
{
  nsFontWin* font = FindLocalFont(aDC, aChar);
  if (!font) {
    font = FindGlobalFont(aDC, aChar);
  }

  return font;
}

static PRBool
FontEnumCallback(const nsString& aFamily, PRBool aGeneric, void *aData)
{
  nsFontMetricsWin* metrics = (nsFontMetricsWin*) aData;
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
      return PR_FALSE; // stop
    }
  }
  metrics->mFonts[metrics->mFontsCount++].SetString(aFamily.GetUnicode());

  if (aGeneric) {
    return PR_FALSE; // stop
  }

  return PR_TRUE; // don't stop
}

void
nsFontMetricsWin::RealizeFont()
{
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

  mFont->EnumerateFamilies(FontEnumCallback, this); 

  nsFontWin* font = FindFont(dc, 'a');
  if (!font) {
    return;
  }
  mFontHandle = font->font;

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
