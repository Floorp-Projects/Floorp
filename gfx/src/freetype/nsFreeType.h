/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ex: set tabstop=8 softtabstop=2 shiftwidth=2 expandtab:
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsFreeType_h__
#define nsFreeType_h__

void     nsFreeTypeFreeGlobals(void);
nsresult nsFreeTypeInitGlobals(void);

#if (!defined(MOZ_ENABLE_FREETYPE2))
// nsFreeType stubs for development systems without a FreeType dev env
class nsFreeTypeFace;
class nsFreeTypeFont : public nsFontGTK {
public:
  static nsFreeTypeFont *NewFont(nsFreeTypeFace *, PRUint16, const char *);
};
#else 

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_CACHE_H
#include FT_CACHE_IMAGE_H
#include FT_TRUETYPE_TABLES_H
#include "nsFT2FontCatalog.h"

typedef struct FT_FaceRec_*  FT_Face;
typedef void (*blitGlyph) (XImage *, FT_BitmapGlyph, nscolor, int, int);
typedef void (*blendPixel)(XImage *, int, int, nscolor, int);

class nsFreeTypeFace;

nsFreeTypeFace * nsFreeTypeGetFaceID(nsFontCatalogEntry *aFce);
void     nsFreeTypeGetFontNames(const char* aPattern, nsFontNodeArray* aNodes);

//
// the FreeType2 function type declarations
//
typedef FT_Error (*FT_Done_Face_t)(FT_Face);
typedef FT_Error (*FT_Done_FreeType_t)(FT_Library);
typedef FT_Error (*FT_Done_Glyph_t)(FT_Glyph);
typedef FT_Error (*FT_Get_Char_Index_t)(FT_Face, FT_ULong);
typedef FT_Error (*FT_Get_Glyph_t)(FT_GlyphSlot, FT_Glyph*);
typedef FT_Error (*FT_Get_Sfnt_Table_t)(FT_Face, FT_Sfnt_Tag);
typedef FT_Error (*FT_Glyph_Get_CBox_t)(FT_Glyph, FT_UInt, FT_BBox*);
typedef FT_Error (*FT_Init_FreeType_t)(FT_Library*);
typedef FT_Error (*FT_Load_Glyph_t)(FT_Face, FT_UInt, FT_Int);
typedef FT_Error (*FT_New_Face_t)(FT_Library, const char*, FT_Long, FT_Face*);
typedef FT_Error (*FT_Set_Charmap_t)(FT_Face face, FT_CharMap  charmap);
typedef FT_Error (*FTC_Image_Cache_Lookup_t)
                      (FTC_Image_Cache, FTC_Image_Desc*, FT_UInt, FT_Glyph*);
typedef FT_Error (*FTC_Manager_Lookup_Size_t)
                      (FTC_Manager, FTC_Font, FT_Face*, FT_Size*);
typedef FT_Error (*FTC_Manager_Done_t)(FTC_Manager);
typedef FT_Error (*FTC_Manager_New_t)(FT_Library, FT_UInt, FT_UInt, FT_ULong,
                       FTC_Face_Requester, FT_Pointer, FTC_Manager*);
typedef FT_Error (*FTC_Image_Cache_New_t)(FTC_Manager, FTC_Image_Cache*);


class nsFreeTypeFont : public nsFontGTK
{
public:

  // run time loaded (function pointers)
  static FT_Done_Face_t            nsFT_Done_Face;
  static FT_Done_FreeType_t        nsFT_Done_FreeType;
  static FT_Done_Glyph_t           nsFT_Done_Glyph;
  static FT_Get_Char_Index_t       nsFT_Get_Char_Index;
  static FT_Get_Glyph_t            nsFT_Get_Glyph;
  static FT_Get_Sfnt_Table_t       nsFT_Get_Sfnt_Table;
  static FT_Glyph_Get_CBox_t       nsFT_Glyph_Get_CBox;
  static FT_Init_FreeType_t        nsFT_Init_FreeType;
  static FT_Load_Glyph_t           nsFT_Load_Glyph;
  static FT_New_Face_t             nsFT_New_Face;
  static FT_Set_Charmap_t          nsFT_Set_Charmap;
  static FTC_Image_Cache_Lookup_t  nsFTC_Image_Cache_Lookup;
  static FTC_Manager_Lookup_Size_t nsFTC_Manager_Lookup_Size;
  static FTC_Manager_Done_t        nsFTC_Manager_Done;
  static FTC_Manager_New_t         nsFTC_Manager_New;
  static FTC_Image_Cache_New_t     nsFTC_Image_Cache_New;

  nsFreeTypeFont();
  nsFreeTypeFont(nsFreeTypeFace *, PRUint16, const char *);
  virtual ~nsFreeTypeFont(void);
  static nsFreeTypeFont *NewFont(nsFreeTypeFace *, PRUint16, const char *);

  void LoadFont(void);

  virtual GdkFont* GetGDKFont(void);
  virtual PRBool   GetGDKFontIs10646(void);
  virtual PRBool   IsFreeTypeFont(void);

  virtual gint GetWidth(const PRUnichar* aString, PRUint32 aLength);
  virtual gint DrawString(nsRenderingContextGTK* aContext,
                          nsDrawingSurfaceGTK* aSurface, nscoord aX,
                          nscoord aY, const PRUnichar* aString,
                          PRUint32 aLength);
#ifdef MOZ_MATHML
  virtual nsresult GetBoundingMetrics(const PRUnichar*   aString,
                                      PRUint32           aLength,
                                      nsBoundingMetrics& aBoundingMetrics);
#endif
  virtual nsresult doGetBoundingMetrics(const PRUnichar*   aString,
                                        PRUint32 aLength,
                                        PRInt32* aLeftBearing,
                                        PRInt32* aRightBearing,
                                        PRInt32* aAscent,
                                        PRInt32* aDescent,
                                        PRInt32* aWidth);

  virtual PRUint32 Convert(const PRUnichar* aSrc, PRUint32 aSrcLen,
                           PRUnichar* aDest, PRUint32 aDestLen);

  FT_Face getFTFace();
  int     ascent();
  int     descent();
  PRBool  getXHeight(unsigned long &val);
  int     max_ascent();
  int     max_descent();
  int     max_width();
  PRBool  superscript_y(long &val);
  PRBool  subscript_y(long &val);
  PRBool  underlinePosition(long &val);
  PRBool  underline_thickness(unsigned long &val);

  FT_Error FaceRequester(FT_Face* aface);
  static void FreeGlobals();

protected:
  XImage *GetXImage(PRUint32 width, PRUint32 height);
  nsFreeTypeFace *mFaceID;
  PRUint16        mPixelSize;
  FTC_Image_Desc  mImageDesc;
};

#endif /* MOZ_ENABLE_FREETYPE2 */

/*
 * Defines for the TrueType codepage bits.
 * Used as a hint for the languages supported in a TrueType font.
 */

/*
 * ulCodePageRange1
 */
#define TT_OS2_CPR1_LATIN1       (0x00000001) /* Latin 1                     */
#define TT_OS2_CPR1_LATIN2       (0x00000002) /* Latin 2: Eastern Europe     */
#define TT_OS2_CPR1_CYRILLIC     (0x00000004) /* Cyrillic                    */
#define TT_OS2_CPR1_GREEK        (0x00000008) /* Greek                       */
#define TT_OS2_CPR1_TURKISH      (0x00000010) /* Turkish                     */
#define TT_OS2_CPR1_HEBREW       (0x00000020) /* Hebrew                      */
#define TT_OS2_CPR1_ARABIC       (0x00000040) /* Arabic                      */
#define TT_OS2_CPR1_BALTIC       (0x00000080) /* Windows Baltic              */
#define TT_OS2_CPR1_VIETNAMESE   (0x00000100) /* Vietnamese                  */
                                 /* 9-15     Reserved for Alternate ANSI     */
#define TT_OS2_CPR1_THAI         (0x00010000) /* Thai                        */
#define TT_OS2_CPR1_JAPANESE     (0x00020000) /* JIS/Japan                   */
#define TT_OS2_CPR1_CHINESE_SIMP (0x00040000) /* Chinese: Simplified         */
#define TT_OS2_CPR1_KO_WANSUNG   (0x00080000) /* Korean Wansung              */
#define TT_OS2_CPR1_CHINESE_TRAD (0x00100000) /* Chinese: Traditional        */
#define TT_OS2_CPR1_KO_JOHAB     (0x00200000) /* Korean Johab                */
                                 /* 22-28    Reserved for Alternate ANSI&OEM */
#define TT_OS2_CPR1_MAC_ROMAN    (0x20000000) /* Mac (US Roman)              */
#define TT_OS2_CPR1_OEM          (0x40000000) /* OEM Character Set           */
#define TT_OS2_CPR1_SYMBOL       (0x80000000) /* Symbol Character Set        */

/*
 * ulCodePageRange2
 */                              /* 32-47    Reserved for OEM                */
#define TT_OS2_CPR2_GREEK        (0x00010000) /* IBM Greek                   */
#define TT_OS2_CPR2_RUSSIAN      (0x00020000) /* MS-DOS Russian              */
#define TT_OS2_CPR2_NORDIC       (0x00040000) /* MS-DOS Nordic               */
#define TT_OS2_CPR2_ARABIC       (0x00080000) /* Arabic                      */
#define TT_OS2_CPR2_CA_FRENCH    (0x00100000) /* MS-DOS Canadian French      */
#define TT_OS2_CPR2_HEBREW       (0x00200000) /* Hebrew                      */
#define TT_OS2_CPR2_ICELANDIC    (0x00400000) /* MS-DOS Icelandic            */
#define TT_OS2_CPR2_PORTUGESE    (0x00800000) /* MS-DOS Portuguese           */
#define TT_OS2_CPR2_TURKISH      (0x01000000) /* IBM Turkish                 */
#define TT_OS2_CPR2_CYRILLIC     (0x02000000)/*IBM Cyrillic; primarily Russian*/
#define TT_OS2_CPR2_LATIN2       (0x04000000) /* Latin 2                     */
#define TT_OS2_CPR2_BALTIC       (0x08000000) /* MS-DOS Baltic               */
#define TT_OS2_CPR2_GREEK_437G   (0x10000000) /* Greek; former 437 G         */
#define TT_OS2_CPR2_ARABIC_708   (0x20000000) /* Arabic; ASMO 708            */
#define TT_OS2_CPR2_WE_LATIN1    (0x40000000) /* WE/Latin 1                  */
#define TT_OS2_CPR2_US           (0x80000000) /* US                          */

#endif
