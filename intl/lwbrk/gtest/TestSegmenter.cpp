/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "mozilla/intl/Segmenter.h"

namespace mozilla::intl {

TEST(IntlSegmenter, TestLineBreakIteratorUtf16)
{
  const SegmenterOptions options{SegmenterGranularity::Line};
  auto result = Segmenter::TryCreate("en", options);
  ASSERT_TRUE(result.isOk());
  auto lineSegmenter = result.unwrap();

  const char16_t text[] = u"hello world";
  UniquePtr<SegmentIteratorUtf16> segIter =
      lineSegmenter->Segment(MakeStringSpan(text));

  // Seek to space between "hello" and "world".
  ASSERT_EQ(segIter->Seek(5u), Some(11u));

  ASSERT_EQ(segIter->Next(), Nothing());

  // Same as calling Next().
  ASSERT_EQ(segIter->Seek(0u), Nothing());
}

TEST(IntlSegmenter, TestWordBreakIteratorUtf16)
{
  const SegmenterOptions options{SegmenterGranularity::Word};
  auto result = Segmenter::TryCreate("en", options);
  ASSERT_TRUE(result.isOk());
  auto wordSegmenter = result.unwrap();

  const char16_t text[] = u"hello world";
  UniquePtr<SegmentIteratorUtf16> segIter =
      wordSegmenter->Segment(MakeStringSpan(text));

  // Seek to the space between "hello" and "world"
  ASSERT_EQ(segIter->Seek(5u), Some(6u));

  ASSERT_EQ(segIter->Next(), Some(11u));
  ASSERT_EQ(segIter->Next(), Nothing());

  // Same as calling Next().
  ASSERT_EQ(segIter->Seek(0u), Nothing());
}

TEST(IntlSegmenter, TestGraphemeClusterBreakIteratorUtf16)
{
  SegmenterOptions options{SegmenterGranularity::Grapheme};
  auto result = Segmenter::TryCreate("en", options);
  ASSERT_TRUE(result.isOk());
  auto graphemeClusterSegmenter = result.unwrap();

  const char16_t text[] = u"hello world";
  UniquePtr<SegmentIteratorUtf16> segIter =
      graphemeClusterSegmenter->Segment(MakeStringSpan(text));

  // Seek to the space between "hello" and "world"
  ASSERT_EQ(segIter->Seek(5u), Some(6u));

  ASSERT_EQ(segIter->Next(), Some(7u));
  ASSERT_EQ(segIter->Next(), Some(8u));
  ASSERT_EQ(segIter->Next(), Some(9u));
  ASSERT_EQ(segIter->Next(), Some(10u));
  ASSERT_EQ(segIter->Next(), Some(11u));
  ASSERT_EQ(segIter->Next(), Nothing());

  // Same as calling Next().
  ASSERT_EQ(segIter->Seek(0u), Nothing());
}

TEST(IntlSegmenter, TestGraphemeClusterBreakReverseIteratorUtf16)
{
  const char16_t text[] = u"hello world";
  GraphemeClusterBreakReverseIteratorUtf16 segIter(MakeStringSpan(text));

  // Seek to the space between "hello" and "world"
  ASSERT_EQ(segIter.Seek(6u), Some(5u));

  ASSERT_EQ(segIter.Next(), Some(4u));
  ASSERT_EQ(segIter.Next(), Some(3u));
  ASSERT_EQ(segIter.Next(), Some(2u));
  ASSERT_EQ(segIter.Next(), Some(1u));
  ASSERT_EQ(segIter.Next(), Some(0u));
  ASSERT_EQ(segIter.Next(), Nothing());

  // Same as calling Next().
  ASSERT_EQ(segIter.Seek(0u), Nothing());
}

TEST(IntlSegmenter, TestSentenceBreakIteratorUtf16)
{
  SegmenterOptions options{SegmenterGranularity::Sentence};
  auto result = Segmenter::TryCreate("en", options);
  ASSERT_TRUE(result.isErr());
}

}  // namespace mozilla::intl
