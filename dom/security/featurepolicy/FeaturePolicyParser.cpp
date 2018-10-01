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

using namespace mozilla;
using namespace mozilla::dom;

namespace {

void
ReportToConsoleUnsupportedFeature(nsIDocument* aDocument,
                                  const nsString& aFeatureName)
{
  const char16_t* params[] = { aFeatureName.get() };

  nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                  NS_LITERAL_CSTRING("Feature Policy"),
                                  aDocument,
                                  nsContentUtils::eSECURITY_PROPERTIES,
                                  "FeaturePolicyUnsupportedFeatureName",
                                  params, ArrayLength(params));
}

void
ReportToConsoleInvalidEmptyAllowValue(nsIDocument* aDocument,
                                      const nsString& aFeatureName)
{
  const char16_t* params[] = { aFeatureName.get() };

  nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                  NS_LITERAL_CSTRING("Feature Policy"),
                                  aDocument,
                                  nsContentUtils::eSECURITY_PROPERTIES,
                                  "FeaturePolicyInvalidEmptyAllowValue",
                                  params, ArrayLength(params));
}

void
ReportToConsoleInvalidAllowValue(nsIDocument* aDocument,
                                 const nsString& aValue)
{
  const char16_t* params[] = { aValue.get() };

  nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                  NS_LITERAL_CSTRING("Feature Policy"),
                                  aDocument,
                                  nsContentUtils::eSECURITY_PROPERTIES,
                                  "FeaturePolicyInvalidAllowValue",
                                  params, ArrayLength(params));
}

} // anonymous

/* static */ bool
FeaturePolicyParser::ParseString(const nsAString& aPolicy,
                                 nsIDocument* aDocument,
                                 const nsAString& aSelfOrigin,
                                 const nsAString& aSrcOrigin,
                                 bool aSrcEnabled,
                                 nsTArray<Feature>& aParsedFeatures)
{
  nsTArray<nsTArray<nsString>> tokens;
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
      if (aSrcEnabled) {
        // Note that this src origin can be empty if opaque.
        feature.AppendOriginToWhiteList(aSrcOrigin);
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
          // Opaque origins are passed as empty string.
          if (!aSelfOrigin.IsEmpty()) {
            feature.AppendOriginToWhiteList(aSelfOrigin);
          }
          continue;
        }

        if (aSrcEnabled && curVal.LowerCaseEqualsASCII("'src'")) {
          // Opaque origins are passed as empty string.
          if (!aSrcOrigin.IsEmpty()) {
            feature.AppendOriginToWhiteList(aSrcOrigin);
          }
          continue;
        }

        nsCOMPtr<nsIURI> uri;
        nsresult rv = NS_NewURI(getter_AddRefs(uri), curVal);
        if (NS_FAILED(rv)) {
          ReportToConsoleInvalidAllowValue(aDocument, curVal);
          continue;
        }

        nsAutoString origin;
        rv = nsContentUtils::GetUTFOrigin(uri, origin);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          ReportToConsoleInvalidAllowValue(aDocument, curVal);
          continue;
        }

        feature.AppendOriginToWhiteList(origin);
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
