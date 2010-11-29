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

#include "hb-private.h"
#include "hb-ot.h"

#include <string.h>

HB_BEGIN_DECLS


/*
 * Complete list at:
 * http://www.microsoft.com/typography/otspec/scripttags.htm
 */
static const hb_tag_t ot_scripts[][3] = {
  {HB_TAG('D','F','L','T')},	/* HB_SCRIPT_COMMON */
  {HB_TAG('D','F','L','T')},	/* HB_SCRIPT_INHERITED */
  {HB_TAG('a','r','a','b')},	/* HB_SCRIPT_ARABIC */
  {HB_TAG('a','r','m','n')},	/* HB_SCRIPT_ARMENIAN */
  {HB_TAG('b','n','g','2'), HB_TAG('b','e','n','g')},	/* HB_SCRIPT_BENGALI */
  {HB_TAG('b','o','p','o')},	/* HB_SCRIPT_BOPOMOFO */
  {HB_TAG('c','h','e','r')},	/* HB_SCRIPT_CHEROKEE */
  {HB_TAG('c','o','p','t')},	/* HB_SCRIPT_COPTIC */
  {HB_TAG('c','y','r','l')},	/* HB_SCRIPT_CYRILLIC */
  {HB_TAG('d','s','r','t')},	/* HB_SCRIPT_DESERET */
  {HB_TAG('d','e','v','2'), HB_TAG('d','e','v','a')},	/* HB_SCRIPT_DEVANAGARI */
  {HB_TAG('e','t','h','i')},	/* HB_SCRIPT_ETHIOPIC */
  {HB_TAG('g','e','o','r')},	/* HB_SCRIPT_GEORGIAN */
  {HB_TAG('g','o','t','h')},	/* HB_SCRIPT_GOTHIC */
  {HB_TAG('g','r','e','k')},	/* HB_SCRIPT_GREEK */
  {HB_TAG('g','j','r','2'), HB_TAG('g','u','j','r')},	/* HB_SCRIPT_GUJARATI */
  {HB_TAG('g','u','r','2'), HB_TAG('g','u','r','u')},	/* HB_SCRIPT_GURMUKHI */
  {HB_TAG('h','a','n','i')},	/* HB_SCRIPT_HAN */
  {HB_TAG('h','a','n','g')},	/* HB_SCRIPT_HANGUL */
  {HB_TAG('h','e','b','r')},	/* HB_SCRIPT_HEBREW */
  {HB_TAG('k','a','n','a')},	/* HB_SCRIPT_HIRAGANA */
  {HB_TAG('k','n','d','2'), HB_TAG('k','n','d','a')},	/* HB_SCRIPT_KANNADA */
  {HB_TAG('k','a','n','a')},	/* HB_SCRIPT_KATAKANA */
  {HB_TAG('k','h','m','r')},	/* HB_SCRIPT_KHMER */
  {HB_TAG('l','a','o',' ')},	/* HB_SCRIPT_LAO */
  {HB_TAG('l','a','t','n')},	/* HB_SCRIPT_LATIN */
  {HB_TAG('m','l','m','2'), HB_TAG('m','l','y','m')},	/* HB_SCRIPT_MALAYALAM */
  {HB_TAG('m','o','n','g')},	/* HB_SCRIPT_MONGOLIAN */
  {HB_TAG('m','y','m','r')},	/* HB_SCRIPT_MYANMAR */
  {HB_TAG('o','g','a','m')},	/* HB_SCRIPT_OGHAM */
  {HB_TAG('i','t','a','l')},	/* HB_SCRIPT_OLD_ITALIC */
  {HB_TAG('o','r','y','2'), HB_TAG('o','r','y','a')},	/* HB_SCRIPT_ORIYA */
  {HB_TAG('r','u','n','r')},	/* HB_SCRIPT_RUNIC */
  {HB_TAG('s','i','n','h')},	/* HB_SCRIPT_SINHALA */
  {HB_TAG('s','y','r','c')},	/* HB_SCRIPT_SYRIAC */
  {HB_TAG('t','m','l','2'), HB_TAG('t','a','m','l')},	/* HB_SCRIPT_TAMIL */
  {HB_TAG('t','e','l','2'), HB_TAG('t','e','l','u')},	/* HB_SCRIPT_TELUGU */
  {HB_TAG('t','h','a','a')},	/* HB_SCRIPT_THAANA */
  {HB_TAG('t','h','a','i')},	/* HB_SCRIPT_THAI */
  {HB_TAG('t','i','b','t')},	/* HB_SCRIPT_TIBETAN */
  {HB_TAG('c','a','n','s')},	/* HB_SCRIPT_CANADIAN_ABORIGINAL */
  {HB_TAG('y','i',' ',' ')},	/* HB_SCRIPT_YI */
  {HB_TAG('t','g','l','g')},	/* HB_SCRIPT_TAGALOG */
  {HB_TAG('h','a','n','o')},	/* HB_SCRIPT_HANUNOO */
  {HB_TAG('b','u','h','d')},	/* HB_SCRIPT_BUHID */
  {HB_TAG('t','a','g','b')},	/* HB_SCRIPT_TAGBANWA */

  /* Unicode-4.0 additions */
  {HB_TAG('b','r','a','i')},	/* HB_SCRIPT_BRAILLE */
  {HB_TAG('c','p','r','t')},	/* HB_SCRIPT_CYPRIOT */
  {HB_TAG('l','i','m','b')},	/* HB_SCRIPT_LIMBU */
  {HB_TAG('o','s','m','a')},	/* HB_SCRIPT_OSMANYA */
  {HB_TAG('s','h','a','w')},	/* HB_SCRIPT_SHAVIAN */
  {HB_TAG('l','i','n','b')},	/* HB_SCRIPT_LINEAR_B */
  {HB_TAG('t','a','l','e')},	/* HB_SCRIPT_TAI_LE */
  {HB_TAG('u','g','a','r')},	/* HB_SCRIPT_UGARITIC */

  /* Unicode-4.1 additions */
  {HB_TAG('t','a','l','u')},	/* HB_SCRIPT_NEW_TAI_LUE */
  {HB_TAG('b','u','g','i')},	/* HB_SCRIPT_BUGINESE */
  {HB_TAG('g','l','a','g')},	/* HB_SCRIPT_GLAGOLITIC */
  {HB_TAG('t','f','n','g')},	/* HB_SCRIPT_TIFINAGH */
  {HB_TAG('s','y','l','o')},	/* HB_SCRIPT_SYLOTI_NAGRI */
  {HB_TAG('x','p','e','o')},	/* HB_SCRIPT_OLD_PERSIAN */
  {HB_TAG('k','h','a','r')},	/* HB_SCRIPT_KHAROSHTHI */

  /* Unicode-5.0 additions */
  {HB_TAG('D','F','L','T')},	/* HB_SCRIPT_UNKNOWN */
  {HB_TAG('b','a','l','i')},	/* HB_SCRIPT_BALINESE */
  {HB_TAG('x','s','u','x')},	/* HB_SCRIPT_CUNEIFORM */
  {HB_TAG('p','h','n','x')},	/* HB_SCRIPT_PHOENICIAN */
  {HB_TAG('p','h','a','g')},	/* HB_SCRIPT_PHAGS_PA */
  {HB_TAG('n','k','o',' ')},	/* HB_SCRIPT_NKO */

  /* Unicode-5.1 additions */
  {HB_TAG('k','a','l','i')},	/* HB_SCRIPT_KAYAH_LI */
  {HB_TAG('l','e','p','c')},	/* HB_SCRIPT_LEPCHA */
  {HB_TAG('r','j','n','g')},	/* HB_SCRIPT_REJANG */
  {HB_TAG('s','u','n','d')},	/* HB_SCRIPT_SUNDANESE */
  {HB_TAG('s','a','u','r')},	/* HB_SCRIPT_SAURASHTRA */
  {HB_TAG('c','h','a','m')},	/* HB_SCRIPT_CHAM */
  {HB_TAG('o','l','c','k')},	/* HB_SCRIPT_OL_CHIKI */
  {HB_TAG('v','a','i',' ')},	/* HB_SCRIPT_VAI */
  {HB_TAG('c','a','r','i')},	/* HB_SCRIPT_CARIAN */
  {HB_TAG('l','y','c','i')},	/* HB_SCRIPT_LYCIAN */
  {HB_TAG('l','y','d','i')},	/* HB_SCRIPT_LYDIAN */

  /* Unicode-5.2 additions */
  {HB_TAG('a','v','s','t')},	/* HB_SCRIPT_AVESTAN */
  {HB_TAG('b','a','m','u')},	/* HB_SCRIPT_BAMUM */
  {HB_TAG('e','g','y','p')},	/* HB_SCRIPT_EGYPTIAN_HIEROGLYPHS */
  {HB_TAG('a','r','m','i')},	/* HB_SCRIPT_IMPERIAL_ARAMAIC */
  {HB_TAG('p','h','l','i')},	/* HB_SCRIPT_INSCRIPTIONAL_PAHLAVI */
  {HB_TAG('p','r','t','i')},	/* HB_SCRIPT_INSCRIPTIONAL_PARTHIAN */
  {HB_TAG('j','a','v','a')},	/* HB_SCRIPT_JAVANESE */
  {HB_TAG('k','t','h','i')},	/* HB_SCRIPT_KAITHI */
  {HB_TAG('l','i','s','u')},	/* HB_SCRIPT_LISU */
  {HB_TAG('m','y','e','i')},	/* HB_SCRIPT_MEETEI_MAYEK */
  {HB_TAG('s','a','r','b')},	/* HB_SCRIPT_OLD_SOUTH_ARABIAN */
  {HB_TAG('o','r','k','h')},	/* HB_SCRIPT_OLD_TURKIC */
  {HB_TAG('s','a','m','r')},	/* HB_SCRIPT_SAMARITAN */
  {HB_TAG('l','a','n','a')},	/* HB_SCRIPT_TAI_THAM */
  {HB_TAG('t','a','v','t')},	/* HB_SCRIPT_TAI_VIET */

  /* Unicode-6.0 additions */
  {HB_TAG('b','a','t','k')},	/* HB_SCRIPT_BATAK */
  {HB_TAG('b','r','a','h')},	/* HB_SCRIPT_BRAHMI */
  {HB_TAG('m','a','n','d')} 	/* HB_SCRIPT_MANDAIC */
};

const hb_tag_t *
hb_ot_tags_from_script (hb_script_t script)
{
  static const hb_tag_t def_tag[] = {HB_OT_TAG_DEFAULT_SCRIPT, HB_TAG_NONE};

  if (unlikely ((unsigned int) script >= ARRAY_LENGTH (ot_scripts)))
    return def_tag;

  return ot_scripts[script];
}

hb_script_t
hb_ot_tag_to_script (hb_tag_t tag)
{
  int i;

  for (i = 0; i < ARRAY_LENGTH (ot_scripts); i++) {
    const hb_tag_t *p;
    for (p = ot_scripts[i]; *p; p++)
      if (tag == *p)
        return i;
  }

  return HB_SCRIPT_UNKNOWN;
}

typedef struct {
  char language[6];
  hb_tag_t tag;
} LangTag;

/*
 * Complete list at:
 * http://www.microsoft.com/typography/otspec/languagetags.htm
 *
 * Generated by intersecting the OpenType language tag list from
 * Draft OpenType 1.5 spec, with with the ISO 639-3 codes from
 * 2008/08/04, matching on name, and finally adjusted manually.
 *
 * Many items still missing.  Those are commented out at the end.
 * Keep sorted for bsearch.
 */
static const LangTag ot_languages[] = {
  {"aa",	HB_TAG('A','F','R',' ')},	/* Afar */
  {"ab",	HB_TAG('A','B','K',' ')},	/* Abkhazian */
  {"abq",	HB_TAG('A','B','A',' ')},	/* Abaza */
  {"ady",	HB_TAG('A','D','Y',' ')},	/* Adyghe */
  {"af",	HB_TAG('A','F','K',' ')},	/* Afrikaans */
  {"aiw",	HB_TAG('A','R','I',' ')},	/* Aari */
  {"am",	HB_TAG('A','M','H',' ')},	/* Amharic */
  {"ar",	HB_TAG('A','R','A',' ')},	/* Arabic */
  {"arn",	HB_TAG('M','A','P',' ')},	/* Mapudungun */
  {"as",	HB_TAG('A','S','M',' ')},	/* Assamese */
  {"av",	HB_TAG('A','V','R',' ')},	/* Avaric */
  {"awa",	HB_TAG('A','W','A',' ')},	/* Awadhi */
  {"ay",	HB_TAG('A','Y','M',' ')},	/* Aymara */
  {"az",	HB_TAG('A','Z','E',' ')},	/* Azerbaijani */
  {"ba",	HB_TAG('B','S','H',' ')},	/* Bashkir */
  {"bal",	HB_TAG('B','L','I',' ')},	/* Baluchi */
  {"bcq",	HB_TAG('B','C','H',' ')},	/* Bench */
  {"bem",	HB_TAG('B','E','M',' ')},	/* Bemba (Zambia) */
  {"bfq",	HB_TAG('B','A','D',' ')},	/* Badaga */
  {"bft",	HB_TAG('B','L','T',' ')},	/* Balti */
  {"bg",	HB_TAG('B','G','R',' ')},	/* Bulgarian */
  {"bhb",	HB_TAG('B','H','I',' ')},	/* Bhili */
  {"bho",	HB_TAG('B','H','O',' ')},	/* Bhojpuri */
  {"bik",	HB_TAG('B','I','K',' ')},	/* Bikol */
  {"bin",	HB_TAG('E','D','O',' ')},	/* Bini */
  {"bm",	HB_TAG('B','M','B',' ')},	/* Bambara */
  {"bn",	HB_TAG('B','E','N',' ')},	/* Bengali */
  {"bo",	HB_TAG('T','I','B',' ')},	/* Tibetan */
  {"br",	HB_TAG('B','R','E',' ')},	/* Breton */
  {"brh",	HB_TAG('B','R','H',' ')},	/* Brahui */
  {"bs",	HB_TAG('B','O','S',' ')},	/* Bosnian */
  {"btb",	HB_TAG('B','T','I',' ')},	/* Beti (Cameroon) */
  {"ca",	HB_TAG('C','A','T',' ')},	/* Catalan */
  {"ce",	HB_TAG('C','H','E',' ')},	/* Chechen */
  {"ceb",	HB_TAG('C','E','B',' ')},	/* Cebuano */
  {"chp",	HB_TAG('C','H','P',' ')},	/* Chipewyan */
  {"chr",	HB_TAG('C','H','R',' ')},	/* Cherokee */
  {"cop",	HB_TAG('C','O','P',' ')},	/* Coptic */
  {"cr",	HB_TAG('C','R','E',' ')},	/* Cree */
  {"crh",	HB_TAG('C','R','T',' ')},	/* Crimean Tatar */
  {"crm",	HB_TAG('M','C','R',' ')},	/* Moose Cree */
  {"crx",	HB_TAG('C','R','R',' ')},	/* Carrier */
  {"cs",	HB_TAG('C','S','Y',' ')},	/* Czech */
  {"cu",	HB_TAG('C','S','L',' ')},	/* Church Slavic */
  {"cv",	HB_TAG('C','H','U',' ')},	/* Chuvash */
  {"cwd",	HB_TAG('D','C','R',' ')},	/* Woods Cree */
  {"cy",	HB_TAG('W','E','L',' ')},	/* Welsh */
  {"da",	HB_TAG('D','A','N',' ')},	/* Danish */
  {"dap",	HB_TAG('N','I','S',' ')},	/* Nisi (India) */
  {"dar",	HB_TAG('D','A','R',' ')},	/* Dargwa */
  {"de",	HB_TAG('D','E','U',' ')},	/* German */
  {"din",	HB_TAG('D','N','K',' ')},	/* Dinka */
  {"dng",	HB_TAG('D','U','N',' ')},	/* Dungan */
  {"doi",	HB_TAG('D','G','R',' ')},	/* Dogri */
  {"dsb",	HB_TAG('L','S','B',' ')},	/* Lower Sorbian */
  {"dv",	HB_TAG('D','I','V',' ')},	/* Dhivehi */
  {"dz",	HB_TAG('D','Z','N',' ')},	/* Dzongkha */
  {"ee",	HB_TAG('E','W','E',' ')},	/* Ewe */
  {"efi",	HB_TAG('E','F','I',' ')},	/* Efik */
  {"el",	HB_TAG('E','L','L',' ')},	/* Modern Greek (1453-) */
  {"en",	HB_TAG('E','N','G',' ')},	/* English */
  {"eo",	HB_TAG('N','T','O',' ')},	/* Esperanto */
  {"eot",	HB_TAG('B','T','I',' ')},	/* Beti (Côte d'Ivoire) */
  {"es",	HB_TAG('E','S','P',' ')},	/* Spanish */
  {"et",	HB_TAG('E','T','I',' ')},	/* Estonian */
  {"eu",	HB_TAG('E','U','Q',' ')},	/* Basque */
  {"eve",	HB_TAG('E','V','N',' ')},	/* Even */
  {"evn",	HB_TAG('E','V','K',' ')},	/* Evenki */
  {"fa",	HB_TAG('F','A','R',' ')},	/* Persian */
  {"ff",	HB_TAG('F','U','L',' ')},	/* Fulah */
  {"fi",	HB_TAG('F','I','N',' ')},	/* Finnish */
  {"fil",	HB_TAG('P','I','L',' ')},	/* Filipino */
  {"fj",	HB_TAG('F','J','I',' ')},	/* Fijian */
  {"fo",	HB_TAG('F','O','S',' ')},	/* Faroese */
  {"fon",	HB_TAG('F','O','N',' ')},	/* Fon */
  {"fr",	HB_TAG('F','R','A',' ')},	/* French */
  {"fur",	HB_TAG('F','R','L',' ')},	/* Friulian */
  {"fy",	HB_TAG('F','R','I',' ')},	/* Western Frisian */
  {"ga",	HB_TAG('I','R','I',' ')},	/* Irish */
  {"gaa",	HB_TAG('G','A','D',' ')},	/* Ga */
  {"gag",	HB_TAG('G','A','G',' ')},	/* Gagauz */
  {"gbm",	HB_TAG('G','A','W',' ')},	/* Garhwali */
  {"gd",	HB_TAG('G','A','E',' ')},	/* Scottish Gaelic */
  {"gl",	HB_TAG('G','A','L',' ')},	/* Galician */
  {"gld",	HB_TAG('N','A','N',' ')},	/* Nanai */
  {"gn",	HB_TAG('G','U','A',' ')},	/* Guarani */
  {"gon",	HB_TAG('G','O','N',' ')},	/* Gondi */
  {"grt",	HB_TAG('G','R','O',' ')},	/* Garo */
  {"gu",	HB_TAG('G','U','J',' ')},	/* Gujarati */
  {"guk",	HB_TAG('G','M','Z',' ')},	/* Gumuz */
  {"gv",	HB_TAG('M','N','X',' ')},	/* Manx Gaelic */
  {"ha",	HB_TAG('H','A','U',' ')},	/* Hausa */
  {"har",	HB_TAG('H','R','I',' ')},	/* Harari */
  {"he",	HB_TAG('I','W','R',' ')},	/* Hebrew */
  {"hi",	HB_TAG('H','I','N',' ')},	/* Hindi */
  {"hil",	HB_TAG('H','I','L',' ')},	/* Hiligaynon */
  {"hoc",	HB_TAG('H','O',' ',' ')},	/* Ho */
  {"hr",	HB_TAG('H','R','V',' ')},	/* Croatian */
  {"hsb",	HB_TAG('U','S','B',' ')},	/* Upper Sorbian */
  {"ht",	HB_TAG('H','A','I',' ')},	/* Haitian */
  {"hu",	HB_TAG('H','U','N',' ')},	/* Hungarian */
  {"hy",	HB_TAG('H','Y','E',' ')},	/* Armenian */
  {"id",	HB_TAG('I','N','D',' ')},	/* Indonesian */
  {"ig",	HB_TAG('I','B','O',' ')},	/* Igbo */
  {"igb",	HB_TAG('E','B','I',' ')},	/* Ebira */
  {"inh",	HB_TAG('I','N','G',' ')},	/* Ingush */
  {"is",	HB_TAG('I','S','L',' ')},	/* Icelandic */
  {"it",	HB_TAG('I','T','A',' ')},	/* Italian */
  {"iu",	HB_TAG('I','N','U',' ')},	/* Inuktitut */
  {"ja",	HB_TAG('J','A','N',' ')},	/* Japanese */
  {"jv",	HB_TAG('J','A','V',' ')},	/* Javanese */
  {"ka",	HB_TAG('K','A','T',' ')},	/* Georgian */
  {"kam",	HB_TAG('K','M','B',' ')},	/* Kamba (Kenya) */
  {"kbd",	HB_TAG('K','A','B',' ')},	/* Kabardian */
  {"kdr",	HB_TAG('K','R','M',' ')},	/* Karaim */
  {"kdt",	HB_TAG('K','U','Y',' ')},	/* Kuy */
  {"kfr",	HB_TAG('K','A','C',' ')},	/* Kachchi */
  {"kfy",	HB_TAG('K','M','N',' ')},	/* Kumaoni */
  {"kha",	HB_TAG('K','S','I',' ')},	/* Khasi */
  {"khw",	HB_TAG('K','H','W',' ')},	/* Khowar */
  {"ki",	HB_TAG('K','I','K',' ')},	/* Kikuyu */
  {"kk",	HB_TAG('K','A','Z',' ')},	/* Kazakh */
  {"kl",	HB_TAG('G','R','N',' ')},	/* Kalaallisut */
  {"kln",	HB_TAG('K','A','L',' ')},	/* Kalenjin */
  {"km",	HB_TAG('K','H','M',' ')},	/* Central Khmer */
  {"kmw",	HB_TAG('K','M','O',' ')},	/* Komo (Democratic Republic of Congo) */
  {"kn",	HB_TAG('K','A','N',' ')},	/* Kannada */
  {"ko",	HB_TAG('K','O','R',' ')},	/* Korean */
  {"koi",	HB_TAG('K','O','P',' ')},	/* Komi-Permyak */
  {"kok",	HB_TAG('K','O','K',' ')},	/* Konkani */
  {"kpe",	HB_TAG('K','P','L',' ')},	/* Kpelle */
  {"kpv",	HB_TAG('K','O','Z',' ')},	/* Komi-Zyrian */
  {"kpy",	HB_TAG('K','Y','K',' ')},	/* Koryak */
  {"kqy",	HB_TAG('K','R','T',' ')},	/* Koorete */
  {"kr",	HB_TAG('K','N','R',' ')},	/* Kanuri */
  {"kri",	HB_TAG('K','R','I',' ')},	/* Krio */
  {"krl",	HB_TAG('K','R','L',' ')},	/* Karelian */
  {"kru",	HB_TAG('K','U','U',' ')},	/* Kurukh */
  {"ks",	HB_TAG('K','S','H',' ')},	/* Kashmiri */
  {"ku",	HB_TAG('K','U','R',' ')},	/* Kurdish */
  {"kum",	HB_TAG('K','U','M',' ')},	/* Kumyk */
  {"kvd",	HB_TAG('K','U','I',' ')},	/* Kui (Indonesia) */
  {"kxu",	HB_TAG('K','U','I',' ')},	/* Kui (India) */
  {"ky",	HB_TAG('K','I','R',' ')},	/* Kirghiz */
  {"la",	HB_TAG('L','A','T',' ')},	/* Latin */
  {"lad",	HB_TAG('J','U','D',' ')},	/* Ladino */
  {"lb",	HB_TAG('L','T','Z',' ')},	/* Luxembourgish */
  {"lbe",	HB_TAG('L','A','K',' ')},	/* Lak */
  {"lbj",	HB_TAG('L','D','K',' ')},	/* Ladakhi */
  {"lif",	HB_TAG('L','M','B',' ')},	/* Limbu */
  {"lld",	HB_TAG('L','A','D',' ')},	/* Ladin */
  {"ln",	HB_TAG('L','I','N',' ')},	/* Lingala */
  {"lo",	HB_TAG('L','A','O',' ')},	/* Lao */
  {"lt",	HB_TAG('L','T','H',' ')},	/* Lithuanian */
  {"luo",	HB_TAG('L','U','O',' ')},	/* Luo (Kenya and Tanzania) */
  {"luw",	HB_TAG('L','U','O',' ')},	/* Luo (Cameroon) */
  {"lv",	HB_TAG('L','V','I',' ')},	/* Latvian */
  {"lzz",	HB_TAG('L','A','Z',' ')},	/* Laz */
  {"mai",	HB_TAG('M','T','H',' ')},	/* Maithili */
  {"mdc",	HB_TAG('M','L','E',' ')},	/* Male (Papua New Guinea) */
  {"mdf",	HB_TAG('M','O','K',' ')},	/* Moksha */
  {"mdy",	HB_TAG('M','L','E',' ')},	/* Male (Ethiopia) */
  {"men",	HB_TAG('M','D','E',' ')},	/* Mende (Sierra Leone) */
  {"mg",	HB_TAG('M','L','G',' ')},	/* Malagasy */
  {"mi",	HB_TAG('M','R','I',' ')},	/* Maori */
  {"mk",	HB_TAG('M','K','D',' ')},	/* Macedonian */
  {"ml",	HB_TAG('M','L','R',' ')},	/* Malayalam */
  {"mn",	HB_TAG('M','N','G',' ')},	/* Mongolian */
  {"mnc",	HB_TAG('M','C','H',' ')},	/* Manchu */
  {"mni",	HB_TAG('M','N','I',' ')},	/* Manipuri */
  {"mnk",	HB_TAG('M','N','D',' ')},	/* Mandinka */
  {"mns",	HB_TAG('M','A','N',' ')},	/* Mansi */
  {"mnw",	HB_TAG('M','O','N',' ')},	/* Mon */
  {"mo",	HB_TAG('M','O','L',' ')},	/* Moldavian */
  {"moh",	HB_TAG('M','O','H',' ')},	/* Mohawk */
  {"mpe",	HB_TAG('M','A','J',' ')},	/* Majang */
  {"mr",	HB_TAG('M','A','R',' ')},	/* Marathi */
  {"ms",	HB_TAG('M','L','Y',' ')},	/* Malay */
  {"mt",	HB_TAG('M','T','S',' ')},	/* Maltese */
  {"mwr",	HB_TAG('M','A','W',' ')},	/* Marwari */
  {"my",	HB_TAG('B','R','M',' ')},	/* Burmese */
  {"mym",	HB_TAG('M','E','N',' ')},	/* Me'en */
  {"myv",	HB_TAG('E','R','Z',' ')},	/* Erzya */
  {"nb",	HB_TAG('N','O','R',' ')},	/* Norwegian Bokmål */
  {"nco",	HB_TAG('S','I','B',' ')},	/* Sibe */
  {"ne",	HB_TAG('N','E','P',' ')},	/* Nepali */
  {"new",	HB_TAG('N','E','W',' ')},	/* Newari */
  {"ng",	HB_TAG('N','D','G',' ')},	/* Ndonga */
  {"ngl",	HB_TAG('L','M','W',' ')},	/* Lomwe */
  {"niu",	HB_TAG('N','I','U',' ')},	/* Niuean */
  {"niv",	HB_TAG('G','I','L',' ')},	/* Gilyak */
  {"nl",	HB_TAG('N','L','D',' ')},	/* Dutch */
  {"nn",	HB_TAG('N','Y','N',' ')},	/* Norwegian Nynorsk */
  {"no",	HB_TAG('N','O','R',' ')},	/* Norwegian (deprecated) */
  {"nog",	HB_TAG('N','O','G',' ')},	/* Nogai */
  {"nqo",	HB_TAG('N','K','O',' ')},	/* N'Ko */
  {"nsk",	HB_TAG('N','A','S',' ')},	/* Naskapi */
  {"ny",	HB_TAG('C','H','I',' ')},	/* Nyanja */
  {"oc",	HB_TAG('O','C','I',' ')},	/* Occitan (post 1500) */
  {"oj",	HB_TAG('O','J','B',' ')},	/* Ojibwa */
  {"om",	HB_TAG('O','R','O',' ')},	/* Oromo */
  {"or",	HB_TAG('O','R','I',' ')},	/* Oriya */
  {"os",	HB_TAG('O','S','S',' ')},	/* Ossetian */
  {"pa",	HB_TAG('P','A','N',' ')},	/* Panjabi */
  {"pi",	HB_TAG('P','A','L',' ')},	/* Pali */
  {"pl",	HB_TAG('P','L','K',' ')},	/* Polish */
  {"plp",	HB_TAG('P','A','P',' ')},	/* Palpa */
  {"prs",	HB_TAG('D','R','I',' ')},	/* Dari */
  {"ps",	HB_TAG('P','A','S',' ')},	/* Pushto */
  {"pt",	HB_TAG('P','T','G',' ')},	/* Portuguese */
  {"raj",	HB_TAG('R','A','J',' ')},	/* Rajasthani */
  {"ria",	HB_TAG('R','I','A',' ')},	/* Riang (India) */
  {"ril",	HB_TAG('R','I','A',' ')},	/* Riang (Myanmar) */
  {"ro",	HB_TAG('R','O','M',' ')},	/* Romanian */
  {"rom",	HB_TAG('R','O','Y',' ')},	/* Romany */
  {"ru",	HB_TAG('R','U','S',' ')},	/* Russian */
  {"rue",	HB_TAG('R','S','Y',' ')},	/* Rusyn */
  {"sa",	HB_TAG('S','A','N',' ')},	/* Sanskrit */
  {"sah",	HB_TAG('Y','A','K',' ')},	/* Yakut */
  {"sat",	HB_TAG('S','A','T',' ')},	/* Santali */
  {"sck",	HB_TAG('S','A','D',' ')},	/* Sadri */
  {"sd",	HB_TAG('S','N','D',' ')},	/* Sindhi */
  {"se",	HB_TAG('N','S','M',' ')},	/* Northern Sami */
  {"seh",	HB_TAG('S','N','A',' ')},	/* Sena */
  {"sel",	HB_TAG('S','E','L',' ')},	/* Selkup */
  {"sg",	HB_TAG('S','G','O',' ')},	/* Sango */
  {"shn",	HB_TAG('S','H','N',' ')},	/* Shan */
  {"si",	HB_TAG('S','N','H',' ')},	/* Sinhala */
  {"sid",	HB_TAG('S','I','D',' ')},	/* Sidamo */
  {"sjd",	HB_TAG('K','S','M',' ')},	/* Kildin Sami */
  {"sk",	HB_TAG('S','K','Y',' ')},	/* Slovak */
  {"skr",	HB_TAG('S','R','K',' ')},	/* Seraiki */
  {"sl",	HB_TAG('S','L','V',' ')},	/* Slovenian */
  {"sm",	HB_TAG('S','M','O',' ')},	/* Samoan */
  {"sma",	HB_TAG('S','S','M',' ')},	/* Southern Sami */
  {"smj",	HB_TAG('L','S','M',' ')},	/* Lule Sami */
  {"smn",	HB_TAG('I','S','M',' ')},	/* Inari Sami */
  {"sms",	HB_TAG('S','K','S',' ')},	/* Skolt Sami */
  {"snk",	HB_TAG('S','N','K',' ')},	/* Soninke */
  {"so",	HB_TAG('S','M','L',' ')},	/* Somali */
  {"sq",	HB_TAG('S','Q','I',' ')},	/* Albanian */
  {"sr",	HB_TAG('S','R','B',' ')},	/* Serbian */
  {"srr",	HB_TAG('S','R','R',' ')},	/* Serer */
  {"suq",	HB_TAG('S','U','R',' ')},	/* Suri */
  {"sv",	HB_TAG('S','V','E',' ')},	/* Swedish */
  {"sva",	HB_TAG('S','V','A',' ')},	/* Svan */
  {"sw",	HB_TAG('S','W','K',' ')},	/* Swahili */
  {"swb",	HB_TAG('C','M','R',' ')},	/* Comorian */
  {"syr",	HB_TAG('S','Y','R',' ')},	/* Syriac */
  {"ta",	HB_TAG('T','A','M',' ')},	/* Tamil */
  {"tcy",	HB_TAG('T','U','L',' ')},	/* Tulu */
  {"te",	HB_TAG('T','E','L',' ')},	/* Telugu */
  {"tg",	HB_TAG('T','A','J',' ')},	/* Tajik */
  {"th",	HB_TAG('T','H','A',' ')},	/* Thai */
  {"ti",	HB_TAG('T','G','Y',' ')},	/* Tigrinya */
  {"tig",	HB_TAG('T','G','R',' ')},	/* Tigre */
  {"tk",	HB_TAG('T','K','M',' ')},	/* Turkmen */
  {"tn",	HB_TAG('T','N','A',' ')},	/* Tswana */
  {"tnz",	HB_TAG('T','N','G',' ')},	/* Tonga (Thailand) */
  {"to",	HB_TAG('T','N','G',' ')},	/* Tonga (Tonga Islands) */
  {"tog",	HB_TAG('T','N','G',' ')},	/* Tonga (Nyasa) */
  {"toi",	HB_TAG('T','N','G',' ')},	/* Tonga (Zambia) */
  {"tr",	HB_TAG('T','R','K',' ')},	/* Turkish */
  {"ts",	HB_TAG('T','S','G',' ')},	/* Tsonga */
  {"tt",	HB_TAG('T','A','T',' ')},	/* Tatar */
  {"tw",	HB_TAG('T','W','I',' ')},	/* Twi */
  {"ty",	HB_TAG('T','H','T',' ')},	/* Tahitian */
  {"udm",	HB_TAG('U','D','M',' ')},	/* Udmurt */
  {"ug",	HB_TAG('U','Y','G',' ')},	/* Uighur */
  {"uk",	HB_TAG('U','K','R',' ')},	/* Ukrainian */
  {"unr",	HB_TAG('M','U','N',' ')},	/* Mundari */
  {"ur",	HB_TAG('U','R','D',' ')},	/* Urdu */
  {"uz",	HB_TAG('U','Z','B',' ')},	/* Uzbek */
  {"ve",	HB_TAG('V','E','N',' ')},	/* Venda */
  {"vi",	HB_TAG('V','I','T',' ')},	/* Vietnamese */
  {"wbm",	HB_TAG('W','A',' ',' ')},	/* Wa */
  {"wbr",	HB_TAG('W','A','G',' ')},	/* Wagdi */
  {"wo",	HB_TAG('W','L','F',' ')},	/* Wolof */
  {"xal",	HB_TAG('K','L','M',' ')},	/* Kalmyk */
  {"xh",	HB_TAG('X','H','S',' ')},	/* Xhosa */
  {"xom",	HB_TAG('K','M','O',' ')},	/* Komo (Sudan) */
  {"xsl",	HB_TAG('S','S','L',' ')},	/* South Slavey */
  {"yi",	HB_TAG('J','I','I',' ')},	/* Yiddish */
  {"yo",	HB_TAG('Y','B','A',' ')},	/* Yoruba */
  {"yso",	HB_TAG('N','I','S',' ')},	/* Nisi (China) */
  {"zh-cn",	HB_TAG('Z','H','S',' ')},	/* Chinese (China) */
  {"zh-hk",	HB_TAG('Z','H','H',' ')},	/* Chinese (Hong Kong) */
  {"zh-mo",	HB_TAG('Z','H','T',' ')},	/* Chinese (Macao) */
  {"zh-sg",	HB_TAG('Z','H','S',' ')},	/* Chinese (Singapore) */
  {"zh-tw",	HB_TAG('Z','H','T',' ')},	/* Chinese (Taiwan) */
  {"zne",	HB_TAG('Z','N','D',' ')},	/* Zande */
  {"zu",	HB_TAG('Z','U','L',' ')} 	/* Zulu */

  /* I couldn't find the language id for these */

/*{"??",	HB_TAG('A','G','W',' ')},*/	/* Agaw */
/*{"??",	HB_TAG('A','L','S',' ')},*/	/* Alsatian */
/*{"??",	HB_TAG('A','L','T',' ')},*/	/* Altai */
/*{"??",	HB_TAG('A','R','K',' ')},*/	/* Arakanese */
/*{"??",	HB_TAG('A','T','H',' ')},*/	/* Athapaskan */
/*{"??",	HB_TAG('B','A','G',' ')},*/	/* Baghelkhandi */
/*{"??",	HB_TAG('B','A','L',' ')},*/	/* Balkar */
/*{"??",	HB_TAG('B','A','U',' ')},*/	/* Baule */
/*{"??",	HB_TAG('B','B','R',' ')},*/	/* Berber */
/*{"??",	HB_TAG('B','C','R',' ')},*/	/* Bible Cree */
/*{"??",	HB_TAG('B','E','L',' ')},*/	/* Belarussian */
/*{"??",	HB_TAG('B','I','L',' ')},*/	/* Bilen */
/*{"??",	HB_TAG('B','K','F',' ')},*/	/* Blackfoot */
/*{"??",	HB_TAG('B','L','N',' ')},*/	/* Balante */
/*{"??",	HB_TAG('B','M','L',' ')},*/	/* Bamileke */
/*{"??",	HB_TAG('B','R','I',' ')},*/	/* Braj Bhasha */
/*{"??",	HB_TAG('C','H','G',' ')},*/	/* Chaha Gurage */
/*{"??",	HB_TAG('C','H','H',' ')},*/	/* Chattisgarhi */
/*{"??",	HB_TAG('C','H','K',' ')},*/	/* Chukchi */
/*{"??",	HB_TAG('D','J','R',' ')},*/	/* Djerma */
/*{"??",	HB_TAG('D','N','G',' ')},*/	/* Dangme */
/*{"??",	HB_TAG('E','C','R',' ')},*/	/* Eastern Cree */
/*{"??",	HB_TAG('F','A','N',' ')},*/	/* French Antillean */
/*{"??",	HB_TAG('F','L','E',' ')},*/	/* Flemish */
/*{"??",	HB_TAG('F','N','E',' ')},*/	/* Forest Nenets */
/*{"??",	HB_TAG('F','T','A',' ')},*/	/* Futa */
/*{"??",	HB_TAG('G','A','R',' ')},*/	/* Garshuni */
/*{"??",	HB_TAG('G','E','Z',' ')},*/	/* Ge'ez */
/*{"??",	HB_TAG('H','A','L',' ')},*/	/* Halam */
/*{"??",	HB_TAG('H','A','R',' ')},*/	/* Harauti */
/*{"??",	HB_TAG('H','A','W',' ')},*/	/* Hawaiin */
/*{"??",	HB_TAG('H','B','N',' ')},*/	/* Hammer-Banna */
/*{"??",	HB_TAG('H','M','A',' ')},*/	/* High Mari */
/*{"??",	HB_TAG('H','N','D',' ')},*/	/* Hindko */
/*{"??",	HB_TAG('I','J','O',' ')},*/	/* Ijo */
/*{"??",	HB_TAG('I','L','O',' ')},*/	/* Ilokano */
/*{"??",	HB_TAG('I','R','T',' ')},*/	/* Irish Traditional */
/*{"??",	HB_TAG('J','U','L',' ')},*/	/* Jula */
/*{"??",	HB_TAG('K','A','R',' ')},*/	/* Karachay */
/*{"??",	HB_TAG('K','E','B',' ')},*/	/* Kebena */
/*{"??",	HB_TAG('K','G','E',' ')},*/	/* Khutsuri Georgian */
/*{"??",	HB_TAG('K','H','A',' ')},*/	/* Khakass */
/*{"??",	HB_TAG('K','H','K',' ')},*/	/* Khanty-Kazim */
/*{"??",	HB_TAG('K','H','S',' ')},*/	/* Khanty-Shurishkar */
/*{"??",	HB_TAG('K','H','V',' ')},*/	/* Khanty-Vakhi */
/*{"??",	HB_TAG('K','I','S',' ')},*/	/* Kisii */
/*{"??",	HB_TAG('K','K','N',' ')},*/	/* Kokni */
/*{"??",	HB_TAG('K','M','S',' ')},*/	/* Komso */
/*{"??",	HB_TAG('K','O','D',' ')},*/	/* Kodagu */
/*{"??",	HB_TAG('K','O','H',' ')},*/	/* Korean Old Hangul */
/*{"??",	HB_TAG('K','O','N',' ')},*/	/* Kikongo */
/*{"??",	HB_TAG('K','R','K',' ')},*/	/* Karakalpak */
/*{"??",	HB_TAG('K','R','N',' ')},*/	/* Karen */
/*{"??",	HB_TAG('K','U','L',' ')},*/	/* Kulvi */
/*{"??",	HB_TAG('L','A','H',' ')},*/	/* Lahuli */
/*{"??",	HB_TAG('L','A','M',' ')},*/	/* Lambani */
/*{"??",	HB_TAG('L','C','R',' ')},*/	/* L-Cree */
/*{"??",	HB_TAG('L','E','Z',' ')},*/	/* Lezgi */
/*{"??",	HB_TAG('L','M','A',' ')},*/	/* Low Mari */
/*{"??",	HB_TAG('L','U','B',' ')},*/	/* Luba */
/*{"??",	HB_TAG('L','U','G',' ')},*/	/* Luganda */
/*{"??",	HB_TAG('L','U','H',' ')},*/	/* Luhya */
/*{"??",	HB_TAG('M','A','K',' ')},*/	/* Makua */
/*{"??",	HB_TAG('M','A','L',' ')},*/	/* Malayalam Traditional */
/*{"??",	HB_TAG('M','B','N',' ')},*/	/* Mbundu */
/*{"??",	HB_TAG('M','I','Z',' ')},*/	/* Mizo */
/*{"??",	HB_TAG('M','L','N',' ')},*/	/* Malinke */
/*{"??",	HB_TAG('M','N','K',' ')},*/	/* Maninka */
/*{"??",	HB_TAG('M','O','R',' ')},*/	/* Moroccan */
/*{"??",	HB_TAG('N','A','G',' ')},*/	/* Naga-Assamese */
/*{"??",	HB_TAG('N','C','R',' ')},*/	/* N-Cree */
/*{"??",	HB_TAG('N','D','B',' ')},*/	/* Ndebele */
/*{"??",	HB_TAG('N','G','R',' ')},*/	/* Nagari */
/*{"??",	HB_TAG('N','H','C',' ')},*/	/* Norway House Cree */
/*{"??",	HB_TAG('N','K','L',' ')},*/	/* Nkole */
/*{"??",	HB_TAG('N','T','A',' ')},*/	/* Northern Tai */
/*{"??",	HB_TAG('O','C','R',' ')},*/	/* Oji-Cree */
/*{"??",	HB_TAG('P','A','A',' ')},*/	/* Palestinian Aramaic */
/*{"??",	HB_TAG('P','G','R',' ')},*/	/* Polytonic Greek */
/*{"??",	HB_TAG('P','L','G',' ')},*/	/* Palaung */
/*{"??",	HB_TAG('Q','I','N',' ')},*/	/* Chin */
/*{"??",	HB_TAG('R','B','U',' ')},*/	/* Russian Buriat */
/*{"??",	HB_TAG('R','C','R',' ')},*/	/* R-Cree */
/*{"??",	HB_TAG('R','M','S',' ')},*/	/* Rhaeto-Romanic */
/*{"??",	HB_TAG('R','U','A',' ')},*/	/* Ruanda */
/*{"??",	HB_TAG('S','A','Y',' ')},*/	/* Sayisi */
/*{"??",	HB_TAG('S','E','K',' ')},*/	/* Sekota */
/*{"??",	HB_TAG('S','I','G',' ')},*/	/* Silte Gurage */
/*{"??",	HB_TAG('S','L','A',' ')},*/	/* Slavey */
/*{"??",	HB_TAG('S','O','G',' ')},*/	/* Sodo Gurage */
/*{"??",	HB_TAG('S','O','T',' ')},*/	/* Sotho */
/*{"??",	HB_TAG('S','W','A',' ')},*/	/* Swadaya Aramaic */
/*{"??",	HB_TAG('S','W','Z',' ')},*/	/* Swazi */
/*{"??",	HB_TAG('S','X','T',' ')},*/	/* Sutu */
/*{"??",	HB_TAG('T','A','B',' ')},*/	/* Tabasaran */
/*{"??",	HB_TAG('T','C','R',' ')},*/	/* TH-Cree */
/*{"??",	HB_TAG('T','G','N',' ')},*/	/* Tongan */
/*{"??",	HB_TAG('T','M','N',' ')},*/	/* Temne */
/*{"??",	HB_TAG('T','N','E',' ')},*/	/* Tundra Nenets */
/*{"??",	HB_TAG('T','O','D',' ')},*/	/* Todo */
/*{"??",	HB_TAG('T','U','A',' ')},*/	/* Turoyo Aramaic */
/*{"??",	HB_TAG('T','U','V',' ')},*/	/* Tuvin */
/*{"??",	HB_TAG('W','C','R',' ')},*/	/* West-Cree */
/*{"??",	HB_TAG('X','B','D',' ')},*/	/* Tai Lue */
/*{"??",	HB_TAG('Y','C','R',' ')},*/	/* Y-Cree */
/*{"??",	HB_TAG('Y','I','C',' ')},*/	/* Yi Classic */
/*{"??",	HB_TAG('Y','I','M',' ')},*/	/* Yi Modern */
/*{"??",	HB_TAG('Z','H','P',' ')},*/	/* Chinese Phonetic */
};

static int
lang_compare_first_component (const char *a,
			      const char *b)
{
  unsigned int da, db;
  const char *p;

  p = strstr (a, "-");
  da = p ? (unsigned int) (p - a) : strlen (a);

  p = strstr (b, "-");
  db = p ? (unsigned int) (p - b) : strlen (b);

  return strncmp (a, b, MAX (da, db));
}

static hb_bool_t
lang_matches (const char *lang_str, const char *spec)
{
  unsigned int len = strlen (spec);

  return lang_str && strncmp (lang_str, spec, len) == 0 &&
	 (lang_str[len] == '\0' || lang_str[len] == '-');
}

hb_tag_t
hb_ot_tag_from_language (hb_language_t language)
{
  const char *lang_str;
  LangTag *lang_tag;

  if (language == NULL)
    return HB_OT_TAG_DEFAULT_LANGUAGE;

  lang_str = hb_language_to_string (language);

  if (0 == strcmp (lang_str, "x-hbot")) {
    char tag[4];
    int i;
    lang_str += 6;
#define IS_LETTER(c) (((c) >= 'a' && (c) <= 'z') || ((c) >= 'A' && (c) <= 'Z'))
#define TO_UPPER(c) (((c) >= 'a' && (c) <= 'z') ? (c) + 'A' - 'a' : (c))
    for (i = 0; i < 4 && IS_LETTER (lang_str[i]); i++)
      tag[i] = TO_UPPER (lang_str[i]);
    for (; i < 4; i++)
      tag[i] = ' ';
    return HB_TAG_STR (tag);
  }

  /* find a language matching in the first component */
  lang_tag = bsearch (lang_str, ot_languages,
		      ARRAY_LENGTH (ot_languages), sizeof (LangTag),
		      (hb_compare_func_t) lang_compare_first_component);

  /* we now need to find the best language matching */
  if (lang_tag)
  {
    hb_bool_t found = FALSE;

    /* go to the final one matching in the first component */
    while (lang_tag + 1 < ot_languages + ARRAY_LENGTH (ot_languages) &&
	   lang_compare_first_component (lang_str, (lang_tag + 1)->language) == 0)
      lang_tag++;

    /* go back, find which one matches completely */
    while (lang_tag >= ot_languages &&
	   lang_compare_first_component (lang_str, lang_tag->language) == 0)
    {
      if (lang_matches (lang_str, lang_tag->language)) {
	found = TRUE;
	break;
      }

      lang_tag--;
    }

    if (!found)
      lang_tag = NULL;
  }

  if (lang_tag)
    return lang_tag->tag;

  return HB_OT_TAG_DEFAULT_LANGUAGE;
}

hb_language_t
hb_ot_tag_to_language (hb_tag_t tag)
{
  unsigned int i;
  unsigned char buf[11] = "x-hbot";

  for (i = 0; i < ARRAY_LENGTH (ot_languages); i++)
    if (ot_languages[i].tag == tag)
      return hb_language_from_string (ot_languages[i].language);

  buf[6] = tag >> 24;
  buf[7] = (tag >> 16) & 0xFF;
  buf[8] = (tag >> 8) & 0xFF;
  buf[9] = tag & 0xFF;
  buf[10] = '\0';
  return hb_language_from_string ((char *) buf);
}


HB_END_DECLS
