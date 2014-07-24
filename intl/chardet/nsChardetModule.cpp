/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"

#include "nsCharDetConstructors.h"

NS_DEFINE_NAMED_CID(NS_RU_PROBDETECTOR_CID);
NS_DEFINE_NAMED_CID(NS_UK_PROBDETECTOR_CID);
NS_DEFINE_NAMED_CID(NS_RU_STRING_PROBDETECTOR_CID);
NS_DEFINE_NAMED_CID(NS_UK_STRING_PROBDETECTOR_CID);

static const mozilla::Module::CIDEntry kChardetCIDs[] = {
  { &kNS_RU_PROBDETECTOR_CID, false, nullptr, nsRUProbDetectorConstructor },
  { &kNS_UK_PROBDETECTOR_CID, false, nullptr, nsUKProbDetectorConstructor },
  { &kNS_RU_STRING_PROBDETECTOR_CID, false, nullptr, nsRUStringProbDetectorConstructor },
  { &kNS_UK_STRING_PROBDETECTOR_CID, false, nullptr, nsUKStringProbDetectorConstructor },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kChardetContracts[] = {
  { NS_CHARSET_DETECTOR_CONTRACTID_BASE "ruprob", &kNS_RU_PROBDETECTOR_CID },
  { NS_CHARSET_DETECTOR_CONTRACTID_BASE "ukprob", &kNS_UK_PROBDETECTOR_CID },
  { NS_STRCDETECTOR_CONTRACTID_BASE "ruprob", &kNS_RU_STRING_PROBDETECTOR_CID },
  { NS_STRCDETECTOR_CONTRACTID_BASE "ukprob", &kNS_UK_STRING_PROBDETECTOR_CID },
  { nullptr }
};

static const mozilla::Module::CategoryEntry kChardetCategories[] = {
  { NS_CHARSET_DETECTOR_CATEGORY, "off", "off" },
  { NS_CHARSET_DETECTOR_CATEGORY, "ruprob", NS_CHARSET_DETECTOR_CONTRACTID_BASE "ruprob" },
  { NS_CHARSET_DETECTOR_CATEGORY, "ukprob", NS_CHARSET_DETECTOR_CONTRACTID_BASE "ukprob" },
  { nullptr }
};

static const mozilla::Module kChardetModule = {
  mozilla::Module::kVersion,
  kChardetCIDs,
  kChardetContracts,
  kChardetCategories
};

NSMODULE_DEFN(nsChardetModule) = &kChardetModule;
