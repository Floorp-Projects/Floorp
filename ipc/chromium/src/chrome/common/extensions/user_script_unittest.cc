// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/pickle.h"
#include "chrome/common/extensions/user_script.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(UserScriptTest, Match1) {
  UserScript script;
  script.add_glob("*mail.google.com*");
  script.add_glob("*mail.yahoo.com*");
  script.add_glob("*mail.msn.com*");
  EXPECT_TRUE(script.MatchesUrl(GURL("http://mail.google.com")));
  EXPECT_TRUE(script.MatchesUrl(GURL("http://mail.google.com/foo")));
  EXPECT_TRUE(script.MatchesUrl(GURL("https://mail.google.com/foo")));
  EXPECT_TRUE(script.MatchesUrl(GURL("ftp://mail.google.com/foo")));
  EXPECT_TRUE(script.MatchesUrl(GURL("http://woo.mail.google.com/foo")));
  EXPECT_TRUE(script.MatchesUrl(GURL("http://mail.yahoo.com/bar")));
  EXPECT_TRUE(script.MatchesUrl(GURL("http://mail.msn.com/baz")));
  EXPECT_FALSE(script.MatchesUrl(GURL("http://www.hotmail.com")));
}

TEST(UserScriptTest, Match2) {
  UserScript script;
  script.add_glob("*mail.google.com/");
  // GURL normalizes the URL to have a trailing "/"
  EXPECT_TRUE(script.MatchesUrl(GURL("http://mail.google.com")));
  EXPECT_TRUE(script.MatchesUrl(GURL("http://mail.google.com/")));
  EXPECT_FALSE(script.MatchesUrl(GURL("http://mail.google.com/foo")));
}

TEST(UserScriptTest, Match3) {
  UserScript script;
  script.add_glob("http://mail.google.com/*");
  // GURL normalizes the URL to have a trailing "/"
  EXPECT_TRUE(script.MatchesUrl(GURL("http://mail.google.com")));
  EXPECT_TRUE(script.MatchesUrl(GURL("http://mail.google.com/foo")));
  EXPECT_FALSE(script.MatchesUrl(GURL("https://mail.google.com/foo")));
}

TEST(UserScriptTest, Match4) {
  UserScript script;
  script.add_glob("*");
  EXPECT_TRUE(script.MatchesUrl(GURL("http://foo.com/bar")));
  EXPECT_TRUE(script.MatchesUrl(GURL("http://hot.com/dog")));
  EXPECT_TRUE(script.MatchesUrl(GURL("https://hot.com/dog")));
  EXPECT_TRUE(script.MatchesUrl(GURL("file:///foo/bar")));
}

TEST(UserScriptTest, Match5) {
  UserScript script;
  script.add_glob("*foo*");
  EXPECT_TRUE(script.MatchesUrl(GURL("http://foo.com/bar")));
  EXPECT_TRUE(script.MatchesUrl(GURL("http://baz.org/foo/bar")));
  EXPECT_FALSE(script.MatchesUrl(GURL("http://baz.org")));
}

TEST(UserScriptTest, Match6) {
  URLPattern pattern;
  ASSERT_TRUE(pattern.Parse("http://*/foo*"));

  UserScript script;
  script.add_url_pattern(pattern);
  EXPECT_TRUE(script.MatchesUrl(GURL("http://monkey.com/foobar")));
  EXPECT_FALSE(script.MatchesUrl(GURL("http://monkey.com/hotdog")));

  // NOTE: URLPattern is tested more extensively in url_pattern_unittest.cc.
}

TEST(UserScriptTest, Pickle) {
  URLPattern pattern1;
  URLPattern pattern2;
  ASSERT_TRUE(pattern1.Parse("http://*/foo*"));
  ASSERT_TRUE(pattern2.Parse("http://bar/baz*"));

  UserScript script1;
  script1.js_scripts().push_back(UserScript::File(
      FilePath(FILE_PATH_LITERAL("c:\\foo\\foo.user.js")),
      GURL("chrome-user-script:/foo.user.js")));
  script1.css_scripts().push_back(UserScript::File(
      FilePath(FILE_PATH_LITERAL("c:\\foo\\foo.user.css")),
      GURL("chrome-user-script:/foo.user.css")));
  script1.css_scripts().push_back(UserScript::File(
      FilePath(FILE_PATH_LITERAL("c:\\foo\\foo2.user.css")),
      GURL("chrome-user-script:/foo2.user.css")));
  script1.set_run_location(UserScript::DOCUMENT_START);

  script1.add_url_pattern(pattern1);
  script1.add_url_pattern(pattern2);

  Pickle pickle;
  script1.Pickle(&pickle);

  void* iter = NULL;
  UserScript script2;
  script2.Unpickle(pickle, &iter);

  EXPECT_EQ(1U, script2.js_scripts().size());
  EXPECT_EQ(script1.js_scripts()[0].url(), script2.js_scripts()[0].url());

  EXPECT_EQ(2U, script2.css_scripts().size());
  for (size_t i = 0; i < script2.js_scripts().size(); ++i) {
    EXPECT_EQ(script1.css_scripts()[i].url(), script2.css_scripts()[i].url());
  }

  ASSERT_EQ(script1.globs().size(), script2.globs().size());
  for (size_t i = 0; i < script1.globs().size(); ++i) {
    EXPECT_EQ(script1.globs()[i], script2.globs()[i]);
  }
  ASSERT_EQ(script1.url_patterns().size(), script2.url_patterns().size());
  for (size_t i = 0; i < script1.url_patterns().size(); ++i) {
    EXPECT_EQ(script1.url_patterns()[i].GetAsString(),
              script2.url_patterns()[i].GetAsString());
  }
}

TEST(UserScriptTest, Defaults) {
  UserScript script;
  ASSERT_EQ(UserScript::DOCUMENT_END, script.run_location());
}
