/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for CSS "display: grid | inline-grid" */

#ifndef nsGridContainerFrame_h___
#define nsGridContainerFrame_h___

#include "mozilla/Maybe.h"
#include "mozilla/HashTable.h"
#include "nsContainerFrame.h"

namespace mozilla {
class PresShell;
}  // namespace mozilla

/**
 * Factory function.
 * @return a newly allocated nsGridContainerFrame (infallible)
 */
nsContainerFrame* NS_NewGridContainerFrame(mozilla::PresShell* aPresShell,
                                           mozilla::ComputedStyle* aStyle);

namespace mozilla {

// Forward-declare typedefs for grid item iterator helper-class:
template <typename Iterator>
class CSSOrderAwareFrameIteratorT;
typedef CSSOrderAwareFrameIteratorT<nsFrameList::iterator>
    CSSOrderAwareFrameIterator;
typedef CSSOrderAwareFrameIteratorT<nsFrameList::reverse_iterator>
    ReverseCSSOrderAwareFrameIterator;

/**
 * The number of implicit / explicit tracks and their sizes.
 */
struct ComputedGridTrackInfo {
  ComputedGridTrackInfo(
      uint32_t aNumLeadingImplicitTracks, uint32_t aNumExplicitTracks,
      uint32_t aStartFragmentTrack, uint32_t aEndFragmentTrack,
      nsTArray<nscoord>&& aPositions, nsTArray<nscoord>&& aSizes,
      nsTArray<uint32_t>&& aStates, nsTArray<bool>&& aRemovedRepeatTracks,
      uint32_t aRepeatFirstTrack,
      nsTArray<nsTArray<StyleCustomIdent>>&& aResolvedLineNames,
      bool aIsSubgrid, bool aIsMasonry)
      : mNumLeadingImplicitTracks(aNumLeadingImplicitTracks),
        mNumExplicitTracks(aNumExplicitTracks),
        mStartFragmentTrack(aStartFragmentTrack),
        mEndFragmentTrack(aEndFragmentTrack),
        mPositions(std::move(aPositions)),
        mSizes(std::move(aSizes)),
        mStates(std::move(aStates)),
        mRemovedRepeatTracks(std::move(aRemovedRepeatTracks)),
        mResolvedLineNames(std::move(aResolvedLineNames)),
        mRepeatFirstTrack(aRepeatFirstTrack),
        mIsSubgrid(aIsSubgrid),
        mIsMasonry(aIsMasonry) {}
  uint32_t mNumLeadingImplicitTracks;
  uint32_t mNumExplicitTracks;
  uint32_t mStartFragmentTrack;
  uint32_t mEndFragmentTrack;
  nsTArray<nscoord> mPositions;
  nsTArray<nscoord> mSizes;
  nsTArray<uint32_t> mStates;
  // Indicates if a track has been collapsed. This will be populated for each
  // track in the repeat(auto-fit) and repeat(auto-fill), even if there are no
  // collapsed tracks.
  nsTArray<bool> mRemovedRepeatTracks;
  // Contains lists of all line name lists, including the name lists inside
  // repeats. When a repeat(auto) track exists, the internal track names will
  // appear once each in this array.
  nsTArray<nsTArray<StyleCustomIdent>> mResolvedLineNames;
  uint32_t mRepeatFirstTrack;
  bool mIsSubgrid;
  bool mIsMasonry;
};

struct ComputedGridLineInfo {
  explicit ComputedGridLineInfo(
      nsTArray<nsTArray<RefPtr<nsAtom>>>&& aNames,
      const nsTArray<RefPtr<nsAtom>>& aNamesBefore,
      const nsTArray<RefPtr<nsAtom>>& aNamesAfter,
      nsTArray<RefPtr<nsAtom>>&& aNamesFollowingRepeat)
      : mNames(std::move(aNames)),
        mNamesBefore(aNamesBefore.Clone()),
        mNamesAfter(aNamesAfter.Clone()),
        mNamesFollowingRepeat(std::move(aNamesFollowingRepeat)) {}
  nsTArray<nsTArray<RefPtr<nsAtom>>> mNames;
  nsTArray<RefPtr<nsAtom>> mNamesBefore;
  nsTArray<RefPtr<nsAtom>> mNamesAfter;
  nsTArray<RefPtr<nsAtom>> mNamesFollowingRepeat;
};
}  // namespace mozilla

class nsGridContainerFrame final : public nsContainerFrame {
 public:
  NS_DECL_FRAMEARENA_HELPERS(nsGridContainerFrame)
  NS_DECL_QUERYFRAME
  using ComputedGridTrackInfo = mozilla::ComputedGridTrackInfo;
  using ComputedGridLineInfo = mozilla::ComputedGridLineInfo;
  using LogicalAxis = mozilla::LogicalAxis;
  using BaselineSharingGroup = mozilla::BaselineSharingGroup;
  using NamedArea = mozilla::StyleNamedArea;

  template <typename T>
  using PerBaseline = mozilla::EnumeratedArray<BaselineSharingGroup,
                                               BaselineSharingGroup(2), T>;

  template <typename T>
  using PerLogicalAxis =
      mozilla::EnumeratedArray<LogicalAxis, LogicalAxis(2), T>;

  // nsIFrame overrides
  void Reflow(nsPresContext* aPresContext, ReflowOutput& aDesiredSize,
              const ReflowInput& aReflowInput,
              nsReflowStatus& aStatus) override;
  void Init(nsIContent* aContent, nsContainerFrame* aParent,
            nsIFrame* aPrevInFlow) override;
  void DidSetComputedStyle(ComputedStyle* aOldStyle) override;
  nscoord GetMinISize(gfxContext* aRenderingContext) override;
  nscoord GetPrefISize(gfxContext* aRenderingContext) override;
  void MarkIntrinsicISizesDirty() override;
  bool IsFrameOfType(uint32_t aFlags) const override {
    return nsContainerFrame::IsFrameOfType(
        aFlags & ~nsIFrame::eCanContainOverflowContainers);
  }

  void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                        const nsDisplayListSet& aLists) override;

  nscoord GetLogicalBaseline(mozilla::WritingMode aWM) const override {
    if (HasAnyStateBits(NS_STATE_GRID_SYNTHESIZE_BASELINE)) {
      // Return a baseline synthesized from our margin-box.
      return nsContainerFrame::GetLogicalBaseline(aWM);
    }
    nscoord b;
    GetBBaseline(BaselineSharingGroup::First, &b);
    return b;
  }

  bool GetVerticalAlignBaseline(mozilla::WritingMode aWM,
                                nscoord* aBaseline) const override {
    return GetNaturalBaselineBOffset(aWM, BaselineSharingGroup::First,
                                     aBaseline);
  }

  bool GetNaturalBaselineBOffset(mozilla::WritingMode aWM,
                                 BaselineSharingGroup aBaselineGroup,
                                 nscoord* aBaseline) const override {
    if (HasAnyStateBits(NS_STATE_GRID_SYNTHESIZE_BASELINE)) {
      return false;
    }
    return GetBBaseline(aBaselineGroup, aBaseline);
  }

#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const override;
  void ExtraContainerFrameInfo(nsACString& aTo) const override;
#endif

  // nsContainerFrame overrides
  bool DrainSelfOverflowList() override;
  void AppendFrames(ChildListID aListID, nsFrameList& aFrameList) override;
  void InsertFrames(ChildListID aListID, nsIFrame* aPrevFrame,
                    const nsLineList::iterator* aPrevFrameLine,
                    nsFrameList& aFrameList) override;
  void RemoveFrame(ChildListID aListID, nsIFrame* aOldFrame) override;
  mozilla::StyleAlignFlags CSSAlignmentForAbsPosChild(
      const ReflowInput& aChildRI, LogicalAxis aLogicalAxis) const override;

#ifdef DEBUG
  void SetInitialChildList(ChildListID aListID,
                           nsFrameList& aChildList) override;
#endif

  /**
   * Return the containing block for aChild which MUST be an abs.pos. child
   * of a grid container and that container must have been reflowed.
   */
  static const nsRect& GridItemCB(nsIFrame* aChild);

  NS_DECLARE_FRAME_PROPERTY_DELETABLE(GridItemContainingBlockRect, nsRect)

  /**
   * These properties are created by a call to
   * nsGridContainerFrame::GetGridFrameWithComputedInfo, typically from
   * Element::GetGridFragments.
   */
  NS_DECLARE_FRAME_PROPERTY_DELETABLE(GridColTrackInfo, ComputedGridTrackInfo)
  const ComputedGridTrackInfo* GetComputedTemplateColumns() {
    const ComputedGridTrackInfo* info = GetProperty(GridColTrackInfo());
    MOZ_ASSERT(info, "Property generation wasn't requested.");
    return info;
  }

  NS_DECLARE_FRAME_PROPERTY_DELETABLE(GridRowTrackInfo, ComputedGridTrackInfo)
  const ComputedGridTrackInfo* GetComputedTemplateRows() {
    const ComputedGridTrackInfo* info = GetProperty(GridRowTrackInfo());
    MOZ_ASSERT(info, "Property generation wasn't requested.");
    return info;
  }

  NS_DECLARE_FRAME_PROPERTY_DELETABLE(GridColumnLineInfo, ComputedGridLineInfo)
  const ComputedGridLineInfo* GetComputedTemplateColumnLines() {
    const ComputedGridLineInfo* info = GetProperty(GridColumnLineInfo());
    MOZ_ASSERT(info, "Property generation wasn't requested.");
    return info;
  }

  NS_DECLARE_FRAME_PROPERTY_DELETABLE(GridRowLineInfo, ComputedGridLineInfo)
  const ComputedGridLineInfo* GetComputedTemplateRowLines() {
    const ComputedGridLineInfo* info = GetProperty(GridRowLineInfo());
    MOZ_ASSERT(info, "Property generation wasn't requested.");
    return info;
  }

  struct AtomKey {
    RefPtr<nsAtom> mKey;

    explicit AtomKey(nsAtom* aAtom) : mKey(aAtom) {}

    using Lookup = nsAtom*;

    static mozilla::HashNumber hash(const Lookup& aKey) { return aKey->hash(); }

    static bool match(const AtomKey& aFirst, const Lookup& aSecond) {
      return aFirst.mKey == aSecond;
    }
  };

  using ImplicitNamedAreas = mozilla::HashMap<AtomKey, NamedArea, AtomKey>;
  NS_DECLARE_FRAME_PROPERTY_DELETABLE(ImplicitNamedAreasProperty,
                                      ImplicitNamedAreas)
  ImplicitNamedAreas* GetImplicitNamedAreas() const {
    return GetProperty(ImplicitNamedAreasProperty());
  }

  using ExplicitNamedAreas = mozilla::StyleOwnedSlice<NamedArea>;
  NS_DECLARE_FRAME_PROPERTY_DELETABLE(ExplicitNamedAreasProperty,
                                      ExplicitNamedAreas)
  ExplicitNamedAreas* GetExplicitNamedAreas() const {
    return GetProperty(ExplicitNamedAreasProperty());
  }

  using nsContainerFrame::IsMasonry;

  /** Return true if this frame has masonry layout in any axis. */
  bool IsMasonry() const {
    return HasAnyStateBits(NS_STATE_GRID_IS_ROW_MASONRY |
                           NS_STATE_GRID_IS_COL_MASONRY);
  }

  /** Return true if this frame is subgridded in its aAxis. */
  bool IsSubgrid(LogicalAxis aAxis) const {
    return HasAnyStateBits(aAxis == mozilla::eLogicalAxisBlock
                               ? NS_STATE_GRID_IS_ROW_SUBGRID
                               : NS_STATE_GRID_IS_COL_SUBGRID);
  }
  bool IsColSubgrid() const { return IsSubgrid(mozilla::eLogicalAxisInline); }
  bool IsRowSubgrid() const { return IsSubgrid(mozilla::eLogicalAxisBlock); }
  /** Return true if this frame is subgridded in any axis. */
  bool IsSubgrid() const {
    return HasAnyStateBits(NS_STATE_GRID_IS_ROW_SUBGRID |
                           NS_STATE_GRID_IS_COL_SUBGRID);
  }

  /** Return true if this frame has an item that is subgridded in our aAxis. */
  bool HasSubgridItems(LogicalAxis aAxis) const {
    return HasAnyStateBits(aAxis == mozilla::eLogicalAxisBlock
                               ? NS_STATE_GRID_HAS_ROW_SUBGRID_ITEM
                               : NS_STATE_GRID_HAS_COL_SUBGRID_ITEM);
  }
  /** Return true if this frame has any subgrid items. */
  bool HasSubgridItems() const {
    return HasAnyStateBits(NS_STATE_GRID_HAS_ROW_SUBGRID_ITEM |
                           NS_STATE_GRID_HAS_COL_SUBGRID_ITEM);
  }

  /**
   * Return a container grid frame for the supplied frame, if available.
   * @return nullptr if aFrame has no grid container.
   */
  static nsGridContainerFrame* GetGridContainerFrame(nsIFrame* aFrame);

  /**
   * Return a container grid frame, and ensure it has computed grid info
   * @return nullptr if aFrame has no grid container, or frame was destroyed
   * @note this might destroy layout/style data since it may flush layout
   */
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  static nsGridContainerFrame* GetGridFrameWithComputedInfo(nsIFrame* aFrame);

  struct Subgrid;
  struct UsedTrackSizes;
  struct TrackSize;
  struct GridItemInfo;
  struct GridReflowInput;
  struct FindItemInGridOrderResult {
    // The first(last) item in (reverse) grid order.
    const GridItemInfo* mItem;
    // Does the above item span the first(last) track?
    bool mIsInEdgeTrack;
  };

  /** Return our parent grid container; |this| MUST be a subgrid. */
  nsGridContainerFrame* ParentGridContainerForSubgrid() const;

  // https://drafts.csswg.org/css-sizing/#constraints
  enum class SizingConstraint {
    MinContent,   // sizing under min-content constraint
    MaxContent,   // sizing under max-content constraint
    NoConstraint  // no constraint, used during Reflow
  };

 protected:
  typedef mozilla::LogicalPoint LogicalPoint;
  typedef mozilla::LogicalRect LogicalRect;
  typedef mozilla::LogicalSize LogicalSize;
  typedef mozilla::CSSOrderAwareFrameIterator CSSOrderAwareFrameIterator;
  typedef mozilla::ReverseCSSOrderAwareFrameIterator
      ReverseCSSOrderAwareFrameIterator;
  typedef mozilla::WritingMode WritingMode;
  typedef mozilla::layout::AutoFrameListPtr AutoFrameListPtr;
  typedef nsLayoutUtils::IntrinsicISizeType IntrinsicISizeType;
  struct Grid;
  struct GridArea;
  class LineNameMap;
  struct LineRange;
  struct SharedGridData;
  struct SubgridFallbackTrackSizingFunctions;
  struct TrackSizingFunctions;
  struct Tracks;
  struct TranslatedLineRange;
  friend nsContainerFrame* NS_NewGridContainerFrame(
      mozilla::PresShell* aPresShell, ComputedStyle* aStyle);
  explicit nsGridContainerFrame(ComputedStyle* aStyle,
                                nsPresContext* aPresContext)
      : nsContainerFrame(aStyle, aPresContext, kClassID),
        mCachedMinISize(NS_INTRINSIC_ISIZE_UNKNOWN),
        mCachedPrefISize(NS_INTRINSIC_ISIZE_UNKNOWN) {
    for (auto& perAxisBaseline : mBaseline) {
      for (auto& baseline : perAxisBaseline) {
        baseline = NS_INTRINSIC_ISIZE_UNKNOWN;
      }
    }
  }

  /**
   * XXX temporary - move the ImplicitNamedAreas stuff to the style system.
   * The implicit area names that come from x-start .. x-end lines in
   * grid-template-columns / grid-template-rows are stored in this frame
   * property when needed, as a ImplicitNamedAreas* value.
   */
  void InitImplicitNamedAreas(const nsStylePosition* aStyle);

  using LineNameList =
      const mozilla::StyleOwnedSlice<mozilla::StyleCustomIdent>;
  void AddImplicitNamedAreas(mozilla::Span<LineNameList>);

  void NormalizeChildLists();

  /**
   * Reflow and place our children.
   * @return the consumed size of all of this grid container's continuations
   *         so far including this frame
   */
  nscoord ReflowChildren(GridReflowInput& aState,
                         const LogicalRect& aContentArea,
                         const nsSize& aContainerSize,
                         ReflowOutput& aDesiredSize, nsReflowStatus& aStatus);

  /**
   * Helper for GetMinISize / GetPrefISize.
   */
  nscoord IntrinsicISize(gfxContext* aRenderingContext,
                         IntrinsicISizeType aConstraint);

  // Helper for AppendFrames / InsertFrames.
  void NoteNewChildren(ChildListID aListID, const nsFrameList& aFrameList);

  bool GetBBaseline(BaselineSharingGroup aBaselineGroup,
                    nscoord* aResult) const {
    *aResult = mBaseline[mozilla::eLogicalAxisBlock][aBaselineGroup];
    return true;
  }
  bool GetIBaseline(BaselineSharingGroup aBaselineGroup,
                    nscoord* aResult) const {
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
    eNone = 0x0,
    eFirst = 0x1,
    eLast = 0x2,
    eBoth = eFirst | eLast,
  };
  void CalculateBaselines(BaselineSet aBaselineSet,
                          CSSOrderAwareFrameIterator* aIter,
                          const nsTArray<GridItemInfo>* aGridItems,
                          const Tracks& aTracks, uint32_t aFragmentStartTrack,
                          uint32_t aFirstExcludedTrack, WritingMode aWM,
                          const nsSize& aCBPhysicalSize,
                          nscoord aCBBorderPaddingStart,
                          nscoord aCBBorderPaddingStartEnd, nscoord aCBSize);

  /**
   * Synthesize a Grid container baseline for aGroup.
   */
  nscoord SynthesizeBaseline(const FindItemInGridOrderResult& aItem,
                             LogicalAxis aAxis, BaselineSharingGroup aGroup,
                             const nsSize& aCBPhysicalSize, nscoord aCBSize,
                             WritingMode aCBWM);
  /**
   * Find the first item in Grid Order in this fragment.
   * https://drafts.csswg.org/css-grid/#grid-order
   * @param aFragmentStartTrack is the first track in this fragment in the same
   * axis as aMajor.  Pass zero if that's not the axis we're fragmenting in.
   */
  static FindItemInGridOrderResult FindFirstItemInGridOrder(
      CSSOrderAwareFrameIterator& aIter,
      const nsTArray<GridItemInfo>& aGridItems, LineRange GridArea::*aMajor,
      LineRange GridArea::*aMinor, uint32_t aFragmentStartTrack);
  /**
   * Find the last item in Grid Order in this fragment.
   * @param aFragmentStartTrack is the first track in this fragment in the same
   * axis as aMajor.  Pass zero if that's not the axis we're fragmenting in.
   * @param aFirstExcludedTrack should be the first track in the next fragment
   * or one beyond the final track in the last fragment, in aMajor's axis.
   * Pass the number of tracks if that's not the axis we're fragmenting in.
   */
  static FindItemInGridOrderResult FindLastItemInGridOrder(
      ReverseCSSOrderAwareFrameIterator& aIter,
      const nsTArray<GridItemInfo>& aGridItems, LineRange GridArea::*aMajor,
      LineRange GridArea::*aMinor, uint32_t aFragmentStartTrack,
      uint32_t aFirstExcludedTrack);

#ifdef DEBUG
  void SanityCheckGridItemsBeforeReflow() const;
#endif  // DEBUG

  /**
   * Update our NS_STATE_GRID_IS_COL/ROW_SUBGRID bits and related subgrid state
   * on our entire continuation chain based on the current style.
   * This is needed because grid-template-columns/rows style changes only
   * trigger a reflow so we need to update this dynamically.
   */
  void UpdateSubgridFrameState();

  /**
   * Return the NS_STATE_GRID_IS_COL/ROW_SUBGRID and
   * NS_STATE_GRID_IS_ROW/COL_MASONRY bits we ought to have.
   */
  nsFrameState ComputeSelfSubgridMasonryBits() const;

  /** Helper for ComputeSelfSubgridMasonryBits(). */
  bool WillHaveAtLeastOneTrackInAxis(LogicalAxis aAxis) const;

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

  mozilla::Maybe<nsGridContainerFrame::Fragmentainer> GetNearestFragmentainer(
      const GridReflowInput& aState) const;

  // @return the consumed size of all continuations so far including this frame
  nscoord ReflowInFragmentainer(GridReflowInput& aState,
                                const LogicalRect& aContentArea,
                                ReflowOutput& aDesiredSize,
                                nsReflowStatus& aStatus,
                                Fragmentainer& aFragmentainer,
                                const nsSize& aContainerSize);

  // Helper for ReflowInFragmentainer
  // @return the consumed size of all continuations so far including this frame
  nscoord ReflowRowsInFragmentainer(
      GridReflowInput& aState, const LogicalRect& aContentArea,
      ReflowOutput& aDesiredSize, nsReflowStatus& aStatus,
      Fragmentainer& aFragmentainer, const nsSize& aContainerSize,
      const nsTArray<const GridItemInfo*>& aItems, uint32_t aStartRow,
      uint32_t aEndRow, nscoord aBSize, nscoord aAvailableSize);

  // Helper for ReflowChildren / ReflowInFragmentainer
  void ReflowInFlowChild(nsIFrame* aChild, const GridItemInfo* aGridItemInfo,
                         nsSize aContainerSize,
                         const mozilla::Maybe<nscoord>& aStretchBSize,
                         const Fragmentainer* aFragmentainer,
                         const GridReflowInput& aState,
                         const LogicalRect& aContentArea,
                         ReflowOutput& aDesiredSize, nsReflowStatus& aStatus);

  /**
   * Places and reflows items when we have masonry layout.
   * It handles unconstrained reflow and also fragmentation when the row axis
   * is the masonry axis.  ReflowInFragmentainer handles the case when we're
   * fragmenting and our row axis is a grid axis and it handles masonry layout
   * in the column axis in that case.
   * @return the intrinsic size in the masonry axis
   */
  nscoord MasonryLayout(GridReflowInput& aState,
                        const LogicalRect& aContentArea,
                        SizingConstraint aConstraint,
                        ReflowOutput& aDesiredSize, nsReflowStatus& aStatus,
                        Fragmentainer* aFragmentainer,
                        const nsSize& aContainerSize);

  // Return the stored UsedTrackSizes, if any.
  UsedTrackSizes* GetUsedTrackSizes() const;

  // Store the given TrackSizes in aAxis on a UsedTrackSizes frame property.
  void StoreUsedTrackSizes(LogicalAxis aAxis,
                           const nsTArray<TrackSize>& aSizes);

  /**
   * Cached values to optimize GetMinISize/GetPrefISize.
   */
  nscoord mCachedMinISize;
  nscoord mCachedPrefISize;

  // Our baselines, one per BaselineSharingGroup per axis.
  PerLogicalAxis<PerBaseline<nscoord>> mBaseline;

#ifdef DEBUG
  // If true, NS_STATE_GRID_DID_PUSH_ITEMS may be set even though all pushed
  // frames may have been removed.  This is used to suppress an assertion
  // in case RemoveFrame removed all associated child frames.
  bool mDidPushItemsBitMayLie{false};
#endif
};

#endif /* nsGridContainerFrame_h___ */
