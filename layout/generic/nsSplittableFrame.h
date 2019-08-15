/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
class nsSplittableFrame : public nsFrame {
 public:
  NS_DECL_ABSTRACT_FRAME(nsSplittableFrame)

  void Init(nsIContent* aContent, nsContainerFrame* aParent,
            nsIFrame* aPrevInFlow) override;

  void DestroyFrom(nsIFrame* aDestructRoot,
                   PostDestroyData& aPostDestroyData) override;

  /*
   * Frame continuations can be either fluid or not:
   * Fluid continuations ("in-flows") are the result of line breaking,
   * column breaking, or page breaking.
   * Other (non-fluid) continuations can be the result of BiDi frame splitting.
   * A "flow" is a chain of fluid continuations.
   */

  // Get the previous/next continuation, regardless of its type (fluid or
  // non-fluid).
  nsIFrame* GetPrevContinuation() const final;
  nsIFrame* GetNextContinuation() const final;

  // Set a previous/next non-fluid continuation.
  void SetPrevContinuation(nsIFrame*) final;
  void SetNextContinuation(nsIFrame*) final;

  // Get the first/last continuation for this frame.
  nsIFrame* FirstContinuation() const final;
  nsIFrame* LastContinuation() const final;

#ifdef DEBUG
  // Can aFrame2 be reached from aFrame1 by following prev/next continuations?
  static bool IsInPrevContinuationChain(nsIFrame* aFrame1, nsIFrame* aFrame2);
  static bool IsInNextContinuationChain(nsIFrame* aFrame1, nsIFrame* aFrame2);
#endif

  // Get the previous/next continuation, only if it is fluid (an "in-flow").
  nsIFrame* GetPrevInFlow() const final;
  nsIFrame* GetNextInFlow() const final;

  // Set a previous/next fluid continuation.
  void SetPrevInFlow(nsIFrame*) final;
  void SetNextInFlow(nsIFrame*) final;

  // Get the first/last frame in the current flow.
  nsIFrame* FirstInFlow() const final;
  nsIFrame* LastInFlow() const final;

  // Remove the frame from the flow. Connects the frame's prev-in-flow
  // and its next-in-flow. This should only be called in frame Destroy()
  // methods.
  static void RemoveFromFlow(nsIFrame* aFrame);

 protected:
  nsSplittableFrame(ComputedStyle* aStyle, nsPresContext* aPresContext,
                    ClassID aID)
      : nsFrame(aStyle, aPresContext, aID),
        mPrevContinuation(nullptr),
        mNextContinuation(nullptr) {}

  /**
   * Return the sum of the block-axis content size of our previous
   * continuations.
   *
   * @param aWM a writing-mode to determine the block-axis
   *
   * @note (bz) This makes laying out a splittable frame with N continuations
   *       O(N^2)! So, use this function with caution and minimize the number
   *       of calls to this method.
   */
  nscoord ConsumedBSize(mozilla::WritingMode aWM) const;

  /**
   * Retrieve the effective computed block size of this frame, which is the
   * computed block size, minus the block size consumed by any previous
   * continuations.
   */
  nscoord GetEffectiveComputedBSize(
      const ReflowInput& aReflowInput,
      nscoord aConsumed = NS_UNCONSTRAINEDSIZE) const;

  /**
   * @see nsIFrame::GetLogicalSkipSides()
   */
  LogicalSides GetLogicalSkipSides(
      const ReflowInput* aReflowInput = nullptr) const override;

  /**
   * A faster version of GetLogicalSkipSides() that is intended to be used
   * inside Reflow before it's known if |this| frame will be COMPLETE or not.
   * It returns a result that assumes this fragment is the last and thus
   * should apply the block-end border/padding etc (except for "true" overflow
   * containers which always skip block sides).  You're then expected to
   * recalculate the block-end side (as needed) when you know |this| frame's
   * reflow status is INCOMPLETE.
   * This method is intended for frames that breaks in the block axis.
   */
  LogicalSides PreReflowBlockLevelLogicalSkipSides() const;

  nsIFrame* mPrevContinuation;
  nsIFrame* mNextContinuation;
};

#endif /* nsSplittableFrame_h___ */
