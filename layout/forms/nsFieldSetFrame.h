/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsFieldSetFrame_h___
#define nsFieldSetFrame_h___

#include "mozilla/Attributes.h"
#include "ImgDrawResult.h"
#include "nsContainerFrame.h"
#include "nsIScrollableFrame.h"

class nsFieldSetFrame final : public nsContainerFrame {
  typedef mozilla::image::ImgDrawResult ImgDrawResult;

 public:
  NS_DECL_FRAMEARENA_HELPERS(nsFieldSetFrame)
  NS_DECL_QUERYFRAME

  explicit nsFieldSetFrame(ComputedStyle* aStyle, nsPresContext* aPresContext);

  nscoord GetIntrinsicISize(gfxContext* aRenderingContext,
                            mozilla::IntrinsicISizeType);
  nscoord GetMinISize(gfxContext* aRenderingContext) override;
  nscoord GetPrefISize(gfxContext* aRenderingContext) override;

  /**
   * The area to paint box-shadows around.  It's the border rect except
   * when there's a <legend> we offset the y-position to the center of it.
   */
  nsRect VisualBorderRectRelativeToSelf() const override;

  void Reflow(nsPresContext* aPresContext, ReflowOutput& aDesiredSize,
              const ReflowInput& aReflowInput,
              nsReflowStatus& aStatus) override;

  nscoord SynthesizeFallbackBaseline(
      mozilla::WritingMode aWM,
      BaselineSharingGroup aBaselineGroup) const override;
  BaselineSharingGroup GetDefaultBaselineSharingGroup() const override;
  Maybe<nscoord> GetNaturalBaselineBOffset(
      mozilla::WritingMode aWM, BaselineSharingGroup aBaselineGroup,
      BaselineExportContext aExportContext) const override;

  void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                        const nsDisplayListSet& aLists) override;

  ImgDrawResult PaintBorder(nsDisplayListBuilder* aBuilder,
                            gfxContext& aRenderingContext, nsPoint aPt,
                            const nsRect& aDirtyRect);

  void SetInitialChildList(ChildListID aListID,
                           nsFrameList&& aChildList) override;
  void AppendFrames(ChildListID aListID, nsFrameList&& aFrameList) override;
  void InsertFrames(ChildListID aListID, nsIFrame* aPrevFrame,
                    const nsLineList::iterator* aPrevFrameLine,
                    nsFrameList&& aFrameList) override;
#ifdef DEBUG
  void RemoveFrame(DestroyContext&, ChildListID aListID,
                   nsIFrame* aOldFrame) override;
#endif

  nsIScrollableFrame* GetScrollTargetFrame() const override;

  // Return the block wrapper around our kids.
  void AppendDirectlyOwnedAnonBoxes(nsTArray<OwnedAnonBox>& aResult) override;

#ifdef ACCESSIBILITY
  virtual mozilla::a11y::AccType AccessibleType() override;
#endif

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(u"FieldSet"_ns, aResult);
  }
#endif

  /**
   * Return the anonymous frame that contains all descendants except the legend
   * frame.  This can be a block/grid/flex/scroll frame.  It always has
   * the pseudo type nsCSSAnonBoxes::fieldsetContent.  If it's a scroll frame,
   * the scrolled frame can be a block/grid/flex frame.
   * This may return null, for example for a fieldset that is a true overflow
   * container.
   */
  nsContainerFrame* GetInner() const;

  /**
   * Return the frame that represents the rendered legend if any.
   * https://html.spec.whatwg.org/multipage/rendering.html#rendered-legend
   */
  nsIFrame* GetLegend() const;

  /** @see mLegendSpace below */
  nscoord LegendSpace() const { return mLegendSpace; }

 protected:
  /**
   * Convenience method to create a continuation for aChild after we've
   * reflowed it and got the reflow status aStatus.
   */
  void EnsureChildContinuation(nsIFrame* aChild, const nsReflowStatus& aStatus);

  mozilla::LogicalRect mLegendRect;

  // This is how much to subtract from our inner frame's content-box block-size
  // to account for a protruding legend.  It's zero if there's no legend or
  // the legend fits entirely inside our start border.
  nscoord mLegendSpace;
};

#endif  // nsFieldSetFrame_h___
