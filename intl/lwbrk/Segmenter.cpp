/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Classes to iterate over grapheme, word, sentence, or line. */

#include "mozilla/intl/Segmenter.h"

#include "mozilla/intl/LineBreaker.h"
#include "mozilla/intl/WordBreaker.h"

namespace mozilla::intl {

SegmentIteratorUtf16::SegmentIteratorUtf16(Span<const char16_t> aText)
    : mText(aText) {}

Maybe<uint32_t> SegmentIteratorUtf16::Seek(uint32_t aPos) {
  if (mPos < aPos) {
    mPos = aPos;
  }
  return Next();
}

LineBreakIteratorUtf16::LineBreakIteratorUtf16(Span<const char16_t> aText,
                                               const LineBreakOptions& aOptions)
    : SegmentIteratorUtf16(aText), mOptions(aOptions) {}

Maybe<uint32_t> LineBreakIteratorUtf16::Next() {
  const int32_t nextPos =
      LineBreaker::Next(mText.Elements(), mText.Length(), mPos);
  if (nextPos == NS_LINEBREAKER_NEED_MORE_TEXT) {
    return Nothing();
  }
  mPos = nextPos;
  return Some(mPos);
}

WordBreakIteratorUtf16::WordBreakIteratorUtf16(Span<const char16_t> aText)
    : SegmentIteratorUtf16(aText) {}

Maybe<uint32_t> WordBreakIteratorUtf16::Next() {
  const int32_t nextPos =
      WordBreaker::Next(mText.Elements(), mText.Length(), mPos);
  if (nextPos == NS_WORDBREAKER_NEED_MORE_TEXT) {
    return Nothing();
  }
  mPos = nextPos;
  return Some(mPos);
}

Result<UniquePtr<Segmenter>, ICUError> Segmenter::TryCreate(
    Span<const char> aLocale, const SegmenterOptions& aOptions) {
  if (aOptions.mGranularity == SegmenterGranularity::Grapheme ||
      aOptions.mGranularity == SegmenterGranularity::Sentence) {
    // Grapheme and Sentence iterator are not yet implemented.
    return Err(ICUError::InternalError);
  }
  return MakeUnique<Segmenter>(aLocale, aOptions);
}

UniquePtr<SegmentIteratorUtf16> Segmenter::Segment(
    Span<const char16_t> aText) const {
  switch (mOptions.mGranularity) {
    case SegmenterGranularity::Grapheme:
    case SegmenterGranularity::Sentence:
      MOZ_ASSERT_UNREACHABLE("Unimplemented yet!");
      return nullptr;
    case SegmenterGranularity::Word:
      return MakeUnique<WordBreakIteratorUtf16>(aText);
    case SegmenterGranularity::Line:
      return MakeUnique<LineBreakIteratorUtf16>(aText);
  }
  MOZ_ASSERT_UNREACHABLE("All granularities must be handled!");
  return nullptr;
}

}  // namespace mozilla::intl
