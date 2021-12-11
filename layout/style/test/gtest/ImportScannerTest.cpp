/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/ImportScanner.h"

using namespace mozilla;

static nsTArray<nsString> Scan(const char* aCssCode) {
  nsTArray<nsString> urls;
  ImportScanner scanner;
  scanner.Start();
  urls.AppendElements(scanner.Scan(NS_ConvertUTF8toUTF16(aCssCode)));
  urls.AppendElements(scanner.Stop());
  return urls;
}

TEST(ImportScanner, Simple)
{
  auto urls = Scan(
      "/* Something something */ "
      "@charset \"utf-8\";"
      "@import url(bar);"
      "@import uRL( baz );"
      "@import \"bazz)\"");

  ASSERT_EQ(urls.Length(), 3u);
  ASSERT_EQ(urls[0], u"bar"_ns);
  ASSERT_EQ(urls[1], u"baz"_ns);
  ASSERT_EQ(urls[2], u"bazz)"_ns);
}

TEST(ImportScanner, UrlWithQuotes)
{
  auto urls = Scan(
      "/* Something something */ "
      "@import url(\"bar\");"
      "@import\tuRL( \"baz\" );"
      "@imPort\turL( 'bazz' );"
      "something else {}"
      "@import\turL( 'bazz' ); ");

  ASSERT_EQ(urls.Length(), 3u);
  ASSERT_EQ(urls[0], u"bar"_ns);
  ASSERT_EQ(urls[1], u"baz"_ns);
  ASSERT_EQ(urls[2], u"bazz"_ns);
}

TEST(ImportScanner, MediaIsIgnored)
{
  auto urls = Scan(
      "@chArset \"utf-8\";"
      "@import url(\"bar\") print;"
      "/* Something something */ "
      "@import\tuRL( \"baz\" ) (min-width: 100px);"
      "@import\turL( bazz ) (max-width: 100px);");

  ASSERT_EQ(urls.Length(), 3u);
  ASSERT_EQ(urls[0], u"bar"_ns);
  ASSERT_EQ(urls[1], u"baz"_ns);
  ASSERT_EQ(urls[2], u"bazz"_ns);
}
