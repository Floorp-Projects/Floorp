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

#ifdef DEBUG
  virtual void DumpBaseRegressionData(nsPresContext* aPresContext, FILE* out, int32_t aIndent) MOZ_OVERRIDE;
#endif

  nsIFrame*   mPrevContinuation;
  nsIFrame*   mNextContinuation;
};

#endif /* nsSplittableFrame_h___ */
