/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
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
 *   Roger B. Sidje <rbs@maths.uq.edu.au>
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
#include "nsILanguageAtomService.h"
#include "nsICharsetConverterManager.h"
#include "nsICharsetConverterManager2.h"
#include "nsICharRepresentable.h"
#include "nsISaveAsCharset.h"
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
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsUnicodeRange.h"

#define DEFAULT_TTF_SYMBOL_ENCODING "windows-1252"

#define NOT_SETUP 0x33
static PRBool gIsWIN95OR98 = NOT_SETUP;

PRBool IsWin95OrWin98()
{
  if (NOT_SETUP == gIsWIN95OR98) {
    OSVERSIONINFO os;
    os.dwOSVersionInfoSize = sizeof(os);
    ::GetVersionEx(&os);
    // XXX This may need tweaking for win98
    if (VER_PLATFORM_WIN32_WINDOWS == os.dwPlatformId) {
      gIsWIN95OR98 = PR_TRUE;
    }
    else {
      gIsWIN95OR98 = PR_FALSE;
    }
  }
  return gIsWIN95OR98;
}

extern PRBool UseAFunctions();

#undef USER_DEFINED
#define USER_DEFINED "x-user-def"

// Note: the replacement char must be a char that can be found in common unicode fonts
#define NS_REPLACEMENT_CHAR  PRUnichar(0x003F) // question mark
//#define NS_REPLACEMENT_CHAR  PRUnichar(0xFFFD) // Unicode's replacement char
//#define NS_REPLACEMENT_CHAR  PRUnichar(0x2370) // Boxed question mark
//#define NS_REPLACEMENT_CHAR  PRUnichar(0x00BF) // Inverted question mark
//#define NS_REPLACEMENT_CHAR  PRUnichar(0x25AD) // White rectangle

enum eCharset
{
  eCharset_DEFAULT = 0,
  eCharset_ANSI,
  eCharset_EASTEUROPE,
  eCharset_RUSSIAN,
  eCharset_GREEK,
  eCharset_TURKISH,
  eCharset_HEBREW,
  eCharset_ARABIC,
  eCharset_BALTIC,
  eCharset_THAI,
  eCharset_SHIFTJIS,
  eCharset_GB2312,
  eCharset_HANGEUL,
  eCharset_CHINESEBIG5,
  eCharset_JOHAB,
  eCharset_COUNT
};

struct nsCharsetInfo
{
  char*    mName;
  PRUint16 mCodePage;
  char*    mLangGroup;
  PRUint16* (*GenerateMap)(nsCharsetInfo* aSelf);
  PRUint16* mCCMap;
};

static PRUint16* GenerateDefault(nsCharsetInfo* aSelf);
static PRUint16* GenerateSingleByte(nsCharsetInfo* aSelf);
static PRUint16* GenerateMultiByte(nsCharsetInfo* aSelf);

static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

nsVoidArray* nsFontMetricsWin::gGlobalFonts = nsnull;
PLHashTable* nsFontMetricsWin::gFamilyNames = nsnull;
PLHashTable* nsFontMetricsWin::gFontWeights = nsnull;
PLHashTable* nsFontMetricsWin::gFontMaps = nsnull;
PRUint16* nsFontMetricsWin::gEmptyCCMap = nsnull;

#define NS_MAX_FONT_WEIGHT 900
#define NS_MIN_FONT_WEIGHT 100


static nsIPersistentProperties* gFontEncodingProperties = nsnull;
static nsICharsetConverterManager2* gCharsetManager = nsnull;
static nsIUnicodeEncoder* gUserDefinedConverter = nsnull;
static nsISaveAsCharset* gFontSubstituteConverter = nsnull;
static nsIPref* gPref = nsnull;
static nsIFontPackageProxy* gFontPackageProxy = nsnull;

static nsIAtom* gUsersLocale = nsnull;
static nsIAtom* gSystemLocale = nsnull;
static nsIAtom* gUserDefined = nsnull;
static nsIAtom* gJA = nsnull;
static nsIAtom* gKO = nsnull;
static nsIAtom* gZHTW = nsnull;
static nsIAtom* gZHCN = nsnull;

static int gInitialized = 0;
static PRBool gDoingLineheightFixup = PR_FALSE;
static PRUint16* gUserDefinedCCMap = nsnull;

static nsCharsetInfo gCharsetInfo[eCharset_COUNT] =
{
  { "DEFAULT",     0,    "",               GenerateDefault,    nsnull},
  { "ANSI",        1252, "x-western",      GenerateSingleByte, nsnull},
  { "EASTEUROPE",  1250, "x-central-euro", GenerateSingleByte, nsnull},
  { "RUSSIAN",     1251, "x-cyrillic",     GenerateSingleByte, nsnull},
  { "GREEK",       1253, "el",             GenerateSingleByte, nsnull},
  { "TURKISH",     1254, "tr",             GenerateSingleByte, nsnull},
  { "HEBREW",      1255, "he",             GenerateSingleByte, nsnull},
  { "ARABIC",      1256, "ar",             GenerateSingleByte, nsnull},
  { "BALTIC",      1257, "x-baltic",       GenerateSingleByte, nsnull},
  { "THAI",        874,  "th",             GenerateSingleByte, nsnull},
  { "SHIFTJIS",    932,  "ja",             GenerateMultiByte,  nsnull},
  { "GB2312",      936,  "zh-CN",          GenerateMultiByte,  nsnull},
  { "HANGEUL",     949,  "ko",             GenerateMultiByte,  nsnull},
  { "CHINESEBIG5", 950,  "zh-TW",          GenerateMultiByte,  nsnull},
  { "JOHAB",       1361, "ko-XXX",         GenerateMultiByte,  nsnull}
};

static void
FreeGlobals(void)
{
  gInitialized = 0;

  NS_IF_RELEASE(gFontEncodingProperties);
  NS_IF_RELEASE(gCharsetManager);
  NS_IF_RELEASE(gPref);
  NS_IF_RELEASE(gUsersLocale);
  NS_IF_RELEASE(gSystemLocale);
  NS_IF_RELEASE(gUserDefined);
  NS_IF_RELEASE(gUserDefinedConverter);
  NS_IF_RELEASE(gFontPackageProxy);
  if (gUserDefinedCCMap)
    FreeCCMap(gUserDefinedCCMap);
  NS_IF_RELEASE(gJA);
  NS_IF_RELEASE(gKO);
  NS_IF_RELEASE(gZHTW);
  NS_IF_RELEASE(gZHCN);

  // free CMap
  if (nsFontMetricsWin::gFontMaps) {
    PL_HashTableDestroy(nsFontMetricsWin::gFontMaps);
    nsFontMetricsWin::gFontMaps = nsnull;
    if (nsFontMetricsWin::gEmptyCCMap) {
      FreeCCMap(nsFontMetricsWin::gEmptyCCMap);
      nsFontMetricsWin::gEmptyCCMap = nsnull;
    }
  }

  if (nsFontMetricsWin::gGlobalFonts) {
    for (int i = nsFontMetricsWin::gGlobalFonts->Count()-1; i >= 0; --i) {
      nsGlobalFont* font = (nsGlobalFont*)nsFontMetricsWin::gGlobalFonts->ElementAt(i);
      delete font;
    }
    delete nsFontMetricsWin::gGlobalFonts;
    nsFontMetricsWin::gGlobalFonts = nsnull;
  }

  // free FamilyNames
  if (nsFontMetricsWin::gFamilyNames) {
    PL_HashTableDestroy(nsFontMetricsWin::gFamilyNames);
    nsFontMetricsWin::gFamilyNames = nsnull;
  }

  // free Font weight
  if (nsFontMetricsWin::gFontWeights) {
    PL_HashTableDestroy(nsFontMetricsWin::gFontWeights);
    nsFontMetricsWin::gFontWeights = nsnull;
  }

  // free generated charset maps
  for (int i = 0; i < eCharset_COUNT; ++i) {
    if (gCharsetInfo[i].mCCMap) {
      FreeCCMap(gCharsetInfo[i].mCCMap);
      gCharsetInfo[i].mCCMap = nsnull;
    }
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

NS_IMETHODIMP nsFontCleanupObserver::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *someData)
{
  if (!nsCRT::strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    FreeGlobals();
  }
  return NS_OK;
}

static nsFontCleanupObserver *gFontCleanupObserver;

static nsresult
InitFontEncodingProperties(void)
{
  nsresult rv;
  // load the special encoding resolver
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
  return rv;
}

static nsresult
InitGlobals(void)
{
  nsServiceManager::GetService(kCharsetConverterManagerCID,
    NS_GET_IID(nsICharsetConverterManager2), (nsISupports**) &gCharsetManager);
  if (!gCharsetManager) {
    FreeGlobals();
    return NS_ERROR_FAILURE;
  }
  nsServiceManager::GetService(kPrefCID, NS_GET_IID(nsIPref),
    (nsISupports**) &gPref);
  if (!gPref) {
    FreeGlobals();
    return NS_ERROR_FAILURE;
  }

  // if we do not include/compensate external leading in calculating normal 
  // line height, we don't set gDoingLineheightFixup either to keep old behavior.
  // These code should be eliminated in future when we choose to stay with 
  // one normal lineheight calculation method.
  PRInt32 intPref;
  if (NS_SUCCEEDED(gPref->GetIntPref(
      "browser.display.normal_lineheight_calc_control", &intPref)))
    gDoingLineheightFixup = (intPref != 0);

  nsCOMPtr<nsILanguageAtomService> langService;
  langService = do_GetService(NS_LANGUAGEATOMSERVICE_CONTRACTID);
  if (langService) {
    langService->GetLocaleLanguageGroup(&gUsersLocale);
  }
  if (!gUsersLocale) {
    gUsersLocale = NS_NewAtom("x-western");
  }
  if (!gUsersLocale) {
    FreeGlobals();
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  if (!gSystemLocale) {
    UINT cp= ::GetACP();
    for (int i = 1; i < eCharset_COUNT; ++i) {
      if (gCharsetInfo[i].mCodePage == cp) {
        gSystemLocale = NS_NewAtom(gCharsetInfo[i].mLangGroup);
        break;
      }
    }
  }
  if (!gSystemLocale) {
    gSystemLocale = gUsersLocale;
    NS_ADDREF(gSystemLocale);
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

  //register an observer to take care of cleanup
  gFontCleanupObserver = new nsFontCleanupObserver();
  NS_ASSERTION(gFontCleanupObserver, "failed to create observer");
  if (gFontCleanupObserver) {
    // register for shutdown
    nsresult rv;
    nsCOMPtr<nsIObserverService> observerService(do_GetService("@mozilla.org/observer-service;1", &rv));
    if (NS_SUCCEEDED(rv)) {
      rv = observerService->AddObserver(gFontCleanupObserver, NS_XPCOM_SHUTDOWN_OBSERVER_ID, PR_FALSE);
    }    
  }
  gInitialized = 1;

  return NS_OK;
}

static void CheckFontLangGroup(nsIAtom* lang1, nsIAtom* lang2, const char* lang3)
{
  if (lang1 == lang2) {
    nsresult res = NS_OK;
    if (!gFontPackageProxy) {
      res = nsServiceManager::GetService("@mozilla.org/intl/fontpackageservice;1",
                    NS_GET_IID(nsIFontPackageProxy), (nsISupports**) &gFontPackageProxy);
      if (NS_FAILED(res)) {
        NS_ERROR("Cannot get the font package proxy");
        return;
      }
    }

    char fontpackageid[256];
    PR_snprintf(fontpackageid, sizeof(fontpackageid), "lang:%s", lang3);
    res = gFontPackageProxy->NeedFontPackage(fontpackageid);
    NS_ASSERTION(NS_SUCCEEDED(res), "cannot notify missing font package ");
  }
}

nsFontMetricsWin::nsFontMetricsWin()
{
  NS_INIT_ISUPPORTS();
}
  
nsFontMetricsWin::~nsFontMetricsWin()
{
  mSubstituteFont = nsnull; // released below
  mFontHandle = nsnull; // released below

  for (PRInt32 i = mLoadedFonts.Count()-1; i >= 0; --i) {
    delete (nsFontWin*)mLoadedFonts[i];
  }
  mLoadedFonts.Clear();

  if (mDeviceContext) {
    // Notify our device context that owns us so that it can update its font cache
    mDeviceContext->FontMetricsDeleted(this);
    mDeviceContext = nsnull;
  }
}

#ifdef LEAK_DEBUG
nsrefcnt
nsFontMetricsWin::AddRef()
{
  NS_PRECONDITION(mRefCnt != 0, "resurrecting a dead object");
  return ++mRefCnt;
}

nsrefcnt
nsFontMetricsWin::Release()
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
nsFontMetricsWin::Init(const nsFont& aFont, nsIAtom* aLangGroup,
  nsIDeviceContext *aContext)
{
  nsresult res;
  if (!gInitialized) {
    res = InitGlobals();
    //XXXrbs this should be a fatal startup error
    NS_ASSERTION(NS_SUCCEEDED(res), "No font at all has been created");
    if (NS_FAILED(res)) {
      return res;
    }
  }

  mFont = aFont;
  mLangGroup = aLangGroup;

  // do special checking for the following lang group
  // * use fonts?
  PRInt32 useDccFonts = 0;
  if (NS_SUCCEEDED(gPref->GetIntPref("browser.display.use_document_fonts", &useDccFonts)) && (useDccFonts != 0)) {
    CheckFontLangGroup(mLangGroup, gJA,   "ja");
    CheckFontLangGroup(mLangGroup, gKO,   "ko");
    CheckFontLangGroup(mLangGroup, gZHTW, "zh-TW");
    CheckFontLangGroup(mLangGroup, gZHCN, "zh-CN");
  }

  //don't addref this to avoid circular refs
  mDeviceContext = (nsDeviceContextWin *)aContext;
  return RealizeFont();
}

NS_IMETHODIMP
nsFontMetricsWin::Destroy()
{
  mDeviceContext = nsnull;
  return NS_OK;
}

// The following flag is not defined by MS in the vc include files
// This flag is document in the following document
// HOWTO: Display Graphic Chars on Chinese & Korean Windows (Q171153)
// http://support.microsoft.com/default.aspx?scid=kb;EN-US;q171153
// According to the document, this flag will only impact Korean and
// Chinese window
#define CLIP_TURNOFF_FONTASSOCIATION 0x40

void
nsFontMetricsWin::FillLogFont(LOGFONT* logFont, PRInt32 aWeight,
  PRBool aSizeOnly)
{
  float app2dev, app2twip, scale;
  mDeviceContext->GetAppUnitsToDevUnits(app2dev);
  if (nsDeviceContextWin::gRound) {
    mDeviceContext->GetDevUnitsToTwips(app2twip);
    mDeviceContext->GetCanonicalPixelScale(scale);
    app2twip *= app2dev * scale;

    // This interesting bit of code rounds the font size off to the floor point
    // value. This is necessary for proper font scaling under windows.
    PRInt32 sizePoints = NSTwipsToFloorIntPoints(nscoord(mFont.size*app2twip));
    float rounded = ((float)NSIntPointsToTwips(sizePoints)) / app2twip;

    // round font size off to floor point size to be windows compatible
    // this is proper (windows) rounding
    logFont->lfHeight = - NSToIntRound(rounded * app2dev);

    // this floor rounding is to make ours compatible with Nav 4.0
    //logFont->lfHeight = - LONG(rounded * app2dev);
  }
  else {
    logFont->lfHeight = - NSToIntRound(mFont.size * app2dev);
  }

  // Quick return if we came here just to compute the font size
  if (aSizeOnly) return;

  // Fill in logFont structure; stolen from awt
  logFont->lfWidth          = 0; 
  logFont->lfEscapement     = 0;
  logFont->lfOrientation    = 0;
  logFont->lfUnderline      =
    (mFont.decorations & NS_FONT_DECORATION_UNDERLINE)
    ? TRUE : FALSE;
  logFont->lfStrikeOut      =
    (mFont.decorations & NS_FONT_DECORATION_LINE_THROUGH)
    ? TRUE : FALSE;
  logFont->lfCharSet        = mIsUserDefined ? ANSI_CHARSET : DEFAULT_CHARSET;
  logFont->lfOutPrecision   = OUT_TT_PRECIS;
  logFont->lfClipPrecision  = CLIP_TURNOFF_FONTASSOCIATION;
  logFont->lfQuality        = DEFAULT_QUALITY;
  logFont->lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
  logFont->lfWeight = aWeight;
  logFont->lfItalic = (mFont.style & (NS_FONT_STYLE_ITALIC | NS_FONT_STYLE_OBLIQUE))
    ? TRUE : FALSE;   // XXX need better oblique support

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

#ifdef IS_BIG_ENDIAN 
# undef GET_SHORT
# define GET_SHORT(p) (*((PRUint16*)p))
# undef GET_LONG
# define GET_LONG(p)  (*((PRUint32*)p))
#else
# ifdef IS_LITTLE_ENDIAN 
#  undef GET_SHORT
#  define GET_SHORT(p) (((p)[0] << 8) | (p)[1])
#  undef GET_LONG
#  define GET_LONG(p) (((p)[0] << 24) | ((p)[1] << 16) | ((p)[2] << 8) | (p)[3])
# endif
#endif


#define AUTO_FONTDATA_BUFFER_SIZE 16384 /* 16K */
#undef CHAR_BUFFER_SIZE
#define CHAR_BUFFER_SIZE 1024

// template for helper classes for temporary buffer allocation
template<class T, PRInt32 sz> class nsAutoArray {
public:
  nsAutoArray();
  ~nsAutoArray();

  T* GetArray(PRInt32 aMinLength = 0);

private:
  T* mArray;
  T  mAutoArray[sz];
  PRInt32  mCount;
};

template<class T, PRInt32 sz> 
nsAutoArray<T, sz>::nsAutoArray()
  : mArray(mAutoArray),
    mCount(sz)
{
}

template<class T, PRInt32 sz> 
nsAutoArray<T, sz>::~nsAutoArray()
{
  if (mArray && (mArray != mAutoArray)) {
    delete [] mArray;
  }
}

template<class T, PRInt32 sz> T*
nsAutoArray<T, sz>::GetArray(PRInt32 aMinCount)
{
  if (aMinCount > mCount) {
    T* newArray = new T[aMinCount];
    if (!newArray) {
      return nsnull;
    }
    if (mArray != mAutoArray) {
      delete [] mArray;
    }
    mArray = newArray;
    mCount = aMinCount;
  }
  return mArray;
}

typedef nsAutoArray<PRUint8, AUTO_FONTDATA_BUFFER_SIZE> nsAutoFontDataBuffer;
typedef nsAutoArray<char, CHAR_BUFFER_SIZE> nsAutoCharBuffer;
typedef nsAutoArray<PRUint16, CHAR_BUFFER_SIZE> nsAutoChar16Buffer;

static PRUint16
GetGlyphIndex(PRUint16 segCount, PRUint16* endCode, PRUint16* startCode,
  PRUint16* idRangeOffset, PRUint16* idDelta, PRUint8* end, PRUint16 aChar)
{
  PRUint16 glyphIndex = 0;
  PRUint16 i;
  for (i = 0; i < segCount; ++i) {
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

enum eGetNameError
{
  eGetName_OK = 0,
  eGetName_GDIError,
  eGetName_OtherError
};

static eGetNameError
GetNAME(HDC aDC, nsString* aName)
{
  DWORD len = GetFontData(aDC, NAME, 0, nsnull, 0);
  if (len == GDI_ERROR) {
    return eGetName_GDIError;
  }
  if (!len) {
    return eGetName_OtherError;
  }
  nsAutoFontDataBuffer buffer;
  PRUint8* buf = buffer.GetArray(len);
  if (!buf) {
    return eGetName_OtherError;
  }
  DWORD newLen = GetFontData(aDC, NAME, 0, buf, len);
  if (newLen != len) {
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
  for (i = 0; i < n; ++i) {
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
    return eGetName_OtherError;
  }
  p = buf + offset + idOffset;
  idLength /= 2;
  for (i = 0; i < idLength; ++i) {
    PRUnichar c = GET_SHORT(p);
    p += 2;
    aName->Append(c);
  }

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

static nsresult
GetSpaces(HDC aDC, PRUint32* aMaxGlyph, nsAutoFontDataBuffer& aIsSpace)
{
  int isLong = GetIndexToLocFormat(aDC);
  if (isLong < 0) {
    return NS_ERROR_FAILURE;
  }
  DWORD len = GetFontData(aDC, LOCA, 0, nsnull, 0);
  if ((len == GDI_ERROR) || (!len)) {
    return NS_ERROR_FAILURE;
  }
  PRUint8* buf = aIsSpace.GetArray(len);
  if (!buf) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  DWORD newLen = GetFontData(aDC, LOCA, 0, buf, len);
  if (newLen != len) {
    return NS_ERROR_FAILURE;
  }
  if (isLong) {
    DWORD longLen = ((len / 4) - 1);
    *aMaxGlyph = longLen;
    PRUint32* longBuf = (PRUint32*) buf;
    for (PRUint32 i = 0; i < longLen; ++i) {
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
    for (PRUint16 i = 0; i < shortLen; ++i) {
      if (shortBuf[i] == shortBuf[i+1]) {
        buf[i] = 1;
      }
      else {
        buf[i] = 0;
      }
    }
  }

  return NS_OK;
}

// The following is a workaround for a Japanese Windows 95 problem.

static PRUint8 gBitToCharset[64] =
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

static eCharset gCharsetToIndex[256] =
{
  /* 000 */ eCharset_ANSI,
  /* 001 */ eCharset_DEFAULT,
  /* 002 */ eCharset_DEFAULT, // SYMBOL
  /* 003 */ eCharset_DEFAULT,
  /* 004 */ eCharset_DEFAULT,
  /* 005 */ eCharset_DEFAULT,
  /* 006 */ eCharset_DEFAULT,
  /* 007 */ eCharset_DEFAULT,
  /* 008 */ eCharset_DEFAULT,
  /* 009 */ eCharset_DEFAULT,
  /* 010 */ eCharset_DEFAULT,
  /* 011 */ eCharset_DEFAULT,
  /* 012 */ eCharset_DEFAULT,
  /* 013 */ eCharset_DEFAULT,
  /* 014 */ eCharset_DEFAULT,
  /* 015 */ eCharset_DEFAULT,
  /* 016 */ eCharset_DEFAULT,
  /* 017 */ eCharset_DEFAULT,
  /* 018 */ eCharset_DEFAULT,
  /* 019 */ eCharset_DEFAULT,
  /* 020 */ eCharset_DEFAULT,
  /* 021 */ eCharset_DEFAULT,
  /* 022 */ eCharset_DEFAULT,
  /* 023 */ eCharset_DEFAULT,
  /* 024 */ eCharset_DEFAULT,
  /* 025 */ eCharset_DEFAULT,
  /* 026 */ eCharset_DEFAULT,
  /* 027 */ eCharset_DEFAULT,
  /* 028 */ eCharset_DEFAULT,
  /* 029 */ eCharset_DEFAULT,
  /* 030 */ eCharset_DEFAULT,
  /* 031 */ eCharset_DEFAULT,
  /* 032 */ eCharset_DEFAULT,
  /* 033 */ eCharset_DEFAULT,
  /* 034 */ eCharset_DEFAULT,
  /* 035 */ eCharset_DEFAULT,
  /* 036 */ eCharset_DEFAULT,
  /* 037 */ eCharset_DEFAULT,
  /* 038 */ eCharset_DEFAULT,
  /* 039 */ eCharset_DEFAULT,
  /* 040 */ eCharset_DEFAULT,
  /* 041 */ eCharset_DEFAULT,
  /* 042 */ eCharset_DEFAULT,
  /* 043 */ eCharset_DEFAULT,
  /* 044 */ eCharset_DEFAULT,
  /* 045 */ eCharset_DEFAULT,
  /* 046 */ eCharset_DEFAULT,
  /* 047 */ eCharset_DEFAULT,
  /* 048 */ eCharset_DEFAULT,
  /* 049 */ eCharset_DEFAULT,
  /* 050 */ eCharset_DEFAULT,
  /* 051 */ eCharset_DEFAULT,
  /* 052 */ eCharset_DEFAULT,
  /* 053 */ eCharset_DEFAULT,
  /* 054 */ eCharset_DEFAULT,
  /* 055 */ eCharset_DEFAULT,
  /* 056 */ eCharset_DEFAULT,
  /* 057 */ eCharset_DEFAULT,
  /* 058 */ eCharset_DEFAULT,
  /* 059 */ eCharset_DEFAULT,
  /* 060 */ eCharset_DEFAULT,
  /* 061 */ eCharset_DEFAULT,
  /* 062 */ eCharset_DEFAULT,
  /* 063 */ eCharset_DEFAULT,
  /* 064 */ eCharset_DEFAULT,
  /* 065 */ eCharset_DEFAULT,
  /* 066 */ eCharset_DEFAULT,
  /* 067 */ eCharset_DEFAULT,
  /* 068 */ eCharset_DEFAULT,
  /* 069 */ eCharset_DEFAULT,
  /* 070 */ eCharset_DEFAULT,
  /* 071 */ eCharset_DEFAULT,
  /* 072 */ eCharset_DEFAULT,
  /* 073 */ eCharset_DEFAULT,
  /* 074 */ eCharset_DEFAULT,
  /* 075 */ eCharset_DEFAULT,
  /* 076 */ eCharset_DEFAULT,
  /* 077 */ eCharset_DEFAULT, // MAC
  /* 078 */ eCharset_DEFAULT,
  /* 079 */ eCharset_DEFAULT,
  /* 080 */ eCharset_DEFAULT,
  /* 081 */ eCharset_DEFAULT,
  /* 082 */ eCharset_DEFAULT,
  /* 083 */ eCharset_DEFAULT,
  /* 084 */ eCharset_DEFAULT,
  /* 085 */ eCharset_DEFAULT,
  /* 086 */ eCharset_DEFAULT,
  /* 087 */ eCharset_DEFAULT,
  /* 088 */ eCharset_DEFAULT,
  /* 089 */ eCharset_DEFAULT,
  /* 090 */ eCharset_DEFAULT,
  /* 091 */ eCharset_DEFAULT,
  /* 092 */ eCharset_DEFAULT,
  /* 093 */ eCharset_DEFAULT,
  /* 094 */ eCharset_DEFAULT,
  /* 095 */ eCharset_DEFAULT,
  /* 096 */ eCharset_DEFAULT,
  /* 097 */ eCharset_DEFAULT,
  /* 098 */ eCharset_DEFAULT,
  /* 099 */ eCharset_DEFAULT,
  /* 100 */ eCharset_DEFAULT,
  /* 101 */ eCharset_DEFAULT,
  /* 102 */ eCharset_DEFAULT,
  /* 103 */ eCharset_DEFAULT,
  /* 104 */ eCharset_DEFAULT,
  /* 105 */ eCharset_DEFAULT,
  /* 106 */ eCharset_DEFAULT,
  /* 107 */ eCharset_DEFAULT,
  /* 108 */ eCharset_DEFAULT,
  /* 109 */ eCharset_DEFAULT,
  /* 110 */ eCharset_DEFAULT,
  /* 111 */ eCharset_DEFAULT,
  /* 112 */ eCharset_DEFAULT,
  /* 113 */ eCharset_DEFAULT,
  /* 114 */ eCharset_DEFAULT,
  /* 115 */ eCharset_DEFAULT,
  /* 116 */ eCharset_DEFAULT,
  /* 117 */ eCharset_DEFAULT,
  /* 118 */ eCharset_DEFAULT,
  /* 119 */ eCharset_DEFAULT,
  /* 120 */ eCharset_DEFAULT,
  /* 121 */ eCharset_DEFAULT,
  /* 122 */ eCharset_DEFAULT,
  /* 123 */ eCharset_DEFAULT,
  /* 124 */ eCharset_DEFAULT,
  /* 125 */ eCharset_DEFAULT,
  /* 126 */ eCharset_DEFAULT,
  /* 127 */ eCharset_DEFAULT,
  /* 128 */ eCharset_SHIFTJIS,
  /* 129 */ eCharset_HANGEUL,
  /* 130 */ eCharset_JOHAB,
  /* 131 */ eCharset_DEFAULT,
  /* 132 */ eCharset_DEFAULT,
  /* 133 */ eCharset_DEFAULT,
  /* 134 */ eCharset_GB2312,
  /* 135 */ eCharset_DEFAULT,
  /* 136 */ eCharset_CHINESEBIG5,
  /* 137 */ eCharset_DEFAULT,
  /* 138 */ eCharset_DEFAULT,
  /* 139 */ eCharset_DEFAULT,
  /* 140 */ eCharset_DEFAULT,
  /* 141 */ eCharset_DEFAULT,
  /* 142 */ eCharset_DEFAULT,
  /* 143 */ eCharset_DEFAULT,
  /* 144 */ eCharset_DEFAULT,
  /* 145 */ eCharset_DEFAULT,
  /* 146 */ eCharset_DEFAULT,
  /* 147 */ eCharset_DEFAULT,
  /* 148 */ eCharset_DEFAULT,
  /* 149 */ eCharset_DEFAULT,
  /* 150 */ eCharset_DEFAULT,
  /* 151 */ eCharset_DEFAULT,
  /* 152 */ eCharset_DEFAULT,
  /* 153 */ eCharset_DEFAULT,
  /* 154 */ eCharset_DEFAULT,
  /* 155 */ eCharset_DEFAULT,
  /* 156 */ eCharset_DEFAULT,
  /* 157 */ eCharset_DEFAULT,
  /* 158 */ eCharset_DEFAULT,
  /* 159 */ eCharset_DEFAULT,
  /* 160 */ eCharset_DEFAULT,
  /* 161 */ eCharset_GREEK,
  /* 162 */ eCharset_TURKISH,
  /* 163 */ eCharset_DEFAULT, // VIETNAMESE
  /* 164 */ eCharset_DEFAULT,
  /* 165 */ eCharset_DEFAULT,
  /* 166 */ eCharset_DEFAULT,
  /* 167 */ eCharset_DEFAULT,
  /* 168 */ eCharset_DEFAULT,
  /* 169 */ eCharset_DEFAULT,
  /* 170 */ eCharset_DEFAULT,
  /* 171 */ eCharset_DEFAULT,
  /* 172 */ eCharset_DEFAULT,
  /* 173 */ eCharset_DEFAULT,
  /* 174 */ eCharset_DEFAULT,
  /* 175 */ eCharset_DEFAULT,
  /* 176 */ eCharset_DEFAULT,
  /* 177 */ eCharset_HEBREW,
  /* 178 */ eCharset_ARABIC,
  /* 179 */ eCharset_DEFAULT,
  /* 180 */ eCharset_DEFAULT,
  /* 181 */ eCharset_DEFAULT,
  /* 182 */ eCharset_DEFAULT,
  /* 183 */ eCharset_DEFAULT,
  /* 184 */ eCharset_DEFAULT,
  /* 185 */ eCharset_DEFAULT,
  /* 186 */ eCharset_BALTIC,
  /* 187 */ eCharset_DEFAULT,
  /* 188 */ eCharset_DEFAULT,
  /* 189 */ eCharset_DEFAULT,
  /* 190 */ eCharset_DEFAULT,
  /* 191 */ eCharset_DEFAULT,
  /* 192 */ eCharset_DEFAULT,
  /* 193 */ eCharset_DEFAULT,
  /* 194 */ eCharset_DEFAULT,
  /* 195 */ eCharset_DEFAULT,
  /* 196 */ eCharset_DEFAULT,
  /* 197 */ eCharset_DEFAULT,
  /* 198 */ eCharset_DEFAULT,
  /* 199 */ eCharset_DEFAULT,
  /* 200 */ eCharset_DEFAULT,
  /* 201 */ eCharset_DEFAULT,
  /* 202 */ eCharset_DEFAULT,
  /* 203 */ eCharset_DEFAULT,
  /* 204 */ eCharset_RUSSIAN,
  /* 205 */ eCharset_DEFAULT,
  /* 206 */ eCharset_DEFAULT,
  /* 207 */ eCharset_DEFAULT,
  /* 208 */ eCharset_DEFAULT,
  /* 209 */ eCharset_DEFAULT,
  /* 210 */ eCharset_DEFAULT,
  /* 211 */ eCharset_DEFAULT,
  /* 212 */ eCharset_DEFAULT,
  /* 213 */ eCharset_DEFAULT,
  /* 214 */ eCharset_DEFAULT,
  /* 215 */ eCharset_DEFAULT,
  /* 216 */ eCharset_DEFAULT,
  /* 217 */ eCharset_DEFAULT,
  /* 218 */ eCharset_DEFAULT,
  /* 219 */ eCharset_DEFAULT,
  /* 220 */ eCharset_DEFAULT,
  /* 221 */ eCharset_DEFAULT,
  /* 222 */ eCharset_THAI,
  /* 223 */ eCharset_DEFAULT,
  /* 224 */ eCharset_DEFAULT,
  /* 225 */ eCharset_DEFAULT,
  /* 226 */ eCharset_DEFAULT,
  /* 227 */ eCharset_DEFAULT,
  /* 228 */ eCharset_DEFAULT,
  /* 229 */ eCharset_DEFAULT,
  /* 230 */ eCharset_DEFAULT,
  /* 231 */ eCharset_DEFAULT,
  /* 232 */ eCharset_DEFAULT,
  /* 233 */ eCharset_DEFAULT,
  /* 234 */ eCharset_DEFAULT,
  /* 235 */ eCharset_DEFAULT,
  /* 236 */ eCharset_DEFAULT,
  /* 237 */ eCharset_DEFAULT,
  /* 238 */ eCharset_EASTEUROPE,
  /* 239 */ eCharset_DEFAULT,
  /* 240 */ eCharset_DEFAULT,
  /* 241 */ eCharset_DEFAULT,
  /* 242 */ eCharset_DEFAULT,
  /* 243 */ eCharset_DEFAULT,
  /* 244 */ eCharset_DEFAULT,
  /* 245 */ eCharset_DEFAULT,
  /* 246 */ eCharset_DEFAULT,
  /* 247 */ eCharset_DEFAULT,
  /* 248 */ eCharset_DEFAULT,
  /* 249 */ eCharset_DEFAULT,
  /* 250 */ eCharset_DEFAULT,
  /* 251 */ eCharset_DEFAULT,
  /* 252 */ eCharset_DEFAULT,
  /* 253 */ eCharset_DEFAULT,
  /* 254 */ eCharset_DEFAULT,
  /* 255 */ eCharset_DEFAULT  // OEM
};

static PRUint8 gCharsetToBit[eCharset_COUNT] =
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
  // this is "MS P Gothic" in Japanese
  static const char* mspgothic=  
    "\x82\x6c\x82\x72 \x82\x6f\x83\x53\x83\x56\x83\x62\x83\x4e";
  // below is a list of common used name for startup
  if ( (!strcmp(aFontName, "Tahoma" )) ||
       (!strcmp(aFontName, "Arial" )) ||
       (!strcmp(aFontName, "Times New Roman" )) ||
       (!strcmp(aFontName, "Courier New" )) ||
       (!strcmp(aFontName, mspgothic )) )
    return NS_ERROR_NOT_AVAILABLE; // error mean do not get a special encoding

  // XXX We need this kludge to deal with aFontName in CP949 when the locale 
  // is Korean until we figure out a way to get facename in US-ASCII
  // regardless of the current locale. We have no control over what 
  // EnumFontFamiliesEx() does.
  nsCAutoString name;
  if ( ::GetACP() != 949) 
    name.Assign(NS_LITERAL_CSTRING("encoding.") + nsDependentCString(aFontName) + NS_LITERAL_CSTRING(".ttf"));
  else {
    PRUnichar fname[LF_FACESIZE];
    fname[0] = 0;
    MultiByteToWideChar(CP_ACP, 0, aFontName,
    strlen(aFontName) + 1, fname, sizeof(fname)/sizeof(fname[0]));
    name.Assign(NS_LITERAL_CSTRING("encoding.") + NS_ConvertUCS2toUTF8(fname) + NS_LITERAL_CSTRING(".ttf"));
  }

  name.StripWhitespace();
  ToLowerCase(name);

  // if we have not init the property yet, init it right now.
  if (! gFontEncodingProperties)
    InitFontEncodingProperties();

  if (gFontEncodingProperties)
    return gFontEncodingProperties->GetStringProperty(name, aValue);
  return NS_ERROR_NOT_AVAILABLE;
}

// This function uses the charset converter manager to get a pointer on the 
// converter for the font whose name is given. The caller holds a reference
// to the converter, and should take care of the release...
static nsresult
GetConverterCommon(nsString& aEncoding, nsIUnicodeEncoder** aConverter)
{
  *aConverter = nsnull;
  nsCOMPtr<nsIAtom> charset;
  nsresult rv = gCharsetManager->GetCharsetAtom(aEncoding.get(), getter_AddRefs(charset));
  if (NS_FAILED(rv)) return rv;
  rv = gCharsetManager->GetUnicodeEncoder(charset, aConverter);
  if (NS_FAILED(rv)) return rv;
  return (*aConverter)->SetOutputErrorBehavior((*aConverter)->kOnError_Replace, nsnull, '?');
}

static nsresult
GetDefaultConverterForTTFSymbolEncoding(nsIUnicodeEncoder** aConverter)
{
  nsAutoString value;
  value.AssignWithConversion(DEFAULT_TTF_SYMBOL_ENCODING);
  return GetConverterCommon(value, aConverter);
}

static nsresult
GetConverter(const char* aFontName, nsIUnicodeEncoder** aConverter, PRBool* aIsWide = nsnull)
{
  *aConverter = nsnull;

  nsAutoString value;
  nsresult rv = GetEncoding(aFontName, value);
  if (NS_FAILED(rv)) return rv;
  // The encoding name of a wide NonUnicode font in fontEncoding.properties
  // has '.wide' suffix which has to be removed to get the converter
  // for the encoding.
  if (Substring(value, value.Length() - 5, 5) == (NS_LITERAL_STRING(".wide"))) {
    value.Truncate(value.Length()-5);
    if (aIsWide)
      *aIsWide = PR_TRUE;
  }
  else 
    if (aIsWide)
      *aIsWide = PR_FALSE;

  return GetConverterCommon(value, aConverter);
}

// This function uses the charset converter manager to fill the map for the
// font whose name is given
static PRUint16*
GetCCMapThroughConverter(const char* aFontName)
{
  // see if we know something about the converter of this font 
  nsCOMPtr<nsIUnicodeEncoder> converter;
  // we don't check "familyNameQuirks" here because we are only generating CCMAP.
  // the flag will be checked later when the font is about to be loaded.
  if (NS_SUCCEEDED(GetConverter(aFontName, getter_AddRefs(converter))) ||
      NS_SUCCEEDED(GetDefaultConverterForTTFSymbolEncoding(getter_AddRefs(converter)))) {
    nsCOMPtr<nsICharRepresentable> mapper(do_QueryInterface(converter));
    if (mapper)
      return MapperToCCMap(mapper);
  } 
  return nsnull;
}

static nsresult
ConvertUnicodeToGlyph(const PRUnichar* aSrc,  PRInt32 aSrcLength, 
  PRInt32& aDestLength, nsIUnicodeEncoder* aConverter,
  PRBool aIsWide, nsAutoCharBuffer& aResult)
{
  if (aIsWide && 
      NS_FAILED(aConverter->GetMaxLength(aSrc, aSrcLength, &aDestLength))) {
    return NS_ERROR_UNEXPECTED;
  }

  char* str = aResult.GetArray(aDestLength); 
  if (!str) return NS_ERROR_OUT_OF_MEMORY;

  aConverter->Convert(aSrc, &aSrcLength, str, &aDestLength);

#ifdef IS_LITTLE_ENDIAN
  // Convert BE UCS2 to LE UCS2 for 'wide' fonts
  if (aIsWide) {
    char* pstr = str;
    while (pstr < str + aDestLength) {
      PRUint8 tmp = pstr[0];
      pstr[0] = pstr[1];
      pstr[1] = tmp;
      pstr += 2;  // swap every two bytes
    }
  }
#endif
  return NS_OK;
}

class nsFontInfo : public PLHashEntry
{
public:
  nsFontInfo(eFontType aFontType, PRUint8 aCharset, PRUint16* aCCMap)
  {
    mType = aFontType;
    mCharset = aCharset;
    mCCMap = aCCMap;
#ifdef MOZ_MATHML
    mCMAP.mData = nsnull;  // These are initializations to characterize
    mCMAP.mLength = -1;    // the first call to GetGlyphIndices().
#endif
  };

  eFontType mType;
  PRUint8   mCharset;
  PRUint16* mCCMap;
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
  return nsMemory::Alloc(size);
}

PR_STATIC_CALLBACK(void) fontmap_FreeTable(void *pool, void *item)
{
  nsMemory::Free(item);
}

PR_STATIC_CALLBACK(PLHashEntry*) fontmap_AllocEntry(void *pool, const void *key)
{
 return new nsFontInfo(eFontType_Unicode, DEFAULT_CHARSET, nsnull);
}

PR_STATIC_CALLBACK(void) fontmap_FreeEntry(void *pool, PLHashEntry *he, PRUint32 flag)
{
  if (flag == HT_FREE_ENTRY)  {
    nsFontInfo *fontInfo = NS_STATIC_CAST(nsFontInfo *, he);
    if (fontInfo->mCCMap && (fontInfo->mCCMap != nsFontMetricsWin::gEmptyCCMap))
      FreeCCMap(fontInfo->mCCMap); 
#ifdef MOZ_MATHML
    if (fontInfo->mCMAP.mData)
      nsMemory::Free(fontInfo->mCMAP.mData);
#endif
    delete (nsString *) (he->key);
    delete fontInfo;
  }
}

PLHashAllocOps fontmap_HashAllocOps = {
  fontmap_AllocTable, fontmap_FreeTable,
  fontmap_AllocEntry, fontmap_FreeEntry
};

#define SHOULD_BE_SPACE_CHAR(ch)  ((ch)==0x0020 || (ch)==0x00A0 || ((ch)>=0x2000 && ((ch)<=0x200B || (ch)==0x3000)))

enum {
  eTTPlatformIDUnicode = 0,
  eTTPlatformIDMacintosh = 1,
  eTTPlatformIDMicrosoft = 3
};
enum {
  eTTMicrosoftEncodingSymbol = 0,
  eTTMicrosoftEncodingUnicode = 1,
  eTTMicrosoftEncodingUCS4 = 10
};
// the name of the following enum is from 
// http://www.microsoft.com/typography/otspec/cmap.htm
enum {
  eTTFormatUninitialize = -1,
  eTTFormat0ByteEncodingTable = 0,
  eTTFormat2HighbyteMappingThroughTable = 2,
  eTTFormat4SegmentMappingToDeltaValues = 4,
  eTTFormat6TrimmedTableMapping = 6,
  eTTFormat8Mixed16bitAnd32bitCoverage = 8,
  eTTFormat10TrimmedArray = 10,
  eTTFormat12SegmentedCoverage = 12,
};

static void 
ReadCMAPTableFormat12(PRUint8* aBuf, PRInt32 len, PRUint32 **aExtMap) 
{
  PRUint8* p = aBuf;
  PRUint8* end = aBuf + len;
  PRUint32 i;

  p += sizeof(PRUint16); // skip format
  p += sizeof(PRUint16); // skip reserve field
  PRUint32 tabLen = GET_LONG(p);
  p += sizeof(PRUint32); // skip tableLen
  p += sizeof(PRUint32); // skip language
  PRUint32 nGroup = GET_LONG(p);
  p += sizeof(PRUint32); 

  PRUint32 plane;
  PRUint32 startCode;
  PRUint32 endCode;
  PRUint32 c;
  for (i = 0; i < nGroup; i++) {
    startCode = GET_LONG(p);
    p += sizeof(PRUint32); 
    endCode = GET_LONG(p);
    p += sizeof(PRUint32); 
    for ( c = startCode; c <= endCode; ++c) {
      plane = c >> 16;
      if (!aExtMap[plane]) {
        aExtMap[plane] = new PRUint32[UCS2_MAP_LEN];
        if (!aExtMap[plane])
          return; // i.e., we will only retain the BMP and what we were able to allocate so far
      }
      ADD_GLYPH(aExtMap[plane], c & 0xffff);
    }
    p += sizeof(PRUint32); // skip startGlyphID  field
  }
}


static void 
ReadCMAPTableFormat4(PRUint8* aBuf, PRInt32 aLength, PRUint32* aMap, PRUint8* aIsSpace, PRUint32 aMaxGlyph) 
{
  PRUint8* p = aBuf;
  PRUint8* end = aBuf + aLength;
  PRUint32 i;

  // XXX byte swapping only required for little endian (ifdef?)
  while (p < end) {
    PRUint8 tmp = p[0];
    p[0] = p[1];
    p[1] = tmp;
    p += 2; // every two bytes
  }

  PRUint16* s = (PRUint16*) aBuf;
  PRUint16 segCount = s[3] / 2;
  PRUint16* endCode = &s[7];
  PRUint16* startCode = endCode + segCount + 1;
  PRUint16* idDelta = startCode + segCount;
  PRUint16* idRangeOffset = idDelta + segCount;
  PRUint16* glyphIdArray = idRangeOffset + segCount;

  for (i = 0; i < segCount; ++i) {
    if (idRangeOffset[i]) {
      PRUint16 startC = startCode[i];
      PRUint16 endC = endCode[i];
      for (PRUint32 c = startC; c <= endC; ++c) {
        PRUint16* g =
          (idRangeOffset[i]/2 + (c - startC) + &idRangeOffset[i]);
        if ((PRUint8*) g < end) {
          if (*g) {
            PRUint16 glyph = idDelta[i] + *g;
            if (glyph < aMaxGlyph) {
              if (aIsSpace[glyph]) {
                if (SHOULD_BE_SPACE_CHAR(c)) {
                  ADD_GLYPH(aMap, c);
                } 
              }
              else {
                ADD_GLYPH(aMap, c);
              }
            }
          }
          else {
            // index 0 also applies to space character
            if (SHOULD_BE_SPACE_CHAR(c))
              ADD_GLYPH(aMap, c);
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
      for (PRUint32 c = startCode[i]; c <= endC; ++c) {
        PRUint16 glyph = idDelta[i] + c;
        if (glyph < aMaxGlyph) {
          if (aIsSpace[glyph]) {
            if (SHOULD_BE_SPACE_CHAR(c)) {
              ADD_GLYPH(aMap, c);
            }
          }
          else {
            ADD_GLYPH(aMap, c);
          }
        }
      }
      //printf("0x%04X-0x%04X ", startCode[i], endC);
    }
  }
  //printf("\n");
}

PRUint16*
nsFontMetricsWin::GetFontCCMAP(HDC aDC, const char* aShortName, eFontType& aFontType, PRUint8& aCharset)
{
  PRUint16 *ccmap = nsnull;
  DWORD len = GetFontData(aDC, CMAP, 0, nsnull, 0);
  if ((len == GDI_ERROR) || (!len)) {
    return nsnull;
  }
  nsAutoFontDataBuffer buffer;
  PRUint8* buf = buffer.GetArray(len);
  if (!buf) {
    return nsnull;
  }
  DWORD newLen = GetFontData(aDC, CMAP, 0, buf, len);
  if (newLen != len) {
    return nsnull;
  }

  PRUint32 map[UCS2_MAP_LEN];
  memset(map, 0, sizeof(map));
  PRUint8* p = buf + sizeof(PRUint16); // skip version, move to numberSubtables
  PRUint16 n = GET_SHORT(p); // get numberSubtables
  p += sizeof(PRUint16); // skip numberSubtables, move to the encoding subtables
  PRUint16 i;
  PRUint32 keepOffset;
  PRUint32 offset;
  PRUint32 keepFormat = eTTFormatUninitialize;

  for (i = 0; i < n; ++i) {
    PRUint16 platformID = GET_SHORT(p); // get platformID
    p += sizeof(PRUint16); // move to platformSpecificID
    PRUint16 encodingID = GET_SHORT(p); // get platformSpecificID
    p += sizeof(PRUint16); // move to offset
    offset = GET_LONG(p);  // get offset
    p += sizeof(PRUint32); // move to next entry
    if (platformID == eTTPlatformIDMicrosoft) { 
      if (encodingID == eTTMicrosoftEncodingUnicode) { // Unicode
        // Some fonts claim to be unicode when they are actually
        // 'pseudo-unicode' fonts that require a converter...
        // Here, we check if this font is a pseudo-unicode font that 
        // we know something about, and we force it to be treated as
        // a non-unicode font.
        nsAutoString encoding;
        if (NS_SUCCEEDED(GetEncoding(aShortName, encoding))) {
          aCharset = DEFAULT_CHARSET;
          aFontType = eFontType_NonUnicode;
          return GetCCMapThroughConverter(aShortName);
        } // if GetEncoding();
        PRUint16 format = GET_SHORT(buf+offset);
        if (format == eTTFormat4SegmentMappingToDeltaValues) {
          keepFormat = eTTFormat4SegmentMappingToDeltaValues;
          keepOffset = offset;
        }
      } // if (encodingID == eTTMicrosoftEncodingUnicode) 
      else if (encodingID == eTTMicrosoftEncodingSymbol) { // symbol
        aCharset = SYMBOL_CHARSET;
        aFontType = eFontType_NonUnicode;
        return GetCCMapThroughConverter(aShortName);
      } // if (encodingID == eTTMicrosoftEncodingSymbol)
      else if (encodingID == eTTMicrosoftEncodingUCS4) {
        PRUint16 format = GET_SHORT(buf+offset);
        if (format == eTTFormat12SegmentedCoverage) {
          keepFormat = eTTFormat12SegmentedCoverage;
          keepOffset = offset;
          // we don't want to try anything else when this format is available.
          break;
        }
      }
    } // if (platformID == eTTPlatformIDMicrosoft) 
  } // for loop


  if (eTTFormat12SegmentedCoverage == keepFormat) {
    PRUint32* extMap[EXTENDED_UNICODE_PLANES+1];
    extMap[0] = map;
    memset(extMap+1, 0, sizeof(PRUint32*)*EXTENDED_UNICODE_PLANES);
    ReadCMAPTableFormat12(buf+keepOffset, len-keepOffset, extMap);
    ccmap = MapToCCMapExt(map, extMap+1, EXTENDED_UNICODE_PLANES);
    for (i = 1; i <= EXTENDED_UNICODE_PLANES; ++i) {
      if (extMap[i])
        delete [] extMap[i];
    }
    aCharset = DEFAULT_CHARSET;
    aFontType = eFontType_Unicode;
  }
  else if (eTTFormat4SegmentMappingToDeltaValues == keepFormat) {
    PRUint32 maxGlyph;
    nsAutoFontDataBuffer isSpace;
    if (NS_SUCCEEDED(GetSpaces(aDC, &maxGlyph, isSpace))) {
      ReadCMAPTableFormat4(buf+keepOffset, len-keepOffset, map, isSpace.GetArray(), maxGlyph);
      ccmap = MapToCCMap(map);
      aCharset = DEFAULT_CHARSET;
      aFontType = eFontType_Unicode;
    }
  }

  return ccmap;
}

// Maps that are successfully returned by GetCCMAP() are also cached in the
// gFontMaps hashtable and these are possibly shared. They are going to be
// deleted by the cleanup observer. You don't need to delete a map upon
// calling GetCCMAP (even if you subsequently fail to create a current font
// with it). Otherwise, you will leave a dangling pointer in the gFontMaps
// hashtable.
PRUint16*
nsFontMetricsWin::GetCCMAP(HDC aDC, const char* aShortName, eFontType* aFontType, PRUint8* aCharset)
{
  if (!gFontMaps) {
    gFontMaps = PL_NewHashTable(0, HashKey, CompareKeys, nsnull, &fontmap_HashAllocOps,
      nsnull);
    if (!gFontMaps) { // error checking
      return nsnull;
    }
    gEmptyCCMap = CreateEmptyCCMap();
    if (!gEmptyCCMap) {
      PL_HashTableDestroy(gFontMaps);
      gFontMaps = nsnull;
      return nsnull;
    }
  }
  eFontType fontType = eFontType_Unicode;
  PRUint8 charset = DEFAULT_CHARSET;
  nsString* name = new nsString(); // deleted by fontmap_FreeEntry
  if (!name) {
    return nsnull;
  }
  nsFontInfo* info;
  PLHashEntry *he, **hep = NULL; // shouldn't be NULL, using it as a flag to catch bad changes
  PLHashNumber hash;
  eGetNameError ret = GetNAME(aDC, name);
  if (ret == eGetName_OK) {
    // lookup the hashtable (if we miss, the computed hash and hep are fed back in HT-RawAdd)
    hash = HashKey(name);
    hep = PL_HashTableRawLookup(gFontMaps, hash, name);
    he = *hep;
    if (he) {
      // an identical map has already been added
      delete name;
      info = NS_STATIC_CAST(nsFontInfo *, he);
      if (aCharset) {
        *aCharset = info->mCharset;
      }
      if (aFontType) {
        *aFontType = info->mType;
      }
      return info->mCCMap;
    }
  }
  // GDIError occurs when we have raster font (not TrueType)
  else if (ret == eGetName_GDIError) {
    delete name;
    charset = GetTextCharset(aDC);
    if (charset & (~0xFF)) {
      return gEmptyCCMap;
    }
    int j = gCharsetToIndex[charset];
    
    //default charset is not dependable, skip it at this time
    if (j == eCharset_DEFAULT) {
      return gEmptyCCMap;
    }
    PRUint16* charsetCCMap = gCharsetInfo[j].mCCMap;
    if (!charsetCCMap) {
      charsetCCMap = gCharsetInfo[j].GenerateMap(&gCharsetInfo[j]);
      if (charsetCCMap)
        gCharsetInfo[j].mCCMap = charsetCCMap;
      else
        return gEmptyCCMap;
    }
    if (aCharset) {
      *aCharset = charset;
    }
    if (aFontType) {
      *aFontType = eFontType_Unicode;
    }
    return charsetCCMap;   
  }
  else {
    // return an empty map, so that we never try this font again
    delete name;
    return gEmptyCCMap;
  }

  if (aFontType)
    fontType = *aFontType;
  if (aCharset)
    charset = *aCharset;
  PRUint16* ccmap = GetFontCCMAP(aDC, aShortName, fontType, charset);
  if (aFontType)
    *aFontType = fontType; 
  if (aCharset)
    *aCharset = charset;

  if (!ccmap) {
    delete name;
    return gEmptyCCMap;
  }

  // XXX Need to check if an identical map has already been added - Bug 75260
  NS_ASSERTION(hep, "bad code");
  he = PL_HashTableRawAdd(gFontMaps, hep, hash, name, nsnull);
  if (he) {
    info = NS_STATIC_CAST(nsFontInfo*, he);
    he->value = info;    // so PL_HashTableLookup returns an nsFontInfo*
    info->mType = fontType;
    info->mCharset = charset;
    info->mCCMap = ccmap;
    return ccmap;
  }
  delete name;
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
static nsresult
GetGlyphIndices(HDC                 aDC,
                nsCharacterMap**    aCMAP,
                const PRUnichar*    aString, 
                PRUint32            aLength,
                nsAutoChar16Buffer& aResult)
{  
  NS_ASSERTION(aString, "null arg");
  if (!aString)
    return NS_ERROR_NULL_POINTER;

  nsAutoFontDataBuffer buffer;
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
      return NS_ERROR_UNEXPECTED;
    }
    buf = buffer.GetArray(len);
    if (!buf) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    DWORD newLen = GetFontData(aDC, CMAP, 0, buf, len);
    if (newLen != len) {
      return NS_ERROR_UNEXPECTED;
    }
    PRUint8* p = buf + sizeof(PRUint16); // skip version, move to numberOfSubtables
    PRUint16 n = GET_SHORT(p);
    p += sizeof(PRUint16); // skip numberSubtables, move to the encoding subtables
    PRUint16 i;
    PRUint32 offset;
    for (i = 0; i < n; ++i) {
      PRUint16 platformID = GET_SHORT(p); // get platformID
      p += sizeof(PRUint16); // move to platformSpecificID
      PRUint16 encodingID = GET_SHORT(p); // get platformSpecificID
      p += sizeof(PRUint16); // move to offset
      offset = GET_LONG(p);  // get offset
      p += sizeof(PRUint32); // move to next entry
      if (platformID == eTTPlatformIDMicrosoft && 
          encodingID == eTTMicrosoftEncodingUnicode) // Unicode
        break;
    }
    if (i == n) {
      NS_WARNING("nsFontMetricsWin::GetGlyphIndices() called for a non-unicode font!");
      return NS_ERROR_UNEXPECTED;
    }
    p = buf + offset;
    PRUint16 format = GET_SHORT(p);
    if (format != eTTFormat4SegmentMappingToDeltaValues) {
      return NS_ERROR_UNEXPECTED;
    }
    PRUint8* end = buf + len;

    // XXX byte swapping only required for little endian (ifdef?)
    while (p < end) {
      PRUint8 tmp = p[0];
      p[0] = p[1];
      p[1] = tmp;
      p += 2; // swap every two bytes
    }
#ifdef MOZ_MATHML
    // cache these for later re-use
    if (aCMAP) {
      nsAutoString name;
      nsFontInfo* info;
      if (GetNAME(aDC, &name) == eGetName_OK) {
        info = (nsFontInfo*)PL_HashTableLookup(nsFontMetricsWin::gFontMaps, &name);
        if (info) {
          info->mCMAP.mData = (PRUint8*)nsMemory::Alloc(len);
          if (info->mCMAP.mData) {
            memcpy(info->mCMAP.mData, buf, len);
            info->mCMAP.mLength = len;
            *aCMAP = &(info->mCMAP);
          }          
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
    PRUint8* p = buf + sizeof(PRUint16); // skip version, move to numberOfSubtables
    PRUint16 n = GET_SHORT(p);
    p += sizeof(PRUint16); // skip numberSubtables, move to the encoding subtables
    PRUint16 i;
    PRUint32 offset;
    for (i = 0; i < n; ++i) {
      PRUint16 platformID = GET_SHORT(p); // get platformID
      p += sizeof(PRUint16); // move to platformSpecificID
      PRUint16 encodingID = GET_SHORT(p); // get platformSpecificID
      p += sizeof(PRUint16); // move to offset
      offset = GET_LONG(p);  // get offset
      p += sizeof(PRUint32); // move to next entry
      if (platformID == eTTPlatformIDMicrosoft && 
          encodingID == eTTMicrosoftEncodingUnicode) // Unicode
        break;
    }
    PRUint8* end = buf + len;

    PRUint16* s = (PRUint16*) (buf + offset);
    PRUint16 segCount = s[3] / 2;
    PRUint16* endCode = &s[7];
    PRUint16* startCode = endCode + segCount + 1;
    PRUint16* idDelta = startCode + segCount;
    PRUint16* idRangeOffset = idDelta + segCount;

    PRUint16* result = aResult.GetArray(aLength);
    if (!result) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    for (i = 0; i < aLength; ++i) {
      result[i] = GetGlyphIndex(segCount, endCode, startCode,
                                idRangeOffset, idDelta, end, 
                                aString[i]);
    }

    return NS_OK;
  }
  return NS_ERROR_UNEXPECTED;
}

// ----------------------------------------------------------------------
// We use GetGlyphOutlineW() or GetGlyphOutlineA() to get the metrics. 
// GetGlyphOutlineW() is faster, but not implemented on all platforms.
// nsGlyphAgent provides an interface to hide these details, and it
// also hides the parameters/setup needed to call GetGlyphOutline.

enum eGlyphAgent {
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

  eGlyphAgent GetState()
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

  eGlyphAgent mState; 
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

  // Otherwise, we are on a platform that doesn't implement GetGlyphOutlineW()
  // (see Q241358: The GetGlyphOutlineW Function Fails on Windows 95 & 98
  // http://support.microsoft.com/support/kb/articles/Q241/3/58.ASP)
  // we will use glyph indices as a work around.
  if (0 == aGlyphIndex) { // caller doesn't know the glyph index, so find it
    nsAutoChar16Buffer buf;
    if (NS_SUCCEEDED(GetGlyphIndices(aDC, nsnull, &aChar, 1, buf)))
      aGlyphIndex = *(buf.GetArray());
  }
  if (0 < aGlyphIndex) {
    return GetGlyphOutlineA(aDC, aGlyphIndex, GGO_METRICS | GGO_GLYPH_INDEX, aGlyphMetrics, 0, nsnull, &mMat);
  }

  // if we ever reach here, something went wrong in GetGlyphIndices() above
  // because the current font in aDC wasn't a Unicode font
  return GDI_ERROR;
}

// the global glyph agent that we will be using
nsGlyphAgent gGlyphAgent;

#ifdef MOZ_MATHML

// the common part of GetBoundingMetrics used by nsFontWinUnicode
// and 'wide' nsFontWinNonUnicode.
inline static nsresult 
GetBoundingMetricsCommon(HDC aDC, const PRUnichar* aString, PRUint32 aLength, 
  nsBoundingMetrics& aBoundingMetrics, PRUnichar* aGlyphStr)
{
  // measure the string
  nscoord descent;
  GLYPHMETRICS gm;                                                
  DWORD len = gGlyphAgent.GetGlyphMetrics(aDC, aString[0], aGlyphStr[0], &gm);
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
    for (PRUint32 i = 1; i < aLength; ++i) {
      len = gGlyphAgent.GetGlyphMetrics(aDC, aString[i], aGlyphStr[i], &gm);
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

  return NS_OK;
}
#endif

// Subclass for common unicode fonts (e.g, Times New Roman, Arial, etc.).
// Offers the fastest rendering because no mapping table is needed (here, 
// unicode code points are identical to font's encoding indices). 
// Uses 'W'ide functions (ExtTextOutW, GetTextExtentPoint32W).
class nsFontWinUnicode : public nsFontWin
{
public:
  nsFontWinUnicode(LOGFONT* aLogFont, HFONT aFont, PRUint16* aCCMap);
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

// Subclass for non-unicode fonts that need a mapping table. 'Narrow'
// ones use 'A'nsi functions (ExtTextOutA, GetTextExtentPoint32A) after
// converting unicode code points to font's encoding indices while
// 'wide' ones use 'W' functions (ExtTextOutW, GetTextExtentPoint32W). 
// (A slight  overhead arises from this conversion.)
// NOTE: This subclass also handles some fonts that claim to be 
// unicode, but need a converter.  Converter used for 'wide' fonts
// among them is assumed to return the result in UCS2(BE) corresponding 
// to  pseudo-Unicode code points used as 'glyph indices'(different from
// genuine internal glyph indices used by a font)  of non-Unicode
// fonts claiming to be Unicode fonts.
class nsFontWinNonUnicode : public nsFontWin
{
public:
  nsFontWinNonUnicode(LOGFONT* aLogFont, HFONT aFont, PRUint16* aCCMap, nsIUnicodeEncoder* aConverter, PRBool aIsWide = PR_FALSE);
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
  PRBool mIsWide; 
};

void
nsFontMetricsWin::InitMetricsFor(HDC aDC, nsFontWin* aFont)
{
  float dev2app;
  mDeviceContext->GetDevUnitsToAppUnits(dev2app);

  TEXTMETRIC metrics;
  ::GetTextMetrics(aDC, &metrics);
  aFont->mMaxAscent = NSToCoordRound(metrics.tmAscent * dev2app);
  aFont->mMaxDescent = NSToCoordRound(metrics.tmDescent * dev2app);
}

HFONT
nsFontMetricsWin::CreateFontAdjustHandle(HDC aDC, LOGFONT* aLogFont)
{
  // Adjust the aspect-value so that the x-height of the final font
  // is mFont.size * mFont.sizeAdjust

  PRInt32 dummy = 0;
  nscoord baseSize = mFont.size; 
  nscoord size72 = NSIntPointsToTwips(72); // large value improves accuracy
  mFont.size = size72;
  nscoord baselfHeight = aLogFont->lfHeight;
  FillLogFont(aLogFont, dummy, PR_TRUE);

  HFONT hfont = ::CreateFontIndirect(aLogFont);
  mFont.size = baseSize;
  aLogFont->lfHeight = baselfHeight;

  if (hfont) {
    HFONT oldFont = (HFONT)::SelectObject(aDC, (HGDIOBJ)hfont);
    char name[sizeof(aLogFont->lfFaceName)];
    if (::GetTextFace(aDC, sizeof(name), name) &&
        !strcmpi(name, aLogFont->lfFaceName)) {
      float dev2app;
      mDeviceContext->GetDevUnitsToAppUnits(dev2app);

      // Get the x-height
      nscoord xheight72;
      OUTLINETEXTMETRIC oMetrics;
      TEXTMETRIC& metrics = oMetrics.otmTextMetrics;
      if (0 < ::GetOutlineTextMetrics(aDC, sizeof(oMetrics), &oMetrics)) {
        xheight72 = NSToCoordRound((float)metrics.tmAscent * dev2app * 0.56f); // 50% of ascent, best guess for true type
        GLYPHMETRICS gm;
        DWORD len = gGlyphAgent.GetGlyphMetrics(aDC, PRUnichar('x'), 0, &gm);
        if (GDI_ERROR != len && gm.gmptGlyphOrigin.y > 0) {
          xheight72 = NSToCoordRound(gm.gmptGlyphOrigin.y * dev2app);
        }
      }
      else {
        ::GetTextMetrics(aDC, &metrics);
        xheight72 = NSToCoordRound((float)metrics.tmAscent * dev2app * 0.56f); // 56% of ascent, best guess for non-true type
      }
      ::SelectObject(aDC, (HGDIOBJ)oldFont);  

      // Apply the adjustment
      float adjust = mFont.sizeAdjust / (float(xheight72) / float(size72));
      mFont.size = NSToCoordRound(float(baseSize) * adjust);
      FillLogFont(aLogFont, dummy, PR_TRUE);

      hfont = ::CreateFontIndirect(aLogFont);

      // restore the original values before leaving
      mFont.size = baseSize;
      aLogFont->lfHeight = baselfHeight;
      return hfont;
    }
    ::SelectObject(aDC, (HGDIOBJ)oldFont);  
    ::DeleteObject((HFONT)hfont);  
  }
  return nsnull;
}

HFONT
nsFontMetricsWin::CreateFontHandle(HDC aDC, nsString* aName, LOGFONT* aLogFont)
{
  PRUint16 weightTable = LookForFontWeightTable(aDC, aName);
  PRInt32 weight = GetFontWeight(mFont.weight, weightTable);

  FillLogFont(aLogFont, weight);
 
  /*
   * XXX we are losing info by converting from Unicode to system code page
   * but we don't really have a choice since CreateFontIndirectW is
   * not supported on Windows 9X (see below) -- erik
   */
  WideCharToMultiByte(CP_ACP, 0, aName->get(), aName->Length() + 1,
    aLogFont->lfFaceName, sizeof(aLogFont->lfFaceName), nsnull, nsnull);

  if (mFont.sizeAdjust <= 0) {
    // Quick return for the common case where no adjustement is needed
    return ::CreateFontIndirect(aLogFont);
  }
  return CreateFontAdjustHandle(aDC, aLogFont);
}

HFONT
nsFontMetricsWin::CreateFontHandle(HDC aDC, nsGlobalFont* aGlobalFont, LOGFONT* aLogFont)
{
  PRUint16 weightTable = LookForFontWeightTable(aDC, &aGlobalFont->name);
  PRInt32 weight = GetFontWeight(mFont.weight, weightTable);

  FillLogFont(aLogFont, weight);
  aLogFont->lfCharSet = aGlobalFont->logFont.lfCharSet;
  aLogFont->lfPitchAndFamily = aGlobalFont->logFont.lfPitchAndFamily;
  strcpy(aLogFont->lfFaceName, aGlobalFont->logFont.lfFaceName);

  if (mFont.sizeAdjust <= 0) {
    // Quick return for the common case where no adjustement is needed
    return ::CreateFontIndirect(aLogFont);
  }
  return CreateFontAdjustHandle(aDC, aLogFont);
}

nsFontWin*
nsFontMetricsWin::LoadFont(HDC aDC, nsString* aName)
{
  LOGFONT logFont;
  HFONT hfont = CreateFontHandle(aDC, aName, &logFont);
  if (hfont) {
    HFONT oldFont = (HFONT)::SelectObject(aDC, (HGDIOBJ)hfont);
    char name[sizeof(logFont.lfFaceName)];
    if (::GetTextFace(aDC, sizeof(name), name) &&
        !strcmpi(name, logFont.lfFaceName)) {
      eFontType fontType = eFontType_UNKNOWN;
      nsFontWin* font = nsnull;
      if (mIsUserDefined) {
        font = new nsFontWinNonUnicode(&logFont, hfont, gUserDefinedCCMap,
                                       gUserDefinedConverter);
      } else {
        PRUint16* ccmap = GetCCMAP(aDC, logFont.lfFaceName, &fontType, nsnull);
        if (ccmap) {
          if (eFontType_Unicode == fontType) {
            font = new nsFontWinUnicode(&logFont, hfont, ccmap);
          }
          else if (eFontType_NonUnicode == fontType) {
            nsCOMPtr<nsIUnicodeEncoder> converter;
            PRBool isWide;
            if (NS_SUCCEEDED(GetConverter(logFont.lfFaceName, getter_AddRefs(converter), &isWide)))
              font = new nsFontWinNonUnicode(&logFont, hfont, ccmap, converter, isWide);
            else if (mFont.familyNameQuirks)
              if (NS_SUCCEEDED(GetDefaultConverterForTTFSymbolEncoding(getter_AddRefs(converter))))
                font = new nsFontWinNonUnicode(&logFont, hfont, ccmap, converter);
          }
        }
      }

      if (font) {
        InitMetricsFor(aDC, font);
        mLoadedFonts.AppendElement(font);
        ::SelectObject(aDC, (HGDIOBJ)oldFont);  
        return font;
      }
      // do not free 'ccmap', it is cached in the gFontMaps hashtable and
      // it is going to be deleted by the cleanup observer
    }
    ::SelectObject(aDC, (HGDIOBJ)oldFont);
    ::DeleteObject(hfont);
  }
  return nsnull;
}

nsFontWin*
nsFontMetricsWin::LoadGlobalFont(HDC aDC, nsGlobalFont* aGlobalFont)
{
  LOGFONT logFont;
  HFONT hfont = CreateFontHandle(aDC, aGlobalFont, &logFont);
  if (hfont) {
    nsFontWin* font = nsnull;
    if (eFontType_Unicode == aGlobalFont->fonttype) {
      font = new nsFontWinUnicode(&logFont, hfont, aGlobalFont->ccmap);
    }
    else if (eFontType_NonUnicode == aGlobalFont->fonttype) {
      nsCOMPtr<nsIUnicodeEncoder> converter;
      PRBool isWide;
      if (NS_SUCCEEDED(GetConverter(logFont.lfFaceName, getter_AddRefs(converter), &isWide))) {
        font = new nsFontWinNonUnicode(&logFont, hfont, aGlobalFont->ccmap, converter, isWide);
      }
    }
    if (font) {
      HFONT oldFont = (HFONT)::SelectObject(aDC, (HGDIOBJ)hfont);
      InitMetricsFor(aDC, font);
      mLoadedFonts.AppendElement(font);
      ::SelectObject(aDC, (HGDIOBJ)oldFont);
      return font;
    }
    ::DeleteObject((HGDIOBJ)hfont);
  }
  return nsnull;
}

static int CALLBACK 
enumProc(const LOGFONT* logFont, const TEXTMETRIC* metrics,
         DWORD fontType, LPARAM closure)
{
  // XXX ignore vertical fonts
  if (logFont->lfFaceName[0] == '@') {
    return 1;
  }

  for (int i = nsFontMetricsWin::gGlobalFonts->Count()-1; i >= 0; --i) {
    nsGlobalFont* font = (nsGlobalFont*)nsFontMetricsWin::gGlobalFonts->ElementAt(i);
    if (!strcmp(font->logFont.lfFaceName, logFont->lfFaceName)) {
      // work-around for Win95/98 problem 
      int charsetSigBit = gCharsetToBit[gCharsetToIndex[logFont->lfCharSet]];
      if (charsetSigBit >= 0) {
        DWORD charsetSigAdd = 1 << charsetSigBit;
        font->signature.fsCsb[0] |= charsetSigAdd;
      }
      return 1;
    }
  }

  // XXX make this smarter: don't add font to list if we already have a font
  // with the same font signature -- erik

  PRUnichar name[LF_FACESIZE];
  name[0] = 0;
  MultiByteToWideChar(CP_ACP, 0, logFont->lfFaceName,
    strlen(logFont->lfFaceName) + 1, name, sizeof(name)/sizeof(name[0]));

  nsGlobalFont* font = new nsGlobalFont;
  if (!font) {
    return 0;
  }
  font->name.Assign(name);
  font->ccmap = nsnull;
  font->logFont = *logFont;
  font->signature.fsCsb[0] = 0;
  font->signature.fsCsb[1] = 0;
  font->fonttype = eFontType_UNKNOWN;
  font->flags = 0;

  if (fontType & TRUETYPE_FONTTYPE) {
    font->flags |= NS_GLOBALFONT_TRUETYPE;
  }
  if (logFont->lfCharSet == SYMBOL_CHARSET) {
    font->flags |= NS_GLOBALFONT_SYMBOL;
  }

  int charsetSigBit = gCharsetToBit[gCharsetToIndex[logFont->lfCharSet]];
  if (charsetSigBit >= 0) {
    DWORD charsetSigAdd = 1 << charsetSigBit;
    font->signature.fsCsb[0] |= charsetSigAdd;
  }

  nsFontMetricsWin::gGlobalFonts->AppendElement(font);
  return 1;
}

static int PR_CALLBACK
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

nsVoidArray*
nsFontMetricsWin::InitializeGlobalFonts(HDC aDC)
{
  if (!gGlobalFonts) {
    gGlobalFonts = new nsVoidArray();
    if (!gGlobalFonts) return nsnull;

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
    gGlobalFonts->Sort(CompareGlobalFonts, nsnull);
  }

  return gGlobalFonts;
}

int
nsFontMetricsWin::SameAsPreviousMap(int aIndex)
{
  // aIndex is 0...gGlobalFonts.Count()-1 in caller
  nsGlobalFont* font = (nsGlobalFont*)gGlobalFonts->ElementAt(aIndex);
  for (int i = 0; i < aIndex; ++i) {
    nsGlobalFont* tmp = (nsGlobalFont*)gGlobalFonts->ElementAt(i);
    if (tmp->flags & NS_GLOBALFONT_SKIP) {
      continue;
    }
    if (tmp->ccmap == font->ccmap) {
      font->flags |= NS_GLOBALFONT_SKIP;
      return 1;
    }

    if (IsSameCCMap(tmp->ccmap, font->ccmap)) {
      font->flags |= NS_GLOBALFONT_SKIP;
      return 1;
    }
  }

  return 0;
}

nsFontWin*
nsFontMetricsWin::FindGlobalFont(HDC aDC, PRUint32 c)
{
  //now try global font
  if (!gGlobalFonts) {
    if (!InitializeGlobalFonts(aDC)) {
      return nsnull;
    }
  }
  int count = gGlobalFonts->Count();
  for (int i = 0; i < count; ++i) {
    nsGlobalFont* font = (nsGlobalFont*)gGlobalFonts->ElementAt(i);
    if (font->flags & NS_GLOBALFONT_SKIP) {
      continue;
    }
    if (!font->ccmap) {
      // don't adjust here, we just want to quickly get the CMAP. Adjusting
      // is meant to only happen when loading the final font in LoadFont()
      HFONT hfont = ::CreateFontIndirect(&font->logFont);
      if (!hfont) {
        continue;
      }
      HFONT oldFont = (HFONT)::SelectObject(aDC, hfont);
      font->ccmap = GetCCMAP(aDC, font->logFont.lfFaceName, &font->fonttype, nsnull);
      ::SelectObject(aDC, oldFont);
      ::DeleteObject(hfont);
      if (!font->ccmap || font->ccmap == gEmptyCCMap) {
        font->flags |= NS_GLOBALFONT_SKIP;
        continue;
      }
      if (SameAsPreviousMap(i)) {
        continue;
      }
    }
    if (CCMAP_HAS_CHAR_EXT(font->ccmap, c)) {
      return LoadGlobalFont(aDC, font);
    }
  }
  return nsnull;
}

// XXX perf: also sync a static global map of missing chars, and check the
// map to see if we can skip the lookup in FindGlobalFont(), and remember
// to clear the map in UpdateFontList() when new fonts are installed
nsFontWin*
nsFontMetricsWin::FindSubstituteFont(HDC aDC, PRUint32 c)
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
  // a lookup in the hashtable. We only know its entry in mLoadedFonts[]
  // (Notice that the nsFontInfo of the font that is picked as the
  // "substitute font" *can* be in the hashtable, and as expected, its 
  // own map can be retrieved with the normal lookup).

  // The "substitute font" is a *unicode font*. No conversion here.
  // XXX Is it worth having another subclass which has a converter? 
  // Such a variant may allow to display a boxed question mark, etc.

  if (mSubstituteFont) {
    //make the char representable so that we don't have to go over all font before fallback to 
    //subsituteFont.
    ((nsFontWinSubstitute*)mSubstituteFont)->SetRepresentable(c);
    return mSubstituteFont;
  }

  // The "substitute font" has not yet been created... 
  // The first unicode font (no converter) that has the
  // replacement char is taken and placed as the substitute font.

  // Try local/loaded fonts first
  int i, count = mLoadedFonts.Count();
  for (i = 0; i < count; ++i) {
    nsAutoString name;
    nsFontWin* font = (nsFontWin*)mLoadedFonts[i];
    HFONT oldFont = (HFONT)::SelectObject(aDC, font->mFont);
    eGetNameError res = GetNAME(aDC, &name);
    ::SelectObject(aDC, oldFont);
    if (res == eGetName_OK) { // TrueType font
      nsFontInfo* info = (nsFontInfo*)PL_HashTableLookup(gFontMaps, &name);
      if (!info || info->mType != eFontType_Unicode) {
        continue;
      }
    }
    else if (res == eGetName_GDIError) { // Bitmap font
      // alright, was treated as unicode font in GetCCMAP()
    }
    else {
      continue;
    }
    if (font->HasGlyph(NS_REPLACEMENT_CHAR)) {
      // XXX if the mode is to display unicode points "&#xNNNN;", should we check
      // that the substitute font also has glyphs for '&', '#', 'x', ';' and digits?
      // (Because this is a unicode font, those glyphs should in principle be there.)
      name.AssignWithConversion(font->mName);
      font = LoadSubstituteFont(aDC, &name);
      if (font) {
        ((nsFontWinSubstitute*)font)->SetRepresentable(c);
        mSubstituteFont = font;
        return font;
      }
    }
  }

  // Try global fonts
  // Since we reach here after FindGlobalFont() is called, we have already
  // scanned the global list of fonts and have set the attributes of interest
  count = gGlobalFonts->Count();
  for (i = 0; i < count; ++i) {
    nsGlobalFont* globalFont = (nsGlobalFont*)gGlobalFonts->ElementAt(i);
    if (!globalFont->ccmap || 
        globalFont->flags & NS_GLOBALFONT_SKIP ||
        globalFont->fonttype != eFontType_Unicode) {
      continue;
    }
    if (CCMAP_HAS_CHAR(globalFont->ccmap, NS_REPLACEMENT_CHAR)) {
      nsFontWin* font = LoadSubstituteFont(aDC, &globalFont->name);
      if (font) {
        ((nsFontWinSubstitute*)font)->SetRepresentable(c);
        mSubstituteFont = font;
        return font;
      }
    }
  }

  // We are running out of resources if we reach here... Try a stock font
  NS_ASSERTION(::GetMapMode(aDC) == MM_TEXT, "mapping mode needs to be MM_TEXT");
  nsFontWin* font = LoadSubstituteFont(aDC, nsnull);
  if (font) {
    ((nsFontWinSubstitute*)font)->SetRepresentable(c);
    mSubstituteFont = font;
    return font;
  }

  NS_ERROR("Could not provide a substititute font");
  return nsnull;
}

nsFontWin*
nsFontMetricsWin::LoadSubstituteFont(HDC aDC, nsString* aName)
{
  LOGFONT logFont;
  HFONT hfont = aName
    ? CreateFontHandle(aDC, aName, &logFont)
    : (HFONT)::GetStockObject(SYSTEM_FONT);
  if (hfont) {
    // XXX 'displayUnicode' has to be initialized based on the desired rendering mode
    PRBool displayUnicode = PR_FALSE;
    /* substitute font does not need ccmap */
    LOGFONT* lfont = aName ? &logFont : nsnull;
    nsFontWinSubstitute* font = new nsFontWinSubstitute(lfont, hfont, nsnull, displayUnicode);
    if (font) {
      HFONT oldFont = (HFONT)::SelectObject(aDC, (HGDIOBJ)hfont);
      InitMetricsFor(aDC, font);
      mLoadedFonts.AppendElement((nsFontWin*)font);
      ::SelectObject(aDC, (HGDIOBJ)oldFont);
      return font;
    }
    ::DeleteObject((HGDIOBJ)hfont);
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
  return (PLHashNumber)nsCRT::HashCode(string->get());
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
  return nsMemory::Alloc(size);
}

PR_STATIC_CALLBACK(void) fontweight_FreeTable(void *pool, void *item)
{
  nsMemory::Free(item);
}

PR_STATIC_CALLBACK(PLHashEntry*) fontweight_AllocEntry(void *pool, const void *key)
{
  return new nsFontWeightEntry;
}

PR_STATIC_CALLBACK(void) fontweight_FreeEntry(void *pool, PLHashEntry *he, PRUint32 flag)
{
  if (flag == HT_FREE_ENTRY)  {
    nsFontWeightEntry *fontWeightEntry = NS_STATIC_CAST(nsFontWeightEntry *, he);
    delete fontWeightEntry;
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
  return (bitwiseWeight & aWeightTable) != 0;
}

typedef struct {
  LOGFONT  mLogFont;
  PRUint16 mWeights;
  int      mFontCount;
} nsFontWeightInfo;

static int CALLBACK
nsFontWeightCallback(const LOGFONT* logFont, const TEXTMETRIC * metrics,
                     DWORD fontType, LPARAM closure)
{
// printf("Name %s Log font sizes %d\n",logFont->lfFaceName,logFont->lfWeight);
  nsFontWeightInfo* weightInfo = (nsFontWeightInfo*)closure;
  if (metrics) {
    int pos = metrics->tmWeight / 100;
      // Set a bit to indicate the font weight is available
    if (weightInfo->mFontCount == 0)
      weightInfo->mLogFont = *logFont;
    nsFontMetricsWin::SetFontWeight(metrics->tmWeight / 100,
      &weightInfo->mWeights);
    ++weightInfo->mFontCount;
  }
  return TRUE; // Keep looking for more weights.
}

static void
SearchSimulatedFontWeight(HDC aDC, nsFontWeightInfo* aWeightInfo)
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
      ++weightCount;
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
nsFontMetricsWin::GetFontWeightTable(HDC aDC, nsString* aFontName)
{
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

  // Check for exact match
  if ((aWeight > 0) && IsFontWeightAvailable(aWeight, aWeightTable)) {
    return aWeight;
  }

  // Find lighter and darker weights to be used later.

  // First look for lighter
  PRBool done = PR_FALSE;
  PRInt32 lighterWeight = 0;
  PRInt32 proposedLighterWeight = PR_MAX(0, aWeight - 100);
  while (!done && (proposedLighterWeight >= 100)) {
    if (IsFontWeightAvailable(proposedLighterWeight, aWeightTable)) {
      lighterWeight = proposedLighterWeight;
      done = PR_TRUE;
    } else {
      proposedLighterWeight -= 100;
    }
  }

  // Now look for darker
  done = PR_FALSE;
  PRInt32 darkerWeight = 0;
  PRInt32 proposedDarkerWeight = PR_MIN(aWeight + 100, 900);
  while (!done && (proposedDarkerWeight <= 900)) {
    if (IsFontWeightAvailable(proposedDarkerWeight, aWeightTable)) {
      darkerWeight = proposedDarkerWeight;
      done = PR_TRUE;   
    } else {
      proposedDarkerWeight += 100;
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
    return lighterWeight ? lighterWeight : darkerWeight;
  } 

  // Automatically chose the bolder weight if the next lighter weight
  // makes it normal. (i.e goes over the normal to bold threshold.)

  // From CSS2 section 15.5.1 
  // if any of the values '600', '700', '800', or '900' remains unassigned, 
  // they are assigned to the same face as the next darker assigned keyword, 
  // if any, or the next lighter one otherwise.
  return darkerWeight ? darkerWeight : lighterWeight;
}

PRInt32
nsFontMetricsWin::GetBolderWeight(PRInt32 aWeight, PRInt32 aDistance, PRUint16 aWeightTable)
{
  PRInt32 newWeight = aWeight;
  PRInt32 proposedWeight = aWeight + 100; // Start 1 bolder than the current
  for (PRInt32 j = 0; j < aDistance; ++j) {
    PRBool foundWeight = PR_FALSE;
    while (!foundWeight && (proposedWeight <= NS_MAX_FONT_WEIGHT)) {
      if (IsFontWeightAvailable(proposedWeight, aWeightTable)) {
        newWeight = proposedWeight; 
        foundWeight = PR_TRUE;
      }
      proposedWeight += 100; 
    }
  }
  return newWeight;
}

PRInt32
nsFontMetricsWin::GetLighterWeight(PRInt32 aWeight, PRInt32 aDistance, PRUint16 aWeightTable)
{
  PRInt32 newWeight = aWeight;
  PRInt32 proposedWeight = aWeight - 100; // Start 1 lighter than the current
  for (PRInt32 j = 0; j < aDistance; ++j) {
    PRBool foundWeight = PR_FALSE;
    while (!foundWeight && (proposedWeight >= NS_MIN_FONT_WEIGHT)) {
      if (IsFontWeightAvailable(proposedWeight, aWeightTable)) {
        newWeight = proposedWeight; 
        foundWeight = PR_TRUE;
      }
      proposedWeight -= 100; 
    }
  }
  return newWeight;
}

PRInt32
nsFontMetricsWin::GetFontWeight(PRInt32 aWeight, PRUint16 aWeightTable)
{
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
  if (!gFontWeights) {
    gFontWeights = PL_NewHashTable(0, HashKeyFontWeight, CompareKeysFontWeight, nsnull, &fontweight_HashAllocOps,
      nsnull);
    if (!gFontWeights) {
      return 0;
    }
  }

  // Use lower case name for hash table searches. This eliminates
  // keeping multiple font weights entries when the font name varies 
  // only by case.
  nsAutoString low(*aName);
  ToLowerCase(low);

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
nsFontMetricsWin::FindUserDefinedFont(HDC aDC, PRUint32 aChar)
{
  if (mIsUserDefined) {
    // the user-defined font is always loaded as the first font
    nsFontWin* font = LoadFont(aDC, &mUserDefined);
    mIsUserDefined = PR_FALSE;
    if (font && font->HasGlyph(aChar))
      return font;    
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
  return nsMemory::Alloc(size);
}

PR_STATIC_CALLBACK(void) familyname_FreeTable(void *pool, void *item)
{
  nsMemory::Free(item);
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
    nsMemory::Free(he);
  }
}

PLHashAllocOps familyname_HashAllocOps = {
  familyname_AllocTable, familyname_FreeTable,
  familyname_AllocEntry, familyname_FreeEntry
};

PLHashTable*
nsFontMetricsWin::InitializeFamilyNames(void)
{
  if (!gFamilyNames) {
    gFamilyNames = PL_NewHashTable(0, HashKey, CompareKeys, nsnull, &familyname_HashAllocOps,
      nsnull);
    if (!gFamilyNames) {
      return nsnull;
    }
    nsFontFamilyName* f = gFamilyNameTable;
    while (f->mName) {
      nsString* name = new nsString;
      nsString* winName = new nsString;
      if (name && winName) {
        name->AssignWithConversion(f->mName);
        winName->AssignWithConversion(f->mWinName);
        if (PL_HashTableAdd(gFamilyNames, name, (void*) winName)) { 
          ++f;
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
nsFontMetricsWin::FindLocalFont(HDC aDC, PRUint32 aChar)
{
  if (!gFamilyNames) {
    if (!InitializeFamilyNames()) {
      return nsnull;
    }
  }
  while (mFontsIndex < mFonts.Count()) {
    if (mFontsIndex == mGenericIndex) {
      return nsnull;
    }

    nsString* name = mFonts.StringAt(mFontsIndex++);
    nsAutoString low(*name);
    ToLowerCase(low);
    nsString* winName = (nsString*) PL_HashTableLookup(gFamilyNames, &low);
    if (!winName) {
      winName = name;
    }
    nsFontWin* font = LoadFont(aDC, winName);
    if (font && font->HasGlyph(aChar)) {
      return font;
    }
  }
  return nsnull;
}

nsFontWin*
nsFontMetricsWin::LoadGenericFont(HDC aDC, PRUint32 aChar, nsString* aName)
{
  for (int i = mLoadedFonts.Count()-1; i >= 0; --i) {
    // woah, this seems bad
    const nsACString& fontName =
      nsDependentCString(((nsFontWin*)mLoadedFonts[i])->mName);
    if (aName->Equals(NS_ConvertASCIItoUCS2(((nsFontWin*)mLoadedFonts[i])->mName),
                                            nsCaseInsensitiveStringComparator()))
      return nsnull;

  }
  nsFontWin* font = LoadFont(aDC, aName);
  if (font && font->HasGlyph(aChar)) {
    return font;
  }
  return nsnull;
}

struct GenericFontEnumContext
{
  HDC               mDC;
  PRUint32          mChar;
  nsFontWin*        mFont;
  nsFontMetricsWin* mMetrics;
};

static PRBool PR_CALLBACK
GenericFontEnumCallback(const nsString& aFamily, PRBool aGeneric, void* aData)
{
  GenericFontEnumContext* context = (GenericFontEnumContext*)aData;
  HDC dc = context->mDC;
  PRUint32 ch = context->mChar;
  nsFontMetricsWin* metrics = context->mMetrics;
  context->mFont = metrics->LoadGenericFont(dc, ch, (nsString*)&aFamily);
  if (context->mFont) {
    return PR_FALSE; // stop enumerating the list
  }
  return PR_TRUE; // don't stop
}

#define MAKE_FONT_PREF_KEY(_pref, _s0, _s1) \
 _pref.Assign(_s0); \
 _pref.Append(_s1);

static void 
AppendGenericFontFromPref(nsString& aFontname,
                          const char* aLangGroup,
                          const char* aGeneric)
{
  nsresult res;
  nsCAutoString pref;
  nsXPIDLString value;
  nsCAutoString generic_dot_langGroup;

  generic_dot_langGroup.Assign(aGeneric);
  generic_dot_langGroup.Append('.');
  generic_dot_langGroup.Append(aLangGroup);

  // font.name.[generic].[langGroup]
  // the current user' selected font, it gives the first preferred font
  MAKE_FONT_PREF_KEY(pref, "font.name.", generic_dot_langGroup);
  res = gPref->CopyUnicharPref(pref.get(), getter_Copies(value));      
  if (NS_SUCCEEDED(res)) {
    if(aFontname.Length() > 0)
      aFontname.Append((PRUnichar)',');
    aFontname.Append(value);
  }

  // font.name-list.[generic].[langGroup]
  // the pre-built list of default fonts, it gives alternative fonts
  MAKE_FONT_PREF_KEY(pref, "font.name-list.", generic_dot_langGroup);
  res = gPref->CopyUnicharPref(pref.get(), getter_Copies(value));      
  if (NS_SUCCEEDED(res)) {
    if(aFontname.Length() > 0)
      aFontname.Append((PRUnichar)',');
    aFontname.Append(value);
  }
}

nsFontWin*
nsFontMetricsWin::FindGenericFont(HDC aDC, PRUint32 aChar)
{
  if (mTriedAllGenerics) {
    // don't bother anymore because mLoadedFonts[] already has all our generic fonts
    return nsnull;
  }

  // This is a nifty hook that we will use to just iterate over
  // the list of names using the callback mechanism of nsFont...
  nsFont font("", 0, 0, 0, 0, 0);

  if (mLangGroup) {
    nsAutoString langGroup;
    mLangGroup->ToString(langGroup);
    AppendGenericFontFromPref(font.name, 
                              NS_ConvertUCS2toUTF8(langGroup).get(), 
                              NS_ConvertUCS2toUTF8(mGeneric).get());
  }

  // Iterate over the list of names using the callback mechanism of nsFont...
  GenericFontEnumContext context = {aDC, aChar, nsnull, this};
  font.EnumerateFamilies(GenericFontEnumCallback, &context);
  if (context.mFont) { // a suitable font was found
    return context.mFont;
  }

#if defined(DEBUG_rbs) || defined(DEBUG_shanjian)
  nsAutoString langGroupName;
  mLangGroup->ToString(langGroupName);
  nsCAutoString lang; lang.Assign(NS_ConvertUCS2toUTF8(langGroupName));
  nsCAutoString generic; generic.Assign(NS_ConvertUCS2toUTF8(mGeneric));
  nsCAutoString family; family.Assign(NS_ConvertUCS2toUTF8(mFont.name));
  printf("FindGenericFont missed:U+%04X langGroup:%s generic:%s mFont.name:%s\n", 
         aChar, lang.get(), generic.get(), family.get());
#endif

  mTriedAllGenerics = 1;
  return nsnull;
}

#define IsCJKLangGroupAtom(a)  ((a)==gJA || (a)==gKO || (a)==gZHCN || (a)==gZHTW)

nsFontWin*
nsFontMetricsWin::FindPrefFont(HDC aDC, PRUint32 aChar)
{
  if (mTriedAllPref) {
    // don't bother anymore because mLoadedFonts[] already has all our pref fonts
    return nsnull;
  }
  nsFont font("", 0, 0, 0, 0, 0);

  // Sometimes we could not find the font in doc's suggested langGroup,(this usually means  
  // the language specified by doc is incorrect). The characters can, to a certain degree, 
  // tell us what language it is. This allows us to quickly locate and use a more appropriate 
  // font as indicated by user's preference. In some situations a set of possible languages may
  // be identified instead of a single language (eg. CJK and latin). In this case we have to 
  // try every language in the set. gUserLocale and gSystemLocale provide some hints about 
  // which one should be tried first. This is important for CJK font, since the glyph for single 
  // char varies dramatically in different langauges. For latin languages, their glyphs are 
  // similar. In fact, they almost always share identical fonts. It will be a waste of time to 
  // figure out which one comes first. As a final fallback, unicode preference is always tried. 

  PRUint32 unicodeRange = FindCharUnicodeRange(aChar);
  if (unicodeRange > kRangeSpecificItemNum) { 
    // a single language is identified
    AppendGenericFontFromPref(font.name, LangGroupFromUnicodeRange(unicodeRange), 
                              NS_ConvertUCS2toUTF8(mGeneric).get());
  } else if (kRangeSetLatin == unicodeRange) { 
    // Character is from a latin language set, so try western and central european
    // If mLangGroup is western or central european, this most probably will not be
    // used, but is here as a fallback scenario.    
    AppendGenericFontFromPref(font.name, "x-western",
                              NS_ConvertUCS2toUTF8(mGeneric).get());
    AppendGenericFontFromPref(font.name, "x-central-euro",
                              NS_ConvertUCS2toUTF8(mGeneric).get());
  } else if (kRangeSetCJK == unicodeRange) { 
    // CJK, we have to be careful about the order, use locale info as hint
    
    // then try user locale first, if it is CJK
    if ((gUsersLocale != mLangGroup) && IsCJKLangGroupAtom(gUsersLocale)) {
      const PRUnichar *usersLocaleLangGroup;
      gUsersLocale->GetUnicode(&usersLocaleLangGroup);
      AppendGenericFontFromPref(font.name, NS_ConvertUCS2toUTF8(usersLocaleLangGroup).get(), 
                                NS_ConvertUCS2toUTF8(mGeneric).get());
    }
    
    // then system locale (os language)
    if ((gSystemLocale != mLangGroup) && (gSystemLocale != gUsersLocale) && IsCJKLangGroupAtom(gSystemLocale)) {
      const PRUnichar *systemLocaleLangGroup;
      gSystemLocale->GetUnicode(&systemLocaleLangGroup);
      AppendGenericFontFromPref(font.name, NS_ConvertUCS2toUTF8(systemLocaleLangGroup).get(), 
                                NS_ConvertUCS2toUTF8(mGeneric).get());
    }

    // try all other languages in this set.
    if (mLangGroup != gJA && gUsersLocale != gJA && gSystemLocale != gJA)
      AppendGenericFontFromPref(font.name, "ja",
                                NS_ConvertUCS2toUTF8(mGeneric).get());
    if (mLangGroup != gZHCN && gUsersLocale != gZHCN && gSystemLocale != gZHCN)
      AppendGenericFontFromPref(font.name, "zh-CN",
                                NS_ConvertUCS2toUTF8(mGeneric).get());
    if (mLangGroup != gZHTW && gUsersLocale != gZHTW && gSystemLocale != gZHTW)
      AppendGenericFontFromPref(font.name, "zh-TW",
                                NS_ConvertUCS2toUTF8(mGeneric).get());
    if (mLangGroup != gKO && gUsersLocale != gKO && gSystemLocale != gKO)
      AppendGenericFontFromPref(font.name, "ko",
                                NS_ConvertUCS2toUTF8(mGeneric).get());
  } 

  // always try unicode as fallback
  AppendGenericFontFromPref(font.name, "x-unicode",
                            NS_ConvertUCS2toUTF8(mGeneric).get());
  
  // use the font list to find font
  GenericFontEnumContext context = {aDC, aChar, nsnull, this};
  font.EnumerateFamilies(GenericFontEnumCallback, &context);
  if (context.mFont) { // a suitable font was found
    return context.mFont;
  }
  mTriedAllPref = 1;
  return nsnull;
}

nsFontWin*
nsFontMetricsWin::FindFont(HDC aDC, PRUint32 aChar)
{
  nsFontWin* font = FindUserDefinedFont(aDC, aChar);
  if (!font) {
    font = FindLocalFont(aDC, aChar);
    if (!font) {
      font = FindGenericFont(aDC, aChar);
      if (!font) {
        font = FindPrefFont(aDC, aChar);
        if (!font) {
          font = FindGlobalFont(aDC, aChar);
          if (!font) {
            font = FindSubstituteFont(aDC, aChar);
          }
        }
      }
    }
  }
  return font;
}

nsFontWin*
nsFontMetricsWin::GetFontFor(HFONT aHFONT)
{
  int count = mLoadedFonts.Count();
  for (int i = 0; i < count; ++i) {
    nsFontWin* font = (nsFontWin*)mLoadedFonts[i];
    if (font->mFont == aHFONT)
      return font;
  }
  NS_ERROR("Cannot find the font that owns the handle");
  return nsnull;
}

static PRBool
FontEnumCallback(const nsString& aFamily, PRBool aGeneric, void *aData)
{
  nsFontMetricsWin* metrics = (nsFontMetricsWin*) aData;
  metrics->mFonts.AppendString(aFamily);
  if (aGeneric) {
    metrics->mGeneric.Assign(aFamily);
    ToLowerCase(metrics->mGeneric);
    return PR_FALSE; // stop
  }
  ++metrics->mGenericIndex;

  return PR_TRUE; // don't stop
}

/** ---------------------------------------------------
 *  See documentation in nsFontMetricsWin.h
 *	@update 05/28/99 dwc
 */

nsresult
nsFontMetricsWin::RealizeFont()
{
  nsresult rv;
  HWND win = NULL;
  HDC  dc = NULL;
  HDC  dc1 = NULL;

  if (mDeviceContext->mDC){
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

  mFont.EnumerateFamilies(FontEnumCallback, this); 

  nsCAutoString pref;
  nsXPIDLString value;

  // set a fallback generic font if the font-family list didn't have one
  if (mGeneric.IsEmpty()) {
    rv = gPref->CopyUnicharPref("font.default", getter_Copies(value));
    if (NS_SUCCEEDED(rv)) {
      mGeneric.Assign(value);
    }
    else {
      mGeneric.Assign(NS_LITERAL_STRING("serif"));
    }
  }

  if (mLangGroup.get() == gUserDefined) {
    if (!gUserDefinedConverter) {
      nsCOMPtr<nsIAtom> charset;
      rv = gCharsetManager->GetCharsetAtom2("x-user-defined",
        getter_AddRefs(charset));
      if (NS_FAILED(rv)) return rv;
      rv = gCharsetManager->GetUnicodeEncoder(charset, &gUserDefinedConverter);
      if (NS_FAILED(rv)) return rv;
      gUserDefinedConverter->SetOutputErrorBehavior(
        gUserDefinedConverter->kOnError_Replace, nsnull, '?');
      nsCOMPtr<nsICharRepresentable> mapper =
        do_QueryInterface(gUserDefinedConverter);
      if (mapper) {
        gUserDefinedCCMap = MapperToCCMap(mapper);
      }
    }

    // See if this is a special user-defined font encoding by checking:
    // font.name.[generic].x-user-def
    pref.Assign("font.name.");
    pref.AppendWithConversion(mGeneric);
    pref.Append(".x-user-def");
    rv = gPref->CopyUnicharPref(pref.get(), getter_Copies(value));
    if (NS_SUCCEEDED(rv)) {
      mUserDefined.Assign(value);
      mIsUserDefined = 1;
    }
  }

  nsFontWin* font = FindFont(dc1, 'a');
  NS_ASSERTION(font, "missing font");
  if (!font) {
    ::ReleaseDC(win, mDeviceContext->mDC ? dc1 : dc);
    return NS_ERROR_FAILURE;
  }
  mFontHandle = font->mFont;

  HFONT oldfont = (HFONT)::SelectObject(dc, (HGDIOBJ) mFontHandle);

  // Get font metrics
  float dev2app;
  mDeviceContext->GetDevUnitsToAppUnits(dev2app);
  OUTLINETEXTMETRIC oMetrics;
  TEXTMETRIC& metrics = oMetrics.otmTextMetrics;
  nscoord onePixel = NSToCoordRound(1 * dev2app);
  nscoord descentPos = 0;

  if (0 < ::GetOutlineTextMetrics(dc, sizeof(oMetrics), &oMetrics)) {
//    mXHeight = NSToCoordRound(oMetrics.otmsXHeight * dev2app);  XXX not really supported on windows
    mXHeight = NSToCoordRound((float)metrics.tmAscent * dev2app * 0.56f); // 50% of ascent, best guess for true type
    mSuperscriptOffset = NSToCoordRound(oMetrics.otmptSuperscriptOffset.y * dev2app);
    mSubscriptOffset = NSToCoordRound(oMetrics.otmptSubscriptOffset.y * dev2app);

    mStrikeoutSize = PR_MAX(onePixel, NSToCoordRound(oMetrics.otmsStrikeoutSize * dev2app));
    mStrikeoutOffset = NSToCoordRound(oMetrics.otmsStrikeoutPosition * dev2app);
    mUnderlineSize = PR_MAX(onePixel, NSToCoordRound(oMetrics.otmsUnderscoreSize * dev2app));
    if (gDoingLineheightFixup) {
      if(IsCJKLangGroupAtom(mLangGroup.get())) {
        mUnderlineOffset = NSToCoordRound(PR_MIN(oMetrics.otmsUnderscorePosition, 
                                                 oMetrics.otmDescent + oMetrics.otmsUnderscoreSize) 
                                                 * dev2app);
        // keep descent position, use it for mUnderlineOffset if leading allows
        descentPos = NSToCoordRound(oMetrics.otmDescent * dev2app);
      } else {
        mUnderlineOffset = NSToCoordRound(PR_MIN(oMetrics.otmsUnderscorePosition*dev2app, 
                                                 oMetrics.otmDescent*dev2app + mUnderlineSize));
      }
    }
    else
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

  mInternalLeading = NSToCoordRound(metrics.tmInternalLeading * dev2app);
  mExternalLeading = NSToCoordRound(metrics.tmExternalLeading * dev2app);
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

  // descent position is preferred, but we need to make sure there is enough 
  // space available. Baseline need to be raised so that underline will stay 
  // within boundary.
  // only do this for CJK to minimize possible risk
  if (mLangGroup.get() == gJA || 
      mLangGroup.get() == gKO || 
      mLangGroup.get() == gZHTW || 
      mLangGroup.get() == gZHCN ) {
    if (gDoingLineheightFixup && 
        mInternalLeading+mExternalLeading > mUnderlineSize &&
        descentPos < mUnderlineOffset) {
      mEmAscent -= mUnderlineSize;
      mEmDescent += mUnderlineSize;
      mMaxAscent -= mUnderlineSize;
      mMaxDescent += mUnderlineSize;
      mUnderlineOffset = descentPos;
    }
  }
  // Cache the width of a single space.
  SIZE  size;
  ::GetTextExtentPoint32(dc, " ", 1, &size);
  mSpaceWidth = NSToCoordRound(size.cx * dev2app);

  ::SelectObject(dc, oldfont);

  ::ReleaseDC(win, mDeviceContext->mDC ? dc1 : dc);
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin::GetSpaceWidth(nscoord& aSpaceWidth)
{
  aSpaceWidth = mSpaceWidth;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin::GetXHeight(nscoord& aResult)
{
  aResult = mXHeight;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin::GetSuperscriptOffset(nscoord& aResult)
{
  aResult = mSuperscriptOffset;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin::GetSubscriptOffset(nscoord& aResult)
{
  aResult = mSubscriptOffset;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin::GetStrikeout(nscoord& aOffset, nscoord& aSize)
{
  aOffset = mStrikeoutOffset;
  aSize = mStrikeoutSize;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin::GetUnderline(nscoord& aOffset, nscoord& aSize)
{
  aOffset = mUnderlineOffset;
  aSize = mUnderlineSize;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin::GetHeight(nscoord &aHeight)
{
  aHeight = mMaxHeight;
  return NS_OK;
}

#ifdef FONT_LEADING_APIS_V2
NS_IMETHODIMP
nsFontMetricsWin::GetInternalLeading(nscoord &aLeading)
{
  aLeading = mInternalLeading;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin::GetExternalLeading(nscoord &aLeading)
{
  aLeading = mExternalLeading;
  return NS_OK;
}
#else
NS_IMETHODIMP
nsFontMetricsWin::GetLeading(nscoord &aLeading)
{
  aLeading = mInternalLeading;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin::GetNormalLineHeight(nscoord &aHeight)
{
  aHeight = mEmHeight + mInternalLeading;
  return NS_OK;
}
#endif //FONT_LEADING_APIS_V2

NS_IMETHODIMP
nsFontMetricsWin::GetEmHeight(nscoord &aHeight)
{
  aHeight = mEmHeight;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin::GetEmAscent(nscoord &aAscent)
{
  aAscent = mEmAscent;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin::GetEmDescent(nscoord &aDescent)
{
  aDescent = mEmDescent;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin::GetMaxHeight(nscoord &aHeight)
{
  aHeight = mMaxHeight;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin::GetMaxAscent(nscoord &aAscent)
{
  aAscent = mMaxAscent;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin::GetMaxDescent(nscoord &aDescent)
{
  aDescent = mMaxDescent;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin::GetMaxAdvance(nscoord &aAdvance)
{
  aAdvance = mMaxAdvance;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin::GetAveCharWidth(nscoord &aAveCharWidth)
{
  aAveCharWidth = mAveCharWidth;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin::GetFont(const nsFont *&aFont)
{
  aFont = &mFont;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin::GetLangGroup(nsIAtom** aLangGroup)
{
  NS_ENSURE_ARG_POINTER(aLangGroup);
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

nsFontWin*
nsFontMetricsWin::LocateFont(HDC aDC, PRUint32 aChar, PRInt32 & aCount)
{
  nsFontWin *font;
  PRInt32 i;

  // see if one of our loaded fonts can represent the character
  for (i = 0; i < aCount; ++i) {
    font = (nsFontWin*)mLoadedFonts[i];
    if (font->HasGlyph(aChar))
      return font;
  }

  font = FindFont(aDC, aChar);
  aCount = mLoadedFonts.Count(); // update since FindFont() can change it
  NS_ASSERTION(font && mLoadedFonts.IndexOf(font) >= 0,
               "Could not find a font");
  return font;
}

nsresult
nsFontMetricsWin::ResolveForwards(HDC                  aDC,
                                  const PRUnichar*     aString,
                                  PRUint32             aLength,
                                  nsFontSwitchCallback aFunc, 
                                  void*                aData)
{
  NS_ASSERTION(aString || !aLength, "invalid call");
  const PRUnichar* firstChar = aString;
  const PRUnichar* currChar = firstChar;
  const PRUnichar* lastChar  = aString + aLength;
  nsFontWin* currFont;
  nsFontWin* nextFont;
  PRInt32 count;
  nsFontSwitch fontSwitch;

  if (firstChar == lastChar)
    return NS_OK;

  count = mLoadedFonts.Count();

  if (IS_HIGH_SURROGATE(*currChar) && (currChar+1) < lastChar && IS_LOW_SURROGATE(*(currChar+1))) {
    currFont = LocateFont(aDC, SURROGATE_TO_UCS4(*currChar, *(currChar+1)), count);
    currChar += 2;
  }
  else {
    currFont = LocateFont(aDC, *currChar, count);
    ++currChar;
  }

  //This if block is meant to speedup the process in normal situation, when
  //most characters can be found in first font
  if (currFont == mLoadedFonts[0]) {
    while (currChar < lastChar && (currFont->HasGlyph(*currChar)))
      ++currChar;
    fontSwitch.mFontWin = currFont;
    if (!(*aFunc)(&fontSwitch, firstChar, currChar - firstChar, aData))
      return NS_OK;
    if (currChar == lastChar)
      return NS_OK;
    // continue with the next substring, re-using the available loaded fonts
    firstChar = currChar;
    if (IS_HIGH_SURROGATE(*currChar) && (currChar+1) < lastChar && IS_LOW_SURROGATE(*(currChar+1))) {
      currFont = LocateFont(aDC, SURROGATE_TO_UCS4(*currChar, *(currChar+1)), count);
      currChar += 2;
    }
    else {
      currFont = LocateFont(aDC, *currChar, count);
      ++currChar;
    }
  }

  // see if we can keep the same font for adjacent characters
  PRInt32 lastCharLen;
  while (currChar < lastChar) {
    if (IS_HIGH_SURROGATE(*currChar) && (currChar+1) < lastChar && IS_LOW_SURROGATE(*(currChar+1))) {
      nextFont = LocateFont(aDC, SURROGATE_TO_UCS4(*currChar, *(currChar+1)), count);
      lastCharLen = 2;
    }
    else {
      nextFont = LocateFont(aDC, *currChar, count);
      lastCharLen = 1;
    }
    if (nextFont != currFont) {
      // We have a substring that can be represented with the same font, and
      // we are about to switch fonts, it is time to notify our caller.
      fontSwitch.mFontWin = currFont;
      if (!(*aFunc)(&fontSwitch, firstChar, currChar - firstChar, aData))
        return NS_OK;
      // continue with the next substring, re-using the available loaded fonts
      firstChar = currChar;

      currFont = nextFont; // use the font found earlier for the char
    }
    currChar += lastCharLen;
  }

  //do it for last part of the string
  fontSwitch.mFontWin = currFont;
  (*aFunc)(&fontSwitch, firstChar, currChar - firstChar, aData);
  return NS_OK;
}

nsresult
nsFontMetricsWin::ResolveBackwards(HDC                  aDC,
                                   const PRUnichar*     aString,
                                   PRUint32             aLength,
                                   nsFontSwitchCallback aFunc, 
                                   void*                aData)
{
  NS_ASSERTION(aString || !aLength, "invalid call");
  const PRUnichar* firstChar = aString + aLength - 1;
  const PRUnichar* lastChar  = aString - 1;
  const PRUnichar* currChar  = firstChar;
  nsFontWin* currFont;
  nsFontWin* nextFont;
  PRInt32 count;
  nsFontSwitch fontSwitch;

  if (firstChar == lastChar)
    return NS_OK;

  count = mLoadedFonts.Count();

  // see if one of our loaded fonts can represent the current character
  currFont = LocateFont(aDC, *currChar, count);

  // see if we can keep the same font for adjacent characters
  while (--currChar > lastChar) {
    nextFont = LocateFont(aDC, *currChar, count);
    if (nextFont != currFont) {
      // We have a substring that can be represented with the same font, and
      // we are about to switch fonts, it is time to notify our caller.
      fontSwitch.mFontWin = currFont;
      if (!(*aFunc)(&fontSwitch, currChar+1, firstChar - currChar, aData))
        return NS_OK;
      // continue with the next substring, re-using the available loaded fonts
      firstChar = currChar;
      currFont = nextFont; // use the font found earlier for the char
    }
  }

  //do it for last part of the string
  fontSwitch.mFontWin = currFont;
  (*aFunc)(&fontSwitch, currChar+1, firstChar - currChar, aData);

  return NS_OK;
}

//

nsFontWin::nsFontWin(LOGFONT* aLogFont, HFONT aFont, PRUint16* aCCMap)
{
  if (aLogFont) {
    strcpy(mName, aLogFont->lfFaceName);
  }
  mFont = aFont;
  mCCMap = aCCMap;
}

nsFontWin::~nsFontWin()
{
  if (mFont) {
    ::DeleteObject(mFont);
    mFont = nsnull;
  }
}

nsFontWinUnicode::nsFontWinUnicode(LOGFONT* aLogFont, HFONT aFont,
  PRUint16* aCCMap) : nsFontWin(aLogFont, aFont, aCCMap)
{
// This is used for a bug in WIN95 where it does not render
// unicode TrueType fonts with underline or strikeout correctly.
// @see  nsFontWinUnicode::DrawString
  NS_ASSERTION(aLogFont != NULL, "Null logfont passed to nsFontWinUnicode constructor");
  mUnderlinedOrStrikeOut = aLogFont->lfUnderline || aLogFont->lfStrikeOut;
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
  if (mUnderlinedOrStrikeOut) {
    if (IsWin95OrWin98()) {
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
  nsAutoChar16Buffer buffer;

  // at this stage, the glyph agent should have already been initialized
  // given that it was used to compute the x-height in RealizeFont()
  NS_ASSERTION(gGlyphAgent.GetState() != eGlyphAgent_UNKNOWN, "Glyph agent is not yet initialized");
  if (gGlyphAgent.GetState() != eGlyphAgent_UNICODE) {
    // we are on a platform that doesn't implement GetGlyphOutlineW() 
    // we need to use glyph indices
    nsresult rv = GetGlyphIndices(aDC, &mCMAP, aString, aLength, buffer);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  return GetBoundingMetricsCommon(aDC, aString, aLength, aBoundingMetrics, buffer.GetArray());
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
  PRUint16* aCCMap, nsIUnicodeEncoder* aConverter, PRBool aIsWide) : nsFontWin(aLogFont, aFont, aCCMap)
{
  mConverter = aConverter;
  mIsWide = aIsWide;
}

nsFontWinNonUnicode::~nsFontWinNonUnicode()
{
}

PRInt32
nsFontWinNonUnicode::GetWidth(HDC aDC, const PRUnichar* aString,
  PRUint32 aLength)
{
  nsAutoCharBuffer buffer;

  PRInt32 destLength = aLength;
  if (NS_FAILED(ConvertUnicodeToGlyph(aString, aLength, destLength, 
                mConverter, mIsWide, buffer))) {
    return 0;
  }

  SIZE size;
  if (!mIsWide)
    ::GetTextExtentPoint32A(aDC, buffer.GetArray(), destLength, &size);
  else
    ::GetTextExtentPoint32W(aDC, (PRUnichar*) buffer.GetArray(), destLength / 2, &size);

  return size.cx;
}

void
nsFontWinNonUnicode::DrawString(HDC aDC, PRInt32 aX, PRInt32 aY,
  const PRUnichar* aString, PRUint32 aLength)
{
  nsAutoCharBuffer buffer;
  PRInt32 destLength = aLength;

  if (NS_FAILED(ConvertUnicodeToGlyph(aString, aLength, destLength, 
                mConverter, mIsWide, buffer))) {
    return;
  }

  if (!mIsWide)
    ::ExtTextOutA(aDC, aX, aY, 0, NULL, buffer.GetArray(), aLength, NULL);
  else 
    ::ExtTextOutW(aDC, aX, aY, 0, NULL, (PRUnichar*) buffer.GetArray(), destLength / 2, NULL);
}

#ifdef MOZ_MATHML
nsresult
nsFontWinNonUnicode::GetBoundingMetrics(HDC                aDC, 
                                        const PRUnichar*   aString,
                                        PRUint32           aLength,
                                        nsBoundingMetrics& aBoundingMetrics)
{
  aBoundingMetrics.Clear();
  nsAutoCharBuffer buffer;
  PRInt32 destLength = aLength;
  nsresult rv = NS_OK;

  rv = ConvertUnicodeToGlyph(aString, aLength, destLength, mConverter, 
                             mIsWide, buffer);
  if (NS_FAILED(rv))
    return rv;

  if (mIsWide) {
    nsAutoChar16Buffer buf;
    // at this stage, the glyph agent should have already been initialized
    // given that it was used to compute the x-height in RealizeFont()
    NS_ASSERTION(gGlyphAgent.GetState() != eGlyphAgent_UNKNOWN, "Glyph agent is not yet initialized");
    if (gGlyphAgent.GetState() != eGlyphAgent_UNICODE) {
      // we are on a platform that doesn't implement GetGlyphOutlineW() 
      // we need to use glyph indices
      rv = GetGlyphIndices(aDC, &mCMAP, (PRUint16*) buffer.GetArray(), destLength / 2, buf);
      if (NS_FAILED(rv)) {
        return rv;
      }
    }

    // buffer.mBuffer is now a pseudo-Unicode string so that we can use 
    // GetBoundingMetricsCommon() also used by nsFontWinUnicode. 
    return  GetBoundingMetricsCommon(aDC, (PRUint16*) buffer.GetArray(), 
              destLength / 2, aBoundingMetrics, buf.GetArray());

  }

  // measure the string
  nscoord descent;
  GLYPHMETRICS gm;
  char* pstr = buffer.GetArray();
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
    for (PRUint32 i = 1; i < aLength; ++i) {
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
#endif // MOZ_MATHML

nsFontWinSubstitute::nsFontWinSubstitute(LOGFONT* aLogFont, HFONT aFont,
  PRUint16* aCCMap, PRBool aDisplayUnicode) : nsFontWin(aLogFont, aFont, aCCMap)
{
  mDisplayUnicode = aDisplayUnicode;
  memset(mRepresentableCharMap, 0, sizeof(mRepresentableCharMap));
}

nsFontWinSubstitute::~nsFontWinSubstitute()
{
}

static nsresult
SubstituteChars(PRBool              aDisplayUnicode,
                const PRUnichar*    aString, 
                PRUint32            aLength,
                nsAutoChar16Buffer& aResult,
                PRUint32*           aCount)
{
  if (!gFontSubstituteConverter) {
    nsComponentManager::
    CreateInstance(NS_SAVEASCHARSET_CONTRACTID, nsnull,
                   NS_GET_IID(nsISaveAsCharset),
                   (void**)&gFontSubstituteConverter);
    if (gFontSubstituteConverter) {
      // if aDisplayUnicode is set: transliterate, and then fallback
      //   to literal Unicode points &#xNNNN; for unknown characters
      // otherwise: transliterate, and then fallback to entities for
      //   recorded characters and question marks for unknown characters
      nsresult res;
      res = gFontSubstituteConverter->Init("ISO-8859-1",
              aDisplayUnicode
              ? nsISaveAsCharset::attr_FallbackHexNCR
              : nsISaveAsCharset::attr_EntityAfterCharsetConv + nsISaveAsCharset::attr_FallbackQuestionMark,
              nsIEntityConverter::transliterate);
      if (NS_FAILED(res)) {
        NS_RELEASE(gFontSubstituteConverter);
      }
    }
  }

  // do the transliteration if we have a converter
  PRUnichar* result; 
  if (gFontSubstituteConverter) {
    char* conv = nsnull;
    nsAutoString tmp(aString, aLength); // we need to pass a null-terminated string
    gFontSubstituteConverter->Convert(tmp.get(), &conv);
    if (conv) {
      *aCount = strlen(conv);
      result = aResult.GetArray(*aCount);
      if (!result) {
        nsMemory::Free(conv);
        return NS_ERROR_OUT_OF_MEMORY;
      }
      PRUnichar* u = result;
      char* c = conv;
      for (; *c; ++c, ++u) {
        *u = *c;
      }
      nsMemory::Free(conv);
      return NS_OK;
    }
  }

  // we reach here if we couldn't transliterate, so fallback to question marks 
  result = aResult.GetArray(aLength);
  if (!result) return NS_ERROR_OUT_OF_MEMORY;
  for (PRUint32 i = 0; i < aLength; i++) {
    result[i] = NS_REPLACEMENT_CHAR;
  }
  *aCount = aLength;
  return NS_OK;
}

PRInt32
nsFontWinSubstitute::GetWidth(HDC aDC, const PRUnichar* aString,
  PRUint32 aLength)
{
  nsAutoChar16Buffer buffer;
  nsresult rv = SubstituteChars(PR_FALSE, aString, aLength, buffer, &aLength);
  if (NS_FAILED(rv) || !aLength) return 0;

  SIZE size;
  ::GetTextExtentPoint32W(aDC, buffer.GetArray(), aLength, &size);

  return size.cx;
}

void
nsFontWinSubstitute::DrawString(HDC aDC, PRInt32 aX, PRInt32 aY,
  const PRUnichar* aString, PRUint32 aLength)
{
  nsAutoChar16Buffer buffer;
  nsresult rv = SubstituteChars(PR_FALSE, aString, aLength, buffer, &aLength);
  if (NS_FAILED(rv) || !aLength) return;

  ::ExtTextOutW(aDC, aX, aY, 0, NULL, buffer.GetArray(), aLength, NULL);
}

#ifdef MOZ_MATHML
nsresult
nsFontWinSubstitute::GetBoundingMetrics(HDC                aDC, 
                                        const PRUnichar*   aString,
                                        PRUint32           aLength,
                                        nsBoundingMetrics& aBoundingMetrics)
{
  aBoundingMetrics.Clear();
  nsAutoChar16Buffer buffer;
  nsresult rv = SubstituteChars(mDisplayUnicode, aString, aLength, buffer, &aLength);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (!aLength) return NS_OK;
  nsAutoChar16Buffer buf;

  // at this stage, the glyph agent should have already been initialized
  // given that it was used to compute the x-height in RealizeFont()
  NS_ASSERTION(gGlyphAgent.GetState() != eGlyphAgent_UNKNOWN, "Glyph agent is not yet initialized");
  if (gGlyphAgent.GetState() != eGlyphAgent_UNICODE) {
    // we are on a platform that doesn't implement GetGlyphOutlineW() 
    // we better get all glyph indices in one swoop
    rv = GetGlyphIndices(aDC, &mCMAP, buffer.GetArray(), aLength, buf);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  PRUnichar* pstr = buffer.GetArray();
  PRUnichar* ps = buf.GetArray();

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
    for (PRUint32 i = 1; i < aLength; ++i) {
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

static PRUint16*
GenerateDefault(nsCharsetInfo* aSelf)
{ 
  PRUint32 map[UCS2_MAP_LEN];

  memset(map, 0xff, sizeof(map));
  return MapToCCMap(map);
}

static PRUint16*
GenerateSingleByte(nsCharsetInfo* aSelf)
{ 
  PRUint32 map[UCS2_MAP_LEN];
  PRUint8 mb[256];
  PRUint16 wc[256];
  int i;

  memset(map, 0, sizeof(map));
  for (i = 0; i < 127; ++i) {
    mb[i] = i;
  }
  mb[145] = 145;
  mb[146] = 146;

  if (aSelf->mCodePage == 1250) {
    mb[138] = 138;
    mb[140] = 140;
    mb[141] = 141;
    mb[142] = 142;
    mb[143] = 143;
    mb[154] = 154;
    mb[156] = 156;
    mb[158] = 158;
    mb[159] = 159;
  }

  for (i = 160; i < 255; ++i) {
    mb[i] = i;
  }

  int len = MultiByteToWideChar(aSelf->mCodePage, 0, (char*) mb, 256, wc, 256);
#ifdef NS_DEBUG
  if (len != 256) {
    printf("%s: MultiByteToWideChar returned %d\n", aSelf->mName, len);
  }
#endif
  for (i = 0; i < 256; ++i) {
    //win95/98 have problem in some raster fonts (MS Sans Serif and MS Serif) in
    //rendering 0xb7. So let's skip this in charmap, that will let system resort 
    //to other fonts.  
    if ( i == 0x00b7 && IsWin95OrWin98() && !UseAFunctions())
      continue;
    ADD_GLYPH(map, wc[i]);
  }
  return MapToCCMap(map);
}

static PRUint16*
GenerateMultiByte(nsCharsetInfo* aSelf)
{ 
  PRUint32 map[UCS2_MAP_LEN];
  memset(map, 0, sizeof(map));
  for (PRUint16 c = 0; c < 0xFFFF; ++c) {
    BOOL defaulted = FALSE;
    WideCharToMultiByte(aSelf->mCodePage, 0, &c, 1, nsnull, 0, nsnull,
      &defaulted);
    if (!defaulted) {
      ADD_GLYPH(map, c);
    }
  }
  return MapToCCMap(map);
}

static int
HaveConverterFor(PRUint8 aCharset)
{
  PRUint16 wc = 'a';
  char mb[8];
  if (WideCharToMultiByte(gCharsetInfo[gCharsetToIndex[aCharset]].mCodePage, 0,
                          &wc, 1, mb, sizeof(mb), nsnull, nsnull)) {
    return 1;
  }
  // remove from table, since we can't support it anyway
  for (int i = sizeof(gBitToCharset)-1; i >= 0; --i) {
    if (gBitToCharset[i] == aCharset) {
      gBitToCharset[i] = DEFAULT_CHARSET;
    }
  }

  return 0;
}

int
nsFontWinA::GetSubsets(HDC aDC)
{
  mSubsetsCount = 0;

  FONTSIGNATURE signature;
  if (::GetTextCharsetInfo(aDC, &signature, 0) == DEFAULT_CHARSET) {
    return 0;
  }

  int dword;
  DWORD* array = signature.fsCsb;
  int i = 0;
  for (dword = 0; dword < 2; ++dword) {
    for (int bit = 0; bit < sizeof(DWORD) * 8; ++bit, ++i) {
      if ((array[dword] >> bit) & 1) {
        PRUint8 charset = gBitToCharset[i];
#ifdef DEBUG_FONT_SIGNATURE
        printf("  %02d %s\n", i, gCharsetInfo[gCharsetToIndex[charset]].mName);
#endif
        if ((charset != DEFAULT_CHARSET) && HaveConverterFor(charset)) {
          ++mSubsetsCount;
        }
      }
    }
  }

  if (!mSubsetsCount) {
    return 0;
  }
  mSubsets = (nsFontSubset**) nsMemory::Alloc(mSubsetsCount* sizeof(nsFontSubset*));
  if (!mSubsets) {
    mSubsetsCount = 0;
    return 0;
  }
  memset(mSubsets, 0, mSubsetsCount * sizeof(nsFontSubset*)); 

  int j;
  for (j = 0; j < mSubsetsCount; ++j) {
    mSubsets[j] = new nsFontSubset();
    if (!mSubsets[j]) {
      for (j = j - 1; j >= 0; --j) {
        delete mSubsets[j];
      }
      nsMemory::Free(mSubsets);
      mSubsets = nsnull;
      mSubsetsCount = 0;
      return 0;
    }
  }

  i = j = 0;
  for (dword = 0; dword < 2; ++dword) {
    for (int bit = 0; bit < sizeof(DWORD) * 8; ++bit, ++i) {
      if ((array[dword] >> bit) & 1) {
        PRUint8 charset = gBitToCharset[i];
        if ((charset != DEFAULT_CHARSET) && HaveConverterFor(charset)) {
          mSubsets[j]->mCharset = charset;
          ++j;
        }
      }
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
}

void
nsFontSubset::Convert(const PRUnichar* aString, PRUint32 aLength,
  char** aResult /*IN/OUT*/, int* aResultLength /*IN/OUT*/)
{
  int len = *aResultLength; // current available len of aResult
  *aResultLength = 0;

  // Get the number of bytes needed for the conversion
  int nb = WideCharToMultiByte(mCodePage, 0, aString, aLength,
                               *aResult, 0, nsnull, nsnull);
  if (!nb) return;
  if (nb > len) {
    // Allocate a bigger array that the caller should free
    *aResult = new char[nb];
    if (!*aResult) return;
  }
  // Convert the Unicode string to ANSI
  *aResultLength = WideCharToMultiByte(mCodePage, 0, aString, aLength,
                                       *aResult, nb, nsnull, nsnull);
}

PRInt32
nsFontSubset::GetWidth(HDC aDC, const PRUnichar* aString, PRUint32 aLength)
{
  char str[CHAR_BUFFER_SIZE];
  char* pstr = str;
  int len = CHAR_BUFFER_SIZE;
  Convert(aString, aLength, &pstr, &len);
  if (len) {
    SIZE size;
    ::GetTextExtentPoint32A(aDC, pstr, len, &size);
    if (pstr != str) {
      delete[] pstr;
    }
    return size.cx;
  }
  return 0;
}

void
nsFontSubset::DrawString(HDC aDC, PRInt32 aX, PRInt32 aY,
  const PRUnichar* aString, PRUint32 aLength)
{
  char str[CHAR_BUFFER_SIZE];
  char* pstr = str;
  int len = CHAR_BUFFER_SIZE;
  Convert(aString, aLength, &pstr, &len);
  if (len) {
    ::ExtTextOutA(aDC, aX, aY, 0, NULL, pstr, len, NULL);
    if (pstr != str) {
      delete[] pstr;
    }
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
  char str[CHAR_BUFFER_SIZE];
  char* pstr = str;
  int length = CHAR_BUFFER_SIZE;
  Convert(aString, aLength, &pstr, &length);
  if (!pstr) return NS_ERROR_OUT_OF_MEMORY;
  if (!length) return NS_OK;

  // measure the string
  nscoord descent;
  GLYPHMETRICS gm;
  DWORD len = gGlyphAgent.GetGlyphMetrics(aDC, PRUint8(pstr[0]), &gm);
  if (GDI_ERROR == len) {
    if (pstr != str) {
      delete[] pstr;
    }
    return NS_ERROR_UNEXPECTED;
  }

  // flip sign of descent for cross-platform compatibility
  descent = -(nscoord(gm.gmptGlyphOrigin.y) - nscoord(gm.gmBlackBoxY));
  aBoundingMetrics.leftBearing = gm.gmptGlyphOrigin.x;
  aBoundingMetrics.rightBearing = gm.gmptGlyphOrigin.x + gm.gmBlackBoxX;
  aBoundingMetrics.ascent = gm.gmptGlyphOrigin.y;
  aBoundingMetrics.descent = descent;
  aBoundingMetrics.width = gm.gmCellIncX;

  if (1 < length) {
    // loop over each glyph to get the ascent and descent
    for (int i = 1; i < length; ++i) {
      len = gGlyphAgent.GetGlyphMetrics(aDC, PRUint8(pstr[0]), &gm);
      if (GDI_ERROR == len) {
        if (pstr != str) {
          delete[] pstr;
        }
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

class nsFontSubsetSubstitute : public nsFontSubset
{
public:
  nsFontSubsetSubstitute();
  virtual ~nsFontSubsetSubstitute();

  // overloaded method to convert all chars to substitute chars
  virtual void Convert(const PRUnichar* aString, PRUint32 aLength,
                       char** aResult, int* aResultLength);
  //when fontSubstitute declare it support certain char, no need to check subset,
  // since we have one and only one subset.
  virtual PRBool HasGlyph(PRUnichar ch) {return PR_TRUE;};
};

nsFontSubsetSubstitute::nsFontSubsetSubstitute()
  : nsFontSubset()
{
}

nsFontSubsetSubstitute::~nsFontSubsetSubstitute()
{
}

void
nsFontSubsetSubstitute::Convert(const PRUnichar* aString, PRUint32 aLength,
  char** aResult, int* aResultLength)
{
  nsAutoChar16Buffer buffer;
  nsresult rv = SubstituteChars(PR_FALSE, aString, aLength, buffer, &aLength);
  if (NS_FAILED(rv)) {
    // this is the out-of-memory error case
    *aResult = nsnull;
    *aResultLength = 0;
    return;
  }
  if (!aLength) {
    // this is the case where the substitute string collapsed to nothingness
    **aResult = '\0';
    *aResultLength = 0;
    return;
  }
  nsFontSubset::Convert(buffer.GetArray(), aLength, aResult, aResultLength);
}

nsFontWinA::nsFontWinA(LOGFONT* aLogFont, HFONT aFont, PRUint16* aCCMap)
  : nsFontWin(aLogFont, aFont, aCCMap)
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
    nsFontSubset** endSubsets = subset + mSubsetsCount;
    while (subset < endSubsets) {
      delete *subset;
      ++subset;
    }
    nsMemory::Free(mSubsets);
    mSubsets = nsnull;
  }
}

PRInt32
nsFontWinA::GetWidth(HDC aDC, const PRUnichar* aString, PRUint32 aLength)
{
  NS_ERROR("must call nsFontSubset's GetWidth");
  return 0;
}

void
nsFontWinA::DrawString(HDC aDC, PRInt32 aX, PRInt32 aY,
  const PRUnichar* aString, PRUint32 aLength)
{
  NS_ERROR("must call nsFontSubset's DrawString");
}

nsFontSubset* 
nsFontWinA::FindSubset(HDC aDC, PRUnichar aChar, nsFontMetricsWinA* aFontMetrics)
{
  nsFontSubset **subsetPtr = mSubsets;
  nsFontSubset **endSubset = subsetPtr + mSubsetsCount;

  while (subsetPtr < endSubset) {
    if (!(*subsetPtr)->mCCMap)
      if (!(*subsetPtr)->Load(aDC, aFontMetrics, this))
        continue;
    if ((*subsetPtr)->HasGlyph(aChar)) {
      return *subsetPtr;
    }
    ++subsetPtr;
  }
  return nsnull;
}


#ifdef MOZ_MATHML
nsresult
nsFontWinA::GetBoundingMetrics(HDC                aDC, 
                               const PRUnichar*   aString,
                               PRUint32           aLength,
                               nsBoundingMetrics& aBoundingMetrics)
{
  NS_ERROR("must call nsFontSubset's GetBoundingMetrics");
  return NS_ERROR_FAILURE;
}

#ifdef NS_DEBUG
void 
nsFontWinA::DumpFontInfo()
{
  NS_ERROR("must call nsFontSubset's DumpFontInfo");
}
#endif // NS_DEBUG
#endif

nsFontWin*
nsFontMetricsWinA::LoadFont(HDC aDC, nsString* aName)
{
  LOGFONT logFont;
  HFONT hfont = CreateFontHandle(aDC, aName, &logFont);
  if (hfont) {
#ifdef DEBUG_FONT_SIGNATURE
    printf("%s\n", logFont.lfFaceName);
#endif
    HFONT oldFont = (HFONT)::SelectObject(aDC, (HGDIOBJ)hfont);
    char name[sizeof(logFont.lfFaceName)];
    if (::GetTextFace(aDC, sizeof(name), name) &&
        !strcmpi(name, logFont.lfFaceName)) {
      PRUint16* ccmap = GetCCMAP(aDC, logFont.lfFaceName, nsnull, nsnull);
      if (ccmap) {
        nsFontWinA* font = new nsFontWinA(&logFont, hfont, ccmap);
        if (font) {
          if (font->GetSubsets(aDC)) {
            // XXX InitMetricsFor(aDC, font) is not here, except if
            // we can assume that it is the same for all subsets?
            mLoadedFonts.AppendElement(font);
            ::SelectObject(aDC, (HGDIOBJ)oldFont);
            return font;
          }
          ::SelectObject(aDC, (HGDIOBJ)oldFont);
          delete font; // will release hfont as well
          return nsnull;
        }
        // do not free 'map', it is cached in the gFontMaps hashtable and
        // it is going to be deleted by the cleanup observer
      }
    }
    ::SelectObject(aDC, (HGDIOBJ)oldFont);
    ::DeleteObject(hfont);
  }
  return nsnull;
}

nsFontWin*
nsFontMetricsWinA::LoadGlobalFont(HDC aDC, nsGlobalFont* aGlobalFont)
{
  LOGFONT logFont;
  HFONT hfont = CreateFontHandle(aDC, aGlobalFont, &logFont);
  if (hfont) {
    nsFontWinA* font = new nsFontWinA(&logFont, hfont, aGlobalFont->ccmap);
    if (font) {
      HFONT oldFont = (HFONT)::SelectObject(aDC, (HGDIOBJ)hfont);
      if (font->GetSubsets(aDC)) {
        // XXX InitMetricsFor(aDC, font) is not here, except if
        // we can assume that it is the same for all subsets?
        mLoadedFonts.AppendElement(font);
        ::SelectObject(aDC, (HGDIOBJ)oldFont);
        return font;
      }
      ::SelectObject(aDC, (HGDIOBJ)oldFont);
      delete font; // will release hfont as well
      return nsnull;
    }
    ::DeleteObject(hfont);
  }
  return nsnull;
}

int
nsFontSubset::Load(HDC aDC, nsFontMetricsWinA* aFontMetricsWinA, nsFontWinA* aFont)
{
  LOGFONT* logFont = &aFont->mLogFont;
  logFont->lfCharSet = mCharset;
  // create a font handle without filling & overwriting what is in logFont
  const nsFont* font;
  aFontMetricsWinA->GetFont(font);
  HFONT hfont = (font->sizeAdjust <= 0) 
    ? ::CreateFontIndirect(logFont)
    : aFontMetricsWinA->CreateFontAdjustHandle(aDC, logFont);
  if (hfont) {
    int i = gCharsetToIndex[mCharset];
    if (!gCharsetInfo[i].mCCMap)
      gCharsetInfo[i].mCCMap = gCharsetInfo[i].GenerateMap(&gCharsetInfo[i]);
    if (!gCharsetInfo[i].mCCMap) {
      ::DeleteObject(hfont);
      return 0;
    }
    mCCMap = gCharsetInfo[i].mCCMap;
    mCodePage = gCharsetInfo[i].mCodePage;
    mFont = hfont;

    //XXX can we safely assume the same metrics for all subsets and remove this?
    HFONT oldFont = (HFONT)::SelectObject(aDC, (HGDIOBJ)hfont);
    aFontMetricsWinA->InitMetricsFor(aDC, this);
    ::SelectObject(aDC, (HGDIOBJ)oldFont);

    return 1;
  }
  return 0;
}

nsFontWin*
nsFontMetricsWinA::GetFontFor(HFONT aHFONT)
{
  int count = mLoadedFonts.Count();
  for (int i = 0; i < count; ++i) {
    nsFontWinA* font = (nsFontWinA*)mLoadedFonts[i];
    nsFontSubset** subset = font->mSubsets;
    nsFontSubset** endSubsets = subset + font->mSubsetsCount;
    while (subset < endSubsets) {
      if ((*subset)->mFont == aHFONT) {
        return *subset;
      }
      ++subset;
    }
  }
  NS_ERROR("Cannot find the font that owns the handle");
  return nsnull;
}

nsFontWin*
nsFontMetricsWinA::FindLocalFont(HDC aDC, PRUint32 aChar)
{
  if (!gFamilyNames) {
    if (!InitializeFamilyNames()) {
      return nsnull;
    }
  }
  while (mFontsIndex < mFonts.Count()) {
    if (mFontsIndex == mGenericIndex) {
      return nsnull;
    }
    nsString* name = mFonts.StringAt(mFontsIndex++);
    nsAutoString low(*name);
    ToLowerCase(low);
    nsString* winName = (nsString*)PL_HashTableLookup(gFamilyNames, &low);
    if (!winName) {
      winName = name;
    }
    nsFontWinA* font = (nsFontWinA*)LoadFont(aDC, winName);
    if (font && font->HasGlyph(aChar)) {
      nsFontSubset* subset = font->FindSubset(aDC, (PRUnichar)aChar, this);
      if (subset) 
        return subset;
    }
  }

  return nsnull;
}

nsFontWin*
nsFontMetricsWinA::LoadGenericFont(HDC aDC, PRUint32 aChar, nsString* aName)
{
  for (int i = mLoadedFonts.Count()-1; i >= 0; --i) {

    if (aName->Equals(NS_ConvertASCIItoUCS2(((nsFontWin*)mLoadedFonts[i])->mName),
                                            nsCaseInsensitiveStringComparator()))
      return nsnull;

  }
  nsFontWinA* font = (nsFontWinA*)LoadFont(aDC, aName);
  if (font && font->HasGlyph(aChar)) {
    return font->FindSubset(aDC, (PRUnichar)aChar, this);
  }

  return nsnull;
}

static int
SystemSupportsChar(PRUnichar aChar)
{
  for (int i = 0; i < sizeof(gBitToCharset); ++i) {
    PRUint8 charset = gBitToCharset[i];
    if (charset == DEFAULT_CHARSET) 
      continue;
    if (!HaveConverterFor(charset)) 
      continue;
    int j = gCharsetToIndex[charset];
    if (!gCharsetInfo[j].mCCMap) {
      gCharsetInfo[j].mCCMap = gCharsetInfo[j].GenerateMap(&gCharsetInfo[j]);
      if (!gCharsetInfo[j].mCCMap) 
          return 0;
    }
    if (CCMAP_HAS_CHAR(gCharsetInfo[j].mCCMap, aChar)) 
      return 1;
  }

  return 0;
}

nsFontWin*
nsFontMetricsWinA::FindGlobalFont(HDC aDC, PRUint32 c)
{
  if (!gGlobalFonts) {
    if (!InitializeGlobalFonts(aDC)) {
      return nsnull;
    }
  }
  if (!SystemSupportsChar(c)) {
    return nsnull;
  }
  int count = gGlobalFonts->Count();
  for (int i = 0; i < count; ++i) {
    nsGlobalFont* globalFont = (nsGlobalFont*)gGlobalFonts->ElementAt(i); 
    if (globalFont->flags & NS_GLOBALFONT_SKIP) {
      continue;
    }
    if (!globalFont->ccmap) {
      // don't adjust here, we just want to quickly get the CMAP. Adjusting
      // is meant to only happen when loading the final font in LoadFont()
      HFONT hfont = ::CreateFontIndirect(&globalFont->logFont);
      if (!hfont) {
        continue;
      }
      HFONT oldFont = (HFONT)::SelectObject(aDC, hfont);
      globalFont->ccmap = GetCCMAP(aDC, globalFont->logFont.lfFaceName, nsnull, nsnull);
      ::SelectObject(aDC, oldFont);
      ::DeleteObject(hfont);
      if (!globalFont->ccmap || globalFont->ccmap == gEmptyCCMap) {
        globalFont->flags |= NS_GLOBALFONT_SKIP;
        continue;
      }
      if (SameAsPreviousMap(i)) {
        continue;
      }
    }
    if (CCMAP_HAS_CHAR(globalFont->ccmap, c)) {
      nsFontWinA* font = (nsFontWinA*)LoadGlobalFont(aDC, globalFont);
      if (!font) {
        // disable this global font because when LoadGlobalFont() fails,
        // this means that no subset of interest was found in the font
        globalFont->flags |= NS_GLOBALFONT_SKIP;
        continue;
      }
      nsFontSubset* subset = font->FindSubset(aDC, (PRUnichar)c, this);
      if (subset) {
        return subset;
      }
      mLoadedFonts.RemoveElement(font);
      delete font;
    }
  }

  return nsnull;
}

nsFontWinSubstituteA::nsFontWinSubstituteA(LOGFONT* aLogFont, HFONT aFont,
  PRUint16* aCCMap) : nsFontWinA(aLogFont, aFont, aCCMap)
{
  memset(mRepresentableCharMap, 0, sizeof(mRepresentableCharMap));
}

nsFontWinSubstituteA::~nsFontWinSubstituteA()
{
}

nsFontWin*
nsFontMetricsWinA::FindSubstituteFont(HDC aDC, PRUint32 aChar)
{
  // @see nsFontMetricsWin::FindSubstituteFont() for the general idea behind
  // this function. The fundamental difference here in nsFontMetricsWinA is
  // that the substitute font is setup as a 'wrapper' around a 'substitute
  // subset' that has a glyph for the replacement character. This allows
  // a transparent integration with all the other 'A' functions. 

  if (mSubstituteFont) {
    //make the char representable so that we don't have to go over all font before fallback to 
    //subsituteFont.
    ((nsFontWinSubstituteA*)mSubstituteFont)->SetRepresentable(aChar);
    return ((nsFontWinA*)mSubstituteFont)->mSubsets[0];
  }

  // Try local/loaded fonts first
  int i, count = mLoadedFonts.Count();
  for (i = 0; i < count; ++i) {
    nsFontWinA* font = (nsFontWinA*)mLoadedFonts[i];
    if (font->HasGlyph(NS_REPLACEMENT_CHAR)) {
      nsFontSubset* subset = font->FindSubset(aDC, NS_REPLACEMENT_CHAR, this);
      if (subset) {
        // make a substitute font from this one
        nsAutoString name;
        name.AssignWithConversion(font->mName);
        nsFontWinSubstituteA* substituteFont = (nsFontWinSubstituteA*)LoadSubstituteFont(aDC, &name);
        if (substituteFont) {
          nsFontSubset* substituteSubset = substituteFont->mSubsets[0];
          substituteSubset->mCharset = subset->mCharset;
          if (substituteSubset->Load(aDC, this, substituteFont)) {
            substituteFont->SetRepresentable(aChar);
            mSubstituteFont = (nsFontWin*)substituteFont;
            return substituteSubset;
          }
          mLoadedFonts.RemoveElement(substituteFont);
          delete substituteFont;
        }
      }
    }
  }

  // Try global fonts
  // Since we reach here after FindGlobalFont() is called, we have already
  // scanned the global list of fonts and have set the attributes of interest
  count = gGlobalFonts->Count();
  for (i = 0; i < count; ++i) {
    nsGlobalFont* globalFont = (nsGlobalFont*)gGlobalFonts->ElementAt(i);
    if (!globalFont->ccmap || 
        globalFont->flags & NS_GLOBALFONT_SKIP) {
      continue;
    }
    if (CCMAP_HAS_CHAR(globalFont->ccmap, NS_REPLACEMENT_CHAR)) {
      // to find out the subset of interest, we will load this font for a moment
      BYTE charset = DEFAULT_CHARSET;
      nsFontWinA* font = (nsFontWinA*)LoadGlobalFont(aDC, globalFont);
      if (!font) {
        globalFont->flags |= NS_GLOBALFONT_SKIP;
        continue;
      }
      nsFontSubset* subset = font->FindSubset(aDC, NS_REPLACEMENT_CHAR, this);
      if (subset) {
        charset = subset->mCharset;
      }
      mLoadedFonts.RemoveElement(font);
      delete font;
      if (charset != DEFAULT_CHARSET) {
        // make a substitute font now
        nsFontWinSubstituteA* substituteFont = (nsFontWinSubstituteA*)LoadSubstituteFont(aDC, &globalFont->name);
        if (substituteFont) {
          nsFontSubset* substituteSubset = substituteFont->mSubsets[0];
          substituteSubset->mCharset = charset;
          if (substituteSubset->Load(aDC, this, substituteFont)) {
            substituteFont->SetRepresentable(aChar);
            mSubstituteFont = (nsFontWin*)substituteFont;
            return substituteSubset;
          }
          mLoadedFonts.RemoveElement(substituteFont);
          delete substituteFont;
        }
      }
    }
  }

  // if we ever reach here, the replacement char should be changed to a more common char
  NS_ERROR("Could not provide a substititute font");
  return nsnull;
}

nsFontWin*
nsFontMetricsWinA::LoadSubstituteFont(HDC aDC, nsString* aName)
{
  LOGFONT logFont;
  HFONT hfont = CreateFontHandle(aDC, aName, &logFont);
  if (hfont) {
    HFONT oldFont = (HFONT)::SelectObject(aDC, (HGDIOBJ)hfont);
    char name[sizeof(logFont.lfFaceName)];
    if (::GetTextFace(aDC, sizeof(name), name) &&
        !strcmpi(name, logFont.lfFaceName)) {
      nsFontWinSubstituteA* font = new nsFontWinSubstituteA(&logFont, hfont, nsnull);
      if (font) {
        font->mSubsets = (nsFontSubset**)nsMemory::Alloc(sizeof(nsFontSubset*));
        if (font->mSubsets) {
          font->mSubsets[0] = nsnull;
          nsFontSubsetSubstitute* subset = new nsFontSubsetSubstitute();
          if (subset) {
            font->mSubsetsCount = 1;
            font->mSubsets[0] = subset;
            mLoadedFonts.AppendElement((nsFontWin*)font);
            ::SelectObject(aDC, (HGDIOBJ)oldFont);
            return font;
          }
        }
        ::SelectObject(aDC, (HGDIOBJ)oldFont);
        delete font; // will release hfont, and mSubsets if there, as well
        return nsnull;
      }
    }
    ::SelectObject(aDC, (HGDIOBJ)oldFont);
    ::DeleteObject(hfont);
  }
  return nsnull;
}

nsFontSubset*
nsFontMetricsWinA::LocateFontSubset(HDC aDC, PRUnichar aChar, PRInt32 & aCount, nsFontWinA*& aFont)
{
  nsFontSubset *fontSubset;
  PRInt32 i;

  // see if one of our loaded fonts can represent the character
  for (i = 0; i < aCount; ++i) {
    aFont = (nsFontWinA*)mLoadedFonts[i];
    if (aFont->HasGlyph(aChar)) {
      fontSubset = aFont->FindSubset(aDC, aChar, this);
      if (fontSubset)
        return fontSubset;
    }
  }

  fontSubset = (nsFontSubset*)FindFont(aDC, aChar);
  aFont = nsnull;   //this simply means we don't know and don't bother to figure out
  aCount = mLoadedFonts.Count(); // update since FindFont() can change it
  return fontSubset;
}

nsresult
nsFontMetricsWinA::ResolveForwards(HDC                  aDC,
                                   const PRUnichar*     aString,
                                   PRUint32             aLength,
                                   nsFontSwitchCallback aFunc, 
                                   void*                aData)
{
  NS_ASSERTION(aString || !aLength, "invalid call");
  const PRUnichar* firstChar = aString;
  const PRUnichar* lastChar  = aString + aLength;
  const PRUnichar* currChar  = firstChar;
  nsFontSubset* currSubset;
  nsFontSubset* nextSubset;
  nsFontWinA* currFont;
  PRInt32 count;
  nsFontSwitch fontSwitch;

  if (firstChar == lastChar)
    return NS_OK;

  // see if one of our loaded fonts can represent the current character
  count = mLoadedFonts.Count();
  currSubset = LocateFontSubset(aDC, *currChar, count, currFont);

  //This if block is meant to speedup the process in normal situation, when
  //most characters can be found in first font
  if (currFont == mLoadedFonts[0]) {
    while (++currChar < lastChar && currFont->HasGlyph(*(currChar)) && currSubset->HasGlyph(*currChar)) ;

    fontSwitch.mFontWin = currSubset;
    if (!(*aFunc)(&fontSwitch, firstChar, currChar - firstChar, aData))
      return NS_OK;
    if (currChar == lastChar)
      return NS_OK;
    // continue with the next substring, re-using the available loaded fonts
    firstChar = currChar;
    currSubset = LocateFontSubset(aDC, *currChar, count, currFont); 
  }

  while (++currChar < lastChar) {
    nextSubset = LocateFontSubset(aDC, *currChar, count, currFont);
    if (nextSubset != currSubset) {
      // We have a substring that can be represented with the same font, and
      // we are about to switch fonts, it is time to notify our caller.
      fontSwitch.mFontWin = currSubset;
      if (!(*aFunc)(&fontSwitch, firstChar, currChar - firstChar, aData))
        return NS_OK;
      // continue with the next substring, re-using the available loaded fonts
      firstChar = currChar;
      currSubset = nextSubset; // use the font found earlier for the char
    }
  }

  //do it for last part of the string
  fontSwitch.mFontWin = currSubset;
  NS_ASSERTION(currSubset, "invalid font here. ");
  (*aFunc)(&fontSwitch, firstChar, currChar - firstChar, aData);

  return NS_OK;
}

nsresult
nsFontMetricsWinA::ResolveBackwards(HDC                  aDC,
                                    const PRUnichar*     aString,
                                    PRUint32             aLength,
                                    nsFontSwitchCallback aFunc, 
                                    void*                aData)
{
  NS_ASSERTION(aString || !aLength, "invalid call");
  const PRUnichar* firstChar = aString + aLength - 1;
  const PRUnichar* lastChar  = aString - 1;
  const PRUnichar* currChar  = firstChar;
  nsFontSubset* currSubset;
  nsFontSubset* nextSubset;
  nsFontWinA* currFont;
  PRInt32 count;
  nsFontSwitch fontSwitch;

  if (firstChar == lastChar)
    return NS_OK;

  // see if one of our loaded fonts can represent the current character
  count = mLoadedFonts.Count();
  currSubset = LocateFontSubset(aDC, *currChar, count, currFont);

  while (--currChar < lastChar) {
    nextSubset = LocateFontSubset(aDC, *currChar, count, currFont);
    if (nextSubset != currSubset) {
      // We have a substring that can be represented with the same font, and
      // we are about to switch fonts, it is time to notify our caller.
      fontSwitch.mFontWin = currSubset;
      if (!(*aFunc)(&fontSwitch, firstChar, firstChar - currChar, aData))
        return NS_OK;
      // continue with the next substring, re-using the available loaded fonts
      firstChar = currChar;
      currSubset = nextSubset; // use the font found earlier for the char
    }
  }

  //do it for last part of the string
  fontSwitch.mFontWin = currSubset;
  (*aFunc)(&fontSwitch, firstChar, firstChar - currChar, aData);

  return NS_OK;
}

// The Font Enumerator

nsFontEnumeratorWin::nsFontEnumeratorWin()
{
  NS_INIT_ISUPPORTS();
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
  NS_ENSURE_ARG_POINTER(aCount);
  NS_ENSURE_ARG_POINTER(aResult);

  *aCount = 0;
  *aResult = nsnull;

  if (!gInitializedFontEnumerator) {
    if (!InitializeFontEnumerator()) {
      return NS_ERROR_FAILURE;
    }
  }

  int count = nsFontMetricsWin::gGlobalFonts->Count();
  PRUnichar** array = (PRUnichar**)nsMemory::Alloc(count * sizeof(PRUnichar*));
  if (!array) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  for (int i = 0; i < count; ++i) {
    nsGlobalFont* font = (nsGlobalFont*)nsFontMetricsWin::gGlobalFonts->ElementAt(i);
    PRUnichar* str = ToNewUnicode(font->name);
    if (!str) {
      for (i = i - 1; i >= 0; --i) {
        nsMemory::Free(array[i]);
      }
      nsMemory::Free(array);
      return NS_ERROR_OUT_OF_MEMORY;
    }
    array[i] = str;
  }

  NS_QuickSort(array, count, sizeof(PRUnichar*), CompareFontNames, nsnull);

  *aCount = count;
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
  for (dword = 0; dword < 2; ++dword) {
    for (int bit = 0; bit < sizeof(DWORD) * 8; ++bit) {
      if ((array[dword] >> bit) & 1) {
        if (!strcmp(gCharsetInfo[gCharsetToIndex[gBitToCharset[i]]].mLangGroup,
                    aLangGroup)) {
          return 1;
        }
      }
      ++i;
    }
  }

  return 0;
}

static int
FontMatchesGenericType(nsGlobalFont* aFont, const char* aGeneric,
  const char* aLangGroup)
{
  if (!strcmp(aLangGroup, "ja"))    return 1;
  if (!strcmp(aLangGroup, "zh-TW")) return 1;
  if (!strcmp(aLangGroup, "zh-CN")) return 1;
  if (!strcmp(aLangGroup, "ko"))    return 1;
  if (!strcmp(aLangGroup, "th"))    return 1;
  if (!strcmp(aLangGroup, "he"))    return 1;
  if (!strcmp(aLangGroup, "ar"))    return 1;

  switch (aFont->logFont.lfPitchAndFamily & 0xF0) {
    case FF_DONTCARE:   return 0;
    case FF_ROMAN:      return !strcmp(aGeneric, "serif");
    case FF_SWISS:      return !strcmp(aGeneric, "sans-serif");
    case FF_MODERN:     return !strcmp(aGeneric, "monospace");
    case FF_SCRIPT:     return !strcmp(aGeneric, "cursive");
    case FF_DECORATIVE: return !strcmp(aGeneric, "fantasy");
  }

  return 0;
}

NS_IMETHODIMP
nsFontEnumeratorWin::EnumerateFonts(const char* aLangGroup,
  const char* aGeneric, PRUint32* aCount, PRUnichar*** aResult)
{
  NS_ENSURE_ARG_POINTER(aLangGroup);
  NS_ENSURE_ARG_POINTER(aGeneric);
  NS_ENSURE_ARG_POINTER(aCount);
  NS_ENSURE_ARG_POINTER(aResult);

  *aCount = 0;
  *aResult = nsnull;

  if ((!strcmp(aLangGroup, "x-unicode")) ||
      (!strcmp(aLangGroup, "x-user-def"))) {
    return EnumerateAllFonts(aCount, aResult);
  }

  if (!gInitializedFontEnumerator) {
    if (!InitializeFontEnumerator()) {
      return NS_ERROR_FAILURE;
    }
  }

  int count = nsFontMetricsWin::gGlobalFonts->Count();
  PRUnichar** array = (PRUnichar**)nsMemory::Alloc(count * sizeof(PRUnichar*));
  if (!array) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  int j = 0;
  for (int i = 0; i < count; ++i) {
    nsGlobalFont* font = (nsGlobalFont*)nsFontMetricsWin::gGlobalFonts->ElementAt(i);
    if (SignatureMatchesLangGroup(&font->signature, aLangGroup) &&
        FontMatchesGenericType(font, aGeneric, aLangGroup)) {
      PRUnichar* str = ToNewUnicode(font->name);
      if (!str) {
        for (j = j - 1; j >= 0; --j) {
          nsMemory::Free(array[j]);
        }
        nsMemory::Free(array);
        return NS_ERROR_OUT_OF_MEMORY;
      }
      array[j] = str;
      ++j;
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
  NS_ENSURE_ARG_POINTER(aLangGroup);
  NS_ENSURE_ARG_POINTER(aResult);

  *aResult = PR_FALSE;
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
  int count = nsFontMetricsWin::gGlobalFonts->Count();
  for (int i = 0; i < count; ++i) {
    nsGlobalFont* font = (nsGlobalFont*)nsFontMetricsWin::gGlobalFonts->ElementAt(i);
    if (SignatureMatchesLangGroup(&font->signature, aLangGroup)) {
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
  int charsetCounter = 0;
  PRUint16 maskBitBefore = 0;

  // initialize updateFontList
  *updateFontList = PR_FALSE;

  // iterate langGoup; skip DEFAULT
  for (charsetCounter = 1; charsetCounter < eCharset_COUNT; ++charsetCounter) {
    HaveFontFor(gCharsetInfo[charsetCounter].mLangGroup, &haveFontForLang);
    if (haveFontForLang) {
      maskBitBefore |= PR_BIT(charsetCounter);
      haveFontForLang = PR_FALSE;
    }
  }

  // delete gGlobalFonts
  if (nsFontMetricsWin::gGlobalFonts) {
    for (int i = nsFontMetricsWin::gGlobalFonts->Count()-1; i >= 0; --i) {
      nsGlobalFont* font = (nsGlobalFont*)nsFontMetricsWin::gGlobalFonts->ElementAt(i);
      delete font;
    }
    delete nsFontMetricsWin::gGlobalFonts;
    nsFontMetricsWin::gGlobalFonts = nsnull;
  }

  // reconstruct gGlobalFonts
  HDC dc = ::GetDC(nsnull);
  if (!nsFontMetricsWin::InitializeGlobalFonts(dc)) {
    ::ReleaseDC(nsnull, dc);
    return NS_ERROR_FAILURE;
  }
  ::ReleaseDC(nsnull, dc);

  PRUint16 maskBitAfter = 0;
  // iterate langGoup again; skip DEFAULT
  for (charsetCounter = 1; charsetCounter < eCharset_COUNT; ++charsetCounter) {
    HaveFontFor(gCharsetInfo[charsetCounter].mLangGroup, &haveFontForLang);
    if (haveFontForLang) {
      maskBitAfter |= PR_BIT(charsetCounter);
      haveFontForLang = PR_FALSE;
    }
  }

  // check for change
  *updateFontList = (maskBitBefore != maskBitAfter);
  return NS_OK;
}
