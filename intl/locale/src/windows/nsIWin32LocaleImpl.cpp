/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsISupports.h"
#include "nscore.h"
#include "nsString.h"
#include "nsILocale.h"
#include "nsIWin32LocaleImpl.h"
#include "nsLocaleCID.h"
#include "prprf.h"
#include <Windows.h>

NS_DEFINE_IID(kIWin32LocaleImplCID, NS_WIN32LOCALE_CID);

#define USER_DEFINED_PRIMARYLANG	0x0200
#define USER_DEFINED_SUBLANGUAGE	0x20

#define LENGTH_MAPPING_LIST		50

struct iso_pair 
{
	char*	iso_code;
	DWORD	win_code;
};
typedef struct iso_pair iso_pair;

struct iso_map
{
	char*				iso_code;
	DWORD				win_code;
	iso_pair			sublang_list[20];
};
typedef struct iso_map iso_map;


//
// Additional ISO <--> Windows 32 Language values from http://www.unicode.org
//
#define MOZ_LANG_HINDI			0x39
#define MOZ_LANG_MACEDONIAN		0x2f
#define MOZ_LANG_MALAY			0x3e
#define MOZ_LANG_SWAHILI		0x41
#define MOZ_LANG_URDU			0x20

//
// This list is used to map between ISO language
iso_map iso_list[LENGTH_MAPPING_LIST] =
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
		{ "",0}}
	},
	{ "es", LANG_SPANISH, {
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
		{"",0}}
	},
	{"he",	LANG_HEBREW, {
		{ "IL", SUBLANG_DEFAULT},
		{ "", 0}}
	},
	{"hi",	MOZ_LANG_HINDI, {
		{ "IN", SUBLANG_DEFAULT },
		{ "", 0}}
	},
	{"hr",	LANG_CROATIAN, {
		{ "HR", SUBLANG_DEFAULT},
		{ "" ,0 }}
	},
	{"hu",	LANG_HUNGARIAN, {
		{ "HU", SUBLANG_DEFAULT },
		{ "" , 0 }}
	},
	{"id",	LANG_INDONESIAN, {
		{ "ID", SUBLANG_DEFAULT },
		{"", 0}}
	},
	{"in",	LANG_INDONESIAN, {
		{ "ID", SUBLANG_DEFAULT },
		{ "", 0}}
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
	{ "ko", LANG_KOREAN, {
		{ "KO", SUBLANG_KOREAN },
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
	{"mk",	MOZ_LANG_MACEDONIAN, {
		{ "MK", SUBLANG_DEFAULT },
		{ "", 0 }}
	},
	{"ms",	MOZ_LANG_MALAY, {
		{ "MY", SUBLANG_DEFAULT },
		{ "", 0}}
	},
	{"nl",	LANG_DUTCH, {
		{"NL", SUBLANG_DUTCH },
		{"BE", SUBLANG_DUTCH_BELGIAN },
		{ "", 0}}
	},
	{"no",	LANG_NORWEGIAN, {
		{ "NO",  SUBLANG_DEFAULT },
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
	{"sr",	LANG_SERBIAN, {
		{ "YU", SUBLANG_DEFAULT },
		{ "", 0}}
	},
	{ "sv", LANG_SWEDISH, {
		{ "SE", SUBLANG_SWEDISH },
		{ "FI", SUBLANG_SWEDISH_FINLAND },
		{ "", 0 }}
	},
	{"sw",	MOZ_LANG_SWAHILI, {
		{ "KE", SUBLANG_DEFAULT },
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
	{"uk",	LANG_UKRAINIAN, {
		{ "UA", SUBLANG_DEFAULT },
		{ "", 0}}
	},
	{"ur",	MOZ_LANG_URDU, {
		{ "IN", SUBLANG_DEFAULT },
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
		{ "",0}}
	}
};
	
//
// This list maps ISO 2 digit country codes to Win32 country codes.
// This list must be kept in alphabetic (by iso code) order and synchronized
// with the list above.  This is only used in debug builds to check the consistentcy
// of the internal tables.
//
#ifdef DEBUG
iso_pair dbg_list[LENGTH_MAPPING_LIST+1] =
{
	{"af",	LANG_AFRIKAANS},		
	{"ar",	LANG_ARABIC},
	{"be",	LANG_BELARUSIAN},
	{"bg",	LANG_BULGARIAN},
	{"ca",	LANG_CATALAN},
	{"cs",	LANG_CZECH},
	{"da",	LANG_DANISH},
	{"de",	LANG_GERMAN},
	{"el",	LANG_GREEK},
	{"en",	LANG_ENGLISH},
	{"es",	LANG_SPANISH},
	{"et",	LANG_ESTONIAN},
	{"eu",	LANG_BASQUE},
	{"fa",	LANG_FARSI},
	{"fi",	LANG_FINNISH},
	{"fo",	LANG_FAEROESE},
	{"fr",	LANG_FRENCH},
	{"he",	LANG_HEBREW},
	{"hi",	MOZ_LANG_HINDI},
	{"hr",	LANG_CROATIAN},
	{"hu",	LANG_HUNGARIAN},
	{"id",	LANG_INDONESIAN},
	{"in",	LANG_INDONESIAN},
	{"is",	LANG_ICELANDIC},
	{"it",	LANG_ITALIAN},
	{"iw",	LANG_HEBREW},
	{"ja",	LANG_JAPANESE},
	{"ko",	LANG_KOREAN},
	{"lt",	LANG_LITHUANIAN},
	{"lv",	LANG_LATVIAN},
	{"mk",	MOZ_LANG_MACEDONIAN},
	{"ms",	MOZ_LANG_MALAY},
	{"nl",	LANG_DUTCH},
	{"no",	LANG_NORWEGIAN},
	{"pl",	LANG_POLISH},
	{"pt",	LANG_PORTUGUESE},
	{"ro",	LANG_ROMANIAN},
	{"ru",	LANG_RUSSIAN},
	{"sk",	LANG_SLOVAK},
	{"sl",	LANG_SLOVENIAN},
	{"sq",	LANG_ALBANIAN},		
	{"sr",	LANG_SERBIAN},
	{"sv",	LANG_SWEDISH},
	{"sw",	MOZ_LANG_SWAHILI},
	{"th",	LANG_THAI},
	{"tr",	LANG_TURKISH},
	{"uk",	LANG_UKRAINIAN},
	{"ur",	MOZ_LANG_URDU},
	{"vi",	LANG_VIETNAMESE},
	{"zh",	LANG_CHINESE},
	{"",0}
};
#endif

/* nsIWin32LocaleImpl */
NS_IMPL_ISUPPORTS1(nsIWin32LocaleImpl,nsIWin32Locale)

nsIWin32LocaleImpl::nsIWin32LocaleImpl(void)
{
	NS_INIT_REFCNT();
}

nsIWin32LocaleImpl::~nsIWin32LocaleImpl(void)
{

}

//
// the mapping routines are a first approximation to get us going on
// the tier-1 languages.  we are making an assumption that we can map
// language and country codes separately on Windows, which isn't true
//

NS_IMETHODIMP
nsIWin32LocaleImpl::GetPlatformLocale(const nsString* locale,LCID* winLCID)
{
	char*		locale_string;
	char		language_code[3];
	char		country_code[3];
	char		region_code[3];
	int			i,j;

	locale_string = locale->ToNewCString();
	if (locale_string!=NULL)
	{	
		if (!ParseLocaleString(locale_string,language_code,country_code,region_code)) {
			*winLCID = MAKELCID(MAKELANGID(USER_DEFINED_PRIMARYLANG,USER_DEFINED_SUBLANGUAGE),
				SORT_DEFAULT);
			free(locale_string);
			return NS_OK;
		}
		// we have a LL-CC-RR style string

		for(i=0;i<LENGTH_MAPPING_LIST;i++) {
			if (strcmp(language_code,iso_list[i].iso_code)==0) {
				for(j=0;strlen(iso_list[i].sublang_list[j].iso_code)!=0;j++) {
					if (strcmp(country_code,iso_list[i].sublang_list[j].iso_code)==0) {
						free(locale_string);
						*winLCID = MAKELCID(MAKELANGID(iso_list[i].win_code,iso_list[i].sublang_list[j].win_code),SORT_DEFAULT);
						return NS_OK;
					}
				}
				// here we have a language match but no country match
				free(locale_string);
				*winLCID = MAKELCID(MAKELANGID(iso_list[i].win_code,SUBLANG_DEFAULT),SORT_DEFAULT);
				return NS_OK;
			}
		}
	}
		
	return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsIWin32LocaleImpl::GetXPLocale(LCID winLCID, nsString* locale)
{
	DWORD		lang_id, sublang_id;
	char		rfc_locale_string[9];
	int			i,j;

	lang_id = PRIMARYLANGID(LANGIDFROMLCID(winLCID));
	sublang_id = SUBLANGID(LANGIDFROMLCID(winLCID));

	for(i=0;i<LENGTH_MAPPING_LIST;i++) {
		if (lang_id==iso_list[i].win_code) {
			for(j=0;strlen(iso_list[i].sublang_list[j].iso_code)!=0;j++) {
				if (sublang_id == iso_list[i].sublang_list[j].win_code) {
					PR_snprintf(rfc_locale_string,9,"%s-%s%c",iso_list[i].iso_code,
						iso_list[i].sublang_list[j].iso_code,0);
					locale->AssignWithConversion(rfc_locale_string);
					return NS_OK;
				}
			}
			// no sublang, so just lang
			PR_snprintf(rfc_locale_string,9,"%s%c",iso_list[i].iso_code,0);
			locale->AssignWithConversion(rfc_locale_string);
			return NS_OK;
		}
	}

	//
	// totally didn't find it
	//
	return NS_ERROR_FAILURE;

}

//
// returns PR_FALSE/PR_TRUE depending on if it was of the form LL-CC-RR
PRBool
nsIWin32LocaleImpl::ParseLocaleString(const char* locale_string, char* language, char* country, char* region)
{
	size_t		len;

	len = strlen(locale_string);
	if (len==0 || (len!=2 && len!=5 && len!=8))
		return PR_FALSE;
	
	if (len==2) {
		language[0]=locale_string[0];
		language[1]=locale_string[1];
		language[2]=0;
		country[0]=0;
		region[0]=0;
	} else if (len==5) {
		language[0]=locale_string[0];
		language[1]=locale_string[1];
		language[2]=0;
		country[0]=locale_string[3];
		country[1]=locale_string[4];
		country[2]=0;
		region[0]=0;
		if (locale_string[2]!='-') return PR_FALSE;
	} else if (len==8) {
		language[0]=locale_string[0];
		language[1]=locale_string[1];
		language[2]=0;
		country[0]=locale_string[3];
		country[1]=locale_string[4];
		country[2]=0;
		region[0]=locale_string[6];
		region[1]=locale_string[7];
		region[2]=0;
		if (locale_string[2]!='-' || locale_string[5]!='-') return PR_FALSE;
	} else {
		return PR_FALSE;
	}

	return PR_TRUE;
}


#ifdef DEBUG
void
test_internal_tables(void)
{
	int	i;

	for(i=1;i<LENGTH_MAPPING_LIST;i++) {
		if (strcmp(dbg_list[i-1].iso_code,dbg_list[i].iso_code)>=0)
			fprintf(stderr,"nsLocale: language_list %s and %s are not ordered\n",dbg_list[i-1].iso_code,dbg_list[i].iso_code);
	}

	i=0;
	while(strlen(dbg_list[i].iso_code)!=0) {
		i++;
	}
	if (i!=LENGTH_MAPPING_LIST)
		fprintf(stderr,"nsLocale: language_list length is %d, reported length is %d\n",i,LENGTH_MAPPING_LIST);

	for(i=0;i<LENGTH_MAPPING_LIST;i++) {
		if (strcmp(iso_list[i].iso_code,dbg_list[i].iso_code)!=0) {
			fprintf(stderr,"nsLocale: iso_list and dbg_list differet at item: %d\n",i);
		}
	}
}

#endif

