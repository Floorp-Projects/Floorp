/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Classes to iterate over grapheme, word, sentence, or line. */

#ifndef intl_components_Segmenter_h_
#define intl_components_Segmenter_h_

#include "mozilla/intl/ICUError.h"
#include "mozilla/Maybe.h"
#include "mozilla/Result.h"
#include "mozilla/Span.h"
#include "mozilla/UniquePtr.h"

#if defined(MOZ_ICU4X) && defined(JS_HAS_INTL_API)
namespace capi {
struct ICU4XLineSegmenter;
struct ICU4XLineBreakIteratorUtf16;
struct ICU4XWordSegmenter;
struct ICU4XWordBreakIteratorUtf16;
struct ICU4XGraphemeClusterSegmenter;
struct ICU4XGraphemeClusterBreakIteratorUtf16;
struct ICU4XSentenceSegmenter;
struct ICU4XSentenceBreakIteratorUtf16;
}  // namespace capi
#endif

namespace mozilla::intl {

enum class SegmenterGranularity : uint8_t {
  Grapheme,
  Word,
  Sentence,
  Line,
};

struct SegmenterOptions final {
  SegmenterGranularity mGranularity = SegmenterGranularity::Grapheme;
};

/**
 * Interface of segment iterators. Subclass this class to implement iterator for
 * UTF-16 text.
 */
class SegmentIteratorUtf16 {
 public:
  virtual ~SegmentIteratorUtf16() = default;

  // Disable copy or move semantics. Move semantic could be enabled in the
  // future if needed.
  SegmentIteratorUtf16(SegmentIteratorUtf16&&) = delete;
  SegmentIteratorUtf16& operator=(SegmentIteratorUtf16&&) = delete;
  SegmentIteratorUtf16(const SegmentIteratorUtf16&) = delete;
  SegmentIteratorUtf16& operator=(const SegmentIteratorUtf16&) = delete;

  /**
   * Advance the iterator to the next break position.
   *
   * @return the break position. If there's no further break position, return
   * Nothing().
   */
  virtual Maybe<uint32_t> Next() = 0;

  /**
   * Advance the iterator to the first break position following the specified
   * position aPos.
   *
   * Note: if this iterator's current position is already >= aPos, this method
   * behaves the same as Next().
   */
  virtual Maybe<uint32_t> Seek(uint32_t aPos);

 protected:
  explicit SegmentIteratorUtf16(Span<const char16_t> aText);

  // The text to iterate over.
  Span<const char16_t> mText;

  // The current break position within mText.
  uint32_t mPos = 0;
};

// Each enum value has the same meaning with respect to the `word-break`
// property values in the CSS Text spec. See the details in
// https://drafts.csswg.org/css-text-3/#word-break-property
enum class WordBreakRule : uint8_t {
  Normal = 0,
  BreakAll,
  KeepAll,
};

// Each enum value has the same meaning with respect to the `line-break`
// property values in the CSS Text spec. See the details in
// https://drafts.csswg.org/css-text-3/#line-break-property.
enum class LineBreakRule : uint8_t {
  Auto = 0,
  Loose,
  Normal,
  Strict,
  Anywhere,
};

// Extra options for line break iterator.
struct LineBreakOptions final {
  WordBreakRule mWordBreakRule = WordBreakRule::Normal;
  LineBreakRule mLineBreakRule = LineBreakRule::Auto;
  bool mScriptIsChineseOrJapanese = false;
};

/**
 * Line break iterator for UTF-16 text.
 */
class LineBreakIteratorUtf16 final : public SegmentIteratorUtf16 {
 public:
  explicit LineBreakIteratorUtf16(Span<const char16_t> aText,
                                  const LineBreakOptions& aOptions = {});
  ~LineBreakIteratorUtf16() override;

  Maybe<uint32_t> Next() override;
  Maybe<uint32_t> Seek(uint32_t aPos) override;

 private:
  LineBreakOptions mOptions;

#ifdef MOZ_ICU4X
  capi::ICU4XLineSegmenter* mSegmenter = nullptr;
  capi::ICU4XLineBreakIteratorUtf16* mIterator = nullptr;
#endif
};

/**
 * Word break iterator for UTF-16 text.
 */
class WordBreakIteratorUtf16 final : public SegmentIteratorUtf16 {
 public:
  explicit WordBreakIteratorUtf16(Span<const char16_t> aText);
  ~WordBreakIteratorUtf16() override;

  Maybe<uint32_t> Next() override;
  Maybe<uint32_t> Seek(uint32_t aPos) override;

#if defined(MOZ_ICU4X) && defined(JS_HAS_INTL_API)
 private:
  capi::ICU4XWordSegmenter* mSegmenter = nullptr;
  capi::ICU4XWordBreakIteratorUtf16* mIterator = nullptr;
#endif
};

/**
 * Grapheme cluster break iterator for UTF-16 text.
 */
class GraphemeClusterBreakIteratorUtf16 final : public SegmentIteratorUtf16 {
 public:
  explicit GraphemeClusterBreakIteratorUtf16(Span<const char16_t> aText);
  ~GraphemeClusterBreakIteratorUtf16() override;

  Maybe<uint32_t> Next() override;
  Maybe<uint32_t> Seek(uint32_t aPos) override;

#if defined(MOZ_ICU4X) && defined(JS_HAS_INTL_API)
 private:
  static capi::ICU4XGraphemeClusterSegmenter* sSegmenter;
  capi::ICU4XGraphemeClusterBreakIteratorUtf16* mIterator = nullptr;
#endif
};

/**
 * Grapheme cluster break reverse iterator for UTF-16 text.
 *
 * Note: The reverse iterator doesn't handle conjoining Jamo and emoji. Use it
 * at your own risk.
 */
class GraphemeClusterBreakReverseIteratorUtf16 final
    : public SegmentIteratorUtf16 {
 public:
  explicit GraphemeClusterBreakReverseIteratorUtf16(Span<const char16_t> aText);

  Maybe<uint32_t> Next() override;
  Maybe<uint32_t> Seek(uint32_t aPos) override;
};

#if defined(MOZ_ICU4X) && defined(JS_HAS_INTL_API)
/**
 * Sentence break iterator for UTF-16 text.
 */
class SentenceBreakIteratorUtf16 final : public SegmentIteratorUtf16 {
 public:
  explicit SentenceBreakIteratorUtf16(Span<const char16_t> aText);
  ~SentenceBreakIteratorUtf16() override;

  Maybe<uint32_t> Next() override;
  Maybe<uint32_t> Seek(uint32_t aPos) override;

 private:
  capi::ICU4XSentenceSegmenter* mSegmenter = nullptr;
  capi::ICU4XSentenceBreakIteratorUtf16* mIterator = nullptr;
};
#endif

/**
 * This component is a Mozilla-focused API for working with segmenters in
 * internationalization code.
 *
 * This is a factor class. Calling Segment() to create an iterator over a text
 * of given granularity.
 */
class Segmenter final {
 public:
  // NOTE: aLocale is a no-op currently.
  static Result<UniquePtr<Segmenter>, ICUError> TryCreate(
      Span<const char> aLocale, const SegmenterOptions& aOptions);

  explicit Segmenter(Span<const char> aLocale, const SegmenterOptions& aOptions)
      : mOptions(aOptions) {}

  // Creates an iterator over aText of a given granularity in mOptions.
  UniquePtr<SegmentIteratorUtf16> Segment(Span<const char16_t> aText) const;

  // TODO: Implement an iterator for Latin1 text.
  // UniquePtr<SegmentIteratorLatin1> Segment(Span<const uint8_t> aText) const;

 private:
  SegmenterOptions mOptions;
};

}  // namespace mozilla::intl

#endif
