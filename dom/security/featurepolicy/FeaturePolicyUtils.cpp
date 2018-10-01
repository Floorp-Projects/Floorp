/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FeaturePolicyUtils.h"

using namespace mozilla::dom;

static const char* sSupportedFeatures[] = {
  "camera",
  "geolocation",
  "microphone",
};

/* static */ bool
FeaturePolicyUtils::IsSupportedFeature(const nsAString& aFeatureName)
{
  uint32_t numFeatures = (sizeof(sSupportedFeatures) / sizeof(sSupportedFeatures[0]));
  for (uint32_t i = 0; i < numFeatures; i++) {
    if (aFeatureName.LowerCaseEqualsASCII(sSupportedFeatures[i])) {
      return true;
    }
  }
  return false;
}
