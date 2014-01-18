/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsFieldSetFrame_h___
#define nsFieldSetFrame_h___

#include "nsContainerFrame.h"

class nsFieldSetFrame MOZ_FINAL : public nsContainerFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS

  nsFieldSetFrame(nsStyleContext* aContext);

  NS_HIDDEN_(nscoord)
    GetIntrinsicWidth(nsRenderingContext* aRenderingContext,
                      nsLayoutUtils::IntrinsicWidthType);
  virtual nscoord GetMinWidth(nsRenderingContext* aRenderingContext);
  virtual nscoord GetPrefWidth(nsRenderingContext* aRenderingContext);
  virtual nsSize ComputeSize(nsRenderingContext *aRenderingContext,
                             nsSize aCBSize, nscoord aAvailableWidth,
                             nsSize aMargin, nsSize aBorder, nsSize aPadding,
                             uint32_t aFlags) MOZ_OVERRIDE;
  virtual nscoord GetBaseline() const;

  /**
   * The area to paint box-shadows around.  It's the border rect except
   * when there's a <legend> we offset the y-position to the center of it.
   */
  virtual nsRect VisualBorderRectRelativeToSelf() const MOZ_OVERRIDE;

  NS_IMETHOD Reflow(nsPresContext*           aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);
                               
  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) MOZ_OVERRIDE;

  void PaintBorderBackground(nsRenderingContext& aRenderingContext,
    nsPoint aPt, const nsRect& aDirtyRect, uint32_t aBGFlags);

  NS_IMETHOD AppendFrames(ChildListID    aListID,
                          nsFrameList&   aFrameList);
  NS_IMETHOD InsertFrames(ChildListID    aListID,
                          nsIFrame*      aPrevFrame,
                          nsFrameList&   aFrameList);
  NS_IMETHOD RemoveFrame(ChildListID    aListID,
                         nsIFrame*      aOldFrame);

  virtual nsIAtom* GetType() const;
  virtual bool IsFrameOfType(uint32_t aFlags) const
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

#ifdef DEBUG
  NS_IMETHOD SetInitialChildList(ChildListID    aListID,
                                 nsFrameList&   aChildList);
#endif

#ifdef DEBUG_FRAME_DUMP
  NS_IMETHOD GetFrameName(nsAString& aResult) const {
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
