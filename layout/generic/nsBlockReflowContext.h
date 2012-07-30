/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
                       bool                aApplyTopMargin,
                       nsCollapsingMargin& aPrevMargin,
                       nscoord             aClearance,
                       bool                aIsAdjacentWithTop,
                       nsLineBox*          aLine,
                       nsHTMLReflowState&  aReflowState,
                       nsReflowStatus&     aReflowStatus,
                       nsBlockReflowState& aState);

  bool PlaceBlock(const nsHTMLReflowState& aReflowState,
                    bool                     aForceFit,
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
   * We return true if we changed the clearance state of any line and marked it dirty.
   */
  static bool ComputeCollapsedTopMargin(const nsHTMLReflowState& aRS,
                                          nsCollapsingMargin* aMargin, nsIFrame* aClearanceFrame,
                                          bool* aMayNeedRetry, bool* aIsEmpty = nullptr);

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
