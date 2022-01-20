/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gtest/gtest.h"

#include "mozilla/intl/NumberParser.h"

namespace mozilla::intl {

TEST(IntlNumberParser, Basic)
{
  // Try with an English locale
  UniquePtr<NumberParser> np = NumberParser::TryCreate("en-US", true).unwrap();
  auto result = np->ParseDouble(MakeStringSpan(u"1,234.56"));
  ASSERT_TRUE(result.isOk());
  ASSERT_EQ(result.unwrap().first, 1234.56);
  ASSERT_EQ(result.unwrap().second, 8);

  // Disable grouping, parsing will stop at the first comma
  np = NumberParser::TryCreate("en-US", false).unwrap();
  result = np->ParseDouble(MakeStringSpan(u"1,234.56"));
  ASSERT_TRUE(result.isOk());
  ASSERT_EQ(result.unwrap().first, 1);
  ASSERT_EQ(result.unwrap().second, 1);

  // Try with a Spanish locale
  np = NumberParser::TryCreate("es-CR", true).unwrap();
  result = np->ParseDouble(MakeStringSpan(u"1234,56"));
  ASSERT_TRUE(result.isOk());
  ASSERT_EQ(result.unwrap().first, 1234.56);
  ASSERT_EQ(result.unwrap().second, 7);
}

}  // namespace mozilla::intl
