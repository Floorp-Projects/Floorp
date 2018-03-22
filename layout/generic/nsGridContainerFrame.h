/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

// Forward-declare typedefs for grid item iterator helper-class:
template<typename Iterator> class CSSOrderAwareFrameIteratorT;
typedef CSSOrderAwareFrameIteratorT<nsFrameList::iterator>
  CSSOrderAwareFrameIterator;
typedef CSSOrderAwareFrameIteratorT<nsFrameList::reverse_iterator>
  ReverseCSSOrderAwareFrameIterator;

/**
 * The number of implicit / explicit tracks and their sizes.
 */
struct ComputedGridTrackInfo
{
  ComputedGridTrackInfo(uint32_t aNumLeadingImplicitTracks,
                        uint32_t aNumExplicitTracks,
                        uint32_t aStartFragmentTrack,
                        uint32_t aEndFragmentTrack,
                        nsTArray<nscoord>&& aPositions,
                        nsTArray<nscoord>&& aSizes,
                        nsTArray<uint32_t>&& aStates,
                        nsTArray<bool>&& aRemovedRepeatTracks,
                        uint32_t aRepeatFirstTrack)
    : mNumLeadingImplicitTracks(aNumLeadingImplicitTracks)
    , mNumExplicitTracks(aNumExplicitTracks)
    , mStartFragmentTrack(aStartFragmentTrack)
    , mEndFragmentTrack(aEndFragmentTrack)
    , mPositions(aPositions)
    , mSizes(aSizes)
    , mStates(aStates)
    , mRemovedRepeatTracks(aRemovedRepeatTracks)
    , mRepeatFirstTrack(aRepeatFirstTrack)
  {}
  uint32_t mNumLeadingImplicitTracks;
  uint32_t mNumExplicitTracks;
  uint32_t mStartFragmentTrack;
  uint32_t mEndFragmentTrack;
  nsTArray<nscoord> mPositions;
  nsTArray<nscoord> mSizes;
  nsTArray<uint32_t> mStates;
  nsTArray<bool> mRemovedRepeatTracks;
  uint32_t mRepeatFirstTrack;
};

struct ComputedGridLineInfo
{
  explicit ComputedGridLineInfo(nsTArray<nsTArray<nsString>>&& aNames,
                                const nsTArray<nsString>& aNamesBefore,
                                const nsTArray<nsString>& aNamesAfter,
                                nsTArray<nsString>&& aNamesFollowingRepeat)
    : mNames(aNames)
    , mNamesBefore(aNamesBefore)
    , mNamesAfter(aNamesAfter)
    , mNamesFollowingRepeat(aNamesFollowingRepeat)
  {}
  nsTArray<nsTArray<nsString>> mNames;
  nsTArray<nsString> mNamesBefore;
  nsTArray<nsString> mNamesAfter;
  nsTArray<nsString> mNamesFollowingRepeat;
};
} // namespace mozilla

class nsGridContainerFrame final : public nsContainerFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS(nsGridContainerFrame)
  NS_DECL_QUERYFRAME
  typedef mozilla::ComputedGridTrackInfo ComputedGridTrackInfo;
  typedef mozilla::ComputedGridLineInfo ComputedGridLineInfo;

  // nsIFrame overrides
  void Reflow(nsPresContext*           aPresContext,
              ReflowOutput&     aDesiredSize,
              const ReflowInput& aReflowInput,
              nsReflowStatus&          aStatus) override;
  nscoord GetMinISize(gfxContext* aRenderingContext) override;
  nscoord GetPrefISize(gfxContext* aRenderingContext) override;
  void MarkIntrinsicISizesDirty() override;
  bool IsFrameOfType(uint32_t aFlags) const override
  {
    return nsContainerFrame::IsFrameOfType(aFlags &
             ~nsIFrame::eCanContainOverflowContainers);
  }

  void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                        const nsDisplayListSet& aLists) override;

  nscoord GetLogicalBaseline(mozilla::WritingMode aWM) const override
  {
    if (HasAnyStateBits(NS_STATE_GRID_SYNTHESIZE_BASELINE)) {
      // Return a baseline synthesized from our margin-box.
      return nsContainerFrame::GetLogicalBaseline(aWM);
    }
    nscoord b;
    GetBBaseline(BaselineSharingGroup::eFirst, &b);
    return b;
  }

  bool GetVerticalAlignBaseline(mozilla::WritingMode aWM,
                                nscoord* aBaseline) const override
  {
    return GetNaturalBaselineBOffset(aWM, BaselineSharingGroup::eFirst, aBaseline);
  }

  bool GetNaturalBaselineBOffset(mozilla::WritingMode aWM,
                                 BaselineSharingGroup aBaselineGroup,
                                 nscoord*             aBaseline) const override
  {
    if (HasAnyStateBits(NS_STATE_GRID_SYNTHESIZE_BASELINE)) {
      return false;
    }
    return GetBBaseline(aBaselineGroup, aBaseline);
  }

#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const override;
#endif

  // nsContainerFrame overrides
  bool DrainSelfOverflowList() override;
  void AppendFrames(ChildListID aListID, nsFrameList& aFrameList) override;
  void InsertFrames(ChildListID aListID, nsIFrame* aPrevFrame,
                    nsFrameList& aFrameList) override;
  void RemoveFrame(ChildListID aListID, nsIFrame* aOldFrame) override;
  uint16_t CSSAlignmentForAbsPosChild(
            const ReflowInput& aChildRI,
            mozilla::LogicalAxis aLogicalAxis) const override;

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

  /**
   * These properties are created by a call to
   * nsGridContainerFrame::GetGridFrameWithComputedInfo, typically from
   * Element::GetGridFragments.
   */
  NS_DECLARE_FRAME_PROPERTY_DELETABLE(GridColTrackInfo, ComputedGridTrackInfo)
  const ComputedGridTrackInfo* GetComputedTemplateColumns()
  {
    const ComputedGridTrackInfo* info = GetProperty(GridColTrackInfo());
    MOZ_ASSERT(info, "Property generation wasn't requested.");
    return info;
  }

  NS_DECLARE_FRAME_PROPERTY_DELETABLE(GridRowTrackInfo, ComputedGridTrackInfo)
  const ComputedGridTrackInfo* GetComputedTemplateRows()
  {
    const ComputedGridTrackInfo* info = GetProperty(GridRowTrackInfo());
    MOZ_ASSERT(info, "Property generation wasn't requested.");
    return info;
  }

  NS_DECLARE_FRAME_PROPERTY_DELETABLE(GridColumnLineInfo, ComputedGridLineInfo)
  const ComputedGridLineInfo* GetComputedTemplateColumnLines()
  {
    const ComputedGridLineInfo* info = GetProperty(GridColumnLineInfo());
    MOZ_ASSERT(info, "Property generation wasn't requested.");
    return info;
  }

  NS_DECLARE_FRAME_PROPERTY_DELETABLE(GridRowLineInfo, ComputedGridLineInfo)
  const ComputedGridLineInfo* GetComputedTemplateRowLines()
  {
    const ComputedGridLineInfo* info = GetProperty(GridRowLineInfo());
    MOZ_ASSERT(info, "Property generation wasn't requested.");
    return info;
  }

  typedef nsBaseHashtable<nsStringHashKey,
                          mozilla::css::GridNamedArea,
                          mozilla::css::GridNamedArea> ImplicitNamedAreas;
  NS_DECLARE_FRAME_PROPERTY_DELETABLE(ImplicitNamedAreasProperty,
                                      ImplicitNamedAreas)
  ImplicitNamedAreas* GetImplicitNamedAreas() const {
    return GetProperty(ImplicitNamedAreasProperty());
  }

  typedef nsTArray<mozilla::css::GridNamedArea> ExplicitNamedAreas;
  NS_DECLARE_FRAME_PROPERTY_DELETABLE(ExplicitNamedAreasProperty,
                                      ExplicitNamedAreas)
  ExplicitNamedAreas* GetExplicitNamedAreas() const {
    return GetProperty(ExplicitNamedAreasProperty());
  }

  /**
   * Return a containing grid frame, and ensure it has computed grid info
   * @return nullptr if aFrame has no grid container, or frame was destroyed
   * @note this might destroy layout/style data since it may flush layout
   */
  static nsGridContainerFrame* GetGridFrameWithComputedInfo(nsIFrame* aFrame);

  struct TrackSize;
  struct GridItemInfo;
  struct GridReflowInput;
  struct FindItemInGridOrderResult
  {
    // The first(last) item in (reverse) grid order.
    const GridItemInfo* mItem;
    // Does the above item span the first(last) track?
    bool mIsInEdgeTrack;
  };
protected:
  static const uint32_t kAutoLine;
  // The maximum line number, in the zero-based translated grid.
  static const uint32_t kTranslatedMaxLine;
  typedef mozilla::LogicalPoint LogicalPoint;
  typedef mozilla::LogicalRect LogicalRect;
  typedef mozilla::LogicalSize LogicalSize;
  typedef mozilla::CSSOrderAwareFrameIterator CSSOrderAwareFrameIterator;
  typedef mozilla::ReverseCSSOrderAwareFrameIterator
    ReverseCSSOrderAwareFrameIterator;
  typedef mozilla::WritingMode WritingMode;
  typedef mozilla::css::GridNamedArea GridNamedArea;
  typedef mozilla::layout::AutoFrameListPtr AutoFrameListPtr;
  typedef nsLayoutUtils::IntrinsicISizeType IntrinsicISizeType;
  struct Grid;
  struct GridArea;
  class LineNameMap;
  struct LineRange;
  struct SharedGridData;
  struct TrackSizingFunctions;
  struct Tracks;
  struct TranslatedLineRange;
  friend nsContainerFrame* NS_NewGridContainerFrame(nsIPresShell* aPresShell,
                                                    nsStyleContext* aContext);
  explicit nsGridContainerFrame(nsStyleContext* aContext)
    : nsContainerFrame(aContext, kClassID)
    , mCachedMinISize(NS_INTRINSIC_WIDTH_UNKNOWN)
    , mCachedPrefISize(NS_INTRINSIC_WIDTH_UNKNOWN)
  {
    mBaseline[0][0] = NS_INTRINSIC_WIDTH_UNKNOWN;
    mBaseline[0][1] = NS_INTRINSIC_WIDTH_UNKNOWN;
    mBaseline[1][0] = NS_INTRINSIC_WIDTH_UNKNOWN;
    mBaseline[1][1] = NS_INTRINSIC_WIDTH_UNKNOWN;
  }

  /**
   * XXX temporary - move the ImplicitNamedAreas stuff to the style system.
   * The implicit area names that come from x-start .. x-end lines in
   * grid-template-columns / grid-template-rows are stored in this frame
   * property when needed, as a ImplicitNamedAreas* value.
   */
  void InitImplicitNamedAreas(const nsStylePosition* aStyle);
  void AddImplicitNamedAreas(const nsTArray<nsTArray<nsString>>& aLineNameLists);

  /**
   * Reflow and place our children.
   * @return the consumed size of all of this grid container's continuations
   *         so far including this frame
   */
  nscoord ReflowChildren(GridReflowInput&     aState,
                         const LogicalRect&   aContentArea,
                         ReflowOutput& aDesiredSize,
                         nsReflowStatus&      aStatus);

  /**
   * Helper for GetMinISize / GetPrefISize.
   */
  nscoord IntrinsicISize(gfxContext*         aRenderingContext,
                         IntrinsicISizeType  aConstraint);

  // Helper for AppendFrames / InsertFrames.
  void NoteNewChildren(ChildListID aListID, const nsFrameList& aFrameList);

  // Helper to move child frames into the kOverflowList.
  void MergeSortedOverflow(nsFrameList& aList);
  // Helper to move child frames into the kExcessOverflowContainersList:.
  void MergeSortedExcessOverflowContainers(nsFrameList& aList);

  bool GetBBaseline(BaselineSharingGroup aBaselineGroup, nscoord* aResult) const
  {
    *aResult = mBaseline[mozilla::eLogicalAxisBlock][aBaselineGroup];
    return true;
  }
  bool GetIBaseline(BaselineSharingGroup aBaselineGroup, nscoord* aResult) const
  {
    *aResult = mBaseline[mozilla::eLogicalAxisInline][aBaselineGroup];
    return true;
  }

  /**
   * Calculate this grid container's baselines.
   * @param aBaselineSet which baseline(s) to derive from a baseline-group or
   * items; a baseline not included is synthesized from the border-box instead.
   * @param aFragmentStartTrack is the first track in this fragment in the same
   * axis as aMajor.  Pass zero if that's not the axis we're fragmenting in.
   * @param aFirstExcludedTrack should be the first track in the next fragment
   * or one beyond the final track in the last fragment, in aMajor's axis.
   * Pass the number of tracks if that's not the axis we're fragmenting in.
   */
  enum BaselineSet : uint32_t {
    eNone =  0x0,
    eFirst = 0x1,
    eLast  = 0x2,
    eBoth  = eFirst | eLast,
  };
  void CalculateBaselines(BaselineSet                   aBaselineSet,
                          CSSOrderAwareFrameIterator*   aIter,
                          const nsTArray<GridItemInfo>* aGridItems,
                          const Tracks&    aTracks,
                          uint32_t         aFragmentStartTrack,
                          uint32_t         aFirstExcludedTrack,
                          WritingMode      aWM,
                          const nsSize&    aCBPhysicalSize,
                          nscoord          aCBBorderPaddingStart,
                          nscoord          aCBBorderPaddingStartEnd,
                          nscoord          aCBSize);

  /**
   * Synthesize a Grid container baseline for aGroup.
   */
  nscoord SynthesizeBaseline(const FindItemInGridOrderResult& aItem,
                             mozilla::LogicalAxis aAxis,
                             BaselineSharingGroup aGroup,
                             const nsSize&        aCBPhysicalSize,
                             nscoord              aCBSize,
                             WritingMode          aCBWM);
  /**
   * Find the first item in Grid Order in this fragment.
   * https://drafts.csswg.org/css-grid/#grid-order
   * @param aFragmentStartTrack is the first track in this fragment in the same
   * axis as aMajor.  Pass zero if that's not the axis we're fragmenting in.
   */
  static FindItemInGridOrderResult
  FindFirstItemInGridOrder(CSSOrderAwareFrameIterator& aIter,
                           const nsTArray<GridItemInfo>& aGridItems,
                           LineRange GridArea::* aMajor,
                           LineRange GridArea::* aMinor,
                           uint32_t aFragmentStartTrack);
  /**
   * Find the last item in Grid Order in this fragment.
   * @param aFragmentStartTrack is the first track in this fragment in the same
   * axis as aMajor.  Pass zero if that's not the axis we're fragmenting in.
   * @param aFirstExcludedTrack should be the first track in the next fragment
   * or one beyond the final track in the last fragment, in aMajor's axis.
   * Pass the number of tracks if that's not the axis we're fragmenting in.
   */
  static FindItemInGridOrderResult
  FindLastItemInGridOrder(ReverseCSSOrderAwareFrameIterator& aIter,
                          const nsTArray<GridItemInfo>& aGridItems,
                          LineRange GridArea::* aMajor,
                          LineRange GridArea::* aMinor,
                          uint32_t aFragmentStartTrack,
                          uint32_t aFirstExcludedTrack);

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
    GetNearestFragmentainer(const GridReflowInput& aState) const;

  // @return the consumed size of all continuations so far including this frame
  nscoord ReflowInFragmentainer(GridReflowInput&     aState,
                                const LogicalRect&   aContentArea,
                                ReflowOutput& aDesiredSize,
                                nsReflowStatus&      aStatus,
                                Fragmentainer&       aFragmentainer,
                                const nsSize&        aContainerSize);

  // Helper for ReflowInFragmentainer
  // @return the consumed size of all continuations so far including this frame
  nscoord ReflowRowsInFragmentainer(GridReflowInput&     aState,
                                    const LogicalRect&   aContentArea,
                                    ReflowOutput& aDesiredSize,
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
                         const mozilla::Maybe<nscoord>& aStretchBSize,
                         const Fragmentainer*    aFragmentainer,
                         const GridReflowInput&  aState,
                         const LogicalRect&      aContentArea,
                         ReflowOutput&    aDesiredSize,
                         nsReflowStatus&         aStatus);

  /**
   * Cached values to optimize GetMinISize/GetPrefISize.
   */
  nscoord mCachedMinISize;
  nscoord mCachedPrefISize;

  // Our baselines, one per BaselineSharingGroup per axis.
  nscoord mBaseline[2/*LogicalAxis*/][2/*BaselineSharingGroup*/];

#ifdef DEBUG
  // If true, NS_STATE_GRID_DID_PUSH_ITEMS may be set even though all pushed
  // frames may have been removed.  This is used to suppress an assertion
  // in case RemoveFrame removed all associated child frames.
  bool mDidPushItemsBitMayLie { false };
#endif
};

#endif /* nsGridContainerFrame_h___ */
