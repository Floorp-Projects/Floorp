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


NS_GENERIC_FACTORY_CONSTRUCTOR(nsUniversalXPCOMDetector)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUniversalXPCOMStringDetector)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsJAPSMDetector)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsJAStringPSMDetector)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsKOPSMDetector)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsKOStringPSMDetector)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsZHTWPSMDetector)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsZHTWStringPSMDetector)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsZHCNPSMDetector)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsZHCNStringPSMDetector)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsZHPSMDetector)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsZHStringPSMDetector)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCJKPSMDetector)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCJKStringPSMDetector)
NS_DEFINE_NAMED_CID(NS_UNIVERSAL_DETECTOR_CID);
NS_DEFINE_NAMED_CID(NS_UNIVERSAL_STRING_DETECTOR_CID);
NS_DEFINE_NAMED_CID(NS_JA_PSMDETECTOR_CID);
NS_DEFINE_NAMED_CID(NS_JA_STRING_PSMDETECTOR_CID);
NS_DEFINE_NAMED_CID(NS_KO_PSMDETECTOR_CID);
NS_DEFINE_NAMED_CID(NS_KO_STRING_PSMDETECTOR_CID);
NS_DEFINE_NAMED_CID(NS_ZHTW_PSMDETECTOR_CID);
NS_DEFINE_NAMED_CID(NS_ZHTW_STRING_PSMDETECTOR_CID);
NS_DEFINE_NAMED_CID(NS_ZHCN_PSMDETECTOR_CID);
NS_DEFINE_NAMED_CID(NS_ZHCN_STRING_PSMDETECTOR_CID);
NS_DEFINE_NAMED_CID(NS_ZH_PSMDETECTOR_CID);
NS_DEFINE_NAMED_CID(NS_ZH_STRING_PSMDETECTOR_CID);
NS_DEFINE_NAMED_CID(NS_CJK_PSMDETECTOR_CID);
NS_DEFINE_NAMED_CID(NS_CJK_STRING_PSMDETECTOR_CID);

static const mozilla::Module::CIDEntry kChardetCIDs[] = {
  { &kNS_UNIVERSAL_DETECTOR_CID, false, nullptr, nsUniversalXPCOMDetectorConstructor },
  { &kNS_UNIVERSAL_STRING_DETECTOR_CID, false, nullptr, nsUniversalXPCOMStringDetectorConstructor },
  { &kNS_JA_PSMDETECTOR_CID, false, nullptr, nsJAPSMDetectorConstructor },
  { &kNS_JA_STRING_PSMDETECTOR_CID, false, nullptr, nsJAStringPSMDetectorConstructor },
  { &kNS_KO_PSMDETECTOR_CID, false, nullptr, nsKOPSMDetectorConstructor },
  { &kNS_KO_STRING_PSMDETECTOR_CID, false, nullptr, nsKOStringPSMDetectorConstructor },
  { &kNS_ZHTW_PSMDETECTOR_CID, false, nullptr, nsZHTWPSMDetectorConstructor },
  { &kNS_ZHTW_STRING_PSMDETECTOR_CID, false, nullptr, nsZHTWStringPSMDetectorConstructor },
  { &kNS_ZHCN_PSMDETECTOR_CID, false, nullptr, nsZHCNPSMDetectorConstructor },
  { &kNS_ZHCN_STRING_PSMDETECTOR_CID, false, nullptr, nsZHCNStringPSMDetectorConstructor },
  { &kNS_ZH_PSMDETECTOR_CID, false, nullptr, nsZHPSMDetectorConstructor },
  { &kNS_ZH_STRING_PSMDETECTOR_CID, false, nullptr, nsZHStringPSMDetectorConstructor },
  { &kNS_CJK_PSMDETECTOR_CID, false, nullptr, nsCJKPSMDetectorConstructor },
  { &kNS_CJK_STRING_PSMDETECTOR_CID, false, nullptr, nsCJKStringPSMDetectorConstructor },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kChardetContracts[] = {
  { NS_CHARSET_DETECTOR_CONTRACTID_BASE "universal_charset_detector", &kNS_UNIVERSAL_DETECTOR_CID },
  { NS_STRCDETECTOR_CONTRACTID_BASE "universal_charset_detector", &kNS_UNIVERSAL_STRING_DETECTOR_CID },
  { NS_CHARSET_DETECTOR_CONTRACTID_BASE "ja_parallel_state_machine", &kNS_JA_PSMDETECTOR_CID },
  { NS_STRCDETECTOR_CONTRACTID_BASE "ja_parallel_state_machine", &kNS_JA_STRING_PSMDETECTOR_CID },
  { NS_CHARSET_DETECTOR_CONTRACTID_BASE "ko_parallel_state_machine", &kNS_KO_PSMDETECTOR_CID },
  { NS_STRCDETECTOR_CONTRACTID_BASE "ko_parallel_state_machine", &kNS_KO_STRING_PSMDETECTOR_CID },
  { NS_CHARSET_DETECTOR_CONTRACTID_BASE "zhtw_parallel_state_machine", &kNS_ZHTW_PSMDETECTOR_CID },
  { NS_STRCDETECTOR_CONTRACTID_BASE "zhtw_parallel_state_machine", &kNS_ZHTW_STRING_PSMDETECTOR_CID },
  { NS_CHARSET_DETECTOR_CONTRACTID_BASE "zhcn_parallel_state_machine", &kNS_ZHCN_PSMDETECTOR_CID },
  { NS_STRCDETECTOR_CONTRACTID_BASE "zhcn_parallel_state_machine", &kNS_ZHCN_STRING_PSMDETECTOR_CID },
  { NS_CHARSET_DETECTOR_CONTRACTID_BASE "zh_parallel_state_machine", &kNS_ZH_PSMDETECTOR_CID },
  { NS_STRCDETECTOR_CONTRACTID_BASE "zh_parallel_state_machine", &kNS_ZH_STRING_PSMDETECTOR_CID },
  { NS_CHARSET_DETECTOR_CONTRACTID_BASE "cjk_parallel_state_machine", &kNS_CJK_PSMDETECTOR_CID },
  { NS_STRCDETECTOR_CONTRACTID_BASE "cjk_parallel_state_machine", &kNS_CJK_STRING_PSMDETECTOR_CID },
  { nullptr }
};

static const mozilla::Module::CategoryEntry kChardetCategories[] = {
  { NS_CHARSET_DETECTOR_CATEGORY, "universal_charset_detector", NS_CHARSET_DETECTOR_CONTRACTID_BASE "universal_charset_detector" },
  { NS_CHARSET_DETECTOR_CATEGORY, "ja_parallel_state_machine", NS_CHARSET_DETECTOR_CONTRACTID_BASE "ja_parallel_state_machine" },
  { NS_CHARSET_DETECTOR_CATEGORY, "ko_parallel_state_machine", NS_CHARSET_DETECTOR_CONTRACTID_BASE "ko_parallel_state_machine" },
  { NS_CHARSET_DETECTOR_CATEGORY, "zhtw_parallel_state_machine", NS_CHARSET_DETECTOR_CONTRACTID_BASE "zhtw_parallel_state_machine" },
  { NS_CHARSET_DETECTOR_CATEGORY, "zhcn_parallel_state_machine", NS_CHARSET_DETECTOR_CONTRACTID_BASE "zhcn_parallel_state_machine" },
  { NS_CHARSET_DETECTOR_CATEGORY, "zh_parallel_state_machine", NS_CHARSET_DETECTOR_CONTRACTID_BASE "zh_parallel_state_machine" },
  { NS_CHARSET_DETECTOR_CATEGORY, "cjk_parallel_state_machine", NS_CHARSET_DETECTOR_CONTRACTID_BASE "cjk_parallel_state_machine" },
  { nullptr }
};

static const mozilla::Module kChardetModule = {
  mozilla::Module::kVersion,
  kChardetCIDs,
  kChardetContracts,
  kChardetCategories
};

NSMODULE_DEFN(nsUniversalCharDetModule) = &kChardetModule;
