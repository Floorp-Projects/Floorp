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

#include "nspr.h"
#include "nsCOMPtr.h"
#include "nsHashtable.h"
#include "nsICharsetConverterManager.h"
#include "nsFontMetricsGTK.h"
#include "nsIRenderingContext.h"
#include "nsFreeType.h"
#include "nsFT2FontCatalog.h"

#ifdef DEBUG
// set this to 1 to have the code draw the bounding boxes
#define DEBUG_SHOW_GLYPH_BOX 0
#endif
#include "nsX11AlphaBlend.h"
#include "nsAntiAliasedGlyph.h"

extern PRUint8 gAATTDarkTextMinValue;
extern double  gAATTDarkTextGain;

#if (!defined(MOZ_ENABLE_FREETYPE2))
// nsFreeType stubs for development systems without a FreeType dev env
nsresult nsFreeTypeInitGlobals() { 
  FREETYPE_FONT_PRINTF(("freetype not compiled in"));
  NS_WARNING("freetype not compiled in");
  return NS_OK;
};

nsFreeTypeFont *
nsFreeTypeFont::NewFont(nsFreeTypeFace *, PRUint16, const char *)
{
  return nsnull;
}

void nsFreeTypeFreeGlobals() {};

#else
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_TRUETYPE_TABLES_H
#include FT_TRUETYPE_IDS_H

#include "freetype/ftcache.h"
#include "freetype/cache/ftcimage.h"

extern const char *gFreeType2SharedLibraryName;

// macros to handle FreeType2 26.6 numbers (26 bit number with 6 bit fraction)
#define FT_CEIL(x) (((x) + 63) & -64)
#define FT_FLOOR(x) ((x) & -64)
#define FT_TRUNC(x) ((x) >> 6)

// macros to handle FreeType2 16.16 numbers
#define FT_16_16_TO_REG(x) ((x)>>16)

#define FREE_IF(x) if(x) free((void*)x)
#define IMAGE_BUFFER_SIZE 2048

typedef int Error;

extern PRInt32 gAntiAliasMinimum;
extern PRInt32 gEmbeddedBitmapMaximumHeight;
extern PRBool  gEnableFreeType2;
extern PRBool  gFreeType2Autohinted;
extern PRBool  gFreeType2Unhinted;

/*FT_CALLBACK_DEF*/ FT_Error nsFreeTypeFaceRequester(FTC_FaceID, FT_Library, 
                                                 FT_Pointer, FT_Face*);
static FT_Error nsFreeType__DummyFunc();

PRUint32 deltaMicroSeconds(PRTime aStartTime, PRTime aEndTime);
void GetFallbackGlyphMetrics(FT_BBox *aBoundingBox, FT_Face aFace);
static void WeightTableInitCorrection(PRUint8*, PRUint8, double);

static nsHashtable* gFreeTypeFaces = nsnull;
PRUint8  sLinearWeightTable[256];
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
FT_Done_Face_t            nsFreeTypeFont::nsFT_Done_Face;
FT_Done_FreeType_t        nsFreeTypeFont::nsFT_Done_FreeType;
FT_Done_Glyph_t           nsFreeTypeFont::nsFT_Done_Glyph;
FT_Get_Char_Index_t       nsFreeTypeFont::nsFT_Get_Char_Index;
FT_Get_Glyph_t            nsFreeTypeFont::nsFT_Get_Glyph;
FT_Get_Sfnt_Table_t       nsFreeTypeFont::nsFT_Get_Sfnt_Table;
FT_Glyph_Get_CBox_t       nsFreeTypeFont::nsFT_Glyph_Get_CBox;
FT_Init_FreeType_t        nsFreeTypeFont::nsFT_Init_FreeType;
FT_Load_Glyph_t           nsFreeTypeFont::nsFT_Load_Glyph;
FT_New_Face_t             nsFreeTypeFont::nsFT_New_Face;
FT_Set_Charmap_t          nsFreeTypeFont::nsFT_Set_Charmap;
FTC_Image_Cache_Lookup_t  nsFreeTypeFont::nsFTC_Image_Cache_Lookup;
FTC_Manager_Lookup_Size_t nsFreeTypeFont::nsFTC_Manager_Lookup_Size;
FTC_Manager_Done_t        nsFreeTypeFont::nsFTC_Manager_Done;
FTC_Manager_New_t         nsFreeTypeFont::nsFTC_Manager_New;
FTC_Image_Cache_New_t     nsFreeTypeFont::nsFTC_Image_Cache_New;


typedef struct {
  const char *FuncName;
  PRFuncPtr  *FuncPtr;
} FtFuncList;

static FtFuncList FtFuncs [] = {
  {"FT_Done_Face",            (PRFuncPtr*)&nsFreeTypeFont::nsFT_Done_Face},
  {"FT_Done_FreeType",        (PRFuncPtr*)&nsFreeTypeFont::nsFT_Done_FreeType},
  {"FT_Done_Glyph",           (PRFuncPtr*)&nsFreeTypeFont::nsFT_Done_Glyph},
  {"FT_Get_Char_Index",       (PRFuncPtr*)&nsFreeTypeFont::nsFT_Get_Char_Index},
  {"FT_Get_Glyph",            (PRFuncPtr*)&nsFreeTypeFont::nsFT_Get_Glyph},
  {"FT_Get_Sfnt_Table",       (PRFuncPtr*)&nsFreeTypeFont::nsFT_Get_Sfnt_Table},
  {"FT_Glyph_Get_CBox",       (PRFuncPtr*)&nsFreeTypeFont::nsFT_Glyph_Get_CBox},
  {"FT_Init_FreeType",        (PRFuncPtr*)&nsFreeTypeFont::nsFT_Init_FreeType},
  {"FT_Load_Glyph",           (PRFuncPtr*)&nsFreeTypeFont::nsFT_Load_Glyph},
  {"FT_New_Face",             (PRFuncPtr*)&nsFreeTypeFont::nsFT_New_Face},
  {"FT_Set_Charmap",          (PRFuncPtr*)&nsFreeTypeFont::nsFT_Set_Charmap},
  {"FTC_Image_Cache_Lookup",  (PRFuncPtr*)&nsFreeTypeFont::nsFTC_Image_Cache_Lookup},
  {"FTC_Manager_Lookup_Size", (PRFuncPtr*)&nsFreeTypeFont::nsFTC_Manager_Lookup_Size},
  {"FTC_Manager_Done",        (PRFuncPtr*)&nsFreeTypeFont::nsFTC_Manager_Done},
  {"FTC_Manager_New",         (PRFuncPtr*)&nsFreeTypeFont::nsFTC_Manager_New},
  {"FTC_Image_Cache_New",     (PRFuncPtr*)&nsFreeTypeFont::nsFTC_Image_Cache_New},
  {nsnull,                    (PRFuncPtr*)nsnull},
};

extern PRUint32 gFontDebug;

///////////////////////////////////////////////////////////////////////
//
// class nsFreeType class definition
//
///////////////////////////////////////////////////////////////////////
class nsFreeType {
public:
  inline PRBool FreeTypeAvailable() { return sAvailable; };
  inline static FTC_Manager GetFTCacheManager() { return sFTCacheManager; };
  inline static FT_Library GetLibrary() { return sFreeTypeLibrary; };
  inline static FTC_Image_Cache GetImageCache() { return sImageCache; };
  static void FreeGlobals();
  static nsresult InitGlobals();

protected:
  static void ClearGlobals();
  static void ClearFunctions();
  static PRBool InitLibrary();
  static PRBool LoadSharedLib();
  static void UnloadSharedLib();

  static PRLibrary      *sSharedLib;
  static PRBool          sInited;
  static PRBool          sAvailable;
  static FT_Error        sInitError;
  static FT_Library      sFreeTypeLibrary;
  static FTC_Manager     sFTCacheManager;
  static FTC_Image_Cache sImageCache;
};

///////////////////////////////////////////////////////////////////////
//
// class nsFreeTypeFace definition
//
///////////////////////////////////////////////////////////////////////
/* this simple record is used to model a given `installed' face */
class nsFreeTypeFace {
public:
  nsFreeTypeFace(nsFontCatalogEntry *aFce);
  ~nsFreeTypeFace();
  static PRBool FreeFace(nsHashKey* aKey, void* aData, void* aClosure);
  const char *GetFilename() { return nsFT2FontCatalog::GetFileName(mFce); }
  int *GetEmbeddedBitmapHeights() { 
                  return nsFT2FontCatalog::GetEmbeddedBitmapHeights(mFce); } ;
  int GetFaceIndex() { return nsFT2FontCatalog::GetFaceIndex(mFce); }
  int GetNumEmbeddedBitmaps() { 
                  return nsFT2FontCatalog::GetNumEmbeddedBitmaps(mFce); } ;
  PRUint16 *GetCCMap();
  nsFontCatalogEntry* GetFce() { return mFce; };

protected:
  nsFontCatalogEntry *mFce;
  PRUint16           *mCCMap;

};

///////////////////////////////////////////////////////////////////////
//
// class nsFreeTypeXImage definition
//
///////////////////////////////////////////////////////////////////////
class nsFreeTypeXImage : public nsFreeTypeFont {
public:
  nsFreeTypeXImage(nsFreeTypeFace *aFaceID, PRUint16 aPixelSize,
                   const char *aName);
  gint DrawString(nsRenderingContextGTK* aContext,
                            nsDrawingSurfaceGTK* aSurface, nscoord aX,
                            nscoord aY, const PRUnichar* aString,
                            PRUint32 aLength);
protected:
  nsFreeTypeXImage();
};

///////////////////////////////////////////////////////////////////////
//
// class nsFreeTypeXImageSBC (Single Byte Converter) definition
//
///////////////////////////////////////////////////////////////////////
class nsFreeTypeXImageSBC : public nsFreeTypeXImage {
public:
  nsFreeTypeXImageSBC(nsFreeTypeFace *aFaceID, PRUint16 aPixelSize,
                      const char *aName);
#ifdef MOZ_MATHML
  virtual nsresult GetBoundingMetrics(const PRUnichar*   aString,
                                      PRUint32           aLength,
                                      nsBoundingMetrics& aBoundingMetrics);
#endif

  virtual gint GetWidth(const PRUnichar* aString, PRUint32 aLength);
  virtual gint DrawString(nsRenderingContextGTK* aContext,
                          nsDrawingSurfaceGTK* aSurface, nscoord aX,
                          nscoord aY, const PRUnichar* aString,
                          PRUint32 aLength);
protected:
  nsFreeTypeXImageSBC();
};

///////////////////////////////////////////////////////////////////////
//
// class nsFreeTypeFont data/functions
//
///////////////////////////////////////////////////////////////////////

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

#ifdef ENABLE_TIME_MACROS
PRUint32
deltaMicroSeconds(PRTime aStartTime, PRTime aEndTime)
{
  PRUint32 delta;
  PRUint64 loadTime64;

  LL_SUB(loadTime64, aEndTime, aStartTime);
  LL_L2UI(delta, loadTime64);

  return delta;
}
#endif

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
  if (gFreeTypeFaces) {
    gFreeTypeFaces->Reset(nsFreeTypeFace::FreeFace, nsnull);
    delete gFreeTypeFaces;
    gFreeTypeFaces = nsnull;
  }
  // sImageCache released by cache manager
  if (sFTCacheManager) {
    (*nsFreeTypeFont::nsFTC_Manager_Done)(sFTCacheManager);
    sFTCacheManager = nsnull;
  }
  if (sFreeTypeLibrary) {
    (*nsFreeTypeFont::nsFT_Done_FreeType)(sFreeTypeLibrary);
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

  // setup the weighting table
  WeightTableInitCorrection(sLinearWeightTable,
                            gAATTDarkTextMinValue, gAATTDarkTextGain);
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

    sInitError = (*nsFreeTypeFont::nsFT_Init_FreeType)(&sFreeTypeLibrary);
    if (sInitError) {
      FREETYPE_FONT_PRINTF(("\n\n*********\nFreeType initialization error = %d",
                          sInitError));
      sFreeTypeLibrary = nsnull;
      goto cleanup_and_return;
    }
    FT_Error error = (*nsFreeTypeFont::nsFTC_Manager_New)(sFreeTypeLibrary,
                       0, 0, 0, nsFreeTypeFaceRequester, 0, &sFTCacheManager);
    NS_ASSERTION(error==0, "failed to create FreeType Cache manager");
    if (error)
      goto cleanup_and_return;
    error = (*nsFreeTypeFont::nsFTC_Image_Cache_New)(sFTCacheManager,
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
// class nsFreeTypeFont data/functions
//
///////////////////////////////////////////////////////////////////////
nsFreeTypeFont::nsFreeTypeFont()
{
  NS_ERROR("should never call nsFreeTypeFont::nsFreeTypeFont");
}

nsFreeTypeFont *
nsFreeTypeFont::NewFont(nsFreeTypeFace *aFaceID, PRUint16 aPixelSize, 
                        const char *aName)
{
  // for now we only support ximage (XGetImage/alpha-blend/XPutImage) display
  // when we support XRender then we will need to test if it is
  // available and if so use it since it is faster than ximage.
  PRBool ximage = PR_TRUE;
  PRBool render = PR_FALSE;
  nsFreeTypeFont *ftfont;
  nsFontCatalogEntry* fce = aFaceID->GetFce();
  nsTTFontFamilyEncoderInfo *ffei = nsFT2FontCatalog::GetCustomEncoderInfo(fce);
  if (ximage) {
    if (ffei) {
      ftfont = new nsFreeTypeXImageSBC(aFaceID, aPixelSize, aName);
    }
    else {
      ftfont = new nsFreeTypeXImage(aFaceID, aPixelSize, aName);
    }
    return ftfont;
  }
  else if (render) {
    NS_ERROR("need to construct a render type FreeType object");
    return nsnull;
  }
  NS_ERROR("need to construct other type FreeType objects");
  return nsnull;
}

FT_Face
nsFreeTypeFont::getFTFace()
{
  FT_Face face = nsnull;
  FT_Error error = (*nsFreeTypeFont::nsFTC_Manager_Lookup_Size)(
                                     nsFreeType::GetFTCacheManager(),
                                     &mImageDesc.font, &face, nsnull);
  NS_ASSERTION(error==0, "failed to get face/size");
  if (error)
    return nsnull;
  return face;
}

nsFreeTypeFont::nsFreeTypeFont(nsFreeTypeFace *aFaceID,
                               PRUint16 aPixelSize, const char *aName)
{
  PRBool anti_alias = PR_TRUE;
  PRBool embedded_bimap = PR_FALSE;
  mFaceID = aFaceID;
  mPixelSize = aPixelSize;
  mImageDesc.font.face_id    = (void*)mFaceID;
  mImageDesc.font.pix_width  = aPixelSize;
  mImageDesc.font.pix_height = aPixelSize;
  mImageDesc.image_type = 0;

  if (aPixelSize < gAntiAliasMinimum) {
    mImageDesc.image_type |= ftc_image_mono;
    anti_alias = PR_FALSE;
  }

  if (gFreeType2Autohinted)
    mImageDesc.image_type |= ftc_image_flag_autohinted;

  if (gFreeType2Unhinted)
    mImageDesc.image_type |= ftc_image_flag_unhinted;

  // check if we have an embedded bitmap
  if (aPixelSize<=gEmbeddedBitmapMaximumHeight) {
    int num_embedded_bitmaps = mFaceID->GetNumEmbeddedBitmaps();
    if (num_embedded_bitmaps) {
      int *embedded_bitmapheights = mFaceID->GetEmbeddedBitmapHeights();
      for (int i=0; i<num_embedded_bitmaps; i++) {
        if (embedded_bitmapheights[i] == aPixelSize) {
          embedded_bimap = PR_TRUE;
          // unhinted must be set for embedded bitmaps to be used
          mImageDesc.image_type |= ftc_image_flag_unhinted;
          break;
        }
      }
    }
  }

  FREETYPE_FONT_PRINTF(("anti_alias=%d, embedded_bitmap=%d, "
                          "AutoHinted=%d, gFreeType2Unhinted = %d, "
                          "size=%dpx, \"%s\"", 
                          anti_alias, embedded_bimap, 
                          gFreeType2Autohinted, gFreeType2Unhinted,
                          aPixelSize, aName));
}

void
nsFreeTypeFont::LoadFont()
{
  if (mAlreadyCalledLoadFont) {
    return;
  }

  mAlreadyCalledLoadFont = PR_TRUE;
  mCCMap = mFaceID->GetCCMap();
#ifdef NS_FONT_DEBUG_LOAD_FONT
  if (gFontDebug & NS_FONT_DEBUG_LOAD_FONT) {
    printf("loaded \"%s\", size=%d, filename=%s\n", 
                 mName, mSize, mFaceID->GetFilename());
  }    
#endif
}

nsFreeTypeFont::~nsFreeTypeFont()
{
}

#ifdef MOZ_MATHML
nsresult
nsFreeTypeFont::GetBoundingMetrics(const PRUnichar*   aString,
                                   PRUint32           aLength,
                                   nsBoundingMetrics& aBoundingMetrics)
{
  return doGetBoundingMetrics(aString, aLength, 
                              &aBoundingMetrics.leftBearing,
                              &aBoundingMetrics.rightBearing,
                              &aBoundingMetrics.ascent,
                              &aBoundingMetrics.descent,
                              &aBoundingMetrics.width);
}
#endif



nsresult
nsFreeTypeFont::doGetBoundingMetrics(const PRUnichar* aString, PRUint32 aLength,
                                     PRInt32* aLeftBearing,
                                     PRInt32* aRightBearing,
                                     PRInt32* aAscent,
                                     PRInt32* aDescent,
                                     PRInt32* aWidth)
{
  *aLeftBearing = 0;
  *aRightBearing = 0;
  *aAscent = 0;
  *aDescent = 0;
  *aWidth = 0;

  if (aLength < 1) {
    return NS_ERROR_FAILURE;
  }

  FT_Pos pos = 0;
  FT_BBox bbox;
  // initialize to "uninitialized" values
  bbox.xMin = bbox.yMin = 32000;
  bbox.xMax = bbox.yMax = -32000;

  // get the face/size from the FreeType cache
  FT_Face face = getFTFace();
  NS_ASSERTION(face, "failed to get face/size");
  if (!face)
    return NS_ERROR_FAILURE;

  // get the text size
  PRUint32 i;
  for (i=0; i<aLength; i++) {
    FT_UInt glyph_index;
    FT_Glyph glyph;
    FT_Error error;
    FT_BBox glyph_bbox;
    FT_Pos advance;
    glyph_index = (*nsFreeTypeFont::nsFT_Get_Char_Index)(face, 
                                  (FT_ULong)aString[i]);
    //NS_ASSERTION(glyph_index,"failed to get glyph");
    if (glyph_index) {
      error = (*nsFreeTypeFont::nsFTC_Image_Cache_Lookup)(
                                   nsFreeType::GetImageCache(),
                                   &mImageDesc, glyph_index, &glyph);
      NS_ASSERTION(error==0,"error loading glyph");
    }
    if ((glyph_index) && (!error)) {
      (*nsFreeTypeFont::nsFT_Glyph_Get_CBox)(glyph, ft_glyph_bbox_pixels, 
                                             &glyph_bbox);
      advance = FT_16_16_TO_REG(glyph->advance.x);
    }
    else {
      // allocate space to draw an empty box in
      GetFallbackGlyphMetrics(&glyph_bbox, face);
      advance = glyph_bbox.xMax + 1;
    }
    bbox.xMin = MIN(pos+glyph_bbox.xMin, bbox.xMin);
    bbox.xMax = MAX(pos+glyph_bbox.xMax, bbox.xMax);
    bbox.yMin = MIN(glyph_bbox.yMin, bbox.yMin);
    bbox.yMax = MAX(glyph_bbox.yMax, bbox.yMax);
    pos += advance;
  }

  // check we got at least one size
  if (bbox.xMin > bbox.xMax)
    bbox.xMin = bbox.xMax = bbox.yMin = bbox.yMax = 0;

  *aLeftBearing  = bbox.xMin;
  *aRightBearing = bbox.xMax;
  *aAscent       = bbox.yMax;
  *aDescent      = -bbox.yMin;
  *aWidth        = pos;
  return NS_OK;
}

GdkFont*
nsFreeTypeFont::GetGDKFont()
{
  return nsnull;
}

PRBool
nsFreeTypeFont::GetGDKFontIs10646()
{
  return PR_TRUE;
}

PRBool
nsFreeTypeFont::IsFreeTypeFont()
{
  return PR_TRUE;
}

gint
nsFreeTypeFont::GetWidth(const PRUnichar* aString, PRUint32 aLength)
{
  FT_UInt glyph_index;
  FT_Glyph glyph;
  FT_Pos origin_x = 0;

  // get the face/size from the FreeType cache
  FT_Face face = getFTFace();
  NS_ASSERTION(face, "failed to get face/size");
  if (!face)
    return 0;

  for (PRUint32 i=0; i<aLength; i++) {
    glyph_index = (*nsFreeTypeFont::nsFT_Get_Char_Index)((FT_Face)face, 
                                   (FT_ULong)aString[i]);
    FT_Error error = (*nsFreeTypeFont::nsFTC_Image_Cache_Lookup)(
                                   nsFreeType::GetImageCache(),
                                   &mImageDesc, glyph_index, &glyph);
    NS_ASSERTION(error==0,"error loading glyph");
    if (error) {
      origin_x += face->size->metrics.x_ppem/2 + 2;
      continue;
    }
    origin_x += FT_16_16_TO_REG(glyph->advance.x);
  }

  return origin_x;
}

gint
nsFreeTypeFont::DrawString(nsRenderingContextGTK* aContext,
                            nsDrawingSurfaceGTK* aSurface, nscoord aX,
                            nscoord aY, const PRUnichar* aString,
                            PRUint32 aLength)
{
  NS_ERROR("should never call nsFreeTypeFont::DrawString");
  return 0;
}

PRUint32
nsFreeTypeFont::Convert(const PRUnichar* aSrc, PRUint32 aSrcLen,
                           PRUnichar* aDest, PRUint32 aDestLen)
{
  NS_ERROR("should not be calling nsFreeTypeFont::Convert");
  return 0;
}

int
nsFreeTypeFont::ascent()
{
  int ascent;
  FT_Face face = getFTFace();
  NS_ASSERTION(face, "failed to get face/size");
  if (!face)
    return 0;
  ascent = FT_16_16_TO_REG(face->ascender * face->size->metrics.y_scale);
  ascent = FT_CEIL(ascent);
  ascent = FT_TRUNC(ascent);
  return ascent;
}

int
nsFreeTypeFont::descent()
{
  int descent;
  FT_Face face = getFTFace();
  NS_ASSERTION(face, "failed to get face/size");
  if (!face)
    return 0;
  descent = FT_16_16_TO_REG(-face->descender * face->size->metrics.y_scale);
  descent = FT_CEIL(descent);
  descent = FT_TRUNC(descent);
  return descent;
}

int
nsFreeTypeFont::max_ascent()
{
  int max_ascent;
  FT_Face face = getFTFace();
  NS_ASSERTION(face, "failed to get face/size");
  if (!face)
    return 0;

  TT_OS2 * tt_os2 = (TT_OS2 *)(*nsFreeTypeFont::nsFT_Get_Sfnt_Table)(face,
                                                             ft_sfnt_os2);
  NS_ASSERTION(tt_os2, "unable to get OS2 table");
  if (tt_os2)
    max_ascent = FT_16_16_TO_REG(tt_os2->sTypoAscender
                                 * face->size->metrics.y_scale);
  else
    max_ascent = FT_16_16_TO_REG(face->bbox.yMax * face->size->metrics.y_scale);
  max_ascent = FT_CEIL(max_ascent);
  max_ascent = FT_TRUNC(max_ascent);
  return max_ascent;
}

int
nsFreeTypeFont::max_descent()
{
  int max_descent;
  FT_Face face = getFTFace();
  NS_ASSERTION(face, "failed to get face/size");
  if (!face)
    return 0;

  TT_OS2 *tt_os2 = (TT_OS2 *)(*nsFreeTypeFont::nsFT_Get_Sfnt_Table)(face,
                                                             ft_sfnt_os2);
  NS_ASSERTION(tt_os2, "unable to get OS2 table");
  if (tt_os2)
    max_descent = FT_16_16_TO_REG(-tt_os2->sTypoDescender *
                                  face->size->metrics.y_scale);
  else
    max_descent = FT_16_16_TO_REG(-face->bbox.yMin *
                                  face->size->metrics.y_scale);
  max_descent = FT_CEIL(max_descent);
  max_descent = FT_TRUNC(max_descent);
  return max_descent;
}

int
nsFreeTypeFont::max_width()
{
  int max_advance_width;
  FT_Face face = getFTFace();
  NS_ASSERTION(face, "failed to get face/size");
  if (!face)
    return 0;
  max_advance_width = FT_16_16_TO_REG(face->max_advance_width *
                                      face->size->metrics.x_scale);
  max_advance_width = FT_CEIL(max_advance_width);
  max_advance_width = FT_TRUNC(max_advance_width);
  return max_advance_width;
}

PRBool
nsFreeTypeFont::getXHeight(unsigned long &val)
{
  int height;
  FT_Face face = getFTFace();
  NS_ASSERTION(face, "failed to get face/size");
  if (!face || !val)
    return PR_FALSE;
  height = FT_16_16_TO_REG(face->height * face->size->metrics.x_scale);
  height = FT_CEIL(height);
  height = FT_TRUNC(height);

  val = height;
  return PR_TRUE;
}

PRBool
nsFreeTypeFont::underlinePosition(long &val)
{
  long underline_position;
  FT_Face face = getFTFace();
  NS_ASSERTION(face, "failed to get face/size");
  if (!face)
    return PR_FALSE;
  underline_position = FT_16_16_TO_REG(-face->underline_position * 
                        face->size->metrics.x_scale);
  underline_position = FT_CEIL(underline_position);
  underline_position = FT_TRUNC(underline_position);
  val = underline_position;
  return PR_TRUE;
}

PRBool
nsFreeTypeFont::underline_thickness(unsigned long &val)
{
  unsigned long underline_thickness;
  FT_Face face = getFTFace();
  NS_ASSERTION(face, "failed to get face/size");
  if (!face)
    return PR_FALSE;
  underline_thickness = FT_16_16_TO_REG(face->underline_thickness *
                         face->size->metrics.x_scale);
  underline_thickness = FT_CEIL(underline_thickness);
  underline_thickness = FT_TRUNC(underline_thickness);
  val = underline_thickness;
  return PR_TRUE;
}

PRBool
nsFreeTypeFont::superscript_y(long &val)
{
  return PR_FALSE;
}

PRBool
nsFreeTypeFont::subscript_y(long &val)
{
  return PR_FALSE;
}

///////////////////////////////////////////////////////////////////////
//
// class nsFreeTypeRender
//
///////////////////////////////////////////////////////////////////////

// this needs to be written
class nsFreeTypeRender : nsFreeTypeFont {
private:
  nsFreeTypeRender();
};

///////////////////////////////////////////////////////////////////////
//
// class nsFreeTypeXImage data/functions
//
///////////////////////////////////////////////////////////////////////

nsFreeTypeXImage::nsFreeTypeXImage()
{
  NS_ERROR("should never call nsFreeTypeXImage::nsFreeTypeXImage");
}

nsFreeTypeXImage::nsFreeTypeXImage(nsFreeTypeFace *aFaceID, PRUint16 aPixelSize,
                                   const char *aName)
: nsFreeTypeFont(aFaceID, aPixelSize, aName)
{
  //NS_ERROR("should never call nsFreeTypeXImage::nsFreeTypeXImage");
}

gint
nsFreeTypeXImage::DrawString(nsRenderingContextGTK* aContext,
                            nsDrawingSurfaceGTK* aSurface, nscoord aX,
                            nscoord aY, const PRUnichar* aString,
                            PRUint32 aLength)
{

#if DEBUG_SHOW_GLYPH_BOX
  PRUint32 x, y;
  // grey shows image size
  // red shows character cells
  // green box shows text ink
#endif

  if (aLength < 1) {
    return 0;
  }

  // get the face/size from the FreeType cache
  FT_Face face = getFTFace();
  NS_ASSERTION(face, "failed to get face/size");
  if (!face)
    return 0;

  nsresult rslt;
  PRInt32 leftBearing, rightBearing, ascent, descent, width;
  rslt = doGetBoundingMetrics(aString, aLength, &leftBearing, &rightBearing, 
                              &ascent, &descent, &width);
  if (NS_FAILED(rslt))
    return 0;

  // make sure we bring down enough background for blending
  rightBearing = MAX(rightBearing, width+1);

  // offset in the ximage to the x origin
  PRInt32 x_origin = MAX(0, -leftBearing);
  // offset in the ximage to the x origin
  PRInt32 y_origin = ascent;
  PRInt32 x_pos = x_origin;

  int image_width  = x_origin + rightBearing;
  int image_height = y_origin + MAX(descent, 0);
  if ((image_width<=0) || (image_height<=0)) {
    // if we do not have any pixels then no point in trying to draw
    // eg: the space char has 0 height
    return 0;
  }
  Display *dpy = GDK_DISPLAY();
  Drawable win = GDK_WINDOW_XWINDOW(aSurface->GetDrawable());
  GC gc = GDK_GC_XGC(aContext->GetGC());
  XGCValues values;
  if (!XGetGCValues(dpy, gc, GCForeground, &values)) {
    NS_ERROR("failed to get foreground pixel");
    return 0;
  }
  nscolor color = nsX11AlphaBlend::PixelToNSColor(values.foreground);

#if DEBUG_SHOW_GLYPH_BOX
  // show X/Y origin
  XDrawLine(dpy, win, DefaultGC(dpy, 0), aX-2, aY, aX+2, aY);
  XDrawLine(dpy, win, DefaultGC(dpy, 0), aX, aY-2, aX, aY+2);
  // show width
  XDrawLine(dpy, win, DefaultGC(dpy, 0), aX-x_origin,  aY-y_origin-2, 
                                         aX+rightBearing, aY-y_origin-2);
#endif

  //
  // Get the background
  //
  XImage *sub_image = nsX11AlphaBlend::GetBackground(dpy, DefaultScreen(dpy),
                                 win, aX-x_origin, aY-y_origin,
                                 image_width, image_height);
  if (sub_image==nsnull) {
#ifdef DEBUG
    int screen = DefaultScreen(dpy);
    // complain if the requested area is not completely off screen
    int win_width = DisplayWidth(dpy, screen);
    int win_height = DisplayHeight(dpy, screen);
    if (((int)(aX-leftBearing+image_width) > 0)  // not hidden to left
        && ((int)(aX-leftBearing) < win_width)   // not hidden to right
        && ((int)(aY-ascent+image_height) > 0)// not hidden to top
        && ((int)(aY-ascent) < win_height))   // not hidden to bottom
    {
      NS_ASSERTION(sub_image, "failed to get the image");
    }
#endif
    return 0;
  }

#if DEBUG_SHOW_GLYPH_BOX
  DEBUG_AADRAWBOX(sub_image,0,0,image_width,image_height,0,0,0,255/4);
  nscolor black NS_RGB(0,255,0);
  blendPixel blendPixelFunc = nsX11AlphaBlend::GetBlendPixel();
  // x origin
  for (x=0; x<(unsigned int)image_height; x++)
    if (x%4==0) (*blendPixelFunc)(sub_image, x_origin, x, black, 255/2);
  // y origin
  for (y=0; y<(unsigned int)image_width; y++)
    if (y%4==0) (*blendPixelFunc)(sub_image, y, ascent-1, black, 255/2);
#endif

  //
  // Get aa glyphs and blend with background
  //
  blendGlyph blendGlyph = nsX11AlphaBlend::GetBlendGlyph();
  PRUint32 i;
  for (i=0; i<aLength; i++) {
    FT_UInt glyph_index;
    FT_Glyph glyph;
    FT_Error error;
    FT_BBox glyph_bbox;
    glyph_index = (*nsFreeTypeFont::nsFT_Get_Char_Index)(face, 
                                                   (FT_ULong)aString[i]);
    if (glyph_index) {
      error = (*nsFreeTypeFont::nsFTC_Image_Cache_Lookup)(
                                   nsFreeType::GetImageCache(),
                                   &mImageDesc, glyph_index, &glyph);
    }
    if ((glyph_index) && (!error)) {
      (*nsFreeTypeFont::nsFT_Glyph_Get_CBox)(glyph, ft_glyph_bbox_pixels, 
                                             &glyph_bbox);
    }
    else {
      // draw an empty box for the missing glyphs
      GetFallbackGlyphMetrics(&glyph_bbox, face);
      int x, y, w = glyph_bbox.xMax, h = glyph_bbox.yMax;
      for (x=1; x<w; x++) {
        XPutPixel(sub_image, x_pos+x, ascent-1,   values.foreground);
        XPutPixel(sub_image, x_pos+x, ascent-h, values.foreground);
      }
      for (y=1; y<h; y++) {
        XPutPixel(sub_image, x_pos+1, ascent-y, values.foreground);
        XPutPixel(sub_image, x_pos+w-1, ascent-y, values.foreground);
        x = (y*(w-2))/h;
        XPutPixel(sub_image, x_pos+x+1, ascent-y,   values.foreground);
      }
      x_pos += w + 1;
      continue;
    }

    FT_BitmapGlyph slot = (FT_BitmapGlyph)glyph;
    nsAntiAliasedGlyph aaglyph(glyph_bbox.xMax-glyph_bbox.xMin, 
                               glyph_bbox.yMax-glyph_bbox.yMin, 0);
    PRUint8 buf[IMAGE_BUFFER_SIZE]; // try to use the stack for data
    if (!aaglyph.WrapFreeType(&glyph_bbox, slot, buf, IMAGE_BUFFER_SIZE)) {
      NS_ERROR("failed to wrap freetype image");
      XDestroyImage(sub_image);
      return 0;
    }

    //
    // blend the aa-glyph onto the background
    //
    NS_ASSERTION(ascent>=glyph_bbox.yMax,"glyph too tall");
    NS_ASSERTION(x_pos>=-aaglyph.GetLBearing(),"glyph extends too far to left");

#if DEBUG_SHOW_GLYPH_BOX
  // draw box around part of glyph that extends to the left 
  // of the main area (negative LBearing)
  if (aaglyph.GetLBearing() < 0) {
    DEBUG_AADRAWBOX(sub_image, x_pos + aaglyph.GetLBearing(), 
                    ascent-glyph_bbox.yMax, 
                    -aaglyph.GetLBearing(), glyph_bbox.yMax, 255,0,0, 255/4);
  }
  // draw box around main glyph area
  DEBUG_AADRAWBOX(sub_image, x_pos, ascent-glyph_bbox.yMax, 
                  aaglyph.GetAdvance(), glyph_bbox.yMax, 0,255,0, 255/4);
  // draw box around part of glyph that extends to the right
  // of the main area (negative LBearing)
  if (aaglyph.GetRBearing() > (int)aaglyph.GetAdvance()) {
    DEBUG_AADRAWBOX(sub_image, x_pos + aaglyph.GetAdvance(), 
                    ascent-glyph_bbox.yMax, 
                    aaglyph.GetRBearing()-aaglyph.GetAdvance(), 
                    glyph_bbox.yMax, 0,0,255, 255/4);
  }
#endif
    (*blendGlyph)(sub_image, &aaglyph, sLinearWeightTable, color,
                  x_pos + aaglyph.GetLBearing(), ascent-glyph_bbox.yMax);

    x_pos += aaglyph.GetAdvance();
  }

  //
  // Send it to the display
  //
  XPutImage(dpy, win, gc, sub_image, 0, 0, aX-x_origin , aY-ascent,
            image_width, image_height);
  XDestroyImage(sub_image);

  return width;
}

///////////////////////////////////////////////////////////////////////
//
// class nsFreeTypeXImage data/functions
//
///////////////////////////////////////////////////////////////////////

nsFreeTypeXImageSBC::nsFreeTypeXImageSBC()
{
  NS_ERROR("should never call nsFreeTypeXImageSBC::nsFreeTypeXImageSBC");
}

nsFreeTypeXImageSBC::nsFreeTypeXImageSBC(nsFreeTypeFace *aFaceID, 
                                         PRUint16 aPixelSize,
                                         const char *aName)
: nsFreeTypeXImage(aFaceID, aPixelSize, aName)
{
}

#ifdef MOZ_MATHML
nsresult
nsFreeTypeXImageSBC::GetBoundingMetrics(const PRUnichar*   aString,
                                        PRUint32           aLength,
                                        nsBoundingMetrics& aBoundingMetrics)
{
  nsresult res;
  char buf[512];
  PRInt32 bufLen = sizeof(buf);
  PRInt32 stringLen = aLength;
  nsFontCatalogEntry* fce = mFaceID->GetFce();
  nsTTFontFamilyEncoderInfo *ffei = nsFT2FontCatalog::GetCustomEncoderInfo(fce);
  NS_ASSERTION(ffei,"failed to find font encoder info");
  if (!ffei)
    return NS_ERROR_FAILURE;
  res = ffei->mEncodingInfo->mConverter->Convert(aString, &stringLen, 
                                                 buf, &bufLen);
  NS_ASSERTION((aLength&&bufLen)||(!aLength&&!bufLen), "converter failed");

  //
  // Widen to 16 bit
  //
  PRUnichar unibuf[512];
  int i;
  for (i=0; i<bufLen; i++) {
    unibuf[i] = (unsigned char)buf[i];
  }

  res = nsFreeTypeXImage::GetBoundingMetrics(unibuf, bufLen, aBoundingMetrics);
  return res;
}
#endif

gint
nsFreeTypeXImageSBC::GetWidth(const PRUnichar* aString, PRUint32 aLength)
{
  nsresult res;
  char buf[512];
  PRInt32 bufLen = sizeof(buf);
  PRInt32 stringLen = aLength;
  nsFontCatalogEntry* fce = mFaceID->GetFce();
  nsTTFontFamilyEncoderInfo *ffei = nsFT2FontCatalog::GetCustomEncoderInfo(fce);
  NS_ASSERTION(ffei,"failed to find font encoder info");
  if (!ffei)
    return NS_ERROR_FAILURE;
  res = ffei->mEncodingInfo->mConverter->Convert(aString, &stringLen, 
                                                 buf, &bufLen);
  NS_ASSERTION((aLength&&bufLen)||(!aLength&&!bufLen), "converter failed");

  //
  // Widen to 16 bit
  //
  PRUnichar unibuf[512];
  int i;
  for (i=0; i<bufLen; i++) {
    unibuf[i] = (unsigned char)buf[i];
  }

  gint width;
  width = nsFreeTypeXImage::GetWidth(unibuf, bufLen);
  return width;
}

gint
nsFreeTypeXImageSBC::DrawString(nsRenderingContextGTK* aContext,
                                nsDrawingSurfaceGTK* aSurface, nscoord aX,
                                nscoord aY, const PRUnichar* aString,
                                PRUint32 aLength)
{
  nsresult res;
  char buf[512];
  PRInt32 bufLen = sizeof(buf);
  PRInt32 stringLen = aLength;
  nsFontCatalogEntry* fce = mFaceID->GetFce();
  nsTTFontFamilyEncoderInfo *ffei = nsFT2FontCatalog::GetCustomEncoderInfo(fce);
  NS_ASSERTION(ffei,"failed to find font encoder info");
  if (!ffei)
    return NS_ERROR_FAILURE;
  res = ffei->mEncodingInfo->mConverter->Convert(aString, &stringLen, 
                                                 buf, &bufLen);
  NS_ASSERTION((aLength&&bufLen)||(!aLength&&!bufLen), "converter failed");

  //
  // Widen to 16 bit
  //
  PRUnichar unibuf[512];
  int i;
  for (i=0; i<bufLen; i++) {
    unibuf[i] = (unsigned char)buf[i];
  }

  gint width;
  width = nsFreeTypeXImage::DrawString(aContext, aSurface, aX, aY, 
                                       unibuf, bufLen);
  return width;
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

  fterror = (*nsFreeTypeFont::nsFT_New_Face)(nsFreeType::GetLibrary(), 
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
      fterror = (*nsFreeTypeFont::nsFT_Set_Charmap)(face, face->charmaps[i]);
      if (fterror) {
        FREETYPE_FONT_PRINTF(("failed to set cmap"));
        (*nsFreeTypeFont::nsFT_Done_Face)(face);
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

void
GetFallbackGlyphMetrics(FT_BBox *aBoundingBox, FT_Face aFace) {
  aBoundingBox->xMin = 0;
  aBoundingBox->yMin = 0;
  aBoundingBox->xMax = MAX(aFace->size->metrics.x_ppem/2 - 1, 0);
  aBoundingBox->yMax = MAX(aFace->size->metrics.y_ppem/2, 1);
}


static void
WeightTableInitCorrection(PRUint8* aTable, PRUint8 aMinValue,
                                double aGain)
{
  // setup the wieghting table
  for (int i=0; i<256; i++) {
    int val = i + (int)rint((double)(i-aMinValue)*aGain);
    val = MAX(0, val);
    val = MIN(val, 255);
    aTable[i] = (PRUint8)val;
  }
}

#endif /* (!defined(MOZ_ENABLE_FREETYPE2)) */
