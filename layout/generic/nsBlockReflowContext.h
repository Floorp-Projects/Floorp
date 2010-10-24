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

/* class that a parent frame uses to reflow a block frame */

#ifndef nsBlockReflowContext_h___
#define nsBlockReflowContext_h___

#include "nsIFrame.h"
#include "nsHTMLReflowMetrics.h"

class nsBlockFrame;
class nsBlockReflowState;
struct nsHTMLReflowState;
class nsLineBox;
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
                       const nsHTMLReflowState& aParentRS);
  ~nsBlockReflowContext() { }

  nsresult ReflowBlock(const nsRect&       aSpace,
                       PRBool              aApplyTopMargin,
                       nsCollapsingMargin& aPrevMargin,
                       nscoord             aClearance,
                       PRBool              aIsAdjacentWithTop,
                       nsLineBox*          aLine,
                       nsHTMLReflowState&  aReflowState,
                       nsReflowStatus&     aReflowStatus,
                       nsBlockReflowState& aState);

  PRBool PlaceBlock(const nsHTMLReflowState& aReflowState,
                    PRBool                   aForceFit,
                    nsLineBox*               aLine,
                    nsCollapsingMargin&      aBottomMarginResult /* out */,
                    nsRect&                  aInFlowBounds,
                    nsOverflowAreas&         aOverflowAreas,
                    nsReflowStatus           aReflowStatus);

  nsCollapsingMargin& GetCarriedOutBottomMargin() {
    return mMetrics.mCarriedOutBottomMargin;
  }

  nscoord GetTopMargin() const {
    return mTopMargin.get();
  }

  const nsHTMLReflowMetrics& GetMetrics() const {
    return mMetrics;
  }

  /**
   * Computes the collapsed top margin for a block whose reflow state is in aRS.
   * The computed margin is added into aMargin.
   * If aClearanceFrame is null then this is the first optimistic pass which shall assume
   * that no frames have clearance, and we clear the HasClearance on all frames encountered.
   * If non-null, this is the second pass and
   * the caller has decided aClearanceFrame needs clearance (and we will
   * therefore stop collapsing there); also, this function is responsible for marking
   * it with SetHasClearance.
   * If in the optimistic pass any frame is encountered that might possibly need
   * clearance (i.e., if we really needed the optimism assumption) then we set aMayNeedRetry
   * to true.
   * We return PR_TRUE if we changed the clearance state of any line and marked it dirty.
   */
  static PRBool ComputeCollapsedTopMargin(const nsHTMLReflowState& aRS,
                                          nsCollapsingMargin* aMargin, nsIFrame* aClearanceFrame,
                                          PRBool* aMayNeedRetry, PRBool* aIsEmpty = nsnull);

protected:
  nsPresContext* mPresContext;
  const nsHTMLReflowState& mOuterReflowState;

  nsIFrame* mFrame;
  nsRect mSpace;

  nscoord mX, mY;
  nsHTMLReflowMetrics mMetrics;
  nsCollapsingMargin mTopMargin;
};

#endif /* nsBlockReflowContext_h___ */
