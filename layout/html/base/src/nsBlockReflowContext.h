/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
#ifndef nsBlockReflowContext_h___
#define nsBlockReflowContext_h___

#include "nsIFrame.h"
#include "nsHTMLReflowState.h"
#include "nsHTMLReflowMetrics.h"

class nsBlockFrame;
class nsIFrame;
class nsPresContext;
class nsLineLayout;
struct nsStylePosition;
struct nsBlockHorizontalAlign;

/**
 * An encapsulation of the state and algorithm for reflowing block frames.
 */
class nsBlockReflowContext {
public:
  nsBlockReflowContext(nsPresContext* aPresContext,
                       const nsHTMLReflowState& aParentRS,
                       PRBool aComputeMaxElementWidth,
                       PRBool aComputeMaximumWidth);
  ~nsBlockReflowContext() { }

  nsresult ReflowBlock(const nsRect&       aSpace,
                       PRBool              aApplyTopMargin,
                       nsCollapsingMargin& aPrevBottomMargin,
                       PRBool              aIsAdjacentWithTop,
                       nsMargin&           aComputedOffsets,
                       nsHTMLReflowState&  aReflowState,
                       nsReflowStatus&     aReflowStatus);

  PRBool PlaceBlock(const nsHTMLReflowState& aReflowState,
                    PRBool                   aForceFit,
                    const nsMargin&          aComputedOffsets,
                    nsCollapsingMargin&      aBottomMarginResult /* out */,
                    nsRect&                  aInFlowBounds,
                    nsRect&                  aCombinedRect);

  void AlignBlockHorizontally(nscoord aWidth, nsBlockHorizontalAlign&);

  nsCollapsingMargin& GetCarriedOutBottomMargin() {
    return mMetrics.mCarriedOutBottomMargin;
  }

  nscoord GetTopMargin() const {
    return mTopMargin.get();
  }

  const nsMargin& GetMargin() const {
    return mMargin;
  }

  const nsHTMLReflowMetrics& GetMetrics() const {
    return mMetrics;
  }

  nscoord GetMaxElementWidth() const {
    return mMetrics.mMaxElementWidth;
  }
  
  nscoord GetMaximumWidth() const {
    return mMetrics.mMaximumWidth;
  }

  static void ComputeCollapsedTopMargin(nsPresContext* aPresContext,
                                        nsHTMLReflowState& aRS,
                           /* inout */  nsCollapsingMargin& aMargin);

protected:
  nsPresContext* mPresContext;
  const nsHTMLReflowState& mOuterReflowState;

  nsIFrame* mFrame;
  nsRect mSpace;

  // Spacing style for the frame we are reflowing; only valid after reflow
  const nsStyleBorder* mStyleBorder;
  const nsStyleMargin* mStyleMargin;
  const nsStylePadding* mStylePadding;

  nscoord mComputedWidth;               // copy of reflowstate's computedWidth
  nsMargin mMargin;
  nscoord mX, mY;
  nsHTMLReflowMetrics mMetrics;
  nsCollapsingMargin mTopMargin;
  PRPackedBool mComputeMaximumWidth;
};

#endif /* nsBlockReflowContext_h___ */
