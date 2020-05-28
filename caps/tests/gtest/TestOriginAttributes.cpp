/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gtest/gtest.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/NullPrincipal.h"
#include "mozilla/Preferences.h"
#include "nsNetUtil.h"

using mozilla::OriginAttributes;
using mozilla::Preferences;

#define FPI_PREF "privacy.firstparty.isolate"
#define SITE_PREF "privacy.firstparty.isolate.use_site"

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
  bool oldFpiPref = Preferences::GetBool(FPI_PREF);
  Preferences::SetBool(FPI_PREF, true);
  bool oldSitePref = Preferences::GetBool(SITE_PREF);
  Preferences::SetBool(SITE_PREF, false);

  TEST_FPD("http://www.example.com", "example.com");
  TEST_FPD("http://www.example.com:80", "example.com");
  TEST_FPD("http://www.example.com:8080", "example.com");
  TEST_FPD("http://s3.amazonaws.com", "s3.amazonaws.com");
  TEST_FPD("http://com", "com");
  TEST_FPD("http://com.", "com.");
  TEST_FPD("http://com:8080", "com");
  TEST_FPD("http://.com", "");
  TEST_FPD("http://..com", "");
  TEST_FPD("http://127.0.0.1", "127.0.0.1");
  TEST_FPD("http://[::1]", "[::1]");
  TEST_FPD("about:config",
           "about.ef2a7dd5-93bc-417f-a698-142c3116864f.mozilla");
  TEST_FPD("moz-extension://f5b6ca10-5bd4-4ed6-9baf-820dc5152bc1", "");

  Preferences::SetBool(FPI_PREF, oldFpiPref);
  Preferences::SetBool(FPI_PREF, oldSitePref);
}

TEST(OriginAttributes, FirstPartyDomain_site)
{
  bool oldFpiPref = Preferences::GetBool(FPI_PREF);
  Preferences::SetBool(FPI_PREF, true);
  bool oldSitePref = Preferences::GetBool(SITE_PREF);
  Preferences::SetBool(SITE_PREF, true);

  TEST_FPD("http://www.example.com", "(http,example.com)");
  TEST_FPD("http://www.example.com:80", "(http,example.com)");
  TEST_FPD("http://www.example.com:8080", "(http,example.com)");
  TEST_FPD("http://s3.amazonaws.com", "(http,s3.amazonaws.com)");
  TEST_FPD("http://com", "(http,com)");
  TEST_FPD("http://com.", "(http,com.)");
  TEST_FPD("http://com:8080", "(http,com,8080)");
  TEST_FPD("http://.com", "(http,.com)");
  TEST_FPD("http://..com", "(http,..com)");
  TEST_FPD("http://127.0.0.1", "(http,127.0.0.1)");
  TEST_FPD("http://[::1]", "(http,[::1])");
  TEST_FPD("about:config",
           "(about,about.ef2a7dd5-93bc-417f-a698-142c3116864f.mozilla)");
  TEST_FPD("moz-extension://f5b6ca10-5bd4-4ed6-9baf-820dc5152bc1", "");

  Preferences::SetBool(FPI_PREF, oldFpiPref);
  Preferences::SetBool(FPI_PREF, oldSitePref);
}

TEST(OriginAttributes, NullPrincipal)
{
  bool oldFpiPref = Preferences::GetBool(FPI_PREF);
  Preferences::SetBool(FPI_PREF, true);
  bool oldSitePref = Preferences::GetBool(SITE_PREF);
  Preferences::SetBool(SITE_PREF, true);

  NS_NAMED_LITERAL_STRING(
      spec, "moz-nullprincipal:{9bebdabb-828a-4284-8b00-432a968c6e42}");
  NS_NAMED_LITERAL_STRING(expected,
                          "9bebdabb-828a-4284-8b00-432a968c6e42.mozilla");

  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), spec);

  RefPtr<NullPrincipal> prin = new NullPrincipal();
  prin->Init(OriginAttributes(), true, uri);
  EXPECT_TRUE(prin->OriginAttributesRef().mFirstPartyDomain.Equals(expected));

  Preferences::SetBool(FPI_PREF, oldFpiPref);
  Preferences::SetBool(FPI_PREF, oldSitePref);
}

}  // namespace mozilla
