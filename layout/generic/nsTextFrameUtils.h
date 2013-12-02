/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSTEXTFRAMEUTILS_H_
#define NSTEXTFRAMEUTILS_H_

#include "gfxSkipChars.h"

class nsIContent;
struct nsStyleText;

#define BIG_TEXT_NODE_SIZE 4096

#define CH_NBSP   160
#define CH_SHY    173
#define CH_CJKSP  12288 // U+3000 IDEOGRAPHIC SPACE (CJK Full-Width Space)

class nsTextFrameUtils {
public:
  // These constants are used as textrun flags for textframe textruns.
  enum {
    // The following flags are set by TransformText

    // the text has at least one untransformed tab character
    TEXT_HAS_TAB             = 0x010000,
    // the original text has at least one soft hyphen character
    TEXT_HAS_SHY             = 0x020000,
    TEXT_WAS_TRANSFORMED     = 0x040000,
    TEXT_UNUSED_FLAG         = 0x080000,

    // The following flags are set by nsTextFrame

    TEXT_IS_SIMPLE_FLOW      = 0x100000,
    TEXT_INCOMING_WHITESPACE = 0x200000,
    TEXT_TRAILING_WHITESPACE = 0x400000,
    TEXT_COMPRESSED_LEADING_WHITESPACE = 0x800000,
    TEXT_NO_BREAKS           = 0x1000000,
    TEXT_IS_TRANSFORMED      = 0x2000000,
    // This gets set if there's a break opportunity at the end of the textrun.
    // We normally don't use this break opportunity because the following text
    // will have a break opportunity at the start, but it's useful for line
    // layout to know about it in case the following content is not text
    TEXT_HAS_TRAILING_BREAK  = 0x4000000,

    // This is set if the textrun was created for a textframe whose
    // TEXT_IS_IN_SINGLE_CHAR_MI flag is set.  This occurs if the textframe
    // belongs to a MathML <mi> element whose embedded text consists of a
    // single character.
    TEXT_IS_SINGLE_CHAR_MI   = 0x8000000

    // The following are defined by gfxTextRunWordCache rather than here,
    // so that it also has access to the _INCOMING flag
    // TEXT_TRAILING_ARABICCHAR
    // TEXT_INCOMING_ARABICCHAR
  };

  // These constants are used in TransformText to represent context information
  // from previous textruns.
  enum {
    INCOMING_NONE       = 0,
    INCOMING_WHITESPACE = 1,
    INCOMING_ARABICCHAR = 2
  };

  /**
   * Returns true if aChars/aLength are something that make a space
   * character not be whitespace when they follow the space character.
   * For now, this is true if and only if aChars starts with a ZWJ. (This
   * is what Uniscribe assumes.)
   */
  static bool
  IsSpaceCombiningSequenceTail(const PRUnichar* aChars, int32_t aLength) {
    return aLength > 0 && aChars[0] == 0x200D; // ZWJ
  }

  enum CompressionMode {
    COMPRESS_NONE,
    COMPRESS_WHITESPACE,
    COMPRESS_WHITESPACE_NEWLINE,
    DISCARD_NEWLINE
  };

  /**
   * Create a text run from a run of Unicode text. The text may have whitespace
   * compressed. A preformatted tab is sent to the text run as a single space.
   * (Tab spacing must be performed by textframe later.) Certain other
   * characters are discarded.
   * 
   * @param aCompressWhitespace control what is compressed to a
   * single space character: no compression, compress spaces (not followed
   * by combining mark) and tabs, compress those plus newlines, or
   * no compression except newlines are discarded.
   * @param aIncomingFlags a flag indicating whether there was whitespace
   * or an Arabic character preceding this text. We set it to indicate if
   * there's an Arabic character or whitespace preceding the end of this text.
   */
  static PRUnichar* TransformText(const PRUnichar* aText, uint32_t aLength,
                                  PRUnichar* aOutput,
                                  CompressionMode aCompression,
                                  uint8_t * aIncomingFlags,
                                  gfxSkipCharsBuilder* aSkipChars,
                                  uint32_t* aAnalysisFlags);

  static uint8_t* TransformText(const uint8_t* aText, uint32_t aLength,
                                uint8_t* aOutput,
                                CompressionMode aCompression,
                                uint8_t * aIncomingFlags,
                                gfxSkipCharsBuilder* aSkipChars,
                                uint32_t* aAnalysisFlags);

  static void
  AppendLineBreakOffset(nsTArray<uint32_t>* aArray, uint32_t aOffset)
  {
    if (aArray->Length() > 0 && (*aArray)[aArray->Length() - 1] == aOffset)
      return;
    aArray->AppendElement(aOffset);
  }

  static uint32_t
  ComputeApproximateLengthWithWhitespaceCompression(nsIContent *aContent,
                                                    const nsStyleText
                                                      *aStyleText);
};

class nsSkipCharsRunIterator {
public:
  enum LengthMode {
    LENGTH_UNSKIPPED_ONLY   = false,
    LENGTH_INCLUDES_SKIPPED = true
  };
  nsSkipCharsRunIterator(const gfxSkipCharsIterator& aStart,
      LengthMode aLengthIncludesSkipped, uint32_t aLength)
    : mIterator(aStart), mRemainingLength(aLength), mRunLength(0),
      mVisitSkipped(false),
      mLengthIncludesSkipped(aLengthIncludesSkipped) {
  }
  void SetVisitSkipped() { mVisitSkipped = true; }
  void SetOriginalOffset(int32_t aOffset) {
    mIterator.SetOriginalOffset(aOffset);
  }
  void SetSkippedOffset(uint32_t aOffset) {
    mIterator.SetSkippedOffset(aOffset);
  }

  // guaranteed to return only positive-length runs
  bool NextRun();
  bool IsSkipped() const { return mSkipped; }
  // Always returns something > 0
  int32_t GetRunLength() const { return mRunLength; }
  const gfxSkipCharsIterator& GetPos() const { return mIterator; }
  int32_t GetOriginalOffset() const { return mIterator.GetOriginalOffset(); }
  uint32_t GetSkippedOffset() const { return mIterator.GetSkippedOffset(); }

private:
  gfxSkipCharsIterator mIterator;
  int32_t              mRemainingLength;
  int32_t              mRunLength;
  bool                 mSkipped;
  bool                 mVisitSkipped;
  bool                 mLengthIncludesSkipped;
};

#endif /*NSTEXTFRAMEUTILS_H_*/
