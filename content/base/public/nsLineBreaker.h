/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Novell code.
 *
 * The Initial Developer of the Original Code is Novell.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *         Robert O'Callahan <robert@ocallahan.org> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef NSLINEBREAKER_H_
#define NSLINEBREAKER_H_

#include "nsString.h"
#include "nsTArray.h"
#include "nsILineBreaker.h"

class nsIAtom;

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
   */
  virtual void SetBreaks(PRUint32 aStart, PRUint32 aLength, PRPackedBool* aBreakBefore) = 0;
  
  /**
   * Indicates which characters should be capitalized. Only called if
   * BREAK_NEED_CAPITALIZATION was requested.
   */
  virtual void SetCapitalization(PRUint32 aStart, PRUint32 aLength, PRPackedBool* aCapitalize) = 0;
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
  
  static inline PRBool IsSpace(PRUnichar u) { return NS_IsSpace(u); }

  static inline PRBool IsComplexASCIIChar(PRUnichar u)
  {
    return !((0x0030 <= u && u <= 0x0039) ||
             (0x0041 <= u && u <= 0x005A) ||
             (0x0061 <= u && u <= 0x007A));
  }

  static inline PRBool IsComplexChar(PRUnichar u)
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
    BREAK_NEED_CAPITALIZATION = 0x08
  };

  /**
   * Append "invisible whitespace". This acts like whitespace, but there is
   * no actual text associated with it. Only the BREAK_SUPPRESS_INSIDE flag
   * is relevant here.
   */
  nsresult AppendInvisibleWhitespace(PRUint32 aFlags);

  /**
   * Feed Unicode text into the linebreaker for analysis. aLength must be
   * nonzero.
   * @param aSink can be null if the breaks are not actually needed (we may
   * still be setting up state for later breaks)
   */
  nsresult AppendText(nsIAtom* aLangGroup, const PRUnichar* aText, PRUint32 aLength,
                      PRUint32 aFlags, nsILineBreakSink* aSink);
  /**
   * Feed 8-bit text into the linebreaker for analysis. aLength must be nonzero.
   * @param aSink can be null if the breaks are not actually needed (we may
   * still be setting up state for later breaks)
   */
  nsresult AppendText(nsIAtom* aLangGroup, const PRUint8* aText, PRUint32 aLength,
                      PRUint32 aFlags, nsILineBreakSink* aSink);
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
  nsresult Reset(PRBool* aTrailingBreak);

private:
  // This is a list of text sources that make up the "current word" (i.e.,
  // run of text which does not contain any whitespace). All the mLengths
  // are are nonzero, these cannot overlap.
  struct TextItem {
    TextItem(nsILineBreakSink* aSink, PRUint32 aSinkOffset, PRUint32 aLength,
             PRUint32 aFlags)
      : mSink(aSink), mSinkOffset(aSinkOffset), mLength(aLength), mFlags(aFlags) {}

    nsILineBreakSink* mSink;
    PRUint32          mSinkOffset;
    PRUint32          mLength;
    PRUint32          mFlags;
  };

  // State for the nonwhitespace "word" that started in previous text and hasn't
  // finished yet.

  // When the current word ends, this computes the linebreak opportunities
  // *inside* the word (excluding either end) and sets them through the
  // appropriate sink(s). Then we clear the current word state.
  nsresult FlushCurrentWord();

  nsAutoTArray<PRUnichar,100> mCurrentWord;
  // All the items that contribute to mCurrentWord
  nsAutoTArray<TextItem,2>    mTextItems;
  PRPackedBool                mCurrentWordContainsComplexChar;

  // True if the previous character was breakable whitespace
  PRPackedBool                mAfterBreakableSpace;
  // True if a break must be allowed at the current position because
  // a run of breakable whitespace ends here
  PRPackedBool                mBreakHere;
};

#endif /*NSLINEBREAKER_H_*/
