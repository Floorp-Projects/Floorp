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
static int gGlobalFontsAlloc = 0;
int nsFontMetricsOS2::gGlobalFontsCount = 0;

PLHashTable* nsFontMetricsOS2::gFontMaps = nsnull;

PLHashTable* nsFontMetricsOS2::gFamilyNames = nsnull;

//-- Font weight
PLHashTable* nsFontMetricsOS2::gFontWeights = nsnull;

#define NS_MAX_FONT_WEIGHT 900
#define NS_MIN_FONT_WEIGHT 100

#undef CHAR_BUFFER_SIZE
#define CHAR_BUFFER_SIZE 1024

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
  { "HEBREW",      FM_DEFN_HEBREW,   1255, "he" },
  { "ARABIC",      FM_DEFN_ARABIC,   1256, "ar" },
  { "BALTIC",      0,                1257, "x-baltic" },
  { "THAI",        FM_DEFN_THAI,     874,  "th" },
  { "SHIFTJIS",    FM_DEFN_KANA,     932,  "ja" },
//  { "GB2312",      FM_DEFN_KANA,     936,  "zh-CN" },
  { "GB2312",      FM_DEFN_KANA,     1386,  "zh-CN" },
  { "HANGEUL",     FM_DEFN_KANA,     949,  "ko" },
  { "CHINESEBIG5", FM_DEFN_KANA,     950,  "zh-TW" },
  { "JOHAB",       FM_DEFN_KANA,     1361, "ko-XXX", }
};

// font handle
nsFontHandleOS2::nsFontHandleOS2()
{
   memset( &fattrs, 0, sizeof fattrs);
   fattrs.usRecordLength = sizeof fattrs;
   charbox.cx = charbox.cy = 0;
}

void nsFontHandleOS2::SelectIntoPS( HPS hps, long lcid)
{
   if( !GpiSetCharBox( hps, &charbox))
      PMERROR("GpiSetCharBox");
   if( !GpiSetCharSet( hps, lcid))
      PMERROR("GpiSetCharSet");
}

static void
FreeGlobals(void)
{
  // XXX complete this

  gInitialized = 0;

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

  gInitialized = 1;

  return NS_OK;
}

nsFontMetricsOS2::nsFontMetricsOS2()
{
   NS_INIT_REFCNT();
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

NS_IMPL_ISUPPORTS( nsFontMetricsOS2, nsIFontMetrics::GetIID())

nsresult nsFontMetricsOS2::Init( const nsFont &aFont,  nsIAtom* aLangGroup, nsIDeviceContext *aContext)
{
  nsresult res;
  if (!gInitialized) {
    res = InitGlobals();
    if (NS_FAILED(res)) {
      return res;
    }
  }
   mFont = new nsFont( aFont);
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
    nsCRT::HashCode(str->GetUnicode());
}

static PRIntn PR_CALLBACK
CompareKeys(const void* aStr1, const void* aStr2)
{
  return nsCRT::strcmp(((const nsString*) aStr1)->GetUnicode(),
    ((const nsString*) aStr2)->GetUnicode()) == 0;
}
nsFontOS2*
nsFontMetricsOS2::LoadFont(HPS aPS, nsString* aName)
{ 
  nsFontOS2* font = nsnull;
  char fontName[FACESIZE];
  WideCharToMultiByte(0, aName->GetUnicode(), aName->Length() + 1,
    fontName, sizeof(fontName));
  long lWant = 0;
  long lFonts = GpiQueryFonts( aPS, QF_PUBLIC | QF_PRIVATE,
                               fontName, &lWant, 0, 0);
  if (lFonts > 0) {
    font = new nsFontOS2();
    aName->ToCString(font->mName, sizeof(font->mName));
  }
  return font;
}


nsFontOS2*
nsFontMetricsOS2::FindUserDefinedFont(HPS aPS, PRUnichar aChar)
{
  if (mIsUserDefined) {
    nsFontOS2* font = LoadFont(aPS, &mUserDefined);
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
  { "times new roman", "Times New Roman" },
  { "arial",           "Arial" },
  { "helvetica",       "Helv" },
  { "courier",         "Courier" },
  { "courier new",     "Courier New" },

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
    nsFontFamilyName* f = gFamilyNameTable;
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

nsFontOS2*
nsFontMetricsOS2::FindLocalFont(HPS aPS, PRUnichar aChar)
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
    nsFontOS2* font = LoadFont(aPS, winName);
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

nsFontOS2*
nsFontMetricsOS2::LoadGenericFont(HPS aPS, PRUnichar aChar, char** aName)
{
  if (*aName) {
    int found = 0;
    int i;
#ifndef XP_OS2
    for (i = 0; i < mLoadedFontsCount; i++) {
      nsFontOS2* font = mLoadedFonts[i];
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
#endif
    PRUnichar name[FACESIZE] = { 0 };
    PRUnichar format[] = { '%', 's', 0 };
    PRUint32 n = nsTextFormatter::snprintf(name, FACESIZE, format, *aName);
    nsMemory::Free(*aName);
    *aName = nsnull;
    if (n && (n != (PRUint32) -1)) {
      nsAutoString  fontName(name);

      nsFontOS2* font = LoadFont(aPS, &fontName);
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
  nsFontOS2*        mFont;
  nsFontMetricsOS2* mMetrics;
} PrefEnumInfo;

void
PrefEnumCallback(const char* aName, void* aClosure)
{
  PrefEnumInfo* info = (PrefEnumInfo*) aClosure;
  if (info->mFont) {
    return;
  }
  PRUnichar ch = info->mChar;
  HPS ps = info->mPS;
  nsFontMetricsOS2* metrics = info->mMetrics;
  char* value = nsnull;
  gPref->CopyCharPref(aName, &value);
  nsFontOS2* font = metrics->LoadGenericFont(ps, ch, &value);
  if (font) {
    info->mFont = font;
  }
  else {
    gPref->CopyDefaultCharPref(aName, &value);
    font = metrics->LoadGenericFont(ps, ch, &value);
    if (font) {
      info->mFont = font;
    }
  }
}

nsFontOS2*
nsFontMetricsOS2::FindGenericFont(HDC aPS, PRUnichar aChar)
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
    nsFontOS2* font = LoadGenericFont(aPS, aChar, &value);
    if (font) {
      return font;
    }
    gPref->CopyDefaultCharPref(name, &value);
    font = LoadGenericFont(aPS, aChar, &value);
    if (font) {
      return font;
    }
  }
  prefix.ToCString(name, sizeof(name));
  PrefEnumInfo info = { aChar, aPS, nsnull, this };
  gPref->EnumerateChildren(name, PrefEnumCallback, &info);
  if (info.mFont) {
    return info.mFont;
  }
  mTriedAllGenerics = 1;

  return nsnull;
}

nsFontOS2*
nsFontMetricsOS2::FindFont(HPS aPS, PRUnichar aChar)
{
  nsFontOS2* font = FindUserDefinedFont(aPS, aChar);
  if (!font) {
    font = FindLocalFont(aPS, aChar);
    if (!font) {
      font = FindGenericFont(aPS, aChar);
#ifndef XP_OS2
      if (!font) {
        font = FindGlobalFont(aPS, aChar);
        if (!font) {
          font = FindSubstituteFont(aPS, aChar);
        }
      }
#endif
    }
  }

  return font;
}


static PRBool
FontEnumCallback(const nsString& aFamily, PRBool aGeneric, void *aData)
{
  nsFontMetricsOS2* metrics = (nsFontMetricsOS2*) aData;
  metrics->mFonts.AppendString(aFamily);
  metrics->mFontIsGeneric.AppendElement((void*) aGeneric);
  if (aGeneric) {
    metrics->mGeneric = metrics->mFonts.StringAt(metrics->mFonts.Count() - 1);
    return PR_FALSE; // stop
  }

  return PR_TRUE; // don't stop
}

// Current strategy wrt. image/outline fonts:
//   If image face requested is available in the point size requested,
//   use it.  If not, use the corresponding outline font.  The reason
//   why we deal with this here instead of letting GpiCreateLogFont() do
//   something sensible is because it doesn't do the right thing with
//   bold/italic effects: if we ask for bold Tms Rmn in 35 pt, we don't
//   get Times New Roman Bold in 35 pt, but Times New Roman 35pt with a
//   fake bold effect courtesy of gpi, which looks ugly.
//
// Yes, it would be easier & quite plausable to ignore image fonts altogether
//   and just use outlines, but I reckon image fonts look better at lower
//   point sizes.
//
// Candidate for butchery!

// Utility; delete [] when done.
static PFONTMETRICS getMetrics( long &lFonts, PCSZ facename, HPS hps)
{
   LONG lWant = 0;
   lFonts = GpiQueryFonts( hps, QF_PUBLIC | QF_PRIVATE,
                           facename, &lWant, 0, 0);
   PFONTMETRICS pMetrics = new FONTMETRICS [ lFonts];

   GpiQueryFonts( hps, QF_PUBLIC | QF_PRIVATE, facename, &lFonts,
                  sizeof( FONTMETRICS), pMetrics);

   return pMetrics;
}

nsresult nsFontMetricsOS2::RealizeFont()
{
nsresult res;
HWND  win = NULL;
HDC   ps = NULL;
  
  if (NULL != mDeviceContext->mDC){
    ps = mDeviceContext->mPS;
  } else {
    win = (HWND)mDeviceContext->mWidget;
    ps = ::WinGetPS(win);
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

#ifndef XP_OS2
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
    gPref->CopyUnicharPref(name.GetBuffer(), &value);
    if (value) {
      mUserDefined = value;
      nsMemory::Free(value);
      value = nsnull;
      mIsUserDefined = 1;
    }
  }
#endif

  nsFontOS2* font = FindFont(ps, 'a');
  if (!font) {
    return NS_ERROR_FAILURE;
  }

  char szFamily[FACESIZE] = "";

  strcpy(szFamily, font->mName);

  delete font;

  if (!strcmp(szFamily, "Verdana")) {
    return NS_ERROR_FAILURE;
  } /* endif */

   nsFontHandleOS2 *fh = new nsFontHandleOS2;
   if (!fh)
     return NS_ERROR_OUT_OF_MEMORY;

   // 2) Get a representative PS for doing font queries into
   HPS hps;

   if (NULL != mDeviceContext->mDC){
     hps = mDeviceContext->mPS;
   } else {
     hps = WinGetPS((HWND)mDeviceContext->mWidget);
   }

   // 3) Work out what our options are wrt. image/outline, prefer image.
   BOOL bOutline = FALSE, bImage = FALSE;
   long lFonts = 0; int i;
   PFONTMETRICS pMetrics = getMetrics( lFonts, szFamily, hps);

   for( i = 0; i < lFonts && !(bImage && bOutline); i++)
      if( pMetrics[ i].fsDefn & FM_DEFN_OUTLINE) bOutline = TRUE;
      else                                       bImage = TRUE;
   delete [] pMetrics;

   if( !bImage) fh->fattrs.fsFontUse = FATTR_FONTUSE_OUTLINE |
                                       FATTR_FONTUSE_TRANSFORMABLE;

   // 4) Try to munge the face for italic & bold effects (could do better)
   BOOL bBold = mFont->weight > NS_FONT_WEIGHT_NORMAL;
   BOOL bItalic = !!(mFont->style & NS_FONT_STYLE_ITALIC);
   FACENAMEDESC fnd = { sizeof( FACENAMEDESC),
                        bBold ? FWEIGHT_BOLD : FWEIGHT_DONT_CARE,
                        FWIDTH_DONT_CARE,
                        0,
                        bItalic ? FTYPE_ITALIC : 0 };
   
   ULONG rc = GpiQueryFaceString( hps, szFamily, &fnd,
                                  FACESIZE, fh->fattrs.szFacename);
   if( rc == GPI_ERROR)
   {  // no real font, fake it
      strcpy( fh->fattrs.szFacename, szFamily);
      if( bBold) fh->fattrs.fsSelection |= FATTR_SEL_BOLD;
      if( bItalic) fh->fattrs.fsSelection |= FATTR_SEL_ITALIC;
   }

   // 5) Add misc effects
   if( mFont->decorations & NS_FONT_DECORATION_UNDERLINE)
      fh->fattrs.fsSelection |= FATTR_SEL_UNDERSCORE;
   if( mFont->decorations & NS_FONT_DECORATION_LINE_THROUGH)
      fh->fattrs.fsSelection |= FATTR_SEL_STRIKEOUT;

   // 6) Encoding
   // There doesn't seem to be any encoding stuff yet, so guess.
   // (XXX unicode hack; use same codepage as converter!)
     char name[128];
    const PRUnichar* langGroup = nsnull;
    mLangGroup->GetUnicode(&langGroup);
    nsAutoString langName = langGroup;
    langName.ToCString(name, sizeof(name));
     for (int j=0; j < eCharSet_COUNT; j++ ) {
        if (name[0] == gCharSetInfo[j].mLangGroup[0]) {
          if (!strcmp(name, gCharSetInfo[j].mLangGroup)) {
            fh->fattrs.usCodePage = gCharSetInfo[j].mCodePage;
            mCodePage = gCharSetInfo[j].mCodePage;
          } /* endif */
       } /* endif */
    } /* endfor */
//   fh->fattrs.usCodePage = gModuleData.ulCodepage;
           
   // 7) Find the point size for the font, and set up the charbox too
   float app2dev, app2twip, twip2dev;
   mDeviceContext->GetAppUnitsToDevUnits( app2dev);
   mDeviceContext->GetDevUnitsToTwips( app2twip);
   mDeviceContext->GetTwipsToDevUnits( twip2dev);

   // !! Windows wants to mply up here.  I don't think I do.  If fonts
   // !! ever begin to look `squished', try enabling the following code
#if 0
   float scale;
   mDeviceContext->GetCanonicalPixelScale(scale);
   app2twip *= app2dev * scale;
#else
   app2twip *= app2dev;
#endif

   // Note: are you confused by the block above, and thinking that app2twip
   //       must be 1?  Well, there's *no* guarantee that app units are
   //       twips, despite whatever nscoord.h says!
   int points = NSTwipsToFloorIntPoints( nscoord( mFont->size * app2twip));
   fh->charbox.cx = MAKEFIXED( points * 20 * twip2dev, 0);
   fh->charbox.cy = fh->charbox.cx;

   // 8) If we're using an image font, check it's available in the size
   //    required, substituting an outline if necessary.
   if( bImage)
   {
      HDC    hdc = GpiQueryDevice( hps);
      long   res[ 2];
      DevQueryCaps( hdc, CAPS_HORIZONTAL_FONT_RES, 2, res);
      pMetrics = getMetrics( lFonts, szFamily, hps);

      for( i = 0; i < lFonts; i++)
         if( !stricmp( szFamily, pMetrics[ i].szFamilyname) &&
             pMetrics[ i].sNominalPointSize / 10 == points  &&
             pMetrics[ i].sXDeviceRes == res[0]             &&
             pMetrics[ i].sYDeviceRes == res[1]) break;

      if( i == lFonts)
      {
         // Couldn't find an appropriate font, need to use an outline.
         // If there was an outline originally, fine. If not...
         if( !bOutline)
         {
            // Can't have the requested font in requested size; fake.
            if( !stricmp( szFamily, "Helv"))
               strcpy( szFamily, "Helvetica");
            else if( !stricmp( szFamily, "Courier"))
               strcpy( szFamily, "Courier New");
            else
               strcpy( szFamily, "Times New Roman"); // hmm
            fh->fattrs.fsSelection &= ~(FATTR_SEL_BOLD | FATTR_SEL_ITALIC);
            rc = GpiQueryFaceString( hps, szFamily, &fnd,
                                     FACESIZE, fh->fattrs.szFacename);
            if( rc == GPI_ERROR)
            {
               strcpy( fh->fattrs.szFacename, szFamily);
               if( bBold) fh->fattrs.fsSelection |= FATTR_SEL_BOLD;
               if( bItalic) fh->fattrs.fsSelection |= FATTR_SEL_ITALIC;
            }
         }
         fh->fattrs.fsFontUse = FATTR_FONTUSE_OUTLINE |
                                FATTR_FONTUSE_TRANSFORMABLE;
      }
      else
      {
         // image face found fine, set required size in fattrs.
         fh->fattrs.lMaxBaselineExt = pMetrics[ i].lMaxBaselineExt;
         fh->fattrs.lAveCharWidth = pMetrics[ i].lAveCharWidth;
      }
      delete [] pMetrics;
   }

   // 9) Record font handle & record various font metrics to cache
   mFontHandle = fh;
   if( GPI_ERROR == GpiCreateLogFont( hps, 0, 1, &fh->fattrs))
      PMERROR( "GpiCreateLogFont");
   fh->SelectIntoPS( hps, 1);

   FONTMETRICS fm;
   GpiQueryFontMetrics( hps, sizeof fm, &fm);

   float dev2app;
   mDeviceContext->GetDevUnitsToAppUnits( dev2app);

   // PM includes the internal leading in the max ascender.  So the max
   // ascender we tell raptor about should be lMaxAscent - lInternalLeading.
   //
   // This is probably all moot 'cos lInternalLeading is usually zero.
   // More so 'cos layout doesn't look at the leading we give it.
   //
   // So let's leave it as zero to avoid confusion & change it if necessary.

   mHeight             = NSToCoordRound( fm.lMaxBaselineExt * dev2app);
   mMaxAscent          = NSToCoordRound( fm.lMaxAscender * dev2app);
   mMaxDescent         = NSToCoordRound( fm.lMaxDescender * dev2app);
   mMaxAdvance         = NSToCoordRound( fm.lMaxCharInc * dev2app);

   mMaxHeight          = NSToCoordRound( fm.lMaxBaselineExt * dev2app);
   mEmHeight           = NSToCoordRound( fm.lEmHeight * dev2app);
   // XXXXOS2TODO Not sure about these two - hs
   mEmAscent           = NSToCoordRound((fm.lMaxAscender - fm.lInternalLeading) * dev2app);
   mEmDescent          = NSToCoordRound( fm.lMaxDescender * dev2app);

   mLeading            = NSToCoordRound( fm.lInternalLeading * dev2app);
   mXHeight            = NSToCoordRound( fm.lXHeight * dev2app);

   mSuperscriptYOffset = NSToCoordRound( fm.lSuperscriptYOffset * dev2app);
   mSubscriptYOffset   = NSToCoordRound( fm.lSubscriptYOffset * dev2app); // !! check this
   mStrikeoutPosition  = NSToCoordRound( fm.lStrikeoutPosition * dev2app);
   mStrikeoutSize      = NSToCoordRound( fm.lStrikeoutSize * dev2app);
   mUnderlinePosition  = NSToCoordRound( -fm.lUnderscorePosition * dev2app);
   mUnderlineSize      = NSToCoordRound( fm.lUnderscoreSize * dev2app);

   // OS/2 field (needs to be kept in sync with mMaxAscent)
   mDevMaxAscent       = fm.lMaxAscender;

   // 10) Clean up
   GpiSetCharSet( hps, LCID_DEFAULT);
   if( !GpiDeleteSetId( hps, 1))
      PMERROR( "GpiDeleteSetID (FM)");
   if (NULL == mDeviceContext->mDC)
      WinReleasePS(hps);

   return NS_OK;
}

nscoord nsFontMetricsOS2::GetSpaceWidth( nsIRenderingContext *aRContext)
{
   if( !mSpaceWidth)
   {
      char buf[1];
      buf[0] = ' ';
      aRContext->GetWidth( buf, 1, mSpaceWidth);
   }

   return mSpaceWidth;
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
   aHeight = mHeight;
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

nsGlobalFont*
nsFontMetricsOS2::InitializeGlobalFonts(HPS aPS)
{
  static int gInitializedGlobalFonts = 0;
  if (!gInitializedGlobalFonts) {
    LONG lRemFonts = 0, lNumFonts;
    lNumFonts = GpiQueryFonts(aPS, QF_PUBLIC, NULL, &lRemFonts, 0, 0);
    PFONTMETRICS pFontMetrics = (PFONTMETRICS) nsMemory::Alloc(lNumFonts * sizeof(FONTMETRICS));
    lRemFonts = GpiQueryFonts(aPS, QF_PUBLIC, NULL, &lNumFonts, sizeof(FONTMETRICS), pFontMetrics);
    for (int i=0; i < lNumFonts; i++) {
      BOOL fAlreadyFound = FALSE;
      for (int j = 0; j < nsFontMetricsOS2::gGlobalFontsCount && !fAlreadyFound; j++) {
        if (!strcmp(nsFontMetricsOS2::gGlobalFonts[j].fontMetrics.szFamilyname,
                 pFontMetrics[i].szFamilyname)) {
              fAlreadyFound = TRUE;
         }
      }
      if (fAlreadyFound) {
         continue;
      } /* endif */

#ifdef MOZ_MATHML
      // XXX need a better way to deal with non-TrueType fonts?
      if (!(fontType & TRUETYPE_FONTTYPE)) {
        //printf("rejecting %s\n", logFont->lfFaceName);
        return 1;
      }
#endif
      // XXX ignore vertical fonts
      if (pFontMetrics[i].szFamilyname[0] == '@') {
        continue;
      }

#ifdef OLDCODE
      for (int j = 0; j < nsFontMetricsOS2::gGlobalFontsCount; j++) {
        if (!strcmp(nsFontMetricsOS2::gGlobalFonts[i].logFont.szFamilyname,
                 logFont->lfFaceName)) {

          //work-around for Win95/98 problem 
          int   charSetSigBit = charSetToBit[gCharSetToIndex[logFont->lfCharSet]];
          if 	(charSetSigBit >= 0) {
            DWORD  charsetSigAdd = 1 << charSetSigBit;
            nsFontMetricsOS2::gGlobalFonts[i].signature.fsCsb[0] |= charsetSigAdd;
          }

          return 1;
        }
      }
#endif

      // XXX make this smarter: don't add font to list if we already have a font
      // with the same font signature -- erik
      if (nsFontMetricsOS2::gGlobalFontsCount == gGlobalFontsAlloc) {
        int newSize = 2 * (gGlobalFontsAlloc ? gGlobalFontsAlloc : 1);
        nsGlobalFont* newPointer = (nsGlobalFont*)
        PR_Realloc(nsFontMetricsOS2::gGlobalFonts, newSize*sizeof(nsGlobalFont));
        if (newPointer) {
          nsFontMetricsOS2::gGlobalFonts = newPointer;
          gGlobalFontsAlloc = newSize;
        }
      }

      nsGlobalFont* font = &nsFontMetricsOS2::gGlobalFonts[nsFontMetricsOS2::gGlobalFontsCount];
      nsFontMetricsOS2::gGlobalFontsCount++;

//      PRUnichar name[FACESIZE*2];
//      name[0] = 0;
//      strcpy(name, pFontMetrics[i].szFamilyname);
      font->name = new nsString();
      font->name->AssignWithConversion(pFontMetrics[i].szFamilyname);
      if (!font->name) {
        nsFontMetricsOS2::gGlobalFontsCount--;
        continue;
      }
      font->map = nsnull;
      font->fontMetrics = pFontMetrics[i];
      font->skip = 0;

      nsFontMetricsOS2::gGlobalFonts[i].signature  = pFontMetrics[i].fsDefn;
    } /* endwhile */
    gInitializedGlobalFonts = 1;
    nsMemory::Free(pFontMetrics);
  }

  return gGlobalFonts;
}

// The Font Enumerator

nsFontEnumeratorOS2::nsFontEnumeratorOS2()
{
  NS_INIT_REFCNT();
}

NS_IMPL_ISUPPORTS(nsFontEnumeratorOS2,
                  NS_GET_IID(nsIFontEnumerator));

static int gInitializedFontEnumerator = 0;

static int
InitializeFontEnumerator(void)
{
  gInitializedFontEnumerator = 1;

  if (!nsFontMetricsOS2::gGlobalFonts) {
    HPS ps = ::WinGetScreenPS(HWND_DESKTOP);
    if (!nsFontMetricsOS2::InitializeGlobalFonts(ps)) {
      ::WinReleasePS(ps);
      return 0;
    }
    ::WinReleasePS(ps);
  }

  return 1;
}

static int PR_CALLBACK
CompareFontNames(const void* aArg1, const void* aArg2, void* aClosure)
{
  const PRUnichar* str1 = *((const PRUnichar**) aArg1);
  const PRUnichar* str2 = *((const PRUnichar**) aArg2);

  // XXX add nsICollation stuff

  return nsCRT::strcmp(str1, str2);
}

NS_IMETHODIMP
nsFontEnumeratorOS2::EnumerateAllFonts(PRUint32* aCount, PRUnichar*** aResult)
{
#ifndef XP_OS2
  *aCount = 18;
  PRUnichar** newarray = (PRUnichar**)
    nsMemory::Alloc(18 * sizeof(PRUnichar*));
  newarray[0] = nsString(NS_ConvertASCIItoUCS2("Tms Rmn")).ToNewUnicode();
  newarray[1] = nsString(NS_ConvertASCIItoUCS2("Tms Rmn")).ToNewUnicode();
  newarray[2] = nsString(NS_ConvertASCIItoUCS2("Tms Rmn")).ToNewUnicode();
  newarray[3] = nsString(NS_ConvertASCIItoUCS2("Tms Rmn")).ToNewUnicode();
  newarray[4] = nsString(NS_ConvertASCIItoUCS2("Tms Rmn")).ToNewUnicode();
  newarray[5] = nsString(NS_ConvertASCIItoUCS2("Tms Rmn")).ToNewUnicode();
  newarray[6] = nsString(NS_ConvertASCIItoUCS2("Tms Rmn")).ToNewUnicode();
  newarray[7] = nsString(NS_ConvertASCIItoUCS2("Tms Rmn")).ToNewUnicode();
  newarray[8] = nsString(NS_ConvertASCIItoUCS2("Tms Rmn")).ToNewUnicode();
  newarray[9] = nsString(NS_ConvertASCIItoUCS2("Tms Rmn")).ToNewUnicode();
  newarray[10] = nsString(NS_ConvertASCIItoUCS2("Tms Rmn")).ToNewUnicode();
  newarray[11] = nsString(NS_ConvertASCIItoUCS2("Tms Rmn")).ToNewUnicode();
  newarray[12] = nsString(NS_ConvertASCIItoUCS2("Tms Rmn")).ToNewUnicode();
  newarray[13] = nsString(NS_ConvertASCIItoUCS2("Tms Rmn")).ToNewUnicode();
  newarray[14] = nsString(NS_ConvertASCIItoUCS2("Tms Rmn")).ToNewUnicode();
  newarray[15] = nsString(NS_ConvertASCIItoUCS2("Tms Rmn")).ToNewUnicode();
  newarray[16] = nsString(NS_ConvertASCIItoUCS2("Tms Rmn")).ToNewUnicode();
  newarray[17] = nsString(NS_ConvertASCIItoUCS2("Tms Rmn")).ToNewUnicode();
  *aResult = newarray;
  return NS_OK;
#endif

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
    nsMemory::Alloc(nsFontMetricsOS2::gGlobalFontsCount * sizeof(PRUnichar*));
  if (!array) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  for (int i = 0; i < nsFontMetricsOS2::gGlobalFontsCount; i++) {
    PRUnichar* str = nsFontMetricsOS2::gGlobalFonts[i].name->ToNewUnicode();
    if (!str) {
      for (i = i - 1; i >= 0; i--) {
        nsMemory::Free(array[i]);
      }
      nsMemory::Free(array);
      return NS_ERROR_OUT_OF_MEMORY;
    }
    array[i] = str;
  }

  NS_QuickSort(array, nsFontMetricsOS2::gGlobalFontsCount, sizeof(PRUnichar*),
    CompareFontNames, nsnull);

  *aCount = nsFontMetricsOS2::gGlobalFontsCount;
  *aResult = array;

  return NS_OK;
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

#ifdef OLDCODE
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
#endif

  return 0;
}

NS_IMETHODIMP
nsFontEnumeratorOS2::EnumerateFonts(const char* aLangGroup,
  const char* aGeneric, PRUint32* aCount, PRUnichar*** aResult)
{
#ifndef XP_OS2
  *aCount = 18;
  PRUnichar** newarray = (PRUnichar**)
    nsMemory::Alloc(18 * sizeof(PRUnichar*));
  newarray[0] = nsString(NS_ConvertASCIItoUCS2("Tms Rmn")).ToNewUnicode();
  newarray[1] = nsString(NS_ConvertASCIItoUCS2("Tms Rmn")).ToNewUnicode();
  newarray[2] = nsString(NS_ConvertASCIItoUCS2("Tms Rmn")).ToNewUnicode();
  newarray[3] = nsString(NS_ConvertASCIItoUCS2("Tms Rmn")).ToNewUnicode();
  newarray[4] = nsString(NS_ConvertASCIItoUCS2("Tms Rmn")).ToNewUnicode();
  newarray[5] = nsString(NS_ConvertASCIItoUCS2("Tms Rmn")).ToNewUnicode();
  newarray[6] = nsString(NS_ConvertASCIItoUCS2("Tms Rmn")).ToNewUnicode();
  newarray[7] = nsString(NS_ConvertASCIItoUCS2("Tms Rmn")).ToNewUnicode();
  newarray[8] = nsString(NS_ConvertASCIItoUCS2("Tms Rmn")).ToNewUnicode();
  newarray[9] = nsString(NS_ConvertASCIItoUCS2("Tms Rmn")).ToNewUnicode();
  newarray[10] = nsString(NS_ConvertASCIItoUCS2("Tms Rmn")).ToNewUnicode();
  newarray[11] = nsString(NS_ConvertASCIItoUCS2("Tms Rmn")).ToNewUnicode();
  newarray[12] = nsString(NS_ConvertASCIItoUCS2("Tms Rmn")).ToNewUnicode();
  newarray[13] = nsString(NS_ConvertASCIItoUCS2("Tms Rmn")).ToNewUnicode();
  newarray[14] = nsString(NS_ConvertASCIItoUCS2("Tms Rmn")).ToNewUnicode();
  newarray[15] = nsString(NS_ConvertASCIItoUCS2("Tms Rmn")).ToNewUnicode();
  newarray[16] = nsString(NS_ConvertASCIItoUCS2("Tms Rmn")).ToNewUnicode();
  newarray[17] = nsString(NS_ConvertASCIItoUCS2("Tms Rmn")).ToNewUnicode();
  *aResult = newarray;
  return NS_OK;
#endif


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
  return EnumerateAllFonts(aCount, aResult);

  if ((!strcmp(aLangGroup, "x-unicode")) ||
      (!strcmp(aLangGroup, "x-user-def"))) {
  }

  if (!gInitializedFontEnumerator) {
    if (!InitializeFontEnumerator()) {
      return NS_ERROR_FAILURE;
    }
  }

  PRUnichar** array = (PRUnichar**)
    nsMemory::Alloc(nsFontMetricsOS2::gGlobalFontsCount * sizeof(PRUnichar*));
  if (!array) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  int j = 0;
  for (int i = 0; i < nsFontMetricsOS2::gGlobalFontsCount; i++) {
    if (SignatureMatchesLangGroup(&nsFontMetricsOS2::gGlobalFonts[i].signature,
                                  aLangGroup) &&
        FontMatchesGenericType(&nsFontMetricsOS2::gGlobalFonts[i], aGeneric,
                               aLangGroup)) {
      PRUnichar* str = nsFontMetricsOS2::gGlobalFonts[i].name->ToNewUnicode();
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
