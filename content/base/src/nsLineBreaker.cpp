/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsLineBreaker.h"
#include "nsContentUtils.h"
#include "nsILineBreaker.h"
#include "gfxFont.h" // for the gfxTextRun::CompressedGlyph::FLAG_BREAK_TYPE_* values
#include "nsHyphenationManager.h"
#include "nsHyphenator.h"

nsLineBreaker::nsLineBreaker()
  : mCurrentWordLangGroup(nsnull),
    mCurrentWordContainsMixedLang(false),
    mCurrentWordContainsComplexChar(false),
    mAfterBreakableSpace(false), mBreakHere(false),
    mWordBreak(nsILineBreaker::kWordBreak_Normal)
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
  // Capitalize the first alphanumeric character after a space or start
  // of the word.
  // The only space character a word can contain is NBSP.
  bool capitalizeNextChar = true;
  for (PRUint32 i = 0; i < aLength; ++i) {
    PRUint32 ch = aWord[i];
    if (capitalizeNextChar) {
      if (NS_IS_HIGH_SURROGATE(ch) && i + 1 < aLength &&
          NS_IS_LOW_SURROGATE(aWord[i + 1])) {
        ch = SURROGATE_TO_UCS4(ch, aWord[i + 1]);
      }
      if (nsContentUtils::IsAlphanumeric(ch)) {
        aCapitalization[i] = true;
        capitalizeNextChar = false;
      }
      if (!IS_IN_BMP(ch)) {
        ++i;
      }
    }
    if (ch == 0xA0 /*NBSP*/) {
      capitalizeNextChar = true;
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
    // For break-strict set everything internal to "break", otherwise
    // to "no break"!
    memset(breakState.Elements(),
           mWordBreak == nsILineBreaker::kWordBreak_BreakAll ?
             gfxTextRun::CompressedGlyph::FLAG_BREAK_TYPE_NORMAL :
             gfxTextRun::CompressedGlyph::FLAG_BREAK_TYPE_NONE,
           length*sizeof(PRUint8));
  } else {
    nsContentUtils::LineBreaker()->
      GetJISx4051Breaks(mCurrentWord.Elements(), length, mWordBreak,
                        breakState.Elements());
  }

  bool autoHyphenate = mCurrentWordLangGroup &&
    !mCurrentWordContainsMixedLang;
  PRUint32 i;
  for (i = 0; autoHyphenate && i < mTextItems.Length(); ++i) {
    TextItem* ti = &mTextItems[i];
    if (!(ti->mFlags & BREAK_USE_AUTO_HYPHENATION)) {
      autoHyphenate = false;
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
  mCurrentWordContainsComplexChar = false;
  mCurrentWordContainsMixedLang = false;
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
        mCurrentWordContainsComplexChar = true;
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
    memset(capitalizationState.Elements(), false, aLength*sizeof(bool));
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
        mBreakHere || (mAfterBreakableSpace && !isBreakableSpace) ||
        (mWordBreak == nsILineBreaker::kWordBreak_BreakAll)  ?
          gfxTextRun::CompressedGlyph::FLAG_BREAK_TYPE_NORMAL :
          gfxTextRun::CompressedGlyph::FLAG_BREAK_TYPE_NONE;
    }
    mBreakHere = false;
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
                                mWordBreak,
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
      wordHasComplexChar = false;
      ++offset;
      if (offset >= aLength)
        break;
      wordStart = offset;
    } else {
      if (!wordHasComplexChar && IsComplexChar(ch)) {
        wordHasComplexChar = true;
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
        mCurrentWordContainsComplexChar = true;
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
      // Consider word-break style.  Since the break position of CJK scripts
      // will be set by nsILineBreaker, we don't consider CJK at this point.
      breakState[offset] =
        mBreakHere || (mAfterBreakableSpace && !isBreakableSpace) ||
        (mWordBreak == nsILineBreaker::kWordBreak_BreakAll) ?
          gfxTextRun::CompressedGlyph::FLAG_BREAK_TYPE_NORMAL :
          gfxTextRun::CompressedGlyph::FLAG_BREAK_TYPE_NONE;
    }
    mBreakHere = false;
    mAfterBreakableSpace = isBreakableSpace;

    if (isSpace) {
      if (offset > wordStart && wordHasComplexChar) {
        if (aSink && !(aFlags & BREAK_SUPPRESS_INSIDE)) {
          // Save current start-of-word state because GetJISx4051Breaks will
          // set it to false
          PRUint8 currentStart = breakState[wordStart];
          nsContentUtils::LineBreaker()->
            GetJISx4051Breaks(aText + wordStart, offset - wordStart,
                              mWordBreak,
                              breakState.Elements() + wordStart);
          breakState[wordStart] = currentStart;
        }
        wordHasComplexChar = false;
      }

      ++offset;
      if (offset >= aLength)
        break;
      wordStart = offset;
    } else {
      if (!wordHasComplexChar && IsComplexASCIIChar(ch)) {
        wordHasComplexChar = true;
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
    mCurrentWordContainsMixedLang = true;
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
    mBreakHere = true;
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
  mBreakHere = false;
  mAfterBreakableSpace = false;
  return NS_OK;
}
