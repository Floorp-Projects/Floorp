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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *         Robert O'Callahan <robert@ocallahan.org>
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

#include "nsLineBreaker.h"
#include "nsContentUtils.h"
#include "nsILineBreaker.h"

#define UNICODE_ZWSP 0x200b

static inline int
IS_SPACE(PRUnichar u)
{
  return u == 0x0020 || u == UNICODE_ZWSP;
}

static inline int
IS_SPACE(PRUint8 u)
{
  return u == 0x0020;
}

static inline int
IS_CJK_CHAR(PRUnichar u)
{
  return (0x1100 <= u && u <= 0x11ff) ||
         (0x2e80 <= u && u <= 0xd7ff) ||
         (0xf900 <= u && u <= 0xfaff) ||
         (0xff00 <= u && u <= 0xffef);
}

nsLineBreaker::nsLineBreaker()
  : mCurrentWordContainsCJK(PR_FALSE),
    mAfterSpace(PR_FALSE)
{
}

nsLineBreaker::~nsLineBreaker()
{
  NS_ASSERTION(mCurrentWord.Length() == 0, "Should have Reset() before destruction!");
}

nsresult
nsLineBreaker::FlushCurrentWord()
{
  nsAutoTArray<PRPackedBool,4000> breakState;
  if (!breakState.AppendElements(mCurrentWord.Length()))
    return NS_ERROR_OUT_OF_MEMORY;

  if (!mCurrentWordContainsCJK) {
    // Just set everything internal to "no break"!
    memset(breakState.Elements(), PR_FALSE, mCurrentWord.Length());
  } else {
    nsContentUtils::LineBreaker()->
      GetJISx4051Breaks(mCurrentWord.Elements(), mCurrentWord.Length(), breakState.Elements());
  }

  PRUint32 i;
  PRUint32 offset = 0;
  for (i = 0; i < mTextItems.Length(); ++i) {
    TextItem* ti = &mTextItems[i];
    NS_ASSERTION(ti->mLength > 0, "Zero length word contribution?");

    if (!(ti->mFlags & BREAK_ALLOW_INITIAL) && ti->mSinkOffset == 0) {
      breakState[offset] = PR_FALSE;
    }
    if (!(ti->mFlags & BREAK_ALLOW_INSIDE)) {
      PRUint32 exclude = ti->mSinkOffset == 0 ? 1 : 0;
      memset(breakState.Elements() + offset + exclude, PR_FALSE, ti->mLength - exclude);
    }

    // Don't set the break state for the first character of the word, because
    // it was already set correctly earlier and we don't know what the true
    // value should be.
    PRUint32 skipSet = i == 0 ? 1 : 0;
    ti->mSink->SetBreaks(ti->mSinkOffset + skipSet, ti->mLength - skipSet,
                         breakState.Elements() + offset + skipSet);
    offset += ti->mLength;
  }

  mCurrentWord.Clear();
  mTextItems.Clear();
  mCurrentWordContainsCJK = PR_FALSE;
  return NS_OK;
}

nsresult
nsLineBreaker::AppendText(nsIAtom* aLangGroup, const PRUnichar* aText, PRUint32 aLength,
                          PRUint32 aFlags, nsILineBreakSink* aSink)
{
  NS_ASSERTION(aLength > 0, "Appending empty text...");

  PRUint32 offset = 0;

  // Continue the current word
  if (mCurrentWord.Length() > 0) {
    NS_ASSERTION(!mAfterSpace, "These should not be set");

    while (offset < aLength && !IS_SPACE(aText[offset])) {
      mCurrentWord.AppendElement(aText[offset]);
      if (!mCurrentWordContainsCJK && IS_CJK_CHAR(aText[offset])) {
        mCurrentWordContainsCJK = PR_TRUE;
      }
      ++offset;
    }

    if (offset > 0) {
      mTextItems.AppendElement(TextItem(aSink, 0, offset, aFlags));
    }

    if (offset == aLength)
      return NS_OK;

    // We encountered whitespace, so we're done with this word
    nsresult rv = FlushCurrentWord();
    if (NS_FAILED(rv))
      return rv;
  }

  nsAutoTArray<PRPackedBool,4000> breakState;
  if (!breakState.AppendElements(aLength))
    return NS_ERROR_OUT_OF_MEMORY;

  PRUint32 start = offset;
  PRUint32 wordStart = offset;
  PRBool wordHasCJK = PR_FALSE;

  for (;;) {
    PRUnichar ch = aText[offset];
    PRBool isSpace = IS_SPACE(ch);

    breakState[offset] = mAfterSpace && !isSpace &&
      (aFlags & (start == 0 ? BREAK_ALLOW_INITIAL : BREAK_ALLOW_INSIDE));
    mAfterSpace = isSpace;

    if (isSpace) {
      if (offset > wordStart && wordHasCJK) {
        if (aFlags & BREAK_ALLOW_INSIDE) {
          // Save current start-of-word state because GetJISx4051Breaks will
          // set it to false
          PRPackedBool currentStart = breakState[wordStart];
          nsContentUtils::LineBreaker()->
            GetJISx4051Breaks(aText + wordStart, offset - wordStart,
                              breakState.Elements() + wordStart);
          breakState[wordStart] = currentStart;
        }
        wordHasCJK = PR_FALSE;
      }

      ++offset;
      if (offset >= aLength)
        break;
      wordStart = offset;
    } else {
      if (!wordHasCJK && IS_CJK_CHAR(ch)) {
        wordHasCJK = PR_TRUE;
      }
      ++offset;
      if (offset >= aLength) {
        // Save this word
        mCurrentWordContainsCJK = wordHasCJK;
        PRUint32 len = offset - wordStart;
        PRUnichar* elems = mCurrentWord.AppendElements(len);
        if (!elems)
          return NS_ERROR_OUT_OF_MEMORY;
        memcpy(elems, aText + wordStart, sizeof(PRUnichar)*len);
        mTextItems.AppendElement(TextItem(aSink, wordStart, len, aFlags));
        // Ensure that the break-before for this word is written out
        offset = wordStart + 1;
        break;
      }
    }
  }

  aSink->SetBreaks(start, offset - start, breakState.Elements() + start);
  return NS_OK;
}

nsresult
nsLineBreaker::AppendText(nsIAtom* aLangGroup, const PRUint8* aText, PRUint32 aLength,
                          PRUint32 aFlags, nsILineBreakSink* aSink)
{
  NS_ASSERTION(aLength > 0, "Appending empty text...");

  PRUint32 offset = 0;

  // Continue the current word
  if (mCurrentWord.Length() > 0) {
    NS_ASSERTION(!mAfterSpace, "These should not be set");

    while (offset < aLength && !IS_SPACE(aText[offset])) {
      mCurrentWord.AppendElement(aText[offset]);
      ++offset;
    }

    if (offset > 0) {
      mTextItems.AppendElement(TextItem(aSink, 0, offset, aFlags));
    }

    if (offset == aLength) {
      // We did not encounter whitespace so the word hasn't finished yet.
      return NS_OK;
    }

    // We encountered whitespace, so we're done with this word
    nsresult rv = FlushCurrentWord();
    if (NS_FAILED(rv))
      return rv;
  }

  nsAutoTArray<PRPackedBool,4000> breakState;
  if (!breakState.AppendElements(aLength))
    return NS_ERROR_OUT_OF_MEMORY;

  PRUint32 start = offset;
  PRUint32 wordStart = offset;

  for (;;) {
    PRUint8 ch = aText[offset];
    PRBool isSpace = IS_SPACE(ch);

    breakState[offset] = mAfterSpace && !isSpace &&
      (aFlags & (start == 0 ? BREAK_ALLOW_INITIAL : BREAK_ALLOW_INSIDE));
    mAfterSpace = isSpace;

    if (isSpace) {
      // The current word can't have any special (CJK/Thai) characters inside it
      // because this is 8-bit text, so just ignore it
      ++offset;
      if (offset >= aLength)
        break;
      wordStart = offset;
    } else {
      ++offset;
      if (offset >= aLength) {
        // Save this word
        mCurrentWordContainsCJK = PR_FALSE;
        PRUint32 len = offset - wordStart;
        PRUnichar* elems = mCurrentWord.AppendElements(len);
        if (!elems)
          return NS_ERROR_OUT_OF_MEMORY;
        PRUint32 i;
        for (i = wordStart; i < offset; ++i) {
          elems[i - wordStart] = aText[i];
        }
        mTextItems.AppendElement(TextItem(aSink, wordStart, len, aFlags));
        // Ensure that the break-before for this word is written out
        offset = wordStart + 1;
        break;
      }
      // We can't break inside words in 8-bit text (no CJK characters), so
      // there is no need to do anything else to handle words
    }
  }

  aSink->SetBreaks(start, offset - start, breakState.Elements() + start);
  return NS_OK;
}

nsresult
nsLineBreaker::AppendInvisibleWhitespace() {
  // Treat as "invisible whitespace"
  nsresult rv = FlushCurrentWord();
  if (NS_FAILED(rv))
    return rv;
  mAfterSpace = PR_TRUE;
  return NS_OK;  
}
