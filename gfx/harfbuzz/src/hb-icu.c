/*
 * Copyright (C) 2009  Red Hat, Inc.
 * Copyright (C) 2009  Keith Stribley <devel@thanlwinsoft.org>
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

#include "hb-private.h"

#include "hb-icu.h"

#include "hb-unicode-private.h"

#include <unicode/uversion.h>
#include <unicode/uchar.h>
#include <unicode/uscript.h>

HB_BEGIN_DECLS


static hb_codepoint_t hb_icu_get_mirroring (hb_codepoint_t unicode) { return u_charMirror(unicode); }
static unsigned int hb_icu_get_combining_class (hb_codepoint_t unicode) { return u_getCombiningClass (unicode); }

static unsigned int
hb_icu_get_eastasian_width (hb_codepoint_t unicode)
{
  switch (u_getIntPropertyValue(unicode, UCHAR_EAST_ASIAN_WIDTH))
  {
  case U_EA_WIDE:
  case U_EA_FULLWIDTH:
    return 2;
  case U_EA_NEUTRAL:
  case U_EA_AMBIGUOUS:
  case U_EA_HALFWIDTH:
  case U_EA_NARROW:
    return 1;
  }
  return 1;
}

static hb_category_t
hb_icu_get_general_category (hb_codepoint_t unicode)
{
  switch (u_getIntPropertyValue(unicode, UCHAR_GENERAL_CATEGORY))
  {
  case U_UNASSIGNED:			return HB_CATEGORY_UNASSIGNED;

  case U_UPPERCASE_LETTER:		return HB_CATEGORY_UPPERCASE_LETTER;	/* Lu */
  case U_LOWERCASE_LETTER:		return HB_CATEGORY_LOWERCASE_LETTER;	/* Ll */
  case U_TITLECASE_LETTER:		return HB_CATEGORY_TITLECASE_LETTER;	/* Lt */
  case U_MODIFIER_LETTER:		return HB_CATEGORY_MODIFIER_LETTER;	/* Lm */
  case U_OTHER_LETTER:			return HB_CATEGORY_OTHER_LETTER;	/* Lo */

  case U_NON_SPACING_MARK:		return HB_CATEGORY_NON_SPACING_MARK;	/* Mn */
  case U_ENCLOSING_MARK:		return HB_CATEGORY_ENCLOSING_MARK;	/* Me */
  case U_COMBINING_SPACING_MARK:	return HB_CATEGORY_COMBINING_MARK;	/* Mc */

  case U_DECIMAL_DIGIT_NUMBER:		return HB_CATEGORY_DECIMAL_NUMBER;	/* Nd */
  case U_LETTER_NUMBER:			return HB_CATEGORY_LETTER_NUMBER;	/* Nl */
  case U_OTHER_NUMBER:			return HB_CATEGORY_OTHER_NUMBER;	/* No */

  case U_SPACE_SEPARATOR:		return HB_CATEGORY_SPACE_SEPARATOR;	/* Zs */
  case U_LINE_SEPARATOR:		return HB_CATEGORY_LINE_SEPARATOR;	/* Zl */
  case U_PARAGRAPH_SEPARATOR:		return HB_CATEGORY_PARAGRAPH_SEPARATOR;	/* Zp */

  case U_CONTROL_CHAR:			return HB_CATEGORY_CONTROL;		/* Cc */
  case U_FORMAT_CHAR:			return HB_CATEGORY_FORMAT;		/* Cf */
  case U_PRIVATE_USE_CHAR:		return HB_CATEGORY_PRIVATE_USE;		/* Co */
  case U_SURROGATE:			return HB_CATEGORY_SURROGATE;		/* Cs */


  case U_DASH_PUNCTUATION:		return HB_CATEGORY_DASH_PUNCTUATION;	/* Pd */
  case U_START_PUNCTUATION:		return HB_CATEGORY_OPEN_PUNCTUATION;	/* Ps */
  case U_END_PUNCTUATION:		return HB_CATEGORY_CLOSE_PUNCTUATION;	/* Pe */
  case U_CONNECTOR_PUNCTUATION:		return HB_CATEGORY_CONNECT_PUNCTUATION;	/* Pc */
  case U_OTHER_PUNCTUATION:		return HB_CATEGORY_OTHER_PUNCTUATION;	/* Po */

  case U_MATH_SYMBOL:			return HB_CATEGORY_MATH_SYMBOL;		/* Sm */
  case U_CURRENCY_SYMBOL:		return HB_CATEGORY_CURRENCY_SYMBOL;	/* Sc */
  case U_MODIFIER_SYMBOL:		return HB_CATEGORY_MODIFIER_SYMBOL;	/* Sk */
  case U_OTHER_SYMBOL:			return HB_CATEGORY_OTHER_SYMBOL;	/* So */

  case U_INITIAL_PUNCTUATION:		return HB_CATEGORY_INITIAL_PUNCTUATION;	/* Pi */
  case U_FINAL_PUNCTUATION:		return HB_CATEGORY_FINAL_PUNCTUATION;	/* Pf */
  }

  return HB_CATEGORY_UNASSIGNED;
}

static hb_script_t
hb_icu_get_script (hb_codepoint_t unicode)
{
  UErrorCode status = U_ZERO_ERROR;
  UScriptCode scriptCode = uscript_getScript(unicode, &status);
  switch ((int) scriptCode)
  {
#define CHECK_ICU_VERSION(major, minor) \
	U_ICU_VERSION_MAJOR_NUM > (major) || (U_ICU_VERSION_MAJOR_NUM == (major) && U_ICU_VERSION_MINOR_NUM >= (minor))
#define MATCH_SCRIPT(C) case USCRIPT_##C: return HB_SCRIPT_##C
#define MATCH_SCRIPT2(C1, C2) case USCRIPT_##C1: return HB_SCRIPT_##C2
  MATCH_SCRIPT (INVALID_CODE);
  MATCH_SCRIPT (COMMON);             /* Zyyy */
  MATCH_SCRIPT (INHERITED);          /* Qaai */
  MATCH_SCRIPT (ARABIC);             /* Arab */
  MATCH_SCRIPT (ARMENIAN);           /* Armn */
  MATCH_SCRIPT (BENGALI);            /* Beng */
  MATCH_SCRIPT (BOPOMOFO);           /* Bopo */
  MATCH_SCRIPT (CHEROKEE);           /* Cher */
  MATCH_SCRIPT (COPTIC);             /* Qaac */
  MATCH_SCRIPT (CYRILLIC);           /* Cyrl (Cyrs) */
  MATCH_SCRIPT (DESERET);            /* Dsrt */
  MATCH_SCRIPT (DEVANAGARI);         /* Deva */
  MATCH_SCRIPT (ETHIOPIC);           /* Ethi */
  MATCH_SCRIPT (GEORGIAN);           /* Geor (Geon); Geoa) */
  MATCH_SCRIPT (GOTHIC);             /* Goth */
  MATCH_SCRIPT (GREEK);              /* Grek */
  MATCH_SCRIPT (GUJARATI);           /* Gujr */
  MATCH_SCRIPT (GURMUKHI);           /* Guru */
  MATCH_SCRIPT (HAN);                /* Hani */
  MATCH_SCRIPT (HANGUL);             /* Hang */
  MATCH_SCRIPT (HEBREW);             /* Hebr */
  MATCH_SCRIPT (HIRAGANA);           /* Hira */
  MATCH_SCRIPT (KANNADA);            /* Knda */
  MATCH_SCRIPT (KATAKANA);           /* Kana */
  MATCH_SCRIPT (KHMER);              /* Khmr */
  MATCH_SCRIPT (LAO);                /* Laoo */
  MATCH_SCRIPT (LATIN);              /* Latn (Latf); Latg) */
  MATCH_SCRIPT (MALAYALAM);          /* Mlym */
  MATCH_SCRIPT (MONGOLIAN);          /* Mong */
  MATCH_SCRIPT (MYANMAR);            /* Mymr */
  MATCH_SCRIPT (OGHAM);              /* Ogam */
  MATCH_SCRIPT (OLD_ITALIC);         /* Ital */
  MATCH_SCRIPT (ORIYA);              /* Orya */
  MATCH_SCRIPT (RUNIC);              /* Runr */
  MATCH_SCRIPT (SINHALA);            /* Sinh */
  MATCH_SCRIPT (SYRIAC);             /* Syrc (Syrj, Syrn); Syre) */
  MATCH_SCRIPT (TAMIL);              /* Taml */
  MATCH_SCRIPT (TELUGU);             /* Telu */
  MATCH_SCRIPT (THAANA);             /* Thaa */
  MATCH_SCRIPT (THAI);               /* Thai */
  MATCH_SCRIPT (TIBETAN);            /* Tibt */
  MATCH_SCRIPT (CANADIAN_ABORIGINAL);/* Cans */
  MATCH_SCRIPT (YI);                 /* Yiii */
  MATCH_SCRIPT (TAGALOG);            /* Tglg */
  MATCH_SCRIPT (HANUNOO);            /* Hano */
  MATCH_SCRIPT (BUHID);              /* Buhd */
  MATCH_SCRIPT (TAGBANWA);           /* Tagb */

  /* Unicode-4.0 additions */
  MATCH_SCRIPT (BRAILLE);            /* Brai */
  MATCH_SCRIPT (CYPRIOT);            /* Cprt */
  MATCH_SCRIPT (LIMBU);              /* Limb */
  MATCH_SCRIPT (OSMANYA);            /* Osma */
  MATCH_SCRIPT (SHAVIAN);            /* Shaw */
  MATCH_SCRIPT (LINEAR_B);           /* Linb */
  MATCH_SCRIPT (TAI_LE);             /* Tale */
  MATCH_SCRIPT (UGARITIC);           /* Ugar */

  /* Unicode-4.1 additions */
  MATCH_SCRIPT (NEW_TAI_LUE);        /* Talu */
  MATCH_SCRIPT (BUGINESE);           /* Bugi */
  MATCH_SCRIPT (GLAGOLITIC);         /* Glag */
  MATCH_SCRIPT (TIFINAGH);           /* Tfng */
  MATCH_SCRIPT (SYLOTI_NAGRI);       /* Sylo */
  MATCH_SCRIPT (OLD_PERSIAN);        /* Xpeo */
  MATCH_SCRIPT (KHAROSHTHI);         /* Khar */

  /* Unicode-5.0 additions */
  MATCH_SCRIPT (UNKNOWN);            /* Zzzz */
  MATCH_SCRIPT (BALINESE);           /* Bali */
  MATCH_SCRIPT (CUNEIFORM);          /* Xsux */
  MATCH_SCRIPT (PHOENICIAN);         /* Phnx */
  MATCH_SCRIPT (PHAGS_PA);           /* Phag */
  MATCH_SCRIPT (NKO);                /* Nkoo */

  /* Unicode-5.1 additions */
  MATCH_SCRIPT (KAYAH_LI);           /* Kali */
  MATCH_SCRIPT (LEPCHA);             /* Lepc */
  MATCH_SCRIPT (REJANG);             /* Rjng */
  MATCH_SCRIPT (SUNDANESE);          /* Sund */
  MATCH_SCRIPT (SAURASHTRA);         /* Saur */
  MATCH_SCRIPT (CHAM);               /* Cham */
  MATCH_SCRIPT (OL_CHIKI);           /* Olck */
  MATCH_SCRIPT (VAI);                /* Vaii */
  MATCH_SCRIPT (CARIAN);             /* Cari */
  MATCH_SCRIPT (LYCIAN);             /* Lyci */
  MATCH_SCRIPT (LYDIAN);             /* Lydi */

  /* Unicode-5.2 additions */
  MATCH_SCRIPT (AVESTAN);                /* Avst */
#if CHECK_ICU_VERSION (4, 4)
  MATCH_SCRIPT (BAMUM);                  /* Bamu */
#endif
  MATCH_SCRIPT (EGYPTIAN_HIEROGLYPHS);   /* Egyp */
  MATCH_SCRIPT (IMPERIAL_ARAMAIC);       /* Armi */
  MATCH_SCRIPT (INSCRIPTIONAL_PAHLAVI);  /* Phli */
  MATCH_SCRIPT (INSCRIPTIONAL_PARTHIAN); /* Prti */
  MATCH_SCRIPT (JAVANESE);               /* Java */
  MATCH_SCRIPT (KAITHI);                 /* Kthi */
  MATCH_SCRIPT2(LANNA, TAI_THAM);        /* Lana */
#if CHECK_ICU_VERSION (4, 4)
  MATCH_SCRIPT (LISU);                   /* Lisu */
#endif
  MATCH_SCRIPT2(MEITEI_MAYEK, MEETEI_MAYEK);/* Mtei */
#if CHECK_ICU_VERSION (4, 4)
  MATCH_SCRIPT (OLD_SOUTH_ARABIAN);      /* Sarb */
#endif
  MATCH_SCRIPT2(ORKHON, OLD_TURKIC);     /* Orkh */
  MATCH_SCRIPT (SAMARITAN);              /* Samr */
  MATCH_SCRIPT (TAI_VIET);               /* Tavt */

  /* Unicode-6.0 additions */
  MATCH_SCRIPT (BATAK);                  /* Batk */
  MATCH_SCRIPT (BRAHMI);                 /* Brah */
  MATCH_SCRIPT2(MANDAEAN, MANDAIC);      /* Mand */

  }
  return HB_SCRIPT_UNKNOWN;
}

static hb_unicode_funcs_t icu_ufuncs = {
  HB_REFERENCE_COUNT_INVALID, /* ref_count */
  TRUE, /* immutable */
  {
    hb_icu_get_general_category,
    hb_icu_get_combining_class,
    hb_icu_get_mirroring,
    hb_icu_get_script,
    hb_icu_get_eastasian_width
  }
};

hb_unicode_funcs_t *
hb_icu_get_unicode_funcs (void)
{
  return &icu_ufuncs;
}


HB_END_DECLS
