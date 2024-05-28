/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsFirstLetterFrame_h__
#define nsFirstLetterFrame_h__

/* rendering object for CSS :first-letter pseudo-element */

#include "nsContainerFrame.h"

class nsFirstLetterFrame : public nsContainerFrame {
 public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsFirstLetterFrame)

  nsFirstLetterFrame(ComputedStyle* aStyle, nsPresContext* aPresContext,
                     ClassID aClassID)
      : nsContainerFrame(aStyle, aPresContext, aClassID) {}

  nsFirstLetterFrame(ComputedStyle* aStyle, nsPresContext* aPresContext)
      : nsContainerFrame(aStyle, aPresContext, kClassID) {}

  void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                        const nsDisplayListSet& aLists) final;

  void Init(nsIContent* aContent, nsContainerFrame* aParent,
            nsIFrame* aPrevInFlow) final;
  void SetInitialChildList(ChildListID aListID, nsFrameList&& aChildList) final;
#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const final;
#endif

  bool IsFloating() const { return HasAnyStateBits(NS_FRAME_OUT_OF_FLOW); }

  nscoord GetMinISize(gfxContext* aRenderingContext) final;
  nscoord GetPrefISize(gfxContext* aRenderingContext) final;
  void AddInlineMinISize(gfxContext* aRenderingContext,
                         InlineMinISizeData* aData) final;
  void AddInlinePrefISize(gfxContext* aRenderingContext,
                          InlinePrefISizeData* aData) final;

  SizeComputationResult ComputeSize(
      gfxContext* aRenderingContext, mozilla::WritingMode aWM,
      const mozilla::LogicalSize& aCBSize, nscoord aAvailableISize,
      const mozilla::LogicalSize& aMargin,
      const mozilla::LogicalSize& aBorderPadding,
      const mozilla::StyleSizeOverrides& aSizeOverrides,
      mozilla::ComputeSizeFlags aFlags) final;

  void Reflow(nsPresContext* aPresContext, ReflowOutput& aDesiredSize,
              const ReflowInput& aReflowInput, nsReflowStatus& aStatus) final;

  bool CanContinueTextRun() const final;
  Maybe<nscoord> GetNaturalBaselineBOffset(mozilla::WritingMode aWM,
                                           BaselineSharingGroup aBaselineGroup,
                                           BaselineExportContext) const final;
  LogicalSides GetLogicalSkipSides() const final;

  // final of nsFrame method
  nsresult GetChildFrameContainingOffset(int32_t inContentOffset, bool inHint,
                                         int32_t* outFrameContentOffset,
                                         nsIFrame** outChildFrame) final;

  nscoord GetFirstLetterBaseline() const { return mBaseline; }

  // For floating first letter frames, create a continuation for aChild and
  // place it in the correct place. aContinuation is an outparam for the
  // continuation that is created. aIsFluid determines if the continuation is
  // fluid or not.
  void CreateContinuationForFloatingParent(nsIFrame* aChild,
                                           nsIFrame** aContinuation,
                                           bool aIsFluid);

  // Create a new continuation for the first-letter frame (to hold text that
  // is not part of the first-letter range), and move any frames after aFrame
  // to it; if there are none, create a new text continuation there instead.
  // Returns the first text frame of the new continuation.
  nsTextFrame* CreateContinuationForFramesAfter(nsTextFrame* aFrame);

  // Whether to use tight glyph bounds for a floating first-letter frame,
  // or "loose" bounds based on font metrics rather than individual glyphs.
  bool UseTightBounds() const;

 protected:
  nscoord mBaseline;

  void DrainOverflowFrames(nsPresContext* aPresContext);
};

class nsFloatingFirstLetterFrame : public nsFirstLetterFrame {
 public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsFloatingFirstLetterFrame)

  nsFloatingFirstLetterFrame(ComputedStyle* aStyle, nsPresContext* aPresContext)
      : nsFirstLetterFrame(aStyle, aPresContext, kClassID) {}
};

#endif /* nsFirstLetterFrame_h__ */
