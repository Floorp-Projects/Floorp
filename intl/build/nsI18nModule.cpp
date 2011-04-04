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

#include "mozilla/ModuleUtils.h"

// lwbrk
#include "nsLWBrkCIID.h"
#include "nsJISx4501LineBreaker.h"
#include "nsSampleWordBreaker.h"
#include "nsLWBRKDll.h"

#include "nsSemanticUnitScanner.h"

// unicharutil
#include "nsCategoryImp.h"
#include "nsUnicharUtilCIID.h"
#include "nsCaseConversionImp2.h"
#include "nsEntityConverter.h"
#include "nsSaveAsCharset.h"
#include "nsUnicodeNormalizer.h"

// string bundles (intl)
#include "nsStringBundleService.h"
#include "nsStringBundleTextOverride.h"

// locale
#include "nsLocaleConstructors.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsJISx4051LineBreaker)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSampleWordBreaker)

NS_GENERIC_FACTORY_CONSTRUCTOR(nsSemanticUnitScanner)

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsStringBundleService, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsStringBundleTextOverride, Init)

NS_GENERIC_FACTORY_CONSTRUCTOR(nsCaseConversionImp2)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsCategoryImp,
                                         nsCategoryImp::GetInstance)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsEntityConverter)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSaveAsCharset)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeNormalizer)

NS_DEFINE_NAMED_CID(NS_LBRK_CID);
NS_DEFINE_NAMED_CID(NS_WBRK_CID);
NS_DEFINE_NAMED_CID(NS_SEMANTICUNITSCANNER_CID);
NS_DEFINE_NAMED_CID(NS_UNICHARUTIL_CID);
NS_DEFINE_NAMED_CID(NS_UNICHARCATEGORY_CID);
NS_DEFINE_NAMED_CID(NS_ENTITYCONVERTER_CID);
NS_DEFINE_NAMED_CID(NS_SAVEASCHARSET_CID);
NS_DEFINE_NAMED_CID(NS_UNICODE_NORMALIZER_CID);
NS_DEFINE_NAMED_CID(NS_STRINGBUNDLESERVICE_CID);
NS_DEFINE_NAMED_CID(NS_STRINGBUNDLETEXTOVERRIDE_CID);
NS_DEFINE_NAMED_CID(NS_LOCALESERVICE_CID);
NS_DEFINE_NAMED_CID(NS_COLLATIONFACTORY_CID);
NS_DEFINE_NAMED_CID(NS_SCRIPTABLEDATEFORMAT_CID);
NS_DEFINE_NAMED_CID(NS_LANGUAGEATOMSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_CHARSETALIAS_CID);
NS_DEFINE_NAMED_CID(NS_PLATFORMCHARSET_CID);
#ifdef XP_WIN
NS_DEFINE_NAMED_CID(NS_WIN32LOCALE_CID);
NS_DEFINE_NAMED_CID(NS_COLLATION_CID);
NS_DEFINE_NAMED_CID(NS_DATETIMEFORMAT_CID);
#endif
#ifdef USE_UNIX_LOCALE
NS_DEFINE_NAMED_CID(NS_POSIXLOCALE_CID);
NS_DEFINE_NAMED_CID(NS_COLLATION_CID);
NS_DEFINE_NAMED_CID(NS_DATETIMEFORMAT_CID);
#endif
#ifdef USE_MAC_LOCALE
NS_DEFINE_NAMED_CID(NS_MACLOCALE_CID);
NS_DEFINE_NAMED_CID(NS_COLLATION_CID);
NS_DEFINE_NAMED_CID(NS_DATETIMEFORMAT_CID);
#endif
#ifdef XP_OS2
NS_DEFINE_NAMED_CID(NS_OS2LOCALE_CID);
NS_DEFINE_NAMED_CID(NS_COLLATION_CID);
NS_DEFINE_NAMED_CID(NS_DATETIMEFORMAT_CID);
#endif

static const mozilla::Module::CIDEntry kIntlCIDs[] = {
    { &kNS_LBRK_CID, false, NULL, nsJISx4051LineBreakerConstructor },
    { &kNS_WBRK_CID, false, NULL, nsSampleWordBreakerConstructor },
    { &kNS_SEMANTICUNITSCANNER_CID, false, NULL, nsSemanticUnitScannerConstructor },
    { &kNS_UNICHARUTIL_CID, false, NULL, nsCaseConversionImp2Constructor },
    { &kNS_UNICHARCATEGORY_CID, false, NULL, nsCategoryImpConstructor },
    { &kNS_ENTITYCONVERTER_CID, false, NULL, nsEntityConverterConstructor },
    { &kNS_SAVEASCHARSET_CID, false, NULL, nsSaveAsCharsetConstructor },
    { &kNS_UNICODE_NORMALIZER_CID, false, NULL, nsUnicodeNormalizerConstructor },
    { &kNS_STRINGBUNDLESERVICE_CID, false, NULL, nsStringBundleServiceConstructor },
    { &kNS_STRINGBUNDLETEXTOVERRIDE_CID, false, NULL, nsStringBundleTextOverrideConstructor },
    { &kNS_LOCALESERVICE_CID, false, NULL, CreateLocaleService },
    { &kNS_COLLATIONFACTORY_CID, false, NULL, nsCollationFactoryConstructor },
    { &kNS_SCRIPTABLEDATEFORMAT_CID, false, NULL, NS_NewScriptableDateFormat },
    { &kNS_LANGUAGEATOMSERVICE_CID, false, NULL, nsLanguageAtomServiceConstructor },
    { &kNS_CHARSETALIAS_CID, false, NULL, nsCharsetAlias2Constructor },
    { &kNS_PLATFORMCHARSET_CID, false, NULL, nsPlatformCharsetConstructor },
#ifdef XP_WIN
    { &kNS_WIN32LOCALE_CID, false, NULL, nsIWin32LocaleImplConstructor },
    { &kNS_COLLATION_CID, false, NULL, nsCollationWinConstructor },
    { &kNS_DATETIMEFORMAT_CID, false, NULL, nsDateTimeFormatWinConstructor },
#endif
#ifdef USE_UNIX_LOCALE
    { &kNS_POSIXLOCALE_CID, false, NULL, nsPosixLocaleConstructor },
    { &kNS_COLLATION_CID, false, NULL, nsCollationUnixConstructor },
    { &kNS_DATETIMEFORMAT_CID, false, NULL, nsDateTimeFormatUnixConstructor },
#endif
#ifdef USE_MAC_LOCALE
    { &kNS_MACLOCALE_CID, false, NULL, nsMacLocaleConstructor },
    { &kNS_COLLATION_CID, false, NULL, nsCollationMacUCConstructor },
    { &kNS_DATETIMEFORMAT_CID, false, NULL, nsDateTimeFormatMacConstructor },
#endif
#ifdef XP_OS2
    { &kNS_OS2LOCALE_CID, false, NULL, nsOS2LocaleConstructor },
    { &kNS_COLLATION_CID, false, NULL, nsCollationOS2Constructor },
    { &kNS_DATETIMEFORMAT_CID, false, NULL, nsDateTimeFormatOS2Constructor },
#endif
    { NULL }
};

static const mozilla::Module::ContractIDEntry kIntlContracts[] = {
    { NS_LBRK_CONTRACTID, &kNS_LBRK_CID },
    { NS_WBRK_CONTRACTID, &kNS_WBRK_CID },
    { NS_SEMANTICUNITSCANNER_CONTRACTID, &kNS_SEMANTICUNITSCANNER_CID },
    { NS_UNICHARUTIL_CONTRACTID, &kNS_UNICHARUTIL_CID },
    { NS_UNICHARCATEGORY_CONTRACTID, &kNS_UNICHARCATEGORY_CID },
    { NS_ENTITYCONVERTER_CONTRACTID, &kNS_ENTITYCONVERTER_CID },
    { NS_SAVEASCHARSET_CONTRACTID, &kNS_SAVEASCHARSET_CID },
    { NS_UNICODE_NORMALIZER_CONTRACTID, &kNS_UNICODE_NORMALIZER_CID },
    { NS_STRINGBUNDLE_CONTRACTID, &kNS_STRINGBUNDLESERVICE_CID },
    { NS_STRINGBUNDLETEXTOVERRIDE_CONTRACTID, &kNS_STRINGBUNDLETEXTOVERRIDE_CID },
    { NS_LOCALESERVICE_CONTRACTID, &kNS_LOCALESERVICE_CID },
    { NS_COLLATIONFACTORY_CONTRACTID, &kNS_COLLATIONFACTORY_CID },
    { NS_SCRIPTABLEDATEFORMAT_CONTRACTID, &kNS_SCRIPTABLEDATEFORMAT_CID },
    { NS_LANGUAGEATOMSERVICE_CONTRACTID, &kNS_LANGUAGEATOMSERVICE_CID },
    { NS_CHARSETALIAS_CONTRACTID, &kNS_CHARSETALIAS_CID },
    { NS_PLATFORMCHARSET_CONTRACTID, &kNS_PLATFORMCHARSET_CID },
#ifdef XP_WIN
    { NS_WIN32LOCALE_CONTRACTID, &kNS_WIN32LOCALE_CID },
    { NS_COLLATION_CONTRACTID, &kNS_COLLATION_CID },
    { NS_DATETIMEFORMAT_CONTRACTID, &kNS_DATETIMEFORMAT_CID },
#endif
#ifdef USE_UNIX_LOCALE
    { NS_POSIXLOCALE_CONTRACTID, &kNS_POSIXLOCALE_CID },
    { NS_COLLATION_CONTRACTID, &kNS_COLLATION_CID },
    { NS_DATETIMEFORMAT_CONTRACTID, &kNS_DATETIMEFORMAT_CID },
#endif
#ifdef USE_MAC_LOCALE
    { NS_MACLOCALE_CONTRACTID, &kNS_MACLOCALE_CID },
    { NS_COLLATION_CONTRACTID, &kNS_COLLATION_CID },
    { NS_DATETIMEFORMAT_CONTRACTID, &kNS_DATETIMEFORMAT_CID },
#endif
#ifdef XP_OS2
    { NS_OS2LOCALE_CONTRACTID, &kNS_OS2LOCALE_CID },
    { NS_COLLATION_CONTRACTID, &kNS_COLLATION_CID },
    { NS_DATETIMEFORMAT_CONTRACTID, &kNS_DATETIMEFORMAT_CID },
#endif
    { NULL }
};

static const mozilla::Module kIntlModule = {
    mozilla::Module::kVersion,
    kIntlCIDs,
    kIntlContracts,
};

NSMODULE_DEFN(nsI18nModule) = &kIntlModule;
