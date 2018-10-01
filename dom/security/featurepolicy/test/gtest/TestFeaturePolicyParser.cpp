/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/dom/Feature.h"
#include "mozilla/dom/FeaturePolicyParser.h"
#include "nsNetUtil.h"
#include "nsTArray.h"

using namespace mozilla::dom;

void
CheckParser(const nsAString& aInput, bool aExpectedResults,
            uint32_t aExpectedFeatures, nsTArray<Feature>& aParsedFeatures)
{
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri),
                          NS_LITERAL_STRING("https://example.com"));
  ASSERT_EQ(NS_OK, rv) << "NS_NewURI works.";

  nsTArray<Feature> parsedFeatures;
  ASSERT_TRUE(FeaturePolicyParser::ParseString(aInput, nullptr, uri,
                                               parsedFeatures) ==
                aExpectedResults);
  ASSERT_TRUE(parsedFeatures.Length() == aExpectedFeatures);

  parsedFeatures.SwapElements(aParsedFeatures);
}

TEST(FeaturePolicyParser, Basic)
{
  nsCOMPtr<nsIURI> uriSelf;
  nsresult rv = NS_NewURI(getter_AddRefs(uriSelf),
                          NS_LITERAL_STRING("https://example.com"));
  ASSERT_EQ(NS_OK, rv) << "NS_NewURI works.";

  nsCOMPtr<nsIURI> uriExampleCom;
  rv = NS_NewURI(getter_AddRefs(uriExampleCom),
                 NS_LITERAL_STRING("http://example.com"));
  ASSERT_EQ(NS_OK, rv) << "NS_NewURI works.";

  nsCOMPtr<nsIURI> uriExampleNet;
  rv = NS_NewURI(getter_AddRefs(uriExampleNet),
                 NS_LITERAL_STRING("http://example.net"));
  ASSERT_EQ(NS_OK, rv) << "NS_NewURI works.";

  nsTArray<Feature> parsedFeatures;

  // Empty string is a valid policy.
  CheckParser(EmptyString(), true, 0, parsedFeatures);

  // Empty string with spaces is still valid.
  CheckParser(NS_LITERAL_STRING("   "), true, 0, parsedFeatures);

  // Non-Existing features with no allowed values
  CheckParser(NS_LITERAL_STRING("non-existing-feature"), true, 0, parsedFeatures);
  CheckParser(NS_LITERAL_STRING("non-existing-feature;another-feature"), true,
              0, parsedFeatures);

  // Existing feature with no allowed values
  CheckParser(NS_LITERAL_STRING("camera"), true, 1, parsedFeatures);
  ASSERT_TRUE(parsedFeatures[0].Name().Equals(NS_LITERAL_STRING("camera")));
  ASSERT_TRUE(parsedFeatures[0].IsWhiteList());

  // Some spaces.
  CheckParser(NS_LITERAL_STRING(" camera "), true, 1, parsedFeatures);
  ASSERT_TRUE(parsedFeatures[0].Name().Equals(NS_LITERAL_STRING("camera")));
  ASSERT_TRUE(parsedFeatures[0].IsWhiteList());

  // A random ;
  CheckParser(NS_LITERAL_STRING("camera;"), true, 1, parsedFeatures);
  ASSERT_TRUE(parsedFeatures[0].Name().Equals(NS_LITERAL_STRING("camera")));
  ASSERT_TRUE(parsedFeatures[0].IsWhiteList());

  // Another random ;
  CheckParser(NS_LITERAL_STRING(";camera;"), true, 1, parsedFeatures);
  ASSERT_TRUE(parsedFeatures[0].Name().Equals(NS_LITERAL_STRING("camera")));
  ASSERT_TRUE(parsedFeatures[0].IsWhiteList());

  // 2 features
  CheckParser(NS_LITERAL_STRING("camera;microphone"), true, 2, parsedFeatures);
  ASSERT_TRUE(parsedFeatures[0].Name().Equals(NS_LITERAL_STRING("camera")));
  ASSERT_TRUE(parsedFeatures[0].IsWhiteList());
  ASSERT_TRUE(parsedFeatures[1].Name().Equals(NS_LITERAL_STRING("microphone")));
  ASSERT_TRUE(parsedFeatures[1].IsWhiteList());

  // 2 features with spaces
  CheckParser(NS_LITERAL_STRING(" camera ; microphone "), true, 2,
              parsedFeatures);
  ASSERT_TRUE(parsedFeatures[0].Name().Equals(NS_LITERAL_STRING("camera")));
  ASSERT_TRUE(parsedFeatures[0].IsWhiteList());
  ASSERT_TRUE(parsedFeatures[1].Name().Equals(NS_LITERAL_STRING("microphone")));
  ASSERT_TRUE(parsedFeatures[1].IsWhiteList());

  // 3 features, but only 2 exist.
  CheckParser(NS_LITERAL_STRING("camera;microphone;foobar"), true, 2,
              parsedFeatures);
  ASSERT_TRUE(parsedFeatures[0].Name().Equals(NS_LITERAL_STRING("camera")));
  ASSERT_TRUE(parsedFeatures[0].IsWhiteList());
  ASSERT_TRUE(parsedFeatures[1].Name().Equals(NS_LITERAL_STRING("microphone")));
  ASSERT_TRUE(parsedFeatures[1].IsWhiteList());

  // Multiple spaces around the value
  CheckParser(NS_LITERAL_STRING("camera      'self'"), true, 1,
              parsedFeatures);
  ASSERT_TRUE(parsedFeatures[0].Name().Equals(NS_LITERAL_STRING("camera")));
  ASSERT_TRUE(parsedFeatures[0].WhiteListContains(uriSelf));

  // Multiple spaces around the value
  CheckParser(NS_LITERAL_STRING("camera      'self'    "), true, 1,
              parsedFeatures);
  ASSERT_TRUE(parsedFeatures[0].Name().Equals(NS_LITERAL_STRING("camera")));
  ASSERT_TRUE(parsedFeatures[0].WhiteListContains(uriSelf));

  // No final '
  CheckParser(NS_LITERAL_STRING("camera      'self"), true, 1,
              parsedFeatures);
  ASSERT_TRUE(parsedFeatures[0].Name().Equals(NS_LITERAL_STRING("camera")));
  ASSERT_TRUE(parsedFeatures[0].IsWhiteList());
  ASSERT_TRUE(!parsedFeatures[0].WhiteListContains(uriSelf));

  // Lowercase/Uppercase
  CheckParser(NS_LITERAL_STRING("camera      'selF'"), true, 1,
              parsedFeatures);
  ASSERT_TRUE(parsedFeatures[0].Name().Equals(NS_LITERAL_STRING("camera")));
  ASSERT_TRUE(parsedFeatures[0].WhiteListContains(uriSelf));

  // Lowercase/Uppercase
  CheckParser(NS_LITERAL_STRING("camera * 'self' none' a.com 123"), true, 1,
              parsedFeatures);
  ASSERT_TRUE(parsedFeatures[0].Name().Equals(NS_LITERAL_STRING("camera")));
  ASSERT_TRUE(parsedFeatures[0].AllowsAll());

  // After a 'none' we don't continue the parsing.
  CheckParser(NS_LITERAL_STRING("camera 'none' a.com b.org c.net d.co.uk"), true, 1,
              parsedFeatures);
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
  ASSERT_TRUE(parsedFeatures[0].WhiteListContains(uriSelf));

  // A couple of URLs
  CheckParser(NS_LITERAL_STRING("camera http://example.com http://example.net"), true, 1,
              parsedFeatures);
  ASSERT_TRUE(parsedFeatures[0].Name().Equals(NS_LITERAL_STRING("camera")));
  ASSERT_TRUE(!parsedFeatures[0].WhiteListContains(uriSelf));
  ASSERT_TRUE(parsedFeatures[0].WhiteListContains(uriExampleCom));
  ASSERT_TRUE(parsedFeatures[0].WhiteListContains(uriExampleNet));

  // A couple of URLs + self
  CheckParser(NS_LITERAL_STRING("camera http://example.com 'self' http://example.net"), true, 1,
              parsedFeatures);
  ASSERT_TRUE(parsedFeatures[0].Name().Equals(NS_LITERAL_STRING("camera")));
  ASSERT_TRUE(parsedFeatures[0].WhiteListContains(uriSelf));
  ASSERT_TRUE(parsedFeatures[0].WhiteListContains(uriExampleCom));
  ASSERT_TRUE(parsedFeatures[0].WhiteListContains(uriExampleNet));

  // A couple of URLs but then *
  CheckParser(NS_LITERAL_STRING("camera http://example.com 'self' http://example.net *"), true, 1,
              parsedFeatures);
  ASSERT_TRUE(parsedFeatures[0].Name().Equals(NS_LITERAL_STRING("camera")));
  ASSERT_TRUE(parsedFeatures[0].AllowsAll());
}
