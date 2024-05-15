/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef intl_components_Bidi_h_
#define intl_components_Bidi_h_

#include "mozilla/intl/BidiEmbeddingLevel.h"
#include "mozilla/intl/ICU4CGlue.h"

// Use the Rust unicode-bidi crate to back the Bidi component.
// (Define to 0 to use the legacy ICU4C implementation instead,
// until that code is removed altogether.)
#define USE_RUST_UNICODE_BIDI 1

#if USE_RUST_UNICODE_BIDI
#  include "mozilla/intl/unicode_bidi_ffi_generated.h"
#else
struct UBiDi;
#endif

namespace mozilla::intl {

/**
 * This component is a Mozilla-focused API for working with bidirectional (bidi)
 * text. Text is commonly displayed left to right (LTR), especially for
 * Latin-based alphabets. However, languages like Arabic and Hebrew displays
 * text right to left (RTL). When displaying text, LTR and RTL text can be
 * combined together in the same paragraph. This class gives tools for working
 * with unidirectional, and mixed direction paragraphs.
 *
 * See the Unicode Bidirectional Algorithm document for implementation details:
 * https://unicode.org/reports/tr9/
 */
class Bidi final {
 public:
  Bidi();
  ~Bidi();

  // Not copyable or movable
  Bidi(const Bidi&) = delete;
  Bidi& operator=(const Bidi&) = delete;

  /**
   * This enum indicates the text direction for the set paragraph. Some
   * paragraphs are unidirectional, where they only have one direction, or a
   * paragraph could use both LTR and RTL. In this case the paragraph's
   * direction would be mixed.
   */
  enum class ParagraphDirection { LTR, RTL, Mixed };

  /**
   * Set the current paragraph of text to analyze for its bidi properties. This
   * performs the Unicode bidi algorithm as specified by:
   * https://unicode.org/reports/tr9/
   *
   * After setting the text, the other getter methods can be used to find out
   * the directionality of the paragraph text.
   */
  ICUResult SetParagraph(Span<const char16_t> aParagraph,
                         BidiEmbeddingLevel aLevel);

  /**
   * Get the embedding level for the paragraph that was set by SetParagraph.
   */
  BidiEmbeddingLevel GetParagraphEmbeddingLevel() const;

  /**
   * Get the directionality of the paragraph text that was set by SetParagraph.
   */
  ParagraphDirection GetParagraphDirection() const;

  /**
   * Get the number of runs. This function may invoke the actual reordering on
   * the Bidi object, after SetParagraph may have resolved only the levels of
   * the text. Therefore, `CountRuns` may have to allocate memory, and may fail
   * doing so.
   */
  Result<int32_t, ICUError> CountRuns();

  /**
   * Get the next logical run. The logical runs are a run of text that has the
   * same directionality and embedding level. These runs are in memory order,
   * and not in display order.
   *
   * Important! `Bidi::CountRuns` must be called before calling this method.
   *
   * @param aLogicalStart is the offset into the paragraph text that marks the
   *      logical start of the text.
   * @param aLogicalLimitOut is an out param that is the length of the string
   *      that makes up the logical run.
   * @param aLevelOut is an out parameter that returns the embedding level for
   *      the run
   */
  void GetLogicalRun(int32_t aLogicalStart, int32_t* aLogicalLimitOut,
                     BidiEmbeddingLevel* aLevelOut);

  /**
   * This is a convenience function that does not use the ICU Bidi object.
   * It is intended to be used for when an application has determined the
   * embedding levels of objects (character sequences) and just needs to have
   * them reordered (L2).
   *
   * @param aLevels is an array with `aLength` levels that have been
   *      determined by the application.
   *
   * @param aLength is the number of levels in the array, or, semantically,
   *      the number of objects to be reordered. It must be greater than 0.
   *
   * @param aIndexMap is a pointer to an array of `aLength`
   *      indexes which will reflect the reordering of the characters.
   *      The array does not need to be initialized.
   *      The index map will result in
   *        `aIndexMap[aVisualIndex]==aLogicalIndex`.
   */
  static void ReorderVisual(const BidiEmbeddingLevel* aLevels, int32_t aLength,
                            int32_t* aIndexMap);

  /**
   * This enum indicates the bidi character type of the first strong character
   * for the set paragraph.
   * LTR: bidi character type 'L'.
   * RTL: bidi character type 'R' or 'AL'.
   * Neutral: The rest of bidi character types.
   */
  enum class BaseDirection { LTR, RTL, Neutral };

  /**
   * Get the base direction of the text.
   */
  static BaseDirection GetBaseDirection(Span<const char16_t> aText);

  /**
   * Get one run's logical start, length, and directionality. In an RTL run, the
   * character at the logical start is visually on the right of the displayed
   * run. The length is the number of characters in the run.
   * `Bidi::CountRuns` should be called before the runs are retrieved.
   *
   * @param aRunIndex is the number of the run in visual order, in the
   *      range `[0..CountRuns-1]`.
   *
   * @param aLogicalStart is the first logical character index in the text.
   *      The pointer may be `nullptr` if this index is not needed.
   *
   * @param aLength is the number of characters (at least one) in the run.
   *      The pointer may be `nullptr` if this is not needed.
   *
   * Note that in right-to-left runs, the code places modifier letters before
   * base characters and second surrogates before first ones.
   */
  BidiDirection GetVisualRun(int32_t aRunIndex, int32_t* aLogicalStart,
                             int32_t* aLength);

 private:
#if USE_RUST_UNICODE_BIDI
  using UnicodeBidi = mozilla::intl::ffi::UnicodeBidi;
  struct BidiFreePolicy {
    void operator()(void* aPtr) {
      bidi_destroy(static_cast<UnicodeBidi*>(aPtr));
    }
  };
  mozilla::UniquePtr<UnicodeBidi, BidiFreePolicy> mBidi;
#else
  ICUPointer<UBiDi> mBidi = ICUPointer<UBiDi>(nullptr);

  /**
   * An array of levels that is the same length as the paragraph from
   * `Bidi::SetParagraph`.
   */
  const BidiEmbeddingLevel* mLevels = nullptr;

  /**
   * The length of the paragraph from `Bidi::SetParagraph`.
   */
  int32_t mLength = 0;
#endif
};

}  // namespace mozilla::intl
#endif
