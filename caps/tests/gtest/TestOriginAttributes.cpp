/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gtest/gtest.h"
#include "mozilla/BasePrincipal.h"

using mozilla::OriginAttributes;

static void
TestSuffix(const OriginAttributes& attrs)
{
  nsAutoCString suffix;
  attrs.CreateSuffix(suffix);

  OriginAttributes attrsFromSuffix;
  bool success = attrsFromSuffix.PopulateFromSuffix(suffix);
  EXPECT_TRUE(success);

  EXPECT_EQ(attrs, attrsFromSuffix);
}

TEST(OriginAttributes, Suffix_default)
{
  OriginAttributes attrs;
  TestSuffix(attrs);
}

TEST(OriginAttributes, Suffix_appId_inIsolatedMozBrowser)
{
  OriginAttributes attrs(1, true);
  TestSuffix(attrs);
}

TEST(OriginAttributes, Suffix_maxAppId_inIsolatedMozBrowser)
{
  OriginAttributes attrs(4294967295, true);
  TestSuffix(attrs);
}
