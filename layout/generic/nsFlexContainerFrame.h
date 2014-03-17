/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */

/* This Source Code is subject to the terms of the Mozilla Public License
 * version 2.0 (the "License"). You can obtain a copy of the License at
 * http://mozilla.org/MPL/2.0/. */

/* rendering object for CSS "display: flex" */

#ifndef nsFlexContainerFrame_h___
#define nsFlexContainerFrame_h___

#include "nsContainerFrame.h"
#include "nsTArrayForwardDeclare.h"

nsIFrame* NS_NewFlexContainerFrame(nsIPresShell* aPresShell,
                                   nsStyleContext* aContext);

typedef nsContainerFrame nsFlexContainerFrameSuper;

class nsFlexContainerFrame : public nsFlexContainerFrameSuper {
public:
  NS_DECL_FRAMEARENA_HELPERS
  NS_DECL_QUERYFRAME_TARGET(nsFlexContainerFrame)
  NS_DECL_QUERYFRAME

  // Factory method:
  friend nsIFrame* NS_NewFlexContainerFrame(nsIPresShell* aPresShell,
                                            nsStyleContext* aContext);

  // Forward-decls of helper classes
  class FlexItem;
  class FlexLine;
  class FlexboxAxisTracker;
  class StrutInfo;

  // nsIFrame overrides
  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) MOZ_OVERRIDE;

  virtual nsresult Reflow(nsPresContext*           aPresContext,
                          nsHTMLReflowMetrics&     aDesiredSize,
                          const nsHTMLReflowState& aReflowState,
                          nsReflowStatus&          aStatus) MOZ_OVERRIDE;

  virtual nscoord
    GetMinWidth(nsRenderingContext* aRenderingContext) MOZ_OVERRIDE;
  virtual nscoord
    GetPrefWidth(nsRenderingContext* aRenderingContext) MOZ_OVERRIDE;

  virtual nsIAtom* GetType() const MOZ_OVERRIDE;
#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const MOZ_OVERRIDE;
#endif
  // Flexbox-specific public methods
  bool IsHorizontal();

protected:
  // Protected constructor & destructor
  nsFlexContainerFrame(nsStyleContext* aContext) :
    nsFlexContainerFrameSuper(aContext)
  {}
  virtual ~nsFlexContainerFrame();

  /*
   * This method does the bulk of the flex layout, implementing the algorithm
   * described at:
   *   http://dev.w3.org/csswg/css-flexbox/#layout-algorithm
   * (with a few initialization pieces happening in the caller, Reflow().
   *
   * Since this is a helper for Reflow(), this takes all the same parameters
   * as Reflow(), plus a few more parameters that Reflow() sets up for us.
   *
   * (The logic behind the division of work between Reflow and DoFlexLayout is
   * as follows: DoFlexLayout() begins at the step that we have to jump back
   * to, if we find any visibility:collapse children, and Reflow() does
   * everything before that point.)
   */
  nsresult DoFlexLayout(nsPresContext*           aPresContext,
                        nsHTMLReflowMetrics&     aDesiredSize,
                        const nsHTMLReflowState& aReflowState,
                        nsReflowStatus&          aStatus,
                        nscoord aContentBoxMainSize,
                        nscoord aAvailableHeightForContent,
                        nsTArray<StrutInfo>& aStruts,
                        const FlexboxAxisTracker& aAxisTracker);

  /**
   * Checks whether our child-frame list "mFrames" is sorted, using the given
   * IsLessThanOrEqual function, and sorts it if it's not already sorted.
   *
   * XXXdholbert Once we support pagination, we need to make this function
   * check our continuations as well (or wrap it in a function that does).
   *
   * @return true if we had to sort mFrames, false if it was already sorted.
   */
  template<bool IsLessThanOrEqual(nsIFrame*, nsIFrame*)>
  bool SortChildrenIfNeeded();

  // Protected flex-container-specific methods / member-vars
#ifdef DEBUG
  void SanityCheckAnonymousFlexItems() const;
#endif // DEBUG

  FlexItem GenerateFlexItemForChild(nsPresContext* aPresContext,
                                    nsIFrame* aChildFrame,
                                    const nsHTMLReflowState& aParentReflowState,
                                    const FlexboxAxisTracker& aAxisTracker);

  // Returns nsresult because we might have to reflow aFlexItem.Frame() (to
  // get its vertical intrinsic size in a vertical flexbox), and if that
  // reflow fails (returns a failure nsresult), we want to bail out.
  nsresult ResolveFlexItemMaxContentSizing(nsPresContext* aPresContext,
                                           FlexItem& aFlexItem,
                                           const nsHTMLReflowState& aParentReflowState,
                                           const FlexboxAxisTracker& aAxisTracker);

  nsresult GenerateFlexLines(nsPresContext* aPresContext,
                             const nsHTMLReflowState& aReflowState,
                             nscoord aContentBoxMainSize,
                             nscoord aAvailableHeightForContent,
                             const nsTArray<StrutInfo>& aStruts,
                             const FlexboxAxisTracker& aAxisTracker,
                             nsTArray<FlexLine>& aLines);

  nscoord GetMainSizeFromReflowState(const nsHTMLReflowState& aReflowState,
                                     const FlexboxAxisTracker& aAxisTracker);

  nscoord ComputeCrossSize(const nsHTMLReflowState& aReflowState,
                           const FlexboxAxisTracker& aAxisTracker,
                           nscoord aSumLineCrossSizes,
                           nscoord aAvailableHeightForContent,
                           bool* aIsDefinite,
                           nsReflowStatus& aStatus);

  nsresult SizeItemInCrossAxis(nsPresContext* aPresContext,
                               const FlexboxAxisTracker& aAxisTracker,
                               nsHTMLReflowState& aChildReflowState,
                               FlexItem& aItem);

  bool mChildrenHaveBeenReordered; // Have we ever had to reorder our kids
                                   // to satisfy their 'order' values?
};

#endif /* nsFlexContainerFrame_h___ */
