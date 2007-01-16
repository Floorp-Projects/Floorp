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
 * whitespace-delimited "words". Then for words that contain a CJK character,
 * we break within the word using JISx4051 rules.
 * XXX This approach is not very good and we should replace it with something
 * better, such as some variant of UAX#14.
 */
class nsLineBreaker {
public:
  nsLineBreaker();
  ~nsLineBreaker();

  // We need finegrained control of the line breaking behaviour to ensure
  // that we get tricky CSS semantics right (in particular, the way we currently
  // interpret and implement them; there's some ambiguity in the spec). The
  // rules for CSS 'white-space' are slightly different for breaks induced by
  // whitespace and space induced by nonwhitespace. Breaks induced by
  // whitespace are always controlled by the
  // 'white-space' property of the text node containing the
  // whitespace. Breaks induced by non-whitespace where the break is between
  // two nodes are controled by the 'white-space' property on the nearest
  // common ancestor node. Therefore we provide separate control over
  // a) whether whitespace in this text induces breaks b) whether we can
  // break between nonwhitespace inside this text and c) whether we can break
  // between nonwhitespace between the last text and this text.
  enum {
    /**
     * Allow breaks before and after whitespace in this block of text
     */
    BREAK_WHITESPACE           = 0x01,
    /**
     * Allow breaks between eligible nonwhitespace characters when the break
     * is in the interior of this block of text.
     */
    BREAK_NONWHITESPACE_INSIDE = 0x02,
    /**
     * Allow break between eligible nonwhitespace characters when the break
     * is at the beginning of this block of text.
     */
    BREAK_NONWHITESPACE_BEFORE = 0x04
  };

  /**
   * Feed Unicode text into the linebreaker for analysis.
   * If aLength is zero, then we assume the string is "invisible whitespace"
   * which can induce breaks.
   */
  nsresult AppendText(nsIAtom* aLangGroup, const PRUnichar* aText, PRUint32 aLength,
                      PRUint32 aFlags, nsILineBreakSink* aSink);
  /**
   * Feed 8-bit text into the linebreaker for analysis.
   * If aLength is zero, then we assume the string is "invisible whitespace"
   * which can induce breaks.
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
   */
  nsresult Reset() { return FlushCurrentWord(); }

private:
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
  PRPackedBool                mCurrentWordContainsCJK;

  // When mCurrentWord is empty, this indicates whether we should allow a break
  // before the next text.
  PRPackedBool             mBreakBeforeNextWord;
};

#endif /*NSLINEBREAKER_H_*/
