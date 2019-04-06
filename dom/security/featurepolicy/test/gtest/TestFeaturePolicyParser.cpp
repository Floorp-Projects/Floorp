/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/dom/Feature.h"
#include "mozilla/dom/FeaturePolicyParser.h"
#include "nsNetUtil.h"
#include "nsTArray.h"

using namespace mozilla;
using namespace mozilla::dom;

#define URL_SELF NS_LITERAL_CSTRING("https://example.com")
#define URL_EXAMPLE_COM NS_LITERAL_CSTRING("http://example.com")
#define URL_EXAMPLE_NET NS_LITERAL_CSTRING("http://example.net")

void CheckParser(const nsAString& aInput, bool aExpectedResults,
                 uint32_t aExpectedFeatures,
                 nsTArray<Feature>& aParsedFeatures) {
  nsCOMPtr<nsIPrincipal> principal =
      mozilla::BasePrincipal::CreateCodebasePrincipal(URL_SELF);
  nsTArray<Feature> parsedFeatures;
  ASSERT_TRUE(FeaturePolicyParser::ParseString(aInput, nullptr, principal,
                                               principal, parsedFeatures) ==
              aExpectedResults);
  ASSERT_TRUE(parsedFeatures.Length() == aExpectedFeatures);

  parsedFeatures.SwapElements(aParsedFeatures);
}

TEST(FeaturePolicyParser, Basic)
{
  nsCOMPtr<nsIPrincipal> selfPrincipal =
      mozilla::BasePrincipal::CreateCodebasePrincipal(URL_SELF);
  nsCOMPtr<nsIPrincipal> exampleComPrincipal =
      mozilla::BasePrincipal::CreateCodebasePrincipal(URL_EXAMPLE_COM);
  nsCOMPtr<nsIPrincipal> exampleNetPrincipal =
      mozilla::BasePrincipal::CreateCodebasePrincipal(URL_EXAMPLE_NET);

  nsTArray<Feature> parsedFeatures;

  // Empty string is a valid policy.
  CheckParser(EmptyString(), true, 0, parsedFeatures);

  // Empty string with spaces is still valid.
  CheckParser(NS_LITERAL_STRING("   "), true, 0, parsedFeatures);

  // Non-Existing features with no allowed values
  CheckParser(NS_LITERAL_STRING("non-existing-feature"), true, 0,
              parsedFeatures);
  CheckParser(NS_LITERAL_STRING("non-existing-feature;another-feature"), true,
              0, parsedFeatures);

  // Existing feature with no allowed values
  CheckParser(NS_LITERAL_STRING("camera"), true, 1, parsedFeatures);
  ASSERT_TRUE(parsedFeatures[0].Name().Equals(NS_LITERAL_STRING("camera")));
  ASSERT_TRUE(parsedFeatures[0].HasAllowList());

  // Some spaces.
  CheckParser(NS_LITERAL_STRING(" camera "), true, 1, parsedFeatures);
  ASSERT_TRUE(parsedFeatures[0].Name().Equals(NS_LITERAL_STRING("camera")));
  ASSERT_TRUE(parsedFeatures[0].HasAllowList());

  // A random ;
  CheckParser(NS_LITERAL_STRING("camera;"), true, 1, parsedFeatures);
  ASSERT_TRUE(parsedFeatures[0].Name().Equals(NS_LITERAL_STRING("camera")));
  ASSERT_TRUE(parsedFeatures[0].HasAllowList());

  // Another random ;
  CheckParser(NS_LITERAL_STRING(";camera;"), true, 1, parsedFeatures);
  ASSERT_TRUE(parsedFeatures[0].Name().Equals(NS_LITERAL_STRING("camera")));
  ASSERT_TRUE(parsedFeatures[0].HasAllowList());

  // 2 features
  CheckParser(NS_LITERAL_STRING("camera;microphone"), true, 2, parsedFeatures);
  ASSERT_TRUE(parsedFeatures[0].Name().Equals(NS_LITERAL_STRING("camera")));
  ASSERT_TRUE(parsedFeatures[0].HasAllowList());
  ASSERT_TRUE(parsedFeatures[1].Name().Equals(NS_LITERAL_STRING("microphone")));
  ASSERT_TRUE(parsedFeatures[1].HasAllowList());

  // 2 features with spaces
  CheckParser(NS_LITERAL_STRING(" camera ; microphone "), true, 2,
              parsedFeatures);
  ASSERT_TRUE(parsedFeatures[0].Name().Equals(NS_LITERAL_STRING("camera")));
  ASSERT_TRUE(parsedFeatures[0].HasAllowList());
  ASSERT_TRUE(parsedFeatures[1].Name().Equals(NS_LITERAL_STRING("microphone")));
  ASSERT_TRUE(parsedFeatures[1].HasAllowList());

  // 3 features, but only 2 exist.
  CheckParser(NS_LITERAL_STRING("camera;microphone;foobar"), true, 2,
              parsedFeatures);
  ASSERT_TRUE(parsedFeatures[0].Name().Equals(NS_LITERAL_STRING("camera")));
  ASSERT_TRUE(parsedFeatures[0].HasAllowList());
  ASSERT_TRUE(parsedFeatures[1].Name().Equals(NS_LITERAL_STRING("microphone")));
  ASSERT_TRUE(parsedFeatures[1].HasAllowList());

  // Multiple spaces around the value
  CheckParser(NS_LITERAL_STRING("camera      'self'"), true, 1, parsedFeatures);
  ASSERT_TRUE(parsedFeatures[0].Name().Equals(NS_LITERAL_STRING("camera")));
  ASSERT_TRUE(parsedFeatures[0].AllowListContains(selfPrincipal));

  // Multiple spaces around the value
  CheckParser(NS_LITERAL_STRING("camera      'self'    "), true, 1,
              parsedFeatures);
  ASSERT_TRUE(parsedFeatures[0].Name().Equals(NS_LITERAL_STRING("camera")));
  ASSERT_TRUE(parsedFeatures[0].AllowListContains(selfPrincipal));

  // No final '
  CheckParser(NS_LITERAL_STRING("camera      'self"), true, 1, parsedFeatures);
  ASSERT_TRUE(parsedFeatures[0].Name().Equals(NS_LITERAL_STRING("camera")));
  ASSERT_TRUE(parsedFeatures[0].HasAllowList());
  ASSERT_TRUE(!parsedFeatures[0].AllowListContains(selfPrincipal));

  // Lowercase/Uppercase
  CheckParser(NS_LITERAL_STRING("camera      'selF'"), true, 1, parsedFeatures);
  ASSERT_TRUE(parsedFeatures[0].Name().Equals(NS_LITERAL_STRING("camera")));
  ASSERT_TRUE(parsedFeatures[0].AllowListContains(selfPrincipal));

  // Lowercase/Uppercase
  CheckParser(NS_LITERAL_STRING("camera * 'self' none' a.com 123"), true, 1,
              parsedFeatures);
  ASSERT_TRUE(parsedFeatures[0].Name().Equals(NS_LITERAL_STRING("camera")));
  ASSERT_TRUE(parsedFeatures[0].AllowsAll());

  // After a 'none' we don't continue the parsing.
  CheckParser(NS_LITERAL_STRING("camera 'none' a.com b.org c.net d.co.uk"),
              true, 1, parsedFeatures);
  ASSERT_TRUE(parsedFeatures[0].Name().Equals(NS_LITERAL_STRING("camera")));
  ASSERT_TRUE(parsedFeatures[0].AllowsNone());

  // After a * we don't continue the parsing.
  CheckParser(NS_LITERAL_STRING("camera * a.com b.org c.net d.co.uk"), true, 1,
              parsedFeatures);
  ASSERT_TRUE(parsedFeatures[0].Name().Equals(NS_LITERAL_STRING("camera")));
  ASSERT_TRUE(parsedFeatures[0].AllowsAll());

  // 'self'
  CheckParser(NS_LITERAL_STRING("camera 'self'"), true, 1, parsedFeatures);
  ASSERT_TRUE(parsedFeatures[0].Name().Equals(NS_LITERAL_STRING("camera")));
  ASSERT_TRUE(parsedFeatures[0].AllowListContains(selfPrincipal));

  // A couple of URLs
  CheckParser(NS_LITERAL_STRING("camera http://example.com http://example.net"),
              true, 1, parsedFeatures);
  ASSERT_TRUE(parsedFeatures[0].Name().Equals(NS_LITERAL_STRING("camera")));
  ASSERT_TRUE(!parsedFeatures[0].AllowListContains(selfPrincipal));
  ASSERT_TRUE(parsedFeatures[0].AllowListContains(exampleComPrincipal));
  ASSERT_TRUE(parsedFeatures[0].AllowListContains(exampleNetPrincipal));

  // A couple of URLs + self
  CheckParser(
      NS_LITERAL_STRING("camera http://example.com 'self' http://example.net"),
      true, 1, parsedFeatures);
  ASSERT_TRUE(parsedFeatures[0].Name().Equals(NS_LITERAL_STRING("camera")));
  ASSERT_TRUE(parsedFeatures[0].AllowListContains(selfPrincipal));
  ASSERT_TRUE(parsedFeatures[0].AllowListContains(exampleComPrincipal));
  ASSERT_TRUE(parsedFeatures[0].AllowListContains(exampleNetPrincipal));

  // A couple of URLs but then *
  CheckParser(NS_LITERAL_STRING(
                  "camera http://example.com 'self' http://example.net *"),
              true, 1, parsedFeatures);
  ASSERT_TRUE(parsedFeatures[0].Name().Equals(NS_LITERAL_STRING("camera")));
  ASSERT_TRUE(parsedFeatures[0].AllowsAll());
}
