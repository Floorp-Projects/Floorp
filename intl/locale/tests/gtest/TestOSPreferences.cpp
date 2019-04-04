/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/intl/OSPreferences.h"

using namespace mozilla::intl;

/**
 * We test that on all platforms we test against (irrelevant of the tier),
 * we will be able to retrieve at least a single locale out of the system.
 *
 * In theory, that may not be true, but if we encounter such platform we should
 * decide how to handle this and special case and this test should make
 * it not happen without us noticing.
 */
TEST(Intl_Locale_OSPreferences, GetSystemLocales)
{
  nsTArray<nsCString> systemLocales;
  ASSERT_TRUE(NS_SUCCEEDED(
      OSPreferences::GetInstance()->GetSystemLocales(systemLocales)));

  ASSERT_FALSE(systemLocales.IsEmpty());
}

/**
 * We test that on all platforms we test against (irrelevant of the tier),
 * we will be able to retrieve at least a single locale out of the system.
 *
 * In theory, that may not be true, but if we encounter such platform we should
 * decide how to handle this and special case and this test should make
 * it not happen without us noticing.
 */
TEST(Intl_Locale_OSPreferences, GetRegionalPrefsLocales)
{
  nsTArray<nsCString> rgLocales;
  ASSERT_TRUE(NS_SUCCEEDED(
      OSPreferences::GetInstance()->GetRegionalPrefsLocales(rgLocales)));

  ASSERT_FALSE(rgLocales.IsEmpty());
}

/**
 * We test that on all platforms we test against,
 * we will be able to retrieve a date and time pattern.
 *
 * This may come back empty on platforms where we don't have platforms
 * bindings for, so effectively, we're testing for crashes. We should
 * never crash.
 */
TEST(Intl_Locale_OSPreferences, GetDateTimePattern)
{
  nsAutoString pattern;
  OSPreferences* osprefs = OSPreferences::GetInstance();

  struct Test {
    int dateStyle;
    int timeStyle;
    const char* locale;
  };
  Test tests[] = {{0, 0, ""},   {1, 0, "pl"}, {2, 0, "de-DE"}, {3, 0, "fr"},
                  {4, 0, "ar"},

                  {0, 1, ""},   {0, 2, "it"}, {0, 3, ""},      {0, 4, "ru"},

                  {4, 1, ""},   {3, 2, "cs"}, {2, 3, ""},      {1, 4, "ja"}};

  for (unsigned i = 0; i < mozilla::ArrayLength(tests); i++) {
    const Test& t = tests[i];
    nsAutoString pattern;
    if (NS_SUCCEEDED(osprefs->GetDateTimePattern(
            t.dateStyle, t.timeStyle, nsDependentCString(t.locale), pattern))) {
      ASSERT_TRUE((t.dateStyle == 0 && t.timeStyle == 0) || !pattern.IsEmpty());
    }
  }

  ASSERT_TRUE(1);
}
