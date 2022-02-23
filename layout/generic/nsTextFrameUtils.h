/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSTEXTFRAMEUTILS_H_
#define NSTEXTFRAMEUTILS_H_

#include "gfxSkipChars.h"
#include "nsBidiUtils.h"

class nsIContent;
struct nsStyleText;

namespace mozilla {
namespace dom {
class Text;
}
}  // namespace mozilla

#define BIG_TEXT_NODE_SIZE 4096

#define CH_NBSP 160
#define CH_SHY 173
#define CH_CJKSP 12288  // U+3000 IDEOGRAPHIC SPACE (CJK Full-Width Space)

class nsTextFrameUtils {
 public:
  // These constants are used as textrun flags for textframe textruns.
  //
  // If you add a flag, please add support for it in gfxTextRun::Dump.
  enum class Flags : uint16_t {
    // The following flags are set by TransformText

    // the text has at least one untransformed tab character
    HasTab = 0x01,
    // the original text has at least one soft hyphen character
    HasShy = 0x02,
    UnusedFlags = 0x04,

    // Flag used in textrun construction to *prevent* hiding of fallback text
    // for pending user-fonts (used for Canvas2d text).
    DontSkipDrawingForPendingUserFonts = 0x08,

    // The following flags are set by nsTextFrame

    IsSimpleFlow = 0x10,
    IncomingWhitespace = 0x20,
    TrailingWhitespace = 0x40,
    CompressedLeadingWhitespace = 0x80,
    NoBreaks = 0x100,
    IsTransformed = 0x200,
    // This gets set if there's a break opportunity at the end of the textrun.
    // We normally don't use this break opportunity because the following text
    // will have a break opportunity at the start, but it's useful for line
    // layout to know about it in case the following content is not text
    HasTrailingBreak = 0x400,

    // This is set if the textrun was created for a textframe whose
    // NS_FRAME_IS_IN_SINGLE_CHAR_MI flag is set.  This occurs if the textframe
    // belongs to a MathML <mi> element whose embedded text consists of a
    // single character.
    IsSingleCharMi = 0x800,

    // This is set if the text run might be observing for glyph changes.
    MightHaveGlyphChanges = 0x1000,

    // For internal use by the memory reporter when accounting for
    // storage used by textruns.
    // Because the reporter may visit each textrun multiple times while
    // walking the frame trees and textrun cache, it needs to mark
    // textruns that have been seen so as to avoid multiple-accounting.
    RunSizeAccounted = 0x2000,

    // The following are defined by gfxTextRunFactory rather than here,
    // so that it also has access to the _INCOMING and MATH_SCRIPT flags
    // for shaping purposes.
    // They live in the gfxShapedText::mFlags field rather than the
    // gfxTextRun::mFlags2 field.
    // TEXT_TRAILING_ARABICCHAR
    // TEXT_INCOMING_ARABICCHAR
    // TEXT_USE_MATH_SCRIPT
  };

  // These constants are used in TransformText to represent context information
  // from previous textruns.
  enum { INCOMING_NONE = 0, INCOMING_WHITESPACE = 1, INCOMING_ARABICCHAR = 2 };

  /**
   * Returns true if aChars/aLength are something that make a space
   * character not be whitespace when they follow the space character
   * (combining mark or join control, ignoring intervening direction
   * controls).
   */
  static bool IsSpaceCombiningSequenceTail(const char16_t* aChars,
                                           int32_t aLength);
  static bool IsSpaceCombiningSequenceTail(const uint8_t* aChars,
                                           int32_t aLength) {
    return false;
  }

  enum CompressionMode {
    COMPRESS_NONE,
    COMPRESS_WHITESPACE,
    COMPRESS_WHITESPACE_NEWLINE,
    COMPRESS_NONE_TRANSFORM_TO_SPACE
  };

  /**
   * Create a text run from a run of Unicode text. The text may have whitespace
   * compressed. A preformatted tab is sent to the text run as a single space.
   * (Tab spacing must be performed by textframe later.) Certain other
   * characters are discarded.
   *
   * @param aCompression control what is compressed to a
   * single space character: no compression, compress spaces (not followed
   * by combining mark) and tabs, compress those plus newlines, or
   * no compression except newlines are discarded.
   * @param aIncomingFlags a flag indicating whether there was whitespace
   * or an Arabic character preceding this text. We set it to indicate if
   * there's an Arabic character or whitespace preceding the end of this text.
   */
  template <class CharT>
  static CharT* TransformText(const CharT* aText, uint32_t aLength,
                              CharT* aOutput, CompressionMode aCompression,
                              uint8_t* aIncomingFlags, gfxSkipChars* aSkipChars,
                              nsTextFrameUtils::Flags* aAnalysisFlags);

  /**
   * Returns whether aChar is a character that nsTextFrameUtils::TransformText
   * might mark as skipped.  This is used by
   * SVGTextContentElement::GetNumberOfChars to know whether reflowing frames,
   * so that we have the results of TransformText, is required, or whether we
   * can use a fast path instead.
   */
  template <class CharT>
  static bool IsSkippableCharacterForTransformText(CharT aChar);

  static void AppendLineBreakOffset(nsTArray<uint32_t>* aArray,
                                    uint32_t aOffset) {
    if (aArray->Length() > 0 && (*aArray)[aArray->Length() - 1] == aOffset) {
      return;
    }
    aArray->AppendElement(aOffset);
  }

  static uint32_t ComputeApproximateLengthWithWhitespaceCompression(
      mozilla::dom::Text*, const nsStyleText*);
  static uint32_t ComputeApproximateLengthWithWhitespaceCompression(
      const nsAString&, const nsStyleText*);
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(nsTextFrameUtils::Flags)

class nsSkipCharsRunIterator {
 public:
  enum LengthMode {
    LENGTH_UNSKIPPED_ONLY = false,
    LENGTH_INCLUDES_SKIPPED = true
  };
  nsSkipCharsRunIterator(const gfxSkipCharsIterator& aStart,
                         LengthMode aLengthIncludesSkipped, uint32_t aLength)
      : mIterator(aStart),
        mRemainingLength(aLength),
        mRunLength(0),
        mSkipped(false),
        mVisitSkipped(false),
        mLengthIncludesSkipped(aLengthIncludesSkipped) {}
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
  int32_t mRemainingLength;
  int32_t mRunLength;
  bool mSkipped;
  bool mVisitSkipped;
  bool mLengthIncludesSkipped;
};

#endif /*NSTEXTFRAMEUTILS_H_*/
