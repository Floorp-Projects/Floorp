/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "intlpriv.h"


/* Mapping table between platform code and ISO code.
 */
typedef struct
{
	char			iso_code[6];
	unsigned short	win_langid;
	unsigned short	mac_lang;
	unsigned short	mac_region;
} ISO_Lang_Country_To_Platform;

static ISO_Lang_Country_To_Platform iso_mapping_table[] = {
	{"af-ZA", 0x0436, 32767, 32767},
	{"ar-AE", 0x3801, 32767, 32767},
	{"ar-BH", 0x3c01, 32767, 32767},
	{"ar-DZ", 0x1401, 32767, 32767},
	{"ar-EG", 0x0c01, 32767, 32767},
	{"ar-IQ", 0x0801, 32767, 32767},
	{"ar-JO", 0x2c01, 32767, 32767},
	{"ar-KW", 0x3401, 32767, 32767},
	{"ar-LB", 0x3001, 32767, 32767},
	{"ar-LY", 0x1001, 32767, 32767},
	{"ar-MA", 0x1801, 32767, 32767},
	{"ar-OM", 0x2001, 32767, 32767},
	{"ar-QA", 0x4001, 32767, 32767},
	{"ar-SA", 0x0401, 32767, 32767},
	{"ar-SY", 0x2801, 32767, 32767},
	{"ar-TN", 0x1c01, 32767, 32767},
	{"ar-YE", 0x2401, 32767, 32767},
	{"az-AZ", 0x042c, 32767, 32767},
	{"az-AZ", 0x082c, 32767, 32767},
	{"be-BY", 0x0423, 32767, 32767},
	{"bg-BG", 0x0402, 32767, 32767},
	{"ca-ES", 0x0403, 32767, 32767},
	{"cs-CZ", 0x0405, 32767, 32767},
	{"da-DK", 0x0406, 32767, 32767},
	{"de-AT", 0x0c07, 32767, 32767},
	{"de-CH", 0x0807, 32767, 32767},
	{"de-DE", 0x0407, 32767, 32767},
	{"de-LI", 0x1407, 32767, 32767},
	{"de-LU", 0x1007, 32767, 32767},
	{"el-GR", 0x0408, 32767, 32767},
	{"en-AU", 0x0c09, 32767, 32767},
	{"en-BZ", 0x2809, 32767, 32767},
	{"en-CA", 0x1009, 32767, 32767},
	{"en-CB", 0x2409, 32767, 32767},
	{"en-GB", 0x0809, 32767, 32767},
	{"en-IE", 0x1809, 32767, 32767},
	{"en-JM", 0x2009, 32767, 32767},
	{"en-NZ", 0x1409, 32767, 32767},
	{"en-PH", 0x3409, 32767, 32767},
	{"en-TT", 0x2c09, 32767, 32767},
	{"en-US", 0x0409, 0, 0},
	{"en-ZA", 0x1c09, 32767, 32767},
	{"en-ZW", 0x3009, 32767, 32767},
	{"es-AR", 0x2c0a, 32767, 32767},
	{"es-BO", 0x400a, 32767, 32767},
	{"es-CL", 0x340a, 32767, 32767},
	{"es-CO", 0x240a, 32767, 32767},
	{"es-CR", 0x140a, 32767, 32767},
	{"es-DO", 0x1c0a, 32767, 32767},
	{"es-EC", 0x300a, 32767, 32767},
	{"es-ES", 0x040a, 32767, 32767},
	{"es-ES", 0x0c0a, 32767, 32767},
	{"es-GT", 0x100a, 32767, 32767},
	{"es-HN", 0x480a, 32767, 32767},
	{"es-MX", 0x080a, 32767, 32767},
	{"es-NI", 0x4c0a, 32767, 32767},
	{"es-PA", 0x180a, 32767, 32767},
	{"es-PE", 0x280a, 32767, 32767},
	{"es-PR", 0x500a, 32767, 32767},
	{"es-PY", 0x3c0a, 32767, 32767},
	{"es-SV", 0x440a, 32767, 32767},
	{"es-UY", 0x380a, 32767, 32767},
	{"es-VE", 0x200a, 32767, 32767},
	{"et-EE", 0x0425, 32767, 32767},
	{"eu-ES", 0x042d, 32767, 32767},
	{"fa-IR", 0x0429, 32767, 32767},
	{"fi-FI", 0x040b, 32767, 32767},
	{"fo-FO", 0x0438, 32767, 32767},
	{"fr-BE", 0x080c, 32767, 32767},
	{"fr-CA", 0x0c0c, 32767, 32767},
	{"fr-CH", 0x100c, 32767, 32767},
	{"fr-FR", 0x040c, 32767, 32767},
	{"fr-LU", 0x140c, 32767, 32767},
	{"fr-MC", 0x180c, 32767, 32767},
	{"hr-HR", 0x041a, 32767, 32767},
	{"hu-HU", 0x040e, 32767, 32767},
	{"id-ID", 0x0421, 32767, 32767},
	{"is-IS", 0x040f, 32767, 32767},
	{"it-CH", 0x0810, 32767, 32767},
	{"it-IT", 0x0410, 32767, 32767},
	{"iw-IL", 0x040d, 32767, 32767},
	{"ja-JP", 0x0411, 32767, 32767},
	{"kk-KZ", 0x043f, 32767, 32767},
	{"ko-KR", 0x0412, 32767, 32767},
	{"lt-LT", 0x0427, 32767, 32767},
	{"lt-LT", 0x0827, 32767, 32767},
	{"lv-LV", 0x0426, 32767, 32767},
	{"mk-MK", 0x042f, 32767, 32767},
	{"ms-BN", 0x083e, 32767, 32767},
	{"ms-MY", 0x043e, 32767, 32767},
	{"nl-BE", 0x0813, 32767, 32767},
	{"nl-NL", 0x0413, 32767, 32767},
	{"no-NO", 0x0414, 32767, 32767},
	{"no-NO", 0x0814, 32767, 32767},
	{"pl-PL", 0x0415, 32767, 32767},
	{"pt-BR", 0x0416, 32767, 32767},
	{"pt-PT", 0x0816, 32767, 32767},
	{"ro-RO", 0x0418, 32767, 32767},
	{"ru-RU", 0x0419, 32767, 32767},
	{"sk-SK", 0x041b, 32767, 32767},
	{"sl-SI", 0x0424, 32767, 32767},
	{"sq-AL", 0x041c, 32767, 32767},
	{"sr-SP", 0x081a, 32767, 32767},
	{"sr-SP", 0x0c1a, 32767, 32767},
	{"sv-FI", 0x081d, 32767, 32767},
	{"sv-SE", 0x041d, 32767, 32767},
	{"sw-KE", 0x0441, 32767, 32767},
	{"th-TH", 0x041e, 32767, 32767},
	{"tr-TR", 0x041f, 32767, 32767},
	{"tt-TA", 0x0444, 32767, 32767},
	{"uk-UA", 0x0422, 32767, 32767},
	{"ur-PK", 0x0420, 32767, 32767},
	{"uz-UZ", 0x0443, 32767, 32767},
	{"uz-UZ", 0x0843, 32767, 32767},
	{"vi-VN", 0x042a, 32767, 32767},
	{"zh-CN", 0x0804, 32767, 32767},
	{"zh-HK", 0x0c04, 32767, 32767},
	{"zh-MO", 0x1404, 32767, 32767},
	{"zh-SG", 0x1004, 32767, 32767},
	{"zh-TW", 0x0404, 32767, 32767},
	{"", 0, 0, 0}
};



static XP_Bool MatchPlatformId(ISO_Lang_Country_To_Platform *entry, XP_Bool bLanguage, 
							   unsigned short platformIdNum, char *platformIdStr)
{
#if defined(XP_WIN)
	return platformIdNum == entry->win_langid;
#elif defined(XP_MAC)
	return bLanguage ? 	(platformIdNum == entry->mac_lang) : (platformIdNum == entry->mac_region);
#elif defined(XP_UNIX)
	/* implement XFE code here */
#endif
	XP_ASSERT(0);	/* platform not supported */
	return FALSE;
}

static XP_Bool MatchISOCode(ISO_Lang_Country_To_Platform *entry, char *ISOCode)
{
	return (XP_STRCMP(entry->iso_code, ISOCode) == 0);
}

PUBLIC const char *INTL_PlatformIdToISOCode(unsigned short platformIdNum, char *platformIdStr, XP_Bool bLanguage) 
{
	ISO_Lang_Country_To_Platform *tmpPtr = iso_mapping_table;


	while (tmpPtr->iso_code[0])
	{
		if (MatchPlatformId(tmpPtr, bLanguage, platformIdNum, platformIdStr))
			break;
		tmpPtr++;
	}
#ifdef DEBUG
#ifdef XP_WIN
	/* Not found in the table, this is usually ok as EnumSystemLocales() passes unavailable langid 
	 * even called with LCID_INSTALLED. 
	 * For debugging, we verify by calling GetLocaleInfo(). If that API works then it means the table is missing the entry.
	 */
	if(!tmpPtr->iso_code[0])
	{
		char locale_string[128];

		XP_ASSERT( GetLocaleInfo(platformIdNum, LOCALE_SENGLANGUAGE, 
							locale_string, sizeof(locale_string)) == 0 );
	}
#else
	XP_ASSERT(tmpPtr->iso_code[0]);
#endif
#endif	/* DEBUG */

	return tmpPtr->iso_code;
  }

PUBLIC char *INTL_GetLanguageCountry(INTL_LanguageCountry_Selector selector)
{
/* return constants until FE implemented */
#if defined(XP_WIN)
	return FE_GetLanguageCountry(selector);
#else
	switch (selector)
	{
	case INTL_LanguageSel:
	case INTL_LanguageCollateSel:
	case INTL_LanguageMonetarySel:
	case INTL_LanguageNumericSel:
	case INTL_LanguageTimeSel:
		return XP_STRDUP("en");
	case INTL_CountrySel:
	case INTL_CountryCollateSel:
	case INTL_CountryMonetarySel:
	case INTL_CountryNumericSel:
	case INTL_CountryTimeSel:
		return XP_STRDUP("US");
	case INTL_ALL_LocalesSel:
		return XP_STRDUP("en-US,en-GB,fr-FR");
	default:
		return XP_STRDUP("");
	}
#endif
}

/* FE implementations, for now put them here for potability. */
#if defined(XP_WIN)
/* Definitions of states to check if API supports ISO code. */
typedef enum 
{
	kAPI_ISONAME_NotInitialzied,
	kAPI_ISONAME_Supported,
	kAPI_ISONAME_NotSupported
} STATE_API_Support_ISONAME;

static STATE_API_Support_ISONAME bAPI_Support_ISONAME = kAPI_ISONAME_NotInitialzied;
static BOOL API_Support_ISONAME()
{
	char locale_string[128];

	return GetLocaleInfo(GetUserDefaultLCID(), LOCALE_SISO639LANGNAME, 
						locale_string, sizeof(locale_string)) != 0;
}

/* Variables and callbacks for construct a string of all installed locales. */
static char *locale_buf = NULL;
static int locale_num =0;
BOOL CALLBACK EnumLocalesProc0(LPTSTR lpLocaleString)
{
 	char locale_string[128];
	char *endptr;
	LCID lcid = (LCID) strtol(lpLocaleString, &endptr, 16);	/* the argument is in hex string */

	if (GetLocaleInfo(lcid, LOCALE_SENGLANGUAGE, locale_string, sizeof(locale_string)))
		locale_num++;
	
	return TRUE;
}
BOOL CALLBACK EnumLocalesProc(LPTSTR lpLocaleString)
{
 	char locale_string[128];
	char *endptr;
	LCID lcid = (LCID) strtol(lpLocaleString, &endptr, 16);	/* the argument is in hex string */

	if (bAPI_Support_ISONAME == kAPI_ISONAME_Supported)
	{
		if (GetLocaleInfo(lcid, LOCALE_SISO639LANGNAME, locale_string, sizeof(locale_string)))
		{
			XP_STRCAT(locale_buf, locale_string);
 			XP_STRCAT(locale_buf, "-"); 
			if (GetLocaleInfo(lcid, LOCALE_SISO3166CTRYNAME, locale_string, sizeof(locale_string)))
			{
				XP_STRCAT(locale_buf, locale_string); 
				XP_STRCAT(locale_buf, ",");
			}
		}
	}
	else
	{
		const char *iso_code = INTL_PlatformIdToISOCode((unsigned short) LANGIDFROMLCID(lcid), NULL, TRUE);
		if (iso_code && iso_code[0])
		{
			XP_STRCAT(locale_buf, iso_code);
			XP_STRCAT(locale_buf, ",");
		}
	}
	
	return TRUE;
}

static const char *GetAllLocales(void)
{
	int len;
	
	/* It it possible to re-use the buffer if NT4 or earlier since locales cannot be added 
	 * while the Navigator is running. 
	 */
	XP_FREEIF(locale_buf);
	locale_num = 0;

	/* get length and allocate a buffer */
	(void) EnumSystemLocales(EnumLocalesProc0, LCID_INSTALLED);
	locale_buf = (char *) XP_ALLOC(locale_num * 6 + 1);
	if (locale_buf == NULL)
		return "";
	locale_buf[0] = '\0';
	/* copy locale info to the buffer */
	(void) EnumSystemLocales(EnumLocalesProc, LCID_INSTALLED);
	/* trim the last comma */
	len = XP_STRLEN(locale_buf);
	if (len >= 1)
		locale_buf[len-1] = '\0';

	return (const char *) locale_buf;
}

char *FE_GetLanguageCountry(INTL_LanguageCountry_Selector selector)
{
 	char *name = NULL;
 	char locale_string[128];
	LCID lcid = GetUserDefaultLCID();

	/* check if the API supports the ISO code output */
	if (bAPI_Support_ISONAME == kAPI_ISONAME_NotInitialzied)
		bAPI_Support_ISONAME = API_Support_ISONAME() ? kAPI_ISONAME_Supported : kAPI_ISONAME_NotSupported;

	switch (selector)
	{
	case INTL_LanguageSel:
	case INTL_LanguageCollateSel:
	case INTL_LanguageMonetarySel:
	case INTL_LanguageNumericSel:
	case INTL_LanguageTimeSel:
		if (bAPI_Support_ISONAME == kAPI_ISONAME_Supported)
		{
			if (GetLocaleInfo(lcid, LOCALE_SISO639LANGNAME, locale_string, sizeof(locale_string)) > 0)
				name = XP_STRDUP(locale_string);
		}
		else
		{
			name = (char *) INTL_PlatformIdToISOCode((unsigned short) LANGIDFROMLCID(lcid), NULL, TRUE);
			XP_ASSERT(name && XP_STRLEN(name)==5);
			name = XP_STRDUP(XP_STRNCPY_SAFE(locale_string, name, 3));	/* copy language */
		}
		break;
	case INTL_CountrySel:
	case INTL_CountryCollateSel:
	case INTL_CountryMonetarySel:
	case INTL_CountryNumericSel:
	case INTL_CountryTimeSel:
		if (bAPI_Support_ISONAME == kAPI_ISONAME_Supported)
		{
			if (GetLocaleInfo(lcid, LOCALE_SISO3166CTRYNAME, locale_string, sizeof(locale_string)) > 0)
				name = XP_STRDUP(locale_string);
		}
		else
		{
			name = (char *) INTL_PlatformIdToISOCode((unsigned short) LANGIDFROMLCID(lcid), NULL, FALSE);
			XP_ASSERT(name && XP_STRLEN(name)==5);
			name = XP_STRDUP(XP_STRNCPY_SAFE(locale_string, &name[3], 3));	/* copy country */
		}
		break;
	case INTL_ALL_LocalesSel:
		name = XP_STRDUP(GetAllLocales());
		break;
	default:
		name = XP_STRDUP("");
		break;
	}

	
	return name;
}
#endif /* XP_WIN */
