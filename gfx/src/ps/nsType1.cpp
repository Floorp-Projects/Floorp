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
 *   Brian Stell <bstell@ix.netcom.com>.
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

static const PRUint16 type1_encryption_c1 = TYPE1_ENCRYPTION_C1;
static const PRUint16 type1_encryption_c2 = TYPE1_ENCRYPTION_C2;

typedef struct {
#ifndef MOZ_ENABLE_XFT
  nsIFreeType2  *ft2;
#endif
  FT_Face        face;
  int            elm_cnt;
  int            len;
  double         cur_x;
  double         cur_y;
  unsigned char *buf;
  int            wmode;
} FT2PT1_info;

static int cubicto(FT_Vector *aControlPt1, FT_Vector *aControlPt2, 
                   FT_Vector *aEndPt, void *aClosure);
static int Type1CharStringCommand(unsigned char **aBufPtrPtr, int aCmd);
static int Type1EncodeCharStringInt(unsigned char **aBufPtrPtr, int aValue);

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
  *aKeyPtr = (cipher + *aKeyPtr) * type1_encryption_c1 + type1_encryption_c2;
  return cipher;
}

static void
Type1EncryptString(unsigned char *aInBuf, unsigned char *aOutBuf, int aLen)
{
  int i;
  PRUint16 key = TYPE1_ENCRYPTION_KEY;

  for (i=0; i<aLen; i++)
    aOutBuf[i] = Type1Encrypt(aInBuf[i], &key);
}

static PRBool
sideWidthAndBearing(FT_Vector *aEndPt, FT2PT1_info *aFti)
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
moveto(FT_Vector *aEndPt, void *aClosure)
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
lineto(FT_Vector *aEndPt, void *aClosure)
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
conicto(FT_Vector *aControlPt, FT_Vector *aEndPt, void *aClosure)
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
cubicto(FT_Vector *aControlPt1, FT_Vector *aControlPt2, FT_Vector *aEndPt,
        void *aClosure)
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

#ifndef MOZ_ENABLE_XFT
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
