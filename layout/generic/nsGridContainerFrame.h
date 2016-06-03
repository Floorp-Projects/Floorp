/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code is subject to the terms of the Mozilla Public License
 * version 2.0 (the "License"). You can obtain a copy of the License at
 * http://mozilla.org/MPL/2.0/. */

/* rendering object for CSS "display: grid | inline-grid" */

#ifndef nsGridContainerFrame_h___
#define nsGridContainerFrame_h___

#include "mozilla/Maybe.h"
#include "mozilla/TypeTraits.h"
#include "nsContainerFrame.h"
#include "nsHashKeys.h"
#include "nsTHashtable.h"

/**
 * Factory function.
 * @return a newly allocated nsGridContainerFrame (infallible)
 */
nsContainerFrame* NS_NewGridContainerFrame(nsIPresShell* aPresShell,
                                           nsStyleContext* aContext);

namespace mozilla {
/**
 * The number of implicit / explicit tracks and their sizes.
 */
struct ComputedGridTrackInfo
{
  ComputedGridTrackInfo(uint32_t aNumLeadingImplicitTracks,
                        uint32_t aNumExplicitTracks,
                        nsTArray<nscoord>&& aSizes)
    : mNumLeadingImplicitTracks(aNumLeadingImplicitTracks)
    , mNumExplicitTracks(aNumExplicitTracks)
    , mSizes(aSizes)
  {}
  uint32_t mNumLeadingImplicitTracks;
  uint32_t mNumExplicitTracks;
  nsTArray<nscoord> mSizes;
};
} // namespace mozilla

class nsGridContainerFrame final : public nsContainerFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS
  NS_DECL_QUERYFRAME_TARGET(nsGridContainerFrame)
  NS_DECL_QUERYFRAME
  typedef mozilla::ComputedGridTrackInfo ComputedGridTrackInfo;

  // nsIFrame overrides
  void Reflow(nsPresContext*           aPresContext,
              nsHTMLReflowMetrics&     aDesiredSize,
              const nsHTMLReflowState& aReflowState,
              nsReflowStatus&          aStatus) override;
  nscoord GetMinISize(nsRenderingContext* aRenderingContext) override;
  nscoord GetPrefISize(nsRenderingContext* aRenderingContext) override;
  void MarkIntrinsicISizesDirty() override;
  nsIAtom* GetType() const override;
  bool IsFrameOfType(uint32_t aFlags) const override
  {
    return nsContainerFrame::IsFrameOfType(aFlags &
             ~nsIFrame::eCanContainOverflowContainers);
  }

  void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                        const nsRect&           aDirtyRect,
                        const nsDisplayListSet& aLists) override;

#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const override;
#endif

  // nsContainerFrame overrides
  bool DrainSelfOverflowList() override;
  void AppendFrames(ChildListID aListID, nsFrameList& aFrameList) override;
  void InsertFrames(ChildListID aListID, nsIFrame* aPrevFrame,
                    nsFrameList& aFrameList) override;
  void RemoveFrame(ChildListID aListID, nsIFrame* aOldFrame) override;

#ifdef DEBUG
  void SetInitialChildList(ChildListID  aListID,
                           nsFrameList& aChildList) override;
#endif

  /**
   * Return the containing block for aChild which MUST be an abs.pos. child
   * of a grid container.  This is just a helper method for
   * nsAbsoluteContainingBlock::Reflow - it's not meant to be used elsewhere.
   */
  static const nsRect& GridItemCB(nsIFrame* aChild);

  NS_DECLARE_FRAME_PROPERTY_DELETABLE(GridItemContainingBlockRect, nsRect)

  NS_DECLARE_FRAME_PROPERTY_DELETABLE(GridColTrackInfo, ComputedGridTrackInfo)
  const ComputedGridTrackInfo* GetComputedTemplateColumns()
  {
    return Properties().Get(GridColTrackInfo());
  }

  NS_DECLARE_FRAME_PROPERTY_DELETABLE(GridRowTrackInfo, ComputedGridTrackInfo)
  const ComputedGridTrackInfo* GetComputedTemplateRows()
  {
    return Properties().Get(GridRowTrackInfo());
  }

  struct TrackSize;
  struct GridItemInfo;
protected:
  static const uint32_t kAutoLine;
  // The maximum line number, in the zero-based translated grid.
  static const uint32_t kTranslatedMaxLine;
  typedef mozilla::LogicalPoint LogicalPoint;
  typedef mozilla::LogicalRect LogicalRect;
  typedef mozilla::LogicalSize LogicalSize;
  typedef mozilla::WritingMode WritingMode;
  typedef mozilla::css::GridNamedArea GridNamedArea;
  typedef mozilla::layout::AutoFrameListPtr AutoFrameListPtr;
  typedef nsLayoutUtils::IntrinsicISizeType IntrinsicISizeType;
  struct Grid;
  struct GridArea;
  class GridItemCSSOrderIterator;
  struct GridReflowState;
  class LineNameMap;
  struct LineRange;
  struct SharedGridData;
  struct TrackSizingFunctions;
  struct Tracks;
  struct TranslatedLineRange;
  friend nsContainerFrame* NS_NewGridContainerFrame(nsIPresShell* aPresShell,
                                                    nsStyleContext* aContext);
  explicit nsGridContainerFrame(nsStyleContext* aContext)
    : nsContainerFrame(aContext)
    , mCachedMinISize(NS_INTRINSIC_WIDTH_UNKNOWN)
    , mCachedPrefISize(NS_INTRINSIC_WIDTH_UNKNOWN)
  {}

  /**
   * XXX temporary - move the ImplicitNamedAreas stuff to the style system.
   * The implicit area names that come from x-start .. x-end lines in
   * grid-template-columns / grid-template-rows are stored in this frame
   * property when needed, as a ImplicitNamedAreas* value.
   */
  typedef nsTHashtable<nsStringHashKey> ImplicitNamedAreas;
  NS_DECLARE_FRAME_PROPERTY_DELETABLE(ImplicitNamedAreasProperty,
                                      ImplicitNamedAreas)
  void InitImplicitNamedAreas(const nsStylePosition* aStyle);
  void AddImplicitNamedAreas(const nsTArray<nsTArray<nsString>>& aLineNameLists);
  ImplicitNamedAreas* GetImplicitNamedAreas() const {
    return Properties().Get(ImplicitNamedAreasProperty());
  }

  /**
   * Reflow and place our children.
   * @return the consumed size of all of this grid container's continuations
   *         so far including this frame
   */
  nscoord ReflowChildren(GridReflowState&     aState,
                         const LogicalRect&   aContentArea,
                         nsHTMLReflowMetrics& aDesiredSize,
                         nsReflowStatus&      aStatus);

  /**
   * Helper for GetMinISize / GetPrefISize.
   */
  nscoord IntrinsicISize(nsRenderingContext* aRenderingContext,
                         IntrinsicISizeType  aConstraint);

  // Helper for AppendFrames / InsertFrames.
  void NoteNewChildren(ChildListID aListID, const nsFrameList& aFrameList);

  // Helper to move child frames into the kOverflowList.
  void MergeSortedOverflow(nsFrameList& aList);
  // Helper to move child frames into the kExcessOverflowContainersList:.
  void MergeSortedExcessOverflowContainers(nsFrameList& aList);

#ifdef DEBUG
  void SanityCheckGridItemsBeforeReflow() const;
#endif // DEBUG

private:
  // Helpers for ReflowChildren
  struct Fragmentainer {
    /**
     * The distance from the first grid container fragment's block-axis content
     * edge to the fragmentainer end.
     */
    nscoord mToFragmentainerEnd;
    /**
     * True if the current fragment is at the start of the fragmentainer.
     */
    bool mIsTopOfPage;
    /**
     * Is there a Class C break opportunity at the start content edge?
     */
    bool mCanBreakAtStart;
    /**
     * Is there a Class C break opportunity at the end content edge?
     */
    bool mCanBreakAtEnd;
    /**
     * Is the grid container's block-size unconstrained?
     */
    bool mIsAutoBSize;
  };

  mozilla::Maybe<nsGridContainerFrame::Fragmentainer>
    GetNearestFragmentainer(const GridReflowState& aState) const;

  // @return the consumed size of all continuations so far including this frame
  nscoord ReflowInFragmentainer(GridReflowState&     aState,
                                const LogicalRect&   aContentArea,
                                nsHTMLReflowMetrics& aDesiredSize,
                                nsReflowStatus&      aStatus,
                                Fragmentainer&       aFragmentainer,
                                const nsSize&        aContainerSize);

  // Helper for ReflowInFragmentainer
  // @return the consumed size of all continuations so far including this frame
  nscoord ReflowRowsInFragmentainer(GridReflowState&     aState,
                                    const LogicalRect&   aContentArea,
                                    nsHTMLReflowMetrics& aDesiredSize,
                                    nsReflowStatus&      aStatus,
                                    Fragmentainer&       aFragmentainer,
                                    const nsSize&        aContainerSize,
                                    const nsTArray<const GridItemInfo*>& aItems,
                                    uint32_t             aStartRow,
                                    uint32_t             aEndRow,
                                    nscoord              aBSize,
                                    nscoord              aAvailableSize);

  // Helper for ReflowChildren / ReflowInFragmentainer
  void ReflowInFlowChild(nsIFrame*               aChild,
                         const GridItemInfo*     aGridItemInfo,
                         nsSize                  aContainerSize,
                         mozilla::Maybe<nscoord> aStretchBSize,
                         const Fragmentainer*    aFragmentainer,
                         const GridReflowState&  aState,
                         const LogicalRect&      aContentArea,
                         nsHTMLReflowMetrics&    aDesiredSize,
                         nsReflowStatus&         aStatus);

  /**
   * Cached values to optimize GetMinISize/GetPrefISize.
   */
  nscoord mCachedMinISize;
  nscoord mCachedPrefISize;

#ifdef DEBUG
  // If true, NS_STATE_GRID_DID_PUSH_ITEMS may be set even though all pushed
  // frames may have been removed.  This is used to suppress an assertion
  // in case RemoveFrame removed all associated child frames.
  bool mDidPushItemsBitMayLie;
#endif
};

#endif /* nsGridContainerFrame_h___ */
