/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsFirstLetterFrame_h__
#define nsFirstLetterFrame_h__

/* rendering object for CSS :first-letter pseudo-element */

#include "mozilla/Attributes.h"
#include "nsContainerFrame.h"

class nsFirstLetterFrame MOZ_FINAL : public nsContainerFrame {
public:
  NS_DECL_QUERYFRAME_TARGET(nsFirstLetterFrame)
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  nsFirstLetterFrame(nsStyleContext* aContext) : nsContainerFrame(aContext) {}

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) MOZ_OVERRIDE;

  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow) MOZ_OVERRIDE;
  virtual void SetInitialChildList(ChildListID     aListID,
                                   nsFrameList&    aChildList) MOZ_OVERRIDE;
#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const MOZ_OVERRIDE;
#endif
  virtual nsIAtom* GetType() const MOZ_OVERRIDE;

  bool IsFloating() const { return GetStateBits() & NS_FRAME_OUT_OF_FLOW; }

  virtual bool IsFrameOfType(uint32_t aFlags) const MOZ_OVERRIDE
  {
    if (!IsFloating())
      aFlags = aFlags & ~(nsIFrame::eLineParticipant);
    return nsContainerFrame::IsFrameOfType(aFlags &
      ~(nsIFrame::eBidiInlineContainer));
  }

  virtual nscoord GetMinWidth(nsRenderingContext *aRenderingContext) MOZ_OVERRIDE;
  virtual nscoord GetPrefWidth(nsRenderingContext *aRenderingContext) MOZ_OVERRIDE;
  virtual void AddInlineMinWidth(nsRenderingContext *aRenderingContext,
                                 InlineMinWidthData *aData) MOZ_OVERRIDE;
  virtual void AddInlinePrefWidth(nsRenderingContext *aRenderingContext,
                                  InlinePrefWidthData *aData) MOZ_OVERRIDE;
  virtual nsSize ComputeSize(nsRenderingContext *aRenderingContext,
                             nsSize aCBSize, nscoord aAvailableWidth,
                             nsSize aMargin, nsSize aBorder, nsSize aPadding,
                             uint32_t aFlags) MOZ_OVERRIDE;
  virtual void Reflow(nsPresContext*           aPresContext,
                      nsHTMLReflowMetrics&     aDesiredSize,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus&          aStatus) MOZ_OVERRIDE;

  virtual bool CanContinueTextRun() const MOZ_OVERRIDE;
  virtual nscoord GetLogicalBaseline(mozilla::WritingMode aWritingMode) const MOZ_OVERRIDE;
  virtual LogicalSides GetLogicalSkipSides(const nsHTMLReflowState* aReflowState = nullptr) const MOZ_OVERRIDE;

//override of nsFrame method
  virtual nsresult GetChildFrameContainingOffset(int32_t inContentOffset,
                                                 bool inHint,
                                                 int32_t* outFrameContentOffset,
                                                 nsIFrame** outChildFrame) MOZ_OVERRIDE;

  nscoord GetFirstLetterBaseline() const { return mBaseline; }

  // For floating first letter frames, create a continuation for aChild and
  // place it in the correct place. aContinuation is an outparam for the
  // continuation that is created. aIsFluid determines if the continuation is
  // fluid or not.
  nsresult CreateContinuationForFloatingParent(nsPresContext* aPresContext,
                                               nsIFrame* aChild,
                                               nsIFrame** aContinuation,
                                               bool aIsFluid);

protected:
  nscoord mBaseline;

  void DrainOverflowFrames(nsPresContext* aPresContext);
};

#endif /* nsFirstLetterFrame_h__ */
