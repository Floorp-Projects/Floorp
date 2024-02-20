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
  TestFPD(nsLiteralString(_spec), nsLiteralString(_expected))

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

  TEST_FPD(u"http://www.example.com", u"example.com");
  TEST_FPD(u"http://www.example.com:80", u"example.com");
  TEST_FPD(u"http://www.example.com:8080", u"example.com");
  TEST_FPD(u"http://s3.amazonaws.com", u"s3.amazonaws.com");
  TEST_FPD(u"http://com", u"com");
  TEST_FPD(u"http://com.", u"com.");
  TEST_FPD(u"http://com:8080", u"com");
  TEST_FPD(u"http://.com", u"");
  TEST_FPD(u"http://..com", u"");
  TEST_FPD(u"http://127.0.0.1", u"127.0.0.1");
  TEST_FPD(u"http://[::1]", u"[::1]");
  TEST_FPD(u"about:config",
           u"about.ef2a7dd5-93bc-417f-a698-142c3116864f.mozilla");
  TEST_FPD(u"moz-extension://f5b6ca10-5bd4-4ed6-9baf-820dc5152bc1", u"");
  TEST_FPD(u"moz-nullprincipal:{9bebdabb-828a-4284-8b00-432a968c6e42}",
           u"9bebdabb-828a-4284-8b00-432a968c6e42.mozilla");
  TEST_FPD(
      u"moz-nullprincipal:{9bebdabb-828a-4284-8b00-432a968c6e42}"
      u"?https://www.example.com",
      u"9bebdabb-828a-4284-8b00-432a968c6e42.mozilla");

  Preferences::SetBool(FPI_PREF, oldFpiPref);
  Preferences::SetBool(SITE_PREF, oldSitePref);
}

TEST(OriginAttributes, FirstPartyDomain_site)
{
  bool oldFpiPref = Preferences::GetBool(FPI_PREF);
  Preferences::SetBool(FPI_PREF, true);
  bool oldSitePref = Preferences::GetBool(SITE_PREF);
  Preferences::SetBool(SITE_PREF, true);

  TEST_FPD(u"http://www.example.com", u"(http,example.com)");
  TEST_FPD(u"http://www.example.com:80", u"(http,example.com)");
  TEST_FPD(u"http://www.example.com:8080", u"(http,example.com)");
  TEST_FPD(u"http://s3.amazonaws.com", u"(http,s3.amazonaws.com)");
  TEST_FPD(u"http://com", u"(http,com)");
  TEST_FPD(u"http://com.", u"(http,com.)");
  TEST_FPD(u"http://com:8080", u"(http,com,8080)");
  TEST_FPD(u"http://.com", u"(http,.com)");
  TEST_FPD(u"http://..com", u"(http,..com)");
  TEST_FPD(u"http://127.0.0.1", u"(http,127.0.0.1)");
  TEST_FPD(u"http://[::1]", u"(http,[::1])");
  TEST_FPD(u"about:config",
           u"(about,about.ef2a7dd5-93bc-417f-a698-142c3116864f.mozilla)");
  TEST_FPD(u"moz-extension://f5b6ca10-5bd4-4ed6-9baf-820dc5152bc1", u"");
  TEST_FPD(u"moz-nullprincipal:{9bebdabb-828a-4284-8b00-432a968c6e42}",
           u"9bebdabb-828a-4284-8b00-432a968c6e42.mozilla");
  TEST_FPD(
      u"moz-nullprincipal:{9bebdabb-828a-4284-8b00-432a968c6e42}"
      u"?https://www.example.com",
      u"9bebdabb-828a-4284-8b00-432a968c6e42.mozilla");

  Preferences::SetBool(FPI_PREF, oldFpiPref);
  Preferences::SetBool(SITE_PREF, oldSitePref);
}

}  // namespace mozilla
