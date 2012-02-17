/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#include "nsCOMPtr.h"
#include "nsICategoryManager.h"
#include "nsIServiceManager.h"

#include "nsCharDetConstructors.h"

NS_DEFINE_NAMED_CID(NS_RU_PROBDETECTOR_CID);
NS_DEFINE_NAMED_CID(NS_UK_PROBDETECTOR_CID);
NS_DEFINE_NAMED_CID(NS_RU_STRING_PROBDETECTOR_CID);
NS_DEFINE_NAMED_CID(NS_UK_STRING_PROBDETECTOR_CID);

static const mozilla::Module::CIDEntry kChardetCIDs[] = {
  { &kNS_RU_PROBDETECTOR_CID, false, NULL, nsRUProbDetectorConstructor },
  { &kNS_UK_PROBDETECTOR_CID, false, NULL, nsUKProbDetectorConstructor },
  { &kNS_RU_STRING_PROBDETECTOR_CID, false, NULL, nsRUStringProbDetectorConstructor },
  { &kNS_UK_STRING_PROBDETECTOR_CID, false, NULL, nsUKStringProbDetectorConstructor },
  { NULL }
};

static const mozilla::Module::ContractIDEntry kChardetContracts[] = {
  { NS_CHARSET_DETECTOR_CONTRACTID_BASE "ruprob", &kNS_RU_PROBDETECTOR_CID },
  { NS_CHARSET_DETECTOR_CONTRACTID_BASE "ukprob", &kNS_UK_PROBDETECTOR_CID },
  { NS_STRCDETECTOR_CONTRACTID_BASE "ruprob", &kNS_RU_STRING_PROBDETECTOR_CID },
  { NS_STRCDETECTOR_CONTRACTID_BASE "ukprob", &kNS_UK_STRING_PROBDETECTOR_CID },
  { NULL }
};

static const mozilla::Module::CategoryEntry kChardetCategories[] = {
  { NS_CHARSET_DETECTOR_CATEGORY, "off", "off" },
  { NS_CHARSET_DETECTOR_CATEGORY, "ruprob", NS_CHARSET_DETECTOR_CONTRACTID_BASE "ruprob" },
  { NS_CHARSET_DETECTOR_CATEGORY, "ukprob", NS_CHARSET_DETECTOR_CONTRACTID_BASE "ukprob" },
  { NULL }
};

static const mozilla::Module kChardetModule = {
  mozilla::Module::kVersion,
  kChardetCIDs,
  kChardetContracts,
  kChardetCategories
};

NSMODULE_DEFN(nsChardetModule) = &kChardetModule;
