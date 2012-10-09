/*
 * Copyright © 2012  Google, Inc.
 *
 *  This is part of HarfBuzz, a text shaping library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
 * IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * Google Author(s): Behdad Esfahbod
 */

#ifndef HB_OT_SHAPE_COMPLEX_INDIC_PRIVATE_HH
#define HB_OT_SHAPE_COMPLEX_INDIC_PRIVATE_HH

#include "hb-private.hh"


#include "hb-ot-shape-complex-private.hh"
#include "hb-ot-shape-private.hh" /* XXX Remove */


/* buffer var allocations */
#define indic_category() complex_var_u8_0() /* indic_category_t */
#define indic_position() complex_var_u8_1() /* indic_matra_category_t */


#define INDIC_TABLE_ELEMENT_TYPE uint8_t

/* Cateories used in the OpenType spec:
 * https://www.microsoft.com/typography/otfntdev/devanot/shaping.aspx
 */
/* Note: This enum is duplicated in the -machine.rl source file.
 * Not sure how to avoid duplication. */
enum indic_category_t {
  OT_X = 0,
  OT_C,
  OT_V,
  OT_N,
  OT_H,
  OT_ZWNJ,
  OT_ZWJ,
  OT_M,
  OT_SM,
  OT_VD,
  OT_A,
  OT_NBSP,
  OT_DOTTEDCIRCLE, /* Not in the spec, but special in Uniscribe. /Very very/ special! */
  OT_RS, /* Register Shifter, used in Khmer OT spec */
  OT_Coeng,
  OT_Repha,
  OT_Ra /* Not explicitly listed in the OT spec, but used in the grammar. */
};

/* Visual positions in a syllable from left to right. */
enum indic_position_t {
  POS_START,

  POS_RA_TO_BECOME_REPH,
  POS_PRE_M,
  POS_PRE_C,

  POS_BASE_C,
  POS_AFTER_MAIN,

  POS_ABOVE_C,

  POS_BEFORE_SUB,
  POS_BELOW_C,
  POS_AFTER_SUB,

  POS_BEFORE_POST,
  POS_POST_C,
  POS_AFTER_POST,

  POS_FINAL_C,
  POS_SMVD,

  POS_END
};

/* Categories used in IndicSyllabicCategory.txt from UCD. */
enum indic_syllabic_category_t {
  INDIC_SYLLABIC_CATEGORY_OTHER			= OT_X,

  INDIC_SYLLABIC_CATEGORY_AVAGRAHA		= OT_X,
  INDIC_SYLLABIC_CATEGORY_BINDU			= OT_SM,
  INDIC_SYLLABIC_CATEGORY_CONSONANT		= OT_C,
  INDIC_SYLLABIC_CATEGORY_CONSONANT_DEAD	= OT_C,
  INDIC_SYLLABIC_CATEGORY_CONSONANT_FINAL	= OT_C,
  INDIC_SYLLABIC_CATEGORY_CONSONANT_HEAD_LETTER	= OT_C,
  INDIC_SYLLABIC_CATEGORY_CONSONANT_MEDIAL	= OT_C,
  INDIC_SYLLABIC_CATEGORY_CONSONANT_PLACEHOLDER	= OT_NBSP,
  INDIC_SYLLABIC_CATEGORY_CONSONANT_SUBJOINED	= OT_C,
  INDIC_SYLLABIC_CATEGORY_CONSONANT_REPHA	= OT_Repha,
  INDIC_SYLLABIC_CATEGORY_MODIFYING_LETTER	= OT_X,
  INDIC_SYLLABIC_CATEGORY_NUKTA			= OT_N,
  INDIC_SYLLABIC_CATEGORY_REGISTER_SHIFTER	= OT_RS,
  INDIC_SYLLABIC_CATEGORY_TONE_LETTER		= OT_X,
  INDIC_SYLLABIC_CATEGORY_TONE_MARK		= OT_X,
  INDIC_SYLLABIC_CATEGORY_VIRAMA		= OT_H,
  INDIC_SYLLABIC_CATEGORY_VISARGA		= OT_SM,
  INDIC_SYLLABIC_CATEGORY_VOWEL			= OT_V,
  INDIC_SYLLABIC_CATEGORY_VOWEL_DEPENDENT	= OT_M,
  INDIC_SYLLABIC_CATEGORY_VOWEL_INDEPENDENT	= OT_V
};

/* Categories used in IndicSMatraCategory.txt from UCD */
enum indic_matra_category_t {
  INDIC_MATRA_CATEGORY_NOT_APPLICABLE		= POS_END,

  INDIC_MATRA_CATEGORY_LEFT			= POS_PRE_C,
  INDIC_MATRA_CATEGORY_TOP			= POS_ABOVE_C,
  INDIC_MATRA_CATEGORY_BOTTOM			= POS_BELOW_C,
  INDIC_MATRA_CATEGORY_RIGHT			= POS_POST_C,

  /* These should resolve to the position of the last part of the split sequence. */
  INDIC_MATRA_CATEGORY_BOTTOM_AND_RIGHT		= INDIC_MATRA_CATEGORY_RIGHT,
  INDIC_MATRA_CATEGORY_LEFT_AND_RIGHT		= INDIC_MATRA_CATEGORY_RIGHT,
  INDIC_MATRA_CATEGORY_TOP_AND_BOTTOM		= INDIC_MATRA_CATEGORY_BOTTOM,
  INDIC_MATRA_CATEGORY_TOP_AND_BOTTOM_AND_RIGHT	= INDIC_MATRA_CATEGORY_RIGHT,
  INDIC_MATRA_CATEGORY_TOP_AND_LEFT		= INDIC_MATRA_CATEGORY_TOP,
  INDIC_MATRA_CATEGORY_TOP_AND_LEFT_AND_RIGHT	= INDIC_MATRA_CATEGORY_RIGHT,
  INDIC_MATRA_CATEGORY_TOP_AND_RIGHT		= INDIC_MATRA_CATEGORY_RIGHT,

  INDIC_MATRA_CATEGORY_INVISIBLE		= INDIC_MATRA_CATEGORY_NOT_APPLICABLE,
  INDIC_MATRA_CATEGORY_OVERSTRUCK		= INDIC_MATRA_CATEGORY_NOT_APPLICABLE,
  INDIC_MATRA_CATEGORY_VISUAL_ORDER_LEFT	= INDIC_MATRA_CATEGORY_NOT_APPLICABLE
};

/* Note: We use ASSERT_STATIC_EXPR_ZERO() instead of ASSERT_STATIC_EXPR() and the comma operation
 * because gcc fails to optimize the latter and fills the table in at runtime. */
#define INDIC_COMBINE_CATEGORIES(S,M) \
  (ASSERT_STATIC_EXPR_ZERO (M == INDIC_MATRA_CATEGORY_NOT_APPLICABLE || (S == INDIC_SYLLABIC_CATEGORY_VIRAMA || S == INDIC_SYLLABIC_CATEGORY_VOWEL_DEPENDENT)) + \
   ASSERT_STATIC_EXPR_ZERO (S < 16 && M < 16) + \
   ((M << 4) | S))


#include "hb-ot-shape-complex-indic-table.hh"


#define IN_HALF_BLOCK(u, Base) (((u) & ~0x7F) == (Base))

#define IS_DEVA(u) (IN_HALF_BLOCK (u, 0x0900))
#define IS_BENG(u) (IN_HALF_BLOCK (u, 0x0980))
#define IS_GURU(u) (IN_HALF_BLOCK (u, 0x0A00))
#define IS_GUJR(u) (IN_HALF_BLOCK (u, 0x0A80))
#define IS_ORYA(u) (IN_HALF_BLOCK (u, 0x0B00))
#define IS_TAML(u) (IN_HALF_BLOCK (u, 0x0B80))
#define IS_TELU(u) (IN_HALF_BLOCK (u, 0x0C00))
#define IS_KNDA(u) (IN_HALF_BLOCK (u, 0x0C80))
#define IS_MLYM(u) (IN_HALF_BLOCK (u, 0x0D00))
#define IS_SINH(u) (IN_HALF_BLOCK (u, 0x0D80))
#define IS_KHMR(u) (IN_HALF_BLOCK (u, 0x1780))


#define MATRA_POS_LEFT(u)	POS_PRE_M
#define MATRA_POS_RIGHT(u)	( \
				  IS_DEVA(u) ? POS_AFTER_SUB  : \
				  IS_BENG(u) ? POS_AFTER_POST : \
				  IS_GURU(u) ? POS_AFTER_POST : \
				  IS_GUJR(u) ? POS_AFTER_POST : \
				  IS_ORYA(u) ? POS_AFTER_POST : \
				  IS_TAML(u) ? POS_AFTER_POST : \
				  IS_TELU(u) ? (u <= 0x0C42 ? POS_BEFORE_SUB : POS_AFTER_SUB) : \
				  IS_KNDA(u) ? (u < 0x0CC3 || u > 0xCD6 ? POS_BEFORE_SUB : POS_AFTER_SUB) : \
				  IS_MLYM(u) ? POS_AFTER_POST : \
				  IS_SINH(u) ? POS_AFTER_SUB  : \
				  IS_KHMR(u) ? POS_AFTER_POST : \
				  /*default*/  POS_AFTER_SUB    \
				)
#define MATRA_POS_TOP(u)	( /* BENG and MLYM don't have top matras. */ \
				  IS_DEVA(u) ? POS_AFTER_SUB  : \
				  IS_GURU(u) ? POS_AFTER_POST : /* Deviate from spec */ \
				  IS_GUJR(u) ? POS_AFTER_SUB  : \
				  IS_ORYA(u) ? POS_AFTER_MAIN : \
				  IS_TAML(u) ? POS_AFTER_SUB  : \
				  IS_TELU(u) ? POS_BEFORE_SUB : \
				  IS_KNDA(u) ? POS_BEFORE_SUB : \
				  IS_SINH(u) ? POS_AFTER_SUB  : \
				  IS_KHMR(u) ? POS_AFTER_POST : \
				  /*default*/  POS_AFTER_SUB    \
				)
#define MATRA_POS_BOTTOM(u)	( \
				  IS_DEVA(u) ? POS_AFTER_SUB  : \
				  IS_BENG(u) ? POS_AFTER_SUB  : \
				  IS_GURU(u) ? POS_AFTER_POST : \
				  IS_GUJR(u) ? POS_AFTER_POST : \
				  IS_ORYA(u) ? POS_AFTER_SUB  : \
				  IS_TAML(u) ? POS_AFTER_POST : \
				  IS_TELU(u) ? POS_BEFORE_SUB : \
				  IS_KNDA(u) ? POS_BEFORE_SUB : \
				  IS_MLYM(u) ? POS_AFTER_POST : \
				  IS_SINH(u) ? POS_AFTER_SUB  : \
				  IS_KHMR(u) ? POS_AFTER_POST : \
				  /*default*/  POS_AFTER_SUB    \
				)


static inline indic_position_t
matra_position (hb_codepoint_t u, indic_position_t side)
{
  switch ((int) side)
  {
    case POS_PRE_C:	return MATRA_POS_LEFT (u);
    case POS_POST_C:	return MATRA_POS_RIGHT (u);
    case POS_ABOVE_C:	return MATRA_POS_TOP (u);
    case POS_BELOW_C:	return MATRA_POS_BOTTOM (u);
  };
  abort ();
}



/* XXX
 * This is a hack for now.  We should move this data into the main Indic table.
 * Or completely remove it and just check in the tables.
 */
static const hb_codepoint_t ra_chars[] = {
  0x0930, /* Devanagari */
  0x09B0, /* Bengali */
  0x09F0, /* Bengali */
  0x0A30, /* Gurmukhi */	/* No Reph */
  0x0AB0, /* Gujarati */
  0x0B30, /* Oriya */
  0x0BB0, /* Tamil */		/* No Reph */
  0x0C30, /* Telugu */		/* Reph formed only with ZWJ */
  0x0CB0, /* Kannada */
  0x0D30, /* Malayalam */	/* No Reph, Logical Repha */

  0x0DBB, /* Sinhala */		/* Reph formed only with ZWJ */

  0x179A, /* Khmer */		/* No Reph, Visual Repha */
};

static inline indic_position_t
consonant_position (hb_codepoint_t  u)
{
  if ((u & ~0x007F) == 0x1780)
    return POS_BELOW_C; /* In Khmer coeng model, post and below forms should not be reordered. */
  return POS_BASE_C; /* Will recategorize later based on font lookups. */
}

static inline bool
is_ra (hb_codepoint_t u)
{
  for (unsigned int i = 0; i < ARRAY_LENGTH (ra_chars); i++)
    if (u == ra_chars[i])
      return true;
  return false;
}


static inline bool
is_one_of (const hb_glyph_info_t &info, unsigned int flags)
{
  /* If it ligated, all bets are off. */
  if (is_a_ligature (info)) return false;
  return !!(FLAG (info.indic_category()) & flags);
}

#define JOINER_FLAGS (FLAG (OT_ZWJ) | FLAG (OT_ZWNJ))
static inline bool
is_joiner (const hb_glyph_info_t &info)
{
  return is_one_of (info, JOINER_FLAGS);
}

/* Note:
 *
 * We treat Vowels and placeholders as if they were consonants.  This is safe because Vowels
 * cannot happen in a consonant syllable.  The plus side however is, we can call the
 * consonant syllable logic from the vowel syllable function and get it all right! */
#define CONSONANT_FLAGS (FLAG (OT_C) | FLAG (OT_Ra) | FLAG (OT_V) | FLAG (OT_NBSP) | FLAG (OT_DOTTEDCIRCLE))
static inline bool
is_consonant (const hb_glyph_info_t &info)
{
  return is_one_of (info, CONSONANT_FLAGS);
}

#define HALANT_OR_COENG_FLAGS (FLAG (OT_H) | FLAG (OT_Coeng))
static inline bool
is_halant_or_coeng (const hb_glyph_info_t &info)
{
  return is_one_of (info, HALANT_OR_COENG_FLAGS);
}

static inline void
set_indic_properties (hb_glyph_info_t &info)
{
  hb_codepoint_t u = info.codepoint;
  unsigned int type = get_indic_categories (u);
  indic_category_t cat = (indic_category_t) (type & 0x0F);
  indic_position_t pos = (indic_position_t) (type >> 4);


  /*
   * Re-assign category
   */


  /* The spec says U+0952 is OT_A.  However, testing shows that Uniscribe
   * treats U+0951..U+0952 all as OT_VD.
   * TESTS:
   * U+092E,U+0947,U+0952
   * U+092E,U+0952,U+0947
   * U+092E,U+0947,U+0951
   * U+092E,U+0951,U+0947
   * */
  if (unlikely (hb_in_range<hb_codepoint_t> (u, 0x0951, 0x0954)))
    cat = OT_VD;

  if (unlikely (u == 0x17D1))
    cat = OT_X;
  if (cat == OT_X &&
      unlikely (hb_in_range<hb_codepoint_t> (u, 0x17CB, 0x17D3))) /* Khmer Various signs */
  {
    /* These are like Top Matras. */
    cat = OT_M;
    pos = POS_ABOVE_C;
  }
  if (u == 0x17C6) /* Khmer Bindu doesn't like to be repositioned. */
    cat = OT_N;

  if (unlikely (u == 0x17D2)) cat = OT_Coeng; /* Khmer coeng */
  else if (unlikely (u == 0x200C)) cat = OT_ZWNJ;
  else if (unlikely (u == 0x200D)) cat = OT_ZWJ;
  else if (unlikely (u == 0x25CC)) cat = OT_DOTTEDCIRCLE;
  else if (unlikely (u == 0x0A71)) cat = OT_SM; /* GURMUKHI ADDAK.  More like consonant medial. like 0A75. */

  if (cat == OT_Repha) {
    /* There are two kinds of characters marked as Repha:
     * - The ones that are GenCat=Mn are already positioned visually, ie. after base. (eg. Khmer)
     * - The ones that are GenCat=Lo is encoded logically, ie. beginning of syllable. (eg. Malayalam)
     *
     * We recategorize the first kind to look like a Nukta and attached to the base directly.
     */
    if (_hb_glyph_info_get_general_category (&info) == HB_UNICODE_GENERAL_CATEGORY_NON_SPACING_MARK)
      cat = OT_N;
  }



  /*
   * Re-assign position.
   */

  if ((FLAG (cat) & CONSONANT_FLAGS))
  {
    pos = consonant_position (u);
    if (is_ra (u))
      cat = OT_Ra;
  }
  else if (cat == OT_M)
  {
    pos = matra_position (u, pos);
  }
  else if (cat == OT_SM || cat == OT_VD)
  {
    pos = POS_SMVD;
  }

  if (unlikely (u == 0x0B01)) pos = POS_BEFORE_SUB; /* Oriya Bindu is BeforeSub in the spec. */



  info.indic_category() = cat;
  info.indic_position() = pos;
}



#endif /* HB_OT_SHAPE_COMPLEX_INDIC_PRIVATE_HH */
