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
#include "nsICollation.h"
#include "nsCollationCID.h"
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
static NS_DEFINE_IID(kIFontMetricsIID, NS_IFONT_METRICS_IID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kCollationFactoryCID, NS_COLLATIONFACTORY_CID);
static NS_DEFINE_CID(kLocaleServiceCID, NS_LOCALESERVICE_CID); 

#define NS_MAX_FONT_WEIGHT 900
#define NS_MIN_FONT_WEIGHT 100

#undef CHAR_BUFFER_SIZE
#define CHAR_BUFFER_SIZE 1024


/***** Global variables *****/
nsVoidArray  *nsFontMetricsOS2::gGlobalFonts = nsnull;
PRBool        nsFontMetricsOS2::gSubstituteVectorFonts = PR_TRUE;
PLHashTable  *nsFontMetricsOS2::gFamilyNames = nsnull;
int           nsFontMetricsOS2::gCachedIndex = 0;
nsICollation *nsFontMetricsOS2::gCollation = nsnull;

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
  memset( &mFattrs, 0, sizeof(mFattrs));
  mFattrs.usRecordLength = sizeof(mFattrs);
  mCharbox.cx = mCharbox.cy = 0;
  mHashMe = curHashValue;
  curHashValue++;
  mMaxAscent = 0;
  mMaxDescent = 0;
}

void
nsFontOS2::SelectIntoPS( HPS aPS, long aLcid )
{
   GFX (::GpiSetCharSet (aPS, aLcid), FALSE);
   GFX (::GpiSetCharBox (aPS, &mCharbox), FALSE);
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

  if (destLength > CHAR_BUFFER_SIZE)
    pstr = new char[destLength];
  else
    destLength = CHAR_BUFFER_SIZE;

#ifdef WINCODE
  PRInt32 srcLength = aLength, destLength = aLength;
  mConverter->Convert(aString, &srcLength, pstr, &destLength);
#else
  int convertedLength = WideCharToMultiByte( mFattrs.usCodePage, aString,
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
  
  if (destLength > CHAR_BUFFER_SIZE)
    pstr = new char[destLength];
  else
    destLength = CHAR_BUFFER_SIZE;

#ifdef WINCODE
  PRInt32 srcLength = aLength, destLength = aLength;
  mConverter->Convert(aString, &srcLength, pstr, &destLength);
#else
  int convertedLength = WideCharToMultiByte( mFattrs.usCodePage, aString,
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

static void
FreeGlobals(void)
{
  gInitialized = 0;

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
  NS_IF_RELEASE(nsFontMetricsOS2::gCollation);

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

  if( !nsFontMetricsOS2::gGlobalFonts )
  {
    nsresult res = nsFontMetricsOS2::InitializeGlobalFonts();
    if( NS_FAILED(res) )
    {
      FreeGlobals();
      return res;
    }
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

nsFontMetricsOS2::nsFontMetricsOS2()
{
  NS_INIT_ISUPPORTS();

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
  return (PLHashNumber) nsCRT::HashCode(str->get());
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
   if (!lFonts) {
      return NULL;
   }
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

  FATTRS* fattrs = &(aFont->mFattrs);
  if( fattrs->fsFontUse == 0 )  // if image font
  {
    char alias[FACESIZE];

     // If the font.substitute_vector_fonts pref is set, then we exchange
     // Times New Roman for Tms Rmn and Helvetica for Helv if the requested
     // points size is less than the minimum or more than the maximum point
     // size available for Tms Rmn and Helv.
    if( gSubstituteVectorFonts &&
        (points > 18 || points < 8) &&
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
      int browserRes = mDeviceContext->GetDPI();

      int curPoints = 0;
      for( int i = 0; i < lFonts; i++)
      {
        if( pMetrics[i].sYDeviceRes == browserRes )
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
      }

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

  /* if image font and not printing */
  if ((fattrs->fsFontUse == 0) && (!mDeviceContext->mPrintDC))
    fHeight = NSIntPointsToTwips(points) * app2dev;
  else
    fHeight = mFont.size * app2dev;

  long lFloor = NSToIntFloor( fHeight ); 
  aFont->mCharbox.cx = MAKEFIXED( lFloor, (fHeight - (float)lFloor) * 65536.0f );
  aFont->mCharbox.cy = aFont->mCharbox.cx;
}

/* aName is a font family name.  see if fonts of that family exist
 *  if so, return font structure with family name
 */
nsFontOS2*
nsFontMetricsOS2::LoadFont( HPS aPS, nsString* aFontname )
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
      GetVectorSubstitute( aPS, aFontname, alias ))
  {
    fh = new nsFontOS2();
    PL_strcpy( fh->mFattrs.szFacename, alias );
    fh->mFattrs.fsFontUse = FATTR_FONTUSE_OUTLINE | FATTR_FONTUSE_TRANSFORMABLE;
    SetFontHandle( aPS, fh );
    return fh;
  }
  
   /* rather than doing string compare all the way through the loop, create
    * a collation key out of the incoming fontname, and compare with that.  */
  nsAutoString tempname( *aFontname );
  PRUint8* key;
  PRUint32 keylen;
  if( aFontname->Find( "@", PR_FALSE, 0, 1 ) != kNotFound )
  {
    tempname.Trim( "@", PR_TRUE, PR_FALSE );  /* remove leading "@" */
    tempname.Append( NS_LITERAL_STRING(" ") );
  }
    
  PRInt32 res = gCollation->GetSortKeyLen(kCollationCaseInSensitive, 
                                 tempname, &keylen);

  if( NS_SUCCEEDED(res) )
  {
    key = (PRUint8*) nsMemory::Alloc(keylen * sizeof(PRUint8));
    res = gCollation->CreateRawSortKey( kCollationCaseInSensitive, 
                                       tempname, key,
                                       &keylen );
  }
  NS_ASSERTION( NS_SUCCEEDED(res), "Create collation keys failed!" );
 
  int i = gCachedIndex;
  int count = gGlobalFonts->Count();
  int limit = count + gCachedIndex;
  
  while( i < limit && !fh )
  {
    nsGlobalFont* font = (nsGlobalFont*)gGlobalFonts->ElementAt(i%count);

    gCollation->CompareRawSortKey(key, keylen, font->key, font->len, &res);
    if( res == 0 )
    {
       /* Save current index such that next time LoadFont() is called, we
        * start the loop at the last used index                           */
      gCachedIndex = i % count;
      int plainIndex = -1;
      
      for( int j = i%count; j < font->nextFamily; j++ )
      {
        nsGlobalFont* fontj = (nsGlobalFont*)gGlobalFonts->ElementAt(j);
        nsMiniFontMetrics *fm = &(fontj->metrics);
        if( (fm->fsSelection & (FM_SEL_ITALIC | FM_SEL_BOLD)) == flags )
        {
          fh = new nsFontOS2();
          FATTRS* fattrs = &(fh->mFattrs);
     
          PL_strcpy( fattrs->szFacename, fm->szFacename );
          if( fm->fsDefn & FM_DEFN_OUTLINE ||
              !mDeviceContext->SupportsRasterFonts() )
            fh->mFattrs.fsFontUse = FATTR_FONTUSE_OUTLINE | FATTR_FONTUSE_TRANSFORMABLE;
          if( fm->fsType & FM_TYPE_MBCS )
            fh->mFattrs.fsType |= FATTR_TYPE_MBCS;
          if( fm->fsType & FM_TYPE_DBCS )
            fh->mFattrs.fsType |= FATTR_TYPE_DBCS;
            
          SetFontHandle( aPS, fh );
          break;
        }

        /* Set index for 'plain' font (non-bold, non-italic) */
        if( plainIndex == -1 && !(fm->fsSelection & (FM_SEL_ITALIC | FM_SEL_BOLD)))
          plainIndex = j;
      }
 
       // A font of family "familyname" and with the applied effects (bold &
       // italic) was not found.  Therefore, just look for a 'regular' font
       // (that is not italic and not bold), and then have the
       // system simulate the appropriate effects (see RealizeFont()).
      if( !fh && plainIndex != -1 )
      {
        nsGlobalFont* fontm = (nsGlobalFont*)gGlobalFonts->ElementAt(plainIndex);
        nsMiniFontMetrics *fm = &(fontm->metrics);
          
        fh = new nsFontOS2();
        FATTRS* fattrs = &(fh->mFattrs);
   
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
      }
    }
 
    if( i < count )
      i = font->nextFamily;
    else
      i = font->nextFamily + count;
  }   // end while

  if( fh )
  {
    if( key )
      nsMemory::Free(key);
    return fh;
  }

   // If a font was not found, then maybe "familyname" is really a face name.
   // See if a font with that facename exists on system and fake any applied
   // effects
  long lFonts = 0;
  char fontname[FACESIZE];
  WideCharToMultiByte(0, aFontname->get(), aFontname->Length() + 1,
    fontname, FACESIZE);
  FONTMETRICS* pMetrics = getMetrics( lFonts, fontname, aPS );
 
  if( lFonts > 0 )
  {
    fh = new nsFontOS2();
    PL_strcpy( fh->mFattrs.szFacename, fontname );
    fh->mFattrs.fsFontUse = FATTR_FONTUSE_OUTLINE | FATTR_FONTUSE_TRANSFORMABLE;

    if( bBold )
      fh->mFattrs.fsSelection |= FATTR_SEL_BOLD;
    if( bItalic )
      fh->mFattrs.fsSelection |= FATTR_SEL_ITALIC;
 
    if( mDeviceContext->SupportsRasterFonts() )
    {
      for (int i = 0 ; i < lFonts ; i++)
        if( !(pMetrics[i].fsDefn & FM_DEFN_OUTLINE) )
        {
          fh->mFattrs.fsFontUse = 0;
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
nsFontMetricsOS2::GetVectorSubstitute( HPS aPS, nsString* aFamilyname,
                                       char* alias )
{
  char fontname[FACESIZE];
  WideCharToMultiByte(0, aFamilyname->get(), aFamilyname->Length() + 1,
    fontname, FACESIZE);

  return GetVectorSubstitute( aPS, fontname, alias );
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
  GFX (::GpiCreateLogFont (ps, 0, 1, &(font->mFattrs)), GPI_ERROR);
  font->SelectIntoPS( ps, 1 );

  FONTMETRICS fm;
  GFX (::GpiQueryFontMetrics (ps, sizeof (fm), &fm), FALSE);
  /* Due to a bug in OS/2 MINCHO, need to cast lInternalLeading */
  fm.lInternalLeading = (signed short)fm.lInternalLeading;

  float dev2app;
  mDeviceContext->GetDevUnitsToAppUnits(dev2app);
  
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

  mUnderlinePosition  = -NSToCoordRound( fm.lUnderscorePosition * dev2app );
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
#endif

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
    fh->mFattrs.usCodePage = 1208;
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
    context.mFont->mFattrs.usCodePage = 1208;
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
  nsFontOS2* fh = nsnull;
  
  if( mCodePage == 1252 )
  {
    while( running && firstChar < lastChar )
    {
      if(( *currChar > 0x00FF ) && (*currChar != 0x20AC))
      { 
        fh = GetUnicodeFont(aPS);
        NS_ASSERTION(fh, "GetUnicodeFont failed");
        if (!fh)
           fh = FindGlobalFont(aPS);
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
        fh = new nsFontOS2();
        *fh = *mFontHandle;
        fontSwitch.mFont = fh;
        while( ++currChar < lastChar )
        {
          if( (*currChar > 0x00FF) && (*currChar != 0x20AC) )
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
      if( (*currChar >= 0x0080 && *currChar <= 0x00FF) || (*currChar == 0x20AC)  )
      { 
        fh = new nsFontOS2();
        *fh = *mFontHandle;
        fh->mFattrs.usCodePage = 1252;
        fontSwitch.mFont = fh;
        while( ++currChar < lastChar )
        {
          if(( *currChar < 0x0080 || *currChar > 0x00FF) && (*currChar != 0x20AC) )
            break;
        }

        running = (*aFunc)(&fontSwitch, firstChar, currChar - firstChar, aData);
      }
      else
      {
         // Use currently selected font
        fh = new nsFontOS2();
        *fh = *mFontHandle;
        fontSwitch.mFont = fh;
        while( ++currChar < lastChar )
        {
          if( (*currChar >= 0x0080 && *currChar <= 0x00FF) || (*currChar == 0x20AC)  )
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
  nsFontOS2* fh = nsnull;
  
  if( mCodePage == 1252 )
  {
    while( running && firstChar > lastChar )
    {
      if(( *currChar > 0x00FF ) && (*currChar != 0x20AC))
      { 
        fh = GetUnicodeFont(aPS);
        NS_ASSERTION(fh, "GetUnicodeFont failed");
        if (!fh)
           fh = FindGlobalFont(aPS);
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
        fh = new nsFontOS2();
        *fh = *mFontHandle;
        fontSwitch.mFont = fh;
        while( --currChar > lastChar )
        {
          if(( *currChar > 0x00FF ) && (*currChar != 0x20AC))
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
      if( (*currChar >= 0x0080 && *currChar <= 0x00FF) || (*currChar == 0x20AC)  )
      { 
        fh = new nsFontOS2();
        *fh = *mFontHandle;
        fh->mFattrs.usCodePage = 1252;
        fontSwitch.mFont = fh;
        while( --currChar > lastChar )
        {
          if(( *currChar < 0x0080 || *currChar > 0x00FF) && (*currChar != 0x20AC) )
            break;
        }

        running = (*aFunc)(&fontSwitch, firstChar, currChar - firstChar, aData);
      }
      else
      {
         // Use currently selected font
        fh = new nsFontOS2();
        *fh = *mFontHandle;
        fontSwitch.mFont = fh;
        while( --currChar > lastChar )
        {
          if( (*currChar >= 0x0080 && *currChar <= 0x00FF) || (*currChar == 0x20AC)  )
            break;
        }

        running = (*aFunc)(&fontSwitch, firstChar, currChar - firstChar, aData);
      }
      
      firstChar = currChar;
    }
  }

  return NS_OK;
}

static nsresult
GetCollation(void)
{
  nsresult res = NS_OK;
  nsCOMPtr<nsILocale> locale = nsnull;
  nsICollationFactory * collationFactory = nsnull;
  
  nsCOMPtr<nsILocaleService> localeServ = 
           do_GetService(kLocaleServiceCID, &res);
  if (NS_FAILED(res)) return res;

  res = localeServ->GetApplicationLocale(getter_AddRefs(locale));
  if (NS_FAILED(res)) return res;

  res = nsComponentManager::CreateInstance(kCollationFactoryCID, NULL, 
      NS_GET_IID(nsICollationFactory), (void**) &collationFactory);
  if (NS_FAILED(res)) return res;

  res = collationFactory->CreateCollation(locale, &nsFontMetricsOS2::gCollation);
  NS_RELEASE(collationFactory);
  return res;
}

static int PR_CALLBACK
CompareFontFamilyNames(const void* aArg1, const void* aArg2, void* aData)
{
  PRInt32 res; 
  nsICollation * collation = (nsICollation *) aData;
  nsGlobalFont* font1 = (nsGlobalFont*) aArg1;
  nsGlobalFont* font2 = (nsGlobalFont*) aArg2;

  collation->CompareRawSortKey(font1->key, font1->len, font2->key, font2->len, &res);

  return res;
}
                                   
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


nsresult
nsFontMetricsOS2::InitializeGlobalFonts()
{
  gGlobalFonts = new nsVoidArray();

  HPS ps = ::WinGetScreenPS(HWND_DESKTOP);
  LONG lRemFonts = 0, lNumFonts;
  lNumFonts = GFX (::GpiQueryFonts (ps, QF_PUBLIC, NULL, &lRemFonts, 0, 0),
                   GPI_ALTERROR);
  FONTMETRICS* pFontMetrics = (PFONTMETRICS) nsMemory::Alloc(lNumFonts * sizeof(FONTMETRICS));
  lRemFonts = GFX (::GpiQueryFonts (ps, QF_PUBLIC, NULL, &lNumFonts,
                                    sizeof (FONTMETRICS), pFontMetrics),
                   GPI_ALTERROR);

  PRUnichar* str = (PRUnichar*)nsMemory::Alloc((FACESIZE+1) * sizeof(PRUnichar));

  PRInt32 res = GetCollation();
  NS_ASSERTION( NS_SUCCEEDED(res), "GetCollation failed!" );

  int convertedLength;
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

     // Set the FM_SEL_BOLD flag in fsSelection.  This makes the check for
     // bold and italic much easier in LoadFont
    if( pFontMetrics[i].usWeightClass > 5 )
      font->metrics.fsSelection |= FM_SEL_BOLD;

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
    char* fontptr = nsnull;
    if (PL_strcasestr(font->metrics.szFacename, "bold") != nsnull ||
        PL_strcasestr(font->metrics.szFacename, "italic") != nsnull ||
        PL_strcasestr(font->metrics.szFacename, "oblique") != nsnull ||
        PL_strcasestr(font->metrics.szFacename, "regular") != nsnull ||
        PL_strcasestr(font->metrics.szFacename, "-normal") != nsnull)
    {
      fontptr = font->metrics.szFamilyname;
    } else {
      fontptr = font->metrics.szFacename;
    }
    
     // The fonts in gBadDBCSFontMapping do not display well in non-Chinese
     //   systems.  Map them to a more intelligible name.
    if (font->metrics.fsType & FM_TYPE_DBCS)
    {
      if ((ulSystemCodePage != 1386) &&
          (ulSystemCodePage != 1381) &&
          (ulSystemCodePage != 950)) {
        int j=0;
        while (gBadDBCSFontMapping[j].mName) {
           if ((strcmp(fontptr, gBadDBCSFontMapping[j].mName) == 0)) {
              fontptr = gBadDBCSFontMapping[j].mWinName;
              break;
           }
           j++;
        }
      }
    }

     // For vertical DBCS fonts (those that start with '@'), we want them to
     //   appear directly below the non-vertical font of the same family.
    convertedLength = MultiByteToWideChar(0, fontptr, strlen(fontptr), str, FACESIZE);
    if (fontptr[0] == '@')
    {
      font->name.Assign( str + 1, convertedLength-1 );
      font->name.Append( NS_LITERAL_STRING(" ") );
    } else {
      font->name.Assign( str, convertedLength );
    }

     // Create collation sort key
    res = gCollation->GetSortKeyLen(kCollationCaseInSensitive, 
                                   font->name, &font->len);

    if (NS_SUCCEEDED(res))
    {
      font->key = (PRUint8*) nsMemory::Alloc(font->len * sizeof(PRUint8));
      res = gCollation->CreateRawSortKey( kCollationCaseInSensitive, 
                                         font->name, font->key,
                                         &font->len );

       // reset DBCS name to actual value
      if (fontptr[0] == '@')
        font->name.Insert( NS_LITERAL_STRING("@"), 0 );
    }
    
    gGlobalFonts->AppendElement(font);
  }

  gGlobalFonts->Sort(CompareFontFamilyNames, gCollation);

   // Set nextFamily in the array, such that we can skip from family to
   //   family in order to speed up searches.
  int prevIndex = 0;
  nsGlobalFont* font = (nsGlobalFont*)gGlobalFonts->ElementAt(0);
  PRUint8* lastkey = font->key;
  PRUint32 lastlen = font->len;

  int count = gGlobalFonts->Count(); 
  for( int j = 0; j < count; j++ )
  {
    font = (nsGlobalFont*)gGlobalFonts->ElementAt(j);
    
    gCollation->CompareRawSortKey(font->key, font->len, lastkey, lastlen, &res);
    if( res != 0 )
    {
      lastkey = font->key;
      lastlen = font->len;

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
    printf( " %3d: %32s :", k, fm->szFamilyname );
    printf( " %32s", fm->szFacename );
    
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

    if( fm->fsType & FM_TYPE_DBCS ||
        fm->fsType & FM_TYPE_MBCS )
      printf( " : M/DBCS" );
    else
      printf( " :       " );

    if( font->nextFamily )
      printf( " : %d", font->nextFamily );
      
    printf( "\n" );
  }
  fflush( stdout );
#endif

  nsMemory::Free(str);
  nsMemory::Free(pFontMetrics);
    
  return res;
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
  fh = LoadFont( aPS, &fontname );
  NS_ASSERTION(fh, "Couldn't load default font - BAD things are happening");
  return fh;
}


// The Font Enumerator

nsFontEnumeratorOS2::nsFontEnumeratorOS2()
{
  NS_INIT_ISUPPORTS();
}

NS_IMPL_ISUPPORTS1(nsFontEnumeratorOS2, nsIFontEnumerator)

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

  if( !nsFontMetricsOS2::gGlobalFonts )
  {
    nsresult res = nsFontMetricsOS2::InitializeGlobalFonts();
    if( NS_FAILED(res) )
    {
      FreeGlobals();
      return res;
    }
  }

  int count = nsFontMetricsOS2::gGlobalFonts->Count();
  PRUnichar** array = (PRUnichar**)nsMemory::Alloc(count * sizeof(PRUnichar*));
  NS_ENSURE_TRUE( array, NS_ERROR_OUT_OF_MEMORY );

  int i = 0;
  int j = 0;
  while( i < count )
  {
    nsGlobalFont* font = (nsGlobalFont*)nsFontMetricsOS2::gGlobalFonts->ElementAt(i);
    array[j++] = ToNewUnicode( font->name );
    i = font->nextFamily;
  }

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
  return NS_ERROR_NOT_IMPLEMENTED;
}

