/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestHarness.h"

#include "nsServiceManagerUtils.h"
#include "nsStringGlue.h"
#include "nsIDocumentEncoder.h"
#include "nsCRT.h"
#include "nsIParserUtils.h"

void
ConvertBufToPlainText(nsString &aConBuf, int aFlag)
{
  nsCOMPtr<nsIParserUtils> utils =
    do_GetService(NS_PARSERUTILS_CONTRACTID);
  utils->ConvertToPlainText(aConBuf, aFlag, 72, aConBuf);
}

// Test for ASCII with format=flowed; delsp=yes
nsresult
TestASCIIWithFlowedDelSp()
{
  nsString test;
  nsString result;

  test.AssignLiteral("<html><body>"
                     "Firefox Firefox Firefox Firefox "
                     "Firefox Firefox Firefox Firefox "
                     "Firefox Firefox Firefox Firefox"
                     "</body></html>");

  ConvertBufToPlainText(test, nsIDocumentEncoder::OutputFormatted |
                              nsIDocumentEncoder::OutputCRLineBreak |
                              nsIDocumentEncoder::OutputLFLineBreak |
                              nsIDocumentEncoder::OutputFormatFlowed |
                              nsIDocumentEncoder::OutputFormatDelSp);

  // create result case
  result.AssignLiteral("Firefox Firefox Firefox Firefox "
                       "Firefox Firefox Firefox Firefox "
                       "Firefox  \r\nFirefox Firefox Firefox\r\n");

  if (!test.Equals(result)) {
    fail("Wrong HTML to ASCII text serialization with format=flowed; delsp=yes");
    return NS_ERROR_FAILURE;
  }

  passed("HTML to ASCII text serialization with format=flowed; delsp=yes");

  return NS_OK;
}

// Test for CJK with format=flowed; delsp=yes
nsresult
TestCJKWithFlowedDelSp()
{
  nsString test;
  nsString result;

  test.AssignLiteral("<html><body>");
  for (uint32_t i = 0; i < 40; i++) {
    // Insert Kanji (U+5341)
    test.Append(0x5341);
  }
  test.AppendLiteral("</body></html>");

  ConvertBufToPlainText(test, nsIDocumentEncoder::OutputFormatted |
                              nsIDocumentEncoder::OutputCRLineBreak |
                              nsIDocumentEncoder::OutputLFLineBreak |
                              nsIDocumentEncoder::OutputFormatFlowed |
                              nsIDocumentEncoder::OutputFormatDelSp);

  // create result case
  for (uint32_t i = 0; i < 36; i++) {
    result.Append(0x5341);
  }
  result.AppendLiteral(" \r\n");
  for (uint32_t i = 0; i < 4; i++) {
    result.Append(0x5341);
  }
  result.AppendLiteral("\r\n");

  if (!test.Equals(result)) {
    fail("Wrong HTML to CJK text serialization with format=flowed; delsp=yes");
    return NS_ERROR_FAILURE;
  }

  passed("HTML to CJK text serialization with format=flowed; delsp=yes");

  return NS_OK;
}

// Test for CJK with DisallowLineBreaking
nsresult
TestCJKWithDisallowLineBreaking()
{
  nsString test;
  nsString result;

  test.AssignLiteral("<html><body>");
  for (uint32_t i = 0; i < 400; i++) {
    // Insert Kanji (U+5341)
    test.Append(0x5341);
  }
  test.AppendLiteral("</body></html>");

  ConvertBufToPlainText(test, nsIDocumentEncoder::OutputFormatted |
                              nsIDocumentEncoder::OutputCRLineBreak |
                              nsIDocumentEncoder::OutputLFLineBreak |
                              nsIDocumentEncoder::OutputFormatFlowed |
                              nsIDocumentEncoder::OutputDisallowLineBreaking);

  // create result case
  for (uint32_t i = 0; i < 400; i++) {
    result.Append(0x5341);
  }
  result.AppendLiteral("\r\n");

  if (!test.Equals(result)) {
    fail("Wrong HTML to CJK text serialization with OutputDisallowLineBreaking");
    return NS_ERROR_FAILURE;
  }

  passed("HTML to CJK text serialization with OutputDisallowLineBreaking");

  return NS_OK;
}

nsresult
TestPrettyPrintedHtml()
{
  nsString test;
  test.AppendLiteral(
    "<html>" NS_LINEBREAK
    "<body>" NS_LINEBREAK
    "  first<br>" NS_LINEBREAK
    "  second<br>" NS_LINEBREAK
    "</body>" NS_LINEBREAK "</html>");

  ConvertBufToPlainText(test, 0);
  if (!test.EqualsLiteral("first" NS_LINEBREAK "second" NS_LINEBREAK)) {
    fail("Wrong prettyprinted html to text serialization");
    return NS_ERROR_FAILURE;
  }

  passed("prettyprinted HTML to text serialization test");
  return NS_OK;
}

nsresult
TestPreElement()
{
  nsString test;
  test.AppendLiteral(
    "<html>" NS_LINEBREAK
    "<body>" NS_LINEBREAK
    "<pre>" NS_LINEBREAK
    "  first" NS_LINEBREAK
    "  second" NS_LINEBREAK
    "</pre>" NS_LINEBREAK
    "</body>" NS_LINEBREAK "</html>");

  ConvertBufToPlainText(test, 0);
  if (!test.EqualsLiteral("  first" NS_LINEBREAK "  second" NS_LINEBREAK NS_LINEBREAK)) {
    fail("Wrong prettyprinted html to text serialization");
    return NS_ERROR_FAILURE;
  }

  passed("prettyprinted HTML to text serialization test");
  return NS_OK;
}

nsresult
TestBlockElement()
{
  nsString test;
  test.AppendLiteral(
    "<html>" NS_LINEBREAK
    "<body>" NS_LINEBREAK
    "<div>" NS_LINEBREAK
    "  first" NS_LINEBREAK
    "</div>" NS_LINEBREAK
    "<div>" NS_LINEBREAK
    "  second" NS_LINEBREAK
    "</div>" NS_LINEBREAK
    "</body>" NS_LINEBREAK "</html>");

  ConvertBufToPlainText(test, 0);
  if (!test.EqualsLiteral("first" NS_LINEBREAK "second" NS_LINEBREAK)) {
    fail("Wrong prettyprinted html to text serialization");
    return NS_ERROR_FAILURE;
  }

  passed("prettyprinted HTML to text serialization test");
  return NS_OK;
}

nsresult
TestPreWrapElementForThunderbird()
{
  // This test examines the magic pre-wrap setup that Thunderbird relies on.
  nsString test;
  test.AppendLiteral(
    "<html>" NS_LINEBREAK
    "<body style=\"white-space: pre-wrap; width: 10ch;\">" NS_LINEBREAK
    "<pre>" NS_LINEBREAK
    "  first line is too long" NS_LINEBREAK
    "  second line is even loooonger  " NS_LINEBREAK
    "</pre>" NS_LINEBREAK
    "</body>" NS_LINEBREAK "</html>");

  ConvertBufToPlainText(test, nsIDocumentEncoder::OutputWrap);
  // "\n\n  first\nline is\ntoo long\n  second\nline is\neven\nloooonger\n\n\n"
  if (!test.EqualsLiteral(NS_LINEBREAK NS_LINEBREAK
                          "  first" NS_LINEBREAK
                          "line is" NS_LINEBREAK
                          "too long" NS_LINEBREAK
                          "  second" NS_LINEBREAK
                          "line is" NS_LINEBREAK
                          "even" NS_LINEBREAK
                          "loooonger" NS_LINEBREAK
                          NS_LINEBREAK NS_LINEBREAK)) {
    fail("Wrong prettyprinted html to text serialization");
    return NS_ERROR_FAILURE;
  }

  passed("prettyprinted HTML to text serialization test");
  return NS_OK;
}

nsresult
TestPlainTextSerializer()
{
  nsString test;
  test.AppendLiteral("<html><base>base</base><head><span>span</span></head>"
                     "<body>body</body></html>");
  ConvertBufToPlainText(test, 0);
  if (!test.EqualsLiteral("basespanbody")) {
    fail("Wrong html to text serialization");
    return NS_ERROR_FAILURE;
  }

  passed("HTML to text serialization test");

  nsresult rv = TestASCIIWithFlowedDelSp();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = TestCJKWithFlowedDelSp();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = TestPrettyPrintedHtml();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = TestPreElement();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = TestBlockElement();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = TestPreWrapElementForThunderbird();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = TestCJKWithDisallowLineBreaking();
  NS_ENSURE_SUCCESS(rv, rv);

  // Add new tests here...
  return NS_OK;
}

int main(int argc, char** argv)
{
  ScopedXPCOM xpcom("PlainTextSerializer");
  if (xpcom.failed())
    return 1;

  int retval = 0;
  if (NS_FAILED(TestPlainTextSerializer())) {
    retval = 1;
  }

  return retval;
}
