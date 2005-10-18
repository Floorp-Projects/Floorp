/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Golden Hills Computer Services code.
 *
 * The Initial Developer of the Original Code is
 * Brian Stell <bstell@ix.netcom.com>.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian Stell <bstell@ix.netcom.com>
 *   Jungshik Shin <jshin@i18nl10n.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

//
// This file has routines to build Postscript Type 1 fonts
//
// For general information on Postscript see:
//   Adobe Solutions Network: Technical Notes - Fonts
//   http://partners.adobe.com/asn/developer/technotes/fonts.html
//
// For information on Postscript Type 1 fonts see:
//
//   Adobe Type 1 Font Format
//   http://partners.adobe.com/asn/developer/pdfs/tn/T1_SPEC.PDF
//
// This file uses the FreeType2 FT_Outline_Decompose facility to
// access the outlines which are then converted to Postscript Type 1
//
// For general information on FreeType2 see:
//
//   The FreeType Project
//   http://www.freetype.org/
//
//
// For information on FreeType2's outline processing see:
//   FT_Outline_Decompose
//   http://freetype.sourceforge.net/freetype2/docs/reference/ft2-outline_processing.html#FT_Outline_Decompose
//
//

#include "nsType1.h"
#include "gfx-config.h"
#include <string.h>
#include <unistd.h>

#ifdef MOZ_ENABLE_FREETYPE2
#include "nsIFreeType2.h"
#include "nsServiceManagerUtils.h"
#endif
#include "nsPrintfCString.h"
#include "nsAutoBuffer.h"

#define HEXASCII_LINE_LEN 64

static const PRUint16 kType1EncryptionC1 = 52845;
static const PRUint16 kType1EncryptionC2  = 22719;
static const PRUint16 kType1CharstringEncryptionKey = 4330;
static const PRUint16 kType1EexecEncryptionKey = 55665;

struct FT2PT1_info {
#ifdef MOZ_ENABLE_FREETYPE2
  nsIFreeType2  *ft2;
#endif
  FT_Face        face;
  int            elm_cnt;
  int            len;
  double         cur_x;
  double         cur_y;
  unsigned char *buf;
  int            wmode;
};

// In FT 2.2.0, 'const' was added to the prototypes of some callback functions.
#if (FREETYPE_MAJOR == 2) && (FREETYPE_MINOR >= 2) 
#define nsFT_CONST const
#else 
#define nsFT_CONST
#endif

static int cubicto(nsFT_CONST FT_Vector *aControlPt1,
                   nsFT_CONST FT_Vector *aControlPt2, 
                   nsFT_CONST FT_Vector *aEndPt, void *aClosure);
static int Type1CharStringCommand(unsigned char **aBufPtrPtr, int aCmd);
static int Type1EncodeCharStringInt(unsigned char **aBufPtrPtr, int aValue);

static void encryptAndHexOut(FILE *aFile, PRUint32* aPos, PRUint16* aKey,
                             const char *aBuf, PRInt32 aLen = -1);
static void charStringOut(FILE* aFile, PRUint32* aPos, PRUint16* aKey,
                          const char *aStr, PRUint32 aLen, 
                          PRUnichar aId);
static void flattenName(nsCString& aString);

/* thunk a short name for this function */
static inline int
csc(unsigned char **aBufPtrPtr, int aCmd)
{
  return Type1CharStringCommand(aBufPtrPtr, aCmd);
}

/* thunk a short name for this function */
static inline int
ecsi(unsigned char **aBufPtrPtr, int aValue)
{
  return Type1EncodeCharStringInt(aBufPtrPtr, aValue);
}

static int
Type1CharStringCommand(unsigned char **aBufPtrPtr, int aCmd)
{
  unsigned char *p = *aBufPtrPtr;
  if (p) {
    *p = aCmd;
    *aBufPtrPtr = p + 1;
  }
  return 1;
}

static int
Type1EncodeCharStringInt(unsigned char **aBufPtrPtr, int aValue)
{
  unsigned char *p = *aBufPtrPtr;

  if ((aValue >= -107) && (aValue <= 107)) { 
    if (p) {
      p[0] = aValue + 139;
      *aBufPtrPtr = p + 1;
    }
    return 1;
  }
  else if ((aValue >= 108) && (aValue <= 1131)) {
    if (p) {
      p[0] = ((aValue - 108)>>8) + 247; 
      p[1] = (aValue - 108) & 0xFF;
      *aBufPtrPtr = p + 2;
    }
    return 2;
  }
  else if ((aValue <= -108) && (aValue >= -1131)) {
    if (p) {
      p[0] = ((-aValue - 108)>>8) + 251;
      p[1] = (-aValue - 108) & 0xFF;
      *aBufPtrPtr = p + 2;
    }
    return 2;
  }
  else {
    unsigned int tmp = (unsigned int)aValue;
    if (p) {
      p[0] = 255;
      p[1] = (tmp>>24) & 0xFF;
      p[2] = (tmp>>16) & 0xFF;
      p[3] = (tmp>>8) & 0xFF;
      p[4] = tmp & 0xFF;
      *aBufPtrPtr = p + 5;
    }
    return 5;
  }
}

inline unsigned char
Type1Encrypt(unsigned char aPlain, PRUint16 *aKeyPtr)
{
  unsigned char cipher;
  cipher = (aPlain ^ (*aKeyPtr >> 8));
  *aKeyPtr = (cipher + *aKeyPtr) * kType1EncryptionC1 + kType1EncryptionC2;
  return cipher;
}

static void
Type1EncryptString(unsigned char *aInBuf, unsigned char *aOutBuf, int aLen)
{
  int i;
  PRUint16 key = kType1CharstringEncryptionKey;

  for (i=0; i<aLen; i++)
    aOutBuf[i] = Type1Encrypt(aInBuf[i], &key);
}

static PRBool
sideWidthAndBearing(const FT_Vector *aEndPt, FT2PT1_info *aFti)
{
  int aw = 0;
  int ah = 0;
  FT_UShort upm = aFti->face->units_per_EM;
  FT_GlyphSlot slot;
  FT_Glyph glyph;
  FT_BBox bbox;

  slot = aFti->face->glyph;

#ifdef MOZ_ENABLE_XFT
  FT_Error error = FT_Get_Glyph(slot, &glyph);
  if (error) {
    NS_ERROR("sideWidthAndBearing failed to get glyph");
    return PR_FALSE;
  }
  FT_Glyph_Get_CBox(glyph, ft_glyph_bbox_unscaled, &bbox);
#else
  nsresult rv = aFti->ft2->GetGlyph(slot, &glyph);
  if (NS_FAILED(rv)) {
    NS_ERROR("sideWidthAndBearing failed to get glyph");
    return PR_FALSE;
  }
  aFti->ft2->GlyphGetCBox(glyph, ft_glyph_bbox_unscaled, &bbox);
#endif

  if (aFti->wmode == 0)
    aw = toCS(upm, slot->metrics.horiAdvance);
  else
    aw = -toCS(upm, slot->metrics.vertAdvance);

  if (aEndPt->y == 0) {
    aFti->len += ecsi(&aFti->buf, (int)(aFti->cur_x = toCS(upm, bbox.xMin)));
    aFti->cur_y = 0;
    aFti->len += ecsi(&aFti->buf, aw);
    aFti->len += csc(&aFti->buf, T1_HSBW);
  }
  else {
    aFti->len += ecsi(&aFti->buf, (int)(aFti->cur_x = toCS(upm, bbox.xMin)));
    aFti->len += ecsi(&aFti->buf, (int)(aFti->cur_y = toCS(upm, bbox.yMin)));
    aFti->len += ecsi(&aFti->buf, aw);
    aFti->len += ecsi(&aFti->buf, ah);
    aFti->len += csc(&aFti->buf, T1_ESC_CMD);
    aFti->len += csc(&aFti->buf, T1_ESC_SBW);
  }
  return PR_TRUE;
}

static int
moveto(nsFT_CONST FT_Vector *aEndPt, void *aClosure)
{
  FT2PT1_info *fti = (FT2PT1_info *)aClosure;
  FT_UShort upm = fti->face->units_per_EM;
  PRBool rslt;

  if (fti->elm_cnt == 0) {
    rslt = sideWidthAndBearing(aEndPt, fti);
    if (rslt != PR_TRUE) {
      return 1;
    }
  }
  else {
    fti->len += csc(&fti->buf, T1_CLOSEPATH);
  }

  if (toCS(upm, aEndPt->x) == fti->cur_x) {
    fti->len += ecsi(&fti->buf, toCS(upm, aEndPt->y) - (int)fti->cur_y);
    fti->len += csc(&fti->buf, T1_VMOVETO);
  }
  else if (toCS(upm, aEndPt->y) == fti->cur_y) {
    fti->len += ecsi(&fti->buf, toCS(upm, aEndPt->x) - (int)fti->cur_x);
    fti->len += csc(&fti->buf, T1_HMOVETO);
  }
  else {
    fti->len += ecsi(&fti->buf, toCS(upm, aEndPt->x) - (int)fti->cur_x);
    fti->len += ecsi(&fti->buf, toCS(upm, aEndPt->y) - (int)fti->cur_y);
    fti->len += csc(&fti->buf, T1_RMOVETO);
  }

  fti->cur_x = toCS(upm, aEndPt->x);
  fti->cur_y = toCS(upm, aEndPt->y);
  fti->elm_cnt++;
  return 0;
}

static int
lineto(nsFT_CONST FT_Vector *aEndPt, void *aClosure)
{
  FT2PT1_info *fti = (FT2PT1_info *)aClosure;
  FT_UShort upm = fti->face->units_per_EM;

  if (toCS(upm, aEndPt->x) == fti->cur_x) {
    fti->len += ecsi(&fti->buf, toCS(upm, aEndPt->y) - (int)fti->cur_y);
    fti->len += csc(&fti->buf, T1_VLINETO);
  }
  else if (toCS(upm, aEndPt->y) == fti->cur_y) {
    fti->len += ecsi(&fti->buf, toCS(upm, aEndPt->x) - (int)fti->cur_x);
    fti->len += csc(&fti->buf, T1_HLINETO);
  }
  else {
    fti->len += ecsi(&fti->buf, toCS(upm, aEndPt->x) - (int)fti->cur_x);
    fti->len += ecsi(&fti->buf, toCS(upm, aEndPt->y) - (int)fti->cur_y);
    fti->len += csc(&fti->buf, T1_RLINETO);
  }

  fti->cur_x = toCS(upm, aEndPt->x);
  fti->cur_y = toCS(upm, aEndPt->y);
  fti->elm_cnt++;
  return 0;
}

static int
conicto(nsFT_CONST FT_Vector *aControlPt, nsFT_CONST FT_Vector *aEndPt,
        void *aClosure)
{
  FT2PT1_info *ftinfo = (FT2PT1_info *)aClosure;
  FT_UShort upm = ftinfo->face->units_per_EM;
  double ctl_x, ctl_y;
  double cur_x, cur_y, x3, y3;
  FT_Vector aControlPt1, aControlPt2;
  int rslt;

  cur_x = ftinfo->cur_x;
  cur_y = ftinfo->cur_y;
  ctl_x = toCS(upm, aControlPt->x);
  ctl_y = toCS(upm, aControlPt->y);
  x3 = toCS(upm, aEndPt->x);
  y3 = toCS(upm, aEndPt->y);

  // So we can use the cubic curve code convert quadradic to cubic; 

  // the 1st control point is 2/3 the way from the start to the control point
  aControlPt1.x = fromCS(upm, (cur_x + (2 * (ctl_x - cur_x) + 1)/3));
  aControlPt1.y = fromCS(upm, (cur_y + (2 * (ctl_y - cur_y) + 1)/3));

  // the 2nd control point is 1/3 the way from the control point to the end
  aControlPt2.x = fromCS(upm, (ctl_x + (x3 - ctl_x + 1)/3));
  aControlPt2.y = fromCS(upm, (ctl_y + (y3 - ctl_y + 1)/3));

  // call the cubic code
  rslt = cubicto(&aControlPt1, &aControlPt2, aEndPt, aClosure);
  return rslt;
}

static int
cubicto(nsFT_CONST FT_Vector *aControlPt1, nsFT_CONST FT_Vector *aControlPt2,
        nsFT_CONST FT_Vector *aEndPt, void *aClosure)
{
  FT2PT1_info *ftinfo = (FT2PT1_info *)aClosure;
  FT_UShort upm = ftinfo->face->units_per_EM;
  double cur_x, cur_y, x1, y1, x2, y2, x3, y3;

  cur_x = ftinfo->cur_x;
  cur_y = ftinfo->cur_y;

  x1 = toCS(upm, aControlPt1->x);
  y1 = toCS(upm, aControlPt1->y);

  x2 = toCS(upm, aControlPt2->x);
  y2 = toCS(upm, aControlPt2->y);

  x3 = toCS(upm, aEndPt->x);
  y3 = toCS(upm, aEndPt->y);

  /* horizontal to vertical curve */
  if (((int)(y1) == (int)(cur_y)) && ((int)(x3) == (int)(x2))) {
    ftinfo->len += ecsi(&ftinfo->buf, (int)(x1-cur_x));
    ftinfo->len += ecsi(&ftinfo->buf, (int)(x2-x1));
    ftinfo->len += ecsi(&ftinfo->buf, (int)(y2-y1));
    ftinfo->len += ecsi(&ftinfo->buf, (int)(y3-y2));
    ftinfo->len += csc(&ftinfo->buf, T1_HVCURVETO);
  }
  /* vertical to horizontal curve */
  else if (((int)(x1) == (int)(cur_x)) && ((int)(y3) == (int)(y2))) {
    ftinfo->len += ecsi(&ftinfo->buf, (int)(y1-cur_y));
    ftinfo->len += ecsi(&ftinfo->buf, (int)(x2-x1));
    ftinfo->len += ecsi(&ftinfo->buf, (int)(y2-y1));
    ftinfo->len += ecsi(&ftinfo->buf, (int)(x3-x2));
    ftinfo->len += csc(&ftinfo->buf, T1_VHCURVETO);
  }
  else {
    ftinfo->len += ecsi(&ftinfo->buf,  (int)(x1-cur_x));
    ftinfo->len += ecsi(&ftinfo->buf,  (int)(y1-cur_y));
    ftinfo->len += ecsi(&ftinfo->buf,  (int)(x2-x1));
    ftinfo->len += ecsi(&ftinfo->buf,  (int)(y2-y1));
    ftinfo->len += ecsi(&ftinfo->buf,  (int)(x3-x2));
    ftinfo->len += ecsi(&ftinfo->buf,  (int)(y3-y2));
    ftinfo->len += csc(&ftinfo->buf, T1_RRCURVETO);
  }
  ftinfo->cur_x = x3;
  ftinfo->cur_y = y3;
  ftinfo->elm_cnt++;
  return 0;
}

static FT_Outline_Funcs ft_outline_funcs = {
  moveto,
  lineto,
  conicto,
  cubicto,
  0,
  0
};

FT_Error
#ifdef MOZ_ENABLE_XFT
FT2GlyphToType1CharString(FT_Face aFace, PRUint32 aGlyphID,
                          int aWmode, int aLenIV, unsigned char *aBuf)
#else
FT2GlyphToType1CharString(nsIFreeType2 *aFt2, FT_Face aFace, PRUint32 aGlyphID,
                          int aWmode, int aLenIV, unsigned char *aBuf)
#endif
{
  int j;
  FT_Int32 flags = FT_LOAD_NO_BITMAP | FT_LOAD_NO_SCALE | FT_LOAD_NO_HINTING;
  FT_GlyphSlot slot;
  unsigned char *start = aBuf;
  FT2PT1_info fti;

#ifdef MOZ_ENABLE_XFT
  FT_Error error = FT_Load_Glyph(aFace, aGlyphID, flags);
  if (error) {
    NS_ERROR("failed to load aGlyphID");
    return error;
  }
#else
  nsresult rv = aFt2->LoadGlyph(aFace, aGlyphID, flags);
  if (NS_FAILED(rv)) {
    NS_ERROR("failed to load aGlyphID");
    return 1;
  }
#endif
  slot = aFace->glyph;

  if (slot->format != ft_glyph_format_outline) {
    NS_ERROR("aGlyphID is not an outline glyph");
    return 1;
  }

#ifdef MOZ_ENABLE_FREETYPE2
  fti.ft2     = aFt2;
#endif
  fti.face    = aFace;
  fti.buf     = aBuf;
  fti.elm_cnt = 0;
  fti.len     = 0;
  fti.wmode   = aWmode;

  /* add space for "random" bytes */
  for (j=0; j< aLenIV; j++) {
    fti.len += ecsi(&fti.buf, 0);
  }
#ifdef MOZ_ENABLE_XFT
  if (FT_Outline_Decompose(&slot->outline, &ft_outline_funcs, &fti))  {
    NS_ERROR("error decomposing aGlyphID");
    return 1;
  }
#else
  rv = aFt2->OutlineDecompose(&slot->outline, &ft_outline_funcs, &fti);
  if (NS_FAILED(rv)) {
    NS_ERROR("error decomposing aGlyphID");
    return 1;
  }
#endif

  if (fti.elm_cnt) {
    fti.len += csc(&fti.buf, T1_CLOSEPATH);
    fti.len += csc(&fti.buf, T1_ENDCHAR);
  }
  else {
    FT_Vector end_pt;
    end_pt.x = 0;
    end_pt.y = 1; /* dummy value to cause sbw instead of hsbw */
    PRBool rslt = sideWidthAndBearing(&end_pt, &fti);
    if (rslt != PR_TRUE) {
      return 1;
    }
    fti.len += csc(&fti.buf, T1_ENDCHAR);
  }
  if (fti.buf) {
    Type1EncryptString(start, start, fti.len);
  }

  return fti.len;
}

static PRBool
#ifdef MOZ_ENABLE_XFT
outputType1SubFont(FT_Face aFace,
#else
outputType1SubFont(nsIFreeType2 *aFt2, FT_Face aFace,
#endif
                 const nsAString &aCharIDs, const char *aFontName,
                 int aWmode, int aLenIV, FILE *aFile);

nsresult
FT2ToType1FontName(FT_Face aFace, int aWmode, nsCString& aFontName)
{
  aFontName = aFace->family_name;
  aFontName.AppendLiteral(".");
  aFontName += aFace->style_name;
  aFontName += nsPrintfCString(".%ld.%d", aFace->face_index, aWmode ? 1 : 0);
  flattenName(aFontName);
  return NS_OK;
}

// output a subsetted truetype font converted to multiple type 1 fonts
PRBool
FT2SubsetToType1FontSet(FT_Face aFace, const nsString& aSubset,
                        int aWmode,  FILE *aFile)
{
#ifdef MOZ_ENABLE_FREETYPE2
  nsresult rv; 
  nsCOMPtr<nsIFreeType2> ft2 = do_GetService(NS_FREETYPE2_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    NS_ERROR("Failed to get nsIFreeType2 service");
    return PR_FALSE;
  }
#endif

  nsCAutoString fontNameBase;
  FT2ToType1FontName(aFace, aWmode, fontNameBase);
  PRUint32 i = 0;
  for (; i <= aSubset.Length() / 255 ; i++) {
    nsCAutoString fontName(fontNameBase);
    fontName.AppendLiteral(".Set");
    fontName.AppendInt(i);
#ifdef MOZ_ENABLE_XFT
    outputType1SubFont(aFace,
#else
    outputType1SubFont(ft2, aFace, 
#endif
      Substring(aSubset, i * 255, PR_MIN(255, aSubset.Length() - i * 255)),
      fontName.get(), aWmode, 4, aFile);
  }
  return PR_TRUE;
}

// output a type 1 font (with 255 characters or fewer) 
static PRBool
#ifdef MOZ_ENABLE_XFT
outputType1SubFont(FT_Face aFace,
#else
outputType1SubFont(nsIFreeType2 *aFt2, FT_Face aFace,
#endif
                 const nsAString& aCharIDs, const char *aFontName,
                 int aWmode, int aLenIV, FILE *aFile)
{
  FT_UShort upm = aFace->units_per_EM;

  fprintf(aFile, "%%%%BeginResource: font %s\n"
                 "%%!PS-AdobeFont-1.0-3.0 %s 1.0\n"
                 "%%%%Creator: Mozilla Freetype2 Printing code 2.0\n"
                 "%%%%Title: %s\n"
                 "%%%%Pages: 0\n"
                 "%%%%EndComments\n"
                 "8 dict begin\n", aFontName, aFontName, aFontName);

  fprintf(aFile, "/FontName /%s def\n"
                 "/FontType 1 def\n"
                 "/FontMatrix [ 0.001 0 0 0.001 0 0 ]readonly def\n"
                 "/PaintType 0 def\n", aFontName);

  fprintf(aFile, "/FontBBox [%d %d %d %d]readonly def\n", 
                 toCS(upm, aFace->bbox.xMin),
                 toCS(upm, aFace->bbox.yMin),
                 toCS(upm, aFace->bbox.xMax),
                 toCS(upm, aFace->bbox.yMax));

  nsString charIDstr(aCharIDs);
  PRUint32 len = aCharIDs.Length();
  
  if (len < 10) { 
    // Add a small set of characters to the subset of the user
    // defined font to produce to make sure the font ends up
    // being larger than 2000 bytes, a threshold under which
    // some printers will consider the font invalid.  (bug 253219)
    // XXX : need to check if this is true of type 1 fonts as well.
    // I suspect it's only the case of CID-keyed fonts (type 9) we used to
    // generate. 
    charIDstr.AppendLiteral("1234567890"); 
    len += 10;
  }
  
  const PRUnichar *charIDs = charIDstr.get();

  PRUint32 i;

  // construct an Encoding vector : the 0th element
  // is /.notdef
  fputs("/Encoding [\n/.notdef\n", aFile); 
  for (i = 0; i < len; ++i) {
      fprintf(aFile, "/uni%04X", charIDs[i]); 
      if (i % 8 == 7) fputc('\n', aFile);
  }

  for (i = len; i < 255; ++i) {
      fputs("/.notdef", aFile);
      if (i % 8 == 7) fputc('\n', aFile);
  }
  fputs("] def\n", aFile); 

  fprintf(aFile, "currentdict end\n"
                 "currentfile eexec\n");

  PRUint32 pos = 0;
  PRUint16 key = kType1EexecEncryptionKey;

  // add 'random' bytes (a sequence of zeros would suffice)
  for (i = 0; i < PRUint32(aLenIV); ++i)
    encryptAndHexOut(aFile, &pos, &key, "\0", 1);

  // We don't need any fancy stuff (hint, subroutines, etc) in 
  // 'Private' dictionary. (ref. Adobe Type 1 Spec. Chapters 2 and 5)
  encryptAndHexOut(aFile, &pos, &key,
                   "dup /Private 6 dict dup begin\n"
                   "/RD {string currentfile exch readstring pop} executeonly def\n"
                   "/ND {noaccess def} executeonly def\n"
                   "/NP {noaccess put} executeonly def\n"
                   "/BlueValues [] def\n"
                   "/MinFeature {16 16} def\n"
                   "/password 5839 def\n"); 

  // get the maximum charstring length without actually filling up the buffer
  PRInt32 charStringLen;
  PRInt32 maxCharStringLen =
#ifdef MOZ_ENABLE_XFT
    FT2GlyphToType1CharString(aFace, 0, aWmode, aLenIV, nsnull);
#else
    FT2GlyphToType1CharString(aFt2, aFace, 0, aWmode, aLenIV, nsnull);
#endif

  PRUint32 glyphID;

  for (i = 0; i < len; i++) {
#ifdef MOZ_ENABLE_XFT
    glyphID = FT_Get_Char_Index(aFace, charIDs[i]);
    charStringLen =
      FT2GlyphToType1CharString(aFace, glyphID, aWmode, aLenIV, nsnull);
#else
    aFt2->GetCharIndex(aFace, charIDs[i], &glyphID);
    charStringLen =
      FT2GlyphToType1CharString(aFt2, aFace, glyphID, aWmode, aLenIV, nsnull);
#endif

    if (charStringLen > maxCharStringLen)
      maxCharStringLen = charStringLen;
  }

  nsAutoBuffer<PRUint8, 1024> charString;

  if (!charString.EnsureElemCapacity(maxCharStringLen)) {
    NS_ERROR("Failed to alloc bytes for charstring");
    return PR_FALSE;
  }

  // output CharString 
  encryptAndHexOut(aFile, &pos, &key,
                   nsPrintfCString(60, "2 index /CharStrings %d dict dup begin\n",
                                   len + 1).get()); 

  // output the notdef glyph
#ifdef MOZ_ENABLE_XFT
  charStringLen = FT2GlyphToType1CharString(aFace, 0, aWmode, aLenIV,
                                            charString.get());
#else
  charStringLen = FT2GlyphToType1CharString(aFt2, aFace, 0, aWmode, aLenIV,
                                            charString.get());
#endif

  // enclose charString with  "/.notdef RD .....  ND" 
  charStringOut(aFile, &pos, &key, NS_REINTERPRET_CAST(const char*, charString.get()),
                charStringLen, 0); 


  // output the charstrings for each glyph in this sub font
  for (i = 0; i < len; i++) {
#ifdef MOZ_ENABLE_XFT
    glyphID = FT_Get_Char_Index(aFace, charIDs[i]);
    charStringLen = FT2GlyphToType1CharString(aFace, glyphID, aWmode,
                                              aLenIV, charString.get());
#else
    aFt2->GetCharIndex(aFace, charIDs[i], &glyphID);
    charStringLen = FT2GlyphToType1CharString(aFt2, aFace, glyphID, aWmode,
                                              aLenIV, charString.get());
#endif
    charStringOut(aFile, &pos, &key, NS_REINTERPRET_CAST(const char*, charString.get()),
                  charStringLen, charIDs[i]);
  }

  // wrap up the encrypted part of the font definition
  encryptAndHexOut(aFile, &pos, &key,
                   "end\nend\n"
                   "readonly put\n"
                   "noaccess put\n"
                   "dup /FontName get exch definefont pop\n"
                   "mark currentfile closefile\n");
  if (pos) 
    fputc('\n', aFile);

  // output mandatory 512 0's
  const static char sixtyFourZeros[] =  
      "0000000000000000000000000000000000000000000000000000000000000000\n";
  for (i = 0; i < 8; i++) 
    fprintf(aFile, sixtyFourZeros);

  fprintf(aFile, "cleartomark\n%%%%EndResource\n"); 

  return PR_TRUE;
}
  
/* static */ 
void flattenName(nsCString& aString)
{
  nsCString::iterator start, end;
  aString.BeginWriting(start);
  aString.EndWriting(end);
  while(start != end) {
    if (*start == ' ')
      *start= '_';
    else if (*start == '(')
      *start = '_';
    else if (*start == ')')
      *start = '_';
    ++start;
  }
}

/* static */ 
void encryptAndHexOut(FILE *aFile, PRUint32* aPos, PRUint16* aKey,
                      const char *aBuf, PRInt32 aLen) 
{
  if (aLen == -1) 
      aLen = strlen(aBuf); 

  PRInt32 i;
  for (i = 0; i < aLen; i++) {
    PRUint8 cipher = Type1Encrypt((unsigned char) aBuf[i], aKey);  
    fprintf(aFile, "%02X", cipher); 
    *aPos += 2;
    if (*aPos >= HEXASCII_LINE_LEN) {
      fprintf(aFile, "\n");
      *aPos = 0;
    }
  }
}

/* static */ 
void charStringOut(FILE* aFile,  PRUint32* aPos, PRUint16* aKey, 
                   const char *aStr, PRUint32 aLen, PRUnichar aId)
{
    // use a local buffer instead of nsPrintfCString to avoid alloc.
    char buf[30];
    int oLen;
    if (aId == 0)
      oLen = PR_snprintf(buf, 30, "/.notdef %d RD ", aLen); 
    else 
      oLen = PR_snprintf(buf, 30, "/uni%04X %d RD ", aId, aLen); 

    if (oLen >= 30) {
      NS_WARNING("buffer size exceeded. charstring will be truncated");
      encryptAndHexOut(aFile, aPos, aKey, buf, 30); 
    }
    else 
      encryptAndHexOut(aFile, aPos, aKey, buf); 

    encryptAndHexOut(aFile, aPos, aKey, aStr, aLen);

    encryptAndHexOut(aFile, aPos, aKey, "ND\n");
}
