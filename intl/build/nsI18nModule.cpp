/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"

// lwbrk
#include "nsLWBrkCIID.h"
#include "nsJISx4051LineBreaker.h"
#include "nsSampleWordBreaker.h"

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

// uconv
#include "nsCharsetConverterManager.h"

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
NS_DEFINE_NAMED_CID(NS_PLATFORMCHARSET_CID);
#ifdef XP_WIN
NS_DEFINE_NAMED_CID(NS_COLLATION_CID);
NS_DEFINE_NAMED_CID(NS_DATETIMEFORMAT_CID);
#endif
#ifdef USE_UNIX_LOCALE
NS_DEFINE_NAMED_CID(NS_COLLATION_CID);
NS_DEFINE_NAMED_CID(NS_DATETIMEFORMAT_CID);
#endif
#ifdef USE_MAC_LOCALE
NS_DEFINE_NAMED_CID(NS_COLLATION_CID);
NS_DEFINE_NAMED_CID(NS_DATETIMEFORMAT_CID);
#endif

static const mozilla::Module::CIDEntry kIntlCIDs[] = {
    { &kNS_LBRK_CID, false, nullptr, nsJISx4051LineBreakerConstructor },
    { &kNS_WBRK_CID, false, nullptr, nsSampleWordBreakerConstructor },
    { &kNS_SEMANTICUNITSCANNER_CID, false, nullptr, nsSemanticUnitScannerConstructor },
    { &kNS_UNICHARUTIL_CID, false, nullptr, nsCaseConversionImp2Constructor },
    { &kNS_UNICHARCATEGORY_CID, false, nullptr, nsCategoryImpConstructor },
    { &kNS_ENTITYCONVERTER_CID, false, nullptr, nsEntityConverterConstructor },
    { &kNS_SAVEASCHARSET_CID, false, nullptr, nsSaveAsCharsetConstructor },
    { &kNS_UNICODE_NORMALIZER_CID, false, nullptr, nsUnicodeNormalizerConstructor },
    { &kNS_STRINGBUNDLESERVICE_CID, false, nullptr, nsStringBundleServiceConstructor },
    { &kNS_STRINGBUNDLETEXTOVERRIDE_CID, false, nullptr, nsStringBundleTextOverrideConstructor },
    { &kNS_LOCALESERVICE_CID, false, nullptr, CreateLocaleService },
    { &kNS_COLLATIONFACTORY_CID, false, nullptr, nsCollationFactoryConstructor },
    { &kNS_SCRIPTABLEDATEFORMAT_CID, false, nullptr, NS_NewScriptableDateFormat },
    { &kNS_LANGUAGEATOMSERVICE_CID, false, nullptr, nsLanguageAtomServiceConstructor },
    { &kNS_PLATFORMCHARSET_CID, false, nullptr, nsPlatformCharsetConstructor },
#ifdef XP_WIN
    { &kNS_COLLATION_CID, false, nullptr, nsCollationWinConstructor },
    { &kNS_DATETIMEFORMAT_CID, false, nullptr, nsDateTimeFormatWinConstructor },
#endif
#ifdef USE_UNIX_LOCALE
    { &kNS_COLLATION_CID, false, nullptr, nsCollationUnixConstructor },
    { &kNS_DATETIMEFORMAT_CID, false, nullptr, nsDateTimeFormatUnixConstructor },
#endif
#ifdef USE_MAC_LOCALE
    { &kNS_COLLATION_CID, false, nullptr, nsCollationMacUCConstructor },
    { &kNS_DATETIMEFORMAT_CID, false, nullptr, nsDateTimeFormatMacConstructor },
#endif
    { nullptr }
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
    { NS_PLATFORMCHARSET_CONTRACTID, &kNS_PLATFORMCHARSET_CID },
#ifdef XP_WIN
    { NS_COLLATION_CONTRACTID, &kNS_COLLATION_CID },
    { NS_DATETIMEFORMAT_CONTRACTID, &kNS_DATETIMEFORMAT_CID },
#endif
#ifdef USE_UNIX_LOCALE
    { NS_COLLATION_CONTRACTID, &kNS_COLLATION_CID },
    { NS_DATETIMEFORMAT_CONTRACTID, &kNS_DATETIMEFORMAT_CID },
#endif
#ifdef USE_MAC_LOCALE
    { NS_COLLATION_CONTRACTID, &kNS_COLLATION_CID },
    { NS_DATETIMEFORMAT_CONTRACTID, &kNS_DATETIMEFORMAT_CID },
#endif
    { nullptr }
};

static void
I18nModuleDtor()
{
    nsCharsetConverterManager::Shutdown();
}

static const mozilla::Module kIntlModule = {
    mozilla::Module::kVersion,
    kIntlCIDs,
    kIntlContracts,
    nullptr,
    nullptr,
    nullptr,
    I18nModuleDtor
};

NSMODULE_DEFN(nsI18nModule) = &kIntlModule;
