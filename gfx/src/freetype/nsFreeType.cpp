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

# define FREETYPE_PRINTF(x) \
            PR_BEGIN_MACRO \
              if (gFreeTypeDebug) { \
                printf x ; \
                printf(", %s %d\n", __FILE__, __LINE__); \
              } \
            PR_END_MACRO

PRUint32 gFreeTypeDebug = 0;

//
// these belong in nsFontFreeType
//
PRBool       nsFreeType2::gFreeType2Autohinted = PR_FALSE;
PRBool       nsFreeType2::gFreeType2Unhinted = PR_TRUE;
PRUint8      nsFreeType2::gAATTDarkTextMinValue = 64;
double       nsFreeType2::gAATTDarkTextGain = 0.8;
PRInt32      nsFreeType2::gAntiAliasMinimum = 8;
PRInt32      nsFreeType2::gEmbeddedBitmapMaximumHeight = 1000000;
nsHashtable* nsFreeType2::sFontFamilies = nsnull;
nsHashtable* nsFreeType2::sRange1CharSetNames = nsnull;
nsHashtable* nsFreeType2::sRange2CharSetNames = nsnull;
nsICharsetConverterManager2* nsFreeType2::sCharSetManager = nsnull;

extern nsulCodePageRangeCharSetName ulCodePageRange1CharSetNames[];
extern nsulCodePageRangeCharSetName ulCodePageRange2CharSetNames[];
extern nsTTFontFamilyEncoderInfo    gFontFamilyEncoderInfo[];

typedef int Error;

/*FT_CALLBACK_DEF*/ FT_Error nsFreeTypeFaceRequester(FTC_FaceID, FT_Library, 
                                                 FT_Pointer, FT_Face*);
static FT_Error nsFreeType2__DummyFunc();

static nsHashtable* gFreeTypeFaces = nsnull;

static NS_DEFINE_CID(kCharSetManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);

//
// Define the FreeType2 functions we resolve at run time.
// see the comment near nsFreeType2::DoneFace() for more info
//
#define NS_FT2_OFFSET(f) (int)&((nsFreeType2*)0)->f
FtFuncList nsFreeType2::FtFuncs [] = {
  {"FT_Done_Face",            NS_FT2_OFFSET(nsFT_Done_Face)},
  {"FT_Done_FreeType",        NS_FT2_OFFSET(nsFT_Done_FreeType)},
  {"FT_Done_Glyph",           NS_FT2_OFFSET(nsFT_Done_Glyph)},
  {"FT_Get_Char_Index",       NS_FT2_OFFSET(nsFT_Get_Char_Index)},
  {"FT_Get_Glyph",            NS_FT2_OFFSET(nsFT_Get_Glyph)},
  {"FT_Get_Sfnt_Table",       NS_FT2_OFFSET(nsFT_Get_Sfnt_Table)},
  {"FT_Glyph_Get_CBox",       NS_FT2_OFFSET(nsFT_Glyph_Get_CBox)},
  {"FT_Init_FreeType",        NS_FT2_OFFSET(nsFT_Init_FreeType)},
  {"FT_Load_Glyph",           NS_FT2_OFFSET(nsFT_Load_Glyph)},
  {"FT_New_Face",             NS_FT2_OFFSET(nsFT_New_Face)},
  {"FT_Outline_Decompose",    NS_FT2_OFFSET(nsFT_Outline_Decompose)},
  {"FT_Set_Charmap",          NS_FT2_OFFSET(nsFT_Set_Charmap)},
  {"FTC_Image_Cache_Lookup",  NS_FT2_OFFSET(nsFTC_Image_Cache_Lookup)},
  {"FTC_Manager_Lookup_Size", NS_FT2_OFFSET(nsFTC_Manager_Lookup_Size)},
  {"FTC_Manager_Done",        NS_FT2_OFFSET(nsFTC_Manager_Done)},
  {"FTC_Manager_New",         NS_FT2_OFFSET(nsFTC_Manager_New)},
  {"FTC_Image_Cache_New",     NS_FT2_OFFSET(nsFTC_Image_Cache_New)},
  {nsnull,                    0},
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
// class nsFreeType2 data/functions
//
///////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS1(nsFreeType2, nsIFreeType2);

//
// Since the Freetype2 library may not be available on the user's
// system we cannot link directly to it otherwise the whole app would
// fail to run on those systems.  Instead we access the FreeType2
// functions via function pointers.
//
// If we can load the Freetype2 library with PR_LoadLIbrary we to load 
// pointers to the Ft2 functions. If not, then the function pointers
// point to a dummy function which always returns an error.
//
 
NS_IMETHODIMP
nsFreeType2::DoneFace(FT_Face face)
{ 
  // call the FreeType2 function via the function pointer
  FT_Error error = nsFT_Done_Face(face);
  return error ? NS_ERROR_FAILURE : NS_OK;
} 
 
NS_IMETHODIMP
nsFreeType2::DoneFreeType(FT_Library library)
{ 
  // call the FreeType2 function via the function pointer
  FT_Error error = nsFT_Done_FreeType(library);
  return error ? NS_ERROR_FAILURE : NS_OK;
} 
 
NS_IMETHODIMP
nsFreeType2::DoneGlyph(FT_Glyph glyph)
{ 
  // call the FreeType2 function via the function pointer
  nsFT_Done_Glyph(glyph);
  return NS_OK;
} 
 
NS_IMETHODIMP
nsFreeType2::GetCharIndex(FT_Face face, FT_ULong charcode, FT_UInt *index)
{ 
  // call the FreeType2 function via the function pointer
  *index = nsFT_Get_Char_Index(face, charcode);
  return NS_OK;
} 
 
NS_IMETHODIMP
nsFreeType2::GetGlyph(FT_GlyphSlot slot, FT_Glyph *glyph)
{ 
  // call the FreeType2 function via the function pointer
  FT_Error error = nsFT_Get_Glyph(slot, glyph);
  return error ? NS_ERROR_FAILURE : NS_OK;
} 
 
NS_IMETHODIMP
nsFreeType2::GetSfntTable(FT_Face face, FT_Sfnt_Tag tag, void** table)
{ 
  // call the FreeType2 function via the function pointer
  *table = nsFT_Get_Sfnt_Table(face, tag);
  return NS_OK;
} 
 
NS_IMETHODIMP
nsFreeType2::GlyphGetCBox(FT_Glyph glyph, FT_UInt mode, FT_BBox *bbox)
{ 
  // call the FreeType2 function via the function pointer
  nsFT_Glyph_Get_CBox(glyph, mode, bbox);
  return NS_OK;
} 
 
NS_IMETHODIMP
nsFreeType2::InitFreeType(FT_Library *library)
{ 
  // call the FreeType2 function via the function pointer
  FT_Error error = nsFT_Init_FreeType(library);
  return error ? NS_ERROR_FAILURE : NS_OK;
} 
 
NS_IMETHODIMP
nsFreeType2::LoadGlyph(FT_Face face, FT_UInt glyph, FT_Int flags)
{ 
  // call the FreeType2 function via the function pointer
  FT_Error error = nsFT_Load_Glyph(face, glyph, flags);
  return error ? NS_ERROR_FAILURE : NS_OK;
} 
 
NS_IMETHODIMP
nsFreeType2::NewFace(FT_Library library, const char *path,
                         FT_Long face_index, FT_Face *face)
{ 
  // call the FreeType2 function via the function pointer
  FT_Error error = nsFT_New_Face(library, path, face_index, face);
  return error ? NS_ERROR_FAILURE : NS_OK;
} 
 
NS_IMETHODIMP
nsFreeType2::OutlineDecompose(FT_Outline *outline,
                              const FT_Outline_Funcs *funcs, void *user)
{ 
  // call the FreeType2 function via the function pointer
  FT_Error error = nsFT_Outline_Decompose(outline, funcs, user);
  return error ? NS_ERROR_FAILURE : NS_OK;
} 
 
NS_IMETHODIMP
nsFreeType2::SetCharmap(FT_Face face, FT_CharMap  charmap)
{ 
  // call the FreeType2 function via the function pointer
  FT_Error error = nsFT_Set_Charmap(face, charmap);
  return error ? NS_ERROR_FAILURE : NS_OK;
} 
 
NS_IMETHODIMP
nsFreeType2::ImageCacheLookup(FTC_Image_Cache cache, FTC_Image_Desc *desc,
                              FT_UInt glyphID, FT_Glyph *glyph)
{ 
  // call the FreeType2 function via the function pointer
  FT_Error error = nsFTC_Image_Cache_Lookup(cache, desc, glyphID, glyph);
  return error ? NS_ERROR_FAILURE : NS_OK;
} 
 
NS_IMETHODIMP
nsFreeType2::ManagerLookupSize(FTC_Manager manager, FTC_Font font,
                               FT_Face *face, FT_Size *size)
{ 
  // call the FreeType2 function via the function pointer
  FT_Error error = nsFTC_Manager_Lookup_Size(manager, font, face, size);
  return error ? NS_ERROR_FAILURE : NS_OK;
} 
 
NS_IMETHODIMP
nsFreeType2::ManagerDone(FTC_Manager manager)
{ 
  // call the FreeType2 function via the function pointer
  nsFTC_Manager_Done(manager);
  return NS_OK;
} 
 
NS_IMETHODIMP
nsFreeType2::ManagerNew(FT_Library library, FT_UInt max_faces,
                        FT_UInt max_sizes, FT_ULong max_bytes,
                        FTC_Face_Requester requester, FT_Pointer req_data,
                        FTC_Manager *manager)
{ 
  // call the FreeType2 function via the function pointer
  FT_Error error = nsFTC_Manager_New(library, max_faces, max_sizes, max_bytes,
                                     requester, req_data, manager);
  return error ? NS_ERROR_FAILURE : NS_OK;
} 
 
NS_IMETHODIMP
nsFreeType2::ImageCacheNew(FTC_Manager manager, FTC_Image_Cache *cache)
{ 
  // call the FreeType2 function via the function pointer
  FT_Error error = nsFTC_Image_Cache_New(manager, cache);
  return error ? NS_ERROR_FAILURE : NS_OK;
} 
 
NS_IMETHODIMP
nsFreeType2::GetImageCache(FTC_Image_Cache *aCache)
{
  *aCache = mImageCache;
  return NS_OK;
}

NS_IMETHODIMP
nsFreeType2::GetFTCacheManager(FTC_Manager *aManager)
{ 
  *aManager = mFTCacheManager;
  return NS_OK;
} 
 
NS_IMETHODIMP
nsFreeType2::GetLibrary(FT_Library *aLibrary)
{ 
  *aLibrary = mFreeTypeLibrary;
  return NS_OK;
} 

void
nsFreeType2::ClearFunctions()
{
  FtFuncList *p;
  void *ptr = this;
  for (p=FtFuncs; p->FuncName; p++) {
    *((PRFuncPtr*)((char*)ptr+p->FuncOffset)) = 
                        (PRFuncPtr)&nsFreeType2__DummyFunc;
  }
}

void
nsFreeType2::ClearGlobals()
{
  mSharedLib = nsnull;
  mFreeTypeLibrary = nsnull;
  mFTCacheManager  = nsnull;
  mImageCache      = nsnull;
}

// I would like to make this a static member function but the compilier 
// warning about converting a data pointer to a function pointer cannot
// distinguish static member functions from static data members
static FT_Error
nsFreeType2__DummyFunc()
{
  NS_ERROR("nsFreeType2__DummyFunc should never be called");
  return 1;
}

nsFreeType2::~nsFreeType2()
{
  FreeGlobals();
}

void
nsFreeType2::FreeGlobals()
{
  if (mFreeType2SharedLibraryName) {
    free(mFreeType2SharedLibraryName);
    mFreeType2SharedLibraryName = nsnull;
  }
  if (gFreeTypeFaces) {
    gFreeTypeFaces->Reset(nsFreeTypeFace::FreeFace, nsnull);
    delete gFreeTypeFaces;
    gFreeTypeFaces = nsnull;
  }
  // mImageCache released by cache manager
  if (mFTCacheManager) {
    // use "this->" to make sure it is obivious we are calling the member func
    this->ManagerDone(mFTCacheManager);
    mFTCacheManager = nsnull;
  }
  if (mFreeTypeLibrary) {
    // use "this->" to make sure it is obivious we are calling the member func
    this->DoneFreeType(mFreeTypeLibrary);
    mFreeTypeLibrary = nsnull;
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
nsFreeType2::Init()
{
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
    mEnableFreeType2 = enable_freetype2;
    FREETYPE_PRINTF(("mEnableFreeType2 = %d", mEnableFreeType2));
  }

  rv = mPref->GetCharPref("font.freetype2.shared-library",
                          &mFreeType2SharedLibraryName);
  if (NS_FAILED(rv)) {
    enable_freetype2 = PR_FALSE;
    FREETYPE_PRINTF((
                  "mFreeType2SharedLibraryName missing, FreeType2 disabled"));
    mFreeType2SharedLibraryName = nsnull;
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
    mEnableFreeType2 = PR_FALSE;
    mFreeType2SharedLibraryName = nsnull;
    gFreeType2Autohinted = PR_FALSE;
    gFreeType2Unhinted = PR_TRUE;
    gAATTDarkTextMinValue = 64;
    gAATTDarkTextGain = 0.8;
    gAntiAliasMinimum = 8;
    gEmbeddedBitmapMaximumHeight = 1000000;
  }
  
  mPref = nsnull;
  
  if (!InitLibrary()) {
    FreeGlobals();
    return NS_ERROR_OUT_OF_MEMORY;
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
nsFreeType2::InitLibrary()
{
  FT_Error error;
#ifdef MOZ_MATHML
  // do not yet support MathML
  // goto cleanup_and_return;
#endif

  if (!mEnableFreeType2)
    return PR_FALSE;

  // since the library may not be available on any given system
  // failing to load is not considered a fatal error
  if (!LoadSharedLib())
    return PR_FALSE;

  // use "this->" to make sure it is obivious we are calling the member func
  nsresult rv = this->InitFreeType(&mFreeTypeLibrary);
  if (NS_FAILED(rv)) {
    FREETYPE_PRINTF(("\n\n*********\nFreeType initialization error = %d",
                         error));
    mFreeTypeLibrary = nsnull;
    goto cleanup_and_return;
  }
  // use "this->" to make sure it is obivious we are calling the member func
  rv = this->ManagerNew(mFreeTypeLibrary, 0, 0, 0, nsFreeTypeFaceRequester,
                         this, &mFTCacheManager);
  NS_ASSERTION(NS_SUCCEEDED(rv), "failed to create FreeType Cache manager");
  if (NS_FAILED(rv))
    goto cleanup_and_return;
  // use "this->" to make sure it is obivious we are calling the member func
  rv = this->ImageCacheNew(mFTCacheManager, &mImageCache);
  NS_ASSERTION(NS_SUCCEEDED(rv), "failed to create FreeType image cache");
  if (NS_FAILED(rv))
    goto cleanup_and_return;
  return PR_TRUE;

cleanup_and_return:
  // clean everything up but note that init was called
  FreeGlobals();
  return(PR_FALSE);
}

PRBool
nsFreeType2::LoadSharedLib()
{
  NS_ASSERTION(mSharedLib==nsnull, "library already loaded");

  if (!mFreeType2SharedLibraryName)
    return PR_FALSE;
  mSharedLib = PR_LoadLibrary(mFreeType2SharedLibraryName);
  // since the library may not be available on any given system
  // failing to load is not considered a fatal error
  if (!mSharedLib) {
    NS_WARNING("freetype library not found");
    return PR_FALSE;
  }

  FtFuncList *p;
  PRFuncPtr func;
  void *ptr = this;
  for (p=FtFuncs; p->FuncName; p++) {
    func = PR_FindFunctionSymbol(mSharedLib, p->FuncName);
    if (!func) {
      NS_WARNING("nsFreeType2::LoadSharedLib Error");
      ClearFunctions();
      return PR_FALSE;
    }
    *((PRFuncPtr*)((char*)ptr+p->FuncOffset)) = func;
  }

  return PR_TRUE;

}

void
nsFreeType2::UnloadSharedLib()
{
  if (mSharedLib)
    PR_UnloadLibrary(mSharedLib);
  mSharedLib = nsnull;
}

const char *
nsFreeType2::GetRange1CharSetName(unsigned long aBit)
{
  char buf[32];
  sprintf(buf, "0x%08lx", aBit);
  nsCStringKey key(buf);
  const char *charsetName = (const char *)sRange1CharSetNames->Get(&key);
  return charsetName;
}

const char *
nsFreeType2::GetRange2CharSetName(unsigned long aBit)
{
  char buf[32];
  sprintf(buf, "0x%08lx", aBit);
  nsCStringKey key(buf);
  const char *charsetName = (const char *)sRange2CharSetNames->Get(&key);
  return charsetName;
}

nsTTFontFamilyEncoderInfo*
nsFreeType2::GetCustomEncoderInfo(const char * aFamilyName)
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
nsFreeType2::GetCharSetManager()
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
nsFreeType2::GetCCMap(nsFontCatalogEntry *aFce)
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
  *ccMaps = nsFreeType2::GetCCMap(mFce);
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
    mCCMap = nsFreeType2::GetCCMap(mFce);
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
  FT_Error fterror = 0;
  nsFreeType2 *ft2 = (nsFreeType2 *)request_data;
  nsresult rv;

  rv = ft2->NewFace(lib, faceID->GetFilename(), faceID->GetFaceIndex(), aFace);
  if (NS_FAILED(rv))
    return fterror;

  FT_Face face = *aFace;
  FT_UShort platform_id = TT_PLATFORM_MICROSOFT;
  FT_UShort encoding_id = TT_MS_ID_UNICODE_CS;
  nsFontCatalogEntry* fce = faceID->GetFce();
  nsTTFontFamilyEncoderInfo *ffei =
                     nsFreeType2::GetCustomEncoderInfo(fce->mFamilyName);
  if (ffei) {
    platform_id = ffei->mEncodingInfo->mCmapPlatformID;
    encoding_id = ffei->mEncodingInfo->mCmapEncoding;
  }
  for (int i=0; i < face->num_charmaps; i++) {
    if (   (face->charmaps[i]->platform_id == platform_id)
        && (face->charmaps[i]->encoding_id == encoding_id)) {
      rv = ft2->SetCharmap(face, face->charmaps[i]);
      if (NS_FAILED(rv)) {
        FREETYPE_PRINTF(("failed to set cmap"));
        ft2->DoneFace(face);
        *aFace = nsnull;
        fterror = 1;
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

