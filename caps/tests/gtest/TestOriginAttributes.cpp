/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gtest/gtest.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/Preferences.h"
#include "nsNetUtil.h"

using mozilla::OriginAttributes;
using mozilla::Preferences;

#define TEST_FPD(_spec, _expected) \
  TestFPD(NS_LITERAL_STRING(_spec), NS_LITERAL_STRING(_expected))

namespace mozilla {

static void TestSuffix(const OriginAttributes& attrs) {
  nsAutoCString suffix;
  attrs.CreateSuffix(suffix);

  OriginAttributes attrsFromSuffix;
  bool success = attrsFromSuffix.PopulateFromSuffix(suffix);
  EXPECT_TRUE(success);

  EXPECT_EQ(attrs, attrsFromSuffix);
}

static void TestFPD(const nsAString& spec, const nsAString& expected) {
  OriginAttributes attrs;
  nsCOMPtr<nsIURI> url;
  ASSERT_EQ(NS_NewURI(getter_AddRefs(url), spec), NS_OK);
  attrs.SetFirstPartyDomain(true, url);
  EXPECT_TRUE(attrs.mFirstPartyDomain.Equals(expected));

  TestSuffix(attrs);
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
  TEST_FPD("http://www.example.com", "example.com");
  TEST_FPD("http://s3.amazonaws.com", "s3.amazonaws.com");
  TEST_FPD("http://com", "com");
  TEST_FPD("http://com:8080", "com");
  TEST_FPD("http://.com", "");
  TEST_FPD("http://..com", "");
  TEST_FPD("http://127.0.0.1", "127.0.0.1");
  TEST_FPD("http://[::1]", "[::1]");
  Preferences::SetBool(prefKey, oldPref);
}

}  // namespace mozilla
