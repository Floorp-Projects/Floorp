// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "googleurl/src/url_util.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(URLUtil, Scheme) {
  enum SchemeType {
    NO_SCHEME       = 0,
    SCHEME          = 1 << 0,
    STANDARD_SCHEME = 1 << 1,
  };
  const struct {
    const wchar_t* url;
    const char* try_scheme;
    const bool scheme_matches;
    const int flags;
  } tests[] = {
    {L"  ",                 "hello", false, NO_SCHEME},
    {L"foo",                NULL, false, NO_SCHEME},
    {L"google.com/foo:bar", NULL, false, NO_SCHEME},
    {L"Garbage:foo.com",    "garbage", true, SCHEME},
    {L"Garbage:foo.com",    "trash", false, SCHEME},
    {L"gopher:",            "gopher", true, SCHEME | STANDARD_SCHEME},
    {L"About:blank",        "about", true, SCHEME},
    {L"http://foo.com:123", "foo", false, SCHEME | STANDARD_SCHEME},
    {L"file://c/",          "file", true, SCHEME | STANDARD_SCHEME},
  };

  for (size_t i = 0; i < arraysize(tests); i++) {
    int url_len = static_cast<int>(wcslen(tests[i].url));

    url_parse::Component parsed_scheme;
    bool has_scheme = url_parse::ExtractScheme(tests[i].url, url_len,
                                               &parsed_scheme);

    EXPECT_EQ(!!(tests[i].flags & STANDARD_SCHEME),
              url_util::IsStandard(tests[i].url, url_len));
    EXPECT_EQ(!!(tests[i].flags & STANDARD_SCHEME),
              url_util::IsStandardScheme(tests[i].url, parsed_scheme.len));

    url_parse::Component found_scheme;
    EXPECT_EQ(tests[i].scheme_matches,
              url_util::FindAndCompareScheme(tests[i].url, url_len,
                                             tests[i].try_scheme,
                                             &found_scheme));
    EXPECT_EQ(parsed_scheme.begin, found_scheme.begin);
    EXPECT_EQ(parsed_scheme.len, found_scheme.len);
  }
}
