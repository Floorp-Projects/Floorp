/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "nsICharsetConverterManager.h"
#include "nsICharsetConverterManager2.h"
#include "nsICharRepresentable.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsFontMetricsWin.h"
#include "nsQuickSort.h"
#include "nsTextFormatter.h"
#include "nsIFontPackageProxy.h"
#include "nsIPersistentProperties2.h"
#include "nsNetUtil.h"
#include "nsIURI.h"
#include "prmem.h"
#include "plhash.h"
#include "prprf.h"

#define NS_FONT_TYPE_UNKNOWN          -1
#define NS_FONT_TYPE_UNICODE           0
#define NS_FONT_TYPE_NON_UNICODE       1


#define NOT_SETUP 0x33
static PRBool gIsWIN95 = NOT_SETUP;

#undef USER_DEFINED
#define USER_DEFINED "x-user-def"

// Note: the replacement char must be a char that can be found in common unicode fonts
#define NS_REPLACEMENT_CHAR  PRUnichar(0x003F) // question mark
//#define NS_REPLACEMENT_CHAR  PRUnichar(0xFFFD) // Unicode's replacement char
//#define NS_REPLACEMENT_CHAR  PRUnichar(0x2370) // Boxed question mark
//#define NS_REPLACEMENT_CHAR  PRUnichar(0x00BF) // Inverted question mark
//#define NS_REPLACEMENT_CHAR  PRUnichar(0x25AD) // White rectangle

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

struct nsCharSetInfo
{
  char*    mName;
  PRUint16 mCodePage;
  char*    mLangGroup;
  void     (*GenerateMap)(nsCharSetInfo* aSelf);
  PRUint32* mMap;
};

static void GenerateDefault(nsCharSetInfo* aSelf);
static void GenerateSingleByte(nsCharSetInfo* aSelf);
static void GenerateMultiByte(nsCharSetInfo* aSelf);

static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

static int gInitializedGlobalFonts = 0;
PRUint32* nsFontMetricsWin::gEmptyMap = nsnull;

nsGlobalFont* nsFontMetricsWin::gGlobalFonts = nsnull;
static int gGlobalFontsAlloc = 0;
int nsFontMetricsWin::gGlobalFontsCount = 0;

static int gInitializedFontMaps = 0;
PLHashTable* nsFontMetricsWin::gFontMaps = nsnull;

static int gInitializedFamilyNames = 0;
PLHashTable* nsFontMetricsWin::gFamilyNames = nsnull;

//-- Font weight
static int gInitializedFontWeights = 0;
PLHashTable* nsFontMetricsWin::gFontWeights = nsnull;

#define NS_MAX_FONT_WEIGHT 900
#define NS_MIN_FONT_WEIGHT 100

#undef CHAR_BUFFER_SIZE
#define CHAR_BUFFER_SIZE 1024

static nsIPersistentProperties* gFontEncodingProperties = nsnull;
static nsICharsetConverterManager2* gCharSetManager = nsnull;
static nsIPref* gPref = nsnull;
static nsIUnicodeEncoder* gUserDefinedConverter = nsnull;

static nsIAtom* gUserDefined = nsnull;
static nsIAtom* gJA = nsnull;
static nsIAtom* gKO = nsnull;
static nsIAtom* gZHTW = nsnull;
static nsIAtom* gZHCN = nsnull;
static BOOL gCheckJAFont = PR_FALSE;
static BOOL gHaveJAFont = PR_FALSE;
static BOOL gHitJACase = PR_FALSE;
static BOOL gCheckKOFont = PR_FALSE;
static BOOL gHaveKOFont = PR_FALSE;
static BOOL gHitKOCase = PR_FALSE;
static BOOL gCheckZHTWFont = PR_FALSE;
static BOOL gHaveZHTWFont = PR_FALSE;
static BOOL gHitZHTWCase = PR_FALSE;
static BOOL gCheckZHCNFont = PR_FALSE;
static BOOL gHaveZHCNFont = PR_FALSE;
static BOOL gHitZHCNCase = PR_FALSE;


static int gInitialized = 0;

static PRUint32 gUserDefinedMap[2048];

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

static void
FreeGlobals(void)
{
  // XXX complete this

  gInitialized = 0;

  NS_IF_RELEASE(gFontEncodingProperties);
  NS_IF_RELEASE(gCharSetManager);
  NS_IF_RELEASE(gPref);
  NS_IF_RELEASE(gUserDefined);
  NS_IF_RELEASE(gUserDefinedConverter);
  NS_IF_RELEASE(gJA);
  NS_IF_RELEASE(gKO);
  NS_IF_RELEASE(gZHTW);
  NS_IF_RELEASE(gZHCN);

  // free CMap
  if (nsFontMetricsWin::gFontMaps) {
    PL_HashTableDestroy(nsFontMetricsWin::gFontMaps);
    if (nsFontMetricsWin::gEmptyMap) {
      PR_Free(nsFontMetricsWin::gEmptyMap);
      nsFontMetricsWin::gEmptyMap = nsnull;
    }
    nsFontMetricsWin::gFontMaps = nsnull;
    gInitializedFontMaps = 0;
  }

  if (nsFontMetricsWin::gGlobalFonts) {
    for (int i = 0; i < nsFontMetricsWin::gGlobalFontsCount; i++) {
      delete nsFontMetricsWin::gGlobalFonts[i].name;
    }
    PR_Free(nsFontMetricsWin::gGlobalFonts);
    nsFontMetricsWin::gGlobalFonts = nsnull;
    gGlobalFontsAlloc = 0;
    nsFontMetricsWin::gGlobalFontsCount = 0;
  }

  // free FamilyNames
  if (nsFontMetricsWin::gFamilyNames) {
    PL_HashTableDestroy(nsFontMetricsWin::gFamilyNames);
    nsFontMetricsWin::gFamilyNames = nsnull;
    gInitializedFamilyNames = 0;
  }

  // free Font weight
  if (nsFontMetricsWin::gFontWeights) {
    PL_HashTableDestroy(nsFontMetricsWin::gFontWeights);
    nsFontMetricsWin::gFontWeights = nsnull;
    gInitializedFontWeights = 0;
  }
}

class nsFontCleanupObserver : public nsIObserver {
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  nsFontCleanupObserver() { NS_INIT_ISUPPORTS(); }
  virtual ~nsFontCleanupObserver() {}
};
NS_IMPL_ISUPPORTS1(nsFontCleanupObserver, nsIObserver);

NS_IMETHODIMP nsFontCleanupObserver::Observe(nsISupports *aSubject, const PRUnichar *aTopic, const PRUnichar *someData)
{
  nsDependentString aTopicString(aTopic);
  if (aTopicString.Equals(NS_LITERAL_STRING(NS_XPCOM_SHUTDOWN_OBSERVER_ID))) {
    FreeGlobals();
  }
  return NS_OK;
}

static nsFontCleanupObserver *gFontCleanupObserver;

static nsresult
InitGlobals(void)
{
  // load the special encoding resolver
  nsresult rv;
  nsCOMPtr<nsIURI> uri;
  rv = NS_NewURI(getter_AddRefs(uri), "resource:/res/fonts/fontEncoding.properties");
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIInputStream> in;
    rv = NS_OpenURI(getter_AddRefs(in), uri);
    if (NS_SUCCEEDED(rv)) {
      rv = nsComponentManager::
           CreateInstance(NS_PERSISTENTPROPERTIES_CONTRACTID, nsnull,
                          NS_GET_IID(nsIPersistentProperties),
                          (void**)&gFontEncodingProperties);
      if (NS_SUCCEEDED(rv)) {
        rv = gFontEncodingProperties->Load(in);
      }
    }
  }
  if (NS_FAILED(rv)) {
    FreeGlobals();
    return rv;
  }

  nsServiceManager::GetService(kCharsetConverterManagerCID,
    NS_GET_IID(nsICharsetConverterManager2), (nsISupports**) &gCharSetManager);
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

  gUserDefined = NS_NewAtom(USER_DEFINED);
  if (!gUserDefined) {
    FreeGlobals();
    return NS_ERROR_OUT_OF_MEMORY;
  }
  gJA = NS_NewAtom("ja");
  if (!gJA) {
    FreeGlobals();
    return NS_ERROR_OUT_OF_MEMORY;
  }
  gKO = NS_NewAtom("ko");
  if (!gKO) {
    FreeGlobals();
    return NS_ERROR_OUT_OF_MEMORY;
  }
  gZHCN = NS_NewAtom("zh-CN");
  if (!gZHCN) {
    FreeGlobals();
    return NS_ERROR_OUT_OF_MEMORY;
  }
  gZHTW = NS_NewAtom("zh-TW");
  if (!gZHTW) {
    FreeGlobals();
    return NS_ERROR_OUT_OF_MEMORY;
  }

  //register an oberver to take care of cleanup
  gFontCleanupObserver = new nsFontCleanupObserver();
  NS_ASSERTION(gFontCleanupObserver, "failed to create observer");
  if (gFontCleanupObserver) {
     // register for shutdown
     nsresult rv;
     nsCOMPtr<nsIObserverService> observerService = do_GetService(NS_OBSERVERSERVICE_CONTRACTID, &rv);
     if (NS_SUCCEEDED(rv)) {
        rv = observerService->AddObserver(gFontCleanupObserver, NS_LITERAL_STRING(NS_XPCOM_SHUTDOWN_OBSERVER_ID).get());
     }    
  }
  
  gInitialized = 1;

  return NS_OK;
}

static void
CheckFontLangGroup(
     nsIAtom* lang1, nsIAtom* lang2, const char* lang3, PRBool& hit,
     PRBool& check, PRBool& have)
{
  if ((!have) && (!hit) && (lang1 == lang2)) {
     hit = PR_TRUE; // so next time we don't bother to ask
     if (!check) {
       nsFontEnumeratorWin enumerator;
       if (NS_SUCCEEDED(enumerator.HaveFontFor(lang3, &have)))
         check = PR_TRUE; // so next time we don't bother to check.
     }
     if (!have) {
       nsresult res = NS_OK;
       nsCOMPtr<nsIFontPackageProxy> proxy = do_GetService("@mozilla.org/intl/fontpackageservice;1", &res);
       NS_ASSERTION(NS_SUCCEEDED(res), "cannot get the font package proxy");
       NS_ASSERTION(proxy, "cannot get the font package proxy");
       if (proxy) {
          char fontpackageid[256];
          sprintf(fontpackageid, "lang:%s", lang3);
          res = proxy->NeedFontPackage(fontpackageid);
          NS_ASSERTION(NS_SUCCEEDED(res), "cannot notify missing font package ");
       }
     }
  }
}

nsFontMetricsWin :: nsFontMetricsWin()
{
  NS_INIT_REFCNT();
  mSpaceWidth = 0;
}
  
nsFontMetricsWin :: ~nsFontMetricsWin()
{
  // do not free mGeneric here

  if (nsnull != mFont) {
    delete mFont;
    mFont = nsnull;
  }

  mFontHandle = nsnull; // released below

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

NS_IMPL_QUERY_INTERFACE1(nsFontMetricsWin, nsIFontMetrics)
  
#else
NS_IMPL_ISUPPORTS1(nsFontMetricsWin, nsIFontMetrics)
#endif

NS_IMETHODIMP
nsFontMetricsWin :: Init(const nsFont& aFont, nsIAtom* aLangGroup,
  nsIDeviceContext *aContext)
{
  nsresult res;
  if (!gInitialized) {
    res = InitGlobals();
    NS_ASSERTION(NS_SUCCEEDED(res), "No font at all has been created");
    if (NS_FAILED(res)) {
      return res;
    }
  }
  mFont = new nsFont(aFont);
  mLangGroup = aLangGroup;

  // do special checking for the following lang group
  CheckFontLangGroup(mLangGroup, gJA,   "ja",    gHitJACase,   gCheckJAFont,   gHaveJAFont);
  CheckFontLangGroup(mLangGroup, gKO,   "ko",    gHitKOCase,   gCheckKOFont,   gHaveKOFont);
  CheckFontLangGroup(mLangGroup, gZHTW, "zh-TW", gHitZHTWCase, gCheckZHTWFont, gHaveZHTWFont);
  CheckFontLangGroup(mLangGroup, gZHCN, "zh-CN", gHitZHCNCase, gCheckZHCNFont, gHaveZHCNFont);

  mIndexOfSubstituteFont = -1;
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
  logFont->lfCharSet        = (mIsUserDefined ? ANSI_CHARSET: DEFAULT_CHARSET);
  logFont->lfOutPrecision   = OUT_TT_PRECIS;
  logFont->lfClipPrecision  = CLIP_DEFAULT_PRECIS;
  logFont->lfQuality        = DEFAULT_QUALITY;
  logFont->lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
  logFont->lfWeight = aWeight;
  logFont->lfItalic = (mFont->style & (NS_FONT_STYLE_ITALIC | NS_FONT_STYLE_OBLIQUE))
    ? TRUE : FALSE;   // XXX need better oblique support

  float app2dev, app2twip, scale;
  mDeviceContext->GetAppUnitsToDevUnits(app2dev);
  float textZoom = 1.0;
  mDeviceContext->GetTextZoom(textZoom);
  if (nsDeviceContextWin::gRound) {
    mDeviceContext->GetDevUnitsToTwips(app2twip);
    mDeviceContext->GetCanonicalPixelScale(scale);
    app2twip *= app2dev * scale;

    // This interesting bit of code rounds the font size off to the floor point
    // value. This is necessary for proper font scaling under windows.
    PRInt32 sizePoints = NSTwipsToFloorIntPoints(nscoord(mFont->size*app2twip));
    float rounded = ((float)NSIntPointsToTwips(sizePoints)) / app2twip;

    // round font size off to floor point size to be windows compatible
    // this is proper (windows) rounding
    logFont->lfHeight = - NSToIntRound(rounded * app2dev * textZoom);

    // this floor rounding is to make ours compatible with Nav 4.0
    //logFont->lfHeight = - LONG(rounded * app2dev * textZoom);
  }
  else {
    logFont->lfHeight = - NSToIntRound(mFont->size * app2dev * textZoom);
  }

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
GetGlyphIndex(PRUint16 segCount, PRUint16* endCode, PRUint16* startCode,
  PRUint16* idRangeOffset, PRUint16* idDelta, PRUint8* end, PRUint16 aChar)
{
  PRUint16 glyphIndex = 0;
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
          glyphIndex = (idDelta[i] + *p) % 65536;
        }
      }
    }
    else {
      glyphIndex = (idDelta[i] + aChar) % 65536;
    }
  }

  return glyphIndex;
}

enum nsGetNameError
{
  eGetName_OK = 0,
  eGetName_GDIError,
  eGetName_OtherError
};

static nsGetNameError
GetNAME(HDC aDC, nsString* aName)
{
  DWORD len = GetFontData(aDC, NAME, 0, nsnull, 0);
  if (len == GDI_ERROR) {
    return eGetName_GDIError;
  }
  if (!len) {
    return eGetName_OtherError;
  }
  PRUint8* buf = (PRUint8*) PR_Malloc(len);
  if (!buf) {
    return eGetName_OtherError;
  }
  DWORD newLen = GetFontData(aDC, NAME, 0, buf, len);
  if (newLen != len) {
    PR_Free(buf);
    return eGetName_OtherError;
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
    return eGetName_OtherError;
  }
  p = buf + offset + idOffset;
  idLength /= 2;
  for (i = 0; i < idLength; i++) {
    PRUnichar c = GET_SHORT(p);
    p += 2;
    aName->Append(c);
  }

  PR_Free(buf);

  return eGetName_OK;
}

static PLHashNumber
HashKey(const void* aString)
{
  const nsString* str = (const nsString*)aString;
  return (PLHashNumber)
    nsCRT::HashCode(str->get());
}

static PRIntn
CompareKeys(const void* aStr1, const void* aStr2)
{
  return nsCRT::strcmp(((const nsString*) aStr1)->get(),
    ((const nsString*) aStr2)->get()) == 0;
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


PRUint8 charSetToBit[eCharSet_COUNT] =
{
	-1,  // DEFAULT
	 0,  // ANSI_CHARSET,
	 1,  // EASTEUROPE_CHARSET,
	 2,  // RUSSIAN_CHARSET,
	 3,  // GREEK_CHARSET,
	 4,  // TURKISH_CHARSET,
	 5,  // HEBREW_CHARSET,
	 6,  // ARABIC_CHARSET,
	 7,  // BALTIC_CHARSET,
	16,  // THAI_CHARSET,
	17,  // SHIFTJIS_CHARSET,
	18,  // GB2312_CHARSET,
	19,  // HANGEUL_CHARSET,
	20,  // CHINESEBIG5_CHARSET,
	21   // JOHAB_CHARSET,
};

// Helper to determine if a font has a private encoding that we know something about
static nsresult
GetEncoding(const char* aFontName, nsString& aValue)
{
  nsAutoString name;
  name.Assign(NS_LITERAL_STRING("encoding."));
  name.AppendWithConversion(aFontName);
  name.Append(NS_LITERAL_STRING(".ttf"));
  name.StripWhitespace();
  name.ToLowerCase();
  return gFontEncodingProperties->GetStringProperty(name, aValue);
}

// This function uses the charset converter manager to get a pointer on the 
// converter for the font whose name is given. The caller holds a reference
// to the converter, and should take care of the release...
static nsresult
GetConverter(const char* aFontName, nsIUnicodeEncoder** aConverter)
{
  *aConverter = nsnull;

  nsAutoString value;
  nsresult rv = GetEncoding(aFontName, value);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIAtom> charset;
  rv = gCharSetManager->GetCharsetAtom(value.get(), getter_AddRefs(charset));
  if (NS_FAILED(rv)) return rv;

  rv = gCharSetManager->GetUnicodeEncoder(charset, aConverter);
  if (NS_FAILED(rv)) return rv;
  return (*aConverter)->SetOutputErrorBehavior((*aConverter)->kOnError_Replace, nsnull, '?');
}

// This function uses the charset converter manager to fill the map for the
// font whose name is given
static int
GetMap(const char* aFontName, PRUint32* aMap)
{
  // see if we know something about the converter of this font 
  nsCOMPtr<nsIUnicodeEncoder> converter;
  if (NS_SUCCEEDED(GetConverter(aFontName, getter_AddRefs(converter)))) {
    nsCOMPtr<nsICharRepresentable> mapper = do_QueryInterface(converter);
    if (mapper && NS_SUCCEEDED(mapper->FillInfo(aMap))) {
      return 1;
    }
  }
  return 0;
}

class nsFontInfo : public PLHashEntry
{
public:
  nsFontInfo(int aFontType, PRUint8 aCharset, PRUint32* aMap)
  {
    mType = aFontType;
    mCharset = aCharset;
    mMap = aMap;
#ifdef MOZ_MATHML
    mCMAP.mData = nsnull;  // These are initializations to characterize
    mCMAP.mLength = -1;    // the first call to GetGlyphIndices().
#endif
  };

  int       mType;
  PRUint8   mCharset;
  PRUint32* mMap;
#ifdef MOZ_MATHML
  // We need to cache the CMAP for performance reasons. This
  // way, we can retrieve glyph indices without having to call
  // GetFontData() -- (with malloc/free) for every function call.
  // However, the CMAP is created *lazily* and is typically less than
  // 1K or 2K. If no glyph indices are requested for this font, the
  // mCMAP.mData array will never be created.
  nsCharacterMap mCMAP;
#endif
};

/*-----------------------
** Hash table allocation
**----------------------*/
//-- Font Metrics
PR_STATIC_CALLBACK(void*) fontmap_AllocTable(void *pool, size_t size)
{
  return PR_Malloc(size);
}

PR_STATIC_CALLBACK(void) fontmap_FreeTable(void *pool, void *item)
{
  PR_Free(item);
}

PR_STATIC_CALLBACK(PLHashEntry*) fontmap_AllocEntry(void *pool, const void *key)
{
 return new nsFontInfo(NS_FONT_TYPE_UNICODE, DEFAULT_CHARSET, nsnull);
}

PR_STATIC_CALLBACK(void) fontmap_FreeEntry(void *pool, PLHashEntry *he, PRUint32 flag)
{
  if (flag == HT_FREE_ENTRY)  {
    nsFontInfo *fontInfo = NS_STATIC_CAST(nsFontInfo *, he);
    if (fontInfo->mMap && (fontInfo->mMap != nsFontMetricsWin::gEmptyMap))
      PR_Free(fontInfo->mMap); 
#ifdef MOZ_MATHML
    if (fontInfo->mCMAP.mData)
      PR_Free(fontInfo->mCMAP.mData);
#endif
    delete (nsString *) (he->key);
    delete fontInfo;
  }
}

PLHashAllocOps fontmap_HashAllocOps = {
  fontmap_AllocTable, fontmap_FreeTable,
  fontmap_AllocEntry, fontmap_FreeEntry
};

PRUint32*
nsFontMetricsWin::GetCMAP(HDC aDC, const char* aShortName, int* aFontType, PRUint8* aCharset)
{
  if (!gInitializedFontMaps) {
    gFontMaps = PL_NewHashTable(0, HashKey, CompareKeys, nsnull, &fontmap_HashAllocOps,
      nsnull);
    if (!gFontMaps) { // error checking
      return nsnull;
    }
    gEmptyMap = (PRUint32*) PR_Calloc(2048, 4);
    if (!nsFontMetricsWin::gEmptyMap) {
      PL_HashTableDestroy(gFontMaps);
      gFontMaps = nsnull;
      return nsnull;
    }
    gInitializedFontMaps = 1;
  }
  nsString* name = new nsString();
  if (!name) {
    return nsnull;
  }
  PRUint32* map;
  nsFontInfo* info;
  PLHashEntry *he, **hep = NULL; // shouldn't be NULL, using it as a flag to catch bad changes
  PLHashNumber hash;
  nsGetNameError ret = GetNAME(aDC, name);
  if (ret == eGetName_OK) {
    // lookup the hashtable (if we miss, the computed hash and hep are fed back in HT-RawAdd)
    hash = HashKey(name);
    hep = PL_HashTableRawLookup(gFontMaps, hash, name);
    he = *hep;
    if (he) {
      delete name;
      // an identical map has already been added
      info = NS_STATIC_CAST(nsFontInfo *, he);
      if (aCharset) {
        *aCharset = info->mCharset;
      }
      if (aFontType) {
        *aFontType = info->mType;
      }
      return info->mMap;
    }
    map = (PRUint32*) PR_Calloc(2048, 4);
    if (!map) {
      delete name;
      return nsnull;
    }
  }
  // GDIError occurs when we have raster font (not TrueType)
  else if (ret == eGetName_GDIError) {
    delete name;
    int charset = GetTextCharset(aDC);
    if (charset & (~0xFF)) {
      return gEmptyMap;
    }
    else {
      int j = gCharSetToIndex[charset];
      
      //default charset is not dependable, skip it at this time
      if (j == eCharSet_DEFAULT)
        return gEmptyMap;
      PRUint32* charSetMap = gCharSetInfo[j].mMap;
      if (!charSetMap) {
        charSetMap = (PRUint32*) PR_Calloc(2048, 4);
        if (charSetMap) {
          gCharSetInfo[j].mMap = charSetMap;
          gCharSetInfo[j].GenerateMap(&gCharSetInfo[j]);
        }
        else {
          return gEmptyMap;
        }
      }
      if (aFontType) {
        *aFontType = NS_FONT_TYPE_UNICODE;
      }
      return charSetMap;
    }
  }
  else {
    // return an empty map, so that we never try this font again
    delete name;
    return gEmptyMap;
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

        // Some fonts claim to be unicode when they are actually
        // 'pseudo-unicode' fonts that require a converter...
        // Here, we check if this font is a pseudo-unicode font that 
        // we know something about, and we force it to be treated as
        // a non-unicode font.
        nsAutoString encoding;
        if (NS_SUCCEEDED(GetEncoding(aShortName, encoding))) {
          if (aCharset) {
            *aCharset = DEFAULT_CHARSET;
          }
          if (aFontType) {
            *aFontType = NS_FONT_TYPE_NON_UNICODE;
          }
          if (!GetMap(aShortName, map)) {
            PR_Free(map);
            map = gEmptyMap;
          }
          PR_Free(buf);

          // XXX Need to check if an identical map has already been added - Bug 75260
          NS_ASSERTION(hep, "bad code");
          he = PL_HashTableRawAdd(gFontMaps, hep, hash, name, nsnull);
          if (he) {   
            info = NS_STATIC_CAST(nsFontInfo*, he);
            info->mType = NS_FONT_TYPE_NON_UNICODE;
            info->mMap = map;
            he->value = info;    // so PL_HashTableLookup returns an nsFontInfo*
            return map;
          }
          delete name;
          if (map != gEmptyMap)
            PR_Free(map);
          return nsnull;
        } // if GetEncoding();
        break;  // break out from for(;;) loop
      } //if (encodingID == 1)
      else if (encodingID == 0) { // symbol
        if (aCharset) {
          *aCharset = SYMBOL_CHARSET;
        }
        if (aFontType) {
          *aFontType = NS_FONT_TYPE_NON_UNICODE;
        }
        if (!GetMap(aShortName, map)) {
          PR_Free(map);
          map = gEmptyMap;
        }
        PR_Free(buf);

        // XXX Need to check if an identical map has already been added - Bug 75260
        NS_ASSERTION(hep, "bad code");
        he = PL_HashTableRawAdd(gFontMaps, hep, hash, name, nsnull);
        if (he) {
          info = NS_STATIC_CAST(nsFontInfo*, he);
          info->mCharset = SYMBOL_CHARSET;
          info->mType = NS_FONT_TYPE_NON_UNICODE;
          info->mMap = map;
          he->value = info;    // so PL_HashTableLookup returns an nsFontInfo*
          return map;
        }
        delete name;
        if (map != gEmptyMap)
          PR_Free(map);
        return nsnull;
      }
    } // if (platformID == 3) 
  } // for loop

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
  static PRUint32 spaces[2048];
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

  if (aCharset) {
    *aCharset = DEFAULT_CHARSET;
  }
  if (aFontType) {
    *aFontType = NS_FONT_TYPE_UNICODE;
  }

  // XXX Need to check if an identical map has already been added - Bug 75260
  NS_ASSERTION(hep, "bad code");
  he = PL_HashTableRawAdd(gFontMaps, hep, hash, name, nsnull);
  if (he) {
    info = NS_STATIC_CAST(nsFontInfo*, he);
    he->value = info;    // so PL_HashTableLookup returns an nsFontInfo*
    info->mMap = map;
    return map;
  }
  delete name;
  if (map != gEmptyMap)
    PR_Free(map);
  return nsnull;
}

// The following function returns the glyph indices of a Unicode string.
// To this end, GetGlyphIndices() retrieves the CMAP of the current font
// in the DC:
// 1) If the param aCMAP = nsnull, the function does not cache the CMAP.
// 2) If the param aCMAP = address of a pointer on a nsCharacterMap
// variable, and this is the first call, the function will cache
// the CMAP in the gFontMaps hash table, and returns its location, hence
// the caller can re-use the cached value in subsequent calls.
static PRUint16*
GetGlyphIndices(HDC              aDC,
                nsCharacterMap** aCMAP,
                const PRUnichar* aString, 
                PRUint32         aLength,
                PRUint16*        aBuffer, 
                PRUint32         aBufferLength)
{  
  NS_ASSERTION(aString && aBuffer, "null arg");
  if (!aString || !aBuffer)
    return nsnull;

  PRUint8* buf = nsnull;
  DWORD len = -1;
  
  // --------------
  // Dig for the Unicode subtable if this is the first call.

  if (!aCMAP || // the caller doesn't cache the CMAP, or
      !*aCMAP || (*aCMAP)->mLength < 0  // the CMAP is not yet initialized
     ) 
  {
    // Initialize as a 0-length CMAP, so that if there is an
    // error with this CMAP, it will never be tried again.
    if (aCMAP && *aCMAP) (*aCMAP)->mLength = 0; 

    len = GetFontData(aDC, CMAP, 0, nsnull, 0);
    if ((len == GDI_ERROR) || (!len)) {
      return nsnull;
    }
    buf = (PRUint8*) PR_Malloc(len);
    if (!buf) {
      return nsnull;
    }
    DWORD newLen = GetFontData(aDC, CMAP, 0, buf, len);
    if (newLen != len) {
      PR_Free(buf);
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
      if (platformID == 3 && encodingID == 1) // Unicode
        break;
    }
    if (i == n) {
      NS_WARNING("nsFontMetricsWin::GetGlyphIndices() called for a non-unicode font!");
      PR_Free(buf);
      return nsnull;
    }
    p = buf + offset;
    PRUint16 format = GET_SHORT(p);
    if (format != 4) {
      PR_Free(buf);
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
#ifdef MOZ_MATHML
    // cache these for later re-use
    if (aCMAP) {
      nsAutoString name;
      nsFontInfo* info;
      if (GetNAME(aDC, &name) == eGetName_OK) {
        info = (nsFontInfo*) PL_HashTableLookup(nsFontMetricsWin::gFontMaps, &name);
        if (info) {
          info->mCMAP.mData = buf;
          info->mCMAP.mLength = len;
          *aCMAP = &(info->mCMAP);
        }
      }
    }
#endif
  }

  // --------------
  // get glyph indices

  if (aCMAP && *aCMAP) { // recover cached CMAP if we have skipped the previous step
    buf = (*aCMAP)->mData;
    len = (*aCMAP)->mLength;
  }
  if (buf && len > 0) {
    // get the offset   
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
      if (platformID == 3 && encodingID == 1) // Unicode
        break;
    }
    PRUint8* end = buf + len;

    PRUint16* s = (PRUint16*) (buf + offset);
    PRUint16 segCount = s[3] / 2;
    PRUint16* endCode = &s[7];
    PRUint16* startCode = endCode + segCount + 1;
    PRUint16* idDelta = startCode + segCount;
    PRUint16* idRangeOffset = idDelta + segCount;

    // if output buffer too small, allocate a bigger array -- the caller should free
    PRUint16* result = aBuffer;
    if (aLength > aBufferLength) {
      result = new PRUint16[aLength];
      if (!result) return nsnull;
    }
    for (i = 0; i < aLength; i++) {
      result[i] = GetGlyphIndex(segCount, endCode, startCode,
                                idRangeOffset, idDelta, end, 
                                aString[i]);
    }

    if (!aCMAP) { // free work-space if the CMAP is not to be cached
      PR_Free(buf);
    }

    return result;
  }
  return nsnull;
}

// ----------------------------------------------------------------------
// We use GetGlyphOutlineW() or GetGlyphOutlineA() to get the metrics. 
// GetGlyphOutlineW() is faster, but not implemented on all platforms.
// nsGlyphAgent provides an interface to hide these details, and it
// also hides the parameters/setup needed to call GetGlyphOutline.

enum nsGlyphAgentEnum {
  eGlyphAgent_UNKNOWN = -1,
  eGlyphAgent_UNICODE,
  eGlyphAgent_ANSI
};

class nsGlyphAgent
{
public:
  nsGlyphAgent()
  {
    mState = eGlyphAgent_UNKNOWN;
    // set glyph transform matrix to identity
    FIXED zero, one;
    zero.fract = 0; one.fract = 0;
    zero.value = 0; one.value = 1; 
    mMat.eM12 = mMat.eM21 = zero;
    mMat.eM11 = mMat.eM22 = one;  	
  }

  ~nsGlyphAgent()
  {}

  nsGlyphAgentEnum GetState()
  {
    return mState;
  }

  DWORD GetGlyphMetrics(HDC           aDC,
                        PRUint8       aChar,
                        GLYPHMETRICS* aGlyphMetrics);

  DWORD GetGlyphMetrics(HDC           aDC, 
                        PRUnichar     aChar,
                        PRUint16      aGlyphIndex,
                        GLYPHMETRICS* aGlyphMetrics);
private:
  MAT2 mMat;    // glyph transform matrix (always identity in our context)

  nsGlyphAgentEnum mState; 
                // eGlyphAgent_UNKNOWN : glyph agent is not yet fully initialized
                // eGlyphAgent_UNICODE : this platform implements GetGlyphOutlineW()
                // eGlyphAgent_ANSI : this platform doesn't implement GetGlyphOutlineW()
};

// Version of GetGlyphMetrics() for a non-Unicode font.
// Here we use a simple wrapper on top of GetGlyphOutlineA().
DWORD
nsGlyphAgent::GetGlyphMetrics(HDC           aDC,
                              PRUint8       aChar,
                              GLYPHMETRICS* aGlyphMetrics)
{
  memset(aGlyphMetrics, 0, sizeof(GLYPHMETRICS)); // UMR: bug 46438
  return GetGlyphOutlineA(aDC, aChar, GGO_METRICS, aGlyphMetrics, 0, nsnull, &mMat);
}

// Version of GetGlyphMetrics() for a Unicode font.
// Here we use GetGlyphOutlineW() or GetGlyphOutlineA() to get the metrics. 
// aGlyphIndex should be 0 if the caller doesn't know the glyph index of the char.
DWORD
nsGlyphAgent::GetGlyphMetrics(HDC           aDC,
                              PRUnichar     aChar,
                              PRUint16      aGlyphIndex,
                              GLYPHMETRICS* aGlyphMetrics)
{
  memset(aGlyphMetrics, 0, sizeof(GLYPHMETRICS)); // UMR: bug 46438
  if (eGlyphAgent_UNKNOWN == mState) { // first time we have been in this function
    // see if this platform implements GetGlyphOutlineW()
    DWORD len = GetGlyphOutlineW(aDC, aChar, GGO_METRICS, aGlyphMetrics, 0, nsnull, &mMat);
    if (GetLastError() == ERROR_CALL_NOT_IMPLEMENTED) {
      // next time, we won't bother trying GetGlyphOutlineW()
      mState = eGlyphAgent_ANSI;
    }
    else {
      // all is well with GetGlyphOutlineW(), we will be using it from now on
      mState = eGlyphAgent_UNICODE;
      return len;
    }
  }

  if (eGlyphAgent_UNICODE == mState) {
    return GetGlyphOutlineW(aDC, aChar, GGO_METRICS, aGlyphMetrics, 0, nsnull, &mMat);
  }
  else {
    // we are on a platform that doesn't implement GetGlyphOutlineW()
    // (see Q241358: The GetGlyphOutlineW Function Fails on Windows 95 & 98
    //  http://support.microsoft.com/support/kb/articles/Q241/3/58.ASP)
    // we will use glyph indices as a work around.
    if (0 == aGlyphIndex) { // caller doesn't know the glyph index, so find it
      GetGlyphIndices(aDC, nsnull, &aChar, 1, &aGlyphIndex, 1);
    }
    if (0 < aGlyphIndex) {
      return GetGlyphOutlineA(aDC, aGlyphIndex, GGO_METRICS | GGO_GLYPH_INDEX, aGlyphMetrics, 0, nsnull, &mMat);
    }
  }

  // if we ever reach here, something went wrong in GetGlyphIndices() above
  // because the current font in aDC wasn't a Unicode font
  return GDI_ERROR;
}

// the global glyph agent that we will be using
nsGlyphAgent gGlyphAgent;


// Subclass for common unicode fonts (e.g, Times New Roman, Arial, etc.).
// Offers the fastest rendering because no mapping table is needed (here, 
// unicode code points are identical to font's encoding indices). 
// Uses 'W'ide functions (ExtTextOutW, GetTextExtentPoint32W).
class nsFontWinUnicode : public nsFontWin
{
public:
  nsFontWinUnicode(LOGFONT* aLogFont, HFONT aFont, PRUint32* aMap);
  virtual ~nsFontWinUnicode();

  virtual PRInt32 GetWidth(HDC aDC, const PRUnichar* aString, PRUint32 aLength);
  virtual void DrawString(HDC aDC, PRInt32 aX, PRInt32 aY,

    const PRUnichar* aString, PRUint32 aLength);
#ifdef MOZ_MATHML
  virtual nsresult
  GetBoundingMetrics(HDC                aDC,
                     const PRUnichar*   aString,
                     PRUint32           aLength,
                     nsBoundingMetrics& aBoundingMetrics);

#ifdef NS_DEBUG
  virtual void DumpFontInfo();
#endif // NS_DEBUG
#endif

private:
  PRBool mUnderlinedOrStrikeOut;

};

// Subclass for non-unicode fonts that need a mapping table and uses
// 'A'nsi functions (ExtTextOutA, GetTextExtentPoint32A) after
// converting unicode code points to font's encoding indices. (A slight
// overhead arises from this conversion.)
// NOTE: This subclass also handles some fonts that claim to be 
// unicode, but need a converter.
class nsFontWinNonUnicode : public nsFontWin
{
public:
  nsFontWinNonUnicode(LOGFONT* aLogFont, HFONT aFont, PRUint32* aMap, nsIUnicodeEncoder* aConverter);
  virtual ~nsFontWinNonUnicode();

  virtual PRInt32 GetWidth(HDC aDC, const PRUnichar* aString, PRUint32 aLength);
  virtual void DrawString(HDC aDC, PRInt32 aX, PRInt32 aY,
                          const PRUnichar* aString, PRUint32 aLength);
#ifdef MOZ_MATHML
  virtual nsresult
  GetBoundingMetrics(HDC                aDC,
                     const PRUnichar*   aString,
                     PRUint32           aLength,
                     nsBoundingMetrics& aBoundingMetrics);
#ifdef NS_DEBUG
  virtual void DumpFontInfo();
#endif // NS_DEBUG
#endif

private:
  nsCOMPtr<nsIUnicodeEncoder> mConverter;
};

// A "substitute font" to deal with missing glyphs -- see bug 6585
// Ultimately, this "font" should map Unicodes to other character codes
// for which we *do* have glyphs on the system.
// But, for now, this "font" is only mapping all characters to 
// a REPLACEMENT CHAR. If the member variable mDisplayUnicode set,
// the font will instead render unicode points of missing glyphs in
// the format: &xNNNN;
class nsFontWinSubstitute : public nsFontWin
{
public:
  nsFontWinSubstitute(LOGFONT* aLogFont, HFONT aFont, PRUint32* aMap, PRBool aDisplayUnicode);
  virtual ~nsFontWinSubstitute();

  virtual PRInt32 GetWidth(HDC aDC, const PRUnichar* aString, PRUint32 aLength);
  virtual void DrawString(HDC aDC, PRInt32 aX, PRInt32 aY,
                          const PRUnichar* aString, PRUint32 aLength);
#ifdef MOZ_MATHML
  virtual nsresult
  GetBoundingMetrics(HDC                aDC,
                     const PRUnichar*   aString,
                     PRUint32           aLength,
                     nsBoundingMetrics& aBoundingMetrics);
#ifdef NS_DEBUG
  virtual void DumpFontInfo();
#endif // NS_DEBUG
#endif
private:
  PRBool mDisplayUnicode;
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
  WideCharToMultiByte(CP_ACP, 0, aName->get(), aName->Length() + 1,
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
    char name[sizeof(logFont.lfFaceName)];
    if ((!::GetTextFace(aDC, sizeof(name), name)) ||
        // MJA strcmp(name, logFont.lfFaceName)) {
        strcmpi(name, logFont.lfFaceName)) {
      ::SelectObject(aDC, (HGDIOBJ) oldFont);
      ::DeleteObject(hfont);
      return nsnull;
    }
    PRUint8 charset = DEFAULT_CHARSET;
    int fontType = NS_FONT_TYPE_UNKNOWN;
    PRUint32* map = GetCMAP(aDC, logFont.lfFaceName, &fontType, &charset);
    if (!map) {
      ::SelectObject(aDC, (HGDIOBJ) oldFont);
      ::DeleteObject(hfont);
      return nsnull;
    }
    nsFontWin* font = nsnull;
    if (mIsUserDefined) {
      font = new nsFontWinNonUnicode(&logFont, hfont, gUserDefinedMap,
                                     gUserDefinedConverter);
    }
    else if (NS_FONT_TYPE_UNICODE == fontType) {
      font = new nsFontWinUnicode(&logFont, hfont, map);
    }
    else if (NS_FONT_TYPE_NON_UNICODE == fontType) {
      nsCOMPtr<nsIUnicodeEncoder> converter;
      if (NS_SUCCEEDED(GetConverter(logFont.lfFaceName, getter_AddRefs(converter)))) {
#if 0
// RBS trying to reset hfont with this piece of code causes GDI to be confused
        if (DEFAULT_CHARSET != charset) {
          ::SelectObject(aDC, (HGDIOBJ) oldFont);
          ::DeleteObject(hfont);
          logFont.lfCharSet = charset;
          hfont = ::CreateFontIndirect(&logFont);
        }
#endif
        font = new nsFontWinNonUnicode(&logFont, hfont, map, converter);
      }
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

nsFontWin*
nsFontMetricsWin::LoadGlobalFont(HDC aDC, nsGlobalFont* aGlobalFontItem)
{
  LOGFONT logFont;
  HFONT hfont;

  PRUint16 weightTable = LookForFontWeightTable(aDC, aGlobalFontItem->name);
  PRInt32 weight = GetFontWeight(mFont->weight, weightTable);
  FillLogFont(&logFont, weight);
  logFont.lfCharSet = aGlobalFontItem->logFont.lfCharSet;
  logFont.lfPitchAndFamily = aGlobalFontItem->logFont.lfPitchAndFamily;
  strcpy(logFont.lfFaceName, aGlobalFontItem->logFont.lfFaceName);;
  hfont = ::CreateFontIndirect(&logFont);

  if (hfont) {
    if (mLoadedFontsCount == mLoadedFontsAlloc) {
      int newSize = 2 * (mLoadedFontsAlloc ? mLoadedFontsAlloc : 1);
      nsFontWin** newPointer = (nsFontWin**) PR_Realloc(mLoadedFonts,
        newSize * sizeof(nsFontWin*));
      if (newPointer) {
        mLoadedFonts = (nsFontWin**) newPointer;
        mLoadedFontsAlloc = newSize;
      }
      else {
        ::DeleteObject(hfont);
        return nsnull;
      }
    }

    nsFontWin* font = nsnull;
  
    if (mIsUserDefined) {
      font = new nsFontWinNonUnicode(&logFont, hfont, gUserDefinedMap,
                                     gUserDefinedConverter);
    }
    else if (NS_FONT_TYPE_UNICODE == aGlobalFontItem->fonttype) {
      font = new nsFontWinUnicode(&logFont, hfont, aGlobalFontItem->map);
    }
    else if (NS_FONT_TYPE_NON_UNICODE == aGlobalFontItem->fonttype) {
      nsCOMPtr<nsIUnicodeEncoder> converter;
      if (NS_SUCCEEDED(GetConverter(logFont.lfFaceName, getter_AddRefs(converter)))) {
        font = new nsFontWinNonUnicode(&logFont, hfont, aGlobalFontItem->map, converter);
      }
    }

    if (!font) {
      ::DeleteObject(hfont);
      return nsnull;
    }

    mLoadedFonts[mLoadedFontsCount++] = font;

    return font;
  }

  return nsnull;
}


static int CALLBACK enumProc(const LOGFONT* logFont, const TEXTMETRIC* metrics,
  DWORD fontType, LPARAM closure)
{
  // XXX ignore vertical fonts
  if (logFont->lfFaceName[0] == '@') {
    return 1;
  }

  for (int i = 0; i < nsFontMetricsWin::gGlobalFontsCount; i++) {
    if (!strcmp(nsFontMetricsWin::gGlobalFonts[i].logFont.lfFaceName,
                logFont->lfFaceName)) {
      // work-around for Win95/98 problem 
      int charSetSigBit = charSetToBit[gCharSetToIndex[logFont->lfCharSet]];
      if (charSetSigBit >= 0) {
        DWORD charsetSigAdd = 1 << charSetSigBit;
        nsFontMetricsWin::gGlobalFonts[i].signature.fsCsb[0] |= charsetSigAdd;
      }
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
    &nsFontMetricsWin::gGlobalFonts[nsFontMetricsWin::gGlobalFontsCount];

  PRUnichar name[LF_FACESIZE];
  name[0] = 0;
  MultiByteToWideChar(CP_ACP, 0, logFont->lfFaceName,
    strlen(logFont->lfFaceName) + 1, name, sizeof(name)/sizeof(name[0]));
  font->name = new nsString(name);
  if (!font->name) {
    return 0;
  }
  font->map = nsnull;
  font->logFont = *logFont;
  font->flags = 0;
  font->signature.fsCsb[0] = 0;
  font->signature.fsCsb[1] = 0;

  if (fontType & TRUETYPE_FONTTYPE) {
    font->flags |= NS_GLOBALFONT_TRUETYPE;
  }
  if (logFont->lfCharSet == SYMBOL_CHARSET) {
    font->flags |= NS_GLOBALFONT_SYMBOL;
  }

  int charSetSigBit = charSetToBit[gCharSetToIndex[logFont->lfCharSet]];
  if (charSetSigBit >= 0) {
    DWORD charsetSigAdd = 1 << charSetSigBit;
    font->signature.fsCsb[0] |= charsetSigAdd;
  }

  ++nsFontMetricsWin::gGlobalFontsCount;
  return 1;
}

static int
CompareGlobalFonts(const void* aArg1, const void* aArg2, void* aClosure)
{
  const nsGlobalFont* font1 = (const nsGlobalFont*)aArg1;
  const nsGlobalFont* font2 = (const nsGlobalFont*)aArg2;

  // Sorting criteria is like a tree:
  // + TrueType fonts
  //    + non-Symbol fonts
  //    + Symbol fonts
  // + non-TrueType fonts
  //    + non-Symbol fonts
  //    + Symbol fonts
  PRInt32 weight1 = 0, weight2 = 0; // computed as node mask 
  if (!(font1->flags & NS_GLOBALFONT_TRUETYPE))
    weight1 |= 0x2;
  if (!(font2->flags & NS_GLOBALFONT_TRUETYPE))
    weight2 |= 0x2; 
  if (font1->flags & NS_GLOBALFONT_SYMBOL)
    weight1 |= 0x1;
  if (font2->flags & NS_GLOBALFONT_SYMBOL)
    weight2 |= 0x1;

  return weight1 - weight2;
}

nsGlobalFont*
nsFontMetricsWin::InitializeGlobalFonts(HDC aDC)
{
  if (!gInitializedGlobalFonts) {
    gInitializedGlobalFonts = 1;
    LOGFONT logFont;
    logFont.lfCharSet = DEFAULT_CHARSET;
    logFont.lfFaceName[0] = 0;
    logFont.lfPitchAndFamily = 0;

    /*
     * msdn.microsoft.com/library states that
     * EnumFontFamiliesExW is only on NT/2000
     */
    EnumFontFamiliesEx(aDC, &logFont, enumProc, nsnull, 0);

    // Sort the global list of fonts to put the 'preferred' fonts first
    NS_QuickSort(gGlobalFonts, gGlobalFontsCount, sizeof(nsGlobalFont),
                 CompareGlobalFonts, nsnull);
  }

  return gGlobalFonts;
}

int
nsFontMetricsWin::SameAsPreviousMap(int aIndex)
{
  for (int i = 0; i < aIndex; i++) {
    if (gGlobalFonts[i].flags & NS_GLOBALFONT_SKIP) {
      continue;
    }
    if (gGlobalFonts[i].map == gGlobalFonts[aIndex].map) {
      gGlobalFonts[aIndex].flags |= NS_GLOBALFONT_SKIP;
      return 1;
    }
    PRUint32* map1 = gGlobalFonts[i].map;
    PRUint32* map2 = gGlobalFonts[aIndex].map;
    int j;
    for (j = 0; j < 2048; j++) {
      if (map1[j] != map2[j]) {
        break;
      }
    }
    if (j == 2048) {
      gGlobalFonts[aIndex].flags |= NS_GLOBALFONT_SKIP;
      return 1;
    }
  }

  return 0;
}

nsFontWin*
nsFontMetricsWin::FindGlobalFont(HDC aDC, PRUnichar c)
{
  //now try global font
  if (!gGlobalFonts) {
    if (!InitializeGlobalFonts(aDC)) {
      return nsnull;
    }
  }
  for (int i = 0; i < gGlobalFontsCount; i++) {
    if (gGlobalFonts[i].flags & NS_GLOBALFONT_SKIP) {
      continue;
    }
    if (!gGlobalFonts[i].map) {
      HFONT font = ::CreateFontIndirect(&gGlobalFonts[i].logFont);
      if (!font) {
        continue;
      }
      HFONT oldFont = (HFONT) ::SelectObject(aDC, font);
      gGlobalFonts[i].map = GetCMAP(aDC, gGlobalFonts[i].logFont.lfFaceName,
        &(gGlobalFonts[i].fonttype), nsnull);
      ::SelectObject(aDC, oldFont);
      ::DeleteObject(font);
      if (!gGlobalFonts[i].map || gGlobalFonts[i].map == gEmptyMap) {
        gGlobalFonts[i].flags |= NS_GLOBALFONT_SKIP;
        continue;
      }
      if (SameAsPreviousMap(i)) {
        continue;
      }
    }
    if (FONT_HAS_GLYPH(gGlobalFonts[i].map, c)) {
      return LoadGlobalFont(aDC, &(gGlobalFonts[i]));
    }
  }

  return nsnull;
}

nsFontWin*
nsFontMetricsWin::FindSubstituteFont(HDC aDC, PRUnichar c)
{
  /*
  When this function is called, it means all other alternatives have
  been unsuccessfully tried! So the idea is this:
  
  See if the "substitute font" is already loaded?
  a/ if yes, ADD_GLYPH(c), to record that the font should be used
     to render this char from now on. 
  b/ if no, load the font, and ADD_GLYPH(c)
  */

  // Assumptions: 
  // The nsFontInfo of the "substitute font" is not in the hashtable!
  // This way, its name doesn't matter since it cannot be retrieved with
  // a lookup in the hashtable. We only know its index in mLoadedFonts[]
  // (Notice that the nsFontInfo of the font that is picked as the
  // "substitute font" *can* be in the hashtable, and as expected, its 
  // own map can be retrieved with the normal lookup).

  // The "substitute font" is a *unicode font*. No conversion here.
  // XXX Is it worth having another subclass which has a converter? 
  // Such a variant may allow to display a boxed question mark, etc.

  if (0 <= mIndexOfSubstituteFont) {
    NS_ASSERTION(mLoadedFonts[mIndexOfSubstituteFont], "null substitute font");
    if (mLoadedFonts[mIndexOfSubstituteFont]) {
      ADD_GLYPH(mLoadedFonts[mIndexOfSubstituteFont]->mMap, c);
      return mLoadedFonts[mIndexOfSubstituteFont];
    }
    return nsnull;
  }

  // The "substitute font" has not yet been created... 
  // The first unicode font (no converter) that has the
  // replacement char is taken and placed as the substitute font.

  int i;
  for (i = 0; i < mLoadedFontsCount; i++) {
    nsFontWin* font = mLoadedFonts[i];
    nsAutoString name;
    HFONT oldFont = (HFONT) ::SelectObject(aDC, font->mFont);
    nsGetNameError res = GetNAME(aDC, &name);
    ::SelectObject(aDC, oldFont);
    if (res != eGetName_OK) {
    	continue;
    }
    nsFontInfo* info = (nsFontInfo*)PL_HashTableLookup(nsFontMetricsWin::gFontMaps, &name);
    if (!info || info->mType != NS_FONT_TYPE_UNICODE) {
      continue;
    }
    if (FONT_HAS_GLYPH(font->mMap, NS_REPLACEMENT_CHAR))
    {
    	// XXX if the mode is to display unicode points "&#xNNNN;", should we check
    	// that the substitute font also has glyphs for '&', '#', 'x', ';' and digits?
    	// (Because this is a unicode font, those glyphs should in principle be there.)
      name.AssignWithConversion(font->mName);
      font = LoadSubstituteFont(aDC, &name);
      if (font) {
        ADD_GLYPH(font->mMap, c);
        return font;
      }
    }
  }

  // if we ever reach here, the replacement char should be changed to a more common char
  NS_ASSERTION(0, "Could not provide a substititute font");
  return nsnull;
}

nsFontWin*
nsFontMetricsWin::LoadSubstituteFont(HDC aDC, nsString* aName)
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
  WideCharToMultiByte(CP_ACP, 0, aName->get(), aName->Length() + 1,
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
    nsFontWin* font = nsnull;
    PRUint32* map = (PRUint32*) PR_Calloc(2048, 4);
    if (map) {
      // XXX 'displayUnicode' has to be initialized based on the desired rendering mode
      PRBool displayUnicode = PR_FALSE;
      font = new nsFontWinSubstitute(&logFont, hfont, map, displayUnicode);
    }
    if (!font) {
      if (map) PR_Free(map);
      ::SelectObject(aDC, (HGDIOBJ) oldFont);
      ::DeleteObject(hfont);
      return nsnull;
    }
    mIndexOfSubstituteFont = mLoadedFontsCount; // got the index
    mLoadedFonts[mLoadedFontsCount++] = font;
    ::SelectObject(aDC, (HGDIOBJ) oldFont);

    return font;
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


class nsFontWeightEntry : public PLHashEntry
{
public:
  nsString mFontName;
  PRUint16 mWeightTable; // Each bit indicates the availability of a font weight.
};

static PLHashNumber
HashKeyFontWeight(const void* aFontWeightEntry)
{
  const nsString* string = &((const nsFontWeightEntry*) aFontWeightEntry)->mFontName;
  return (PLHashNumber)
    nsCRT::HashCode(string->get());
}

static PRIntn
CompareKeysFontWeight(const void* aFontWeightEntry1, const void* aFontWeightEntry2)
{
  const nsString* str1 = &((const nsFontWeightEntry*) aFontWeightEntry1)->mFontName;
  const nsString* str2 = &((const nsFontWeightEntry*) aFontWeightEntry2)->mFontName;

  return nsCRT::strcmp(str1->get(), str2->get()) == 0;
}

/*-----------------------
** Hash table allocation
**----------------------*/
//-- Font weight
PR_STATIC_CALLBACK(void*) fontweight_AllocTable(void *pool, size_t size)
{
  return PR_Malloc(size);
}

PR_STATIC_CALLBACK(void) fontweight_FreeTable(void *pool, void *item)
{
  PR_Free(item);
}

PR_STATIC_CALLBACK(PLHashEntry*) fontweight_AllocEntry(void *pool, const void *key)
{
  return new nsFontWeightEntry;
}

PR_STATIC_CALLBACK(void) fontweight_FreeEntry(void *pool, PLHashEntry *he, PRUint32 flag)
{
  if (flag == HT_FREE_ENTRY)  {
    nsFontWeightEntry *fontWeightEntry = NS_STATIC_CAST(nsFontWeightEntry *, he);
    delete (fontWeightEntry);
  }
}

PLHashAllocOps fontweight_HashAllocOps = {
  fontweight_AllocTable, fontweight_FreeTable,
  fontweight_AllocEntry, fontweight_FreeEntry
};

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

typedef struct {
  LOGFONT  mLogFont;
  PRUint16 mWeights;
  int      mFontCount;
} nsFontWeightInfo;

static int CALLBACK nsFontWeightCallback(const LOGFONT* logFont, const TEXTMETRIC * metrics,
  DWORD fontType, LPARAM closure)
{
  // printf("Name %s Log font sizes %d\n",logFont->lfFaceName,logFont->lfWeight);
  
  nsFontWeightInfo* weightInfo = (nsFontWeightInfo*)closure;
  if (NULL != metrics) {
    int pos = metrics->tmWeight / 100;
      // Set a bit to indicate the font weight is available
    if (weightInfo->mFontCount == 0)
      weightInfo->mLogFont = *logFont;
    nsFontMetricsWin::SetFontWeight(metrics->tmWeight / 100,
      &weightInfo->mWeights);
    weightInfo->mFontCount++;
  }

  return TRUE; // Keep looking for more weights.
}


static void SearchSimulatedFontWeight(HDC aDC, nsFontWeightInfo* aWeightInfo)
{
  int weight, weightCount;

  // if the font does not exist return immediately 
  if (aWeightInfo->mFontCount == 0) {
    return;
  }

  // If two or more nonsimulated variants exist, just use them.
  weightCount = 0;
  for (weight = 100; weight <= 900; weight += 100) {
    if (nsFontMetricsWin::IsFontWeightAvailable(weight, aWeightInfo->mWeights))
      weightCount++;
  }

  // If the font does not exist (weightCount == 0) or
  // there are 2 or more weights already, don't look for
  // simulated font weights.
  if ((weightCount == 0) || (weightCount > 1)) {
    return;
  }

  // Searching simulated variants.
  // The tmWeight member holds simulated font weight.
  LOGFONT logFont = aWeightInfo->mLogFont;
  for (weight = 100; weight <= 900; weight += 100) {
    logFont.lfWeight = weight;
    HFONT hfont = ::CreateFontIndirect(&logFont);
    HFONT oldfont = (HFONT)::SelectObject(aDC, (HGDIOBJ)hfont);

    TEXTMETRIC metrics;
    GetTextMetrics(aDC, &metrics);
    if (metrics.tmWeight == weight) {
//      printf("font weight for %s found: %d%s\n", logFont.lfFaceName, weight,
//        nsFontMetricsWin::IsFontWeightAvailable(weight, aWeightInfo->mWeights) ?
//        "" : " (simulated)");
      nsFontMetricsWin::SetFontWeight(weight / 100, &aWeightInfo->mWeights);
    }

    ::SelectObject(aDC, (HGDIOBJ)oldfont);
    ::DeleteObject((HGDIOBJ)hfont);
  }
}


PRUint16 
nsFontMetricsWin::GetFontWeightTable(HDC aDC, nsString* aFontName) {

    // Look for all of the weights for a given font.
  LOGFONT logFont;
  logFont.lfCharSet = DEFAULT_CHARSET;

  WideCharToMultiByte(CP_ACP, 0, aFontName->get(), aFontName->Length() + 1,
    logFont.lfFaceName, sizeof(logFont.lfFaceName), nsnull, nsnull);
 
  logFont.lfPitchAndFamily = 0;

  nsFontWeightInfo weightInfo;
  weightInfo.mWeights = 0;
  weightInfo.mFontCount = 0;
   ::EnumFontFamiliesEx(aDC, &logFont, nsFontWeightCallback, (LPARAM)&weightInfo, 0);
  SearchSimulatedFontWeight(aDC, &weightInfo);
 //  printf("font weights for %s dec %d hex %x \n", logFont.lfFaceName, weightInfo.mWeights, weightInfo.mWeights);
  return weightInfo.mWeights;
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
    // Initialize the font weight table if need be.
  if (!gInitializedFontWeights) {
    gInitializedFontWeights = 1;
    gFontWeights = PL_NewHashTable(0, HashKeyFontWeight, CompareKeysFontWeight, nsnull, &fontweight_HashAllocOps,
      nsnull);
    if (!gFontWeights) {
      gInitializedFontWeights = 0;
      return 0;
    }
  }

  // Use lower case name for hash table searches. This eliminates
  // keeping multiple font weights entries when the font name varies 
  // only by case.
  nsAutoString low; low.Assign(*aName);
  low.ToLowerCase();

   // See if the font weight has already been computed.
  nsFontWeightEntry searchEntry;
  searchEntry.mFontName = low;
  searchEntry.mWeightTable = 0;

  nsFontWeightEntry* weightEntry;
  PLHashEntry **hep, *he;
  PLHashNumber hash = HashKeyFontWeight(&searchEntry);
  hep = PL_HashTableRawLookup(gFontWeights, hash, &searchEntry);
  he = *hep;
  if (he) {
    // an identical fontweight has already been added
    weightEntry = NS_STATIC_CAST(nsFontWeightEntry *, he);
    return weightEntry->mWeightTable;
  }

   // Hasn't been computed, so need to compute and store it.
  PRUint16 weightTable = GetFontWeightTable(aDC, aName);
//  printf("Compute font weight %d\n",  weightTable);

   // Store it in font weight HashTable.
  he = PL_HashTableRawAdd(gFontWeights, hep, hash, &searchEntry, nsnull);
  if (he) {   
    weightEntry = NS_STATIC_CAST(nsFontWeightEntry*, he);
    weightEntry->mFontName = low;
    weightEntry->mWeightTable = weightTable;
    he->key = weightEntry;
    he->value = weightEntry;
    return weightEntry->mWeightTable;
  }

  return 0;
}


// ------------ End of font weight utilities

nsFontWin*
nsFontMetricsWin::FindUserDefinedFont(HDC aDC, PRUnichar aChar)
{
  if (mIsUserDefined) {
    nsFontWin* font = LoadFont(aDC, &mUserDefined);
    if (font && FONT_HAS_GLYPH(font->mMap, aChar)) {
      return font;
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

  { nsnull, nsnull }
};

/*-----------------------
** Hash table allocation
**----------------------*/
//-- Font FamilyNames
PR_STATIC_CALLBACK(void*) familyname_AllocTable(void *pool, size_t size)
{
  return PR_Malloc(size);
}

PR_STATIC_CALLBACK(void) familyname_FreeTable(void *pool, void *item)
{
  PR_Free(item);
}

PR_STATIC_CALLBACK(PLHashEntry*) familyname_AllocEntry(void *pool, const void *key)
{
  return PR_NEW(PLHashEntry);
}

PR_STATIC_CALLBACK(void) familyname_FreeEntry(void *pool, PLHashEntry *he, PRUint32 flag)
{
  delete (nsString *) (he->value);

  if (flag == HT_FREE_ENTRY)  {
    delete (nsString *) (he->key);
    PR_Free(he);
  }
}

PLHashAllocOps familyname_HashAllocOps = {
  familyname_AllocTable, familyname_FreeTable,
  familyname_AllocEntry, familyname_FreeEntry
};

PLHashTable*
nsFontMetricsWin::InitializeFamilyNames(void)
{
  if (!gInitializedFamilyNames) {
    gInitializedFamilyNames = 1;
    gFamilyNames = PL_NewHashTable(0, HashKey, CompareKeys, nsnull, &familyname_HashAllocOps,
      nsnull);
    if (!gFamilyNames) {
      gInitializedFamilyNames = 0;
      return nsnull;
    }
    nsFontFamilyName* f = gFamilyNameTable;
    while (f->mName) {
      nsString* name = new nsString;
      nsString* winName = new nsString;
      if (name && winName) {
        name->AssignWithConversion(f->mName);
        winName->AssignWithConversion(f->mWinName);
        if (PL_HashTableAdd(gFamilyNames, name, (void*) winName))  
        { 
          f++;
          continue;
        }
      }
      // if we reach here, no FamilyName was added to the hashtable
      if (name) delete name;
      if (winName) delete winName;
      return nsnull;
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
  while (mFontsIndex < mFonts.Count()) {
    if (mFontIsGeneric[mFontsIndex]) {
      return nsnull;
    }
    nsString* name = mFonts.StringAt(mFontsIndex++);
    nsAutoString low(*name);
    low.ToLowerCase();
    nsString* winName = (nsString*) PL_HashTableLookup(gFamilyNames, &low);
    if (!winName) {
      winName = name;
    }
    nsFontWin* font = LoadFont(aDC, winName);
    if (font && FONT_HAS_GLYPH(font->mMap, aChar)) {
      return font;
    }
  }

  return nsnull;
}

nsFontWin*
nsFontMetricsWin::LoadGenericFont(HDC aDC, PRUnichar aChar, char** aName)
{
  if (*aName) {
    int found = 0;
    int i;
    for (i = 0; i < mLoadedFontsCount; i++) {
      nsFontWin* font = mLoadedFonts[i];
      if (!strcmp(font->mName, *aName)) {
        found = 1;
        break;
      }
    }
    if (found) {
      nsMemory::Free(*aName);
      *aName = nsnull;
      return nsnull;
    }
    PRUnichar name[LF_FACESIZE] = { 0 };
    PRUnichar format[] = { '%', 's', 0 };
    PRUint32 n = nsTextFormatter::snprintf(name, LF_FACESIZE, format, *aName);
    nsMemory::Free(*aName);
    *aName = nsnull;
    if (n && (n != (PRUint32) -1)) {
      nsAutoString  fontName(name);

      nsFontWin* font = LoadFont(aDC, &fontName);
      if (font && FONT_HAS_GLYPH(font->mMap, aChar)) {
        return font;
      }
    }
  }

  return nsnull;
}

typedef struct PrefEnumInfo
{
  PRUnichar         mChar;
  HDC               mDC;
  nsFontWin*        mFont;
  nsFontMetricsWin* mMetrics;
} PrefEnumInfo;

void
PrefEnumCallback(const char* aName, void* aClosure)
{
  PrefEnumInfo* info = (PrefEnumInfo*) aClosure;
  if (info->mFont) {
    return;
  }
  PRUnichar ch = info->mChar;
  HDC dc = info->mDC;
  nsFontMetricsWin* metrics = info->mMetrics;
  char* value = nsnull;
  gPref->CopyCharPref(aName, &value);
  nsFontWin* font = metrics->LoadGenericFont(dc, ch, &value);
  if (font) {
    info->mFont = font;
  }
  else {
    gPref->CopyDefaultCharPref(aName, &value);
    font = metrics->LoadGenericFont(dc, ch, &value);
    if (font) {
      info->mFont = font;
    }
  }
}

nsFontWin*
nsFontMetricsWin::FindGenericFont(HDC aDC, PRUnichar aChar)
{
  if (mTriedAllGenerics) {
    return nsnull;
  }
  nsAutoString prefix;
  prefix.AssignWithConversion("font.name.");
  prefix.Append(*mGeneric);
  char name[128];
  if (mLangGroup) {
    nsAutoString pref = prefix;
    pref.AppendWithConversion('.');
    const PRUnichar* langGroup = nsnull;
    mLangGroup->GetUnicode(&langGroup);
    pref.Append(langGroup);
    pref.ToCString(name, sizeof(name));
    char* value = nsnull;
    gPref->CopyCharPref(name, &value);
    nsFontWin* font = LoadGenericFont(aDC, aChar, &value);
    if (font) {
      return font;
    }
    gPref->CopyDefaultCharPref(name, &value);
    font = LoadGenericFont(aDC, aChar, &value);
    if (font) {
      return font;
    }

    //let's try fall back list
    PRInt32 i = strlen(name);
    char ch;
    name[i+2] = '\0';
    name[i] = '.';
    for (ch = '1'; ch <= '9'; ch++)
    {
       name[i+1] = ch;
       if (NS_FAILED(gPref->CopyCharPref(name, &value)))
         break;
       font = LoadGenericFont(aDC, aChar, &value);
       if (font)
         return font;
    }

  }
  prefix.ToCString(name, sizeof(name));
  PrefEnumInfo info = { aChar, aDC, nsnull, this };
  gPref->EnumerateChildren(name, PrefEnumCallback, &info);
  if (info.mFont) {
    return info.mFont;
  }
  mTriedAllGenerics = 1;

  return nsnull;
}

nsFontWin*
nsFontMetricsWin::FindFont(HDC aDC, PRUnichar aChar)
{
  nsFontWin* font = FindUserDefinedFont(aDC, aChar);
  if (!font) {
    font = FindLocalFont(aDC, aChar);
    if (!font) {
      font = FindGenericFont(aDC, aChar);
      if (!font) {
        font = FindGlobalFont(aDC, aChar);
        if (!font) {
          font = FindSubstituteFont(aDC, aChar);
        }
      }
    }
  }

  return font;
}

static PRBool
FontEnumCallback(const nsString& aFamily, PRBool aGeneric, void *aData)
{
  nsFontMetricsWin* metrics = (nsFontMetricsWin*) aData;
  metrics->mFonts.AppendString(aFamily);
  metrics->mFontIsGeneric.AppendElement((void*) aGeneric);
  if (aGeneric) {
    metrics->mGeneric = metrics->mFonts.StringAt(metrics->mFonts.Count() - 1);
    metrics->mGeneric->ToLowerCase();
    return PR_FALSE; // stop
  }

  return PR_TRUE; // don't stop
}

/** ---------------------------------------------------
 *  See documentation in nsFontMetricsWin.h
 *	@update 05/28/99 dwc
 */
nsresult
nsFontMetricsWin::RealizeFont()
{
nsresult res;
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
  PRUnichar* value = nsnull;
  if (!mGeneric) {
    gPref->CopyUnicharPref("font.default", &value);
    if (value) {
      mDefaultFont = value;
      nsMemory::Free(value);
      value = nsnull;
    }
    else {
      mDefaultFont.AssignWithConversion("serif");
    }
    mGeneric = &mDefaultFont;
  }

  if (mLangGroup.get() == gUserDefined) {
    if (!gUserDefinedConverter) {
      nsCOMPtr<nsIAtom> charset;
      res = gCharSetManager->GetCharsetAtom2("x-user-defined",
        getter_AddRefs(charset));
      if (NS_SUCCEEDED(res)) {
        res = gCharSetManager->GetUnicodeEncoder(charset,
                                                 &gUserDefinedConverter);
        if (NS_SUCCEEDED(res)) {
          res = gUserDefinedConverter->SetOutputErrorBehavior(
            gUserDefinedConverter->kOnError_Replace, nsnull, '?');
          nsCOMPtr<nsICharRepresentable> mapper =
            do_QueryInterface(gUserDefinedConverter);
          if (mapper) {
            res = mapper->FillInfo(gUserDefinedMap);
          }
        }
        else {
          return res;
        }
      }
      else {
        return res;
      }
    }

    nsCAutoString name("font.name.");
    name.AppendWithConversion(*mGeneric);
    name.Append('.');
    name.Append(USER_DEFINED);
    gPref->CopyUnicharPref(name.get(), &value);
    if (value) {
      mUserDefined = value;
      nsMemory::Free(value);
      value = nsnull;
      mIsUserDefined = 1;
    }
  }

  nsFontWin* font = FindFont(dc1, 'a');
  if (!font) {
    return NS_ERROR_FAILURE;
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
    mXHeight = NSToCoordRound((float)metrics.tmAscent * dev2app * 0.56f); // 50% of ascent, best guess for true type
    mSuperscriptOffset = NSToCoordRound(oMetrics.otmptSuperscriptOffset.y * dev2app);
    mSubscriptOffset = NSToCoordRound(oMetrics.otmptSubscriptOffset.y * dev2app);

    mStrikeoutSize = PR_MAX(onePixel, NSToCoordRound(oMetrics.otmsStrikeoutSize * dev2app));
    mStrikeoutOffset = NSToCoordRound(oMetrics.otmsStrikeoutPosition * dev2app);
    mUnderlineSize = PR_MAX(onePixel, NSToCoordRound(oMetrics.otmsUnderscoreSize * dev2app));
    mUnderlineOffset = NSToCoordRound(oMetrics.otmsUnderscorePosition * dev2app);

    // Begin -- section of code to get the real x-height with GetGlyphOutline()
    GLYPHMETRICS gm;
    DWORD len = gGlyphAgent.GetGlyphMetrics(dc, PRUnichar('x'), 0, &gm);
    if (GDI_ERROR != len && gm.gmptGlyphOrigin.y > 0)
    {
      mXHeight = NSToCoordRound(gm.gmptGlyphOrigin.y * dev2app);
    }
    // End -- getting x-height
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

  mLeading = NSToCoordRound(metrics.tmInternalLeading * dev2app);
  mEmHeight = NSToCoordRound((metrics.tmHeight - metrics.tmInternalLeading) *
                             dev2app);
  mEmAscent = NSToCoordRound((metrics.tmAscent - metrics.tmInternalLeading) *
                             dev2app);
  mEmDescent = NSToCoordRound(metrics.tmDescent * dev2app);
  mMaxHeight = NSToCoordRound(metrics.tmHeight * dev2app);
  mMaxAscent = NSToCoordRound(metrics.tmAscent * dev2app);
  mMaxDescent = NSToCoordRound(metrics.tmDescent * dev2app);
  mMaxAdvance = NSToCoordRound(metrics.tmMaxCharWidth * dev2app);
  mAveCharWidth = PR_MAX(1, NSToCoordRound(metrics.tmAveCharWidth * dev2app));

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

  return NS_OK;
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
  aHeight = mMaxHeight;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin :: GetNormalLineHeight(nscoord &aHeight)
{
  aHeight = mEmHeight + mLeading;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin :: GetLeading(nscoord &aLeading)
{
  aLeading = mLeading;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin :: GetEmHeight(nscoord &aHeight)
{
  aHeight = mEmHeight;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin :: GetEmAscent(nscoord &aAscent)
{
  aAscent = mEmAscent;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin :: GetEmDescent(nscoord &aDescent)
{
  aDescent = mEmDescent;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin :: GetMaxHeight(nscoord &aHeight)
{
  aHeight = mMaxHeight;
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
nsFontMetricsWin::GetLangGroup(nsIAtom** aLangGroup)
{
  if (!aLangGroup) {
    return NS_ERROR_NULL_POINTER;
  }

  *aLangGroup = mLangGroup;
  NS_IF_ADDREF(*aLangGroup);

  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin::GetFontHandle(nsFontHandle &aHandle)
{
  aHandle = mFontHandle;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin::GetAveCharWidth(nscoord &aAveCharWidth)
{
  aAveCharWidth = mAveCharWidth;
  return NS_OK;
}

nsFontWin::nsFontWin(LOGFONT* aLogFont, HFONT aFont, PRUint32* aMap)
{
  if (aLogFont) {
    strcpy(mName, aLogFont->lfFaceName);
  }
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
  PRUint32* aMap) : nsFontWin(aLogFont, aFont, aMap)
{
// This is used for a bug in WIN95 where it does not render
// unicode TrueType fonts with underline or strikeout correctly.
// @see  nsFontWinUnicode::DrawString
  NS_ASSERTION(aLogFont != NULL, "Null logfont passed to nsFontWinUnicode constructor");
  if ((aLogFont->lfUnderline) || (aLogFont->lfStrikeOut)) {
    mUnderlinedOrStrikeOut = PR_TRUE;
  } else {
    mUnderlinedOrStrikeOut = PR_FALSE;
  }
}

nsFontWinUnicode::~nsFontWinUnicode()
{
}

PRInt32
nsFontWinUnicode::GetWidth(HDC aDC, const PRUnichar* aString, PRUint32 aLength)
{
  SIZE size;
  ::GetTextExtentPoint32W(aDC, aString, aLength, &size);
  return size.cx;
}

void
nsFontWinUnicode::DrawString(HDC aDC, PRInt32 aX, PRInt32 aY,
  const PRUnichar* aString, PRUint32 aLength)
{

  // Due to a bug in WIN95 unicode rendering of truetype fonts
  // with underline or strikeout, we need to set a clip rect
  // to prevent the underline and/or strikethru from being rendered
  // too far to the right of the character string.
  // The WIN95 bug causes the underline to be 20-30% longer than
  // it should be. @see bugzilla bug 8322
                           
  // Do check for underline or strikeout first since they are rarely
  // used. HTML text frames draw the underlines or strikeout rather
  // than relying on loading and underline or strikeout font so very
  // few fonts should have mUnderlinedOrStrikeOut set.
  if (mUnderlinedOrStrikeOut)
  {
     //XXX: This code to test the OS version
     //was lifted out of nsRenderingContext
     //It really should be moved to a common location that both
     //the rendering context and nsFontMetricsWin can access.
     // Determine if OS = WIN95
    if (NOT_SETUP == gIsWIN95) {
      OSVERSIONINFO os;
      os.dwOSVersionInfoSize = sizeof(os);
      ::GetVersionEx(&os);
      // XXX This may need tweaking for win98
      if (VER_PLATFORM_WIN32_NT == os.dwPlatformId) {
        gIsWIN95 = PR_FALSE;
      }
      else {
        gIsWIN95 = PR_TRUE;
      }
    }

    if (gIsWIN95)
    {
        // Clip out the extra underline/strikethru caused by the
        // bug in WIN95.
      SIZE size;
      ::GetTextExtentPoint32W(aDC, aString, aLength, &size);
      RECT clipRect;
      clipRect.top = aY - size.cy;
      clipRect.bottom = aY + size.cy; // Make it plenty large to allow for character descent.
                                      // Not necessary to clip vertically, only horizontally
      clipRect.left = aX;
      clipRect.right = aX + size.cx;
      ::ExtTextOutW(aDC, aX, aY, ETO_CLIPPED, &clipRect, aString, aLength, NULL); 
      return;
    }
  } 

  // Do normal non-WIN95 text output without clipping
  ::ExtTextOutW(aDC, aX, aY, 0, NULL, aString, aLength, NULL);  
}

#ifdef MOZ_MATHML
nsresult
nsFontWinUnicode::GetBoundingMetrics(HDC                aDC, 
                                     const PRUnichar*   aString,
                                     PRUint32           aLength,
                                     nsBoundingMetrics& aBoundingMetrics)
{
  aBoundingMetrics.Clear();
  if (aString && 0 < aLength) {
    PRUint16 str[CHAR_BUFFER_SIZE];
    PRUint16* pstr = (PRUint16*)&str;

    // at this stage, the glyph agent should have already been initialized
    // given that it was used to compute the x-height in RealizeFont()
    NS_ASSERTION(gGlyphAgent.GetState() != eGlyphAgent_UNKNOWN, "Glyph agent is not yet initialized");
    if (gGlyphAgent.GetState() != eGlyphAgent_UNICODE) {
      // we are on a platform that doesn't implement GetGlyphOutlineW() 
      // we need to use glyph indices
      pstr = GetGlyphIndices(aDC, &mCMAP, aString, aLength, str, CHAR_BUFFER_SIZE);
      if (!pstr) {
        return NS_ERROR_UNEXPECTED;
      }
    }

    // measure the string
    nscoord descent;
    GLYPHMETRICS gm;                                                
    DWORD len = gGlyphAgent.GetGlyphMetrics(aDC, aString[0], pstr[0], &gm);
    if (GDI_ERROR == len) {
      return NS_ERROR_UNEXPECTED;
    }
    // flip sign of descent for cross-platform compatibility
    descent = -(nscoord(gm.gmptGlyphOrigin.y) - nscoord(gm.gmBlackBoxY));
    aBoundingMetrics.leftBearing = gm.gmptGlyphOrigin.x;
    aBoundingMetrics.rightBearing = gm.gmptGlyphOrigin.x + gm.gmBlackBoxX;
    aBoundingMetrics.ascent = gm.gmptGlyphOrigin.y;
    aBoundingMetrics.descent = descent;
    aBoundingMetrics.width = gm.gmCellIncX;

    if (1 < aLength) {
      // loop over each glyph to get the ascent and descent
      PRUint32 i;
      for (i = 1; i < aLength; i++) {
        len = gGlyphAgent.GetGlyphMetrics(aDC, aString[i], pstr[i], &gm);
        if (GDI_ERROR == len) {
          return NS_ERROR_UNEXPECTED;
        }
        // flip sign of descent for cross-platform compatibility
        descent = -(nscoord(gm.gmptGlyphOrigin.y) - nscoord(gm.gmBlackBoxY));
        if (aBoundingMetrics.ascent < gm.gmptGlyphOrigin.y)
          aBoundingMetrics.ascent = gm.gmptGlyphOrigin.y;
        if (aBoundingMetrics.descent < descent)
          aBoundingMetrics.descent = descent;
      }
      // get the final rightBearing and width. Possible kerning is taken into account.
      SIZE size;
      ::GetTextExtentPointW(aDC, aString, aLength, &size);
      aBoundingMetrics.width = size.cx;
      aBoundingMetrics.rightBearing = size.cx - gm.gmCellIncX + gm.gmptGlyphOrigin.x + gm.gmBlackBoxX;
    }

    if (pstr != str) {
      delete[] pstr;
    }
  }
  return NS_OK;
}

#ifdef NS_DEBUG
void 
nsFontWinUnicode::DumpFontInfo()
{
  printf("FontName: %s @%p\n", mName, this);
  printf("FontType: nsFontWinUnicode\n");
}
#endif // NS_DEBUG
#endif

nsFontWinNonUnicode::nsFontWinNonUnicode(LOGFONT* aLogFont, HFONT aFont,
  PRUint32* aMap, nsIUnicodeEncoder* aConverter) : nsFontWin(aLogFont, aFont, aMap)
{
  mConverter = aConverter;
}

nsFontWinNonUnicode::~nsFontWinNonUnicode()
{
}

PRInt32
nsFontWinNonUnicode::GetWidth(HDC aDC, const PRUnichar* aString,
  PRUint32 aLength)
{
  if (mConverter && aString && 0 < aLength) {
    char str[CHAR_BUFFER_SIZE];
    char* pstr = str;
    if (aLength > CHAR_BUFFER_SIZE) {
      pstr = new char[aLength];
      if (!pstr) return 0;
    }
    PRInt32 srcLength = aLength, destLength = aLength;
    mConverter->Convert(aString, &srcLength, pstr, &destLength);

    SIZE size;
    ::GetTextExtentPoint32A(aDC, pstr, aLength, &size);

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
  if (mConverter && aString && 0 < aLength) {
    char str[CHAR_BUFFER_SIZE];
    char* pstr = str;
    if (aLength > CHAR_BUFFER_SIZE) {
      pstr = new char[aLength];
      if (!pstr) return;
    }
    PRInt32 srcLength = aLength, destLength = aLength;
    mConverter->Convert(aString, &srcLength, pstr, &destLength);

    ::ExtTextOutA(aDC, aX, aY, 0, NULL, pstr, aLength, NULL);

    if (pstr != str) {
      delete[] pstr;
    }
  }
}

#ifdef MOZ_MATHML
nsresult
nsFontWinNonUnicode::GetBoundingMetrics(HDC                aDC, 
                                        const PRUnichar*   aString,
                                        PRUint32           aLength,
                                        nsBoundingMetrics& aBoundingMetrics)
{
  aBoundingMetrics.Clear();
  if (mConverter && aString && 0 < aLength) {
    char str[CHAR_BUFFER_SIZE];
    char* pstr = str;
    if (aLength > CHAR_BUFFER_SIZE) {
      pstr = new char[aLength];
      if (!pstr) return NS_ERROR_OUT_OF_MEMORY;
    }
    PRInt32 srcLength = aLength, destLength = aLength;
    mConverter->Convert(aString, &srcLength, pstr, &destLength);

    // measure the string
    nscoord descent;
    GLYPHMETRICS gm;
    DWORD len = gGlyphAgent.GetGlyphMetrics(aDC, PRUint8(pstr[0]), &gm);
    if (GDI_ERROR == len) {
      return NS_ERROR_UNEXPECTED;
    }
    // flip sign of descent for cross-platform compatibility
    descent = -(nscoord(gm.gmptGlyphOrigin.y) - nscoord(gm.gmBlackBoxY));
    aBoundingMetrics.leftBearing = gm.gmptGlyphOrigin.x;
    aBoundingMetrics.rightBearing = gm.gmptGlyphOrigin.x + gm.gmBlackBoxX;
    aBoundingMetrics.ascent = gm.gmptGlyphOrigin.y;
    aBoundingMetrics.descent = descent;
    aBoundingMetrics.width = gm.gmCellIncX;

    if (1 < aLength) {
      // loop over each glyph to get the ascent and descent
      PRUint32 i;
      for (i = 1; i < aLength; i++) {
        len = gGlyphAgent.GetGlyphMetrics(aDC, PRUint8(pstr[i]), &gm);
        if (GDI_ERROR == len) {
          return NS_ERROR_UNEXPECTED;
        }
        // flip sign of descent for cross-platform compatibility
        descent = -(nscoord(gm.gmptGlyphOrigin.y) - nscoord(gm.gmBlackBoxY));
        if (aBoundingMetrics.ascent < gm.gmptGlyphOrigin.y)
          aBoundingMetrics.ascent = gm.gmptGlyphOrigin.y;
        if (aBoundingMetrics.descent < descent)
          aBoundingMetrics.descent = descent;
      }
      // get the final rightBearing and width. Possible kerning is taken into account.
      SIZE size;
      ::GetTextExtentPointA(aDC, pstr, aLength, &size);
      aBoundingMetrics.width = size.cx;
      aBoundingMetrics.rightBearing = size.cx - gm.gmCellIncX + gm.gmBlackBoxX;
    }

    if (pstr != str) {
      delete[] pstr;
    }
  }
  return NS_OK;
}

#ifdef NS_DEBUG
void 
nsFontWinNonUnicode::DumpFontInfo()
{
  printf("FontName: %s @%p\n", mName, this);
  printf("FontType: nsFontWinNonUnicode\n");
}
#endif // NS_DEBUG
#endif

nsFontWinSubstitute::nsFontWinSubstitute(LOGFONT* aLogFont, HFONT aFont,
  PRUint32* aMap, PRBool aDisplayUnicode) : nsFontWin(aLogFont, aFont, aMap)
{
  mDisplayUnicode = aDisplayUnicode;
}

nsFontWinSubstitute::~nsFontWinSubstitute()
{
}

static PRUnichar*
SubstituteChars(PRBool           aDisplayUnicode,
                const PRUnichar* aString, 
                PRUint32         aLength,
                PRUnichar*       aBuffer, 
                PRUint32         aBufferLength, 
                PRUint32*        aCount)
{
  PRUnichar* result = aBuffer;
  if (aDisplayUnicode) {
    if (aLength*8 > aBufferLength) {
      result = new PRUnichar[aLength*8];  //8 is the length of "&x#NNNN;"
      if (!result) return nsnull;
    }
    char cbuf[10];
    PRUint32 count = 0;
    for (PRUint32 i = 0; i < aLength; i++) {
      // the substitute font should also have glyphs for '&', '#', 'x', ';' and digits
      PR_snprintf(cbuf, sizeof(cbuf), "&#x%04X;", aString[i]);
      for (PRUint32 j = 0; j < 8; j++)
        result[count++] = PRUnichar(cbuf[j]); 
    }
    *aCount = count;
  }
  else { // !aDisplayUnicode
    if (aLength > aBufferLength) {
      result = new PRUnichar[aLength];
      if (!result) return nsnull;
    }
    for (PRUint32 i = 0; i < aLength; i++)
      result[i] = NS_REPLACEMENT_CHAR;
    *aCount = aLength;
  }
  return result;
}

PRInt32
nsFontWinSubstitute::GetWidth(HDC aDC, const PRUnichar* aString,
  PRUint32 aLength)
{
  if (aString && 0 < aLength) {
    PRUnichar str[CHAR_BUFFER_SIZE];
    PRUnichar* pstr = SubstituteChars(mDisplayUnicode,
                             aString, aLength, str, CHAR_BUFFER_SIZE, &aLength);
    if (!pstr) return 0;

    SIZE size;
    ::GetTextExtentPoint32W(aDC, pstr, aLength, &size);

    if (pstr != str) {
      delete[] pstr;
    }

    return size.cx;
  }

  return 0;
}

void
nsFontWinSubstitute::DrawString(HDC aDC, PRInt32 aX, PRInt32 aY,
  const PRUnichar* aString, PRUint32 aLength)
{
  if (aString && 0 < aLength) {
    PRUnichar str[CHAR_BUFFER_SIZE];
    PRUnichar* pstr = SubstituteChars(mDisplayUnicode,
                             aString, aLength, str, CHAR_BUFFER_SIZE, &aLength);
    if (!pstr) return;

    ::ExtTextOutW(aDC, aX, aY, 0, NULL, pstr, aLength, NULL);

    if (pstr != str) {
      delete[] pstr;
    }
  }
}

#ifdef MOZ_MATHML
nsresult
nsFontWinSubstitute::GetBoundingMetrics(HDC                aDC, 
                                        const PRUnichar*   aString,
                                        PRUint32           aLength,
                                        nsBoundingMetrics& aBoundingMetrics)
{
  aBoundingMetrics.Clear();
  if (aString && 0 < aLength) {
    PRUnichar str[CHAR_BUFFER_SIZE];
    PRUnichar* pstr = SubstituteChars(mDisplayUnicode,
                             aString, aLength, str, CHAR_BUFFER_SIZE, &aLength);
    if (!pstr) return NS_ERROR_OUT_OF_MEMORY;
    PRUint16 s[CHAR_BUFFER_SIZE];
    PRUint16* ps = (PRUint16*)&s;

    // at this stage, the glyph agent should have already been initialized
    // given that it was used to compute the x-height in RealizeFont()
    NS_ASSERTION(gGlyphAgent.GetState() != eGlyphAgent_UNKNOWN, "Glyph agent is not yet initialized");
    if (gGlyphAgent.GetState() != eGlyphAgent_UNICODE) {
      // we are on a platform that doesn't implement GetGlyphOutlineW() 
      // we need to use glyph indices
      ps = GetGlyphIndices(aDC, &mCMAP, str, aLength, s, CHAR_BUFFER_SIZE);
      if (!ps) {
        return NS_ERROR_UNEXPECTED;
      }
    }

    // measure the string
    nscoord descent;
    GLYPHMETRICS gm;
    DWORD len = gGlyphAgent.GetGlyphMetrics(aDC, pstr[0], ps[0], &gm);
    if (GDI_ERROR == len) {
      return NS_ERROR_UNEXPECTED;
    }
    // flip sign of descent for cross-platform compatibility
    descent = -(nscoord(gm.gmptGlyphOrigin.y) - nscoord(gm.gmBlackBoxY));
    aBoundingMetrics.leftBearing = gm.gmptGlyphOrigin.x;
    aBoundingMetrics.rightBearing = gm.gmptGlyphOrigin.x + gm.gmBlackBoxX;
    aBoundingMetrics.ascent = gm.gmptGlyphOrigin.y;
    aBoundingMetrics.descent = descent;
    aBoundingMetrics.width = gm.gmCellIncX;

    if (1 < aLength) {
      // loop over each glyph to get the ascent and descent
      PRUint32 i;
      for (i = 1; i < aLength; i++) {
        len = gGlyphAgent.GetGlyphMetrics(aDC, pstr[i], ps[i], &gm);        
        if (GDI_ERROR == len) {
          return NS_ERROR_UNEXPECTED;
        }
        // flip sign of descent for cross-platform compatibility
        descent = -(nscoord(gm.gmptGlyphOrigin.y) - nscoord(gm.gmBlackBoxY));
        if (aBoundingMetrics.ascent < gm.gmptGlyphOrigin.y)
          aBoundingMetrics.ascent = gm.gmptGlyphOrigin.y;
        if (aBoundingMetrics.descent < descent)
          aBoundingMetrics.descent = descent;
      }
      // get the final rightBearing and width. Possible kerning is taken into account.
      SIZE size;
      ::GetTextExtentPointW(aDC, pstr, aLength, &size);
      aBoundingMetrics.width = size.cx;
      aBoundingMetrics.rightBearing = size.cx - gm.gmCellIncX + gm.gmptGlyphOrigin.x + gm.gmBlackBoxX;
    }

    if (pstr != str) {
      delete[] pstr;
    }
    if (ps != s) {
      delete[] ps;
    }
  }
  return NS_OK;
}

#ifdef NS_DEBUG
void 
nsFontWinSubstitute::DumpFontInfo()
{
  printf("FontName: %s @%p\n", mName, this);
  printf("FontType: nsFontWinSubstitute\n");
}
#endif // NS_DEBUG
#endif

static void
GenerateDefault(nsCharSetInfo* aSelf)
{ 
#ifdef NS_DEBUG
  printf("%s defaulted\n", aSelf->mName);
#endif
  PRUint32* map = aSelf->mMap;
  for (int i = 0; i < 2048; i++) {
    map[i] = 0xFFFFFFFF;
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
#ifdef NS_DEBUG
  if (len != 256) {
    printf("%s: MultiByteToWideChar returned %d\n", aSelf->mName, len);
  }
#endif
  PRUint32* map = aSelf->mMap;
  for (i = 0; i < 256; i++) {
    ADD_GLYPH(map, wc[i]);
  }
}

static void
GenerateMultiByte(nsCharSetInfo* aSelf)
{ 
  PRUint32* map = aSelf->mMap;
  for (PRUint16 c = 0; c < 0xFFFF; c++) {
    BOOL defaulted = FALSE;
    WideCharToMultiByte(aSelf->mCodePage, 0, &c, 1, nsnull, 0, nsnull,
      &defaulted);
    if (!defaulted) {
      ADD_GLYPH(map, c);
    }
  }
}

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

  mSubsets = (nsFontSubset**) PR_Calloc(mSubsetsCount, sizeof(nsFontSubset*));
  if (!mSubsets) {
    mSubsetsCount = 0;
    return 0;
  }
  int j;
  for (j = 0; j < mSubsetsCount; j++) {
    mSubsets[j] = new nsFontSubset();
    if (!mSubsets[j]) {
      for (j = j - 1; j >= 0; j--) {
        delete mSubsets[j];
      }
      PR_Free(mSubsets);
      mSubsets = nsnull;
      mSubsetsCount = 0;
      return 0;
    }
  }

  i = 0;
  j = 0;
  for (dword = 0; dword < 2; dword++) {
    for (int bit = 0; bit < sizeof(DWORD) * 8; bit++) {
      if ((array[dword] >> bit) & 1) {
        PRUint8 charSet = bitToCharSet[i];
        if (charSet != DEFAULT_CHARSET) {
          if (HaveConverterFor(charSet)) {
            mSubsets[j]->mCharSet = charSet;
            j++;
          }
        }
      }
      i++;
    }
  }

  return 1;
}

nsFontSubset::nsFontSubset()
  : nsFontWin(nsnull, nsnull, nsnull)
{
}

nsFontSubset::~nsFontSubset()
{
  if (mMap) {
    PR_Free(mMap);
    mMap = nsnull;
  }
}

PRInt32
nsFontSubset::GetWidth(HDC aDC, const PRUnichar* aString, PRUint32 aLength)
{
  // XXX allocate on heap if string is too long
  char str[1024];
  int len = WideCharToMultiByte(mCodePage, 0, aString, aLength, str,
    sizeof(str), nsnull, nsnull);
  if (len) {
    SIZE size;
    ::GetTextExtentPoint32A(aDC, str, len, &size);
    return size.cx;
  }

  return 0;
}

void
nsFontSubset::DrawString(HDC aDC, PRInt32 aX, PRInt32 aY,
  const PRUnichar* aString, PRUint32 aLength)
{
  // XXX allocate on heap if string is too long
  char str[1024];
  int len = WideCharToMultiByte(mCodePage, 0, aString, aLength, str,
    sizeof(str), nsnull, nsnull);
  if (len) {
    ::ExtTextOutA(aDC, aX, aY, 0, NULL, str, len, NULL);
  }
}

#ifdef MOZ_MATHML
nsresult
nsFontSubset::GetBoundingMetrics(HDC                aDC, 
                                 const PRUnichar*   aString,
                                 PRUint32           aLength,
                                 nsBoundingMetrics& aBoundingMetrics)
{
  aBoundingMetrics.Clear();
  if (aString && 0 < aLength) {
    char str[CHAR_BUFFER_SIZE];
    char* pstr = str;
    // Get number of bytes needed for the conversion
    int nb;
    nb = WideCharToMultiByte(mCodePage, 0, aString, aLength,
                             pstr, 0, nsnull, nsnull);
    if (!nb) return NS_ERROR_UNEXPECTED;
    if (nb > CHAR_BUFFER_SIZE) {
      pstr = new char[nb];
      if (!pstr) return NS_ERROR_OUT_OF_MEMORY;
    }
    // Convert the string Unicode to ANSI
    nb = WideCharToMultiByte(mCodePage, 0, aString, aLength,
                             pstr, nb, nsnull, nsnull);
    if (!nb) return NS_ERROR_UNEXPECTED;
    nb--; //ignore the null terminator

    HFONT oldFont = (HFONT) ::SelectObject(aDC, mFont);

    // measure the string
    nscoord descent;
    GLYPHMETRICS gm;
    DWORD len = gGlyphAgent.GetGlyphMetrics(aDC, PRUint8(pstr[0]), &gm);
    if (GDI_ERROR == len) {
      return NS_ERROR_UNEXPECTED;
    }
    // flip sign of descent for cross-platform compatibility
    descent = -(nscoord(gm.gmptGlyphOrigin.y) - nscoord(gm.gmBlackBoxY));
    aBoundingMetrics.leftBearing = gm.gmptGlyphOrigin.x;
    aBoundingMetrics.rightBearing = gm.gmptGlyphOrigin.x + gm.gmBlackBoxX;
    aBoundingMetrics.ascent = gm.gmptGlyphOrigin.y;
    aBoundingMetrics.descent = descent;
    aBoundingMetrics.width = gm.gmCellIncX;

    if (1 < nb) {
      // loop over each glyph to get the ascent and descent
      int i;
      for (i = 1; i < nb; i++) {
        len = gGlyphAgent.GetGlyphMetrics(aDC, PRUint8(pstr[0]), &gm);
        if (GDI_ERROR == len) {
          return NS_ERROR_UNEXPECTED;
        }
        // flip sign of descent for cross-platform compatibility
        descent = -(nscoord(gm.gmptGlyphOrigin.y) - nscoord(gm.gmBlackBoxY));
        if (aBoundingMetrics.ascent < gm.gmptGlyphOrigin.y)
          aBoundingMetrics.ascent = gm.gmptGlyphOrigin.y;
        if (aBoundingMetrics.descent > descent)
          aBoundingMetrics.descent = descent;
      }
      // get the final rightBearing and width. Possible kerning is taken into account.

      SIZE size;
      ::GetTextExtentPointA(aDC, pstr, aLength, &size);
      aBoundingMetrics.width = size.cx;
      aBoundingMetrics.rightBearing = size.cx - gm.gmCellIncX + gm.gmBlackBoxX;

    }

    if (pstr != str) {
      delete[] pstr;
    }

    ::SelectObject(aDC, oldFont);
  }
  return NS_OK;
}

#ifdef NS_DEBUG
void 
nsFontSubset::DumpFontInfo()
{
  printf("FontName: %s @%p\n", mName, this);
  printf("FontType: nsFontSubset\n");
}
#endif // NS_DEBUG
#endif

nsFontWinA::nsFontWinA(LOGFONT* aLogFont, HFONT aFont, PRUint32* aMap)
  : nsFontWin(aLogFont, aFont, aMap)
{
  NS_ASSERTION(aLogFont, "must pass LOGFONT here");
  if (aLogFont) {
    mLogFont = *aLogFont;
  }
}

nsFontWinA::~nsFontWinA()
{
  if (mSubsets) {
    nsFontSubset** subset = mSubsets;
    nsFontSubset** endSubsets = &mSubsets[mSubsetsCount];
    while (subset < endSubsets) {
      delete *subset;
      subset++;
    }
    PR_Free(mSubsets);
    mSubsets = nsnull;
  }
}

PRInt32
nsFontWinA::GetWidth(HDC aDC, const PRUnichar* aString, PRUint32 aLength)
{
  NS_ASSERTION(0, "must call nsFontSubset's GetWidth");
  return 0;
}

void
nsFontWinA::DrawString(HDC aDC, PRInt32 aX, PRInt32 aY,
  const PRUnichar* aString, PRUint32 aLength)
{
  NS_ASSERTION(0, "must call nsFontSubset's DrawString");
}

#ifdef MOZ_MATHML
nsresult
nsFontWinA::GetBoundingMetrics(HDC                aDC, 
                               const PRUnichar*   aString,
                               PRUint32           aLength,
                               nsBoundingMetrics& aBoundingMetrics)
{
  NS_ASSERTION(0, "must call nsFontSubset's GetBoundingMetrics");
  return NS_ERROR_FAILURE;
}

#ifdef NS_DEBUG
void 
nsFontWinA::DumpFontInfo()
{
  NS_ASSERTION(0, "must call nsFontSubset's DumpFontInfo");
}
#endif // NS_DEBUG
#endif

nsFontWin*
nsFontMetricsWinA::LoadFont(HDC aDC, nsString* aName)
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
  WideCharToMultiByte(CP_ACP, 0, aName->get(), aName->Length() + 1,
    logFont.lfFaceName, sizeof(logFont.lfFaceName), nsnull, nsnull);

  /*
   * According to http://msdn.microsoft.com/library/
   * CreateFontIndirectW is only supported on NT/2000
   */
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
    HFONT oldFont = (HFONT) ::SelectObject(aDC, (HGDIOBJ) hfont);
    char name[sizeof(logFont.lfFaceName)];
    if ((!::GetTextFace(aDC, sizeof(name), name)) ||
        strcmp(name, logFont.lfFaceName)) {
      ::SelectObject(aDC, (HGDIOBJ) oldFont);
      ::DeleteObject(hfont);
      return nsnull;
    }
    PRUint32* map = GetCMAP(aDC, logFont.lfFaceName, nsnull, nsnull);
    if (!map) {
      ::SelectObject(aDC, (HGDIOBJ) oldFont);
      ::DeleteObject(hfont);
      return nsnull;
    }
    nsFontWinA* font = new nsFontWinA(&logFont, hfont, map);
    if (!font) {
      ::SelectObject(aDC, (HGDIOBJ) oldFont);
      ::DeleteObject(hfont);
      return nsnull;
    }
#ifdef DEBUG_FONT_SIGNATURE
    printf("%s\n", logFont.lfFaceName);
#endif
    if (!font->GetSubsets(aDC)) {
      delete font;
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

nsFontWin*
nsFontMetricsWinA::LoadGlobalFont(HDC aDC, nsGlobalFont* aGlobalFontItem)
{
  /*
   * According to http://msdn.microsoft.com/library/
   * CreateFontIndirectW is only supported on NT/2000
   */
  LOGFONT logFont;
  HFONT hfont;

  PRUint16 weightTable = LookForFontWeightTable(aDC, aGlobalFontItem->name);
  PRInt32 weight = GetFontWeight(mFont->weight, weightTable);
  FillLogFont(&logFont, weight);
  logFont.lfCharSet = aGlobalFontItem->logFont.lfCharSet;
  logFont.lfPitchAndFamily = aGlobalFontItem->logFont.lfPitchAndFamily;
  strcpy(logFont.lfFaceName, aGlobalFontItem->logFont.lfFaceName);;
  hfont = ::CreateFontIndirect(&logFont);

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

    nsFontWinA* font = new nsFontWinA(&logFont, hfont, aGlobalFontItem->map);
    if (!font)
      return nsnull;

    if (!font->GetSubsets(aDC)) {
      delete font;
      return nsnull;
    }
    mLoadedFonts[mLoadedFontsCount++] = font;

    return font;
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
    PRUint32* charSetMap = gCharSetInfo[i].mMap;
    if (!charSetMap) {
      charSetMap = (PRUint32*) PR_Calloc(2048, 4);
      if (charSetMap) {
        gCharSetInfo[i].mMap = charSetMap;
        gCharSetInfo[i].GenerateMap(&gCharSetInfo[i]);
      }
      else {
        ::DeleteObject(hfont);
        return 0;
      }
    }
    mMap = (PRUint32*) PR_Calloc(2048, 4);
    if (!mMap) {
      ::DeleteObject(hfont);
      return 0;
    }
    PRUint32* fontMap = aFont->mMap;
    for (int j = 0; j < 2048; j++) {
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

  while (mFontsIndex < mFonts.Count()) {
    if (mFontIsGeneric[mFontsIndex]) {
      return nsnull;
    }
    nsString* name = mFonts.StringAt(mFontsIndex++);
    nsAutoString low(*name);
    low.ToLowerCase();
    nsString* winName = (nsString*) PL_HashTableLookup(gFamilyNames, &low);
    if (!winName) {
      winName = name;
    }
    nsFontWinA* font = (nsFontWinA*) LoadFont(aDC, winName);
    if (font && FONT_HAS_GLYPH(font->mMap, aChar)) {
      nsFontSubset** subset = font->mSubsets;
      nsFontSubset** endSubsets = &(font->mSubsets[font->mSubsetsCount]);
      while (subset < endSubsets) {
        if (!(*subset)->mMap) {
          if (!(*subset)->Load(font)) {
            subset++;
            continue;
          }
        }
        if (FONT_HAS_GLYPH((*subset)->mMap, aChar)) {
          return *subset;
        }
        subset++;
      }
    }
  }

  return nsnull;
}

nsFontWin*
nsFontMetricsWinA::LoadGenericFont(HDC aDC, PRUnichar aChar, char** aName)
{
  if (*aName) {
    int found = 0;
    int i;
    for (i = 0; i < mLoadedFontsCount; i++) {
      nsFontWin* font = mLoadedFonts[i];
      if (!strcmp(font->mName, *aName)) {
        found = 1;
        break;
      }
    }
    if (found) {
      nsMemory::Free(*aName);
      *aName = nsnull;
      return nsnull;
    }
    PRUnichar name[LF_FACESIZE] = { 0 };
    PRUnichar format[] = { '%', 's', 0 };
    PRUint32 n = nsTextFormatter::snprintf(name, LF_FACESIZE, format, *aName);
    nsMemory::Free(*aName);
    *aName = nsnull;
    if (n && (n != (PRUint32) -1)) {
      nsAutoString fontName(name);

      nsFontWinA* font = (nsFontWinA*) LoadFont(aDC, &fontName);
      if (font && FONT_HAS_GLYPH(font->mMap, aChar)) {
        nsFontSubset** subset = font->mSubsets;
        nsFontSubset** endSubsets = &(font->mSubsets[font->mSubsetsCount]);
        while (subset < endSubsets) {
          if (!(*subset)->mMap) {
            if (!(*subset)->Load(font)) {
              subset++;
              continue;
            }
          }
          if (FONT_HAS_GLYPH((*subset)->mMap, aChar)) {
            return *subset;
          }
          subset++;
        }
      }
    }
  }

  return nsnull;
}

static int
SystemSupportsChar(PRUnichar aChar)
{
  int i;
  for (i = 0; i < sizeof(bitToCharSet); i++) {
    PRUint8 charSet = bitToCharSet[i];
    if (charSet != DEFAULT_CHARSET) {
      if (HaveConverterFor(charSet)) {
        int j = gCharSetToIndex[charSet];
        PRUint32* charSetMap = gCharSetInfo[j].mMap;
        if (!charSetMap) {
          charSetMap = (PRUint32*) PR_Calloc(2048, 4);
          if (charSetMap) {
            gCharSetInfo[j].mMap = charSetMap;
            gCharSetInfo[j].GenerateMap(&gCharSetInfo[j]);
          }
          else {
            return 0;
          }
        }
        if (FONT_HAS_GLYPH(charSetMap, aChar)) {
          return 1;
        }
      }
    }
  }

  return 0;
}

nsFontWin*
nsFontMetricsWinA::FindGlobalFont(HDC aDC, PRUnichar c)
{
  if (!gGlobalFonts) {
    if (!InitializeGlobalFonts(aDC)) {
      return nsnull;
    }
  }
  if (!SystemSupportsChar(c)) {
    return nsnull;
  }
  for (int i = 0; i < gGlobalFontsCount; i++) {
    if (gGlobalFonts[i].flags & NS_GLOBALFONT_SKIP) {
      continue;
    }
    if (!gGlobalFonts[i].map) {
      HFONT font = ::CreateFontIndirect(&gGlobalFonts[i].logFont);
      if (!font) {
        continue;
      }
      HFONT oldFont = (HFONT) ::SelectObject(aDC, font);
      gGlobalFonts[i].map = GetCMAP(aDC, gGlobalFonts[i].logFont.lfFaceName,
        nsnull, nsnull);
      ::SelectObject(aDC, oldFont);
      ::DeleteObject(font);
      if (!gGlobalFonts[i].map || gGlobalFonts[i].map == gEmptyMap) {
        gGlobalFonts[i].flags |= NS_GLOBALFONT_SKIP;
        continue;
      }
      if (SameAsPreviousMap(i)) {
        continue;
      }
    }
    if (FONT_HAS_GLYPH(gGlobalFonts[i].map, c)) {
      nsFontWinA* font = (nsFontWinA*) LoadFont(aDC, gGlobalFonts[i].name);
      if (font) {
        nsFontSubset** subset = font->mSubsets;
        nsFontSubset** endSubsets = &(font->mSubsets[font->mSubsetsCount]);
        while (subset < endSubsets) {
          if (!(*subset)->mMap) {
            if (!(*subset)->Load(font)) {
              subset++;
              continue;
            }
          }
          if (FONT_HAS_GLYPH((*subset)->mMap, c)) {
            return *subset;
          }
          subset++;
        }
        mLoadedFontsCount--;
        delete font;
      }
    }
  }

  return nsnull;
}

nsFontWin*
nsFontMetricsWinA::FindSubstituteFont(HDC aDC, PRUnichar aChar)
{
  // XXX Write me.
  return nsnull;
}

nsFontWin*
nsFontMetricsWinA::LoadSubstituteFont(HDC aDC, nsString* aName)
{
  // XXX Write me.
  return nsnull;
}

// The Font Enumerator

nsFontEnumeratorWin::nsFontEnumeratorWin()
{
  NS_INIT_REFCNT();
}

NS_IMPL_ISUPPORTS1(nsFontEnumeratorWin,nsIFontEnumerator)

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
  if (aCount) {
    *aCount = 0;
  }
  else {
    return NS_ERROR_NULL_POINTER;
  }
  if (aResult) {
    *aResult = nsnull;
  }
  else {
    return NS_ERROR_NULL_POINTER;
  }

  if (!gInitializedFontEnumerator) {
    if (!InitializeFontEnumerator()) {
      return NS_ERROR_FAILURE;
    }
  }

  PRUnichar** array = (PRUnichar**)
    nsMemory::Alloc(nsFontMetricsWin::gGlobalFontsCount * sizeof(PRUnichar*));
  if (!array) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  for (int i = 0; i < nsFontMetricsWin::gGlobalFontsCount; i++) {
    PRUnichar* str = nsFontMetricsWin::gGlobalFonts[i].name->ToNewUnicode();
    if (!str) {
      for (i = i - 1; i >= 0; i--) {
        nsMemory::Free(array[i]);
      }
      nsMemory::Free(array);
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
  if ((!aLangGroup) || (!aGeneric)) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aCount) {
    *aCount = 0;
  }
  else {
    return NS_ERROR_NULL_POINTER;
  }
  if (aResult) {
    *aResult = nsnull;
  }
  else {
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
    nsMemory::Alloc(nsFontMetricsWin::gGlobalFontsCount * sizeof(PRUnichar*));
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
          nsMemory::Free(array[j]);
        }
        nsMemory::Free(array);
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

NS_IMETHODIMP
nsFontEnumeratorWin::HaveFontFor(const char* aLangGroup, PRBool* aResult)
{
  if (aResult) {
    *aResult = PR_FALSE;
  }
  else {
    return NS_ERROR_NULL_POINTER;
  }
  if ((!aLangGroup)) {
    return NS_ERROR_NULL_POINTER;
  }
  if ((!strcmp(aLangGroup, "x-unicode")) ||
      (!strcmp(aLangGroup, "x-user-def"))) {
    *aResult = PR_TRUE;
    return NS_OK;
  }
  if (!gInitializedFontEnumerator) {
    if (!InitializeFontEnumerator()) {
      return NS_ERROR_FAILURE;
    }
  }
  for (int i = 0; i < nsFontMetricsWin::gGlobalFontsCount; i++) {
    if (SignatureMatchesLangGroup(&nsFontMetricsWin::gGlobalFonts[i].signature,
                                  aLangGroup))
    {
       *aResult = PR_TRUE;
       return NS_OK;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsFontEnumeratorWin::UpdateFontList(PRBool *updateFontList)
{
  PRBool haveFontForLang = PR_FALSE;
  int charSetCounter = 0;
  PRUint16 maskBitBefore = 0;

  // initialize updateFontList
  *updateFontList = PR_FALSE;

  // iterate langGoup; skip DEFAULT
  for (charSetCounter = 1; charSetCounter < eCharSet_COUNT; charSetCounter++) {
    HaveFontFor(gCharSetInfo[charSetCounter].mLangGroup, &haveFontForLang);
    if (haveFontForLang)  {
      maskBitBefore |= PR_BIT(charSetCounter);
      haveFontForLang = PR_FALSE;
    }
  }
  
  // delete gGlobalFont
  if (nsFontMetricsWin::gGlobalFonts) {
    for (int i = 0; i < nsFontMetricsWin::gGlobalFontsCount; i++) {
      delete nsFontMetricsWin::gGlobalFonts[i].name;
    }
    PR_Free(nsFontMetricsWin::gGlobalFonts);
    nsFontMetricsWin::gGlobalFonts = nsnull;
    gGlobalFontsAlloc = 0;
    nsFontMetricsWin::gGlobalFontsCount = 0;
    gInitializedGlobalFonts = 0;
  }

  // reconstruct gGlobalFont
  HDC dc = ::GetDC(nsnull);
  if (!nsFontMetricsWin::InitializeGlobalFonts(dc)) {
    ::ReleaseDC(nsnull, dc);
    return NS_ERROR_FAILURE;
  }
  ::ReleaseDC(nsnull, dc);
  
  PRUint16 maskBitAfter = 0;
  // iterate langGoup again; skip DEFAULT
  for (charSetCounter = 1; charSetCounter < eCharSet_COUNT; charSetCounter++) {
    HaveFontFor(gCharSetInfo[charSetCounter].mLangGroup, &haveFontForLang);
    if (haveFontForLang)  {
      maskBitAfter |= PR_BIT(charSetCounter);
      haveFontForLang = PR_FALSE;
    }
  }

  // check for change
  *updateFontList = (maskBitBefore != maskBitAfter);
  return NS_OK;
}
