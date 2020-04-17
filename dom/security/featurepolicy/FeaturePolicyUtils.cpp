/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FeaturePolicyUtils.h"
#include "nsIOService.h"

#include "mozilla/dom/DOMTypes.h"
#include "mozilla/ipc/IPDLParamTraits.h"
#include "mozilla/dom/FeaturePolicyViolationReportBody.h"
#include "mozilla/dom/ReportingUtils.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/dom/Document.h"

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
    {"camera", FeaturePolicyUtils::FeaturePolicyValue::eSelf},
    {"geolocation", FeaturePolicyUtils::FeaturePolicyValue::eSelf},
    {"microphone", FeaturePolicyUtils::FeaturePolicyValue::eSelf},
    {"display-capture", FeaturePolicyUtils::FeaturePolicyValue::eSelf},
    {"fullscreen", FeaturePolicyUtils::FeaturePolicyValue::eSelf},
};

/*
 * This is experimental features list, which is disabled by default by pref
 * dom.security.featurePolicy.experimental.enabled.
 */
static FeatureMap sExperimentalFeatures[] = {
    // We don't support 'autoplay' for now, because it would be overwrote by
    // 'user-gesture-activation' policy. However, we can still keep it in the
    // list as we might start supporting it after we use different autoplay
    // policy.
    {"autoplay", FeaturePolicyUtils::FeaturePolicyValue::eAll},
    {"encrypted-media", FeaturePolicyUtils::FeaturePolicyValue::eAll},
    {"midi", FeaturePolicyUtils::FeaturePolicyValue::eSelf},
    {"payment", FeaturePolicyUtils::FeaturePolicyValue::eAll},
    {"document-domain", FeaturePolicyUtils::FeaturePolicyValue::eAll},
    // TODO: not supported yet!!!
    {"speaker", FeaturePolicyUtils::FeaturePolicyValue::eSelf},
    {"vr", FeaturePolicyUtils::FeaturePolicyValue::eAll},
    // https://immersive-web.github.io/webxr/#feature-policy
    {"xr-spatial-tracking", FeaturePolicyUtils::FeaturePolicyValue::eSelf},
};

/* static */
bool FeaturePolicyUtils::IsExperimentalFeature(const nsAString& aFeatureName) {
  uint32_t numFeatures =
      (sizeof(sExperimentalFeatures) / sizeof(sExperimentalFeatures[0]));
  for (uint32_t i = 0; i < numFeatures; ++i) {
    if (aFeatureName.LowerCaseEqualsASCII(
            sExperimentalFeatures[i].mFeatureName)) {
      return true;
    }
  }

  return false;
}

/* static */
bool FeaturePolicyUtils::IsSupportedFeature(const nsAString& aFeatureName) {
  uint32_t numFeatures =
      (sizeof(sSupportedFeatures) / sizeof(sSupportedFeatures[0]));
  for (uint32_t i = 0; i < numFeatures; ++i) {
    if (aFeatureName.LowerCaseEqualsASCII(sSupportedFeatures[i].mFeatureName)) {
      return true;
    }
  }

  return StaticPrefs::dom_security_featurePolicy_experimental_enabled() &&
         IsExperimentalFeature(aFeatureName);
}

/* static */
void FeaturePolicyUtils::ForEachFeature(
    const std::function<void(const char*)>& aCallback) {
  uint32_t numFeatures =
      (sizeof(sSupportedFeatures) / sizeof(sSupportedFeatures[0]));
  for (uint32_t i = 0; i < numFeatures; ++i) {
    aCallback(sSupportedFeatures[i].mFeatureName);
  }

  if (StaticPrefs::dom_security_featurePolicy_experimental_enabled()) {
    numFeatures =
        (sizeof(sExperimentalFeatures) / sizeof(sExperimentalFeatures[0]));
    for (uint32_t i = 0; i < numFeatures; ++i) {
      aCallback(sExperimentalFeatures[i].mFeatureName);
    }
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

  if (StaticPrefs::dom_security_featurePolicy_experimental_enabled()) {
    numFeatures =
        (sizeof(sExperimentalFeatures) / sizeof(sExperimentalFeatures[0]));
    for (uint32_t i = 0; i < numFeatures; ++i) {
      if (aFeatureName.LowerCaseEqualsASCII(
              sExperimentalFeatures[i].mFeatureName)) {
        return sExperimentalFeatures[i].mDefaultAllowList;
      }
    }
  }

  return FeaturePolicyValue::eNone;
}

static bool IsSameOriginAsTop(Document* aDocument) {
  MOZ_ASSERT(aDocument);

  BrowsingContext* browsingContext = aDocument->GetBrowsingContext();
  if (!browsingContext) {
    return false;
  }

  nsPIDOMWindowOuter* topWindow = browsingContext->Top()->GetDOMWindow();
  if (!topWindow) {
    // If we don't have a DOMWindow, We are not in same origin.
    return false;
  }

  Document* topLevelDocument = topWindow->GetExtantDoc();
  if (!topLevelDocument) {
    return false;
  }

  return NS_SUCCEEDED(
      nsContentUtils::CheckSameOrigin(topLevelDocument, aDocument));
}

/* static */
bool FeaturePolicyUtils::IsFeatureUnsafeAllowedAll(
    Document* aDocument, const nsAString& aFeatureName) {
  MOZ_ASSERT(aDocument);

  if (!StaticPrefs::dom_security_featurePolicy_enabled()) {
    return false;
  }

  if (!aDocument->IsHTMLDocument()) {
    return false;
  }

  FeaturePolicy* policy = aDocument->FeaturePolicy();
  MOZ_ASSERT(policy);

  return policy->HasFeatureUnsafeAllowsAll(aFeatureName) &&
         !policy->AllowsFeatureExplicitlyInAncestorChain(
             aFeatureName, policy->DefaultOrigin()) &&
         !IsSameOriginAsTop(aDocument);
}

/* static */
bool FeaturePolicyUtils::IsFeatureAllowed(Document* aDocument,
                                          const nsAString& aFeatureName) {
  MOZ_ASSERT(aDocument);

  if (!StaticPrefs::dom_security_featurePolicy_enabled()) {
    return true;
  }

  // Skip apply features in experimental pharse
  if (!StaticPrefs::dom_security_featurePolicy_experimental_enabled() &&
      IsExperimentalFeature(aFeatureName)) {
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
  nsCOMPtr<nsIURI> exposableURI = net::nsIOService::CreateExposableURI(uri);
  nsAutoCString spec;
  nsresult rv = exposableURI->GetSpec(spec);
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

namespace ipc {
void IPDLParamTraits<dom::FeaturePolicy*>::Write(IPC::Message* aMsg,
                                                 IProtocol* aActor,
                                                 dom::FeaturePolicy* aParam) {
  if (!aParam) {
    WriteIPDLParam(aMsg, aActor, false);
    return;
  }

  WriteIPDLParam(aMsg, aActor, true);

  dom::FeaturePolicyInfo info;
  info.defaultOrigin() = aParam->DefaultOrigin();
  info.selfOrigin() = aParam->GetSelfOrigin();
  info.srcOrigin() = aParam->GetSrcOrigin();

  aParam->GetDeclaredString(info.declaredString());
  aParam->GetInheritedDeniedFeatureNames(info.inheritedDeniedFeatureNames());

  WriteIPDLParam(aMsg, aActor, info);
}

bool IPDLParamTraits<dom::FeaturePolicy*>::Read(
    const IPC::Message* aMsg, PickleIterator* aIter, IProtocol* aActor,
    RefPtr<dom::FeaturePolicy>* aResult) {
  *aResult = nullptr;
  bool notnull = false;
  if (!ReadIPDLParam(aMsg, aIter, aActor, &notnull)) {
    return false;
  }

  if (!notnull) {
    return true;
  }

  dom::FeaturePolicyInfo info;
  if (!ReadIPDLParam(aMsg, aIter, aActor, &info)) {
    return false;
  }

  // Note that we only do IPC for feature policy to inherit poicy from parent
  // to child document. That does not need to bind feature policy with a node.
  RefPtr<dom::FeaturePolicy> featurePolicy = new dom::FeaturePolicy(nullptr);
  featurePolicy->SetDefaultOrigin(info.defaultOrigin());
  featurePolicy->SetInheritedDeniedFeatureNames(
      info.inheritedDeniedFeatureNames());

  nsString declaredString = info.declaredString();
  if (declaredString.IsEmpty() || !info.selfOrigin()) {
    *aResult = std::move(featurePolicy);
    return true;
  }
  featurePolicy->SetDeclaredPolicy(nullptr, declaredString, info.selfOrigin(),
                                   info.srcOrigin());
  *aResult = std::move(featurePolicy);
  return true;
}
}  // namespace ipc

}  // namespace mozilla
