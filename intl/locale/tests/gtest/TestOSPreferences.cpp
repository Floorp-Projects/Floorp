/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
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
TEST(Intl_Locale_OSPreferences, GetSystemLocales) {
  nsTArray<nsCString> systemLocales;
  ASSERT_TRUE(OSPreferences::GetInstance()->GetSystemLocales(systemLocales));

  ASSERT_FALSE(systemLocales.IsEmpty());
}
