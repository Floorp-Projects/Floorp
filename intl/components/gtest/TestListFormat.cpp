/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gtest/gtest.h"

#include "mozilla/intl/ListFormat.h"
#include "mozilla/Span.h"
#include "TestBuffer.h"

namespace mozilla::intl {

// Test ListFormat.format with default options.
TEST(IntlListFormat, FormatDefault)
{
  ListFormat::Options options;
  UniquePtr<ListFormat> lf =
      ListFormat::TryCreate(MakeStringSpan("en-US"), options).unwrap();
  ListFormat::StringList list;
  MOZ_RELEASE_ASSERT(list.append(MakeStringSpan(u"Alice")));
  MOZ_RELEASE_ASSERT(list.append(MakeStringSpan(u"Bob")));
  MOZ_RELEASE_ASSERT(list.append(MakeStringSpan(u"Charlie")));
  TestBuffer<char16_t> buf16;
  ASSERT_TRUE(lf->Format(list, buf16).isOk());
  ASSERT_EQ(buf16.get_string_view(), u"Alice, Bob, and Charlie");

  UniquePtr<ListFormat> lfDe =
      ListFormat::TryCreate(MakeStringSpan("de"), options).unwrap();
  ASSERT_TRUE(lfDe->Format(list, buf16).isOk());
  ASSERT_EQ(buf16.get_string_view(), u"Alice, Bob und Charlie");
}

// Test ListFormat.format with Type::Conjunction and other styles.
TEST(IntlListFormat, FormatConjunction)
{
  ListFormat::Options options{ListFormat::Type::Conjunction,
                              ListFormat::Style::Narrow};
  UniquePtr<ListFormat> lf =
      ListFormat::TryCreate(MakeStringSpan("en-US"), options).unwrap();
  ListFormat::StringList list;
  MOZ_RELEASE_ASSERT(list.append(MakeStringSpan(u"Alice")));
  MOZ_RELEASE_ASSERT(list.append(MakeStringSpan(u"Bob")));
  MOZ_RELEASE_ASSERT(list.append(MakeStringSpan(u"Charlie")));
  TestBuffer<char16_t> buf16;
  ASSERT_TRUE(lf->Format(list, buf16).isOk());
  ASSERT_EQ(buf16.get_string_view(), u"Alice, Bob, Charlie");

  ListFormat::Options optionsSh{ListFormat::Type::Conjunction,
                                ListFormat::Style::Short};
  UniquePtr<ListFormat> lfSh =
      ListFormat::TryCreate(MakeStringSpan("en-US"), optionsSh).unwrap();
  ASSERT_TRUE(lfSh->Format(list, buf16).isOk());
  ASSERT_EQ(buf16.get_string_view(), u"Alice, Bob, & Charlie");
}

// Test ListFormat.format with Type::Disjunction.
TEST(IntlListFormat, FormatDisjunction)
{
  // When Type is Disjunction, the results will be the same regardless of the
  // style for most locales, so simply test with Style::Long.
  ListFormat::Options options{ListFormat::Type::Disjunction,
                              ListFormat::Style::Long};
  UniquePtr<ListFormat> lf =
      ListFormat::TryCreate(MakeStringSpan("en-US"), options).unwrap();
  ListFormat::StringList list;
  MOZ_RELEASE_ASSERT(list.append(MakeStringSpan(u"Alice")));
  MOZ_RELEASE_ASSERT(list.append(MakeStringSpan(u"Bob")));
  MOZ_RELEASE_ASSERT(list.append(MakeStringSpan(u"Charlie")));
  TestBuffer<char16_t> buf16;
  ASSERT_TRUE(lf->Format(list, buf16).isOk());
  ASSERT_EQ(buf16.get_string_view(), u"Alice, Bob, or Charlie");
}

// Test ListFormat.format with Type::Unit.
TEST(IntlListFormat, FormatUnit)
{
  ListFormat::Options options{ListFormat::Type::Unit, ListFormat::Style::Long};
  // For locale "en", Style::Long and Style::Short have the same result, so just
  // test Style::Long here.
  UniquePtr<ListFormat> lf =
      ListFormat::TryCreate(MakeStringSpan("en-US"), options).unwrap();
  ListFormat::StringList list;
  MOZ_RELEASE_ASSERT(list.append(MakeStringSpan(u"Alice")));
  MOZ_RELEASE_ASSERT(list.append(MakeStringSpan(u"Bob")));
  MOZ_RELEASE_ASSERT(list.append(MakeStringSpan(u"Charlie")));
  TestBuffer<char16_t> buf16;
  ASSERT_TRUE(lf->Format(list, buf16).isOk());
  ASSERT_EQ(buf16.get_string_view(), u"Alice, Bob, Charlie");

  ListFormat::Options optionsNa{ListFormat::Type::Unit,
                                ListFormat::Style::Narrow};
  UniquePtr<ListFormat> lfNa =
      ListFormat::TryCreate(MakeStringSpan("en-US"), optionsNa).unwrap();
  ASSERT_TRUE(lfNa->Format(list, buf16).isOk());
  ASSERT_EQ(buf16.get_string_view(), u"Alice Bob Charlie");
}

// Pass a long list (list.length() > DEFAULT_LIST_LENGTH) and check the result
// is still correct. (result.length > INITIAL_CHAR_BUFFER_SIZE)
TEST(IntlListFormat, FormatBufferLength)
{
  ListFormat::Options options;
  UniquePtr<ListFormat> lf =
      ListFormat::TryCreate(MakeStringSpan("en-US"), options).unwrap();
  ListFormat::StringList list;
  MOZ_RELEASE_ASSERT(list.append(MakeStringSpan(u"Alice")));
  MOZ_RELEASE_ASSERT(list.append(MakeStringSpan(u"Bob")));
  MOZ_RELEASE_ASSERT(list.append(MakeStringSpan(u"Charlie")));
  MOZ_RELEASE_ASSERT(list.append(MakeStringSpan(u"David")));
  MOZ_RELEASE_ASSERT(list.append(MakeStringSpan(u"Eve")));
  MOZ_RELEASE_ASSERT(list.append(MakeStringSpan(u"Frank")));
  MOZ_RELEASE_ASSERT(list.append(MakeStringSpan(u"Grace")));
  MOZ_RELEASE_ASSERT(list.append(MakeStringSpan(u"Heidi")));
  MOZ_RELEASE_ASSERT(list.append(MakeStringSpan(u"Ivan")));
  TestBuffer<char16_t> buf16;
  ASSERT_TRUE(lf->Format(list, buf16).isOk());
  ASSERT_EQ(buf16.get_string_view(),
            u"Alice, Bob, Charlie, David, Eve, Frank, Grace, Heidi, and Ivan");
}

TEST(IntlListFormat, FormatToParts)
{
  ListFormat::Options options;
  UniquePtr<ListFormat> lf =
      ListFormat::TryCreate(MakeStringSpan("en-US"), options).unwrap();
  ListFormat::StringList list;
  MOZ_RELEASE_ASSERT(list.append(MakeStringSpan(u"Alice")));
  MOZ_RELEASE_ASSERT(list.append(MakeStringSpan(u"Bob")));
  MOZ_RELEASE_ASSERT(list.append(MakeStringSpan(u"Charlie")));

  TestBuffer<char16_t> buf16;
  mozilla::intl::ListFormat::PartVector parts;
  ASSERT_TRUE(lf->FormatToParts(list, buf16, parts).isOk());

  std::u16string_view strView = buf16.get_string_view();
  ASSERT_EQ(strView, u"Alice, Bob, and Charlie");

  // 3 elements, and 2 literals.
  ASSERT_EQ((parts.length()), (5u));

  auto getSubStringView = [strView, &parts](size_t index) {
    size_t pos = index == 0 ? 0 : parts[index - 1].second;
    size_t count = parts[index].second - pos;
    return strView.substr(pos, count);
  };

  ASSERT_EQ(parts[0].first, ListFormat::PartType::Element);
  ASSERT_EQ(getSubStringView(0), u"Alice");

  ASSERT_EQ(parts[1].first, ListFormat::PartType::Literal);
  ASSERT_EQ(getSubStringView(1), u", ");

  ASSERT_EQ(parts[2].first, ListFormat::PartType::Element);
  ASSERT_EQ(getSubStringView(2), u"Bob");

  ASSERT_EQ(parts[3].first, ListFormat::PartType::Literal);
  ASSERT_EQ(getSubStringView(3), u", and ");

  ASSERT_EQ(parts[4].first, ListFormat::PartType::Element);
  ASSERT_EQ(getSubStringView(4), u"Charlie");
}

}  // namespace mozilla::intl
