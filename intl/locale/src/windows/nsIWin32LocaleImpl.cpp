/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsISupports.h"
#include "nscore.h"
#include "nsString.h"
#include "nsILocale.h"
#include "nsIWin32LocaleImpl.h"
#include "nsLocaleCID.h"
#include "prprf.h"

NS_DEFINE_IID(kIWin32LocaleIID, NS_IWIN32LOCALE_IID);
NS_DEFINE_IID(kIWin32LocaleImplCID, NS_WIN32LOCALE_CID);

struct iso_lang_mapping
{
	char*	iso_code;
	DWORD	lang_code;
};
typedef struct iso_lang_mapping iso_lang_mapping;

struct iso_country_mapping
{
	char*	iso_code;
	DWORD	country_code;
};
typedef struct iso_country_mapping iso_country_mapping;

//
// ISO <--> Windows 32 Mappings from www.unicode.org
//
iso_lang_mapping language_list[50] =
{
	{"af",	0x0036},
	{"sq",	0x001c},
	{"ar",	0x0001},
	{"eu",	0x002d},
	{"bg",	0x0002},
	{"be",	0x0023},
	{"ca",	0x0003},
	{"zh",	0x0004},
	{"hr",	0x001a},
	{"cs",	0x0005},
	{"da",	0x0006},
	{"nl",	0x0013},
	{"en",	0x0009},
	{"et",	0x0025},
	{"fo",	0x0038},
	{"fa",	0x0029},
	{"fi",	0x000b},
	{"de",	0x0007},
	{"el",	0x0008},
	{"iw",	0x000d},
	{"he",	0x000d},
	{"hi",	0x0039},
	{"hu",	0x000e},
	{"is",	0x000f},
	{"in",	0x0021},
	{"id",	0x0021},
	{"it",	0x0010},
	{"ja",	0x0011},
	{"ko",	0x0012},
	{"lv",	0x0026},
	{"lt",	0x0027},
	{"mk",	0x002f},
	{"ms",	0x003e},
	{"no",	0x0014},
	{"pl",	0x0015},
	{"pt",	0x0016},
	{"ro",	0x0018},
	{"ru",	0x0019},
	{"sr",	0x001a},
	{"sk",	0x001b},
	{"sl",	0x0024},
	{"es",	0x000a},
	{"sw",	0x0041},
	{"sv",	0x001d},
	{"th",	0x001e},
	{"tr",	0x001f},
	{"uk",	0x0022},
	{"ur",	0x0020},
	{"vi",	0x002a}
};

iso_country_mapping country_list[94] =
{
	{"AL",0x0400},
	{"DZ",0x1400},
	{"AR",0x2c00},
	{"AU",0x0c00},
	{"AT",0x0c00},
	{"BH",0x3c00},
	{"BY",0x0400},
	{"BE",0x0800},
	{"BZ",0x2800},
	{"BO",0x4000},
	{"BR",0x0400},
	{"BN",0x0800},
	{"BG",0x0400},
	{"CA",0x1000},
	{"CL",0x3400},
	{"CN",0x0800},
	{"CO",0x2400},
	{"CR",0x1400},
	{"CZ",0x0400},
	{"DK",0x0400},
	{"DO",0x1c00},
	{"EC",0x3000},
	{"EG",0x0c00},
	{"SV",0x4400},
	{"EE",0x0400},
	{"FO",0x0400},
	{"FI",0x0400},
	{"DE",0x0400},
	{"GR",0x0400},
	{"GT",0x1000},
	{"HN",0x4800},
	{"HK",0x0c00},
	{"HU",0x0400},
	{"IS",0x0400},
	{"IN",0x0400},
	{"ID",0x0400},
	{"IR",0x0400},
	{"IQ",0x0800},
	{"IE",0x1800},
	{"IL",0x0400},
	{"IT",0x0400},
	{"JM",0x2000},
	{"JP",0x0400},
	{"JO",0x2c00},
	{"KE",0x0400},
	{"KR",0x0400},
	{"KW",0x3400},
	{"LV",0x0400},
	{"LB",0x3000},
	{"LY",0x1000},
	{"LI",0x1400},
	{"LT",0x0400},
	{"LU",0x1000},
	{"MO",0x1400},
	{"MK",0x0400},
	{"MY",0x0400},
	{"MX",0x0800},
	{"MC",0x1800},
	{"MA",0x1800},
	{"NL",0x0400},
	{"NZ",0x1400},
	{"NI",0x4c00},
	{"NO",0x0400},
	{"OM",0x2000},
	{"PK",0x0400},
	{"PA",0x1800},
	{"PY",0x3c00},
	{"PE",0x2800},
	{"PH",0x3400},
	{"PL",0x0400},
	{"PT",0x0800},
	{"PR",0x5000},
	{"QA",0x4000},
	{"RO",0x0400},
	{"RU",0x0400},
	{"SA",0x0400},
	{"SG",0x1000},
	{"SK",0x0400},
	{"SI",0x0400},
	{"ZA",0x0400},
	{"ES",0x0400},
	{"TH",0x0400},
	{"TT",0x2c00},
	{"TN",0x1c00},
	{"TR",0x0400},
	{"UA",0x0400},
	{"AE",0x3800},
	{"GB",0x0800},
	{"US",0x0400},
	{"UY",0x3800},
	{"VE",0x2000},
	{"VN",0x2400},
	{"YE",0x2400},
	{"ZW",0x3000}
};



/* nsIWin32LocaleImpl */
NS_IMPL_ISUPPORTS(nsIWin32LocaleImpl,kIWin32LocaleIID)

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
	int			i;

	locale_string = locale->ToNewCString();
	if (locale_string!=NULL)
	{
		PR_sscanf("%s2-%s2-%s",language_code,country_code);
		for(i=0;i<50;i++)
		{
			if(strcmp(language_code,language_list[i].iso_code))
			{

				*winLCID = MAKELCID(language_list[i].lang_code,SORT_DEFAULT);
				delete[] locale_string;
				return NS_OK;
			}
		}
		delete[] locale_string;
		return NS_ERROR_FAILURE;
	}

	return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsIWin32LocaleImpl::GetXPLocale(LCID winLCID, nsString* locale)
{
	LCID		lang_id;
	char		language_code[3];
	char		country_code[3];
	char		rfc_locale_string[9];
	int			i;
	

	language_code[0] = 0;
	country_code[0] = 0;

	lang_id = LANGIDFROMLCID(winLCID);
	for(i=0;i<50;i++)
	{
		if (lang_id==language_list[i].lang_code)
		{
			strcpy(language_code,language_list[i].iso_code);
		}
	}

	for(i=0;i<94;i++)
	{
		if (winLCID==country_list[i].country_code)
		{
			strcpy(country_code,country_list[i].iso_code);
		}
	}

	PR_snprintf(rfc_locale_string,9,"%s-%s",language_code,country_code);


	return NS_OK;
}
