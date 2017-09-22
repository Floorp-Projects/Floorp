/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsBidi_h__
#define nsBidi_h__

#include "unicode/ubidi.h"
#include "ICUUtils.h"
#include "nsIFrame.h" // for nsBidiLevel/nsBidiDirection declarations

// nsBidi implemented as a simple wrapper around the bidi reordering engine
// from ICU.
// We could eliminate this and let callers use the ICU functions directly
// once we no longer care about building without ICU available.

class nsBidi
{
public:
  /** @brief Default constructor.
   *
   * The nsBidi object is initially empty. It is assigned
   * the Bidi properties of a paragraph by <code>SetPara()</code>.
   */
  nsBidi()
  {
    mBiDi = ubidi_open();
  }

  /** @brief Destructor. */
  ~nsBidi()
  {
    ubidi_close(mBiDi);
  }

  /**
   * Perform the Unicode Bidi algorithm.
   *
   * @param aText is a pointer to the single-paragraph text that the
   *      Bidi algorithm will be performed on
   *      (step (P1) of the algorithm is performed externally).
   *      <strong>The text must be (at least) <code>aLength</code> long.</strong>
   *
   * @param aLength is the length of the text; if <code>aLength==-1</code> then
   *      the text must be zero-terminated.
   *
   * @param aParaLevel specifies the default level for the paragraph;
   *      it is typically 0 (LTR) or 1 (RTL).
   *      If the function shall determine the paragraph level from the text,
   *      then <code>aParaLevel</code> can be set to
   *      either <code>NSBIDI_DEFAULT_LTR</code>
   *      or <code>NSBIDI_DEFAULT_RTL</code>;
   *      if there is no strongly typed character, then
   *      the desired default is used (0 for LTR or 1 for RTL).
   *      Any other value between 0 and <code>NSBIDI_MAX_EXPLICIT_LEVEL</code>
   *      is also valid, with odd levels indicating RTL.
   */
  nsresult SetPara(const char16_t* aText, int32_t aLength,
                   nsBidiLevel aParaLevel)
  {
    UErrorCode error = U_ZERO_ERROR;
    ubidi_setPara(mBiDi, reinterpret_cast<const UChar*>(aText), aLength,
                  aParaLevel, nullptr, &error);
    return ICUUtils::UErrorToNsResult(error);
  }

  /**
   * Get the directionality of the text.
   *
   * @param aDirection receives a <code>NSBIDI_XXX</code> value that indicates
   *       if the entire text represented by this object is unidirectional,
   *       and which direction, or if it is mixed-directional.
   *
   * @see nsBidiDirection
   */
  nsBidiDirection GetDirection()
  {
    return nsBidiDirection(ubidi_getDirection(mBiDi));
  }

  /**
   * Get the paragraph level of the text.
   *
   * @param aParaLevel receives a <code>NSBIDI_XXX</code> value indicating
   *                   the paragraph level
   *
   * @see nsBidiLevel
   */
  nsBidiLevel GetParaLevel()
  {
    return ubidi_getParaLevel(mBiDi);
  }

  /**
   * Get a logical run.
   * This function returns information about a run and is used
   * to retrieve runs in logical order.<p>
   * This is especially useful for line-breaking on a paragraph.
   *
   * @param aLogicalStart is the first character of the run.
   *
   * @param aLogicalLimit will receive the limit of the run.
   *      The l-value that you point to here may be the
   *      same expression (variable) as the one for
   *      <code>aLogicalStart</code>.
   *      This pointer can be <code>nullptr</code> if this
   *      value is not necessary.
   *
   * @param aLevel will receive the level of the run.
   *      This pointer can be <code>nullptr</code> if this
   *      value is not necessary.
   */
  void GetLogicalRun(int32_t aLogicalStart, int32_t* aLogicalLimit,
                     nsBidiLevel* aLevel)
  {
    ubidi_getLogicalRun(mBiDi, aLogicalStart, aLogicalLimit, aLevel);
  }

  /**
   * Get the number of runs.
   * This function may invoke the actual reordering on the
   * <code>nsBidi</code> object, after <code>SetPara</code>
   * may have resolved only the levels of the text. Therefore,
   * <code>CountRuns</code> may have to allocate memory,
   * and may fail doing so.
   *
   * @param aRunCount will receive the number of runs.
   */
  nsresult CountRuns(int32_t* aRunCount)
  {
    UErrorCode errorCode = U_ZERO_ERROR;
    *aRunCount = ubidi_countRuns(mBiDi, &errorCode);
    return ICUUtils::UErrorToNsResult(errorCode);
  }

  /**
   * Get one run's logical start, length, and directionality,
   * which can be 0 for LTR or 1 for RTL.
   * In an RTL run, the character at the logical start is
   * visually on the right of the displayed run.
   * The length is the number of characters in the run.<p>
   * <code>CountRuns</code> should be called
   * before the runs are retrieved.
   *
   * @param aRunIndex is the number of the run in visual order, in the
   *      range <code>[0..CountRuns-1]</code>.
   *
   * @param aLogicalStart is the first logical character index in the text.
   *      The pointer may be <code>nullptr</code> if this index is not needed.
   *
   * @param aLength is the number of characters (at least one) in the run.
   *      The pointer may be <code>nullptr</code> if this is not needed.
   *
   * @returns the directionality of the run,
   *       <code>NSBIDI_LTR==0</code> or <code>NSBIDI_RTL==1</code>,
   *       never <code>NSBIDI_MIXED</code>.
   *
   * @see CountRuns<p>
   *
   * Example:
   * @code
   *  int32_t i, count, logicalStart, visualIndex=0, length;
   *  nsBidiDirection dir;
   *  pBidi->CountRuns(&count);
   *  for(i=0; i<count; ++i) {
   *    dir = pBidi->GetVisualRun(i, &logicalStart, &length);
   *    if(NSBIDI_LTR==dir) {
   *      do { // LTR
   *        show_char(text[logicalStart++], visualIndex++);
   *      } while(--length>0);
   *    } else {
   *      logicalStart+=length;  // logicalLimit
   *      do { // RTL
   *        show_char(text[--logicalStart], visualIndex++);
   *      } while(--length>0);
   *    }
   *  }
   * @endcode
   *
   * Note that in right-to-left runs, code like this places
   * modifier letters before base characters and second surrogates
   * before first ones.
   */
  nsBidiDirection GetVisualRun(int32_t aRunIndex,
                               int32_t* aLogicalStart, int32_t* aLength)
  {
    return nsBidiDirection(ubidi_getVisualRun(mBiDi, aRunIndex,
                                              aLogicalStart, aLength));
  }

  /**
   * This is a convenience function that does not use a nsBidi object.
   * It is intended to be used for when an application has determined the levels
   * of objects (character sequences) and just needs to have them reordered (L2).
   * This is equivalent to using <code>GetVisualMap</code> on a
   * <code>nsBidi</code> object.
   *
   * @param aLevels is an array with <code>aLength</code> levels that have been
   *      determined by the application.
   *
   * @param aLength is the number of levels in the array, or, semantically,
   *      the number of objects to be reordered.
   *      It must be <code>aLength>0</code>.
   *
   * @param aIndexMap is a pointer to an array of <code>aLength</code>
   *      indexes which will reflect the reordering of the characters.
   *      The array does not need to be initialized.<p>
   *      The index map will result in
   *        <code>aIndexMap[aVisualIndex]==aLogicalIndex</code>.
   */
  static void ReorderVisual(const nsBidiLevel* aLevels, int32_t aLength,
                            int32_t* aIndexMap)
  {
    ubidi_reorderVisual(aLevels, aLength, aIndexMap);
  }

private:
  nsBidi(const nsBidi&) = delete;
  void operator=(const nsBidi&) = delete;

  UBiDi* mBiDi;
};

#endif // _nsBidi_h_
