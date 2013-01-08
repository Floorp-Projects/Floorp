/*
 * Copyright © 2009  Red Hat, Inc.
 * Copyright © 2011  Google, Inc.
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
 * Google Author(s): Behdad Esfahbod, Roozbeh Pournader
 */

#include "hb-private.hh"
#include "hb-ot.h"

#include <string.h>



/* hb_script_t */

static hb_tag_t
hb_ot_old_tag_from_script (hb_script_t script)
{
  /* This seems to be accurate as of end of 2012. */

  switch ((hb_tag_t) script) {
    case HB_SCRIPT_INVALID:		return HB_OT_TAG_DEFAULT_SCRIPT;

    /* KATAKANA and HIRAGANA both map to 'kana' */
    case HB_SCRIPT_HIRAGANA:		return HB_TAG('k','a','n','a');

    /* Spaces at the end are preserved, unlike ISO 15924 */
    case HB_SCRIPT_LAO:			return HB_TAG('l','a','o',' ');
    case HB_SCRIPT_YI:			return HB_TAG('y','i',' ',' ');
    /* Unicode-5.0 additions */
    case HB_SCRIPT_NKO:			return HB_TAG('n','k','o',' ');
    /* Unicode-5.1 additions */
    case HB_SCRIPT_VAI:			return HB_TAG('v','a','i',' ');
    /* Unicode-5.2 additions */
    /* Unicode-6.0 additions */
  }

  /* Else, just change first char to lowercase and return */
  return ((hb_tag_t) script) | 0x20000000;
}

static hb_script_t
hb_ot_old_tag_to_script (hb_tag_t tag)
{
  if (unlikely (tag == HB_OT_TAG_DEFAULT_SCRIPT))
    return HB_SCRIPT_INVALID;

  /* This side of the conversion is fully algorithmic. */

  /* Any spaces at the end of the tag are replaced by repeating the last
   * letter.  Eg 'nko ' -> 'Nkoo' */
  if (unlikely ((tag & 0x0000FF00) == 0x00002000))
    tag |= (tag >> 8) & 0x0000FF00; /* Copy second letter to third */
  if (unlikely ((tag & 0x000000FF) == 0x00000020))
    tag |= (tag >> 8) & 0x000000FF; /* Copy third letter to fourth */

  /* Change first char to uppercase and return */
  return (hb_script_t) (tag & ~0x20000000);
}

static hb_tag_t
hb_ot_new_tag_from_script (hb_script_t script)
{
  switch ((hb_tag_t) script) {
    case HB_SCRIPT_BENGALI:		return HB_TAG('b','n','g','2');
    case HB_SCRIPT_DEVANAGARI:		return HB_TAG('d','e','v','2');
    case HB_SCRIPT_GUJARATI:		return HB_TAG('g','j','r','2');
    case HB_SCRIPT_GURMUKHI:		return HB_TAG('g','u','r','2');
    case HB_SCRIPT_KANNADA:		return HB_TAG('k','n','d','2');
    case HB_SCRIPT_MALAYALAM:		return HB_TAG('m','l','m','2');
    case HB_SCRIPT_ORIYA:		return HB_TAG('o','r','y','2');
    case HB_SCRIPT_TAMIL:		return HB_TAG('t','m','l','2');
    case HB_SCRIPT_TELUGU:		return HB_TAG('t','e','l','2');
    case HB_SCRIPT_MYANMAR:		return HB_TAG('m','y','m','2');
  }

  return HB_OT_TAG_DEFAULT_SCRIPT;
}

static hb_script_t
hb_ot_new_tag_to_script (hb_tag_t tag)
{
  switch (tag) {
    case HB_TAG('b','n','g','2'):	return HB_SCRIPT_BENGALI;
    case HB_TAG('d','e','v','2'):	return HB_SCRIPT_DEVANAGARI;
    case HB_TAG('g','j','r','2'):	return HB_SCRIPT_GUJARATI;
    case HB_TAG('g','u','r','2'):	return HB_SCRIPT_GURMUKHI;
    case HB_TAG('k','n','d','2'):	return HB_SCRIPT_KANNADA;
    case HB_TAG('m','l','m','2'):	return HB_SCRIPT_MALAYALAM;
    case HB_TAG('o','r','y','2'):	return HB_SCRIPT_ORIYA;
    case HB_TAG('t','m','l','2'):	return HB_SCRIPT_TAMIL;
    case HB_TAG('t','e','l','2'):	return HB_SCRIPT_TELUGU;
    case HB_TAG('m','y','m','2'):	return HB_SCRIPT_MYANMAR;
  }

  return HB_SCRIPT_UNKNOWN;
}

/*
 * Complete list at:
 * https://www.microsoft.com/typography/otspec/scripttags.htm
 * https://www.microsoft.com/typography/otspec160/scripttagsProposed.htm
 *
 * Most of the script tags are the same as the ISO 15924 tag but lowercased.
 * So we just do that, and handle the exceptional cases in a switch.
 */

void
hb_ot_tags_from_script (hb_script_t  script,
			hb_tag_t    *script_tag_1,
			hb_tag_t    *script_tag_2)
{
  hb_tag_t new_tag;

  *script_tag_2 = HB_OT_TAG_DEFAULT_SCRIPT;
  *script_tag_1 = hb_ot_old_tag_from_script (script);

  new_tag = hb_ot_new_tag_from_script (script);
  if (unlikely (new_tag != HB_OT_TAG_DEFAULT_SCRIPT)) {
    *script_tag_2 = *script_tag_1;
    *script_tag_1 = new_tag;
  }
}

hb_script_t
hb_ot_tag_to_script (hb_tag_t tag)
{
  if (unlikely ((tag & 0x000000FF) == '2'))
    return hb_ot_new_tag_to_script (tag);

  return hb_ot_old_tag_to_script (tag);
}


/* hb_language_t */

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
 * Updated on 2012/12/07 with more research into remaining codes.
 *
 * Some items still missing.  Those are commented out at the end.
 * Keep sorted for bsearch.
 */

static const LangTag ot_languages[] = {
  {"aa",	HB_TAG('A','F','R',' ')},	/* Afar */
  {"ab",	HB_TAG('A','B','K',' ')},	/* Abkhazian */
  {"abq",	HB_TAG('A','B','A',' ')},	/* Abaza */
  {"ada",	HB_TAG('D','N','G',' ')},	/* Dangme */
  {"ady",	HB_TAG('A','D','Y',' ')},	/* Adyghe */
  {"af",	HB_TAG('A','F','K',' ')},	/* Afrikaans */
  {"aii",	HB_TAG('S','W','A',' ')},	/* Swadaya Aramaic */
  {"aiw",	HB_TAG('A','R','I',' ')},	/* Aari */
  {"alt",	HB_TAG('A','L','T',' ')},	/* [Southern] Altai */
  {"am",	HB_TAG('A','M','H',' ')},	/* Amharic */
  {"amf",	HB_TAG('H','B','N',' ')},	/* Hammer-Banna */
  {"ar",	HB_TAG('A','R','A',' ')},	/* Arabic */
  {"arn",	HB_TAG('M','A','P',' ')},	/* Mapudungun */
  {"as",	HB_TAG('A','S','M',' ')},	/* Assamese */
  {"ath",	HB_TAG('A','T','H',' ')},	/* Athapaskan [family] */
  {"atv",	HB_TAG('A','L','T',' ')},	/* [Northern] Altai */
  {"av",	HB_TAG('A','V','R',' ')},	/* Avaric */
  {"awa",	HB_TAG('A','W','A',' ')},	/* Awadhi */
  {"ay",	HB_TAG('A','Y','M',' ')},	/* Aymara */
  {"az",	HB_TAG('A','Z','E',' ')},	/* Azerbaijani */
  {"ba",	HB_TAG('B','S','H',' ')},	/* Bashkir */
  {"bai",	HB_TAG('B','M','L',' ')},	/* Bamileke [family] */
  {"bal",	HB_TAG('B','L','I',' ')},	/* Baluchi */
  {"bci",	HB_TAG('B','A','U',' ')},	/* Baule */
  {"bcq",	HB_TAG('B','C','H',' ')},	/* Bench */
  {"be",	HB_TAG('B','E','L',' ')},  	/* Belarussian */
  {"bem",	HB_TAG('B','E','M',' ')},	/* Bemba (Zambia) */
  {"ber",	HB_TAG('B','E','R',' ')},  	/* Berber [family] */
  {"bfq",	HB_TAG('B','A','D',' ')},	/* Badaga */
  {"bft",	HB_TAG('B','L','T',' ')},	/* Balti */
  {"bfy",	HB_TAG('B','A','G',' ')},	/* Baghelkhandi */
  {"bg",	HB_TAG('B','G','R',' ')},	/* Bulgarian */
  {"bhb",	HB_TAG('B','H','I',' ')},	/* Bhili */
  {"bho",	HB_TAG('B','H','O',' ')},	/* Bhojpuri */
  {"bik",	HB_TAG('B','I','K',' ')},	/* Bikol */
  {"bin",	HB_TAG('E','D','O',' ')},	/* Bini */
  {"bjt",	HB_TAG('B','L','N',' ')},	/* Balanta-Ganja */
  {"bla",	HB_TAG('B','K','F',' ')},	/* Blackfoot */
  {"ble",	HB_TAG('B','L','N',' ')},	/* Balanta-Kentohe */
  {"bm",	HB_TAG('B','M','B',' ')},	/* Bambara */
  {"bn",	HB_TAG('B','E','N',' ')},	/* Bengali */
  {"bo",	HB_TAG('T','I','B',' ')},	/* Tibetan */
  {"br",	HB_TAG('B','R','E',' ')},	/* Breton */
  {"bra",	HB_TAG('B','R','I',' ')},	/* Braj Bhasha */
  {"brh",	HB_TAG('B','R','H',' ')},	/* Brahui */
  {"bs",	HB_TAG('B','O','S',' ')},	/* Bosnian */
  {"btb",	HB_TAG('B','T','I',' ')},	/* Beti (Cameroon) */
  {"bxr",	HB_TAG('R','B','U',' ')},	/* Russian Buriat */
  {"byn",	HB_TAG('B','I','L',' ')},	/* Bilen */
  {"ca",	HB_TAG('C','A','T',' ')},	/* Catalan */
  {"ce",	HB_TAG('C','H','E',' ')},	/* Chechen */
  {"ceb",	HB_TAG('C','E','B',' ')},	/* Cebuano */
  {"chp",	HB_TAG('C','H','P',' ')},	/* Chipewyan */
  {"chr",	HB_TAG('C','H','R',' ')},	/* Cherokee */
  {"ckt",	HB_TAG('C','H','K',' ')},	/* Chukchi */
  {"cop",	HB_TAG('C','O','P',' ')},	/* Coptic */
  {"cr",	HB_TAG('C','R','E',' ')},	/* Cree */
  {"crh",	HB_TAG('C','R','T',' ')},	/* Crimean Tatar */
  {"crj",	HB_TAG('E','C','R',' ')},	/* [Southern] East Cree */
  {"crl",	HB_TAG('E','C','R',' ')},	/* [Northern] East Cree */
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
  {"dje",	HB_TAG('D','J','R',' ')},	/* Djerma */
  {"dng",	HB_TAG('D','U','N',' ')},	/* Dungan */
  {"doi",	HB_TAG('D','G','R',' ')},	/* Dogri */
  {"dsb",	HB_TAG('L','S','B',' ')},	/* Lower Sorbian */
  {"dv",	HB_TAG('D','I','V',' ')},	/* Dhivehi */
  {"dyu",	HB_TAG('J','U','L',' ')},	/* Jula */
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
  {"gez",	HB_TAG('G','E','Z',' ')},	/* Ge'ez */
  {"gl",	HB_TAG('G','A','L',' ')},	/* Galician */
  {"gld",	HB_TAG('N','A','N',' ')},	/* Nanai */
  {"gn",	HB_TAG('G','U','A',' ')},	/* Guarani */
  {"gon",	HB_TAG('G','O','N',' ')},	/* Gondi */
  {"grt",	HB_TAG('G','R','O',' ')},	/* Garo */
  {"gru",	HB_TAG('S','O','G',' ')},	/* Sodo Gurage */
  {"gu",	HB_TAG('G','U','J',' ')},	/* Gujarati */
  {"guk",	HB_TAG('G','M','Z',' ')},	/* Gumuz */
  {"gv",	HB_TAG('M','N','X',' ')},	/* Manx Gaelic */
  {"ha",	HB_TAG('H','A','U',' ')},	/* Hausa */
  {"har",	HB_TAG('H','R','I',' ')},	/* Harari */
  {"haw",	HB_TAG('H','A','W',' ')},  	/* Hawaiin */
  {"he",	HB_TAG('I','W','R',' ')},	/* Hebrew */
  {"hi",	HB_TAG('H','I','N',' ')},	/* Hindi */
  {"hil",	HB_TAG('H','I','L',' ')},	/* Hiligaynon */
  {"hnd",	HB_TAG('H','N','D',' ')},	/* [Southern] Hindko */
  {"hne",	HB_TAG('C','H','H',' ')},	/* Chattisgarhi */
  {"hno",	HB_TAG('H','N','D',' ')},	/* [Northern] Hindko */
  {"hoc",	HB_TAG('H','O',' ',' ')},	/* Ho */
  {"hoj",	HB_TAG('H','A','R',' ')},	/* Harauti */
  {"hr",	HB_TAG('H','R','V',' ')},	/* Croatian */
  {"hsb",	HB_TAG('U','S','B',' ')},	/* Upper Sorbian */
  {"ht",	HB_TAG('H','A','I',' ')},	/* Haitian */
  {"hu",	HB_TAG('H','U','N',' ')},	/* Hungarian */
  {"hy",	HB_TAG('H','Y','E',' ')},	/* Armenian */
  {"id",	HB_TAG('I','N','D',' ')},	/* Indonesian */
  {"ig",	HB_TAG('I','B','O',' ')},	/* Igbo */
  {"igb",	HB_TAG('E','B','I',' ')},	/* Ebira */
  {"ijo",	HB_TAG('I','J','O',' ')},	/* Ijo [family] */
  {"ilo",	HB_TAG('I','L','O',' ')},	/* Ilokano */
  {"inh",	HB_TAG('I','N','G',' ')},	/* Ingush */
  {"is",	HB_TAG('I','S','L',' ')},	/* Icelandic */
  {"it",	HB_TAG('I','T','A',' ')},	/* Italian */
  {"iu",	HB_TAG('I','N','U',' ')},	/* Inuktitut */
  {"ja",	HB_TAG('J','A','N',' ')},	/* Japanese */
  {"jv",	HB_TAG('J','A','V',' ')},	/* Javanese */
  {"ka",	HB_TAG('K','A','T',' ')},	/* Georgian */
  {"kaa",	HB_TAG('K','R','K',' ')},	/* Karakalpak */
  {"kam",	HB_TAG('K','M','B',' ')},	/* Kamba (Kenya) */
  {"kar",	HB_TAG('K','R','N',' ')},	/* Karen [family] */
  {"kbd",	HB_TAG('K','A','B',' ')},	/* Kabardian */
  {"kdr",	HB_TAG('K','R','M',' ')},	/* Karaim */
  {"kdt",	HB_TAG('K','U','Y',' ')},	/* Kuy */
  {"kex",	HB_TAG('K','K','N',' ')},	/* Kokni */
  {"kfr",	HB_TAG('K','A','C',' ')},	/* Kachchi */
  {"kfy",	HB_TAG('K','M','N',' ')},	/* Kumaoni */
  {"kha",	HB_TAG('K','S','I',' ')},	/* Khasi */
  {"khb",	HB_TAG('X','B','D',' ')},	/* Tai Lue */
  {"khw",	HB_TAG('K','H','W',' ')},	/* Khowar */
  {"ki",	HB_TAG('K','I','K',' ')},	/* Kikuyu */
  {"kjh",	HB_TAG('K','H','A',' ')},	/* Khakass */
  {"kk",	HB_TAG('K','A','Z',' ')},	/* Kazakh */
  {"kl",	HB_TAG('G','R','N',' ')},	/* Kalaallisut */
  {"kln",	HB_TAG('K','A','L',' ')},	/* Kalenjin */
  {"km",	HB_TAG('K','H','M',' ')},	/* Central Khmer */
  {"kmb",	HB_TAG('M','B','N',' ')},	/* [North] Mbundu */
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
  {"kxc",	HB_TAG('K','M','S',' ')},	/* Komso */
  {"kxu",	HB_TAG('K','U','I',' ')},	/* Kui (India) */
  {"ky",	HB_TAG('K','I','R',' ')},	/* Kirghiz */
  {"la",	HB_TAG('L','A','T',' ')},	/* Latin */
  {"lad",	HB_TAG('J','U','D',' ')},	/* Ladino */
  {"lb",	HB_TAG('L','T','Z',' ')},	/* Luxembourgish */
  {"lbe",	HB_TAG('L','A','K',' ')},	/* Lak */
  {"lbj",	HB_TAG('L','D','K',' ')},	/* Ladakhi */
  {"lez",	HB_TAG('L','E','Z',' ')},	/* Lezgi */
  {"lg",	HB_TAG('L','U','G',' ')},	/* Luganda */
  {"lif",	HB_TAG('L','M','B',' ')},	/* Limbu */
  {"lld",	HB_TAG('L','A','D',' ')},	/* Ladin */
  {"lmn",	HB_TAG('L','A','M',' ')},	/* Lambani */
  {"ln",	HB_TAG('L','I','N',' ')},	/* Lingala */
  {"lo",	HB_TAG('L','A','O',' ')},	/* Lao */
  {"lt",	HB_TAG('L','T','H',' ')},	/* Lithuanian */
  {"lu",	HB_TAG('L','U','B',' ')},	/* Luba-Katanga */
  {"lua",	HB_TAG('L','U','B',' ')},	/* Luba-Kasai */
  {"luo",	HB_TAG('L','U','O',' ')},	/* Luo (Kenya and Tanzania) */
  {"lus",	HB_TAG('M','I','Z',' ')},	/* Mizo */
  {"luy",	HB_TAG('L','U','H',' ')},	/* Luhya [macrolanguage] */
  {"lv",	HB_TAG('L','V','I',' ')},	/* Latvian */
  {"lzz",	HB_TAG('L','A','Z',' ')},	/* Laz */
  {"mai",	HB_TAG('M','T','H',' ')},	/* Maithili */
  {"mdc",	HB_TAG('M','L','E',' ')},	/* Male (Papua New Guinea) */
  {"mdf",	HB_TAG('M','O','K',' ')},	/* Moksha */
  {"mdy",	HB_TAG('M','L','E',' ')},	/* Male (Ethiopia) */
  {"men",	HB_TAG('M','D','E',' ')},	/* Mende (Sierra Leone) */
  {"mg",	HB_TAG('M','L','G',' ')},	/* Malagasy */
  {"mhr",	HB_TAG('L','M','A',' ')},	/* Low Mari */
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
  {"mrj",	HB_TAG('H','M','A',' ')},	/* High Mari */
  {"ms",	HB_TAG('M','L','Y',' ')},	/* Malay */
  {"mt",	HB_TAG('M','T','S',' ')},	/* Maltese */
  {"mwr",	HB_TAG('M','A','W',' ')},	/* Marwari */
  {"my",	HB_TAG('B','R','M',' ')},	/* Burmese */
  {"mym",	HB_TAG('M','E','N',' ')},	/* Me'en */
  {"myv",	HB_TAG('E','R','Z',' ')},	/* Erzya */
  {"nag",	HB_TAG('N','A','G',' ')},	/* Naga-Assamese */
  {"nb",	HB_TAG('N','O','R',' ')},	/* Norwegian Bokmål */
  {"nco",	HB_TAG('S','I','B',' ')},	/* Sibe */
  {"nd",	HB_TAG('N','D','B',' ')},	/* [North] Ndebele */
  {"ne",	HB_TAG('N','E','P',' ')},	/* Nepali */
  {"new",	HB_TAG('N','E','W',' ')},	/* Newari */
  {"ng",	HB_TAG('N','D','G',' ')},	/* Ndonga */
  {"ngl",	HB_TAG('L','M','W',' ')},	/* Lomwe */
  {"niu",	HB_TAG('N','I','U',' ')},	/* Niuean */
  {"niv",	HB_TAG('G','I','L',' ')},	/* Gilyak */
  {"nl",	HB_TAG('N','L','D',' ')},	/* Dutch */
  {"nn",	HB_TAG('N','Y','N',' ')},	/* Norwegian Nynorsk */
  {"no",	HB_TAG('N','O','R',' ')},	/* Norwegian (deprecated) */
  {"nod",	HB_TAG('N','T','A',' ')},	/* Northern Tai */
  {"nog",	HB_TAG('N','O','G',' ')},	/* Nogai */
  {"nqo",	HB_TAG('N','K','O',' ')},	/* N'Ko */
  {"nr",	HB_TAG('N','D','B',' ')},	/* [South] Ndebele */
  {"nsk",	HB_TAG('N','A','S',' ')},	/* Naskapi */
  {"nso",	HB_TAG('S','O','T',' ')},	/* [Northern] Sotho */
  {"ny",	HB_TAG('C','H','I',' ')},	/* Nyanja */
  {"nyn",	HB_TAG('N','K','L',' ')},	/* Nkole */
  {"oc",	HB_TAG('O','C','I',' ')},	/* Occitan (post 1500) */
  {"oj",	HB_TAG('O','J','B',' ')},	/* Ojibwa */
  {"ojs",	HB_TAG('O','C','R',' ')},	/* Oji-Cree */
  {"om",	HB_TAG('O','R','O',' ')},	/* Oromo */
  {"or",	HB_TAG('O','R','I',' ')},	/* Oriya */
  {"os",	HB_TAG('O','S','S',' ')},	/* Ossetian */
  {"pa",	HB_TAG('P','A','N',' ')},	/* Panjabi */
  {"pce",	HB_TAG('P','L','G',' ')},	/* [Ruching] Palaung */
  {"pi",	HB_TAG('P','A','L',' ')},	/* Pali */
  {"pl",	HB_TAG('P','L','K',' ')},	/* Polish */
  {"pll",	HB_TAG('P','L','G',' ')},	/* [Shwe] Palaung */
  {"plp",	HB_TAG('P','A','P',' ')},	/* Palpa */
  {"prs",	HB_TAG('D','R','I',' ')},	/* Dari */
  {"ps",	HB_TAG('P','A','S',' ')},	/* Pushto */
  {"pt",	HB_TAG('P','T','G',' ')},	/* Portuguese */
  {"raj",	HB_TAG('R','A','J',' ')},	/* Rajasthani */
  {"rbb",	HB_TAG('P','L','G',' ')},	/* [Rumai] Palaung */
  {"ria",	HB_TAG('R','I','A',' ')},	/* Riang (India) */
  {"ril",	HB_TAG('R','I','A',' ')},	/* Riang (Myanmar) */
  {"rki",	HB_TAG('A','R','K',' ')},	/* Arakanese */
  {"rm",	HB_TAG('R','M','S',' ')},	/* Rhaeto-Romanic */
  {"ro",	HB_TAG('R','O','M',' ')},	/* Romanian */
  {"rom",	HB_TAG('R','O','Y',' ')},	/* Romany */
  {"ru",	HB_TAG('R','U','S',' ')},	/* Russian */
  {"rue",	HB_TAG('R','S','Y',' ')},	/* Rusyn */
  {"rw",	HB_TAG('R','U','A',' ')},	/* Ruanda */
  {"sa",	HB_TAG('S','A','N',' ')},	/* Sanskrit */
  {"sah",	HB_TAG('Y','A','K',' ')},	/* Yakut */
  {"sat",	HB_TAG('S','A','T',' ')},	/* Santali */
  {"sck",	HB_TAG('S','A','D',' ')},	/* Sadri */
  {"scs",	HB_TAG('S','L','A',' ')},	/* [North] Slavey */
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
  {"ss",	HB_TAG('S','W','Z',' ')},	/* Swazi */
  {"st",	HB_TAG('S','O','T',' ')},	/* [Southern] Sotho */
  {"suq",	HB_TAG('S','U','R',' ')},	/* Suri */
  {"sv",	HB_TAG('S','V','E',' ')},	/* Swedish */
  {"sva",	HB_TAG('S','V','A',' ')},	/* Svan */
  {"sw",	HB_TAG('S','W','K',' ')},	/* Swahili */
  {"swb",	HB_TAG('C','M','R',' ')},	/* Comorian */
  {"syr",	HB_TAG('S','Y','R',' ')},	/* Syriac */
  {"ta",	HB_TAG('T','A','M',' ')},	/* Tamil */
  {"tab",	HB_TAG('T','A','B',' ')},	/* Tabasaran */
  {"tcy",	HB_TAG('T','U','L',' ')},	/* Tulu */
  {"te",	HB_TAG('T','E','L',' ')},	/* Telugu */
  {"tem",	HB_TAG('T','M','N',' ')},	/* Temne */
  {"tg",	HB_TAG('T','A','J',' ')},	/* Tajik */
  {"th",	HB_TAG('T','H','A',' ')},	/* Thai */
  {"ti",	HB_TAG('T','G','Y',' ')},	/* Tigrinya */
  {"tig",	HB_TAG('T','G','R',' ')},	/* Tigre */
  {"tk",	HB_TAG('T','K','M',' ')},	/* Turkmen */
  {"tn",	HB_TAG('T','N','A',' ')},	/* Tswana */
  {"to",	HB_TAG('T','G','N',' ')},	/* Tonga (Tonga Islands) */
  {"tr",	HB_TAG('T','R','K',' ')},	/* Turkish */
  {"tru",	HB_TAG('T','U','A',' ')},	/* Turoyo Aramaic */
  {"ts",	HB_TAG('T','S','G',' ')},	/* Tsonga */
  {"tt",	HB_TAG('T','A','T',' ')},	/* Tatar */
  {"tw",	HB_TAG('T','W','I',' ')},	/* Twi */
  {"ty",	HB_TAG('T','H','T',' ')},	/* Tahitian */
  {"tyv",	HB_TAG('T','U','V',' ')},	/* Tuvin */
  {"udm",	HB_TAG('U','D','M',' ')},	/* Udmurt */
  {"ug",	HB_TAG('U','Y','G',' ')},	/* Uighur */
  {"uk",	HB_TAG('U','K','R',' ')},	/* Ukrainian */
  {"umb",	HB_TAG('M','B','N',' ')},	/* [South] Mbundu */
  {"unr",	HB_TAG('M','U','N',' ')},	/* Mundari */
  {"ur",	HB_TAG('U','R','D',' ')},	/* Urdu */
  {"uz",	HB_TAG('U','Z','B',' ')},	/* Uzbek */
  {"ve",	HB_TAG('V','E','N',' ')},	/* Venda */
  {"vi",	HB_TAG('V','I','T',' ')},	/* Vietnamese */
  {"vmw",	HB_TAG('M','A','K',' ')},	/* Makua */
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
  {"zne",	HB_TAG('Z','N','D',' ')},	/* Zande */
  {"zu",	HB_TAG('Z','U','L',' ')} 	/* Zulu */

  /* The corresponding languages IDs for the following IDs are unclear,
   * overlap, or are architecturally weird. Needs more research. */

/*{"ahg/awn/xan?",	HB_TAG('A','G','W',' ')},*/	/* Agaw */
/*{"gsw?/gsw-FR?",	HB_TAG('A','L','S',' ')},*/	/* Alsatian */
/*{"krc",	HB_TAG('B','A','L',' ')},*/	/* Balkar */
/*{"??",	HB_TAG('B','C','R',' ')},*/	/* Bible Cree */
/*{"sgw?",	HB_TAG('C','H','G',' ')},*/	/* Chaha Gurage */
/*{"acf/gcf?",	HB_TAG('F','A','N',' ')},*/	/* French Antillean */
/*{"vls/nl-be",	HB_TAG('F','L','E',' ')},*/	/* Flemish */
/*{"enf?/yrk?",	HB_TAG('F','N','E',' ')},*/	/* Forest Nenets */
/*{"fuf?",	HB_TAG('F','T','A',' ')},*/	/* Futa */
/*{"ar-Syrc?",	HB_TAG('G','A','R',' ')},*/	/* Garshuni */
/*{"cfm/rnl?",	HB_TAG('H','A','L',' ')},*/	/* Halam */
/*{"ga-Latg?/Latg?",	HB_TAG('I','R','T',' ')},*/	/* Irish Traditional */
/*{"krc",	HB_TAG('K','A','R',' ')},*/	/* Karachay */
/*{"alw?/ktb?",	HB_TAG('K','E','B',' ')},*/	/* Kebena */
/*{"Geok",	HB_TAG('K','G','E',' ')},*/	/* Khutsuri Georgian */
/*{"kca",	HB_TAG('K','H','K',' ')},*/	/* Khanty-Kazim */
/*{"kca",	HB_TAG('K','H','S',' ')},*/	/* Khanty-Shurishkar */
/*{"kca",	HB_TAG('K','H','V',' ')},*/	/* Khanty-Vakhi */
/*{"guz?/kqs?/kss?",	HB_TAG('K','I','S',' ')},*/	/* Kisii */
/*{"kfa/kfi?/kpb?/xua?/xuj?",	HB_TAG('K','O','D',' ')},*/	/* Kodagu */
/*{"okm?/oko?",	HB_TAG('K','O','H',' ')},*/	/* Korean Old Hangul */
/*{"kon?/ktu?/...",	HB_TAG('K','O','N',' ')},*/	/* Kikongo */
/*{"kfx?",	HB_TAG('K','U','L',' ')},*/	/* Kulvi */
/*{"??",	HB_TAG('L','A','H',' ')},*/	/* Lahuli */
/*{"??",	HB_TAG('L','C','R',' ')},*/	/* L-Cree */
/*{"??",	HB_TAG('M','A','L',' ')},*/	/* Malayalam Traditional */
/*{"mnk?/mlq?/...",	HB_TAG('M','L','N',' ')},*/	/* Malinke */
/*{"man?/myq?/mku?/msc?/...",	HB_TAG('M','N','K',' ')},*/	/* Maninka */
/*{"??",	HB_TAG('M','O','R',' ')},*/	/* Moroccan */
/*{"??",	HB_TAG('N','C','R',' ')},*/	/* N-Cree */
/*{"??",	HB_TAG('N','H','C',' ')},*/	/* Norway House Cree */
/*{"jpa?/sam?",	HB_TAG('P','A','A',' ')},*/	/* Palestinian Aramaic */
/*{"polyton",	HB_TAG('P','G','R',' ')},*/	/* Polytonic Greek */
/*{"??",	HB_TAG('Q','I','N',' ')},*/	/* Asho Chin */
/*{"??",	HB_TAG('R','C','R',' ')},*/	/* R-Cree */
/*{"chp?",	HB_TAG('S','A','Y',' ')},*/	/* Sayisi */
/*{"xan?",	HB_TAG('S','E','K',' ')},*/	/* Sekota */
/*{"stv/wle?/xst?",	HB_TAG('S','I','G',' ')},*/	/* Silte Gurage */
/*{"ngo?",	HB_TAG('S','X','T',' ')},*/	/* Sutu */
/*{"??",	HB_TAG('T','C','R',' ')},*/	/* TH-Cree */
/*{"tnz?/tog?/toi?",	HB_TAG('T','N','G',' ')},*/	/* Tonga */
/*{"enh?/yrk?",	HB_TAG('T','N','E',' ')},*/	/* Tundra Nenets */
/*{"??",	HB_TAG('T','O','D',' ')},*/	/* Todo */
/*{"??",	HB_TAG('W','C','R',' ')},*/	/* West-Cree */
/*{"??",	HB_TAG('Y','C','R',' ')},*/	/* Y-Cree */
/*{"??",	HB_TAG('Y','I','C',' ')},*/	/* Yi Classic */
/*{"ii?/Yiii?",	HB_TAG('Y','I','M',' ')},*/	/* Yi Modern */
/*{"??",	HB_TAG('Z','H','P',' ')},*/	/* Chinese Phonetic */
};

static const LangTag ot_languages_zh[] = {
  {"zh-cn",	HB_TAG('Z','H','S',' ')},	/* Chinese (China) */
  {"zh-hk",	HB_TAG('Z','H','H',' ')},	/* Chinese (Hong Kong) */
  {"zh-mo",	HB_TAG('Z','H','T',' ')},	/* Chinese (Macao) */
  {"zh-sg",	HB_TAG('Z','H','S',' ')},	/* Chinese (Singapore) */
  {"zh-tw",	HB_TAG('Z','H','T',' ')} 	/* Chinese (Taiwan) */
};

static int
lang_compare_first_component (const char *a,
			      const char *b)
{
  unsigned int da, db;
  const char *p;

  p = strchr (a, '-');
  da = p ? (unsigned int) (p - a) : strlen (a);

  p = strchr (b, '-');
  db = p ? (unsigned int) (p - b) : strlen (b);

  return strncmp (a, b, MAX (da, db));
}

static hb_bool_t
lang_matches (const char *lang_str, const char *spec)
{
  unsigned int len = strlen (spec);

  return strncmp (lang_str, spec, len) == 0 &&
	 (lang_str[len] == '\0' || lang_str[len] == '-');
}

hb_tag_t
hb_ot_tag_from_language (hb_language_t language)
{
  const char *lang_str, *s;
  const LangTag *lang_tag;

  if (language == HB_LANGUAGE_INVALID)
    return HB_OT_TAG_DEFAULT_LANGUAGE;

  lang_str = hb_language_to_string (language);

  s = strstr (lang_str, "x-hbot");
  if (s) {
    char tag[4];
    int i;
    s += 6;
    for (i = 0; i < 4 && ISALPHA (s[i]); i++)
      tag[i] = TOUPPER (s[i]);
    if (i) {
      for (; i < 4; i++)
	tag[i] = ' ';
      return HB_TAG_CHAR4 (tag);
    }
  }

  /* Find a language matching in the first component */
  lang_tag = (LangTag *) bsearch (lang_str, ot_languages,
				  ARRAY_LENGTH (ot_languages), sizeof (LangTag),
				  (hb_compare_func_t) lang_compare_first_component);
  if (lang_tag)
    return lang_tag->tag;

  /* Otherwise, check the Chinese ones */
  if (0 == lang_compare_first_component (lang_str, "zh"))
  {
    unsigned int i;

    for (i = 0; i < ARRAY_LENGTH (ot_languages_zh); i++)
    {
      lang_tag = &ot_languages_zh[i];
      if (lang_matches (lang_tag->language, lang_str))
	return lang_tag->tag;
    }

    /* Otherwise just return 'ZHS ' */
    return HB_TAG('Z','H','S',' ');
  }

  s = strchr (lang_str, '-');
  if (!s)
    s = lang_str + strlen (lang_str);
  if (s - lang_str == 3) {
    /* Assume it's ISO-639-3 and upper-case and use it. */
    return hb_tag_from_string (lang_str, s - lang_str) & ~0x20202000;
  }

  return HB_OT_TAG_DEFAULT_LANGUAGE;
}

hb_language_t
hb_ot_tag_to_language (hb_tag_t tag)
{
  unsigned int i;

  if (tag == HB_OT_TAG_DEFAULT_LANGUAGE)
    return NULL;

  for (i = 0; i < ARRAY_LENGTH (ot_languages); i++)
    if (ot_languages[i].tag == tag)
      return hb_language_from_string (ot_languages[i].language, -1);

  /* If tag starts with ZH, it's Chinese */
  if ((tag & 0xFFFF0000)  == 0x5A480000) {
    switch (tag) {
      case HB_TAG('Z','H','H',' '): return hb_language_from_string ("zh-hk", -1); /* Hong Kong */
      default: {
        /* Encode the tag... */
	unsigned char buf[14] = "zh-x-hbot";
	buf[9] = tag >> 24;
	buf[10] = (tag >> 16) & 0xFF;
	buf[11] = (tag >> 8) & 0xFF;
	buf[12] = tag & 0xFF;
	if (buf[12] == 0x20)
	  buf[12] = '\0';
	buf[13] = '\0';
	return hb_language_from_string ((char *) buf, -1);
      }
    }
  }

  /* Else return a custom language in the form of "x-hbotABCD" */
  {
    unsigned char buf[11] = "x-hbot";
    buf[6] = tag >> 24;
    buf[7] = (tag >> 16) & 0xFF;
    buf[8] = (tag >> 8) & 0xFF;
    buf[9] = tag & 0xFF;
    if (buf[9] == 0x20)
      buf[9] = '\0';
    buf[10] = '\0';
    return hb_language_from_string ((char *) buf, -1);
  }
}


