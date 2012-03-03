/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Olli Pettay <Olli.Pettay@helsinki.fi> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
  for (PRUint32 i = 0; i < 40; i++) {
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
  for (PRUint32 i = 0; i < 36; i++) {
    result.Append(0x5341);
  }
  result.Append(NS_LITERAL_STRING(" \r\n"));
  for (PRUint32 i = 0; i < 4; i++) {
    result.Append(0x5341);
  }
  result.Append(NS_LITERAL_STRING("\r\n"));

  if (!test.Equals(result)) {
    fail("Wrong HTML to CJK text serialization with format=flowed; delsp=yes");
    return NS_ERROR_FAILURE;
  }

  passed("HTML to CJK text serialization with format=flowed; delsp=yes");

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
