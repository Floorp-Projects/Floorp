/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsFontMetricsWin.h"
#include "nsQuickSort.h"
#include "prmem.h"
#include "plhash.h"

static NS_DEFINE_IID(kIFontMetricsIID, NS_IFONT_METRICS_IID);

nsGlobalFont* nsFontMetricsWin::gGlobalFonts = nsnull;
static int gGlobalFontsAlloc = 0;
int nsFontMetricsWin::gGlobalFontsCount = 0;

PLHashTable* nsFontMetricsWin::gFamilyNames = nsnull;

//-- Font weight
PLHashTable* nsFontMetricsWin::gFontWeights = nsnull;

#define NS_MAX_FONT_WEIGHT 900
#define NS_MIN_FONT_WEIGHT 100

#undef CHAR_BUFFER_SIZE
#define CHAR_BUFFER_SIZE 1024

nsFontMetricsWin :: nsFontMetricsWin()
{
  NS_INIT_REFCNT();
  mSpaceWidth = 0;
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

nsresult nsFontMetricsWin :: GetSpaceWidth(nscoord &aSpaceWidth)
{
  aSpaceWidth = mSpaceWidth;
  return NS_OK;
}

void
nsFontMetricsWin::FillLogFont(LOGFONT* logFont, PRInt32 aWeight)
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
  logFont->lfOutPrecision   = OUT_TT_PRECIS;
  logFont->lfClipPrecision  = CLIP_DEFAULT_PRECIS;
  logFont->lfQuality        = DEFAULT_QUALITY;
  logFont->lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
  logFont->lfWeight = aWeight;
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
    // encoding: 1 == Unicode; 0 == symbol
    if ((platform == 3) && ((encoding == 1) || (!encoding)) && (name == 3)) {
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
GetSpaces(HDC aDC, PRUint32* aMaxGlyph)
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
    *aMaxGlyph = longLen;
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
    *aMaxGlyph = shortLen;
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

static PRUint16 SymbolTable[] =
{
  0x0020, 0x20, // SPACE	# space
  0x00A0, 0x20, // NO-BREAK SPACE	# space
  0x0021, 0x21, // EXCLAMATION MARK	# exclam
  0x2200, 0x22, // FOR ALL	# universal
  0x0023, 0x23, // NUMBER SIGN	# numbersign
  0x2203, 0x24, // THERE EXISTS	# existential
  0x0025, 0x25, // PERCENT SIGN	# percent
  0x0026, 0x26, // AMPERSAND	# ampersand
  0x220B, 0x27, // CONTAINS AS MEMBER	# suchthat
  0x0028, 0x28, // LEFT PARENTHESIS	# parenleft
  0x0029, 0x29, // RIGHT PARENTHESIS	# parenright
  0x2217, 0x2A, // ASTERISK OPERATOR	# asteriskmath
  0x002B, 0x2B, // PLUS SIGN	# plus
  0x002C, 0x2C, // COMMA	# comma
  0x2212, 0x2D, // MINUS SIGN	# minus
  0x002E, 0x2E, // FULL STOP	# period
  0x002F, 0x2F, // SOLIDUS	# slash
  0x0030, 0x30, // DIGIT ZERO	# zero
  0x0031, 0x31, // DIGIT ONE	# one
  0x0032, 0x32, // DIGIT TWO	# two
  0x0033, 0x33, // DIGIT THREE	# three
  0x0034, 0x34, // DIGIT FOUR	# four
  0x0035, 0x35, // DIGIT FIVE	# five
  0x0036, 0x36, // DIGIT SIX	# six
  0x0037, 0x37, // DIGIT SEVEN	# seven
  0x0038, 0x38, // DIGIT EIGHT	# eight
  0x0039, 0x39, // DIGIT NINE	# nine
  0x003A, 0x3A, // COLON	# colon
  0x003B, 0x3B, // SEMICOLON	# semicolon
  0x003C, 0x3C, // LESS-THAN SIGN	# less
  0x003D, 0x3D, // EQUALS SIGN	# equal
  0x003E, 0x3E, // GREATER-THAN SIGN	# greater
  0x003F, 0x3F, // QUESTION MARK	# question
  0x2245, 0x40, // APPROXIMATELY EQUAL TO	# congruent
  0x0391, 0x41, // GREEK CAPITAL LETTER ALPHA	# Alpha
  0x0392, 0x42, // GREEK CAPITAL LETTER BETA	# Beta
  0x03A7, 0x43, // GREEK CAPITAL LETTER CHI	# Chi
  0x0394, 0x44, // GREEK CAPITAL LETTER DELTA	# Delta
  0x2206, 0x44, // INCREMENT	# Delta
  0x0395, 0x45, // GREEK CAPITAL LETTER EPSILON	# Epsilon
  0x03A6, 0x46, // GREEK CAPITAL LETTER PHI	# Phi
  0x0393, 0x47, // GREEK CAPITAL LETTER GAMMA	# Gamma
  0x0397, 0x48, // GREEK CAPITAL LETTER ETA	# Eta
  0x0399, 0x49, // GREEK CAPITAL LETTER IOTA	# Iota
  0x03D1, 0x4A, // GREEK THETA SYMBOL	# theta1
  0x039A, 0x4B, // GREEK CAPITAL LETTER KAPPA	# Kappa
  0x039B, 0x4C, // GREEK CAPITAL LETTER LAMDA	# Lambda
  0x039C, 0x4D, // GREEK CAPITAL LETTER MU	# Mu
  0x039D, 0x4E, // GREEK CAPITAL LETTER NU	# Nu
  0x039F, 0x4F, // GREEK CAPITAL LETTER OMICRON	# Omicron
  0x03A0, 0x50, // GREEK CAPITAL LETTER PI	# Pi
  0x0398, 0x51, // GREEK CAPITAL LETTER THETA	# Theta
  0x03A1, 0x52, // GREEK CAPITAL LETTER RHO	# Rho
  0x03A3, 0x53, // GREEK CAPITAL LETTER SIGMA	# Sigma
  0x03A4, 0x54, // GREEK CAPITAL LETTER TAU	# Tau
  0x03A5, 0x55, // GREEK CAPITAL LETTER UPSILON	# Upsilon
  0x03C2, 0x56, // GREEK SMALL LETTER FINAL SIGMA	# sigma1
  0x03A9, 0x57, // GREEK CAPITAL LETTER OMEGA	# Omega
  0x2126, 0x57, // OHM SIGN	# Omega
  0x039E, 0x58, // GREEK CAPITAL LETTER XI	# Xi
  0x03A8, 0x59, // GREEK CAPITAL LETTER PSI	# Psi
  0x0396, 0x5A, // GREEK CAPITAL LETTER ZETA	# Zeta
  0x005B, 0x5B, // LEFT SQUARE BRACKET	# bracketleft
  0x2234, 0x5C, // THEREFORE	# therefore
  0x005D, 0x5D, // RIGHT SQUARE BRACKET	# bracketright
  0x22A5, 0x5E, // UP TACK	# perpendicular
  0x005F, 0x5F, // LOW LINE	# underscore
  0xF8E5, 0x60, // RADICAL EXTENDER	# radicalex (CUS)
  0x03B1, 0x61, // GREEK SMALL LETTER ALPHA	# alpha
  0x03B2, 0x62, // GREEK SMALL LETTER BETA	# beta
  0x03C7, 0x63, // GREEK SMALL LETTER CHI	# chi
  0x03B4, 0x64, // GREEK SMALL LETTER DELTA	# delta
  0x03B5, 0x65, // GREEK SMALL LETTER EPSILON	# epsilon
  0x03C6, 0x66, // GREEK SMALL LETTER PHI	# phi
  0x03B3, 0x67, // GREEK SMALL LETTER GAMMA	# gamma
  0x03B7, 0x68, // GREEK SMALL LETTER ETA	# eta
  0x03B9, 0x69, // GREEK SMALL LETTER IOTA	# iota
  0x03D5, 0x6A, // GREEK PHI SYMBOL	# phi1
  0x03BA, 0x6B, // GREEK SMALL LETTER KAPPA	# kappa
  0x03BB, 0x6C, // GREEK SMALL LETTER LAMDA	# lambda
  0x00B5, 0x6D, // MICRO SIGN	# mu
  0x03BC, 0x6D, // GREEK SMALL LETTER MU	# mu
  0x03BD, 0x6E, // GREEK SMALL LETTER NU	# nu
  0x03BF, 0x6F, // GREEK SMALL LETTER OMICRON	# omicron
  0x03C0, 0x70, // GREEK SMALL LETTER PI	# pi
  0x03B8, 0x71, // GREEK SMALL LETTER THETA	# theta
  0x03C1, 0x72, // GREEK SMALL LETTER RHO	# rho
  0x03C3, 0x73, // GREEK SMALL LETTER SIGMA	# sigma
  0x03C4, 0x74, // GREEK SMALL LETTER TAU	# tau
  0x03C5, 0x75, // GREEK SMALL LETTER UPSILON	# upsilon
  0x03D6, 0x76, // GREEK PI SYMBOL	# omega1
  0x03C9, 0x77, // GREEK SMALL LETTER OMEGA	# omega
  0x03BE, 0x78, // GREEK SMALL LETTER XI	# xi
  0x03C8, 0x79, // GREEK SMALL LETTER PSI	# psi
  0x03B6, 0x7A, // GREEK SMALL LETTER ZETA	# zeta
  0x007B, 0x7B, // LEFT CURLY BRACKET	# braceleft
  0x007C, 0x7C, // VERTICAL LINE	# bar
  0x007D, 0x7D, // RIGHT CURLY BRACKET	# braceright
  0x223C, 0x7E, // TILDE OPERATOR	# similar
  0x20AC, 0xA0, // EURO SIGN	# Euro
  0x03D2, 0xA1, // GREEK UPSILON WITH HOOK SYMBOL	# Upsilon1
  0x2032, 0xA2, // PRIME	# minute
  0x2264, 0xA3, // LESS-THAN OR EQUAL TO	# lessequal
  0x2044, 0xA4, // FRACTION SLASH	# fraction
  0x2215, 0xA4, // DIVISION SLASH	# fraction
  0x221E, 0xA5, // INFINITY	# infinity
  0x0192, 0xA6, // LATIN SMALL LETTER F WITH HOOK	# florin
  0x2663, 0xA7, // BLACK CLUB SUIT	# club
  0x2666, 0xA8, // BLACK DIAMOND SUIT	# diamond
  0x2665, 0xA9, // BLACK HEART SUIT	# heart
  0x2660, 0xAA, // BLACK SPADE SUIT	# spade
  0x2194, 0xAB, // LEFT RIGHT ARROW	# arrowboth
  0x2190, 0xAC, // LEFTWARDS ARROW	# arrowleft
  0x2191, 0xAD, // UPWARDS ARROW	# arrowup
  0x2192, 0xAE, // RIGHTWARDS ARROW	# arrowright
  0x2193, 0xAF, // DOWNWARDS ARROW	# arrowdown
  0x00B0, 0xB0, // DEGREE SIGN	# degree
  0x00B1, 0xB1, // PLUS-MINUS SIGN	# plusminus
  0x2033, 0xB2, // DOUBLE PRIME	# second
  0x2265, 0xB3, // GREATER-THAN OR EQUAL TO	# greaterequal
  0x00D7, 0xB4, // MULTIPLICATION SIGN	# multiply
  0x221D, 0xB5, // PROPORTIONAL TO	# proportional
  0x2202, 0xB6, // PARTIAL DIFFERENTIAL	# partialdiff
  0x2022, 0xB7, // BULLET	# bullet
  0x00F7, 0xB8, // DIVISION SIGN	# divide
  0x2260, 0xB9, // NOT EQUAL TO	# notequal
  0x2261, 0xBA, // IDENTICAL TO	# equivalence
  0x2248, 0xBB, // ALMOST EQUAL TO	# approxequal
  0x2026, 0xBC, // HORIZONTAL ELLIPSIS	# ellipsis
  0xF8E6, 0xBD, // VERTICAL ARROW EXTENDER	# arrowvertex (CUS)
  0xF8E7, 0xBE, // HORIZONTAL ARROW EXTENDER	# arrowhorizex (CUS)
  0x21B5, 0xBF, // DOWNWARDS ARROW WITH CORNER LEFTWARDS	# carriagereturn
  0x2135, 0xC0, // ALEF SYMBOL	# aleph
  0x2111, 0xC1, // BLACK-LETTER CAPITAL I	# Ifraktur
  0x211C, 0xC2, // BLACK-LETTER CAPITAL R	# Rfraktur
  0x2118, 0xC3, // SCRIPT CAPITAL P	# weierstrass
  0x2297, 0xC4, // CIRCLED TIMES	# circlemultiply
  0x2295, 0xC5, // CIRCLED PLUS	# circleplus
  0x2205, 0xC6, // EMPTY SET	# emptyset
  0x2229, 0xC7, // INTERSECTION	# intersection
  0x222A, 0xC8, // UNION	# union
  0x2283, 0xC9, // SUPERSET OF	# propersuperset
  0x2287, 0xCA, // SUPERSET OF OR EQUAL TO	# reflexsuperset
  0x2284, 0xCB, // NOT A SUBSET OF	# notsubset
  0x2282, 0xCC, // SUBSET OF	# propersubset
  0x2286, 0xCD, // SUBSET OF OR EQUAL TO	# reflexsubset
  0x2208, 0xCE, // ELEMENT OF	# element
  0x2209, 0xCF, // NOT AN ELEMENT OF	# notelement
  0x2220, 0xD0, // ANGLE	# angle
  0x2207, 0xD1, // NABLA	# gradient
  0xF6DA, 0xD2, // REGISTERED SIGN SERIF	# registerserif (CUS)
  0xF6D9, 0xD3, // COPYRIGHT SIGN SERIF	# copyrightserif (CUS)
  0xF6DB, 0xD4, // TRADE MARK SIGN SERIF	# trademarkserif (CUS)
  0x220F, 0xD5, // N-ARY PRODUCT	# product
  0x221A, 0xD6, // SQUARE ROOT	# radical
  0x22C5, 0xD7, // DOT OPERATOR	# dotmath
  0x00AC, 0xD8, // NOT SIGN	# logicalnot
  0x2227, 0xD9, // LOGICAL AND	# logicaland
  0x2228, 0xDA, // LOGICAL OR	# logicalor
  0x21D4, 0xDB, // LEFT RIGHT DOUBLE ARROW	# arrowdblboth
  0x21D0, 0xDC, // LEFTWARDS DOUBLE ARROW	# arrowdblleft
  0x21D1, 0xDD, // UPWARDS DOUBLE ARROW	# arrowdblup
  0x21D2, 0xDE, // RIGHTWARDS DOUBLE ARROW	# arrowdblright
  0x21D3, 0xDF, // DOWNWARDS DOUBLE ARROW	# arrowdbldown
  0x25CA, 0xE0, // LOZENGE	# lozenge
  0x2329, 0xE1, // LEFT-POINTING ANGLE BRACKET	# angleleft
  0xF8E8, 0xE2, // REGISTERED SIGN SANS SERIF	# registersans (CUS)
  0xF8E9, 0xE3, // COPYRIGHT SIGN SANS SERIF	# copyrightsans (CUS)
  0xF8EA, 0xE4, // TRADE MARK SIGN SANS SERIF	# trademarksans (CUS)
  0x2211, 0xE5, // N-ARY SUMMATION	# summation
  0xF8EB, 0xE6, // LEFT PAREN TOP	# parenlefttp (CUS)
  0xF8EC, 0xE7, // LEFT PAREN EXTENDER	# parenleftex (CUS)
  0xF8ED, 0xE8, // LEFT PAREN BOTTOM	# parenleftbt (CUS)
  0xF8EE, 0xE9, // LEFT SQUARE BRACKET TOP	# bracketlefttp (CUS)
  0xF8EF, 0xEA, // LEFT SQUARE BRACKET EXTENDER	# bracketleftex (CUS)
  0xF8F0, 0xEB, // LEFT SQUARE BRACKET BOTTOM	# bracketleftbt (CUS)
  0xF8F1, 0xEC, // LEFT CURLY BRACKET TOP	# bracelefttp (CUS)
  0xF8F2, 0xED, // LEFT CURLY BRACKET MID	# braceleftmid (CUS)
  0xF8F3, 0xEE, // LEFT CURLY BRACKET BOTTOM	# braceleftbt (CUS)
  0xF8F4, 0xEF, // CURLY BRACKET EXTENDER	# braceex (CUS)
  0x232A, 0xF1, // RIGHT-POINTING ANGLE BRACKET	# angleright
  0x222B, 0xF2, // INTEGRAL	# integral
  0x2320, 0xF3, // TOP HALF INTEGRAL	# integraltp
  0xF8F5, 0xF4, // INTEGRAL EXTENDER	# integralex (CUS)
  0x2321, 0xF5, // BOTTOM HALF INTEGRAL	# integralbt
  0xF8F6, 0xF6, // RIGHT PAREN TOP	# parenrighttp (CUS)
  0xF8F7, 0xF7, // RIGHT PAREN EXTENDER	# parenrightex (CUS)
  0xF8F8, 0xF8, // RIGHT PAREN BOTTOM	# parenrightbt (CUS)
  0xF8F9, 0xF9, // RIGHT SQUARE BRACKET TOP	# bracketrighttp (CUS)
  0xF8FA, 0xFA, // RIGHT SQUARE BRACKET EXTENDER	# bracketrightex (CUS)
  0xF8FB, 0xFB, // RIGHT SQUARE BRACKET BOTTOM	# bracketrightbt (CUS)
  0xF8FC, 0xFC, // RIGHT CURLY BRACKET TOP	# bracerighttp (CUS)
  0xF8FD, 0xFD, // RIGHT CURLY BRACKET MID	# bracerightmid (CUS)
  0xF8FE, 0xFE, // RIGHT CURLY BRACKET BOTTOM	# bracerightbt (CUS)
};

static int
GetSymbolCMAP(const char* aName, PRUint8* aMap)
{
  if (!strcmp(aName, "Symbol")) {
    int end = sizeof(SymbolTable) / 2;
    for (int i = 0; i < end; i += 2) {
      ADD_GLYPH(aMap, SymbolTable[i]);
    }
    return 1;
  }

  return 0;
}

static PRUint8* SymbolConverter = nsnull;

static PRUint8*
GetSymbolConverter(const char* aName)
{
  if (!strcmp(aName, "Symbol")) {
    if (!SymbolConverter) {
      SymbolConverter = (PRUint8*) PR_Malloc(65536);
      if (!SymbolConverter) {
        return 0;
      }
    }
    int end = sizeof(SymbolTable) / 2;
    for (int i = 0; i < end; i += 2) {
      SymbolConverter[SymbolTable[i]] = (PRUint8) SymbolTable[i + 1];
    }
    return SymbolConverter;
  }

  return nsnull;
}

#undef SET_SPACE
#define SET_SPACE(c) ADD_GLYPH(spaces, c)
#undef SHOULD_BE_SPACE
#define SHOULD_BE_SPACE(c) FONT_HAS_GLYPH(spaces, c)

class nsFontInfo
{
public:
  nsFontInfo(int aIsUnicode, PRUint8* aMap)
  {
    mIsUnicode = aIsUnicode;
    mMap = aMap;
  };

  int      mIsUnicode;
  PRUint8* mMap;
};

PRUint8*
nsFontMetricsWin::GetCMAP(HDC aDC, const char* aShortName, int* aIsUnicode)
{
  static PLHashTable* fontMaps = nsnull;
  static PRUint8* emptyMap = nsnull;
  static int initialized = 0;
  if (!initialized) {
    initialized = 1;
    fontMaps = PL_NewHashTable(0, HashKey, CompareKeys, nsnull, nsnull,
      nsnull);
    emptyMap = (PRUint8*) PR_Calloc(8192, 1);
  }
  nsString* name = new nsString();
  if (!name) {
    return nsnull;
  }
  PRUint8* map;
  nsFontInfo* info;
  if (GetNAME(aDC, name)) {
    info = (nsFontInfo*) PL_HashTableLookup(fontMaps, name);
    if (info) {
      delete name;
      if (aIsUnicode) {
        *aIsUnicode = info->mIsUnicode;
      }
      return info->mMap;
   }
    map = (PRUint8*) PR_Calloc(8192, 1);
    if (!map) {
      delete name;
      return nsnull;
    }
  }
  else {
    // return an empty map, so that we never try this font again
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
    if (platformID == 3) {
      if (encodingID == 1) { // Unicode
        if (aIsUnicode) {
          *aIsUnicode = 1;
        }
        break;
      }
      else if (encodingID == 0) { // symbol
        if (aIsUnicode) {
          *aIsUnicode = 0;
        }
        if (!GetSymbolCMAP(aShortName, map)) {
          PR_Free(map);
          map = emptyMap;
        }
        PR_Free(buf);

        info = new nsFontInfo(0, map);
        if (info) {
          // XXX check to see if an identical map has already been added
          PL_HashTableAdd(fontMaps, name, info);
        }

        return map;
      }
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
  PRUint32 maxGlyph;
  PRUint8* isSpace = GetSpaces(aDC, &maxGlyph);
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
            if (glyph < maxGlyph) {
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
        if (glyph < maxGlyph) {
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
      //printf("0x%04X-0x%04X ", startCode[i], endC);
    }
  }
  //printf("\n");

  PR_Free(buf);
  PR_Free(isSpace);

  // XXX check to see if an identical map has already been added to table
  info = new nsFontInfo(1, map);
  if (info) {
    // XXX check to see if an identical map has already been added to table
    PL_HashTableAdd(fontMaps, name, info);
  }

  return map;
}

class nsFontWinUnicode : public nsFontWin
{
public:
  nsFontWinUnicode(LOGFONT* aLogFont, HFONT aFont, PRUint8* aMap);
  virtual ~nsFontWinUnicode();

  virtual PRInt32 GetWidth(HDC aDC, const PRUnichar* aString, PRUint32 aLength);
  virtual void DrawString(HDC aDC, PRInt32 aX, PRInt32 aY,
                          const PRUnichar* aString, PRUint32 aLength);
#ifdef MOZ_MATHML
  NS_IMETHOD
  GetBoundingMetrics(HDC                aDC, 
                     const PRUnichar*   aString,
                     PRUint32           aLength,
                     nsBoundingMetrics& aBoundingMetrics);
#endif
};

class nsFontWinNonUnicode : public nsFontWin
{
public:
  nsFontWinNonUnicode(LOGFONT* aLogFont, HFONT aFont, PRUint8* aMap);
  virtual ~nsFontWinNonUnicode();

  virtual PRInt32 GetWidth(HDC aDC, const PRUnichar* aString, PRUint32 aLength);
  virtual void DrawString(HDC aDC, PRInt32 aX, PRInt32 aY,
                          const PRUnichar* aString, PRUint32 aLength);
#ifdef MOZ_MATHML
  NS_IMETHOD
  GetBoundingMetrics(HDC                aDC, 
                     const PRUnichar*   aString,
                     PRUint32           aLength,
                     nsBoundingMetrics& aBoundingMetrics);
#endif

private:
  PRUint8* mConverter;
};

nsFontWin*
nsFontMetricsWin::LoadFont(HDC aDC, nsString* aName)
{
  LOGFONT logFont;

  PRUint16 weightTable = LookForFontWeightTable(aDC, aName);
  PRInt32 weight = GetFontWeight(mFont->weight, weightTable);
  FillLogFont(&logFont, weight);
 
  /*
   * XXX we are losing info by converting from Unicode to system code page
   * but we don't really have a choice since CreateFontIndirectW is
   * not supported on Windows 9X (see below) -- erik
   */
  logFont.lfFaceName[0] = 0;
  WideCharToMultiByte(CP_ACP, 0, aName->GetUnicode(), aName->Length() + 1,
    logFont.lfFaceName, sizeof(logFont.lfFaceName), nsnull, nsnull);

  /*
   * According to http://msdn.microsoft.com/library/
   * CreateFontIndirectW is only supported on NT/2000
   */
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
    HFONT oldFont = (HFONT) ::SelectObject(aDC, (HGDIOBJ) hfont);
    int isUnicodeFont = -1;
    PRUint8* map = GetCMAP(aDC, logFont.lfFaceName, &isUnicodeFont);
    if (!map) {
      ::SelectObject(aDC, (HGDIOBJ) oldFont);
      ::DeleteObject(hfont);
      return nsnull;
    }
    nsFontWin* font = nsnull;
    if (isUnicodeFont == 1) {
      font = new nsFontWinUnicode(&logFont, hfont, map);
    }
    else if (isUnicodeFont == 0) {
      font = new nsFontWinNonUnicode(&logFont, hfont, map);
    }
    if (!font) {
      ::SelectObject(aDC, (HGDIOBJ) oldFont);
      ::DeleteObject(hfont);
      return nsnull;
    }
    mLoadedFonts[mLoadedFontsCount++] = font;
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

  // XXX ignore vertical fonts
  if (logFont->lfFaceName[0] == '@') {
    return 1;
  }

  for (int i = 0; i < nsFontMetricsWin::gGlobalFontsCount; i++) {
    if (!strcmp(nsFontMetricsWin::gGlobalFonts[i].logFont.lfFaceName,
                logFont->lfFaceName)) {
      return 1;
    }
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

  PRUnichar name[LF_FACESIZE];
  name[0] = 0;
  MultiByteToWideChar(CP_ACP, 0, logFont->lfFaceName,
    strlen(logFont->lfFaceName) + 1, name, sizeof(name)/sizeof(name[0]));
  font->name = new nsString(name);
  if (!font->name) {
    nsFontMetricsWin::gGlobalFontsCount--;
    return 0;
  }
  font->map = nsnull;
  font->logFont = *logFont;
  font->skip = 0;
  font->signature = ((NEWTEXTMETRICEX*) metrics)->ntmFontSig;

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
        gGlobalFonts[i].map = GetCMAP(aDC, gGlobalFonts[i].logFont.lfFaceName,
          nsnull);
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

//------------ Font weight utilities -------------------

// XXX: Should not need to store all these in a hash table.
// We need to restructure the font management code so there is one
// global place to cache font info. As the code is right now, there
// are two separate places that font info is stored, in the gFamilyNames
// hash table and the global font array. There are also cases where the
// font info is not cached at all. 
// I initially tried to add the font weight info to those two places, but
// it was messy. In addition I discovered another code path which does not
// cache anything. 

// Entry for storing hash table. Store as a single
// entry rather than doing a separate allocation for the
// fontName and weight.


typedef struct nsFontWeightEntry
{
  nsString mFontName;
  PRUint16 mWeightTable; // Each bit indicates the availability of a font weight.
} nsFontWeightEntry;


static PLHashNumber
HashKeyFontWeight(const void* aFontWeightEntry)
{
  const nsString* string = &((const nsFontWeightEntry*) aFontWeightEntry)->mFontName;
  return (PLHashNumber)
    nsCRT::HashValue(string->GetUnicode());
}

static PRIntn
CompareKeysFontWeight(const void* aFontWeightEntry1, const void* aFontWeightEntry2)
{
  const nsString* str1 = &((const nsFontWeightEntry*) aFontWeightEntry1)->mFontName;
  const nsString* str2 = &((const nsFontWeightEntry*) aFontWeightEntry2)->mFontName;

  return nsCRT::strcmp(str1->GetUnicode(), str2->GetUnicode()) == 0;
}


// Store the font weight as a bit in the aWeightTable
void nsFontMetricsWin::SetFontWeight(PRInt32 aWeight, PRUint16* aWeightTable) {
  NS_ASSERTION((aWeight >= 0) && (aWeight <= 9), "Invalid font weight passed");
  *aWeightTable |= 1 << (aWeight - 1);
}

// Check to see if a font weight is available within the font weight table
PRBool nsFontMetricsWin::IsFontWeightAvailable(PRInt32 aWeight, PRUint16 aWeightTable) {
  PRInt32 normalizedWeight = aWeight / 100;
  NS_ASSERTION((aWeight >= 100) && (aWeight <= 900), "Invalid font weight passed");
  PRUint16 bitwiseWeight = 1 << (normalizedWeight - 1);
  if (bitwiseWeight & aWeightTable) {
    return PR_TRUE;
  }
  else {
    return PR_FALSE;
  }
}

static int CALLBACK nsFontWeightCallback(const LOGFONT* logFont, const TEXTMETRIC * metrics,
  DWORD fontType, LPARAM closure)
{
  // printf("Name %s Log font sizes %d\n",logFont->lfFaceName,logFont->lfWeight);
  if (!(fontType & TRUETYPE_FONTTYPE)) {
 //     printf("rejecting %s\n", logFont->lfFaceName);
    return TRUE;
  }
  
  if (NULL != metrics) {
    int pos = metrics->tmWeight / 100;
      // Set a bit to indicate the font weight is available
    nsFontMetricsWin::SetFontWeight(metrics->tmWeight / 100, (PRUint16*)closure);
  }

  return TRUE; // Keep looking for more weights.
}

PRUint16 
nsFontMetricsWin::GetFontWeightTable(HDC aDC, nsString* aFontName) {

    // Look for all of the weights for a given font.
  LOGFONT logFont;
  logFont.lfCharSet = DEFAULT_CHARSET;
  aFontName->ToCString(logFont.lfFaceName, LF_FACESIZE);
  logFont.lfPitchAndFamily = 0;

  PRUint16 weights = 0;
   ::EnumFontFamiliesEx(aDC, &logFont, nsFontWeightCallback, (LPARAM)&weights, 0);
//   printf("font weights for %s dec %d hex %x \n", logFont.lfFaceName, weights, weights);
   return weights;
}


// Calculate the closest font weight. This is necessary because we need to
// control the mapping of logical font weight to available weight to handle both CSS2
// default algorithm + the case where a font weight is choosen which is not available then made
// bolder or lighter. (e.g. a font weight of 200 is choosen but not available
// on the system so a weight of 400 is used instead when mapping to a physical font.
// If the font is made bolder we need to know that a font weight of 400 was choosen, so
// we can select a font weight which is greater. 
PRInt32
nsFontMetricsWin::GetClosestWeight(PRInt32 aWeight, PRUint16 aWeightTable)
{
  // Algorithm used From CSS2 section 15.5.1 Mapping font weight values to font names
  PRInt32 newWeight = aWeight;
   // Check for exact match
  if ((aWeight > 0) && (nsFontMetricsWin::IsFontWeightAvailable(aWeight, aWeightTable))) {
    return aWeight;
  }

    // Find lighter and darker weights to be used later.

    // First look for lighter
  PRInt32 lighterWeight = 0;
  PRInt32 proposedLighterWeight = PR_MAX(0, aWeight - 100);

  PRBool done = PR_FALSE;
  while ((PR_FALSE == done) && (proposedLighterWeight >= 100)) {
    if (nsFontMetricsWin::IsFontWeightAvailable(proposedLighterWeight, aWeightTable)) {
      lighterWeight = proposedLighterWeight;
      done = PR_TRUE;
    } else {
      proposedLighterWeight-= 100;
    }
  }

     // Now look for darker
  PRInt32 darkerWeight = 0;
  done = PR_FALSE;
  PRInt32 proposedDarkerWeight = PR_MIN(aWeight + 100, 900);
  while ((PR_FALSE == done) && (proposedDarkerWeight <= 900)) {
    if (nsFontMetricsWin::IsFontWeightAvailable(proposedDarkerWeight, aWeightTable)) {
      darkerWeight = proposedDarkerWeight;
      done = PR_TRUE;   
    } else {
      proposedDarkerWeight+= 100;
    }
  }

  // From CSS2 section 15.5.1 

  // If '500' is unassigned, it will be
  // assigned the same font as '400'.
  // If any of '300', '200', or '100' remains unassigned, it is
  // assigned to the next lighter assigned keyword, if any, or 
  // the next darker otherwise. 
  // What about if the desired  weight is 500 and 400 is unassigned?.
  // This is not inlcluded in the CSS spec so I'll treat it in a consistent
  // manner with unassigned '300', '200' and '100'


  if (aWeight <= 500) {
    if (0 != lighterWeight) {
      return lighterWeight;
    }
    else {
      return darkerWeight;
    }

  } else {

    // Automatically chose the bolder weight if the next lighter weight
    // makes it normal. (i.e goes over the normal to bold threshold.)

  // From CSS2 section 15.5.1 
  // if any of the values '600', '700', '800', or '900' remains unassigned, 
  // they are assigned to the same face as the next darker assigned keyword, 
  // if any, or the next lighter one otherwise.

    if (0 != darkerWeight) {
      return darkerWeight;
    } else {
      return lighterWeight;
    }
  }

  return aWeight;
}



PRInt32
nsFontMetricsWin::GetBolderWeight(PRInt32 aWeight, PRInt32 aDistance, PRUint16 aWeightTable)
{
  PRInt32 newWeight = aWeight;
 
  PRInt32 proposedWeight = aWeight + 100; // Start 1 bolder than the current
  for (PRInt32 j = 0; j < aDistance; j++) {
    PRBool aFoundAWeight = PR_FALSE;
    while ((proposedWeight <= NS_MAX_FONT_WEIGHT) && (PR_FALSE == aFoundAWeight)) {
      if (nsFontMetricsWin::IsFontWeightAvailable(proposedWeight, aWeightTable)) {
         // 
        newWeight = proposedWeight; 
        aFoundAWeight = PR_TRUE;
      }
      proposedWeight+=100; 
    }
  }

  return newWeight;
}

PRInt32
nsFontMetricsWin::GetLighterWeight(PRInt32 aWeight, PRInt32 aDistance, PRUint16 aWeightTable)
{
  PRInt32 newWeight = aWeight;
 
  PRInt32 proposedWeight = aWeight - 100; // Start 1 lighter than the current
  for (PRInt32 j = 0; j < aDistance; j++) {
    PRBool aFoundAWeight = PR_FALSE;
    while ((proposedWeight >= NS_MIN_FONT_WEIGHT) && (PR_FALSE == aFoundAWeight)) {
      if (nsFontMetricsWin::IsFontWeightAvailable(proposedWeight, aWeightTable)) {
         // 
        newWeight = proposedWeight; 
        aFoundAWeight = PR_TRUE;
      }
      proposedWeight-=100; 
    }
  }

  return newWeight;
}


PRInt32
nsFontMetricsWin::GetFontWeight(PRInt32 aWeight, PRUint16 aWeightTable) {

   // The remainder is used to determine whether to make
   // the font lighter or bolder

  PRInt32 remainder = aWeight % 100;
  PRInt32 normalizedWeight = aWeight / 100;
  PRInt32 selectedWeight = 0;

    // No remainder, so get the closest weight
  if (remainder == 0) {
    selectedWeight = GetClosestWeight(aWeight, aWeightTable);
  } else {

    NS_ASSERTION((remainder < 10) || (remainder > 90), "Invalid bolder or lighter value");

    if (remainder < 10) {
      PRInt32 weight = GetClosestWeight(normalizedWeight * 100, aWeightTable);
      selectedWeight = GetBolderWeight(weight, remainder, aWeightTable);
    } else {
       // Have to add back 1 for the lighter weight since aWeight really refers to the 
       // whole number. eq. 398 really means 2 lighter than font weight 400.
      PRInt32 weight = GetClosestWeight((normalizedWeight + 1) * 100, aWeightTable);
      selectedWeight = GetLighterWeight(weight, 100-remainder, aWeightTable);
    }
  }

//  printf("XXX Input weight %d output weight %d weight table hex %x\n", aWeight, selectedWeight, aWeightTable);
  return selectedWeight;
}


PRUint16
nsFontMetricsWin::LookForFontWeightTable(HDC aDC, nsString* aName)
{
  static int gInitializedFontWeights = 0;
 
    // Initialize the font weight table if need be.
  if (!gInitializedFontWeights) {
    gInitializedFontWeights = 1;
    gFontWeights = PL_NewHashTable(0, HashKeyFontWeight, CompareKeysFontWeight, nsnull, nsnull,
      nsnull);
    if (!gFontWeights) {
      return 0;
    }
  }

     // Use lower case name for hash table searches. This eliminates
     // keeping multiple font weights entries when the font name varies 
     // only by case.
  nsAutoString low = *aName;
  low.ToLowerCase();

   // See if the font weight has already been computed.
  nsFontWeightEntry searchEntry;
  searchEntry.mFontName = low;
  searchEntry.mWeightTable = 0;

  nsFontWeightEntry* weightEntry = (nsFontWeightEntry*)PL_HashTableLookup(gFontWeights, &searchEntry);
  if (nsnull != weightEntry) {
 //   printf("Re-use weight entry\n");
    return weightEntry->mWeightTable;
  }

   // Hasn't been computed, so need to compute and store it.
  PRUint16 weightTable = GetFontWeightTable(aDC, aName);
//  printf("Compute font weight %d\n",  weightTable);

    // Store it in font weight HashTable.
   nsFontWeightEntry* fontWeightEntry = new nsFontWeightEntry;
   fontWeightEntry->mFontName = low;
   fontWeightEntry->mWeightTable = weightTable;
   PL_HashTableAdd(gFontWeights, fontWeightEntry, fontWeightEntry);
     
   return weightTable;
}


// ------------ End of font weight utilities

typedef struct nsFontFamilyName
{
  char* mName;
  char* mWinName;
} nsFontFamilyName;

static nsFontFamilyName gFamilyNameTable[] =
{
#ifdef MOZ_MATHML
  { "-moz-math-text",   "Times New Roman" },
  { "-moz-math-symbol", "Symbol" },
#endif
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
  { "-moz-fixed", "Courier New" },

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
      if (font && FONT_HAS_GLYPH(font->mMap, aChar)) {
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

/** ---------------------------------------------------
 *  See documentation in nsFontMetricsWin.h
 *	@update 05/28/99 dwc
 */
void
nsFontMetricsWin::RealizeFont()
{
HWND  win = NULL;
HDC   dc = NULL;
HDC   dc1 = NULL;

  
  if (NULL != mDeviceContext->mDC){
    // XXX - DC If we are printing, we need to get the printer HDC and a screen HDC
    // The screen HDC is because there seems to be a bug or requirment that the 
    // GetFontData() method call have a screen HDC, some printers HDC's return nothing
    // thats will give us bad font data, and break us.  
    dc = mDeviceContext->mDC;
    win = (HWND)mDeviceContext->mWidget;
    dc1 = ::GetDC(win);
  } else {
    // Find font metrics and character widths
    win = (HWND)mDeviceContext->mWidget;
    dc = ::GetDC(win);
    dc1 = dc;
  }

  mFont->EnumerateFamilies(FontEnumCallback, this); 

  nsFontWin* font = FindFont(dc1, 'a');
  if (!font) {
    return;
  }
  mFontHandle = font->mFont;

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

    mStrikeoutSize = PR_MAX(onePixel, NSToCoordRound(oMetrics.otmsStrikeoutSize * dev2app));
    mStrikeoutOffset = NSToCoordRound(oMetrics.otmsStrikeoutPosition * dev2app);
    mUnderlineSize = PR_MAX(onePixel, NSToCoordRound(oMetrics.otmsUnderscoreSize * dev2app));
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

   // Cache the width of a single space.
  SIZE  size;
  ::GetTextExtentPoint32(dc, " ", 1, &size);
  mSpaceWidth = NSToCoordRound(size.cx * dev2app);

  ::SelectObject(dc, oldfont);

  if (NULL == mDeviceContext->mDC){
    ::ReleaseDC(win, dc);
  } else {
    ::ReleaseDC(win,dc1);
  }
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

nsFontWin::nsFontWin(LOGFONT* aLogFont, HFONT aFont, PRUint8* aMap)
{
  strcpy(mName, aLogFont->lfFaceName);
  mFont = aFont;
  mMap = aMap;
}

nsFontWin::~nsFontWin()
{
  if (mFont) {
    ::DeleteObject(mFont);
    mFont = nsnull;
  }
}

nsFontWinUnicode::nsFontWinUnicode(LOGFONT* aLogFont, HFONT aFont,
  PRUint8* aMap) : nsFontWin(aLogFont, aFont, aMap)
{
}

nsFontWinUnicode::~nsFontWinUnicode()
{
}

PRInt32
nsFontWinUnicode::GetWidth(HDC aDC, const PRUnichar* aString, PRUint32 aLength)
{
  HFONT oldFont = (HFONT) ::SelectObject(aDC, mFont);
  SIZE size;
  ::GetTextExtentPoint32W(aDC, aString, aLength, &size);
  ::SelectObject(aDC, oldFont);

  return size.cx;
}

void
nsFontWinUnicode::DrawString(HDC aDC, PRInt32 aX, PRInt32 aY,
  const PRUnichar* aString, PRUint32 aLength)
{
  HFONT oldFont = (HFONT) ::SelectObject(aDC, mFont);
  ::ExtTextOutW(aDC, aX, aY, 0, NULL, aString, aLength, NULL);
  ::SelectObject(aDC, oldFont);
}

#ifdef MOZ_MATHML
NS_IMETHODIMP
nsFontWinUnicode::GetBoundingMetrics(HDC                aDC, 
                                     const PRUnichar*   aString,
                                     PRUint32           aLength,
                                     nsBoundingMetrics& aBoundingMetrics)
{
  aBoundingMetrics.Clear();
  if (0 < aLength) {
    HFONT oldFont = (HFONT) ::SelectObject(aDC, mFont);
   
    // set glyph transform matrix to identity
    MAT2 mat2;
    FIXED zero, one;
    zero.fract = 0; one.fract = 0;
    zero.value = 0; one.value = 1; 
    mat2.eM12 = mat2.eM21 = zero; 
    mat2.eM11 = mat2.eM22 = one; 
   
    // measure the string
    GLYPHMETRICS gm;
    DWORD len = GetGlyphOutline(aDC, aString[0], GGO_METRICS, &gm, 0, nsnull, &mat2);
    if (GDI_ERROR == len) {
      return NS_ERROR_UNEXPECTED;
    }
    else {
      aBoundingMetrics.leftBearing = gm.gmptGlyphOrigin.x;
      aBoundingMetrics.rightBearing = gm.gmptGlyphOrigin.x + gm.gmBlackBoxX;
      aBoundingMetrics.ascent = gm.gmptGlyphOrigin.y;
      aBoundingMetrics.descent = gm.gmptGlyphOrigin.y - gm.gmBlackBoxY;
      aBoundingMetrics.width = gm.gmCellIncX;
    }
    if (1 < aLength) {
      // loop over each glyph to get the ascent and descent
      PRUint32 i;
      for (i = 1; i < aLength; i++) {
        len = GetGlyphOutline(aDC, aString[i], GGO_METRICS, &gm, 0, nsnull, &mat2);
        if (GDI_ERROR == len) {
          return NS_ERROR_UNEXPECTED;
        }
        else {
          if (aBoundingMetrics.ascent < gm.gmptGlyphOrigin.y)
            aBoundingMetrics.ascent = gm.gmptGlyphOrigin.y;
          if (aBoundingMetrics.descent > nscoord(gm.gmptGlyphOrigin.y - gm.gmBlackBoxY))
            aBoundingMetrics.descent = gm.gmptGlyphOrigin.y - gm.gmBlackBoxY;
        }
      }
      // get the final rightBearing and width. Possible kerning is taken into account.
      SIZE size;
      ::GetTextExtentPointW(aDC, aString, aLength, &size);
      aBoundingMetrics.width = size.cx;
      aBoundingMetrics.rightBearing = size.cx - gm.gmCellIncX + gm.gmBlackBoxX;
    }
   
    ::SelectObject(aDC, oldFont);
  }
  return NS_OK;
}      
#endif

nsFontWinNonUnicode::nsFontWinNonUnicode(LOGFONT* aLogFont, HFONT aFont,
  PRUint8* aMap) : nsFontWin(aLogFont, aFont, aMap)
{
  ::DeleteObject(mFont);
  aLogFont->lfCharSet = SYMBOL_CHARSET;
  mFont = ::CreateFontIndirect(aLogFont);
  mConverter = GetSymbolConverter(aLogFont->lfFaceName);
}

nsFontWinNonUnicode::~nsFontWinNonUnicode()
{
}

PRInt32
nsFontWinNonUnicode::GetWidth(HDC aDC, const PRUnichar* aString,
  PRUint32 aLength)
{
  if (mConverter && 0 < aLength) {
    char str[CHAR_BUFFER_SIZE];
    char* pstr = str;
    if (aLength > CHAR_BUFFER_SIZE) {
      pstr = new char[aLength];
      if (!pstr) return 0;
    }
    for (PRUint32 i = 0; i < aLength; i++) {
      pstr[i] = mConverter[aString[i]];
    }

    HFONT oldFont = (HFONT) ::SelectObject(aDC, mFont);
    SIZE size;
    ::GetTextExtentPoint32A(aDC, pstr, aLength, &size);
    ::SelectObject(aDC, oldFont);

    if (pstr != str) {
      delete[] pstr;
    }
    return size.cx;
  }
  return 0;
}

void
nsFontWinNonUnicode::DrawString(HDC aDC, PRInt32 aX, PRInt32 aY,
  const PRUnichar* aString, PRUint32 aLength)
{
  if (mConverter && 0 < aLength) {
    char str[CHAR_BUFFER_SIZE];
    char* pstr = str;
    if (aLength > CHAR_BUFFER_SIZE) {
      pstr = new char[aLength];
      if (!pstr) return;
    }
    for (PRUint32 i = 0; i < aLength; i++) {
      pstr[i] = mConverter[aString[i]];
    }

    HFONT oldFont = (HFONT) ::SelectObject(aDC, mFont);
    ::ExtTextOutA(aDC, aX, aY, 0, NULL, pstr, aLength, NULL);
    ::SelectObject(aDC, oldFont);

    if (pstr != str) {
      delete[] pstr;
    }
  }
}

#ifdef MOZ_MATHML
NS_IMETHODIMP
nsFontWinNonUnicode::GetBoundingMetrics(HDC                aDC, 
                                        const PRUnichar*   aString,
                                        PRUint32           aLength,
                                        nsBoundingMetrics& aBoundingMetrics)
{
  aBoundingMetrics.Clear();
  if (mConverter && 0 < aLength) {
    HFONT oldFont = (HFONT) ::SelectObject(aDC, mFont);

    // set glyph transform matrix to identity
    MAT2 mat2;
    FIXED zero, one;
    zero.fract = 0; one.fract = 0;
    zero.value = 0; one.value = 1; 
    mat2.eM12 = mat2.eM21 = zero; 
    mat2.eM11 = mat2.eM22 = one; 

    // measure the string
    GLYPHMETRICS gm;
    DWORD len = GetGlyphOutline(aDC, mConverter[aString[0]], GGO_METRICS, &gm, 0, nsnull, &mat2);
    if (GDI_ERROR == len) {
      return NS_ERROR_UNEXPECTED;
    }
    else {
      aBoundingMetrics.leftBearing = gm.gmptGlyphOrigin.x;
      aBoundingMetrics.rightBearing = gm.gmptGlyphOrigin.x + gm.gmBlackBoxX;
      aBoundingMetrics.ascent = gm.gmptGlyphOrigin.y;
      aBoundingMetrics.descent = gm.gmptGlyphOrigin.y - gm.gmBlackBoxY;
      aBoundingMetrics.width = gm.gmCellIncX;
    }
    if (1 < aLength) {
      // loop over each glyph to get the ascent and descent
      PRUint32 i;
      for (i = 1; i < aLength; i++) {
        len = GetGlyphOutline(aDC, mConverter[aString[i]], GGO_METRICS, &gm, 0, nsnull, &mat2);
        if (GDI_ERROR == len) {
          return NS_ERROR_UNEXPECTED;
        }
        else {
          if (aBoundingMetrics.ascent < gm.gmptGlyphOrigin.y)
            aBoundingMetrics.ascent = gm.gmptGlyphOrigin.y;
          if (aBoundingMetrics.descent > nscoord(gm.gmptGlyphOrigin.y - gm.gmBlackBoxY))
            aBoundingMetrics.descent = gm.gmptGlyphOrigin.y - gm.gmBlackBoxY;
        }
      }
      // get the final rightBearing and width. Possible kerning is taken into account.
      char str[CHAR_BUFFER_SIZE];
      char* pstr = str;
      if (aLength > CHAR_BUFFER_SIZE) {
        pstr = new char[aLength];
        if (!pstr) return NS_ERROR_OUT_OF_MEMORY;
      }
      for (i = 0; i < aLength; i++) {
        pstr[i] = mConverter[aString[i]];
      }
      SIZE size;
      ::GetTextExtentPointA(aDC, pstr, aLength, &size);
      aBoundingMetrics.width = size.cx;
      aBoundingMetrics.rightBearing = size.cx - gm.gmCellIncX + gm.gmBlackBoxX;
      if (pstr != str) {
        delete[] pstr;
      }
    }

    ::SelectObject(aDC, oldFont);
  }
  return NS_OK;
}
#endif

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
  char*    mLangGroup;
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
  { "DEFAULT",     0,    "",               GenerateDefault },
  { "ANSI",        1252, "x-western",      GenerateSingleByte },
  { "EASTEUROPE",  1250, "x-central-euro", GenerateSingleByte },
  { "RUSSIAN",     1251, "x-cyrillic",     GenerateSingleByte },
  { "GREEK",       1253, "el",             GenerateSingleByte },
  { "TURKISH",     1254, "tr",             GenerateSingleByte },
  { "HEBREW",      1255, "he",             GenerateSingleByte },
  { "ARABIC",      1256, "ar",             GenerateSingleByte },
  { "BALTIC",      1257, "x-baltic",       GenerateSingleByte },
  { "THAI",        874,  "th",             GenerateSingleByte },
  { "SHIFTJIS",    932,  "ja",             GenerateMultiByte },
  { "GB2312",      936,  "zh-CN",          GenerateMultiByte },
  { "HANGEUL",     949,  "ko",             GenerateMultiByte },
  { "CHINESEBIG5", 950,  "zh-TW",          GenerateMultiByte },
  { "JOHAB",       1361, "ko-XXX",         GenerateMultiByte }
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

  PRUint16 weightTable = LookForFontWeightTable(aDC, aName);
  PRInt32 weight = GetFontWeight(mFont->weight, weightTable);
  FillLogFont(&logFont, weight);

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
    font->mMap = GetCMAP(aDC, logFont.lfFaceName, nsnull);
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
        gGlobalFonts[i].map = GetCMAP(aDC, gGlobalFonts[i].logFont.lfFaceName,
          nsnull);
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


// The Font Enumerator

nsFontEnumeratorWin::nsFontEnumeratorWin()
{
  NS_INIT_REFCNT();
}

NS_IMPL_ISUPPORTS(nsFontEnumeratorWin,
                  nsCOMTypeInfo<nsIFontEnumerator>::GetIID());

static int gInitializedFontEnumerator = 0;

static int
InitializeFontEnumerator(void)
{
  gInitializedFontEnumerator = 1;

  if (!nsFontMetricsWin::gGlobalFonts) {
    HDC dc = ::GetDC(nsnull);
    if (!nsFontMetricsWin::InitializeGlobalFonts(dc)) {
      ::ReleaseDC(nsnull, dc);
      return 0;
    }
    ::ReleaseDC(nsnull, dc);
  }

  return 1;
}

static int
CompareFontNames(const void* aArg1, const void* aArg2, void* aClosure)
{
  const PRUnichar* str1 = *((const PRUnichar**) aArg1);
  const PRUnichar* str2 = *((const PRUnichar**) aArg2);

  // XXX add nsICollation stuff

  return nsCRT::strcmp(str1, str2);
}

NS_IMETHODIMP
nsFontEnumeratorWin::EnumerateAllFonts(PRUint32* aCount, PRUnichar*** aResult)
{
  if ((!aCount) || (!aResult)) {
    return NS_ERROR_NULL_POINTER;
  }

  if (!gInitializedFontEnumerator) {
    if (!InitializeFontEnumerator()) {
      return NS_ERROR_FAILURE;
    }
  }

  PRUnichar** array = (PRUnichar**)
    nsAllocator::Alloc(nsFontMetricsWin::gGlobalFontsCount * sizeof(PRUnichar*));
  if (!array) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  for (int i = 0; i < nsFontMetricsWin::gGlobalFontsCount; i++) {
    PRUnichar* str = nsFontMetricsWin::gGlobalFonts[i].name->ToNewUnicode();
    if (!str) {
      for (i = i - 1; i >= 0; i--) {
        nsAllocator::Free(array[i]);
      }
      nsAllocator::Free(array);
      return NS_ERROR_OUT_OF_MEMORY;
    }
    array[i] = str;
  }

  NS_QuickSort(array, nsFontMetricsWin::gGlobalFontsCount, sizeof(PRUnichar*),
    CompareFontNames, nsnull);

  *aCount = nsFontMetricsWin::gGlobalFontsCount;
  *aResult = array;

  return NS_OK;
}

static int
SignatureMatchesLangGroup(FONTSIGNATURE* aSignature,
  const char* aLangGroup)
{
  int dword;
  DWORD* array = aSignature->fsCsb;
  int i = 0;
  for (dword = 0; dword < 2; dword++) {
    for (int bit = 0; bit < sizeof(DWORD) * 8; bit++) {
      if ((array[dword] >> bit) & 1) {
        if (!strcmp(gCharSetInfo[gCharSetToIndex[bitToCharSet[i]]].mLangGroup,
                    aLangGroup)) {
          return 1;
        }
      }
      i++;
    }
  }

  return 0;
}

static int
FontMatchesGenericType(nsGlobalFont* aFont, const char* aGeneric,
  const char* aLangGroup)
{
  if (!strcmp(aLangGroup, "ja")) {
    return 1;
  }
  else if (!strcmp(aLangGroup, "zh-TW")) {
    return 1;
  }
  else if (!strcmp(aLangGroup, "zh-CN")) {
    return 1;
  }
  else if (!strcmp(aLangGroup, "ko")) {
    return 1;
  }
  else if (!strcmp(aLangGroup, "th")) {
    return 1;
  }
  else if (!strcmp(aLangGroup, "he")) {
    return 1;
  }
  else if (!strcmp(aLangGroup, "ar")) {
    return 1;
  }

  switch (aFont->logFont.lfPitchAndFamily & 0xF0) {
  case FF_DONTCARE:
    return 0;
  case FF_ROMAN:
    if (!strcmp(aGeneric, "serif")) {
      return 1;
    }
    return 0;
  case FF_SWISS:
    if (!strcmp(aGeneric, "sans-serif")) {
      return 1;
    }
    return 0;
  case FF_MODERN:
    if (!strcmp(aGeneric, "monospace")) {
      return 1;
    }
    return 0;
  case FF_SCRIPT:
    if (!strcmp(aGeneric, "cursive")) {
      return 1;
    }
    return 0;
  case FF_DECORATIVE:
    if (!strcmp(aGeneric, "fantasy")) {
      return 1;
    }
    return 0;
  default:
    return 0;
  }

  return 0;
}

NS_IMETHODIMP
nsFontEnumeratorWin::EnumerateFonts(const char* aLangGroup,
  const char* aGeneric, PRUint32* aCount, PRUnichar*** aResult)
{
  if ((!aLangGroup) || (!aGeneric) || (!aCount) || (!aResult)) {
    return NS_ERROR_NULL_POINTER;
  }

  if ((!strcmp(aLangGroup, "x-unicode")) ||
      (!strcmp(aLangGroup, "x-user-def"))) {
    return EnumerateAllFonts(aCount, aResult);
  }

  if (!gInitializedFontEnumerator) {
    if (!InitializeFontEnumerator()) {
      return NS_ERROR_FAILURE;
    }
  }

  PRUnichar** array = (PRUnichar**)
    nsAllocator::Alloc(nsFontMetricsWin::gGlobalFontsCount * sizeof(PRUnichar*));
  if (!array) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  int j = 0;
  for (int i = 0; i < nsFontMetricsWin::gGlobalFontsCount; i++) {
    if (SignatureMatchesLangGroup(&nsFontMetricsWin::gGlobalFonts[i].signature,
                                  aLangGroup) &&
        FontMatchesGenericType(&nsFontMetricsWin::gGlobalFonts[i], aGeneric,
                               aLangGroup)) {
      PRUnichar* str = nsFontMetricsWin::gGlobalFonts[i].name->ToNewUnicode();
      if (!str) {
        for (j = j - 1; j >= 0; j--) {
          nsAllocator::Free(array[j]);
        }
        nsAllocator::Free(array);
        return NS_ERROR_OUT_OF_MEMORY;
      }
      array[j] = str;
      j++;
    }
  }

  NS_QuickSort(array, j, sizeof(PRUnichar*), CompareFontNames, nsnull);

  *aCount = j;
  *aResult = array;

  return NS_OK;
}
