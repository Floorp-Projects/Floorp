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
#include "nsCOMPtr.h"
#include "nsICharsetConverterManager.h"
#include "nsIRenderingContext.h"
#include "nsFontDebug.h"
#include "nsFreeType.h"

PRUint32 gFontDebug = 0 | NS_FONT_DEBUG_FONT_SCAN;

#if (!defined(MOZ_ENABLE_FREETYPE2))
// nsFreeType stubs for development systems without a FreeType dev env
nsresult nsFreeTypeInitGlobals() { 
  FREETYPE_FONT_PRINTF(("direct freetype not compiled in"));
  NS_WARNING("direct freetype not compiled in");
  return NS_OK;
};

void nsFreeTypeFreeGlobals() {};

#else
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_TRUETYPE_TABLES_H
#include FT_TRUETYPE_IDS_H

#include "freetype/ftcache.h"
#include "freetype/cache/ftcimage.h"

char*             nsFreeType::gFreeType2SharedLibraryName = nsnull;
PRBool            nsFreeType::gEnableFreeType2 = PR_FALSE;
PRBool            nsFreeType::gFreeType2Autohinted = PR_FALSE;
PRBool            nsFreeType::gFreeType2Unhinted = PR_TRUE;
PRUint8           nsFreeType::gAATTDarkTextMinValue = 64;
double            nsFreeType::gAATTDarkTextGain = 0.8;
PRInt32           nsFreeType::gAntiAliasMinimum = 8;
PRInt32           nsFreeType::gEmbeddedBitmapMaximumHeight = 1000000;


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
  FREETYPE_FONT_PRINTF(("initialize freetype"));

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
  nsFT2FontCatalog::FreeGlobals();
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
    FREETYPE_FONT_PRINTF(("gEnableFreeType2 = %d", gEnableFreeType2));
  }

  rv = mPref->GetCharPref("font.freetype2.shared-library",
                          &gFreeType2SharedLibraryName);
  if (NS_FAILED(rv)) {
    enable_freetype2 = PR_FALSE;
    FREETYPE_FONT_PRINTF((
                  "gFreeType2SharedLibraryName missing, FreeType2 disabled"));
    gFreeType2SharedLibraryName = nsnull;
  }

  PRBool freetype2_autohinted = PR_FALSE;
  rv = mPref->GetBoolPref("font.FreeType2.autohinted", &freetype2_autohinted);
  if (NS_SUCCEEDED(rv)) {
    gFreeType2Autohinted = freetype2_autohinted;
    FREETYPE_FONT_PRINTF(("gFreeType2Autohinted = %d", gFreeType2Autohinted));
  }
  
  PRBool freetype2_unhinted = PR_TRUE;
  rv = mPref->GetBoolPref("font.FreeType2.unhinted", &freetype2_unhinted);
  if (NS_SUCCEEDED(rv)) {
    gFreeType2Unhinted = freetype2_unhinted;
    FREETYPE_FONT_PRINTF(("gFreeType2Unhinted = %d", gFreeType2Unhinted));
  }

  PRInt32 int_val = 0;
  rv = mPref->GetIntPref("font.scale.tt_bitmap.dark_text.min", &int_val);
  if (NS_SUCCEEDED(rv)) {
    gAATTDarkTextMinValue = int_val;
    SIZE_FONT_PRINTF(("gAATTDarkTextMinValue = %d", gAATTDarkTextMinValue));
  }

  nsXPIDLCString str;
  rv = mPref->GetCharPref("font.scale.tt_bitmap.dark_text.gain",
                           getter_Copies(str));
  if (NS_SUCCEEDED(rv)) {
    gAATTDarkTextGain = atof(str.get());
    SIZE_FONT_PRINTF(("gAATTDarkTextGain = %g", gAATTDarkTextGain));
  }

  PRInt32 antialias_minimum = 8;
  rv = mPref->GetIntPref("font.antialias.min", &antialias_minimum);
  if (NS_SUCCEEDED(rv)) {
    gAntiAliasMinimum = antialias_minimum;
    FREETYPE_FONT_PRINTF(("gAntiAliasMinimum = %d", gAntiAliasMinimum));
  }

  PRInt32 embedded_bitmaps_maximum = 1000000;
  rv = mPref->GetIntPref("font.embedded_bitmaps.max",&embedded_bitmaps_maximum);
  if (NS_SUCCEEDED(rv)) {
    gEmbeddedBitmapMaximumHeight = embedded_bitmaps_maximum;
    FREETYPE_FONT_PRINTF(("gEmbeddedBitmapMaximumHeight = %d",
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
    return NS_ERROR_OUT_OF_MEMORY;
  }
  if (!nsFT2FontCatalog::InitGlobals(nsFreeType::GetLibrary())) {
    return NS_ERROR_FAILURE;
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
      FREETYPE_FONT_PRINTF(("\n\n*********\nFreeType initialization error = %d",
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

///////////////////////////////////////////////////////////////////////
//
// class nsFreeTypeFace data/functions
//
///////////////////////////////////////////////////////////////////////

nsFreeTypeFace::nsFreeTypeFace(nsFontCatalogEntry *aFce)
{
  mFce = aFce;
  mCCMap = nsnull;
}

nsFreeTypeFace::~nsFreeTypeFace()
{
  if (mCCMap)
    FreeCCMap(mCCMap);
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
    mCCMap = nsFT2FontCatalog::GetCCMap(mFce);
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
  nsTTFontFamilyEncoderInfo *ffei = nsFT2FontCatalog::GetCustomEncoderInfo(fce);
  if (ffei) {
    platform_id = ffei->mEncodingInfo->mCmapPlatformID;
    encoding_id = ffei->mEncodingInfo->mCmapEncoding;
  }
  for (int i=0; i < face->num_charmaps; i++) {
    if (   (face->charmaps[i]->platform_id == platform_id)
        && (face->charmaps[i]->encoding_id == encoding_id)) {
      fterror = (*nsFreeType::nsFT_Set_Charmap)(face, face->charmaps[i]);
      if (fterror) {
        FREETYPE_FONT_PRINTF(("failed to set cmap"));
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
  nsCAutoString key_str(nsFT2FontCatalog::GetFileName(aFce));
  key_str.Append('/');
  key_str.AppendInt(nsFT2FontCatalog::GetFaceIndex(aFce));
  nsCStringKey key(key_str);
  nsFreeTypeFace *face = (nsFreeTypeFace *)gFreeTypeFaces->Get(&key);
  if (!face) {
    face = new nsFreeTypeFace(aFce);
    NS_ASSERTION(face, "memory error while creating nsFreeTypeFace");
    if (!face)
      return nsnull;
    gFreeTypeFaces->Put(&key, face);
  }
  return face;
}

#endif /* (!defined(MOZ_ENABLE_FREETYPE2)) */
