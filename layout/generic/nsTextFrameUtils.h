/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Initial Developer of the Original Code is Novell Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   robert@ocallahan.org
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

#ifndef NSTEXTFRAMEUTILS_H_
#define NSTEXTFRAMEUTILS_H_

#include "gfxFont.h"
#include "gfxSkipChars.h"
#include "nsTextFragment.h"

#define BIG_TEXT_NODE_SIZE 4096

class nsTextFrameUtils {
public:
  // These constants are used as textrun flags for textframe textruns.
  enum {
    // The following flags are set by TransformText

    // the text has at least one untransformed tab character
    TEXT_HAS_TAB             = 0x010000,
    // the original text has at least one soft hyphen character
    TEXT_HAS_SHY             = 0x020000,
    TEXT_HAS_NON_ASCII       = 0x040000,
    TEXT_WAS_TRANSFORMED     = 0x080000,

    // The following flags are set by nsTextFrame

    TEXT_IS_SIMPLE_FLOW      = 0x100000,
    TEXT_INCOMING_WHITESPACE = 0x200000
  };

  static PRBool
  IsPunctuationMark(PRUnichar aChar);

  /**
   * Returns PR_TRUE if aChars/aLength are something that make a space
   * character not be whitespace when they follow the space character.
   * For now, this is true if and only if aChars starts with a ZWJ. (This
   * is what Uniscribe assumes.)
   */
  static PRBool
  IsSpaceCombiningSequenceTail(const PRUnichar* aChars, PRInt32 aLength) {
    return aLength > 0 && aChars[0] == 0x200D; // ZWJ
  }

  /**
   * Create a text run from a run of Unicode text. The text may have whitespace
   * compressed. A preformatted tab is sent to the text run as a single space.
   * (Tab spacing must be performed by textframe later.) Certain other
   * characters are discarded.
   * 
   * @param aCompressWhitespace runs of consecutive whitespace (spaces not
   * followed by a diacritical mark, tabs, and newlines) are compressed to a
   * single space character.
   */
  static PRUnichar* TransformText(const PRUnichar* aText, PRUint32 aLength,
                                  PRUnichar* aOutput,
                                  PRBool aCompressWhitespace,
                                  PRPackedBool* aIncomingWhitespace,
                                  gfxSkipCharsBuilder* aSkipChars,
                                  PRUint32* aAnalysisFlags);

  static PRUint8* TransformText(const PRUint8* aText, PRUint32 aLength,
                                PRUint8* aOutput,
                                PRBool aCompressWhitespace,
                                PRPackedBool* aIncomingWhitespace,
                                gfxSkipCharsBuilder* aSkipChars,
                                PRUint32* aAnalysisFlags);

  /**
   * Find a word boundary starting from a given position and proceeding either
   * forwards (aDirection == 1) or backwards (aDirection == -1). The search
   * is limited to a substring of an nsTextFragment, and starts with the
   * first character of the word (the character at aOffset). We return the length
   * of the word.
   * 
   * @param aTextRun a text run which we will use to ensure that we don't
   * return a boundary inside a cluster
   * @param aPosition a character in the substring aOffset/aLength
   * @param aBreakBeforePunctuation if true, then we allow a word break
   * when transitioning from regular word text to punctuation (in content order)
   * @param aBreakAfterPunctuation if true, then we allow a word break
   * when transitioning from punctuation to regular word text (in content order)
   * @param aWordIsWhitespace we set this to true if this word is whitespace
   * 
   * For the above properties, "punctuation" is defined as any ASCII character
   * which is not a letter or a digit. Regular word text is any non-whitespace
   * (here "whitespace" includes non-breaking whitespace).
   * Word break points are the punctuation breaks defined above, plus
   * for Unicode text, whatever intl's wordbreaker identifies, and for
   * ASCII text, boundaries between whitespace and non-whitespace.
   */
  static PRInt32
  FindWordBoundary(const nsTextFragment* aText,
                   gfxTextRun* aTextRun,
                   gfxSkipCharsIterator* aIterator,
                   PRInt32 aOffset, PRInt32 aLength,
                   PRInt32 aPosition, PRInt32 aDirection,
                   PRBool aBreakBeforePunctuation,
                   PRBool aBreakAfterPunctuation,
                   PRBool* aWordIsWhitespace);                
};

class nsSkipCharsRunIterator {
public:
  enum LengthMode {
    LENGTH_UNSKIPPED_ONLY   = PR_FALSE,
    LENGTH_INCLUDES_SKIPPED = PR_TRUE
  };
  nsSkipCharsRunIterator(const gfxSkipCharsIterator& aStart,
      LengthMode aLengthIncludesSkipped, PRUint32 aLength)
    : mIterator(aStart), mRemainingLength(aLength), mRunLength(0),
      mVisitSkipped(PR_FALSE),
      mLengthIncludesSkipped(aLengthIncludesSkipped) {
  }
  void SetVisitSkipped() { mVisitSkipped = PR_TRUE; }
  void SetOriginalOffset(PRInt32 aOffset) {
    mIterator.SetOriginalOffset(aOffset);
  }
  void SetSkippedOffset(PRUint32 aOffset) {
    mIterator.SetSkippedOffset(aOffset);
  }

  // guaranteed to return only positive-length runs
  PRBool NextRun();
  PRBool IsSkipped() const { return mSkipped; }
  // Always returns something > 0
  PRInt32 GetRunLength() const { return mRunLength; }
  const gfxSkipCharsIterator& GetPos() const { return mIterator; }
  PRInt32 GetOriginalOffset() const { return mIterator.GetOriginalOffset(); }
  PRUint32 GetSkippedOffset() const { return mIterator.GetSkippedOffset(); }

private:
  gfxSkipCharsIterator mIterator;
  PRInt32              mRemainingLength;
  PRInt32              mRunLength;
  PRPackedBool         mSkipped;
  PRPackedBool         mVisitSkipped;
  PRPackedBool         mLengthIncludesSkipped;
};

#endif /*NSTEXTFRAMEUTILS_H_*/
