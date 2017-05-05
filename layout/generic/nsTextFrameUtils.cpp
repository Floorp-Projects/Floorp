/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTextFrameUtils.h"

#include "nsBidiUtils.h"
#include "nsCharTraits.h"
#include "nsIContent.h"
#include "nsStyleStruct.h"
#include "nsTextFragment.h"
#include "nsUnicharUtils.h"
#include <algorithm>

using namespace mozilla;

static bool
IsDiscardable(char16_t ch, nsTextFrameUtils::Flags* aFlags)
{
  // Unlike IS_DISCARDABLE, we don't discard \r. \r will be ignored by gfxTextRun
  // and discarding it would force us to copy text in many cases of preformatted
  // text containing \r\n.
  if (ch == CH_SHY) {
    *aFlags |= nsTextFrameUtils::Flags::TEXT_HAS_SHY;
    return true;
  }
  return IsBidiControl(ch);
}

static bool
IsDiscardable(uint8_t ch, nsTextFrameUtils::Flags* aFlags)
{
  if (ch == CH_SHY) {
    *aFlags |= nsTextFrameUtils::Flags::TEXT_HAS_SHY;
    return true;
  }
  return false;
}

static bool
IsSegmentBreak(char16_t aCh)
{
  return aCh == '\n' || aCh == '\r';
}

static bool
IsSpaceOrTab(char16_t aCh)
{
  return aCh == ' ' || aCh == '\t';
}

static bool
IsSpaceOrTabOrSegmentBreak(char16_t aCh)
{
  return IsSpaceOrTab(aCh) || IsSegmentBreak(aCh);
}

template<typename CharT>
/* static */ bool
nsTextFrameUtils::IsSkippableCharacterForTransformText(CharT aChar)
{
  return aChar == ' ' ||
         aChar == '\t' ||
         aChar == '\n' ||
         aChar == CH_SHY ||
         (aChar > 0xFF && IsBidiControl(aChar));
}

#ifdef DEBUG
template<typename CharT>
static void AssertSkippedExpectedChars(const CharT* aText,
                                       const gfxSkipChars& aSkipChars,
                                       int32_t aSkipCharsOffset)
{
  gfxSkipCharsIterator it(aSkipChars);
  it.AdvanceOriginal(aSkipCharsOffset);
  while (it.GetOriginalOffset() < it.GetOriginalEnd()) {
    CharT ch = aText[it.GetOriginalOffset() - aSkipCharsOffset];
    MOZ_ASSERT(!it.IsOriginalCharSkipped() ||
               nsTextFrameUtils::IsSkippableCharacterForTransformText(ch),
               "skipped unexpected character; need to update "
               "IsSkippableCharacterForTransformText?");
    it.AdvanceOriginal(1);
  }
}
#endif

template<class CharT>
static CharT*
TransformWhiteSpaces(const CharT* aText, uint32_t aLength,
                     uint32_t aBegin, uint32_t aEnd,
                     bool aHasSegmentBreak,
                     bool& aInWhitespace,
                     CharT* aOutput,
                     nsTextFrameUtils::Flags& aFlags,
                     nsTextFrameUtils::CompressionMode aCompression,
                     gfxSkipChars* aSkipChars)
{
  MOZ_ASSERT(aCompression == nsTextFrameUtils::COMPRESS_WHITESPACE ||
             aCompression == nsTextFrameUtils::COMPRESS_WHITESPACE_NEWLINE,
             "whitespaces should be skippable!!");
  // Get the context preceding/following this white space range.
  // For 8-bit text (sizeof CharT == 1), the checks here should get optimized
  // out, and isSegmentBreakSkippable should be initialized to be 'false'.
  bool isSegmentBreakSkippable =
    sizeof(CharT) > 1 &&
    ((aBegin > 0 && IS_ZERO_WIDTH_SPACE(aText[aBegin - 1])) ||
     (aEnd < aLength && IS_ZERO_WIDTH_SPACE(aText[aEnd])));
  if (sizeof(CharT) > 1 && !isSegmentBreakSkippable &&
      aBegin > 0 && aEnd < aLength) {
    uint32_t ucs4before;
    uint32_t ucs4after;
    if (aBegin > 1 &&
        NS_IS_LOW_SURROGATE(aText[aBegin - 1]) &&
        NS_IS_HIGH_SURROGATE(aText[aBegin - 2])) {
      ucs4before = SURROGATE_TO_UCS4(aText[aBegin - 2], aText[aBegin - 1]);
    } else {
      ucs4before = aText[aBegin - 1];
    }
    if (aEnd + 1 < aLength &&
        NS_IS_HIGH_SURROGATE(aText[aEnd]) &&
        NS_IS_LOW_SURROGATE(aText[aEnd + 1])) {
      ucs4after = SURROGATE_TO_UCS4(aText[aEnd], aText[aEnd + 1]);
    } else {
      ucs4after = aText[aEnd];
    }
    // Discard newlines between characters that have F, W, or H
    // EastAsianWidth property and neither side is Hangul.
    isSegmentBreakSkippable = IsSegmentBreakSkipChar(ucs4before) &&
                              IsSegmentBreakSkipChar(ucs4after);
  }

  for (uint32_t i = aBegin; i < aEnd; ++i) {
    CharT ch = aText[i];
    bool keepChar = false;
    bool keepTransformedWhiteSpace = false;
    if (IsDiscardable(ch, &aFlags)) {
      aSkipChars->SkipChar();
      continue;
    }
    if (IsSpaceOrTab(ch)) {
      if (aHasSegmentBreak) {
        // If white-space is set to normal, nowrap, or pre-line, white space
        // characters are considered collapsible and all spaces and tabs
        // immediately preceding or following a segment break are removed.
        aSkipChars->SkipChar();
        continue;
      }

      if (aInWhitespace) {
        aSkipChars->SkipChar();
        continue;
      } else {
        keepTransformedWhiteSpace = true;
      }
    } else {
      // Apply Segment Break Transformation Rules (CSS Text 3 - 4.1.2) for
      // segment break characters.
      if (aCompression == nsTextFrameUtils::COMPRESS_WHITESPACE ||
          // XXX: According to CSS Text 3, a lone CR should not always be
          //      kept, but still go through the Segment Break Transformation
          //      Rules. However, this is what current modern browser engines
          //      (webkit/blink/edge) do. So, once we can get some clarity
          //      from the specification issue, we should either remove the
          //      lone CR condition here, or leave it here with this comment
          //      being rephrased.
          //      Please see https://github.com/w3c/csswg-drafts/issues/855.
          ch == '\r') {
        keepChar = true;
      } else {
        // aCompression == COMPRESS_WHITESPACE_NEWLINE

        // Any collapsible segment break immediately following another
        // collapsible segment break is removed.  Then the remaining segment
        // break is either transformed into a space (U+0020) or removed
        // depending on the context before and after the break.
        if (isSegmentBreakSkippable || aInWhitespace) {
          aSkipChars->SkipChar();
          continue;
        }
        isSegmentBreakSkippable = true;
        keepTransformedWhiteSpace = true;
      }
    }

    if (keepChar) {
      *aOutput++ = ch;
      aSkipChars->KeepChar();
      aInWhitespace = IsSpaceOrTab(ch);
    } else if (keepTransformedWhiteSpace) {
      if (ch != ' ') {
        aFlags |= nsTextFrameUtils::Flags::TEXT_WAS_TRANSFORMED;
      }
      *aOutput++ = ' ';
      aSkipChars->KeepChar();
      aInWhitespace = true;
    } else {
      MOZ_ASSERT_UNREACHABLE("Should've skipped the character!!");
    }
  }
  return aOutput;
}

template<class CharT>
CharT*
nsTextFrameUtils::TransformText(const CharT* aText, uint32_t aLength,
                                CharT* aOutput,
                                CompressionMode aCompression,
                                uint8_t* aIncomingFlags,
                                gfxSkipChars* aSkipChars,
                                Flags* aAnalysisFlags)
{
  Flags flags = Flags();
  CharT* outputStart = aOutput;
#ifdef DEBUG
  int32_t skipCharsOffset = aSkipChars->GetOriginalCharCount();
#endif

  bool lastCharArabic = false;
  if (aCompression == COMPRESS_NONE ||
      aCompression == COMPRESS_NONE_TRANSFORM_TO_SPACE) {
    // Skip discardables.
    uint32_t i;
    for (i = 0; i < aLength; ++i) {
      CharT ch = aText[i];
      if (IsDiscardable(ch, &flags)) {
        aSkipChars->SkipChar();
      } else {
        aSkipChars->KeepChar();
        if (ch > ' ') {
          lastCharArabic = IS_ARABIC_CHAR(ch);
        } else if (aCompression == COMPRESS_NONE_TRANSFORM_TO_SPACE) {
          if (ch == '\t' || ch == '\n') {
            ch = ' ';
            flags |= Flags::TEXT_WAS_TRANSFORMED;
          }
        } else {
          // aCompression == COMPRESS_NONE
          if (ch == '\t') {
            flags |= Flags::TEXT_HAS_TAB;
          }
        }
        *aOutput++ = ch;
      }
    }
    if (lastCharArabic) {
      *aIncomingFlags |= INCOMING_ARABICCHAR;
    } else {
      *aIncomingFlags &= ~INCOMING_ARABICCHAR;
    }
    *aIncomingFlags &= ~INCOMING_WHITESPACE;
  } else {
    bool inWhitespace = (*aIncomingFlags & INCOMING_WHITESPACE) != 0;
    uint32_t i;
    for (i = 0; i < aLength; ++i) {
      CharT ch = aText[i];
      // CSS Text 3 - 4.1. The White Space Processing Rules
      // White space processing in CSS affects only the document white space
      // characters: spaces (U+0020), tabs (U+0009), and segment breaks.
      // Since we need the context of segment breaks and their surrounding
      // white spaces to proceed the white space processing, a consecutive run
      // of spaces/tabs/segment breaks is collected in a first pass loop, then
      // we apply the collapsing and transformation rules to this run in a
      // second pass loop.
      if (IsSpaceOrTabOrSegmentBreak(ch)) {
        bool keepLastSpace = false;
        bool hasSegmentBreak = IsSegmentBreak(ch);
        uint32_t countTrailingDiscardables = 0;
        uint32_t j;
        for (j = i + 1; j < aLength &&
                        (IsSpaceOrTabOrSegmentBreak(aText[j]) ||
                         IsDiscardable(aText[j], &flags));
             j++) {
          if (IsSegmentBreak(aText[j])) {
            hasSegmentBreak = true;
          }
        }
        // Exclude trailing discardables before checking space combining
        // sequence tail.
        for (; IsDiscardable(aText[j - 1], &flags); j--) {
          countTrailingDiscardables++;
        }
        // If the last white space is followed by a combining sequence tail,
        // exclude it from the range of TransformWhiteSpaces.
        if (sizeof(CharT) > 1 && aText[j - 1] == ' ' && j < aLength &&
            IsSpaceCombiningSequenceTail(&aText[j], aLength - j)) {
          keepLastSpace = true;
          j--;
        }
        if (j > i) {
          aOutput = TransformWhiteSpaces(aText, aLength, i, j, hasSegmentBreak,
                                         inWhitespace, aOutput, flags,
                                         aCompression, aSkipChars);
        }
        // We need to keep KeepChar()/SkipChar() in order, so process the
        // last white space first, then process the trailing discardables.
        if (keepLastSpace) {
          keepLastSpace = false;
          *aOutput++ = ' ';
          aSkipChars->KeepChar();
          lastCharArabic = false;
          j++;
        }
        for (; countTrailingDiscardables > 0; countTrailingDiscardables--) {
          aSkipChars->SkipChar();
          j++;
        }
        i = j - 1;
        continue;
      }
      // Process characters other than the document white space characters.
      if (IsDiscardable(ch, &flags)) {
        aSkipChars->SkipChar();
      } else {
        *aOutput++ = ch;
        aSkipChars->KeepChar();
      }
      lastCharArabic = IS_ARABIC_CHAR(ch);
      inWhitespace = false;
    }

    if (lastCharArabic) {
      *aIncomingFlags |= INCOMING_ARABICCHAR;
    } else {
      *aIncomingFlags &= ~INCOMING_ARABICCHAR;
    }
    if (inWhitespace) {
      *aIncomingFlags |= INCOMING_WHITESPACE;
    } else {
      *aIncomingFlags &= ~INCOMING_WHITESPACE;
    }
  }

  if (outputStart + aLength != aOutput) {
    flags |= Flags::TEXT_WAS_TRANSFORMED;
  }
  *aAnalysisFlags = flags;

#ifdef DEBUG
  AssertSkippedExpectedChars(aText, *aSkipChars, skipCharsOffset);
#endif
  return aOutput;
}

/*
 * NOTE: The TransformText and IsSkippableCharacterForTransformText template
 * functions are part of the public API of nsTextFrameUtils, while
 * their function bodies are not available in the header. They may stop working
 * (fail to resolve symbol in link time) once their callsites are moved to a
 * different translation unit (e.g. a different unified source file).
 * Explicit instantiating this function template with `uint8_t` and `char16_t`
 * could prevent us from the potential risk.
 */
template uint8_t*
nsTextFrameUtils::TransformText(const uint8_t* aText, uint32_t aLength,
                                uint8_t* aOutput,
                                CompressionMode aCompression,
                                uint8_t* aIncomingFlags,
                                gfxSkipChars* aSkipChars,
                                Flags* aAnalysisFlags);
template char16_t*
nsTextFrameUtils::TransformText(const char16_t* aText, uint32_t aLength,
                                char16_t* aOutput,
                                CompressionMode aCompression,
                                uint8_t* aIncomingFlags,
                                gfxSkipChars* aSkipChars,
                                Flags* aAnalysisFlags);
template bool
nsTextFrameUtils::IsSkippableCharacterForTransformText(uint8_t aChar);
template bool
nsTextFrameUtils::IsSkippableCharacterForTransformText(char16_t aChar);

uint32_t
nsTextFrameUtils::ComputeApproximateLengthWithWhitespaceCompression(
                    nsIContent *aContent, const nsStyleText *aStyleText)
{
  const nsTextFragment *frag = aContent->GetText();
  // This is an approximation so we don't really need anything
  // too fancy here.
  uint32_t len;
  if (aStyleText->WhiteSpaceIsSignificant()) {
    len = frag->GetLength();
  } else {
    bool is2b = frag->Is2b();
    union {
      const char *s1b;
      const char16_t *s2b;
    } u;
    if (is2b) {
      u.s2b = frag->Get2b();
    } else {
      u.s1b = frag->Get1b();
    }
    bool prevWS = true; // more important to ignore blocks with
                        // only whitespace than get inline boundaries
                        // exactly right
    len = 0;
    for (uint32_t i = 0, i_end = frag->GetLength(); i < i_end; ++i) {
      char16_t c = is2b ? u.s2b[i] : u.s1b[i];
      if (c == ' ' || c == '\n' || c == '\t' || c == '\r') {
        if (!prevWS) {
          ++len;
        }
        prevWS = true;
      } else {
        ++len;
        prevWS = false;
      }
    }
  }
  return len;
}

bool nsSkipCharsRunIterator::NextRun() {
  do {
    if (mRunLength) {
      mIterator.AdvanceOriginal(mRunLength);
      NS_ASSERTION(mRunLength > 0, "No characters in run (initial length too large?)");
      if (!mSkipped || mLengthIncludesSkipped) {
        mRemainingLength -= mRunLength;
      }
    }
    if (!mRemainingLength)
      return false;
    int32_t length;
    mSkipped = mIterator.IsOriginalCharSkipped(&length);
    mRunLength = std::min(length, mRemainingLength);
  } while (!mVisitSkipped && mSkipped);

  return true;
}
