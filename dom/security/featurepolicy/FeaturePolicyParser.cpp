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
                                 nsIURI* aSelfURI,
                                 nsTArray<Feature>& aParsedFeatures)
{
  MOZ_ASSERT(aSelfURI);

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
        feature.AppendURIToWhiteList(aSelfURI);
        continue;
      }

      nsCOMPtr<nsIURI> uri;
      nsresult rv = NS_NewURI(getter_AddRefs(uri), curVal);
      if (NS_FAILED(rv)) {
        ReportToConsoleInvalidAllowValue(aDocument, curVal);
        continue;
      }

      feature.AppendURIToWhiteList(uri);
    }

    parsedFeatures.AppendElement(feature);
  }

  aParsedFeatures.SwapElements(parsedFeatures);
  return true;
}
