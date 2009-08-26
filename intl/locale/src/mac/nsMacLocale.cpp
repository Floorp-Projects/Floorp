/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
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
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <Carbon/Carbon.h>

#include "nsISupports.h"
#include "nscore.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsMacLocale.h"
#include "nsLocaleCID.h"
#include "prprf.h"
#include "nsReadableUtils.h"
#include "nsXPCOMStrings.h"

struct iso_lang_map
{
  const char*  iso_code;
  short        mac_lang_code;
  short        mac_script_code;

};
typedef struct iso_lang_map iso_lang_map;

const iso_lang_map lang_list[] = {
  { "sq", langAlbanian, smRoman },
  { "am", langAmharic, smEthiopic  },
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
  { "hi", langHindi, smDevanagari },
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
  { "nb", langNorwegian, smRoman }, // Norwegian Bokmål
  { "no", langNorwegian, smRoman },
  { "nn", langNynorsk, smRoman },
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
  { nsnull, 0, 0}
};

struct iso_country_map
{
  const char*  iso_code;
  short        mac_region_code;
};

typedef struct iso_country_map iso_country_map;

const iso_country_map country_list[] = {
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
  { "KR", verKorea },
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
  { nsnull, 0 }
};
  

/* nsMacLocale ISupports */
NS_IMPL_ISUPPORTS1(nsMacLocale,nsIMacLocale)

nsMacLocale::nsMacLocale(void)
{
}

nsMacLocale::~nsMacLocale(void)
{

}

NS_IMETHODIMP 
nsMacLocale::GetPlatformLocale(const nsAString& locale, short* scriptCode, short* langCode, short* regionCode)
{
  char    locale_string[9] = {'\0','\0','\0','\0','\0','\0','\0','\0','\0'};
  char*   language_code;
  char*   country_code;
  bool  validCountryFound;
  int  i, j;

  // parse the locale
  const PRUnichar* data;
  j = NS_StringGetData(locale, &data);
  for (i = 0; i < 7 && i < j; i++) {
    locale_string[i] = data[i] == '-' ? '\0' : data[i];
  }

  language_code = locale_string;
  country_code = locale_string + strlen(locale_string) + 1;

  // convert parsed locale to Mac OS locale
  if (country_code[0]!=0) 
  {
    validCountryFound=false;
    for(i=0;country_list[i].iso_code;i++) {
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
      return NS_ERROR_FAILURE;
    }
  }
    
  for(i=0;lang_list[i].iso_code;i++) {
    if (strcmp(lang_list[i].iso_code, language_code)==0) {
      *scriptCode = lang_list[i].mac_script_code;
      *langCode = lang_list[i].mac_lang_code;
      return NS_OK;
    }
  }
  *scriptCode = smRoman;
  *langCode = langEnglish;
  *regionCode = verUS;
  return NS_ERROR_FAILURE;
}
  
NS_IMETHODIMP
nsMacLocale::GetXPLocale(short scriptCode, short langCode, short regionCode, nsAString& locale)
{

  int i;
  bool validResultFound = false;

  locale.Truncate();
  
  //
  // parse language
  //
  for(i=0;lang_list[i].iso_code;i++) {
    if (langCode==lang_list[i].mac_lang_code && scriptCode==lang_list[i].mac_script_code) {
      CopyASCIItoUTF16(nsDependentCString(lang_list[i].iso_code), locale);
      validResultFound = true;
      break;
    }
  }
  
  if (!validResultFound) {
    return NS_ERROR_FAILURE;
  }
  
  //
  // parse region
  //
  for(i=0;country_list[i].iso_code;i++) {
    if (regionCode==country_list[i].mac_region_code) {
      locale.Append(PRUnichar('-'));
      AppendASCIItoUTF16(country_list[i].iso_code, locale);
      validResultFound = true;
      break;
    }
  }
  
  if (validResultFound) {
    return NS_OK;
  }
  
  return NS_ERROR_FAILURE;
}
