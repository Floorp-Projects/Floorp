/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsIDocumentEncoder.h"
#include "nsCRT.h"
#include "nsIParserUtils.h"

const uint32_t kDefaultWrapColumn = 72;

void ConvertBufToPlainText(nsString& aConBuf, int aFlag, uint32_t aWrapColumn) {
  nsCOMPtr<nsIParserUtils> utils = do_GetService(NS_PARSERUTILS_CONTRACTID);
  utils->ConvertToPlainText(aConBuf, aFlag, aWrapColumn, aConBuf);
}

// Test for ASCII with format=flowed; delsp=yes
TEST(PlainTextSerializer, ASCIIWithFlowedDelSp)
{
  nsString test;
  nsString result;

  test.AssignLiteral(
      "<html><body>"
      "Firefox Firefox Firefox Firefox "
      "Firefox Firefox Firefox Firefox "
      "Firefox Firefox Firefox Firefox"
      "</body></html>");

  ConvertBufToPlainText(test,
                        nsIDocumentEncoder::OutputFormatted |
                            nsIDocumentEncoder::OutputCRLineBreak |
                            nsIDocumentEncoder::OutputLFLineBreak |
                            nsIDocumentEncoder::OutputFormatFlowed |
                            nsIDocumentEncoder::OutputFormatDelSp,
                        kDefaultWrapColumn);

  // create result case
  result.AssignLiteral(
      "Firefox Firefox Firefox Firefox "
      "Firefox Firefox Firefox Firefox "
      "Firefox  \r\nFirefox Firefox Firefox\r\n");

  ASSERT_TRUE(test.Equals(result))
  << "Wrong HTML to ASCII text serialization with format=flowed; delsp=yes";
}

TEST(PlainTextSerializer, Bug1864820)
{
  nsString test(
      uR"#(
<html><body>
&gt;&nbsp;&nbsp;label=master&amp;label=experimental&amp;product=chrome&amp;product=firefox&amp;product=safari&amp;aligned&amp;view=interop&amp;q=label%3Ainterop-2023-property
<blockquote>
&gt;&nbsp;&nbsp;label=master&amp;label=experimental&amp;product=chrome&amp;product=firefox&amp;product=safari&amp;aligned&amp;view=interop&amp;q=label%3Ainterop-2023-property
</blockquote>
</body></html>
)#");

  ConvertBufToPlainText(test,
                        nsIDocumentEncoder::OutputFormatted |
                            nsIDocumentEncoder::OutputPersistNBSP |
                            nsIDocumentEncoder::OutputLFLineBreak |
                            nsIDocumentEncoder::OutputFormatFlowed,
                        kDefaultWrapColumn);

  nsString result(
      uR"#(
 >  label=master&label=experimental&product=chrome&product=firefox&product=safari&aligned&view=interop&q=label%3Ainterop-2023-property

     >  label=master&label=experimental&product=chrome&product=firefox&product=safari&aligned&view=interop&q=label%3Ainterop-2023-property
)#");
  result.Trim(" \n");
  test.Trim(" \n");
  ASSERT_TRUE(test.Equals(result))
  << "Shouldn't hang with format=flowed: " << NS_ConvertUTF16toUTF8(test).get();
}

// Test for CJK with format=flowed; delsp=yes
TEST(PlainTextSerializer, CJKWithFlowedDelSp)
{
  nsString test;
  nsString result;

  test.AssignLiteral("<html><body>");
  for (uint32_t i = 0; i < 40; i++) {
    // Insert Kanji (U+5341)
    test.Append(0x5341);
  }
  test.AppendLiteral("</body></html>");

  ConvertBufToPlainText(test,
                        nsIDocumentEncoder::OutputFormatted |
                            nsIDocumentEncoder::OutputCRLineBreak |
                            nsIDocumentEncoder::OutputLFLineBreak |
                            nsIDocumentEncoder::OutputFormatFlowed |
                            nsIDocumentEncoder::OutputFormatDelSp,
                        kDefaultWrapColumn);

  // create result case
  for (uint32_t i = 0; i < 36; i++) {
    result.Append(0x5341);
  }
  result.AppendLiteral(" \r\n");
  for (uint32_t i = 0; i < 4; i++) {
    result.Append(0x5341);
  }
  result.AppendLiteral("\r\n");

  ASSERT_TRUE(test.Equals(result))
  << "Wrong HTML to CJK text serialization with format=flowed; delsp=yes";
}

// Test for CJK with DisallowLineBreaking
TEST(PlainTextSerializer, CJKWithDisallowLineBreaking)
{
  nsString test;
  nsString result;

  test.AssignLiteral("<html><body>");
  for (uint32_t i = 0; i < 400; i++) {
    // Insert Kanji (U+5341)
    test.Append(0x5341);
  }
  test.AppendLiteral("</body></html>");

  ConvertBufToPlainText(test,
                        nsIDocumentEncoder::OutputFormatted |
                            nsIDocumentEncoder::OutputCRLineBreak |
                            nsIDocumentEncoder::OutputLFLineBreak |
                            nsIDocumentEncoder::OutputFormatFlowed |
                            nsIDocumentEncoder::OutputDisallowLineBreaking,
                        kDefaultWrapColumn);

  // create result case
  for (uint32_t i = 0; i < 400; i++) {
    result.Append(0x5341);
  }
  result.AppendLiteral("\r\n");

  ASSERT_TRUE(test.Equals(result))
  << "Wrong HTML to CJK text serialization with OutputDisallowLineBreaking";
}

// Test for Latin with DisallowLineBreaking
TEST(PlainTextSerializer, LatinWithDisallowLineBreaking)
{
  nsString test;
  test.AssignLiteral("<html><body>");
  for (uint32_t i = 0; i < 400; i++) {
    // Insert á (Latin Small Letter a with Acute) (U+00E1)
    test.Append(0x00E1);
  }
  test.AppendLiteral("</body></html>\r\n");

  ConvertBufToPlainText(test,
                        nsIDocumentEncoder::OutputFormatted |
                            nsIDocumentEncoder::OutputCRLineBreak |
                            nsIDocumentEncoder::OutputLFLineBreak |
                            nsIDocumentEncoder::OutputFormatFlowed |
                            nsIDocumentEncoder::OutputDisallowLineBreaking,
                        kDefaultWrapColumn);

  // Create expect case.
  nsString expect;
  for (uint32_t i = 0; i < 400; i++) {
    expect.Append(0x00E1);
  }
  expect.AppendLiteral(" \r\n\r\n");

  ASSERT_TRUE(test.Equals(expect))
  << "Wrong HTML to Latin text serialization with OutputDisallowLineBreaking";
}

// Test for ASCII with format=flowed; and quoted lines in preformatted span.
TEST(PlainTextSerializer, PreformatFlowedQuotes)
{
  nsString test;
  nsString result;

  test.AssignLiteral(
      "<html><body>"
      "<span style=\"white-space: pre-wrap;\" _moz_quote=\"true\">"
      "&gt; Firefox Firefox Firefox Firefox <br>"
      "&gt; Firefox Firefox Firefox Firefox<br>"
      "&gt;<br>"
      "&gt;&gt; Firefox Firefox Firefox Firefox <br>"
      "&gt;&gt; Firefox Firefox Firefox Firefox<br>"
      "</span></body></html>");

  ConvertBufToPlainText(test,
                        nsIDocumentEncoder::OutputFormatted |
                            nsIDocumentEncoder::OutputCRLineBreak |
                            nsIDocumentEncoder::OutputLFLineBreak |
                            nsIDocumentEncoder::OutputFormatFlowed,
                        kDefaultWrapColumn);

  // create result case
  result.AssignLiteral(
      "> Firefox Firefox Firefox Firefox \r\n"
      "> Firefox Firefox Firefox Firefox\r\n"
      ">\r\n"
      ">> Firefox Firefox Firefox Firefox \r\n"
      ">> Firefox Firefox Firefox Firefox\r\n");

  ASSERT_TRUE(test.Equals(result))
  << "Wrong HTML to ASCII text serialization "
     "with format=flowed; and quoted "
     "lines";
}

TEST(PlainTextSerializer, PrettyPrintedHtml)
{
  nsString test;
  test.AppendLiteral("<html>" NS_LINEBREAK "<body>" NS_LINEBREAK
                     "  first<br>" NS_LINEBREAK "  second<br>" NS_LINEBREAK
                     "</body>" NS_LINEBREAK "</html>");

  ConvertBufToPlainText(test, 0, kDefaultWrapColumn);
  ASSERT_TRUE(test.EqualsLiteral("first" NS_LINEBREAK "second" NS_LINEBREAK))
  << "Wrong prettyprinted html to text serialization";
}

TEST(PlainTextSerializer, PreElement)
{
  nsString test;
  test.AppendLiteral("<html>" NS_LINEBREAK "<body>" NS_LINEBREAK
                     "<pre>" NS_LINEBREAK "  first" NS_LINEBREAK
                     "  second" NS_LINEBREAK "</pre>" NS_LINEBREAK
                     "</body>" NS_LINEBREAK "</html>");

  ConvertBufToPlainText(test, 0, kDefaultWrapColumn);
  ASSERT_TRUE(test.EqualsLiteral("  first" NS_LINEBREAK
                                 "  second" NS_LINEBREAK NS_LINEBREAK))
  << "Wrong prettyprinted html to text serialization";
}

TEST(PlainTextSerializer, BlockElement)
{
  nsString test;
  test.AppendLiteral("<html>" NS_LINEBREAK "<body>" NS_LINEBREAK
                     "<div>" NS_LINEBREAK "  first" NS_LINEBREAK
                     "</div>" NS_LINEBREAK "<div>" NS_LINEBREAK
                     "  second" NS_LINEBREAK "</div>" NS_LINEBREAK
                     "</body>" NS_LINEBREAK "</html>");

  ConvertBufToPlainText(test, 0, kDefaultWrapColumn);
  ASSERT_TRUE(test.EqualsLiteral("first" NS_LINEBREAK "second" NS_LINEBREAK))
  << "Wrong prettyprinted html to text serialization";
}

TEST(PlainTextSerializer, PreWrapElementForThunderbird)
{
  // This test examines the magic pre-wrap setup that Thunderbird relies on.
  nsString test;
  test.AppendLiteral("<html>" NS_LINEBREAK
                     "<body style=\"white-space: pre-wrap;\">" NS_LINEBREAK
                     "<pre>" NS_LINEBREAK
                     "  first line is too long" NS_LINEBREAK
                     "  second line is even loooonger  " NS_LINEBREAK
                     "</pre>" NS_LINEBREAK "</body>" NS_LINEBREAK "</html>");

  const uint32_t wrapColumn = 10;
  ConvertBufToPlainText(test, nsIDocumentEncoder::OutputWrap, wrapColumn);
  // "\n\n  first\nline is\ntoo long\n  second\nline is\neven\nloooonger\n\n\n"
  ASSERT_TRUE(test.EqualsLiteral(
      NS_LINEBREAK NS_LINEBREAK
      "  first" NS_LINEBREAK "line is" NS_LINEBREAK "too long" NS_LINEBREAK
      "  second" NS_LINEBREAK "line is" NS_LINEBREAK "even" NS_LINEBREAK
      "loooonger" NS_LINEBREAK NS_LINEBREAK NS_LINEBREAK))
  << "Wrong prettyprinted html to text serialization";
}

TEST(PlainTextSerializer, Simple)
{
  nsString test;
  test.AppendLiteral(
      "<html><base>base</base><head><span>span</span></head>"
      "<body>body</body></html>");
  ConvertBufToPlainText(test, 0, kDefaultWrapColumn);
  ASSERT_TRUE(test.EqualsLiteral("basespanbody"))
  << "Wrong html to text serialization";
}

TEST(PlainTextSerializer, OneHundredAndOneOL)
{
  nsAutoString test;
  test.AppendLiteral(
      "<html>"
      "<body>"
      "<ol><ol><ol><ol><ol><ol><ol><ol><ol><ol><ol><ol><ol><ol><ol><ol><"
      "ol><ol><ol><ol><ol><ol><ol><ol><ol><ol><ol><ol><ol><ol><ol><ol><ol><ol><"
      "ol><ol><ol><ol><ol><ol><ol><ol><ol><ol><ol><ol><ol><ol><ol><ol><ol><ol><"
      "ol><ol><ol><ol><ol><ol><ol><ol><ol><ol><ol><ol><ol><ol><ol><ol><ol><ol><"
      "ol><ol><ol><ol><ol><ol><ol><ol><ol><ol><ol><ol><ol><ol><ol><ol><ol><ol><"
      "ol><ol><ol><ol><ol><ol><ol><ol><ol><ol><ol><ol><ol><ol></ol></ol></ol></"
      "ol></ol></ol></ol></ol></ol></ol></ol></ol></ol></ol></ol></"
      "ol></ol></ol></ol></ol></ol></ol></ol></ol></ol></ol></ol></ol></ol></"
      "ol></ol></ol></ol></ol></ol></ol></ol></ol></ol></ol></ol></ol></ol></"
      "ol></ol></ol></ol></ol></ol></ol></ol></ol></ol></ol></ol></ol></ol></"
      "ol></ol></ol></ol></ol></ol></ol></ol></ol></ol></ol></ol></ol></ol></"
      "ol></ol></ol></ol></ol></ol></ol></ol></ol></ol></ol></ol></ol></ol></"
      "ol></ol></ol></ol></ol></ol></ol></ol></ol></ol></ol></ol></ol></ol></"
      "ol></ol><li>X</li></ol>"
      "</body>"
      "</html>");

  ConvertBufToPlainText(test, nsIDocumentEncoder::OutputFormatted,
                        kDefaultWrapColumn);

  nsAutoString expected;
  expected.AppendLiteral(" 1. X" NS_LINEBREAK);
  ASSERT_EQ(test, expected);
}

TEST(PlainTextSerializer, BlockQuoteCite)
{
  nsAutoString test;
  test.AppendLiteral(u"<blockquote type=cite>hello world</blockquote>");

  const uint32_t wrapColumn = 10;
  ConvertBufToPlainText(test,
                        nsIDocumentEncoder::OutputFormatted |
                            nsIDocumentEncoder::OutputFormatFlowed |
                            nsIDocumentEncoder::OutputCRLineBreak |
                            nsIDocumentEncoder::OutputLFLineBreak,
                        wrapColumn);

  constexpr auto expect = NS_LITERAL_STRING_FROM_CSTRING(
      "> hello \r\n"
      "> world\r\n");

  ASSERT_TRUE(test.Equals(expect))
  << "Wrong blockquote cite to text serialization";
}
