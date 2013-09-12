/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */

/* This Source Code is subject to the terms of the Mozilla Public License
 * version 2.0 (the "License"). You can obtain a copy of the License at
 * http://mozilla.org/MPL/2.0/. */

/* rendering object for CSS "display: flex" */

#ifndef nsFlexContainerFrame_h___
#define nsFlexContainerFrame_h___

#include "nsContainerFrame.h"

nsIFrame* NS_NewFlexContainerFrame(nsIPresShell* aPresShell,
                                   nsStyleContext* aContext);

typedef nsContainerFrame nsFlexContainerFrameSuper;

class FlexItem;
class FlexboxAxisTracker;
class MainAxisPositionTracker;
class SingleLineCrossAxisPositionTracker;
template <class T> class nsTArray;

class nsFlexContainerFrame : public nsFlexContainerFrameSuper {
  NS_DECL_FRAMEARENA_HELPERS
  NS_DECL_QUERYFRAME_TARGET(nsFlexContainerFrame)
  NS_DECL_QUERYFRAME

  // Factory method:
  friend nsIFrame* NS_NewFlexContainerFrame(nsIPresShell* aPresShell,
                                            nsStyleContext* aContext);

public:
  // nsIFrame overrides
  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) MOZ_OVERRIDE;

  NS_IMETHOD Reflow(nsPresContext*           aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus) MOZ_OVERRIDE;

  virtual nscoord
    GetMinWidth(nsRenderingContext* aRenderingContext) MOZ_OVERRIDE;
  virtual nscoord
    GetPrefWidth(nsRenderingContext* aRenderingContext) MOZ_OVERRIDE;

  virtual nsIAtom* GetType() const MOZ_OVERRIDE;
#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const MOZ_OVERRIDE;
#endif // DEBUG
  // Flexbox-specific public methods
  bool IsHorizontal();

protected:
  // Protected constructor & destructor
  nsFlexContainerFrame(nsStyleContext* aContext) :
    nsFlexContainerFrameSuper(aContext),
    mChildrenHaveBeenReordered(false)
  {}
  virtual ~nsFlexContainerFrame();

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


  // Returns nsresult because we might have to reflow aChildFrame (to get its
  // vertical intrinsic size in a vertical flexbox), and if that reflow fails
  // (returns a failure nsresult), we want to bail out.
  nsresult AppendFlexItemForChild(nsPresContext* aPresContext,
                                  nsIFrame* aChildFrame,
                                  const nsHTMLReflowState& aParentReflowState,
                                  const FlexboxAxisTracker& aAxisTracker,
                                  nsTArray<FlexItem>& aFlexItems);

  // Runs the "resolve the flexible lengths" algorithm, distributing
  // |aFlexContainerMainSize| among the |aItems| and freezing them.
  void ResolveFlexibleLengths(const FlexboxAxisTracker& aAxisTracker,
                              nscoord aFlexContainerMainSize,
                              nsTArray<FlexItem>& aItems);

  nsresult GenerateFlexItems(nsPresContext* aPresContext,
                             const nsHTMLReflowState& aReflowState,
                             const FlexboxAxisTracker& aAxisTracker,
                             nsTArray<FlexItem>& aItems);

  nscoord ComputeFlexContainerMainSize(const nsHTMLReflowState& aReflowState,
                                       const FlexboxAxisTracker& aAxisTracker,
                                       const nsTArray<FlexItem>& aFlexItems);

  void PositionItemInMainAxis(MainAxisPositionTracker& aMainAxisPosnTracker,
                              FlexItem& aItem);

  nsresult SizeItemInCrossAxis(nsPresContext* aPresContext,
                               const FlexboxAxisTracker& aAxisTracker,
                               nsHTMLReflowState& aChildReflowState,
                               FlexItem& aItem);

  void PositionItemInCrossAxis(
    nscoord aLineStartPosition,
    SingleLineCrossAxisPositionTracker& aLineCrossAxisPosnTracker,
    FlexItem& aItem);

  bool    mChildrenHaveBeenReordered; // Have we ever had to reorder our kids
                                      // to satisfy their 'order' values?
};

#endif /* nsFlexContainerFrame_h___ */
