/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gtest/gtest.h"
#include "mozilla/BasePrincipal.h"

using mozilla::PrincipalOriginAttributes;

static void
TestSuffix(const PrincipalOriginAttributes& attrs)
{
  nsAutoCString suffix;
  attrs.CreateSuffix(suffix);

  PrincipalOriginAttributes attrsFromSuffix;
  bool success = attrsFromSuffix.PopulateFromSuffix(suffix);
  EXPECT_TRUE(success);

  EXPECT_EQ(attrs, attrsFromSuffix);
}

TEST(PrincipalOriginAttributes, Suffix_default)
{
  PrincipalOriginAttributes attrs;
  TestSuffix(attrs);
}

TEST(PrincipalOriginAttributes, Suffix_appId_inIsolatedMozBrowser)
{
  PrincipalOriginAttributes attrs(1, true);
  TestSuffix(attrs);
}

TEST(PrincipalOriginAttributes, Suffix_maxAppId_inIsolatedMozBrowser)
{
  PrincipalOriginAttributes attrs(4294967295, true);
  TestSuffix(attrs);
}
