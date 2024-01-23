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
#include "nsIFrame.h"

// Derived class that allows splitting
class nsSplittableFrame : public nsIFrame {
 public:
  NS_DECL_ABSTRACT_FRAME(nsSplittableFrame)
  NS_DECL_QUERYFRAME_TARGET(nsSplittableFrame)
  NS_DECL_QUERYFRAME

  void Init(nsIContent* aContent, nsContainerFrame* aParent,
            nsIFrame* aPrevInFlow) override;

  void Destroy(DestroyContext&) override;

  /*
   * Frame continuations can be either fluid or non-fluid.
   *
   * Fluid continuations ("in-flows") are the result of line breaking,
   * column breaking, or page breaking.
   *
   * Non-fluid continuations can be the result of BiDi frame splitting,
   * column-span splitting, or <col span="N"> where N > 1.
   *
   * A "flow" is a chain of fluid continuations.
   *
   * For more information, see https://wiki.mozilla.org/Gecko:Continuation_Model
   */

  // Get the previous/next continuation, regardless of its type (fluid or
  // non-fluid).
  nsIFrame* GetPrevContinuation() const final;
  nsIFrame* GetNextContinuation() const final;

  // Set a previous/next non-fluid continuation.
  void SetPrevContinuation(nsIFrame*) final;
  void SetNextContinuation(nsIFrame*) final;

  // Get the first/last continuation for this frame.
  nsIFrame* FirstContinuation() const override;
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
      : nsIFrame(aStyle, aPresContext, aID) {}

  /**
   * Return the sum of the block-axis content size of our previous
   * continuations.
   *
   * Classes that call this are _required_ to call this at least once for each
   * reflow (unless you're the first continuation, in which case you can skip
   * it, because as an optimization we don't cache it there).
   *
   * This guarantees that the internal cache works, by refreshing it. Calling it
   * multiple times in the same reflow is wasteful, but not an error.
   */
  nscoord CalcAndCacheConsumedBSize();

  /**
   * This static wrapper over CalcAndCacheConsumedBSize() is intended for a
   * specific scenario where an nsSplittableFrame's subclass needs to access
   * another subclass' consumed block-size. For ordinary use cases,
   * CalcAndCacheConsumedBSize() should be called.
   *
   * This has the same requirements as CalcAndCacheConsumedBSize(). In
   * particular, classes that call this are _required_ to call this at least
   * once for each reflow.
   */
  static nscoord ConsumedBSize(nsSplittableFrame* aFrame) {
    return aFrame->CalcAndCacheConsumedBSize();
  }

  /**
   * Retrieve the effective computed block size of this frame, which is the
   * computed block size, minus the block size consumed by any previous
   * continuations.
   */
  nscoord GetEffectiveComputedBSize(const ReflowInput& aReflowInput,
                                    nscoord aConsumed) const;

  /**
   * @see nsIFrame::GetLogicalSkipSides()
   */
  LogicalSides GetLogicalSkipSides() const override {
    return GetBlockLevelLogicalSkipSides(true);
  }

  LogicalSides GetBlockLevelLogicalSkipSides(bool aAfterReflow) const;

  /**
   * A version of GetLogicalSkipSides() that is intended to be used inside
   * Reflow before it's known if |this| frame will be COMPLETE or not.
   * It returns a result that assumes this fragment is the last and thus
   * should apply the block-end border/padding etc (except for "true" overflow
   * containers which always skip block sides).  You're then expected to
   * recalculate the block-end side (as needed) when you know |this| frame's
   * reflow status is INCOMPLETE.
   * This method is intended for frames that break in the block axis.
   */
  LogicalSides PreReflowBlockLevelLogicalSkipSides() const {
    return GetBlockLevelLogicalSkipSides(false);
  };

  nsIFrame* mPrevContinuation = nullptr;
  nsIFrame* mNextContinuation = nullptr;
};

#endif /* nsSplittableFrame_h___ */
