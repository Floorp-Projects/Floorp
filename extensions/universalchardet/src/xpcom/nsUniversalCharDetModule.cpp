/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"

#include "nsCOMPtr.h"

#include "nspr.h"
#include "nsString.h"
#include "nsUniversalCharDetDll.h"
#include "nsISupports.h"
#include "nsICategoryManager.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsICharsetDetector.h"
#include "nsIStringCharsetDetector.h"

#include "nsUniversalDetector.h"
#include "nsUdetXPCOMWrapper.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsJAPSMDetector)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsJAStringPSMDetector)
NS_DEFINE_NAMED_CID(NS_JA_PSMDETECTOR_CID);
NS_DEFINE_NAMED_CID(NS_JA_STRING_PSMDETECTOR_CID);

static const mozilla::Module::CIDEntry kChardetCIDs[] = {
  { &kNS_JA_PSMDETECTOR_CID, false, nullptr, nsJAPSMDetectorConstructor },
  { &kNS_JA_STRING_PSMDETECTOR_CID, false, nullptr, nsJAStringPSMDetectorConstructor },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kChardetContracts[] = {
  { NS_CHARSET_DETECTOR_CONTRACTID_BASE "ja_parallel_state_machine", &kNS_JA_PSMDETECTOR_CID },
  { NS_STRCDETECTOR_CONTRACTID_BASE "ja_parallel_state_machine", &kNS_JA_STRING_PSMDETECTOR_CID },
  { nullptr }
};

static const mozilla::Module::CategoryEntry kChardetCategories[] = {
  { NS_CHARSET_DETECTOR_CATEGORY, "ja_parallel_state_machine", NS_CHARSET_DETECTOR_CONTRACTID_BASE "ja_parallel_state_machine" },
  { nullptr }
};

static const mozilla::Module kChardetModule = {
  mozilla::Module::kVersion,
  kChardetCIDs,
  kChardetContracts,
  kChardetCategories
};

NSMODULE_DEFN(nsUniversalCharDetModule) = &kChardetModule;
