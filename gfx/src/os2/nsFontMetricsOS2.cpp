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
#include "nsICharsetConverterManager2.h"
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

#ifdef MOZ_MATHML
  #include <math.h>
#endif

#undef USER_DEFINED
#define USER_DEFINED "x-user-def"

static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);
static NS_DEFINE_IID(kIFontMetricsIID, NS_IFONT_METRICS_IID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

#define NS_MAX_FONT_WEIGHT 900
#define NS_MIN_FONT_WEIGHT 100

#undef CHAR_BUFFER_SIZE
#define CHAR_BUFFER_SIZE 1024


/***** Global variables *****/
nsVoidArray  *nsFontMetricsOS2::gGlobalFonts = nsnull;
PRBool        nsFontMetricsOS2::gSubstituteVectorFonts = PR_TRUE;
PLHashTable  *nsFontMetricsOS2::gFamilyNames = nsnull;
nscoord       nsFontMetricsOS2::gDPI = 0;
long          nsFontMetricsOS2::gSystemRes = 0;

#ifdef WINCODE
static nsICharsetConverterManager2 *gCharsetManager = nsnull;
#endif
static nsIPref           *gPref = nsnull;
static nsIAtom           *gUsersLocale = nsnull;
static nsIAtom           *gSystemLocale = nsnull;
static nsIAtom           *gUserDefined = nsnull;
#ifdef WINCODE
static nsIUnicodeEncoder *gUserDefinedConverter = nsnull;
static PRUint32           gUserDefinedMap[2048];
#endif
static int                gInitialized = 0;
static ULONG              ulSystemCodePage = 0;
static ULONG              curHashValue = 1;



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
  USHORT   mMask;
  PRUint16 mCodePage;
  char*    mLangGroup;
  PRUint32* mMap;
};

static nsCharsetInfo gCharsetInfo[eCharset_COUNT] =
{
  { "DEFAULT",     0,                0,    "" },
  { "ANSI",        FM_DEFN_LATIN1,   1252, "x-western" },
  { "EASTEUROPE",  FM_DEFN_LATIN2,   1250, "x-central-euro" },
  { "RUSSIAN",     FM_DEFN_CYRILLIC, 1251, "x-cyrillic" },
  { "GREEK",       FM_DEFN_GREEK,    813,  "el" },
  { "TURKISH",     0,                1254, "tr" },
  { "HEBREW",      FM_DEFN_HEBREW,   862,  "he" },
  { "ARABIC",      FM_DEFN_ARABIC,   864,  "ar" },
  { "BALTIC",      0,                1257, "x-baltic" },
  { "THAI",        FM_DEFN_THAI,     874,  "th" },
  { "SHIFTJIS",    FM_DEFN_KANA,     932,  "ja" },
  { "GB2312",      FM_DEFN_KANA,     1381, "zh-CN" },
  { "HANGEUL",     FM_DEFN_KANA,     949,  "ko" },
  { "CHINESEBIG5", FM_DEFN_KANA,     950,  "zh-TW" },
  { "JOHAB",       FM_DEFN_KANA,     1361, "ko-XXX", }
};


/**********************************************************
    nsFontOS2
 **********************************************************/    
nsFontOS2::nsFontOS2()
{
  memset( &fattrs, 0, sizeof fattrs);
  fattrs.usRecordLength = sizeof fattrs;
  charbox.cx = charbox.cy = 0;
  ulHashMe = curHashValue;
  curHashValue++;
  mMaxAscent = 0;
  mMaxDescent = 0;
}

void
nsFontOS2::SelectIntoPS( HPS aPS, long lcid )
{
   GFX (::GpiSetCharSet (aPS, lcid), FALSE);
   GFX (::GpiSetCharBox (aPS, &charbox), FALSE);
}

static nsresult
GetConverter(const char* aFontName, nsIUnicodeEncoder** aConverter)
{
  // XXX OS2TODO - write me!
#ifdef WINCODE
  *aConverter = nsnull;

  nsAutoString value;
  nsresult rv = GetEncoding(aFontName, value);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIAtom> charset;
  rv = gCharsetManager->GetCharsetAtom(value.get(), getter_AddRefs(charset));
  if (NS_FAILED(rv)) return rv;

  rv = gCharsetManager->GetUnicodeEncoder(charset, aConverter);
  if (NS_FAILED(rv)) return rv;
  return (*aConverter)->SetOutputErrorBehavior((*aConverter)->kOnError_Replace, nsnull, '?');
#endif
}

PRInt32
nsFontOS2::GetWidth( HPS aPS, const PRUnichar* aString,
                     PRUint32 aLength )
{
  char  str[CHAR_BUFFER_SIZE];
  char* pstr = str;
  int   destLength = aLength * 3 + 1;

  if (destLength > CHAR_BUFFER_SIZE) {
    pstr = new char[destLength];
    if (!pstr)
      return 0;
  }
  else
    destLength = CHAR_BUFFER_SIZE;

#ifdef WINCODE
  PRInt32 srcLength = aLength, destLength = aLength;
  mConverter->Convert(aString, &srcLength, pstr, &destLength);
#else
  int convertedLength = WideCharToMultiByte( fattrs.usCodePage, aString,
                                             aLength, pstr, destLength );
#endif

  SIZEL size;
  GetTextExtentPoint32(aPS, pstr, convertedLength, &size);

  if (pstr != str) {
    delete[] pstr;
  }

  return size.cx;
}

void
nsFontOS2::DrawString( HPS aPS, nsDrawingSurfaceOS2* aSurface,
                       PRInt32 aX, PRInt32 aY,
                       const PRUnichar* aString, PRUint32 aLength )
{
  char  str[CHAR_BUFFER_SIZE];
  char* pstr = str;
  int   destLength = aLength * 3 + 1;
  
  if (destLength > CHAR_BUFFER_SIZE) {
    pstr = new char[destLength];
    if (!pstr)
      return;
  }
  else
    destLength = CHAR_BUFFER_SIZE;

#ifdef WINCODE
  PRInt32 srcLength = aLength, destLength = aLength;
  mConverter->Convert(aString, &srcLength, pstr, &destLength);
#else
  int convertedLength = WideCharToMultiByte( fattrs.usCodePage, aString,
                                             aLength, pstr, destLength );
#endif

  POINTL ptl = { aX, aY };
  aSurface->NS2PM (&ptl, 1);

  ExtTextOut( aPS, ptl.x, ptl.y, 0, NULL, pstr, convertedLength, NULL);

  if (pstr != str) {
    delete[] pstr;
  }
}



/**********************************************************
    nsFontMetricsOS2
 **********************************************************/    
int PR_CALLBACK
prefChanged(const char *aPref, void *aClosure)
{
  nsresult rv;

  if( PL_strcmp(aPref, "browser.display.screen_resolution") == 0 )
  {
    PRInt32 dpi;
    rv = gPref->GetIntPref( aPref, &dpi );
    if( NS_SUCCEEDED(rv) && dpi != 0)
      nsFontMetricsOS2::gDPI = dpi;
    else
      nsFontMetricsOS2::gDPI = nsFontMetricsOS2::gSystemRes;
  }

  return 0;
}

static void
FreeGlobals(void)
{
  // XXX complete this

  gInitialized = 0;

  gPref->UnregisterCallback( "browser.display.screen_resolution", prefChanged, NULL );

#ifdef WINCODE
  NS_IF_RELEASE(gCharsetManager);
#endif
  NS_IF_RELEASE(gPref);
  NS_IF_RELEASE(gUsersLocale);
  NS_IF_RELEASE(gSystemLocale);
  NS_IF_RELEASE(gUserDefined);
#ifdef WINCODE
  NS_IF_RELEASE(gUserDefinedConverter);
#endif

  if (nsFontMetricsOS2::gGlobalFonts) {
    for (int i = nsFontMetricsOS2::gGlobalFonts->Count()-1; i >= 0; --i) {
      nsGlobalFont* font = (nsGlobalFont*)nsFontMetricsOS2::gGlobalFonts->ElementAt(i);
      delete font;
    }
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
InitGlobals(void)
{
#ifdef WINCODE
  nsServiceManager::GetService(kCharsetConverterManagerCID,
    NS_GET_IID(nsICharsetConverterManager2), (nsISupports**) &gCharsetManager);
  if (!gCharsetManager) {
    FreeGlobals();
    return NS_ERROR_FAILURE;
  }
#endif
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
    UINT cp = WinQueryCp(HMQ_CURRENT);
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

  ulSystemCodePage = WinQueryCp(HMQ_CURRENT);

   // find system screen resolution. used here and in RealizeFont
  HPS ps = ::WinGetScreenPS(HWND_DESKTOP);
  HDC hdc = GFX (::GpiQueryDevice (ps), HDC_ERROR);
  GFX (::DevQueryCaps(hdc, CAPS_HORIZONTAL_FONT_RES, 1, &nsFontMetricsOS2::gSystemRes), FALSE);
  ::WinReleasePS(ps);

  if (!nsFontMetricsOS2::gGlobalFonts) {
    if (!nsFontMetricsOS2::InitializeGlobalFonts()) {
      FreeGlobals();
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  // Set prefVal the value of the pref "browser.display.screen_resolution"
  // When a new profile is created, the pref is set to 0.  This tells the code
  // to default to font resolution of the screen (96 or 120)
  nsresult res;
  PRInt32 prefVal = -1;

  res = gPref->GetIntPref( "browser.display.screen_resolution", &prefVal );
  if (NS_FAILED(res))
    prefVal = 0;

  gPref->RegisterCallback( "browser.display.screen_resolution", prefChanged, NULL );

  if (prefVal == 0)
  {
    prefVal = nsFontMetricsOS2::gSystemRes;
  }

  nsFontMetricsOS2::gDPI = prefVal;

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

nsFontMetricsOS2::nsFontMetricsOS2()
{
  NS_INIT_REFCNT();

  // members are zeroed by new operator (hmm) - yeah right
  mTriedAllGenerics = 0;
}

nsFontMetricsOS2::~nsFontMetricsOS2()
{
  delete mFontHandle;
  if (mDeviceContext) {
    // Notify our device context that owns us so that it can update its font cache
    mDeviceContext->FontMetricsDeleted(this);
    mDeviceContext = nsnull;
  }
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

  static int initialized = 0;
  if (!initialized)
  {
    initialized = 1;

    nsCOMPtr<nsIPref> prefs(do_GetService(kPrefCID, &res));
    if (NS_SUCCEEDED(res) && prefs)
    {
      res = prefs->GetBoolPref("font.substitute_vector_fonts", &gSubstituteVectorFonts);
      NS_ASSERTION( NS_SUCCEEDED(res), "Could not get pref 'font.substitute_vector_fonts'" );
    }
  }

   mFont = aFont;
   mLangGroup = aLangGroup;
   mDeviceContext = (nsDeviceContextOS2 *) aContext;
   mIsUserDefined = PR_FALSE;
   return RealizeFont();
}

nsresult nsFontMetricsOS2::Destroy()
{
   mDeviceContext = nsnull;
   return NS_OK;
}

static PLHashNumber PR_CALLBACK
HashKey(const void* aString)
{
  const nsString* str = (const nsString*)aString;
  return (PLHashNumber)
    nsCRT::HashCode(str->get());
}

static PRIntn PR_CALLBACK
CompareKeys(const void* aStr1, const void* aStr2)
{
  return nsCRT::strcmp(((const nsString*) aStr1)->get(),
    ((const nsString*) aStr2)->get()) == 0;
}

// Utility; delete [] when done.
static FONTMETRICS* getMetrics( long &lFonts, PCSZ facename, HPS hps)
{
   LONG lWant = 0;
   lFonts = GFX (::GpiQueryFonts (hps, QF_PUBLIC | QF_PRIVATE,
                                  facename, &lWant, 0, 0),
                 GPI_ALTERROR);
   FONTMETRICS* pMetrics = (FONTMETRICS*)nsMemory::Alloc(lFonts * sizeof(FONTMETRICS));

   GFX (::GpiQueryFonts (hps, QF_PUBLIC | QF_PRIVATE, facename,
                         &lFonts, sizeof (FONTMETRICS), pMetrics),
        GPI_ALTERROR);

   return pMetrics;
}

/* For a vector font, we create a charbox based directly on mFont.size, so
 * we don't need to calculate anything here.  For image fonts, though, we
 * want to find the closest match for the given mFont.size.
 */
void
nsFontMetricsOS2::SetFontHandle( HPS aPS, nsFontOS2* aFont )
{
  int points = NSTwipsToIntPoints( mFont.size );

  FATTRS* fattrs = &(aFont->fattrs);
  if( fattrs->fsFontUse == 0 )  // if image font
  {
    char alias[FACESIZE];

     // If the font.substitute_vector_fonts pref is set, then we exchange
     // Times New Roman for Tms Rmn and Helvetica for Helv if the requested
     // points size is less than the minimum or more than the maximum point
     // size available for Tms Rmn and Helv.
    if( gSubstituteVectorFonts &&
        (points > 18 || points < 8 || (gDPI != 96 && gDPI != 120)) &&
        GetVectorSubstitute( aPS, fattrs->szFacename, alias ))
    {
      PL_strcpy( fattrs->szFacename, alias );
      fattrs->fsFontUse = FATTR_FONTUSE_OUTLINE | FATTR_FONTUSE_TRANSFORMABLE;
      fattrs->fsSelection &= ~(FM_SEL_BOLD | FM_SEL_ITALIC);
    }

    if( fattrs->fsFontUse == 0 )    // if still image font
    {
      long lFonts = 0;
      FONTMETRICS* pMetrics = getMetrics( lFonts, fattrs->szFacename, aPS);

      int curPoints = 0;
      for( int i = 0; i < lFonts; i++)
      {
        if( pMetrics[i].sYDeviceRes == gSystemRes )
        {
          if (pMetrics[i].sNominalPointSize / 10 == points)
          {
            // image face found fine, set required size in fattrs.
            curPoints = pMetrics[i].sNominalPointSize / 10;
            fattrs->lMaxBaselineExt = pMetrics[i].lMaxBaselineExt;
            fattrs->lAveCharWidth = pMetrics[i].lAveCharWidth;
            break;
          }
          else
          {
            if (abs(pMetrics[i].sNominalPointSize / 10 - points) < abs(curPoints - points))
            {
              curPoints = pMetrics[i].sNominalPointSize / 10;
              fattrs->lMaxBaselineExt = pMetrics[i].lMaxBaselineExt;
              fattrs->lAveCharWidth = pMetrics[i].lAveCharWidth;
            }
            else if (abs(pMetrics[i].sNominalPointSize / 10 - points) == abs(curPoints - points))
            {
              if ((pMetrics[i].sNominalPointSize / 10) > curPoints)
              {
                curPoints = pMetrics[i].sNominalPointSize / 10;
                fattrs->lMaxBaselineExt = pMetrics[i].lMaxBaselineExt;
                fattrs->lAveCharWidth = pMetrics[i].lAveCharWidth;
              }
            }
          }
        }
      } /* endfor */

      points = curPoints;
      nsMemory::Free(pMetrics);
    }
  }

   // 5) Add effects
  if( mFont.decorations & NS_FONT_DECORATION_UNDERLINE)
    fattrs->fsSelection |= FATTR_SEL_UNDERSCORE;
  if( mFont.decorations & NS_FONT_DECORATION_LINE_THROUGH)
    fattrs->fsSelection |= FATTR_SEL_STRIKEOUT;

   // 6) Encoding
   // There doesn't seem to be any encoding stuff yet, so guess.
   // (XXX unicode hack; use same codepage as converter!)
  const PRUnichar* langGroup = nsnull;
  mLangGroup->GetUnicode(&langGroup);
  nsCAutoString name(NS_LossyConvertUCS2toASCII(langGroup).get());
  for (int j=0; j < eCharset_COUNT; j++ )
  {
    if (name.get()[0] == gCharsetInfo[j].mLangGroup[0])
    {
      if (!strcmp(name.get(), gCharsetInfo[j].mLangGroup))
      {
        fattrs->usCodePage = gCharsetInfo[j].mCodePage;
        mCodePage = gCharsetInfo[j].mCodePage;
        break;
      }
    }
  }

   // set up the charbox;  set for image fonts also, in case we need to
   //  substitute a vector font later on (for UTF-8, etc)
  float app2dev, fHeight;
  mDeviceContext->GetAppUnitsToDevUnits( app2dev );

  if( !mDeviceContext->mPrintDC ) /* if not printing */
    if( fattrs->fsFontUse == 0 )    /* if image font */
      fHeight = points * gDPI / 72;
    else
      fHeight = mFont.size * app2dev * gDPI / nsFontMetricsOS2::gSystemRes;
  else
    fHeight = mFont.size * app2dev;

  long lFloor = NSToIntFloor( fHeight ); 
  aFont->charbox.cx = MAKEFIXED( lFloor, (fHeight - (float)lFloor) * 65536.0f );
  aFont->charbox.cy = aFont->charbox.cx;
}

/* aName is a font family name.  see if fonts of that family exist
 *  if so, return font structure with family name
 */
nsFontOS2*
nsFontMetricsOS2::LoadFont( HPS aPS, nsString* aName )
{
  char familyname[FACESIZE];
  WideCharToMultiByte(0, aName->get(), aName->Length() + 1,
    familyname, sizeof(familyname));
  
  return LoadFont( aPS, familyname );
}

nsFontOS2*
nsFontMetricsOS2::LoadFont( HPS aPS, const char* fontname )
{
  nsFontOS2* fh = nsnull;
  char alias[FACESIZE];

   // set style flags
  PRBool bBold = mFont.weight > NS_FONT_WEIGHT_NORMAL;
  PRBool bItalic = (mFont.style & (NS_FONT_STYLE_ITALIC | NS_FONT_STYLE_OBLIQUE));
  USHORT flags = bBold ? FM_SEL_BOLD : 0;
  flags |= bItalic ? FM_SEL_ITALIC : 0;
 
   // always pass vector fonts to the printer
  if( !mDeviceContext->SupportsRasterFonts() &&
      GetVectorSubstitute( aPS, fontname, alias ))
  {
    fh = new nsFontOS2();
    PL_strcpy( fh->fattrs.szFacename, alias );
    fh->fattrs.fsFontUse = FATTR_FONTUSE_OUTLINE | FATTR_FONTUSE_TRANSFORMABLE;
    SetFontHandle( aPS, fh );
    return fh;
  }
 
  int i = 0;
  while( i < gGlobalFonts->Count() )
  {
    nsGlobalFont* font = (nsGlobalFont*)gGlobalFonts->ElementAt(i);
    char* metricsfontname = NULL;
    if( font->metrics.fsType & FM_TYPE_DBCS )
      metricsfontname = (char*)&(font->metrics.szFacename);
    else
      metricsfontname = (char*)&(font->metrics.szFamilyname);
    
    if( PL_strcasecmp( metricsfontname, fontname ) == 0 )
    {
      fh = new nsFontOS2();
      FATTRS* fattrs = &(fh->fattrs);
 
      for( int j = i; j < font->nextFamily; j++ )
      {
        nsGlobalFont* fontj = (nsGlobalFont*)gGlobalFonts->ElementAt(j);
        nsMiniFontMetrics *fm = &(fontj->metrics);
        if( (fm->fsSelection & (FM_SEL_ITALIC | FM_SEL_BOLD)) == flags )
        {
          PL_strcpy( fattrs->szFacename, fm->szFacename );
          if( fm->fsDefn & FM_DEFN_OUTLINE ||
              !mDeviceContext->SupportsRasterFonts() )
            fh->fattrs.fsFontUse = FATTR_FONTUSE_OUTLINE | FATTR_FONTUSE_TRANSFORMABLE;
          if( fm->fsType & FM_TYPE_MBCS )
            fh->fattrs.fsType |= FATTR_TYPE_MBCS;
          if( fm->fsType & FM_TYPE_DBCS )
            fh->fattrs.fsType |= FATTR_TYPE_DBCS;
            
          SetFontHandle( aPS, fh );
          return fh;
        }
      }
 
       // A font of family "familyname" and with the applied effects (bold &
       // italic) was not found.  Therefore, just look for a 'regular' font
       // (that is not italic and not bold), and then have the
       // system simulate the appropriate effects (see RealizeFont()).
      for( int m = i; m < font->nextFamily; m++ )
      {
        nsGlobalFont* fontm = (nsGlobalFont*)gGlobalFonts->ElementAt(m);
        nsMiniFontMetrics *fm = &(fontm->metrics);
        if( !(fm->fsSelection & (FM_SEL_ITALIC | FM_SEL_BOLD)) )
        {
          PL_strcpy( fattrs->szFacename, fm->szFacename );
          if( fm->fsDefn & FM_DEFN_OUTLINE ||
              !mDeviceContext->SupportsRasterFonts() )
            fattrs->fsFontUse = FATTR_FONTUSE_OUTLINE | FATTR_FONTUSE_TRANSFORMABLE;
          else
            fattrs->fsFontUse = 0;
          if( fm->fsType & FM_TYPE_MBCS )
            fattrs->fsType |= FATTR_TYPE_MBCS;
          if( fm->fsType & FM_TYPE_DBCS )
            fattrs->fsType |= FATTR_TYPE_DBCS;

           // fake the effects
          if( bBold )
            fattrs->fsSelection |= FATTR_SEL_BOLD;
          if( bItalic )
            fattrs->fsSelection |= FATTR_SEL_ITALIC;

          SetFontHandle( aPS, fh );
          return fh;
        }
      }
    }
 
    i = font->nextFamily;
  }   // end while
 
   // If a font was not found, then maybe "familyname" is really a face name.
   // See if a font with that facename exists on system and fake any applied
   // effects
  long lFonts = 0;
  FONTMETRICS* pMetrics = getMetrics( lFonts, fontname, aPS );
 
  if( lFonts > 0 )
  {
    fh = new nsFontOS2();
    PL_strcpy( fh->fattrs.szFacename, fontname );
    fh->fattrs.fsFontUse = FATTR_FONTUSE_OUTLINE | FATTR_FONTUSE_TRANSFORMABLE;

    if( bBold )
      fh->fattrs.fsSelection |= FATTR_SEL_BOLD;
    if( bItalic )
      fh->fattrs.fsSelection |= FATTR_SEL_ITALIC;
 
    if( mDeviceContext->SupportsRasterFonts() )
    {
      for (int i = 0 ; i < lFonts ; i++)
        if( !(pMetrics[i].fsDefn & FM_DEFN_OUTLINE) )
        {
          fh->fattrs.fsFontUse = 0;
          break;
        }
    }
  }
  
  nsMemory::Free(pMetrics);
 
  if( fh )
    SetFontHandle( aPS, fh );
    
  return fh;
}

nsFontOS2*
nsFontMetricsOS2::FindUserDefinedFont( HPS aPS )
{
  if (mIsUserDefined) {
    nsFontOS2* font = LoadFont( aPS, &mUserDefined );
    if( font )
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
nsFontMetricsOS2::InitializeFamilyNames(void)
{
  if( !gFamilyNames )
  {
    gFamilyNames = PL_NewHashTable(0, HashKey, CompareKeys, nsnull, &familyname_HashAllocOps,
      nsnull);
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
    if (mFontIsGeneric[mFontsIndex]) {
      return nsnull;
    }
    nsString* name = mFonts.StringAt(mFontsIndex++);
    nsAutoString low(*name);
    ToLowerCase(low);
    nsString* winName = (nsString*) PL_HashTableLookup(gFamilyNames, &low);
    if (!winName) {
      winName = name;
    }
    nsFontOS2* font = LoadFont( aPS, winName );
    if( font )
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
  context->mFont = metrics->LoadFont( ps, (nsString*)&aFamily );
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

nsFontOS2*
nsFontMetricsOS2::FindGenericFont( HDC aPS )
{
  if (mTriedAllGenerics) {
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
#ifdef WINCODE
  if (mTriedAllPref) {
    // don't bother anymore because mLoadedFonts[] already has all our pref fonts
    return nsnull;
  }
#endif
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
#ifdef WINCODE
  mTriedAllPref = 1;
#endif
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

  return font;
}

static PRBool
FontEnumCallback(const nsString& aFamily, PRBool aGeneric, void *aData)
{
  nsFontMetricsOS2* metrics = (nsFontMetricsOS2*) aData;
  /* Hack for Truetype on OS/2 - if it's Arial and not 1252 or 0, just get another font */
  if (aFamily.Find("Arial", IGNORE_CASE) != -1) {
     if (metrics->mCodePage != 1252) {
        if ((metrics->mCodePage == 0) &&
            (ulSystemCodePage != 850) &&
            (ulSystemCodePage != 437)) {
           return PR_TRUE; // don't stop
        }
     }
  }
  metrics->mFonts.AppendString(aFamily);
  metrics->mFontIsGeneric.AppendElement((void*) aGeneric);
  if (aGeneric) {
    metrics->mGeneric.Assign(aFamily);
    ToLowerCase(metrics->mGeneric);
    return PR_FALSE; // stop
  }

  return PR_TRUE; // don't stop
}

PRBool
nsFontMetricsOS2::GetVectorSubstitute( HPS aPS, const char* aFamilyname,
                                       char* alias )
{
   // set style flags
  PRBool isBold = mFont.weight > NS_FONT_WEIGHT_NORMAL;
  PRBool isItalic = (mFont.style & (NS_FONT_STYLE_ITALIC | NS_FONT_STYLE_OBLIQUE));

  if( PL_strcasecmp( aFamilyname, "Tms Rmn" ) == 0 )
  {
    if( !isBold && !isItalic )
      PL_strcpy( alias, "Times New Roman" );
    else if( isBold )
      if( !isItalic )
        PL_strcpy( alias, "Times New Roman Bold" );
      else
        PL_strcpy( alias, "Times New Roman Bold Italic" );
    else
      PL_strcpy( alias, "Times New Roman Italic" );

    return PR_TRUE;
  }

  if( PL_strcasecmp( aFamilyname, "Helv" ) == 0 )
  {
    if( !isBold && !isItalic )
      PL_strcpy( alias, "Helvetica" );
    else if( isBold )
      if( !isItalic )
        PL_strcpy( alias, "Helvetica Bold" );
      else
        PL_strcpy( alias, "Helvetica Bold Italic" );
    else
      PL_strcpy( alias, "Helvetica Italic" );

    return PR_TRUE;
  }

   // When printing, substitute vector fonts for these common bitmap fonts
  if( !mDeviceContext->SupportsRasterFonts() )
  {
    if( PL_strcasecmp( aFamilyname, "System Proportional" ) == 0 )
    {
      if( !isBold && !isItalic )
        PL_strcpy( alias, "Helvetica" );
      else if( isBold )
        if( !isItalic )
          PL_strcpy( alias, "Helvetica Bold" );
        else
          PL_strcpy( alias, "Helvetica Bold Italic" );
      else
        PL_strcpy( alias, "Helvetica Italic" );

      return PR_TRUE;
    }

    if( PL_strcasecmp( aFamilyname, "System Monospaced" ) == 0 ||
        PL_strcasecmp( aFamilyname, "System VIO" ) == 0 )
    {
      if( !isBold && !isItalic )
        PL_strcpy( alias, "Courier" );
      else if( isBold )
        if( !isItalic )
          PL_strcpy( alias, "Courier Bold" );
        else
          PL_strcpy( alias, "Courier Bold Italic" );
      else
        PL_strcpy( alias, "Courier Italic" );

      return PR_TRUE;
    }
  }
  
#if 0
   // Generic case:  see if this family has a vector font matching the
   // given style
  USHORT flags = isBold ? FM_SEL_BOLD : 0;
  flags |= isItalic ? FM_SEL_ITALIC : 0;

  long lFonts = 0;
  FONTMETRICS* pMetrics = getMetrics( lFonts, aFamilyname, aPS );

  if( lFonts > 0 )
  {
    for( int i = 0; i < lFonts; i++ )
    {
      if( (pMetrics[i].fsDefn & FM_DEFN_OUTLINE)  &&
          ((pMetrics[i].fsSelection & (FM_SEL_ITALIC | FM_SEL_BOLD)) == flags) )
      {
        PL_strcpy( alias, pMetrics[i].szFacename );
        return PR_TRUE;
      }
    }
  }
  
  nsMemory::Free(pMetrics);
#endif

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
#ifdef WINCODE
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
        rv = mapper->FillInfo(gUserDefinedMap);
      }
    }
#endif
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
  if( !font )
  {
    if( mDeviceContext->mPrintDC == NULL)
      ::WinReleasePS(ps);
    return NS_ERROR_FAILURE;
  }

   // 9) Record font handle & record various font metrics to cache
  mFontHandle = font;
  GFX (::GpiCreateLogFont (ps, 0, 1, &(font->fattrs)), GPI_ERROR);
  font->SelectIntoPS( ps, 1 );

  FONTMETRICS fm;
  GFX (::GpiQueryFontMetrics (ps, sizeof (fm), &fm), FALSE);

  float dev2app;
  mDeviceContext->GetDevUnitsToAppUnits(dev2app);
  
   // Sometimes the fontmetrics for fonts don't add up as documented.
   // Sometimes EmHeight + InternalLeading is greater than the MaxBaselineExt.
   // This hack attempts to appease all fonts.  
  if( fm.lMaxBaselineExt >= fm.lEmHeight + fm.lInternalLeading )
  {
    mMaxHeight = NSToCoordRound( fm.lMaxBaselineExt * dev2app );
    mEmHeight = NSToCoordRound((fm.lMaxBaselineExt - fm.lInternalLeading) * dev2app );
    mEmAscent = NSToCoordRound((fm.lMaxAscender - fm.lInternalLeading) * dev2app );
  }
  else
  {
    mMaxHeight = NSToCoordRound((fm.lEmHeight + fm.lInternalLeading) * dev2app );
    mEmHeight = NSToCoordRound( fm.lEmHeight * dev2app);
    mEmAscent = NSToCoordRound((fm.lEmHeight - fm.lMaxDescender) * dev2app );
  } 

#if 0
 #if 0
   // There is another problem where mozilla displays text highlighting too
   // high for bitmap fonts other than warpsans.  So we fiddle with mMaxAscent
   // and mMacDescent in these cases so that mozilla will properly position the
   // highlight box.
  if( !(fm.fsDefn & FM_DEFN_OUTLINE) &&
      PL_strcasecmp("WarpSans", fm.szFamilyname) != 0 )
  {
    mMaxAscent  = NSToCoordRound((fm.lMaxAscender - 1) * dev2app );
    mMaxDescent = NSToCoordRound((fm.lMaxDescender + 1) * dev2app );
  }
 #else
  if( PL_strcasecmp( "WarpSans", fm.szFamilyname ) != 0 )
  {
    mMaxAscent  = NSToCoordRound((fm.lMaxAscender - 1) * dev2app );
    mMaxDescent = NSToCoordRound((fm.lMaxDescender + 1) * dev2app );
  }
 #endif
  else
  {
    mMaxAscent  = NSToCoordRound( fm.lMaxAscender * dev2app );
    mMaxDescent = NSToCoordRound( fm.lMaxDescender * dev2app );
  }
#else
  mMaxAscent  = NSToCoordRound( fm.lMaxAscender * dev2app );
  mMaxDescent = NSToCoordRound( fm.lMaxDescender * dev2app );
#endif
  
  mMaxAdvance = NSToCoordRound( fm.lMaxCharInc * dev2app );
  mEmDescent  = NSToCoordRound( fm.lMaxDescender * dev2app );
  mLeading    = NSToCoordRound( fm.lInternalLeading * dev2app );
  mXHeight    = NSToCoordRound( fm.lXHeight * dev2app );

  nscoord onePixel = NSToCoordRound(1 * dev2app);

   // Not all fonts specify these two values correctly, and some not at all
  mSuperscriptYOffset = mXHeight;
  mSubscriptYOffset   = NSToCoordRound( mXHeight / 3.0f );

   // Using lStrikeoutPosition puts the strikeout too high
   // Use 50% of lXHeight instead
  mStrikeoutPosition  = NSToCoordRound( mXHeight / 2.0f);
  mStrikeoutSize      = PR_MAX(onePixel, NSToCoordRound(fm.lStrikeoutSize * dev2app));

   // Let there always be a minimum of one pixel space between the text
   // and the underline
  if( fm.lEmHeight < 10 )
    mUnderlinePosition  = -NSToCoordRound( fm.lUnderscorePosition * dev2app );
  else
    mUnderlinePosition  = -PR_MAX(onePixel*2, NSToCoordRound( fm.lUnderscorePosition * dev2app));
  mUnderlineSize      = PR_MAX(onePixel, NSToCoordRound(fm.lUnderscoreSize * dev2app));

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

NS_IMETHODIMP nsFontMetricsOS2::GetLeading( nscoord &aLeading)
{
   aLeading = mLeading;
   return NS_OK;
}

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
  if (!aLangGroup) {
    return NS_ERROR_NULL_POINTER;
  }

  *aLangGroup = mLangGroup;
  NS_IF_ADDREF(*aLangGroup);

  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsOS2::GetNormalLineHeight(nscoord &aHeight)
{
  aHeight = mEmHeight + mLeading;
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
nsFontMetricsOS2::GetUnicodeFont( HPS aPS )
{
  nsresult res;
  nsCAutoString pref, generic;
  nsXPIDLString value;

  generic.Assign(NS_ConvertUCS2toUTF8(mGeneric));

  pref.Assign( "font.name." );
  pref.Append( generic );
  pref.Append( ".x-unicode" );
   
  res = gPref->CopyUnicharPref(pref.get(), getter_Copies(value));      
  if (NS_FAILED(res))
    return nsnull;
    
  nsAutoString fontname;
  fontname.Assign( value );
  
  nsFontOS2* fh =  LoadFont( aPS, &fontname );
  if( fh )
  {
    fh->fattrs.usCodePage = 1208;
    return fh;
  }
    
   // User defined unicode font did not load.  Fall back to font
   // specified in font.name-list.%.x-unicode
  pref.Assign( "font.name-list." );
  pref.Append( generic );
  pref.Append( ".x-unicode" );
   
  res = gPref->CopyUnicharPref(pref.get(), getter_Copies(value));      
  if (NS_FAILED(res))
    return nsnull;
    
  nsFont font("", 0, 0, 0, 0, 0);
  font.name.Assign(value);
  
   // Iterate over the list of names using the callback mechanism of nsFont
  GenericFontEnumContext context = {aPS, nsnull, this};
  font.EnumerateFamilies(GenericFontEnumCallback, &context);
  if (context.mFont) // a suitable font was found
  {
    context.mFont->fattrs.usCodePage = 1208;
    return context.mFont;
  }
  
  return nsnull;
}

nsresult
nsFontMetricsOS2::ResolveForwards(HPS                  aPS,
                                  const PRUnichar*     aString,
                                  PRUint32             aLength,
                                  nsFontSwitchCallback aFunc, 
                                  void*                aData)
{
  NS_ASSERTION(aString || !aLength, "invalid call");
  PRBool running = PR_TRUE;
  const PRUnichar* firstChar = aString;
  const PRUnichar* lastChar  = aString + aLength;
  const PRUnichar* currChar = firstChar;

  nsFontSwitch fontSwitch;
  fontSwitch.mFont = 0;
  
  if( mCodePage == 1252 )
  {
    while( running && firstChar < lastChar )
    {
      if( *currChar > 0x00FF )
      { 
        nsFontOS2* fh;
        fh = GetUnicodeFont(aPS);
        fontSwitch.mFont = fh;
        while( ++currChar < lastChar )
        {
          if( *currChar <= 0x00FF )
            break;
        }

        running = (*aFunc)(&fontSwitch, firstChar, currChar - firstChar, aData);
      }
      else
      {
         // Use currently selected font
        nsFontOS2* fh = new nsFontOS2();
        *fh = *mFontHandle;
        fontSwitch.mFont = fh;
        while( ++currChar < lastChar )
        {
          if( *currChar > 0x00FF )
            break;
        }

        running = (*aFunc)(&fontSwitch, firstChar, currChar - firstChar, aData);
      }
      
      firstChar = currChar;
    }
  }
  else
  {
    while( running && firstChar < lastChar )
    {
      if( *currChar >= 0x0080 && *currChar <= 0x00FF )
      { 
        nsFontOS2* fh = new nsFontOS2();
        *fh = *mFontHandle;
        fh->fattrs.usCodePage = 1252;
        fontSwitch.mFont = fh;
        while( ++currChar < lastChar )
        {
          if( *currChar < 0x0080 || *currChar > 0x00FF )
            break;
        }

        running = (*aFunc)(&fontSwitch, firstChar, currChar - firstChar, aData);
      }
      else
      {
         // Use currently selected font
        nsFontOS2* fh = new nsFontOS2();
        *fh = *mFontHandle;
        fontSwitch.mFont = fh;
        while( ++currChar < lastChar )
        {
          if( *currChar >= 0x0080 && *currChar <= 0x00FF )
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
  PRBool running = PR_TRUE;
  const PRUnichar* firstChar = aString + aLength - 1;
  const PRUnichar* lastChar  = aString - 1;
  const PRUnichar* currChar  = firstChar;

  nsFontSwitch fontSwitch;
  fontSwitch.mFont = 0;
  
  if( mCodePage == 1252 )
  {
    while( running && firstChar > lastChar )
    {
      if( *currChar > 0x00FF )
      { 
        nsFontOS2* fh;
        fh = GetUnicodeFont(aPS);
        fontSwitch.mFont = fh;
        while( --currChar > lastChar )
        {
          if( *currChar <= 0x00FF )
            break;
        }

        running = (*aFunc)(&fontSwitch, firstChar, currChar - firstChar, aData);
      }
      else
      {
         // Use currently selected font
        nsFontOS2* fh = new nsFontOS2();
        *fh = *mFontHandle;
        fontSwitch.mFont = fh;
        while( --currChar > lastChar )
        {
          if( *currChar > 0x00FF )
            break;
        }

        running = (*aFunc)(&fontSwitch, firstChar, currChar - firstChar, aData);
      }
      
      firstChar = currChar;
    }
  }
  else
  {
    while( running && firstChar > lastChar )
    {
      if( *currChar >= 0x0080 && *currChar <= 0x00FF )
      { 
        nsFontOS2* fh = new nsFontOS2();
        *fh = *mFontHandle;
        fh->fattrs.usCodePage = 1252;
        fontSwitch.mFont = fh;
        while( --currChar > lastChar )
        {
          if( *currChar < 0x0080 || *currChar > 0x00FF )
            break;
        }

        running = (*aFunc)(&fontSwitch, firstChar, currChar - firstChar, aData);
      }
      else
      {
         // Use currently selected font
        nsFontOS2* fh = new nsFontOS2();
        *fh = *mFontHandle;
        fontSwitch.mFont = fh;
        while( --currChar > lastChar )
        {
          if( *currChar >= 0x0080 && *currChar <= 0x00FF )
            break;
        }

        running = (*aFunc)(&fontSwitch, firstChar, currChar - firstChar, aData);
      }
      
      firstChar = currChar;
    }
  }

  return NS_OK;
}

static int PR_CALLBACK
CompareFontFamilyNames(const void* aArg1, const void* aArg2, void* aClosure)
{
  const nsGlobalFont* font1 = (const nsGlobalFont*) aArg1;
  const nsGlobalFont* font2 = (const nsGlobalFont*) aArg2;
  const nsMiniFontMetrics* fm1 = &(font1->metrics);
  const nsMiniFontMetrics* fm2 = &(font2->metrics);

  PRInt32 weight1 = 0, weight2 = 0;

   // put vertical fonts last
  if( fm1->szFacename[0] == '@' )
    weight1 |= 0x100;
  if( fm2->szFacename[0] == '@' )
    weight2 |= 0x100;

  if( weight1 == weight2 )
  {
    if( fm1->fsType & FM_TYPE_DBCS || fm2->fsType & FM_TYPE_DBCS )
      return PL_strcasecmp( fm1->szFacename, fm2->szFacename );
    else
      return PL_strcasecmp( fm1->szFamilyname, fm2->szFamilyname );
  }
  else
    return weight1 - weight2;
}

nsVoidArray*
nsFontMetricsOS2::InitializeGlobalFonts()
{
  gGlobalFonts = new nsVoidArray();
  if (!gGlobalFonts)
    return nsnull;

  HPS ps = ::WinGetScreenPS(HWND_DESKTOP);

  LONG lRemFonts = 0, lNumFonts;
  lNumFonts = GFX (::GpiQueryFonts (ps, QF_PUBLIC, NULL, &lRemFonts, 0, 0),
                   GPI_ALTERROR);
  FONTMETRICS* pFontMetrics = (PFONTMETRICS) nsMemory::Alloc(lNumFonts * sizeof(FONTMETRICS));
  lRemFonts = GFX (::GpiQueryFonts (ps, QF_PUBLIC, NULL, &lNumFonts,
                                    sizeof (FONTMETRICS), pFontMetrics),
                   GPI_ALTERROR);

  for( int i = 0; i < lNumFonts; i++ )
  {
     // The discrepencies between the Courier bitmap and outline fonts are
     // too much to deal with, so we only use the outline font
    if( PL_strcasecmp(pFontMetrics[i].szFamilyname, "Courier") == 0 &&
        !(pFontMetrics[i].fsDefn & FM_DEFN_OUTLINE))
      continue;

    nsGlobalFont* font = new nsGlobalFont;
    PL_strcpy(font->metrics.szFamilyname, pFontMetrics[i].szFamilyname);
    PL_strcpy(font->metrics.szFacename, pFontMetrics[i].szFacename);
    font->metrics.fsType = pFontMetrics[i].fsType;
    font->metrics.fsDefn = pFontMetrics[i].fsDefn;
    font->metrics.fsSelection = pFontMetrics[i].fsSelection;
  #ifdef DEBUG_pedemont
    font->metrics.lMatch = pFontMetrics[i].lMatch;
  #endif

     // Set the FM_SEL_BOLD flag in fsSelection.  This makes the check for
     // bold and italic much easier in LoadFont
    if( pFontMetrics[i].usWeightClass > 5 )
      font->metrics.fsSelection |= FM_SEL_BOLD;

    //font->signature  = pFontMetrics[i].fsDefn;
    
    gGlobalFonts->AppendElement(font);
  }

  nsMemory::Free(pFontMetrics);

   // Sort the global list of fonts to put the 'preferred' fonts first
  gGlobalFonts->Sort(CompareFontFamilyNames, nsnull);

  int prevIndex = 0;
  nsGlobalFont* font = (nsGlobalFont*)gGlobalFonts->ElementAt(0);
  char* lastName = font->metrics.szFamilyname;

  int count = gGlobalFonts->Count(); 
  for( int j = 0; j < count; j++ )
  {
    font = (nsGlobalFont*)gGlobalFonts->ElementAt(j);
    
     // for DBCS fonts, differentiate fonts by Face Name;  for others,
     // Family Name is enough
    char* fontname = NULL;
    if( font->metrics.fsType & FM_TYPE_DBCS )
      fontname = (char*)&(font->metrics.szFacename);
    else
      fontname = (char*)&(font->metrics.szFamilyname);

    if( PL_strcasecmp( fontname, lastName ) != 0 )
    {
      lastName = fontname;
      font = (nsGlobalFont*)gGlobalFonts->ElementAt(prevIndex);
      font->nextFamily = j;
      prevIndex = j;
    }
    else
      font->nextFamily = 0;
  }

  font = (nsGlobalFont*)gGlobalFonts->ElementAt(prevIndex);
  font->nextFamily = count;
  ::WinReleasePS(ps);
    
#ifdef DEBUG_pedemont
  for( int k = 0; k < gGlobalFonts->Count(); k++ )
  {
    font = (nsGlobalFont*)gGlobalFonts->ElementAt(k);
    nsMiniFontMetrics* fm = &(font->metrics);
    printf( " %d: %32s", k, fm->szFacename );
    
    if( fm->fsDefn & FM_DEFN_OUTLINE )
      printf( " : vector" );
    else
      printf( " : bitmap" );
      
    if( fm->fsSelection & FM_SEL_BOLD )
      printf( " : bold" );
    else
      printf( " :     " );
      
    if( fm->fsSelection & FM_SEL_ITALIC )
      printf( " : italic" );
    else
      printf( " :       " );

    printf( " : %d\n", fm->lMatch );
  }
  fflush( stdout );
#endif

  return gGlobalFonts;
}

nsFontOS2*
nsFontMetricsOS2::FindGlobalFont( HPS aPS )
{
  nsFontOS2* fh = nsnull;
  int count = gGlobalFonts->Count();
  for (int i = 0; i < count; i++)
  {
    nsGlobalFont* font = (nsGlobalFont*)gGlobalFonts->ElementAt(i);
    fh = LoadFont( aPS, font->metrics.szFacename );
    if( fh )
      return fh;
  }

  return nsnull;
}


// The Font Enumerator

nsFontEnumeratorOS2::nsFontEnumeratorOS2()
{
  NS_INIT_REFCNT();
}

NS_IMPL_ISUPPORTS1(nsFontEnumeratorOS2, nsIFontEnumerator)

static int PR_CALLBACK
CompareFontNames(const void* aArg1, const void* aArg2, void* aClosure)
{
  const nsDependentString str1( *((const PRUnichar**)aArg1) );
  const nsDependentString str2( *((const PRUnichar**)aArg2) );

   // intermingle vertical fonts (start with '@') with horizontal fonts
  if( str1.First() == PRUnichar('@') )
  {
    if( str2.First() == PRUnichar('@') )
      return Compare( str1, str2, nsCaseInsensitiveStringComparator() );
    else
    {
      nsString temp( str1 );
      temp.Trim( "@", PR_TRUE, PR_FALSE );
      int rv = Compare( temp, str2, nsCaseInsensitiveStringComparator() );
      if( rv == 0 )
        return 1;
      else
        return rv;
    }
  }
  else if( str2.First() == PRUnichar('@') )
  {
    nsString temp( str2 );
    temp.Trim( "@", PR_TRUE, PR_FALSE );
    int rv = Compare( str1, temp, nsCaseInsensitiveStringComparator() );
    if( rv == 0 )
      return -1;
    else
      return rv;
  }
  else
    return Compare( str1, str2, nsCaseInsensitiveStringComparator() );
}

NS_IMETHODIMP
nsFontEnumeratorOS2::EnumerateFonts(const char* aLangGroup,
  const char* aGeneric, PRUint32* aCount, PRUnichar*** aResult)
{
  return EnumerateAllFonts(aCount, aResult);
}

NS_IMETHODIMP
nsFontEnumeratorOS2::EnumerateAllFonts(PRUint32* aCount, PRUnichar*** aResult)
{
  NS_ENSURE_ARG_POINTER(aCount);
  NS_ENSURE_ARG_POINTER(aResult);

  if (!nsFontMetricsOS2::gGlobalFonts) {
    if (!nsFontMetricsOS2::InitializeGlobalFonts()) {
      return nsnull;
    }
  }

  int count = nsFontMetricsOS2::gGlobalFonts->Count();
  PRUnichar** array = (PRUnichar**)nsMemory::Alloc(count * sizeof(PRUnichar*));
  if (!array) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  int i = 0;
  int j = 0;
  while( i < count )
  {
    nsGlobalFont* font = (nsGlobalFont*)nsFontMetricsOS2::gGlobalFonts->ElementAt(i);
    PRUnichar* str = (PRUnichar*)nsMemory::Alloc((FACESIZE+1) * sizeof(PRUnichar));
    if (!str) {
      for (i = i - 1; i >= 0; --i) {
        nsMemory::Free(array[i]);
      }
      nsMemory::Free(array);
      return NS_ERROR_OUT_OF_MEMORY;
    }

    str[0] = L'\0';
    if( font->metrics.fsType & FM_TYPE_DBCS )
    {
      MultiByteToWideChar(0, font->metrics.szFacename,
         strlen(font->metrics.szFacename)+1, str, FACESIZE+1);
    }
    else
    {
      MultiByteToWideChar(0, font->metrics.szFamilyname,
         strlen(font->metrics.szFamilyname)+1, str, FACESIZE+1);
    }
    array[j++] = str;
    
    i = font->nextFamily;
  }

  NS_QuickSort(array, j, sizeof(PRUnichar*), CompareFontNames, nsnull);

  *aCount = j;
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
nsFontEnumeratorOS2::UpdateFontList(PRBool *updateFontList)
{
  *updateFontList = PR_FALSE; // always return false for now
  NS_ASSERTION( 0, "UpdateFontList is not implemented" );
  return NS_OK;
}

