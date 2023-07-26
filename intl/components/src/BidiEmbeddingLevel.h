/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef intl_components_BidiEmbeddingLevel_h_
#define intl_components_BidiEmbeddingLevel_h_

#include <cstdint>

/**
 * This file has the BidiEmbeddingLevel and BidiDirection enum broken out from
 * the main Bidi class for faster includes. This code is used in Layout which
 * could trigger long build times when changing core mozilla::intl files.
 */
namespace mozilla::intl {

/**
 * This enum unambiguously classifies text runs as either being left to right,
 * or right to left.
 */
enum class BidiDirection : uint8_t {
  // Left to right text.
  LTR = 0,
  // Right to left text.
  RTL = 1,
};

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
class BidiEmbeddingLevel {
 public:
  constexpr explicit BidiEmbeddingLevel(uint8_t aValue) : mValue(aValue) {}
  constexpr explicit BidiEmbeddingLevel(int aValue)
      : mValue(static_cast<uint8_t>(aValue)) {}

  BidiEmbeddingLevel() = default;

  // Enable the copy operators, but disable move as this is only a uint8_t.
  BidiEmbeddingLevel(const BidiEmbeddingLevel& other) = default;
  BidiEmbeddingLevel& operator=(const BidiEmbeddingLevel& other) = default;

  /**
   * Determine the direction of the embedding level by looking at the least
   * significant bit. If it is 0, then it is LTR. If it is 1, then it is RTL.
   */
  BidiDirection Direction();

  /**
   * Create a left-to-right embedding level.
   */
  static BidiEmbeddingLevel LTR();

  /**
   * Create an right-to-left embedding level.
   */
  static BidiEmbeddingLevel RTL();

  /**
   * When passed into `SetParagraph`, the direction is determined by first
   * strongly directional character, with the default set to left-to-right if
   * none is found.
   *
   * This is encoded with the highest bit set to 1.
   */
  static BidiEmbeddingLevel DefaultLTR();

  /**
   * When passed into `SetParagraph`, the direction is determined by first
   * strongly directional character, with the default set to right-to-left if
   * none is found.
   *
   * * This is encoded with the highest and lowest bits set to 1.
   */
  static BidiEmbeddingLevel DefaultRTL();

  bool IsDefaultLTR() const;
  bool IsDefaultRTL() const;
  bool IsLTR() const;
  bool IsRTL() const;
  bool IsSameDirection(BidiEmbeddingLevel aOther) const;

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

}  // namespace mozilla::intl
#endif
