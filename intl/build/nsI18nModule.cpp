/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"

// string bundles (intl)
#include "nsStringBundleService.h"

// locale
#include "nsLocaleConstructors.h"

// uconv

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsStringBundleService, Init)

NS_DEFINE_NAMED_CID(MOZ_LOCALESERVICE_CID);
NS_DEFINE_NAMED_CID(MOZ_OSPREFERENCES_CID);
NS_DEFINE_NAMED_CID(NS_STRINGBUNDLESERVICE_CID);
NS_DEFINE_NAMED_CID(NS_COLLATIONFACTORY_CID);
NS_DEFINE_NAMED_CID(NS_COLLATION_CID);

static const mozilla::Module::CIDEntry kIntlCIDs[] = {
    { &kMOZ_LOCALESERVICE_CID, false, nullptr, mozilla::intl::LocaleServiceConstructor },
    { &kMOZ_OSPREFERENCES_CID, false, nullptr, mozilla::intl::OSPreferencesConstructor },
    { &kNS_STRINGBUNDLESERVICE_CID, false, nullptr, nsStringBundleServiceConstructor },
    { &kNS_COLLATIONFACTORY_CID, false, nullptr, nsCollationFactoryConstructor },
    { &kNS_COLLATION_CID, false, nullptr, nsCollationConstructor },
    { nullptr }
};

static const mozilla::Module::ContractIDEntry kIntlContracts[] = {
    { MOZ_LOCALESERVICE_CONTRACTID, &kMOZ_LOCALESERVICE_CID },
    { MOZ_OSPREFERENCES_CONTRACTID, &kMOZ_OSPREFERENCES_CID },
    { NS_STRINGBUNDLE_CONTRACTID, &kNS_STRINGBUNDLESERVICE_CID },
    { NS_COLLATIONFACTORY_CONTRACTID, &kNS_COLLATIONFACTORY_CID },
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
