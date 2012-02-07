/*
 * Copyright (C) 2009  Red Hat, Inc.
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
 * Red Hat Author(s): Behdad Esfahbod
 */

#ifndef HB_UNICODE_H
#define HB_UNICODE_H

#include "hb-common.h"

HB_BEGIN_DECLS


/* Unicode General Category property */
typedef enum
{
  HB_CATEGORY_CONTROL,
  HB_CATEGORY_FORMAT,
  HB_CATEGORY_UNASSIGNED,
  HB_CATEGORY_PRIVATE_USE,
  HB_CATEGORY_SURROGATE,
  HB_CATEGORY_LOWERCASE_LETTER,
  HB_CATEGORY_MODIFIER_LETTER,
  HB_CATEGORY_OTHER_LETTER,
  HB_CATEGORY_TITLECASE_LETTER,
  HB_CATEGORY_UPPERCASE_LETTER,
  HB_CATEGORY_COMBINING_MARK,
  HB_CATEGORY_ENCLOSING_MARK,
  HB_CATEGORY_NON_SPACING_MARK,
  HB_CATEGORY_DECIMAL_NUMBER,
  HB_CATEGORY_LETTER_NUMBER,
  HB_CATEGORY_OTHER_NUMBER,
  HB_CATEGORY_CONNECT_PUNCTUATION,
  HB_CATEGORY_DASH_PUNCTUATION,
  HB_CATEGORY_CLOSE_PUNCTUATION,
  HB_CATEGORY_FINAL_PUNCTUATION,
  HB_CATEGORY_INITIAL_PUNCTUATION,
  HB_CATEGORY_OTHER_PUNCTUATION,
  HB_CATEGORY_OPEN_PUNCTUATION,
  HB_CATEGORY_CURRENCY_SYMBOL,
  HB_CATEGORY_MODIFIER_SYMBOL,
  HB_CATEGORY_MATH_SYMBOL,
  HB_CATEGORY_OTHER_SYMBOL,
  HB_CATEGORY_LINE_SEPARATOR,
  HB_CATEGORY_PARAGRAPH_SEPARATOR,
  HB_CATEGORY_SPACE_SEPARATOR
} hb_category_t;

/* Unicode Script property */
typedef enum
{                               /* ISO 15924 code */
  HB_SCRIPT_INVALID_CODE = -1,
  HB_SCRIPT_COMMON       = 0,   /* Zyyy */
  HB_SCRIPT_INHERITED,          /* Qaai */
  HB_SCRIPT_ARABIC,             /* Arab */
  HB_SCRIPT_ARMENIAN,           /* Armn */
  HB_SCRIPT_BENGALI,            /* Beng */
  HB_SCRIPT_BOPOMOFO,           /* Bopo */
  HB_SCRIPT_CHEROKEE,           /* Cher */
  HB_SCRIPT_COPTIC,             /* Qaac */
  HB_SCRIPT_CYRILLIC,           /* Cyrl (Cyrs) */
  HB_SCRIPT_DESERET,            /* Dsrt */
  HB_SCRIPT_DEVANAGARI,         /* Deva */
  HB_SCRIPT_ETHIOPIC,           /* Ethi */
  HB_SCRIPT_GEORGIAN,           /* Geor (Geon, Geoa) */
  HB_SCRIPT_GOTHIC,             /* Goth */
  HB_SCRIPT_GREEK,              /* Grek */
  HB_SCRIPT_GUJARATI,           /* Gujr */
  HB_SCRIPT_GURMUKHI,           /* Guru */
  HB_SCRIPT_HAN,                /* Hani */
  HB_SCRIPT_HANGUL,             /* Hang */
  HB_SCRIPT_HEBREW,             /* Hebr */
  HB_SCRIPT_HIRAGANA,           /* Hira */
  HB_SCRIPT_KANNADA,            /* Knda */
  HB_SCRIPT_KATAKANA,           /* Kana */
  HB_SCRIPT_KHMER,              /* Khmr */
  HB_SCRIPT_LAO,                /* Laoo */
  HB_SCRIPT_LATIN,              /* Latn (Latf, Latg) */
  HB_SCRIPT_MALAYALAM,          /* Mlym */
  HB_SCRIPT_MONGOLIAN,          /* Mong */
  HB_SCRIPT_MYANMAR,            /* Mymr */
  HB_SCRIPT_OGHAM,              /* Ogam */
  HB_SCRIPT_OLD_ITALIC,         /* Ital */
  HB_SCRIPT_ORIYA,              /* Orya */
  HB_SCRIPT_RUNIC,              /* Runr */
  HB_SCRIPT_SINHALA,            /* Sinh */
  HB_SCRIPT_SYRIAC,             /* Syrc (Syrj, Syrn, Syre) */
  HB_SCRIPT_TAMIL,              /* Taml */
  HB_SCRIPT_TELUGU,             /* Telu */
  HB_SCRIPT_THAANA,             /* Thaa */
  HB_SCRIPT_THAI,               /* Thai */
  HB_SCRIPT_TIBETAN,            /* Tibt */
  HB_SCRIPT_CANADIAN_ABORIGINAL, /* Cans */
  HB_SCRIPT_YI,                 /* Yiii */
  HB_SCRIPT_TAGALOG,            /* Tglg */
  HB_SCRIPT_HANUNOO,            /* Hano */
  HB_SCRIPT_BUHID,              /* Buhd */
  HB_SCRIPT_TAGBANWA,           /* Tagb */

  /* Unicode-4.0 additions */
  HB_SCRIPT_BRAILLE,            /* Brai */
  HB_SCRIPT_CYPRIOT,            /* Cprt */
  HB_SCRIPT_LIMBU,              /* Limb */
  HB_SCRIPT_OSMANYA,            /* Osma */
  HB_SCRIPT_SHAVIAN,            /* Shaw */
  HB_SCRIPT_LINEAR_B,           /* Linb */
  HB_SCRIPT_TAI_LE,             /* Tale */
  HB_SCRIPT_UGARITIC,           /* Ugar */

  /* Unicode-4.1 additions */
  HB_SCRIPT_NEW_TAI_LUE,        /* Talu */
  HB_SCRIPT_BUGINESE,           /* Bugi */
  HB_SCRIPT_GLAGOLITIC,         /* Glag */
  HB_SCRIPT_TIFINAGH,           /* Tfng */
  HB_SCRIPT_SYLOTI_NAGRI,       /* Sylo */
  HB_SCRIPT_OLD_PERSIAN,        /* Xpeo */
  HB_SCRIPT_KHAROSHTHI,         /* Khar */

  /* Unicode-5.0 additions */
  HB_SCRIPT_UNKNOWN,            /* Zzzz */
  HB_SCRIPT_BALINESE,           /* Bali */
  HB_SCRIPT_CUNEIFORM,          /* Xsux */
  HB_SCRIPT_PHOENICIAN,         /* Phnx */
  HB_SCRIPT_PHAGS_PA,           /* Phag */
  HB_SCRIPT_NKO,                /* Nkoo */

  /* Unicode-5.1 additions */
  HB_SCRIPT_KAYAH_LI,           /* Kali */
  HB_SCRIPT_LEPCHA,             /* Lepc */
  HB_SCRIPT_REJANG,             /* Rjng */
  HB_SCRIPT_SUNDANESE,          /* Sund */
  HB_SCRIPT_SAURASHTRA,         /* Saur */
  HB_SCRIPT_CHAM,               /* Cham */
  HB_SCRIPT_OL_CHIKI,           /* Olck */
  HB_SCRIPT_VAI,                /* Vaii */
  HB_SCRIPT_CARIAN,             /* Cari */
  HB_SCRIPT_LYCIAN,             /* Lyci */
  HB_SCRIPT_LYDIAN,             /* Lydi */

  /* Unicode-5.2 additions */
  HB_SCRIPT_AVESTAN,                /* Avst */
  HB_SCRIPT_BAMUM,                  /* Bamu */
  HB_SCRIPT_EGYPTIAN_HIEROGLYPHS,   /* Egyp */
  HB_SCRIPT_IMPERIAL_ARAMAIC,       /* Armi */
  HB_SCRIPT_INSCRIPTIONAL_PAHLAVI,  /* Phli */
  HB_SCRIPT_INSCRIPTIONAL_PARTHIAN, /* Prti */
  HB_SCRIPT_JAVANESE,               /* Java */
  HB_SCRIPT_KAITHI,                 /* Kthi */
  HB_SCRIPT_LISU,                   /* Lisu */
  HB_SCRIPT_MEETEI_MAYEK,           /* Mtei */
  HB_SCRIPT_OLD_SOUTH_ARABIAN,      /* Sarb */
  HB_SCRIPT_OLD_TURKIC,             /* Orkh */
  HB_SCRIPT_SAMARITAN,              /* Samr */
  HB_SCRIPT_TAI_THAM,               /* Lana */
  HB_SCRIPT_TAI_VIET,               /* Tavt */

  /* Unicode-6.0 additions */
  HB_SCRIPT_BATAK,                  /* Batk */
  HB_SCRIPT_BRAHMI,                 /* Brah */
  HB_SCRIPT_MANDAIC,                /* Mand */

  /* Unicode-6.1 additions */
  HB_SCRIPT_CHAKMA,                 /* Cakm */
  HB_SCRIPT_MEROITIC_CURSIVE,       /* Merc */
  HB_SCRIPT_MEROITIC_HIEROGLYPHS,   /* Mero */
  HB_SCRIPT_MIAO,                   /* Plrd */
  HB_SCRIPT_SHARADA,                /* Shrd */
  HB_SCRIPT_SORA_SOMPENG,           /* Sora */
  HB_SCRIPT_TAKRI                   /* Takr */        
} hb_script_t;


/*
 * hb_unicode_funcs_t
 */

typedef struct _hb_unicode_funcs_t hb_unicode_funcs_t;

hb_unicode_funcs_t *
hb_unicode_funcs_create (void);

hb_unicode_funcs_t *
hb_unicode_funcs_reference (hb_unicode_funcs_t *ufuncs);

unsigned int
hb_unicode_funcs_get_reference_count (hb_unicode_funcs_t *ufuncs);

void
hb_unicode_funcs_destroy (hb_unicode_funcs_t *ufuncs);

hb_unicode_funcs_t *
hb_unicode_funcs_copy (hb_unicode_funcs_t *ufuncs);

void
hb_unicode_funcs_make_immutable (hb_unicode_funcs_t *ufuncs);

hb_bool_t
hb_unicode_funcs_is_immutable (hb_unicode_funcs_t *ufuncs);

/*
 * funcs
 */


/* typedefs */

typedef hb_codepoint_t (*hb_unicode_get_mirroring_func_t) (hb_codepoint_t unicode);
typedef hb_category_t (*hb_unicode_get_general_category_func_t) (hb_codepoint_t unicode);
typedef hb_script_t (*hb_unicode_get_script_func_t) (hb_codepoint_t unicode);
typedef unsigned int (*hb_unicode_get_combining_class_func_t) (hb_codepoint_t unicode);
typedef unsigned int (*hb_unicode_get_eastasian_width_func_t) (hb_codepoint_t unicode);


/* setters */

void
hb_unicode_funcs_set_mirroring_func (hb_unicode_funcs_t *ufuncs,
				     hb_unicode_get_mirroring_func_t mirroring_func);

void
hb_unicode_funcs_set_general_category_func (hb_unicode_funcs_t *ufuncs,
					    hb_unicode_get_general_category_func_t general_category_func);

void
hb_unicode_funcs_set_script_func (hb_unicode_funcs_t *ufuncs,
				  hb_unicode_get_script_func_t script_func);

void
hb_unicode_funcs_set_combining_class_func (hb_unicode_funcs_t *ufuncs,
					   hb_unicode_get_combining_class_func_t combining_class_func);

void
hb_unicode_funcs_set_eastasian_width_func (hb_unicode_funcs_t *ufuncs,
					   hb_unicode_get_eastasian_width_func_t eastasian_width_func);


/* getters */

/* These never return NULL.  Return fallback defaults instead. */

hb_unicode_get_mirroring_func_t
hb_unicode_funcs_get_mirroring_func (hb_unicode_funcs_t *ufuncs);

hb_unicode_get_general_category_func_t
hb_unicode_funcs_get_general_category_func (hb_unicode_funcs_t *ufuncs);

hb_unicode_get_script_func_t
hb_unicode_funcs_get_script_func (hb_unicode_funcs_t *ufuncs);

hb_unicode_get_combining_class_func_t
hb_unicode_funcs_get_combining_class_func (hb_unicode_funcs_t *ufuncs);

hb_unicode_get_eastasian_width_func_t
hb_unicode_funcs_get_eastasian_width_func (hb_unicode_funcs_t *ufuncs);


/* accessors */

hb_codepoint_t
hb_unicode_get_mirroring (hb_unicode_funcs_t *ufuncs,
			  hb_codepoint_t unicode);

hb_category_t
hb_unicode_get_general_category (hb_unicode_funcs_t *ufuncs,
				 hb_codepoint_t unicode);

hb_script_t
hb_unicode_get_script (hb_unicode_funcs_t *ufuncs,
		       hb_codepoint_t unicode);

unsigned int
hb_unicode_get_combining_class (hb_unicode_funcs_t *ufuncs,
				hb_codepoint_t unicode);

unsigned int
hb_unicode_get_eastasian_width (hb_unicode_funcs_t *ufuncs,
				hb_codepoint_t unicode);


HB_END_DECLS

#endif /* HB_UNICODE_H */
