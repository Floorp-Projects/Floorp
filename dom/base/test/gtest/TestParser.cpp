/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "mozilla/dom/DOMParser.h"
#include "mozilla/dom/Document.h"
#include "nsIDocumentEncoder.h"
#include "mozilla/ErrorResult.h"

// This is a test for mozilla::dom::DOMParser::CreateWithoutGlobal() which was
// implemented for use in Thunderbird's MailNews module.

// int main(int argc, char** argv)
TEST(TestParser, TestParserMain)
{
  bool allTestsPassed = false;
  constexpr auto htmlInput =
      u"<html><head>"
      "<meta http-equiv=\"content-type\" content=\"text/html; charset=\">"
      "</head><body>Hello <b>Thunderbird!</b></body></html>"_ns;

  do {
    // Parse the HTML source.
    mozilla::IgnoredErrorResult rv2;
    RefPtr<mozilla::dom::DOMParser> parser =
        mozilla::dom::DOMParser::CreateWithoutGlobal(rv2);
    if (rv2.Failed()) break;
    nsCOMPtr<mozilla::dom::Document> document = parser->ParseFromString(
        htmlInput, mozilla::dom::SupportedType::Text_html, rv2);
    if (rv2.Failed()) break;

    // Serialize it back to HTML source again.
    nsCOMPtr<nsIDocumentEncoder> encoder =
        do_createDocumentEncoder("text/html");
    if (!encoder) break;
    nsresult rv =
        encoder->Init(document, u"text/html"_ns, nsIDocumentEncoder::OutputRaw);
    if (NS_FAILED(rv)) break;
    nsString parsed;
    rv = encoder->EncodeToString(parsed);
    if (NS_FAILED(rv)) break;

    EXPECT_TRUE(parsed.Equals(htmlInput));
    allTestsPassed = true;
  } while (false);

  EXPECT_TRUE(allTestsPassed);
}
