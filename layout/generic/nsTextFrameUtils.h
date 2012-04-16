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
 *   Ehsan Akhgari <ehsan.akhgari@gmail.com>
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

class nsIContent;
struct nsStyleText;

#define BIG_TEXT_NODE_SIZE 4096

#define CH_NBSP   160
#define CH_SHY    173
#define CH_CJKSP  12288 // U+3000 IDEOGRAPHIC SPACE (CJK Full-Width Space)

#define CH_LRM  8206  //<!ENTITY lrm     CDATA "&#8206;" -- left-to-right mark, U+200E NEW RFC 2070 -->
#define CH_RLM  8207  //<!ENTITY rlm     CDATA "&#8207;" -- right-to-left mark, U+200F NEW RFC 2070 -->
#define CH_LRE  8234  //<!CDATA "&#8234;" -- left-to-right embedding, U+202A -->
#define CH_RLO  8238  //<!CDATA "&#8238;" -- right-to-left override, U+202E -->

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
    TEXT_HAS_TRAILING_BREAK  = 0x4000000

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
  IsSpaceCombiningSequenceTail(const PRUnichar* aChars, PRInt32 aLength) {
    return aLength > 0 && aChars[0] == 0x200D; // ZWJ
  }

  enum CompressionMode {
    COMPRESS_NONE,
    COMPRESS_WHITESPACE,
    COMPRESS_WHITESPACE_NEWLINE
  };

  /**
   * Create a text run from a run of Unicode text. The text may have whitespace
   * compressed. A preformatted tab is sent to the text run as a single space.
   * (Tab spacing must be performed by textframe later.) Certain other
   * characters are discarded.
   * 
   * @param aCompressWhitespace control what is compressed to a
   * single space character: no compression, compress spaces (not followed
   * by combining mark) and tabs, and compress those plus newlines.
   * @param aIncomingFlags a flag indicating whether there was whitespace
   * or an Arabic character preceding this text. We set it to indicate if
   * there's an Arabic character or whitespace preceding the end of this text.
   */
  static PRUnichar* TransformText(const PRUnichar* aText, PRUint32 aLength,
                                  PRUnichar* aOutput,
                                  CompressionMode aCompression,
                                  PRUint8 * aIncomingFlags,
                                  gfxSkipCharsBuilder* aSkipChars,
                                  PRUint32* aAnalysisFlags);

  static PRUint8* TransformText(const PRUint8* aText, PRUint32 aLength,
                                PRUint8* aOutput,
                                CompressionMode aCompression,
                                PRUint8 * aIncomingFlags,
                                gfxSkipCharsBuilder* aSkipChars,
                                PRUint32* aAnalysisFlags);

  static void
  AppendLineBreakOffset(nsTArray<PRUint32>* aArray, PRUint32 aOffset)
  {
    if (aArray->Length() > 0 && (*aArray)[aArray->Length() - 1] == aOffset)
      return;
    aArray->AppendElement(aOffset);
  }

  static PRUint32
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
      LengthMode aLengthIncludesSkipped, PRUint32 aLength)
    : mIterator(aStart), mRemainingLength(aLength), mRunLength(0),
      mVisitSkipped(false),
      mLengthIncludesSkipped(aLengthIncludesSkipped) {
  }
  void SetVisitSkipped() { mVisitSkipped = true; }
  void SetOriginalOffset(PRInt32 aOffset) {
    mIterator.SetOriginalOffset(aOffset);
  }
  void SetSkippedOffset(PRUint32 aOffset) {
    mIterator.SetSkippedOffset(aOffset);
  }

  // guaranteed to return only positive-length runs
  bool NextRun();
  bool IsSkipped() const { return mSkipped; }
  // Always returns something > 0
  PRInt32 GetRunLength() const { return mRunLength; }
  const gfxSkipCharsIterator& GetPos() const { return mIterator; }
  PRInt32 GetOriginalOffset() const { return mIterator.GetOriginalOffset(); }
  PRUint32 GetSkippedOffset() const { return mIterator.GetSkippedOffset(); }

private:
  gfxSkipCharsIterator mIterator;
  PRInt32              mRemainingLength;
  PRInt32              mRunLength;
  bool                 mSkipped;
  bool                 mVisitSkipped;
  bool                 mLengthIncludesSkipped;
};

#endif /*NSTEXTFRAMEUTILS_H_*/
