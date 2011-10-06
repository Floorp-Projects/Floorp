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
#include "gfxFont.h" // for the gfxTextRun::CompressedGlyph::FLAG_BREAK_TYPE_* values
#include "nsHyphenationManager.h"
#include "nsHyphenator.h"

nsLineBreaker::nsLineBreaker()
  : mCurrentWordLangGroup(nsnull),
    mCurrentWordContainsMixedLang(PR_FALSE),
    mCurrentWordContainsComplexChar(PR_FALSE),
    mAfterBreakableSpace(PR_FALSE), mBreakHere(PR_FALSE)
{
}

nsLineBreaker::~nsLineBreaker()
{
  NS_ASSERTION(mCurrentWord.Length() == 0, "Should have Reset() before destruction!");
}

static void
SetupCapitalization(const PRUnichar* aWord, PRUint32 aLength,
                    bool* aCapitalization)
{
  // Capitalize the first non-punctuation character after a space or start
  // of the word.
  // The only space character a word can contain is NBSP.
  bool capitalizeNextChar = true;
  for (PRUint32 i = 0; i < aLength; ++i) {
    if (capitalizeNextChar && !nsContentUtils::IsPunctuationMark(aWord[i])) {
      aCapitalization[i] = PR_TRUE;
      capitalizeNextChar = PR_FALSE;
    }
    if (aWord[i] == 0xA0 /*NBSP*/) {
      capitalizeNextChar = PR_TRUE;
    }
  }
}

nsresult
nsLineBreaker::FlushCurrentWord()
{
  PRUint32 length = mCurrentWord.Length();
  nsAutoTArray<PRUint8,4000> breakState;
  if (!breakState.AppendElements(length))
    return NS_ERROR_OUT_OF_MEMORY;
  
  nsTArray<bool> capitalizationState;

  if (!mCurrentWordContainsComplexChar) {
    // Just set everything internal to "no break"!
    memset(breakState.Elements(),
           gfxTextRun::CompressedGlyph::FLAG_BREAK_TYPE_NONE,
           length*sizeof(PRUint8));
  } else {
    nsContentUtils::LineBreaker()->
      GetJISx4051Breaks(mCurrentWord.Elements(), length, breakState.Elements());
  }

  bool autoHyphenate = mCurrentWordLangGroup &&
    !mCurrentWordContainsMixedLang;
  PRUint32 i;
  for (i = 0; autoHyphenate && i < mTextItems.Length(); ++i) {
    TextItem* ti = &mTextItems[i];
    if (!(ti->mFlags & BREAK_USE_AUTO_HYPHENATION)) {
      autoHyphenate = PR_FALSE;
    }
  }
  if (autoHyphenate) {
    nsRefPtr<nsHyphenator> hyphenator =
      nsHyphenationManager::Instance()->GetHyphenator(mCurrentWordLangGroup);
    if (hyphenator) {
      FindHyphenationPoints(hyphenator,
                            mCurrentWord.Elements(),
                            mCurrentWord.Elements() + length,
                            breakState.Elements());
    }
  }

  PRUint32 offset = 0;
  for (i = 0; i < mTextItems.Length(); ++i) {
    TextItem* ti = &mTextItems[i];
    NS_ASSERTION(ti->mLength > 0, "Zero length word contribution?");

    if ((ti->mFlags & BREAK_SUPPRESS_INITIAL) && ti->mSinkOffset == 0) {
      breakState[offset] = gfxTextRun::CompressedGlyph::FLAG_BREAK_TYPE_NONE;
    }
    if (ti->mFlags & BREAK_SUPPRESS_INSIDE) {
      PRUint32 exclude = ti->mSinkOffset == 0 ? 1 : 0;
      memset(breakState.Elements() + offset + exclude,
             gfxTextRun::CompressedGlyph::FLAG_BREAK_TYPE_NONE,
             (ti->mLength - exclude)*sizeof(PRUint8));
    }

    // Don't set the break state for the first character of the word, because
    // it was already set correctly earlier and we don't know what the true
    // value should be.
    PRUint32 skipSet = i == 0 ? 1 : 0;
    if (ti->mSink) {
      ti->mSink->SetBreaks(ti->mSinkOffset + skipSet, ti->mLength - skipSet,
                           breakState.Elements() + offset + skipSet);

      if (ti->mFlags & BREAK_NEED_CAPITALIZATION) {
        if (capitalizationState.Length() == 0) {
          if (!capitalizationState.AppendElements(length))
            return NS_ERROR_OUT_OF_MEMORY;
          memset(capitalizationState.Elements(), false, length*sizeof(bool));
          SetupCapitalization(mCurrentWord.Elements(), length,
                              capitalizationState.Elements());
        }
        ti->mSink->SetCapitalization(ti->mSinkOffset, ti->mLength,
                                     capitalizationState.Elements() + offset);
      }
    }
    
    offset += ti->mLength;
  }

  mCurrentWord.Clear();
  mTextItems.Clear();
  mCurrentWordContainsComplexChar = PR_FALSE;
  mCurrentWordContainsMixedLang = PR_FALSE;
  mCurrentWordLangGroup = nsnull;
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
    NS_ASSERTION(!mAfterBreakableSpace && !mBreakHere, "These should not be set");

    while (offset < aLength && !IsSpace(aText[offset])) {
      mCurrentWord.AppendElement(aText[offset]);
      if (!mCurrentWordContainsComplexChar && IsComplexChar(aText[offset])) {
        mCurrentWordContainsComplexChar = PR_TRUE;
      }
      UpdateCurrentWordLangGroup(aLangGroup);
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

  nsAutoTArray<PRUint8,4000> breakState;
  if (aSink) {
    if (!breakState.AppendElements(aLength))
      return NS_ERROR_OUT_OF_MEMORY;
  }
  
  nsTArray<bool> capitalizationState;
  if (aSink && (aFlags & BREAK_NEED_CAPITALIZATION)) {
    if (!capitalizationState.AppendElements(aLength))
      return NS_ERROR_OUT_OF_MEMORY;
    memset(capitalizationState.Elements(), PR_FALSE, aLength);
  }

  PRUint32 start = offset;
  bool noBreaksNeeded = !aSink ||
    (aFlags == (BREAK_SUPPRESS_INITIAL | BREAK_SUPPRESS_INSIDE | BREAK_SKIP_SETTING_NO_BREAKS) &&
     !mBreakHere && !mAfterBreakableSpace);
  if (noBreaksNeeded) {
    // Skip to the space before the last word, since either the break data
    // here is not needed, or no breaks are set in the sink and there cannot
    // be any breaks in this chunk; all we need is the context for the next
    // chunk (if any)
    offset = aLength;
    while (offset > start) {
      --offset;
      if (IsSpace(aText[offset]))
        break;
    }
  }
  PRUint32 wordStart = offset;
  bool wordHasComplexChar = false;

  nsRefPtr<nsHyphenator> hyphenator;
  if ((aFlags & BREAK_USE_AUTO_HYPHENATION) && !(aFlags & BREAK_SUPPRESS_INSIDE)) {
    hyphenator = nsHyphenationManager::Instance()->GetHyphenator(aLangGroup);
  }

  for (;;) {
    PRUnichar ch = aText[offset];
    bool isSpace = IsSpace(ch);
    bool isBreakableSpace = isSpace && !(aFlags & BREAK_SUPPRESS_INSIDE);

    if (aSink) {
      breakState[offset] =
        mBreakHere || (mAfterBreakableSpace && !isBreakableSpace) ?
          gfxTextRun::CompressedGlyph::FLAG_BREAK_TYPE_NORMAL :
          gfxTextRun::CompressedGlyph::FLAG_BREAK_TYPE_NONE;
    }
    mBreakHere = PR_FALSE;
    mAfterBreakableSpace = isBreakableSpace;

    if (isSpace) {
      if (offset > wordStart && aSink) {
        if (!(aFlags & BREAK_SUPPRESS_INSIDE)) {
          if (wordHasComplexChar) {
            // Save current start-of-word state because GetJISx4051Breaks will
            // set it to false
            PRUint8 currentStart = breakState[wordStart];
            nsContentUtils::LineBreaker()->
              GetJISx4051Breaks(aText + wordStart, offset - wordStart,
                                breakState.Elements() + wordStart);
            breakState[wordStart] = currentStart;
          }
          if (hyphenator) {
            FindHyphenationPoints(hyphenator,
                                  aText + wordStart, aText + offset,
                                  breakState.Elements() + wordStart);
          }
        }
        if (aFlags & BREAK_NEED_CAPITALIZATION) {
          SetupCapitalization(aText + wordStart, offset - wordStart,
                              capitalizationState.Elements() + wordStart);
        }
      }
      wordHasComplexChar = PR_FALSE;
      ++offset;
      if (offset >= aLength)
        break;
      wordStart = offset;
    } else {
      if (!wordHasComplexChar && IsComplexChar(ch)) {
        wordHasComplexChar = PR_TRUE;
      }
      ++offset;
      if (offset >= aLength) {
        // Save this word
        mCurrentWordContainsComplexChar = wordHasComplexChar;
        PRUint32 len = offset - wordStart;
        PRUnichar* elems = mCurrentWord.AppendElements(len);
        if (!elems)
          return NS_ERROR_OUT_OF_MEMORY;
        memcpy(elems, aText + wordStart, sizeof(PRUnichar)*len);
        mTextItems.AppendElement(TextItem(aSink, wordStart, len, aFlags));
        // Ensure that the break-before for this word is written out
        offset = wordStart + 1;
        UpdateCurrentWordLangGroup(aLangGroup);
        break;
      }
    }
  }

  if (!noBreaksNeeded) {
    // aSink must not be null
    aSink->SetBreaks(start, offset - start, breakState.Elements() + start);
    if (aFlags & BREAK_NEED_CAPITALIZATION) {
      aSink->SetCapitalization(start, offset - start,
                               capitalizationState.Elements() + start);
    }
  }
  return NS_OK;
}

void
nsLineBreaker::FindHyphenationPoints(nsHyphenator *aHyphenator,
                                     const PRUnichar *aTextStart,
                                     const PRUnichar *aTextLimit,
                                     PRUint8 *aBreakState)
{
  nsDependentSubstring string(aTextStart, aTextLimit);
  nsAutoTArray<bool,200> hyphens;
  if (NS_SUCCEEDED(aHyphenator->Hyphenate(string, hyphens))) {
    for (PRUint32 i = 0; i + 1 < string.Length(); ++i) {
      if (hyphens[i]) {
        aBreakState[i + 1] =
          gfxTextRun::CompressedGlyph::FLAG_BREAK_TYPE_HYPHEN;
      }
    }
  }
}

nsresult
nsLineBreaker::AppendText(nsIAtom* aLangGroup, const PRUint8* aText, PRUint32 aLength,
                          PRUint32 aFlags, nsILineBreakSink* aSink)
{
  NS_ASSERTION(aLength > 0, "Appending empty text...");

  if (aFlags & (BREAK_NEED_CAPITALIZATION | BREAK_USE_AUTO_HYPHENATION)) {
    // Defer to the Unicode path if capitalization or hyphenation is required
    nsAutoString str;
    const char* cp = reinterpret_cast<const char*>(aText);
    CopyASCIItoUTF16(nsDependentCSubstring(cp, cp + aLength), str);
    return AppendText(aLangGroup, str.get(), aLength, aFlags, aSink);
  }

  PRUint32 offset = 0;

  // Continue the current word
  if (mCurrentWord.Length() > 0) {
    NS_ASSERTION(!mAfterBreakableSpace && !mBreakHere, "These should not be set");

    while (offset < aLength && !IsSpace(aText[offset])) {
      mCurrentWord.AppendElement(aText[offset]);
      if (!mCurrentWordContainsComplexChar &&
          IsComplexASCIIChar(aText[offset])) {
        mCurrentWordContainsComplexChar = PR_TRUE;
      }
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

  nsAutoTArray<PRUint8,4000> breakState;
  if (aSink) {
    if (!breakState.AppendElements(aLength))
      return NS_ERROR_OUT_OF_MEMORY;
  }

  PRUint32 start = offset;
  bool noBreaksNeeded = !aSink ||
    (aFlags == (BREAK_SUPPRESS_INITIAL | BREAK_SUPPRESS_INSIDE | BREAK_SKIP_SETTING_NO_BREAKS) &&
     !mBreakHere && !mAfterBreakableSpace);
  if (noBreaksNeeded) {
    // Skip to the space before the last word, since either the break data
    // here is not needed, or no breaks are set in the sink and there cannot
    // be any breaks in this chunk; all we need is the context for the next
    // chunk (if any)
    offset = aLength;
    while (offset > start) {
      --offset;
      if (IsSpace(aText[offset]))
        break;
    }
  }
  PRUint32 wordStart = offset;
  bool wordHasComplexChar = false;

  for (;;) {
    PRUint8 ch = aText[offset];
    bool isSpace = IsSpace(ch);
    bool isBreakableSpace = isSpace && !(aFlags & BREAK_SUPPRESS_INSIDE);

    if (aSink) {
      breakState[offset] =
        mBreakHere || (mAfterBreakableSpace && !isBreakableSpace) ?
          gfxTextRun::CompressedGlyph::FLAG_BREAK_TYPE_NORMAL :
          gfxTextRun::CompressedGlyph::FLAG_BREAK_TYPE_NONE;
    }
    mBreakHere = PR_FALSE;
    mAfterBreakableSpace = isBreakableSpace;

    if (isSpace) {
      if (offset > wordStart && wordHasComplexChar) {
        if (aSink && !(aFlags & BREAK_SUPPRESS_INSIDE)) {
          // Save current start-of-word state because GetJISx4051Breaks will
          // set it to false
          PRUint8 currentStart = breakState[wordStart];
          nsContentUtils::LineBreaker()->
            GetJISx4051Breaks(aText + wordStart, offset - wordStart,
                              breakState.Elements() + wordStart);
          breakState[wordStart] = currentStart;
        }
        wordHasComplexChar = PR_FALSE;
      }

      ++offset;
      if (offset >= aLength)
        break;
      wordStart = offset;
    } else {
      if (!wordHasComplexChar && IsComplexASCIIChar(ch)) {
        wordHasComplexChar = PR_TRUE;
      }
      ++offset;
      if (offset >= aLength) {
        // Save this word
        mCurrentWordContainsComplexChar = wordHasComplexChar;
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
    }
  }

  if (!noBreaksNeeded) {
    aSink->SetBreaks(start, offset - start, breakState.Elements() + start);
  }
  return NS_OK;
}

void
nsLineBreaker::UpdateCurrentWordLangGroup(nsIAtom *aLangGroup)
{
  if (mCurrentWordLangGroup && mCurrentWordLangGroup != aLangGroup) {
    mCurrentWordContainsMixedLang = PR_TRUE;
  } else {
    mCurrentWordLangGroup = aLangGroup;
  }
}

nsresult
nsLineBreaker::AppendInvisibleWhitespace(PRUint32 aFlags)
{
  nsresult rv = FlushCurrentWord();
  if (NS_FAILED(rv))
    return rv;

  bool isBreakableSpace = !(aFlags & BREAK_SUPPRESS_INSIDE);
  if (mAfterBreakableSpace && !isBreakableSpace) {
    mBreakHere = PR_TRUE;
  }
  mAfterBreakableSpace = isBreakableSpace;
  return NS_OK;
}

nsresult
nsLineBreaker::Reset(bool* aTrailingBreak)
{
  nsresult rv = FlushCurrentWord();
  if (NS_FAILED(rv))
    return rv;

  *aTrailingBreak = mBreakHere || mAfterBreakableSpace;
  mBreakHere = PR_FALSE;
  mAfterBreakableSpace = PR_FALSE;
  return NS_OK;
}
