/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef intl_components_Bidi_h_
#define intl_components_Bidi_h_

#include "mozilla/intl/ICU4CGlue.h"

struct UBiDi;

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
   * This enum unambiguously classifies text runs as either being left to right,
   * or right to left.
   */
  enum class Direction : uint8_t {
    // Left to right text.
    LTR = 0,
    // Right to left text.
    RTL = 1,
  };

  /**
   * This enum indicates the text direction for the set paragraph. Some
   * paragraphs are unidirectional, where they only have one direction, or a
   * paragraph could use both LTR and RTL. In this case the paragraph's
   * direction would be mixed.
   */
  enum ParagraphDirection { LTR, RTL, Mixed };

  /**
   * Embedding levels are numbers that indicate how deeply the bidi text is
   * embedded, and the direction of text on that embedding level. When switching
   * between strongly LTR code points and strongly RTL code points the embedding
   * level normally switches between an embedding level of 0 (LTR) and 1 (RTL).
   * The only time the embedding level increases is if the embedding code points
   * are used. This is the Left-to-Right Embedding (LRE) code point (U+202A), or
   * the Right-to-Left Embedding (RLE) code point (U+202B). The minimum
   * embedding level of text is zero, and the maximum explicit depth is 125.
   *
   * The most significant bit is reserved for additional meaning. It can be used
   * to signify in certain APIs that the text should by default be LTR or RTL if
   * no strongly directional code points are found.
   *
   * Bug 1736595: At the time of this writing, some places in Gecko code use a 1
   * in the most significant bit to indicate that an embedding level has not
   * been set. This leads to an ambiguous understanding of what the most
   * significant bit actually means.
   */
  class EmbeddingLevel {
   public:
    explicit EmbeddingLevel(uint8_t aValue) : mValue(aValue) {}
    explicit EmbeddingLevel(int aValue)
        : mValue(static_cast<uint8_t>(aValue)) {}

    EmbeddingLevel() = default;

    // Enable the copy operators, but disable move as this is only a uint8_t.
    EmbeddingLevel(const EmbeddingLevel& other) = default;
    EmbeddingLevel& operator=(const EmbeddingLevel& other) = default;

    /**
     * Determine the direction of the embedding level by looking at the least
     * significant bit. If it is 0, then it is LTR. If it is 1, then it is RTL.
     */
    Bidi::Direction Direction();

    /**
     * Create a left-to-right embedding level.
     */
    static EmbeddingLevel LTR();

    /**
     * Create an right-to-left embedding level.
     */
    static EmbeddingLevel RTL();

    /**
     * When passed into `SetParagraph`, the direction is determined by first
     * strongly directional character, with the default set to left-to-right if
     * none is found.
     *
     * This is encoded with the highest bit set to 1.
     */
    static EmbeddingLevel DefaultLTR();

    /**
     * When passed into `SetParagraph`, the direction is determined by first
     * strongly directional character, with the default set to right-to-left if
     * none is found.
     *
     * * This is encoded with the highest and lowest bits set to 1.
     */
    static EmbeddingLevel DefaultRTL();

    bool IsDefaultLTR() const;
    bool IsDefaultRTL() const;
    bool IsLTR() const;
    bool IsRTL() const;
    bool IsSameDirection(EmbeddingLevel aOther) const;

    /**
     * Get the underlying value as a uint8_t.
     */
    uint8_t Value() const;

    /**
     * Implicitly convert to the underlying value.
     */
    operator uint8_t() const { return mValue; }

   private:
    uint8_t mValue = 0;
  };

  /**
   * Set the current paragraph of text to analyze for its bidi properties. This
   * performs the Unicode bidi algorithm as specified by:
   * https://unicode.org/reports/tr9/
   *
   * After setting the text, the other getter methods can be used to find out
   * the directionality of the paragraph text.
   */
  ICUResult SetParagraph(Span<const char16_t> aParagraph,
                         EmbeddingLevel aLevel);

  /**
   * Get the embedding level for the paragraph that was set by SetParagraph.
   */
  EmbeddingLevel GetParagraphEmbeddingLevel() const;

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
                     EmbeddingLevel* aLevelOut);

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
  static void ReorderVisual(const EmbeddingLevel* aLevels, int32_t aLength,
                            int32_t* aIndexMap);

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
  Direction GetVisualRun(int32_t aRunIndex, int32_t* aLogicalStart,
                         int32_t* aLength);

 private:
  ICUPointer<UBiDi> mBidi = ICUPointer<UBiDi>(nullptr);

  /**
   * An array of levels that is the same length as the paragraph from
   * `Bidi::SetParagraph`.
   */
  const EmbeddingLevel* mLevels = nullptr;

  /**
   * The length of the paragraph from `Bidi::SetParagraph`.
   */
  int32_t mLength = 0;
};

}  // namespace mozilla::intl
#endif
