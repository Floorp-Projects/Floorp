/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *   Brian Stell <bstell@ix.netcom.com>
 *   Morten Nilsen <morten@nilsen.com>
 *   Jungshik Shin <jshin@mailaps.org>
 *   IBM Corporation
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#define ENABLE_X_FONT_BANNING 1

#include <sys/types.h>
#include "gfx-config.h"
#include "nscore.h"
#include "nsQuickSort.h"
#include "nsFontMetricsGTK.h"
#include "nsIServiceManager.h"
#include "nsICharsetConverterManager.h"
#include "nsILanguageAtomService.h"
#include "nsISaveAsCharset.h"
#include "nsIPref.h"
#include "nsCOMPtr.h"
#include "nsPrintfCString.h"
#include "nspr.h"
#include "nsHashtable.h"
#include "nsReadableUtils.h"
#include "nsAString.h"
#include "nsXPIDLString.h"
#include "nsFontDebug.h"
#include "nsFT2FontNode.h"
#include "nsFontFreeType.h"
#include "nsXFontNormal.h"
#include "nsX11AlphaBlend.h"
#include "nsXFontAAScaledBitmap.h"
#include "nsUnicharUtils.h"
#ifdef ENABLE_X_FONT_BANNING
#include <regex.h>
#endif /* ENABLE_X_FONT_BANNING */

#include <X11/Xatom.h>
#include <gdk/gdk.h>

#define IS_SURROGATE(u)      (u > 0x10000)

#define UCS2_NOMAPPING 0XFFFD

#ifdef PR_LOGGING 
static PRLogModuleInfo * FontMetricsGTKLM = PR_NewLogModule("FontMetricsGTK");
#endif /* PR_LOGGING */

#ifdef ENABLE_X_FONT_BANNING
/* Not all platforms may have REG_OK */
#ifndef REG_OK
#define REG_OK (0)
#endif /* !REG_OK */
#endif /* ENABLE_X_FONT_BANNING */

#undef USER_DEFINED
#define USER_DEFINED "x-user-def"

// This is the scaling factor that we keep fonts limited to against
// the display size.  If a pixel size is requested that is more than
// this factor larger than the height of the display, it's clamped to
// that value instead of the requested size.
#define FONT_MAX_FONT_SCALE 2

#undef NOISY_FONTS
#undef REALLY_NOISY_FONTS

struct nsFontCharSetMap;
struct nsFontFamilyName;
struct nsFontPropertyName;
struct nsFontStyle;
struct nsFontWeight;
struct nsFontLangGroup;

struct nsFontCharSetInfo
{
  const char*            mCharSet;
  nsFontCharSetConverter Convert;
  PRUint8                mSpecialUnderline;
  PRInt32                mCodeRange1Bits;
  PRInt32                mCodeRange2Bits;
  PRUint16*              mCCMap;
  nsIUnicodeEncoder*     mConverter;
  nsIAtom*               mLangGroup;
  PRBool                 mInitedSizeInfo;
  PRInt32                mOutlineScaleMin;
  PRInt32                mAABitmapScaleMin;
  double                 mAABitmapOversize;
  double                 mAABitmapUndersize;
  PRBool                 mAABitmapScaleAlways;
  PRInt32                mBitmapScaleMin;
  double                 mBitmapOversize;
  double                 mBitmapUndersize;
};

struct nsFontFamily
{
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  nsFontNodeArray mNodes;
};

struct nsFontFamilyName
{
  const char* mName;
  const char* mXName;
};

struct nsFontPropertyName
{
  const char* mName;
  int         mValue;
};

static NS_DEFINE_CID(kCharSetManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kSaveAsCharsetCID, NS_SAVEASCHARSET_CID);
static void SetCharsetLangGroup(nsFontCharSetInfo* aCharSetInfo);

static int gFontMetricsGTKCount = 0;
static int gInitialized = 0;
static PRBool gForceOutlineScaledFonts = PR_FALSE;
static PRBool gAllowDoubleByteSpecialChars = PR_TRUE;

// XXX many of these statics need to be freed at shutdown time

static nsIPref* gPref = nsnull;
static float gDevScale = 0.0f; /* Scaler value from |GetCanonicalPixelScale()| */
static PRBool gScaleBitmapFontsWithDevScale = PR_FALSE;
static nsICharsetConverterManager* gCharSetManager = nsnull;
static nsIUnicodeEncoder* gUserDefinedConverter = nsnull;

static nsHashtable* gAliases = nsnull;
static nsHashtable* gCharSetMaps = nsnull;
static nsHashtable* gFamilies = nsnull;
static nsHashtable* gFFRENodes = nsnull;
static nsHashtable* gAFRENodes = nsnull;
// gCachedFFRESearches holds the "already looked up"
// FFRE (Foundry Family Registry Encoding) font searches
static nsHashtable* gCachedFFRESearches = nsnull;
static nsHashtable* gSpecialCharSets = nsnull;
static nsHashtable* gStretches = nsnull;
static nsHashtable* gWeights = nsnull;
nsISaveAsCharset* gFontSubConverter = nsnull;

static nsFontNodeArray* gGlobalList = nsnull;

static nsIAtom* gUnicode = nsnull;
static nsIAtom* gUserDefined = nsnull;
static nsIAtom* gZHTW = nsnull;
static nsIAtom* gZHHK = nsnull;
static nsIAtom* gZHTWHK = nsnull;  // for fonts common to zh-TW and zh-HK
static nsIAtom* gUsersLocale = nsnull;
static nsIAtom* gWesternLocale = nsnull;

// Controls for Outline Scaled Fonts (okay looking)

static PRInt32 gOutlineScaleMinimum = 6;
// Controls for Anti-Aliased Scaled Bitmaps (okay looking)
static PRBool  gAABitmapScaleEnabled = PR_TRUE;
static PRBool  gAABitmapScaleAlways = PR_FALSE;
static PRInt32 gAABitmapScaleMinimum = 6;
static double  gAABitmapOversize = 1.1;
static double  gAABitmapUndersize = 0.9;

// Controls for (regular) Scaled Bitmaps (very ugly)
static PRInt32 gBitmapScaleMinimum = 10;
static double  gBitmapOversize = 1.2;
static double  gBitmapUndersize = 0.8;

#ifdef ENABLE_X_FONT_BANNING
static regex_t *gFontRejectRegEx = nsnull,
               *gFontAcceptRegEx = nsnull;
#endif /* ENABLE_X_FONT_BANNING */

static gint SingleByteConvert(nsFontCharSetInfo* aSelf, XFontStruct* aFont,
  const PRUnichar* aSrcBuf, PRInt32 aSrcLen, char* aDestBuf, PRInt32 aDestLen);
static gint DoubleByteConvert(nsFontCharSetInfo* aSelf, XFontStruct* aFont,
  const PRUnichar* aSrcBuf, PRInt32 aSrcLen, char* aDestBuf, PRInt32 aDestLen);
static gint ISO10646Convert(nsFontCharSetInfo* aSelf, XFontStruct* aFont,
  const PRUnichar* aSrcBuf, PRInt32 aSrcLen, char* aDestBuf, PRInt32 aDestLen);

static nsFontCharSetInfo Unknown = { nsnull };
static nsFontCharSetInfo Special = { nsnull };

static nsFontCharSetInfo USASCII =
  { "us-ascii", SingleByteConvert, 0,
    TT_OS2_CPR1_LATIN1 | TT_OS2_CPR1_MAC_ROMAN,
    TT_OS2_CPR2_CA_FRENCH |  TT_OS2_CPR2_PORTUGESE
    | TT_OS2_CPR2_WE_LATIN1 |  TT_OS2_CPR2_US };
static nsFontCharSetInfo ISO88591 =
  { "ISO-8859-1", SingleByteConvert, 0,
    TT_OS2_CPR1_LATIN1 | TT_OS2_CPR1_MAC_ROMAN,
    TT_OS2_CPR2_CA_FRENCH |  TT_OS2_CPR2_PORTUGESE
    | TT_OS2_CPR2_WE_LATIN1 |  TT_OS2_CPR2_US };
static nsFontCharSetInfo ISO88592 =
  { "ISO-8859-2", SingleByteConvert, 0,
    TT_OS2_CPR1_LATIN2, TT_OS2_CPR2_LATIN2 };
static nsFontCharSetInfo ISO88593 =
  { "ISO-8859-3", SingleByteConvert, 0,
    TT_OS2_CPR1_TURKISH, TT_OS2_CPR2_TURKISH };
static nsFontCharSetInfo ISO88594 =
  { "ISO-8859-4", SingleByteConvert, 0,
    TT_OS2_CPR1_BALTIC, TT_OS2_CPR2_BALTIC };
static nsFontCharSetInfo ISO88595 =
  { "ISO-8859-5", SingleByteConvert, 0,
    TT_OS2_CPR1_CYRILLIC, TT_OS2_CPR2_RUSSIAN | TT_OS2_CPR2_CYRILLIC };
static nsFontCharSetInfo ISO88596 =
  { "ISO-8859-6", SingleByteConvert, 0,
      TT_OS2_CPR1_ARABIC, TT_OS2_CPR2_ARABIC | TT_OS2_CPR2_ARABIC_708 };
static nsFontCharSetInfo ISO885968x =
  { "x-iso-8859-6-8-x", SingleByteConvert, 0,
      TT_OS2_CPR1_ARABIC, TT_OS2_CPR2_ARABIC | TT_OS2_CPR2_ARABIC_708 };
static nsFontCharSetInfo ISO8859616 =
  { "x-iso-8859-6-16", SingleByteConvert, 0,
      TT_OS2_CPR1_ARABIC, TT_OS2_CPR2_ARABIC | TT_OS2_CPR2_ARABIC_708 };
static nsFontCharSetInfo IBM1046 =
  { "x-IBM1046", SingleByteConvert, 0,
      TT_OS2_CPR1_ARABIC, TT_OS2_CPR2_ARABIC | TT_OS2_CPR2_ARABIC_708 };
static nsFontCharSetInfo ISO88597 =
  { "ISO-8859-7", SingleByteConvert, 0,
    TT_OS2_CPR1_GREEK, TT_OS2_CPR2_GREEK | TT_OS2_CPR2_GREEK_437G };
static nsFontCharSetInfo ISO88598 =
  { "ISO-8859-8", SingleByteConvert, 0,
    TT_OS2_CPR1_HEBREW, TT_OS2_CPR2_HEBREW };
// change from  
// { "ISO-8859-8", SingleByteConvertReverse, 0, 0, 0 };
// untill we fix the layout and ensure we only call this with pure RTL text
static nsFontCharSetInfo ISO88599 =
  { "ISO-8859-9", SingleByteConvert, 0,
    TT_OS2_CPR1_TURKISH, TT_OS2_CPR2_TURKISH };
// no support for iso-8859-10 (Nordic/Icelandic) currently
// static nsFontCharSetInfo ISO885910 =
// { "ISO-8859-10", SingleByteConvert, 0,
//   0, TT_OS2_CPR2_NORDIC | TT_OS2_CPR2_ICELANDIC };
// no support for iso-8859-12 (Vietnamese) currently
// static nsFontCharSetInfo ISO885912 =
// { "ISO-8859-12", SingleByteConvert, 0,
//   TT_OS2_CPR1_VIETNAMESE, 0 };
static nsFontCharSetInfo ISO885913 =
  { "ISO-8859-13", SingleByteConvert, 0,
    TT_OS2_CPR1_BALTIC, TT_OS2_CPR2_BALTIC };
static nsFontCharSetInfo ISO885915 =
  { "ISO-8859-15", SingleByteConvert, 0,
    TT_OS2_CPR1_LATIN2, TT_OS2_CPR2_LATIN2 };
static nsFontCharSetInfo JISX0201 =
  { "jis_0201", SingleByteConvert, 1,
    TT_OS2_CPR1_JAPANESE, 0 };
static nsFontCharSetInfo KOI8R =
  { "KOI8-R", SingleByteConvert, 0,
    TT_OS2_CPR1_CYRILLIC, TT_OS2_CPR2_RUSSIAN | TT_OS2_CPR2_CYRILLIC };
static nsFontCharSetInfo KOI8U =
  { "KOI8-U", SingleByteConvert, 0,
    TT_OS2_CPR1_CYRILLIC, TT_OS2_CPR2_RUSSIAN | TT_OS2_CPR2_CYRILLIC };
static nsFontCharSetInfo TIS6202 =
/* Added to support thai context sensitive shaping if
 * CTL extension is is in force */
#ifdef SUNCTL
  { "tis620-2", SingleByteConvert, 0,
    TT_OS2_CPR1_THAI, 0 };
#else
  { "windows-874", SingleByteConvert, 0,
    TT_OS2_CPR1_THAI, 0 };
#endif /* SUNCTL */
static nsFontCharSetInfo TIS620 =
  { "TIS-620", SingleByteConvert, 0,
    TT_OS2_CPR1_THAI, 0 };
static nsFontCharSetInfo ISO885911 =
  { "ISO-8859-11", SingleByteConvert, 0,
    TT_OS2_CPR1_THAI, 0 };
static nsFontCharSetInfo Big5 =
  { "x-x-big5", DoubleByteConvert, 1,
    TT_OS2_CPR1_CHINESE_TRAD, 0 };
// a kludge to distinguish zh-TW only fonts in Big5 (such as hpbig5-)
// from zh-TW/zh-HK common fonts in Big5 (such as big5-1)
static nsFontCharSetInfo Big5TWHK =
  { "x-x-big5", DoubleByteConvert, 1,
    TT_OS2_CPR1_CHINESE_TRAD, 0 };
static nsFontCharSetInfo CNS116431 =
  { "x-cns-11643-1", DoubleByteConvert, 1,
    TT_OS2_CPR1_CHINESE_TRAD, 0 };
static nsFontCharSetInfo CNS116432 =
  { "x-cns-11643-2", DoubleByteConvert, 1,
    TT_OS2_CPR1_CHINESE_TRAD, 0 };
static nsFontCharSetInfo CNS116433 =
  { "x-cns-11643-3", DoubleByteConvert, 1,
    TT_OS2_CPR1_CHINESE_TRAD, 0 };
static nsFontCharSetInfo CNS116434 =
  { "x-cns-11643-4", DoubleByteConvert, 1,
    TT_OS2_CPR1_CHINESE_TRAD, 0 };
static nsFontCharSetInfo CNS116435 =
  { "x-cns-11643-5", DoubleByteConvert, 1,
    TT_OS2_CPR1_CHINESE_TRAD, 0 };
static nsFontCharSetInfo CNS116436 =
  { "x-cns-11643-6", DoubleByteConvert, 1,
    TT_OS2_CPR1_CHINESE_TRAD, 0 };
static nsFontCharSetInfo CNS116437 =
  { "x-cns-11643-7", DoubleByteConvert, 1,
    TT_OS2_CPR1_CHINESE_TRAD, 0 };
static nsFontCharSetInfo GB2312 =
  { "gb_2312-80", DoubleByteConvert, 1,
    TT_OS2_CPR1_CHINESE_SIMP, 0 };
static nsFontCharSetInfo GB18030_0 =
  { "gb18030.2000-0", DoubleByteConvert, 1,
    TT_OS2_CPR1_CHINESE_SIMP, 0 };
static nsFontCharSetInfo GB18030_1 =
  { "gb18030.2000-1", DoubleByteConvert, 1,
    TT_OS2_CPR1_CHINESE_SIMP, 0 };
static nsFontCharSetInfo GBK =
  { "x-gbk-noascii", DoubleByteConvert, 1,
    TT_OS2_CPR1_CHINESE_SIMP, 0 };
static nsFontCharSetInfo HKSCS =
  { "hkscs-1", DoubleByteConvert, 1,
    TT_OS2_CPR1_CHINESE_TRAD, 0 };
static nsFontCharSetInfo JISX0208 =
  { "jis_0208-1983", DoubleByteConvert, 1,
    TT_OS2_CPR1_JAPANESE, 0 };
static nsFontCharSetInfo JISX0212 =
  { "jis_0212-1990", DoubleByteConvert, 1,
    TT_OS2_CPR1_JAPANESE, 0 };
static nsFontCharSetInfo KSC5601 =
  { "ks_c_5601-1987", DoubleByteConvert, 1,
    TT_OS2_CPR1_KO_WANSUNG | TT_OS2_CPR1_KO_JOHAB, 0 };
static nsFontCharSetInfo X11Johab =
  { "x-x11johab", DoubleByteConvert, 1,
    TT_OS2_CPR1_KO_WANSUNG | TT_OS2_CPR1_KO_JOHAB, 0 };
static nsFontCharSetInfo JohabNoAscii =
  { "x-johab-noascii", DoubleByteConvert, 1,
    TT_OS2_CPR1_KO_WANSUNG | TT_OS2_CPR1_KO_JOHAB, 0 };
static nsFontCharSetInfo JamoTTF =
  { "x-koreanjamo-0", DoubleByteConvert, 1,
    TT_OS2_CPR1_KO_WANSUNG | TT_OS2_CPR1_KO_JOHAB, 0 };
static nsFontCharSetInfo TamilTTF =
  { "x-tamilttf-0", DoubleByteConvert, 0,
    0, 0 };
static nsFontCharSetInfo CP1250 =
  { "windows-1250", SingleByteConvert, 0,
    TT_OS2_CPR1_LATIN2, TT_OS2_CPR2_LATIN2 };
static nsFontCharSetInfo CP1251 =
  { "windows-1251", SingleByteConvert, 0,
    TT_OS2_CPR1_CYRILLIC, TT_OS2_CPR2_RUSSIAN };
static nsFontCharSetInfo CP1252 =
  { "windows-1252", SingleByteConvert, 0,
    TT_OS2_CPR1_LATIN1 | TT_OS2_CPR1_MAC_ROMAN,
    TT_OS2_CPR2_CA_FRENCH |  TT_OS2_CPR2_PORTUGESE
    | TT_OS2_CPR2_WE_LATIN1 |  TT_OS2_CPR2_US };
static nsFontCharSetInfo CP1253 =
  { "windows-1253", SingleByteConvert, 0,
    TT_OS2_CPR1_GREEK, TT_OS2_CPR2_GREEK | TT_OS2_CPR2_GREEK_437G };
static nsFontCharSetInfo CP1257 =
  { "windows-1257", SingleByteConvert, 0,
    TT_OS2_CPR1_BALTIC, TT_OS2_CPR2_BALTIC };

#ifdef SUNCTL
/* Hindi range currently unsupported in FT2 range. Change TT* once we 
   arrive at a way to identify hindi */
static nsFontCharSetInfo SunIndic =
  { "x-sun-unicode-india-0", DoubleByteConvert, 0,
    0, 0 };
#endif /* SUNCTL */

static nsFontCharSetInfo ISO106461 =
  { nsnull, ISO10646Convert, 1, 0xFFFFFFFF, 0xFFFFFFFF };

static nsFontCharSetInfo AdobeSymbol =
   { "Adobe-Symbol-Encoding", SingleByteConvert, 0,
    TT_OS2_CPR1_SYMBOL, 0 };
static nsFontCharSetInfo AdobeEuro =
  { "x-adobe-euro", SingleByteConvert, 0,
    0, 0 };

#ifdef MOZ_MATHML
static nsFontCharSetInfo CMCMEX =
   { "x-t1-cmex", SingleByteConvert, 0, TT_OS2_CPR1_SYMBOL, 0};
static nsFontCharSetInfo CMCMSY =
   { "x-t1-cmsy", SingleByteConvert, 0, TT_OS2_CPR1_SYMBOL, 0};
static nsFontCharSetInfo CMCMR =
   { "x-t1-cmr", SingleByteConvert, 0, TT_OS2_CPR1_SYMBOL, 0};
static nsFontCharSetInfo CMCMMI =
   { "x-t1-cmmi", SingleByteConvert, 0, TT_OS2_CPR1_SYMBOL, 0};
static nsFontCharSetInfo Mathematica1 =
   { "x-mathematica1", SingleByteConvert, 0, TT_OS2_CPR1_SYMBOL, 0};
static nsFontCharSetInfo Mathematica2 =
   { "x-mathematica2", SingleByteConvert, 0, TT_OS2_CPR1_SYMBOL, 0};
static nsFontCharSetInfo Mathematica3 =
   { "x-mathematica3", SingleByteConvert, 0, TT_OS2_CPR1_SYMBOL, 0};
static nsFontCharSetInfo Mathematica4 =
   { "x-mathematica4", SingleByteConvert, 0, TT_OS2_CPR1_SYMBOL, 0};
static nsFontCharSetInfo Mathematica5 =
   { "x-mathematica5", SingleByteConvert, 0, TT_OS2_CPR1_SYMBOL, 0};
#endif

static nsFontLangGroup FLG_WESTERN = { "x-western",     nsnull };
static nsFontLangGroup FLG_RUSSIAN = { "x-cyrillic",    nsnull };
static nsFontLangGroup FLG_BALTIC  = { "x-baltic",      nsnull };
static nsFontLangGroup FLG_CE      = { "x-central-euro",nsnull };
static nsFontLangGroup FLG_GREEK   = { "el",            nsnull };
static nsFontLangGroup FLG_TURKISH = { "tr",            nsnull };
static nsFontLangGroup FLG_HEBREW  = { "he",            nsnull };
static nsFontLangGroup FLG_ARABIC  = { "ar",            nsnull };
static nsFontLangGroup FLG_THAI    = { "th",            nsnull };
static nsFontLangGroup FLG_ZHCN    = { "zh-CN",         nsnull };
static nsFontLangGroup FLG_ZHTW    = { "zh-TW",         nsnull };
static nsFontLangGroup FLG_ZHHK    = { "zh-HK",         nsnull };
static nsFontLangGroup FLG_ZHTWHK  = { "x-zh-TWHK",     nsnull }; // TW + HK
static nsFontLangGroup FLG_JA      = { "ja",            nsnull };
static nsFontLangGroup FLG_KO      = { "ko",            nsnull };
#ifdef SUNCTL
static nsFontLangGroup FLG_INDIC   = { "x-devanagari",  nsnull };
#endif
static nsFontLangGroup FLG_TAMIL   = { "x-tamil",       nsnull };
static nsFontLangGroup FLG_NONE    = { nsnull,          nsnull };

/*
 * Normally, the charset of an X font can be determined simply by looking at
 * the last 2 fields of the long XLFD font name (CHARSET_REGISTRY and
 * CHARSET_ENCODING). However, there are a number of special cases:
 *
 * Sometimes, X server vendors use the same name to mean different things. For
 * example, IRIX uses "cns11643-1" to mean the 2nd plane of CNS 11643, while
 * Solaris uses that name for the 1st plane.
 *
 * Some X server vendors use certain names for something completely different.
 * For example, some Solaris fonts say "gb2312.1980-0" but are actually ASCII
 * fonts. These cases can be detected by looking at the POINT_SIZE and
 * AVERAGE_WIDTH fields. If the average width is half the point size, this is
 * an ASCII font, not GB 2312.
 *
 * Some fonts say "fontspecific" in the CHARSET_ENCODING field. Their charsets
 * depend on the FAMILY_NAME. For example, the following is a "Symbol" font:
 *
 *   -adobe-symbol-medium-r-normal--17-120-100-100-p-95-adobe-fontspecific
 *
 * Some vendors use one name to mean 2 different things, depending on the font.
 * For example, AIX has some "ksc5601.1987-0" fonts that require the 8th bit of
 * both bytes to be zero, while other fonts require them to be set to one.
 * These cases can be distinguished by looking at the FOUNDRY field, but a
 * better way is to look at XFontStruct.min_byte1.
 */
static nsFontCharSetMap gCharSetMap[] =
{
  { "-ascii",             &FLG_NONE,    &Unknown       },
  { "-ibm pc",            &FLG_NONE,    &Unknown       },
  { "adobe-fontspecific", &FLG_NONE,    &Special       },
  { "ansi-1251",          &FLG_RUSSIAN, &CP1251        },
  // On Solaris, big5-0 is used for ASCII-only fonts while in XFree86, 
  // it's for Big5 fonts without US-ASCII. When a non-Solaris binary
  // is displayed on a Solaris X server, this would break. 
#ifndef SOLARIS
  { "big5-0",             &FLG_ZHTWHK,  &Big5TWHK      }, // for both TW and HK
#else
  { "big5-0",             &FLG_ZHTW,    &USASCII       }, 
#endif
  { "big5-1",             &FLG_ZHTWHK,  &Big5TWHK      }, // ditto
  { "big5.et-0",          &FLG_ZHTW,    &Big5          },
  { "big5.et.ext-0",      &FLG_ZHTW,    &Big5          },
  { "big5.etext-0",       &FLG_ZHTW,    &Big5          },
  { "big5.hku-0",         &FLG_ZHTW,    &Big5          },
  { "big5.hku-1",         &FLG_ZHTW,    &Big5          },
  { "big5.pc-0",          &FLG_ZHTW,    &Big5          },
  { "big5.shift-0",       &FLG_ZHTW,    &Big5          },
  { "big5hkscs-0",        &FLG_ZHHK,    &HKSCS         },
  { "cns11643.1986-1",    &FLG_ZHTW,    &CNS116431     },
  { "cns11643.1986-2",    &FLG_ZHTW,    &CNS116432     },
  { "cns11643.1992-1",    &FLG_ZHTW,    &CNS116431     },
  { "cns11643.1992.1-0",  &FLG_ZHTW,    &CNS116431     },
  { "cns11643.1992-12",   &FLG_NONE,    &Unknown       },
  { "cns11643.1992.2-0",  &FLG_ZHTW,    &CNS116432     },
  { "cns11643.1992-2",    &FLG_ZHTW,    &CNS116432     },
  { "cns11643.1992-3",    &FLG_ZHTW,    &CNS116433     },
  { "cns11643.1992.3-0",  &FLG_ZHTW,    &CNS116433     },
  { "cns11643.1992.4-0",  &FLG_ZHTW,    &CNS116434     },
  { "cns11643.1992-4",    &FLG_ZHTW,    &CNS116434     },
  { "cns11643.1992.5-0",  &FLG_ZHTW,    &CNS116435     },
  { "cns11643.1992-5",    &FLG_ZHTW,    &CNS116435     },
  { "cns11643.1992.6-0",  &FLG_ZHTW,    &CNS116436     },
  { "cns11643.1992-6",    &FLG_ZHTW,    &CNS116436     },
  { "cns11643.1992.7-0",  &FLG_ZHTW,    &CNS116437     },
  { "cns11643.1992-7",    &FLG_ZHTW,    &CNS116437     },
  { "cns11643-1",         &FLG_ZHTW,    &CNS116431     },
  { "cns11643-2",         &FLG_ZHTW,    &CNS116432     },
  { "cns11643-3",         &FLG_ZHTW,    &CNS116433     },
  { "cns11643-4",         &FLG_ZHTW,    &CNS116434     },
  { "cns11643-5",         &FLG_ZHTW,    &CNS116435     },
  { "cns11643-6",         &FLG_ZHTW,    &CNS116436     },
  { "cns11643-7",         &FLG_ZHTW,    &CNS116437     },
  { "cp1251-1",           &FLG_RUSSIAN, &CP1251        },
  { "dec-dectech",        &FLG_NONE,    &Unknown       },
  { "dtsymbol-1",         &FLG_NONE,    &Unknown       },
  { "fontspecific-0",     &FLG_NONE,    &Unknown       },
  { "gb2312.1980-0",      &FLG_ZHCN,    &GB2312        },
  { "gb2312.1980-1",      &FLG_ZHCN,    &GB2312        },
  { "gb13000.1993-1",     &FLG_ZHCN,    &GBK           },
  { "gb18030.2000-0",     &FLG_ZHCN,    &GB18030_0     },
  { "gb18030.2000-1",     &FLG_ZHCN,    &GB18030_1     },
  { "gbk-0",              &FLG_ZHCN,    &GBK           },
  { "gbk1988.1989-0",     &FLG_ZHCN,    &USASCII       },
  { "hkscs-1",            &FLG_ZHHK,    &HKSCS         },
  { "hp-japanese15",      &FLG_NONE,    &Unknown       },
  { "hp-japaneseeuc",     &FLG_NONE,    &Unknown       },
  { "hp-roman8",          &FLG_NONE,    &Unknown       },
  { "hp-schinese15",      &FLG_NONE,    &Unknown       },
  { "hp-tchinese15",      &FLG_NONE,    &Unknown       },
  { "hp-tchinesebig5",    &FLG_ZHTW,    &Big5          },
  { "hp-wa",              &FLG_NONE,    &Unknown       },
  { "hpbig5-",            &FLG_ZHTW,    &Big5          },
  { "hphkbig5-",          &FLG_ZHHK,    &HKSCS         },
  { "hproc16-",           &FLG_NONE,    &Unknown       },
  { "ibm-1046",           &FLG_ARABIC,  &IBM1046       },
  { "ibm-1252",           &FLG_NONE,    &Unknown       },
  { "ibm-850",            &FLG_NONE,    &Unknown       },
  { "ibm-fontspecific",   &FLG_NONE,    &Unknown       },
  { "ibm-sbdcn",          &FLG_NONE,    &Unknown       },
  { "ibm-sbdtw",          &FLG_NONE,    &Unknown       },
  { "ibm-special",        &FLG_NONE,    &Unknown       },
  { "ibm-udccn",          &FLG_NONE,    &Unknown       },
  { "ibm-udcjp",          &FLG_NONE,    &Unknown       },
  { "ibm-udctw",          &FLG_NONE,    &Unknown       },
  { "iso646.1991-irv",    &FLG_NONE,    &Unknown       },
  { "iso8859-1",          &FLG_WESTERN, &ISO88591      },
  { "iso8859-13",         &FLG_BALTIC,  &ISO885913     },
  { "iso8859-15",         &FLG_WESTERN, &ISO885915     },
  { "iso8859-1@cn",       &FLG_NONE,    &Unknown       },
  { "iso8859-1@kr",       &FLG_NONE,    &Unknown       },
  { "iso8859-1@tw",       &FLG_NONE,    &Unknown       },
  { "iso8859-1@zh",       &FLG_NONE,    &Unknown       },
  { "iso8859-2",          &FLG_CE,      &ISO88592      },
  { "iso8859-3",          &FLG_WESTERN, &ISO88593      },
  { "iso8859-4",          &FLG_BALTIC,  &ISO88594      },
  { "iso8859-5",          &FLG_RUSSIAN, &ISO88595      },
  { "iso8859-6",          &FLG_ARABIC,  &ISO88596      },
  { "iso8859-6.8x",       &FLG_ARABIC,  &ISO885968x    },
  { "iso8859-6.16" ,      &FLG_ARABIC,  &ISO8859616    },
  { "iso8859-7",          &FLG_GREEK,   &ISO88597      },
  { "iso8859-8",          &FLG_HEBREW,  &ISO88598      },
  { "iso8859-9",          &FLG_TURKISH, &ISO88599      },
  { "iso10646-1",         &FLG_NONE,    &ISO106461     },
  { "jisx0201.1976-0",    &FLG_JA,      &JISX0201      },
  { "jisx0201.1976-1",    &FLG_JA,      &JISX0201      },
  { "jisx0208.1983-0",    &FLG_JA,      &JISX0208      },
  { "jisx0208.1990-0",    &FLG_JA,      &JISX0208      },
  { "jisx0212.1990-0",    &FLG_JA,      &JISX0212      },
  { "koi8-r",             &FLG_RUSSIAN, &KOI8R         },
  { "koi8-u",             &FLG_RUSSIAN, &KOI8U         },
  { "johab-1",            &FLG_KO,      &X11Johab      },
  { "johabs-1",           &FLG_KO,      &X11Johab      },
  { "johabsh-1",          &FLG_KO,      &X11Johab      },
  { "ksc5601.1987-0",     &FLG_KO,      &KSC5601       },
  // we can handle GR fonts with GL encoders (KSC5601 and GB2312)
  // See |DoubleByteConvert|.
  { "ksc5601.1987-1",     &FLG_KO,      &KSC5601       },
  { "ksc5601.1992-3",     &FLG_KO,      &JohabNoAscii  },
  { "koreanjamo-0",       &FLG_KO,      &JamoTTF       },
  { "microsoft-cp1250",   &FLG_CE,      &CP1250        },
  { "microsoft-cp1251",   &FLG_RUSSIAN, &CP1251        },
  { "microsoft-cp1252",   &FLG_WESTERN, &CP1252        },
  { "microsoft-cp1253",   &FLG_GREEK,   &CP1253        },
  { "microsoft-cp1257",   &FLG_BALTIC,  &CP1257        },
  { "misc-fontspecific",  &FLG_NONE,    &Unknown       },
  { "sgi-fontspecific",   &FLG_NONE,    &Unknown       },
  { "sun-fontspecific",   &FLG_NONE,    &Unknown       },
  { "sunolcursor-1",      &FLG_NONE,    &Unknown       },
  { "sunolglyph-1",       &FLG_NONE,    &Unknown       },
  { "symbol-fontspecific",&FLG_NONE,    &Special       },
  { "tis620.2529-1",      &FLG_THAI,    &TIS620        },
  { "tis620.2533-0",      &FLG_THAI,    &TIS620        },
  { "tis620.2533-1",      &FLG_THAI,    &TIS620        },
  { "tis620-0",           &FLG_THAI,    &TIS620        },
  { "tis620-2",           &FLG_THAI,    &TIS6202       },
  { "iso8859-11",         &FLG_THAI,    &ISO885911     },
  { "ucs2.cjk-0",         &FLG_NONE,    &ISO106461     },
  { "ucs2.cjk_china-0",   &FLG_ZHCN,    &ISO106461     },
  { "iso10646.2000-cn",   &FLG_ZHCN,    &ISO106461     },  // HP/UX
  { "ucs2.cjk_japan-0",   &FLG_JA,      &ISO106461     },
  { "ucs2.cjk_korea-0",   &FLG_KO,      &ISO106461     },
  { "korean.ucs2-0",      &FLG_KO,      &ISO106461     },  // HP/UX
  { "ucs2.cjk_taiwan-0",  &FLG_ZHTW,    &ISO106461     },
  { "ucs2.thai-0",        &FLG_THAI,    &ISO106461     },
  { "tamilttf-0",         &FLG_TAMIL,   &TamilTTF      },
#ifdef SUNCTL
  { "sun.unicode.india-0",&FLG_INDIC,   &SunIndic      },
#endif /* SUNCTL */

  { nsnull,               nsnull,       nsnull         }
};

static nsFontFamilyName gFamilyNameTable[] =
{
  { "arial",           "helvetica" },
  { "courier new",     "courier" },
  { "times new roman", "times" },

#ifdef MOZ_MATHML
  { "cmex",             "cmex10" },
  { "cmsy",             "cmsy10" },
  { "-moz-math-text",   "times" },
  { "-moz-math-symbol", "symbol" },
#endif

  { nsnull, nsnull }
};

static nsFontCharSetMap gNoneCharSetMap[] = { { nsnull }, };

static nsFontCharSetMap gSpecialCharSetMap[] =
{
  { "symbol-adobe-fontspecific", &FLG_NONE, &AdobeSymbol  },
  { "euromono-adobe-fontspecific",  &FLG_NONE, &AdobeEuro },
  { "eurosans-adobe-fontspecific",  &FLG_NONE, &AdobeEuro },
  { "euroserif-adobe-fontspecific", &FLG_NONE, &AdobeEuro },

#ifdef MOZ_MATHML
  { "cmex10-adobe-fontspecific", &FLG_NONE, &CMCMEX  },
  { "cmsy10-adobe-fontspecific", &FLG_NONE, &CMCMSY  },
  { "cmr10-adobe-fontspecific",  &FLG_NONE, &CMCMR  },
  { "cmmi10-adobe-fontspecific", &FLG_NONE, &CMCMMI  },

  { "math1-adobe-fontspecific", &FLG_NONE, &Mathematica1 },
  { "math2-adobe-fontspecific", &FLG_NONE, &Mathematica2 },
  { "math3-adobe-fontspecific", &FLG_NONE, &Mathematica3 },
  { "math4-adobe-fontspecific", &FLG_NONE, &Mathematica4 },
  { "math5-adobe-fontspecific", &FLG_NONE, &Mathematica5 },
 
  { "math1mono-adobe-fontspecific", &FLG_NONE, &Mathematica1 },
  { "math2mono-adobe-fontspecific", &FLG_NONE, &Mathematica2 },
  { "math3mono-adobe-fontspecific", &FLG_NONE, &Mathematica3 },
  { "math4mono-adobe-fontspecific", &FLG_NONE, &Mathematica4 },
  { "math5mono-adobe-fontspecific", &FLG_NONE, &Mathematica5 },
#endif

  { nsnull,                      nsnull        }
};

static nsFontPropertyName gStretchNames[] =
{
  { "block",         5 }, // XXX
  { "bold",          7 }, // XXX
  { "double wide",   9 },
  { "medium",        5 },
  { "narrow",        3 },
  { "normal",        5 },
  { "semicondensed", 4 },
  { "wide",          7 },

  { nsnull,          0 }
};

static nsFontPropertyName gWeightNames[] =
{
  { "black",    900 },
  { "bold",     700 },
  { "book",     400 },
  { "demi",     600 },
  { "demibold", 600 },
  { "light",    300 },
  { "medium",   400 },
  { "regular",  400 },
  
  { nsnull,     0 }
};

static char*
atomToName(nsIAtom* aAtom)
{
  const char *namePRU;
  aAtom->GetUTF8String(&namePRU);
  return ToNewCString(nsDependentCString(namePRU));
}

static PRUint16* gUserDefinedCCMap = nsnull;
static PRUint16* gEmptyCCMap = nsnull;

//
// smart quotes (and other special chars) in Asian (double byte)
// fonts are too large to use is western fonts.
// Here we define those characters.
// XXX: This array can (and need, for performance) be made |const| when 
// GTK port of gfx gets sync'd with Xlib port for multiple device contexts.
  
#include "dbyte_special_chars.ccmap"
DEFINE_CCMAP(gDoubleByteSpecialCharsCCMap, /* nothing */);

static PRBool
FreeCharSetMap(nsHashKey* aKey, void* aData, void* aClosure)
{
  nsFontCharSetMap* charsetMap = (nsFontCharSetMap*) aData;
  NS_IF_RELEASE(charsetMap->mInfo->mConverter);
  NS_IF_RELEASE(charsetMap->mInfo->mLangGroup);
  FreeCCMap(charsetMap->mInfo->mCCMap);

  return PR_TRUE;
}

static PRBool
FreeFamily(nsHashKey* aKey, void* aData, void* aClosure)
{
  delete (nsFontFamily*) aData;

  return PR_TRUE;
}

static void
FreeStretch(nsFontStretch* aStretch)
{
  PR_smprintf_free(aStretch->mScalable);

  for (PRInt32 count = aStretch->mScaledFonts.Count()-1; count >= 0; --count) {
    nsFontGTK *font = (nsFontGTK*)aStretch->mScaledFonts.ElementAt(count);
    if (font) delete font;
  }
  // aStretch->mScaledFonts.Clear(); handled by delete of aStretch

  for (int i = 0; i < aStretch->mSizesCount; i++) {
    delete aStretch->mSizes[i];
  }
  delete [] aStretch->mSizes;
  delete aStretch;
}

static void
FreeWeight(nsFontWeight* aWeight)
{
  for (int i = 0; i < 9; i++) {
    if (aWeight->mStretches[i]) {
      for (int j = i + 1; j < 9; j++) {
        if (aWeight->mStretches[j] == aWeight->mStretches[i]) {
          aWeight->mStretches[j] = nsnull;
        }
      }
      FreeStretch(aWeight->mStretches[i]);
    }
  }
  delete aWeight;
}

static void
FreeStyle(nsFontStyle* aStyle)
{
  for (int i = 0; i < 9; i++) {
    if (aStyle->mWeights[i]) {
      for (int j = i + 1; j < 9; j++) {
        if (aStyle->mWeights[j] == aStyle->mWeights[i]) {
          aStyle->mWeights[j] = nsnull;
        }
      }
      FreeWeight(aStyle->mWeights[i]);
    }
  }
  delete aStyle;
}

PRBool
FreeNode(nsHashKey* aKey, void* aData, void* aClosure)
{
  nsFontNode* node = (nsFontNode*) aData;
  for (int i = 0; i < 3; i++) {
    if (node->mStyles[i]) {
      for (int j = i + 1; j < 3; j++) {
        if (node->mStyles[j] == node->mStyles[i]) {
          node->mStyles[j] = nsnull;
        }
      }
      FreeStyle(node->mStyles[i]);
    }
  }
  delete node;

  return PR_TRUE;
}

static PRBool
FreeNodeArray(nsHashKey* aKey, void* aData, void* aClosure)
{
  nsFontNodeArray* nodes = (nsFontNodeArray*) aData;
  delete nodes;

  return PR_TRUE;
}

static void
FreeGlobals(void)
{
  // XXX complete this

  gInitialized = 0;

  nsFT2FontNode::FreeGlobals();

#ifdef ENABLE_X_FONT_BANNING
  if (gFontRejectRegEx) {
    regfree(gFontRejectRegEx);
    delete gFontRejectRegEx;
    gFontRejectRegEx = nsnull;
  }
  
  if (gFontAcceptRegEx) {
    regfree(gFontAcceptRegEx);
    delete gFontAcceptRegEx;
    gFontAcceptRegEx = nsnull;
  }  
#endif /* ENABLE_X_FONT_BANNING */

  nsXFontAAScaledBitmap::FreeGlobals();
  nsX11AlphaBlendFreeGlobals();

  if (gAliases) {
    delete gAliases;
    gAliases = nsnull;
  }
  NS_IF_RELEASE(gCharSetManager);
  if (gCharSetMaps) {
    gCharSetMaps->Reset(FreeCharSetMap, nsnull);
    delete gCharSetMaps;
    gCharSetMaps = nsnull;
  }
  if (gFamilies) {
    gFamilies->Reset(FreeFamily, nsnull);
    delete gFamilies;
    gFamilies = nsnull;
  }
  if (gGlobalList) {
    delete gGlobalList;
    gGlobalList = nsnull;
  }
  if (gCachedFFRESearches) {
    gCachedFFRESearches->Reset(FreeNodeArray, nsnull);
    delete gCachedFFRESearches;
    gCachedFFRESearches = nsnull;
  }
  if (gFFRENodes) {
    gFFRENodes->Reset(FreeNode, nsnull);
    delete gFFRENodes;
    gFFRENodes = nsnull;
  }
  if (gAFRENodes) {
    gAFRENodes->Reset(FreeNode, nsnull);
    delete gAFRENodes;
    gAFRENodes = nsnull;
  }
  NS_IF_RELEASE(gPref);
  if (gSpecialCharSets) {
    gSpecialCharSets->Reset(FreeCharSetMap, nsnull);
    delete gSpecialCharSets;
    gSpecialCharSets = nsnull;
  }
  if (gStretches) {
    delete gStretches;
    gStretches = nsnull;
  }
  NS_IF_RELEASE(gUnicode);
  NS_IF_RELEASE(gUserDefined);
  NS_IF_RELEASE(gZHTW);
  NS_IF_RELEASE(gZHHK);
  NS_IF_RELEASE(gZHTWHK);
  NS_IF_RELEASE(gUserDefinedConverter);
  NS_IF_RELEASE(gUsersLocale);
  NS_IF_RELEASE(gWesternLocale);
  NS_IF_RELEASE(gFontSubConverter);
  if (gWeights) {
    delete gWeights;
    gWeights = nsnull;
  }
  nsFontCharSetMap* charSetMap;
  for (charSetMap=gCharSetMap; charSetMap->mFontLangGroup; charSetMap++) {
    NS_IF_RELEASE(charSetMap->mFontLangGroup->mFontLangGroupAtom);
    charSetMap->mFontLangGroup->mFontLangGroupAtom = nsnull;
  }
  FreeCCMap(gUserDefinedCCMap);
  FreeCCMap(gEmptyCCMap);
}

/*
 * Initialize all the font lookup hash tables and other globals
 */
static nsresult
InitGlobals(nsIDeviceContext *aDevice)
{
#ifdef NS_FONT_DEBUG
  /* First check gfx/src/gtk/-specific env var "NS_FONT_DEBUG_GTK",
   * then the more general "NS_FONT_DEBUG" if "NS_FONT_DEBUG_GTK"
   * is not present */
  const char *debug = PR_GetEnv("NS_FONT_DEBUG_GTK");
  if (!debug) {
    debug = PR_GetEnv("NS_FONT_DEBUG");
  }
  
  if (debug) {
    PR_sscanf(debug, "%lX", &gFontDebug);
  }
#endif /* NS_FONT_DEBUG */

  NS_ENSURE_TRUE(nsnull != aDevice, NS_ERROR_NULL_POINTER);

  aDevice->GetCanonicalPixelScale(gDevScale);

  nsServiceManager::GetService(kCharSetManagerCID,
    NS_GET_IID(nsICharsetConverterManager), (nsISupports**) &gCharSetManager);
  if (!gCharSetManager) {
    FreeGlobals();
    return NS_ERROR_FAILURE;
  }
  nsServiceManager::GetService(kPrefCID, NS_GET_IID(nsIPref),
    (nsISupports**) &gPref);
  if (!gPref) {
    FreeGlobals();
    return NS_ERROR_FAILURE;
  }

  nsCompressedCharMap empty_ccmapObj;
  gEmptyCCMap = empty_ccmapObj.NewCCMap();
  if (!gEmptyCCMap)
    return NS_ERROR_OUT_OF_MEMORY;

  // get the "disable double byte font special chars" setting
  PRBool val = PR_TRUE;
  nsresult rv = gPref->GetBoolPref("font.allow_double_byte_special_chars", &val);
  if (NS_SUCCEEDED(rv))
    gAllowDoubleByteSpecialChars = val;

  PRInt32 scale_minimum = 0;
  rv = gPref->GetIntPref("font.scale.outline.min", &scale_minimum);
  if (NS_SUCCEEDED(rv)) {
    gOutlineScaleMinimum = scale_minimum;
    SIZE_FONT_PRINTF(("gOutlineScaleMinimum = %d", gOutlineScaleMinimum));
  }

  val = PR_TRUE;
  rv = gPref->GetBoolPref("font.scale.aa_bitmap.enable", &val);
  if (NS_SUCCEEDED(rv)) {
    gAABitmapScaleEnabled = val;
    SIZE_FONT_PRINTF(("gAABitmapScaleEnabled = %d", gAABitmapScaleEnabled));
  }

  val = PR_FALSE;
  rv = gPref->GetBoolPref("font.scale.aa_bitmap.always", &val);
  if (NS_SUCCEEDED(rv)) {
    gAABitmapScaleAlways = val;
    SIZE_FONT_PRINTF(("gAABitmapScaleAlways = %d", gAABitmapScaleAlways));
  }

  rv = gPref->GetIntPref("font.scale.aa_bitmap.min", &scale_minimum);
  if (NS_SUCCEEDED(rv)) {
    gAABitmapScaleMinimum = scale_minimum;
    SIZE_FONT_PRINTF(("gAABitmapScaleMinimum = %d", gAABitmapScaleMinimum));
  }

  PRInt32 percent = 0;
  rv = gPref->GetIntPref("font.scale.aa_bitmap.undersize", &percent);
  if ((NS_SUCCEEDED(rv)) && (percent)) {
    gAABitmapUndersize = percent/100.0;
    SIZE_FONT_PRINTF(("gAABitmapUndersize = %g", gAABitmapUndersize));
  }
  percent = 0;
  rv = gPref->GetIntPref("font.scale.aa_bitmap.oversize", &percent);
  if ((NS_SUCCEEDED(rv)) && (percent)) {
    gAABitmapOversize = percent/100.0;
    SIZE_FONT_PRINTF(("gAABitmapOversize = %g", gAABitmapOversize));
  }
  PRInt32 int_val = 0;
  rv = gPref->GetIntPref("font.scale.aa_bitmap.dark_text.min", &int_val);
  if (NS_SUCCEEDED(rv)) {
    gAASBDarkTextMinValue = int_val;
    SIZE_FONT_PRINTF(("gAASBDarkTextMinValue = %d", gAASBDarkTextMinValue));
  }
  nsXPIDLCString str;
  rv = gPref->GetCharPref("font.scale.aa_bitmap.dark_text.gain",
                           getter_Copies(str));
  if (NS_SUCCEEDED(rv)) {
    gAASBDarkTextGain = atof(str.get());
    SIZE_FONT_PRINTF(("gAASBDarkTextGain = %g", gAASBDarkTextGain));
  }
  int_val = 0;
  rv = gPref->GetIntPref("font.scale.aa_bitmap.light_text.min", &int_val);
  if (NS_SUCCEEDED(rv)) {
    gAASBLightTextMinValue = int_val;
    SIZE_FONT_PRINTF(("gAASBLightTextMinValue = %d", gAASBLightTextMinValue));
  }
  rv = gPref->GetCharPref("font.scale.aa_bitmap.light_text.gain",
                           getter_Copies(str));
  if (NS_SUCCEEDED(rv)) {
    gAASBLightTextGain = atof(str.get());
    SIZE_FONT_PRINTF(("gAASBLightTextGain = %g", gAASBLightTextGain));
  }

  rv = gPref->GetIntPref("font.scale.bitmap.min", &scale_minimum);
  if (NS_SUCCEEDED(rv)) {
    gBitmapScaleMinimum = scale_minimum;
    SIZE_FONT_PRINTF(("gBitmapScaleMinimum = %d", gBitmapScaleMinimum));
  }
  percent = 0;
  gPref->GetIntPref("font.scale.bitmap.oversize", &percent);
  if (percent) {
    gBitmapOversize = percent/100.0;
    SIZE_FONT_PRINTF(("gBitmapOversize = %g", gBitmapOversize));
  }
  percent = 0;
  gPref->GetIntPref("font.scale.bitmap.undersize", &percent);
  if (percent) {
    gBitmapUndersize = percent/100.0;
    SIZE_FONT_PRINTF(("gBitmapUndersize = %g", gBitmapUndersize));
  }

  PRBool force_outline_scaled_fonts = gForceOutlineScaledFonts;
  rv = gPref->GetBoolPref("font.x11.force_outline_scaled_fonts", &force_outline_scaled_fonts);
  if (NS_SUCCEEDED(rv)) {
    gForceOutlineScaledFonts = force_outline_scaled_fonts;
  }
  
  PRBool scale_bitmap_fonts_with_devscale = gScaleBitmapFontsWithDevScale;

  rv = gPref->GetBoolPref("font.x11.scale_bitmap_fonts_with_devscale", &scale_bitmap_fonts_with_devscale);
  if (NS_SUCCEEDED(rv)) {
    gScaleBitmapFontsWithDevScale = scale_bitmap_fonts_with_devscale;
  }

  gFFRENodes = new nsHashtable();
  if (!gFFRENodes) {
    FreeGlobals();
    return NS_ERROR_OUT_OF_MEMORY;
  }
  gAFRENodes = new nsHashtable();
  if (!gAFRENodes) {
    FreeGlobals();
    return NS_ERROR_OUT_OF_MEMORY;
  }
  gCachedFFRESearches = new nsHashtable();
  if (!gCachedFFRESearches) {
    FreeGlobals();
    return NS_ERROR_OUT_OF_MEMORY;
  }
  gFamilies = new nsHashtable();
  if (!gFamilies) {
    FreeGlobals();
    return NS_ERROR_OUT_OF_MEMORY;
  }
  gAliases = new nsHashtable();
  if (!gAliases) {
    FreeGlobals();
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsFontFamilyName* f = gFamilyNameTable;
  while (f->mName) {
    nsCStringKey key(f->mName);
    gAliases->Put(&key, (void *)f->mXName);
    f++;
  }
  gWeights = new nsHashtable();
  if (!gWeights) {
    FreeGlobals();
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsFontPropertyName* p = gWeightNames;
  while (p->mName) {
    nsCStringKey key(p->mName);
    gWeights->Put(&key, (void*) p->mValue);
    p++;
  }
  gStretches = new nsHashtable();
  if (!gStretches) {
    FreeGlobals();
    return NS_ERROR_OUT_OF_MEMORY;
  }
  p = gStretchNames;
  while (p->mName) {
    nsCStringKey key(p->mName);
    gStretches->Put(&key, (void*) p->mValue);
    p++;
  }
  gCharSetMaps = new nsHashtable();
  if (!gCharSetMaps) {
    FreeGlobals();
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsFontCharSetMap* charSetMap = gCharSetMap;
  while (charSetMap->mName) {
    nsCStringKey key(charSetMap->mName);
    gCharSetMaps->Put(&key, charSetMap);
    charSetMap++;
  }
  gSpecialCharSets = new nsHashtable();
  if (!gSpecialCharSets) {
    FreeGlobals();
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsFontCharSetMap* specialCharSetMap = gSpecialCharSetMap;
  while (specialCharSetMap->mName) {
    nsCStringKey key(specialCharSetMap->mName);
    gSpecialCharSets->Put(&key, specialCharSetMap);
    specialCharSetMap++;
  }

  gUnicode = NS_NewAtom("x-unicode");
  if (!gUnicode) {
    FreeGlobals();
    return NS_ERROR_OUT_OF_MEMORY;
  }
  gUserDefined = NS_NewAtom(USER_DEFINED);
  if (!gUserDefined) {
    FreeGlobals();
    return NS_ERROR_OUT_OF_MEMORY;
  }
  gZHTW = NS_NewAtom("zh-TW");
  if (!gZHTW) {
    FreeGlobals();
    return NS_ERROR_OUT_OF_MEMORY;
  }
  gZHHK = NS_NewAtom("zh-HK");
  if (!gZHHK) {
    FreeGlobals();
    return NS_ERROR_OUT_OF_MEMORY;
  }
  gZHTWHK = NS_NewAtom("x-zh-TWHK");
  if (!gZHTWHK) {
    FreeGlobals();
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // the user's locale
  nsCOMPtr<nsILanguageAtomService> langService;
  langService = do_GetService(NS_LANGUAGEATOMSERVICE_CONTRACTID);
  if (langService) {
    NS_IF_ADDREF(gUsersLocale = langService->GetLocaleLanguageGroup());
  }
  if (!gUsersLocale) {
    gUsersLocale = NS_NewAtom("x-western");
  }
  gWesternLocale = NS_NewAtom("x-western");
  if (!gUsersLocale) {
    FreeGlobals();
    return NS_ERROR_OUT_OF_MEMORY;
  }

  rv = nsX11AlphaBlendInitGlobals(GDK_DISPLAY());
  if (NS_FAILED(rv) || (!nsX11AlphaBlend::CanAntiAlias())) {
    gAABitmapScaleEnabled = PR_FALSE;
  }

  if (gAABitmapScaleEnabled) {
      gAABitmapScaleEnabled = nsXFontAAScaledBitmap::InitGlobals(GDK_DISPLAY(),
                                     DefaultScreen(GDK_DISPLAY()));
  }
  
#ifdef ENABLE_X_FONT_BANNING
  /* get the font banning pattern */
  nsXPIDLCString fbpattern;
  rv = gPref->GetCharPref("font.x11.rejectfontpattern", getter_Copies(fbpattern));
  if (NS_SUCCEEDED(rv)) {
    gFontRejectRegEx = new regex_t;
    if (!gFontRejectRegEx) {
      FreeGlobals();
      return NS_ERROR_OUT_OF_MEMORY;
    }
    
    /* Compile the pattern - and return an error if we get an invalid pattern... */
    if (regcomp(gFontRejectRegEx, fbpattern.get(), REG_EXTENDED|REG_NOSUB) != REG_OK) {
      PR_LOG(FontMetricsGTKLM, PR_LOG_DEBUG, ("Invalid rejectfontpattern '%s'\n", fbpattern.get()));
      BANNED_FONT_PRINTF(("Invalid font.x11.rejectfontpattern '%s'", fbpattern.get()));
      delete gFontRejectRegEx;
      gFontRejectRegEx = nsnull;
      
      FreeGlobals();
      return NS_ERROR_INVALID_ARG;
    }    
  }

  rv = gPref->GetCharPref("font.x11.acceptfontpattern", getter_Copies(fbpattern));
  if (NS_SUCCEEDED(rv)) {
    gFontAcceptRegEx = new regex_t;
    if (!gFontAcceptRegEx) {
      FreeGlobals();
      return NS_ERROR_OUT_OF_MEMORY;
    }
    
    /* Compile the pattern - and return an error if we get an invalid pattern... */
    if (regcomp(gFontAcceptRegEx, fbpattern.get(), REG_EXTENDED|REG_NOSUB) != REG_OK) {
      PR_LOG(FontMetricsGTKLM, PR_LOG_DEBUG, ("Invalid acceptfontpattern '%s'\n", fbpattern.get()));
      BANNED_FONT_PRINTF(("Invalid font.x11.acceptfontpattern '%s'", fbpattern.get()));
      delete gFontAcceptRegEx;
      gFontAcceptRegEx = nsnull;
      
      FreeGlobals();
      return NS_ERROR_INVALID_ARG;
    }    
  }
#endif /* ENABLE_X_FONT_BANNING */

  rv = nsFT2FontNode::InitGlobals();
  if (NS_FAILED(rv)) {
    FreeGlobals();
    return NS_ERROR_OUT_OF_MEMORY;
  }

  gInitialized = 1;

  return NS_OK;
}

// do the 8 to 16 bit conversion on the stack
// if the data is less than this size
#define WIDEN_8_TO_16_BUF_SIZE 1024

// handle 8 bit data with a 16 bit font
gint
Widen8To16AndMove(const gchar *char_p, 
                  gint char_len, 
                  XChar2b *xchar2b_p)
{
  int i;
  for (i=0; i<char_len; i++) {
    (xchar2b_p)->byte1 = 0;
    (xchar2b_p++)->byte2 = *char_p++;
  }
  return(char_len*2);
}

// handle 8 bit data with a 16 bit font
gint
Widen8To16AndGetWidth (nsXFont        *xFont,
                       const gchar    *text,
                       gint            text_length)
{
  NS_ASSERTION(!xFont->IsSingleByte(),"wrong string/font size");
  XChar2b buf[WIDEN_8_TO_16_BUF_SIZE];
  XChar2b *p = buf;
  int uchar_size;
  gint rawWidth;

  if (text_length > WIDEN_8_TO_16_BUF_SIZE) {
    p = (XChar2b*)PR_Malloc(text_length*sizeof(XChar2b));
    if (!p) return(0); // handle malloc failure
  }

  uchar_size = Widen8To16AndMove(text, text_length, p);
  rawWidth = xFont->TextWidth16(p, uchar_size/2);

  if (text_length > WIDEN_8_TO_16_BUF_SIZE) {
    PR_Free((char*)p);
  }
  return(rawWidth);
}

void
Widen8To16AndDraw (GdkDrawable *drawable,
                   nsXFont     *xFont,
                   GdkGC       *gc,
                   gint         x,
                   gint         y,
                   const gchar *text,
                   gint         text_length)
{
  NS_ASSERTION(!xFont->IsSingleByte(),"wrong string/font size");
  XChar2b buf[WIDEN_8_TO_16_BUF_SIZE];
  XChar2b *p = buf;
  int uchar_size;

  if (text_length > WIDEN_8_TO_16_BUF_SIZE) {
    p = (XChar2b*)PR_Malloc(text_length*sizeof(XChar2b));
    if (!p) return; // handle malloc failure
  }

  uchar_size = Widen8To16AndMove(text, text_length, p);
  xFont->DrawText16(drawable, gc, x, y, p, uchar_size/2);

  if (text_length > WIDEN_8_TO_16_BUF_SIZE) {
    PR_Free((char*)p);
  }
}

#ifdef MOZ_MATHML

void
Widen8To16AndGetTextExtents (nsXFont *xFont,  
                        const gchar *text,
                        gint         text_length,
                        gint        *lbearing,
                        gint        *rbearing,
                        gint        *width,
                        gint        *ascent,
                        gint        *descent)
{
  NS_ASSERTION(!xFont->IsSingleByte(),"wrong string/font size");
  XChar2b buf[WIDEN_8_TO_16_BUF_SIZE];
  XChar2b *p = buf;
  int uchar_size;

  if (text_length > WIDEN_8_TO_16_BUF_SIZE) {
    p = (XChar2b*)PR_Malloc(text_length*sizeof(XChar2b));
    if (!p) { // handle malloc failure
      *lbearing = 0;
      *rbearing = 0;
      *width    = 0;
      *ascent   = 0;
      *descent  = 0;
      return;
    }
  }

  uchar_size = Widen8To16AndMove(text, text_length, p);
  xFont->TextExtents16(p, uchar_size/2,
                    lbearing, 
                    rbearing, 
                    width, 
                    ascent, 
                    descent);

  if (text_length > WIDEN_8_TO_16_BUF_SIZE) {
    PR_Free((char*)p);
  }
}

#endif /* MOZ_MATHML */

nsFontMetricsGTK::nsFontMetricsGTK()
  : mFonts() // I'm not sure what the common size is here - I generally
  // see 2-5 entries.  For now, punt and let it be allocated later.  We can't
  // make it an nsAutoVoidArray since it's a cString array.
  // XXX mFontIsGeneric will generally need to be the same size; right now
  // it's an nsAutoVoidArray.  If the average is under 8, that's ok.
{
  gFontMetricsGTKCount++;
}

nsFontMetricsGTK::~nsFontMetricsGTK()
{
  // do not free mGeneric here

  if (nsnull != mFont) {
    delete mFont;
    mFont = nsnull;
  }

  if (mLoadedFonts) {
    PR_Free(mLoadedFonts);
    mLoadedFonts = nsnull;
  }

  if (mSubstituteFont) {
    delete mSubstituteFont;
    mSubstituteFont = nsnull;
  }

  mWesternFont = nsnull;
  mCurrentFont = nsnull;

  if (mDeviceContext) {
    // Notify our device context that owns us so that it can update its font cache
    mDeviceContext->FontMetricsDeleted(this);
    mDeviceContext = nsnull;
  }

  if (!--gFontMetricsGTKCount) {
    FreeGlobals();
  }
}

NS_IMPL_ISUPPORTS1(nsFontMetricsGTK, nsIFontMetrics)

static PRBool
IsASCIIFontName(const nsString& aName)
{
  PRUint32 len = aName.Length();
  const PRUnichar* str = aName.get();
  for (PRUint32 i = 0; i < len; i++) {
    /*
     * X font names are printable ASCII, ignore others (for now)
     */
    if ((str[i] < 0x20) || (str[i] > 0x7E)) {
      return PR_FALSE;
    }
  }

  return PR_TRUE;
}

static PRBool
FontEnumCallback(const nsString& aFamily, PRBool aGeneric, void *aData)
{
#ifdef REALLY_NOISY_FONTS
  printf("font = '");
  fputs(NS_LossyConvertUCS2toASCII(aFamily).get(), stdout);
  printf("'\n");
#endif

  if (!IsASCIIFontName(aFamily)) {
    return PR_TRUE; // skip and continue
  }

  nsCAutoString name;
  name.AssignWithConversion(aFamily.get());
  ToLowerCase(name);
  nsFontMetricsGTK* metrics = (nsFontMetricsGTK*) aData;
  metrics->mFonts.AppendCString(name);
  metrics->mFontIsGeneric.AppendElement((void*) aGeneric);
  if (aGeneric) {
    metrics->mGeneric = metrics->mFonts.CStringAt(metrics->mFonts.Count() - 1);
    return PR_FALSE; // stop
  }

  return PR_TRUE; // continue
}

NS_IMETHODIMP nsFontMetricsGTK::Init(const nsFont& aFont, nsIAtom* aLangGroup,
  nsIDeviceContext* aContext)
{
  NS_ASSERTION(!(nsnull == aContext), "attempt to init fontmetrics with null device context");

  nsresult res = NS_OK;
  mDocConverterType = nsnull;

  if (!gInitialized) {
    res = InitGlobals(aContext);
    if (NS_FAILED(res))
      return res;
  }

  mFont = new nsFont(aFont);
  mLangGroup = aLangGroup;

  mDeviceContext = aContext;

  float app2dev;
  app2dev = mDeviceContext->AppUnitsToDevUnits();

  mPixelSize = NSToIntRound(app2dev * mFont->size);
  // Make sure to clamp the pixel size to something reasonable so we
  // don't make the X server blow up.
  mPixelSize = PR_MIN(gdk_screen_height() * FONT_MAX_FONT_SCALE, mPixelSize);

  mStretchIndex = 4; // normal
  mStyleIndex = mFont->style;

  mFont->EnumerateFamilies(FontEnumCallback, this);
  nsXPIDLCString value;
  if (!mGeneric) {
    gPref->CopyCharPref("font.default", getter_Copies(value));
    if (value.get()) {
      mDefaultFont = value.get();
    }
    else {
      mDefaultFont = "serif";
    }
    mGeneric = &mDefaultFont;
  }

  if (mLangGroup) {
    nsCAutoString name("font.min-size.");
    if (mGeneric->Equals("monospace")) {
      name.Append("fixed");
    }
    else {
      name.Append("variable");
    }
    name.Append(char('.'));
    const char* langGroup = nsnull;
    mLangGroup->GetUTF8String(&langGroup);
    name.Append(langGroup);
    PRInt32 minimum = 0;
    res = gPref->GetIntPref(name.get(), &minimum);
    if (NS_FAILED(res)) {
      gPref->GetDefaultIntPref(name.get(), &minimum);
    }
    if (minimum < 0) {
      minimum = 0;
    }
    if (mPixelSize < minimum) {
      mPixelSize = minimum;
    }
  }

  if (mLangGroup.get() == gUserDefined) {
    if (!gUserDefinedConverter) {
      res = gCharSetManager->GetUnicodeEncoderRaw("x-user-defined",
                                                 &gUserDefinedConverter);
      if (NS_FAILED(res)) {
        return res;
      }
      res = gUserDefinedConverter->SetOutputErrorBehavior(
          gUserDefinedConverter->kOnError_Replace, nsnull, '?');
      nsCOMPtr<nsICharRepresentable> mapper =
        do_QueryInterface(gUserDefinedConverter);
      if (mapper) {
        gUserDefinedCCMap = MapperToCCMap(mapper);
        if (!gUserDefinedCCMap)
          return NS_ERROR_OUT_OF_MEMORY;          
      }
    }

    nsCAutoString name("font.name.");
    name.Append(*mGeneric);
    name.Append(char('.'));
    name.Append(USER_DEFINED);
    gPref->CopyCharPref(name.get(), getter_Copies(value));
    if (value.get()) {
      mUserDefined = value.get();
      mIsUserDefined = 1;
    }
  }

  mWesternFont = FindFont('a');
  if (!mWesternFont) {
    return NS_ERROR_FAILURE;
  }


  mCurrentFont = mWesternFont;

  RealizeFont();

  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsGTK::Destroy()
{
  mDeviceContext = nsnull;
  return NS_OK;
}

void nsFontMetricsGTK::RealizeFont()
{
  float f;
  f = mDeviceContext->DevUnitsToAppUnits();

  if (mWesternFont->IsFreeTypeFont()) {
    nsFreeTypeFont *ft = (nsFreeTypeFont *)mWesternFont;
    if (!ft)
      return;
    // now that there are multiple font types (eg: core X fonts
    // and TrueType fonts) there should be a common set of methods 
    // to get the metrics info from the font object. These methods
    // probably should be virtual functions defined in nsFontGTK.
#if (defined(MOZ_ENABLE_FREETYPE2))
    int lineSpacing = ft->ascent() + ft->descent();
    if (lineSpacing > mWesternFont->mSize) {
      mLeading = nscoord((lineSpacing - mWesternFont->mSize) * f);
    }
    else {
      mLeading = 0;
    }
    mEmHeight = PR_MAX(1, nscoord(mWesternFont->mSize * f));
    mEmAscent = nscoord(ft->ascent() * mWesternFont->mSize * f / lineSpacing);
    mEmDescent = mEmHeight - mEmAscent;

    mMaxHeight  = nscoord((ft->max_ascent() + ft->max_descent()) * f);
    mMaxAscent  = nscoord(ft->max_ascent() * f) ;
    mMaxDescent = nscoord(ft->max_descent() * f);

    mMaxAdvance = nscoord(ft->max_width() * f);

    // 56% of ascent, best guess for non-true type
    mXHeight = NSToCoordRound((float) ft->ascent()* f * 0.56f);

    PRUnichar space = (PRUnichar)' ';
    mSpaceWidth = NSToCoordRound(ft->GetWidth(&space, 1) * f);

    PRUnichar averageX = (PRUnichar)'x';
    mAveCharWidth = NSToCoordRound(ft->GetWidth(&averageX, 1) * f);

    unsigned long pr = 0;
    if (ft->getXHeight(pr)) {
      mXHeight = nscoord(pr * f);
    }

    float height;
    long val;
    if (ft->underlinePosition(val)) {
      /* this will only be provided from adobe .afm fonts and TrueType
       * fonts served by xfsft (not xfstt!) */
      mUnderlineOffset = -NSToIntRound(val * f);
    }
    else {
      height = ft->ascent() + ft->descent();
      mUnderlineOffset = -NSToIntRound(MAX (1, floor (0.1 * height + 0.5)) * f);
    }

    if (ft->underline_thickness(pr)) {
      /* this will only be provided from adobe .afm fonts */
      mUnderlineSize = nscoord(MAX(f, NSToIntRound(pr * f)));
    }
    else {
      height = ft->ascent() + ft->descent();
      mUnderlineSize = NSToIntRound(MAX(1, floor (0.05 * height + 0.5)) * f);
    }

    if (ft->superscript_y(val)) {
      mSuperscriptOffset = nscoord(MAX(f, NSToIntRound(val * f)));
    }
    else {
      mSuperscriptOffset = mXHeight;
    }

    if (ft->subscript_y(val)) {
      mSubscriptOffset = nscoord(MAX(f, NSToIntRound(val * f)));
    }
    else {
     mSubscriptOffset = mXHeight;
    }

    /* need better way to calculate this */
    mStrikeoutOffset = NSToCoordRound(mXHeight / 2.0);
    mStrikeoutSize = mUnderlineSize;

    return;
#endif /* (defined(MOZ_ENABLE_FREETYPE2)) */
  }
  nsXFont *xFont = mWesternFont->GetXFont();
  XFontStruct *fontInfo = xFont->GetXFontStruct();
  f = mDeviceContext->DevUnitsToAppUnits();

  nscoord lineSpacing = nscoord((fontInfo->ascent + fontInfo->descent) * f);
  mEmHeight = PR_MAX(1, nscoord(mWesternFont->mSize * f));
  if (lineSpacing > mEmHeight) {
    mLeading = lineSpacing - mEmHeight;
  }
  else {
    mLeading = 0;
  }
  mMaxHeight = nscoord((fontInfo->ascent + fontInfo->descent) * f);
  mMaxAscent = nscoord(fontInfo->ascent * f);
  mMaxDescent = nscoord(fontInfo->descent * f);

  mEmAscent = nscoord(mMaxAscent * mEmHeight / lineSpacing);
  mEmDescent = mEmHeight - mEmAscent;

  mMaxAdvance = nscoord(fontInfo->max_bounds.width * f);

  gint rawWidth, rawAverage;
  if ((fontInfo->min_byte1 == 0) && (fontInfo->max_byte1 == 0)) {
    rawWidth = xFont->TextWidth8(" ", 1);
    rawAverage = xFont->TextWidth8("x", 1);
  }
  else {
    XChar2b _16bit_space, _16bit_x;
    _16bit_space.byte1 = 0;
    _16bit_space.byte2 = ' ';
    _16bit_x.byte1 = 0;
    _16bit_x.byte2 = 'x';
    rawWidth = xFont->TextWidth16(&_16bit_space, sizeof(_16bit_space)/2);
    rawAverage = xFont->TextWidth16(&_16bit_x, sizeof( _16bit_x)/2);
  }
  mSpaceWidth = NSToCoordRound(rawWidth * f);
  mAveCharWidth = NSToCoordRound(rawAverage * f);

  unsigned long pr = 0;
  if (xFont->GetXFontProperty(XA_X_HEIGHT, &pr) && pr != 0 &&
      pr < 0x00ffffff)  // Bug 43214: arbitrary to exclude garbage values
  {
    mXHeight = nscoord(pr * f);
#ifdef REALLY_NOISY_FONTS
    printf("xHeight=%d\n", mXHeight);
#endif
  }
  else 
  {
    // 56% of ascent, best guess for non-true type
    mXHeight = NSToCoordRound((float) fontInfo->ascent* f * 0.56f);
  }

  if (xFont->GetXFontProperty(XA_UNDERLINE_POSITION, &pr))
  {
    /* this will only be provided from adobe .afm fonts and TrueType
     * fonts served by xfsft (not xfstt!) */
    mUnderlineOffset = -NSToIntRound(pr * f);
#ifdef REALLY_NOISY_FONTS
    printf("underlineOffset=%d\n", mUnderlineOffset);
#endif
  }
  else
  {
    /* this may need to be different than one for those weird asian fonts */
    float height;
    height = fontInfo->ascent + fontInfo->descent;
    mUnderlineOffset = -NSToIntRound(MAX (1, floor (0.1 * height + 0.5)) * f);
  }

  if (xFont->GetXFontProperty(XA_UNDERLINE_THICKNESS, &pr))
  {
    /* this will only be provided from adobe .afm fonts */
    mUnderlineSize = nscoord(MAX(f, NSToIntRound(pr * f)));
#ifdef REALLY_NOISY_FONTS
    printf("underlineSize=%d\n", mUnderlineSize);
#endif
  }
  else
  {
    float height;
    height = fontInfo->ascent + fontInfo->descent;
    mUnderlineSize = NSToIntRound(MAX(1, floor (0.05 * height + 0.5)) * f);
  }

  if (xFont->GetXFontProperty(XA_SUPERSCRIPT_Y, &pr))
  {
    mSuperscriptOffset = nscoord(MAX(f, NSToIntRound(pr * f)));
#ifdef REALLY_NOISY_FONTS
    printf("superscriptOffset=%d\n", mSuperscriptOffset);
#endif
  }
  else
  {
    mSuperscriptOffset = mXHeight;
  }

  if (xFont->GetXFontProperty(XA_SUBSCRIPT_Y, &pr))
  {
    mSubscriptOffset = nscoord(MAX(f, NSToIntRound(pr * f)));
#ifdef REALLY_NOISY_FONTS
    printf("subscriptOffset=%d\n", mSubscriptOffset);
#endif
  }
  else
  {
    mSubscriptOffset = mXHeight;
  }

  /* need better way to calculate this */
  mStrikeoutOffset = NSToCoordRound(mXHeight / 2.0);
  mStrikeoutSize = mUnderlineSize;
}

NS_IMETHODIMP  nsFontMetricsGTK::GetXHeight(nscoord& aResult)
{
  aResult = mXHeight;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsGTK::GetSuperscriptOffset(nscoord& aResult)
{
  aResult = mSuperscriptOffset;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsGTK::GetSubscriptOffset(nscoord& aResult)
{
  aResult = mSubscriptOffset;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsGTK::GetStrikeout(nscoord& aOffset, nscoord& aSize)
{
  aOffset = mStrikeoutOffset;
  aSize = mStrikeoutSize;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsGTK::GetUnderline(nscoord& aOffset, nscoord& aSize)
{
  aOffset = mUnderlineOffset;
  aSize = mUnderlineSize;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsGTK::GetHeight(nscoord &aHeight)
{
  aHeight = mMaxHeight;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsGTK::GetNormalLineHeight(nscoord &aHeight)
{
  aHeight = mEmHeight + mLeading;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsGTK::GetLeading(nscoord &aLeading)
{
  aLeading = mLeading;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsGTK::GetEmHeight(nscoord &aHeight)
{
  aHeight = mEmHeight;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsGTK::GetEmAscent(nscoord &aAscent)
{
  aAscent = mEmAscent;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsGTK::GetEmDescent(nscoord &aDescent)
{
  aDescent = mEmDescent;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsGTK::GetMaxHeight(nscoord &aHeight)
{
  aHeight = mMaxHeight;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsGTK::GetMaxAscent(nscoord &aAscent)
{
  aAscent = mMaxAscent;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsGTK::GetMaxDescent(nscoord &aDescent)
{
  aDescent = mMaxDescent;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsGTK::GetMaxAdvance(nscoord &aAdvance)
{
  aAdvance = mMaxAdvance;
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsGTK::GetAveCharWidth(nscoord &aAveCharWidth)
{
  aAveCharWidth = mAveCharWidth;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsGTK::GetFont(const nsFont*& aFont)
{
  aFont = mFont;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsGTK::GetLangGroup(nsIAtom** aLangGroup)
{
  if (!aLangGroup) {
    return NS_ERROR_NULL_POINTER;
  }

  *aLangGroup = mLangGroup;
  NS_IF_ADDREF(*aLangGroup);

  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsGTK::GetFontHandle(nsFontHandle &aHandle)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsFontGTK*
nsFontMetricsGTK::LocateFont(PRUint32 aChar, PRInt32 & aCount)
{
  nsFontGTK *font;
  PRInt32 i;

  // see if one of our loaded fonts can represent the character
  for (i = 0; i < aCount; ++i) {
    font = (nsFontGTK*)mLoadedFonts[i];
    if (CCMAP_HAS_CHAR_EXT(font->mCCMap, aChar))
      return font;
  }

  font = FindFont(aChar);
  aCount = mLoadedFontsCount; // update since FindFont() can change it

  return font;
}

nsresult
nsFontMetricsGTK::ResolveForwards(const PRUnichar        *aString,
                                  PRUint32                aLength,
                                  nsFontSwitchCallbackGTK aFunc, 
                                  void                   *aData)
{
  NS_ASSERTION(aString || !aLength, "invalid call");
  const PRUnichar* firstChar = aString;
  const PRUnichar* currChar = firstChar;
  const PRUnichar* lastChar  = aString + aLength;
  nsFontGTK* currFont;
  nsFontGTK* nextFont;
  PRInt32 count;
  nsFontSwitchGTK fontSwitch;

  if (firstChar == lastChar)
    return NS_OK;

  count = mLoadedFontsCount;

  if (IS_HIGH_SURROGATE(*currChar) && (currChar+1) < lastChar && IS_LOW_SURROGATE(*(currChar+1))) {
    currFont = LocateFont(SURROGATE_TO_UCS4(*currChar, *(currChar+1)), count);
    currChar += 2;
  }
  else {
    currFont = LocateFont(*currChar, count);
    ++currChar;
  }

  //This if block is meant to speedup the process in normal situation, when
  //most characters can be found in first font
  if (currFont == mLoadedFonts[0]) {
    while (currChar < lastChar && CCMAP_HAS_CHAR_EXT(currFont->mCCMap,*currChar))
      ++currChar;
    fontSwitch.mFontGTK = currFont;
    if (!(*aFunc)(&fontSwitch, firstChar, currChar - firstChar, aData))
      return NS_OK;
    if (currChar == lastChar)
      return NS_OK;
    // continue with the next substring, re-using the available loaded fonts
    firstChar = currChar;
    if (IS_HIGH_SURROGATE(*currChar) && (currChar+1) < lastChar && IS_LOW_SURROGATE(*(currChar+1))) {
      currFont = LocateFont(SURROGATE_TO_UCS4(*currChar, *(currChar+1)), count);
      currChar += 2;
    }
    else {
      currFont = LocateFont(*currChar, count);
      ++currChar;
    }
  }

  // see if we can keep the same font for adjacent characters
  PRInt32 lastCharLen;
  while (currChar < lastChar) {
    if (IS_HIGH_SURROGATE(*currChar) && (currChar+1) < lastChar && IS_LOW_SURROGATE(*(currChar+1))) {
      nextFont = LocateFont(SURROGATE_TO_UCS4(*currChar, *(currChar+1)), count);
      lastCharLen = 2;
    }
    else {
      nextFont = LocateFont(*currChar, count);
      lastCharLen = 1;
    }
    if (nextFont != currFont) {
      // We have a substring that can be represented with the same font, and
      // we are about to switch fonts, it is time to notify our caller.
      fontSwitch.mFontGTK = currFont;
      if (!(*aFunc)(&fontSwitch, firstChar, currChar - firstChar, aData))
        return NS_OK;
      // continue with the next substring, re-using the available loaded fonts
      firstChar = currChar;

      currFont = nextFont; // use the font found earlier for the char
    }
    currChar += lastCharLen;
  }

  //do it for last part of the string
  fontSwitch.mFontGTK = currFont;
  (*aFunc)(&fontSwitch, firstChar, currChar - firstChar, aData);
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsGTK::GetSpaceWidth(nscoord &aSpaceWidth)
{
  aSpaceWidth = mSpaceWidth;
  return NS_OK;
}

/*
 * CSS2 "font properties":
 *   font-family
 *   font-style
 *   font-variant
 *   font-weight
 *   font-stretch
 *   font-size
 *   font-size-adjust
 *   font
 */

/*
 * CSS2 "font descriptors":
 *   font-family
 *   font-style
 *   font-variant
 *   font-weight
 *   font-stretch
 *   font-size
 *   unicode-range
 *   units-per-em
 *   src
 *   panose-1
 *   stemv
 *   stemh
 *   slope
 *   cap-height
 *   x-height
 *   ascent
 *   descent
 *   widths
 *   bbox
 *   definition-src
 *   baseline
 *   centerline
 *   mathline
 *   topline
 */

/*
 * XLFD 1.5 "FontName fields":
 *   FOUNDRY
 *   FAMILY_NAME
 *   WEIGHT_NAME
 *   SLANT
 *   SETWIDTH_NAME
 *   ADD_STYLE_NAME
 *   PIXEL_SIZE
 *   POINT_SIZE
 *   RESOLUTION_X
 *   RESOLUTION_Y
 *   SPACING
 *   AVERAGE_WIDTH
 *   CHARSET_REGISTRY
 *   CHARSET_ENCODING
 * XLFD example:
 *   -adobe-times-medium-r-normal--17-120-100-100-p-84-iso8859-1
 */

/*
 * XLFD 1.5 "font properties":
 *   FOUNDRY
 *   FAMILY_NAME
 *   WEIGHT_NAME
 *   SLANT
 *   SETWIDTH_NAME
 *   ADD_STYLE_NAME
 *   PIXEL_SIZE
 *   POINT_SIZE
 *   RESOLUTION_X
 *   RESOLUTION_Y
 *   SPACING
 *   AVERAGE_WIDTH
 *   CHARSET_REGISTRY
 *   CHARSET_ENCODING
 *   MIN_SPACE
 *   NORM_SPACE
 *   MAX_SPACE
 *   END_SPACE
 *   AVG_CAPITAL_WIDTH
 *   AVG_LOWERCASE_WIDTH
 *   QUAD_WIDTH
 *   FIGURE_WIDTH
 *   SUPERSCRIPT_X
 *   SUPERSCRIPT_Y
 *   SUBSCRIPT_X
 *   SUBSCRIPT_Y
 *   SUPERSCRIPT_SIZE
 *   SUBSCRIPT_SIZE
 *   SMALL_CAP_SIZE
 *   UNDERLINE_POSITION
 *   UNDERLINE_THICKNESS
 *   STRIKEOUT_ASCENT
 *   STRIKEOUT_DESCENT
 *   ITALIC_ANGLE
 *   CAP_HEIGHT
 *   X_HEIGHT
 *   RELATIVE_SETWIDTH
 *   RELATIVE_WEIGHT
 *   WEIGHT
 *   RESOLUTION
 *   FONT
 *   FACE_NAME
 *   FULL_NAME
 *   COPYRIGHT
 *   NOTICE
 *   DESTINATION
 *   FONT_TYPE
 *   FONT_VERSION
 *   RASTERIZER_NAME
 *   RASTERIZER_VERSION
 *   RAW_ASCENT
 *   RAW_DESCENT
 *   RAW_*
 *   AXIS_NAMES
 *   AXIS_LIMITS
 *   AXIS_TYPES
 */

/*
 * XLFD 1.5 BDF 2.1 properties:
 *   FONT_ASCENT
 *   FONT_DESCENT
 *   DEFAULT_CHAR
 */

/*
 * CSS2 algorithm, in the following order:
 *   font-family:  FAMILY_NAME (and FOUNDRY? (XXX))
 *   font-style:   SLANT (XXX: XLFD's RI and RO)
 *   font-variant: implemented in mozilla/layout/html/base/src/nsTextFrame.cpp
 *   font-weight:  RELATIVE_WEIGHT (XXX), WEIGHT (XXX), WEIGHT_NAME
 *   font-size:    XFontStruct.max_bounds.ascent + descent
 *
 * The following property is not specified in the algorithm spec. It will be
 * inserted between the font-weight and font-size steps for now:
 *   font-stretch: RELATIVE_SETWIDTH (XXX), SETWIDTH_NAME
 */

/*
 * XXX: Things to investigate in the future:
 *   ADD_STYLE_NAME font-family's serif and sans-serif
 *   SPACING        font-family's monospace; however, there are very few
 *                  proportional fonts in non-Latin-1 charsets, so beware in
 *                  font prefs dialog
 *   AVERAGE_WIDTH  none (see SETWIDTH_NAME)
 */

static gint
SingleByteConvert(nsFontCharSetInfo* aSelf, XFontStruct* aFont,
  const PRUnichar* aSrcBuf, PRInt32 aSrcLen, char* aDestBuf, PRInt32 aDestLen)
{
  gint count = 0;
  if (aSelf->mConverter) {
    aSelf->mConverter->Convert(aSrcBuf, &aSrcLen, aDestBuf, &aDestLen);
    count = aDestLen;
  }

  return count;
}

/*
static void 
ReverseBuffer(char* aBuf, gint count)
{
    char *head, *tail, *med;
    head = aBuf;
    tail = &aBuf[count-1];
    med = &aBuf[count/2];

    while(head < med)
    {
       char tmp = *head;
       *head++ = *tail;
       *tail-- = tmp;
    }
}
*/

// the following code assume all the PRUnichar is draw in the same
// direction- left to right, without mixing with characters which should
// draw from right to left. This mean it should not be used untill the 
// upper level code resolve bi-di and ensure this assumption. otherwise
// it may break non-bidi pages on a system which have hebrew/arabic fonts
/*
static gint
SingleByteConvertReverse(nsFontCharSetInfo* aSelf, const PRUnichar* aSrcBuf,
  PRInt32 aSrcLen, char* aDestBuf, PRInt32 aDestLen)
{
    gint count = SingleByteConvert(aSelf, aSrcBuf,
                       aSrcLen, aDestBuf,  aDestLen);
    ReverseBuffer(aDestBuf, count);
    return count;
}
*/

static gint
DoubleByteConvert(nsFontCharSetInfo* aSelf, XFontStruct* aFont,
  const PRUnichar* aSrcBuf, PRInt32 aSrcLen, char* aDestBuf, PRInt32 aDestLen)
{
  gint count;
  if (aSelf->mConverter) {
    aSelf->mConverter->Convert(aSrcBuf, &aSrcLen, aDestBuf, &aDestLen);
    count = aDestLen;
    if (count > 0) {
      if ((aDestBuf[0] & 0x80) && (!(aFont->max_byte1 & 0x80))) {
        for (PRInt32 i = 0; i < aDestLen; i++) {
          aDestBuf[i] &= 0x7F;
        }
      }
      // We're using a GL encoder (KSC5601 or GB2312) but the font is a GR font.
      // (ksc5601.1987-1 or gb2312.1980-1)
      else if ((!(aDestBuf[0] & 0x80)) && (aFont->min_byte1 & 0x80)) {
        for (PRInt32 i = 0; i < aDestLen; i++) {
          aDestBuf[i] |= 0x80;
        }
      }
    }
  }
  else {
    count = 0;
  }

  return count;
}

static gint
ISO10646Convert(nsFontCharSetInfo* aSelf, XFontStruct* aFont,
  const PRUnichar* aSrcBuf, PRInt32 aSrcLen, char* aDestBuf, PRInt32 aDestLen)
{
  aDestLen /= 2;
  if (aSrcLen > aDestLen) {
    aSrcLen = aDestLen;
  }
  if (aSrcLen < 0) {
    aSrcLen = 0;
  }
  XChar2b* dest = (XChar2b*) aDestBuf;
  for (PRInt32 i = 0; i < aSrcLen; i++) {
    dest[i].byte1 = (aSrcBuf[i] >> 8);
    dest[i].byte2 = (aSrcBuf[i] & 0xFF);
  }

  return (gint) aSrcLen * 2;
}

#ifdef DEBUG

static void
CheckMap(nsFontCharSetMap* aEntry)
{
  while (aEntry->mName) {
    if (aEntry->mInfo->mCharSet) {
      // used to use NS_NewAtom??
      nsresult res;
      nsCOMPtr<nsIUnicodeEncoder> converter;
      res = gCharSetManager->GetUnicodeEncoderRaw(aEntry->mInfo->mCharSet,
                                                  getter_AddRefs(converter));
      if (NS_FAILED(res)) {
        printf("=== %s failed (%s)\n", aEntry->mInfo->mCharSet, __FILE__);
      }
    }
    aEntry++;
  }
}

static void
CheckSelf(void)
{
  CheckMap(gCharSetMap);

#ifdef MOZ_MATHML
  // For this to pass, the ucvmath module must be built as well
  CheckMap(gSpecialCharSetMap);
#endif
}

#endif /* DEBUG */

static PRBool
SetUpFontCharSetInfo(nsFontCharSetInfo* aSelf)
{

#ifdef DEBUG
  static int checkedSelf = 0;
  if (!checkedSelf) {
    CheckSelf();
    checkedSelf = 1;
  }
#endif

  nsresult res;
  // used NS_NewAtom before??
  nsIUnicodeEncoder* converter = nsnull;
  res = gCharSetManager->GetUnicodeEncoderRaw(aSelf->mCharSet, &converter);
  if (NS_SUCCEEDED(res)) {
    aSelf->mConverter = converter;
    res = converter->SetOutputErrorBehavior(converter->kOnError_Replace,
                                            nsnull, '?');
    nsCOMPtr<nsICharRepresentable> mapper = do_QueryInterface(converter);
    if (mapper) {
      aSelf->mCCMap = MapperToCCMap(mapper);
      if (aSelf->mCCMap) {
#ifdef DEBUG_bzbarsky
          NS_WARNING(nsPrintfCString("\n\ncharset = %s", aSelf->mCharSet).get());
#endif /* DEBUG */
  
        /*
         * We used to disable special characters like smart quotes
         * in CJK fonts because if they are quite a bit larger than
         * western glyphs and we did not want glyph fill-in to use them
         * in single byte documents.
         *
         * Now, single byte documents find these special chars before
         * the CJK fonts are searched so this is no longer needed
         * but is useful when trying to determine which font(s) the
         * special chars are found in.
         */
        if ((aSelf->Convert == DoubleByteConvert) 
            && (!gAllowDoubleByteSpecialChars)) {
          PRUint16* ccmap = aSelf->mCCMap;
          PRUint32 page = CCMAP_BEGIN_AT_START_OF_MAP;
          const PRUint16* specialmap = gDoubleByteSpecialCharsCCMap;
          while (NextNonEmptyCCMapPage(specialmap, &page)) {
            PRUint32 pagechar = page;
            for (int i=0; i < CCMAP_BITS_PER_PAGE; i++) {
              if (CCMAP_HAS_CHAR(specialmap, pagechar)) 
                CCMAP_UNSET_CHAR(ccmap, pagechar);
              pagechar++;
            }
          }
        }
        return PR_TRUE;
      }
    }
    else {
      NS_WARNING("cannot get nsICharRepresentable");
    }
  }
  else {
    NS_WARNING("cannot get Unicode converter");
  }

  //
  // always try to return a map even if it is empty
  //
  nsCompressedCharMap empty_ccmapObj;
  aSelf->mCCMap = empty_ccmapObj.NewCCMap();

  // return false if unable to alloc a map
  if (aSelf->mCCMap == nsnull)
    return PR_FALSE;

  return PR_TRUE;
}

#undef DEBUG_DUMP_TREE
#ifdef DEBUG_DUMP_TREE

static char* gDumpStyles[3] = { "normal", "italic", "oblique" };

static PRIntn
DumpCharSet(PLHashEntry* he, PRIntn i, void* arg)
{
  printf("        %s\n", (char*) he->key);
  nsFontCharSet* charSet = (nsFontCharSet*) he->value;
  for (int sizeIndex = 0; sizeIndex < charSet->mSizesCount; sizeIndex++) {
    nsFontGTK* size = &charSet->mSizes[sizeIndex];
    printf("          %d %s\n", size->mSize, size->mName);
  }
  return HT_ENUMERATE_NEXT;
}

static void
DumpFamily(nsFontFamily* aFamily)
{
  for (int styleIndex = 0; styleIndex < 3; styleIndex++) {
    nsFontStyle* style = aFamily->mStyles[styleIndex];
    if (style) {
      printf("  style: %s\n", gDumpStyles[styleIndex]);
      for (int weightIndex = 0; weightIndex < 8; weightIndex++) {
        nsFontWeight* weight = style->mWeights[weightIndex];
        if (weight) {
          printf("    weight: %d\n", (weightIndex + 1) * 100);
          for (int stretchIndex = 0; stretchIndex < 9; stretchIndex++) {
            nsFontStretch* stretch = weight->mStretches[stretchIndex];
            if (stretch) {
              printf("      stretch: %d\n", stretchIndex + 1);
              PL_HashTableEnumerateEntries(stretch->mCharSets, DumpCharSet,
                nsnull);
            }
          }
        }
      }
    }
  }
}

// this existing debug code was broken and I have partly fixed it
static PRBool
DumpFamilyEnum(nsHashKey* hashKey, void *aData, void* closure)
{
  printf("family: %s\n",
         NS_LossyConvertUCS2toASCII(*NS_STATIC_CAST(nsString*,he->key)));
  nsFontFamily* family = (nsFontFamily*) he->value;
  DumpFamily(family);

  return HT_ENUMERATE_NEXT;
}

static void
DumpTree(void)
{
  gFamilies->Enumerate(DumpFamilyEnum, nsnull);
}
#endif /* DEBUG_DUMP_TREE */

struct nsFontSearch
{
  nsFontMetricsGTK* mMetrics;
  PRUint32          mChar;
  nsFontGTK*        mFont;
};

#if 0
static void
GetUnderlineInfo(nsXFont* aFont, unsigned long* aPositionX2,
  unsigned long* aThickness)
{
  /*
   * XLFD 1.5 says underline position defaults descent/2.
   * Hence we return position*2 to avoid rounding error.
   */
  if (aFont->GetXFontProperty(XA_UNDERLINE_POSITION, aPositionX2)) {
    *aPositionX2 *= 2;
  }
  else {
    *aPositionX2 = aFont->max_bounds.descent;
  }

  /*
   * XLFD 1.5 says underline thickness defaults to cap stem width.
   * We don't know what that is, so we just take the thickness of "_".
   * This way, we get thicker underlines for bold fonts.
   */
  if (!xFont->GetXFontProperty(XA_UNDERLINE_THICKNESS, aThickness)) {
    int dir, ascent, descent;
    XCharStruct overall;
    XTextExtents(aFont, "_", 1, &dir, &ascent, &descent, &overall);
    *aThickness = (overall.ascent + overall.descent);
  }
}
#endif /* 0 */

static PRUint16*
GetMapFor10646Font(XFontStruct* aFont)
{
  if (!aFont->per_char)
    return nsnull;

  nsCompressedCharMap ccmapObj;
  PRInt32 minByte1 = aFont->min_byte1;
  PRInt32 maxByte1 = aFont->max_byte1;
  PRInt32 minByte2 = aFont->min_char_or_byte2;
  PRInt32 maxByte2 = aFont->max_char_or_byte2;
  PRInt32 charsPerRow = maxByte2 - minByte2 + 1;
  for (PRInt32 row = minByte1; row <= maxByte1; row++) {
    PRInt32 offset = (((row - minByte1) * charsPerRow) - minByte2);
    for (PRInt32 cell = minByte2; cell <= maxByte2; cell++) {
      XCharStruct* bounds = &aFont->per_char[offset + cell];
      // From Section 8.5 Font Metrics in the Xlib programming manual:
      // A nonexistent character is represented with all members of its XCharStruct set to zero. 
      if (bounds->ascent ||
          bounds->descent ||
          bounds->lbearing ||
          bounds->rbearing ||
          bounds->width ||
          bounds->attributes) {
        ccmapObj.SetChar((row << 8) | cell);
      }
    }
  }
  PRUint16 *ccmap = ccmapObj.NewCCMap();
  return ccmap;
}

PRBool
nsFontGTK::IsEmptyFont(XFontStruct* xFont)
{

  //
  // scan and see if we can find at least one glyph
  //
  if (xFont->per_char) {
    PRInt32 minByte1 = xFont->min_byte1;
    PRInt32 maxByte1 = xFont->max_byte1;
    PRInt32 minByte2 = xFont->min_char_or_byte2;
    PRInt32 maxByte2 = xFont->max_char_or_byte2;
    PRInt32 charsPerRow = maxByte2 - minByte2 + 1;
    for (PRInt32 row = minByte1; row <= maxByte1; row++) {
      PRInt32 offset = (((row - minByte1) * charsPerRow) - minByte2);
      for (PRInt32 cell = minByte2; cell <= maxByte2; cell++) {
        XCharStruct* bounds = &xFont->per_char[offset + cell];
        if (bounds->ascent || bounds->descent) {
          return PR_FALSE;
        }
      }
    }
  }

  return PR_TRUE;
}

void
nsFontGTK::LoadFont(void)
{
  if (mAlreadyCalledLoadFont) {
    return;
  }

  mAlreadyCalledLoadFont = PR_TRUE;
  GdkFont* gdkFont;
  NS_ASSERTION(!mFont, "mFont should not be loaded");
  if (mAABaseSize==0) {
    NS_ASSERTION(!mFontHolder, "mFontHolder should not be loaded");
    gdk_error_trap_push();
    gdkFont = ::gdk_font_load(mName);
    gdk_error_trap_pop();
    if (!gdkFont)
      return;
    mXFont = new nsXFontNormal(gdkFont);
  }
  else {
    NS_ASSERTION(mFontHolder, "mFontHolder should be loaded");
    gdkFont = mFontHolder;
    mXFont = new nsXFontAAScaledBitmap(GDK_DISPLAY(),
                                       DefaultScreen(GDK_DISPLAY()),
                                       gdkFont, mSize, mAABaseSize);
  }

  NS_ASSERTION(mXFont,"failed to load mXFont");
  if (!mXFont)
    return;
  if (!mXFont->LoadFont()) {
    delete mXFont;
    mXFont = nsnull;
    return;
  }

  if (gdkFont) {
    XFontStruct* xFont = mXFont->GetXFontStruct();
    XFontStruct* xFont_with_per_char;
    if (mAABaseSize==0)
      xFont_with_per_char = xFont;
    else
      xFont_with_per_char = (XFontStruct *)GDK_FONT_XFONT(mFontHolder);

    mMaxAscent = xFont->ascent;
    mMaxDescent = xFont->descent;

    if (mCharSetInfo == &ISO106461) {
      mCCMap = GetMapFor10646Font(xFont_with_per_char);
      if (!mCCMap) {
        mXFont->UnloadFont();
        mXFont = nsnull;
        ::gdk_font_unref(gdkFont);
        mFontHolder = nsnull;
        return;
      }
    }

//
// since we are very close to a release point
// limit the risk of this fix 
// please remove soon
//
// Redhat 6.2 Japanese has invalid jisx201 fonts
// Solaris 2.6 has invalid cns11643 fonts for planes 4-7
if ((mCharSetInfo == &JISX0201)
    || (mCharSetInfo == &CNS116434)
    || (mCharSetInfo == &CNS116435)
    || (mCharSetInfo == &CNS116436)
    || (mCharSetInfo == &CNS116437)
   ) {

    if (IsEmptyFont(xFont_with_per_char)) {
#ifdef NS_FONT_DEBUG_LOAD_FONT
      if (gFontDebug & NS_FONT_DEBUG_LOAD_FONT) {
        printf("\n");
        printf("***************************************\n");
        printf("invalid font \"%s\", %s %d\n", mName, __FILE__, __LINE__);
        printf("***************************************\n");
        printf("\n");
      }
#endif
      mXFont->UnloadFont();
      mXFont = nsnull;
      ::gdk_font_unref(gdkFont);
      mFontHolder = nsnull;
      return;
    }
}
    mFont = gdkFont;

#ifdef NS_FONT_DEBUG_LOAD_FONT
    if (gFontDebug & NS_FONT_DEBUG_LOAD_FONT) {
      printf("loaded %s\n", mName);
    }
#endif

  }

#ifdef NS_FONT_DEBUG_LOAD_FONT
  else if (gFontDebug & NS_FONT_DEBUG_LOAD_FONT) {
    printf("cannot load %s\n", mName);
  }
#endif

}

GdkFont* 
nsFontGTK::GetGDKFont(void)
{
  return mFont;
}

nsXFont*
nsFontGTK::GetXFont(void)
{
  return mXFont;
}

PRBool
nsFontGTK::GetXFontIs10646(void)
{
  return ((PRBool) (mCharSetInfo == &ISO106461));
}

PRBool
nsFontGTK::IsFreeTypeFont(void)
{
  return PR_FALSE;
}

MOZ_DECL_CTOR_COUNTER(nsFontGTK)

nsFontGTK::nsFontGTK()
{
  MOZ_COUNT_CTOR(nsFontGTK);
}

nsFontGTK::~nsFontGTK()
{
  MOZ_COUNT_DTOR(nsFontGTK);
  if (mXFont) {
    delete mXFont;
  }
  if (mFont && (mAABaseSize==0)) {
    gdk_font_unref(mFont);
  }
  if (mCharSetInfo == &ISO106461) {
    FreeCCMap(mCCMap);
  }
  if (mName) {
    PR_smprintf_free(mName);
  }
}

class nsFontGTKNormal : public nsFontGTK
{
public:
  nsFontGTKNormal();
  nsFontGTKNormal(nsFontGTK*);
  virtual ~nsFontGTKNormal();

  virtual gint GetWidth(const PRUnichar* aString, PRUint32 aLength);
  virtual gint DrawString(nsRenderingContextGTK* aContext,
                          nsDrawingSurfaceGTK* aSurface, nscoord aX,
                          nscoord aY, const PRUnichar* aString,
                          PRUint32 aLength);
#ifdef MOZ_MATHML
  virtual nsresult GetBoundingMetrics(const PRUnichar*   aString,
                                      PRUint32           aLength,
                                      nsBoundingMetrics& aBoundingMetrics);
#endif
};

nsFontGTKNormal::nsFontGTKNormal()
{
  mFontHolder = nsnull;
}

nsFontGTKNormal::nsFontGTKNormal(nsFontGTK *aFont)
{
  mAABaseSize = aFont->mSize;
  mFontHolder = aFont->GetGDKFont();
  if (!mFontHolder) {
    aFont->LoadFont();
    mFontHolder = aFont->GetGDKFont();
  }
  NS_ASSERTION(mFontHolder, "font to copy not loaded");
  if (mFontHolder)
    ::gdk_font_ref(mFontHolder);
}

nsFontGTKNormal::~nsFontGTKNormal()
{
  if (mFontHolder)
    ::gdk_font_unref(mFontHolder);
}

gint
nsFontGTKNormal::GetWidth(const PRUnichar* aString, PRUint32 aLength)
{
  if (!mFont) {
    LoadFont();
    if (!mFont) {
      return 0;
    }
  }

  XChar2b buf[512];
  char* p;
  PRInt32 bufLen;
  ENCODER_BUFFER_ALLOC_IF_NEEDED(p, mCharSetInfo->mConverter,
                         aString, aLength, buf, sizeof(buf), bufLen);
  gint len = mCharSetInfo->Convert(mCharSetInfo, mXFont->GetXFontStruct(),
                                   aString, aLength, p, bufLen);
  gint outWidth;
  if (mXFont->IsSingleByte())
    outWidth = mXFont->TextWidth8(p, len);
  else
    outWidth = mXFont->TextWidth16((const XChar2b*)p, len/2);
  ENCODER_BUFFER_FREE_IF_NEEDED(p, buf);
  return outWidth;
}

gint
nsFontGTKNormal::DrawString(nsRenderingContextGTK* aContext,
                            nsDrawingSurfaceGTK* aSurface,
                            nscoord aX, nscoord aY,
                            const PRUnichar* aString, PRUint32 aLength)
{
  if (!mFont) {
    LoadFont();
    if (!mFont) {
      return 0;
    }
  }

  XChar2b buf[512];
  char* p;
  PRInt32 bufLen;
  ENCODER_BUFFER_ALLOC_IF_NEEDED(p, mCharSetInfo->mConverter,
                         aString, aLength, buf, sizeof(buf), bufLen);
  gint len = mCharSetInfo->Convert(mCharSetInfo, mXFont->GetXFontStruct(),
                                   aString, aLength, p, bufLen);
  GdkGC *gc = aContext->GetGC();
  gint outWidth;
  if (mXFont->IsSingleByte()) {
    mXFont->DrawText8(aSurface->GetDrawable(), gc, aX,
                                          aY + mBaselineAdjust, p, len);
    outWidth = mXFont->TextWidth8(p, len);
  }
  else {
    mXFont->DrawText16(aSurface->GetDrawable(), gc, aX, aY + mBaselineAdjust,
                       (const XChar2b*)p, len/2);
    outWidth = mXFont->TextWidth16((const XChar2b*)p, len/2);
  }
  gdk_gc_unref(gc);
  ENCODER_BUFFER_FREE_IF_NEEDED(p, buf);
  return outWidth;
}

#ifdef MOZ_MATHML
// bounding metrics for a string 
// remember returned values are not in app units
nsresult
nsFontGTKNormal::GetBoundingMetrics (const PRUnichar*   aString,
                                     PRUint32           aLength,
                                     nsBoundingMetrics& aBoundingMetrics)                                 
{
  aBoundingMetrics.Clear();               

  if (!mFont) {
    LoadFont();
    if (!mFont) {
      return NS_ERROR_FAILURE;
    }
  }

  if (aString && 0 < aLength) {
    XFontStruct *fontInfo = mXFont->GetXFontStruct();
    XChar2b buf[512];
    char* p;
    PRInt32 bufLen;
    ENCODER_BUFFER_ALLOC_IF_NEEDED(p, mCharSetInfo->mConverter,
                         aString, aLength, buf, sizeof(buf), bufLen);
    gint len = mCharSetInfo->Convert(mCharSetInfo, fontInfo, aString, aLength,
                                     p, bufLen);
    if (mXFont->IsSingleByte()) {
      mXFont->TextExtents8(p, len,
                           &aBoundingMetrics.leftBearing,
                           &aBoundingMetrics.rightBearing,
                           &aBoundingMetrics.width,
                           &aBoundingMetrics.ascent,
                           &aBoundingMetrics.descent);
    }
    else {
      mXFont->TextExtents16((const XChar2b*)p, len,
                           &aBoundingMetrics.leftBearing,
                           &aBoundingMetrics.rightBearing,
                           &aBoundingMetrics.width,
                           &aBoundingMetrics.ascent,
                           &aBoundingMetrics.descent);
    }
    ENCODER_BUFFER_FREE_IF_NEEDED(p, buf);
  }

  return NS_OK;
}
#endif

class nsFontGTKSubstitute : public nsFontGTK
{
public:
  nsFontGTKSubstitute(nsFontGTK* aFont);
  virtual ~nsFontGTKSubstitute();

  virtual GdkFont* GetGDKFont(void);
  virtual nsXFont* GetXFont(void);
  virtual PRBool   GetXFontIs10646(void);
  virtual gint GetWidth(const PRUnichar* aString, PRUint32 aLength);
  virtual gint DrawString(nsRenderingContextGTK* aContext,
                          nsDrawingSurfaceGTK* aSurface, nscoord aX,
                          nscoord aY, const PRUnichar* aString,
                          PRUint32 aLength);
#ifdef MOZ_MATHML
  virtual nsresult GetBoundingMetrics(const PRUnichar*   aString,
                                      PRUint32           aLength,
                                      nsBoundingMetrics& aBoundingMetrics);
#endif
  virtual PRUint32 Convert(const PRUnichar* aSrc, PRUint32 aSrcLen,
                           PRUnichar* aDest, PRUint32 aDestLen);

  nsFontGTK* mSubstituteFont;
};

nsFontGTKSubstitute::nsFontGTKSubstitute(nsFontGTK* aFont)
{
  mSubstituteFont = aFont;
}

nsFontGTKSubstitute::~nsFontGTKSubstitute()
{
  // Do not free mSubstituteFont here. It is owned by somebody else.
}

PRUint32
nsFontGTKSubstitute::Convert(const PRUnichar* aSrc, PRUint32 aSrcLen,
  PRUnichar* aDest, PRUint32 aDestLen)
{
  nsresult res;
  if (!gFontSubConverter) {
    nsComponentManager::CreateInstance(kSaveAsCharsetCID, nsnull,
      NS_GET_IID(nsISaveAsCharset), (void**) &gFontSubConverter);
    if (gFontSubConverter) {
      res = gFontSubConverter->Init("ISO-8859-1",
                             nsISaveAsCharset::attr_FallbackQuestionMark +
                               nsISaveAsCharset::attr_EntityAfterCharsetConv +
                               nsISaveAsCharset::attr_IgnoreIgnorables, 
                             nsIEntityConverter::transliterate);
      if (NS_FAILED(res)) {
        NS_RELEASE(gFontSubConverter);
      }
    }
  }

  if (gFontSubConverter) {
    nsAutoString tmp(aSrc, aSrcLen);
    char* conv = nsnull;
    res = gFontSubConverter->Convert(tmp.get(), &conv);
    if (NS_SUCCEEDED(res) && conv) {
      char* p = conv;
      PRUint32 i;
      for (i = 0; i < aDestLen; i++) {
        if (*p) {
          aDest[i] = *p;
        }
        else {
          break;
        }
        p++;
      }
      nsMemory::Free(conv);
      conv = nsnull;
      return i;
    }
  }

  if (aSrcLen > aDestLen) {
    aSrcLen = aDestLen;
  }
  for (PRUint32 i = 0; i < aSrcLen; i++) {
    aDest[i] = '?';
  }

  return aSrcLen;
}

gint
nsFontGTKSubstitute::GetWidth(const PRUnichar* aString, PRUint32 aLength)
{
  PRUnichar buf[512];
  PRUnichar *p = buf;
  PRUint32 bufLen = sizeof(buf)/sizeof(PRUnichar);
  if ((aLength*2) > bufLen) {
    PRUnichar *tmp;
    tmp = (PRUnichar*)nsMemory::Alloc(sizeof(PRUnichar) * (aLength*2));
    if (tmp) {
      p = tmp;
      bufLen = (aLength*2);
    }
  }
  PRUint32 len = Convert(aString, aLength, p, bufLen);
  gint outWidth = mSubstituteFont->GetWidth(p, len);
  if (p != buf)
    nsMemory::Free(p);
  return outWidth;

}

gint
nsFontGTKSubstitute::DrawString(nsRenderingContextGTK* aContext,
                                nsDrawingSurfaceGTK* aSurface,
                                nscoord aX, nscoord aY,
                                const PRUnichar* aString, PRUint32 aLength)
{
  PRUnichar buf[512];
  PRUnichar *p = buf;
  PRUint32 bufLen = sizeof(buf)/sizeof(PRUnichar);
  if ((aLength*2) > bufLen) {
    PRUnichar *tmp;
    tmp = (PRUnichar*)nsMemory::Alloc(sizeof(PRUnichar) * (aLength*2));
    if (tmp) {
      p = tmp;
      bufLen = (aLength*2);
    }
  }
  PRUint32 len = Convert(aString, aLength, p, bufLen);
  gint outWidth = mSubstituteFont->DrawString(aContext, aSurface, 
                                        aX, aY, p, len);
  if (p != buf)
    nsMemory::Free(p);
  return outWidth;
}

#ifdef MOZ_MATHML
// bounding metrics for a string 
// remember returned values are not in app units
nsresult
nsFontGTKSubstitute::GetBoundingMetrics(const PRUnichar*   aString,
                                        PRUint32           aLength,
                                        nsBoundingMetrics& aBoundingMetrics)                                 
{
  PRUnichar buf[512];
  PRUnichar *p = buf;
  PRUint32 bufLen = sizeof(buf)/sizeof(PRUnichar);
  if ((aLength*2) > bufLen) {
    PRUnichar *tmp;
    tmp = (PRUnichar*)nsMemory::Alloc(sizeof(PRUnichar) * (aLength*2));
    if (tmp) {
      p = tmp;
      bufLen = (aLength*2);
    }
  }
  PRUint32 len = Convert(aString, aLength, p, bufLen);
  nsresult res = mSubstituteFont->GetBoundingMetrics(p, len, 
                                    aBoundingMetrics);
  if (p != buf)
    nsMemory::Free(p);
  return res;
}
#endif

GdkFont* 
nsFontGTKSubstitute::GetGDKFont(void)
{
  return mSubstituteFont->GetGDKFont();
}

nsXFont*
nsFontGTKSubstitute::GetXFont(void)
{
  return mSubstituteFont->GetXFont();
}

PRBool
nsFontGTKSubstitute::GetXFontIs10646(void)
{
  return mSubstituteFont->GetXFontIs10646();
}

class nsFontGTKUserDefined : public nsFontGTK
{
public:
  nsFontGTKUserDefined();
  virtual ~nsFontGTKUserDefined();

  virtual PRBool Init(nsFontGTK* aFont);
  virtual gint GetWidth(const PRUnichar* aString, PRUint32 aLength);
  virtual gint DrawString(nsRenderingContextGTK* aContext,
                          nsDrawingSurfaceGTK* aSurface, nscoord aX,
                          nscoord aY, const PRUnichar* aString,
                          PRUint32 aLength);
#ifdef MOZ_MATHML
  virtual nsresult GetBoundingMetrics(const PRUnichar*   aString,
                                      PRUint32           aLength,
                                      nsBoundingMetrics& aBoundingMetrics);
#endif
  virtual PRUint32 Convert(const PRUnichar* aSrc, PRInt32 aSrcLen,
                           char* aDest, PRInt32 aDestLen);
};

nsFontGTKUserDefined::nsFontGTKUserDefined()
{
}

nsFontGTKUserDefined::~nsFontGTKUserDefined()
{
  // Do not free mFont here. It is owned by somebody else.
}

PRBool
nsFontGTKUserDefined::Init(nsFontGTK* aFont)
{
  if (!aFont->GetXFont()) {
    aFont->LoadFont();
    if (!aFont->GetXFont()) {
      mCCMap = gEmptyCCMap;
      return PR_FALSE;
    }
  }
  mXFont = aFont->GetXFont();
  mCCMap = gUserDefinedCCMap;
  mName = aFont->mName;

  return PR_TRUE;
}

PRUint32
nsFontGTKUserDefined::Convert(const PRUnichar* aSrc, PRInt32 aSrcLen,
  char* aDest, PRInt32 aDestLen)
{
  if (aSrcLen > aDestLen) {
    aSrcLen = aDestLen;
  }
  gUserDefinedConverter->Convert(aSrc, &aSrcLen, aDest, &aDestLen);

  return aSrcLen;
}

gint
nsFontGTKUserDefined::GetWidth(const PRUnichar* aString, PRUint32 aLength)
{
  char buf[1024];
  char* p;
  PRInt32 bufLen;
  ENCODER_BUFFER_ALLOC_IF_NEEDED(p, gUserDefinedConverter,
                         aString, aLength, buf, sizeof(buf), bufLen);
  PRUint32 len = Convert(aString, aLength, p, bufLen);

  gint outWidth;
  if (mXFont->IsSingleByte())
    outWidth = mXFont->TextWidth8(p, len);
  else
    outWidth = mXFont->TextWidth16((const XChar2b*)p, len/2);
  ENCODER_BUFFER_FREE_IF_NEEDED(p, buf);
  return outWidth;
}

gint
nsFontGTKUserDefined::DrawString(nsRenderingContextGTK* aContext,
                                nsDrawingSurfaceGTK* aSurface,
                                nscoord aX, nscoord aY,
                                const PRUnichar* aString, PRUint32 aLength)
{
  char buf[1024];
  char* p;
  PRInt32 bufLen;
  ENCODER_BUFFER_ALLOC_IF_NEEDED(p, gUserDefinedConverter,
                         aString, aLength, buf, sizeof(buf), bufLen);
  PRUint32 len = Convert(aString, aLength, p, bufLen);
  GdkGC *gc = aContext->GetGC();

  gint outWidth;
  if (mXFont->IsSingleByte()) {
    mXFont->DrawText8(aSurface->GetDrawable(), gc, aX,
                                          aY + mBaselineAdjust, p, len);
    outWidth = mXFont->TextWidth8(p, len);
  }
  else {
    mXFont->DrawText16(aSurface->GetDrawable(), gc, aX, aY + mBaselineAdjust,
                       (const XChar2b*)p, len);
    outWidth = mXFont->TextWidth16((const XChar2b*)p, len/2);
  }
  gdk_gc_unref(gc);
  ENCODER_BUFFER_FREE_IF_NEEDED(p, buf);
  return outWidth;
}

#ifdef MOZ_MATHML
// bounding metrics for a string 
// remember returned values are not in app units
nsresult
nsFontGTKUserDefined::GetBoundingMetrics(const PRUnichar*   aString,
                                        PRUint32           aLength,
                                        nsBoundingMetrics& aBoundingMetrics)                                 
{
  aBoundingMetrics.Clear();               

  if (aString && 0 < aLength) {
    char buf[1024];
    char* p;
    PRInt32 bufLen;
    ENCODER_BUFFER_ALLOC_IF_NEEDED(p, gUserDefinedConverter,
                         aString, aLength, buf, sizeof(buf), bufLen);
    PRUint32 len = Convert(aString, aLength, p, bufLen);
    if (mXFont->IsSingleByte()) {
      mXFont->TextExtents8(p, len,
                           &aBoundingMetrics.leftBearing,
                           &aBoundingMetrics.rightBearing,
                           &aBoundingMetrics.width,
                           &aBoundingMetrics.ascent,
                           &aBoundingMetrics.descent);
    }
    else {
      mXFont->TextExtents16((const XChar2b*)p, len,
                           &aBoundingMetrics.leftBearing,
                           &aBoundingMetrics.rightBearing,
                           &aBoundingMetrics.width,
                           &aBoundingMetrics.ascent,
                           &aBoundingMetrics.descent);
    }
    ENCODER_BUFFER_FREE_IF_NEEDED(p, buf);
  }

  return NS_OK;
}
#endif

nsFontGTK*
nsFontMetricsGTK::AddToLoadedFontsList(nsFontGTK* aFont)
{
  if (mLoadedFontsCount == mLoadedFontsAlloc) {
    int newSize;
    if (mLoadedFontsAlloc) {
      newSize = (2 * mLoadedFontsAlloc);
    }
    else {
      newSize = 1;
    }
    nsFontGTK** newPointer = (nsFontGTK**) 
      PR_Realloc(mLoadedFonts, newSize * sizeof(nsFontGTK*));
    if (newPointer) {
      mLoadedFonts = newPointer;
      mLoadedFontsAlloc = newSize;
    }
    else {
      return nsnull;
    }
  }
  mLoadedFonts[mLoadedFontsCount++] = aFont;
  return aFont;
}

// define a size such that a scaled font would always be closer
// to the desired size than this
#define NOT_FOUND_FONT_SIZE 1000*1000*1000

nsFontGTK*
nsFontMetricsGTK::FindNearestSize(nsFontStretch* aStretch, PRUint16 aSize)
{
  nsFontGTK* font = nsnull;
  if (aStretch->mSizes) {
    nsFontGTK** begin = aStretch->mSizes;
    nsFontGTK** end = &aStretch->mSizes[aStretch->mSizesCount];
    nsFontGTK** s;
    // scan the list of sizes
    for (s = begin; s < end; s++) {
      // stop when we hit or overshoot the size
      if ((*s)->mSize >= aSize) {
        break;
      }
    }
    // backup if we hit the end of the list
    if (s == end) {
      s--;
    }
    else if (s != begin) {
      // if we overshot pick the closest size
      if (((*s)->mSize - aSize) >= (aSize - (*(s - 1))->mSize)) {
        s--;
      }
    }
    // this is the nearest bitmap font
    font = *s;
  }
  return font;
}

static PRBool
SetFontCharsetInfo(nsFontGTK *aFont, nsFontCharSetInfo* aCharSet,
                   PRUint32 aChar)
{
  if (aCharSet->mCharSet) {
    aFont->mCCMap = aCharSet->mCCMap;
    // check that the font is not empty
    if (CCMAP_HAS_CHAR_EXT(aFont->mCCMap, aChar)) {
      aFont->LoadFont();
      if (!aFont->GetXFont()) {
        return PR_FALSE;
      }
    }
  }
  else {
    if (aCharSet == &ISO106461) {
      aFont->LoadFont();
      if (!aFont->GetXFont()) {
        return PR_FALSE;
      }
    }
  }
  return PR_TRUE;
}

static nsFontGTK*
SetupUserDefinedFont(nsFontGTK *aFont)
{
  if (!aFont->mUserDefinedFont) {
    aFont->mUserDefinedFont = new nsFontGTKUserDefined();
    if (!aFont->mUserDefinedFont) {
      return nsnull;
    }
    if (!aFont->mUserDefinedFont->Init(aFont)) {
      return nsnull;
    }
  }
  return aFont->mUserDefinedFont;
}


nsFontGTK*
nsFontMetricsGTK::GetAASBBaseFont(nsFontStretch* aStretch, 
                              nsFontCharSetInfo* aCharSet)
{
  nsFontGTK* base_aafont;
  PRInt32 scale_size;
  PRUint32 aa_target_size;

  scale_size = PR_MAX(mPixelSize, aCharSet->mAABitmapScaleMin);
  aa_target_size = MAX((scale_size*2), 16);
  base_aafont = FindNearestSize(aStretch, aa_target_size);
  NS_ASSERTION(base_aafont,
             "failed to find a base font for Anti-Aliased bitmap Scaling");
  return base_aafont;
}

nsFontGTK*
nsFontMetricsGTK::PickASizeAndLoad(nsFontStretch* aStretch,
  nsFontCharSetInfo* aCharSet, PRUint32 aChar, const char *aName)
{
  if (aStretch->mFreeTypeFaceID) {
    //FREETYPE_FONT_PRINTF(("mFreeTypeFaceID = 0x%p", aStretch->mFreeTypeFaceID));
    nsFreeTypeFont *ftfont = nsFreeTypeFont::NewFont(aStretch->mFreeTypeFaceID,
                                                     mPixelSize,
                                                     aName);
    if (!ftfont) {
      FREETYPE_FONT_PRINTF(("failed to create font"));
      return nsnull;
    }
    //FREETYPE_FONT_PRINTF(("created ftfont"));
    /*
     * XXX Instead of passing pixel size, we ought to take underline
     * into account. (Extra space for underline for Asian fonts.)
     */
    ftfont->mName = PR_smprintf("%s", aName);
    if (!ftfont->mName) {
      FREETYPE_FONT_PRINTF(("failed to create mName"));
      delete ftfont;
      return nsnull;
    }
    SetCharsetLangGroup(aCharSet);
    ftfont->mSize = mPixelSize;
    ftfont->LoadFont();
    ftfont->mCharSetInfo = &ISO106461;
    //FREETYPE_FONT_PRINTF(("add the ftfont"));
    return AddToLoadedFontsList(ftfont);
  }

  if (IS_SURROGATE(aChar)) {
    // SURROGATE is only supported by FreeType
    return nsnull;
  }

  PRBool use_scaled_font = PR_FALSE;
  PRBool have_nearly_rightsized_bitmap = PR_FALSE;
  nsFontGTK* base_aafont = nsnull;

  PRInt32 bitmap_size = NOT_FOUND_FONT_SIZE;
  PRInt32 scale_size = mPixelSize;
  nsFontGTK* font = FindNearestSize(aStretch, mPixelSize);
  if (font) {
    bitmap_size = font->mSize;
    if (   (bitmap_size >= mPixelSize-(mPixelSize/10))
        && (bitmap_size <= mPixelSize+(mPixelSize/10)))
      // When the size of a hand tuned font is close to the desired size
      // favor it over outline scaled font
      have_nearly_rightsized_bitmap = PR_TRUE;
  }

  //
  // If the user says always try to aasb (anti alias scaled bitmap) scale
  //
  if (gAABitmapScaleEnabled && aCharSet->mAABitmapScaleAlways) {
    base_aafont = GetAASBBaseFont(aStretch, aCharSet);
    if (base_aafont) {
      use_scaled_font = PR_TRUE;
      SIZE_FONT_PRINTF(("anti-aliased bitmap scaled font: %s\n"
            "                    desired=%d, aa-scaled=%d, bitmap=%d, "
            "aa_bitmap=%d",
            aName, mPixelSize, scale_size, bitmap_size, base_aafont->mSize));
    }
  }

  //
  // if not already aasb scaling and
  // if we do not have a bitmap that is nearly the correct size 
  //
  if (!use_scaled_font && !have_nearly_rightsized_bitmap) {
    // check if we can use an outline scaled font
    if (aStretch->mOutlineScaled) {
      scale_size = PR_MAX(mPixelSize, aCharSet->mOutlineScaleMin);

      if (PR_ABS(mPixelSize-scale_size) < PR_ABS(mPixelSize-bitmap_size)) {
        use_scaled_font = 1;
        SIZE_FONT_PRINTF(("outline font:______ %s\n"
                  "                    desired=%d, scaled=%d, bitmap=%d", 
                  aStretch->mScalable, mPixelSize, scale_size,
                  (bitmap_size=NOT_FOUND_FONT_SIZE?0:bitmap_size)));
      }
    }
    // see if we can aasb (anti alias scaled bitmap)
    if (!use_scaled_font 
        && (bitmap_size<NOT_FOUND_FONT_SIZE) && gAABitmapScaleEnabled) {
      // if we do not have a near-the-right-size font or scalable font
      // see if we can anti-alias bitmap scale one
      scale_size = PR_MAX(mPixelSize, aCharSet->mAABitmapScaleMin);
      double ratio = (bitmap_size / ((double) mPixelSize));
      if (   (ratio < aCharSet->mAABitmapUndersize)
          || (ratio > aCharSet->mAABitmapOversize)) {
        //
        // Try to get a size font to scale that is 2x larger 
        // (but at least 16 pixel)
        //
        base_aafont = GetAASBBaseFont(aStretch, aCharSet);
        if (base_aafont) {
          use_scaled_font = PR_TRUE;
          SIZE_FONT_PRINTF(("anti-aliased bitmap scaled font: %s\n"
              "                    desired=%d, aa-scaled=%d, bitmap=%d, "
              "aa_bitmap=%d",
              aName, mPixelSize, scale_size, bitmap_size, base_aafont->mSize));
        }
      }
    }
    // last resort: consider a bitmap scaled font (ugly!)
    if (!use_scaled_font && aStretch->mScalable) {
      scale_size = PR_MAX(mPixelSize, aCharSet->mBitmapScaleMin);
      double ratio = (bitmap_size / ((double) mPixelSize));
      if ((ratio < aCharSet->mBitmapUndersize)
        || (ratio > aCharSet->mBitmapOversize)) {
        if ((PR_ABS(mPixelSize-scale_size) < PR_ABS(mPixelSize-bitmap_size))) {
          use_scaled_font = 1;
          SIZE_FONT_PRINTF(("bitmap scaled font: %s\n"
                "                    desired=%d, scaled=%d, bitmap=%d", 
                aStretch->mScalable, mPixelSize, scale_size,
                (bitmap_size=NOT_FOUND_FONT_SIZE?0:bitmap_size)));
        }
      }
    }
  }

  NS_ASSERTION((bitmap_size<NOT_FOUND_FONT_SIZE)||use_scaled_font,
                "did not find font size");
  if (!use_scaled_font) {
    SIZE_FONT_PRINTF(("bitmap font:_______ %s\n" 
                      "                    desired=%d, scaled=%d, bitmap=%d", 
                      aName, mPixelSize, scale_size, bitmap_size));
  }

  if (use_scaled_font) {
   SIZE_FONT_PRINTF(("scaled font:_______ %s\n"
                     "                    desired=%d, scaled=%d, bitmap=%d",
                     aName, mPixelSize, scale_size, bitmap_size));

    PRInt32 i;
    PRInt32 n = aStretch->mScaledFonts.Count();
    nsFontGTK* p = nsnull;
    for (i = 0; i < n; i++) {
      p = (nsFontGTK*) aStretch->mScaledFonts.ElementAt(i);
      if (p->mSize == scale_size) {
        break;
      }
    }
    if (i == n) {
      if (base_aafont) {
        // setup the base font
        if (!SetFontCharsetInfo(base_aafont, aCharSet, aChar))
          return nsnull;
        if (mIsUserDefined) {
          base_aafont = SetupUserDefinedFont(base_aafont);
          if (!base_aafont)
            return nsnull;
        }
        font = new nsFontGTKNormal(base_aafont);
      }
      else
        font = new nsFontGTKNormal;
      if (font) {
        /*
         * XXX Instead of passing pixel size, we ought to take underline
         * into account. (Extra space for underline for Asian fonts.)
         */
        if (base_aafont) {
          font->mName = PR_smprintf("%s", base_aafont->mName);
          font->mAABaseSize = base_aafont->mSize;
        }
        else {
          font->mName = PR_smprintf(aStretch->mScalable, scale_size);
          font->mAABaseSize = 0;
        }
        if (!font->mName) {
          delete font;
          return nsnull;
        }
        font->mSize = scale_size;
        font->mCharSetInfo = aCharSet;
        aStretch->mScaledFonts.AppendElement(font);
      }
      else {
        return nsnull;
      }
    }
    else {
      font = p;
    }
  }

  if (!SetFontCharsetInfo(font, aCharSet, aChar))
    return nsnull;

  if (mIsUserDefined) {
    font = SetupUserDefinedFont(font);
    if (!font)
      return nsnull;
  }

  return AddToLoadedFontsList(font);
}

nsresult
nsFontMetricsGTK::GetWidth  (const char* aString, PRUint32 aLength,
                             nscoord& aWidth,
                             nsRenderingContextGTK *aContext)
{
    if (aLength == 0) {
        aWidth = 0;
        return NS_OK;
    }

    nsXFont *xFont = mCurrentFont->GetXFont();
    gint rawWidth;
    
    if (mCurrentFont->IsFreeTypeFont()) {
        PRUnichar unichars[WIDEN_8_TO_16_BUF_SIZE];

        // need to fix this for long strings
        PRUint32 len = PR_MIN(aLength, WIDEN_8_TO_16_BUF_SIZE);

        // convert 7 bit data to unicode
        // this function is only supposed to be called for ascii data
        for (PRUint32 i=0; i<len; i++) {
            unichars[i] = (PRUnichar)((unsigned char)aString[i]);
        }

        rawWidth = mCurrentFont->GetWidth(unichars, len);
    }
    else if (!mCurrentFont->GetXFontIs10646()) {
        NS_ASSERTION(xFont->IsSingleByte(),"wrong string/font size");
        // 8 bit data with an 8 bit font
        rawWidth = xFont->TextWidth8(aString, aLength);
    }
    else {
        NS_ASSERTION(!xFont->IsSingleByte(),"wrong string/font size");
        // we have 8 bit data but a 16 bit font
        rawWidth = Widen8To16AndGetWidth (mCurrentFont->GetXFont(),
                                          aString, aLength);
    }

    float f;
    f = mDeviceContext->DevUnitsToAppUnits();
    aWidth = NSToCoordRound(rawWidth * f);

    return NS_OK;
}

nsresult
nsFontMetricsGTK::GetWidth  (const PRUnichar* aString, PRUint32 aLength,
                             nscoord& aWidth, PRInt32* aFontID,
                             nsRenderingContextGTK *aContext)
{
    if (aLength == 0) {
        aWidth = 0;
        return NS_OK;
    }

    nsFontGTK* prevFont = nsnull;
    gint rawWidth = 0;
    PRUint32 start = 0;
    PRUint32 i;

    PRUint32 extraSurrogateLength;
    for (i = 0; i < aLength; i+=1+extraSurrogateLength) {
        PRUint32 c = aString[i];
        extraSurrogateLength=0;

        if(i < aLength-1 && IS_HIGH_SURROGATE(c) && IS_LOW_SURROGATE(aString[i+1])) {
          // if surrogate, make UCS4 code point from high aString[i] and
          // low surrogate aString[i+1]
          c = SURROGATE_TO_UCS4(c, aString[i+1]);

          // skip aString[i+1], it is already used as low surrogate
          extraSurrogateLength = 1;
        }
        nsFontGTK* currFont = nsnull;
        nsFontGTK** font = mLoadedFonts;
        nsFontGTK** end = &mLoadedFonts[mLoadedFontsCount];
        while (font < end) {
            if (CCMAP_HAS_CHAR_EXT((*font)->mCCMap, c)) {
                currFont = *font;
                goto FoundFont; // for speed -- avoid "if" statement
            }
            font++;
        }
        currFont = FindFont(c);
    FoundFont:
        // XXX avoid this test by duplicating code -- erik
        if (prevFont) {
            if (currFont != prevFont) {
                rawWidth += prevFont->GetWidth(&aString[start], i - start);
                prevFont = currFont;
                start = i;
            }
        }
        else {
            prevFont = currFont;
            start = i;
        }
    }

    if (prevFont) {
        rawWidth += prevFont->GetWidth(&aString[start], i - start);
    }

    float f;
    f = mDeviceContext->DevUnitsToAppUnits();
    aWidth = NSToCoordRound(rawWidth * f);

    if (nsnull != aFontID)
        *aFontID = 0;

    return NS_OK;
}

nsresult
nsFontMetricsGTK::DrawString(const char *aString, PRUint32 aLength,
                             nscoord aX, nscoord aY,
                             const nscoord* aSpacing,
                             nsRenderingContextGTK *aContext,
                             nsDrawingSurfaceGTK *aSurface)
{
  if (!aLength)
      return NS_ERROR_FAILURE;

  nsresult rv = NS_OK;

  g_return_val_if_fail(aString != NULL, NS_ERROR_FAILURE);
  g_return_val_if_fail(mCurrentFont != NULL, NS_ERROR_FAILURE);

  nscoord x = aX;
  nscoord y = aY;

  aContext->UpdateGC();

  nsXFont *xFont = mCurrentFont->GetXFont();

  // Get the gc - note that we have to unref this later
  GdkGC *gc = aContext->GetGC();

  if (nsnull != aSpacing) {
      // Render the string, one character at a time...
      const char* end = aString + aLength;

      while (aString < end) {
          char ch = *aString++;
          nscoord xx = x;
          nscoord yy = y;
          aContext->GetTranMatrix()->TransformCoord(&xx, &yy);

          if (mCurrentFont->IsFreeTypeFont()) {
              PRUnichar unichars[WIDEN_8_TO_16_BUF_SIZE];

              // need to fix this for long strings
              PRUint32 len = PR_MIN(aLength, WIDEN_8_TO_16_BUF_SIZE);

              // convert 7 bit data to unicode
              // this function is only supposed to be called for ascii data
              for (PRUint32 i=0; i<len; i++) {
                  unichars[i] = (PRUnichar)((unsigned char)aString[i]);
              }

              rv = mCurrentFont->DrawString(aContext, aSurface, xx, yy,
                                            unichars, len);
          }
          else if (!mCurrentFont->GetXFontIs10646()) {
              // 8 bit data with an 8 bit font
              NS_ASSERTION(xFont->IsSingleByte(),"wrong string/font size");
              xFont->DrawText8(aSurface->GetDrawable(), gc, xx, yy, &ch, 1);
          }
          else {
              // we have 8 bit data but a 16 bit font
              NS_ASSERTION(!xFont->IsSingleByte(),"wrong string/font size");
              Widen8To16AndDraw(aSurface->GetDrawable(), xFont, gc,
                                xx, yy, &ch, 1);
          }

          x += *aSpacing++;
      }
  }
  else {
      aContext->GetTranMatrix()->TransformCoord(&x, &y);

      if (mCurrentFont->IsFreeTypeFont()) {
          PRUnichar unichars[WIDEN_8_TO_16_BUF_SIZE];

          // need to fix this for long strings
          PRUint32 len = PR_MIN(aLength, WIDEN_8_TO_16_BUF_SIZE);

          // convert 7 bit data to unicode
          // this function is only supposed to be called for ascii data
          for (PRUint32 i=0; i<len; i++) {
              unichars[i] = (PRUnichar)((unsigned char)aString[i]);
          }

          rv = mCurrentFont->DrawString(aContext, aSurface, x, y,
                                        unichars, len);
      }
      else if (!mCurrentFont->GetXFontIs10646()) { // keep 8 bit path fast
          // 8 bit data with an 8 bit font
          NS_ASSERTION(xFont->IsSingleByte(),"wrong string/font size");
          xFont->DrawText8(aSurface->GetDrawable(), gc,
                           x, y, aString, aLength);
      }
      else {
          // we have 8 bit data but a 16 bit font
          NS_ASSERTION(!xFont->IsSingleByte(),"wrong string/font size");
          Widen8To16AndDraw(aSurface->GetDrawable(), xFont, gc,
                            x, y, aString, aLength);
      }
  }

  gdk_gc_unref(gc);

  return rv;
}

nsresult
nsFontMetricsGTK::DrawString(const PRUnichar* aString, PRUint32 aLength,
                             nscoord aX, nscoord aY,
                             PRInt32 aFontID,
                             const nscoord* aSpacing,
                             nsRenderingContextGTK *aContext,
                             nsDrawingSurfaceGTK *aSurface)
{
    if (!aLength)
        return NS_ERROR_FAILURE;

    g_return_val_if_fail(aSurface != NULL, NS_ERROR_FAILURE);
    g_return_val_if_fail(aString != NULL, NS_ERROR_FAILURE);

    nscoord x = aX;
    nscoord y = aY;

    aContext->GetTranMatrix()->TransformCoord(&x, &y);

    nsFontGTK* prevFont = nsnull;
    PRUint32 start = 0;
    PRUint32 i;

    PRUint32 extraSurrogateLength;
    for (i = 0; i < aLength; i+=1+extraSurrogateLength) {
        PRUint32 c = aString[i];
        extraSurrogateLength=0;
        if(i < aLength-1 && IS_HIGH_SURROGATE(c) && IS_LOW_SURROGATE(aString[i+1])) {
          // if surrogate, make UCS4 code point from high aString[i] and
          // low surrogate aString[i+1]
          c = SURROGATE_TO_UCS4(c, aString[i+1]);
 
          // skip aString[i+1], it is already used as low surrogate
          extraSurrogateLength = 1;
        }
        nsFontGTK* currFont = nsnull;
        nsFontGTK** font = mLoadedFonts;
        nsFontGTK** lastFont = &mLoadedFonts[mLoadedFontsCount];
        while (font < lastFont) {
            if (CCMAP_HAS_CHAR_EXT((*font)->mCCMap, c)) {
                currFont = *font;
                goto FoundFont; // for speed -- avoid "if" statement
            }
            font++;
        }

        currFont = FindFont(c);

    FoundFont:
        // XXX avoid this test by duplicating code -- erik
        if (prevFont) {
            if (currFont != prevFont) {
                if (aSpacing) {
                    const PRUnichar* str = &aString[start];
                    const PRUnichar* end = &aString[i];

                    // save off mCurrentFont and set it so that we
                    // cache the GC's font correctly
                    nsFontGTK *oldFont = mCurrentFont;
                    mCurrentFont = prevFont;
                    aContext->UpdateGC();

                    while (str < end) {
                        x = aX;
                        y = aY;
                        aContext->GetTranMatrix()->TransformCoord(&x, &y);
                        prevFont->DrawString(aContext, aSurface, x, y, str, 1);
                        aX += *aSpacing++;
                        str++;
                    }

                    mCurrentFont = oldFont;
                }
                else {
                    nsFontGTK *oldFont = mCurrentFont;
                    mCurrentFont = prevFont;
                    aContext->UpdateGC();

                    x += prevFont->DrawString(aContext, aSurface,
                                              x, y, &aString[start],
                                              i - start);

                    mCurrentFont = oldFont;
                }

                prevFont = currFont;
                start = i;
            }
        }
        else {
            prevFont = currFont;
            start = i;
        }
    }

    if (prevFont) {
        nsFontGTK *oldFont = mCurrentFont;
        mCurrentFont = prevFont;
        aContext->UpdateGC();
    
        if (aSpacing) {
            const PRUnichar* str = &aString[start];
            const PRUnichar* end = &aString[i];

            while (str < end) {
                x = aX;
                y = aY;
                aContext->GetTranMatrix()->TransformCoord(&x, &y);
                prevFont->DrawString(aContext, aSurface, x, y, str, 1);
                aX += *aSpacing++;
                str++;
            }
        }
        else {
            prevFont->DrawString(aContext, aSurface, x, y,
                                 &aString[start], i - start);
        }

        mCurrentFont = oldFont;
    }

  return NS_OK;
}

#ifdef MOZ_MATHML

nsresult
nsFontMetricsGTK::GetBoundingMetrics(const char *aString, PRUint32 aLength,
                                     nsBoundingMetrics &aBoundingMetrics,
                                     nsRenderingContextGTK *aContext)
{
    aBoundingMetrics.Clear();

    if (!aString || !aLength)
        return NS_ERROR_FAILURE;

    nsresult rv = NS_OK;

    nsXFont *xFont = mCurrentFont->GetXFont();

    if (mCurrentFont->IsFreeTypeFont()) {
        PRUnichar unichars[WIDEN_8_TO_16_BUF_SIZE];

        // need to fix this for long strings
        PRUint32 len = PR_MIN(aLength, WIDEN_8_TO_16_BUF_SIZE);

        // convert 7 bit data to unicode
        // this function is only supposed to be called for ascii data
        for (PRUint32 i=0; i<len; i++) {
            unichars[i] = (PRUnichar)((unsigned char)aString[i]);
        }

        rv = mCurrentFont->GetBoundingMetrics(unichars, len,
                                              aBoundingMetrics);
    }
    if (!mCurrentFont->GetXFontIs10646()) {
        // 8 bit data with an 8 bit font
        NS_ASSERTION(xFont->IsSingleByte(),"wrong string/font size");
        xFont->TextExtents8(aString, aLength,
                            &aBoundingMetrics.leftBearing, 
                            &aBoundingMetrics.rightBearing, 
                            &aBoundingMetrics.width, 
                            &aBoundingMetrics.ascent, 
                            &aBoundingMetrics.descent);
    }
    else {
        // we have 8 bit data but a 16 bit font
        NS_ASSERTION(!xFont->IsSingleByte(),"wrong string/font size");
        Widen8To16AndGetTextExtents (mCurrentFont->GetXFont(), 
                                     aString, aLength,
                                     &aBoundingMetrics.leftBearing, 
                                     &aBoundingMetrics.rightBearing, 
                                     &aBoundingMetrics.width, 
                                     &aBoundingMetrics.ascent, 
                                     &aBoundingMetrics.descent);
    }

    float P2T;
    P2T = mDeviceContext->DevUnitsToAppUnits();

    aBoundingMetrics.leftBearing =
        NSToCoordRound(aBoundingMetrics.leftBearing * P2T);
    aBoundingMetrics.rightBearing =
        NSToCoordRound(aBoundingMetrics.rightBearing * P2T);
    aBoundingMetrics.width = NSToCoordRound(aBoundingMetrics.width * P2T);
    aBoundingMetrics.ascent = NSToCoordRound(aBoundingMetrics.ascent * P2T);
    aBoundingMetrics.descent = NSToCoordRound(aBoundingMetrics.descent * P2T);

    return rv;
}

nsresult
nsFontMetricsGTK::GetBoundingMetrics(const PRUnichar *aString,
                                     PRUint32 aLength,
                                     nsBoundingMetrics &aBoundingMetrics,
                                     PRInt32 *aFontID,
                                     nsRenderingContextGTK *aContext)
{
    aBoundingMetrics.Clear(); 

    if (!aString || !aLength)
        return NS_ERROR_FAILURE;

    nsFontGTK* prevFont = nsnull;

    nsBoundingMetrics rawbm;
    PRBool firstTime = PR_TRUE;
    PRUint32 start = 0;
    PRUint32 i;
    PRUint32 extraSurrogateLength;
    for (i = 0; i < aLength; i+=1+extraSurrogateLength) {
        PRUint32 c = aString[i];
        extraSurrogateLength=0;
        if(i < aLength-1 && IS_HIGH_SURROGATE(c) && IS_LOW_SURROGATE(aString[i+1])) {
          // if surrogate, make UCS4 code point from high aString[i] and
          // low surrogate aString[i+1]
          c = SURROGATE_TO_UCS4(c, aString[i+1]);
 
          // skip aString[i+1], it is already used as low surrogate
          extraSurrogateLength = 1;
        }
        nsFontGTK* currFont = nsnull;
        nsFontGTK** font = mLoadedFonts;
        nsFontGTK** end = &mLoadedFonts[mLoadedFontsCount];

        while (font < end) {
            if (CCMAP_HAS_CHAR_EXT((*font)->mCCMap, c)) {
                currFont = *font;
                goto FoundFont; // for speed -- avoid "if" statement
            }
            font++;
        }
        currFont = FindFont(c);

    FoundFont:
        // XXX avoid this test by duplicating code -- erik
        if (prevFont) {
            if (currFont != prevFont) {
                prevFont->GetBoundingMetrics((const PRUnichar*)&aString[start],
                                             i - start, rawbm);
                if (firstTime) {
                    firstTime = PR_FALSE;
                    aBoundingMetrics = rawbm;
                } 
                else {
                    aBoundingMetrics += rawbm;
                }
                prevFont = currFont;
                start = i;
            }
        }
        else {
            prevFont = currFont;
            start = i;
        }
    }
    
    if (prevFont) {
        prevFont->GetBoundingMetrics((const PRUnichar*) &aString[start],
                                     i - start, rawbm);
        if (firstTime)
            aBoundingMetrics = rawbm;
        else
            aBoundingMetrics += rawbm;
    }

    // convert to app units
    float P2T;
    P2T = mDeviceContext->DevUnitsToAppUnits();

    aBoundingMetrics.leftBearing =
        NSToCoordRound(aBoundingMetrics.leftBearing * P2T);
    aBoundingMetrics.rightBearing =
        NSToCoordRound(aBoundingMetrics.rightBearing * P2T);
    aBoundingMetrics.width = NSToCoordRound(aBoundingMetrics.width * P2T);
    aBoundingMetrics.ascent = NSToCoordRound(aBoundingMetrics.ascent * P2T);
    aBoundingMetrics.descent = NSToCoordRound(aBoundingMetrics.descent * P2T);

    if (nsnull != aFontID)
        *aFontID = 0;

    return NS_OK;
}

#endif /* MOZ_MATHML */

nsresult
nsFontMetricsGTK::GetTextDimensions (const PRUnichar* aString,
                                     PRUint32 aLength,
                                     nsTextDimensions& aDimensions,
                                     PRInt32* aFontID,
                                     nsRenderingContextGTK *aContext)
{
    aDimensions.Clear();

    if (!aString || !aLength)
        return NS_ERROR_FAILURE;

    nsFontGTK* prevFont = nsnull;
    gint rawWidth = 0, rawAscent = 0, rawDescent = 0;
    PRUint32 start = 0;
    PRUint32 i;

    PRUint32 extraSurrogateLength;
    for (i = 0; i < aLength; i+=1+extraSurrogateLength) {
        PRUint32 c = aString[i];
        extraSurrogateLength=0;
        if(i < aLength-1 && IS_HIGH_SURROGATE(c) && IS_LOW_SURROGATE(aString[i+1])) {
          // if surrogate, make UCS4 code point from high aString[i] and
          // low surrogate aString[i+1]
          c = SURROGATE_TO_UCS4(c, aString[i+1]);
 
          // skip aString[i+1], it is already used as low surrogate
          extraSurrogateLength = 1;
        }
        nsFontGTK* currFont = nsnull;
        nsFontGTK** font = mLoadedFonts;
        nsFontGTK** end = &mLoadedFonts[mLoadedFontsCount];

        while (font < end) {
            if (CCMAP_HAS_CHAR_EXT((*font)->mCCMap, c)) {
                currFont = *font;
                goto FoundFont; // for speed -- avoid "if" statement
            }
            font++;
        }
        currFont = FindFont(c);

    FoundFont:
        // XXX avoid this test by duplicating code -- erik
        if (prevFont) {
            if (currFont != prevFont) {
                rawWidth += prevFont->GetWidth(&aString[start], i - start);
                if (rawAscent < prevFont->mMaxAscent)
                    rawAscent = prevFont->mMaxAscent;
                if (rawDescent < prevFont->mMaxDescent)
                    rawDescent = prevFont->mMaxDescent;
                prevFont = currFont;
                start = i;
            }
        }
        else {
            prevFont = currFont;
            start = i;
        }
    }

    if (prevFont) {
        rawWidth += prevFont->GetWidth(&aString[start], i - start);
        if (rawAscent < prevFont->mMaxAscent)
            rawAscent = prevFont->mMaxAscent;
        if (rawDescent < prevFont->mMaxDescent)
            rawDescent = prevFont->mMaxDescent;
    }

    float P2T;
    P2T = mDeviceContext->DevUnitsToAppUnits();

    aDimensions.width = NSToCoordRound(rawWidth * P2T);
    aDimensions.ascent = NSToCoordRound(rawAscent * P2T);
    aDimensions.descent = NSToCoordRound(rawDescent * P2T);

    if (nsnull != aFontID)
        *aFontID = 0;

    return NS_OK;
}

nsresult
nsFontMetricsGTK::GetTextDimensions (const char*         aString,
                                     PRInt32             aLength,
                                     PRInt32             aAvailWidth,
                                     PRInt32*            aBreaks,
                                     PRInt32             aNumBreaks,
                                     nsTextDimensions&   aDimensions,
                                     PRInt32&            aNumCharsFit,
                                     nsTextDimensions&   aLastWordDimensions,
                                     PRInt32*            aFontID,
                                     nsRenderingContextGTK *aContext)
{
    NS_PRECONDITION(aBreaks[aNumBreaks - 1] == aLength, "invalid break array");

    // If we need to back up this state represents the last place
    // we could break. We can use this to avoid remeasuring text
    PRInt32 prevBreakState_BreakIndex = -1; // not known
                                            // (hasn't been computed)
    nscoord prevBreakState_Width = 0; // accumulated width to this point

    // Initialize OUT parameters
    GetMaxAscent(aLastWordDimensions.ascent);
    GetMaxDescent(aLastWordDimensions.descent);
    aLastWordDimensions.width = -1;
    aNumCharsFit = 0;

    // Iterate each character in the string and determine which font to use
    nscoord width = 0;
    PRInt32 start = 0;
    nscoord aveCharWidth;
    GetAveCharWidth(aveCharWidth);

    while (start < aLength) {
      // Estimate how many characters will fit. Do that by
      // diving the available space by the average character
      // width. Make sure the estimated number of characters is
      // at least 1
      PRInt32 estimatedNumChars = 0;

      if (aveCharWidth > 0)
        estimatedNumChars = (aAvailWidth - width) / aveCharWidth;

      if (estimatedNumChars < 1)
        estimatedNumChars = 1;

      // Find the nearest break offset
      PRInt32 estimatedBreakOffset = start + estimatedNumChars;
      PRInt32 breakIndex;
      nscoord numChars;

      // Find the nearest place to break that is less than or equal to
      // the estimated break offset
      if (aLength <= estimatedBreakOffset) {
        // All the characters should fit
        numChars = aLength - start;
        breakIndex = aNumBreaks - 1;
      } 
      else {
        breakIndex = prevBreakState_BreakIndex;
        while (((breakIndex + 1) < aNumBreaks) &&
               (aBreaks[breakIndex + 1] <= estimatedBreakOffset)) {
          ++breakIndex;
        }

        if (breakIndex == prevBreakState_BreakIndex) {
          ++breakIndex; // make sure we advanced past the
          // previous break index
        }

        numChars = aBreaks[breakIndex] - start;
      }

      // Measure the text
      nscoord twWidth = 0;
      if ((1 == numChars) && (aString[start] == ' '))
        GetSpaceWidth(twWidth);
      else if (numChars > 0)
        GetWidth(&aString[start], numChars, twWidth, aContext);

      // See if the text fits
      PRBool  textFits = (twWidth + width) <= aAvailWidth;

      // If the text fits then update the width and the number of
      // characters that fit
      if (textFits) {
        aNumCharsFit += numChars;
        width += twWidth;
        start += numChars;

        // This is a good spot to back up to if we need to so remember
        // this state
        prevBreakState_BreakIndex = breakIndex;
        prevBreakState_Width = width;
      }
      else {
        // See if we can just back up to the previous saved
        // state and not have to measure any text
        if (prevBreakState_BreakIndex > 0) {
          // If the previous break index is just before the
          // current break index then we can use it
          if (prevBreakState_BreakIndex == (breakIndex - 1)) {
            aNumCharsFit = aBreaks[prevBreakState_BreakIndex];
            width = prevBreakState_Width;
            break;
          }
        }

        // We can't just revert to the previous break state
        if (0 == breakIndex) {
          // There's no place to back up to, so even though
          // the text doesn't fit return it anyway
          aNumCharsFit += numChars;
          width += twWidth;
          break;
        }

        // Repeatedly back up until we get to where the text
        // fits or we're all the way back to the first word
        width += twWidth;
        while ((breakIndex >= 1) && (width > aAvailWidth)) {
          twWidth = 0;
          start = aBreaks[breakIndex - 1];
          numChars = aBreaks[breakIndex] - start;

          if ((1 == numChars) && (aString[start] == ' '))
            GetSpaceWidth(twWidth);
          else if (numChars > 0)
            GetWidth(&aString[start], numChars, twWidth,
                     aContext);
          width -= twWidth;
          aNumCharsFit = start;
          breakIndex--;
        }
        break;
      }
    }

    aDimensions.width = width;
    GetMaxAscent(aDimensions.ascent);
    GetMaxDescent(aDimensions.descent);

    return NS_OK;
}

struct BreakGetTextDimensionsData {
  float    mP2T;               // IN
  PRInt32  mAvailWidth;        // IN
  PRInt32* mBreaks;            // IN
  PRInt32  mNumBreaks;         // IN
  nscoord  mSpaceWidth;        // IN
  nscoord  mAveCharWidth;      // IN
  PRInt32  mEstimatedNumChars; // IN (running -- to handle the edge case of one word)

  PRInt32  mNumCharsFit;  // IN/OUT -- accumulated number of chars that fit so far
  nscoord  mWidth;        // IN/OUT -- accumulated width so far

  // If we need to back up, this state represents the last place
  // we could break. We can use this to avoid remeasuring text
  PRInt32 mPrevBreakState_BreakIndex; // IN/OUT, initialized as -1, i.e., not yet computed
  nscoord mPrevBreakState_Width;      // IN/OUT, initialized as  0

  // Remember the fonts that we use so that we can deal with
  // line-breaking in-between fonts later. mOffsets[0] is also used
  // to initialize the current offset from where to start measuring
  nsVoidArray* mFonts;   // OUT
  nsVoidArray* mOffsets; // IN/OUT
};

static PRBool PR_CALLBACK
do_BreakGetTextDimensions(const nsFontSwitchGTK *aFontSwitch,
                          const PRUnichar*       aSubstring,
                          PRUint32               aSubstringLength,
                          void*                  aData)
{
  nsFontGTK* fontGTK = aFontSwitch->mFontGTK;

  // Make sure the font is selected
  BreakGetTextDimensionsData* data = (BreakGetTextDimensionsData*)aData;

  // Our current state relative to the _full_ string...
  // This allows emulation of the previous code...
  const PRUnichar* pstr = (const PRUnichar*)data->mOffsets->ElementAt(0);
  PRInt32 numCharsFit = data->mNumCharsFit;
  nscoord width = data->mWidth;
  PRInt32 start = (PRInt32)(aSubstring - pstr);
  PRInt32 i = start + aSubstringLength;
  PRBool allDone = PR_FALSE;

  while (start < i) {
    // Estimate how many characters will fit. Do that by dividing the
    // available space by the average character width
    PRInt32 estimatedNumChars = data->mEstimatedNumChars;
    if (!estimatedNumChars && data->mAveCharWidth > 0) {
      estimatedNumChars = (data->mAvailWidth - width) / data->mAveCharWidth;
    }
    // Make sure the estimated number of characters is at least 1
    if (estimatedNumChars < 1) {
      estimatedNumChars = 1;
    }

    // Find the nearest break offset
    PRInt32 estimatedBreakOffset = start + estimatedNumChars;
    PRInt32 breakIndex = -1; // not yet computed
    PRBool  inMiddleOfSegment = PR_FALSE;
    nscoord numChars;

    // Avoid scanning the break array in the case where we think all
    // the text should fit
    if (i <= estimatedBreakOffset) {
      // Everything should fit
      numChars = i - start;
    }
    else {
      // Find the nearest place to break that is less than or equal to
      // the estimated break offset
      breakIndex = data->mPrevBreakState_BreakIndex;
      while (breakIndex + 1 < data->mNumBreaks &&
             data->mBreaks[breakIndex + 1] <= estimatedBreakOffset) {
        ++breakIndex;
      }

      if (breakIndex == -1)
        breakIndex = 0;

      // We found a place to break that is before the estimated break
      // offset. Where we break depends on whether the text crosses a
      // segment boundary
      if (start < data->mBreaks[breakIndex]) {
        // The text crosses at least one segment boundary so measure to the
        // break point just before the estimated break offset
        numChars = PR_MIN(data->mBreaks[breakIndex] - start, (PRInt32)aSubstringLength);
      } 
      else {
        // See whether there is another segment boundary between this one
        // and the end of the text
        if ((breakIndex < (data->mNumBreaks - 1)) && (data->mBreaks[breakIndex] < i)) {
          ++breakIndex;
          numChars = PR_MIN(data->mBreaks[breakIndex] - start, (PRInt32)aSubstringLength);
        }
        else {
          NS_ASSERTION(i != data->mBreaks[breakIndex], "don't expect to be at segment boundary");

          // The text is all within the same segment
          numChars = i - start;

          // Remember we're in the middle of a segment and not between
          // two segments
          inMiddleOfSegment = PR_TRUE;
        }
      }
    }

    // Measure the text
    nscoord twWidth, pxWidth;
    if ((1 == numChars) && (pstr[start] == ' ')) {
      twWidth = data->mSpaceWidth;
    }
    else {
      pxWidth = fontGTK->GetWidth(&pstr[start], numChars);
      twWidth = NSToCoordRound(float(pxWidth) * data->mP2T);
    }

    // See if the text fits
    PRBool textFits = (twWidth + width) <= data->mAvailWidth;

    // If the text fits then update the width and the number of
    // characters that fit
    if (textFits) {
      numCharsFit += numChars;
      width += twWidth;

      // If we computed the break index and we're not in the middle
      // of a segment then this is a spot that we can back up to if
      // we need to, so remember this state
      if ((breakIndex != -1) && !inMiddleOfSegment) {
        data->mPrevBreakState_BreakIndex = breakIndex;
        data->mPrevBreakState_Width = width;
      }
    }
    else {
      // The text didn't fit. If we're out of room then we're all done
      allDone = PR_TRUE;

      // See if we can just back up to the previous saved state and not
      // have to measure any text
      if (data->mPrevBreakState_BreakIndex != -1) {
        PRBool canBackup;

        // If we're in the middle of a word then the break index
        // must be the same if we can use it. If we're at a segment
        // boundary, then if the saved state is for the previous
        // break index then we can use it
        if (inMiddleOfSegment) {
          canBackup = data->mPrevBreakState_BreakIndex == breakIndex;
        } else {
          canBackup = data->mPrevBreakState_BreakIndex == (breakIndex - 1);
        }

        if (canBackup) {
          numCharsFit = data->mBreaks[data->mPrevBreakState_BreakIndex];
          width = data->mPrevBreakState_Width;
          break;
        }
      }

      // We can't just revert to the previous break state. Find the break
      // index just before the end of the text
      i = start + numChars;
      if (breakIndex == -1) {
        breakIndex = 0;
        if (data->mBreaks[breakIndex] < i) {
          while ((breakIndex + 1 < data->mNumBreaks) && (data->mBreaks[breakIndex + 1] < i)) {
            ++breakIndex;
          }
        }
      }

      if ((0 == breakIndex) && (i <= data->mBreaks[0])) {
        // There's no place to back up to, so even though the text doesn't fit
        // return it anyway
        numCharsFit += numChars;
        width += twWidth;

        // Edge case of one word: it could be that we just measured a fragment of the
        // first word and its remainder involves other fonts, so we want to keep going
        // until we at least measure the entire first word
        if (numCharsFit < data->mBreaks[0]) {
          allDone = PR_FALSE;         
          // From now on we don't care anymore what is the _real_ estimated
          // number of characters that fits. Rather, we have no where to break
          // and have to measure one word fully, but the real estimate is less
          // than that one word. However, since the other bits of code rely on
          // what is in "data->mEstimatedNumChars", we want to override
          // "data->mEstimatedNumChars" and pass in what _has_ to be measured
          // so that it is transparent to the other bits that depend on it.
          data->mEstimatedNumChars = data->mBreaks[0] - numCharsFit;
          start += numChars;
        }

        break;
      }

      // Repeatedly back up until we get to where the text fits or we're
      // all the way back to the first word
      width += twWidth;
      while ((breakIndex >= 0) && (width > data->mAvailWidth)) {
        twWidth = 0;
        start = data->mBreaks[breakIndex];
        numChars = i - start;
        if ((1 == numChars) && (pstr[start] == ' ')) {
          twWidth = data->mSpaceWidth;
        }
        else if (numChars > 0) {
          pxWidth = fontGTK->GetWidth(&pstr[start], numChars);
          twWidth = NSToCoordRound(float(pxWidth) * data->mP2T);
        }

        width -= twWidth;
        numCharsFit = start;
        --breakIndex;
        i = start;
      }
    }

    start += numChars;
  }

#ifdef DEBUG_rbs
  NS_ASSERTION(allDone || start == i, "internal error");
  NS_ASSERTION(allDone || data->mNumCharsFit != numCharsFit, "internal error");
#endif /* DEBUG_rbs */

  if (data->mNumCharsFit != numCharsFit) {
    // some text was actually retained
    data->mWidth = width;
    data->mNumCharsFit = numCharsFit;
    data->mFonts->AppendElement(fontGTK);
    data->mOffsets->AppendElement((void*)&pstr[numCharsFit]);
  }

  if (allDone) {
    // stop now
    return PR_FALSE;
  }

  return PR_TRUE; // don't stop if we still need to measure more characters
}

nsresult
nsFontMetricsGTK::GetTextDimensions (const PRUnichar*    aString,
                                     PRInt32             aLength,
                                     PRInt32             aAvailWidth,
                                     PRInt32*            aBreaks,
                                     PRInt32             aNumBreaks,
                                     nsTextDimensions&   aDimensions,
                                     PRInt32&            aNumCharsFit,
                                     nsTextDimensions&   aLastWordDimensions,
                                     PRInt32*            aFontID,
                                     nsRenderingContextGTK *aContext)
{

    nscoord spaceWidth, aveCharWidth;
    GetSpaceWidth(spaceWidth);
    GetAveCharWidth(aveCharWidth);

    // Note: aBreaks[] is supplied to us so that the first word is
    // located at aString[0 .. aBreaks[0]-1] and more generally, the
    // k-th word is located at aString[aBreaks[k-1]
    // .. aBreaks[k]-1]. Whitespace can be included and each of them
    // counts as a word in its own right.

    // Upon completion of glyph resolution, characters that can be
    // represented with fonts[i] are at offsets[i] .. offsets[i+1]-1

    nsAutoVoidArray fonts, offsets;
    offsets.AppendElement((void*)aString);

    float f;
    f = mDeviceContext->DevUnitsToAppUnits();
    BreakGetTextDimensionsData data = { f, aAvailWidth,
                                        aBreaks, aNumBreaks,
                                        spaceWidth, aveCharWidth,
                                        0, 0, 0, -1, 0, &fonts, &offsets };

    ResolveForwards(aString, aLength, do_BreakGetTextDimensions,
                    &data);

    if (aFontID)
        *aFontID = 0;

    aNumCharsFit = data.mNumCharsFit;
    aDimensions.width = data.mWidth;

    ///////////////////
    // Post-processing for the ascent and descent:
    //
    // The width of the last word is included in the final width, but
    // its ascent and descent are kept aside for the moment. The
    // problem is that line-breaking may occur _before_ the last word,
    // and we don't want its ascent and descent to interfere. We can
    // re-measure the last word and substract its width
    // later. However, we need a special care for the ascent and
    // descent at the break-point. The idea is to keep the ascent and
    // descent of the last word separate, and let layout consider them
    // later when it has determined that line-breaking doesn't occur
    // before the last word.
    //
    // Therefore, there are two things to do:
    // 1. Determine the ascent and descent up to where line-breaking may occur.
    // 2. Determine the ascent and descent of the remainder.  For
    //   efficiency however, it is okay to bail out early if there is
    //   only one font (in this case, the height of the last word has no
    //   special effect on the total height).

    // aLastWordDimensions.width should be set to -1 to reply that we
    // don't know the width of the last word since we measure multiple
    // words
    aLastWordDimensions.Clear();
    aLastWordDimensions.width = -1;

    PRInt32 count = fonts.Count();
    if (!count)
        return NS_OK;

    nsFontGTK* fontGTK = (nsFontGTK*)fonts[0];
    NS_ASSERTION(fontGTK, "internal error in do_BreakGetTextDimensions");
    aDimensions.ascent = fontGTK->mMaxAscent;
    aDimensions.descent = fontGTK->mMaxDescent;

    // fast path - normal case, quick return if there is only one font
    if (count == 1)
        return NS_OK;

    // get the last break index.
    // If there is only one word, we end up with lastBreakIndex =
    // 0. We don't need to worry about aLastWordDimensions in this
    // case too. But if we didn't return earlier, it would mean that
    // the unique word needs several fonts and we will still have to
    // loop over the fonts to return the final height
    PRInt32 lastBreakIndex = 0;
    while (aBreaks[lastBreakIndex] < aNumCharsFit)
        ++lastBreakIndex;

    const PRUnichar* lastWord = (lastBreakIndex > 0) 
        ? aString + aBreaks[lastBreakIndex-1]
        : aString + aNumCharsFit; // let it point outside to play nice
                                  // with the loop

    // now get the desired ascent and descent information... this is
    // however a very fast loop of the order of the number of
    // additional fonts

    PRInt32 currFont = 0;
    const PRUnichar* pstr = aString;
    const PRUnichar* last = aString + aNumCharsFit;

    while (pstr < last) {
        fontGTK = (nsFontGTK*)fonts[currFont];
        PRUnichar* nextOffset = (PRUnichar*)offsets[++currFont]; 

        // For consistent word-wrapping, we are going to handle the
        // whitespace character with special care because a whitespace
        // character can come from a font different from that of the
        // previous word. If 'x', 'y', 'z', are Unicode points that
        // require different fonts, we want 'xyz <br>' and 'xyz<br>'
        // to have the same height because it gives a more stable
        // rendering, especially when the window is resized at the
        // edge of the word.
        // If we don't do this, a 'tall' trailing whitespace, i.e., if
        // the whitespace happens to come from a font with a bigger
        // ascent and/or descent than all current fonts on the line,
        // this can cause the next lines to be shifted down when the
        // window is slowly resized to fit that whitespace.
        if (*pstr == ' ') {
            // skip pass the whitespace to ignore the height that it
            // may contribute
            ++pstr;
            // get out if we reached the end
            if (pstr == last) {
                break;
            }
            // switch to the next font if we just passed the current font 
            if (pstr == nextOffset) {
                fontGTK = (nsFontGTK*)fonts[currFont];
                nextOffset = (PRUnichar*)offsets[++currFont];
            } 
        }

        // see if the last word intersects with the current font
        // (we are testing for 'nextOffset-1 >= lastWord' since the
        // current font ends at nextOffset-1)
        if (nextOffset > lastWord) {
            if (aLastWordDimensions.ascent < fontGTK->mMaxAscent) {
                aLastWordDimensions.ascent = fontGTK->mMaxAscent;
            }
            if (aLastWordDimensions.descent < fontGTK->mMaxDescent) {
                aLastWordDimensions.descent = fontGTK->mMaxDescent;
            }
        }

        // see if we have not reached the last word yet
        if (pstr < lastWord) {
            if (aDimensions.ascent < fontGTK->mMaxAscent) {
                aDimensions.ascent = fontGTK->mMaxAscent;
            }
            if (aDimensions.descent < fontGTK->mMaxDescent) {
                aDimensions.descent = fontGTK->mMaxDescent;
            }
        }

        // advance to where the next font starts
        pstr = nextOffset;
    }

    return NS_OK;
}

GdkFont *
nsFontMetricsGTK::GetCurrentGDKFont(void)
{
  return mCurrentFont->GetGDKFont();
}

nsresult
nsFontMetricsGTK::SetRightToLeftText(PRBool aIsRTL)
{
    return NS_OK;
}

PR_BEGIN_EXTERN_C
static int
CompareSizes(const void* aArg1, const void* aArg2, void *data)
{
  return (*((nsFontGTK**) aArg1))->mSize - (*((nsFontGTK**) aArg2))->mSize;
}
PR_END_EXTERN_C

void
nsFontStretch::SortSizes(void)
{
  NS_QuickSort(mSizes, mSizesCount, sizeof(*mSizes), CompareSizes, NULL);
}

void
nsFontWeight::FillStretchHoles(void)
{
  int i, j;

  for (i = 0; i < 9; i++) {
    if (mStretches[i]) {
      mStretches[i]->SortSizes();
    }
  }

  if (!mStretches[4]) {
    for (i = 5; i < 9; i++) {
      if (mStretches[i]) {
        mStretches[4] = mStretches[i];
        break;
      }
    }
    if (!mStretches[4]) {
      for (i = 3; i >= 0; i--) {
        if (mStretches[i]) {
          mStretches[4] = mStretches[i];
          break;
        }
      }
    }
  }

  for (i = 5; i < 9; i++) {
    if (!mStretches[i]) {
      for (j = i + 1; j < 9; j++) {
        if (mStretches[j]) {
          mStretches[i] = mStretches[j];
          break;
        }
      }
      if (!mStretches[i]) {
        for (j = i - 1; j >= 0; j--) {
          if (mStretches[j]) {
            mStretches[i] = mStretches[j];
            break;
          }
        }
      }
    }
  }
  for (i = 3; i >= 0; i--) {
    if (!mStretches[i]) {
      for (j = i - 1; j >= 0; j--) {
        if (mStretches[j]) {
          mStretches[i] = mStretches[j];
          break;
        }
      }
      if (!mStretches[i]) {
        for (j = i + 1; j < 9; j++) {
          if (mStretches[j]) {
            mStretches[i] = mStretches[j];
            break;
          }
        }
      }
    }
  }
}

void
nsFontStyle::FillWeightHoles(void)
{
  int i, j;

  for (i = 0; i < 9; i++) {
    if (mWeights[i]) {
      mWeights[i]->FillStretchHoles();
    }
  }

  if (!mWeights[3]) {
    for (i = 4; i < 9; i++) {
      if (mWeights[i]) {
        mWeights[3] = mWeights[i];
        break;
      }
    }
    if (!mWeights[3]) {
      for (i = 2; i >= 0; i--) {
        if (mWeights[i]) {
          mWeights[3] = mWeights[i];
          break;
        }
      }
    }
  }

  // CSS2, section 15.5.1
  if (!mWeights[4]) {
    mWeights[4] = mWeights[3];
  }
  for (i = 5; i < 9; i++) {
    if (!mWeights[i]) {
      for (j = i + 1; j < 9; j++) {
        if (mWeights[j]) {
          mWeights[i] = mWeights[j];
          break;
        }
      }
      if (!mWeights[i]) {
        for (j = i - 1; j >= 0; j--) {
          if (mWeights[j]) {
            mWeights[i] = mWeights[j];
            break;
          }
        }
      }
    }
  }
  for (i = 2; i >= 0; i--) {
    if (!mWeights[i]) {
      for (j = i - 1; j >= 0; j--) {
        if (mWeights[j]) {
          mWeights[i] = mWeights[j];
          break;
        }
      }
      if (!mWeights[i]) {
        for (j = i + 1; j < 9; j++) {
          if (mWeights[j]) {
            mWeights[i] = mWeights[j];
            break;
          }
        }
      }
    }
  }
}

void
nsFontNode::FillStyleHoles(void)
{
  if (mHolesFilled) {
    return;
  }
  mHolesFilled = 1;

#ifdef DEBUG_DUMP_TREE
  DumpFamily(this);
#endif

  for (int i = 0; i < 3; i++) {
    if (mStyles[i]) {
      mStyles[i]->FillWeightHoles();
    }
  }

  // XXX If both italic and oblique exist, there is probably something
  // wrong. Try counting the fonts, and removing the one that has less.
  if (!mStyles[NS_FONT_STYLE_NORMAL]) {
    if (mStyles[NS_FONT_STYLE_ITALIC]) {
      mStyles[NS_FONT_STYLE_NORMAL] = mStyles[NS_FONT_STYLE_ITALIC];
    }
    else {
      mStyles[NS_FONT_STYLE_NORMAL] = mStyles[NS_FONT_STYLE_OBLIQUE];
    }
  }
  if (!mStyles[NS_FONT_STYLE_ITALIC]) {
    if (mStyles[NS_FONT_STYLE_OBLIQUE]) {
      mStyles[NS_FONT_STYLE_ITALIC] = mStyles[NS_FONT_STYLE_OBLIQUE];
    }
    else {
      mStyles[NS_FONT_STYLE_ITALIC] = mStyles[NS_FONT_STYLE_NORMAL];
    }
  }
  if (!mStyles[NS_FONT_STYLE_OBLIQUE]) {
    if (mStyles[NS_FONT_STYLE_ITALIC]) {
      mStyles[NS_FONT_STYLE_OBLIQUE] = mStyles[NS_FONT_STYLE_ITALIC];
    }
    else {
      mStyles[NS_FONT_STYLE_OBLIQUE] = mStyles[NS_FONT_STYLE_NORMAL];
    }
  }

#ifdef DEBUG_DUMP_TREE
  DumpFamily(this);
#endif
}

static void
SetCharsetLangGroup(nsFontCharSetInfo* aCharSetInfo)
{
  if (!aCharSetInfo->mCharSet || aCharSetInfo->mLangGroup)
    return;

  nsresult res;
  
  res = gCharSetManager->GetCharsetLangGroupRaw(aCharSetInfo->mCharSet,
                                                &aCharSetInfo->mLangGroup);
  if (NS_FAILED(res)) {
    aCharSetInfo->mLangGroup = NS_NewAtom("");
#ifdef NOISY_FONTS
    printf("=== cannot get lang group for %s\n", aCharSetInfo->mCharSet);
#endif
  }
}

#define GET_WEIGHT_INDEX(index, weight) \
  do {                                  \
    (index) = WEIGHT_INDEX(weight);     \
    if ((index) < 0) {                  \
      (index) = 0;                      \
    }                                   \
    else if ((index) > 8) {             \
      (index) = 8;                      \
    }                                   \
  } while (0)

nsFontGTK*
nsFontMetricsGTK::SearchNode(nsFontNode* aNode, PRUint32 aChar)
{
  if (aNode->mDummy) {
    return nsnull;
  }

  nsFontCharSetInfo* charSetInfo = aNode->mCharSetInfo;

  /*
   * mCharSet is set if we know which glyphs will be found in these fonts.
   * If mCCMap has already been created for this charset, we compare it with
   * the mCCMaps of the previously loaded fonts. If it is the same as any of
   * the previous ones, we return nsnull because there is no point in
   * loading a font with the same map.
   */
  if (charSetInfo->mCharSet) {
    // if SURROGATE char, ignore charSetInfo->mCCMap checking
    // because the exact ccmap is never created before loading
    // NEED TO FIX: need better way
    if (IS_SURROGATE(aChar) ) {
      goto check_done;
    }
    PRUint16* ccmap = charSetInfo->mCCMap;
    if (ccmap) {
      for (int i = 0; i < mLoadedFontsCount; i++) {
        if (mLoadedFonts[i]->mCCMap == ccmap) {
          return nsnull;
        }
      }
    }
    else {
      if (!SetUpFontCharSetInfo(charSetInfo))
        return nsnull;
    }
  }
  else {
    if ((!mIsUserDefined) && (charSetInfo == &Unknown)) {
      return nsnull;
    }
  }

check_done:

  aNode->FillStyleHoles();
  nsFontStyle* style = aNode->mStyles[mStyleIndex];

  nsFontWeight** weights = style->mWeights;
  int weight = mFont->weight;
  int steps = (weight % 100);
  int weightIndex;
  if (steps) {
    if (steps < 10) {
      int base = (weight - steps);
      GET_WEIGHT_INDEX(weightIndex, base);
      while (steps--) {
        nsFontWeight* prev = weights[weightIndex];
        for (weightIndex++; weightIndex < 9; weightIndex++) {
          if (weights[weightIndex] != prev) {
            break;
          }
        }
        if (weightIndex >= 9) {
          weightIndex = 8;
        }
      }
    }
    else if (steps > 90) {
      steps = (100 - steps);
      int base = (weight + steps);
      GET_WEIGHT_INDEX(weightIndex, base);
      while (steps--) {
        nsFontWeight* prev = weights[weightIndex];
        for (weightIndex--; weightIndex >= 0; weightIndex--) {
          if (weights[weightIndex] != prev) {
            break;
          }
        }
        if (weightIndex < 0) {
          weightIndex = 0;
        }
      }
    }
    else {
      GET_WEIGHT_INDEX(weightIndex, weight);
    }
  }
  else {
    GET_WEIGHT_INDEX(weightIndex, weight);
  }

  FIND_FONT_PRINTF(("        load font %s", aNode->mName.get()));
  return PickASizeAndLoad(weights[weightIndex]->mStretches[mStretchIndex],
    charSetInfo, aChar, aNode->mName.get());
}

static void 
SetFontLangGroupInfo(nsFontCharSetMap* aCharSetMap)
{
  nsFontLangGroup *fontLangGroup = aCharSetMap->mFontLangGroup;
  if (!fontLangGroup)
    return;

  // get the atom for mFontLangGroup->mFontLangGroupName so we can
  // apply fontLangGroup operations to it
  // eg: search for related groups, check for scaling prefs
  const char *langGroup = fontLangGroup->mFontLangGroupName;

  // the font.scale prefs don't make sense without a langGroup
  // XXX FIX!!!  if langGroup is "", we need to bypass the font.scale stuff
  if (!langGroup)
    langGroup = "";
  if (!fontLangGroup->mFontLangGroupAtom) {
    fontLangGroup->mFontLangGroupAtom = NS_NewAtom(langGroup);
  }

  // hack : map 'x-zh-TWHK' to 'zh-TW' when retrieving font scaling-control
  // preferences via |Get*Pref|.
  // XXX : This would make the font scaling controls for 
  // zh-HK NOT work if a font common to zh-TW and zh-HK (e.g. big5-0) 
  // were chosen for zh-HK. An alternative would be to make it 
  // locale-dependent. Even with that, it'd work only under zh-HK locale.
  if (fontLangGroup->mFontLangGroupAtom == gZHTWHK) {
    langGroup = "zh-TW";  
  }

  // get the font scaling controls
  nsFontCharSetInfo *charSetInfo = aCharSetMap->mInfo;
  if (!charSetInfo->mInitedSizeInfo) {
    charSetInfo->mInitedSizeInfo = PR_TRUE;

    nsCAutoString name;
    nsresult rv;
    name.Assign("font.scale.outline.min.");
    name.Append(langGroup);
    rv = gPref->GetIntPref(name.get(), &charSetInfo->mOutlineScaleMin);
    if (NS_SUCCEEDED(rv))
      SIZE_FONT_PRINTF(("%s = %d", name.get(), charSetInfo->mOutlineScaleMin));
    else
      charSetInfo->mOutlineScaleMin = gOutlineScaleMinimum;

    name.Assign("font.scale.aa_bitmap.min.");
    name.Append(langGroup);
    rv = gPref->GetIntPref(name.get(), &charSetInfo->mAABitmapScaleMin);
    if (NS_SUCCEEDED(rv))
      SIZE_FONT_PRINTF(("%s = %d", name.get(), charSetInfo->mAABitmapScaleMin));
    else
      charSetInfo->mAABitmapScaleMin = gAABitmapScaleMinimum;

    name.Assign("font.scale.bitmap.min.");
    name.Append(langGroup);
    rv = gPref->GetIntPref(name.get(), &charSetInfo->mBitmapScaleMin);
    if (NS_SUCCEEDED(rv))
      SIZE_FONT_PRINTF(("%s = %d", name.get(), charSetInfo->mBitmapScaleMin));
    else
      charSetInfo->mBitmapScaleMin = gBitmapScaleMinimum;

    name.Assign("font.scale.aa_bitmap.oversize.");
    name.Append(langGroup);
    PRInt32 percent = 0;
    rv = gPref->GetIntPref(name.get(), &percent);
    if (NS_SUCCEEDED(rv)) {
      charSetInfo->mAABitmapOversize = percent/100.0;
      SIZE_FONT_PRINTF(("%s = %g", name.get(), charSetInfo->mAABitmapOversize));
    }
    else
      charSetInfo->mAABitmapOversize = gAABitmapOversize;

    percent = 0;
    name.Assign("font.scale.aa_bitmap.undersize.");
    name.Append(langGroup);
    rv = gPref->GetIntPref(name.get(), &percent);
    if (NS_SUCCEEDED(rv)) {
      charSetInfo->mAABitmapUndersize = percent/100.0;
      SIZE_FONT_PRINTF(("%s = %g", name.get(),charSetInfo->mAABitmapUndersize));
    }
    else
      charSetInfo->mAABitmapUndersize = gAABitmapUndersize;

    PRBool val = PR_TRUE;
    name.Assign("font.scale.aa_bitmap.always.");
    name.Append(langGroup);
    rv = gPref->GetBoolPref(name.get(), &val);
    if (NS_SUCCEEDED(rv)) {
      charSetInfo->mAABitmapScaleAlways = val;
      SIZE_FONT_PRINTF(("%s = %d", name.get(),charSetInfo->mAABitmapScaleAlways));
    }
    else
      charSetInfo->mAABitmapScaleAlways = gAABitmapScaleAlways;

    percent = 0;
    name.Assign("font.scale.bitmap.oversize.");
    name.Append(langGroup);
    rv = gPref->GetIntPref(name.get(), &percent);
    if (NS_SUCCEEDED(rv)) {
      charSetInfo->mBitmapOversize = percent/100.0;
      SIZE_FONT_PRINTF(("%s = %g", name.get(), charSetInfo->mBitmapOversize));
    }
    else
      charSetInfo->mBitmapOversize = gBitmapOversize;

    percent = 0;
    name.Assign("font.scale.bitmap.undersize.");
    name.Append(langGroup);
    rv = gPref->GetIntPref(name.get(), &percent);
    if (NS_SUCCEEDED(rv)) {
      charSetInfo->mBitmapUndersize = percent/100.0;
      SIZE_FONT_PRINTF(("%s = %g", name.get(), charSetInfo->mBitmapUndersize));
    }
    else
      charSetInfo->mBitmapUndersize = gBitmapUndersize;
  }
}

static nsFontStyle*
NodeGetStyle(nsFontNode* aNode, int aStyleIndex)
{
  nsFontStyle* style = aNode->mStyles[aStyleIndex];
  if (!style) {
    style = new nsFontStyle;
    if (!style) {
      return nsnull;
    }
    aNode->mStyles[aStyleIndex] = style;
  }
  return style;
}

static nsFontWeight*
NodeGetWeight(nsFontStyle* aStyle, int aWeightIndex)
{
  nsFontWeight* weight = aStyle->mWeights[aWeightIndex];
  if (!weight) {
    weight = new nsFontWeight;
    if (!weight) {
      return nsnull;
    }
    aStyle->mWeights[aWeightIndex] = weight;
  }
  return weight;
}

static nsFontStretch* 
NodeGetStretch(nsFontWeight* aWeight, int aStretchIndex)
{
  nsFontStretch* stretch = aWeight->mStretches[aStretchIndex];
  if (!stretch) {
    stretch = new nsFontStretch;
    if (!stretch) {
      return nsnull;
    }
    aWeight->mStretches[aStretchIndex] = stretch;
  }
  return stretch;
}

static PRBool
NodeAddScalable(nsFontStretch* aStretch, PRBool aOutlineScaled, 
                const char *aDashFoundry, const char *aFamily, 
                const char *aWeight,      const char * aSlant, 
                const char *aWidth,       const char *aStyle, 
                const char *aSpacing,     const char *aCharSet)
{
  // if we have both an outline scaled font and a bitmap 
  // scaled font pick the outline scaled font
  if ((aStretch->mScalable) && (!aStretch->mOutlineScaled) 
      && (aOutlineScaled)) {
    PR_smprintf_free(aStretch->mScalable);
    aStretch->mScalable = nsnull;
  }
  if (!aStretch->mScalable) {
    aStretch->mOutlineScaled = aOutlineScaled;
    if (aOutlineScaled) {
      aStretch->mScalable = 
          PR_smprintf("%s-%s-%s-%s-%s-%s-%%d-*-0-0-%s-*-%s", 
          aDashFoundry, aFamily, aWeight, aSlant, aWidth, aStyle, 
          aSpacing, aCharSet);
      if (!aStretch->mScalable)
        return PR_FALSE;
    }
    else {
      aStretch->mScalable = 
          PR_smprintf("%s-%s-%s-%s-%s-%s-%%d-*-*-*-%s-*-%s", 
          aDashFoundry, aFamily, aWeight, aSlant, aWidth, aStyle, 
          aSpacing, aCharSet);
      if (!aStretch->mScalable)
        return PR_FALSE;
    }
  }
  return PR_TRUE;
}

static PRBool
NodeAddSize(nsFontStretch* aStretch, 
            int aPixelSize, int aPointSize,
            float scaler,
            int aResX,      int aResY,
            const char *aDashFoundry, const char *aFamily, 
            const char *aWeight,      const char * aSlant, 
            const char *aWidth,       const char *aStyle, 
            const char *aSpacing,     const char *aCharSet,
            nsFontCharSetInfo* aCharSetInfo)
{
  if (scaler!=1.0f)
  {
    aPixelSize = int(float(aPixelSize) * scaler);
    aPointSize = int(float(aPointSize) * scaler);
    aResX = 0;
    aResY = 0;
  }

  PRBool haveSize = PR_FALSE;
  if (aStretch->mSizesCount) {
    nsFontGTK** end = &aStretch->mSizes[aStretch->mSizesCount];
    nsFontGTK** s;
    for (s = aStretch->mSizes; s < end; s++) {
      if ((*s)->mSize == aPixelSize) {
        haveSize = PR_TRUE;
        break;
      }
    }
  }
  if (!haveSize) {
    if (aStretch->mSizesCount == aStretch->mSizesAlloc) {
      int newSize = 2 * (aStretch->mSizesAlloc ? aStretch->mSizesAlloc : 1);
      nsFontGTK** newSizes = new nsFontGTK*[newSize];
      if (!newSizes)
        return PR_FALSE;
      for (int j = aStretch->mSizesAlloc - 1; j >= 0; j--) {
        newSizes[j] = aStretch->mSizes[j];
      }
      aStretch->mSizesAlloc = newSize;
      delete [] aStretch->mSizes;
      aStretch->mSizes = newSizes;
    }
    char *name = PR_smprintf("%s-%s-%s-%s-%s-%s-%d-%d-%d-%d-%s-*-%s", 
                             aDashFoundry, aFamily, aWeight, aSlant, aWidth, aStyle, 
                             aPixelSize, aPointSize, aResX, aResY, aSpacing, aCharSet);  

    if (!name) {
      return PR_FALSE;
    }
    nsFontGTK* size = new nsFontGTKNormal();
    if (!size) {
      return PR_FALSE;
    }
    aStretch->mSizes[aStretch->mSizesCount++] = size;
    size->mName           = name;
    // size->mFont is initialized in the constructor
    size->mSize           = aPixelSize;
    size->mBaselineAdjust = 0;
    size->mCCMap          = nsnull;
    size->mCharSetInfo    = aCharSetInfo;
  }
  return PR_TRUE;
}

static void
GetFontNames(const char* aPattern, PRBool aAnyFoundry, PRBool aOnlyOutlineScaledFonts, nsFontNodeArray* aNodes)
{
#ifdef NS_FONT_DEBUG_CALL_TRACE
  if (gFontDebug & NS_FONT_DEBUG_CALL_TRACE) {
    printf("GetFontNames %s\n", aPattern);
  }
#endif

  // get FreeType fonts
  nsFT2FontNode::GetFontNames(aPattern, aNodes);

  nsCAutoString previousNodeName;
  nsHashtable* node_hash;
  if (aAnyFoundry) {
    NS_ASSERTION(aPattern[1] == '*', "invalid 'anyFoundry' pattern");
    node_hash = gAFRENodes;
  }
  else {
    node_hash = gFFRENodes;
  }
  
#ifdef ENABLE_X_FONT_BANNING
  int  screen_xres,
       screen_yres;
  /* Get Xserver DPI. 
   * We cannot use Mozilla's API here because it may "override" the DPI
   * got from the Xserver via prefs. But we want to filter ("ban") fonts
   * we get from the Xserver which _it_(=Xserver) has "choosen" for us
   * using its DPI value ...
   */
  screen_xres = int((float(::gdk_screen_width())  / (float(::gdk_screen_width_mm())  / 25.4f)) + 0.5f);
  screen_yres = int((float(::gdk_screen_height()) / (float(::gdk_screen_height_mm()) / 25.4f)) + 0.5f);
#endif /* ENABLE_X_FONT_BANNING */
      
  BANNED_FONT_PRINTF(("Loading font '%s'", aPattern));
  /*
   * We do not use XListFontsWithInfo here, because it is very expensive.
   * Instead, we get that info at the time when we actually load the font.
   */
  int count;
  char** list = ::XListFonts(GDK_DISPLAY(), aPattern, INT_MAX, &count);
  if ((!list) || (count < 1)) {
    return;
  }
  for (int i = 0; i < count; i++) {
    char name[256]; /* X11 font names are never larger than 255 chars */
    strcpy(name, list[i]);
 
    /* Check if we can handle the font name ('*' and '?' are only valid in
     * input patterns passed as argument to |XListFont()|&co. but _not_ in
     * font names returned by these functions (see bug 136743 ("xlib complains
     * a lot about fonts with '*' in the XLFD string"))) */
    if ((!name) || (name[0] != '-') || (PL_strpbrk(name, "*?") != nsnull)) {
      continue;
    }
    
    char buf[512];
    PL_strncpyz(buf, name, sizeof(buf));
    char *fName = buf;
    char* p = name + 1;
    int scalable = 0;
    PRBool outline_scaled = PR_FALSE;
    int    resX = -1,
           resY = -1;

#ifdef FIND_FIELD
#undef FIND_FIELD
#endif
#define FIND_FIELD(var)           \
  char* var = p;                  \
  while ((*p) && ((*p) != '-')) { \
    p++;                          \
  }                               \
  if (*p) {                       \
    *p++ = 0;                     \
  }                               \
  else {                          \
    continue;                     \
  }

#ifdef SKIP_FIELD
#undef SKIP_FIELD
#endif
#define SKIP_FIELD(var)           \
  while ((*p) && ((*p) != '-')) { \
    p++;                          \
  }                               \
  if (*p) {                       \
    p++;                          \
  }                               \
  else {                          \
    continue;                     \
  }

    FIND_FIELD(foundry);
    // XXX What to do about the many Applix fonts that start with "ax"?
    FIND_FIELD(familyName);
    FIND_FIELD(weightName);
    FIND_FIELD(slant);
    FIND_FIELD(setWidth);
    FIND_FIELD(addStyle);
    FIND_FIELD(pixelSize);
    if (pixelSize[0] == '0') {
      scalable = 1;
    }
    FIND_FIELD(pointSize);
    if (pointSize[0] == '0') {
      scalable = 1;
    }
    FIND_FIELD(resolutionX);
    resX = atoi(resolutionX);
    NS_ASSERTION(!(resolutionX[0] != '0' && resX == 0), "atoi(resolutionX) failure.");
    if (resolutionX[0] == '0') {
      scalable = 1;
    }
    FIND_FIELD(resolutionY);
    resY = atoi(resolutionY);
    NS_ASSERTION(!(resolutionY[0] != '0' && resY == 0), "atoi(resolutionY) failure.");
    if (resolutionY[0] == '0') {
      scalable = 1;
    }
    // check if bitmap non-scaled font
    if ((pixelSize[0] != '0') || (pointSize[0] != '0')) {
      SCALED_FONT_PRINTF(("bitmap (non-scaled) font: %s", fName));
    }
    // check if bitmap scaled font
    else if ((pixelSize[0] == '0') && (pointSize[0] == '0')
          && (resolutionX[0] != '0') && (resolutionY[0] != '0')) {
      SCALED_FONT_PRINTF(("bitmap scaled font: %s", fName));
    }
    // check if outline scaled font
    else if ((pixelSize[0] == '0') && (pointSize[0] == '0')
          && (resolutionX[0] == '0') && (resolutionY[0] == '0')) {
      outline_scaled = PR_TRUE;
      SCALED_FONT_PRINTF(("outline scaled font: %s", fName));
    }
    else {
      SCALED_FONT_PRINTF(("unexpected font values: %s", fName));
      SCALED_FONT_PRINTF(("      pixelSize[0] = %c", pixelSize[0]));
      SCALED_FONT_PRINTF(("      pointSize[0] = %c", pointSize[0]));
      SCALED_FONT_PRINTF(("    resolutionX[0] = %c", resolutionX[0]));
      SCALED_FONT_PRINTF(("    resolutionY[0] = %c", resolutionY[0]));
      static PRBool already_complained = PR_FALSE;
      // only complaing once 
      if (!already_complained) {
        already_complained = PR_TRUE;
        NS_ASSERTION(pixelSize[0] == '0', "font scaler type test failed");
        NS_ASSERTION(pointSize[0] == '0', "font scaler type test failed");
        NS_ASSERTION(resolutionX[0] == '0', "font scaler type test failed");
        NS_ASSERTION(resolutionY[0] == '0', "font scaler type test failed");
      }
    }
    FIND_FIELD(spacing);
    FIND_FIELD(averageWidth);
    if (averageWidth[0] == '0') {
      scalable = 1;
/* Workaround for bug 103159 ("sorting fonts by foundry names cause font
 * size of css ignored in some cases").
 * Hardcoded font ban until bug 104075 ("need X font banning") has been
 * implemented. See http://bugzilla.mozilla.org/show_bug.cgi?id=94327#c34
 * for additional comments...
 */      
#ifndef DISABLE_WORKAROUND_FOR_BUG_103159
      // skip 'mysterious' and 'spurious' cases like
      // -adobe-times-medium-r-normal--17-120-100-100-p-0-iso8859-9
      if ((pixelSize[0] != '0' || pointSize[0] != 0) && 
          (outline_scaled == PR_FALSE)) {
        PR_LOG(FontMetricsGTKLM, PR_LOG_DEBUG, ("rejecting font '%s' (via hardcoded workaround for bug 103159)\n", list[i]));
        BANNED_FONT_PRINTF(("rejecting font '%s' (via hardcoded workaround for bug 103159)", list[i]));          
        continue;
      }  
#endif /* DISABLE_WORKAROUND_FOR_BUG_103159 */
    }
    char* charSetName = p; // CHARSET_REGISTRY & CHARSET_ENCODING
    if (!*charSetName) {
      continue;
    }
    
    if (aOnlyOutlineScaledFonts && (outline_scaled == PR_FALSE)) {
      continue;
    }

#ifdef ENABLE_X_FONT_BANNING
#define BOOL2STR(b) ((b)?("true"):("false"))    
    if (gFontRejectRegEx || gFontAcceptRegEx) {
      char fmatchbuf[512]; /* See sprintf() below. */
           
      sprintf(fmatchbuf, "fname=%s;scalable=%s;outline_scaled=%s;xdisplay=%s;xdpy=%d;ydpy=%d;xdevice=%s",
              list[i], /* full font name */
              BOOL2STR(scalable), 
              BOOL2STR(outline_scaled),
              XDisplayString(GDK_DISPLAY()),
              screen_xres,
              screen_yres,
              "display" /* Xlib gfx supports other devices like "printer", too - DO NOT REMOVE! */
              );
#undef BOOL2STR
                  
      if (gFontRejectRegEx) {
        /* reject font if reject pattern matches it... */        
        if (regexec(gFontRejectRegEx, fmatchbuf, 0, nsnull, 0) == REG_OK) {
          PR_LOG(FontMetricsGTKLM, PR_LOG_DEBUG, ("rejecting font '%s' (via reject pattern)\n", fmatchbuf));
          BANNED_FONT_PRINTF(("rejecting font '%s' (via reject pattern)", fmatchbuf));
          continue;
        }  
      }

      if (gFontAcceptRegEx) {
        if (regexec(gFontAcceptRegEx, fmatchbuf, 0, nsnull, 0) == REG_NOMATCH) {
          PR_LOG(FontMetricsGTKLM, PR_LOG_DEBUG, ("rejecting font '%s' (via accept pattern)\n", fmatchbuf));
          BANNED_FONT_PRINTF(("rejecting font '%s' (via accept pattern)", fmatchbuf));
          continue;
        }
      }       
    }    
#endif /* ENABLE_X_FONT_BANNING */    

    nsFontCharSetMap *charSetMap = GetCharSetMap(charSetName);
    nsFontCharSetInfo* charSetInfo = charSetMap->mInfo;
    // indirection for font specific charset encoding 
    if (charSetInfo == &Special) {
      nsCAutoString familyCharSetName(familyName);
      familyCharSetName.Append('-');
      familyCharSetName.Append(charSetName);
      nsCStringKey familyCharSetKey(familyCharSetName);
      charSetMap = NS_STATIC_CAST(nsFontCharSetMap*, gSpecialCharSets->Get(&familyCharSetKey));
      if (!charSetMap)
        charSetMap = gNoneCharSetMap;
      charSetInfo = charSetMap->mInfo;
    }
    if (!charSetInfo) {
#ifdef NOISY_FONTS
      printf("cannot find charset %s\n", charSetName);
#endif
      charSetInfo = &Unknown;
    }
    SetCharsetLangGroup(charSetInfo);
    SetFontLangGroupInfo(charSetMap);

    nsCAutoString nodeName;
    if (aAnyFoundry)
      nodeName.Assign('*');
    else
      nodeName.Assign(foundry);
    nodeName.Append('-');
    nodeName.Append(familyName);
    nodeName.Append('-');
    nodeName.Append(charSetName);
    nsCStringKey key(nodeName);
    nsFontNode* node = (nsFontNode*) node_hash->Get(&key);
    if (!node) {
      node = new nsFontNode;
      if (!node) {
        continue;
      }
      node_hash->Put(&key, node);
      node->mName = nodeName;
      node->mCharSetInfo = charSetInfo;
    }

    int found = 0;
    if (nodeName == previousNodeName) {
      found = 1;
    }
    else {
      found = (aNodes->IndexOf(node) >= 0);
    }
    previousNodeName = nodeName;
    if (!found) {
      aNodes->AppendElement(node);
    }

    int styleIndex;
    // XXX This does not cover the full XLFD spec for SLANT.
    switch (slant[0]) {
    case 'i':
      styleIndex = NS_FONT_STYLE_ITALIC;
      break;
    case 'o':
      styleIndex = NS_FONT_STYLE_OBLIQUE;
      break;
    case 'r':
    default:
      styleIndex = NS_FONT_STYLE_NORMAL;
      break;
    }
    nsFontStyle* style = NodeGetStyle(node, styleIndex);
    if (!style)
      continue;

    nsCStringKey weightKey(weightName);
    int weightNumber = NS_PTR_TO_INT32(gWeights->Get(&weightKey));
    if (!weightNumber) {
#ifdef NOISY_FONTS
      printf("cannot find weight %s\n", weightName);
#endif
      weightNumber = NS_FONT_WEIGHT_NORMAL;
    }
    int weightIndex = WEIGHT_INDEX(weightNumber);
    nsFontWeight* weight = NodeGetWeight(style, weightIndex);
    if (!weight)
      continue;
  
    nsCStringKey setWidthKey(setWidth);
    int stretchIndex = NS_PTR_TO_INT32(gStretches->Get(&setWidthKey));
    if (!stretchIndex) {
#ifdef NOISY_FONTS
      printf("cannot find stretch %s\n", setWidth);
#endif
      stretchIndex = 5;
    }
    stretchIndex--;
    nsFontStretch* stretch = NodeGetStretch(weight, stretchIndex);
    if (!stretch)
      continue;

    if (scalable) {
      if (!NodeAddScalable(stretch, outline_scaled, name, familyName, 
           weightName, slant, setWidth, addStyle, spacing, charSetName))
        continue;
    }
  
     // get pixel size before the string is changed
    int pixels,
        points;

    pixels = atoi(pixelSize);
    points = atoi(pointSize);

    if (pixels) {
      if (gScaleBitmapFontsWithDevScale && (gDevScale > 1.0f)) {
        /* Add a font size which is exactly scaled as the scaling factor ... */
        if (!NodeAddSize(stretch, pixels, points, gDevScale, resX, resY, name, familyName, weightName, 
                         slant, setWidth, addStyle, spacing, charSetName, charSetInfo))
          continue;

        /* ... and offer a range of scaled fonts with integer scaling factors
         * (we're taking half steps between integers, too - to avoid too big
         * steps between font sizes) */
        float minScaler = PR_MAX(gDevScale / 2.0f, 1.5f),
              maxScaler = gDevScale * 2.f,
              scaler;
        for( scaler = minScaler ; scaler <= maxScaler ; scaler += 0.5f )
        {
          if (!NodeAddSize(stretch, pixels, points, scaler, resX, resY, name, familyName, weightName, 
                           slant, setWidth, addStyle, spacing, charSetName, charSetInfo))
            break;
        }
        if (scaler <= maxScaler) {
          continue; /* |NodeAddSize| returned an error in the loop above... */
        }
      }
      else
      {
        if (!NodeAddSize(stretch, pixels, points, 1.0f, resX, resY, name, familyName, weightName, 
                         slant, setWidth, addStyle, spacing, charSetName, charSetInfo))
          continue;     
      }
    }
  }
  XFreeFontNames(list);

#ifdef DEBUG_DUMP_TREE
  DumpTree();
#endif
}

static nsresult
GetAllFontNames(void)
{
  if (!gGlobalList) {
    // This may well expand further (families * sizes * styles?), but it's
    // only created once.
    gGlobalList = new nsFontNodeArray;
    if (!gGlobalList) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    /* Using "-*" instead of the full-qualified "-*-*-*-*-*-*-*-*-*-*-*-*-*-*"
     * because it's faster and "smarter" - see bug 34242 for details. */
    GetFontNames("-*", PR_FALSE, PR_FALSE, gGlobalList);
  }

  return NS_OK;
}

static nsFontFamily*
FindFamily(nsCString* aName)
{
  nsCStringKey key(*aName);
  nsFontFamily* family = (nsFontFamily*) gFamilies->Get(&key);
  if (!family) {
    family = new nsFontFamily();
    if (family) {
      char pattern[256];
      PR_snprintf(pattern, sizeof(pattern), "-*-%s-*-*-*-*-*-*-*-*-*-*-*-*",
        aName->get());
      GetFontNames(pattern, PR_TRUE, gForceOutlineScaledFonts, &family->mNodes);
      gFamilies->Put(&key, family);
    }
  }

  return family;
}

nsresult
nsFontMetricsGTK::FamilyExists(nsIDeviceContext *aDevice, const nsString& aName)
{
  if (!gInitialized) {
    nsresult res = InitGlobals(aDevice);
    if (NS_FAILED(res))
      return res;
  }

  if (!IsASCIIFontName(aName)) {
    return NS_ERROR_FAILURE;
  }

  nsCAutoString name;
  name.AssignWithConversion(aName.get());
  ToLowerCase(name);
  nsFontFamily* family = FindFamily(&name);
  if (family && family->mNodes.Count()) {
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

PRUint32
nsFontMetricsGTK::GetHints(void)
{
  PRUint32 result = 0;

  /* We can't enable fast text measuring (yet) on platforms which
   * force natural alignment of datatypes (see
   * http://bugzilla.mozilla.org/show_bug.cgi?id=36146#c46) ... ;-(
   */

#ifndef CPU_DOES_NOT_REQUIRE_NATURAL_ALIGNMENT
#if defined(__i386)
#define CPU_DOES_NOT_REQUIRE_NATURAL_ALIGNMENT 1
#endif /* __i386 */
#endif /* !CPU_DOES_NOT_REQUIRE_NATURAL_ALIGNMENT */

  static PRBool enable_fast_measure;
  static PRBool getenv_done = PR_FALSE;
    
  /* Check for the env vars "MOZILLA_GFX_ENABLE_FAST_MEASURE" and
   * "MOZILLA_GFX_DISABLE_FAST_MEASURE" to enable/disable fast text
   * measuring (for debugging the feature and doing regression tests).
   * This code will be removed one all issues around this new feature
   * have been fixed. */
  if (!getenv_done) {
#ifdef CPU_DOES_NOT_REQUIRE_NATURAL_ALIGNMENT
    enable_fast_measure = PR_TRUE;
#else
    enable_fast_measure = PR_FALSE;
#endif /* CPU_DOES_NOT_REQUIRE_NATURAL_ALIGNMENT */

    if (PR_GetEnv("MOZILLA_GFX_ENABLE_FAST_MEASURE"))
      enable_fast_measure = PR_TRUE;

    if (PR_GetEnv("MOZILLA_GFX_DISABLE_FAST_MEASURE"))
      enable_fast_measure = PR_FALSE;
        
    getenv_done = PR_TRUE;
  }

  if (enable_fast_measure) {
    // We have GetTextDimensions()
    result |= NS_RENDERING_HINT_FAST_MEASURE;
  }

  return result;
}

//
// convert a FFRE (Foundry-Family-Registry-Encoding) To XLFD Pattern
//
static void
FFREToXLFDPattern(nsACString &aFFREName, nsACString &oPattern)
{
  PRInt32 charsetHyphen;

  oPattern.Append("-");
  oPattern.Append(aFFREName);
  /* Search for the 3rd appearance of '-' */
  charsetHyphen = oPattern.FindChar('-');
  charsetHyphen = oPattern.FindChar('-', charsetHyphen + 1);
  charsetHyphen = oPattern.FindChar('-', charsetHyphen + 1);
  oPattern.Insert("-*-*-*-*-*-*-*-*-*-*", charsetHyphen);
}

//
// substitute the charset in a FFRE (Foundry-Family-Registry-Encoding)
//
static void
FFRESubstituteCharset(nsACString &aFFREName,
                      const char *aReplacementCharset)
{
  PRInt32 charsetHyphen = aFFREName.FindChar('-');
  charsetHyphen = aFFREName.FindChar('-', charsetHyphen + 1);
  aFFREName.Truncate(charsetHyphen+1);
  aFFREName.Append(aReplacementCharset);
}

//
// substitute the encoding in a FFRE (Foundry-Family-Registry-Encoding)
//
static void
FFRESubstituteEncoding(nsACString &aFFREName,
                       const char *aReplacementEncoding)
{
  PRInt32 encodingHyphen;
  /* Search for the 3rd apperance of '-' */
  encodingHyphen = aFFREName.FindChar('-');
  encodingHyphen = aFFREName.FindChar('-', encodingHyphen + 1);
  encodingHyphen = aFFREName.FindChar('-', encodingHyphen + 1);
  aFFREName.Truncate(encodingHyphen+1);
  aFFREName.Append(aReplacementEncoding);
}

nsFontGTK*
nsFontMetricsGTK::TryNodes(nsACString &aFFREName, PRUint32 aChar)
{
  const nsPromiseFlatCString& FFREName = PromiseFlatCString(aFFREName);

  FIND_FONT_PRINTF(("        TryNodes aFFREName = %s", FFREName.get()));
  nsCStringKey key(FFREName);
  PRBool anyFoundry = (FFREName.First() == '*');
  nsFontNodeArray* nodes = (nsFontNodeArray*) gCachedFFRESearches->Get(&key);
  if (!nodes) {
    nsCAutoString pattern;
    FFREToXLFDPattern(aFFREName, pattern);
    nodes = new nsFontNodeArray;
    if (!nodes)
      return nsnull;
    GetFontNames(pattern.get(), anyFoundry, gForceOutlineScaledFonts, nodes);
    gCachedFFRESearches->Put(&key, nodes);
  }
  int i, cnt = nodes->Count();
  for (i=0; i<cnt; i++) {
    nsFontNode* node = nodes->GetElement(i);
    nsFontGTK * font;
    font = SearchNode(node, aChar);
    if (font && font->SupportsChar(aChar))
      return font;
  }
  return nsnull;
}

nsFontGTK*
nsFontMetricsGTK::TryNode(nsCString* aName, PRUint32 aChar)
{
  FIND_FONT_PRINTF(("        TryNode aName = %s", (*aName).get()));
  //
  // check the specified font (foundry-family-registry-encoding)
  //
  if (aName->IsEmpty()) {
    return nsnull;
  }
  nsFontGTK* font;
 
  nsCStringKey key(*aName);
  nsFontNode* node = (nsFontNode*) gFFRENodes->Get(&key);
  if (!node) {
    nsCAutoString pattern;
    FFREToXLFDPattern(*aName, pattern);
    nsFontNodeArray nodes;
    GetFontNames(pattern.get(), PR_FALSE, gForceOutlineScaledFonts, &nodes);
    // no need to call gFFRENodes->Put() since GetFontNames already did
    if (nodes.Count() > 0) {
      // This assertion is not spurious; when searching for an FFRE
      // like -*-courier-iso8859-1 TryNodes should be called not TryNode
      NS_ASSERTION((nodes.Count() == 1), "unexpected number of nodes");
      node = nodes.GetElement(0);
    }
    else {
      // add a dummy node to the hash table to avoid calling XListFonts again
      node = new nsFontNode();
      if (!node) {
        return nsnull;
      }
      gFFRENodes->Put(&key, node);
      node->mDummy = 1;
    }
  }

  if (node) {
    font = SearchNode(node, aChar);
    if (font && font->SupportsChar(aChar))
      return font;
  }

  //
  // do not check related sub-planes for UserDefined
  //
  if (mIsUserDefined) {
    return nsnull;
  }
  //
  // check related sub-planes (wild-card the encoding)
  //
  nsCAutoString ffreName(*aName);
  FFRESubstituteEncoding(ffreName, "*");
  FIND_FONT_PRINTF(("        TrySubplane: wild-card the encoding"));
  font = TryNodes(ffreName, aChar);
  if (font) {
    NS_ASSERTION(font->SupportsChar(aChar), "font supposed to support this char");
    return font;
  }
  return nsnull;
}

nsFontGTK* 
nsFontMetricsGTK::TryLangGroup(nsIAtom* aLangGroup, nsCString* aName, PRUint32 aChar)
{
  //
  // for this family check related registry-encoding (for the language)
  //
  FIND_FONT_PRINTF(("      TryLangGroup lang group = %s, aName = %s", 
                            atomToName(aLangGroup), (*aName).get()));
  if (aName->IsEmpty()) {
    return nsnull;
  }
  nsFontGTK* font = FindLangGroupFont(aLangGroup, aChar, aName);
  return font;
}

nsFontGTK*
nsFontMetricsGTK::TryFamily(nsCString* aName, PRUint32 aChar)
{
  //
  // check the patterh "*-familyname-registry-encoding" for language
  //
  nsFontFamily* family = FindFamily(aName);
  if (family) {
    // try family name of language group first
    nsCAutoString FFREName("*-");
    FFREName.Append(*aName);
    FFREName.Append("-*-*");
    FIND_FONT_PRINTF(("        TryFamily %s with lang group = %s", (*aName).get(),
                                                         atomToName(mLangGroup)));
    nsFontGTK* font = TryLangGroup(mLangGroup, &FFREName, aChar);
    if(font) {
      return font;
    }

    // then try family name regardless of language group
    nsFontNodeArray* nodes = &family->mNodes;
    PRInt32 n = nodes->Count();
    for (PRInt32 i = 0; i < n; i++) {
      FIND_FONT_PRINTF(("        TryFamily %s", nodes->GetElement(i)->mName.get()));
      nsFontGTK* font = SearchNode(nodes->GetElement(i), aChar);
      if (font && font->SupportsChar(aChar)) {
        return font;
      }
    }
  }

  return nsnull;
}

nsFontGTK*
nsFontMetricsGTK::TryAliases(nsCString* aAlias, PRUint32 aChar)
{
  nsCStringKey key(*aAlias);
  char* name = (char*) gAliases->Get(&key);
  if (name) {
    nsCAutoString str(name);
    return TryFamily(&str, aChar);
  }

  return nsnull;
}

nsFontGTK*
nsFontMetricsGTK::FindUserDefinedFont(PRUint32 aChar)
{
  if (mIsUserDefined) {
    FIND_FONT_PRINTF(("        FindUserDefinedFont"));
    nsFontGTK* font = TryNode(&mUserDefined, aChar);
    mIsUserDefined = PR_FALSE;
    if (font) {
      NS_ASSERTION(font->SupportsChar(aChar), "font supposed to support this char");
      return font;
    }
  }

  return nsnull;
}

nsFontGTK*
nsFontMetricsGTK::FindStyleSheetSpecificFont(PRUint32 aChar)
{
  FIND_FONT_PRINTF(("    FindStyleSheetSpecificFont"));
  while (mFontsIndex < mFonts.Count()) {
    if (mFontIsGeneric[mFontsIndex]) {
      return nsnull;
    }
    nsCString* familyName = mFonts.CStringAt(mFontsIndex);

    /*
     * count hyphens
     * XXX It might be good to try to pre-cache this information instead
     * XXX of recalculating it on every font access!
     */
    const char* str = familyName->get();
    FIND_FONT_PRINTF(("        familyName = %s", str));
    PRUint32 len = familyName->Length();
    int hyphens = 0;
    for (PRUint32 i = 0; i < len; i++) {
      if (str[i] == '-') {
        hyphens++;
      }
    }

    /*
     * if there are 3 hyphens, the name is in FFRE form
     * (foundry-family-registry-encoding)
     * ie: something like this:
     *
     *   adobe-times-iso8859-1
     *
     * otherwise it is something like
     *
     *   times new roman
     */
    nsFontGTK* font;
    if (hyphens == 3) {
      font = TryNode(familyName, aChar);
      if (font) {
        NS_ASSERTION(font->SupportsChar(aChar), "font supposed to support this char");
        return font;
      }
    }
    else {
      font = TryFamily(familyName, aChar);
      if (font) {
        NS_ASSERTION(font->SupportsChar(aChar), "font supposed to support this char");
        return font;
      }
      font = TryAliases(familyName, aChar);
      if (font) {
        NS_ASSERTION(font->SupportsChar(aChar), "font supposed to support this char");
        return font;
      }
    }
    // bug 42917: increment only after all of the above fails
    mFontsIndex++;
  }

  return nsnull;
}

static void
PrefEnumCallback(const char* aName, void* aClosure)
{
  nsFontSearch* s = (nsFontSearch*) aClosure;
  if (s->mFont) {
    NS_ASSERTION(s->mFont->SupportsChar(s->mChar), "font supposed to support this char");
    return;
  }
  nsXPIDLCString value;
  gPref->CopyCharPref(aName, getter_Copies(value));
  nsCAutoString name;
  if (value.get()) {
    name = value;
    FIND_FONT_PRINTF(("       PrefEnumCallback"));
    s->mFont = s->mMetrics->TryNode(&name, s->mChar);
    if (s->mFont) {
      NS_ASSERTION(s->mFont->SupportsChar(s->mChar), "font supposed to support this char");
      return;
    }
    s->mFont = s->mMetrics->TryLangGroup(s->mMetrics->mLangGroup, &name, s->mChar);
    if (s->mFont) {
      NS_ASSERTION(s->mFont->SupportsChar(s->mChar), "font supposed to support this char");
      return;
    }
  }
  gPref->CopyDefaultCharPref(aName, getter_Copies(value));
  if (value.get() && (!name.Equals(value))) {
    name = value;
    FIND_FONT_PRINTF(("       PrefEnumCallback:default"));
    s->mFont = s->mMetrics->TryNode(&name, s->mChar);
    if (s->mFont) {
      NS_ASSERTION(s->mFont->SupportsChar(s->mChar), "font supposed to support this char");
      return;
    }
    s->mFont = s->mMetrics->TryLangGroup(s->mMetrics->mLangGroup, &name, s->mChar);
    NS_ASSERTION(s->mFont ? s->mFont->SupportsChar(s->mChar) : 1, "font supposed to support this char");
  }
}

nsFontGTK*
nsFontMetricsGTK::FindStyleSheetGenericFont(PRUint32 aChar)
{
  FIND_FONT_PRINTF(("    FindStyleSheetGenericFont"));
  nsFontGTK* font;

  if (mTriedAllGenerics) {
    return nsnull;
  }

  //
  // find font based on document's lang group
  //
  font = FindLangGroupPrefFont(mLangGroup, aChar);
  if (font) {
    NS_ASSERTION(font->SupportsChar(aChar), "font supposed to support this char");
    return font;
  }

  //
  // Asian smart quote glyphs are much too large for western
  // documents so if this is a single byte document add a
  // special "font" to tranliterate those chars rather than
  // possibly find them in double byte fonts
  //
  // (risk management: since we are close to a ship point we have a 
  //  control (gAllowDoubleByteSpecialChars) to disable this new feature)
  //
if (gAllowDoubleByteSpecialChars) {
  if (!mDocConverterType) {
    if (mLoadedFontsCount) {
      FIND_FONT_PRINTF(("just use the 1st converter type"));
      nsFontGTK* first_font = mLoadedFonts[0];
      if (first_font->mCharSetInfo) {
        mDocConverterType = first_font->mCharSetInfo->Convert;
        if (mDocConverterType == SingleByteConvert ) {
          FIND_FONT_PRINTF(("single byte converter for %s", atomToName(mLangGroup)));
        }
        else {
          FIND_FONT_PRINTF(("double byte converter for %s", atomToName(mLangGroup)));
        }
      }
    }
    if (!mDocConverterType) {
      mDocConverterType = SingleByteConvert;
    }
    if (mDocConverterType == SingleByteConvert) {
      // before we put in the transliterator to disable double byte special chars
      // add the x-western font before the early transliterator
      // to get the EURO sign (hack)

      nsFontGTK* western_font = nsnull;
      if (mLangGroup != gWesternLocale)
        western_font = FindLangGroupPrefFont(gWesternLocale, aChar);

      // add the symbol font before the early transliterator
      // to get the bullet (hack)
      nsCAutoString symbol_ffre("*-symbol-adobe-fontspecific");
      nsFontGTK* symbol_font = TryNodes(symbol_ffre, 0x0030);

      // Add the Adobe Euro fonts before the early transliterator
      nsCAutoString euro_ffre("*-euro*-adobe-fontspecific");
      nsFontGTK* euro_font = TryNodes(euro_ffre, 0x20AC);

      // add the early transliterator
      // to avoid getting Japanese "special chars" such as smart
      // since they are very oversized compared to western fonts
      nsFontGTK* sub_font = FindSubstituteFont(aChar);
      NS_ASSERTION(sub_font, "failed to get a special chars substitute font");
      if (sub_font) {
        sub_font->mCCMap = gDoubleByteSpecialCharsCCMap;
        AddToLoadedFontsList(sub_font);
      }
      if (western_font && CCMAP_HAS_CHAR_EXT(western_font->mCCMap, aChar)) {
        return western_font;
      }
      else if (symbol_font && CCMAP_HAS_CHAR_EXT(symbol_font->mCCMap, aChar)) {
        return symbol_font;
      }
      else if (euro_font && CCMAP_HAS_CHAR_EXT(euro_font->mCCMap, aChar)) {
        return euro_font;
      }
      else if (sub_font && CCMAP_HAS_CHAR_EXT(sub_font->mCCMap, aChar)) {
        FIND_FONT_PRINTF(("      transliterate special chars for single byte docs"));
        return sub_font;
      }
    }
  }
}

  //
  // find font based on user's locale's lang group
  // if different from documents locale
  if (gUsersLocale != mLangGroup) {
    FIND_FONT_PRINTF(("      find font based on user's locale's lang group"));
    font = FindLangGroupPrefFont(gUsersLocale, aChar);
    if (font) {
      NS_ASSERTION(font->SupportsChar(aChar), "font supposed to support this char");
      return font;
    }
  }

  //
  // Search all font prefs for generic
  //
  nsCAutoString prefix("font.name.");
  prefix.Append(*mGeneric);
  nsFontSearch search = { this, aChar, nsnull };
  FIND_FONT_PRINTF(("      Search all font prefs for generic"));
  gPref->EnumerateChildren(prefix.get(), PrefEnumCallback, &search);
  if (search.mFont) {
    NS_ASSERTION(search.mFont->SupportsChar(aChar), "font supposed to support this char");
    return search.mFont;
  }

  //
  // Search all font prefs
  //
  // find based on all prefs (no generic part (eg: sans-serif))
  nsCAutoString allPrefs("font.name.");
  search.mFont = nsnull;
  FIND_FONT_PRINTF(("      Search all font prefs"));
  gPref->EnumerateChildren(allPrefs.get(), PrefEnumCallback, &search);
  if (search.mFont) {
    NS_ASSERTION(search.mFont->SupportsChar(aChar), "font supposed to support this char");
    return search.mFont;
  }

  mTriedAllGenerics = 1;
  return nsnull;
}

nsFontGTK*
nsFontMetricsGTK::FindAnyFont(PRUint32 aChar)
{
  FIND_FONT_PRINTF(("    FindAnyFont"));
  // XXX If we get to this point, that means that we have exhausted all the
  // families in the lists. Maybe we should try a list of fonts that are
  // specific to the vendor of the X server here. Because XListFonts for the
  // whole list is very expensive on some Unixes.

  /*
   * Try all the fonts on the system.
   */
  nsresult res = GetAllFontNames();
  if (NS_FAILED(res)) {
    return nsnull;
  }

  PRInt32 n = gGlobalList->Count();
  for (PRInt32 i = 0; i < n; i++) {
    nsFontGTK* font = SearchNode(gGlobalList->GetElement(i), aChar);
    if (font && font->SupportsChar(aChar)) {
      // XXX We should probably write this family name out to disk, so that
      // we can use it next time. I.e. prefs file or something.
      return font;
    }
  }

  // future work:
  // to properly support the substitute font we
  // need to indicate here that all fonts have been tried
  return nsnull;
}

nsFontGTK*
nsFontMetricsGTK::FindSubstituteFont(PRUint32 aChar)
{
  if (!mSubstituteFont) {
    for (int i = 0; i < mLoadedFontsCount; i++) {
      if (CCMAP_HAS_CHAR_EXT(mLoadedFonts[i]->mCCMap, 'a')) {
        mSubstituteFont = new nsFontGTKSubstitute(mLoadedFonts[i]);
        break;
      }
    }
    // Currently the substitute font does not have a glyph map.
    // This means that even if we have already checked all fonts
    // for a particular character the mLoadedFonts will not know it.
    // Thus we reparse *all* font glyph maps every time we see
    // a character that ends up using a substitute font.
    // future work:
    // create an empty mCCMap and every time we determine a
    // character will get its "glyph" from the substitute font
    // mark that character in the mCCMap.
  }
  // mark the mCCMap to indicate that this character has a "glyph"

  // If we know that mLoadedFonts has every font's glyph map loaded
  // then we can now set all the bit in the substitute font's glyph map
  // and thus direct all umapped characters to the substitute
  // font (without the font search).
  // if tried all glyphs {
  //   create a substitute font with all bits set
  //   set all bits in mCCMap
  // }

  return mSubstituteFont;
}

//
// find font based on lang group
//

nsFontGTK* 
nsFontMetricsGTK::FindLangGroupPrefFont(nsIAtom* aLangGroup, PRUint32 aChar)
{ 
  nsFontGTK* font;
  //
  // get the font specified in prefs
  //
  nsCAutoString prefix("font.name."); 
  prefix.Append(*mGeneric); 
  if (aLangGroup) { 
    // check user set pref
    nsCAutoString pref = prefix;
    pref.Append(char('.'));
    const char* langGroup = nsnull;
    aLangGroup->GetUTF8String(&langGroup);
    pref.Append(langGroup);
    nsXPIDLCString value;
    gPref->CopyCharPref(pref.get(), getter_Copies(value));
    nsCAutoString str;
    nsCAutoString str_user;
    if (value.get()) {
      str = value.get();
      str_user = value.get();
      FIND_FONT_PRINTF(("      user pref %s = %s", pref.get(), str.get()));
      font = TryNode(&str, aChar);
      if (font) {
        NS_ASSERTION(font->SupportsChar(aChar), "font supposed to support this char");
        return font;
      }
      font = TryLangGroup(aLangGroup, &str, aChar);
      if (font) {
        NS_ASSERTION(font->SupportsChar(aChar), "font supposed to support this char");
        return font;
      }
    }
    // check factory set pref
    gPref->CopyDefaultCharPref(pref.get(), getter_Copies(value));
    if (value.get()) {
      str = value.get();
      // check if we already tried this name
      if (str != str_user) {
        FIND_FONT_PRINTF(("      default pref %s = %s", pref.get(), str.get()));
        font = TryNode(&str, aChar);
        if (font) {
          NS_ASSERTION(font->SupportsChar(aChar), "font supposed to support this char");
          return font;
        }
        font = TryLangGroup(aLangGroup, &str, aChar);
        if (font) {
          NS_ASSERTION(font->SupportsChar(aChar), "font supposed to support this char");
          return font;
        }
      }
    }
  }

  //
  // find any style font based on lang group
  //
  FIND_FONT_PRINTF(("      find font based on lang group"));
  font = FindLangGroupFont(aLangGroup, aChar, nsnull);
  if (font) {
    NS_ASSERTION(font->SupportsChar(aChar), "font supposed to support this char");
    return font;
  }

  return nsnull;
}

nsFontGTK*
nsFontMetricsGTK::FindLangGroupFont(nsIAtom* aLangGroup, PRUint32 aChar, nsCString *aName)
{
  nsFontGTK* font;

  FIND_FONT_PRINTF(("      lang group = %s", atomToName(aLangGroup)));

  //  scan gCharSetMap for encodings with matching lang groups
  nsFontCharSetMap* charSetMap;
  for (charSetMap=gCharSetMap; charSetMap->mName; charSetMap++) {
    nsFontLangGroup* fontLangGroup = charSetMap->mFontLangGroup;

    if ((!fontLangGroup) || (!fontLangGroup->mFontLangGroupName)) {
      continue;
    }

    if (!charSetMap->mInfo->mLangGroup) {
      SetCharsetLangGroup(charSetMap->mInfo);
    }

    if (!fontLangGroup->mFontLangGroupAtom) {
      SetFontLangGroupInfo(charSetMap);
    }

    // if font's langGroup is different from requested langGroup, continue.
    // An exception is that font's langGroup ZHTWHK is regarded as matching
    // both ZHTW and ZHHK (Freetype2 and Solaris).
    if ((aLangGroup != fontLangGroup->mFontLangGroupAtom) &&
        (aLangGroup != charSetMap->mInfo->mLangGroup) &&
        (fontLangGroup->mFontLangGroupAtom != gZHTWHK || 
        (aLangGroup != gZHHK && aLangGroup != gZHTW))) {
      continue;
    }
    // look for a font with this charset (registry-encoding) & char
    //
    nsCAutoString ffreName;
    if(aName) {
      // if aName was specified so call TryNode() not TryNodes()
      ffreName.Assign(*aName);
      FFRESubstituteCharset(ffreName, charSetMap->mName); 
      FIND_FONT_PRINTF(("      %s ffre = %s", charSetMap->mName, ffreName.get()));
      if(aName->First() == '*') {
         // called from TryFamily()
         font = TryNodes(ffreName, aChar);
      } else {
         font = TryNode(&ffreName, aChar);
      }
      NS_ASSERTION(font ? font->SupportsChar(aChar) : 1, "font supposed to support this char");
    } else {
      // no name was specified so call TryNodes() for this charset
      ffreName.Assign("*-*-*-*");
      FFRESubstituteCharset(ffreName, charSetMap->mName); 
      FIND_FONT_PRINTF(("      %s ffre = %s", charSetMap->mName, ffreName.get()));
      font = TryNodes(ffreName, aChar);
      NS_ASSERTION(font ? font->SupportsChar(aChar) : 1, "font supposed to support this char");
    }
    if (font) {
      NS_ASSERTION(font->SupportsChar(aChar), "font supposed to support this char");
      return font;
    }
  }

  return nsnull;
}

/*
 * First we try to load the user-defined font, if the user-defined charset
 * has been selected in the menu.
 *
 * Next, we try the fonts listed in the font-family property (FindStyleSheetSpecificFont).
 *
 * Next, we try any CSS generic font encountered in the font-family list and
 * all of the fonts specified by the user for the generic (FindStyleSheetGenericFont).
 *
 * Next, we try all of the fonts on the system (FindAnyFont). This is
 * expensive on some Unixes.
 *
 * Finally, we try to create a substitute font that offers substitute glyphs
 * for the characters (FindSubstituteFont).
 */
nsFontGTK*
nsFontMetricsGTK::FindFont(PRUint32 aChar)
{
  FIND_FONT_PRINTF(("\nFindFont(%c/0x%04x)", aChar, aChar));

  // If this is is the 'unknown' char (ie: converter could not 
  // convert it) there is no sense in searching any further for 
  // a font. Just returing mWesternFont
  if (aChar == UCS2_NOMAPPING) {
    FIND_FONT_PRINTF(("      ignore the 'UCS2_NOMAPPING' character, return mWesternFont"));
    return mWesternFont;
  }

  nsFontGTK* font = FindUserDefinedFont(aChar);
  if (!font) {
    font = FindStyleSheetSpecificFont(aChar);
    if (!font) {
      font = FindStyleSheetGenericFont(aChar);
      if (!font) {
        font = FindAnyFont(aChar);
        if (!font) {
          font = FindSubstituteFont(aChar);
        }
      }
    }
  }

#ifdef NS_FONT_DEBUG_CALL_TRACE
  if (gFontDebug & NS_FONT_DEBUG_CALL_TRACE) {
    printf("FindFont(%04X)[", aChar);
    for (PRInt32 i = 0; i < mFonts.Count(); i++) {
      printf("%s, ", mFonts.CStringAt(i)->get());
    }
    printf("]\nreturns ");
    if (font) {
      printf("%s\n", font->mName ? font->mName : "(substitute)");
    }
    else {
      printf("NULL\n");
    }
  }
#endif

  return font;
}


// The Font Enumerator

nsFontEnumeratorGTK::nsFontEnumeratorGTK()
{
}

NS_IMPL_ISUPPORTS1(nsFontEnumeratorGTK, nsIFontEnumerator)

typedef struct EnumerateNodeInfo
{
  PRUnichar** mArray;
  int         mIndex;
  nsIAtom*    mLangGroup;
} EnumerateNodeInfo;

static PRIntn
EnumerateNode(void* aElement, void* aData)
{
  nsFontNode* node = (nsFontNode*) aElement;
  EnumerateNodeInfo* info = (EnumerateNodeInfo*) aData;
  if (info->mLangGroup != gUserDefined) {
    if (node->mCharSetInfo == &Unknown) {
      return PR_TRUE; // continue
    }
    else if (info->mLangGroup != gUnicode) {
      // if font's langGroup is different from requested langGroup, continue.
      // An exception is that font's langGroup ZHTWHK is regarded as matching
      // both ZHTW and ZHHK (Freetype2 and Solaris).
      if (node->mCharSetInfo->mLangGroup != info->mLangGroup &&
         (node->mCharSetInfo->mLangGroup != gZHTWHK || 
         (info->mLangGroup != gZHHK && info->mLangGroup != gZHTW))) {
        return PR_TRUE; // continue
      }
    }
    // else {
    //   if (lang == add-style-field) {
    //     consider it part of the lang group
    //   }
    //   else if (a Unicode font reports its lang group) {
    //     consider it part of the lang group
    //   }
    //   else if (lang's ranges in list of ranges) {
    //     consider it part of the lang group
    //     // Note: at present we have no way to do this test but we 
    //     // could in the future and this would be the place to enable
    //     // to make the font show up in the preferences dialog
    //   }
    // }

  }
  PRUnichar** array = info->mArray;
  int j = info->mIndex;
  PRUnichar* str = ToNewUnicode(node->mName);
  if (!str) {
    for (j = j - 1; j >= 0; j--) {
      nsMemory::Free(array[j]);
    }
    info->mIndex = 0;
    return PR_FALSE; // stop
  }
  array[j] = str;
  info->mIndex++;

  return PR_TRUE; // continue
}

PR_BEGIN_EXTERN_C
static int
CompareFontNames(const void* aArg1, const void* aArg2, void* aClosure)
{
  const PRUnichar* str1 = *((const PRUnichar**) aArg1);
  const PRUnichar* str2 = *((const PRUnichar**) aArg2);

  // XXX add nsICollation stuff

  return nsCRT::strcmp(str1, str2);
}
PR_END_EXTERN_C

static nsresult
EnumFonts(nsIAtom* aLangGroup, const char* aGeneric, PRUint32* aCount,
  PRUnichar*** aResult)
{
  nsresult res = GetAllFontNames();
  if (NS_FAILED(res)) {
    return res;
  }

  PRUnichar** array =
    (PRUnichar**) nsMemory::Alloc(gGlobalList->Count() * sizeof(PRUnichar*));
  if (!array) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  EnumerateNodeInfo info = { array, 0, aLangGroup };
  if (!gGlobalList->EnumerateForwards(EnumerateNode, &info)) {
    nsMemory::Free(array);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_QuickSort(array, info.mIndex, sizeof(PRUnichar*), CompareFontNames,
               nsnull);

  *aCount = info.mIndex;
  if (*aCount) {
    *aResult = array;
  }
  else {
    nsMemory::Free(array);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsFontEnumeratorGTK::EnumerateAllFonts(PRUint32* aCount, PRUnichar*** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = nsnull;
  NS_ENSURE_ARG_POINTER(aCount);
  *aCount = 0;

  return EnumFonts(nsnull, nsnull, aCount, aResult);
}

NS_IMETHODIMP
nsFontEnumeratorGTK::EnumerateFonts(const char* aLangGroup,
  const char* aGeneric, PRUint32* aCount, PRUnichar*** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = nsnull;
  NS_ENSURE_ARG_POINTER(aCount);
  *aCount = 0;

  // aLangGroup=null or ""  means any (i.e., don't care)
  // aGeneric=null or ""  means any (i.e, don't care)
  nsCOMPtr<nsIAtom> langGroup;
  if (aLangGroup && *aLangGroup)
    langGroup = do_GetAtom(aLangGroup);
  const char* generic = nsnull;
  if (aGeneric && *aGeneric)
    generic = aGeneric;

  // XXX still need to implement aLangGroup and aGeneric
  return EnumFonts(langGroup, generic, aCount, aResult);
}

NS_IMETHODIMP
nsFontEnumeratorGTK::HaveFontFor(const char* aLangGroup, PRBool* aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = PR_FALSE;
  NS_ENSURE_ARG_POINTER(aLangGroup);

  *aResult = PR_TRUE; // always return true for now.
  // Finish me - ftang
  return NS_OK;
}

NS_IMETHODIMP
nsFontEnumeratorGTK::GetDefaultFont(const char *aLangGroup, 
  const char *aGeneric, PRUnichar **aResult)
{
  // aLangGroup=null or ""  means any (i.e., don't care)
  // aGeneric=null or ""  means any (i.e, don't care)

  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsFontEnumeratorGTK::UpdateFontList(PRBool *updateFontList)
{
  *updateFontList = PR_FALSE; // always return false for now
  return NS_OK;
}

nsFontCharSetMap *
GetCharSetMap(const char *aCharSetName)
{
    nsCStringKey charSetKey(aCharSetName);
    nsFontCharSetMap* charSetMap =
      (nsFontCharSetMap*) gCharSetMaps->Get(&charSetKey);
    if (!charSetMap)
      charSetMap = gNoneCharSetMap;
  return charSetMap;
}

void
CharSetNameToCodeRangeBits(const char *aCharset,
                           PRUint32 *aCodeRange1, PRUint32 *aCodeRange2)
{
  nsFontCharSetMap *charSetMap = GetCharSetMap(aCharset);
  nsFontCharSetInfo* charSetInfo = charSetMap->mInfo;

  *aCodeRange1 = charSetInfo->mCodeRange1Bits;
  *aCodeRange2 = charSetInfo->mCodeRange2Bits;
}

