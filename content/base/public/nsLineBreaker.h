/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSLINEBREAKER_H_
#define NSLINEBREAKER_H_

#include "nsString.h"
#include "nsTArray.h"
#include "nsILineBreaker.h"

class nsIAtom;
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
   * @param aBreakBefore the break-before states for the characters in the substring.
   * These are enum values from gfxTextRun::CompressedGlyph:
   *    FLAG_BREAK_TYPE_NONE     - no linebreak is allowed here
   *    FLAG_BREAK_TYPE_NORMAL   - a normal (whitespace) linebreak
   *    FLAG_BREAK_TYPE_HYPHEN   - a hyphenation point
   */
  virtual void SetBreaks(uint32_t aStart, uint32_t aLength, uint8_t* aBreakBefore) = 0;
  
  /**
   * Indicates which characters should be capitalized. Only called if
   * BREAK_NEED_CAPITALIZATION was requested.
   */
  virtual void SetCapitalization(uint32_t aStart, uint32_t aLength, bool* aCapitalize) = 0;
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
 * whitespace-delimited "words". Then those words are passed to the nsILineBreaker
 * service for deeper analysis if they contain a "complex" character as described
 * below.
 * 
 * This class also handles detection of which characters should be capitalized
 * for text-transform:capitalize. This is a good place to handle that because
 * we have all the context we need.
 */
class nsLineBreaker {
public:
  nsLineBreaker();
  ~nsLineBreaker();
  
  static inline bool IsSpace(PRUnichar u) { return NS_IsSpace(u); }

  static inline bool IsComplexASCIIChar(PRUnichar u)
  {
    return !((0x0030 <= u && u <= 0x0039) ||
             (0x0041 <= u && u <= 0x005A) ||
             (0x0061 <= u && u <= 0x007A) ||
             (0x000a == u));
  }

  static inline bool IsComplexChar(PRUnichar u)
  {
    return IsComplexASCIIChar(u) ||
           NS_NeedsPlatformNativeHandling(u) ||
           (0x1100 <= u && u <= 0x11ff) || // Hangul Jamo
           (0x2000 <= u && u <= 0x21ff) || // Punctuations and Symbols
           (0x2e80 <= u && u <= 0xd7ff) || // several CJK blocks
           (0xf900 <= u && u <= 0xfaff) || // CJK Compatibility Idographs
           (0xff00 <= u && u <= 0xffef);   // Halfwidth and Fullwidth Forms
  }

  // Break opportunities exist at the end of each run of breakable whitespace
  // (see IsSpace above). Break opportunities can also exist between pairs of
  // non-whitespace characters, as determined by nsILineBreaker. We pass a whitespace-
  // delimited word to nsILineBreaker if it contains at least one character
  // matching IsComplexChar.
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
     * Do not introduce a break opportunity in the interior of this chunk of text.
     * Also, whitespace in this chunk is treated as non-breakable.
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
  nsresult AppendText(nsIAtom* aHyphenationLanguage, const PRUnichar* aText, uint32_t aLength,
                      uint32_t aFlags, nsILineBreakSink* aSink);
  /**
   * Feed 8-bit text into the linebreaker for analysis. aLength must be nonzero.
   * @param aSink can be null if the breaks are not actually needed (we may
   * still be setting up state for later breaks)
   */
  nsresult AppendText(nsIAtom* aHyphenationLanguage, const uint8_t* aText, uint32_t aLength,
                      uint32_t aFlags, nsILineBreakSink* aSink);
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
   * Set word-break mode for linebreaker.  This is set by word-break property.
   * @param aMode is nsILineBreaker::kWordBreak_* value.
   */
  void SetWordBreak(uint8_t aMode) { mWordBreak = aMode; }

private:
  // This is a list of text sources that make up the "current word" (i.e.,
  // run of text which does not contain any whitespace). All the mLengths
  // are are nonzero, these cannot overlap.
  struct TextItem {
    TextItem(nsILineBreakSink* aSink, uint32_t aSinkOffset, uint32_t aLength,
             uint32_t aFlags)
      : mSink(aSink), mSinkOffset(aSinkOffset), mLength(aLength), mFlags(aFlags) {}

    nsILineBreakSink* mSink;
    uint32_t          mSinkOffset;
    uint32_t          mLength;
    uint32_t          mFlags;
  };

  // State for the nonwhitespace "word" that started in previous text and hasn't
  // finished yet.

  // When the current word ends, this computes the linebreak opportunities
  // *inside* the word (excluding either end) and sets them through the
  // appropriate sink(s). Then we clear the current word state.
  nsresult FlushCurrentWord();

  void UpdateCurrentWordLanguage(nsIAtom *aHyphenationLanguage);

  void FindHyphenationPoints(nsHyphenator *aHyphenator,
                             const PRUnichar *aTextStart,
                             const PRUnichar *aTextLimit,
                             uint8_t *aBreakState);

  nsAutoTArray<PRUnichar,100> mCurrentWord;
  // All the items that contribute to mCurrentWord
  nsAutoTArray<TextItem,2>    mTextItems;
  nsIAtom*                    mCurrentWordLanguage;
  bool                        mCurrentWordContainsMixedLang;
  bool                        mCurrentWordContainsComplexChar;

  // True if the previous character was breakable whitespace
  bool                        mAfterBreakableSpace;
  // True if a break must be allowed at the current position because
  // a run of breakable whitespace ends here
  bool                        mBreakHere;
  // line break mode by "word-break" style
  uint8_t                     mWordBreak;
};

#endif /*NSLINEBREAKER_H_*/
