/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"

// lwbrk
#include "nsLWBrkCIID.h"
#include "nsJISx4051LineBreaker.h"
#include "nsSampleWordBreaker.h"

// unicharutil
#include "nsEntityConverter.h"
#include "nsUnicodeNormalizer.h"

// string bundles (intl)
#include "nsStringBundleService.h"
#include "nsStringBundleTextOverride.h"

// locale
#include "nsLocaleConstructors.h"

// uconv

NS_GENERIC_FACTORY_CONSTRUCTOR(nsJISx4051LineBreaker)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSampleWordBreaker)

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsStringBundleService, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsStringBundleTextOverride, Init)

NS_GENERIC_FACTORY_CONSTRUCTOR(nsEntityConverter)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeNormalizer)

NS_DEFINE_NAMED_CID(MOZ_LOCALESERVICE_CID);
NS_DEFINE_NAMED_CID(MOZ_OSPREFERENCES_CID);
NS_DEFINE_NAMED_CID(NS_LBRK_CID);
NS_DEFINE_NAMED_CID(NS_WBRK_CID);
NS_DEFINE_NAMED_CID(NS_ENTITYCONVERTER_CID);
NS_DEFINE_NAMED_CID(NS_UNICODE_NORMALIZER_CID);
NS_DEFINE_NAMED_CID(NS_STRINGBUNDLESERVICE_CID);
NS_DEFINE_NAMED_CID(NS_STRINGBUNDLETEXTOVERRIDE_CID);
NS_DEFINE_NAMED_CID(NS_COLLATIONFACTORY_CID);
NS_DEFINE_NAMED_CID(NS_SCRIPTABLEDATEFORMAT_CID);
NS_DEFINE_NAMED_CID(NS_PLATFORMCHARSET_CID);
NS_DEFINE_NAMED_CID(NS_COLLATION_CID);

static const mozilla::Module::CIDEntry kIntlCIDs[] = {
    { &kMOZ_LOCALESERVICE_CID, false, nullptr, mozilla::intl::LocaleServiceConstructor },
    { &kMOZ_OSPREFERENCES_CID, false, nullptr, mozilla::intl::OSPreferencesConstructor },
    { &kNS_LBRK_CID, false, nullptr, nsJISx4051LineBreakerConstructor },
    { &kNS_WBRK_CID, false, nullptr, nsSampleWordBreakerConstructor },
    { &kNS_ENTITYCONVERTER_CID, false, nullptr, nsEntityConverterConstructor },
    { &kNS_UNICODE_NORMALIZER_CID, false, nullptr, nsUnicodeNormalizerConstructor },
    { &kNS_STRINGBUNDLESERVICE_CID, false, nullptr, nsStringBundleServiceConstructor },
    { &kNS_STRINGBUNDLETEXTOVERRIDE_CID, false, nullptr, nsStringBundleTextOverrideConstructor },
    { &kNS_COLLATIONFACTORY_CID, false, nullptr, nsCollationFactoryConstructor },
    { &kNS_SCRIPTABLEDATEFORMAT_CID, false, nullptr, NS_NewScriptableDateFormat },
    { &kNS_PLATFORMCHARSET_CID, false, nullptr, nsPlatformCharsetConstructor },
    { &kNS_COLLATION_CID, false, nullptr, nsCollationConstructor },
    { nullptr }
};

static const mozilla::Module::ContractIDEntry kIntlContracts[] = {
    { MOZ_LOCALESERVICE_CONTRACTID, &kMOZ_LOCALESERVICE_CID },
    { MOZ_OSPREFERENCES_CONTRACTID, &kMOZ_OSPREFERENCES_CID },
    { NS_LBRK_CONTRACTID, &kNS_LBRK_CID },
    { NS_WBRK_CONTRACTID, &kNS_WBRK_CID },
    { NS_ENTITYCONVERTER_CONTRACTID, &kNS_ENTITYCONVERTER_CID },
    { NS_UNICODE_NORMALIZER_CONTRACTID, &kNS_UNICODE_NORMALIZER_CID },
    { NS_STRINGBUNDLE_CONTRACTID, &kNS_STRINGBUNDLESERVICE_CID },
    { NS_STRINGBUNDLETEXTOVERRIDE_CONTRACTID, &kNS_STRINGBUNDLETEXTOVERRIDE_CID },
    { NS_COLLATIONFACTORY_CONTRACTID, &kNS_COLLATIONFACTORY_CID },
    { NS_SCRIPTABLEDATEFORMAT_CONTRACTID, &kNS_SCRIPTABLEDATEFORMAT_CID },
    { NS_PLATFORMCHARSET_CONTRACTID, &kNS_PLATFORMCHARSET_CID },
    { NS_COLLATION_CONTRACTID, &kNS_COLLATION_CID },
    { nullptr }
};

static const mozilla::Module kIntlModule = {
    mozilla::Module::kVersion,
    kIntlCIDs,
    kIntlContracts,
    nullptr,
    nullptr,
    nullptr,
    nullptr
};

NSMODULE_DEFN(nsI18nModule) = &kIntlModule;
