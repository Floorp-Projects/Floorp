// Copyright 2018 Mozilla Foundation. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Mostly copied and pasted from
// third_party/rust/shift_or_euc/src/lib.rs , so
// "top-level directory of this distribution" above refers to
// third_party/rust/shift_or_euc/

#ifndef mozilla_JapaneseDetector_h
#define mozilla_JapaneseDetector_h

#include "mozilla/Encoding.h"

namespace mozilla {
class JapaneseDetector;
};  // namespace mozilla

#define SHIFT_OR_EUC_DETECTOR mozilla::JapaneseDetector

#include "shift_or_euc.h"

namespace mozilla {

/**
 * A Japanese legacy encoding detector for detecting between Shift_JIS,
 * EUC-JP, and, optionally, ISO-2022-JP _given_ the assumption that the
 * encoding is one of those.
 *
 * # Principle of Operation
 *
 * The detector is based on two observations:
 *
 * 1. The ISO-2022-JP escape sequences don't normally occur in Shift_JIS or
 * EUC-JP, so encountering such an escape sequence (before non-ASCII has been
 * encountered) can be taken as indication of ISO-2022-JP.
 * 2. When normal (full-with) kana or common kanji encoded as Shift_JIS is
 * decoded as EUC-JP, or vice versa, the result is either an error or
 * half-width katakana, and it's very uncommon for Japanese HTML to have
 * half-width katakana character before a normal kana or common kanji
 * character. Therefore, if decoding as Shift_JIS results in error or
 * have-width katakana, the detector decides that the content is EUC-JP, and
 * vice versa.
 *
 * # Failure Modes
 *
 * The detector gives the wrong answer if the text has a half-width katakana
 * character before normal kana or common kanji. Some uncommon kanji are
 * undecidable. (All JIS X 0208 Level 1 kanji are decidable.)
 *
 * The half-width katakana issue is mainly relevant for old 8-bit JIS X
 * 0201-only text files that would decode correctly as Shift_JIS but that the
 * detector detects as EUC-JP.
 *
 * The undecidable kanji issue does not realistically show up when a full
 * document is fed to the detector, because, realistically, in a full
 * document, there is at least one kana or common kanji. It can occur,
 * though, if the detector is only run on a prefix of a document and the
 * prefix only contains the title of the document. It is possible for
 * document title to consist entirely of undecidable kanji. (Indeed,
 * Japanese Wikipedia has articles with such titles.) If the detector is
 * undecided, a fallback to Shift_JIS should be used.
 */
class JapaneseDetector final {
 public:
  ~JapaneseDetector() {}

  static void operator delete(void* aDetector) {
    shift_or_euc_detector_free(reinterpret_cast<JapaneseDetector*>(aDetector));
  }

  /**
   * Instantiates the detector. If `aAllow2022` is `true` the possible
   * guesses are Shift_JIS, EUC-JP, ISO-2022-JP, and undecided. If
   * `aAllow2022` is `false`, the possible guesses are Shift_JIS, EUC-JP,
   * and undecided.
   */
  static inline UniquePtr<JapaneseDetector> Create(bool aAllow2022) {
    UniquePtr<JapaneseDetector> detector(shift_or_euc_detector_new(aAllow2022));
    return detector;
  }

  /**
   * Feeds bytes to the detector. If `aLast` is `true` the end of the stream
   * is considered to occur immediately after the end of `aBuffer`.
   * Otherwise, the stream is expected to continue. `aBuffer` may be empty.
   *
   * If you're running the detector only on a prefix of a complete
   * document, _do not_ pass `aLast` as `true` after the prefix if the
   * stream as a whole still contains more content.
   *
   * Returns `SHIFT_JIS_ENCODING` if the detector guessed
   * Shift_JIS. Returns `EUC_JP_ENCODING` if the detector
   * guessed EUC-JP. Returns `ISO_2022_JP_ENCODING` if the
   * detector guessed ISO-2022-JP (only possible if `true` was passed as
   * `aAllow2022` when instantiating the detector). Returns `nullptr` if the
   * detector is undecided. If `nullptr` is returned even when passing `true`
   * as `aLast`, falling back to Shift_JIS is the best guess for Web
   * purposes.
   *
   * Do not call again after the method has returned non-`nullptr` or after
   * the method has been called with `true` as `aLast`. (Asserts if the
   * previous sentence isn't adhered to.)
   */
  inline const mozilla::Encoding* Feed(Span<const uint8_t> aBuffer,
                                       bool aLast) {
    return shift_or_euc_detector_feed(this, aBuffer.Elements(),
                                      aBuffer.Length(), aLast);
  }

 private:
  JapaneseDetector() = delete;
  JapaneseDetector(const JapaneseDetector&) = delete;
  JapaneseDetector& operator=(const JapaneseDetector&) = delete;
};

};  // namespace mozilla

#endif  // mozilla_JapaneseDetector_h