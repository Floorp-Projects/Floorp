/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/intl/MozLocale.h"

using namespace mozilla::intl;


TEST(Intl_Locale_Locale, Locale) {
  Locale loc = Locale("en-US");

  ASSERT_TRUE(loc.GetLanguage().Equals("en"));
  ASSERT_TRUE(loc.GetRegion().Equals("US"));
}

TEST(Intl_Locale_Locale, AsString) {
  Locale loc = Locale("ja-jp-windows");

  ASSERT_TRUE(loc.AsString().Equals("ja-JP-windows"));
}

TEST(Intl_Locale_Locale, GetSubTags) {
  Locale loc = Locale("en-latn-us-macos");

  ASSERT_TRUE(loc.GetLanguage().Equals("en"));
  ASSERT_TRUE(loc.GetScript().Equals("Latn"));
  ASSERT_TRUE(loc.GetRegion().Equals("US"));
  ASSERT_TRUE(loc.GetVariants().Length() == 1);
  ASSERT_TRUE(loc.GetVariants()[0].Equals("macos"));
}

TEST(Intl_Locale_Locale, Matches) {
  Locale loc = Locale("en-US");

  Locale loc2 = Locale("en-GB");
  ASSERT_FALSE(loc == loc2);

  Locale loc3 = Locale("en-US");
  ASSERT_TRUE(loc == loc3);

  Locale loc4 = Locale("En_us");
  ASSERT_TRUE(loc == loc4);
}

TEST(Intl_Locale_Locale, MatchesRange) {
  Locale loc = Locale("en-US");

  Locale loc2 = Locale("en-Latn-US");
  ASSERT_FALSE(loc == loc2);
  ASSERT_TRUE(loc.Matches(loc2, true, false));
  ASSERT_FALSE(loc.Matches(loc2, false, true));
  ASSERT_FALSE(loc.Matches(loc2, false, false));
  ASSERT_TRUE(loc.Matches(loc2, true, true));

  Locale loc3 = Locale("en");
  ASSERT_FALSE(loc == loc3);
  ASSERT_TRUE(loc.Matches(loc3, false, true));
  ASSERT_FALSE(loc.Matches(loc3, true, false));
  ASSERT_FALSE(loc.Matches(loc3, false, false));
  ASSERT_TRUE(loc.Matches(loc3, true, true));
}

TEST(Intl_Locale_Locale, PrivateUse) {
  Locale loc = Locale("x-test");

  ASSERT_TRUE(loc.IsValid());
  ASSERT_TRUE(loc.GetLanguage().Equals(""));
  ASSERT_TRUE(loc.GetScript().Equals(""));
  ASSERT_TRUE(loc.GetRegion().Equals(""));
  ASSERT_TRUE(loc.GetVariants().Length() == 0);

  ASSERT_TRUE(loc.AsString().Equals("x-test"));

  Locale loc2 = Locale("fr-x-test");

  ASSERT_TRUE(loc2.IsValid());
  ASSERT_TRUE(loc2.GetLanguage().Equals("fr"));
  ASSERT_TRUE(loc2.GetScript().Equals(""));
  ASSERT_TRUE(loc2.GetRegion().Equals(""));
  ASSERT_TRUE(loc2.GetVariants().Length() == 0);

  ASSERT_TRUE(loc2.AsString().Equals("fr-x-test"));

  // Make sure that we preserve private use tags order.
  Locale loc3 = Locale("fr-x-foo-bar-baz");

  ASSERT_TRUE(loc3.IsValid());
  ASSERT_TRUE(loc3.AsString().Equals("fr-x-foo-bar-baz"));
}
