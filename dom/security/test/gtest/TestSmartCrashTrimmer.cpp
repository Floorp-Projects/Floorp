/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "nsContentSecurityUtils.h"
#include "nsStringFwd.h"

#define ASSERT_STRCMP(first, second) ASSERT_TRUE(strcmp(first, second) == 0);

#define ASSERT_STRCMP_AND_PRINT(first, second)             \
  fprintf(stderr, "First: %s\n", first);                   \
  fprintf(stderr, "Second: %s\n", second);                 \
  fprintf(stderr, "strcmp = %i\n", strcmp(first, second)); \
  ASSERT_EQUAL(first, second);

TEST(SmartCrashTrimmer, Test)
{
  static_assert(sPrintfCrashReasonSize == 1024);
  {
    auto ret = nsContentSecurityUtils::SmartFormatCrashString(
        std::string(1025, '.').c_str());
    ASSERT_EQ(strlen(ret), 1023ul);
  }

  {
    auto ret = nsContentSecurityUtils::SmartFormatCrashString(
        std::string(1025, '.').c_str(), std::string(1025, 'A').c_str(),
        "Hello %s world %s!");
    char expected[1025];
    sprintf(expected, "Hello %s world AAAAAAAAAAAAAAAAAAAAAAAAA!",
            std::string(984, '.').c_str());
    ASSERT_STRCMP(ret.get(), expected);
  }
}
