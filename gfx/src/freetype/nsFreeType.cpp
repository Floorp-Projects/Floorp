/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ex: set tabstop=8 softtabstop=2 shiftwidth=2 expandtab: */
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
 *   Brian Stell <bstell@netscape.com>
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

#include "math.h"
#include "nsIPref.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsCompressedCharMap.h"
#include "nsICharsetConverterManager.h"
#include "nsIRenderingContext.h"
#include "nsFreeType.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_TRUETYPE_TABLES_H
#include FT_TRUETYPE_IDS_H

#include "freetype/ftcache.h"
#include "freetype/cache/ftcimage.h"

# define FREETYPE_PRINTF(x) \
            PR_BEGIN_MACRO \
              if (gFreeTypeDebug) { \
                printf x ; \
                printf(", %s %d\n", __FILE__, __LINE__); \
              } \
            PR_END_MACRO

PRUint32 gFreeTypeDebug = 0;

char*             nsFreeType::gFreeType2SharedLibraryName = nsnull;
PRBool            nsFreeType::gEnableFreeType2 = PR_FALSE;
PRBool            nsFreeType::gFreeType2Autohinted = PR_FALSE;
PRBool            nsFreeType::gFreeType2Unhinted = PR_TRUE;
PRUint8           nsFreeType::gAATTDarkTextMinValue = 64;
double            nsFreeType::gAATTDarkTextGain = 0.8;
PRInt32           nsFreeType::gAntiAliasMinimum = 8;
PRInt32           nsFreeType::gEmbeddedBitmapMaximumHeight = 1000000;
nsHashtable*      nsFreeType::sFontFamilies = nsnull;
nsHashtable*      nsFreeType::sRange1CharSetNames = nsnull;
nsHashtable*      nsFreeType::sRange2CharSetNames = nsnull;
nsICharsetConverterManager2* nsFreeType::sCharSetManager = nsnull;

extern nsulCodePageRangeCharSetName ulCodePageRange1CharSetNames[];
extern nsulCodePageRangeCharSetName ulCodePageRange2CharSetNames[];
extern nsTTFontFamilyEncoderInfo    gFontFamilyEncoderInfo[];

typedef int Error;

/*FT_CALLBACK_DEF*/ FT_Error nsFreeTypeFaceRequester(FTC_FaceID, FT_Library, 
                                                 FT_Pointer, FT_Face*);
static FT_Error nsFreeType__DummyFunc();

static nsHashtable* gFreeTypeFaces = nsnull;

static NS_DEFINE_CID(kCharSetManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);

//
// Since the Freetype2 library may not be available on the user's
// system we cannot link directly to it otherwise we would fail
// to run on those systems.  Instead we use function pointers to 
// access its functions.
//
// If we can load the Freetype2 library with PR_LoadLIbrary 
// we to load these with pointers to its functions. If not, then
// they point to a dummy function which always returns an error.
//
// These are the Freetype2 the functions pointers

FT_Done_Face_t            nsFreeType::nsFT_Done_Face;
FT_Done_FreeType_t        nsFreeType::nsFT_Done_FreeType;
FT_Done_Glyph_t           nsFreeType::nsFT_Done_Glyph;
FT_Get_Char_Index_t       nsFreeType::nsFT_Get_Char_Index;
FT_Get_Glyph_t            nsFreeType::nsFT_Get_Glyph;
FT_Get_Sfnt_Table_t       nsFreeType::nsFT_Get_Sfnt_Table;
FT_Glyph_Get_CBox_t       nsFreeType::nsFT_Glyph_Get_CBox;
FT_Init_FreeType_t        nsFreeType::nsFT_Init_FreeType;
FT_Load_Glyph_t           nsFreeType::nsFT_Load_Glyph;
FT_New_Face_t             nsFreeType::nsFT_New_Face;
FT_Set_Charmap_t          nsFreeType::nsFT_Set_Charmap;
FTC_Image_Cache_Lookup_t  nsFreeType::nsFTC_Image_Cache_Lookup;
FTC_Manager_Lookup_Size_t nsFreeType::nsFTC_Manager_Lookup_Size;
FTC_Manager_Done_t        nsFreeType::nsFTC_Manager_Done;
FTC_Manager_New_t         nsFreeType::nsFTC_Manager_New;
FTC_Image_Cache_New_t     nsFreeType::nsFTC_Image_Cache_New;


typedef struct {
  const char *FuncName;
  PRFuncPtr  *FuncPtr;
} FtFuncList;

static FtFuncList FtFuncs [] = {
  {"FT_Done_Face",            (PRFuncPtr*)&nsFreeType::nsFT_Done_Face},
  {"FT_Done_FreeType",        (PRFuncPtr*)&nsFreeType::nsFT_Done_FreeType},
  {"FT_Done_Glyph",           (PRFuncPtr*)&nsFreeType::nsFT_Done_Glyph},
  {"FT_Get_Char_Index",       (PRFuncPtr*)&nsFreeType::nsFT_Get_Char_Index},
  {"FT_Get_Glyph",            (PRFuncPtr*)&nsFreeType::nsFT_Get_Glyph},
  {"FT_Get_Sfnt_Table",       (PRFuncPtr*)&nsFreeType::nsFT_Get_Sfnt_Table},
  {"FT_Glyph_Get_CBox",       (PRFuncPtr*)&nsFreeType::nsFT_Glyph_Get_CBox},
  {"FT_Init_FreeType",        (PRFuncPtr*)&nsFreeType::nsFT_Init_FreeType},
  {"FT_Load_Glyph",           (PRFuncPtr*)&nsFreeType::nsFT_Load_Glyph},
  {"FT_New_Face",             (PRFuncPtr*)&nsFreeType::nsFT_New_Face},
  {"FT_Set_Charmap",          (PRFuncPtr*)&nsFreeType::nsFT_Set_Charmap},
  {"FTC_Image_Cache_Lookup",  (PRFuncPtr*)&nsFreeType::nsFTC_Image_Cache_Lookup},
  {"FTC_Manager_Lookup_Size", (PRFuncPtr*)&nsFreeType::nsFTC_Manager_Lookup_Size},
  {"FTC_Manager_Done",        (PRFuncPtr*)&nsFreeType::nsFTC_Manager_Done},
  {"FTC_Manager_New",         (PRFuncPtr*)&nsFreeType::nsFTC_Manager_New},
  {"FTC_Image_Cache_New",     (PRFuncPtr*)&nsFreeType::nsFTC_Image_Cache_New},
  {nsnull,                    (PRFuncPtr*)nsnull},
};

nsTTFontEncoderInfo FEI_Adobe_Symbol_Encoding = {
  "Adobe-Symbol-Encoding", TT_PLATFORM_MICROSOFT, TT_MS_ID_SYMBOL_CS, nsnull
};
nsTTFontEncoderInfo FEI_x_ttf_cmr = {
  "x-ttf-cmr", TT_PLATFORM_MICROSOFT, TT_MS_ID_UNICODE_CS, nsnull
};
nsTTFontEncoderInfo FEI_x_ttf_cmmi = {
  "x-ttf-cmmi", TT_PLATFORM_MICROSOFT, TT_MS_ID_UNICODE_CS, nsnull
};
nsTTFontEncoderInfo FEI_x_ttf_cmsy = {
  "x-ttf-cmsy", TT_PLATFORM_MICROSOFT, TT_MS_ID_UNICODE_CS, nsnull
};
nsTTFontEncoderInfo FEI_x_ttf_cmex = {
  "x-ttf-cmex", TT_PLATFORM_MICROSOFT, TT_MS_ID_UNICODE_CS, nsnull
};
nsTTFontEncoderInfo FEI_x_mathematica1 = {
  "x-mathematica1", TT_PLATFORM_MACINTOSH, TT_MAC_ID_ROMAN, nsnull
};
nsTTFontEncoderInfo FEI_x_mathematica2 = {
  "x-mathematica2", TT_PLATFORM_MACINTOSH, TT_MAC_ID_ROMAN, nsnull
};
nsTTFontEncoderInfo FEI_x_mathematica3 = {
  "x-mathematica3", TT_PLATFORM_MACINTOSH, TT_MAC_ID_ROMAN, nsnull
};
nsTTFontEncoderInfo FEI_x_mathematica4 = {
  "x-mathematica4", TT_PLATFORM_MACINTOSH, TT_MAC_ID_ROMAN, nsnull
};
nsTTFontEncoderInfo FEI_x_mathematica5 = {
  "x-mathematica5", TT_PLATFORM_MACINTOSH, TT_MAC_ID_ROMAN, nsnull
};
nsTTFontEncoderInfo FEI_x_mtextra = {
  "x-mtextra", TT_PLATFORM_MICROSOFT, TT_MS_ID_SYMBOL_CS, nsnull
};
nsTTFontEncoderInfo FEI_windows_1252 = {
  "windows-1252", TT_PLATFORM_MICROSOFT, TT_MS_ID_SYMBOL_CS, nsnull
};


///////////////////////////////////////////////////////////////////////
//
// class nsFreeType data/functions
//
///////////////////////////////////////////////////////////////////////

// Public Functions
void
nsFreeTypeFreeGlobals()
{
  nsFreeType::FreeGlobals();
}

nsresult
nsFreeTypeInitGlobals()
{
  FREETYPE_PRINTF(("initialize freetype"));

  nsresult rv = nsFreeType::InitGlobals();

  return rv;
}


// FREETYPE Globals
PRLibrary      *nsFreeType::sSharedLib;
PRBool          nsFreeType::sInited;
PRBool          nsFreeType::sAvailable;
FT_Library      nsFreeType::sFreeTypeLibrary;
FT_Error        nsFreeType::sInitError;
FTC_Manager     nsFreeType::sFTCacheManager;
FTC_Image_Cache nsFreeType::sImageCache;

void
nsFreeType::ClearFunctions()
{
  FtFuncList *p;
  for (p=FtFuncs; p->FuncName; p++) {
    *p->FuncPtr = (PRFuncPtr)&nsFreeType__DummyFunc;
  }
}

void
nsFreeType::ClearGlobals()
{
  sSharedLib = nsnull;
  sInited = PR_FALSE;
  sAvailable = PR_FALSE;
  sFreeTypeLibrary = nsnull;
  sInitError = 0;
  sFTCacheManager  = nsnull;
  sImageCache      = nsnull;
}

// I would like to make this a static member function but the compilier 
// warning about converting a data pointer to a function pointer cannot
// distinguish static member functions from static data members
static FT_Error
nsFreeType__DummyFunc()
{
  NS_ERROR("nsFreeType__DummyFunc should never be called");
  return 1;
}

void
nsFreeType::FreeGlobals()
{
  if (gFreeType2SharedLibraryName) {
    free(gFreeType2SharedLibraryName);
    gFreeType2SharedLibraryName = nsnull;
  }
  if (gFreeTypeFaces) {
    gFreeTypeFaces->Reset(nsFreeTypeFace::FreeFace, nsnull);
    delete gFreeTypeFaces;
    gFreeTypeFaces = nsnull;
  }
  // sImageCache released by cache manager
  if (sFTCacheManager) {
    (*nsFreeType::nsFTC_Manager_Done)(sFTCacheManager);
    sFTCacheManager = nsnull;
  }
  if (sFreeTypeLibrary) {
    (*nsFreeType::nsFT_Done_FreeType)(sFreeTypeLibrary);
    sFreeTypeLibrary = nsnull;
  }
  
  if (sRange1CharSetNames)
    delete sRange1CharSetNames;
  if (sRange2CharSetNames)
    delete sRange2CharSetNames;
  if (sFontFamilies)
    delete sFontFamilies;
  
  NS_IF_RELEASE(sCharSetManager);
  
  // release any encoders that were created
  int i;
  for (i=0; gFontFamilyEncoderInfo[i].mFamilyName; i++) {
    nsTTFontFamilyEncoderInfo *ffei = &gFontFamilyEncoderInfo[i];
    nsTTFontEncoderInfo *fei = ffei->mEncodingInfo;
    NS_IF_RELEASE(fei->mConverter);
  }

  UnloadSharedLib();
  ClearFunctions();
  ClearGlobals();
}

nsresult
nsFreeType::InitGlobals(void)
{
  NS_ASSERTION(sInited==PR_FALSE, "InitGlobals called more than once");
  // set all the globals to default values
  ClearGlobals();

  nsulCodePageRangeCharSetName *crn = nsnull;
  nsTTFontFamilyEncoderInfo *ff = gFontFamilyEncoderInfo;
  nsCOMPtr<nsIPref> mPref = do_GetService(NS_PREF_CONTRACTID);
  
  if (!mPref) {
    FreeGlobals();
    return NS_ERROR_FAILURE;
  }
  nsresult rv;

  PRBool enable_freetype2 = PR_TRUE;
  rv = mPref->GetBoolPref("font.FreeType2.enable", &enable_freetype2);
  if (NS_SUCCEEDED(rv)) {
    gEnableFreeType2 = enable_freetype2;
    FREETYPE_PRINTF(("gEnableFreeType2 = %d", gEnableFreeType2));
  }

  rv = mPref->GetCharPref("font.freetype2.shared-library",
                          &gFreeType2SharedLibraryName);
  if (NS_FAILED(rv)) {
    enable_freetype2 = PR_FALSE;
    FREETYPE_PRINTF((
                  "gFreeType2SharedLibraryName missing, FreeType2 disabled"));
    gFreeType2SharedLibraryName = nsnull;
  }

  PRBool freetype2_autohinted = PR_FALSE;
  rv = mPref->GetBoolPref("font.FreeType2.autohinted", &freetype2_autohinted);
  if (NS_SUCCEEDED(rv)) {
    gFreeType2Autohinted = freetype2_autohinted;
    FREETYPE_PRINTF(("gFreeType2Autohinted = %d", gFreeType2Autohinted));
  }
  
  PRBool freetype2_unhinted = PR_TRUE;
  rv = mPref->GetBoolPref("font.FreeType2.unhinted", &freetype2_unhinted);
  if (NS_SUCCEEDED(rv)) {
    gFreeType2Unhinted = freetype2_unhinted;
    FREETYPE_PRINTF(("gFreeType2Unhinted = %d", gFreeType2Unhinted));
  }

  PRInt32 int_val = 0;
  rv = mPref->GetIntPref("font.scale.tt_bitmap.dark_text.min", &int_val);
  if (NS_SUCCEEDED(rv)) {
    gAATTDarkTextMinValue = int_val;
    FREETYPE_PRINTF(("gAATTDarkTextMinValue = %d", gAATTDarkTextMinValue));
  }

  nsXPIDLCString str;
  rv = mPref->GetCharPref("font.scale.tt_bitmap.dark_text.gain",
                           getter_Copies(str));
  if (NS_SUCCEEDED(rv)) {
    gAATTDarkTextGain = atof(str.get());
    FREETYPE_PRINTF(("gAATTDarkTextGain = %g", gAATTDarkTextGain));
  }

  PRInt32 antialias_minimum = 8;
  rv = mPref->GetIntPref("font.antialias.min", &antialias_minimum);
  if (NS_SUCCEEDED(rv)) {
    gAntiAliasMinimum = antialias_minimum;
    FREETYPE_PRINTF(("gAntiAliasMinimum = %d", gAntiAliasMinimum));
  }

  PRInt32 embedded_bitmaps_maximum = 1000000;
  rv = mPref->GetIntPref("font.embedded_bitmaps.max",&embedded_bitmaps_maximum);
  if (NS_SUCCEEDED(rv)) {
    gEmbeddedBitmapMaximumHeight = embedded_bitmaps_maximum;
    FREETYPE_PRINTF(("gEmbeddedBitmapMaximumHeight = %d",
                             gEmbeddedBitmapMaximumHeight));
  }
  
  if (NS_FAILED(rv)) {
    gEnableFreeType2 = PR_FALSE;
    gFreeType2SharedLibraryName = nsnull;
    gFreeType2SharedLibraryName = nsnull;
    gEnableFreeType2 = PR_FALSE;
    gFreeType2Autohinted = PR_FALSE;
    gFreeType2Unhinted = PR_TRUE;
    gAATTDarkTextMinValue = 64;
    gAATTDarkTextGain = 0.8;
    gAntiAliasMinimum = 8;
    gEmbeddedBitmapMaximumHeight = 1000000;
  }
  
  mPref = nsnull;
  
  if (!nsFreeType::InitLibrary()) {
    // since the library may not be available on any given system
    // failing to load is not considered a fatal error
    // return NS_ERROR_OUT_OF_MEMORY;
  }
  gFreeTypeFaces = new nsHashtable();
  if (!gFreeTypeFaces) {
    FreeGlobals();
    return NS_ERROR_OUT_OF_MEMORY;
  }

  sRange1CharSetNames = new nsHashtable();
  if (!sRange1CharSetNames) {
    FreeGlobals();
    return NS_ERROR_OUT_OF_MEMORY;
  }
  crn = ulCodePageRange1CharSetNames;
  while (crn->charsetName) {
    char buf[32];
    sprintf(buf, "0x%08lx", crn->bit);
    nsCStringKey key(buf);
    sRange1CharSetNames->Put(&key, (void*)crn->charsetName);
    crn++;
  }

  sRange2CharSetNames = new nsHashtable();
  if (!sRange2CharSetNames) {
    FreeGlobals();
    return NS_ERROR_OUT_OF_MEMORY;
  }
  crn = ulCodePageRange2CharSetNames;
  while (crn->charsetName) {
    char buf[32];
    sprintf(buf, "0x%08lx", crn->bit);
    nsCStringKey key(buf);
    sRange2CharSetNames->Put(&key, (void*)crn->charsetName);
    crn++;
  }

  sFontFamilies = new nsHashtable();
  if (!sFontFamilies) {
    FreeGlobals();
    return NS_ERROR_OUT_OF_MEMORY;
  }
  while (ff->mFamilyName) {
    nsCAutoString name(ff->mFamilyName);
    ToLowerCase(name);
    nsCStringKey key(name);
    sFontFamilies->Put(&key, (void*)ff);
    ff++;
  }

  return NS_OK;
}

PRBool
nsFreeType::InitLibrary()
{
  if (!sInited) {
    sInited = PR_TRUE;

#ifdef MOZ_MATHML
    // do not yet support MathML
    // goto cleanup_and_return;
#endif

    if (!gEnableFreeType2)
      return PR_FALSE;

    // since the library may not be available on any given system
    // failing to load is not considered a fatal error
    if (!LoadSharedLib())
      return PR_FALSE;

    sInitError = (*nsFreeType::nsFT_Init_FreeType)(&sFreeTypeLibrary);
    if (sInitError) {
      FREETYPE_PRINTF(("\n\n*********\nFreeType initialization error = %d",
                           sInitError));
      sFreeTypeLibrary = nsnull;
      goto cleanup_and_return;
    }
    FT_Error error = (*nsFreeType::nsFTC_Manager_New)(sFreeTypeLibrary,
                       0, 0, 0, nsFreeTypeFaceRequester, 0, &sFTCacheManager);
    NS_ASSERTION(error==0, "failed to create FreeType Cache manager");
    if (error)
      goto cleanup_and_return;
    error = (*nsFreeType::nsFTC_Image_Cache_New)(sFTCacheManager,
                                                     &sImageCache);
    NS_ASSERTION(error==0, "failed to create FreeType image cache");
    if (error)
      goto cleanup_and_return;
    sAvailable = PR_TRUE;
  }
  return sAvailable;

cleanup_and_return:
  // clean everything up but note that init was called
  FreeGlobals();
  sInited = PR_TRUE;
  return(sAvailable);
}

PRBool
nsFreeType::LoadSharedLib()
{
  NS_ASSERTION(sSharedLib==nsnull, "library already loaded");

  if (!gFreeType2SharedLibraryName)
    return PR_FALSE;
  sSharedLib = PR_LoadLibrary(gFreeType2SharedLibraryName);
  // since the library may not be available on any given system
  // failing to load is not considered a fatal error
  if (!sSharedLib) {
    NS_WARNING("freetype library not found");
    return PR_FALSE;
  }

  FtFuncList *p;
  PRFuncPtr func;
  for (p=FtFuncs; p->FuncName; p++) {
    func = PR_FindFunctionSymbol(sSharedLib, p->FuncName);
    if (!func) {
      NS_WARNING("nsFreeType::LoadSharedLib Error");
      ClearFunctions();
      return PR_FALSE;
    }
    *p->FuncPtr = func;
  }

  return PR_TRUE;

}

void
nsFreeType::UnloadSharedLib()
{
  if (sSharedLib)
    PR_UnloadLibrary(sSharedLib);
  sSharedLib = nsnull;
}

const char *
nsFreeType::GetRange1CharSetName(unsigned long aBit)
{
  char buf[32];
  sprintf(buf, "0x%08lx", aBit);
  nsCStringKey key(buf);
  const char *charsetName = (const char *)sRange1CharSetNames->Get(&key);
  return charsetName;
}

const char *
nsFreeType::GetRange2CharSetName(unsigned long aBit)
{
  char buf[32];
  sprintf(buf, "0x%08lx", aBit);
  nsCStringKey key(buf);
  const char *charsetName = (const char *)sRange2CharSetNames->Get(&key);
  return charsetName;
}

nsTTFontFamilyEncoderInfo*
nsFreeType::GetCustomEncoderInfo(const char * aFamilyName)
{
  if (!sFontFamilies)
    return nsnull;

  nsTTFontFamilyEncoderInfo *ffei;
  nsCAutoString name(aFamilyName);
  ToLowerCase(name);
  nsCStringKey key(name);
  ffei = (nsTTFontFamilyEncoderInfo*)sFontFamilies->Get(&key);
  if (!ffei)
    return nsnull;

  // init the converter
  if (!ffei->mEncodingInfo->mConverter) {
    nsTTFontEncoderInfo *fei = ffei->mEncodingInfo;
    //
    // build the converter
    //
    nsICharsetConverterManager2* charSetManager = GetCharSetManager();
    if (!charSetManager)
      return nsnull;
    nsCOMPtr<nsIAtom> charset(dont_AddRef(NS_NewAtom(fei->mConverterName)));
    if (charset) {
      nsresult res;
      res = charSetManager->GetUnicodeEncoder(charset, &fei->mConverter);
      if (NS_FAILED(res)) {
        return nsnull;
      }
    }
  }
  return ffei;
}

nsICharsetConverterManager2*
nsFreeType::GetCharSetManager()
{
  if (!sCharSetManager) {
    //
    // get the sCharSetManager
    //
    nsServiceManager::GetService(kCharSetManagerCID,
                                 NS_GET_IID(nsICharsetConverterManager2),
                                 (nsISupports**) &sCharSetManager);
    NS_ASSERTION(sCharSetManager,"failed to create the charset manager");
  }
  return sCharSetManager;
}

PRUint16*
nsFreeType::GetCCMap(nsFontCatalogEntry *aFce)
{
  nsCompressedCharMap ccmapObj;
  ccmapObj.SetChars(aFce->mCCMap);
  return ccmapObj.NewCCMap();
}

///////////////////////////////////////////////////////////////////////
//
// class nsFreeTypeFace data/functions
//
///////////////////////////////////////////////////////////////////////
NS_IMPL_ISUPPORTS1(nsFreeTypeFace, nsITrueTypeFontCatalogEntry)

nsFreeTypeFace::nsFreeTypeFace()
{
}

nsresult nsFreeTypeFace::Init(nsFontCatalogEntry *aFce)
{
  NS_ASSERTION(aFce, "init of nsFreeTypeFace needs nsFontCatalogEntry");
  NS_INIT_ISUPPORTS();
  if (aFce)
    mFce = aFce;
  else {
    mFce = new nsFontCatalogEntry;
    NS_ASSERTION(mFce, "memory error while creating nsFontCatalogEntry");
    if (!mFce)
      return NS_ERROR_OUT_OF_MEMORY;
  }
  mCCMap = nsnull;
  return NS_OK;
}

nsFreeTypeFace::~nsFreeTypeFace()
{
  if (mCCMap)
    FreeCCMap(mCCMap);
}

NS_IMETHODIMP nsFreeTypeFace::GetFontCatalogType(
                              PRUint16 *aFontCatalogType)
{
  *aFontCatalogType = FONT_CATALOG_TRUETYPE;
  return NS_OK;
}

/* readonly attribute ACString fileName; */
NS_IMETHODIMP nsFreeTypeFace::GetFileName(nsACString & aFileName)
{
  aFileName.Assign(mFce->mFontFileName);
  return NS_OK;
}

/* readonly attribute ACString familyName; */
NS_IMETHODIMP nsFreeTypeFace::GetFamilyName(nsACString & aFamilyName)
{
  aFamilyName.Assign(mFce->mFamilyName);
  return NS_OK;
}

/* readonly attribute ACString styleName; */
NS_IMETHODIMP nsFreeTypeFace::GetStyleName(nsACString & aStyleName)
{
  aStyleName.Assign(mFce->mStyleName);
  return NS_OK;
}

/* readonly attribute ACString vendorID; */
NS_IMETHODIMP nsFreeTypeFace::GetVendorID(nsACString & aVendorID)
{
  aVendorID.Assign(mFce->mVendorID);
  return NS_OK;
}

/* readonly attribute short faceIndex; */
NS_IMETHODIMP nsFreeTypeFace::GetFaceIndex(PRInt16 *aFaceIndex)
{
  *aFaceIndex = mFce->mFaceIndex;
  return NS_OK;
}

/* readonly attribute short numFaces; */
NS_IMETHODIMP nsFreeTypeFace::GetNumFaces(PRInt16 *aNumFaces)
{
  *aNumFaces = mFce->mNumFaces;
  return NS_OK;
}

/* readonly attribute short numEmbeddedBitmaps; */
NS_IMETHODIMP nsFreeTypeFace::GetNumEmbeddedBitmaps(
                              PRInt16 *aNumEmbeddedBitmaps)
{
  *aNumEmbeddedBitmaps = mFce->mNumEmbeddedBitmaps;
  return NS_OK;
}

/* readonly attribute long numGlyphs; */
NS_IMETHODIMP nsFreeTypeFace::GetNumGlyphs(PRInt32 *aNumGlyphs)
{
  *aNumGlyphs = mFce->mNumGlyphs;
  return NS_OK;
}

/* readonly attribute long numUsableGlyphs; */
NS_IMETHODIMP nsFreeTypeFace::GetNumUsableGlyphs(
                              PRInt32 *aNumUsableGlyphs)
{
  *aNumUsableGlyphs = mFce->mNumUsableGlyphs;
  return NS_OK;
}

/* readonly attribute unsigned short weight; */
NS_IMETHODIMP nsFreeTypeFace::GetWeight(PRUint16 *aWeight)
{
  *aWeight = mFce->mWeight;
  return NS_OK;
}

/* readonly attribute unsigned short width; */
NS_IMETHODIMP nsFreeTypeFace::GetWidth(PRUint16 *aWidth)
{
  *aWidth = mFce->mWidth;
  return NS_OK;
}

/* readonly attribute unsigned long flags; */
NS_IMETHODIMP nsFreeTypeFace::GetFlags(PRUint32 *aFlags)
{
  *aFlags = mFce->mFlags;
  return NS_OK;
}

/* readonly attribute long long faceFlags; */
NS_IMETHODIMP nsFreeTypeFace::GetFaceFlags(PRInt64 *aFaceFlags)
{
  *aFaceFlags = mFce->mFaceFlags;
  return NS_OK;
}

/* readonly attribute long long styleFlags; */
NS_IMETHODIMP nsFreeTypeFace::GetStyleFlags(PRInt64 *aStyleFlags)
{
  *aStyleFlags = mFce->mStyleFlags;
  return NS_OK;
}

/* readonly attribute unsigned long codePageRange1; */
NS_IMETHODIMP nsFreeTypeFace::GetCodePageRange1(
                              PRUint32 *aCodePageRange1)
{
  *aCodePageRange1 = mFce->mCodePageRange1;
  return NS_OK;
}

/* readonly attribute unsigned long codePageRange2; */
NS_IMETHODIMP nsFreeTypeFace::GetCodePageRange2(
                              PRUint32 *aCodePageRange2)
{
  *aCodePageRange2 = mFce->mCodePageRange2;
  return NS_OK;
}

/* readonly attribute long long time; */
NS_IMETHODIMP nsFreeTypeFace::GetFileModTime(PRInt64 *aTime)
{
  *aTime = mFce->mMTime;
  return NS_OK;
}

/* void getCCMap (out unsigned long size,
 * [array, size_is (size), retval] out unsigned short ccMaps); */
NS_IMETHODIMP nsFreeTypeFace::GetCCMap(
                              PRUint32 *size, PRUint16 **ccMaps)
{
  *ccMaps = nsFreeType::GetCCMap(mFce);
  *size = CCMAP_SIZE(*ccMaps);
  return NS_OK;
}

/* void getEmbeddedBitmapHeights (out unsigned long size,
 * [array, size_is (size), retval] out short heights); */
NS_IMETHODIMP nsFreeTypeFace::GetEmbeddedBitmapHeights(
                              PRUint32 *size, PRInt32 **heights)
{
  *heights = mFce->mEmbeddedBitmapHeights;
  *size = mFce->mNumEmbeddedBitmaps;
  return NS_OK;
}

PRBool
nsFreeTypeFace::FreeFace(nsHashKey* aKey, void* aData, void* aClosure)
{
  nsFreeTypeFace *face = (nsFreeTypeFace*) aData;
  delete face;

  return PR_TRUE;
}

PRUint16 *
nsFreeTypeFace::GetCCMap()
{
  if (!mCCMap) {
    mCCMap = nsFreeType::GetCCMap(mFce);
  }
  return mCCMap;
}

///////////////////////////////////////////////////////////////////////
//
// miscellaneous routines in alphabetic order
//
///////////////////////////////////////////////////////////////////////

/*FT_CALLBACK_DEF*/
FT_Error
nsFreeTypeFaceRequester(FTC_FaceID face_id, FT_Library lib,
                  FT_Pointer request_data, FT_Face* aFace)
{
  nsFreeTypeFace *faceID = (nsFreeTypeFace *)face_id;
  FT_Error fterror;

  FT_UNUSED(request_data);
  // since all interesting data is in the nsFreeTypeFace object
  // we do not currently need to use request_data
  fterror = (*nsFreeType::nsFT_New_Face)(nsFreeType::GetLibrary(),
                     faceID->GetFilename(),
                     faceID->GetFaceIndex(), aFace);
  if (fterror)
    return fterror;

  FT_Face face = *aFace;
  FT_UShort platform_id = TT_PLATFORM_MICROSOFT;
  FT_UShort encoding_id = TT_MS_ID_UNICODE_CS;
  nsFontCatalogEntry* fce = faceID->GetFce();
  nsTTFontFamilyEncoderInfo *ffei =
                     nsFreeType::GetCustomEncoderInfo(fce->mFamilyName);
  if (ffei) {
    platform_id = ffei->mEncodingInfo->mCmapPlatformID;
    encoding_id = ffei->mEncodingInfo->mCmapEncoding;
  }
  for (int i=0; i < face->num_charmaps; i++) {
    if (   (face->charmaps[i]->platform_id == platform_id)
        && (face->charmaps[i]->encoding_id == encoding_id)) {
      fterror = (*nsFreeType::nsFT_Set_Charmap)(face, face->charmaps[i]);
      if (fterror) {
        FREETYPE_PRINTF(("failed to set cmap"));
        (*nsFreeType::nsFT_Done_Face)(face);
        *aFace = nsnull;
      }
    }
  }

  return fterror;
}

nsFreeTypeFace *
nsFreeTypeGetFaceID(nsFontCatalogEntry *aFce)
{
  // We need to have separate keys for the different faces in a ttc file.
  // We append a slash and the face index to the file name to give us a 
  // unique key for each ttc face.
  nsCAutoString key_str(aFce->mFontFileName);
  key_str.Append('/');
  key_str.AppendInt(aFce->mFaceIndex);
  nsCStringKey key(key_str);
  nsFreeTypeFace *face = (nsFreeTypeFace *)gFreeTypeFaces->Get(&key);
  if (!face) {
    face = new nsFreeTypeFace;
    NS_ASSERTION(face, "memory error while creating nsFreeTypeFace");
    if (!face)
      return nsnull;
    nsresult rv = face->Init(aFce);
    if (NS_FAILED(rv))
      return nsnull;
    gFreeTypeFaces->Put(&key, face);
  }
  return face;
}

nsTTFontFamilyEncoderInfo gFontFamilyEncoderInfo[] = {
  { "symbol",         &FEI_Adobe_Symbol_Encoding },
  { "cmr10",          &FEI_x_ttf_cmr,            },
  { "cmmi10",         &FEI_x_ttf_cmmi,           },
  { "cmsy10",         &FEI_x_ttf_cmsy,           },
  { "cmex10",         &FEI_x_ttf_cmex,           },
  { "math1",          &FEI_x_mathematica1,       },
  { "math1-bold",     &FEI_x_mathematica1,       },
  { "math1mono",      &FEI_x_mathematica1,       },
  { "math1mono-bold", &FEI_x_mathematica1,       },
  { "math2",          &FEI_x_mathematica2,       },
  { "math2-bold",     &FEI_x_mathematica2,       },
  { "math2mono",      &FEI_x_mathematica2,       },
  { "math2mono-bold", &FEI_x_mathematica2,       },
  { "ahMn",           &FEI_x_mathematica3,       }, // weird name for Math3
  { "math3",          &FEI_x_mathematica3,       },
  { "math3-bold",     &FEI_x_mathematica3,       },
  { "math3mono",      &FEI_x_mathematica3,       },
  { "math3mono-bold", &FEI_x_mathematica3,       },
  { "math4",          &FEI_x_mathematica4,       },
  { "math4-bold",     &FEI_x_mathematica4,       },
  { "math4mono",      &FEI_x_mathematica4,       },
  { "math4mono-bold", &FEI_x_mathematica4,       },
  { "math5",          &FEI_x_mathematica5,       },
  { "math5-bold",     &FEI_x_mathematica5,       },
  { "math5bold",      &FEI_x_mathematica5,       },
  { "math5mono",      &FEI_x_mathematica5,       },
  { "math5mono-bold", &FEI_x_mathematica5,       },
  { "math5monobold",  &FEI_x_mathematica5,       },
  { "mtextra",        &FEI_x_mtextra,            },
  { "mt extra",       &FEI_x_mtextra,            },
  { "wingdings",      &FEI_windows_1252,         },
  { "webdings",       &FEI_windows_1252,         },
  { nsnull },
};

nsulCodePageRangeCharSetName ulCodePageRange1CharSetNames[] = {
{ TT_OS2_CPR1_LATIN1,       "iso8859-1"         },
{ TT_OS2_CPR1_LATIN2,       "iso8859-2"         },
{ TT_OS2_CPR1_CYRILLIC,     "iso8859-5"         },
{ TT_OS2_CPR1_GREEK,        "iso8859-7"         },
{ TT_OS2_CPR1_TURKISH,      "iso8859-9"         },
{ TT_OS2_CPR1_HEBREW,       "iso8859-8"         },
{ TT_OS2_CPR1_ARABIC,       "iso8859-6"         },
{ TT_OS2_CPR1_BALTIC,       "iso8859-13"        },
{ TT_OS2_CPR1_VIETNAMESE,   "viscii1.1-1"       },
{ TT_OS2_CPR1_THAI,         "tis620.2533-1"     },
{ TT_OS2_CPR1_JAPANESE,     "jisx0208.1990-0"   },
{ TT_OS2_CPR1_CHINESE_SIMP, "gb2312.1980-1"     },
{ TT_OS2_CPR1_KO_WANSUNG,   "ksc5601.1992-3"    },
{ TT_OS2_CPR1_CHINESE_TRAD, "big5-0"            },
{ TT_OS2_CPR1_KO_JOHAB,     "ksc5601.1992-3"    },
{ TT_OS2_CPR1_MAC_ROMAN,    "iso8859-1"         },
{ TT_OS2_CPR1_OEM,          "fontspecific-0"    },
{ TT_OS2_CPR1_SYMBOL,       "fontspecific-0"    },
{ 0,                         nsnull             },
};

nsulCodePageRangeCharSetName ulCodePageRange2CharSetNames[] = {
{ TT_OS2_CPR2_GREEK,        "iso8859-7"         },
{ TT_OS2_CPR2_RUSSIAN,      "koi8-r"            },
{ TT_OS2_CPR2_NORDIC,       "iso8859-10"        },
{ TT_OS2_CPR2_ARABIC,       "iso8859-6"         },
{ TT_OS2_CPR2_CA_FRENCH,    "iso8859-1"         },
{ TT_OS2_CPR2_HEBREW,       "iso8859-8"         },
{ TT_OS2_CPR2_ICELANDIC,    "iso8859-1"         },
{ TT_OS2_CPR2_PORTUGESE,    "iso8859-1"         },
{ TT_OS2_CPR2_TURKISH,      "iso8859-9"         },
{ TT_OS2_CPR2_CYRILLIC,     "iso8859-5"         },
{ TT_OS2_CPR2_LATIN2,       "iso8859-2"         },
{ TT_OS2_CPR2_BALTIC,       "iso8859-4"         },
{ TT_OS2_CPR2_GREEK_437G,   "iso8859-7"         },
{ TT_OS2_CPR2_ARABIC_708,   "iso8859-6"         },
{ TT_OS2_CPR2_WE_LATIN1,    "iso8859-1"         },
{ TT_OS2_CPR2_US,           "iso8859-1"         },
{ 0,                         nsnull             },
};

