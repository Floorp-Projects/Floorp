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
#include "nsICharsetConverterManager.h"
#include "nsICharsetConverterManager2.h"
#include "nsICharRepresentable.h"
#include "nsFontMetricsOS2.h"
#include "nsQuickSort.h"
#include "nsTextFormatter.h"
#include "prmem.h"
#include "plhash.h"
#include "prprf.h"
#include "nsReadableUtils.h"

#ifdef MOZ_MATHML
  #include <math.h>
#endif

#undef USER_DEFINED
#define USER_DEFINED "x-user-def"

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
  USHORT   mMask;
  PRUint16 mCodePage;
  char*    mLangGroup;
  PRUint32* mMap;
};

static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);
static NS_DEFINE_IID(kIFontMetricsIID, NS_IFONT_METRICS_IID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

nsGlobalFont* nsFontMetricsOS2::gGlobalFonts = nsnull;
int nsFontMetricsOS2::gGlobalFontsCount = 0;
PRBool nsFontMetricsOS2::gSubstituteVectorFonts = PR_TRUE;

PLHashTable* nsFontMetricsOS2::gFamilyNames = nsnull;

#define NS_MAX_FONT_WEIGHT 900
#define NS_MIN_FONT_WEIGHT 100

#undef CHAR_BUFFER_SIZE
#define CHAR_BUFFER_SIZE 1024

nscoord nsFontMetricsOS2::gDPI = 0;
long    nsFontMetricsOS2::gSystemRes = 0;

static nsICharsetConverterManager2* gCharSetManager = nsnull;
static nsIPref* gPref = nsnull;
static nsIUnicodeEncoder* gUserDefinedConverter = nsnull;

static nsIAtom* gUserDefined = nsnull;

static int gFontMetricsOS2Count = 0;
static int gInitialized = 0;

static PRUint32 gUserDefinedMap[2048];

static nsCharSetInfo gCharSetInfo[eCharSet_COUNT] =
{
  { "DEFAULT",     0,                0,    "" },
  { "ANSI",        FM_DEFN_LATIN1,   1252, "x-western" },
  { "EASTEUROPE",  FM_DEFN_LATIN2,   1250, "x-central-euro" },
  { "RUSSIAN",     FM_DEFN_CYRILLIC, 1251, "x-cyrillic" },
  { "GREEK",       FM_DEFN_GREEK,    813, "el" },
  { "TURKISH",     0,                1254, "tr" },
  { "HEBREW",      FM_DEFN_HEBREW,   862, "he" },
  { "ARABIC",      FM_DEFN_ARABIC,   864, "ar" },
  { "BALTIC",      0,                1257, "x-baltic" },
  { "THAI",        FM_DEFN_THAI,     874,  "th" },
  { "SHIFTJIS",    FM_DEFN_KANA,     932,  "ja" },
  { "GB2312",      FM_DEFN_KANA,     1381, "zh-CN" },
  { "HANGEUL",     FM_DEFN_KANA,     949,  "ko" },
  { "CHINESEBIG5", FM_DEFN_KANA,     950,  "zh-TW" },
  { "JOHAB",       FM_DEFN_KANA,     1361, "ko-XXX", }
};

static ULONG ulSystemCodePage = 0;

static ULONG curHashValue = 1;

// font handle
nsFontHandleOS2::nsFontHandleOS2()
{
   memset( &fattrs, 0, sizeof fattrs);
   fattrs.usRecordLength = sizeof fattrs;
   charbox.cx = charbox.cy = 0;
   ulHashMe = curHashValue;
   curHashValue++;
}

void nsFontHandleOS2::SelectIntoPS( HPS hps, long lcid)
{
   GFX (::GpiSetCharSet (hps, lcid), FALSE);
   GFX (::GpiSetCharBox (hps, &charbox), FALSE);
}

int PR_CALLBACK
prefChanged(const char *aPref, void *aClosure)
{
  nsresult rv;

  if( PL_strcmp(aPref, "browser.display.screen_resolution") == 0 )
  {
    PRInt32 dpi;
    rv = gPref->GetIntPref( aPref, &dpi );
    if( NS_SUCCEEDED(rv) )
       nsFontMetricsOS2::gDPI = dpi;
  }

  return 0;
}

static void
FreeGlobals(void)
{
  // XXX complete this

  gInitialized = 0;

  gPref->UnregisterCallback( "browser.display.screen_resolution", prefChanged, NULL );

  NS_IF_RELEASE(gCharSetManager);
  NS_IF_RELEASE(gPref);
  NS_IF_RELEASE(gUserDefined);
  NS_IF_RELEASE(gUserDefinedConverter);
}

static nsresult
InitGlobals(void)
{
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

  ULONG numCP = ::WinQueryCpList((HAB)0, 0, NULL);
  UniChar codepage[20];
  if (numCP > 0) {
     ULONG * pCPList = (ULONG*)malloc(numCP*sizeof(ULONG));
     if (::WinQueryCpList( (HAB)0, numCP, pCPList)) {
        for (int i = 0;i<numCP ;i++ ) {
           if (pCPList[i] == 1386) {
              int unirc = ::UniMapCpToUcsCp( 1386, codepage, 20);
              if (unirc == ULS_SUCCESS) {
                 UconvObject Converter = 0;
                 int unirc = ::UniCreateUconvObject( codepage, &Converter);
                 if (unirc == ULS_SUCCESS) {
                    gCharSetInfo[11].mCodePage = 1386;
                   ::UniFreeUconvObject(Converter);
                 }
              }
           } else if (pCPList[i] == 943) {
              int unirc = ::UniMapCpToUcsCp( 943, codepage, 20);
              if (unirc == ULS_SUCCESS) {
                 UconvObject Converter = 0;
                 int unirc = ::UniCreateUconvObject( codepage, &Converter);
                 if (unirc == ULS_SUCCESS) {
                    gCharSetInfo[11].mCodePage = 943;
                   ::UniFreeUconvObject(Converter);
                 }
              }
           }
        }
     }
     free(pCPList);
  }

  ulSystemCodePage = WinQueryCp(HMQ_CURRENT);

   // find system screen resolution. used here and in RealizeFont
  HPS ps = ::WinGetScreenPS(HWND_DESKTOP);
  HDC hdc = GFX (::GpiQueryDevice (ps), HDC_ERROR);
  GFX (::DevQueryCaps(hdc, CAPS_HORIZONTAL_FONT_RES, 1, &nsFontMetricsOS2::gSystemRes), FALSE);
  ::WinReleasePS(ps);

  if( !nsFontMetricsOS2::gGlobalFonts )
    if( !nsFontMetricsOS2::InitializeGlobalFonts() )
      return NS_ERROR_OUT_OF_MEMORY;

  // Set prefVal the value of the pref "browser.display.screen_resolution"
  // When a new profile is created, the pref is set to 0.  This tells the code
  // to default to font resolution of the screen (96 or 120)
  nsresult res;
  PRInt32 prefVal = -1;

  res = gPref->GetIntPref( "browser.display.screen_resolution", &prefVal );
  if (NS_FAILED(res))
    prefVal = 0;

  gPref->RegisterCallback( "browser.display.screen_resolution", prefChanged, NULL );

  if( prefVal == 0 )
  {
    prefVal = nsFontMetricsOS2::gSystemRes;
    gPref->SetIntPref( "browser.display.screen_resolution", prefVal );
  }

  nsFontMetricsOS2::gDPI = prefVal;

  gInitialized = 1;

  return NS_OK;
}

nsFontMetricsOS2::nsFontMetricsOS2()
{
  NS_INIT_REFCNT();
  mSpaceWidth = 0;
  ++gFontMetricsOS2Count;
  // members are zeroed by new operator (hmm) - yeah right
  mTriedAllGenerics = 0;
}

nsFontMetricsOS2::~nsFontMetricsOS2()
{
   Destroy();

   delete mFont;
   delete mFontHandle;
  if (0 == --gFontMetricsOS2Count) {
    FreeGlobals();
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
    nsresult rv;

    nsCOMPtr<nsIPref> prefs(do_GetService(kPrefCID, &rv));
    if (NS_SUCCEEDED(rv) && prefs)
    {
      res = prefs->GetBoolPref("font.substitute_vector_fonts", &gSubstituteVectorFonts);
      NS_ASSERTION( NS_SUCCEEDED(rv), "Could not get pref 'font.substitute_vector_fonts'" );
    }
  }

   mFont = new nsFont(aFont);
   mLangGroup = aLangGroup;
   //   mIndexOfSubstituteFont = -1;
   mDeviceContext = (nsDeviceContextOS2 *) aContext;
   nsresult rv = RealizeFont();
   return rv;
}

nsresult nsFontMetricsOS2::Destroy()
{
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
static PFONTMETRICS getMetrics( long &lFonts, PCSZ facename, HPS hps)
{
   LONG lWant = 0;
   lFonts = GFX (::GpiQueryFonts (hps, QF_PUBLIC | QF_PRIVATE,
                                  facename, &lWant, 0, 0),
                 GPI_ALTERROR);
   PFONTMETRICS pMetrics = new FONTMETRICS [ lFonts];

   GFX (::GpiQueryFonts (hps, QF_PUBLIC | QF_PRIVATE, facename,
                         &lFonts, sizeof (FONTMETRICS), pMetrics),
        GPI_ALTERROR);

   return pMetrics;
}

// aName is a font family name.  see if fonts of that family exist
//  if so, return font structure with family name
FATTRS*
nsFontMetricsOS2::LoadFont(HPS aPS, nsString* aName, BOOL bBold, BOOL bItalic)
{
  FATTRS*  font = nsnull;

  char familyname[FACESIZE];
  char alias[FACESIZE];
  WideCharToMultiByte(0, aName->get(), aName->Length() + 1,
    familyname, sizeof(familyname));

  USHORT flags = bBold ? FM_SEL_BOLD : 0;
  flags |= bItalic ? FM_SEL_ITALIC : 0;

   // always pass vector fonts to the printer
  if( !mDeviceContext->SupportsRasterFonts() &&
      GetVectorSubstitute( aPS, familyname, bBold, bItalic, alias ))
  {
    font = new FATTRS;
    memset( font, 0, sizeof(FATTRS) );
    font->usRecordLength = sizeof(FATTRS);
    PL_strcpy( font->szFacename, alias );
    font->fsFontUse = FATTR_FONTUSE_OUTLINE | FATTR_FONTUSE_TRANSFORMABLE;
    return font;
  }

  int i = 0;
  while( i < nsFontMetricsOS2::gGlobalFontsCount )
  {
    if( PL_strcasecmp( gGlobalFonts[i].fontMetrics.szFamilyname, familyname ) == 0 )
    {
      font = new FATTRS;
      memset( font, 0, sizeof(FATTRS) );
      font->usRecordLength = sizeof(FATTRS);

      for( int j = i; j < gGlobalFonts[i].nextFamily; j++ )
      {
        FONTMETRICS *fm = &gGlobalFonts[j].fontMetrics;
        if( (fm->fsSelection & (FM_SEL_ITALIC | FM_SEL_BOLD)) == flags )
        {
          PL_strcpy( font->szFacename, fm->szFacename );
          if( fm->fsDefn & FM_DEFN_OUTLINE ||
              !mDeviceContext->SupportsRasterFonts() )
            font->fsFontUse = FATTR_FONTUSE_OUTLINE | FATTR_FONTUSE_TRANSFORMABLE;
          else
            font->fsFontUse = 0;
          if( fm->fsType & FM_TYPE_MBCS )
            font->fsType |= FATTR_TYPE_MBCS;
          if( fm->fsType & FM_TYPE_DBCS )
            font->fsType |= FATTR_TYPE_DBCS;
          return font;
        }
      }

       // A font of family "familyname" and with the applied effects (bold &
       // italic) was not found.  Therefore, just look for a 'regular' font
       // (that is not italic and not bold), and then have the
       // system simulate the appropriate effects (see RealizeFont()).
      for( int m = i; m < gGlobalFonts[i].nextFamily; m++ )
      {
        FONTMETRICS *fm = &gGlobalFonts[m].fontMetrics;
        if( !(fm->fsSelection & (FM_SEL_ITALIC | FM_SEL_BOLD)) )
        {
          PL_strcpy( font->szFacename, fm->szFacename );
          if( fm->fsDefn & FM_DEFN_OUTLINE ||
              !mDeviceContext->SupportsRasterFonts() )
            font->fsFontUse = FATTR_FONTUSE_OUTLINE | FATTR_FONTUSE_TRANSFORMABLE;
          else
            font->fsFontUse = 0;
          if( fm->fsType & FM_TYPE_MBCS )
            font->fsType |= FATTR_TYPE_MBCS;
          if( fm->fsType & FM_TYPE_DBCS )
            font->fsType |= FATTR_TYPE_DBCS;
           // fake the effects
          if( bBold )
            font->fsSelection |= FATTR_SEL_BOLD;
          if( bItalic )
            font->fsSelection |= FATTR_SEL_ITALIC;
          return font;
        }
      }
    }

    i = gGlobalFonts[i].nextFamily;
  }   // end while

   // If a font was not found, then maybe "familyname" is really a face name.
   // See if a font with that facename exists on system and fake any applied
   // effects
  long lFonts = 0;
  PFONTMETRICS pMetrics = getMetrics( lFonts, familyname, aPS );

  if( lFonts > 0 )
  {
    font = new FATTRS;
    memset( font, 0, sizeof(FATTRS) );
    font->usRecordLength = sizeof(FATTRS);
    PL_strcpy( font->szFacename, familyname );
    font->fsFontUse = FATTR_FONTUSE_OUTLINE | FATTR_FONTUSE_TRANSFORMABLE;
    if( bBold )
      font->fsSelection |= FATTR_SEL_BOLD;
    if( bItalic )
      font->fsSelection |= FATTR_SEL_ITALIC;

    if( mDeviceContext->SupportsRasterFonts() )
    {
      for (int i = 0 ; i < lFonts ; i++)
        if( !(pMetrics[i].fsDefn & FM_DEFN_OUTLINE) )
        {
          font->fsFontUse = 0;
          break;
        }
    }
  }
  
  delete [] pMetrics;

  return font;
}

FATTRS*
nsFontMetricsOS2::FindUserDefinedFont(HPS aPS, BOOL bBold, BOOL bItalic)
{
  if (mIsUserDefined) {
    FATTRS* font = LoadFont(aPS, &mUserDefined, bBold, bItalic);
#ifdef XP_OS2
    if (font) {
#else
    if (font && FONT_HAS_GLYPH(font->mMap, aChar)) {
#endif
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
  { "times new roman", "Times New Roman" },
  { "arial",           "Arial" },
  { "courier",         "Courier" },
  { "courier new",     "Courier New" },
  { "warpsans",        "WarpSans Combined" },

  { nsnull, nsnull }
};

PLHashTable*
nsFontMetricsOS2::InitializeFamilyNames(void)
{
  static int gInitializedFamilyNames = 0;
  if (!gInitializedFamilyNames) {
    gInitializedFamilyNames = 1;
    gFamilyNames = PL_NewHashTable(0, HashKey, CompareKeys, nsnull, nsnull,
      nsnull);
    if (!gFamilyNames) {
      return nsnull;
    }
    nsFontFamilyName* f;
    if (!IsDBCS()) {
      f = gFamilyNameTable;
    } else {
      f = gFamilyNameTableDBCS;
    } /* endif */
    while (f->mName) {
      nsString* name = new nsString;
      nsString* winName = new nsString;
      if (name && winName) {
        name->AssignWithConversion(f->mName);
        winName->AssignWithConversion(f->mWinName);
        PL_HashTableAdd(gFamilyNames, name, (void*) winName);
      }
      f++;
    }
  }

  return gFamilyNames;
}

FATTRS*
nsFontMetricsOS2::FindLocalFont(HPS aPS, BOOL bBold, BOOL bItalic)
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
    FATTRS* font = LoadFont(aPS, winName, bBold, bItalic);
#ifdef XP_OS2
    if (font) {
#else
    if (font && FONT_HAS_GLYPH(font->mMap, aChar)) {
#endif
      return font;
    }
  }

  return nsnull;
}

FATTRS*
nsFontMetricsOS2::LoadGenericFont(HPS aPS, BOOL bBold, BOOL bItalic, char** aName)
{
  if (*aName) {
    int found = 0;
    int i;
    PRUnichar name[FACESIZE] = { 0 };
    PRUnichar format[] = { '%', 's', 0 };
    PRUint32 n = nsTextFormatter::snprintf(name, FACESIZE, format, *aName);
    nsMemory::Free(*aName);
    *aName = nsnull;
    if (n && (n != (PRUint32) -1)) {
      nsAutoString  fontName(name);

      FATTRS* font = LoadFont(aPS, &fontName, bBold, bItalic);
#ifdef XP_OS2
      if (font) {
#else
      if (font && FONT_HAS_GLYPH(font->mMap, aChar)) {
#endif
        return font;
      }
    }
  }

  return nsnull;
}

typedef struct PrefEnumInfo
{
  PRUnichar         mChar;
  HPS               mPS;
  FATTRS*        mFont;
  nsFontMetricsOS2* mMetrics;
} PrefEnumInfo;

void
PrefEnumCallback(const char* aName, void* aClosure)
{
  PrefEnumInfo* info = (PrefEnumInfo*) aClosure;
  if (info->mFont) {
    return;
  }
  HPS ps = info->mPS;
  nsFontMetricsOS2* metrics = info->mMetrics;
  char* value = nsnull;
  gPref->CopyCharPref(aName, &value);
  FATTRS* font = metrics->LoadGenericFont(ps, PR_FALSE, PR_FALSE, &value);
  if (font) {
    info->mFont = font;
  }
  else {
    gPref->CopyDefaultCharPref(aName, &value);
    font = metrics->LoadGenericFont(ps, PR_FALSE, PR_FALSE, &value);
    if (font) {
      info->mFont = font;
    }
  }
}

FATTRS*
nsFontMetricsOS2::FindGenericFont(HDC aPS, BOOL bBold, BOOL bItalic)
{
  if (mTriedAllGenerics) {
    return nsnull;
  }
  nsCAutoString prefix(NS_LITERAL_CSTRING("font.name.") +
                       NS_LossyConvertUCS2toASCII(*mGeneric));
  if (mLangGroup) {
    nsCAutoString pref(prefix + NS_LITERAL_CSTRING("."));
    const PRUnichar* langGroup = nsnull;
    mLangGroup->GetUnicode(&langGroup);
    pref.Append(NS_LossyConvertUCS2toASCII(langGroup));
    char* value = nsnull;
    gPref->CopyCharPref(pref.get(), &value);
    FATTRS* font = LoadGenericFont(aPS, bBold, bItalic, &value);
    if (font) {
      return font;
    }
    gPref->CopyDefaultCharPref(pref.get(), &value);
    font = LoadGenericFont(aPS, bBold, bItalic, &value);
    if (font) {
      return font;
    }
  }
  PrefEnumInfo info = { nsnull, aPS, nsnull, this };
  gPref->EnumerateChildren(prefix.get(), PrefEnumCallback, &info);
  if (info.mFont) {
    return info.mFont;
  }
#if 0
  PrefEnumInfo info = { aChar, aPS, nsnull, this };
  gPref->EnumerateChildren(prefix.get(), PrefEnumCallback, &info);
  if (info.mFont) {
    return info.mFont;
  }
#endif
  mTriedAllGenerics = 1;

  return nsnull;
}

// returns family name of font that can display given char
FATTRS*
nsFontMetricsOS2::FindFont(HPS aPS, BOOL bBold, BOOL bItalic)
{
  FATTRS* font = FindUserDefinedFont(aPS, bBold, bItalic);
  if (!font) {
    font = FindLocalFont(aPS, bBold, bItalic);
    if (!font) {
      font = FindGenericFont(aPS, bBold, bItalic);
      if (!font) {
        font = FindGlobalFont(aPS, bBold, bItalic);
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
    metrics->mGeneric = metrics->mFonts.StringAt(metrics->mFonts.Count() - 1);
    metrics->mGeneric->ToLowerCase();
    return PR_FALSE; // stop
  }

  return PR_TRUE; // don't stop
}

PRBool
nsFontMetricsOS2::GetVectorSubstitute( HPS aPS, const char* aFamilyname,
                          PRBool aIsBold, PRBool aIsItalic, char* alias )
{
  if( PL_strcasecmp( aFamilyname, "Tms Rmn" ) == 0 )
  {
    if( !aIsBold && !aIsItalic )
      PL_strcpy( alias, "Times New Roman" );
    else if( aIsBold )
      if( !aIsItalic )
        PL_strcpy( alias, "Times New Roman Bold" );
      else
        PL_strcpy( alias, "Times New Roman Bold Italic" );
    else
      PL_strcpy( alias, "Times New Roman Italic" );

    return PR_TRUE;
  }

  if( PL_strcasecmp( aFamilyname, "Helv" ) == 0 )
  {
    if( !aIsBold && !aIsItalic )
      PL_strcpy( alias, "Helvetica" );
    else if( aIsBold )
      if( !aIsItalic )
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
      if( !aIsBold && !aIsItalic )
        PL_strcpy( alias, "Helvetica" );
      else if( aIsBold )
        if( !aIsItalic )
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
      if( !aIsBold && !aIsItalic )
        PL_strcpy( alias, "Courier" );
      else if( aIsBold )
        if( !aIsItalic )
          PL_strcpy( alias, "Courier Bold" );
        else
          PL_strcpy( alias, "Courier Bold Italic" );
      else
        PL_strcpy( alias, "Courier Italic" );

      return PR_TRUE;
    }
  }
  
   // Generic case:  see if this family has a vector font matching the
   // given style
  USHORT flags = aIsBold ? FM_SEL_BOLD : 0;
  flags |= aIsItalic ? FM_SEL_ITALIC : 0;

  long lFonts = 0;
  PFONTMETRICS pMetrics = getMetrics( lFonts, aFamilyname, aPS );

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
  
  delete [] pMetrics;

  return PR_FALSE;
}

nsresult nsFontMetricsOS2::RealizeFont()
{
  nsresult  res;
  HWND      win = NULL;
  HDC       ps = NULL;
  float     fHeight;
  SIZEF     charbox;

  if (NULL != mDeviceContext->mPrintDC){
    ps = mDeviceContext->mPrintPS;
  } else {
    win = (HWND)mDeviceContext->mWidget;
    ps = ::WinGetPS(win);
    if (!ps) {
      ps = ::WinGetPS(HWND_DESKTOP);
    } /* endif */
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

//#ifndef XP_OS2
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
//#endif

  nsFontHandleOS2 *fh = new nsFontHandleOS2;
  if (!fh)
    return NS_ERROR_OUT_OF_MEMORY;

  BOOL bBold = mFont->weight > NS_FONT_WEIGHT_NORMAL;
  BOOL bItalic = !!(mFont->style & (NS_FONT_STYLE_ITALIC | NS_FONT_STYLE_OBLIQUE));

  FATTRS* fattrs = FindFont(ps, bBold, bItalic);

   // 5) Add effects
  if( mFont->decorations & NS_FONT_DECORATION_UNDERLINE)
    fattrs->fsSelection |= FATTR_SEL_UNDERSCORE;
  if( mFont->decorations & NS_FONT_DECORATION_LINE_THROUGH)
    fattrs->fsSelection |= FATTR_SEL_STRIKEOUT;

   // 6) Encoding
   // There doesn't seem to be any encoding stuff yet, so guess.
   // (XXX unicode hack; use same codepage as converter!)
  const PRUnichar* langGroup = nsnull;
  mLangGroup->GetUnicode(&langGroup);
  nsCAutoString name(NS_LossyConvertUCS2toASCII(langGroup).get());
  for (int j=0; j < eCharSet_COUNT; j++ )
  {
    if (name.get()[0] == gCharSetInfo[j].mLangGroup[0])
    {
      if (!strcmp(name.get(), gCharSetInfo[j].mLangGroup))
      {
        fattrs->usCodePage = gCharSetInfo[j].mCodePage;
        mCodePage = gCharSetInfo[j].mCodePage;
        break;
      }
    }
  }
//   fh->fattrs.usCodePage = gGfxModuleData.ulCodepage;

   // 7) Find the point size for the font, and set up the charbox too
  float app2dev, app2twip, twip2dev;
  float textZoom = 1.0;
  mDeviceContext->GetTextZoom( textZoom );
  mDeviceContext->GetAppUnitsToDevUnits( app2dev );
  mDeviceContext->GetDevUnitsToTwips( app2twip );
  mDeviceContext->GetTwipsToDevUnits( twip2dev );

   // !! Windows wants to mply up here.  I don't think I do.  If fonts
   // !! ever begin to look `squished', try enabling the following code
#if 1
   float scale;
   mDeviceContext->GetCanonicalPixelScale(scale);
   app2twip *= app2dev * scale;
#else
   app2twip *= app2dev;
#endif

   // Note: are you confused by the block above, and thinking that app2twip
   //       must be 1?  Well, there's *no* guarantee that app units are
   //       twips, despite whatever nscoord.h says!

   // 8) If we're using an image font, load the font with the closest
   //    matching size
  int points = NSTwipsToIntPoints( mFont->size * textZoom );
  if( fattrs->fsFontUse == 0 )    /* if image font */
  {
    char alias[FACESIZE];

     // If the font.substitute_vector_fonts pref is set, then we exchange
     // Times New Roman for Tms Rmn and Helvetica for Helv if the requested
     // points size is less than the minimum or more than the maximum point
     // size available for Tms Rmn and Helv.
    if( gSubstituteVectorFonts &&
        (points > 18 || points < 8 || (gDPI != 96 && gDPI != 120)) &&
        GetVectorSubstitute( ps, fattrs->szFacename, bBold, bItalic, alias) )
    {
      PL_strcpy( fattrs->szFacename, alias );
      fattrs->fsFontUse = FATTR_FONTUSE_OUTLINE | FATTR_FONTUSE_TRANSFORMABLE;
      fattrs->fsSelection &= ~(FM_SEL_BOLD | FM_SEL_ITALIC);
    }

     // If still using an image font
    if( fattrs->fsFontUse == 0 )
    {
      long lFonts = 0;
      PFONTMETRICS pMetrics = getMetrics( lFonts, fattrs->szFacename, ps);

      nscoord tempDPI;
      /* No vector substitute and we still have odd DPI - fix it */
      if (gDPI <= 96 ) {
         tempDPI = 96;
      } else {
         tempDPI = 120;
      }

      int curPoints = 0;
      for( int i = 0; i < lFonts; i++)
      {
        if( pMetrics[i].sYDeviceRes == tempDPI )
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
      delete [] pMetrics;
    }
  }

   // set up the charbox;  set for image fonts also, in case we need to
   //  substitute a font later on (for UTF-8, etc)
  if( !mDeviceContext->mPrintDC ) /* if not printing */
    if( fattrs->fsFontUse == 0 )    /* if image font */
      fHeight = points * gDPI / 72;
    else
      fHeight = mFont->size * app2dev * textZoom * gDPI / nsFontMetricsOS2::gSystemRes;
  else
    fHeight = mFont->size * app2dev * textZoom;

  long lFloor = NSToIntFloor( fHeight ); 
  fh->charbox.cx = MAKEFIXED( lFloor, (fHeight - (float)lFloor) * 65536.0f );
  fh->charbox.cy = fh->charbox.cx;

   // 9) Record font handle & record various font metrics to cache
  fh->fattrs = *fattrs;
  mFontHandle = fh;
  GFX (::GpiCreateLogFont (ps, 0, 1, fattrs), GPI_ERROR);
  fh->SelectIntoPS( ps, 1 );

  FONTMETRICS fm;
  GFX (::GpiQueryFontMetrics (ps, sizeof (fm), &fm), FALSE);

  float dev2app;
  mDeviceContext->GetDevUnitsToAppUnits( dev2app);
  
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
  ::GetTextExtentPoint32(ps, " ", 1, &size);
  mSpaceWidth = NSToCoordRound(float(size.cx) * dev2app);

   // 10) Clean up
  GFX (::GpiSetCharSet (ps, LCID_DEFAULT), FALSE);
  GFX (::GpiDeleteSetId (ps, 1), FALSE);
  if (NULL == mDeviceContext->mPrintDC)
    ::WinReleasePS(ps);
  delete fattrs;

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
   aFont = mFont;
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

static int PR_CALLBACK
CompareFontNames(const void* aArg1, const void* aArg2, void* aClosure)
{
  const nsString str1( *((const PRUnichar**)aArg1) );
  const nsString str2( *((const PRUnichar**)aArg2) );

   // intermingle vertical fonts (start with '@') with horizontal fonts
  if( str1.EqualsWithConversion( "@", PR_FALSE, 1 ))
  {
    if( str2.EqualsWithConversion( "@", PR_FALSE, 1 ))
      return str1.CompareWithConversion( str2 );
    else
    {
      nsString temp( str1 );
      temp.Trim( "@", PR_TRUE, PR_FALSE );
      int rv = temp.CompareWithConversion( str2 );
      if( rv == 0 )
        return 1;
      else
        return rv;
    }
  }
  else if( str2.EqualsWithConversion( "@", PR_FALSE, 1 ))
  {
    nsString temp( str2 );
    temp.Trim( "@", PR_TRUE, PR_FALSE );
    int rv = str1.CompareWithConversion( temp );
    if( rv == 0 )
      return -1;
    else
      return rv;
  }
  else
    return str1.CompareWithConversion( str2 );
}

static int PR_CALLBACK
CompareFontFamilyNames(const void* aArg1, const void* aArg2, void* aClosure)
{
  const nsGlobalFont* font1 = (const nsGlobalFont*) aArg1;
  const nsGlobalFont* font2 = (const nsGlobalFont*) aArg2;

   // put vertical fonts at end of global font list
  if( font1->name->EqualsWithConversion( "@", PR_FALSE, 1 ))
  {
    if( font2->name->EqualsWithConversion( "@", PR_FALSE, 1 ))
      return font1->name->CompareWithConversion( *(font2->name) );
    else
      return 1;
  }
  else if( font2->name->EqualsWithConversion( "@", PR_FALSE, 1 ))
     return -1;
  else
    return font1->name->CompareWithConversion( *(font2->name) );
}

nsGlobalFont*
nsFontMetricsOS2::InitializeGlobalFonts()
{
  HPS aPS = ::WinGetScreenPS(HWND_DESKTOP);

  LONG lRemFonts = 0, lNumFonts;
  lNumFonts = GFX (::GpiQueryFonts (aPS, QF_PUBLIC, NULL, &lRemFonts, 0, 0),
                   GPI_ALTERROR);
  PFONTMETRICS pFontMetrics = (PFONTMETRICS) nsMemory::Alloc(lNumFonts * sizeof(FONTMETRICS));
  lRemFonts = GFX (::GpiQueryFonts (aPS, QF_PUBLIC, NULL, &lNumFonts,
                                    sizeof (FONTMETRICS), pFontMetrics),
                   GPI_ALTERROR);

  gGlobalFonts = (nsGlobalFont*) nsMemory::Alloc( lNumFonts * sizeof(nsGlobalFont) );

  for (int i=0; i < lNumFonts; i++)
  {
     // The discrepencies between the Courier bitmap and outline fonts are
     // too much to deal with, so we only use the outline font
    if( !(pFontMetrics[i].fsDefn & FM_DEFN_OUTLINE) &&
        PL_strcmp(pFontMetrics[i].szFamilyname, "Courier") == 0 )
      continue;

    nsGlobalFont* font = &gGlobalFonts[gGlobalFontsCount];
    gGlobalFontsCount++;

    PRUnichar name[FACESIZE];
    name[0] = L'\0';

     // for DBCS fonts, display using facename rather than familyname
    int length;
    if( pFontMetrics[i].fsType & FM_TYPE_DBCS )
    {
      length = MultiByteToWideChar(0, pFontMetrics[i].szFacename,
         strlen(pFontMetrics[i].szFacename) + 1, name, sizeof(name)/sizeof(name[0]));
    }
    else
    {
      length = MultiByteToWideChar(0, pFontMetrics[i].szFamilyname,
         strlen(pFontMetrics[i].szFamilyname) + 1, name, sizeof(name)/sizeof(name[0]));
    }

    font->name = new nsString(name, length-1);
    if (!font->name)
    {
      gGlobalFontsCount--;
      continue;
    }

    font->fontMetrics = pFontMetrics[i];

     // Set the FM_SEL_BOLD flag in fsSelection.  This makes the check for
     // bold and italic much easier in LoadFont
    if( font->fontMetrics.usWeightClass > 5 )
      font->fontMetrics.fsSelection |= FM_SEL_BOLD;

    font->signature  = pFontMetrics[i].fsDefn;
  } /* for loop */

  nsMemory::Free(pFontMetrics);

  NS_QuickSort(gGlobalFonts, gGlobalFontsCount, sizeof(nsGlobalFont),
               CompareFontFamilyNames, nsnull);

  int prevIndex = 0;
  nsString lastName;

  for( int j = 0; j < gGlobalFontsCount; j++ )
  {
    if( !gGlobalFonts[j].name->EqualsWithConversion( lastName ) )
    {
      gGlobalFonts[prevIndex].nextFamily = j;
      prevIndex = j;
      lastName = *(gGlobalFonts[j].name);
    }
    else
      gGlobalFonts[j].nextFamily = 0;
  }
  gGlobalFonts[prevIndex].nextFamily = gGlobalFontsCount;

  ::WinReleasePS(aPS);
  
#ifdef DEBUG_pedemont
  for( int k = 0; k < gGlobalFontsCount; k++ )
  {
    FONTMETRICS* fm = &(gGlobalFonts[k].fontMetrics);
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
      
    printf( " : %d\n", fm->sNominalPointSize );
  }
#endif  

  return gGlobalFonts;
}

FATTRS*
nsFontMetricsOS2::FindGlobalFont(HPS aPS, BOOL bBold, BOOL bItalic)
{
  for (int i = 0; i < gGlobalFontsCount; i++) {
#ifndef XP_OS2
    if (!gGlobalFonts[i].skip) {
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
        if (!gGlobalFonts[i].map) {
          gGlobalFonts[i].skip = 1;
          continue;
        }
        if (SameAsPreviousMap(i)) {
          continue;
        }
      }
      if (FONT_HAS_GLYPH(gGlobalFonts[i].map, c)) {
        return LoadFont(aDC, gGlobalFonts[i].name);
      }
#else
        return LoadFont(aPS, gGlobalFonts[i].name, bBold, bItalic);
#endif
//    }
  }

  return nsnull;
}

static int
SignatureMatchesLangGroup(USHORT* aSignature,
  const char* aLangGroup)
{
   return 1;
  for (int i=0; i < eCharSet_COUNT; i++ ) {
     if (aLangGroup[0] == gCharSetInfo[i].mLangGroup[0]) {
        if (!strcmp(aLangGroup, gCharSetInfo[i].mLangGroup)) {
          if (*aSignature & gCharSetInfo[i].mMask) {
             return 1;
          } /* endif */
        } /* endif */
     } /* endif */
  } /* endfor */
  return 0;
}

static int
FontMatchesGenericType(nsGlobalFont* aFont, const char* aGeneric,
  const char* aLangGroup)
{
   return 1;
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

  return 0;
}

NS_IMETHODIMP
nsFontMetricsOS2::SetUnicodeFont( HPS aPS, LONG lcid )
{
  char fontPref[32];
  char* value = nsnull;
  char* generic = ToNewCString(*mGeneric);

  sprintf( fontPref, "font.name.%s.x-unicode", generic );
  nsMemory::Free( generic );

  gPref->CopyCharPref( fontPref, &value );
  nsAutoString fontName;
  fontName.AssignWithConversion( value );

  PRBool bBold = mFont->weight > NS_FONT_WEIGHT_NORMAL;
  PRBool bItalic = !!(mFont->style & (NS_FONT_STYLE_ITALIC | NS_FONT_STYLE_OBLIQUE));
  FATTRS* fattrs = LoadFont( aPS, &fontName, bBold, bItalic );

  nsMemory::Free( value );

   // if the font specified in prefs was not found, then fall back on
   // Times New Roman MT 30
  if( !fattrs && PL_strcmp( value, "Times New Roman MT 30" ) != 0 )
  {
    fontName.AssignWithConversion( "Times New Roman MT 30" );
    fattrs = LoadFont( aPS, &fontName, bBold, bItalic );
  }

   // if font in prefs not found, and backup font of Times New Roman MT 30
   // failed, then return error
  if( !fattrs )
    return NS_ERROR_FAILURE;

  fattrs->usCodePage = 1208;

   // Add effects
  if( mFont->decorations & NS_FONT_DECORATION_UNDERLINE )
    fattrs->fsSelection |= FATTR_SEL_UNDERSCORE;
  if( mFont->decorations & NS_FONT_DECORATION_LINE_THROUGH )
    fattrs->fsSelection |= FATTR_SEL_STRIKEOUT;

  if( fattrs->fsFontUse == 0 )    /* if image font */
  {
     // If we're using an image font, load the font with the closest
     //  matching size
    long lFonts = 0;
    PFONTMETRICS pMetrics = getMetrics( lFonts, fattrs->szFacename, aPS );

    float textZoom = 1.0;
    mDeviceContext->GetTextZoom( textZoom );
    int points = NSTwipsToIntPoints( mFont->size * textZoom );
    int curPoints = 0;
    for( int i = 0; i < lFonts; i++)
    {
      if( pMetrics[i].sYDeviceRes == gDPI )
      {
        if (pMetrics[i].sNominalPointSize / 10 == points)
        {
           // image face found fine, set required size in fattrs.
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

    delete [] pMetrics;
  }

  GFX (::GpiCreateLogFont (aPS, 0, lcid, fattrs), GPI_ERROR);

  delete fattrs;

  return NS_OK;
}


// The Font Enumerator

nsFontEnumeratorOS2::nsFontEnumeratorOS2()
{
  NS_INIT_REFCNT();
}

NS_IMPL_ISUPPORTS1(nsFontEnumeratorOS2, nsIFontEnumerator)

enum fontTypes
{
  FONT_TYPE_SERIF = 1,
  FONT_TYPE_SANS_SERIF = 2,
  FONT_TYPE_MONOSPACE = 3,
  FONT_TYPE_FANTASY = 4,
  FONT_TYPE_CURSIVE = 5
};


NS_IMETHODIMP
nsFontEnumeratorOS2::EnumerateFonts(const char* aLangGroup,
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

  PRUnichar** array = (PRUnichar**)
    nsMemory::Alloc(nsFontMetricsOS2::gGlobalFontsCount * sizeof(PRUnichar*));
  if (!array) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  int genericType;
  if( PL_strcmp(aGeneric, "serif") == 0 )
    genericType = FONT_TYPE_SERIF;
  else if( PL_strcmp(aGeneric, "sans-serif") == 0 )
    genericType = FONT_TYPE_SANS_SERIF;
  else if( PL_strcmp(aGeneric, "monospace") == 0 )
    genericType = FONT_TYPE_MONOSPACE;
  else if( PL_strcmp(aGeneric, "fantasy") == 0 )
    genericType = FONT_TYPE_FANTASY;
  else if( PL_strcmp(aGeneric, "cursive") == 0 )
    genericType = FONT_TYPE_CURSIVE;
  else
    NS_ASSERTION( 0, "Unknown font type given" );

  int count = 0;
  int i = 0;
  while( i < nsFontMetricsOS2::gGlobalFontsCount )
  {
    nsGlobalFont* font = &(nsFontMetricsOS2::gGlobalFonts[i]);
    FONTMETRICS* fm = &(font->fontMetrics);
    PRUnichar* str = ToNewUnicode(*(font->name));

    if (!str) {
      for (i = count - 1; i >= 0; i--) {
        nsMemory::Free(array[i]);
      }
      nsMemory::Free(array);
      return NS_ERROR_OUT_OF_MEMORY;
    }

    if( fm->fsType & FM_TYPE_DBCS ) /* DBCS */
    {
      if( genericType == FONT_TYPE_MONOSPACE || 
          genericType == FONT_TYPE_SANS_SERIF ||
          genericType == FONT_TYPE_SERIF )
        array[count++] = str;
    }
    else if( fm->fsType & FM_TYPE_FIXED ||   /* Monospace fonts */
        fm->panose.bProportion == 9   )
    {
      if( genericType == FONT_TYPE_MONOSPACE )
        array[count++] = str;
    }
    else if( fm->panose.bFamilyType == 2 )  /* Text and Display */
    {
      if( fm->panose.bSerifStyle > 10 )     /* sans-serif           */
      {                                     /* 11-15 of bSerifStyle */
        if( genericType == FONT_TYPE_SANS_SERIF )
          array[count++] = str;
      }
      else if( fm->panose.bSerifStyle < 2 ) /* if of type "Any" or "No Fit" */
      {                                     /* add to both                  */
        if( genericType == FONT_TYPE_SERIF ||
            genericType == FONT_TYPE_SANS_SERIF )
          array[count++] = str;
      }
      else if( genericType == FONT_TYPE_SERIF ) /* assume remaining fonts of */
        array[count++] = str;                   /* type 2 are serif          */

       // for utf8 purposes, add the font 'Times New Roman MT 30' for the
       // Sans-Serif & Monospace lists, since in most cases, this is the only
       // unicode font available on OS/2 systems
      if( (genericType == FONT_TYPE_SANS_SERIF ||
           genericType == FONT_TYPE_MONOSPACE)   &&
          PL_strcmp(&aLangGroup[2],"unicode") == 0 &&
          font->name->CompareWithConversion("Times New Roman MT 30") == 0 )
        array[count++] = str;
    }
    else if( fm->panose.bFamilyType == 3 )  /* Script */
    {
      if( genericType == FONT_TYPE_CURSIVE )
        array[count++] = str;
    }
    else if( fm->panose.bFamilyType > 3 )   /* Decorative, Pictorial */
    {
      if( genericType == FONT_TYPE_FANTASY )
        array[count++] = str;
    }
    else
    {
       // These common bitmap fonts do not have PANOSE info, so we hardcode
       // these checks here
      if( font->name->CompareWithConversion("Tms Rmn") == 0 )
      {
        if( genericType == FONT_TYPE_SERIF )
          array[count++] = str;
      }
      else if( font->name->CompareWithConversion("Helv") == 0 ||
               font->name->CompareWithConversion("System Proportional") == 0 ||
               font->name->CompareWithConversion("WarpSans") == 0 )
      {
        if( genericType == FONT_TYPE_SANS_SERIF )
          array[count++] = str;
      }
      else if( font->name->CompareWithConversion("Symbol Set") == 0 )
      {
        if( genericType == FONT_TYPE_FANTASY )
          array[count++] = str;
      }
      else
      {
         // Any font that has not set any of the above fields gets added to
         // every font menu in prefs
        array[count++] = str;
      }
    }

    i = font->nextFamily;
  }

  NS_QuickSort(array, count, sizeof(PRUnichar*), CompareFontNames, nsnull);

  *aCount = count;
  *aResult = array;

  return NS_OK;
}

NS_IMETHODIMP
nsFontEnumeratorOS2::EnumerateAllFonts(PRUint32* aCount, PRUnichar*** aResult)
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

  PRUnichar** array = (PRUnichar**)
    nsMemory::Alloc(nsFontMetricsOS2::gGlobalFontsCount * sizeof(PRUnichar*));
  if (!array) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  int i = 0;
  while( i < nsFontMetricsOS2::gGlobalFontsCount )
  {
    PRUnichar* str = ToNewUnicode(*nsFontMetricsOS2::gGlobalFonts[i].name);
    if (!str) {
      for (i = i - 1; i >= 0; i--) {
        nsMemory::Free(array[i]);
      }
      nsMemory::Free(array);
      return NS_ERROR_OUT_OF_MEMORY;
    }
    array[i] = str;

    i = nsFontMetricsOS2::gGlobalFonts[i].nextFamily;
  }

  NS_QuickSort(array, nsFontMetricsOS2::gGlobalFontsCount, sizeof(PRUnichar*),
    CompareFontNames, nsnull);

  *aCount = nsFontMetricsOS2::gGlobalFontsCount;
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

