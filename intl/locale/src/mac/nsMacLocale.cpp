/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#include "nsISupports.h"
#include "nscore.h"
#include "nsString.h"
#include "nsILocale.h"
#include "nsMacLocale.h"
#include "nsLocaleCID.h"
#include "prprf.h"
#include <script.h>

NS_DEFINE_IID(kMacLocaleCID, NS_MACLOCALE_CID);

struct iso_lang_map
{
	char*	iso_code;
	short	mac_lang_code;
	short	mac_script_code;

};
typedef struct iso_lang_map iso_lang_map;

iso_lang_map lang_list[] = {
	{ "sq", langAlbanian, smRoman },
	{ "am", langAmharic, smEthiopic	},
	{ "ar", langArabic, smArabic },
	{ "hy", langArmenian, smArmenian},
	{ "as", langAssamese, smBengali },
	{ "ay", langAymara, smRoman},
	{ "eu", langBasque, smRoman},
	{ "bn", langBengali, smBengali },
	{ "dz", langDzongkha, smTibetan },
	{ "br", langBreton, smRoman },
	{ "bg", langBulgarian, smCyrillic },
	{ "my", langBurmese, smBurmese },
	{ "km", langKhmer, smKhmer },
	{ "ca", langCatalan, smRoman },
	{ "zh", langTradChinese, smTradChinese },
	{ "hr", langCroatian, smRoman },
	{ "cs", langCzech, smCentralEuroRoman },
	{ "da", langDanish, smRoman },
	{ "nl", langDutch, smRoman },
	{ "en", langEnglish, smRoman },
	{ "eo", langEsperanto, smRoman },
	{ "et", langEstonian, smCentralEuroRoman},
	{ "fo", langFaeroese, smRoman },
	{ "fa", langFarsi, smArabic },
	{ "fi", langFinnish, smRoman },
	{ "fr", langFrench, smRoman },
	{ "ka", langGeorgian, smGeorgian },
	{ "de", langGerman, smRoman },
	{ "el", langGreek, smGreek },
	{ "gn", langGuarani, smRoman },
	{ "gu", langGujarati, smGujarati },
	{ "he", langHebrew, smHebrew },
	{ "iw", langHebrew, smHebrew },
	{ "hu", langHungarian, smCentralEuroRoman }, 
	{ "is", langIcelandic, smRoman },
	{ "in", langIndonesian, smRoman },
	{ "id", langIndonesian,  smRoman },
	{ "iu", langInuktitut, smEthiopic },
	{ "ga", langIrish, smRoman }, 
	{ "it", langItalian, smRoman },
	{ "ja", langJapanese, smJapanese },
	{ "jw", langJavaneseRom, smRoman },
	{ "kn", langKannada, smKannada },
	{ "ks", langKashmiri, smArabic },
	{ "kk", langKazakh, smCyrillic },
	{ "ky", langKirghiz, smCyrillic },
	{ "ko", langKorean, smKorean },
	{ "ku", langKurdish, smArabic },
	{ "lo", langLao, smLao },
	{ "la", langLatin, smRoman },
	{ "lv", langLatvian, smCentralEuroRoman },
	{ "lt", langLithuanian, smCentralEuroRoman },
	{ "mk", langMacedonian, smCyrillic },
	{ "mg", langMalagasy, smRoman },
	{ "ml", langMalayalam, smMalayalam },
	{ "mt", langMaltese, smRoman },
	{ "mr", langMarathi, smDevanagari },
	{ "mo", langMoldavian, smCyrillic },
	{ "ne", langNepali, smDevanagari },
	{ "no", langNorwegian, smRoman },
	{ "or", langOriya, smOriya },
	{ "om", langOromo, smEthiopic },
	{ "ps", langPashto, smArabic },
	{ "pl", langPolish, smCentralEuroRoman },
	{ "pt", langPortuguese, smRoman },
	{ "pa", langPunjabi, smGurmukhi },
	{ "ro", langRomanian, smRoman },
	{ "ru", langRussian, smCyrillic },
	{ "sa", langSanskrit, smDevanagari },
	{ "sr", langSerbian, smCyrillic },
	{ "sd", langSindhi, smArabic },
	{ "si", langSinhalese, smSinhalese },
	{ "sk", langSlovak, smCentralEuroRoman },
	{ "sl", langSlovenian, smRoman },
	{ "so", langSomali, smRoman },
	{ "es", langSpanish, smRoman },
	{ "su", langSundaneseRom, smRoman },
	{ "sw", langSwahili, smRoman },
	{ "sv", langSwedish, smRoman }, 
	{ "tl", langTagalog, smRoman },
	{ "tg", langTajiki, smCyrillic },
	{ "ta", langTamil, smTamil },
	{ "tt", langTatar, smCyrillic },
	{ "te", langTelugu, smTelugu },
	{ "th", langThai, smThai },
	{ "bo", langTibetan, smTibetan },
	{ "ti", langTigrinya, smEthiopic },
	{ "tr", langTurkish, smRoman },
	{ "tk", langTurkmen, smCyrillic },
	{ "ug", langUighur, smCyrillic },
	{ "uk", langUkrainian, smCyrillic },
	{ "ur", langUrdu, smArabic },
	{ "uz", langUzbek, smCyrillic },
	{ "vi", langVietnamese, smVietnamese },
	{ "cy", langWelsh, smRoman },
	{ "ji", langYiddish, smHebrew },
	{ "yi", langYiddish, smHebrew },
	{ "", 0, 0}
};

struct iso_country_map
{
	char*	iso_code;
	short	mac_region_code;
};

typedef struct iso_country_map iso_country_map;

iso_country_map country_list[] = {
	{ "US", verUS},
	{ "EG", verArabic},
	{ "DZ", verArabic},
	{ "AU", verAustralia},
	{ "BE", verFrBelgium },
	{ "CA", verEngCanada },
	{ "CN", verChina },
	{ "HR", verYugoCroatian },
	{ "CY", verCyprus },
	{ "DK", verDenmark },
	{ "EE", verEstonia },
	{ "FI", verFinland },
	{ "FR", verFrance },
	{ "DE", verGermany },
	{ "EL", verGreece },
	{ "HU", verHungary },
	{ "IS", verIceland },
	{ "IN", verIndiaHindi},
	{ "IR", verIran },
	{ "IQ", verArabic },
	{ "IE", verIreland },
	{ "IL", verIsrael },
	{ "IT", verItaly },
	{ "JP", verJapan },
	{ "KP", verKorea },
	{ "LV", verLatvia },
	{ "LY", verArabic },
	{ "LT", verLithuania },
	{ "LU", verFrBelgiumLux },
	{ "MT", verMalta },
	{ "MA", verArabic },
	{ "NL", verNetherlands },
	{ "NO", verNorway },
	{ "PK", verPakistan },
	{ "PL", verPoland },
	{ "PT", verPortugal },
	{ "RU", verRussia },
	{ "SA", verArabic },
	{ "ES", verSpain },
	{ "SE", verSweden },
	{ "CH", verFrSwiss },
	{ "TW", verTaiwan},
	{ "TH", verThailand },
	{ "TN", verArabic},
	{ "TR", verTurkey },
	{ "GB", verBritain },
	{ "", 0 }
};
	

/* nsMacLocale ISupports */
NS_IMPL_ISUPPORTS1(nsMacLocale,nsIMacLocale)

nsMacLocale::nsMacLocale(void)
{
  NS_INIT_REFCNT();
}

nsMacLocale::~nsMacLocale(void)
{

}

NS_IMETHODIMP 
nsMacLocale::GetPlatformLocale(const nsString* locale,short* scriptCode, short* langCode, short* regionCode)
{
  	char  country_code[3];
  	char  lang_code[3];
  	char  region_code[3];
	char* xp_locale = locale->ToNewCString();
	bool  validCountryFound;
	int	i;
	
	if (xp_locale != nsnull) {
    	if (!ParseLocaleString(xp_locale,lang_code,country_code,region_code,'-')) {
      		*scriptCode = smRoman;
      		*langCode = langEnglish;
      		*regionCode = verUS;
      		delete [] xp_locale;
      		return NS_ERROR_FAILURE;
   		}
	
		if (country_code[0]!=0) 
		{
			validCountryFound=false;
			for(i=0;strlen(country_list[i].iso_code)!=0;i++) {
				if (strcmp(country_list[i].iso_code,country_code)==0) {
					*regionCode = country_list[i].mac_region_code;
					validCountryFound=true;
					break;
				}
			}
			if (!validCountryFound) {
      			*scriptCode = smRoman;
      			*langCode = langEnglish;
      			*regionCode = verUS;
      			delete [] xp_locale;
      			return NS_ERROR_FAILURE;
      		}
		}
		
		for(i=0;strlen(lang_list[i].iso_code)!=0;i++) {
			if (strcmp(lang_list[i].iso_code,lang_code)==0) {
				*scriptCode = lang_list[i].mac_script_code;
				*langCode = lang_list[i].mac_lang_code;
				delete [] xp_locale;
				return NS_OK;
			}
		}
	}
	
	return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsMacLocale::GetXPLocale(short scriptCode, short langCode, short regionCode, nsString* locale)
{

	int i;
	bool validResultFound = false;
	nsString temp;
	
	//
	// parse language
	//
	for(i=0;strlen(lang_list[i].iso_code)!=0;i++) {
		if (langCode==lang_list[i].mac_lang_code && scriptCode==lang_list[i].mac_script_code) {
			temp.AssignWithConversion(lang_list[i].iso_code);
			validResultFound = true;
			break;
		}
	}
	
	//
	// parse region
	//
	for(i=0;strlen(country_list[i].iso_code)!=0;i++) {
		if (regionCode==country_list[i].mac_region_code) {
			temp.AppendWithConversion('-');
			temp.AppendWithConversion(country_list[i].iso_code);
			validResultFound = true;
			break;
		}
	}
	
	if (validResultFound) {
		*locale = temp;
		return NS_OK;
	}
	
	return NS_ERROR_FAILURE;

}

//
// returns PR_FALSE/PR_TRUE depending on if it was of the form LL-CC-RR
PRBool
nsMacLocale::ParseLocaleString(const char* locale_string, char* language, char* country, char* region, char separator)
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
		if (locale_string[2]!=separator) return PR_FALSE;
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
		if (locale_string[2]!=separator || locale_string[5]!=separator) return PR_FALSE;
	} else {
		return PR_FALSE;
	}

	return PR_TRUE;
}
