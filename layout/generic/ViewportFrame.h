/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * rendering object that is the root of the frame tree, which contains
 * the document's scrollbars and contains fixed-positioned elements
 */

#ifndef mozilla_ViewportFrame_h
#define mozilla_ViewportFrame_h

#include "mozilla/Attributes.h"
#include "nsContainerFrame.h"

class nsPresContext;

namespace mozilla {

/**
  * ViewportFrame is the parent of a single child - the doc root frame or a scroll frame
  * containing the doc root frame. ViewportFrame stores this child in its primary child
  * list.
  */
class ViewportFrame : public nsContainerFrame {
public:
  NS_DECL_QUERYFRAME_TARGET(ViewportFrame)
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  explicit ViewportFrame(nsStyleContext* aContext)
    : ViewportFrame(aContext, mozilla::FrameType::Viewport)
  {}

  virtual ~ViewportFrame() { } // useful for debugging

  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow) override;

#ifdef DEBUG
  virtual void AppendFrames(ChildListID     aListID,
                            nsFrameList&    aFrameList) override;
  virtual void InsertFrames(ChildListID     aListID,
                            nsIFrame*       aPrevFrame,
                            nsFrameList&    aFrameList) override;
  virtual void RemoveFrame(ChildListID     aListID,
                           nsIFrame*       aOldFrame) override;
#endif

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) override;

  void BuildDisplayListForTopLayer(nsDisplayListBuilder* aBuilder,
                                   nsDisplayList* aList);

  virtual nscoord GetMinISize(nsRenderingContext *aRenderingContext) override;
  virtual nscoord GetPrefISize(nsRenderingContext *aRenderingContext) override;
  virtual void Reflow(nsPresContext* aPresContext,
                      ReflowOutput& aDesiredSize,
                      const ReflowInput& aReflowInput,
                      nsReflowStatus& aStatus) override;

  virtual bool ComputeCustomOverflow(nsOverflowAreas& aOverflowAreas) override;

  /**
   * Adjust aReflowInput to account for scrollbars and pres shell
   * GetScrollPositionClampingScrollPortSizeSet and
   * GetContentDocumentFixedPositionMargins adjustments.
   * @return the rect to use as containing block rect
   */
  nsRect AdjustReflowInputAsContainingBlock(ReflowInput* aReflowInput) const;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override;
#endif

protected:
  ViewportFrame(nsStyleContext* aContext, mozilla::FrameType aType)
    : nsContainerFrame(aContext, aType)
    , mView(nullptr)
  {}

  /**
   * Calculate how much room is available for fixed frames. That means
   * determining if the viewport is scrollable and whether the vertical and/or
   * horizontal scrollbars are visible.  Adjust the computed width/height and
   * available width for aReflowInput accordingly.
   * @return the current scroll position, or 0,0 if not scrollable
   */
  nsPoint AdjustReflowInputForScrollbars(ReflowInput* aReflowInput) const;

  nsView* GetViewInternal() const override { return mView; }
  void SetViewInternal(nsView* aView) override { mView = aView; }

private:
  virtual mozilla::layout::FrameChildListID GetAbsoluteListID() const override { return kFixedList; }

  nsView* mView;
};

} // namespace mozilla

#endif // mozilla_ViewportFrame_h
