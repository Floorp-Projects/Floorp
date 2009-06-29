// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/hmac.h"
#include "testing/gtest/include/gtest/gtest.h"

static const int kDigestSize = 20;

TEST(HMACTest, HmacSafeBrowsingResponseTest) {
  const int kKeySize = 16;

  // Client key.
  const unsigned char kClientKey[kKeySize] =
      { 0xbf, 0xf6, 0x83, 0x4b, 0x3e, 0xa3, 0x23, 0xdd,
        0x96, 0x78, 0x70, 0x8e, 0xa1, 0x9d, 0x3b, 0x40 };

  // Expected HMAC result using kMessage and kClientKey.
  const unsigned char kReceivedHmac[kDigestSize] =
      { 0xb9, 0x3c, 0xd6, 0xf0, 0x49, 0x47, 0xe2, 0x52,
        0x59, 0x7a, 0xbd, 0x1f, 0x2b, 0x4c, 0x83, 0xad,
        0x86, 0xd2, 0x48, 0x85 };

  const char kMessage[] =
"n:1896\ni:goog-malware-shavar\nu:s.ytimg.com/safebrowsing/rd/goog-malware-shav"
"ar_s_445-450\nu:s.ytimg.com/safebrowsing/rd/goog-malware-shavar_s_439-444\nu:s"
".ytimg.com/safebrowsing/rd/goog-malware-shavar_s_437\nu:s.ytimg.com/safebrowsi"
"ng/rd/goog-malware-shavar_s_436\nu:s.ytimg.com/safebrowsing/rd/goog-malware-sh"
"avar_s_433-435\nu:s.ytimg.com/safebrowsing/rd/goog-malware-shavar_s_431\nu:s.y"
"timg.com/safebrowsing/rd/goog-malware-shavar_s_430\nu:s.ytimg.com/safebrowsing"
"/rd/goog-malware-shavar_s_429\nu:s.ytimg.com/safebrowsing/rd/goog-malware-shav"
"ar_s_428\nu:s.ytimg.com/safebrowsing/rd/goog-malware-shavar_s_426\nu:s.ytimg.c"
"om/safebrowsing/rd/goog-malware-shavar_s_424\nu:s.ytimg.com/safebrowsing/rd/go"
"og-malware-shavar_s_423\nu:s.ytimg.com/safebrowsing/rd/goog-malware-shavar_s_4"
"22\nu:s.ytimg.com/safebrowsing/rd/goog-malware-shavar_s_420\nu:s.ytimg.com/saf"
"ebrowsing/rd/goog-malware-shavar_s_419\nu:s.ytimg.com/safebrowsing/rd/goog-mal"
"ware-shavar_s_414\nu:s.ytimg.com/safebrowsing/rd/goog-malware-shavar_s_409-411"
"\nu:s.ytimg.com/safebrowsing/rd/goog-malware-shavar_s_405\nu:s.ytimg.com/safeb"
"rowsing/rd/goog-malware-shavar_s_404\nu:s.ytimg.com/safebrowsing/rd/goog-malwa"
"re-shavar_s_402\nu:s.ytimg.com/safebrowsing/rd/goog-malware-shavar_s_401\nu:s."
"ytimg.com/safebrowsing/rd/goog-malware-shavar_a_973-978\nu:s.ytimg.com/safebro"
"wsing/rd/goog-malware-shavar_a_937-972\nu:s.ytimg.com/safebrowsing/rd/goog-mal"
"ware-shavar_a_931-936\nu:s.ytimg.com/safebrowsing/rd/goog-malware-shavar_a_925"
"-930\nu:s.ytimg.com/safebrowsing/rd/goog-malware-shavar_a_919-924\ni:goog-phis"
"h-shavar\nu:s.ytimg.com/safebrowsing/rd/goog-phish-shavar_a_2633\nu:s.ytimg.co"
"m/safebrowsing/rd/goog-phish-shavar_a_2632\nu:s.ytimg.com/safebrowsing/rd/goog"
"-phish-shavar_a_2629-2631\nu:s.ytimg.com/safebrowsing/rd/goog-phish-shavar_a_2"
"626-2628\nu:s.ytimg.com/safebrowsing/rd/goog-phish-shavar_a_2625\n";

  std::string message_data(kMessage);

  base::HMAC hmac(base::HMAC::SHA1);
  ASSERT_TRUE(hmac.Init(kClientKey, kKeySize));
  unsigned char calculated_hmac[kDigestSize];

  EXPECT_TRUE(hmac.Sign(message_data, calculated_hmac, kDigestSize));
  EXPECT_EQ(memcmp(kReceivedHmac, calculated_hmac, kDigestSize), 0);
}

// Test cases from RFC 2202 section 3
TEST(HMACTest, RFC2202TestCases) {
  const struct {
    const char *key;
    const int key_len;
    const char *data;
    const int data_len;
    const char *digest;
  } cases[] = {
    { "\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B"
          "\x0B\x0B\x0B\x0B", 20,
      "Hi There", 8,
      "\xB6\x17\x31\x86\x55\x05\x72\x64\xE2\x8B\xC0\xB6\xFB\x37\x8C\x8E"
          "\xF1\x46\xBE\x00" },
    { "Jefe", 4,
      "what do ya want for nothing?", 28,
      "\xEF\xFC\xDF\x6A\xE5\xEB\x2F\xA2\xD2\x74\x16\xD5\xF1\x84\xDF\x9C"
          "\x25\x9A\x7C\x79" },
    { "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
          "\xAA\xAA\xAA\xAA", 20,
      "\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD"
          "\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD"
          "\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD"
          "\xDD\xDD", 50,
      "\x12\x5D\x73\x42\xB9\xAC\x11\xCD\x91\xA3\x9A\xF4\x8A\xA1\x7B\x4F"
          "\x63\xF1\x75\xD3" },
    { "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F\x10"
          "\x11\x12\x13\x14\x15\x16\x17\x18\x19", 25,
      "\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
          "\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
          "\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
          "\xCD\xCD", 50,
      "\x4C\x90\x07\xF4\x02\x62\x50\xC6\xBC\x84\x14\xF9\xBF\x50\xC8\x6C"
          "\x2D\x72\x35\xDA" },
    { "\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C"
          "\x0C\x0C\x0C\x0C", 20,
      "Test With Truncation", 20,
      "\x4C\x1A\x03\x42\x4B\x55\xE0\x7F\xE7\xF2\x7B\xE1\xD5\x8B\xB9\x32"
          "\x4A\x9A\x5A\x04" },
    { "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
          "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
          "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
          "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
          "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA",
      80,
      "Test Using Larger Than Block-Size Key - Hash Key First", 54,
      "\xAA\x4A\xE5\xE1\x52\x72\xD0\x0E\x95\x70\x56\x37\xCE\x8A\x3B\x55"
          "\xED\x40\x21\x12" },
    { "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
          "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
          "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
          "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
          "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA",
      80,
      "Test Using Larger Than Block-Size Key and Larger "
          "Than One Block-Size Data", 73,
      "\xE8\xE9\x9D\x0F\x45\x23\x7D\x78\x6D\x6B\xBA\xA7\x96\x5C\x78\x08"
          "\xBB\xFF\x1A\x91" }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    base::HMAC hmac(base::HMAC::SHA1);
    ASSERT_TRUE(hmac.Init(reinterpret_cast<const unsigned char*>(cases[i].key),
                          cases[i].key_len));
    std::string data_string(cases[i].data, cases[i].data_len);
    unsigned char digest[kDigestSize];
    EXPECT_TRUE(hmac.Sign(data_string, digest, kDigestSize));
    EXPECT_EQ(memcmp(cases[i].digest, digest, kDigestSize), 0);
  }
}

TEST(HMACTest, HMACObjectReuse) {
  const char *key =
      "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
      "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
      "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
      "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
      "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA";
  const int key_len = 80;

  const struct {
    const char *data;
    const int data_len;
    const char *digest;
  } cases[] = {
    { "Test Using Larger Than Block-Size Key - Hash Key First", 54,
      "\xAA\x4A\xE5\xE1\x52\x72\xD0\x0E\x95\x70\x56\x37\xCE\x8A\x3B\x55"
          "\xED\x40\x21\x12" },
    { "Test Using Larger Than Block-Size Key and Larger "
          "Than One Block-Size Data", 73,
      "\xE8\xE9\x9D\x0F\x45\x23\x7D\x78\x6D\x6B\xBA\xA7\x96\x5C\x78\x08"
          "\xBB\xFF\x1A\x91" }
  };

  base::HMAC hmac(base::HMAC::SHA1);
  ASSERT_TRUE(hmac.Init(reinterpret_cast<const unsigned char*>(key), key_len));
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    std::string data_string(cases[i].data, cases[i].data_len);
    unsigned char digest[kDigestSize];
    EXPECT_TRUE(hmac.Sign(data_string, digest, kDigestSize));
    EXPECT_EQ(memcmp(cases[i].digest, digest, kDigestSize), 0);
  }
}
