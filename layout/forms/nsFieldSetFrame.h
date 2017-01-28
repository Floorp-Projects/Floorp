/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsFieldSetFrame_h___
#define nsFieldSetFrame_h___

#include "mozilla/Attributes.h"
#include "imgIContainer.h"
#include "nsContainerFrame.h"

class nsFieldSetFrame final : public nsContainerFrame
{
  typedef mozilla::image::DrawResult DrawResult;

public:
  NS_DECL_FRAMEARENA_HELPERS

  explicit nsFieldSetFrame(nsStyleContext* aContext);

  nscoord
    GetIntrinsicISize(nsRenderingContext* aRenderingContext,
                      nsLayoutUtils::IntrinsicISizeType);
  virtual nscoord GetMinISize(nsRenderingContext* aRenderingContext) override;
  virtual nscoord GetPrefISize(nsRenderingContext* aRenderingContext) override;
  virtual nscoord GetLogicalBaseline(mozilla::WritingMode aWritingMode) const override;

  /**
   * The area to paint box-shadows around.  It's the border rect except
   * when there's a <legend> we offset the y-position to the center of it.
   */
  virtual nsRect VisualBorderRectRelativeToSelf() const override;

  virtual void Reflow(nsPresContext*           aPresContext,
                      ReflowOutput&     aDesiredSize,
                      const ReflowInput& aReflowInput,
                      nsReflowStatus&          aStatus) override;
                               
  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) override;

  DrawResult PaintBorder(nsDisplayListBuilder* aBuilder,
                         nsRenderingContext& aRenderingContext,
                         nsPoint aPt, const nsRect& aDirtyRect);

#ifdef DEBUG
  virtual void SetInitialChildList(ChildListID    aListID,
                                   nsFrameList&   aChildList) override;
  virtual void AppendFrames(ChildListID    aListID,
                            nsFrameList&   aFrameList) override;
  virtual void InsertFrames(ChildListID    aListID,
                            nsIFrame*      aPrevFrame,
                            nsFrameList&   aFrameList) override;
  virtual void RemoveFrame(ChildListID    aListID,
                           nsIFrame*      aOldFrame) override;
#endif

  virtual nsIAtom* GetType() const override;
  virtual bool IsFrameOfType(uint32_t aFlags) const override
  {
    return nsContainerFrame::IsFrameOfType(aFlags &
             ~nsIFrame::eCanContainOverflowContainers);
  }
  virtual nsIScrollableFrame* GetScrollTargetFrame() override
  {
    return do_QueryFrame(GetInner());
  }

#ifdef ACCESSIBILITY  
  virtual mozilla::a11y::AccType AccessibleType() override;
#endif

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override {
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
  mozilla::LogicalRect mLegendRect;
  nscoord   mLegendSpace;
};

#endif // nsFieldSetFrame_h___
