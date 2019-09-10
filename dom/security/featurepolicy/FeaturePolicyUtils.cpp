/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FeaturePolicyUtils.h"
#include "mozilla/dom/FeaturePolicy.h"
#include "mozilla/dom/FeaturePolicyViolationReportBody.h"
#include "mozilla/dom/ReportingUtils.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/dom/Document.h"
#include "nsIURIFixup.h"

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
    // We don't support 'autoplay' for now, because it would be overwrote by
    // 'user-gesture-activation' policy. However, we can still keep it in the
    // list as we might start supporting it after we use different autoplay
    // policy.
    {"autoplay", FeaturePolicyUtils::FeaturePolicyValue::eAll},
    {"camera", FeaturePolicyUtils::FeaturePolicyValue::eSelf},
    {"encrypted-media", FeaturePolicyUtils::FeaturePolicyValue::eAll},
    {"fullscreen", FeaturePolicyUtils::FeaturePolicyValue::eAll},
    {"geolocation", FeaturePolicyUtils::FeaturePolicyValue::eSelf},
    {"microphone", FeaturePolicyUtils::FeaturePolicyValue::eSelf},
    {"midi", FeaturePolicyUtils::FeaturePolicyValue::eSelf},
    {"payment", FeaturePolicyUtils::FeaturePolicyValue::eAll},
    {"document-domain", FeaturePolicyUtils::FeaturePolicyValue::eAll},
    {"display-capture", FeaturePolicyUtils::FeaturePolicyValue::eSelf},
    // TODO: not supported yet!!!
    {"speaker", FeaturePolicyUtils::FeaturePolicyValue::eSelf},
    {"vr", FeaturePolicyUtils::FeaturePolicyValue::eAll},
};

/* static */
bool FeaturePolicyUtils::IsSupportedFeature(const nsAString& aFeatureName) {
  uint32_t numFeatures =
      (sizeof(sSupportedFeatures) / sizeof(sSupportedFeatures[0]));
  for (uint32_t i = 0; i < numFeatures; ++i) {
    if (aFeatureName.LowerCaseEqualsASCII(sSupportedFeatures[i].mFeatureName)) {
      return true;
    }
  }
  return false;
}

/* static */
void FeaturePolicyUtils::ForEachFeature(
    const std::function<void(const char*)>& aCallback) {
  uint32_t numFeatures =
      (sizeof(sSupportedFeatures) / sizeof(sSupportedFeatures[0]));
  for (uint32_t i = 0; i < numFeatures; ++i) {
    aCallback(sSupportedFeatures[i].mFeatureName);
  }
}

/* static */ FeaturePolicyUtils::FeaturePolicyValue
FeaturePolicyUtils::DefaultAllowListFeature(const nsAString& aFeatureName) {
  uint32_t numFeatures =
      (sizeof(sSupportedFeatures) / sizeof(sSupportedFeatures[0]));
  for (uint32_t i = 0; i < numFeatures; ++i) {
    if (aFeatureName.LowerCaseEqualsASCII(sSupportedFeatures[i].mFeatureName)) {
      return sSupportedFeatures[i].mDefaultAllowList;
    }
  }

  return FeaturePolicyValue::eNone;
}

/* static */
bool FeaturePolicyUtils::IsFeatureAllowed(Document* aDocument,
                                          const nsAString& aFeatureName) {
  MOZ_ASSERT(aDocument);

  if (!StaticPrefs::dom_security_featurePolicy_enabled()) {
    return true;
  }

  if (!aDocument->IsHTMLDocument()) {
    return true;
  }

  FeaturePolicy* policy = aDocument->FeaturePolicy();
  MOZ_ASSERT(policy);

  if (policy->AllowsFeatureInternal(aFeatureName, policy->DefaultOrigin())) {
    return true;
  }

  ReportViolation(aDocument, aFeatureName);
  return false;
}

/* static */
void FeaturePolicyUtils::ReportViolation(Document* aDocument,
                                         const nsAString& aFeatureName) {
  MOZ_ASSERT(aDocument);

  nsCOMPtr<nsIURI> uri = aDocument->GetDocumentURI();
  if (NS_WARN_IF(!uri)) {
    return;
  }

  // Strip the URL of any possible username/password and make it ready to be
  // presented in the UI.
  nsCOMPtr<nsIURIFixup> urifixup = services::GetURIFixup();
  if (NS_WARN_IF(!urifixup)) {
    return;
  }

  nsCOMPtr<nsIURI> exposableURI;
  nsresult rv = urifixup->CreateExposableURI(uri, getter_AddRefs(exposableURI));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  nsAutoCString spec;
  rv = exposableURI->GetSpec(spec);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }
  JSContext* cx = nsContentUtils::GetCurrentJSContext();
  if (NS_WARN_IF(!cx)) {
    return;
  }

  nsAutoString fileName;
  Nullable<int32_t> lineNumber;
  Nullable<int32_t> columnNumber;
  uint32_t line = 0;
  uint32_t column = 0;
  if (nsJSUtils::GetCallingLocation(cx, fileName, &line, &column)) {
    lineNumber.SetValue(static_cast<int32_t>(line));
    columnNumber.SetValue(static_cast<int32_t>(column));
  }

  nsPIDOMWindowInner* window = aDocument->GetInnerWindow();
  if (NS_WARN_IF(!window)) {
    return;
  }

  RefPtr<FeaturePolicyViolationReportBody> body =
      new FeaturePolicyViolationReportBody(window, aFeatureName, fileName,
                                           lineNumber, columnNumber,
                                           NS_LITERAL_STRING("enforce"));

  ReportingUtils::Report(window, nsGkAtoms::featurePolicyViolation,
                         NS_LITERAL_STRING("default"),
                         NS_ConvertUTF8toUTF16(spec), body);
}

}  // namespace dom
}  // namespace mozilla
