/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTextFrameUtils.h"

#include "nsUnicharUtils.h"
#include "nsBidiUtils.h"
#include "nsIContent.h"
#include "nsStyleStruct.h"
#include "nsTextFragment.h"
#include <algorithm>

static bool IsDiscardable(PRUnichar ch, uint32_t* aFlags)
{
  // Unlike IS_DISCARDABLE, we don't discard \r. \r will be ignored by gfxTextRun
  // and discarding it would force us to copy text in many cases of preformatted
  // text containing \r\n.
  if (ch == CH_SHY) {
    *aFlags |= nsTextFrameUtils::TEXT_HAS_SHY;
    return true;
  }
  if ((ch & 0xFF00) != 0x2000) {
    // Not a Bidi control character
    return false;
  }
  return IS_BIDI_CONTROL_CHAR(ch);
}

static bool IsDiscardable(uint8_t ch, uint32_t* aFlags)
{
  if (ch == CH_SHY) {
    *aFlags |= nsTextFrameUtils::TEXT_HAS_SHY;
    return true;
  }
  return false;
}

PRUnichar*
nsTextFrameUtils::TransformText(const PRUnichar* aText, uint32_t aLength,
                                PRUnichar* aOutput,
                                CompressionMode aCompression,
                                uint8_t* aIncomingFlags,
                                gfxSkipCharsBuilder* aSkipChars,
                                uint32_t* aAnalysisFlags)
{
  uint32_t flags = 0;
  PRUnichar* outputStart = aOutput;

  bool lastCharArabic = false;

  if (aCompression == COMPRESS_NONE ||
      aCompression == DISCARD_NEWLINE) {
    // Skip discardables.
    uint32_t i;
    for (i = 0; i < aLength; ++i) {
      PRUnichar ch = *aText++;
      if (IsDiscardable(ch, &flags) ||
          (ch == '\n' && aCompression == DISCARD_NEWLINE)) {
        aSkipChars->SkipChar();
      } else {
        aSkipChars->KeepChar();
        if (ch > ' ') {
          lastCharArabic = IS_ARABIC_CHAR(ch);
        } else if (ch == '\t') {
          flags |= TEXT_HAS_TAB;
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
      PRUnichar ch = *aText++;
      bool nowInWhitespace;
      if (ch == ' ' &&
          (i + 1 >= aLength ||
           !IsSpaceCombiningSequenceTail(aText, aLength - (i + 1)))) {
        nowInWhitespace = true;
      } else if (ch == '\n' && aCompression == COMPRESS_WHITESPACE_NEWLINE) {
        if (i > 0 && IS_CJ_CHAR(aText[-1]) &&
            i + 1 < aLength && IS_CJ_CHAR(aText[1])) {
          // Discard newlines between CJK chars.
          // XXX this really requires more context to get right!
          aSkipChars->SkipChar();
          continue;
        }
        nowInWhitespace = true;
      } else {
        nowInWhitespace = ch == '\t';
      }

      if (!nowInWhitespace) {
        if (IsDiscardable(ch, &flags)) {
          aSkipChars->SkipChar();
          nowInWhitespace = inWhitespace;
        } else {
          *aOutput++ = ch;
          aSkipChars->KeepChar();
          lastCharArabic = IS_ARABIC_CHAR(ch);
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
    flags |= TEXT_WAS_TRANSFORMED;
  }
  *aAnalysisFlags = flags;
  return aOutput;
}

uint8_t*
nsTextFrameUtils::TransformText(const uint8_t* aText, uint32_t aLength,
                                uint8_t* aOutput,
                                CompressionMode aCompression,
                                uint8_t* aIncomingFlags,
                                gfxSkipCharsBuilder* aSkipChars,
                                uint32_t* aAnalysisFlags)
{
  uint32_t flags = 0;
  uint8_t* outputStart = aOutput;

  if (aCompression == COMPRESS_NONE ||
      aCompression == DISCARD_NEWLINE) {
    // Skip discardables.
    uint32_t i;
    for (i = 0; i < aLength; ++i) {
      uint8_t ch = *aText++;
      if (IsDiscardable(ch, &flags) ||
          (ch == '\n' && aCompression == DISCARD_NEWLINE)) {
        aSkipChars->SkipChar();
      } else {
        aSkipChars->KeepChar();
        if (ch == '\t') {
          flags |= TEXT_HAS_TAB;
        }
        *aOutput++ = ch;
      }
    }
    *aIncomingFlags &= ~(INCOMING_ARABICCHAR | INCOMING_WHITESPACE);
  } else {
    bool inWhitespace = (*aIncomingFlags & INCOMING_WHITESPACE) != 0;
    uint32_t i;
    for (i = 0; i < aLength; ++i) {
      uint8_t ch = *aText++;
      bool nowInWhitespace = ch == ' ' || ch == '\t' ||
        (ch == '\n' && aCompression == COMPRESS_WHITESPACE_NEWLINE);
      if (!nowInWhitespace) {
        if (IsDiscardable(ch, &flags)) {
          aSkipChars->SkipChar();
          nowInWhitespace = inWhitespace;
        } else {
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
    *aIncomingFlags &= ~INCOMING_ARABICCHAR;
    if (inWhitespace) {
      *aIncomingFlags |= INCOMING_WHITESPACE;
    } else {
      *aIncomingFlags &= ~INCOMING_WHITESPACE;
    }
  }

  if (outputStart + aLength != aOutput) {
    flags |= TEXT_WAS_TRANSFORMED;
  }
  *aAnalysisFlags = flags;
  return aOutput;
}

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
      const PRUnichar *s2b;
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
      PRUnichar c = is2b ? u.s2b[i] : u.s1b[i];
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
