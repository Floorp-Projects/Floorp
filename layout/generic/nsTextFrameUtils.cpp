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

#include "nsTextFrameUtils.h"

#include "nsContentUtils.h"
#include "nsIWordBreaker.h"
#include "gfxFont.h"
#include "nsTextTransformer.h"
#include "nsCompressedCharMap.h"
#include "nsUnicharUtils.h"

// XXX TODO implement transform of backslash to yen that nsTextTransform does
// when requested by PresContext->LanguageSpecificTransformType(). Do it with
// a new factory type that just munges the input stream. But first, check
// that we really still need this, it's only enabled via a hidden pref
// which defaults false...

// Replaced by precompiled CCMap (see bug 180266). To update the list
// of characters, see one of files included below. As for the way
// the original list of characters was obtained by Frank Tang, see bug 54467.
// Updated to fix the regression (bug 263411). The list contains
// characters of the following Unicode character classes : Ps, Pi, Po, Pf, Pe.
// (ref.: http://www.w3.org/TR/2004/CR-CSS21-20040225/selector.html#first-letter)
// Note that the file does NOT yet include non-BMP characters because 
// there's no point including them without fixing the way we identify 
// 'first-letter' currently working only with BMP characters.
#include "punct_marks.ccmap"
DEFINE_CCMAP(gPuncCharsCCMap, const);
  
#define UNICODE_ZWSP 0x200B
  
PRBool
nsTextFrameUtils::IsPunctuationMark(PRUnichar aChar)
{
  return CCMAP_HAS_CHAR(gPuncCharsCCMap, aChar);
}

static PRBool IsDiscardable(PRUnichar ch, PRUint32* aFlags)
{
  // Unlike IS_DISCARDABLE, we don't discard \r. \r will be ignored by gfxTextRun
  // and discarding it would force us to copy text in many cases of preformatted
  // text containing \r\n.
  if (ch == CH_SHY) {
    *aFlags |= nsTextFrameUtils::TEXT_HAS_SHY;
    return PR_TRUE;
  }
  if ((ch & 0xFF00) != 0x2000) {
    // Not a Bidi control character
    return PR_FALSE;
  }
  return IS_BIDI_CONTROL(ch);
}

static PRBool IsDiscardable(PRUint8 ch, PRUint32* aFlags)
{
  if (ch == CH_SHY) {
    *aFlags |= nsTextFrameUtils::TEXT_HAS_SHY;
    return PR_TRUE;
  }
  return PR_FALSE;
}

PRUnichar*
nsTextFrameUtils::TransformText(const PRUnichar* aText, PRUint32 aLength,
                                PRUnichar* aOutput,
                                PRBool aCompressWhitespace,
                                PRPackedBool* aIncomingWhitespace,
                                gfxSkipCharsBuilder* aSkipChars,
                                PRUint32* aAnalysisFlags)
{
  // We're just going to assume this!
  PRUint32 flags = TEXT_HAS_NON_ASCII;
  PRUnichar* outputStart = aOutput;

  if (!aCompressWhitespace) {
    // Convert tabs and formfeeds to spaces and skip discardables.
    PRUint32 i;
    for (i = 0; i < aLength; ++i) {
      PRUnichar ch = *aText++;
      if (ch == '\t') {
        flags |= TEXT_HAS_TAB|TEXT_WAS_TRANSFORMED;
        aSkipChars->KeepChar();
        *aOutput++ = ' ';
      } else if (IsDiscardable(ch, &flags)) {
        aSkipChars->SkipChar();
      } else {
        aSkipChars->KeepChar();
        if (ch == CH_NBSP) {
          ch = ' ';
          flags |= TEXT_WAS_TRANSFORMED;
        } else if (IS_SURROGATE(ch)) {
          flags |= gfxTextRunFactory::TEXT_HAS_SURROGATES;
        }
        *aOutput++ = ch;
      }
    }
  } else {
    PRBool inWhitespace = *aIncomingWhitespace;
    PRUint32 i;
    for (i = 0; i < aLength; ++i) {
      PRUnichar ch = *aText++;
      PRBool nowInWhitespace;
      if (ch == ' ' &&
          (i + 1 >= aLength ||
           !IsSpaceCombiningSequenceTail(aText, aLength - (i + 1)))) {
        nowInWhitespace = PR_TRUE;
      } else if (ch == '\n') {
        if (i > 0 && IS_CJ_CHAR(aText[-1]) &&
            i + 1 < aLength && IS_CJ_CHAR(aText[1])) {
          // Discard newlines between CJK chars.
          // XXX this really requires more context to get right!
          aSkipChars->SkipChar();
          continue;
        }
        nowInWhitespace = PR_TRUE;
      } else {
        nowInWhitespace = ch == '\t' || ch == '\f' || ch == UNICODE_ZWSP;
      }

      if (!nowInWhitespace) {
        if (IsDiscardable(ch, &flags)) {
          aSkipChars->SkipChar();
          nowInWhitespace = inWhitespace;
        } else {
          if (ch == CH_NBSP) {
            ch = ' ';
            flags |= TEXT_WAS_TRANSFORMED;
          } else if (IS_SURROGATE(ch)) {
            flags |= gfxTextRunFactory::TEXT_HAS_SURROGATES;
          }
          *aOutput++ = ch;
          aSkipChars->KeepChar();
        }
      } else {
        if (inWhitespace) {
          aSkipChars->SkipChar();
        } else {
          if (ch != ' ') {
            flags |= TEXT_WAS_TRANSFORMED;
          }
          *aOutput++ = ' ';
          aSkipChars->KeepChar();
        }
      }
      inWhitespace = nowInWhitespace;
    }
  }

  if (outputStart < aOutput) {
    *aIncomingWhitespace = aOutput[-1] == ' ';
  }
  if (outputStart + aLength != aOutput) {
    flags |= TEXT_WAS_TRANSFORMED;
  }
  *aAnalysisFlags = flags;
  return aOutput;
}

PRUint8*
nsTextFrameUtils::TransformText(const PRUint8* aText, PRUint32 aLength,
                                PRUint8* aOutput,
                                PRBool aCompressWhitespace,
                                PRPackedBool* aIncomingWhitespace,
                                gfxSkipCharsBuilder* aSkipChars,
                                PRUint32* aAnalysisFlags)
{
  PRUint32 flags = 0;
  PRUint8 allBits = 0;
  PRUint8* outputStart = aOutput;

  if (!aCompressWhitespace) {
    // Convert tabs to spaces and skip discardables.
    PRUint32 i;
    for (i = 0; i < aLength; ++i) {
      PRUint8 ch = *aText++;
      allBits |= ch;
      if (ch == '\t') {
        flags |= TEXT_HAS_TAB|TEXT_WAS_TRANSFORMED;
        aSkipChars->KeepChar();
        *aOutput++ = ' ';
      } else if (IsDiscardable(ch, &flags)) {
        aSkipChars->SkipChar();
      } else {
        aSkipChars->KeepChar();
        if (ch == CH_NBSP) {
          ch = ' ';
          flags |= TEXT_WAS_TRANSFORMED;
        }
        *aOutput++ = ch;
      }
    }
  } else {
    PRBool inWhitespace = *aIncomingWhitespace;
    PRUint32 i;
    for (i = 0; i < aLength; ++i) {
      PRUint8 ch = *aText++;
      allBits |= ch;
      PRBool nowInWhitespace = ch == ' ' || ch == '\t' || ch == '\n' || ch == '\f';
      if (!nowInWhitespace) {
        if (IsDiscardable(ch, &flags)) {
          aSkipChars->SkipChar();
          nowInWhitespace = inWhitespace;
        } else {
          if (ch == CH_NBSP) {
            ch = ' ';
            flags |= TEXT_WAS_TRANSFORMED;
          }
          *aOutput++ = ch;
          aSkipChars->KeepChar();
        }
      } else {
        if (inWhitespace) {
          aSkipChars->SkipChar();
        } else {
          if (ch != ' ') {
            flags |= TEXT_WAS_TRANSFORMED;
          }
          *aOutput++ = ' ';
          aSkipChars->KeepChar();
        }
      }
      inWhitespace = nowInWhitespace;
    }
  }

  if (outputStart < aOutput) {
    *aIncomingWhitespace = aOutput[-1] == ' ';
  }
  if (outputStart + aLength != aOutput) {
    flags |= TEXT_WAS_TRANSFORMED;
  }
  if (allBits & 0x80) {
    flags |= TEXT_HAS_NON_ASCII;
  }
  *aAnalysisFlags = flags;
  return aOutput;
}

// TODO The wordbreaker needs to be fixed. It's buggy, for example, it doesn't
// handle diacriticals combined with spaces
enum SimpleCharClass {
  CLASS_ALNUM,
  CLASS_PUNCT,
  CLASS_SPACE
};

// This is what nsSampleWordBreaker::GetClass considers whitespace
static PRBool IsWordBreakerWhitespace(const PRUnichar* aChars, PRInt32 aLength)
{
  NS_ASSERTION(aLength > 0, "Can't handle empty string");
  PRUnichar ch = aChars[0];
  if (ch == '\t' || ch == '\n' || ch == '\r')
    return PR_TRUE;
  if (ch == ' ' &&
      !nsTextFrameUtils::IsSpaceCombiningSequenceTail(aChars + 1, aLength - 1))
    return PR_TRUE;
  return PR_FALSE;
}

// like nsSampleWordBreaker::GetClass
static SimpleCharClass Classify8BitChar(PRUint8 aChar)
{
  if (aChar == ' ' || aChar == '\t' || aChar == '\n' || aChar == '\r')
    return CLASS_SPACE;
  if ((aChar >= 'a' && aChar <= 'z') || (aChar >= 'A' || aChar <= 'Z') ||
      (aChar >= '0' && aChar <= '9') || (aChar >= 128))
    return CLASS_ALNUM;
  return CLASS_PUNCT;
}

PRInt32
nsTextFrameUtils::FindWordBoundary(const nsTextFragment* aText,
                                   gfxTextRun* aTextRun,
                                   gfxSkipCharsIterator* aIterator,
                                   PRInt32 aOffset, PRInt32 aLength,
                                   PRInt32 aPosition, PRInt32 aDirection,
                                   PRBool aBreakBeforePunctuation,
                                   PRBool aBreakAfterPunctuation,
                                   PRBool* aWordIsWhitespace)
{
  // A space followed by combining diacritical marks is not whitespace!!
  PRInt32 textLength = aText->GetLength();
  NS_ASSERTION(aOffset + aLength <= textLength,
               "Substring out of bounds");
  NS_ASSERTION(aPosition >= aOffset && aPosition < aOffset + aLength,
               "Position out of bounds");
  *aWordIsWhitespace = aText->Is2b()
    ? IsWordBreakerWhitespace(aText->Get2b() + aPosition, textLength - aPosition)
    : Classify8BitChar(aText->Get1b()[aPosition]) == CLASS_SPACE;

  PRInt32 len = 0; // length of current "word", excluding first character at aPosition
  if (aText->Is2b()) {
    nsIWordBreaker* wordBreaker = nsContentUtils::WordBreaker();
    const PRUnichar* text = aText->Get2b();
    // XXX the wordbreaker currently isn't cluster-aware. We need to make
    // it cluster-aware. In the meantime, just reject any word breaks
    // inside clusters.
    for (;;) {
      if (aDirection > 0) {
        // Returns the index of the first character of the next word
        PRInt32 nextWordPos = wordBreaker->NextWord(text, textLength, aPosition + len);
        if (nextWordPos < 0)
          break;
        len = nextWordPos - aPosition - 1;
      } else {
        // Returns the index of the first character of the current word
        PRInt32 nextWordPos = wordBreaker->PrevWord(text, textLength, aPosition + len);
        if (nextWordPos < 0)
          break;
        len = aPosition - nextWordPos;
      }
      if (aTextRun->IsClusterStart(aIterator->ConvertOriginalToSkipped(aPosition + len*aDirection)))
        break;
    }
  } else {
    const char* text = aText->Get1b();
    SimpleCharClass cl = Classify8BitChar(text[aPosition]);
    // There shouldn't be any clusters in 8bit text but we'll cover that
    // possibility anyway
    PRInt32 nextWordPos;
    do {
      ++len;
      nextWordPos = aPosition + aDirection*len;
      if (nextWordPos < aOffset || nextWordPos >= aOffset + aLength)
        break;
    } while (Classify8BitChar(text[nextWordPos]) == cl ||
             !aTextRun->IsClusterStart(aIterator->ConvertOriginalToSkipped(nextWordPos)));
  }

  // Handle punctuation breaks
  PRInt32 i;
  PRBool punctPrev = IsPunctuationMark(aText->CharAt(aPosition));
  for (i = 1; i < len; ++i) {
    PRInt32 pos = aPosition + i*aDirection;
    // See if there's a punctuation break between i-aDirection and i
    PRBool punct = IsPunctuationMark(aText->CharAt(pos));
    if (punct != punctPrev &&
        aTextRun->IsClusterStart(aIterator->ConvertOriginalToSkipped(pos))) {
      PRBool punctIsBefore = aDirection < 0 ? punct : punctPrev;
      if (punctIsBefore ? aBreakAfterPunctuation : aBreakBeforePunctuation)
        break;
    }
    punctPrev = punct;
  }
  PRInt32 pos = aPosition + i*aDirection;
  if (pos < aOffset || pos >= aOffset + aLength)
    return -1;
  return i;
}

PRBool nsSkipCharsRunIterator::NextRun() {
  do {
    if (mRunLength) {
      mIterator.AdvanceOriginal(mRunLength);
      NS_ASSERTION(mRunLength > 0, "No characters in run (initial length too large?)");
      if (!mSkipped || mLengthIncludesSkipped) {
        mRemainingLength -= mRunLength;
      }
    }
    if (!mRemainingLength)
      return PR_FALSE;
    PRInt32 length;
    mSkipped = mIterator.IsOriginalCharSkipped(&length);
    mRunLength = PR_MIN(length, mRemainingLength);
  } while (!mVisitSkipped && mSkipped);

  return PR_TRUE;
}
