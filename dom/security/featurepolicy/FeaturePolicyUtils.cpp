/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FeaturePolicyUtils.h"
#include "mozilla/dom/FeaturePolicy.h"
#include "mozilla/StaticPrefs.h"
#include "nsIDocument.h"

namespace mozilla {
namespace dom {

struct FeatureMap {
  const char* mFeatureName;
  FeaturePolicyUtils::FeaturePolicyValue mDefaultAllowList;
};

/*
 * IMPORTANT: Do not change this list without review from a DOM peer _AND_ a
 * DOM Security peer!
 */
static FeatureMap sSupportedFeatures[] = {
  { "autoplay", FeaturePolicyUtils::FeaturePolicyValue::eAll },
  { "camera", FeaturePolicyUtils::FeaturePolicyValue::eSelf },
  { "encrypted-media", FeaturePolicyUtils::FeaturePolicyValue::eAll },
  { "fullscreen", FeaturePolicyUtils::FeaturePolicyValue::eAll },
  { "geolocation", FeaturePolicyUtils::FeaturePolicyValue::eAll },
  { "microphone", FeaturePolicyUtils::FeaturePolicyValue::eSelf },
  { "midi", FeaturePolicyUtils::FeaturePolicyValue::eSelf },
  { "payment", FeaturePolicyUtils::FeaturePolicyValue::eAll },
  // TODO: not supported yet!!!
  { "speaker", FeaturePolicyUtils::FeaturePolicyValue::eSelf },
  { "vr", FeaturePolicyUtils::FeaturePolicyValue::eAll },
};

/* static */ bool
FeaturePolicyUtils::IsSupportedFeature(const nsAString& aFeatureName)
{
  uint32_t numFeatures = (sizeof(sSupportedFeatures) / sizeof(sSupportedFeatures[0]));
  for (uint32_t i = 0; i < numFeatures; ++i) {
    if (aFeatureName.LowerCaseEqualsASCII(sSupportedFeatures[i].mFeatureName)) {
      return true;
    }
  }
  return false;
}

/* static */ void
FeaturePolicyUtils::ForEachFeature(const std::function<void(const char*)>& aCallback)
{
  uint32_t numFeatures = (sizeof(sSupportedFeatures) / sizeof(sSupportedFeatures[0]));
  for (uint32_t i = 0; i < numFeatures; ++i) {
    aCallback(sSupportedFeatures[i].mFeatureName);
  }
}

/* static */ FeaturePolicyUtils::FeaturePolicyValue
FeaturePolicyUtils::DefaultAllowListFeature(const nsAString& aFeatureName)
{
  uint32_t numFeatures = (sizeof(sSupportedFeatures) / sizeof(sSupportedFeatures[0]));
  for (uint32_t i = 0; i < numFeatures; ++i) {
    if (aFeatureName.LowerCaseEqualsASCII(sSupportedFeatures[i].mFeatureName)) {
      return sSupportedFeatures[i].mDefaultAllowList;
    }
  }

  return FeaturePolicyValue::eNone;
}

/* static */ bool
FeaturePolicyUtils::IsFeatureAllowed(nsIDocument* aDocument,
                                     const nsAString& aFeatureName)
{
  MOZ_ASSERT(aDocument);

  if (!StaticPrefs::dom_security_featurePolicy_enabled()) {
    return true;
  }

  if (!aDocument->IsHTMLDocument()) {
    return true;
  }

  FeaturePolicy* policy = aDocument->Policy();
  MOZ_ASSERT(policy);

  return policy->AllowsFeatureInternal(aFeatureName, policy->DefaultOrigin());
}

} // dom namespace
} // mozilla namespace
