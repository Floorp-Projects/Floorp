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

#include "nsVoidArray.h"
#include "nsIFrame.h"
#include "nsBidi.h"
#include "nsBidiUtils.h"
#include "nsCOMPtr.h"
#include "nsDataHashtable.h"

class nsBidiPresUtils {
public:
  nsBidiPresUtils();
  ~nsBidiPresUtils();
  PRBool IsSuccessful(void) const;
  
  /**
   * Make Bidi engine calculate the embedding levels of the frames that are
   * descendants of a given block frame.
   *
   * @param aPresContext         The presContext
   * @param aBlockFrame          The block frame
   * @param aFirstChild          The first child frame of aBlockFrame
   * @param aForceReflow         [OUT] Set if we delete frames and will need to
   *                                   reflow the block frame
   * @param aIsVisualFormControl [IN]  Set if we are in a form control on a
   *                                   visual page.
   *                                   @see nsHTMLReflowState::IsBidiFormControl
   *
   *  @lina 06/18/2000
   */
  nsresult Resolve(nsPresContext* aPresContext,
                   nsIFrame*       aBlockFrame,
                   nsIFrame*       aFirstChild,
                   PRBool&         aForceReflow,
                   PRBool          aIsVisualFormControl);

  /**
   * Reorder this line using Bidi engine.
   * Update frame array, following the new visual sequence.
   * 
   * @lina 05/02/2000
   */
  void ReorderFrames(nsPresContext*      aPresContext,
                     nsIRenderingContext* aRendContext,
                     nsIFrame*            aFirstChild,
                     nsIFrame*            aNextInFlow,
                     PRInt32              aChildCount);

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
                             PRBool          aIsOddLevel,
                             PRBool          aIsBidiSystem);

  /**
   * Reorder Unicode text, taking into account bidi capabilities of the
   * platform. The reordering includes symmetric swapping and removing
   * control characters.
   */
  nsresult ReorderUnicodeText(PRUnichar*      aText,
                              PRInt32&        aTextLength,
                              nsCharType      aCharType,
                              PRBool          aIsOddLevel,
                              PRBool          aIsBidiSystem);

  /**
   * Return our nsBidi object (bidi reordering engine)
   */
  nsresult GetBidiEngine(nsBidi** aBidiEngine);

  /**
   * Reorder plain text using the Unicode Bidi algorithm and send it to
   * a rendering context for rendering.
   *
   * @param aText the string to be rendered
   * @param aLength the number of characters in the string
   * @param aBaseDirection the base direction of the string
   *  NSBIDI_LTR - left-to-right string
   *  NSBIDI_RTL - right-to-left string
   * @param aPresContext the presentation context
   * @param aRenderingContext the rendering context
   * @param aTextRect contains the coordinates to render the string
   */
  nsresult RenderText(PRUnichar*           aText,
                      PRInt32              aLength,
                      nsBidiDirection      aBaseDirection,
                      nsPresContext*      aPresContext,
                      nsIRenderingContext& aRenderingContext,
                      nscoord              aX,
                      nscoord              aY);

private:
  /**
   *  Create a string containing entire text content of this block.
   *
   *  @lina 05/02/2000
   */
  void CreateBlockBuffer(nsPresContext* aPresContext);

  /**
   * Set up an array of the frames after splitting frames so that each frame has
   * consistent directionality. At this point the frames are still in logical
   * order
   */
  nsresult InitLogicalArray(nsPresContext* aPresContext,
                            nsIFrame*       aCurrentFrame,
                            nsIFrame*       aNextInFlow,
                            PRBool          aAddMarkers = PR_FALSE);
  /**
   * Reorder the frame array from logical to visual order
   *
   * @param aPresContext the presentation context
   * @param aBidiEnabled TRUE on return if the visual order is different from
   *                      the logical order
   */
  nsresult Reorder(nsPresContext* aPresContext,
                   PRBool&         aBidiEnabled);
  
  /**
   *  Adjust frame positions following their visual order
   *
   *  @param  <code>nsPresContext*</code>, the first kid
   *
   *  @lina 04/11/2000
   */
  void RepositionInlineFrames(nsPresContext*      aPresContext,
                              nsIRenderingContext* aRendContext,
                              nsIFrame*            aFirstChild,
                              PRInt32              aChildCount) const;
  
  void RepositionContainerFrame(nsPresContext* aPresContext,
                                nsIFrame*       aContainer,
                                PRInt32&        aMinX,
                                PRInt32&        aMaxX) const;
  /**
   * Helper method for Resolve()
   * Truncate a text frame and possibly create a continuation frame with the
   * remainder of its content.
   *
   * @param aPresContext the pres context
   * @param aContent     the content of the frame
   * @param aFrame       the original frame
   * @param aNewFrame    [OUT] the new frame that was created
   * @param aFrameIndex  [IN/OUT] index of aFrame in mLogicalFrames
   *
   * If there is already a bidi continuation for this frame in mLogicalFrames,
   * no new frame will be created. On exit aNewFrame will point to the existing
   * bidi continuation and aFrameIndex will contain its index.
   * @see Resolve()
   * @see RemoveBidiContinuation()
   */
  PRBool EnsureBidiContinuation(nsPresContext* aPresContext,
                                nsIContent*     aContent,
                                nsIFrame*       aFrame,
                                nsIFrame**      aNewFrame,
                                PRInt32&        aFrameIndex);

  /**
   * Helper method for Resolve()
   * Delete one or more bidi continuation frames created in a previous reflow by
   * EnsureBidiContinuation().
   * @param aPresContext the pres context
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
  void RemoveBidiContinuation(nsPresContext* aPresContext,
                              nsIFrame*       aFrame,
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
  nsVoidArray     mLogicalFrames;
  nsVoidArray     mVisualFrames;
  nsDataHashtable<nsISupportsHashKey, PRInt32> mContentToFrameIndex;
  PRInt32         mArraySize;
  PRInt32*        mIndexMap;
  PRUint8*        mLevels;
  nsresult        mSuccess;

  nsBidi*         mBidiEngine;
};

#endif /* nsBidiPresUtils_h___ */

#endif // IBMBIDI
