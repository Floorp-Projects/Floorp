/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * Pango
 * thai-x.c: 
 *
 * The contents of this file are subject to the Mozilla Public	
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Pango Library (www.pango.org) 
 * 
 * The Initial Developer of the Original Code is Red Hat Software
 * Portions created by Red Hat are Copyright (C) 1999 Red Hat Software.
 * 
 * Contributor(s): 
 * Author: Owen Taylor <otaylor@redhat.com>
 *
 * Software and Language Engineering Laboratory, NECTEC
 * Author: Theppitak Karoonboonyanan <thep@links.nectec.or.th>
 *
 * Copyright (c) 1996-2000 by Sun Microsystems, Inc.
 * Author: Chookij Vanatham <Chookij.Vanatham@Eng.Sun.COM>
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Lessor General Public License Version 2 (the 
 * "LGPL"), in which case the provisions of the LGPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the LGPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the LGPL. If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * LGPL.
*/

#include <glib.h>
#include <string.h>

#include "pango-engine.h"
#include "pango-coverage.h"

#define G_N_ELEMENTS(arr)     (sizeof (arr) / sizeof ((arr)[0]))

#define ucs2tis(wc)	(unsigned int)((unsigned int)(wc) - 0x0E00 + 0xA0)
#define tis2uni(c)	((gunichar2)(c) - 0xA0 + 0x0E00)

#define MAX_CLUSTER_CHRS 256
#define MAX_GLYPHS       256
#define GLYPH_COMBINING  256

/* Define TACTIS character classes */
#define CTRL		0
#define NON			1
#define CONS		2
#define LV			3
#define FV1			4
#define FV2			5
#define FV3			6
#define BV1			7
#define BV2			8
#define BD			9
#define TONE		10
#define AD1			11
#define AD2			12
#define AD3			13
#define AV1			14
#define AV2			15
#define AV3			16

#define _ND			0
#define _NC			1
#define _UC			(1<<1)
#define _BC			(1<<2)
#define _SC			(1<<3)
#define _AV			(1<<4)
#define _BV			(1<<5)
#define _TN			(1<<6)
#define _AD			(1<<7)
#define _BD			(1<<8)
#define _AM			(1<<9)

#define NoTailCons	 _NC
#define UpTailCons	 _UC
#define BotTailCons	 _BC
#define SpltTailCons _SC
#define Cons			   (NoTailCons|UpTailCons|BotTailCons|SpltTailCons)
#define AboveVowel	 _AV
#define BelowVowel	 _BV
#define Tone			   _TN
#define AboveDiac	   _AD
#define BelowDiac	   _BD
#define SaraAm		   _AM

#define char_class(wc)		     TAC_char_class[(unsigned int)(wc)]
#define is_char_type(wc, mask) (char_type_table[ucs2tis((wc))] & (mask))

#define SCRIPT_ENGINE_NAME "ThaiScriptEngineX"
#define PANGO_RENDER_TYPE_X "PangoliteRenderX"

typedef guint16 PangoliteXSubfont;
#define PANGO_MOZ_MAKE_GLYPH(index) ((guint32)0 | (index))

/* We handle the range U+0e01 to U+0e5b exactly
 */
static PangoliteEngineRange thai_ranges[] = {
  { 0x0e01, 0x0e5b, "*" },  /* Thai */
};

static PangoliteEngineInfo script_engines[] = {
  {
    SCRIPT_ENGINE_NAME,
    PANGO_ENGINE_TYPE_SHAPE,
    PANGO_RENDER_TYPE_X,
    thai_ranges, 
    G_N_ELEMENTS(thai_ranges)
  }
};

/*
 * X window system script engine portion
 */

typedef struct _ThaiFontInfo ThaiFontInfo;

/* The type of encoding that we will use
 */
typedef enum {
  THAI_FONT_NONE,
  THAI_FONT_XTIS,
  THAI_FONT_TIS,
  THAI_FONT_TIS_MAC,
  THAI_FONT_TIS_WIN,
  THAI_FONT_ISO10646
} ThaiFontType;

struct _ThaiFontInfo
{
  ThaiFontType  type;
  PangoliteXSubfont subfont;
};

/* All combining marks for Thai fall in the range U+0E30-U+0E50,
 * so we confine our data tables to that range, and use
 * default values for characters outside those ranges.
 */

/* Map from code point to group used for rendering with XTIS fonts
 * (0 == base character)
 */
static const char groups[32] = {
  0, 1, 0, 0, 1, 1, 1, 1,
  1, 1, 1, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 2,
  2, 2, 2, 2, 2, 2, 1, 0
};

/* Map from code point to index within group 1
 * (0 == no combining mark from group 1)
 */   
static const char group1_map[32] = {
  0, 1, 0, 0, 2, 3, 4, 5,
  6, 7, 8, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0
};

/* Map from code point to index within group 2
 * (0 == no combining mark from group 2)
 */   
static const char group2_map[32] = {
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 1,
  2, 3, 4, 5, 6, 7, 1, 0
};

static const gint char_type_table[256] = {
  /*       0,   1,   2,   3,   4,   5,   6,   7,
           8,   9,   A,   B,   C,   D,   E,   F  */

  /*00*/ _ND, _ND, _ND, _ND, _ND, _ND, _ND, _ND,
         _ND, _ND, _ND, _ND, _ND, _ND, _ND, _ND,
  /*10*/ _ND, _ND, _ND, _ND, _ND, _ND, _ND, _ND,
         _ND, _ND, _ND, _ND, _ND, _ND, _ND, _ND,
  /*20*/ _ND, _ND, _ND, _ND, _ND, _ND, _ND, _ND,
         _ND, _ND, _ND, _ND, _ND, _ND, _ND, _ND,
  /*30*/ _ND, _ND, _ND, _ND, _ND, _ND, _ND, _ND,
         _ND, _ND, _ND, _ND, _ND, _ND, _ND, _ND,
  /*40*/ _ND, _ND, _ND, _ND, _ND, _ND, _ND, _ND,
         _ND, _ND, _ND, _ND, _ND, _ND, _ND, _ND,
  /*50*/ _ND, _ND, _ND, _ND, _ND, _ND, _ND, _ND,
         _ND, _ND, _ND, _ND, _ND, _ND, _ND, _ND,
  /*60*/ _ND, _ND, _ND, _ND, _ND, _ND, _ND, _ND,
         _ND, _ND, _ND, _ND, _ND, _ND, _ND, _ND,
  /*70*/ _ND, _ND, _ND, _ND, _ND, _ND, _ND, _ND,
         _ND, _ND, _ND, _ND, _ND, _ND, _ND, _ND,
  /*80*/ _ND, _ND, _ND, _ND, _ND, _ND, _ND, _ND,
         _ND, _ND, _ND, _ND, _ND, _ND, _ND, _ND,
  /*90*/ _ND, _ND, _ND, _ND, _ND, _ND, _ND, _ND,
         _ND, _ND, _ND, _ND, _ND, _ND, _ND, _ND,
		
  /*A0*/ _ND, _NC, _NC, _NC, _NC, _NC, _NC, _NC,
         _NC, _NC, _NC, _NC, _NC, _SC, _BC, _BC,
  /*B0*/ _SC, _NC, _NC, _NC, _NC, _NC, _NC, _NC,
         _NC, _NC, _NC, _UC, _NC, _UC, _NC, _UC,
  /*C0*/ _NC, _NC, _NC, _NC, _ND, _NC, _ND, _NC,
         _NC, _NC, _NC, _NC, _UC, _NC, _NC, _ND,
  /*D0*/ _ND, _AV, _ND, _AM, _AV, _AV, _AV, _AV,
         _BV, _BV, _BD, _ND, _ND, _ND, _ND, _ND,
  /*E0*/ _ND, _ND, _ND, _ND, _ND, _ND, _ND, _AD,
         _TN, _TN, _TN, _TN, _AD, _AD, _AD, _ND,
  /*F0*/ _ND, _ND, _ND, _ND, _ND, _ND, _ND, _ND,
         _ND, _ND, _ND, _ND, _ND, _ND, _ND, _ND,
};

static const gint TAC_char_class[256] = {
  /*	   0,   1,   2,   3,   4,   5,   6,   7,
           8,   9,   A,   B,   C,   D,   E,   F  */

  /*00*/ CTRL,CTRL,CTRL,CTRL,CTRL,CTRL,CTRL,CTRL,
         CTRL,CTRL,CTRL,CTRL,CTRL,CTRL,CTRL,CTRL,
  /*10*/ CTRL,CTRL,CTRL,CTRL,CTRL,CTRL,CTRL,CTRL,
         CTRL,CTRL,CTRL,CTRL,CTRL,CTRL,CTRL,CTRL,
  /*20*/  NON, NON, NON, NON, NON, NON, NON, NON,
          NON, NON, NON, NON, NON, NON, NON, NON,
  /*30*/  NON, NON, NON, NON, NON, NON, NON, NON,
          NON, NON, NON, NON, NON, NON, NON, NON,
  /*40*/  NON, NON, NON, NON, NON, NON, NON, NON,
          NON, NON, NON, NON, NON, NON, NON, NON,
  /*50*/  NON, NON, NON, NON, NON, NON, NON, NON,
          NON, NON, NON, NON, NON, NON, NON, NON,
  /*60*/  NON, NON, NON, NON, NON, NON, NON, NON,
          NON, NON, NON, NON, NON, NON, NON, NON,
  /*70*/  NON, NON, NON, NON, NON, NON, NON, NON,
          NON, NON, NON, NON, NON, NON, NON,CTRL,
  /*80*/ CTRL,CTRL,CTRL,CTRL,CTRL,CTRL,CTRL,CTRL,
         CTRL,CTRL,CTRL,CTRL,CTRL,CTRL,CTRL,CTRL,
  /*90*/ CTRL,CTRL,CTRL,CTRL,CTRL,CTRL,CTRL,CTRL,
         CTRL,CTRL,CTRL,CTRL,CTRL,CTRL,CTRL,CTRL,
  /*A0*/  NON,CONS,CONS,CONS,CONS,CONS,CONS,CONS,
         CONS,CONS,CONS,CONS,CONS,CONS,CONS,CONS,
  /*B0*/ CONS,CONS,CONS,CONS,CONS,CONS,CONS,CONS,
         CONS,CONS,CONS,CONS,CONS,CONS,CONS,CONS,
  /*C0*/ CONS,CONS,CONS,CONS, FV3,CONS, FV3,CONS,
         CONS,CONS,CONS,CONS,CONS,CONS,CONS, NON,
  /*D0*/  FV1, AV2, FV1, FV1, AV1, AV3, AV2, AV3,
          BV1, BV2,  BD, NON, NON, NON, NON, NON,
  /*E0*/   LV,  LV,  LV,  LV,  LV, FV2, NON, AD2,
         TONE,TONE,TONE,TONE, AD1, AD1, AD3, NON,
  /*F0*/  NON, NON, NON, NON, NON, NON, NON, NON,
          NON, NON, NON, NON, NON, NON, NON,CTRL,
};

static const gchar TAC_compose_and_input_check_type_table[17][17] = {
  /* Cn */ /* 0,   1,   2,   3,   4,   5,   6,   7,
     8,   9,   A,   B,   C,   D,   E,   F       */
  /* Cn-1 00 */	{ 'X', 'A', 'A', 'A', 'A', 'A', 'A', 'R',
                  'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R' },
  /* 10 */      { 'X', 'A', 'A', 'A', 'S', 'S', 'A', 'R',
                  'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R' },
  /* 20 */      { 'X', 'A', 'A', 'A', 'A', 'S', 'A', 'C',
                  'C', 'C', 'C', 'C', 'C', 'C', 'C', 'C', 'C' },
  /* 30 */      {'X', 'S', 'A', 'S', 'S', 'S', 'S', 'R',
                 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R' },
  /* 40 */      { 'X', 'S', 'A', 'A', 'S', 'S', 'A', 'R',
                  'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R' },
  /* 50 */      { 'X', 'A', 'A', 'A', 'A', 'S', 'A', 'R',
                  'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R' },
  /* 60 */      { 'X', 'A', 'A', 'A', 'S', 'A', 'S', 'R',
                  'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R' },
  /* 70 */      { 'X', 'A', 'A', 'A', 'S', 'S', 'A', 'R',
                  'R', 'R', 'C', 'C', 'R', 'R', 'R', 'R', 'R' },
  /* 80 */      { 'X', 'A', 'A', 'A', 'S', 'S', 'A', 'R',
                  'R', 'R', 'C', 'R', 'R', 'R', 'R', 'R', 'R' },
  /* 90 */      { 'X', 'A', 'A', 'A', 'S', 'S', 'A', 'R',
                  'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R' },
  /* A0 */      { 'X', 'A', 'A', 'A', 'A', 'A', 'A', 'R',
                  'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R' },
  /* B0 */      { 'X', 'A', 'A', 'A', 'S', 'S', 'A', 'R',
                  'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R' },
  /* C0 */      { 'X', 'A', 'A', 'A', 'S', 'S', 'A', 'R',
                  'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R' },
  /* D0 */      { 'X', 'A', 'A', 'A', 'S', 'S', 'A', 'R',
                  'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R' },
  /* E0 */      { 'X', 'A', 'A', 'A', 'S', 'S', 'A', 'R',
                  'R', 'R', 'C', 'C', 'R', 'R', 'R', 'R', 'R' },
  /* F0 */      { 'X', 'A', 'A', 'A', 'S', 'S', 'A', 'R',
                  'R', 'R', 'C', 'R', 'R', 'R', 'R', 'R', 'R' },
  /*    */      { 'X', 'A', 'A', 'A', 'S', 'S', 'A', 'R',
                  'R', 'R', 'C', 'R', 'C', 'R', 'R', 'R', 'R' }
};

typedef struct {
  gint ShiftDown_TONE_AD[8];
  gint ShiftDownLeft_TONE_AD[8];
  gint ShiftLeft_TONE_AD[8];
  gint ShiftLeft_AV[7];
  gint ShiftDown_BV_BD[3];
  gint TailCutCons[4];
} ThaiShapeTable;

#define shiftdown_tone_ad(c,tbl)     ((tbl)->ShiftDown_TONE_AD[(c)-0xE7])
#define shiftdownleft_tone_ad(c,tbl) ((tbl)->ShiftDownLeft_TONE_AD[(c)-0xE7])
#define shiftleft_tone_ad(c,tbl)     ((tbl)->ShiftLeft_TONE_AD[(c)-0xE7])
#define shiftleft_av(c,tbl)          ((tbl)->ShiftLeft_AV[(c)-0xD1])
#define shiftdown_bv_bd(c,tbl)       ((tbl)->ShiftDown_BV_BD[(c)-0xD8])
#define tailcutcons(c,tbl)           ((tbl)->TailCutCons[(c)-0xAD])

/* Macintosh
 */
static const ThaiShapeTable Mac_shape_table = {
  { 0xE7, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0xED, 0xEE },
  { 0xE7, 0x83, 0x84, 0x85, 0x86, 0x87, 0x8F, 0xEE },
  { 0x93, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x8F, 0xEE },
  { 0x92, 0x00, 0x00, 0x94, 0x95, 0x96, 0x97 },
  { 0xD8, 0xD9, 0xDA },
  { 0xAD, 0x00, 0x00, 0xB0 }
};

/* Microsoft Window
 */
static const ThaiShapeTable Win_shape_table = {
    { 0xE7, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F, 0xED, 0xEE },
    { 0xE7, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x99, 0xEE },
    { 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F, 0x99, 0xEE },
    { 0x98, 0x00, 0x00, 0x81, 0x82, 0x83, 0x84 },
    { 0xFC, 0xFD, 0xFE },
    { 0x90, 0x00, 0x00, 0x80 }
};

/* No adjusted vowel/tonemark glyphs (tis620-0)
  */
static const ThaiShapeTable tis620_0_shape_table = {
  { 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE },
  { 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE },
  { 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE },
  { 0xD1, 0x00, 0x00, 0xD4, 0xD5, 0xD6, 0xD7 },
  { 0xD8, 0xD9, 0xDA },
  { 0xAD, 0x00, 0x00, 0xB0 }
};

/* Returns a structure with information we will use to rendering given the
 * #PangoliteFont. This is computed once per font and cached for later retrieval.
 */
static ThaiFontInfo *
get_font_info (const char *fontCharset)
{
  static const char *charsets[] = {
    "tis620-2",
    "tis620-1",
    "tis620-0",
    "xtis620.2529-1",
    "xtis-0",
    "tis620.2533-1",
    "tis620.2529-1",
    "iso8859-11",
    "iso10646-1",
  };

  static const int charset_types[] = {
    THAI_FONT_TIS_WIN,
    THAI_FONT_TIS_MAC,
    THAI_FONT_TIS,
    THAI_FONT_XTIS,
    THAI_FONT_XTIS,
    THAI_FONT_TIS,
    THAI_FONT_TIS,
    THAI_FONT_TIS,
    THAI_FONT_ISO10646
  };
  
  ThaiFontInfo *font_info = g_new(ThaiFontInfo, 1);
  guint        i;

  font_info->type = THAI_FONT_NONE;
  for (i = 0; i < G_N_ELEMENTS(charsets); i++) {
    if (strcmp(fontCharset, charsets[i]) == 0) {	  
      font_info->type = (ThaiFontType)charset_types[i];
      font_info->subfont = (PangoliteXSubfont)i;
      break;
    }
  }
  return font_info;
}

static void
add_glyph(ThaiFontInfo         *font_info,
          PangoliteGlyphString *glyphs,
          gint                 cluster_start,
          PangoliteGlyph       glyph,
          gint                 combining)
{
  gint           index = glyphs->num_glyphs;

  if ((cluster_start == 0) && (index != 0))
     cluster_start++;

  pangolite_glyph_string_set_size(glyphs, index + 1);  
  glyphs->glyphs[index].glyph = glyph;
  glyphs->glyphs[index].is_cluster_start = combining;
  glyphs->log_clusters[index] = cluster_start;
}

static gint
get_adjusted_glyphs_list(ThaiFontInfo *font_info,
                         gunichar2     *cluster,
                         gint         num_chrs,
                         PangoliteGlyph   *glyph_lists,
                         const ThaiShapeTable *shaping_table)
{
  switch (num_chrs) {
  case 1:
    if (is_char_type (cluster[0], BelowVowel|BelowDiac|AboveVowel|AboveDiac|Tone)) {
	    if (font_info->type == THAI_FONT_TIS)
	      glyph_lists[0] = PANGO_MOZ_MAKE_GLYPH (0x20);
	    else
	      glyph_lists[0] = PANGO_MOZ_MAKE_GLYPH (0x7F);
      glyph_lists[1] = PANGO_MOZ_MAKE_GLYPH (ucs2tis (cluster[0]));
	    return 2;
    }
    else {
      glyph_lists[0] = PANGO_MOZ_MAKE_GLYPH (ucs2tis (cluster[0]));
      return 1;
    }
    break;
    
  case 2:
    if (is_char_type (cluster[0], NoTailCons|BotTailCons|SpltTailCons) &&
        is_char_type (cluster[1], SaraAm)) {
      glyph_lists[0] = PANGO_MOZ_MAKE_GLYPH (ucs2tis (cluster[0]));
      glyph_lists[1] = PANGO_MOZ_MAKE_GLYPH (0xED);
      glyph_lists[2] = PANGO_MOZ_MAKE_GLYPH (0xD2);
      return 3;
    }
    else if (is_char_type (cluster[0], UpTailCons) &&
          	 is_char_type (cluster[1], SaraAm)) {
      glyph_lists[0] = PANGO_MOZ_MAKE_GLYPH (ucs2tis (cluster[0]));
      glyph_lists[1] = PANGO_MOZ_MAKE_GLYPH (shiftleft_tone_ad (0xED, shaping_table));
      glyph_lists[2] = PANGO_MOZ_MAKE_GLYPH (0xD2);
      return 3;
    }
    else if (is_char_type (cluster[0], NoTailCons|BotTailCons|SpltTailCons) &&
          	 is_char_type (cluster[1], AboveVowel)) {
      glyph_lists[0] = PANGO_MOZ_MAKE_GLYPH (ucs2tis (cluster[0]));
      glyph_lists[1] = PANGO_MOZ_MAKE_GLYPH (ucs2tis (cluster[1]));
      return 2;
    }
    else if (is_char_type (cluster[0], NoTailCons|BotTailCons|SpltTailCons) &&
          	 is_char_type (cluster[1], AboveDiac|Tone)) {
      glyph_lists[0] = PANGO_MOZ_MAKE_GLYPH (ucs2tis (cluster[0]));
      glyph_lists[1] = PANGO_MOZ_MAKE_GLYPH (shiftdown_tone_ad (ucs2tis (cluster[1]), shaping_table));
      return 2;
    }
    else if (is_char_type (cluster[0], UpTailCons) &&
          	 is_char_type (cluster[1], AboveVowel)) {
      glyph_lists[0] = PANGO_MOZ_MAKE_GLYPH (ucs2tis (cluster[0]));
      glyph_lists[1] = PANGO_MOZ_MAKE_GLYPH (shiftleft_av (ucs2tis (cluster[1]), shaping_table));
      return 2;
    }
    else if (is_char_type (cluster[0], UpTailCons) &&
          	 is_char_type (cluster[1], AboveDiac|Tone)) {
      glyph_lists[0] = PANGO_MOZ_MAKE_GLYPH (ucs2tis (cluster[0]));
      glyph_lists[1] = PANGO_MOZ_MAKE_GLYPH (shiftdownleft_tone_ad (ucs2tis (cluster[1]), shaping_table));
      return 2;
    }
    else if (is_char_type (cluster[0], NoTailCons|UpTailCons) &&
          	 is_char_type (cluster[1], BelowVowel|BelowDiac)) {
      glyph_lists[0] = PANGO_MOZ_MAKE_GLYPH (ucs2tis (cluster[0]));
      glyph_lists[1] = PANGO_MOZ_MAKE_GLYPH (ucs2tis (cluster[1]));
      return 2;
    }
    else if (is_char_type (cluster[0], BotTailCons) &&
             is_char_type (cluster[1], BelowVowel|BelowDiac)) {
      glyph_lists[0] = PANGO_MOZ_MAKE_GLYPH (ucs2tis (cluster[0]));
      glyph_lists[1] = PANGO_MOZ_MAKE_GLYPH (shiftdown_bv_bd (ucs2tis (cluster[1]), shaping_table));
      return 2;
    }
    else if (is_char_type (cluster[0], SpltTailCons) &&
          	 is_char_type (cluster[1], BelowVowel|BelowDiac)) {
        glyph_lists[0] = PANGO_MOZ_MAKE_GLYPH (tailcutcons (ucs2tis (cluster[0]), shaping_table));
        glyph_lists[1] = PANGO_MOZ_MAKE_GLYPH (ucs2tis (cluster[1]));
        return 2;
      }
    else {
      if (font_info->type == THAI_FONT_TIS)
        glyph_lists[0] = PANGO_MOZ_MAKE_GLYPH (0x20);
      else
        glyph_lists[0] = PANGO_MOZ_MAKE_GLYPH (0x7F);
      glyph_lists[1] =
        PANGO_MOZ_MAKE_GLYPH (ucs2tis (cluster[0]));
      glyph_lists[2] =
        PANGO_MOZ_MAKE_GLYPH (ucs2tis (cluster[1]));
      return 3;
    }
    break;
    
  case 3:
    if (is_char_type (cluster[0], NoTailCons|BotTailCons|SpltTailCons) &&
        is_char_type (cluster[1], Tone) &&
        is_char_type (cluster[2], SaraAm)) {
      glyph_lists[0] = PANGO_MOZ_MAKE_GLYPH (ucs2tis (cluster[0]));
      glyph_lists[1] = PANGO_MOZ_MAKE_GLYPH (0xED);
      glyph_lists[2] = PANGO_MOZ_MAKE_GLYPH (ucs2tis (cluster[1]));
      glyph_lists[3] = PANGO_MOZ_MAKE_GLYPH (0xD2);
      return 4;
    }
    else if (is_char_type (cluster[0], UpTailCons) &&
             is_char_type (cluster[1], Tone) &&
             is_char_type (cluster[2], SaraAm)) {
      glyph_lists[0] = PANGO_MOZ_MAKE_GLYPH (ucs2tis (cluster[0]));
      glyph_lists[1] = PANGO_MOZ_MAKE_GLYPH (shiftleft_tone_ad (0xED, shaping_table));
      glyph_lists[2] = PANGO_MOZ_MAKE_GLYPH (shiftleft_tone_ad (ucs2tis (cluster[1]), shaping_table));
      glyph_lists[3] = PANGO_MOZ_MAKE_GLYPH (0xD2);
      return 4;
    }
    else if (is_char_type (cluster[0], UpTailCons) &&
             is_char_type (cluster[1], AboveVowel) &&
             is_char_type (cluster[2], AboveDiac|Tone)) {
      glyph_lists[0] = PANGO_MOZ_MAKE_GLYPH (ucs2tis (cluster[0]));
      glyph_lists[1] = PANGO_MOZ_MAKE_GLYPH (shiftleft_av (ucs2tis (cluster[1]), shaping_table));
      glyph_lists[2] = PANGO_MOZ_MAKE_GLYPH (shiftleft_tone_ad (ucs2tis (cluster[2]), shaping_table));
      return 3;
    }
    else if (is_char_type (cluster[0], UpTailCons) &&
             is_char_type (cluster[1], BelowVowel) &&
             is_char_type (cluster[2], AboveDiac|Tone)) {
      glyph_lists[0] = PANGO_MOZ_MAKE_GLYPH (ucs2tis (cluster[0]));
      glyph_lists[1] = PANGO_MOZ_MAKE_GLYPH (ucs2tis (cluster[1]));
      glyph_lists[2] = PANGO_MOZ_MAKE_GLYPH (shiftdownleft_tone_ad (ucs2tis (cluster[2]), shaping_table));
      return 3;
    }
    else if (is_char_type (cluster[0], NoTailCons) &&
             is_char_type (cluster[1], BelowVowel) &&
             is_char_type (cluster[2], AboveDiac|Tone)) {
      glyph_lists[0] = PANGO_MOZ_MAKE_GLYPH (ucs2tis (cluster[0]));
      glyph_lists[1] = PANGO_MOZ_MAKE_GLYPH (ucs2tis (cluster[1]));
      glyph_lists[2] = PANGO_MOZ_MAKE_GLYPH (shiftdown_tone_ad (ucs2tis (cluster[2]), shaping_table));
      return 3;
    }
    else if (is_char_type (cluster[0], SpltTailCons) &&
             is_char_type (cluster[1], BelowVowel) &&
             is_char_type (cluster[2], AboveDiac|Tone)) {
      glyph_lists[0] = PANGO_MOZ_MAKE_GLYPH (tailcutcons (ucs2tis (cluster[0]), shaping_table));
      glyph_lists[1] = PANGO_MOZ_MAKE_GLYPH (ucs2tis (cluster[1]));
      glyph_lists[2] = PANGO_MOZ_MAKE_GLYPH (shiftdown_tone_ad (ucs2tis (cluster[2]), shaping_table));
      return 3;
    }
    else if (is_char_type (cluster[0], BotTailCons) &&
             is_char_type (cluster[1], BelowVowel) &&
             is_char_type (cluster[2], AboveDiac|Tone)) {
      glyph_lists[0] = PANGO_MOZ_MAKE_GLYPH (ucs2tis (cluster[0]));
      glyph_lists[1] = PANGO_MOZ_MAKE_GLYPH (shiftdown_bv_bd (ucs2tis (cluster[1]), shaping_table));
      glyph_lists[2] = PANGO_MOZ_MAKE_GLYPH (shiftdown_tone_ad (ucs2tis (cluster[2]), shaping_table));
      return 3;
    }
    else {
      glyph_lists[0] = PANGO_MOZ_MAKE_GLYPH (ucs2tis (cluster[0]));
      glyph_lists[1] = PANGO_MOZ_MAKE_GLYPH (ucs2tis (cluster[1]));
      glyph_lists[2] = PANGO_MOZ_MAKE_GLYPH (ucs2tis (cluster[2]));
      return 3;
    }
    break;
  }
  
  return 0;
}

static gint
get_glyphs_list(ThaiFontInfo *font_info,
                gunichar2	   *cluster,
                gint		     num_chrs,
                PangoliteGlyph	 *glyph_lists)
{
  PangoliteGlyph glyph;
  gint       xtis_index, i;

  switch (font_info->type) {
  case THAI_FONT_NONE:
    for (i = 0; i < num_chrs; i++)
      /* Change this to remove font dependency */
      glyph_lists[i] = 0; /* pangolite_x_get_unknown_glyph(font_info->font); */
    return num_chrs;
    
  case THAI_FONT_XTIS:
    /* If we are rendering with an XTIS font, we try to find a precomposed
     * glyph for the cluster.
     */
    xtis_index = 0x100 * (cluster[0] - 0xe00 + 0x20) + 0x30;
    if (cluster[1])
	    xtis_index +=8 * group1_map[cluster[1] - 0xe30];
    if (cluster[2])
	    xtis_index += group2_map[cluster[2] - 0xe30];
    glyph = PANGO_MOZ_MAKE_GLYPH(xtis_index);
    /*
    if (pangolite_x_has_glyph(font_info->font, glyph)) {
      glyph_lists[0] = glyph;
      return 1;
    }
    */
    for (i=0; i < num_chrs; i++)
      glyph_lists[i] = PANGO_MOZ_MAKE_GLYPH(0x100 * (cluster[i] - 0xe00 + 0x20) + 0x30);
    return num_chrs;
    
  case THAI_FONT_TIS:
    /* TIS620-0 + Wtt2.0 Extension
     */
    return get_adjusted_glyphs_list (font_info, cluster,
                                     num_chrs, glyph_lists, &tis620_0_shape_table);
    
  case THAI_FONT_TIS_MAC:
    /* MacIntosh Extension
     */
    return get_adjusted_glyphs_list(font_info, cluster,
                                    num_chrs, glyph_lists, &Mac_shape_table);
    
  case THAI_FONT_TIS_WIN:
    /* Microsoft Extension
     */
    return get_adjusted_glyphs_list(font_info, cluster,
                                    num_chrs, glyph_lists, &Win_shape_table);
    
  case THAI_FONT_ISO10646:
    for (i=0; i < num_chrs; i++)
      glyph_lists[i] = PANGO_MOZ_MAKE_GLYPH(cluster[i]);
    return num_chrs;
  }
  
  return 0;			/* Quiet GCC */
}

static void
add_cluster(ThaiFontInfo         *font_info,
            PangoliteGlyphString *glyphs,
            gint                 cluster_start,
            gunichar2            *cluster,
            gint                 num_chrs)
	     
{
  PangoliteGlyph glyphs_list[MAX_GLYPHS];
  gint           i, num_glyphs, ClusterStart=0;
  
  num_glyphs = get_glyphs_list(font_info, cluster, num_chrs, glyphs_list);
  for (i=0; i<num_glyphs; i++) {
    ClusterStart = (gint)GLYPH_COMBINING;
    if (i == 0)
      ClusterStart = num_chrs;

    add_glyph(font_info, glyphs, cluster_start, glyphs_list[i], ClusterStart);
  }
}

static gboolean
is_wtt_composible (gunichar2 cur_wc, gunichar2 nxt_wc)
{
  switch (TAC_compose_and_input_check_type_table[char_class(ucs2tis(cur_wc))]
          [char_class(ucs2tis(nxt_wc))]) {
  case 'A':
  case 'S':
  case 'R':
  case 'X':
    return FALSE;
    
  case 'C':
    return TRUE;
  }
  
  g_assert_not_reached();
  return FALSE;
}

static const gunichar2 *
get_next_cluster(const gunichar2 *text,
                 gint           length,
                 gunichar2       *cluster,
                 gint           *num_chrs)
{
  const gunichar2 *p;
  gint  n_chars = 0;
  
  p = text;
  while (p < text + length && n_chars < 3) {
    gunichar2 current = *p;
    
    if (n_chars == 0 ||
        is_wtt_composible ((gunichar2)(cluster[n_chars - 1]), current) ||
        (n_chars == 1 &&
         is_char_type (cluster[0], Cons) &&
         is_char_type (current, SaraAm)) ||
        (n_chars == 2 &&
         is_char_type (cluster[0], Cons) &&
         is_char_type (cluster[1], Tone) &&
         is_char_type (current, SaraAm))) {
      cluster[n_chars++] = current;
  p++;
    }
    else
      break;
  }
  
  *num_chrs = n_chars;
  return p;
} 

static void 
thai_engine_shape(const char       *fontCharset,
                  const gunichar2   *text,
                  gint             length,
                  PangoliteAnalysis    *analysis,
                  PangoliteGlyphString *glyphs)
{
  ThaiFontInfo   *font_info;
  const gunichar2 *p;
  const gunichar2 *log_cluster;
  gunichar2       cluster[MAX_CLUSTER_CHRS];
  gint           num_chrs;

  font_info = get_font_info(fontCharset);

  p = text;
  while (p < text + length) {
    log_cluster = p;
    p = get_next_cluster(p, text + length - p, cluster, &num_chrs);
    add_cluster(font_info, glyphs, log_cluster - text, cluster, num_chrs);
  }
}

static PangoliteCoverage *
thai_engine_get_coverage(const char *fontCharset,
                         const char *lang)
{
  PangoliteCoverage *result = pangolite_coverage_new();  
  ThaiFontInfo *font_info = get_font_info(fontCharset);
  
  if (font_info->type != THAI_FONT_NONE) {
    gunichar2 wc;
    
    for (wc = 0xe01; wc <= 0xe3a; wc++)
      pangolite_coverage_set(result, wc, PANGO_COVERAGE_EXACT);
    for (wc = 0xe3f; wc <= 0xe5b; wc++)
      pangolite_coverage_set(result, wc, PANGO_COVERAGE_EXACT);
  }
  
  return result;
}

static PangoliteEngine *
thai_engine_x_new()
{
  PangoliteEngineShape *result;
  
  result = g_new(PangoliteEngineShape, 1);
  
  result->engine.id = SCRIPT_ENGINE_NAME;
  result->engine.type = PANGO_ENGINE_TYPE_SHAPE;
  result->engine.length = sizeof(result);
  result->script_shape = thai_engine_shape;
  result->get_coverage = thai_engine_get_coverage;

  return(PangoliteEngine *)result;
}

/* The following three functions provide the public module API for
 * Pangolite. If we are compiling it is a module, then we name the
 * entry points script_engine_list, etc. But if we are compiling
 * it for inclusion directly in Pangolite, then we need them to
 * to have distinct names for this module, so we prepend
 * _pangolite_thai_x_
 */
#ifdef X_MODULE_PREFIX
#define MODULE_ENTRY(func) _pangolite_thai_x_##func
#else
#define MODULE_ENTRY(func) func
#endif

/* List the engines contained within this module
 */
void 
MODULE_ENTRY(script_engine_list)(PangoliteEngineInfo **engines, gint *n_engines)
{
  *engines = script_engines;
  *n_engines = G_N_ELEMENTS(script_engines);
}

/* Load a particular engine given the ID for the engine
 */
PangoliteEngine *
MODULE_ENTRY(script_engine_load)(const char *id)
{
  if (!strcmp(id, SCRIPT_ENGINE_NAME))
    return thai_engine_x_new();
  else
    return NULL;
}

void 
MODULE_ENTRY(script_engine_unload)(PangoliteEngine *engine)
{
}
