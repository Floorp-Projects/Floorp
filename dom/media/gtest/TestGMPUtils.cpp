/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "GMPUtils.h"

#include <string>
#include <vector>

using namespace std;
using namespace mozilla;

struct SplitAtTest {
  string mInput;
  const char* mDelims;
  vector<string> mTokens;
};

static const SplitAtTest sSplitAtTests[] {
  {
    "1,2,3,4",
    ",",
    { "1", "2", "3", "4" },
  }, {
    "a simple, comma, seperated, list",
    ",",
    {"a simple", " comma", " seperated", " list"},
  }, {
    // Various platform line endings...
    "line1\r\n" // Windows
    "line2\r" // Old MacOSX
    "line3\n" // Unix
    "line4",
    "\r\n",
    { "line1", "line2", "line3", "line4" },
  },
};

TEST(GeckoMediaPlugins, GMPUtils) {
  for (const SplitAtTest& test : sSplitAtTests) {
    nsCString input(test.mInput.c_str(), test.mInput.size());
    nsTArray<nsCString> tokens = SplitAt(test.mDelims, input);
    EXPECT_EQ(tokens.Length(), test.mTokens.size()) << "Should get expected number of tokens";
    for (size_t i = 0; i < tokens.Length(); i++) {
      EXPECT_TRUE(tokens[i].EqualsASCII(test.mTokens[i].c_str()))
        << "Tokenize fail; expected=" << test.mTokens[i] << " got=" <<
        tokens[i].BeginReading();
    }
  }
}
