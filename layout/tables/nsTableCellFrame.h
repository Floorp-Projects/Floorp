/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsTableCellFrame_h__
#define nsTableCellFrame_h__

#include "mozilla/Attributes.h"
#include "celldata.h"
#include "nsITableCellLayout.h"
#include "nscore.h"
#include "nsContainerFrame.h"
#include "mozilla/ComputedStyle.h"
#include "nsIPercentBSizeObserver.h"
#include "nsTArray.h"
#include "nsTableRowFrame.h"
#include "mozilla/WritingModes.h"

namespace mozilla {
class PresShell;
class ScrollContainerFrame;
}  // namespace mozilla

/**
 * nsTableCellFrame
 * data structure to maintain information about a single table cell's frame
 *
 * NOTE:  frames are not ref counted.  We expose addref and release here
 * so we can change that decsion in the future.  Users of nsITableCellLayout
 * should refcount correctly as if this object is being ref counted, though
 * no actual support is under the hood.
 *
 * @author  sclark
 */
class nsTableCellFrame : public nsContainerFrame,
                         public nsITableCellLayout,
                         public nsIPercentBSizeObserver {
  friend nsTableCellFrame* NS_NewTableCellFrame(mozilla::PresShell* aPresShell,
                                                ComputedStyle* aStyle,
                                                nsTableFrame* aTableFrame);

  nsTableCellFrame(ComputedStyle* aStyle, nsTableFrame* aTableFrame)
      : nsTableCellFrame(aStyle, aTableFrame, kClassID) {}

 public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsTableCellFrame)

  mozilla::ScrollContainerFrame* GetScrollTargetFrame() const final;

  nsTableRowFrame* GetTableRowFrame() const {
    nsIFrame* parent = GetParent();
    MOZ_ASSERT(parent && parent->IsTableRowFrame());
    return static_cast<nsTableRowFrame*>(parent);
  }

  nsTableFrame* GetTableFrame() const {
    return GetTableRowFrame()->GetTableFrame();
  }

  void Init(nsIContent* aContent, nsContainerFrame* aParent,
            nsIFrame* aPrevInFlow) override;

  void Destroy(DestroyContext&) override;

#ifdef ACCESSIBILITY
  mozilla::a11y::AccType AccessibleType() override;
#endif

  nsresult AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                            int32_t aModType) override;

  /** @see nsIFrame::DidSetComputedStyle */
  void DidSetComputedStyle(ComputedStyle* aOldComputedStyle) override;

#ifdef DEBUG
  // Our anonymous block frame is the content insertion frame so these
  // methods should never be called:
  void AppendFrames(ChildListID aListID, nsFrameList&& aFrameList) override;
  void InsertFrames(ChildListID aListID, nsIFrame* aPrevFrame,
                    const nsLineList::iterator* aPrevFrameLine,
                    nsFrameList&& aFrameList) override;
  void RemoveFrame(DestroyContext&, ChildListID, nsIFrame*) override;
#endif

  nsContainerFrame* GetContentInsertionFrame() override {
    return PrincipalChildList().FirstChild()->GetContentInsertionFrame();
  }

  nsIFrame* CellContentFrame() const;

  nsMargin GetUsedMargin() const override;

  void NotifyPercentBSize(const ReflowInput& aReflowInput) override;

  bool NeedsToObserve(const ReflowInput& aReflowInput) override;

  void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                        const nsDisplayListSet& aLists) override;

  virtual void ProcessBorders(nsTableFrame* aFrame,
                              nsDisplayListBuilder* aBuilder,
                              const nsDisplayListSet& aLists);

  nscoord GetMinISize(gfxContext* aRenderingContext) override;
  nscoord GetPrefISize(gfxContext* aRenderingContext) override;
  IntrinsicSizeOffsetData IntrinsicISizeOffsets(
      nscoord aPercentageBasis = NS_UNCONSTRAINEDSIZE) override;

  void Reflow(nsPresContext* aPresContext, ReflowOutput& aDesiredSize,
              const ReflowInput& aReflowInput,
              nsReflowStatus& aStatus) override;

#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const override;
#endif

  // Align the cell's child frame within the cell.
  void BlockDirAlignChild(mozilla::WritingMode aWM, nscoord aMaxAscent,
                          mozilla::ForceAlignTopForTableCell aForceAlignTop);

  /*
   * Get the value of vertical-align adjusted for CSS 2's rules for a
   * table cell, which means the result is always
   * StyleVerticalAlignKeyword::{Top,Middle,Bottom,Baseline}.
   */
  virtual mozilla::StyleVerticalAlignKeyword GetVerticalAlign() const;

  bool HasVerticalAlignBaseline() const {
    return GetVerticalAlign() == mozilla::StyleVerticalAlignKeyword::Baseline &&
           !GetContentEmpty();
  }

  /**
   * Get the first-line baseline of the cell relative to its block-start border
   * edge, as if the cell were vertically aligned to the top of the row.
   */
  nscoord GetCellBaseline() const;

  /**
   * return the cell's specified row span. this is what was specified in the
   * content model or in the style info, and is always >= 0.
   * to get the effective row span (the actual value that applies), use
   * GetEffectiveRowSpan()
   * @see nsTableFrame::GetEffectiveRowSpan()
   */
  int32_t GetRowSpan();

  // there is no set row index because row index depends on the cell's parent
  // row only

  // Return our cell content frame.
  void AppendDirectlyOwnedAnonBoxes(nsTArray<OwnedAnonBox>& aResult) override;

  /*---------------- nsITableCellLayout methods ------------------------*/

  /**
   * return the cell's starting row index (starting at 0 for the first row).
   * for continued cell frames the row index is that of the cell's first-in-flow
   * and the column index (starting at 0 for the first column
   */
  NS_IMETHOD GetCellIndexes(int32_t& aRowIndex, int32_t& aColIndex) override;

  /** return the mapped cell's row index (starting at 0 for the first row) */
  uint32_t RowIndex() const {
    return static_cast<nsTableRowFrame*>(GetParent())->GetRowIndex();
  }

  /**
   * return the cell's specified col span. this is what was specified in the
   * content model or in the style info, and is always >= 1.
   * to get the effective col span (the actual value that applies), use
   * GetEffectiveColSpan()
   * @see nsTableFrame::GetEffectiveColSpan()
   */
  int32_t GetColSpan();

  /** return the cell's column index (starting at 0 for the first column) */
  uint32_t ColIndex() const {
    // NOTE: We copy this from previous continuations, and we don't ever have
    // dynamic updates when tables split, so our mColIndex always matches our
    // first continuation's.
    MOZ_ASSERT(static_cast<nsTableCellFrame*>(FirstContinuation())->mColIndex ==
                   mColIndex,
               "mColIndex out of sync with first continuation");
    return mColIndex;
  }

  void SetColIndex(int32_t aColIndex);

  // Get or set the available isize given to this frame during its last reflow.
  nscoord GetPriorAvailISize() const { return mPriorAvailISize; }
  void SetPriorAvailISize(nscoord aPriorAvailISize) {
    mPriorAvailISize = aPriorAvailISize;
  }

  // Get or set the desired size returned by this frame during its last reflow.
  mozilla::LogicalSize GetDesiredSize() const { return mDesiredSize; }
  void SetDesiredSize(const ReflowOutput& aDesiredSize) {
    mDesiredSize = aDesiredSize.Size(GetWritingMode());
  }

  bool GetContentEmpty() const {
    return HasAnyStateBits(NS_TABLE_CELL_CONTENT_EMPTY);
  }
  void SetContentEmpty(bool aContentEmpty) {
    AddOrRemoveStateBits(NS_TABLE_CELL_CONTENT_EMPTY, aContentEmpty);
  }

  nsTableCellFrame* GetNextCell() const {
    nsIFrame* sibling = GetNextSibling();
    MOZ_ASSERT(
        !sibling || static_cast<nsTableCellFrame*>(do_QueryFrame(sibling)),
        "How do we have a non-cell sibling?");
    return static_cast<nsTableCellFrame*>(sibling);
  }

  virtual mozilla::LogicalMargin GetBorderWidth(mozilla::WritingMode aWM) const;

  void DecorateForSelection(DrawTarget* aDrawTarget, nsPoint aPt);

  bool ComputeCustomOverflow(mozilla::OverflowAreas& aOverflowAreas) override;

  void InvalidateFrame(uint32_t aDisplayItemKey = 0,
                       bool aRebuildDisplayItems = true) override;
  void InvalidateFrameWithRect(const nsRect& aRect,
                               uint32_t aDisplayItemKey = 0,
                               bool aRebuildDisplayItems = true) override;
  void InvalidateFrameForRemoval() override { InvalidateFrameSubtree(); }

  bool ShouldPaintBordersAndBackgrounds() const;

  bool ShouldPaintBackground(nsDisplayListBuilder* aBuilder);

 protected:
  nsTableCellFrame(ComputedStyle* aStyle, nsTableFrame* aTableFrame,
                   ClassID aID);
  ~nsTableCellFrame();

  LogicalSides GetLogicalSkipSides() const override;

  /**
   * GetBorderOverflow says how far the cell's own borders extend
   * outside its own bounds.  In the separated borders model this should
   * just be zero (as it is for most frames), but in the collapsed
   * borders model (for which nsBCTableCellFrame overrides this virtual
   * method), it considers the extents of the collapsed border.
   */
  virtual nsMargin GetBorderOverflow();

  friend class nsTableRowFrame;

  // The starting column for this cell
  uint32_t mColIndex = 0;

  // The avail isize during the last reflow
  nscoord mPriorAvailISize = 0;

  // The last desired inline and block size
  mozilla::LogicalSize mDesiredSize;
};

class nsBCTableCellFrame final : public nsTableCellFrame {
 public:
  NS_DECL_FRAMEARENA_HELPERS(nsBCTableCellFrame)

  nsBCTableCellFrame(ComputedStyle* aStyle, nsTableFrame* aTableFrame);

  ~nsBCTableCellFrame();

  nsMargin GetUsedBorder() const override;

  // Get the *inner half of the border only*, in twips.
  mozilla::LogicalMargin GetBorderWidth(
      mozilla::WritingMode aWM) const override;

  // Get the *inner half of the border only*
  nscoord GetBorderWidth(mozilla::LogicalSide aSide) const;

  // Set the full (both halves) width of the border
  void SetBorderWidth(mozilla::LogicalSide aSide, nscoord aValue);

  nsMargin GetBorderOverflow() override;

#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const override;
#endif

 private:
  // These are the entire width of the border (the cell edge contains only
  // the inner half).
  nscoord mBStartBorder;
  nscoord mIEndBorder;
  nscoord mBEndBorder;
  nscoord mIStartBorder;
};

// Implemented here because that's a sane-ish way to make the includes work out.
inline nsTableCellFrame* nsTableRowFrame::GetFirstCell() const {
  nsIFrame* firstChild = mFrames.FirstChild();
  MOZ_ASSERT(
      !firstChild || static_cast<nsTableCellFrame*>(do_QueryFrame(firstChild)),
      "How do we have a non-cell child?");
  return static_cast<nsTableCellFrame*>(firstChild);
}

#endif
