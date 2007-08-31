/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#include "nsIGenericFactory.h"
#include "nsCOMPtr.h"
#include "nsICategoryManager.h"
#include "nsIServiceManager.h"

// lwbrk
#include "nsLWBrkConstructors.h"
#include "nsSemanticUnitScanner.h"

// unicharutil
#include "nsUcharUtilConstructors.h"

// string bundles (intl)
#include "nsStrBundleConstructors.h"

// locale
#include "nsLocaleConstructors.h"


NS_GENERIC_FACTORY_CONSTRUCTOR(nsSemanticUnitScanner)

static nsModuleComponentInfo components[] =
{
 // lwbrk
  { "Line Breaker", NS_LBRK_CID, 
    NS_LBRK_CONTRACTID, nsJISx4051LineBreakerConstructor},
  { "Word Breaker", NS_WBRK_CID,
    NS_WBRK_CONTRACTID, nsSampleWordBreakerConstructor},
  { "Semantic Unit Scanner", NS_SEMANTICUNITSCANNER_CID,
    NS_SEMANTICUNITSCANNER_CONTRACTID, nsSemanticUnitScannerConstructor},

 // unicharutil
  { "Unichar Utility", NS_UNICHARUTIL_CID, 
      NS_UNICHARUTIL_CONTRACTID, nsCaseConversionImp2Constructor},
  { "Unichar Category Table", NS_UNICHARCATEGORY_CID, 
      NS_UNICHARCATEGORY_CONTRACTID, nsCategoryImpConstructor},
  { "Unicode To Entity Converter", NS_ENTITYCONVERTER_CID, 
      NS_ENTITYCONVERTER_CONTRACTID, nsEntityConverterConstructor },
  { "Unicode To Charset Converter", NS_SAVEASCHARSET_CID, 
      NS_SAVEASCHARSET_CONTRACTID, nsSaveAsCharsetConstructor},
  { "Japanese Hankaku To Zenkaku", NS_HANKAKUTOZENKAKU_CID, 
      NS_HANKAKUTOZENKAKU_CONTRACTID, CreateNewHankakuToZenkaku},
  { "Unicode Normlization", NS_UNICODE_NORMALIZER_CID, 
      NS_UNICODE_NORMALIZER_CONTRACTID,  nsUnicodeNormalizerConstructor},


 // strres
  { "String Bundle", NS_STRINGBUNDLESERVICE_CID, NS_STRINGBUNDLE_CONTRACTID,
    nsStringBundleServiceConstructor},
  { "String Textfile Overrides", NS_STRINGBUNDLETEXTOVERRIDE_CID,
    NS_STRINGBUNDLETEXTOVERRIDE_CONTRACTID,
    nsStringBundleTextOverrideConstructor },

 // locale
  { "nsLocaleService component",
    NS_LOCALESERVICE_CID,
    NS_LOCALESERVICE_CONTRACTID,
    CreateLocaleService },
  { "Collation factory",
    NS_COLLATIONFACTORY_CID,
    NS_COLLATIONFACTORY_CONTRACTID,
    nsCollationFactoryConstructor },
  { "Scriptable Date Format",
    NS_SCRIPTABLEDATEFORMAT_CID,
    NS_SCRIPTABLEDATEFORMAT_CONTRACTID,
    NS_NewScriptableDateFormat },
  { "Language Atom Service",
    NS_LANGUAGEATOMSERVICE_CID,
    NS_LANGUAGEATOMSERVICE_CONTRACTID,
    nsLanguageAtomServiceConstructor },
 
#ifdef XP_WIN 
  { "Platform locale",
    NS_WIN32LOCALE_CID,
    NS_WIN32LOCALE_CONTRACTID,
    nsIWin32LocaleImplConstructor },
  { "Collation",
    NS_COLLATION_CID,
    NS_COLLATION_CONTRACTID,
    nsCollationWinConstructor },
  { "Date/Time formatter",
    NS_DATETIMEFORMAT_CID,
    NS_DATETIMEFORMAT_CONTRACTID,
    nsDateTimeFormatWinConstructor },
#endif
 
#ifdef USE_UNIX_LOCALE
  { "Platform locale",
    NS_POSIXLOCALE_CID,
    NS_POSIXLOCALE_CONTRACTID,
    nsPosixLocaleConstructor },

  { "Collation",
    NS_COLLATION_CID,
    NS_COLLATION_CONTRACTID,
    nsCollationUnixConstructor },

  { "Date/Time formatter",
    NS_DATETIMEFORMAT_CID,
    NS_DATETIMEFORMAT_CONTRACTID,
    nsDateTimeFormatUnixConstructor },
#endif

#ifdef USE_MAC_LOCALE
  { "Mac locale",
    NS_MACLOCALE_CID,
    NS_MACLOCALE_CONTRACTID,
    nsMacLocaleConstructor },
  { "Collation",
    NS_COLLATION_CID,
    NS_COLLATION_CONTRACTID,
#ifdef USE_UCCOLLATIONKEY
    nsCollationMacUCConstructor },
#else
    nsCollationMacConstructor },
#endif
  { "Date/Time formatter",
    NS_DATETIMEFORMAT_CID,
    NS_DATETIMEFORMAT_CONTRACTID,
    nsDateTimeFormatMacConstructor },
#endif

#ifdef XP_OS2
  { "OS/2 locale",
    NS_OS2LOCALE_CID,
    NS_OS2LOCALE_CONTRACTID,
    nsOS2LocaleConstructor },
  { "Collation",
    NS_COLLATION_CID,
    NS_COLLATION_CONTRACTID,
    nsCollationOS2Constructor },
  { "Date/Time formatter",
    NS_DATETIMEFORMAT_CID,
    NS_DATETIMEFORMAT_CONTRACTID,
    nsDateTimeFormatOS2Constructor },
#endif
      
};


NS_IMPL_NSGETMODULE(nsI18nModule, components)
