/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include <algorithm>
#include <stdint.h>
#include <vector>

#include "../ClearKeyBase64.cpp"
#include "../ArrayUtils.h"


using namespace std;

struct B64Test {
  const char* b64;
  uint8_t raw[16];
  bool shouldPass;
};

B64Test tests[] = {
  {
    "AAAAADk4AU4AAAAAAAAAAA",
    { 0x0, 0x0, 0x0, 0x0, 0x39, 0x38, 0x1, 0x4e, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
    true
  },
  {
    "h2mqp1zAJjDIC34YXEXBxA==",
    { 0x87, 0x69, 0xaa, 0xa7, 0x5c, 0xc0, 0x26, 0x30, 0xc8, 0xb, 0x7e, 0x18, 0x5c, 0x45, 0xc1, 0xc4 },
    true
  },
  {
    "flcdA35XHQN-Vx0DflcdAw",
    { 0x7e, 0x57, 0x1d, 0x3, 0x7e, 0x57, 0x1d, 0x3, 0x7e, 0x57, 0x1d, 0x3, 0x7e, 0x57, 0x1d, 0x3 },
    true
  },
  {
    "flczM35XMzN-VzMzflczMw",
    { 0x7e, 0x57, 0x33, 0x33, 0x7e, 0x57, 0x33, 0x33, 0x7e, 0x57, 0x33, 0x33, 0x7e, 0x57, 0x33, 0x33 },
    true
  },
  {
    "flcdBH5XHQR-Vx0EflcdBA",
    { 0x7e, 0x57, 0x1d, 0x4, 0x7e, 0x57, 0x1d, 0x4, 0x7e, 0x57, 0x1d, 0x4, 0x7e, 0x57, 0x1d, 0x4 },
    true
  },
  {
    "fldERH5XRER-V0REfldERA",
    { 0x7e, 0x57, 0x44, 0x44, 0x7e, 0x57, 0x44, 0x44, 0x7e, 0x57, 0x44, 0x44, 0x7e, 0x57, 0x44, 0x44 },
    true
  },
  // Failure tests
  { "", { 0 }, false }, // empty
  { "fuzzbiz", { 0 }, false }, // Too short
  { "fuzzbizfuzzbizfuzzbizfuzzbizfuzzbizfuzzbizfuzzbizfuzzbiz", { 0 }, false }, // too long

};

TEST(ClearKey, DecodeBase64KeyOrId) {
  for (size_t i = 0; i < MOZ_ARRAY_LENGTH(tests); i++) {
    vector<uint8_t> v;
    const B64Test& test = tests[i];
    bool rv = DecodeBase64KeyOrId(string(test.b64), v);
    EXPECT_EQ(rv, test.shouldPass);
    if (test.shouldPass) {
      EXPECT_EQ(v.size(), 16u);
      for (size_t k = 0; k < 16; k++) {
        EXPECT_EQ(v[k], test.raw[k]);
      }
    }
  }
}
