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
// For genernal information on Postscript see:
//   Adobe Solutions Network: Technical Notes - Fonts
//   http://partners.adobe.com/asn/developer/technotes/fonts.html
//
// For information on Postscript Type 1 fonts see:
//
//   Adobe Type 1 Font Format
//   http://partners.adobe.com/asn/developer/pdfs/tn/T1_SPEC.PDF
//

#ifndef TYPE1_H
#define TYPE1_H

#include <stdio.h>
#include "nspr.h"
#ifdef MOZ_ENABLE_XFT
#include "nsISupports.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_OUTLINE_H
#else
#include "nsIFreeType2.h"
#endif

/* to/from Character Space */
inline int
toCS(double upm, double x)
{
  return (int)((x*1000.0)/upm);
}

inline int
fromCS(double upm, double x)
{
  return (int) (x*upm/1000.0);
}

/* the 1 byte Postscript Type 1 commands */
#define T1_HSTEM      1  /* 0x01 */
#define T1_VSTEM      3  /* 0x03 */
#define T1_VMOVETO    4  /* 0x04 */
#define T1_RLINETO    5  /* 0x05 */
#define T1_HLINETO    6  /* 0x06 */
#define T1_VLINETO    7  /* 0x07 */
#define T1_RRCURVETO  8  /* 0x08 */
#define T1_CLOSEPATH  9  /* 0x09 */
#define T1_CALLSUBR  10  /* 0x0a */
#define T1_RETURN    11  /* 0x0b */
#define T1_ESC_CMD   12  /* 0x0c */
#define T1_HSBW      13  /* 0x0d */
#define T1_ENDCHAR   14  /* 0x0e */
#define T1_RMOVETO   21  /* 0x15 */
#define T1_HMOVETO   22  /* 0x16 */
#define T1_VHCURVETO 30  /* 0x1e */
#define T1_HVCURVETO 31  /* 0x1f */

/* the 2 byte (escaped) Postscript Type 1 commands */
#define T1_ESC_SBW    7  /* 0x07 */

#define TYPE1_ENCRYPTION_KEY 4330
#define TYPE1_ENCRYPTION_C1  52845
#define TYPE1_ENCRYPTION_C2  22719

#ifdef MOZ_ENABLE_XFT  
FT_Error FT2GlyphToType1CharString(FT_Face aFace,
#else /* MOZ_ENABLE_FREETYPE2 */
FT_Error FT2GlyphToType1CharString(nsIFreeType2 *aFt2, FT_Face aFace,
#endif
                                   PRUint32 aGlyphID, int aWmode, int aLenIV,
                                   unsigned char *aBuf);

#endif /* TYPE1_H */
