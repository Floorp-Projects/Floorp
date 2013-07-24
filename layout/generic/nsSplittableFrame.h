/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * base class for rendering objects that can be split across lines,
 * columns, or pages
 */

#ifndef nsSplittableFrame_h___
#define nsSplittableFrame_h___

#include "mozilla/Attributes.h"
#include "nsFrame.h"

// Derived class that allows splitting
class nsSplittableFrame : public nsFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS

  virtual void Init(nsIContent*      aContent,
                    nsIFrame*        aParent,
                    nsIFrame*        aPrevInFlow) MOZ_OVERRIDE;
  
  virtual nsSplittableType GetSplittableType() const MOZ_OVERRIDE;

  virtual void DestroyFrom(nsIFrame* aDestructRoot) MOZ_OVERRIDE;

  /*
   * Frame continuations can be either fluid or not:
   * Fluid continuations ("in-flows") are the result of line breaking, 
   * column breaking, or page breaking.
   * Other (non-fluid) continuations can be the result of BiDi frame splitting.
   * A "flow" is a chain of fluid continuations.
   */
  
  // Get the previous/next continuation, regardless of its type (fluid or non-fluid).
  virtual nsIFrame* GetPrevContinuation() const MOZ_OVERRIDE;
  virtual nsIFrame* GetNextContinuation() const MOZ_OVERRIDE;

  // Set a previous/next non-fluid continuation.
  NS_IMETHOD SetPrevContinuation(nsIFrame*) MOZ_OVERRIDE;
  NS_IMETHOD SetNextContinuation(nsIFrame*) MOZ_OVERRIDE;

  // Get the first/last continuation for this frame.
  virtual nsIFrame* GetFirstContinuation() const MOZ_OVERRIDE;
  virtual nsIFrame* GetLastContinuation() const MOZ_OVERRIDE;

#ifdef DEBUG
  // Can aFrame2 be reached from aFrame1 by following prev/next continuations?
  static bool IsInPrevContinuationChain(nsIFrame* aFrame1, nsIFrame* aFrame2);
  static bool IsInNextContinuationChain(nsIFrame* aFrame1, nsIFrame* aFrame2);
#endif
  
  // Get the previous/next continuation, only if it is fluid (an "in-flow").
  nsIFrame* GetPrevInFlow() const;
  nsIFrame* GetNextInFlow() const;

  virtual nsIFrame* GetPrevInFlowVirtual() const MOZ_OVERRIDE { return GetPrevInFlow(); }
  virtual nsIFrame* GetNextInFlowVirtual() const MOZ_OVERRIDE { return GetNextInFlow(); }
  
  // Set a previous/next fluid continuation.
  NS_IMETHOD  SetPrevInFlow(nsIFrame*) MOZ_OVERRIDE;
  NS_IMETHOD  SetNextInFlow(nsIFrame*) MOZ_OVERRIDE;

  // Get the first/last frame in the current flow.
  virtual nsIFrame* GetFirstInFlow() const MOZ_OVERRIDE;
  virtual nsIFrame* GetLastInFlow() const MOZ_OVERRIDE;

  // Remove the frame from the flow. Connects the frame's prev-in-flow
  // and its next-in-flow. This should only be called in frame Destroy() methods.
  static void RemoveFromFlow(nsIFrame* aFrame);

protected:
  nsSplittableFrame(nsStyleContext* aContext) : nsFrame(aContext) {}

  /**
   * Determine the height consumed by our previous-in-flows.
   *
   * @note (bz) This makes laying out a splittable frame with N in-flows
   *       O(N^2)! So, use this function with caution and minimize the number
   *       of calls to this method.
   */
  nscoord GetConsumedHeight() const;

  /**
   * Retrieve the effective computed height of this frame, which is the computed
   * height, minus the height consumed by any previous in-flows.
   */
  nscoord GetEffectiveComputedHeight(const nsHTMLReflowState& aReflowState,
                                     nscoord aConsumed = NS_INTRINSICSIZE) const;

  /**
   * Compute the final height of this frame.
   *
   * @param aReflowState Data structure passed from parent during reflow.
   * @param aReflowStatus A pointed to the reflow status for when we're finished
   *        doing reflow. this will get set appropriately if the height causes
   *        us to exceed the current available (page) height.
   * @param aContentHeight The height of content, precomputed outside of this
   *        function. The final height that is used in aMetrics will be set to
   *        either this or the available height, whichever is larger, in the
   *        case where our available height is constrained, and we overflow that
   *        available height.
   * @param aBorderPadding The margins representing the border padding for block
   *        frames. Can be 0.
   * @param aMetrics Out parameter for final height. Taken as an
   *        nsHTMLReflowMetrics object so that aMetrics can be passed in
   *        directly during reflow.
   * @param aConsumed The height already consumed by our previous-in-flows.
   */
  void ComputeFinalHeight(const nsHTMLReflowState& aReflowState,
                          nsReflowStatus*          aStatus,
                          nscoord                  aContentHeight,
                          const nsMargin&          aBorderPadding,
                          nsHTMLReflowMetrics&     aMetrics,
                          nscoord                  aConsumed);

  /**
   * @see nsIFrame::GetSkipSides()
   * @see nsIFrame::ApplySkipSides()
   */
  virtual int GetSkipSides(const nsHTMLReflowState* aReflowState = nullptr) const;

#ifdef DEBUG
  virtual void DumpBaseRegressionData(nsPresContext* aPresContext, FILE* out, int32_t aIndent) MOZ_OVERRIDE;
#endif

  nsIFrame*   mPrevContinuation;
  nsIFrame*   mNextContinuation;
};

#endif /* nsSplittableFrame_h___ */
