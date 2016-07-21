/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/EMEUtils.h"

using namespace std;
using namespace mozilla;

struct ParseKeySystemTestCase {
  const char16_t* mInputKeySystemString;
  int32_t mOutCDMVersion;
  bool mShouldPass;
};

const ParseKeySystemTestCase ParseKeySystemTests[] = {
  {
    u"org.w3.clearkey",
    NO_CDM_VERSION,
    true,
  }, {
    u"org.w3.clearkey.123",
    123,
    true,
  }, {
    u"org.w3.clearkey.-1",
    NO_CDM_VERSION,
    false,
  }, {
    u"org.w3.clearkey.NaN",
    NO_CDM_VERSION,
    false,
  }, {
    u"org.w3.clearkey.0",
    0,
    true,
  }, {
    u"org.w3.clearkey.123567890123567890123567890123567890123567890",
    NO_CDM_VERSION,
    false,
  }, {
    u"org.w3.clearkey.0.1",
    NO_CDM_VERSION,
    false,
  }
};

TEST(EME, EMEParseKeySystem) {
  const nsAutoString clearkey(u"org.w3.clearkey");
  for (const ParseKeySystemTestCase& test : ParseKeySystemTests) {
    nsAutoString keySystem;
    int32_t version;
    bool rv = ParseKeySystem(nsDependentString(test.mInputKeySystemString),
                             keySystem,
                             version);
    EXPECT_EQ(rv, test.mShouldPass) << "parse should succeed if expected to";
    if (!test.mShouldPass) {
      continue;
    }
    EXPECT_TRUE(keySystem.Equals(clearkey)) << NS_ConvertUTF16toUTF8(keySystem).get(); //"should extract expected keysystem" << ;
    EXPECT_EQ(test.mOutCDMVersion, version) << "should extract expected version";
  }
}
