/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FeaturePolicyParser.h"

#include "mozilla/dom/Feature.h"
#include "mozilla/dom/FeaturePolicyUtils.h"
#include "mozilla/dom/PolicyTokenizer.h"
#include "nsIScriptError.h"
#include "nsIURI.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace dom {

namespace {

void ReportToConsoleUnsupportedFeature(Document* aDocument,
                                       const nsString& aFeatureName) {
  if (!aDocument) {
    return;
  }

  AutoTArray<nsString, 1> params = {aFeatureName};

  nsContentUtils::ReportToConsole(
      nsIScriptError::warningFlag, NS_LITERAL_CSTRING("Feature Policy"),
      aDocument, nsContentUtils::eSECURITY_PROPERTIES,
      "FeaturePolicyUnsupportedFeatureName", params);
}

void ReportToConsoleInvalidEmptyAllowValue(Document* aDocument,
                                           const nsString& aFeatureName) {
  if (!aDocument) {
    return;
  }

  AutoTArray<nsString, 1> params = {aFeatureName};

  nsContentUtils::ReportToConsole(
      nsIScriptError::warningFlag, NS_LITERAL_CSTRING("Feature Policy"),
      aDocument, nsContentUtils::eSECURITY_PROPERTIES,
      "FeaturePolicyInvalidEmptyAllowValue", params);
}

void ReportToConsoleInvalidAllowValue(Document* aDocument,
                                      const nsString& aValue) {
  if (!aDocument) {
    return;
  }

  AutoTArray<nsString, 1> params = {aValue};

  nsContentUtils::ReportToConsole(
      nsIScriptError::warningFlag, NS_LITERAL_CSTRING("Feature Policy"),
      aDocument, nsContentUtils::eSECURITY_PROPERTIES,
      "FeaturePolicyInvalidAllowValue", params);
}

}  // namespace

/* static */
bool FeaturePolicyParser::ParseString(const nsAString& aPolicy,
                                      Document* aDocument,
                                      nsIPrincipal* aSelfOrigin,
                                      nsIPrincipal* aSrcOrigin,
                                      nsTArray<Feature>& aParsedFeatures) {
  MOZ_ASSERT(aSelfOrigin);

  nsTArray<CopyableTArray<nsString>> tokens;
  PolicyTokenizer::tokenizePolicy(aPolicy, tokens);

  nsTArray<Feature> parsedFeatures;

  for (const nsTArray<nsString>& featureTokens : tokens) {
    if (featureTokens.IsEmpty()) {
      continue;
    }

    if (!FeaturePolicyUtils::IsSupportedFeature(featureTokens[0])) {
      ReportToConsoleUnsupportedFeature(aDocument, featureTokens[0]);
      continue;
    }

    Feature feature(featureTokens[0]);

    if (featureTokens.Length() == 1) {
      if (aSrcOrigin) {
        feature.AppendToAllowList(aSrcOrigin);
      } else {
        ReportToConsoleInvalidEmptyAllowValue(aDocument, featureTokens[0]);
        continue;
      }
    } else {
      // we gotta start at 1 here
      for (uint32_t i = 1; i < featureTokens.Length(); ++i) {
        const nsString& curVal = featureTokens[i];
        if (curVal.LowerCaseEqualsASCII("'none'")) {
          feature.SetAllowsNone();
          break;
        }

        if (curVal.EqualsLiteral("*")) {
          feature.SetAllowsAll();
          break;
        }

        if (curVal.LowerCaseEqualsASCII("'self'")) {
          feature.AppendToAllowList(aSelfOrigin);
          continue;
        }

        if (aSrcOrigin && curVal.LowerCaseEqualsASCII("'src'")) {
          feature.AppendToAllowList(aSrcOrigin);
          continue;
        }

        nsCOMPtr<nsIURI> uri;
        nsresult rv = NS_NewURI(getter_AddRefs(uri), curVal);
        if (NS_FAILED(rv)) {
          ReportToConsoleInvalidAllowValue(aDocument, curVal);
          continue;
        }

        nsCOMPtr<nsIPrincipal> origin = BasePrincipal::CreateContentPrincipal(
            uri, BasePrincipal::Cast(aSelfOrigin)->OriginAttributesRef());
        if (NS_WARN_IF(!origin)) {
          ReportToConsoleInvalidAllowValue(aDocument, curVal);
          continue;
        }

        feature.AppendToAllowList(origin);
      }
    }

    // No duplicate!
    bool found = false;
    for (const Feature& parsedFeature : parsedFeatures) {
      if (parsedFeature.Name() == feature.Name()) {
        found = true;
        break;
      }
    }

    if (!found) {
      parsedFeatures.AppendElement(feature);
    }
  }

  aParsedFeatures.SwapElements(parsedFeatures);
  return true;
}

}  // namespace dom
}  // namespace mozilla
