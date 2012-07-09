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

#include "nsContainerFrame.h"
#include "nsGkAtoms.h"

class nsPresContext;

/**
  * ViewportFrame is the parent of a single child - the doc root frame or a scroll frame 
  * containing the doc root frame. ViewportFrame stores this child in its primary child 
  * list.
  */
class ViewportFrame : public nsContainerFrame {
public:
  NS_DECL_FRAMEARENA_HELPERS

  typedef nsContainerFrame Super;

  ViewportFrame(nsStyleContext* aContext)
    : nsContainerFrame(aContext)
  {}
  virtual ~ViewportFrame() { } // useful for debugging

  virtual void DestroyFrom(nsIFrame* aDestructRoot);

  NS_IMETHOD Init(nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIFrame*        asPrevInFlow);

  NS_IMETHOD SetInitialChildList(ChildListID     aListID,
                                 nsFrameList&    aChildList);

  NS_IMETHOD AppendFrames(ChildListID     aListID,
                          nsFrameList&    aFrameList);

  NS_IMETHOD InsertFrames(ChildListID     aListID,
                          nsIFrame*       aPrevFrame,
                          nsFrameList&    aFrameList);

  NS_IMETHOD RemoveFrame(ChildListID     aListID,
                         nsIFrame*       aOldFrame);

  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists);

  virtual nscoord GetMinWidth(nsRenderingContext *aRenderingContext);
  virtual nscoord GetPrefWidth(nsRenderingContext *aRenderingContext);
  NS_IMETHOD Reflow(nsPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::viewportFrame
   */
  virtual nsIAtom* GetType() const;

  virtual void InvalidateInternal(const nsRect& aDamageRect,
                                  nscoord aX, nscoord aY, nsIFrame* aForChild,
                                  PRUint32 aFlags);

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
#endif

private:
  virtual mozilla::layout::FrameChildListID GetAbsoluteListID() const { return kFixedList; }

protected:
  nsPoint AdjustReflowStateForScrollbars(nsHTMLReflowState* aReflowState) const;
};


#endif // nsViewportFrame_h___
