/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsFirstLetterFrame_h__
#define nsFirstLetterFrame_h__

/* rendering object for CSS :first-letter pseudo-element */

#include "mozilla/Attributes.h"
#include "nsContainerFrame.h"

class nsFirstLetterFrame final : public nsContainerFrame {
 public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsFirstLetterFrame)

  explicit nsFirstLetterFrame(ComputedStyle* aStyle,
                              nsPresContext* aPresContext)
      : nsContainerFrame(aStyle, aPresContext, kClassID) {}

  virtual void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                const nsDisplayListSet& aLists) override;

  virtual void Init(nsIContent* aContent, nsContainerFrame* aParent,
                    nsIFrame* aPrevInFlow) override;
  virtual void SetInitialChildList(ChildListID aListID,
                                   nsFrameList& aChildList) override;
#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override;
#endif

  bool IsFloating() const { return HasAnyStateBits(NS_FRAME_OUT_OF_FLOW); }

  virtual bool IsFrameOfType(uint32_t aFlags) const override {
    if (!IsFloating()) aFlags = aFlags & ~(nsIFrame::eLineParticipant);
    return nsContainerFrame::IsFrameOfType(aFlags &
                                           ~(nsIFrame::eBidiInlineContainer));
  }

  virtual nscoord GetMinISize(gfxContext* aRenderingContext) override;
  virtual nscoord GetPrefISize(gfxContext* aRenderingContext) override;
  virtual void AddInlineMinISize(gfxContext* aRenderingContext,
                                 InlineMinISizeData* aData) override;
  virtual void AddInlinePrefISize(gfxContext* aRenderingContext,
                                  InlinePrefISizeData* aData) override;

  SizeComputationResult ComputeSize(
      gfxContext* aRenderingContext, mozilla::WritingMode aWM,
      const mozilla::LogicalSize& aCBSize, nscoord aAvailableISize,
      const mozilla::LogicalSize& aMargin,
      const mozilla::LogicalSize& aBorderPadding,
      const mozilla::StyleSizeOverrides& aSizeOverrides,
      mozilla::ComputeSizeFlags aFlags) override;

  virtual void Reflow(nsPresContext* aPresContext, ReflowOutput& aDesiredSize,
                      const ReflowInput& aReflowInput,
                      nsReflowStatus& aStatus) override;

  virtual bool CanContinueTextRun() const override;
  virtual nscoord GetLogicalBaseline(
      mozilla::WritingMode aWritingMode) const override;
  virtual LogicalSides GetLogicalSkipSides() const override;

  // override of nsFrame method
  virtual nsresult GetChildFrameContainingOffset(
      int32_t inContentOffset, bool inHint, int32_t* outFrameContentOffset,
      nsIFrame** outChildFrame) override;

  nscoord GetFirstLetterBaseline() const { return mBaseline; }

  // For floating first letter frames, create a continuation for aChild and
  // place it in the correct place. aContinuation is an outparam for the
  // continuation that is created. aIsFluid determines if the continuation is
  // fluid or not.
  void CreateContinuationForFloatingParent(nsIFrame* aChild,
                                           nsIFrame** aContinuation,
                                           bool aIsFluid);

 protected:
  nscoord mBaseline;

  void DrainOverflowFrames(nsPresContext* aPresContext);
};

#endif /* nsFirstLetterFrame_h__ */
