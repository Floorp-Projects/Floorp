 /*
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
#include <types.r>
#undef BETA
#undef ALPHA
#include <systypes.r>
#include "csid.h"

/* STR# 5028 and STR# 5029 should have the same sequence */
/* We need to localize 5028, but not 5029 */
resource 'STR#' (5028, "Language/Region in Human Readable Form", purgeable) {{
	"English",
	"English/United States",
	"English/United Kingdom",	
	"French",
	"French/France",
	"French/Canada",
	"French/Belgium",
	"French/Switzerland",	
	"German",
	"German/Germany",
	"German/Austria",
	"German/Switzerland",	
	"Japanese",	
	"Chinese",
	"Chinese/China",
	"Chinese/Taiwan",	
	"Korean",	
	"Italian",	
	"Spanish",
	"Spanish/Spain",
	"Spanish/Argentina",
	"Spanish/Colombia",
	"Spanish/Mexico",
	"Portuguese",
	"Portuguese/Brazil",	
	"Afrikaans",
	"Albanian",
	"Basque",
	"Catalan",
	"Danish",
	"Dutch",
	"Dutch/Belgium",
	"Faeroese",
	"Finnish",
	"Galician",
	"Icelandic",
	"Indonesian",
	"Irish",
	"Scots Gaelic",
	"Norwegian",
	"Swedish",
	"Croatian",
	"Czech",
	"Hungarian",
	"Polish",
	"Romanian",
	"Slovak",
	"Slovenian",
	"Bulgarian",
	"Byelorussian",
	"Macedonian",
	"Russian",
	"Serbian",
	"Ukrainian",
	"Greek",
	"Turkish"
	
}};
resource 'STR#' (5029, "Language/Region in ISO639-ISO3166 code", purgeable) {{
	"en",
	"en-US",
	"en-GB",	
	"fr",
	"fr-FR",
	"fr-CA",
	"fr-BE",
	"fr-CH",	
	"de",
	"de-DE",
	"de-AU",
	"de-CH",	
	"ja",	
	"zh",
	"zh-CN",
	"zh-TW",	
	"ko",	
	"it",	
	"es",
	"es-ES",
	"es-AR",
	"es-CO",
	"es-MX",
	"pt",
	"pt-BR",	
	"af",
	"sq",
	"eu",
	"ca",
	"da",
	"nl",
	"nl-BE",
	"fo",
	"fi",
	"gl",
	"is",
	"id",
	"ga",
	"gd",
	"no",
	"sv",
	"hr",
	"cs",
	"hu",
	"pl",
	"ro",
	"sk",
	"sl",
	"bg",
	"be",
	"mk",
	"ru",
	"sr",
	"uk",
	"el",
	"tr"

}};

/*	CSID List for Unicode Font */
type 'UCSL' {
	integer = $$CountOf(table);
	wide array table	{
		unsigned integer;	/* Just an int16 */
	};
};

resource 'UCSL' (0, "CSID List for Unicode Font", purgeable) {{
	CS_MAC_ROMAN,
	CS_SJIS,
	CS_BIG5,
	CS_GB_8BIT,
	CS_CNS_8BIT,
	CS_KSC_8BIT,
	CS_MAC_CE,
	CS_SYMBOL,
	CS_DINGBATS
}};
