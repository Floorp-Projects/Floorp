/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */

/* This Source Code is subject to the terms of the Mozilla Public License
 * version 2.0 (the "License"). You can obtain a copy of the License at
 * http://mozilla.org/MPL/2.0/. */

/* rendering object for CSS display: -moz-flex */

#ifndef nsFlexContainerFrame_h___
#define nsFlexContainerFrame_h___

#include "nsContainerFrame.h"
#include "nsTArray.h"
#include "mozilla/Types.h"

nsIFrame* NS_NewFlexContainerFrame(nsIPresShell* aPresShell,
                                   nsStyleContext* aContext);

typedef nsContainerFrame nsFlexContainerFrameSuper;

class FlexItem;
class FlexboxAxisTracker;
class MainAxisPositionTracker;
class SingleLineCrossAxisPositionTracker;

class nsFlexContainerFrame : public nsFlexContainerFrameSuper {
  NS_DECL_FRAMEARENA_HELPERS
  NS_DECL_QUERYFRAME_TARGET(nsFlexContainerFrame)
  NS_DECL_QUERYFRAME

  // Factory method:
  friend nsIFrame* NS_NewFlexContainerFrame(nsIPresShell* aPresShell,
                                            nsStyleContext* aContext);

public:
  // nsIFrame overrides
  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
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
    mCachedContentBoxCrossSize(nscoord_MIN),
    mCachedAscent(nscoord_MIN)
  {}
  virtual ~nsFlexContainerFrame();

  // Protected nsIFrame overrides:
  virtual void DestroyFrom(nsIFrame* aDestructRoot);

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
                               const nsHTMLReflowState& aChildReflowState,
                               FlexItem& aItem);

  void PositionItemInCrossAxis(
    nscoord aLineStartPosition,
    SingleLineCrossAxisPositionTracker& aLineCrossAxisPosnTracker,
    FlexItem& aItem);

  // Cached values from running flexbox layout algorithm, used in setting our
  // reflow metrics w/out actually reflowing all of our children, in any
  // reflows where we're not dirty:
  nscoord mCachedContentBoxCrossSize; // cross size of our content-box size
  nscoord mCachedAscent;              // our ascent, in prev. reflow.
};

#endif /* nsFlexContainerFrame_h___ */
