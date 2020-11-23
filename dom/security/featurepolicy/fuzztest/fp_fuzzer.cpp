/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "FuzzingInterface.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/dom/Feature.h"
#include "mozilla/dom/FeaturePolicyParser.h"
#include "nsNetUtil.h"
#include "nsStringFwd.h"
#include "nsTArray.h"

using namespace mozilla;
using namespace mozilla::dom;

static nsCOMPtr<nsIPrincipal> selfURIPrincipal;
static nsCOMPtr<nsIURI> selfURI;

static int LVVMFuzzerInitTest(int* argc, char*** argv) {
  nsresult ret;
  ret = NS_NewURI(getter_AddRefs(selfURI), "http://selfuri.com");
  if (ret != NS_OK) {
    MOZ_CRASH("NS_NewURI failed.");
  }

  mozilla::OriginAttributes attrs;
  selfURIPrincipal =
      mozilla::BasePrincipal::CreateContentPrincipal(selfURI, attrs);
  if (!selfURIPrincipal) {
    MOZ_CRASH("CreateContentPrincipal failed.");
  }
  return 0;
}

static int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (!size) {
    return 0;
  }
  nsTArray<Feature> parsedFeatures;

  NS_ConvertASCIItoUTF16 policy(reinterpret_cast<const char*>(data), size);
  if (!policy.get()) return 0;

  FeaturePolicyParser::ParseString(policy, nullptr, selfURIPrincipal,
                                   selfURIPrincipal, parsedFeatures);

  for (const Feature& feature : parsedFeatures) {
    nsTArray<nsCOMPtr<nsIPrincipal>> list;
    feature.GetAllowList(list);

    for (nsIPrincipal* principal : list) {
      nsAutoCString originNoSuffix;
      nsresult rv = principal->GetOriginNoSuffix(originNoSuffix);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return 0;
      }
      printf("%s - %s\n", NS_ConvertUTF16toUTF8(feature.Name()).get(),
             originNoSuffix.get());
    }
  }
  return 0;
}

MOZ_FUZZING_INTERFACE_RAW(LVVMFuzzerInitTest, LLVMFuzzerTestOneInput,
                          FeaturePolicyParser);
