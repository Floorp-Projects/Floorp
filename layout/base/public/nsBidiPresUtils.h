/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * IBM Corporation.  Portions created by IBM are
 * Copyright (C) 2000 IBM Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 */

#ifdef IBMBIDI

#ifndef nsBidiPresUtils_h___
#define nsBidiPresUtils_h___

#include "nsVoidArray.h"
#include "nsIFrame.h"
#include "nsIBidi.h"
#include "nsIUBidiUtils.h"
#include "nsCOMPtr.h"

class nsBidiPresUtils {
public:
  nsBidiPresUtils();
  ~nsBidiPresUtils();
  PRBool IsSuccessful(void) const;
  
  /**
   *  Make Bidi engine calculate embedding levels of the frames.
   *
   *  @lina 06/18/2000
   */
  nsresult Resolve(nsIPresContext* aPresContext,
                   nsIFrame*       aBlockFrame,
                   nsIFrame*       aFirstChild,
                   PRBool&         aForceReflow);

  /**
   * Reorder this line using Bidi engine.
   * Update frame array, following the new visual sequence.
   * 
   * @lina 05/02/2000
   */
  void ReorderFrames(nsIPresContext*      aPresContext,
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
  nsresult FormatUnicodeText(nsIPresContext* aPresContext,
                             PRUnichar*      aText,
                             PRInt32&        aTextLength,
                             nsCharType      aCharType,
                             PRBool          aIsOddLevel,
                             PRBool          aIsBidiSystem);

  /**
   * Return our nsIBidi object (bidi reordering engine)
   */
  nsresult GetBidiEngine(nsIBidi** aBidiEngine);

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
                      nsIPresContext*      aPresContext,
                      nsIRenderingContext& aRenderingContext,
                      nscoord              aX,
                      nscoord              aY);

private:
  /**
   *  Create a string containing entire text content of this block.
   *
   *  @lina 05/02/2000
   */
  void CreateBlockBuffer(nsIPresContext* aPresContext);

  /**
   * Set up an array of the frames after splitting frames so that each frame has
   * consistent directionality. At this point the frames are still in logical
   * order
   */
  nsresult InitLogicalArray(nsIPresContext* aPresContext,
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
  nsresult Reorder(nsIPresContext* aPresContext,
                   PRBool&         aBidiEnabled);
  
  /**
   *  Adjust frame positions following their visual order
   *
   *  @param  <code>nsIPresContext*</code>, the first kid
   *
   *  @lina 04/11/2000
   */
  void RepositionInlineFrames(nsIPresContext*      aPresContext,
                              nsIRenderingContext* aRendContext,
                              nsIFrame*            aFirstChild,
                              PRInt32              aChildCount) const;
  
  void RepositionContainerFrame(nsIPresContext* aPresContext,
                                nsIFrame*       aContainer,
                                PRInt32&        aMinX,
                                PRInt32&        aMaxX) const;
  PRBool EnsureBidiContinuation(nsIPresContext* aPresContext,
                                nsIContent*     aContent,
                                nsIFrame*       aFrame,
                                nsIFrame**      aNewFrame,
                                PRInt32&        aFrameIndex);
  PRBool RemoveBidiContinuation(nsIPresContext* aPresContext,
                                nsIFrame*       aFrame,
                                nsIFrame*       aNextFrame,
                                nsIContent*     aContent,
                                PRInt32&        aFrameIndex,
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
  PRInt32         mArraySize;
  PRInt32*        mIndexMap;
  PRUint8*        mLevels;
  nsresult        mSuccess;

  nsCOMPtr<nsIBidi>        mBidiEngine;
  nsCOMPtr<nsIUBidiUtils>  mUnicodeUtils;
};

#endif /* nsBidiPresUtils_h___ */

#endif // IBMBIDI
