/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gtest/gtest.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/Preferences.h"
#include "nsNetUtil.h"

using mozilla::OriginAttributes;
using mozilla::Preferences;

static void TestSuffix(const OriginAttributes& attrs) {
  nsAutoCString suffix;
  attrs.CreateSuffix(suffix);

  OriginAttributes attrsFromSuffix;
  bool success = attrsFromSuffix.PopulateFromSuffix(suffix);
  EXPECT_TRUE(success);

  EXPECT_EQ(attrs, attrsFromSuffix);
}

static void TestFPD(const nsAString& spec, const nsAString& fpd) {
  OriginAttributes attrs;
  nsCOMPtr<nsIURI> url;
  ASSERT_EQ(NS_NewURI(getter_AddRefs(url), spec), NS_OK);
  attrs.SetFirstPartyDomain(true, url);
  EXPECT_TRUE(attrs.mFirstPartyDomain.Equals(fpd));
}

TEST(OriginAttributes, Suffix_default)
{
  OriginAttributes attrs;
  TestSuffix(attrs);
}

TEST(OriginAttributes, Suffix_inIsolatedMozBrowser)
{
  OriginAttributes attrs(true);
  TestSuffix(attrs);
}

TEST(OriginAttributes, FirstPartyDomain_default)
{
  static const char prefKey[] = "privacy.firstparty.isolate";
  bool oldPref = Preferences::GetBool(prefKey);
  Preferences::SetBool(prefKey, true);
  TestFPD(NS_LITERAL_STRING("http://www.example.com"),
          NS_LITERAL_STRING("example.com"));
  TestFPD(NS_LITERAL_STRING("http://s3.amazonaws.com"),
          NS_LITERAL_STRING("s3.amazonaws.com"));
  TestFPD(NS_LITERAL_STRING("http://com"), NS_LITERAL_STRING("com"));
  TestFPD(NS_LITERAL_STRING("http://.com"), NS_LITERAL_STRING(""));
  TestFPD(NS_LITERAL_STRING("http://..com"), NS_LITERAL_STRING(""));
  TestFPD(NS_LITERAL_STRING("http://127.0.0.1"),
          NS_LITERAL_STRING("127.0.0.1"));
  TestFPD(NS_LITERAL_STRING("http://[::1]"), NS_LITERAL_STRING("[::1]"));
  Preferences::SetBool(prefKey, oldPref);
}
