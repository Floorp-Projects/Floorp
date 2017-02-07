/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include "nscore.h"
#include "nsString.h"
#include "nsXPCOMStrings.h"
#include "nsReadableUtils.h"
#include "nsWin32Locale.h"
#include <windows.h>

using namespace mozilla;

struct iso_pair 
{
	const char*	iso_code;
	DWORD       win_code;
};

struct iso_map
{
	const char* iso_code;
	DWORD       win_code;
	iso_pair    sublang_list[20];
};

//
// This list is used to map between ISO language
// References : 
// http://www.iso.ch/iso/en/prods-services/iso3166ma/02iso-3166-code-lists/list-en1.html
// http://www.loc.gov/standards/iso639-2/
// http://www.ethnologue.com/
// http://msdn.microsoft.com/library/default.asp?url=/library/en-us/intl/nls_19ir.asp
// http://msdn.microsoft.com/library/default.asp?url=/library/en-us/intl/nls_61df.asp
 
static const
iso_map iso_list[] =
{
	{"af",	LANG_AFRIKAANS, {
		{ "ZA", SUBLANG_DEFAULT },
		{ "",0}}
	},
	{ "ar", LANG_ARABIC, {
		{ "SA", SUBLANG_ARABIC_SAUDI_ARABIA }, 
		{ "IQ", SUBLANG_ARABIC_IRAQ },  
		{ "EG",	SUBLANG_ARABIC_EGYPT },	
		{ "LY", SUBLANG_ARABIC_LIBYA },
		{ "DZ", SUBLANG_ARABIC_ALGERIA },
		{ "MA", SUBLANG_ARABIC_MOROCCO },
		{ "TN", SUBLANG_ARABIC_TUNISIA },
		{ "OM", SUBLANG_ARABIC_OMAN }, 
		{ "YE", SUBLANG_ARABIC_YEMEN },
		{ "SY", SUBLANG_ARABIC_SYRIA },
		{ "JO", SUBLANG_ARABIC_JORDAN },
		{ "LB", SUBLANG_ARABIC_LEBANON },
		{ "KW", SUBLANG_ARABIC_KUWAIT },
		{ "AE", SUBLANG_ARABIC_UAE },
		{ "BH", SUBLANG_ARABIC_BAHRAIN },
		{ "QA", SUBLANG_ARABIC_QATAR },
		{"",0}}
	},
	{"az",	LANG_AZERI, {
		{ "AZ",SUBLANG_DEFAULT },  // XXX Latin vs Cyrillic vs Arabic
		{ "",0}}
	},
	{"be",	LANG_BELARUSIAN, {
		{ "BY",SUBLANG_DEFAULT },
		{ "",0}}
	},
	{"bg",	LANG_BULGARIAN, {
		{ "BG", SUBLANG_DEFAULT },
		{ "",0}}
	},
	{"ca",	LANG_CATALAN, {
		{ "ES", SUBLANG_DEFAULT},
		{ "",0}}
	},
	{"cs",	LANG_CZECH, {
		{ "CZ", SUBLANG_DEFAULT},
		{"",0}}
	},
	{ "da", LANG_DANISH, { 
		{ "DK", SUBLANG_DEFAULT },
		{ "",0}}
	},
	{ "de", LANG_GERMAN, {
		{ "DE", SUBLANG_GERMAN },
		{ "CH", SUBLANG_GERMAN_SWISS },
		{ "AT", SUBLANG_GERMAN_AUSTRIAN },
		{ "LU", SUBLANG_GERMAN_LUXEMBOURG },
		{ "LI", SUBLANG_GERMAN_LIECHTENSTEIN },
		{ "" , 0}}
	},
	{"dv",	LANG_DIVEHI, {
		{ "MV", SUBLANG_DEFAULT},
		{ "", 0}}
	},
	{"el",	LANG_GREEK, {
		{ "GR", SUBLANG_DEFAULT},
		{ "", 0}}
	},
	{ "en", LANG_ENGLISH, {
		{ "US", SUBLANG_ENGLISH_US },
		{ "GB", SUBLANG_ENGLISH_UK },
		{ "AU", SUBLANG_ENGLISH_AUS },
		{ "CA", SUBLANG_ENGLISH_CAN },
		{ "NZ", SUBLANG_ENGLISH_NZ },
		{ "IE", SUBLANG_ENGLISH_EIRE },
		{ "ZA", SUBLANG_ENGLISH_SOUTH_AFRICA },
		{ "JM", SUBLANG_ENGLISH_JAMAICA },
		{ "BZ", SUBLANG_ENGLISH_BELIZE },
		{ "TT", SUBLANG_ENGLISH_TRINIDAD },
		{ "ZW", SUBLANG_ENGLISH_ZIMBABWE },
		{ "PH", SUBLANG_ENGLISH_PHILIPPINES },
		{ "",0}}
	},
	{ "es", LANG_SPANISH, {  // XXX : SUBLANG_SPANISH_MODERN
		{ "ES", SUBLANG_SPANISH },
		{ "MX", SUBLANG_SPANISH_MEXICAN },
		{ "GT", SUBLANG_SPANISH_GUATEMALA },
		{ "CR", SUBLANG_SPANISH_COSTA_RICA },
		{ "PA", SUBLANG_SPANISH_PANAMA },
		{ "DO", SUBLANG_SPANISH_DOMINICAN_REPUBLIC },
		{ "VE", SUBLANG_SPANISH_VENEZUELA },
		{ "CO", SUBLANG_SPANISH_COLOMBIA },
		{ "PE", SUBLANG_SPANISH_PERU },
		{ "AR", SUBLANG_SPANISH_ARGENTINA },
		{ "EC", SUBLANG_SPANISH_ECUADOR },
		{ "CL", SUBLANG_SPANISH_CHILE },
		{ "UY", SUBLANG_SPANISH_URUGUAY },
		{ "PY", SUBLANG_SPANISH_PARAGUAY },
		{ "BO", SUBLANG_SPANISH_BOLIVIA },
		{ "SV", SUBLANG_SPANISH_EL_SALVADOR },
		{ "HN", SUBLANG_SPANISH_HONDURAS },
		{ "NI", SUBLANG_SPANISH_NICARAGUA },
	    { "PR", SUBLANG_SPANISH_PUERTO_RICO },
		{ "", 0 }}
	},
	{"et",	LANG_ESTONIAN, {
		{ "EE", SUBLANG_DEFAULT },
		{ "", 0}}
	},
	{"eu",	LANG_BASQUE, {
		{ "ES" , SUBLANG_DEFAULT },
		{ "" , 0 }}
	},
	{"fa",	LANG_FARSI, {
		{ "IR", SUBLANG_DEFAULT},
		{ "", 0}}
	},
	{"fi",	LANG_FINNISH, {
		{ "FI", SUBLANG_DEFAULT },
		{ "",0}}
	},
	{"fo",	LANG_FAEROESE, {
		{ "FO", SUBLANG_DEFAULT },
		{ "", 0}}
	},
	{ "fr", LANG_FRENCH, {
		{ "FR", SUBLANG_FRENCH },
		{ "BE", SUBLANG_FRENCH_BELGIAN },
		{ "CA", SUBLANG_FRENCH_CANADIAN },
		{ "CH", SUBLANG_FRENCH_SWISS },
		{ "LU", SUBLANG_FRENCH_LUXEMBOURG },
		{ "MC", SUBLANG_FRENCH_MONACO },
		{"",0}}
	},
	{ "gl", LANG_GALICIAN, {
		{ "ES", SUBLANG_DEFAULT }, 
		{ "", 0}}
	},
	{ "gu", LANG_GUJARATI, {
		{ "IN", SUBLANG_DEFAULT }, 
		{ "", 0}}
	},
	{"he",	LANG_HEBREW, {
		{ "IL", SUBLANG_DEFAULT},
		{ "", 0}}
	},
	{"hi",	LANG_HINDI, {
		{ "IN", SUBLANG_DEFAULT },
		{ "", 0}}
	},
	/* Duplicate the SUBLANG codes for Croatian and Serbian, because the Windows
	   LANG code is the same for both */
	{"hr",	LANG_CROATIAN, {
		{ "CS", SUBLANG_SERBIAN_LATIN },
		{ "SP", SUBLANG_SERBIAN_CYRILLIC },
		{ "HR", SUBLANG_DEFAULT},
		{ "" ,0 }}
	},
	{"hu",	LANG_HUNGARIAN, {
		{ "HU", SUBLANG_DEFAULT },
		{ "" , 0 }}
	},
	{"hy",	LANG_ARMENIAN, {
		{ "AM", SUBLANG_DEFAULT},
		{ "" ,0 }}
	},
	{"id",	LANG_INDONESIAN, {
		{ "ID", SUBLANG_DEFAULT },
		{"", 0}}
	},
	{"is",	LANG_ICELANDIC, {
		{ "IS", SUBLANG_DEFAULT },
		{ "", 0}}
	},
	{ "it", LANG_ITALIAN, {
		{ "IT", SUBLANG_ITALIAN },
		{ "CH", SUBLANG_ITALIAN_SWISS },
		{ "", 0}}
	},
	{"iw",	LANG_HEBREW, {
		{ "IL", SUBLANG_DEFAULT},
		{ "", 0}}
	},
	{"ja",	LANG_JAPANESE, {
		{ "JP", SUBLANG_DEFAULT },
		{ "", 0}}
	},
	{ "ka", LANG_GEORGIAN, {
		{ "GE", SUBLANG_DEFAULT },
		{ "", 0}}
	},
	{ "kk", LANG_KAZAK, {
		{ "KZ", SUBLANG_DEFAULT }, // KAZAKHSTAN
		{ "", 0}}
	},
	{ "kn", LANG_KANNADA, {
		{ "IN", SUBLANG_DEFAULT }, 
		{ "", 0}}
	},
	{ "ko", LANG_KOREAN, {
		{ "KR", SUBLANG_KOREAN },
		{ "", 0}}
	},
	{ "kok", LANG_KONKANI, {
		{ "IN", SUBLANG_DEFAULT }, 
		{ "", 0}}
	},
	{ "ky", LANG_KYRGYZ, {
		{ "KG", SUBLANG_DEFAULT }, // Kygyzstan
		{ "", 0}}
	},
	{"lt",	LANG_LITHUANIAN, {
		{ "LT", SUBLANG_DEFAULT },
		{ "" ,0 }}
	},
	{"lv",	LANG_LATVIAN, {
		{ "LV", SUBLANG_DEFAULT},
		{ "", 0}}
	},
	{"mk",	LANG_MACEDONIAN, {
		{ "MK", SUBLANG_DEFAULT },
		{ "", 0 }}
	},
	{ "mn", LANG_MONGOLIAN, {
		{ "MN", SUBLANG_DEFAULT }, 
		{ "", 0}}
	},
	{ "mr", LANG_MARATHI, {
		{ "IN", SUBLANG_DEFAULT }, 
		{ "", 0}}
	},
	{"ms",	LANG_MALAY, {
		{ "MY", SUBLANG_MALAY_MALAYSIA },
		{ "BN", SUBLANG_MALAY_BRUNEI_DARUSSALAM }, // XXX
		{ "", 0}}
	},
	{"nb",	LANG_NORWEGIAN, {
		{ "NO",  SUBLANG_NORWEGIAN_BOKMAL },
		{ "", SUBLANG_NORWEGIAN_BOKMAL}}
	},
	{"nl",	LANG_DUTCH, {
		{"NL", SUBLANG_DUTCH },
		{"BE", SUBLANG_DUTCH_BELGIAN },
		{ "", 0}}
	},
	{"nn",	LANG_NORWEGIAN, {
		{ "NO",  SUBLANG_NORWEGIAN_NYNORSK },
		{ "", SUBLANG_NORWEGIAN_NYNORSK}}
	},
	{"no",	LANG_NORWEGIAN, {
		{ "NO",  SUBLANG_DEFAULT },
		{ "", 0}}
	},
	{ "pa", LANG_PUNJABI, {
		{ "IN", SUBLANG_DEFAULT }, 
		{ "", 0}}
	},
	{"pl",	LANG_POLISH, {
		{ "PL", SUBLANG_DEFAULT },
		{ "", 0}}
	},
	{ "pt", LANG_PORTUGUESE, {
		{ "PT", SUBLANG_PORTUGUESE },
		{ "BR", SUBLANG_PORTUGUESE_BRAZILIAN },
		{"",0}}
	},
	{"ro",	LANG_ROMANIAN, {
		{ "RO", SUBLANG_DEFAULT },
		{ "", 0}}
	},
	{"ru",	LANG_RUSSIAN, {
		{ "RU", SUBLANG_DEFAULT },
		{ "", 0 }}
	},
	{ "sa", LANG_SANSKRIT, {
		{ "IN", SUBLANG_DEFAULT }, 
		{ "", 0}}
	},
	{"sk",	LANG_SLOVAK, {
		{ "SK", SUBLANG_DEFAULT },
		{ "", 0}}
	},
	{"sl",	LANG_SLOVENIAN, {
		{ "SI", SUBLANG_DEFAULT },
		{ "", 0}}
	},
	{"sq",	LANG_ALBANIAN, {
		{ "AL", SUBLANG_DEFAULT },
		{ "", 0}}
	},		
	/* Duplicate the SUBLANG codes for Croatian and Serbian, because the Windows
	   LANG code is the same for both */
	{"sr",	LANG_SERBIAN, {
		{ "CS", SUBLANG_SERBIAN_LATIN },
		{ "SP", SUBLANG_SERBIAN_CYRILLIC },
		{ "HR", SUBLANG_DEFAULT },
		{ "", 0}}
	},
	{ "sv", LANG_SWEDISH, {
		{ "SE", SUBLANG_SWEDISH },
		{ "FI", SUBLANG_SWEDISH_FINLAND },
		{ "", 0 }}
	},
	{"sw",	LANG_SWAHILI, {
		{ "KE", SUBLANG_DEFAULT },
		{ "", 0}}
	},
	{ "ta", LANG_TAMIL, {
		{ "IN", SUBLANG_DEFAULT }, 
		{ "", 0}}
	},
	{ "te", LANG_TELUGU, {
		{ "IN", SUBLANG_DEFAULT }, 
		{ "", 0}}
	},
	{"th",	LANG_THAI, {
		{"TH", SUBLANG_DEFAULT},
		{"",0}}
	},
	{"tr",	LANG_TURKISH, {
		{ "TR", SUBLANG_DEFAULT },
		{ "", 0}}
	},
	{ "tt", LANG_TATAR, {
		{ "RU", SUBLANG_DEFAULT }, 
		{ "", 0}}
	},
	{"uk",	LANG_UKRAINIAN, {
		{ "UA", SUBLANG_DEFAULT },
		{ "", 0}}
	},
	{"ur",	LANG_URDU, {
		{ "PK", SUBLANG_URDU_PAKISTAN },
		{ "IN", SUBLANG_URDU_INDIA },
		{ "", 0}}
	},
	{"uz",	LANG_UZBEK, {   // XXX : Cyrillic, Latin
		{ "UZ", SUBLANG_DEFAULT },
		{ "", 0}}
	},
	{"vi",	LANG_VIETNAMESE, {
		{ "VN", SUBLANG_DEFAULT },
		{ "", 0}}
	},
	{ "zh", LANG_CHINESE, {
		{ "TW", SUBLANG_CHINESE_TRADITIONAL },
		{ "CN", SUBLANG_CHINESE_SIMPLIFIED },
		{ "HK", SUBLANG_CHINESE_HONGKONG },
		{ "SG", SUBLANG_CHINESE_SINGAPORE },
		{ "MO", SUBLANG_CHINESE_MACAU },
		{ "",0}}
	}
};

#define LENGTH_MAPPING_LIST		ArrayLength(iso_list)
	
//
// This list maps ISO 2 digit country codes to Win32 country codes.
// This list must be kept in alphabetic (by iso code) order and synchronized
// with the list above.  This is only used in debug builds to check the consistentcy
// of the internal tables.
//
#ifdef DEBUG
static const
iso_pair dbg_list[] =
{
	{"af",	LANG_AFRIKAANS},		
	{"ar",	LANG_ARABIC},
	{"az",	LANG_AZERI},
	{"be",	LANG_BELARUSIAN},
	{"bg",	LANG_BULGARIAN},
	{"ca",	LANG_CATALAN},
	{"cs",	LANG_CZECH},
	{"da",	LANG_DANISH},
	{"de",	LANG_GERMAN},
	{"dv",  LANG_DIVEHI},
	{"el",	LANG_GREEK},
	{"en",	LANG_ENGLISH},
	{"es",	LANG_SPANISH},
	{"et",	LANG_ESTONIAN},
	{"eu",	LANG_BASQUE},
	{"fa",	LANG_FARSI},
	{"fi",	LANG_FINNISH},
	{"fo",	LANG_FAEROESE},
	{"fr",	LANG_FRENCH},
	{"gl",  LANG_GALICIAN},
	{"gu",  LANG_GUJARATI},
	{"he",	LANG_HEBREW},
	{"hi",	LANG_HINDI},
	{"hr",	LANG_CROATIAN},
	{"hu",	LANG_HUNGARIAN},
	{"hy",	LANG_ARMENIAN},
	{"id",	LANG_INDONESIAN},
	{"in",	LANG_INDONESIAN},
	{"is",	LANG_ICELANDIC},
	{"it",	LANG_ITALIAN},
	{"iw",	LANG_HEBREW},
	{"ja",	LANG_JAPANESE},
	{"ka",	LANG_GEORGIAN},
	{"kn",  LANG_KANNADA},
	{"ko",	LANG_KOREAN},
	{"kok", LANG_KONKANI},
	{"lt",	LANG_LITHUANIAN},
	{"lv",	LANG_LATVIAN},
	{"mk",	LANG_MACEDONIAN},
	{"mn",  LANG_MONGOLIAN},
	{"mr",  LANG_MARATHI},
	{"ms",	LANG_MALAY},
	{"nb",	LANG_NORWEGIAN},
	{"nl",	LANG_DUTCH},
	{"nn",	LANG_NORWEGIAN},
	{"no",	LANG_NORWEGIAN},
	{"pa",  LANG_PUNJABI},
	{"pl",	LANG_POLISH},
	{"pt",	LANG_PORTUGUESE},
	{"ro",	LANG_ROMANIAN},
	{"ru",	LANG_RUSSIAN},
	{"sa",  LANG_SANSKRIT},
	{"sk",	LANG_SLOVAK},
	{"sl",	LANG_SLOVENIAN},
	{"sq",	LANG_ALBANIAN},		
	{"sr",	LANG_SERBIAN},
	{"sv",	LANG_SWEDISH},
	{"sw",	LANG_SWAHILI},
	{"ta",  LANG_TAMIL},
	{"te",  LANG_TELUGU},
	{"th",	LANG_THAI},
	{"tr",	LANG_TURKISH},
	{"tt",  LANG_TATAR},
	{"uk",	LANG_UKRAINIAN},
	{"ur",	LANG_URDU},
	{"uz",	LANG_UZBEK},
	{"vi",	LANG_VIETNAMESE},
	{"zh",	LANG_CHINESE},
	{"",0}
};
#endif

#define CROATIAN_ISO_CODE "hr"
#define SERBIAN_ISO_CODE "sr"

//
// the mapping routines are a first approximation to get us going on
// the tier-1 languages.  we are making an assumption that we can map
// language and country codes separately on Windows, which isn't true
//
nsresult
nsWin32Locale::GetPlatformLocale(const nsAString& locale, LCID* winLCID)
{
  LCID lcid = LocaleNameToLCID(PromiseFlatString(locale).get(), 0);
  // The function returning 0 means that the locale name couldn't be matched,
  // so we fallback to the old function
  if (lcid != 0) {
    *winLCID = lcid;
    return NS_OK;
  }

  char    locale_string[9] = {'\0','\0','\0','\0','\0','\0','\0','\0','\0'};
  char*   language_code;
  char*   country_code;
  size_t  i, j;

  // parse the locale
  const char16_t* data;
  j = NS_StringGetData(locale, &data);
  for (i = 0; i < 7 && i < j; i++) {
    locale_string[i] = data[i] == '-' ? '\0' : data[i];
  }

  language_code = locale_string;
  country_code = locale_string + strlen(locale_string) + 1;

  // convert parsed locale to Windows LCID
  for(i=0;i<LENGTH_MAPPING_LIST;i++) {
    if (strcmp(language_code,iso_list[i].iso_code)==0) {
      for(j=0;iso_list[i].sublang_list[j].win_code;j++) {
        if (strcmp(country_code,iso_list[i].sublang_list[j].iso_code)==0) {
          *winLCID = MAKELCID(MAKELANGID(iso_list[i].win_code,iso_list[i].sublang_list[j].win_code),SORT_DEFAULT);
          return NS_OK;
        }
      }
      // here we have a language match but no country match
      *winLCID = MAKELCID(MAKELANGID(iso_list[i].win_code,SUBLANG_DEFAULT),SORT_DEFAULT);
      return NS_OK;
    }
  }
    
  return NS_ERROR_FAILURE;
}

void
nsWin32Locale::GetXPLocale(LCID winLCID, nsAString& locale)
{
  locale.SetCapacity(LOCALE_NAME_MAX_LENGTH);
  int length = LCIDToLocaleName(winLCID,
                                reinterpret_cast<LPWSTR>(locale.BeginWriting()),
                                LOCALE_NAME_MAX_LENGTH, 0);
  // 0 length means that the function failed to match up the LCID,
  // so we fallback to the old function
  if (length) {
    // length contains null terminate.
    locale.SetLength(length - 1);
    return;
  }

  DWORD    lang_id, sublang_id;
  size_t   i, j;

  lang_id = PRIMARYLANGID(LANGIDFROMLCID(winLCID));
  sublang_id = SUBLANGID(LANGIDFROMLCID(winLCID));

  /* Special-case Norwegian Bokmal and Norwegian Nynorsk, which have the same
     LANG_ID on Windows, but have separate ISO-639-2 codes */
  if (lang_id == LANG_NORWEGIAN) {
    if (sublang_id == SUBLANG_NORWEGIAN_BOKMAL) {
      locale.AssignASCII("nb-NO");
    } else if (sublang_id == SUBLANG_NORWEGIAN_NYNORSK) {
      locale.AssignASCII("nn-NO");
    } else {
      locale.AssignASCII("no-NO");
    }
    return;
  }

  for(i=0;i<LENGTH_MAPPING_LIST;i++) {
    if (lang_id==iso_list[i].win_code) {
      /* Special-case Croatian and Serbian, which have the same LANG_ID on
         Windows, but have been split into separate ISO-639-2 codes */
      if (lang_id == LANG_CROATIAN) {
        if (sublang_id == SUBLANG_DEFAULT) {
          locale.AssignLiteral(CROATIAN_ISO_CODE);
        } else {
          locale.AssignLiteral(SERBIAN_ISO_CODE);
        }
      } else {
        locale.AssignASCII(iso_list[i].iso_code);
      }
      for(j=0;iso_list[i].sublang_list[j].win_code;j++) {
        if (sublang_id == iso_list[i].sublang_list[j].win_code) {
          locale.Append(char16_t('-'));
          locale.AppendASCII(iso_list[i].sublang_list[j].iso_code);
          break;
        }
      }
      return;
    }
  }

  //
  // didn't find any match. fall back to en-US, which is better 
  // than unusable buttons without 'OK', 'Cancel', etc (bug 224546)       
  //
  locale.AssignLiteral("en-US"); 
  return;
}

#ifdef DEBUG
void
test_internal_tables(void)
{
	size_t i;

	for(i=1;i<LENGTH_MAPPING_LIST;i++) {
		if (strcmp(dbg_list[i-1].iso_code,dbg_list[i].iso_code)>=0)
			fprintf(stderr,"nsLocale: language_list %s and %s are not ordered\n",dbg_list[i-1].iso_code,dbg_list[i].iso_code);
	}

	i=0;
	while(strlen(dbg_list[i].iso_code)!=0) {
		i++;
	}
	if (i!=LENGTH_MAPPING_LIST)
		fprintf(stderr,"nsLocale: language_list length is %u, reported length is %u\n",
		        unsigned(i), unsigned(LENGTH_MAPPING_LIST));

	for(i=0;i<LENGTH_MAPPING_LIST;i++) {
		if (strcmp(iso_list[i].iso_code,dbg_list[i].iso_code)!=0) {
			fprintf(stderr,"nsLocale: iso_list and dbg_list different at item: %u\n",
			        unsigned(i));
		}
	}
}

#endif

