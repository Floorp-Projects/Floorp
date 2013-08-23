/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * rendering object that is the root of the frame tree, which contains
 * the document's scrollbars and contains fixed-positioned elements
 */

#ifndef nsViewportFrame_h___
#define nsViewportFrame_h___

#include "mozilla/Attributes.h"
#include "nsContainerFrame.h"

class nsPresContext;

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

  typedef nsContainerFrame Super;

  ViewportFrame(nsStyleContext* aContext)
    : nsContainerFrame(aContext)
  {}
  virtual ~ViewportFrame() { } // useful for debugging

  virtual void Init(nsIContent*      aContent,
                    nsIFrame*        aParent,
                    nsIFrame*        asPrevInFlow) MOZ_OVERRIDE;

  NS_IMETHOD SetInitialChildList(ChildListID     aListID,
                                 nsFrameList&    aChildList) MOZ_OVERRIDE;

  NS_IMETHOD AppendFrames(ChildListID     aListID,
                          nsFrameList&    aFrameList) MOZ_OVERRIDE;

  NS_IMETHOD InsertFrames(ChildListID     aListID,
                          nsIFrame*       aPrevFrame,
                          nsFrameList&    aFrameList) MOZ_OVERRIDE;

  NS_IMETHOD RemoveFrame(ChildListID     aListID,
                         nsIFrame*       aOldFrame) MOZ_OVERRIDE;

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) MOZ_OVERRIDE;

  virtual nscoord GetMinWidth(nsRenderingContext *aRenderingContext) MOZ_OVERRIDE;
  virtual nscoord GetPrefWidth(nsRenderingContext *aRenderingContext) MOZ_OVERRIDE;
  NS_IMETHOD Reflow(nsPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus) MOZ_OVERRIDE;

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::viewportFrame
   */
  virtual nsIAtom* GetType() const MOZ_OVERRIDE;

  /**
   * Adjust aReflowState to account for scrollbars and pres shell
   * GetScrollPositionClampingScrollPortSizeSet and
   * GetContentDocumentFixedPositionMargins adjustments.
   * @return the rect to use as containing block rect
   */
  nsRect AdjustReflowStateAsContainingBlock(nsHTMLReflowState* aReflowState) const;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const MOZ_OVERRIDE;
#endif

private:
  virtual mozilla::layout::FrameChildListID GetAbsoluteListID() const MOZ_OVERRIDE { return kFixedList; }

protected:
  /**
   * Calculate how much room is available for fixed frames. That means
   * determining if the viewport is scrollable and whether the vertical and/or
   * horizontal scrollbars are visible.  Adjust the computed width/height and
   * available width for aReflowState accordingly.
   * @return the current scroll position, or 0,0 if not scrollable
   */
  nsPoint AdjustReflowStateForScrollbars(nsHTMLReflowState* aReflowState) const;
};


#endif // nsViewportFrame_h___
