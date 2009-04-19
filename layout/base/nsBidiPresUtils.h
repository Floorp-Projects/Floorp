/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   IBM Corporation
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifdef IBMBIDI

#ifndef nsBidiPresUtils_h___
#define nsBidiPresUtils_h___

#include "nsTArray.h"
#include "nsIFrame.h"
#include "nsBidi.h"
#include "nsBidiUtils.h"
#include "nsCOMPtr.h"
#include "nsDataHashtable.h"
#include "nsBlockFrame.h"
#include "nsTHashtable.h"

/**
 * A structure representing some continuation state for each frame on the line,
 * used to determine the first and the last continuation frame for each
 * continuation chain on the line.
 */
struct nsFrameContinuationState : public nsVoidPtrHashKey
{
  nsFrameContinuationState(const void *aFrame) : nsVoidPtrHashKey(aFrame) {}

  /**
   * The first visual frame in the continuation chain containing this frame, or
   * nsnull if this frame is the first visual frame in the chain.
   */
  nsIFrame* mFirstVisualFrame;

  /**
   * The number of frames in the continuation chain containing this frame, if
   * this frame is the first visual frame of the chain, or 0 otherwise.
   */
  PRUint32 mFrameCount;

  /**
   * TRUE if this frame is the first visual frame of its continuation chain on
   * this line and the chain has some frames on the previous lines.
   */
  PRPackedBool mHasContOnPrevLines;

  /**
   * TRUE if this frame is the first visual frame of its continuation chain on
   * this line and the chain has some frames left for next lines.
   */
  PRPackedBool mHasContOnNextLines;
};

/*
 * Following type is used to pass needed hashtable to reordering methods
 */
typedef nsTHashtable<nsFrameContinuationState> nsContinuationStates;

/**
 * A structure representing a logical position which should be resolved
 * into its visual position during BiDi processing.
 */
struct nsBidiPositionResolve
{
  // [in] Logical index within string.
  PRInt32 logicalIndex;
  // [out] Visual index within string.
  // If the logical position was not found, set to kNotFound.
  PRInt32 visualIndex;
  // [out] Visual position of the character, from the left (on the X axis), in twips.
  // Eessentially, this is the X position (relative to the rendering context) where the text was drawn + the font metric of the visual string to the left of the given logical position.
  // If the logical position was not found, set to kNotFound.
  PRInt32 visualLeftTwips;
  // [out] Visual width of the character, in twips.
  // If the logical position was not found, set to kNotFound.
  PRInt32 visualWidth;
};

class nsBidiPresUtils {
public:
  nsBidiPresUtils();
  ~nsBidiPresUtils();
  PRBool IsSuccessful(void) const;
  
  /**
   * Interface for the processor used by ProcessText. Used by process text to
   * collect information about the width of subruns and to notify where each
   * subrun should be rendered.
   */
  class BidiProcessor {
  public:
    virtual ~BidiProcessor() { }

    /**
     * Sets the current text with the given length and the given direction.
     *
     * @remark The reason that the function gives a string instead of an index
     *  is that ProcessText copies and modifies the string passed to it, so
     *  passing an index would be impossible.
     * 
     * @param aText The string of text.
     * @param aLength The length of the string of text.
     * @param aDirection The direction of the text. The string will never have
     *  mixed direction.
     */
    virtual void SetText(const PRUnichar*   aText,
                         PRInt32            aLength,
                         nsBidiDirection    aDirection) = 0;

    /**
     * Returns the measured width of the text given in SetText. If SetText was
     * not called with valid parameters, the result of this call is undefined.
     * This call is guaranteed to only be called once between SetText calls.
     * Will be invoked before DrawText.
     */
    virtual nscoord GetWidth() = 0;

    /**
     * Draws the text given in SetText to a rendering context. If SetText was
     * not called with valid parameters, the result of this call is undefined.
     * This call is guaranteed to only be called once between SetText calls.
     * 
     * @param aXOffset The offset of the left side of the substring to be drawn
     *  from the beginning of the overall string passed to ProcessText.
     * @param aWidth The width returned by GetWidth.
     */
    virtual void DrawText(nscoord   aXOffset,
                          nscoord   aWidth) = 0;
  };

  /**
   * Make Bidi engine calculate the embedding levels of the frames that are
   * descendants of a given block frame.
   *
   * @param aBlockFrame          The block frame
   * @param aIsVisualFormControl [IN]  Set if we are in a form control on a
   *                                   visual page.
   *                                   @see nsBlockFrame::IsVisualFormControl
   *
   *  @lina 06/18/2000
   */
  nsresult Resolve(nsBlockFrame*   aBlockFrame,
                   PRBool          aIsVisualFormControl);

  /**
   * Reorder this line using Bidi engine.
   * Update frame array, following the new visual sequence.
   * 
   * @lina 05/02/2000
   */
  void ReorderFrames(nsIFrame*            aFirstFrameOnLine,
                     PRInt32              aNumFramesOnLine);

  /**
   * Format Unicode text, taking into account bidi capabilities
   * of the platform. The formatting includes: reordering, Arabic shaping,
   * symmetric and numeric swapping, removing control characters.
   *
   * @lina 06/18/2000 
   */
  nsresult FormatUnicodeText(nsPresContext* aPresContext,
                             PRUnichar*      aText,
                             PRInt32&        aTextLength,
                             nsCharType      aCharType,
                             PRBool          aIsOddLevel);

  /**
   * Return our nsBidi object (bidi reordering engine)
   */
  nsresult GetBidiEngine(nsBidi** aBidiEngine);

  /**
   * Reorder plain text using the Unicode Bidi algorithm and send it to
   * a rendering context for rendering.
   *
   * @param[in] aText  the string to be rendered (in logical order)
   * @param aLength the number of characters in the string
   * @param aBaseDirection the base direction of the string
   *  NSBIDI_LTR - left-to-right string
   *  NSBIDI_RTL - right-to-left string
   * @param aPresContext the presentation context
   * @param aRenderingContext the rendering context
   * @param aX the x-coordinate to render the string
   * @param aY the y-coordinate to render the string
   * @param[in,out] aPosResolve array of logical positions to resolve into visual positions; can be nsnull if this functionality is not required
   * @param aPosResolveCount number of items in the aPosResolve array
   */
  nsresult RenderText(const PRUnichar*       aText,
                      PRInt32                aLength,
                      nsBidiDirection        aBaseDirection,
                      nsPresContext*         aPresContext,
                      nsIRenderingContext&   aRenderingContext,
                      nscoord                aX,
                      nscoord                aY,
                      nsBidiPositionResolve* aPosResolve = nsnull,
                      PRInt32                aPosResolveCount = 0)
  {
    return ProcessTextForRenderingContext(aText, aLength, aBaseDirection, aPresContext, aRenderingContext,
                                          MODE_DRAW, aX, aY, aPosResolve, aPosResolveCount, nsnull);
  }
  
  nscoord MeasureTextWidth(const PRUnichar*     aText,
                           PRInt32              aLength,
                           nsBidiDirection      aBaseDirection,
                           nsPresContext*       aPresContext,
                           nsIRenderingContext& aRenderingContext)
  {
    nscoord length;
    nsresult rv = ProcessTextForRenderingContext(aText, aLength, aBaseDirection, aPresContext, aRenderingContext,
                                                 MODE_MEASURE, 0, 0, nsnull, 0, &length);
    return NS_SUCCEEDED(rv) ? length : 0;
  }

  /**
   * Check if a line is reordered, i.e., if the child frames are not
   * all laid out left-to-right.
   * @param aFirstFrameOnLine : first frame of the line to be tested
   * @param aNumFramesOnLine : number of frames on this line
   * @param[out] aLeftMost : leftmost frame on this line
   * @param[out] aRightMost : rightmost frame on this line
   */
  PRBool CheckLineOrder(nsIFrame*  aFirstFrameOnLine,
                        PRInt32    aNumFramesOnLine,
                        nsIFrame** aLeftmost,
                        nsIFrame** aRightmost);

  /**
   * Get the frame to the right of the given frame, on the same line.
   * @param aFrame : We're looking for the frame to the right of this frame.
   *                 If null, return the leftmost frame on the line.
   * @param aFirstFrameOnLine : first frame of the line to be tested
   * @param aNumFramesOnLine : number of frames on this line
   */
  nsIFrame* GetFrameToRightOf(const nsIFrame*  aFrame,
                              nsIFrame*        aFirstFrameOnLine,
                              PRInt32          aNumFramesOnLine);
    
  /**
   * Get the frame to the left of the given frame, on the same line.
   * @param aFrame : We're looking for the frame to the left of this frame.
   *                 If null, return the rightmost frame on the line.
   * @param aFirstFrameOnLine : first frame of the line to be tested
   * @param aNumFramesOnLine : number of frames on this line
   */
  nsIFrame* GetFrameToLeftOf(const nsIFrame*  aFrame,
                             nsIFrame*        aFirstFrameOnLine,
                             PRInt32          aNumFramesOnLine);
    
  /**
   * Get the bidi embedding level of the given (inline) frame.
   */
  static nsBidiLevel GetFrameEmbeddingLevel(nsIFrame* aFrame);

  /**
   * Get the bidi base level of the given (inline) frame.
   */
  static nsBidiLevel GetFrameBaseLevel(nsIFrame* aFrame);

  enum Mode { MODE_DRAW, MODE_MEASURE };

  /**
   * Reorder plain text using the Unicode Bidi algorithm and send it to
   * a processor for rendering or measuring
   *
   * @param[in] aText  the string to be processed (in logical order)
   * @param aLength the number of characters in the string
   * @param aBaseDirection the base direction of the string
   *  NSBIDI_LTR - left-to-right string
   *  NSBIDI_RTL - right-to-left string
   * @param aPresContext the presentation context
   * @param aprocessor the bidi processor
   * @param aMode the operation to process
   *  MODE_DRAW - invokes DrawText on the processor for each substring
   *  MODE_MEASURE - does not invoke DrawText on the processor
   *  Note that the string is always measured, regardless of mode
   * @param[in,out] aPosResolve array of logical positions to resolve into
   *  visual positions; can be nsnull if this functionality is not required
   * @param aPosResolveCount number of items in the aPosResolve array
   * @param[out] aWidth Pointer to where the width will be stored (may be null)
   */
  nsresult ProcessText(const PRUnichar*       aText,
                       PRInt32                aLength,
                       nsBidiDirection        aBaseDirection,
                       nsPresContext*         aPresContext,
                       BidiProcessor&         aprocessor,
                       Mode                   aMode,
                       nsBidiPositionResolve* aPosResolve,
                       PRInt32                aPosResolveCount,
                       nscoord*               aWidth);

private:
  nsresult ProcessTextForRenderingContext(const PRUnichar*       aText,
                                          PRInt32                aLength,
                                          nsBidiDirection        aBaseDirection,
                                          nsPresContext*         aPresContext,
                                          nsIRenderingContext&   aRenderingContext,
                                          Mode                   aMode,
                                          nscoord                aX, // DRAW only
                                          nscoord                aY, // DRAW only
                                          nsBidiPositionResolve* aPosResolve,  /* may be null */
                                          PRInt32                aPosResolveCount,
                                          nscoord*               aWidth /* may be null */);

  /**
   *  Create a string containing entire text content of this block.
   *
   *  @lina 05/02/2000
   */
  void CreateBlockBuffer();

  /**
   * Set up an array of the frames after splitting frames so that each frame has
   * consistent directionality. At this point the frames are still in logical
   * order
   */
  void InitLogicalArray(nsIFrame* aCurrentFrame);

  /**
   * Initialize the logically-ordered array of frames
   * using the top-level frames of a single line
   */
  void InitLogicalArrayFromLine(nsIFrame* aFirstFrameOnLine,
                                PRInt32   aNumFramesOnLine);

  /**
   * Reorder the frame array from logical to visual order
   * 
   * @param aReordered TRUE on return if the visual order is different from
   *                   the logical order
   * @param aHasRTLFrames TRUE on return if at least one of the frames is RTL
   *                      (and therefore might have reordered descendents)
   */
  nsresult Reorder(PRBool& aReordered, PRBool& aHasRTLFrames);
  
  /*
   * Position aFrame and it's descendants to their visual places. Also if aFrame
   * is not leaf, resize it to embrace it's children.
   *
   * @param aFrame               The frame which itself and its children are going
   *                             to be repositioned
   * @param aIsOddLevel          TRUE means the embedding level of this frame is odd
   * @param[in,out] aLeft        IN value is the starting position of aFrame(without
   *                             considering its left margin)
   *                             OUT value will be the ending position of aFrame(after
   *                             adding its right margin)
   * @param aContinuationStates  A map from nsIFrame* to nsFrameContinuationState
   */
  void RepositionFrame(nsIFrame*              aFrame,
                       PRBool                 aIsOddLevel,
                       nscoord&               aLeft,
                       nsContinuationStates*  aContinuationStates) const;

  /*
   * Initialize the continuation state(nsFrameContinuationState) to
   * (nsnull, 0) for aFrame and its descendants.
   *
   * @param aFrame               The frame which itself and its descendants will
   *                             be initialized
   * @param aContinuationStates  A map from nsIFrame* to nsFrameContinuationState
   */
  void InitContinuationStates(nsIFrame*              aFrame,
                              nsContinuationStates*  aContinuationStates) const;

  /*
   * Determine if aFrame is leftmost or rightmost, and set aIsLeftMost and
   * aIsRightMost values. Also set continuation states of aContinuationStates.
   *
   * A frame is leftmost if it's the first appearance of its continuation chain
   * on the line and the chain is on its first line if it's LTR or the chain is
   * on its last line if it's RTL.
   * A frame is rightmost if it's the last appearance of its continuation chain
   * on the line and the chain is on its first line if it's RTL or the chain is
   * on its last line if it's LTR.
   *
   * @param aContinuationStates  A map from nsIFrame* to nsFrameContinuationState
   * @param[out] aIsLeftMost     TRUE means aFrame is leftmost frame or continuation
   * @param[out] aIsRightMost    TRUE means aFrame is rightmost frame or continuation
   */
   void IsLeftOrRightMost(nsIFrame*              aFrame,
                          nsContinuationStates*  aContinuationStates,
                          PRBool&                aIsLeftMost /* out */,
                          PRBool&                aIsRightMost /* out */) const;

  /**
   *  Adjust frame positions following their visual order
   *
   *  @param aFirstChild the first kid
   *
   *  @lina 04/11/2000
   */
  void RepositionInlineFrames(nsIFrame* aFirstChild) const;
  
  /**
   * Helper method for Resolve()
   * Truncate a text frame to the end of a single-directional run and possibly
   * create a continuation frame for the remainder of its content.
   *
   * @param aFrame       the original frame
   * @param aNewFrame    [OUT] the new frame that was created
   * @param aFrameIndex  [IN/OUT] index of aFrame in mLogicalFrames
   * @param aStart       [IN] the start of the content mapped by aFrame (and 
   *                          any fluid continuations)
   * @param aEnd         [IN] the offset of the end of the single-directional
   *                          text run.
   * @see Resolve()
   * @see RemoveBidiContinuation()
   */
  inline
  void EnsureBidiContinuation(nsIFrame*       aFrame,
                              nsIFrame**      aNewFrame,
                              PRInt32&        aFrameIndex,
                              PRInt32         aStart,
                              PRInt32         aEnd);

  /**
   * Helper method for Resolve()
   * Convert one or more bidi continuation frames created in a previous reflow by
   * EnsureBidiContinuation() into fluid continuations.
   * @param aFrame       the frame whose continuations are to be removed
   * @param aFirstIndex  index of aFrame in mLogicalFrames
   * @param aLastIndex   index of the last frame to be removed
   * @param aOffset      [OUT] count of directional frames removed. Since
   *                     directional frames have control characters
   *                     corresponding to them in mBuffer, the pointers to
   *                     mBuffer in Resolve() will need to be updated after
   *                     deleting the frames.
   *
   * @see Resolve()
   * @see EnsureBidiContinuation()
   */
  void RemoveBidiContinuation(nsIFrame*       aFrame,
                              PRInt32         aFirstIndex,
                              PRInt32         aLastIndex,
                              PRInt32&        aOffset) const;
  void CalculateCharType(PRInt32& aOffset,
                         PRInt32  aCharTypeLimit,
                         PRInt32& aRunLimit,
                         PRInt32& aRunLength,
                         PRInt32& aRunCount,
                         PRUint8& aCharType,
                         PRUint8& aPrevCharType) const;
  
  void StripBidiControlCharacters(PRUnichar* aText,
                                  PRInt32&   aTextLength) const;
  nsAutoString    mBuffer;
  nsTArray<nsIFrame*> mLogicalFrames;
  nsTArray<nsIFrame*> mVisualFrames;
  nsDataHashtable<nsISupportsHashKey, PRInt32> mContentToFrameIndex;
  PRInt32         mArraySize;
  PRInt32*        mIndexMap;
  PRUint8*        mLevels;
  nsresult        mSuccess;

  nsBidi*         mBidiEngine;
};

#endif /* nsBidiPresUtils_h___ */

#endif // IBMBIDI
