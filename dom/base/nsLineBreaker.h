/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSLINEBREAKER_H_
#define NSLINEBREAKER_H_

#include "mozilla/intl/LineBreaker.h"
#include "mozilla/intl/Segmenter.h"
#include "nsString.h"
#include "nsTArray.h"

class nsAtom;
class nsHyphenator;

/**
 * A receiver of line break data.
 */
class nsILineBreakSink {
 public:
  /**
   * Sets the break data for a substring of the associated text chunk.
   * One or more of these calls will be performed; the union of all substrings
   * will cover the entire text chunk. Substrings may overlap (i.e., we may
   * set the break-before state of a character more than once).
   * @param aBreakBefore the break-before states for the characters in the
   * substring. These are enum values from gfxTextRun::CompressedGlyph:
   *    FLAG_BREAK_TYPE_NONE     - no linebreak is allowed here
   *    FLAG_BREAK_TYPE_NORMAL   - a normal (whitespace) linebreak
   *    FLAG_BREAK_TYPE_HYPHEN   - a hyphenation point
   */
  virtual void SetBreaks(uint32_t aStart, uint32_t aLength,
                         uint8_t* aBreakBefore) = 0;

  /**
   * Indicates which characters should be capitalized. Only called if
   * BREAK_NEED_CAPITALIZATION was requested.
   */
  virtual void SetCapitalization(uint32_t aStart, uint32_t aLength,
                                 bool* aCapitalize) = 0;
};

/**
 * A line-breaking state machine. You feed text into it via AppendText calls
 * and it computes the possible line breaks. Because break decisions can
 * require a lot of context, the breaks for a piece of text are sometimes not
 * known until later text has been seen (or all text ends). So breaks are
 * returned via a call to SetBreaks on the nsILineBreakSink object passed
 * with each text chunk, which might happen during the corresponding AppendText
 * call, or might happen during a later AppendText call or even a Reset()
 * call.
 *
 * The linebreak results MUST NOT depend on how the text is broken up
 * into AppendText calls.
 *
 * The current strategy is that we break the overall text into
 * whitespace-delimited "words". Then those words are passed to the LineBreaker
 * for deeper analysis if they might contain breakable characters.
 *
 * This class also handles detection of which characters should be capitalized
 * for text-transform:capitalize. This is a good place to handle that because
 * we have all the context we need.
 */
class nsLineBreaker {
 public:
  nsLineBreaker();
  ~nsLineBreaker();

  static inline bool IsSpace(char16_t u) {
    return mozilla::intl::NS_IsSpace(u);
  }

  // Break opportunities exist at the end of each run of breakable whitespace
  // (see IsSpace above). Break opportunities can also exist between pairs of
  // non-whitespace characters, as determined by mozilla::intl::LineBreaker.
  // We pass a whitespace-
  // delimited word to LineBreaker if it contains at least one character
  // that has breakable line breaking classes.
  // We provide flags to control on a per-chunk basis where breaks are allowed.
  // At any character boundary, exactly one text chunk governs whether a
  // break is allowed at that boundary.
  //
  // We operate on text after whitespace processing has been applied, so
  // other characters (e.g. tabs and newlines) may have been converted to
  // spaces.

  /**
   * Flags passed with each chunk of text.
   */
  enum {
    /*
     * Do not introduce a break opportunity at the start of this chunk of text.
     */
    BREAK_SUPPRESS_INITIAL = 0x01,
    /**
     * Do not introduce a break opportunity in the interior of this chunk of
     * text. Also, whitespace in this chunk is treated as non-breakable.
     */
    BREAK_SUPPRESS_INSIDE = 0x02,
    /**
     * The sink currently is already set up to have no breaks in it;
     * if no breaks are possible, nsLineBreaker does not need to call
     * SetBreaks on it. This is useful when handling large quantities of
     * preformatted text; the textruns will never have any breaks set on them,
     * and there is no need to ever actually scan the text for breaks, except
     * at the end of textruns in case context is needed for following breakable
     * text.
     */
    BREAK_SKIP_SETTING_NO_BREAKS = 0x04,
    /**
     * We need to be notified of characters that should be capitalized
     * (as in text-transform:capitalize) in this chunk of text.
     */
    BREAK_NEED_CAPITALIZATION = 0x08,
    /**
     * Auto-hyphenation is enabled, so we need to get a hyphenator
     * (if available) and use it to find breakpoints.
     */
    BREAK_USE_AUTO_HYPHENATION = 0x10
  };

  /**
   * Append "invisible whitespace". This acts like whitespace, but there is
   * no actual text associated with it. Only the BREAK_SUPPRESS_INSIDE flag
   * is relevant here.
   */
  nsresult AppendInvisibleWhitespace(uint32_t aFlags);

  /**
   * Feed Unicode text into the linebreaker for analysis. aLength must be
   * nonzero.
   * @param aSink can be null if the breaks are not actually needed (we may
   * still be setting up state for later breaks)
   */
  nsresult AppendText(nsAtom* aHyphenationLanguage, const char16_t* aText,
                      uint32_t aLength, uint32_t aFlags,
                      nsILineBreakSink* aSink);
  /**
   * Feed 8-bit text into the linebreaker for analysis. aLength must be nonzero.
   * @param aSink can be null if the breaks are not actually needed (we may
   * still be setting up state for later breaks)
   */
  nsresult AppendText(nsAtom* aHyphenationLanguage, const uint8_t* aText,
                      uint32_t aLength, uint32_t aFlags,
                      nsILineBreakSink* aSink);
  /**
   * Reset all state. This means the current run has ended; any outstanding
   * calls through nsILineBreakSink are made, and all outstanding references to
   * nsILineBreakSink objects are dropped.
   * After this call, this linebreaker can be reused.
   * This must be called at least once between any call to AppendText() and
   * destroying the object.
   * @param aTrailingBreak this is set to true when there is a break opportunity
   * at the end of the text. This will normally only be declared true when there
   * is breakable whitespace at the end.
   */
  nsresult Reset(bool* aTrailingBreak);

  /*
   * Set word-break mode for line breaker. This is set by word-break property.
   */
  void SetWordBreak(mozilla::intl::WordBreakRule aMode) {
    // If current word is non-empty and mode is changing, flush the breaker.
    if (aMode != mWordBreak && !mCurrentWord.IsEmpty()) {
      nsresult rv = FlushCurrentWord();
      if (NS_FAILED(rv)) {
        NS_WARNING("FlushCurrentWord failed, line-breaks may be wrong");
      }
      // If previous mode was break-all, we should allow a break here.
      // XXX (jfkthame) css-text spec seems unclear on this, raised question in
      // https://github.com/w3c/csswg-drafts/issues/3897
      if (mWordBreak == mozilla::intl::WordBreakRule::BreakAll) {
        mBreakHere = true;
      }
    }
    mWordBreak = aMode;
  }

  /*
   * Set line-break rule strictness mode for line breaker. This is set by the
   * line-break property.
   */
  void SetStrictness(mozilla::intl::LineBreakRule aMode) {
    if (aMode != mLineBreak && !mCurrentWord.IsEmpty()) {
      nsresult rv = FlushCurrentWord();
      if (NS_FAILED(rv)) {
        NS_WARNING("FlushCurrentWord failed, line-breaks may be wrong");
      }
      // If previous mode was anywhere, we should allow a break here.
      if (mLineBreak == mozilla::intl::LineBreakRule::Anywhere) {
        mBreakHere = true;
      }
    }
    mLineBreak = aMode;
  }

  /**
   * Return whether the line-breaker has a buffered "current word" that may
   * be extended with additional word-forming characters.
   */
  bool InWord() const { return !mCurrentWord.IsEmpty(); }

  /**
   * Set the word-continuation state, which will suppress capitalization of
   * the next letter that might otherwise apply.
   */
  void SetWordContinuation(bool aContinuation) {
    mWordContinuation = aContinuation;
  }

 private:
  // This is a list of text sources that make up the "current word" (i.e.,
  // run of text which does not contain any whitespace). All the mLengths
  // are are nonzero, these cannot overlap.
  struct TextItem {
    TextItem(nsILineBreakSink* aSink, uint32_t aSinkOffset, uint32_t aLength,
             uint32_t aFlags)
        : mSink(aSink),
          mSinkOffset(aSinkOffset),
          mLength(aLength),
          mFlags(aFlags) {}

    nsILineBreakSink* mSink;
    uint32_t mSinkOffset;
    uint32_t mLength;
    uint32_t mFlags;
  };

  // State for the nonwhitespace "word" that started in previous text and hasn't
  // finished yet.

  // When the current word ends, this computes the linebreak opportunities
  // *inside* the word (excluding either end) and sets them through the
  // appropriate sink(s). Then we clear the current word state.
  nsresult FlushCurrentWord();

  void UpdateCurrentWordLanguage(nsAtom* aHyphenationLanguage);

  void FindHyphenationPoints(nsHyphenator* aHyphenator,
                             const char16_t* aTextStart,
                             const char16_t* aTextLimit, uint8_t* aBreakState);

  inline constexpr bool IsSegmentSpace(char16_t u) const {
    if (mLegacyBehavior) {
      return nsLineBreaker::IsSpace(u);
    }

    return u == 0x0020 ||  // SPACE u
           u == 0x0009 ||  // CHARACTER TABULATION
           u == 0x000D;    // CARRIAGE RETURN
  }

  AutoTArray<char16_t, 100> mCurrentWord;
  // All the items that contribute to mCurrentWord
  AutoTArray<TextItem, 2> mTextItems;
  nsAtom* mCurrentWordLanguage;
  bool mCurrentWordContainsMixedLang;
  bool mCurrentWordMightBeBreakable = false;
  bool mScriptIsChineseOrJapanese;

  // True if the previous character was breakable whitespace
  bool mAfterBreakableSpace;
  // True if a break must be allowed at the current position because
  // a run of breakable whitespace ends here
  bool mBreakHere;
  // Break rules for letters from the "word-break" property.
  mozilla::intl::WordBreakRule mWordBreak;
  // Line breaking strictness from the "line-break" property.
  mozilla::intl::LineBreakRule mLineBreak;
  // Should the text be treated as continuing a word-in-progress (for purposes
  // of initial capitalization)? Normally this is set to false whenever we
  // start using a linebreaker, but it may be set to true if the line-breaker
  // has been explicitly flushed mid-word.
  bool mWordContinuation;
  // True if using old line segmenter.
  const bool mLegacyBehavior;
};

#endif /*NSLINEBREAKER_H_*/
