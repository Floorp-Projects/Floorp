/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

// ToDo: Unicode, encoding.
//       Revision by someone who *really* understands OS/2 fonts.

#include "nsGfxDefs.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "nsILanguageAtomService.h"
#include "nsICharsetConverterManager.h"
#include "nsLocaleCID.h"
#include "nsILocaleService.h"
#include "nsICharRepresentable.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsFontMetricsOS2.h"
#include "nsQuickSort.h"
#include "nsTextFormatter.h"
#include "prmem.h"
#include "plhash.h"
#include "prprf.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"

#include "nsOS2Uni.h"

#ifdef MOZ_MATHML
  #include <math.h>
#endif

#undef USER_DEFINED
#define USER_DEFINED "x-user-def"

static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kLocaleServiceCID, NS_LOCALESERVICE_CID); 

#define NS_MAX_FONT_WEIGHT 900
#define NS_MIN_FONT_WEIGHT 100

#undef CHAR_BUFFER_SIZE
#define CHAR_BUFFER_SIZE 1024


/***** Global variables *****/
nsTHashtable<GlobalFontEntry>* nsFontMetricsOS2::gGlobalFonts = nsnull;
PRBool        nsFontMetricsOS2::gSubstituteVectorFonts = PR_TRUE;
PLHashTable  *nsFontMetricsOS2::gFamilyNames = nsnull;

static nsIPref           *gPref = nsnull;
static nsIAtom           *gUsersLocale = nsnull;
static nsIAtom           *gSystemLocale = nsnull;
static nsIAtom           *gUserDefined = nsnull;
static int                gInitialized = 0;
static ULONG              gSystemCodePage = 0;
static ULONG              gCurrHashValue = 1;



/***** Charset structures *****/
enum nsCharset
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
/*  USHORT   mMask; */
  PRUint16 mCodePage;
  char*    mLangGroup;
};

static nsCharsetInfo gCharsetInfo[eCharset_COUNT] =
{
  { "DEFAULT",     /* 0,                */  0,    "" },
  { "ANSI",        /* FM_DEFN_LATIN1,   */  1252, "x-western" },
  { "EASTEUROPE",  /* FM_DEFN_LATIN2,   */  1250, "x-central-euro" },
  { "RUSSIAN",     /* FM_DEFN_CYRILLIC, */  1251, "x-cyrillic" },
  { "GREEK",       /* FM_DEFN_GREEK,    */  813,  "el" },
  { "TURKISH",     /* 0,                */  1254, "tr" },
  { "HEBREW",      /* FM_DEFN_HEBREW,   */  1208,  "he" },
  { "ARABIC",      /* FM_DEFN_ARABIC,   */  864,  "ar" },
  { "BALTIC",      /* 0,                */  1257, "x-baltic" },
  { "THAI",        /* FM_DEFN_THAI,     */  874,  "th" },
  { "SHIFTJIS",    /* FM_DEFN_KANA,     */  932,  "ja" },
  { "GB2312",      /* FM_DEFN_KANA,     */  1381, "zh-CN" },
  { "HANGEUL",     /* FM_DEFN_KANA,     */  949,  "ko" },
  { "CHINESEBIG5", /* FM_DEFN_KANA,     */  950,  "zh-TW" },
  { "JOHAB",       /* FM_DEFN_KANA,     */  1361, "ko-XXX", }
};


/**********************************************************
    nsFontMetricsOS2
 **********************************************************/    

static void
FreeGlobals(void)
{
  gInitialized = 0;

  NS_IF_RELEASE(gPref);
  NS_IF_RELEASE(gUsersLocale);
  NS_IF_RELEASE(gSystemLocale);
  NS_IF_RELEASE(gUserDefined);

  if (nsFontMetricsOS2::gGlobalFonts) {
    nsFontMetricsOS2::gGlobalFonts->Clear();
    delete nsFontMetricsOS2::gGlobalFonts;
    nsFontMetricsOS2::gGlobalFonts = nsnull;
  }

  if (nsFontMetricsOS2::gFamilyNames) {
    PL_HashTableDestroy(nsFontMetricsOS2::gFamilyNames);
    nsFontMetricsOS2::gFamilyNames = nsnull;
  }
}

class nsFontCleanupObserver : public nsIObserver {
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  nsFontCleanupObserver() { }
  virtual ~nsFontCleanupObserver() {}
};

NS_IMPL_ISUPPORTS1(nsFontCleanupObserver, nsIObserver)

NS_IMETHODIMP nsFontCleanupObserver::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *someData)
{
  if (!nsCRT::strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    FreeGlobals();
  }
  return NS_OK;
}

static nsFontCleanupObserver *gFontCleanupObserver;

static nsresult
InitGlobals(void)
{
  nsServiceManager::GetService(kPrefCID, NS_GET_IID(nsIPref),
    (nsISupports**) &gPref);
  if (!gPref) {
    FreeGlobals();
    return NS_ERROR_FAILURE;
  }

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
    UINT cp = ::WinQueryCp(HMQ_CURRENT);
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

  ULONG numCP = ::WinQueryCpList((HAB)0, 0, NULL);
  UniChar codepage[20];
  if (numCP > 0) {
     ULONG * pCPList = (ULONG*)nsMemory::Alloc(numCP*sizeof(ULONG));
     if (::WinQueryCpList( (HAB)0, numCP, pCPList)) {
        for (int i = 0;i<numCP ;i++ ) {
           if (pCPList[i] == 1386) {
              int unirc = ::UniMapCpToUcsCp( 1386, codepage, 20);
              if (unirc == ULS_SUCCESS) {
                 UconvObject Converter = 0;
                 int unirc = ::UniCreateUconvObject( codepage, &Converter);
                 if (unirc == ULS_SUCCESS) {
                    gCharsetInfo[11].mCodePage = 1386;
                   ::UniFreeUconvObject(Converter);
                 }
              }
           } else if (pCPList[i] == 943) {
              int unirc = ::UniMapCpToUcsCp( 943, codepage, 20);
              if (unirc == ULS_SUCCESS) {
                 UconvObject Converter = 0;
                 int unirc = ::UniCreateUconvObject( codepage, &Converter);
                 if (unirc == ULS_SUCCESS) {
                    gCharsetInfo[10].mCodePage = 943;
                   ::UniFreeUconvObject(Converter);
                 }
              }
           }
        }
     }
     nsMemory::Free(pCPList);
  }

  gSystemCodePage = ::WinQueryCp(HMQ_CURRENT);

  if (!nsFontMetricsOS2::gGlobalFonts) {
    nsresult res = nsFontMetricsOS2::InitializeGlobalFonts();
    if (NS_FAILED(res)) {
      FreeGlobals();
      return res;
    }
  }

  nsresult res = gPref->GetBoolPref("browser.display.substitute_vector_fonts",
                                    &nsFontMetricsOS2::gSubstituteVectorFonts);
  NS_ASSERTION( NS_SUCCEEDED(res), "Could not get pref 'browser.display.substitute_vector_fonts'" );

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

#ifdef DEBUG_FONT_STRUCT_ALLOCS
unsigned long nsFontMetricsOS2::mRefCount = 0;
#endif

nsFontMetricsOS2::nsFontMetricsOS2()
{
#ifdef DEBUG_FONT_STRUCT_ALLOCS
  mRefCount++;
  printf("+++ nsFontMetricsOS2 total = %d\n", mRefCount);
#endif
}

nsFontMetricsOS2::~nsFontMetricsOS2()
{
  mFontHandle = nsnull; // released below
  mUnicodeFont = nsnull; // release below
  mWesternFont = nsnull; // release below

  for (PRInt32 i = mLoadedFonts.Count()-1; i >= 0; --i) {
    delete (nsFontOS2*)mLoadedFonts[i];
  }
  mLoadedFonts.Clear();

  if (mDeviceContext) {
    // Notify our device context that owns us so that it can update its font cache
    mDeviceContext->FontMetricsDeleted(this);
    mDeviceContext = nsnull;
  }
#ifdef DEBUG_FONT_STRUCT_ALLOCS
  mRefCount--;
  printf("--- nsFontMetricsOS2 total = %d\n", mRefCount);
#endif
}

NS_IMPL_ISUPPORTS1(nsFontMetricsOS2, nsIFontMetrics)

NS_IMETHODIMP
nsFontMetricsOS2::Init( const nsFont &aFont,  nsIAtom* aLangGroup,
                        nsIDeviceContext *aContext)
{
  nsresult res;
  if (!gInitialized) {
    res = InitGlobals();
    if (NS_FAILED(res)) {
      return res;
    }
  }

  mFont = aFont;
  mLangGroup = aLangGroup;

  //don't addref this to avoid circular refs
  mDeviceContext = (nsDeviceContextOS2 *) aContext;
  return RealizeFont();
}

NS_IMETHODIMP
nsFontMetricsOS2::Destroy()
{
  mDeviceContext = nsnull;
  return NS_OK;
}

// Utility; delete [] when done.
static FONTMETRICS* getMetrics( long &lFonts, PCSZ facename, HPS hps)
{
   LONG lWant = 0;
   lFonts = GFX (::GpiQueryFonts(hps, QF_PUBLIC | QF_PRIVATE,
                                 facename, &lWant, 0, 0),
                 GPI_ALTERROR);
   if (!lFonts) {
      return NULL;
   }
   FONTMETRICS* pMetrics = (FONTMETRICS*)nsMemory::Alloc(lFonts * sizeof(FONTMETRICS));

   GFX (::GpiQueryFonts(hps, QF_PUBLIC | QF_PRIVATE, facename,
                        &lFonts, sizeof (FONTMETRICS), pMetrics),
        GPI_ALTERROR);

   return pMetrics;
}

/* For a vector font, we create a charbox based directly on mFont.size, so
 * we don't need to calculate anything here.  For image fonts, though, we
 * want to find the closest match for the given mFont.size.
 */
void
nsFontMetricsOS2::SetFontHandle(HPS aPS, nsFontOS2* aFont)
{
  float app2dev, reqEmHeight;
  app2dev = mDeviceContext->AppUnitsToDevUnits();
  reqEmHeight = mFont.size * app2dev;

  FATTRS* fattrs = &(aFont->mFattrs);
  if (fattrs->fsFontUse == 0)  // if image font
  {
    long lFonts = 0;
    FONTMETRICS* pMetrics = getMetrics( lFonts, fattrs->szFacename, aPS);

    int reqPointSize = NSTwipsToIntPoints(mFont.size);
    int browserRes = mDeviceContext->GetDPI();

    int minSize = 99, maxSize = 0, curEmHeight = 0;
    for (int i = 0; i < lFonts; i++)
    {
      // If we are asked for a specific point size for which we have an
      // appropriate font, use it.  This avoids us choosing an incorrect
      // size due to rounding issues
      if (pMetrics[i].sYDeviceRes == browserRes &&
          pMetrics[i].sNominalPointSize / 10 == reqPointSize)
      {
        // image face found fine, set required size in fattrs.
        curEmHeight = pMetrics[i].lEmHeight;
        fattrs->lMaxBaselineExt = pMetrics[i].lMaxBaselineExt;
        fattrs->lAveCharWidth = pMetrics[i].lAveCharWidth;
        minSize = maxSize = pMetrics[i].lEmHeight;
        break;
      }
      else
      {
        if (abs(pMetrics[i].lEmHeight - reqEmHeight) < abs(curEmHeight - reqEmHeight))
        {
          curEmHeight = pMetrics[i].lEmHeight;
          fattrs->lMaxBaselineExt = pMetrics[i].lMaxBaselineExt;
          fattrs->lAveCharWidth = pMetrics[i].lAveCharWidth;
        }
        else if (abs(pMetrics[i].lEmHeight - reqEmHeight) == abs(curEmHeight - reqEmHeight))
        {
          if ((pMetrics[i].lEmHeight) > curEmHeight)
          {
            curEmHeight = pMetrics[i].lEmHeight;
            fattrs->lMaxBaselineExt = pMetrics[i].lMaxBaselineExt;
            fattrs->lAveCharWidth = pMetrics[i].lAveCharWidth;
          }
        }
      }
      
      // record the min/max point size available for given font
      if (pMetrics[i].lEmHeight > maxSize)
        maxSize = pMetrics[i].lEmHeight;
      if (pMetrics[i].lEmHeight < minSize)
        minSize = pMetrics[i].lEmHeight;
    }

    nsMemory::Free(pMetrics);
    
    // Enable font substitution if the requested size is outside of the range
    //  of available sizes by more than 3
    if (reqEmHeight < minSize - 3 ||
        reqEmHeight > maxSize + 3)
    {
      // If the browser.display.substitute_vector_fonts pref is set, then we
      //  exchange Times New Roman for Tms Rmn and Helvetica for Helv if the 
      //  requested points size is less than the minimum or more than the 
      //  maximum point size available for Tms Rmn and Helv.
      char alias[FACESIZE];
      if( gSubstituteVectorFonts &&
          GetVectorSubstitute( aPS, fattrs->szFacename, alias ))
      {
        strcpy( fattrs->szFacename, alias );
        fattrs->fsFontUse = FATTR_FONTUSE_OUTLINE | FATTR_FONTUSE_TRANSFORMABLE;
        fattrs->fsSelection &= ~(FM_SEL_BOLD | FM_SEL_ITALIC);
      }
    }
    
    if (fattrs->fsFontUse == 0) {    // if still image font
      reqEmHeight = curEmHeight;
    }
  }

   // Add effects
  if (mFont.decorations & NS_FONT_DECORATION_UNDERLINE) {
    fattrs->fsSelection |= FATTR_SEL_UNDERSCORE;
  }
  if (mFont.decorations & NS_FONT_DECORATION_LINE_THROUGH) {
    fattrs->fsSelection |= FATTR_SEL_STRIKEOUT;
  }

   // Encoding:
   //  There doesn't seem to be any encoding stuff yet, so guess.
   //  (XXX unicode hack; use same codepage as converter!)
  const char* langGroup;
  mLangGroup->GetUTF8String(&langGroup);
  for (int j=0; j < eCharset_COUNT; j++)
  {
    if (langGroup[0] == gCharsetInfo[j].mLangGroup[0])
    {
      if (!strcmp(langGroup, gCharsetInfo[j].mLangGroup))
      {
        mConvertCodePage = gCharsetInfo[j].mCodePage;
        break;
      }
    }
  }

  // Symbols fonts must be created with codepage 65400,
  // so use 65400 for the fattrs codepage. We still do
  // conversions with the charset codepage
  if (fattrs->usCodePage != 65400) {
    fattrs->usCodePage = mConvertCodePage;
  }

  // set up the charbox;  set for image fonts also, in case we need to
  //  substitute a vector font later on (for UTF-8, etc)
  long lFloor = NSToIntFloor( reqEmHeight ); 
  aFont->mCharbox.cx = MAKEFIXED( lFloor, (reqEmHeight - (float)lFloor) * 65536.0f );
  aFont->mCharbox.cy = aFont->mCharbox.cx;
}

/* aName is a font family name.  see if fonts of that family exist
 *  if so, return font structure with family name
 */
nsFontOS2*
nsFontMetricsOS2::LoadFont(HPS aPS, const nsString& aFontname)
{
  nsFontOS2* font = nsnull;
  char alias[FACESIZE];

   // set style flags
  PRBool bBold = mFont.weight > NS_FONT_WEIGHT_NORMAL;
  PRBool bItalic = (mFont.style & (NS_FONT_STYLE_ITALIC | NS_FONT_STYLE_OBLIQUE));
  USHORT flags = bBold ? FM_SEL_BOLD : 0;
  flags |= bItalic ? FM_SEL_ITALIC : 0;

   // always pass vector fonts to the printer
  if (!mDeviceContext->SupportsRasterFonts() &&
      GetVectorSubstitute(aPS, aFontname, alias))
  {
    font = new nsFontOS2();
    strcpy(font->mFattrs.szFacename, alias);
    font->mFattrs.fsFontUse = FATTR_FONTUSE_OUTLINE |
                              FATTR_FONTUSE_TRANSFORMABLE;
    SetFontHandle(aPS, font);
    return font;
  }

  GlobalFontEntry* globalEntry = gGlobalFonts->GetEntry(aFontname);
  if (globalEntry) {
    nsMiniMetrics* metrics = globalEntry->mMetrics;
    nsMiniMetrics* plainFont = nsnull;
    while (metrics) {
      if ((metrics->fsSelection & (FM_SEL_ITALIC | FM_SEL_BOLD)) == flags) {
        font = new nsFontOS2();
        FATTRS* fattrs = &(font->mFattrs);

        strcpy(fattrs->szFacename, metrics->szFacename);
        if (metrics->fsDefn & FM_DEFN_OUTLINE ||
            !mDeviceContext->SupportsRasterFonts()) {
          font->mFattrs.fsFontUse = FATTR_FONTUSE_OUTLINE |
                                    FATTR_FONTUSE_TRANSFORMABLE;
        }
        if (metrics->fsType & FM_TYPE_MBCS)
          font->mFattrs.fsType |= FATTR_TYPE_MBCS;
        if (metrics->fsType & FM_TYPE_DBCS)
          font->mFattrs.fsType |= FATTR_TYPE_DBCS;
        font->mFattrs.usCodePage = globalEntry->mCodePage;

        SetFontHandle(aPS, font);
        break;
      }

      // Save ref to 'plain' font (non-bold, non-italic)
      if (!plainFont && !(metrics->fsSelection & (FM_SEL_ITALIC | FM_SEL_BOLD)))
        plainFont = metrics;

      metrics = metrics->mNext;
    }

    // A font of family "familyname" and with the applied effects (bold &
    // italic) was not found.  Therefore, just look for a 'regular' font
    // (that is not italic and not bold), and then have the
    // system simulate the appropriate effects (see RealizeFont()).
    if (!font && plainFont) {
      font = new nsFontOS2();
      FATTRS* fattrs = &(font->mFattrs);
 
      strcpy(fattrs->szFacename, plainFont->szFacename);
      if (plainFont->fsDefn & FM_DEFN_OUTLINE ||
          !mDeviceContext->SupportsRasterFonts()) {
        fattrs->fsFontUse = FATTR_FONTUSE_OUTLINE |
                            FATTR_FONTUSE_TRANSFORMABLE;
      } else {
        fattrs->fsFontUse = 0;
      }
      if (plainFont->fsType & FM_TYPE_MBCS)
        fattrs->fsType |= FATTR_TYPE_MBCS;
      if (plainFont->fsType & FM_TYPE_DBCS)
        fattrs->fsType |= FATTR_TYPE_DBCS;

       // fake the effects
      if (bBold)
        fattrs->fsSelection |= FATTR_SEL_BOLD;
      if (bItalic)
        fattrs->fsSelection |= FATTR_SEL_ITALIC;
      fattrs->usCodePage = globalEntry->mCodePage;

      SetFontHandle(aPS, font);
    }
  }

  if (font) {
    mLoadedFonts.AppendElement(font);
    return font;
  }

  // If a font was not found, then maybe "familyname" is really a face name.
  // See if a font with that facename exists on system and fake any applied
  // effects
  long lFonts = 0;
  char* fontname = ToNewCString(aFontname);
  FONTMETRICS* pMetrics = getMetrics(lFonts, fontname, aPS);

  if (lFonts > 0) {
    font = new nsFontOS2();
    strcpy(font->mFattrs.szFacename, fontname);
    font->mFattrs.fsFontUse = FATTR_FONTUSE_OUTLINE |
                              FATTR_FONTUSE_TRANSFORMABLE;

    if (bBold)
      font->mFattrs.fsSelection |= FATTR_SEL_BOLD;
    if (bItalic)
      font->mFattrs.fsSelection |= FATTR_SEL_ITALIC;

    if (mDeviceContext->SupportsRasterFonts()) {
      for (int i = 0 ; i < lFonts ; i++) {
        if (!(pMetrics[i].fsDefn & FM_DEFN_OUTLINE)) {
          font->mFattrs.fsFontUse = 0;
          break;
        }
      }
    }
  }

  nsMemory::Free(pMetrics);

  if (font) {
    SetFontHandle(aPS, font);
    mLoadedFonts.AppendElement(font);
  }

  return font;
}

typedef struct nsFontFamilyName
{
  char* mName;
  char* mWinName;
} nsFontFamilyName;

static nsFontFamilyName gBadDBCSFontMapping[] =
{
  { "\xB7\xC2\xCB\xCE\xB3\xA3\xB9\xE6", "IBM Fang Song SC"},
  { "\xBA\xDA\xCC\xE5\xB3\xA3\xB9\xE6", "IBM Hei SC"},
  { "\xBF\xAC\xCC\xE5\xB3\xA3\xB9\xE6", "IBM Kai SC"},
  { "\x5B\x8B\x4F\x53\x5E\x38\x89\xC4", "IBM Song SC"},
  { "\x91\x76\x91\xCC\x8F\xED",         "IBM Song SC"},

  { "@\xB7\xC2\xCB\xCE\xB3\xA3\xB9\xE6", "@IBM Fang Song SC"},
  { "@\xBA\xDA\xCC\xE5\xB3\xA3\xB9\xE6", "@IBM Hei SC"},
  { "@\xBF\xAC\xCC\xE5\xB3\xA3\xB9\xE6", "@IBM Kai SC"},
  { "@\x5B\x8B\x4F\x53\x5E\x38\x89\xC4", "@IBM Song SC"},
  { "@\x91\x76\x91\xCC\x8F\xED",         "@IBM Song SC"},

  { "\xAC\xD1\xFA\x80\xFA\xBD\x8F\x82",  "MOEKai TC"},
  { "\xBC\xD0\xB7\xC7\xB7\xA2\xC5\xE9",  "MOEKai TC"},
  { "\xAC\xD1\xFA\x80\x15\xA7\x8F\x82",  "MOESung TC"},
  { "\xBC\xD0\xB7\xC7\xA7\xBA\xC5\xE9",  "MOESung TC"},
  { "\xCF\xCF\x14\xB6\x8F\x82",          "Hei TC"},
  { "\xA4\xA4\xB6\xC2\xC5\xE9",          "Hei TC"},

  { "@\xAC\xD1\xFA\x80\xFA\xBD\x8F\x82",  "@MOEKai TC"},
  { "@\xBC\xD0\xB7\xC7\xB7\xA2\xC5\xE9",  "@MOEKai TC"},
  { "@\xAC\xD1\xFA\x80\x15\xA7\x8F\x82",  "@MOESong TC"},
  { "@\xBC\xD0\xB7\xC7\xA7\xBA\xC5\xE9",  "@MOESong TC"},
  { "@\xCF\xCF\x14\xB6\x8F\x82",          "@Hei TC"},
  { "@\xA4\xA4\xB6\xC2\xC5\xE9",          "@Hei TC"},
  { nsnull, nsnull }
};

#ifdef DEBUG
PR_STATIC_CALLBACK(PLDHashOperator)
DebugOutputEnumerator(GlobalFontEntry* aEntry, void* aData)
{
  printf("---------------------------------------------------------------------\n");
  printf(" [[]] %s\n", NS_LossyConvertUCS2toASCII(aEntry->GetKey()).get());
  nsMiniMetrics* metrics = aEntry->mMetrics;
  while (metrics) {
    printf("  %32s", metrics->szFacename);

    if (metrics->fsDefn & FM_DEFN_OUTLINE)
      printf(" : vector");
    else
      printf(" : bitmap");
      
    if (metrics->fsSelection & FM_SEL_BOLD)
      printf(" : bold");
    else
      printf(" :     ");
      
    if (metrics->fsSelection & FM_SEL_ITALIC)
      printf(" : italic");
    else
      printf(" :       ");

    if (metrics->fsType & FM_TYPE_DBCS ||
        metrics->fsType & FM_TYPE_MBCS)
      printf(" : M/DBCS");
    else
      printf(" :       ");

    printf("  : cp=%5d\n", aEntry->mCodePage );
    
    metrics = metrics->mNext;
  }

  return PL_DHASH_NEXT;
}
#endif

nsresult
nsFontMetricsOS2::InitializeGlobalFonts()
{
  gGlobalFonts = new nsTHashtable<GlobalFontEntry>;
  if (!gGlobalFonts->Init(64))
    return NS_ERROR_OUT_OF_MEMORY;

  HPS ps = ::WinGetScreenPS(HWND_DESKTOP);
  LONG lRemFonts = 0, lNumFonts;
  lNumFonts = GFX (::GpiQueryFonts(ps, QF_PUBLIC, NULL, &lRemFonts, 0, 0),
                   GPI_ALTERROR);
  FONTMETRICS* pFontMetrics = (PFONTMETRICS) nsMemory::Alloc(lNumFonts * sizeof(FONTMETRICS));
  lRemFonts = GFX (::GpiQueryFonts(ps, QF_PUBLIC, NULL, &lNumFonts,
                                   sizeof (FONTMETRICS), pFontMetrics),
                   GPI_ALTERROR);

  for( int i = 0; i < lNumFonts; i++ )
  {
    FONTMETRICS* font = &(pFontMetrics[i]);
     // The discrepencies between the Courier bitmap and outline fonts are
     // too much to deal with, so we only use the outline font
    if (strcmp(font->szFamilyname, "Courier") == 0 &&
        !(font->fsDefn & FM_DEFN_OUTLINE)) {
      continue;
    }

    // The facenames for Roman are "Tms Rmn...", for Swiss "Helv...".  This
    // conflicts with the actual "Tms Rmn" and "Helv" font families.  So, we
    // skip over these.
    if (strcmp(pFontMetrics[i].szFamilyname, "Roman") == 0 ||
        strcmp(pFontMetrics[i].szFamilyname, "Swiss") == 0) {
      continue;
    }

     // Problem:  OS/2 has many non-standard fonts that do not follow the
     //           normal Family-name/Face-name conventions (i.e. 'foo',
     //           'foo bold', 'foo italic', 'foo bold italic').  This is
     //           especially true for DBCS fonts (i.e. the 'WarpSans' family
     //           can contain the 'WarpSans', 'WarpSans Bold', and 'WarpSans
     //           Combined' faces).
     // Solution: Unfortunately, there is no perfect way to handle this.  After
     //           many attempts, we will attempt to remedy the situation by
     //           searching the Facename for certain indicators ('bold',
     //           'italic', 'oblique', 'regular').  If the Facename contains
     //           one of these indicators, then we will create the sort key
     //           based on the Familyname.  Otherwise, use the Facename.
    nsAutoString fontptr;
    if (PL_strcasestr(font->szFacename, "bold") != nsnull ||
        PL_strcasestr(font->szFacename, "italic") != nsnull ||
        PL_strcasestr(font->szFacename, "oblique") != nsnull ||
        PL_strcasestr(font->szFacename, "regular") != nsnull ||
        PL_strcasestr(font->szFacename, "-normal") != nsnull)
    {
      CopyASCIItoUCS2(nsDependentCString(NS_STATIC_CAST(char*, font->szFamilyname)),
                      fontptr);
    } else {
      CopyASCIItoUCS2(nsDependentCString(NS_STATIC_CAST(char*, font->szFacename)),
                      fontptr);
    }

    // The fonts in gBadDBCSFontMapping do not display well in non-Chinese
    //   systems.  Map them to a more intelligible name.
    if (font->fsType & FM_TYPE_DBCS)
    {
      if ((gSystemCodePage != 1386) &&
          (gSystemCodePage != 1381) &&
          (gSystemCodePage != 950)) {
        int j=0;
        while (gBadDBCSFontMapping[j].mName) {
           if (fontptr.Equals(NS_ConvertASCIItoUCS2(gBadDBCSFontMapping[j].mName),
                              nsCaseInsensitiveStringComparator()))
           {
              CopyASCIItoUCS2(nsDependentCString(gBadDBCSFontMapping[j].mWinName),
                              fontptr);
              break;
           }
           j++;
        }
      }
    }

    // Create entry for family name, or retrieve already created entry
    GlobalFontEntry* globalEntry = gGlobalFonts->PutEntry(fontptr);
    if (!globalEntry)
      return NS_ERROR_OUT_OF_MEMORY;

    // Init the nsMiniMetrics structure...
    nsMiniMetrics* metrics = new nsMiniMetrics;
    strcpy(metrics->szFacename, font->szFacename);
    metrics->fsType = font->fsType;
    metrics->fsDefn = font->fsDefn;
    metrics->fsSelection = font->fsSelection;
     // Set the FM_SEL_BOLD flag in fsSelection.  This makes the check for
     // bold and italic much easier in LoadFont
    if (font->usWeightClass > 5)
      metrics->fsSelection |= FM_SEL_BOLD;

    // The 'Lucida' set of fonts does not set the bold flag correctly on OS/2,
    // so we'll just set it properly.
    if (strncmp(font->szFamilyname, "Lucida", 6) == 0 &&
        PL_strcasestr(font->szFacename, "bold") != nsnull) {
      metrics->fsSelection |= FM_SEL_BOLD;
    }

    // ... and add it to globalEntry
    metrics->mNext = globalEntry->mMetrics;
    globalEntry->mMetrics = metrics;
    globalEntry->mCodePage = font->usCodePage;
  }

  ::WinReleasePS(ps);

#ifdef DEBUG_pedemont
  gGlobalFonts->EnumerateEntries(DebugOutputEnumerator, nsnull);
  fflush(stdout);
#endif
  nsMemory::Free(pFontMetrics);

  return NS_OK;
}

nsFontOS2*
nsFontMetricsOS2::FindGlobalFont( HPS aPS )
{
  nsFontOS2* fh = nsnull;
  nsAutoString fontname;
  if (!IsDBCS())
    fontname = NS_LITERAL_STRING("Helv");
  else
    fontname = NS_LITERAL_STRING("Helv Combined");
  fh = LoadFont(aPS, fontname);
  NS_ASSERTION(fh, "Couldn't load default font - BAD things are happening");
  return fh;
}

nsFontOS2*
nsFontMetricsOS2::FindUserDefinedFont(HPS aPS)
{
  if (mIsUserDefined) {
    // the user-defined font is always loaded as the first font
    nsFontOS2* font = LoadFont(aPS, mUserDefined);
    mIsUserDefined = PR_FALSE;
    if (font) {
      return font;
    }
  }
  return nsnull;
}

static nsFontFamilyName gFamilyNameTable[] =
{
#ifdef MOZ_MATHML
  { "-moz-math-text",   "Times New Roman" },
  { "-moz-math-symbol", "Symbol" },
#endif
  { "times",           "Times New Roman" },
  { "times roman",     "Times New Roman" },

  { nsnull, nsnull }
};

static nsFontFamilyName gFamilyNameTableDBCS[] =
{
#ifdef MOZ_MATHML
  { "-moz-math-text",   "Times New Roman" },
  { "-moz-math-symbol", "Symbol" },
#endif
  { "times",           "Times New Roman" },
  { "times roman",     "Times New Roman" },
  { "warpsans",        "WarpSans Combined" },

  { nsnull, nsnull }
};

/*-----------------------
** Hash table allocation
**----------------------*/
static PLHashNumber PR_CALLBACK
HashKey(const void* aString)
{
  const nsString* str = (const nsString*)aString;
  return (PLHashNumber) nsCRT::HashCode(str->get());
}

static PRIntn PR_CALLBACK
CompareKeys(const void* aStr1, const void* aStr2)
{
  return nsCRT::strcmp(((const nsString*) aStr1)->get(),
    ((const nsString*) aStr2)->get()) == 0;
}

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
nsFontMetricsOS2::InitializeFamilyNames(void)
{
  if (!gFamilyNames) {
    gFamilyNames = PL_NewHashTable( 0, HashKey, CompareKeys, nsnull,
                                    &familyname_HashAllocOps, nsnull );
    if (!gFamilyNames) {
      return nsnull;
    }

    nsFontFamilyName* f;
    if (!IsDBCS()) {
      f = gFamilyNameTable;
    } else {
      f = gFamilyNameTableDBCS;
    }
    
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

nsFontOS2*
nsFontMetricsOS2::FindLocalFont( HPS aPS )
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
#ifdef DEBUG_FONT_SELECTION
    printf(" FindLocalFont(): attempting to load %s\n",
           NS_LossyConvertUCS2toASCII(*winName).get());
#endif
    nsFontOS2* font = LoadFont(aPS, *winName);
    if (font) {
      return font;
    }
  }
  return nsnull;
}

nsFontOS2*
nsFontMetricsOS2::LoadGenericFont(HPS aPS, const nsString& aName)
{
  for (int i = mLoadedFonts.Count()-1; i >= 0; --i) {
    // woah, this seems bad
    const nsACString& fontName =
      nsDependentCString(((nsFontOS2*)mLoadedFonts[i])->mFattrs.szFacename);
    if (aName.Equals(NS_ConvertASCIItoUCS2(fontName),
                     nsCaseInsensitiveStringComparator()))
      return nsnull;

  }
#ifdef DEBUG_FONT_SELECTION
  printf(" LoadGenericFont(): attempting to load %s\n",
         NS_LossyConvertUCS2toASCII(aName).get());
#endif
  nsFontOS2* font = LoadFont(aPS, aName);
  if (font) {
    return font;
  }
  return nsnull;
}

struct GenericFontEnumContext
{
  HPS               mPS;
  nsFontOS2        *mFont;
  nsFontMetricsOS2 *mMetrics;
};

static PRBool
GenericFontEnumCallback(const nsString& aFamily, PRBool aGeneric, void* aData)
{
  GenericFontEnumContext* context = (GenericFontEnumContext*)aData;
  HPS ps = context->mPS;
  nsFontMetricsOS2* metrics = context->mMetrics;
  context->mFont = metrics->LoadGenericFont(ps, aFamily);
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
    if(!aFontname.IsEmpty())
      aFontname.Append((PRUnichar)',');
    aFontname.Append(value);
  }

  // font.name-list.[generic].[langGroup]
  // the pre-built list of default fonts, it gives alternative fonts
  MAKE_FONT_PREF_KEY(pref, "font.name-list.", generic_dot_langGroup);
  res = gPref->CopyUnicharPref(pref.get(), getter_Copies(value));      
  if (NS_SUCCEEDED(res)) {
    if(!aFontname.IsEmpty())
      aFontname.Append((PRUnichar)',');
    aFontname.Append(value);
  }
}

nsFontOS2*
nsFontMetricsOS2::FindGenericFont(HPS aPS)
{
  if (mTriedAllGenerics) {
    // don't bother anymore because mLoadedFonts[] already has all our generic fonts
    return nsnull;
  }

  // This is a nifty hook that we will use to just iterate over
  // the list of names using the callback mechanism of nsFont...
  nsFont font("", 0, 0, 0, 0, 0);

  if (mLangGroup) {
    const char* langGroup;
    mLangGroup->GetUTF8String(&langGroup);
  
    // x-unicode pseudo-langGroup should be the last resort to turn to.
    // That is, it should be refered to only when we don't  recognize 
    // |langGroup| specified by the authors of documents and  the 
    // determination of |langGroup| based  on Unicode range also fails 
    // in |FindPrefFont|. 

    if (!strcmp(langGroup, "x-unicode")) {
      mTriedAllGenerics = 1;
      return nsnull;
    }

    AppendGenericFontFromPref(font.name, langGroup, 
                              NS_ConvertUCS2toUTF8(mGeneric).get());
  }

  // Iterate over the list of names using the callback mechanism of nsFont...
  GenericFontEnumContext context = {aPS, nsnull, this};
  font.EnumerateFamilies(GenericFontEnumCallback, &context);
  if (context.mFont) { // a suitable font was found
    return context.mFont;
  }

  mTriedAllGenerics = 1;
  return nsnull;
}

nsFontOS2*
nsFontMetricsOS2::FindPrefFont(HPS aPS)
{
  if (mTriedAllPref) {
    // don't bother anymore because mLoadedFonts[] already has all our pref fonts
    return nsnull;
  }
  nsFont font("", 0, 0, 0, 0, 0);
  // Try the pref of the user's ui lang group
  // For example, if the ui language is Japanese, try pref from "ja"
  // Make localized build work better on other OS
  if (gUsersLocale != mLangGroup) {
    nsAutoString langGroup;
    gUsersLocale->ToString(langGroup);
    AppendGenericFontFromPref(font.name, 
                              NS_ConvertUCS2toUTF8(langGroup).get(), 
                              NS_ConvertUCS2toUTF8(mGeneric).get());
  }
  // Try the pref of the user's system lang group
  // For example, if the os language is Simplified Chinese, 
  // try pref from "zh-CN"
  // Make English build work better on other OS
  if ((gSystemLocale != mLangGroup) && (gSystemLocale != gUsersLocale)) {
    nsAutoString langGroup;
    gSystemLocale->ToString(langGroup);
    AppendGenericFontFromPref(font.name, 
                              NS_ConvertUCS2toUTF8(langGroup).get(), 
                              NS_ConvertUCS2toUTF8(mGeneric).get());
  }

  // Also try all the default pref fonts enlisted from other languages
  for (int i = 1; i < eCharset_COUNT; ++i) {
    nsIAtom* langGroup = NS_NewAtom(gCharsetInfo[i].mLangGroup); 
    if((gUsersLocale != langGroup) && (gSystemLocale != langGroup)) {
      AppendGenericFontFromPref(font.name, gCharsetInfo[i].mLangGroup, 
                                NS_ConvertUCS2toUTF8(mGeneric).get());
    }
    NS_IF_RELEASE(langGroup);
  }
  GenericFontEnumContext context = {aPS, nsnull, this};
  font.EnumerateFamilies(GenericFontEnumCallback, &context);
  if (context.mFont) { // a suitable font was found
    return context.mFont;
  }
  mTriedAllPref = 1;
  return nsnull;
}

// returns family name of font that can display given char
nsFontOS2*
nsFontMetricsOS2::FindFont( HPS aPS )
{
  nsFontOS2* font = FindUserDefinedFont(aPS);
  if (!font) {
    font = FindLocalFont(aPS);
    if (!font) {
      font = FindGenericFont(aPS);
      if (!font) {
        font = FindPrefFont(aPS);
        if (!font) {
          font = FindGlobalFont(aPS);
        }
      }
    }
  }
#ifdef DEBUG_FONT_SELECTION
  if (font) {
    printf(" FindFont(): found font %s\n",
           font->mFattrs.szFacename);
  }
#endif
  return font;
}

static PRBool
FontEnumCallback(const nsString& aFamily, PRBool aGeneric, void *aData)
{
  nsFontMetricsOS2* metrics = (nsFontMetricsOS2*) aData;

  /* Hack for Truetype on OS/2 - if it's Arial and not 1252 or 0, just get another font */
  if (aFamily.Find("Arial", IGNORE_CASE) != -1) {
     if (metrics->mConvertCodePage != 1252) {
        if ((metrics->mConvertCodePage == 0) &&
            (gSystemCodePage != 850) &&
            (gSystemCodePage != 437)) {
           return PR_TRUE; // don't stop
        }
     }
  }

  metrics->mFonts.AppendString(aFamily);
  if (aGeneric) {
    metrics->mGeneric.Assign(aFamily);
    ToLowerCase(metrics->mGeneric);
    return PR_FALSE; // stop
  }
  ++metrics->mGenericIndex;

  return PR_TRUE; // don't stop
}

PRBool
nsFontMetricsOS2::GetVectorSubstitute(HPS aPS, const nsString& aFamilyname,
                                      char* alias)
{
  char* fontname = ToNewCString(aFamilyname);
  return GetVectorSubstitute(aPS, fontname, alias);
}

PRBool
nsFontMetricsOS2::GetVectorSubstitute(HPS aPS, const char* aFamilyname,
                                      char* alias)
{
   // set style flags
  PRBool isBold = mFont.weight > NS_FONT_WEIGHT_NORMAL;
  PRBool isItalic = (mFont.style & (NS_FONT_STYLE_ITALIC | NS_FONT_STYLE_OBLIQUE));

  if( stricmp( aFamilyname, "Tms Rmn" ) == 0 )
  {
    if( !isBold && !isItalic )
      strcpy( alias, "Times New Roman" );
    else if( isBold )
      if( !isItalic )
        strcpy( alias, "Times New Roman Bold" );
      else
        strcpy( alias, "Times New Roman Bold Italic" );
    else
      strcpy( alias, "Times New Roman Italic" );

    return PR_TRUE;
  }

  if( stricmp( aFamilyname, "Helv" ) == 0)
  {
    if( !isBold && !isItalic )
      strcpy( alias, "Helvetica" );
    else if( isBold )
      if( !isItalic )
        strcpy( alias, "Helvetica Bold" );
      else
        strcpy( alias, "Helvetica Bold Italic" );
    else
      strcpy( alias, "Helvetica Italic" );

    return PR_TRUE;
  }

   // When printing, substitute vector fonts for these common bitmap fonts
  if( !mDeviceContext->SupportsRasterFonts() )
  {
    if( stricmp( aFamilyname, "System Proportional" ) == 0 ||
        stricmp( aFamilyname, "WarpSans" ) == 0 )
    {
      if( !isBold && !isItalic )
        strcpy( alias, "Helvetica" );
      else if( isBold )
        if( !isItalic )
          strcpy( alias, "Helvetica Bold" );
        else
          strcpy( alias, "Helvetica Bold Italic" );
      else
        strcpy( alias, "Helvetica Italic" );

      return PR_TRUE;
    }

    if( stricmp( aFamilyname, "System Monospaced" ) == 0 ||
        stricmp( aFamilyname, "System VIO" ) == 0 )
    {
      if( !isBold && !isItalic )
        strcpy( alias, "Courier" );
      else if( isBold )
        if( !isItalic )
          strcpy( alias, "Courier Bold" );
        else
          strcpy( alias, "Courier Bold Italic" );
      else
        strcpy( alias, "Courier Italic" );

      return PR_TRUE;
    }
  }
  
  return PR_FALSE;
}

nsresult
nsFontMetricsOS2::RealizeFont()
{
  nsresult  rv;
  HPS       ps = NULL;

  if (mDeviceContext->mPrintDC){
    ps = mDeviceContext->mPrintPS;
  } else {
    HWND win = (HWND)mDeviceContext->mWidget;
    ps = ::WinGetPS(win);
    if (!ps) {
      ps = ::WinGetPS(HWND_DESKTOP);
    }
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

  nsFontOS2* font = FindFont(ps);
  NS_ASSERTION(font, "FindFont() returned null.  THIS IS BAD!");
  if (!font) {
    if (mDeviceContext->mPrintDC == NULL) {
      ::WinReleasePS(ps);
    }
    return NS_ERROR_FAILURE;
  }

  font->mConvertCodePage = mConvertCodePage;

   // Record font handle & record various font metrics to cache
  mFontHandle = font;
  GFX (::GpiCreateLogFont(ps, 0, 1, &(font->mFattrs)), GPI_ERROR);
  font->SelectIntoPS( ps, 1 );

  FONTMETRICS fm;
  GFX (::GpiQueryFontMetrics (ps, sizeof (fm), &fm), FALSE);
  /* Due to a bug in OS/2 MINCHO, need to cast lInternalLeading */
  fm.lInternalLeading = (signed short)fm.lInternalLeading;

  float dev2app;
  dev2app = mDeviceContext->DevUnitsToAppUnits();
  
  mMaxAscent  = NSToCoordRound( (fm.lMaxAscender-1) * dev2app );
  mMaxDescent = NSToCoordRound( (fm.lMaxDescender+1) * dev2app );
  mFontHandle->mMaxAscent = mMaxAscent;
  mFontHandle->mMaxDescent = mMaxDescent;

  mInternalLeading = NSToCoordRound( fm.lInternalLeading * dev2app );
  mExternalLeading = NSToCoordRound( fm.lExternalLeading * dev2app );
  
  /* These two values aren't really used by mozilla */
  mEmAscent = mMaxAscent - mInternalLeading;
  mEmDescent  = mMaxDescent;

  mMaxHeight  = mMaxAscent + mMaxDescent;
  mEmHeight = mEmAscent + mEmDescent;

  mMaxAdvance = NSToCoordRound( fm.lMaxCharInc * dev2app );
  mXHeight    = NSToCoordRound( fm.lXHeight * dev2app );

  nscoord onePixel = NSToCoordRound(1 * dev2app);

   // Not all fonts specify these two values correctly, and some not at all
  mSuperscriptYOffset = mXHeight;
  mSubscriptYOffset   = NSToCoordRound( mXHeight / 3.0f );

   // Using lStrikeoutPosition puts the strikeout too high
   // Use 50% of lXHeight instead
  mStrikeoutPosition  = NSToCoordRound( mXHeight / 2.0f);
  mStrikeoutSize      = PR_MAX(onePixel, NSToCoordRound(fm.lStrikeoutSize * dev2app));

#if 1
  // Can't trust the fonts to give us good results for the underline position
  //  and size.  This calculation, though, gives very good results.
  float height = fm.lMaxAscender + fm.lMaxDescender;
  mUnderlinePosition = -PR_MAX(onePixel, NSToIntRound(floor(0.1 * height + 0.5) * dev2app));
  mUnderlineSize = PR_MAX(onePixel, NSToIntRound(floor(0.05 * height + 0.5) * dev2app));
#else
  mUnderlinePosition  = -NSToCoordRound( fm.lUnderscorePosition * dev2app );
  mUnderlineSize      = PR_MAX(onePixel, NSToCoordRound(fm.lUnderscoreSize * dev2app));
#endif

  mAveCharWidth       = PR_MAX(1, NSToCoordRound(fm.lAveCharWidth * dev2app));

   // Cache the width of a single space.
  SIZEL  size;
  GetTextExtentPoint32(ps, " ", 1, &size);
  mSpaceWidth = NSToCoordRound(float(size.cx) * dev2app);

   // 10) Clean up
  GFX (::GpiSetCharSet (ps, LCID_DEFAULT), FALSE);
  GFX (::GpiDeleteSetId (ps, 1), FALSE);
  if (mDeviceContext->mPrintDC == NULL)
    ::WinReleasePS(ps);

  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsOS2::GetSpaceWidth(nscoord &aSpaceWidth)
{
  aSpaceWidth = mSpaceWidth;
  return NS_OK;
}

// Other metrics
NS_IMETHODIMP nsFontMetricsOS2::GetXHeight( nscoord &aResult)
{
  aResult = mXHeight;
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsOS2::GetSuperscriptOffset(nscoord& aResult)
{
  aResult = mSuperscriptYOffset;
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsOS2::GetSubscriptOffset(nscoord& aResult)
{
  aResult = mSubscriptYOffset;
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsOS2::GetStrikeout(nscoord& aOffset, nscoord& aSize)
{
  aOffset = mStrikeoutPosition;
  aSize = mStrikeoutSize;
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsOS2::GetUnderline(nscoord& aOffset, nscoord& aSize)
{
  aOffset = mUnderlinePosition;
  aSize = mUnderlineSize;
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsOS2::GetHeight( nscoord &aHeight)
{
  aHeight = mMaxHeight;
  return NS_OK;
}

#ifdef FONT_LEADING_APIS_V2
NS_IMETHODIMP
nsFontMetricsOS2::GetInternalLeading(nscoord &aLeading)
{
  aLeading = mInternalLeading;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsOS2::GetExternalLeading(nscoord &aLeading)
{
  aLeading = mExternalLeading;
  return NS_OK;
}
#else
NS_IMETHODIMP nsFontMetricsOS2::GetLeading( nscoord &aLeading)
{
   aLeading = mInternalLeading;
   return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsOS2::GetNormalLineHeight(nscoord &aHeight)
{
  aHeight = mEmHeight + mInternalLeading;
  return NS_OK;
}
#endif //FONT_LEADING_APIS_V2

NS_IMETHODIMP nsFontMetricsOS2::GetMaxAscent( nscoord &aAscent)
{
  aAscent = mMaxAscent;
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsOS2::GetMaxDescent( nscoord &aDescent)
{
  aDescent = mMaxDescent;
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsOS2::GetMaxAdvance( nscoord &aAdvance)
{
  aAdvance = mMaxAdvance;
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsOS2::GetFont( const nsFont *&aFont)
{
  aFont = &mFont;
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsOS2::GetFontHandle( nsFontHandle &aHandle)
{
  aHandle = mFontHandle;
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsOS2::GetLangGroup(nsIAtom** aLangGroup)
{
  NS_ENSURE_ARG_POINTER(aLangGroup);
  *aLangGroup = mLangGroup;
  NS_IF_ADDREF(*aLangGroup);
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsOS2::GetEmHeight(nscoord &aHeight)
{
  aHeight = mEmHeight;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsOS2::GetEmAscent(nscoord &aAscent)
{
  aAscent = mEmAscent;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsOS2::GetEmDescent(nscoord &aDescent)
{
  aDescent = mEmDescent;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsOS2::GetMaxHeight(nscoord &aHeight)
{
  aHeight = mMaxHeight;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsOS2::GetAveCharWidth(nscoord &aAveCharWidth)
{
  aAveCharWidth = mAveCharWidth;
  return NS_OK;
}

nsFontOS2*
nsFontMetricsOS2::LoadUnicodeFont(HPS aPS, const nsString& aName)
{
#ifdef DEBUG_FONT_SELECTION
  printf(" LoadUnicodeFont(): attempting to load %s\n",
         NS_LossyConvertUCS2toASCII(aName).get());
#endif
  nsFontOS2* font = LoadFont(aPS, aName);
  if (font) {
    return font;
  }
  return nsnull;
}

struct UnicodeFontEnumContext
{
  HPS               mPS;
  nsFontOS2*        mFont;
  nsFontMetricsOS2* mMetrics;
};

static PRBool
UnicodeFontEnumCallback(const nsString& aFamily, PRBool aGeneric, void* aData)
{
  UnicodeFontEnumContext* context = (UnicodeFontEnumContext*)aData;
  HPS ps = context->mPS;
  nsFontMetricsOS2* metrics = context->mMetrics;
  context->mFont = metrics->LoadUnicodeFont(ps, aFamily);
  if (context->mFont) {
    return PR_FALSE; // stop enumerating the list
  }
  return PR_TRUE; // don't stop
}

void
nsFontMetricsOS2::FindUnicodeFont(HPS aPS)
{
  nsresult res;
  nsCAutoString pref, generic;
  nsXPIDLString value;

  generic.Assign(NS_ConvertUCS2toUTF8(mGeneric));

  pref.Assign("font.name.");
  pref.Append(generic);
  pref.Append(".x-unicode");
   
  res = gPref->CopyUnicharPref(pref.get(), getter_Copies(value));
  if (NS_FAILED(res))
    return;

  nsAutoString fontname;
  fontname.Assign(value);

  nsFontOS2* fh = nsnull;
  fh = LoadUnicodeFont(aPS, fontname);
  if (!fh)
  {
     // User defined unicode font did not load.  Fall back to font
     // specified in font.name-list.%.x-unicode
    pref.Assign("font.name-list.");
    pref.Append(generic);
    pref.Append(".x-unicode");

    res = gPref->CopyUnicharPref(pref.get(), getter_Copies(value));
    if (NS_FAILED(res))
      return;

    nsFont font("", 0, 0, 0, 0, 0);
    font.name.Assign(value);

     // Iterate over the list of names using the callback mechanism of nsFont
    UnicodeFontEnumContext context = {aPS, nsnull, this};
    font.EnumerateFamilies(UnicodeFontEnumCallback, &context);
    fh = context.mFont;
  }

  if (fh) {   // a suitable font was found
    fh->mFattrs.usCodePage = 1208;
    fh->mConvertCodePage = 1208;
    mUnicodeFont = fh;
#ifdef DEBUG_FONT_SELECTION
    printf(" FindUnicodeFont(): found new Unicode font %s\n",
           mUnicodeFont->mFattrs.szFacename);
#endif
  } else {
    NS_ERROR("Could not find a Unicode font");
  }
  return;
}

void
nsFontMetricsOS2::FindWesternFont()
{
  // Create a 'western' font by making a copy of the currently selected font
  // and changing the codepage 1252
  nsFontOS2* font = new nsFontOS2();
  *font = *mFontHandle;
  font->mFattrs.usCodePage = 1252;
  font->mConvertCodePage = 1252;
  mLoadedFonts.AppendElement(font);
  mWesternFont = font;
}

#define IS_SPECIAL(x) ((x == 0x20AC) ||  /* euro */ \
                       (x == 0x2022) ||  /* bull */ \
                       (x == 0x201C) ||  /* ldquo */ \
                       (x == 0x201D) ||  /* rdquo */ \
                       (x == 0x2018) ||  /* lsquo */ \
                       (x == 0x2019) ||  /* rsquo */ \
                       (x == 0x2026) ||  /* hellip */ \
                       (x == 0x2013) ||  /* ndash */ \
                       (x == 0x2014))    /* mdash */

nsresult
nsFontMetricsOS2::ResolveForwards(HPS                  aPS,
                                  const PRUnichar*     aString,
                                  PRUint32             aLength,
                                  nsFontSwitchCallback aFunc, 
                                  void*                aData)
{
  NS_ASSERTION(aString || !aLength, "invalid call");
  const PRUnichar* firstChar = aString;
  const PRUnichar* currChar = firstChar;
  const PRUnichar* lastChar  = aString + aLength;
  PRBool running = PR_TRUE;

  nsFontSwitch fontSwitch;
  fontSwitch.mFont = 0;

  if (mConvertCodePage == 1252)
  {
    while (running && firstChar < lastChar)
    {
      if ((*currChar > 0x00FF) && !IS_SPECIAL(*currChar))
      {
        if (!mUnicodeFont) {
          FindUnicodeFont(aPS);
          if (!mUnicodeFont) {
            mUnicodeFont = FindGlobalFont(aPS);
          }
        }
        fontSwitch.mFont = mUnicodeFont;
        while( ++currChar < lastChar ) {
          if (( *currChar <= 0x00FF ) || IS_SPECIAL(*currChar))
            break;
        }
        running = (*aFunc)(&fontSwitch, firstChar, currChar - firstChar, aData);
      }
      else
      {
         // Use currently selected font
        fontSwitch.mFont = mFontHandle;
        while( ++currChar < lastChar ) {
          if (( *currChar > 0x00FF ) && !IS_SPECIAL(*currChar))
            break;
        }
        running = (*aFunc)(&fontSwitch, firstChar, currChar - firstChar, aData);
      }
      firstChar = currChar;
    }
  }
  else
  {
    while (running && firstChar < lastChar)
    {
      if ((*currChar >= 0x0080 && *currChar <= 0x00FF) || IS_SPECIAL(*currChar))
      { 
        if (!mWesternFont) {
          FindWesternFont();
        }
        fontSwitch.mFont = mWesternFont;
        while( ++currChar < lastChar ) {
          if ((*currChar < 0x0080 || *currChar > 0x00FF) && !IS_SPECIAL(*currChar))
            break;
        }
        running = (*aFunc)(&fontSwitch, firstChar, currChar - firstChar, aData);
      }
      else
      {
         // Use currently selected font
        fontSwitch.mFont = mFontHandle;
        while( ++currChar < lastChar ) {
          if ((*currChar >= 0x0080 && *currChar <= 0x00FF) || IS_SPECIAL(*currChar))
            break;
        }
        running = (*aFunc)(&fontSwitch, firstChar, currChar - firstChar, aData);
      }
      firstChar = currChar;
    }
  }
  return NS_OK;
}

nsresult
nsFontMetricsOS2::ResolveBackwards(HPS                  aPS,
                                   const PRUnichar*     aString,
                                   PRUint32             aLength,
                                   nsFontSwitchCallback aFunc, 
                                   void*                aData)
{
  NS_ASSERTION(aString || !aLength, "invalid call");
  const PRUnichar* firstChar = aString + aLength - 1;
  const PRUnichar* lastChar  = aString - 1;
  const PRUnichar* currChar  = firstChar;
  PRBool running = PR_TRUE;

  nsFontSwitch fontSwitch;
  fontSwitch.mFont = 0;
  
  if (mConvertCodePage == 1252)
  {
    while (running && firstChar > lastChar)
    {
      if ((*currChar > 0x00FF) && !IS_SPECIAL(*currChar))
      {
        if (!mUnicodeFont) {
          FindUnicodeFont(aPS);
          if (!mUnicodeFont) {
            mUnicodeFont = FindGlobalFont(aPS);
          }
        }
        fontSwitch.mFont = mUnicodeFont;
        while( --currChar > lastChar ) {
          if (( *currChar <= 0x00FF ) || IS_SPECIAL(*currChar))
            break;
        }
        running = (*aFunc)(&fontSwitch, firstChar, currChar - firstChar, aData);
      }
      else
      {
         // Use currently selected font
        fontSwitch.mFont = mFontHandle;
        while( --currChar > lastChar ) {
          if (( *currChar > 0x00FF ) && !IS_SPECIAL(*currChar))
            break;
        }
        running = (*aFunc)(&fontSwitch, firstChar, currChar - firstChar, aData);
      }
      firstChar = currChar;
    }
  }
  else
  {
    while (running && firstChar > lastChar)
    {
      if ((*currChar >= 0x0080 && *currChar <= 0x00FF) || IS_SPECIAL(*currChar))
      { 
        if (!mWesternFont) {
          FindWesternFont();
        }
        fontSwitch.mFont = mWesternFont;
        while( --currChar > lastChar ) {
          if ((*currChar < 0x0080 || *currChar > 0x00FF) && !IS_SPECIAL(*currChar))
            break;
        }
        running = (*aFunc)(&fontSwitch, firstChar, currChar - firstChar, aData);
      }
      else
      {
         // Use currently selected font
        fontSwitch.mFont = mFontHandle;
        while( --currChar > lastChar ) {
          if ((*currChar >= 0x0080 && *currChar <= 0x00FF) || IS_SPECIAL(*currChar))
            break;
        }
        running = (*aFunc)(&fontSwitch, firstChar, currChar - firstChar, aData);
      }
      firstChar = currChar;
    }
  }
  return NS_OK;
}


/**********************************************************
    nsFontOS2
 **********************************************************/    
#ifdef DEBUG_FONT_STRUCT_ALLOCS
unsigned long nsFontOS2::mRefCount = 0;
#endif

nsFontOS2::nsFontOS2(void)
{
  mFattrs.usRecordLength = sizeof(mFattrs);
  mHashMe = gCurrHashValue;
  gCurrHashValue++;
#ifdef DEBUG_FONT_STRUCT_ALLOCS
  mRefCount++;
  printf("+++ nsFontOS2 total = %d\n", mRefCount);
#endif
}

nsFontOS2::~nsFontOS2(void)
{
#ifdef DEBUG_FONT_STRUCT_ALLOCS
  mRefCount--;
  printf("--- nsFontOS2 total = %d\n", mRefCount);
#endif
}

void
nsFontOS2::SelectIntoPS( HPS aPS, long aLcid )
{
   GFX (::GpiSetCharSet(aPS, aLcid), FALSE);
   GFX (::GpiSetCharBox(aPS, &mCharbox), FALSE);
}

PRInt32
nsFontOS2::GetWidth(HPS aPS, const char* aString, PRUint32 aLength)
{
  SIZEL size;
  GetTextExtentPoint32(aPS, aString, aLength, &size);
  return size.cx;
}

PRInt32
nsFontOS2::GetWidth( HPS aPS, const PRUnichar* aString, PRUint32 aLength )
{
  char  str[CHAR_BUFFER_SIZE];
  char* pstr = str;
  int   destLength = aLength * 3 + 1;

  if (destLength > CHAR_BUFFER_SIZE)
    pstr = new char[destLength];
  else
    destLength = CHAR_BUFFER_SIZE;

  int convertedLength = WideCharToMultiByte( mConvertCodePage, aString,
                                             aLength, pstr, destLength );

  SIZEL size;
  GetTextExtentPoint32(aPS, pstr, convertedLength, &size);

  if (pstr != str) {
    delete[] pstr;
  }

  return size.cx;
}

void
nsFontOS2::DrawString(HPS aPS, nsDrawingSurfaceOS2* aSurface,
                      PRInt32 aX, PRInt32 aY,
                      const char* aString, PRUint32 aLength, INT* aDx0)
{
  POINTL ptl = { aX, aY };
  aSurface->NS2PM(&ptl, 1);

  ExtTextOut(aPS, ptl.x, ptl.y, 0, NULL, aString, aLength, aDx0);
}

void
nsFontOS2::DrawString( HPS aPS, nsDrawingSurfaceOS2* aSurface,
                       PRInt32 aX, PRInt32 aY,
                       const PRUnichar* aString, PRUint32 aLength )
{
  char  str[CHAR_BUFFER_SIZE];
  char* pstr = str;
  int   destLength = aLength * 3 + 1;
  
  if (destLength > CHAR_BUFFER_SIZE)
    pstr = new char[destLength];
  else
    destLength = CHAR_BUFFER_SIZE;

  int convertedLength = WideCharToMultiByte( mConvertCodePage, aString,
                                             aLength, pstr, destLength );

  POINTL ptl = { aX, aY };
  aSurface->NS2PM (&ptl, 1);

  ExtTextOut(aPS, ptl.x, ptl.y, 0, NULL, pstr, convertedLength, NULL);

  if (pstr != str) {
    delete[] pstr;
  }
}


// The Font Enumerator

nsFontEnumeratorOS2::nsFontEnumeratorOS2()
{
}

NS_IMPL_ISUPPORTS1(nsFontEnumeratorOS2, nsIFontEnumerator)

// We want vertical fonts (those whose name start with '@') to appear
// immediately following the non-vertical font of the same name.
static int
CompareFontNames(const void* aArg1, const void* aArg2, void* aClosure)
{
  const PRUnichar* str1 = *((const PRUnichar**) aArg1);
  const PRUnichar* str2 = *((const PRUnichar**) aArg2);

  int rc = 9;
  if ((char)str1[0] == '@') {
    str1++;
    rc = nsCRT::strcmp(str1, str2);
    if (rc == 0)
      return 1;
  }
  if ((char)str2[0] == '@') {
    str2++;
    rc = nsCRT::strcmp(str1, str2);
    if (rc == 0)
      return -1;
  }

  if (rc == 9)
    rc = nsCRT::strcmp(str1, str2);
  return rc;
}

NS_IMETHODIMP
nsFontEnumeratorOS2::EnumerateFonts(const char* aLangGroup,
  const char* aGeneric, PRUint32* aCount, PRUnichar*** aResult)
{
  return EnumerateAllFonts(aCount, aResult);
}

struct EnumerateAllFontsData
{
  PRUnichar** array;
  int count;
};

PR_STATIC_CALLBACK(PLDHashOperator)
EnumerateAllFontsCallback(GlobalFontEntry* aEntry, void* aData)
{
  EnumerateAllFontsData* data = NS_STATIC_CAST(EnumerateAllFontsData*, aData);

  data->array[data->count++] = ToNewUnicode(aEntry->GetKey());

  return PL_DHASH_NEXT;
}

NS_IMETHODIMP
nsFontEnumeratorOS2::EnumerateAllFonts(PRUint32* aCount, PRUnichar*** aResult)
{
  NS_ENSURE_ARG_POINTER(aCount);
  NS_ENSURE_ARG_POINTER(aResult);

  if (!nsFontMetricsOS2::gGlobalFonts) {
    nsresult res = nsFontMetricsOS2::InitializeGlobalFonts();
    if (NS_FAILED(res)) {
      FreeGlobals();
      return res;
    }
  }

  *aCount = nsFontMetricsOS2::gGlobalFonts->Count();
  PRUnichar** array = (PRUnichar**)nsMemory::Alloc(*aCount * sizeof(PRUnichar*));
  NS_ENSURE_TRUE(array, NS_ERROR_OUT_OF_MEMORY);

  EnumerateAllFontsData data;
  data.array = array;
  data.count = 0;
  nsFontMetricsOS2::gGlobalFonts->EnumerateEntries(EnumerateAllFontsCallback,
                                                  &data);

  NS_QuickSort(array, *aCount, sizeof(PRUnichar*), CompareFontNames, nsnull);

  *aResult = array;

  return NS_OK;
}

NS_IMETHODIMP
nsFontEnumeratorOS2::HaveFontFor(const char* aLangGroup, PRBool* aResult)
{
  NS_ENSURE_ARG_POINTER(aLangGroup);
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = PR_FALSE;

  // XXX stub
  NS_ASSERTION( 0, "HaveFontFor is not implemented" );

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFontEnumeratorOS2::GetDefaultFont(const char *aLangGroup, 
  const char *aGeneric, PRUnichar **aResult)
{
  // aLangGroup=null or ""  means any (i.e., don't care)
  // aGeneric=null or ""  means any (i.e, don't care)

  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsFontEnumeratorOS2::UpdateFontList(PRBool *updateFontList)
{
  *updateFontList = PR_FALSE; // always return false for now
  NS_ASSERTION( 0, "UpdateFontList is not implemented" );
  return NS_ERROR_NOT_IMPLEMENTED;
}

