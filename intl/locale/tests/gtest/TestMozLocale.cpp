/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/intl/MozLocale.h"

using namespace mozilla::intl;

TEST(Intl_MozLocale_MozLocale, MozLocale)
{
  MozLocale loc = MozLocale("en-US");

  ASSERT_TRUE(loc.GetLanguage().Equals("en"));
  ASSERT_TRUE(loc.GetRegion().Equals("US"));
}

TEST(Intl_MozLocale_MozLocale, AsString)
{
  MozLocale loc = MozLocale("ja-jp-windows");

  ASSERT_TRUE(loc.AsString().Equals("ja-JP-windows"));
}

TEST(Intl_MozLocale_MozLocale, GetSubTags)
{
  MozLocale loc = MozLocale("en-latn-us-macos");

  ASSERT_TRUE(loc.GetLanguage().Equals("en"));
  ASSERT_TRUE(loc.GetScript().Equals("Latn"));
  ASSERT_TRUE(loc.GetRegion().Equals("US"));

  nsTArray<nsCString> variants;
  loc.GetVariants(variants);
  ASSERT_TRUE(variants.Length() == 1);
  ASSERT_TRUE(variants[0].Equals("macos"));
}

TEST(Intl_MozLocale_MozLocale, Matches)
{
  MozLocale loc = MozLocale("en-US");

  MozLocale loc2 = MozLocale("en-GB");
  ASSERT_FALSE(loc == loc2);

  MozLocale loc3 = MozLocale("en-US");
  ASSERT_TRUE(loc == loc3);

  MozLocale loc4 = MozLocale("En_us");
  ASSERT_TRUE(loc == loc4);
}

TEST(Intl_MozLocale_MozLocale, MatchesRange)
{
  MozLocale loc = MozLocale("en-US");

  MozLocale loc2 = MozLocale("en-Latn-US");
  ASSERT_FALSE(loc == loc2);
  ASSERT_TRUE(loc.Matches(loc2, true, false));
  ASSERT_FALSE(loc.Matches(loc2, false, true));
  ASSERT_FALSE(loc.Matches(loc2, false, false));
  ASSERT_TRUE(loc.Matches(loc2, true, true));

  MozLocale loc3 = MozLocale("en");
  ASSERT_FALSE(loc == loc3);
  ASSERT_TRUE(loc.Matches(loc3, false, true));
  ASSERT_FALSE(loc.Matches(loc3, true, false));
  ASSERT_FALSE(loc.Matches(loc3, false, false));
  ASSERT_TRUE(loc.Matches(loc3, true, true));
}

TEST(Intl_MozLocale_MozLocale, Variants)
{
  MozLocale loc = MozLocale("en-US-UniFon-BasicEng");

  // Make sure that we canonicalize and sort variant tags
  ASSERT_TRUE(loc.AsString().Equals("en-US-basiceng-unifon"));
}

TEST(Intl_MozLocale_MozLocale, InvalidMozLocale)
{
  MozLocale loc = MozLocale("en-verylongsubtag");
  ASSERT_FALSE(loc.IsWellFormed());

  MozLocale loc2 = MozLocale("p-te");
  ASSERT_FALSE(loc2.IsWellFormed());
}

TEST(Intl_MozLocale_MozLocale, ClearRegion)
{
  MozLocale loc = MozLocale("en-US");
  loc.ClearRegion();
  ASSERT_TRUE(loc.AsString().Equals("en"));
}

TEST(Intl_MozLocale_MozLocale, ClearVariants)
{
  MozLocale loc = MozLocale("en-US-windows");
  loc.ClearVariants();
  ASSERT_TRUE(loc.AsString().Equals("en-US"));
}

TEST(Intl_MozLocale_MozLocale, jaJPmac)
{
  MozLocale loc = MozLocale("ja-JP-mac");
  ASSERT_TRUE(loc.AsString().Equals("ja-JP-macos"));
}

TEST(Intl_MozLocale_MozLocale, Maximize)
{
  MozLocale loc = MozLocale("en");

  ASSERT_TRUE(loc.GetLanguage().Equals("en"));
  ASSERT_TRUE(loc.GetScript().IsEmpty());
  ASSERT_TRUE(loc.GetRegion().IsEmpty());

  ASSERT_TRUE(loc.Maximize());

  ASSERT_TRUE(loc.GetLanguage().Equals("en"));
  ASSERT_TRUE(loc.GetScript().Equals("Latn"));
  ASSERT_TRUE(loc.GetRegion().Equals("US"));
}
