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

nsGlobalFont* nsFontMetricsWin::gGlobalFonts = nsnull;
static int gGlobalFontsAlloc = 0;
int nsFontMetricsWin::gGlobalFontsCount = 0;

PLHashTable* nsFontMetricsWin::gFamilyNames = nsnull;

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
    nsFontWin** font = mLoadedFonts;
    nsFontWin** end = &mLoadedFonts[mLoadedFontsCount];
    while (font < end) {
      if ((*font)->font) {
        ::DeleteObject((*font)->font);
      }
      delete *font;
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

PRUint8*
nsFontMetricsWin::GetCMAP(HDC aDC)
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
    if (mLoadedFontsCount == mLoadedFontsAlloc) {
      int newSize = 2 * (mLoadedFontsAlloc ? mLoadedFontsAlloc : 1);
      nsFontWin** newPointer = (nsFontWin**) PR_Realloc(mLoadedFonts,
        newSize * sizeof(nsFontWin*));
      if (newPointer) {
        mLoadedFonts = newPointer;
        mLoadedFontsAlloc = newSize;
      }
      else {
        ::DeleteObject(hfont);
        return nsnull;
      }
    }
    nsFontWin* font = new nsFontWin;
    if (!font) {
      ::DeleteObject(hfont);
      return nsnull;
    }
    mLoadedFonts[mLoadedFontsCount++] = font;
    HFONT oldFont = (HFONT) ::SelectObject(aDC, (HGDIOBJ) hfont);
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
  if (nsFontMetricsWin::gGlobalFontsCount == gGlobalFontsAlloc) {
    int newSize = 2 * (gGlobalFontsAlloc ? gGlobalFontsAlloc : 1);
    nsGlobalFont* newPointer = (nsGlobalFont*)
      PR_Realloc(nsFontMetricsWin::gGlobalFonts, newSize*sizeof(nsGlobalFont));
    if (newPointer) {
      nsFontMetricsWin::gGlobalFonts = newPointer;
      gGlobalFontsAlloc = newSize;
    }
    else {
      return 0;
    }
  }
  nsGlobalFont* font =
    &nsFontMetricsWin::gGlobalFonts[nsFontMetricsWin::gGlobalFontsCount++];

  // XXX do correct character encoding conversion here
  font->name = new nsString(logFont->lfFaceName);
  if (!font->name) {
    nsFontMetricsWin::gGlobalFontsCount--;
    return 0;
  }
  font->map = nsnull;
  font->logFont = *logFont;
  font->skip = 0;

  return 1;
}

nsGlobalFont*
nsFontMetricsWin::InitializeGlobalFonts(HDC aDC)
{
  static int gInitialized = 0;
  if (!gInitialized) {
    gInitialized = 1;
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

  return gGlobalFonts;
}

int
nsFontMetricsWin::SameAsPreviousMap(int aIndex)
{
  for (int i = 0; i < aIndex; i++) {
    if (!gGlobalFonts[i].skip) {
      if (gGlobalFonts[i].map == gGlobalFonts[aIndex].map) {
        gGlobalFonts[aIndex].skip = 1;
        return 1;
      }
      PRUint8* map1 = gGlobalFonts[i].map;
      PRUint8* map2 = gGlobalFonts[aIndex].map;
      int j;
      for (j = 0; j < 8192; j++) {
        if (map1[j] != map2[j]) {
          break;
        }
      }
      if (j == 8192) {
        gGlobalFonts[aIndex].skip = 1;
        return 1;
      }
    }
  }

  return 0;
}

nsFontWin*
nsFontMetricsWin::FindGlobalFont(HDC aDC, PRUnichar c)
{
  if (!gGlobalFonts) {
    if (!InitializeGlobalFonts(aDC)) {
      return nsnull;
    }
  }
  for (int i = 0; i < gGlobalFontsCount; i++) {
    if (!gGlobalFonts[i].skip) {
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
        if (SameAsPreviousMap(i)) {
          continue;
        }
      }
      if (FONT_HAS_GLYPH(gGlobalFonts[i].map, c)) {
        return LoadFont(aDC, gGlobalFonts[i].name);
      }
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

PLHashTable*
nsFontMetricsWin::InitializeFamilyNames(void)
{
  static int gInitialized = 0;
  if (!gInitialized) {
    gInitialized = 1;
    gFamilyNames = PL_NewHashTable(0, HashKey, CompareKeys, nsnull, nsnull,
      nsnull);
    if (!gFamilyNames) {
      return nsnull;
    }
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

  return gFamilyNames;
}

nsFontWin*
nsFontMetricsWin::FindLocalFont(HDC aDC, PRUnichar aChar)
{
  if (!gFamilyNames) {
    if (!InitializeFamilyNames()) {
      return nsnull;
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


// The following is a workaround for a Japanese Windows 95 problem.

PRUint8 bitToCharSet[64] =
{
/*00*/ ANSI_CHARSET,
/*01*/ EASTEUROPE_CHARSET,
/*02*/ RUSSIAN_CHARSET,
/*03*/ GREEK_CHARSET,
/*04*/ TURKISH_CHARSET,
/*05*/ HEBREW_CHARSET,
/*06*/ ARABIC_CHARSET,
/*07*/ BALTIC_CHARSET,
/*08*/ DEFAULT_CHARSET,
/*09*/ DEFAULT_CHARSET,
/*10*/ DEFAULT_CHARSET,
/*11*/ DEFAULT_CHARSET,
/*12*/ DEFAULT_CHARSET,
/*13*/ DEFAULT_CHARSET,
/*14*/ DEFAULT_CHARSET,
/*15*/ DEFAULT_CHARSET,
/*16*/ THAI_CHARSET,
/*17*/ SHIFTJIS_CHARSET,
/*18*/ GB2312_CHARSET,
/*19*/ HANGEUL_CHARSET,
/*20*/ CHINESEBIG5_CHARSET,
/*21*/ JOHAB_CHARSET,
/*22*/ DEFAULT_CHARSET,
/*23*/ DEFAULT_CHARSET,
/*24*/ DEFAULT_CHARSET,
/*25*/ DEFAULT_CHARSET,
/*26*/ DEFAULT_CHARSET,
/*27*/ DEFAULT_CHARSET,
/*28*/ DEFAULT_CHARSET,
/*29*/ DEFAULT_CHARSET,
/*30*/ DEFAULT_CHARSET,
/*31*/ DEFAULT_CHARSET,
/*32*/ DEFAULT_CHARSET,
/*33*/ DEFAULT_CHARSET,
/*34*/ DEFAULT_CHARSET,
/*35*/ DEFAULT_CHARSET,
/*36*/ DEFAULT_CHARSET,
/*37*/ DEFAULT_CHARSET,
/*38*/ DEFAULT_CHARSET,
/*39*/ DEFAULT_CHARSET,
/*40*/ DEFAULT_CHARSET,
/*41*/ DEFAULT_CHARSET,
/*42*/ DEFAULT_CHARSET,
/*43*/ DEFAULT_CHARSET,
/*44*/ DEFAULT_CHARSET,
/*45*/ DEFAULT_CHARSET,
/*46*/ DEFAULT_CHARSET,
/*47*/ DEFAULT_CHARSET,
/*48*/ DEFAULT_CHARSET,
/*49*/ DEFAULT_CHARSET,
/*50*/ DEFAULT_CHARSET,
/*51*/ DEFAULT_CHARSET,
/*52*/ DEFAULT_CHARSET,
/*53*/ DEFAULT_CHARSET,
/*54*/ DEFAULT_CHARSET,
/*55*/ DEFAULT_CHARSET,
/*56*/ DEFAULT_CHARSET,
/*57*/ DEFAULT_CHARSET,
/*58*/ DEFAULT_CHARSET,
/*59*/ DEFAULT_CHARSET,
/*60*/ DEFAULT_CHARSET,
/*61*/ DEFAULT_CHARSET,
/*62*/ DEFAULT_CHARSET,
/*63*/ DEFAULT_CHARSET
};

enum nsCharSet
{
  eCharSet_DEFAULT = 0,
  eCharSet_ANSI,
  eCharSet_EASTEUROPE,
  eCharSet_RUSSIAN,
  eCharSet_GREEK,
  eCharSet_TURKISH,
  eCharSet_HEBREW,
  eCharSet_ARABIC,
  eCharSet_BALTIC,
  eCharSet_THAI,
  eCharSet_SHIFTJIS,
  eCharSet_GB2312,
  eCharSet_HANGEUL,
  eCharSet_CHINESEBIG5,
  eCharSet_JOHAB,
  eCharSet_COUNT
};

static nsCharSet gCharSetToIndex[256] =
{
  /* 000 */ eCharSet_ANSI,
  /* 001 */ eCharSet_DEFAULT,
  /* 002 */ eCharSet_DEFAULT, // SYMBOL
  /* 003 */ eCharSet_DEFAULT,
  /* 004 */ eCharSet_DEFAULT,
  /* 005 */ eCharSet_DEFAULT,
  /* 006 */ eCharSet_DEFAULT,
  /* 007 */ eCharSet_DEFAULT,
  /* 008 */ eCharSet_DEFAULT,
  /* 009 */ eCharSet_DEFAULT,
  /* 010 */ eCharSet_DEFAULT,
  /* 011 */ eCharSet_DEFAULT,
  /* 012 */ eCharSet_DEFAULT,
  /* 013 */ eCharSet_DEFAULT,
  /* 014 */ eCharSet_DEFAULT,
  /* 015 */ eCharSet_DEFAULT,
  /* 016 */ eCharSet_DEFAULT,
  /* 017 */ eCharSet_DEFAULT,
  /* 018 */ eCharSet_DEFAULT,
  /* 019 */ eCharSet_DEFAULT,
  /* 020 */ eCharSet_DEFAULT,
  /* 021 */ eCharSet_DEFAULT,
  /* 022 */ eCharSet_DEFAULT,
  /* 023 */ eCharSet_DEFAULT,
  /* 024 */ eCharSet_DEFAULT,
  /* 025 */ eCharSet_DEFAULT,
  /* 026 */ eCharSet_DEFAULT,
  /* 027 */ eCharSet_DEFAULT,
  /* 028 */ eCharSet_DEFAULT,
  /* 029 */ eCharSet_DEFAULT,
  /* 030 */ eCharSet_DEFAULT,
  /* 031 */ eCharSet_DEFAULT,
  /* 032 */ eCharSet_DEFAULT,
  /* 033 */ eCharSet_DEFAULT,
  /* 034 */ eCharSet_DEFAULT,
  /* 035 */ eCharSet_DEFAULT,
  /* 036 */ eCharSet_DEFAULT,
  /* 037 */ eCharSet_DEFAULT,
  /* 038 */ eCharSet_DEFAULT,
  /* 039 */ eCharSet_DEFAULT,
  /* 040 */ eCharSet_DEFAULT,
  /* 041 */ eCharSet_DEFAULT,
  /* 042 */ eCharSet_DEFAULT,
  /* 043 */ eCharSet_DEFAULT,
  /* 044 */ eCharSet_DEFAULT,
  /* 045 */ eCharSet_DEFAULT,
  /* 046 */ eCharSet_DEFAULT,
  /* 047 */ eCharSet_DEFAULT,
  /* 048 */ eCharSet_DEFAULT,
  /* 049 */ eCharSet_DEFAULT,
  /* 050 */ eCharSet_DEFAULT,
  /* 051 */ eCharSet_DEFAULT,
  /* 052 */ eCharSet_DEFAULT,
  /* 053 */ eCharSet_DEFAULT,
  /* 054 */ eCharSet_DEFAULT,
  /* 055 */ eCharSet_DEFAULT,
  /* 056 */ eCharSet_DEFAULT,
  /* 057 */ eCharSet_DEFAULT,
  /* 058 */ eCharSet_DEFAULT,
  /* 059 */ eCharSet_DEFAULT,
  /* 060 */ eCharSet_DEFAULT,
  /* 061 */ eCharSet_DEFAULT,
  /* 062 */ eCharSet_DEFAULT,
  /* 063 */ eCharSet_DEFAULT,
  /* 064 */ eCharSet_DEFAULT,
  /* 065 */ eCharSet_DEFAULT,
  /* 066 */ eCharSet_DEFAULT,
  /* 067 */ eCharSet_DEFAULT,
  /* 068 */ eCharSet_DEFAULT,
  /* 069 */ eCharSet_DEFAULT,
  /* 070 */ eCharSet_DEFAULT,
  /* 071 */ eCharSet_DEFAULT,
  /* 072 */ eCharSet_DEFAULT,
  /* 073 */ eCharSet_DEFAULT,
  /* 074 */ eCharSet_DEFAULT,
  /* 075 */ eCharSet_DEFAULT,
  /* 076 */ eCharSet_DEFAULT,
  /* 077 */ eCharSet_DEFAULT, // MAC
  /* 078 */ eCharSet_DEFAULT,
  /* 079 */ eCharSet_DEFAULT,
  /* 080 */ eCharSet_DEFAULT,
  /* 081 */ eCharSet_DEFAULT,
  /* 082 */ eCharSet_DEFAULT,
  /* 083 */ eCharSet_DEFAULT,
  /* 084 */ eCharSet_DEFAULT,
  /* 085 */ eCharSet_DEFAULT,
  /* 086 */ eCharSet_DEFAULT,
  /* 087 */ eCharSet_DEFAULT,
  /* 088 */ eCharSet_DEFAULT,
  /* 089 */ eCharSet_DEFAULT,
  /* 090 */ eCharSet_DEFAULT,
  /* 091 */ eCharSet_DEFAULT,
  /* 092 */ eCharSet_DEFAULT,
  /* 093 */ eCharSet_DEFAULT,
  /* 094 */ eCharSet_DEFAULT,
  /* 095 */ eCharSet_DEFAULT,
  /* 096 */ eCharSet_DEFAULT,
  /* 097 */ eCharSet_DEFAULT,
  /* 098 */ eCharSet_DEFAULT,
  /* 099 */ eCharSet_DEFAULT,
  /* 100 */ eCharSet_DEFAULT,
  /* 101 */ eCharSet_DEFAULT,
  /* 102 */ eCharSet_DEFAULT,
  /* 103 */ eCharSet_DEFAULT,
  /* 104 */ eCharSet_DEFAULT,
  /* 105 */ eCharSet_DEFAULT,
  /* 106 */ eCharSet_DEFAULT,
  /* 107 */ eCharSet_DEFAULT,
  /* 108 */ eCharSet_DEFAULT,
  /* 109 */ eCharSet_DEFAULT,
  /* 110 */ eCharSet_DEFAULT,
  /* 111 */ eCharSet_DEFAULT,
  /* 112 */ eCharSet_DEFAULT,
  /* 113 */ eCharSet_DEFAULT,
  /* 114 */ eCharSet_DEFAULT,
  /* 115 */ eCharSet_DEFAULT,
  /* 116 */ eCharSet_DEFAULT,
  /* 117 */ eCharSet_DEFAULT,
  /* 118 */ eCharSet_DEFAULT,
  /* 119 */ eCharSet_DEFAULT,
  /* 120 */ eCharSet_DEFAULT,
  /* 121 */ eCharSet_DEFAULT,
  /* 122 */ eCharSet_DEFAULT,
  /* 123 */ eCharSet_DEFAULT,
  /* 124 */ eCharSet_DEFAULT,
  /* 125 */ eCharSet_DEFAULT,
  /* 126 */ eCharSet_DEFAULT,
  /* 127 */ eCharSet_DEFAULT,
  /* 128 */ eCharSet_SHIFTJIS,
  /* 129 */ eCharSet_HANGEUL,
  /* 130 */ eCharSet_JOHAB,
  /* 131 */ eCharSet_DEFAULT,
  /* 132 */ eCharSet_DEFAULT,
  /* 133 */ eCharSet_DEFAULT,
  /* 134 */ eCharSet_GB2312,
  /* 135 */ eCharSet_DEFAULT,
  /* 136 */ eCharSet_CHINESEBIG5,
  /* 137 */ eCharSet_DEFAULT,
  /* 138 */ eCharSet_DEFAULT,
  /* 139 */ eCharSet_DEFAULT,
  /* 140 */ eCharSet_DEFAULT,
  /* 141 */ eCharSet_DEFAULT,
  /* 142 */ eCharSet_DEFAULT,
  /* 143 */ eCharSet_DEFAULT,
  /* 144 */ eCharSet_DEFAULT,
  /* 145 */ eCharSet_DEFAULT,
  /* 146 */ eCharSet_DEFAULT,
  /* 147 */ eCharSet_DEFAULT,
  /* 148 */ eCharSet_DEFAULT,
  /* 149 */ eCharSet_DEFAULT,
  /* 150 */ eCharSet_DEFAULT,
  /* 151 */ eCharSet_DEFAULT,
  /* 152 */ eCharSet_DEFAULT,
  /* 153 */ eCharSet_DEFAULT,
  /* 154 */ eCharSet_DEFAULT,
  /* 155 */ eCharSet_DEFAULT,
  /* 156 */ eCharSet_DEFAULT,
  /* 157 */ eCharSet_DEFAULT,
  /* 158 */ eCharSet_DEFAULT,
  /* 159 */ eCharSet_DEFAULT,
  /* 160 */ eCharSet_DEFAULT,
  /* 161 */ eCharSet_GREEK,
  /* 162 */ eCharSet_TURKISH,
  /* 163 */ eCharSet_DEFAULT, // VIETNAMESE
  /* 164 */ eCharSet_DEFAULT,
  /* 165 */ eCharSet_DEFAULT,
  /* 166 */ eCharSet_DEFAULT,
  /* 167 */ eCharSet_DEFAULT,
  /* 168 */ eCharSet_DEFAULT,
  /* 169 */ eCharSet_DEFAULT,
  /* 170 */ eCharSet_DEFAULT,
  /* 171 */ eCharSet_DEFAULT,
  /* 172 */ eCharSet_DEFAULT,
  /* 173 */ eCharSet_DEFAULT,
  /* 174 */ eCharSet_DEFAULT,
  /* 175 */ eCharSet_DEFAULT,
  /* 176 */ eCharSet_DEFAULT,
  /* 177 */ eCharSet_HEBREW,
  /* 178 */ eCharSet_ARABIC,
  /* 179 */ eCharSet_DEFAULT,
  /* 180 */ eCharSet_DEFAULT,
  /* 181 */ eCharSet_DEFAULT,
  /* 182 */ eCharSet_DEFAULT,
  /* 183 */ eCharSet_DEFAULT,
  /* 184 */ eCharSet_DEFAULT,
  /* 185 */ eCharSet_DEFAULT,
  /* 186 */ eCharSet_BALTIC,
  /* 187 */ eCharSet_DEFAULT,
  /* 188 */ eCharSet_DEFAULT,
  /* 189 */ eCharSet_DEFAULT,
  /* 190 */ eCharSet_DEFAULT,
  /* 191 */ eCharSet_DEFAULT,
  /* 192 */ eCharSet_DEFAULT,
  /* 193 */ eCharSet_DEFAULT,
  /* 194 */ eCharSet_DEFAULT,
  /* 195 */ eCharSet_DEFAULT,
  /* 196 */ eCharSet_DEFAULT,
  /* 197 */ eCharSet_DEFAULT,
  /* 198 */ eCharSet_DEFAULT,
  /* 199 */ eCharSet_DEFAULT,
  /* 200 */ eCharSet_DEFAULT,
  /* 201 */ eCharSet_DEFAULT,
  /* 202 */ eCharSet_DEFAULT,
  /* 203 */ eCharSet_DEFAULT,
  /* 204 */ eCharSet_RUSSIAN,
  /* 205 */ eCharSet_DEFAULT,
  /* 206 */ eCharSet_DEFAULT,
  /* 207 */ eCharSet_DEFAULT,
  /* 208 */ eCharSet_DEFAULT,
  /* 209 */ eCharSet_DEFAULT,
  /* 210 */ eCharSet_DEFAULT,
  /* 211 */ eCharSet_DEFAULT,
  /* 212 */ eCharSet_DEFAULT,
  /* 213 */ eCharSet_DEFAULT,
  /* 214 */ eCharSet_DEFAULT,
  /* 215 */ eCharSet_DEFAULT,
  /* 216 */ eCharSet_DEFAULT,
  /* 217 */ eCharSet_DEFAULT,
  /* 218 */ eCharSet_DEFAULT,
  /* 219 */ eCharSet_DEFAULT,
  /* 220 */ eCharSet_DEFAULT,
  /* 221 */ eCharSet_DEFAULT,
  /* 222 */ eCharSet_THAI,
  /* 223 */ eCharSet_DEFAULT,
  /* 224 */ eCharSet_DEFAULT,
  /* 225 */ eCharSet_DEFAULT,
  /* 226 */ eCharSet_DEFAULT,
  /* 227 */ eCharSet_DEFAULT,
  /* 228 */ eCharSet_DEFAULT,
  /* 229 */ eCharSet_DEFAULT,
  /* 230 */ eCharSet_DEFAULT,
  /* 231 */ eCharSet_DEFAULT,
  /* 232 */ eCharSet_DEFAULT,
  /* 233 */ eCharSet_DEFAULT,
  /* 234 */ eCharSet_DEFAULT,
  /* 235 */ eCharSet_DEFAULT,
  /* 236 */ eCharSet_DEFAULT,
  /* 237 */ eCharSet_DEFAULT,
  /* 238 */ eCharSet_EASTEUROPE,
  /* 239 */ eCharSet_DEFAULT,
  /* 240 */ eCharSet_DEFAULT,
  /* 241 */ eCharSet_DEFAULT,
  /* 242 */ eCharSet_DEFAULT,
  /* 243 */ eCharSet_DEFAULT,
  /* 244 */ eCharSet_DEFAULT,
  /* 245 */ eCharSet_DEFAULT,
  /* 246 */ eCharSet_DEFAULT,
  /* 247 */ eCharSet_DEFAULT,
  /* 248 */ eCharSet_DEFAULT,
  /* 249 */ eCharSet_DEFAULT,
  /* 250 */ eCharSet_DEFAULT,
  /* 251 */ eCharSet_DEFAULT,
  /* 252 */ eCharSet_DEFAULT,
  /* 253 */ eCharSet_DEFAULT,
  /* 254 */ eCharSet_DEFAULT,
  /* 255 */ eCharSet_DEFAULT  // OEM
};

typedef struct nsCharSetInfo nsCharSetInfo;

struct nsCharSetInfo
{
  char*    mName;
  PRUint16 mCodePage;
  void     (*GenerateMap)(nsCharSetInfo* aSelf);
  PRUint8* mMap;
};

static void
GenerateDefault(nsCharSetInfo* aSelf)
{ 
printf("%s defaulted\n", aSelf->mName);
  PRUint8* map = aSelf->mMap;
  for (int i = 0; i < 8192; i++) {
    map[i] = 0xFF;
  }
}

static void
GenerateSingleByte(nsCharSetInfo* aSelf)
{ 
  PRUint8 mb[256];
  PRUint16 wc[256];
  int i;
  for (i = 0; i < 256; i++) {
    mb[i] = i;
  }
  int len = MultiByteToWideChar(aSelf->mCodePage, 0, (char*) mb, 256, wc, 256);
  if (len != 256) {
    printf("%s: MultiByteToWideChar returned %d\n", aSelf->mName, len);
  }
  PRUint8* map = aSelf->mMap;
  for (i = 0; i < 256; i++) {
    ADD_GLYPH(map, wc[i]);
  }
}

static void
GenerateMultiByte(nsCharSetInfo* aSelf)
{ 
  PRUint8* map = aSelf->mMap;
  for (PRUint16 c = 0; c < 0xFFFF; c++) {
    BOOL defaulted = FALSE;
    WideCharToMultiByte(aSelf->mCodePage, 0, &c, 1, nsnull, 0, nsnull,
      &defaulted);
    if (!defaulted) {
      ADD_GLYPH(map, c);
    }
  }
}

static nsCharSetInfo gCharSetInfo[eCharSet_COUNT] =
{
  { "DEFAULT",     0,    GenerateDefault },
  { "ANSI",        1252, GenerateSingleByte },
  { "EASTEUROPE",  1250, GenerateSingleByte },
  { "RUSSIAN",     1251, GenerateSingleByte },
  { "GREEK",       1253, GenerateSingleByte },
  { "TURKISH",     1254, GenerateSingleByte },
  { "HEBREW",      1255, GenerateSingleByte },
  { "ARABIC",      1256, GenerateSingleByte },
  { "BALTIC",      1257, GenerateSingleByte },
  { "THAI",        874,  GenerateSingleByte },
  { "SHIFTJIS",    932,  GenerateMultiByte },
  { "GB2312",      936,  GenerateMultiByte },
  { "HANGEUL",     949,  GenerateMultiByte },
  { "CHINESEBIG5", 950,  GenerateMultiByte },
  { "JOHAB",       1361, GenerateMultiByte }
};

static int
HaveConverterFor(PRUint8 aCharSet)
{
  PRUint16 wc = 'a';
  char mb[8];
  if (WideCharToMultiByte(gCharSetInfo[gCharSetToIndex[aCharSet]].mCodePage, 0,
                          &wc, 1, mb, sizeof(mb), nsnull, nsnull)) {
    return 1;
  }

  // remove from table, since we can't support it anyway
  for (int i = 0; i < sizeof(bitToCharSet); i++) {
    if (bitToCharSet[i] == aCharSet) {
      bitToCharSet[i] = DEFAULT_CHARSET;
    }
  }

  return 0;
}

int
nsFontWinA::GetSubsets(HDC aDC)
{
  FONTSIGNATURE signature;
  if (::GetTextCharsetInfo(aDC, &signature, 0) == DEFAULT_CHARSET) {
    return 0;
  }

  int dword;
  DWORD* array = signature.fsCsb;
  mSubsetsCount = 0;
  int i = 0;
  for (dword = 0; dword < 2; dword++) {
    for (int bit = 0; bit < sizeof(DWORD) * 8; bit++) {
      if ((array[dword] >> bit) & 1) {
        PRUint8 charSet = bitToCharSet[i];
#ifdef DEBUG_FONT_SIGNATURE
        printf("  %02d %s\n", i, gCharSetInfo[gCharSetToIndex[charSet]].mName);
#endif
        if (charSet != DEFAULT_CHARSET) {
          if (HaveConverterFor(charSet)) {
            mSubsetsCount++;
          }
        }
      }
      i++;
    }
  }

  mSubsets = (nsFontSubset*) PR_Calloc(mSubsetsCount, sizeof(nsFontSubset));
  if (!mSubsets) {
    mSubsetsCount = 0;
    return 0;
  }

  i = 0;
  int j = 0;
  for (dword = 0; dword < 2; dword++) {
    for (int bit = 0; bit < sizeof(DWORD) * 8; bit++) {
      if ((array[dword] >> bit) & 1) {
        PRUint8 charSet = bitToCharSet[i];
        if (charSet != DEFAULT_CHARSET) {
          if (HaveConverterFor(charSet)) {
            mSubsets[j].mCharSet = charSet;
            j++;
          }
        }
      }
      i++;
    }
  }

  return 1;
}

static void
FreeFont(nsFontWinA* aFont)
{
  nsFontSubset* subset = aFont->mSubsets;
  nsFontSubset* endSubsets = &(aFont->mSubsets[aFont->mSubsetsCount]);
  while (subset < endSubsets) {
    if (subset->mFont) {
      ::DeleteObject(subset->mFont);
    }
    if (subset->mMap) {
      PR_Free(subset->mMap);
    }
    subset++;
  }
  PR_Free(aFont->mSubsets);
  if (aFont->mFont) {
    ::DeleteObject(aFont->mFont);
  }
  delete aFont;
}

nsFontMetricsWinA::~nsFontMetricsWinA()
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
    nsFontWinA** font = (nsFontWinA**) mLoadedFonts;
    nsFontWinA** end = (nsFontWinA**) &mLoadedFonts[mLoadedFontsCount];
    while (font < end) {
      FreeFont(*font);
      font++;
    }
    PR_Free(mLoadedFonts);
    mLoadedFonts = nsnull;
  }

  mDeviceContext = nsnull;
}

nsFontWin*
nsFontMetricsWinA::LoadFont(HDC aDC, nsString* aName)
{
  LOGFONT logFont;
  FillLogFont(&logFont);

  // XXX need to preserve Unicode chars in face name (use LOGFONTW) -- erik
  aName->ToCString(logFont.lfFaceName, LF_FACESIZE);

  HFONT hfont = ::CreateFontIndirect(&logFont);

  if (hfont) {
    if (mLoadedFontsCount == mLoadedFontsAlloc) {
      int newSize = 2 * (mLoadedFontsAlloc ? mLoadedFontsAlloc : 1);
      nsFontWinA** newPointer = (nsFontWinA**) PR_Realloc(mLoadedFonts,
        newSize * sizeof(nsFontWinA*));
      if (newPointer) {
        mLoadedFonts = (nsFontWin**) newPointer;
        mLoadedFontsAlloc = newSize;
      }
      else {
        ::DeleteObject(hfont);
        return nsnull;
      }
    }
    nsFontWinA* font = new nsFontWinA;
    if (!font) {
      ::DeleteObject(hfont);
      return nsnull;
    }
    mLoadedFonts[mLoadedFontsCount++] = (nsFontWin*) font;
    HFONT oldFont = (HFONT) ::SelectObject(aDC, (HGDIOBJ) hfont);
    font->mFont = hfont;
    font->mLogFont = logFont;
#ifdef DEBUG_FONT_SIGNATURE
    printf("%s\n", logFont.lfFaceName);
#endif
    if (!font->GetSubsets(aDC)) {
      mLoadedFontsCount--;
      ::SelectObject(aDC, (HGDIOBJ) oldFont);
      ::DeleteObject(hfont);
      return nsnull;
    }
    font->mMap = GetCMAP(aDC);
    if (!font->mMap) {
      mLoadedFontsCount--;
      ::SelectObject(aDC, (HGDIOBJ) oldFont);
      ::DeleteObject(hfont);
      return nsnull;
    }
    ::SelectObject(aDC, (HGDIOBJ) oldFont);

    return (nsFontWin*) font;
  }

  return nsnull;
}

int
nsFontSubset::Load(nsFontWinA* aFont)
{
  LOGFONT* logFont = &aFont->mLogFont;
  logFont->lfCharSet = mCharSet;
  HFONT hfont = ::CreateFontIndirect(logFont);
  if (hfont) {
    int i = gCharSetToIndex[mCharSet];
    PRUint8* charSetMap = gCharSetInfo[i].mMap;
    if (!charSetMap) {
      charSetMap = (PRUint8*) PR_Calloc(8192, 1);
      if (charSetMap) {
        gCharSetInfo[i].mMap = charSetMap;
        gCharSetInfo[i].GenerateMap(&gCharSetInfo[i]);
      }
      else {
        ::DeleteObject(hfont);
        return 0;
      }
    }
    mMap = (PRUint8*) PR_Malloc(8192);
    if (!mMap) {
      ::DeleteObject(hfont);
      return 0;
    }
    PRUint8* fontMap = aFont->mMap;
    for (int j = 0; j < 8192; j++) {
      mMap[j] = (charSetMap[j] & fontMap[j]);
    }
    mCodePage = gCharSetInfo[i].mCodePage;
    mFont = hfont;
    return 1;
  }
  return 0;
}

nsFontWin*
nsFontMetricsWinA::FindLocalFont(HDC aDC, PRUnichar aChar)
{
  if (!gFamilyNames) {
    if (!InitializeFamilyNames()) {
      return nsnull;
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
      nsFontWinA* font = (nsFontWinA*) LoadFont(aDC, winName);
      if (font && FONT_HAS_GLYPH(font->mMap, aChar)) {
        nsFontSubset* subset = font->mSubsets;
        nsFontSubset* endSubsets = &(font->mSubsets[font->mSubsetsCount]);
        while (subset < endSubsets) {
          if (!subset->mMap) {
            if (!subset->Load(font)) {
              subset++;
              continue;
            }
          }
          if (FONT_HAS_GLYPH(subset->mMap, aChar)) {
            return (nsFontWin*) subset;
          }
          subset++;
        }
      }
    }
  }

  return nsnull;
}

nsFontWin*
nsFontMetricsWinA::FindGlobalFont(HDC aDC, PRUnichar c)
{
  if (!gGlobalFonts) {
    if (!InitializeGlobalFonts(aDC)) {
      return nsnull;
    }
  }
  for (int i = 0; i < gGlobalFontsCount; i++) {
    if (!gGlobalFonts[i].skip) {
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
        if (SameAsPreviousMap(i)) {
          continue;
        }
      }
      if (FONT_HAS_GLYPH(gGlobalFonts[i].map, c)) {
        nsFontWinA* font = (nsFontWinA*) LoadFont(aDC, gGlobalFonts[i].name);
        if (font) {
          nsFontSubset* subset = font->mSubsets;
          nsFontSubset* endSubsets = &(font->mSubsets[font->mSubsetsCount]);
          while (subset < endSubsets) {
            if (!subset->mMap) {
              if (!subset->Load(font)) {
                subset++;
                continue;
              }
            }
            if (FONT_HAS_GLYPH(subset->mMap, c)) {
              return (nsFontWin*) subset;
            }
            subset++;
          }
          mLoadedFontsCount--;
          FreeFont(font);
        }
      }
    }
  }

  return nsnull;
}
