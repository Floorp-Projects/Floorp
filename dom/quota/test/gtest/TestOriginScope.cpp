/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/dom/quota/OriginScope.h"
#include "nsStringFwd.h"
#include "nsTDependentString.h"
#include "nsTLiteralString.h"

namespace mozilla::dom::quota {

namespace {

struct OriginTest {
  const char* mOrigin;
  bool mMatch;
};

void CheckOriginScopeMatchesOrigin(const OriginScope& aOriginScope,
                                   const char* aOrigin, bool aMatch) {
  bool result = aOriginScope.Matches(
      OriginScope::FromOrigin(nsDependentCString(aOrigin)));

  EXPECT_TRUE(result == aMatch);
}

}  // namespace

TEST(DOM_Quota_OriginScope, SanityChecks)
{
  OriginScope originScope;

  // Sanity checks.

  {
    constexpr auto origin = "http://www.mozilla.org"_ns;
    originScope.SetFromOrigin(origin);
    EXPECT_TRUE(originScope.IsOrigin());
    EXPECT_TRUE(originScope.GetOrigin().Equals(origin));
    EXPECT_TRUE(originScope.GetOriginNoSuffix().Equals(origin));
  }

  {
    constexpr auto prefix = "http://www.mozilla.org"_ns;
    originScope.SetFromPrefix(prefix);
    EXPECT_TRUE(originScope.IsPrefix());
    EXPECT_TRUE(originScope.GetOriginNoSuffix().Equals(prefix));
  }

  {
    originScope.SetFromNull();
    EXPECT_TRUE(originScope.IsNull());
  }
}

TEST(DOM_Quota_OriginScope, MatchesOrigin)
{
  OriginScope originScope;

  // Test each origin scope type against particular origins.

  {
    originScope.SetFromOrigin("http://www.mozilla.org"_ns);

    static const OriginTest tests[] = {
        {"http://www.mozilla.org", true},
        {"http://www.example.org", false},
    };

    for (const auto& test : tests) {
      CheckOriginScopeMatchesOrigin(originScope, test.mOrigin, test.mMatch);
    }
  }

  {
    originScope.SetFromPrefix("http://www.mozilla.org"_ns);

    static const OriginTest tests[] = {
        {"http://www.mozilla.org", true},
        {"http://www.mozilla.org^userContextId=1", true},
        {"http://www.example.org^userContextId=1", false},
    };

    for (const auto& test : tests) {
      CheckOriginScopeMatchesOrigin(originScope, test.mOrigin, test.mMatch);
    }
  }

  {
    originScope.SetFromNull();

    static const OriginTest tests[] = {
        {"http://www.mozilla.org", true},
        {"http://www.mozilla.org^userContextId=1", true},
        {"http://www.example.org^userContextId=1", true},
    };

    for (const auto& test : tests) {
      CheckOriginScopeMatchesOrigin(originScope, test.mOrigin, test.mMatch);
    }
  }
}

}  //  namespace mozilla::dom::quota
