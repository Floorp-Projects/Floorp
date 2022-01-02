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

#define URL_SELF "https://example.com"_ns
#define URL_EXAMPLE_COM "http://example.com"_ns
#define URL_EXAMPLE_NET "http://example.net"_ns

void CheckParser(const nsAString& aInput, bool aExpectedResults,
                 uint32_t aExpectedFeatures,
                 nsTArray<Feature>& aParsedFeatures) {
  nsCOMPtr<nsIPrincipal> principal =
      mozilla::BasePrincipal::CreateContentPrincipal(URL_SELF);
  nsTArray<Feature> parsedFeatures;
  ASSERT_TRUE(FeaturePolicyParser::ParseString(aInput, nullptr, principal,
                                               principal, parsedFeatures) ==
              aExpectedResults);
  ASSERT_TRUE(parsedFeatures.Length() == aExpectedFeatures);

  aParsedFeatures = std::move(parsedFeatures);
}

TEST(FeaturePolicyParser, Basic)
{
  nsCOMPtr<nsIPrincipal> selfPrincipal =
      mozilla::BasePrincipal::CreateContentPrincipal(URL_SELF);
  nsCOMPtr<nsIPrincipal> exampleComPrincipal =
      mozilla::BasePrincipal::CreateContentPrincipal(URL_EXAMPLE_COM);
  nsCOMPtr<nsIPrincipal> exampleNetPrincipal =
      mozilla::BasePrincipal::CreateContentPrincipal(URL_EXAMPLE_NET);

  nsTArray<Feature> parsedFeatures;

  // Empty string is a valid policy.
  CheckParser(u""_ns, true, 0, parsedFeatures);

  // Empty string with spaces is still valid.
  CheckParser(u"   "_ns, true, 0, parsedFeatures);

  // Non-Existing features with no allowed values
  CheckParser(u"non-existing-feature"_ns, true, 0, parsedFeatures);
  CheckParser(u"non-existing-feature;another-feature"_ns, true, 0,
              parsedFeatures);

  // Existing feature with no allowed values
  CheckParser(u"camera"_ns, true, 1, parsedFeatures);
  ASSERT_TRUE(parsedFeatures[0].Name().Equals(u"camera"_ns));
  ASSERT_TRUE(parsedFeatures[0].HasAllowList());

  // Some spaces.
  CheckParser(u" camera "_ns, true, 1, parsedFeatures);
  ASSERT_TRUE(parsedFeatures[0].Name().Equals(u"camera"_ns));
  ASSERT_TRUE(parsedFeatures[0].HasAllowList());

  // A random ;
  CheckParser(u"camera;"_ns, true, 1, parsedFeatures);
  ASSERT_TRUE(parsedFeatures[0].Name().Equals(u"camera"_ns));
  ASSERT_TRUE(parsedFeatures[0].HasAllowList());

  // Another random ;
  CheckParser(u";camera;"_ns, true, 1, parsedFeatures);
  ASSERT_TRUE(parsedFeatures[0].Name().Equals(u"camera"_ns));
  ASSERT_TRUE(parsedFeatures[0].HasAllowList());

  // 2 features
  CheckParser(u"camera;microphone"_ns, true, 2, parsedFeatures);
  ASSERT_TRUE(parsedFeatures[0].Name().Equals(u"camera"_ns));
  ASSERT_TRUE(parsedFeatures[0].HasAllowList());
  ASSERT_TRUE(parsedFeatures[1].Name().Equals(u"microphone"_ns));
  ASSERT_TRUE(parsedFeatures[1].HasAllowList());

  // 2 features with spaces
  CheckParser(u" camera ; microphone "_ns, true, 2, parsedFeatures);
  ASSERT_TRUE(parsedFeatures[0].Name().Equals(u"camera"_ns));
  ASSERT_TRUE(parsedFeatures[0].HasAllowList());
  ASSERT_TRUE(parsedFeatures[1].Name().Equals(u"microphone"_ns));
  ASSERT_TRUE(parsedFeatures[1].HasAllowList());

  // 3 features, but only 2 exist.
  CheckParser(u"camera;microphone;foobar"_ns, true, 2, parsedFeatures);
  ASSERT_TRUE(parsedFeatures[0].Name().Equals(u"camera"_ns));
  ASSERT_TRUE(parsedFeatures[0].HasAllowList());
  ASSERT_TRUE(parsedFeatures[1].Name().Equals(u"microphone"_ns));
  ASSERT_TRUE(parsedFeatures[1].HasAllowList());

  // Multiple spaces around the value
  CheckParser(u"camera      'self'"_ns, true, 1, parsedFeatures);
  ASSERT_TRUE(parsedFeatures[0].Name().Equals(u"camera"_ns));
  ASSERT_TRUE(parsedFeatures[0].AllowListContains(selfPrincipal));

  // Multiple spaces around the value
  CheckParser(u"camera      'self'    "_ns, true, 1, parsedFeatures);
  ASSERT_TRUE(parsedFeatures[0].Name().Equals(u"camera"_ns));
  ASSERT_TRUE(parsedFeatures[0].AllowListContains(selfPrincipal));

  // No final '
  CheckParser(u"camera      'self"_ns, true, 1, parsedFeatures);
  ASSERT_TRUE(parsedFeatures[0].Name().Equals(u"camera"_ns));
  ASSERT_TRUE(parsedFeatures[0].HasAllowList());
  ASSERT_TRUE(!parsedFeatures[0].AllowListContains(selfPrincipal));

  // Lowercase/Uppercase
  CheckParser(u"camera      'selF'"_ns, true, 1, parsedFeatures);
  ASSERT_TRUE(parsedFeatures[0].Name().Equals(u"camera"_ns));
  ASSERT_TRUE(parsedFeatures[0].AllowListContains(selfPrincipal));

  // Lowercase/Uppercase
  CheckParser(u"camera * 'self' none' a.com 123"_ns, true, 1, parsedFeatures);
  ASSERT_TRUE(parsedFeatures[0].Name().Equals(u"camera"_ns));
  ASSERT_TRUE(parsedFeatures[0].AllowsAll());

  // After a 'none' we don't continue the parsing.
  CheckParser(u"camera 'none' a.com b.org c.net d.co.uk"_ns, true, 1,
              parsedFeatures);
  ASSERT_TRUE(parsedFeatures[0].Name().Equals(u"camera"_ns));
  ASSERT_TRUE(parsedFeatures[0].AllowsNone());

  // After a * we don't continue the parsing.
  CheckParser(u"camera * a.com b.org c.net d.co.uk"_ns, true, 1,
              parsedFeatures);
  ASSERT_TRUE(parsedFeatures[0].Name().Equals(u"camera"_ns));
  ASSERT_TRUE(parsedFeatures[0].AllowsAll());

  // 'self'
  CheckParser(u"camera 'self'"_ns, true, 1, parsedFeatures);
  ASSERT_TRUE(parsedFeatures[0].Name().Equals(u"camera"_ns));
  ASSERT_TRUE(parsedFeatures[0].AllowListContains(selfPrincipal));

  // A couple of URLs
  CheckParser(u"camera http://example.com http://example.net"_ns, true, 1,
              parsedFeatures);
  ASSERT_TRUE(parsedFeatures[0].Name().Equals(u"camera"_ns));
  ASSERT_TRUE(!parsedFeatures[0].AllowListContains(selfPrincipal));
  ASSERT_TRUE(parsedFeatures[0].AllowListContains(exampleComPrincipal));
  ASSERT_TRUE(parsedFeatures[0].AllowListContains(exampleNetPrincipal));

  // A couple of URLs + self
  CheckParser(u"camera http://example.com 'self' http://example.net"_ns, true,
              1, parsedFeatures);
  ASSERT_TRUE(parsedFeatures[0].Name().Equals(u"camera"_ns));
  ASSERT_TRUE(parsedFeatures[0].AllowListContains(selfPrincipal));
  ASSERT_TRUE(parsedFeatures[0].AllowListContains(exampleComPrincipal));
  ASSERT_TRUE(parsedFeatures[0].AllowListContains(exampleNetPrincipal));

  // A couple of URLs but then *
  CheckParser(u"camera http://example.com 'self' http://example.net *"_ns, true,
              1, parsedFeatures);
  ASSERT_TRUE(parsedFeatures[0].Name().Equals(u"camera"_ns));
  ASSERT_TRUE(parsedFeatures[0].AllowsAll());
}
