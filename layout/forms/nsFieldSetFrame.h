/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsFieldSetFrame_h___
#define nsFieldSetFrame_h___

#include "mozilla/Attributes.h"
#include "nsContainerFrame.h"

class nsFieldSetFrame MOZ_FINAL : public nsContainerFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS

  nsFieldSetFrame(nsStyleContext* aContext);

  nscoord
    GetIntrinsicISize(nsRenderingContext* aRenderingContext,
                      nsLayoutUtils::IntrinsicISizeType);
  virtual nscoord GetMinISize(nsRenderingContext* aRenderingContext) MOZ_OVERRIDE;
  virtual nscoord GetPrefISize(nsRenderingContext* aRenderingContext) MOZ_OVERRIDE;
  virtual mozilla::LogicalSize
  ComputeSize(nsRenderingContext *aRenderingContext,
              mozilla::WritingMode aWritingMode,
              const mozilla::LogicalSize& aCBSize,
              nscoord aAvailableISize,
              const mozilla::LogicalSize& aMargin,
              const mozilla::LogicalSize& aBorder,
              const mozilla::LogicalSize& aPadding,
              uint32_t aFlags) MOZ_OVERRIDE;
  virtual nscoord GetLogicalBaseline(mozilla::WritingMode aWritingMode) const MOZ_OVERRIDE;

  /**
   * The area to paint box-shadows around.  It's the border rect except
   * when there's a <legend> we offset the y-position to the center of it.
   */
  virtual nsRect VisualBorderRectRelativeToSelf() const MOZ_OVERRIDE;

  virtual void Reflow(nsPresContext*           aPresContext,
                      nsHTMLReflowMetrics&     aDesiredSize,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus&          aStatus) MOZ_OVERRIDE;
                               
  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) MOZ_OVERRIDE;

  void PaintBorderBackground(nsRenderingContext& aRenderingContext,
    nsPoint aPt, const nsRect& aDirtyRect, uint32_t aBGFlags);

#ifdef DEBUG
  virtual void SetInitialChildList(ChildListID    aListID,
                                   nsFrameList&   aChildList) MOZ_OVERRIDE;
  virtual void AppendFrames(ChildListID    aListID,
                            nsFrameList&   aFrameList) MOZ_OVERRIDE;
  virtual void InsertFrames(ChildListID    aListID,
                            nsIFrame*      aPrevFrame,
                            nsFrameList&   aFrameList) MOZ_OVERRIDE;
  virtual void RemoveFrame(ChildListID    aListID,
                           nsIFrame*      aOldFrame) MOZ_OVERRIDE;
#endif

  virtual nsIAtom* GetType() const MOZ_OVERRIDE;
  virtual bool IsFrameOfType(uint32_t aFlags) const MOZ_OVERRIDE
  {
    return nsContainerFrame::IsFrameOfType(aFlags &
             ~nsIFrame::eCanContainOverflowContainers);
  }
  virtual nsIScrollableFrame* GetScrollTargetFrame() MOZ_OVERRIDE
  {
    return do_QueryFrame(GetInner());
  }

#ifdef ACCESSIBILITY  
  virtual mozilla::a11y::AccType AccessibleType() MOZ_OVERRIDE;
#endif

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const MOZ_OVERRIDE {
    return MakeFrameName(NS_LITERAL_STRING("FieldSet"), aResult);
  }
#endif

  /**
   * Return the anonymous frame that contains all descendants except
   * the legend frame.  This is currently always a block frame with
   * pseudo nsCSSAnonBoxes::fieldsetContent -- this may change in the
   * future when we add support for CSS overflow for <fieldset>.
   */
  nsIFrame* GetInner() const;

  /**
   * Return the frame that represents the legend if any.  This may be
   * a nsLegendFrame or a nsHTMLScrollFrame with the nsLegendFrame as the
   * scrolled frame (aka content insertion frame).
   */
  nsIFrame* GetLegend() const;

protected:
  nsRect    mLegendRect;
  nscoord   mLegendSpace;
};

#endif // nsFieldSetFrame_h___
