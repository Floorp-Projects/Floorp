/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "nsCOMPtr.h"
#include "nsIDocumentEncoder.h"
#include "nsString.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/DOMParser.h"

using namespace mozilla;
using namespace mozilla::dom;

// Test that serialising some DOM doesn't destroy links by word-wrapping long
// href values containing spaces.
TEST(TestXMLSerializerNoBreakLink, TestXMLSerializerNoBreakLinkMain)
{
  // Build up a stupidly-long URL with spaces. Default is to wrap at column
  // 72, so we want to exceed that.
  nsString longURL = u"http://www.example.com/link with spaces"_ns;
  for (int i = 1; i < 125; ++i) {
    longURL.Append(u' ');
    longURL.Append(IntToTString<char16_t>(i));
  }
  nsString htmlInput =
      u"<html><head>"
      "<meta charset=\"utf-8\">"
      "</head><body>Hello Thunderbird! <a href=\""_ns +
      longURL + u"\">Link</a></body></html>"_ns;

  // Parse HTML into a Document.
  nsCOMPtr<Document> document;
  {
    IgnoredErrorResult rv;
    RefPtr<DOMParser> parser = DOMParser::CreateWithoutGlobal(rv);
    ASSERT_FALSE(rv.Failed());
    document = parser->ParseFromString(htmlInput, SupportedType::Text_html, rv);
    ASSERT_FALSE(rv.Failed());
  }

  // Serialize back in a variety of flavours and check the URL survives the
  // round trip intact.
  nsCString contentTypes[] = {"text/xml"_ns, "application/xml"_ns,
                              "application/xhtml+xml"_ns, "image/svg+xml"_ns,
                              "text/html"_ns};
  for (auto const& contentType : contentTypes) {
    uint32_t flagsToTest[] = {
        nsIDocumentEncoder::OutputFormatted, nsIDocumentEncoder::OutputWrap,
        nsIDocumentEncoder::OutputFormatted | nsIDocumentEncoder::OutputWrap};
    for (uint32_t flags : flagsToTest) {
      // Serialize doc back to HTML source again.
      nsCOMPtr<nsIDocumentEncoder> encoder =
          do_createDocumentEncoder(contentType.get());
      ASSERT_TRUE(encoder);
      nsresult rv =
          encoder->Init(document, NS_ConvertASCIItoUTF16(contentType), flags);
      ASSERT_TRUE(NS_SUCCEEDED(rv));
      nsString parsed;
      rv = encoder->EncodeToString(parsed);
      ASSERT_TRUE(NS_SUCCEEDED(rv));

      // URL is intact?
      EXPECT_TRUE(parsed.Find(longURL) != kNotFound);
    }
  }
}
