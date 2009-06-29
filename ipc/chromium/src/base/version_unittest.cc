// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_ptr.h"
#include "base/version.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(Version, GetVersionFromString) {
  static const struct version_string {
    const char* input;
    size_t parts;
    bool success;
  } cases[] = {
    {"0", 1, true},
    {"0.0", 2, true},
    {"65537.0", 0, false},
    {"-1.0", 0, false},
    {"1.-1.0", 0, false},
    {"+1.0", 0, false},
    {"1.+1.0", 0, false},
    {"1.0a", 0, false},
    {"1.2.3.4.5.6.7.8.9.0", 10, true},
    {"02.1", 0, false},
    {"f.1", 0, false},
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    scoped_ptr<Version> vers(Version::GetVersionFromString(cases[i].input));
    EXPECT_EQ(cases[i].success, vers.get() != NULL);
    if (cases[i].success)
      EXPECT_EQ(cases[i].parts, vers->components().size());
  }
}

TEST(Version, Compare) {
  static const struct version_compare {
    const char* lhs;
    const char* rhs;
    int expected;
  } cases[] = {
    {"1.0", "1.0", 0},
    {"1.0", "0.0", 1},
    {"1.0", "2.0", -1},
    {"1.0", "1.1", -1},
    {"1.1", "1.0", 1},
    {"1.0", "1.0.1", -1},
    {"1.1", "1.0.1", 1},
    {"1.1", "1.0.1", 1},
    {"1.0.0", "1.0", 0},
    {"1.0.3", "1.0.20", -1},
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    scoped_ptr<Version> lhs(Version::GetVersionFromString(cases[i].lhs));
    scoped_ptr<Version> rhs(Version::GetVersionFromString(cases[i].rhs));
    EXPECT_EQ(lhs->CompareTo(*rhs), cases[i].expected) <<
          cases[i].lhs << " ? " << cases[i].rhs;
  }
}

}
