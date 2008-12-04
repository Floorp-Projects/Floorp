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

#include "nsIParser.h"
#include "nsIHTMLToTextSink.h"
#include "nsIParser.h"
#include "nsIContentSink.h"
#include "nsIParserService.h"
#include "nsServiceManagerUtils.h"
#include "nsStringGlue.h"
#include "nsParserCIID.h"

static NS_DEFINE_CID(kCParserCID, NS_PARSER_CID);

void
ConvertBufToPlainText(nsString &aConBuf)
{
  nsCOMPtr<nsIParser> parser = do_CreateInstance(kCParserCID);
  if (parser) {
    nsCOMPtr<nsIContentSink> sink;
    sink = do_CreateInstance(NS_PLAINTEXTSINK_CONTRACTID);
    if (sink) {
      nsCOMPtr<nsIHTMLToTextSink> textSink(do_QueryInterface(sink));
      if (textSink) {
        nsAutoString convertedText;
        textSink->Initialize(&convertedText, 0, 72);
        parser->SetContentSink(sink);
        parser->Parse(aConBuf, 0, NS_LITERAL_CSTRING("text/html"), PR_TRUE);
        aConBuf = convertedText;
      }
    }
  }
}

nsresult
TestPlainTextSerializer()
{
  nsString test;
  test.AppendLiteral("<html><base>base</base><head><span>span</span></head>"
                     "<body>body</body></html>");
  ConvertBufToPlainText(test);
  if (!test.EqualsLiteral("basespanbody")) {
    fail("Wrong html to text serialization");
    return NS_ERROR_FAILURE;
  }

  passed("HTML to text serialization test");

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
