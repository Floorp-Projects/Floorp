/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Maybe.h"

#include "nsTableRowFrame.h"
#include "nsTableRowGroupFrame.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsStyleContext.h"
#include "nsStyleConsts.h"
#include "nsGkAtoms.h"
#include "nsIContent.h"
#include "nsTableFrame.h"
#include "nsTableCellFrame.h"
#include "nsCSSRendering.h"
#include "nsHTMLParts.h"
#include "nsTableColGroupFrame.h"
#include "nsTableColFrame.h"
#include "nsCOMPtr.h"
#include "nsDisplayList.h"
#include "nsIFrameInlines.h"
#include <algorithm>

using namespace mozilla;

namespace mozilla {

struct TableCellReflowInput : public ReflowInput
{
  TableCellReflowInput(nsPresContext*           aPresContext,
                         const ReflowInput& aParentReflowInput,
                         nsIFrame*                aFrame,
                         const LogicalSize&       aAvailableSpace,
                         uint32_t                 aFlags = 0)
    : ReflowInput(aPresContext, aParentReflowInput, aFrame,
                        aAvailableSpace, nullptr, aFlags)
  {
  }

  void FixUp(const LogicalSize& aAvailSpace);
};

} // namespace mozilla

void TableCellReflowInput::FixUp(const LogicalSize& aAvailSpace)
{
  // fix the mComputed values during a pass 2 reflow since the cell can be a percentage base
  NS_WARNING_ASSERTION(
    NS_UNCONSTRAINEDSIZE != aAvailSpace.ISize(mWritingMode),
    "have unconstrained inline-size; this should only result from very large "
    "sizes, not attempts at intrinsic inline size calculation");
  if (NS_UNCONSTRAINEDSIZE != ComputedISize()) {
    nscoord computedISize = aAvailSpace.ISize(mWritingMode) -
      ComputedLogicalBorderPadding().IStartEnd(mWritingMode);
    computedISize = std::max(0, computedISize);
    SetComputedISize(computedISize);
  }
  if (NS_UNCONSTRAINEDSIZE != ComputedBSize() &&
      NS_UNCONSTRAINEDSIZE != aAvailSpace.BSize(mWritingMode)) {
    nscoord computedBSize = aAvailSpace.BSize(mWritingMode) -
      ComputedLogicalBorderPadding().BStartEnd(mWritingMode);
    computedBSize = std::max(0, computedBSize);
    SetComputedBSize(computedBSize);
  }
}

void
nsTableRowFrame::InitChildReflowInput(nsPresContext&          aPresContext,
                                      const LogicalSize&      aAvailSize,
                                      bool                    aBorderCollapse,
                                      TableCellReflowInput& aReflowInput)
{
  nsMargin collapseBorder;
  nsMargin* pCollapseBorder = nullptr;
  if (aBorderCollapse) {
    // we only reflow cells, so don't need to check frame type
    nsBCTableCellFrame* bcCellFrame = (nsBCTableCellFrame*)aReflowInput.mFrame;
    if (bcCellFrame) {
      WritingMode wm = GetWritingMode();
      collapseBorder = bcCellFrame->GetBorderWidth(wm).GetPhysicalMargin(wm);
      pCollapseBorder = &collapseBorder;
    }
  }
  aReflowInput.Init(&aPresContext, nullptr, pCollapseBorder);
  aReflowInput.FixUp(aAvailSize);
}

void
nsTableRowFrame::SetFixedBSize(nscoord aValue)
{
  nscoord bsize = std::max(0, aValue);
  if (HasFixedBSize()) {
    if (bsize > mStyleFixedBSize) {
      mStyleFixedBSize = bsize;
    }
  }
  else {
    mStyleFixedBSize = bsize;
    if (bsize > 0) {
      SetHasFixedBSize(true);
    }
  }
}

void
nsTableRowFrame::SetPctBSize(float aPctValue,
                             bool  aForce)
{
  nscoord bsize = std::max(0, NSToCoordRound(aPctValue * 100.0f));
  if (HasPctBSize()) {
    if ((bsize > mStylePctBSize) || aForce) {
      mStylePctBSize = bsize;
    }
  }
  else {
    mStylePctBSize = bsize;
    if (bsize > 0) {
      SetHasPctBSize(true);
    }
  }
}

/* ----------- nsTableRowFrame ---------- */

NS_QUERYFRAME_HEAD(nsTableRowFrame)
  NS_QUERYFRAME_ENTRY(nsTableRowFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

nsTableRowFrame::nsTableRowFrame(nsStyleContext* aContext)
  : nsContainerFrame(aContext, LayoutFrameType::TableRow)
  , mContentBSize(0)
  , mStylePctBSize(0)
  , mStyleFixedBSize(0)
  , mMaxCellAscent(0)
  , mMaxCellDescent(0)
  , mBStartBorderWidth(0)
  , mBEndBorderWidth(0)
  , mIEndContBorderWidth(0)
  , mBStartContBorderWidth(0)
  , mIStartContBorderWidth(0)
{
  mBits.mRowIndex = 0;
  mBits.mHasFixedBSize = 0;
  mBits.mHasPctBSize = 0;
  mBits.mFirstInserted = 0;
  ResetBSize(0);
}

nsTableRowFrame::~nsTableRowFrame()
{
}

void
nsTableRowFrame::Init(nsIContent*       aContent,
                      nsContainerFrame* aParent,
                      nsIFrame*         aPrevInFlow)
{
  // Let the base class do its initialization
  nsContainerFrame::Init(aContent, aParent, aPrevInFlow);

  NS_ASSERTION(mozilla::StyleDisplay::TableRow == StyleDisplay()->mDisplay,
               "wrong display on table row frame");

  if (aPrevInFlow) {
    // Set the row index
    nsTableRowFrame* rowFrame = (nsTableRowFrame*)aPrevInFlow;

    SetRowIndex(rowFrame->GetRowIndex());
  } else {
    mWritingMode = GetTableFrame()->GetWritingMode();
  }
}

void
nsTableRowFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  if (HasAnyStateBits(NS_FRAME_CAN_HAVE_ABSPOS_CHILDREN)) {
    nsTableFrame::UnregisterPositionedTablePart(this, aDestructRoot);
  }

  nsContainerFrame::DestroyFrom(aDestructRoot);
}

/* virtual */ void
nsTableRowFrame::DidSetStyleContext(nsStyleContext* aOldStyleContext)
{
  nsContainerFrame::DidSetStyleContext(aOldStyleContext);

  if (!aOldStyleContext) //avoid this on init
    return;

  nsTableFrame* tableFrame = GetTableFrame();
  if (tableFrame->IsBorderCollapse() &&
      tableFrame->BCRecalcNeeded(aOldStyleContext, StyleContext())) {
    TableArea damageArea(0, GetRowIndex(), tableFrame->GetColCount(), 1);
    tableFrame->AddBCDamageArea(damageArea);
  }
}

void
nsTableRowFrame::AppendFrames(ChildListID  aListID,
                              nsFrameList& aFrameList)
{
  NS_ASSERTION(aListID == kPrincipalList, "unexpected child list");

  DrainSelfOverflowList(); // ensure the last frame is in mFrames
  const nsFrameList::Slice& newCells = mFrames.AppendFrames(nullptr, aFrameList);

  // Add the new cell frames to the table
  nsTableFrame* tableFrame = GetTableFrame();
  for (nsFrameList::Enumerator e(newCells) ; !e.AtEnd(); e.Next()) {
    nsIFrame *childFrame = e.get();
    NS_ASSERTION(IS_TABLE_CELL(childFrame->Type()),
                 "Not a table cell frame/pseudo frame construction failure");
    tableFrame->AppendCell(static_cast<nsTableCellFrame&>(*childFrame), GetRowIndex());
  }

  PresContext()->PresShell()->FrameNeedsReflow(this, nsIPresShell::eTreeChange,
                                               NS_FRAME_HAS_DIRTY_CHILDREN);
  tableFrame->SetGeometryDirty();
}


void
nsTableRowFrame::InsertFrames(ChildListID  aListID,
                              nsIFrame*    aPrevFrame,
                              nsFrameList& aFrameList)
{
  NS_ASSERTION(aListID == kPrincipalList, "unexpected child list");
  NS_ASSERTION(!aPrevFrame || aPrevFrame->GetParent() == this,
               "inserting after sibling frame with different parent");
  DrainSelfOverflowList(); // ensure aPrevFrame is in mFrames
  //Insert Frames in the frame list
  const nsFrameList::Slice& newCells = mFrames.InsertFrames(nullptr, aPrevFrame, aFrameList);

  // Get the table frame
  nsTableFrame* tableFrame = GetTableFrame();
  LayoutFrameType cellFrameType = tableFrame->IsBorderCollapse()
      ? LayoutFrameType::BCTableCell : LayoutFrameType::TableCell;
  nsTableCellFrame* prevCellFrame = (nsTableCellFrame *)nsTableFrame::GetFrameAtOrBefore(this, aPrevFrame, cellFrameType);
  nsTArray<nsTableCellFrame*> cellChildren;
  for (nsFrameList::Enumerator e(newCells); !e.AtEnd(); e.Next()) {
    nsIFrame *childFrame = e.get();
    NS_ASSERTION(IS_TABLE_CELL(childFrame->Type()),
                 "Not a table cell frame/pseudo frame construction failure");
    cellChildren.AppendElement(static_cast<nsTableCellFrame*>(childFrame));
  }
  // insert the cells into the cell map
  int32_t colIndex = -1;
  if (prevCellFrame) {
    prevCellFrame->GetColIndex(colIndex);
  }
  tableFrame->InsertCells(cellChildren, GetRowIndex(), colIndex);

  PresContext()->PresShell()->FrameNeedsReflow(this, nsIPresShell::eTreeChange,
                                               NS_FRAME_HAS_DIRTY_CHILDREN);
  tableFrame->SetGeometryDirty();
}

void
nsTableRowFrame::RemoveFrame(ChildListID aListID,
                             nsIFrame*   aOldFrame)
{
  NS_ASSERTION(aListID == kPrincipalList, "unexpected child list");

  MOZ_ASSERT((nsTableCellFrame*)do_QueryFrame(aOldFrame));
  nsTableCellFrame* cellFrame = static_cast<nsTableCellFrame*>(aOldFrame);
  // remove the cell from the cell map
  nsTableFrame* tableFrame = GetTableFrame();
  tableFrame->RemoveCell(cellFrame, GetRowIndex());

  // Remove the frame and destroy it
  mFrames.DestroyFrame(aOldFrame);

  PresContext()->PresShell()->
    FrameNeedsReflow(this, nsIPresShell::eTreeChange,
                     NS_FRAME_HAS_DIRTY_CHILDREN);

  tableFrame->SetGeometryDirty();
}

/* virtual */ nsMargin
nsTableRowFrame::GetUsedMargin() const
{
  return nsMargin(0,0,0,0);
}

/* virtual */ nsMargin
nsTableRowFrame::GetUsedBorder() const
{
  return nsMargin(0,0,0,0);
}

/* virtual */ nsMargin
nsTableRowFrame::GetUsedPadding() const
{
  return nsMargin(0,0,0,0);
}

nscoord
GetBSizeOfRowsSpannedBelowFirst(nsTableCellFrame& aTableCellFrame,
                                nsTableFrame&     aTableFrame,
                                const WritingMode aWM)
{
  nscoord bsize = 0;
  int32_t rowSpan = aTableFrame.GetEffectiveRowSpan(aTableCellFrame);
  // add in bsize of rows spanned beyond the 1st one
  nsIFrame* nextRow = aTableCellFrame.GetParent()->GetNextSibling();
  for (int32_t rowX = 1; ((rowX < rowSpan) && nextRow);) {
    if (nextRow->IsTableRowFrame()) {
      bsize += nextRow->BSize(aWM);
      rowX++;
    }
    bsize += aTableFrame.GetRowSpacing(rowX);
    nextRow = nextRow->GetNextSibling();
  }
  return bsize;
}

nsTableCellFrame*
nsTableRowFrame::GetFirstCell()
{
  for (nsIFrame* childFrame : mFrames) {
    nsTableCellFrame *cellFrame = do_QueryFrame(childFrame);
    if (cellFrame) {
      return cellFrame;
    }
  }
  return nullptr;
}

/**
 * Post-reflow hook. This is where the table row does its post-processing
 */
void
nsTableRowFrame::DidResize()
{
  // Resize and re-align the cell frames based on our row bsize
  nsTableFrame* tableFrame = GetTableFrame();

  WritingMode wm = GetWritingMode();
  ReflowOutput desiredSize(wm);
  desiredSize.SetSize(wm, GetLogicalSize(wm));
  desiredSize.SetOverflowAreasToDesiredBounds();

  nsSize containerSize = mRect.Size();

  for (nsIFrame* childFrame : mFrames) {
    nsTableCellFrame *cellFrame = do_QueryFrame(childFrame);
    if (cellFrame) {
      nscoord cellBSize = BSize(wm) +
        GetBSizeOfRowsSpannedBelowFirst(*cellFrame, *tableFrame, wm);

      // If the bsize for the cell has changed, we need to reset it;
      // and in vertical-rl mode, we need to update the cell's block position
      // to account for the containerSize, which may not have been known
      // earlier, so we always apply it here.
      LogicalSize cellSize = cellFrame->GetLogicalSize(wm);
      if (cellSize.BSize(wm) != cellBSize || wm.IsVerticalRL()) {
        nsRect cellOldRect = cellFrame->GetRect();
        nsRect cellVisualOverflow = cellFrame->GetVisualOverflowRect();

        if (wm.IsVerticalRL()) {
          // Get the old position of the cell, as we want to preserve its
          // inline coordinate.
          LogicalPoint oldPos =
            cellFrame->GetLogicalPosition(wm, containerSize);

          // The cell should normally be aligned with the row's block-start,
          // so set the B component of the position to zero:
          LogicalPoint newPos(wm, oldPos.I(wm), 0);

          // ...unless relative positioning is in effect, in which case the
          // cell may have been moved away from the row's block-start
          if (cellFrame->IsRelativelyPositioned()) {
            // Find out where the cell would have been without relative
            // positioning.
            LogicalPoint oldNormalPos =
              cellFrame->GetLogicalNormalPosition(wm, containerSize);
            // The difference (if any) between oldPos and oldNormalPos reflects
            // relative positioning that was applied to the cell, and which we
            // need to incorporate when resetting the position.
            newPos.B(wm) = oldPos.B(wm) - oldNormalPos.B(wm);
          }

          if (oldPos != newPos) {
            cellFrame->SetPosition(wm, newPos, containerSize);
            nsTableFrame::RePositionViews(cellFrame);
          }
        }

        cellSize.BSize(wm) = cellBSize;
        cellFrame->SetSize(wm, cellSize);
        nsTableFrame::InvalidateTableFrame(cellFrame, cellOldRect,
                                           cellVisualOverflow,
                                           false);
      }

      // realign cell content based on the new bsize.  We might be able to
      // skip this if the bsize didn't change... maybe.  Hard to tell.
      cellFrame->BlockDirAlignChild(wm, mMaxCellAscent);

      // Always store the overflow, even if the height didn't change, since
      // we'll lose part of our overflow area otherwise.
      ConsiderChildOverflow(desiredSize.mOverflowAreas, cellFrame);

      // Note that if the cell's *content* needs to change in response
      // to this height, it will get a special bsize reflow.
    }
  }
  FinishAndStoreOverflow(&desiredSize);
  if (HasView()) {
    nsContainerFrame::SyncFrameViewAfterReflow(PresContext(), this, GetView(),
                                               desiredSize.VisualOverflow(), 0);
  }
  // Let our base class do the usual work
}

// returns max-ascent amongst all cells that have 'vertical-align: baseline'
// *including* cells with rowspans
nscoord nsTableRowFrame::GetMaxCellAscent() const
{
  return mMaxCellAscent;
}

nscoord nsTableRowFrame::GetRowBaseline(WritingMode aWM)
{
  if (mMaxCellAscent) {
    return mMaxCellAscent;
  }

  // If we don't have a baseline on any of the cells we go for the lowest
  // content edge of the inner block frames.
  // Every table cell has a cell frame with its border and padding. Inside
  // the cell is a block frame. The cell is as high as the tallest cell in
  // the parent row. As a consequence the block frame might not touch both
  // the top and the bottom padding of it parent cell frame at the same time.
  //
  // bbbbbbbbbbbbbbbbbb             cell border:  b
  // bppppppppppppppppb             cell padding: p
  // bpxxxxxxxxxxxxxxpb             inner block:  x
  // bpx            xpb
  // bpx            xpb
  // bpx            xpb
  // bpxxxxxxxxxxxxxxpb  base line
  // bp              pb
  // bp              pb
  // bppppppppppppppppb
  // bbbbbbbbbbbbbbbbbb

  nscoord ascent = 0;
  nsSize containerSize = GetSize();
  for (nsIFrame* childFrame : mFrames) {
    if (IS_TABLE_CELL(childFrame->Type())) {
      nsIFrame* firstKid = childFrame->PrincipalChildList().FirstChild();
      ascent = std::max(ascent,
                        LogicalRect(aWM, firstKid->GetNormalRect(),
                                    containerSize).BEnd(aWM));
    }
  }
  return ascent;
}

nscoord
nsTableRowFrame::GetInitialBSize(nscoord aPctBasis) const
{
  nscoord bsize = 0;
  if ((aPctBasis > 0) && HasPctBSize()) {
    bsize = NSToCoordRound(GetPctBSize() * (float)aPctBasis);
  }
  if (HasFixedBSize()) {
    bsize = std::max(bsize, GetFixedBSize());
  }
  return std::max(bsize, GetContentBSize());
}

void
nsTableRowFrame::ResetBSize(nscoord aFixedBSize)
{
  SetHasFixedBSize(false);
  SetHasPctBSize(false);
  SetFixedBSize(0);
  SetPctBSize(0);
  SetContentBSize(0);

  if (aFixedBSize > 0) {
    SetFixedBSize(aFixedBSize);
  }

  mMaxCellAscent = 0;
  mMaxCellDescent = 0;
}

void
nsTableRowFrame::UpdateBSize(nscoord           aBSize,
                             nscoord           aAscent,
                             nscoord           aDescent,
                             nsTableFrame*     aTableFrame,
                             nsTableCellFrame* aCellFrame)
{
  if (!aTableFrame || !aCellFrame) {
    NS_ASSERTION(false , "invalid call");
    return;
  }

  if (aBSize != NS_UNCONSTRAINEDSIZE) {
    if (!(aCellFrame->HasVerticalAlignBaseline())) { // only the cell's height matters
      if (GetInitialBSize() < aBSize) {
        int32_t rowSpan = aTableFrame->GetEffectiveRowSpan(*aCellFrame);
        if (rowSpan == 1) {
          SetContentBSize(aBSize);
        }
      }
    }
    else { // the alignment on the baseline can change the bsize
      NS_ASSERTION((aAscent != NS_UNCONSTRAINEDSIZE) &&
                   (aDescent != NS_UNCONSTRAINEDSIZE), "invalid call");
      // see if this is a long ascender
      if (mMaxCellAscent < aAscent) {
        mMaxCellAscent = aAscent;
      }
      // see if this is a long descender and without rowspan
      if (mMaxCellDescent < aDescent) {
        int32_t rowSpan = aTableFrame->GetEffectiveRowSpan(*aCellFrame);
        if (rowSpan == 1) {
          mMaxCellDescent = aDescent;
        }
      }
      // keep the tallest bsize in sync
      if (GetInitialBSize() < mMaxCellAscent + mMaxCellDescent) {
        SetContentBSize(mMaxCellAscent + mMaxCellDescent);
      }
    }
  }
}

nscoord
nsTableRowFrame::CalcBSize(const ReflowInput& aReflowInput)
{
  nsTableFrame* tableFrame = GetTableFrame();
  nscoord computedBSize = (NS_UNCONSTRAINEDSIZE == aReflowInput.ComputedBSize())
                            ? 0 : aReflowInput.ComputedBSize();
  ResetBSize(computedBSize);

  WritingMode wm = aReflowInput.GetWritingMode();
  const nsStylePosition* position = StylePosition();
  const nsStyleCoord& bsizeStyleCoord = position->BSize(wm);
  if (bsizeStyleCoord.ConvertsToLength()) {
    SetFixedBSize(nsRuleNode::ComputeCoordPercentCalc(bsizeStyleCoord, 0));
  }
  else if (eStyleUnit_Percent == bsizeStyleCoord.GetUnit()) {
    SetPctBSize(bsizeStyleCoord.GetPercentValue());
  }
  // calc() with percentages is treated like 'auto' on table rows.

  for (nsIFrame* kidFrame : mFrames) {
    nsTableCellFrame *cellFrame = do_QueryFrame(kidFrame);
    if (cellFrame) {
      MOZ_ASSERT(cellFrame->GetWritingMode() == wm);
      LogicalSize desSize = cellFrame->GetDesiredSize();
      if ((NS_UNCONSTRAINEDSIZE == aReflowInput.AvailableBSize()) && !GetPrevInFlow()) {
        CalculateCellActualBSize(cellFrame, desSize.BSize(wm), wm);
      }
      // bsize may have changed, adjust descent to absorb any excess difference
      nscoord ascent;
       if (!kidFrame->PrincipalChildList().FirstChild()->PrincipalChildList().FirstChild())
         ascent = desSize.BSize(wm);
       else
         ascent = cellFrame->GetCellBaseline();
       nscoord descent = desSize.BSize(wm) - ascent;
       UpdateBSize(desSize.BSize(wm), ascent, descent, tableFrame, cellFrame);
    }
  }
  return GetInitialBSize();
}

/**
 * We need a custom display item for table row backgrounds. This is only used
 * when the table row is the root of a stacking context (e.g., has 'opacity').
 * Table row backgrounds can extend beyond the row frame bounds, when
 * the row contains row-spanning cells.
 */
class nsDisplayTableRowBackground : public nsDisplayTableItem {
public:
  nsDisplayTableRowBackground(nsDisplayListBuilder* aBuilder,
                              nsTableRowFrame*      aFrame) :
    nsDisplayTableItem(aBuilder, aFrame) {
    MOZ_COUNT_CTOR(nsDisplayTableRowBackground);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayTableRowBackground() {
    MOZ_COUNT_DTOR(nsDisplayTableRowBackground);
  }
#endif

  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     nsRenderingContext*   aCtx) override;
  NS_DISPLAY_DECL_NAME("TableRowBackground", TYPE_TABLE_ROW_BACKGROUND)
};

void
nsDisplayTableRowBackground::Paint(nsDisplayListBuilder* aBuilder,
                                   nsRenderingContext*   aCtx)
{
  auto rowFrame = static_cast<nsTableRowFrame*>(mFrame);
  TableBackgroundPainter painter(rowFrame->GetTableFrame(),
                                 TableBackgroundPainter::eOrigin_TableRow,
                                 mFrame->PresContext(), *aCtx,
                                 mVisibleRect, ToReferenceFrame(),
                                 aBuilder->GetBackgroundPaintFlags());

  DrawResult result = painter.PaintRow(rowFrame);
  nsDisplayTableItemGeometry::UpdateDrawResult(this, result);
}

void
nsTableRowFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                  const nsRect&           aDirtyRect,
                                  const nsDisplayListSet& aLists)
{
  nsDisplayTableItem* item = nullptr;
  if (IsVisibleInSelection(aBuilder)) {
    bool isRoot = aBuilder->IsAtRootOfPseudoStackingContext();
    if (isRoot) {
      // This background is created regardless of whether this frame is
      // visible or not. Visibility decisions are delegated to the
      // table background painter.
      // We would use nsDisplayGeneric for this rare case except that we
      // need the background to be larger than the row frame in some
      // cases.
      item = new (aBuilder) nsDisplayTableRowBackground(aBuilder, this);
      aLists.BorderBackground()->AppendNewToTop(item);
    }
  }
  nsTableFrame::DisplayGenericTablePart(aBuilder, this, aDirtyRect, aLists, item);
}

nsIFrame::LogicalSides
nsTableRowFrame::GetLogicalSkipSides(const ReflowInput* aReflowInput) const
{
  if (MOZ_UNLIKELY(StyleBorder()->mBoxDecorationBreak ==
                     StyleBoxDecorationBreak::Clone)) {
    return LogicalSides();
  }

  LogicalSides skip;
  if (nullptr != GetPrevInFlow()) {
    skip |= eLogicalSideBitsBStart;
  }
  if (nullptr != GetNextInFlow()) {
    skip |= eLogicalSideBitsBEnd;
  }
  return skip;
}

// Calculate the cell's actual bsize given its pass2 bsize.
// Takes into account the specified bsize (in the style).
// Modifies the desired bsize that is passed in.
nsresult
nsTableRowFrame::CalculateCellActualBSize(nsTableCellFrame* aCellFrame,
                                          nscoord&          aDesiredBSize,
                                          WritingMode       aWM)
{
  nscoord specifiedBSize = 0;

  // Get the bsize specified in the style information
  const nsStylePosition* position = aCellFrame->StylePosition();

  int32_t rowSpan = GetTableFrame()->GetEffectiveRowSpan(*aCellFrame);

  const nsStyleCoord& bsizeStyleCoord = position->BSize(aWM);
  switch (bsizeStyleCoord.GetUnit()) {
    case eStyleUnit_Calc: {
      if (bsizeStyleCoord.CalcHasPercent()) {
        // Treat this like "auto"
        break;
      }
      // Fall through to the coord case
      MOZ_FALLTHROUGH;
    }
    case eStyleUnit_Coord: {
      // In quirks mode, table cell isize should be content-box, but bsize
      // should be border-box.
      // Because of this historic anomaly, we do not use quirk.css
      // (since we can't specify one value of box-sizing for isize and another
      // for bsize)
      specifiedBSize = nsRuleNode::ComputeCoordPercentCalc(bsizeStyleCoord, 0);
      if (PresContext()->CompatibilityMode() != eCompatibility_NavQuirks &&
          position->mBoxSizing == StyleBoxSizing::Content) {
        specifiedBSize +=
          aCellFrame->GetLogicalUsedBorderAndPadding(aWM).BStartEnd(aWM);
      }

      if (1 == rowSpan) {
        SetFixedBSize(specifiedBSize);
      }
      break;
    }
    case eStyleUnit_Percent: {
      if (1 == rowSpan) {
        SetPctBSize(bsizeStyleCoord.GetPercentValue());
      }
      // pct bsizes are handled when all of the cells are finished,
      // so don't set specifiedBSize
      break;
    }
    case eStyleUnit_Auto:
    default:
      break;
  }

  // If the specified bsize is greater than the desired bsize,
  // then use the specified bsize
  if (specifiedBSize > aDesiredBSize) {
    aDesiredBSize = specifiedBSize;
  }

  return NS_OK;
}

// Calculates the available isize for the table cell based on the known
// column isizes taking into account column spans and column spacing
static nscoord
CalcAvailISize(nsTableFrame&     aTableFrame,
               nsTableCellFrame& aCellFrame)
{
  nscoord cellAvailISize = 0;
  int32_t colIndex;
  aCellFrame.GetColIndex(colIndex);
  int32_t colspan = aTableFrame.GetEffectiveColSpan(aCellFrame);
  NS_ASSERTION(colspan > 0, "effective colspan should be positive");
  nsTableFrame* fifTable =
    static_cast<nsTableFrame*>(aTableFrame.FirstInFlow());

  for (int32_t spanX = 0; spanX < colspan; spanX++) {
    cellAvailISize +=
      fifTable->GetColumnISizeFromFirstInFlow(colIndex + spanX);
    if (spanX > 0 &&
        aTableFrame.ColumnHasCellSpacingBefore(colIndex + spanX)) {
      cellAvailISize += aTableFrame.GetColSpacing(colIndex + spanX - 1);
    }
  }
  return cellAvailISize;
}

nscoord
GetSpaceBetween(int32_t       aPrevColIndex,
                int32_t       aColIndex,
                int32_t       aColSpan,
                nsTableFrame& aTableFrame,
                bool          aCheckVisibility)
{
  nscoord space = 0;
  int32_t colIdx;
  nsTableFrame* fifTable =
    static_cast<nsTableFrame*>(aTableFrame.FirstInFlow());
  for (colIdx = aPrevColIndex + 1; aColIndex > colIdx; colIdx++) {
    bool isCollapsed = false;
    if (!aCheckVisibility) {
      space += fifTable->GetColumnISizeFromFirstInFlow(colIdx);
    }
    else {
      nsTableColFrame* colFrame = aTableFrame.GetColFrame(colIdx);
      const nsStyleVisibility* colVis = colFrame->StyleVisibility();
      bool collapseCol = (NS_STYLE_VISIBILITY_COLLAPSE == colVis->mVisible);
      nsIFrame* cgFrame = colFrame->GetParent();
      const nsStyleVisibility* groupVis = cgFrame->StyleVisibility();
      bool collapseGroup = (NS_STYLE_VISIBILITY_COLLAPSE ==
                              groupVis->mVisible);
      isCollapsed = collapseCol || collapseGroup;
      if (!isCollapsed)
        space += fifTable->GetColumnISizeFromFirstInFlow(colIdx);
    }
    if (!isCollapsed && aTableFrame.ColumnHasCellSpacingBefore(colIdx)) {
      space += aTableFrame.GetColSpacing(colIdx - 1);
    }
  }
  return space;
}

// subtract the bsizes of aRow's prev in flows from the unpaginated bsize
static
nscoord CalcBSizeFromUnpaginatedBSize(nsTableRowFrame& aRow,
                                      WritingMode      aWM)
{
  nscoord bsize = 0;
  nsTableRowFrame* firstInFlow =
    static_cast<nsTableRowFrame*>(aRow.FirstInFlow());
  if (firstInFlow->HasUnpaginatedBSize()) {
    bsize = firstInFlow->GetUnpaginatedBSize();
    for (nsIFrame* prevInFlow = aRow.GetPrevInFlow(); prevInFlow;
         prevInFlow = prevInFlow->GetPrevInFlow()) {
      bsize -= prevInFlow->BSize(aWM);
    }
  }
  return std::max(bsize, 0);
}

void
nsTableRowFrame::ReflowChildren(nsPresContext*           aPresContext,
                                ReflowOutput&     aDesiredSize,
                                const ReflowInput& aReflowInput,
                                nsTableFrame&            aTableFrame,
                                nsReflowStatus&          aStatus)
{
  aStatus.Reset();

  // XXXldb Should we be checking constrained bsize instead?
  const bool isPaginated = aPresContext->IsPaginated();
  const bool borderCollapse = aTableFrame.IsBorderCollapse();

  int32_t cellColSpan = 1;  // must be defined here so it's set properly for non-cell kids

  // remember the col index of the previous cell to handle rowspans into this row
  int32_t prevColIndex = -1;
  nscoord iCoord = 0; // running total of children inline-coord offset

  // This computes the max of all cell bsizes
  nscoord cellMaxBSize = 0;

  // Reflow each of our existing cell frames
  WritingMode wm = aReflowInput.GetWritingMode();
  nsSize containerSize =
    aReflowInput.ComputedSizeAsContainerIfConstrained();

  for (nsIFrame* kidFrame : mFrames) {
    nsTableCellFrame *cellFrame = do_QueryFrame(kidFrame);
    if (!cellFrame) {
      // XXXldb nsCSSFrameConstructor needs to enforce this!
      NS_NOTREACHED("yikes, a non-row child");

      // it's an unknown frame type, give it a generic reflow and ignore the results
      TableCellReflowInput
        kidReflowInput(aPresContext, aReflowInput, kidFrame,
                       LogicalSize(kidFrame->GetWritingMode(), 0, 0),
                       ReflowInput::CALLER_WILL_INIT);
      InitChildReflowInput(*aPresContext, LogicalSize(wm), false, kidReflowInput);
      ReflowOutput desiredSize(aReflowInput);
      nsReflowStatus  status;
      ReflowChild(kidFrame, aPresContext, desiredSize, kidReflowInput, 0, 0, 0, status);
      kidFrame->DidReflow(aPresContext, nullptr, nsDidReflowStatus::FINISHED);

      continue;
    }

    // See if we should only reflow the dirty child frames
    bool doReflowChild = true;
    if (!aReflowInput.ShouldReflowAllKids() &&
        !aTableFrame.IsGeometryDirty() &&
        !NS_SUBTREE_DIRTY(kidFrame)) {
      if (!aReflowInput.mFlags.mSpecialBSizeReflow)
        doReflowChild = false;
    }
    else if ((NS_UNCONSTRAINEDSIZE != aReflowInput.AvailableBSize())) {
      // We don't reflow a rowspan >1 cell here with a constrained bsize.
      // That happens in nsTableRowGroupFrame::SplitSpanningCells.
      if (aTableFrame.GetEffectiveRowSpan(*cellFrame) > 1) {
        doReflowChild = false;
      }
    }
    if (aReflowInput.mFlags.mSpecialBSizeReflow) {
      if (!isPaginated &&
          !cellFrame->HasAnyStateBits(NS_FRAME_CONTAINS_RELATIVE_BSIZE)) {
        continue;
      }
    }

    int32_t cellColIndex;
    cellFrame->GetColIndex(cellColIndex);
    cellColSpan = aTableFrame.GetEffectiveColSpan(*cellFrame);

    // If the adjacent cell is in a prior row (because of a rowspan) add in the space
    if (prevColIndex != (cellColIndex - 1)) {
      iCoord += GetSpaceBetween(prevColIndex, cellColIndex, cellColSpan, aTableFrame,
                                false);
    }

    // remember the rightmost (ltr) or leftmost (rtl) column this cell spans into
    prevColIndex = cellColIndex + (cellColSpan - 1);

    // Reflow the child frame
    nsRect kidRect = kidFrame->GetRect();
    LogicalPoint origKidNormalPosition =
      kidFrame->GetLogicalNormalPosition(wm, containerSize);
    // All cells' no-relative-positioning position should be snapped to the
    // row's bstart edge.
    // This doesn't hold in vertical-rl mode, where we don't yet know the
    // correct containerSize for the row frame. In that case, we'll have to
    // fix up child positions later, after determining our desiredSize.
    NS_ASSERTION(origKidNormalPosition.B(wm) == 0 || wm.IsVerticalRL(),
                 "unexpected kid position");

    nsRect kidVisualOverflow = kidFrame->GetVisualOverflowRect();
    LogicalPoint kidPosition(wm, iCoord, 0);
    bool firstReflow = kidFrame->HasAnyStateBits(NS_FRAME_FIRST_REFLOW);

    if (doReflowChild) {
      // Calculate the available isize for the table cell using the known
      // column isizes
      nscoord availCellISize = CalcAvailISize(aTableFrame, *cellFrame);

      Maybe<TableCellReflowInput> kidReflowInput;
      ReflowOutput desiredSize(aReflowInput);

      // If the avail isize is not the same as last time we reflowed the cell or
      // the cell wants to be bigger than what was available last time or
      // it is a style change reflow or we are printing, then we must reflow the
      // cell. Otherwise we can skip the reflow.
      // XXXldb Why is this condition distinct from doReflowChild above?
      WritingMode wm = aReflowInput.GetWritingMode();
      NS_ASSERTION(cellFrame->GetWritingMode() == wm,
                   "expected consistent writing-mode within table");
      LogicalSize cellDesiredSize = cellFrame->GetDesiredSize();
      if ((availCellISize != cellFrame->GetPriorAvailISize())           ||
          (cellDesiredSize.ISize(wm) > cellFrame->GetPriorAvailISize()) ||
          HasAnyStateBits(NS_FRAME_IS_DIRTY)                            ||
          isPaginated                                                   ||
          NS_SUBTREE_DIRTY(cellFrame)                                   ||
          // See if it needs a special reflow, or if it had one that we need to undo.
          cellFrame->HasAnyStateBits(NS_FRAME_CONTAINS_RELATIVE_BSIZE)  ||
          HasPctBSize()) {
        // Reflow the cell to fit the available isize, bsize
        // XXX The old IR_ChildIsDirty code used availCellISize here.
        LogicalSize kidAvailSize(wm, availCellISize, aReflowInput.AvailableBSize());

        // Reflow the child
        kidReflowInput.emplace(aPresContext, aReflowInput, kidFrame,
                               kidAvailSize,
                               ReflowInput::CALLER_WILL_INIT);
        InitChildReflowInput(*aPresContext, kidAvailSize, borderCollapse,
                             *kidReflowInput);

        nsReflowStatus status;
        ReflowChild(kidFrame, aPresContext, desiredSize, *kidReflowInput,
                    wm, kidPosition, containerSize, 0, status);

        // allow the table to determine if/how the table needs to be rebalanced
        // If any of the cells are not complete, then we're not complete
        if (status.IsIncomplete()) {
          aStatus.Reset();
          aStatus.SetIncomplete();
        }
      } else {
        if (iCoord != origKidNormalPosition.I(wm)) {
          kidFrame->InvalidateFrameSubtree();
        }

        desiredSize.SetSize(wm, cellDesiredSize);
        desiredSize.mOverflowAreas = cellFrame->GetOverflowAreas();

        // if we are in a floated table, our position is not yet established, so we cannot reposition our views
        // the containing block will do this for us after positioning the table
        if (!aTableFrame.IsFloating()) {
          // Because we may have moved the frame we need to make sure any views are
          // positioned properly. We have to do this, because any one of our parent
          // frames could have moved and we have no way of knowing...
          nsTableFrame::RePositionViews(kidFrame);
        }
      }

      if (NS_UNCONSTRAINEDSIZE == aReflowInput.AvailableBSize()) {
        if (!GetPrevInFlow()) {
          // Calculate the cell's actual bsize given its pass2 bsize. This
          // function takes into account the specified bsize (in the style)
          CalculateCellActualBSize(cellFrame, desiredSize.BSize(wm), wm);
        }
        // bsize may have changed, adjust descent to absorb any excess difference
        nscoord ascent;
        if (!kidFrame->PrincipalChildList().FirstChild()->PrincipalChildList().FirstChild()) {
          ascent = desiredSize.BSize(wm);
        } else {
          ascent = ((nsTableCellFrame *)kidFrame)->GetCellBaseline();
        }
        nscoord descent = desiredSize.BSize(wm) - ascent;
        UpdateBSize(desiredSize.BSize(wm), ascent, descent, &aTableFrame, cellFrame);
      } else {
        cellMaxBSize = std::max(cellMaxBSize, desiredSize.BSize(wm));
        int32_t rowSpan = aTableFrame.GetEffectiveRowSpan((nsTableCellFrame&)*kidFrame);
        if (1 == rowSpan) {
          SetContentBSize(cellMaxBSize);
        }
      }

      // Place the child
      desiredSize.ISize(wm) = availCellISize;

      if (kidReflowInput) {
        // We reflowed. Apply relative positioning in the normal way.
        kidReflowInput->ApplyRelativePositioning(&kidPosition, containerSize);
      } else if (kidFrame->IsRelativelyPositioned()) {
        // We didn't reflow.  Do the positioning part of what
        // MovePositionBy does internally.  (This codepath should really
        // be merged into the else below if we can.)
        nsMargin* computedOffsetProp =
          kidFrame->Properties().Get(nsIFrame::ComputedOffsetProperty());
        // Bug 975644: a position:sticky kid can end up with a null
        // property value here.
        LogicalMargin computedOffsets(wm, computedOffsetProp ?
                                            *computedOffsetProp : nsMargin());
        ReflowInput::ApplyRelativePositioning(kidFrame, wm, computedOffsets,
                                                    &kidPosition, containerSize);
      }

      // In vertical-rl mode, we are likely to have containerSize.width = 0
      // because ComputedWidth() was NS_UNCONSTRAINEDSIZE.
      // For cases where that's wrong, we will fix up the position later.
      FinishReflowChild(kidFrame, aPresContext, desiredSize, nullptr,
                        wm, kidPosition, containerSize, 0);

      nsTableFrame::InvalidateTableFrame(kidFrame, kidRect, kidVisualOverflow,
                                         firstReflow);

      iCoord += desiredSize.ISize(wm);
    } else {
      if (iCoord != origKidNormalPosition.I(wm)) {
        // Invalidate the old position
        kidFrame->InvalidateFrameSubtree();
        // Move to the new position. As above, we need to account for relative
        // positioning.
        kidFrame->MovePositionBy(wm,
          LogicalPoint(wm, iCoord - origKidNormalPosition.I(wm), 0));
        nsTableFrame::RePositionViews(kidFrame);
        // invalidate the new position
        kidFrame->InvalidateFrameSubtree();
      }
      // we need to account for the cell's isize even if it isn't reflowed
      iCoord += kidFrame->ISize(wm);

      if (kidFrame->GetNextInFlow()) {
        aStatus.Reset();
        aStatus.SetIncomplete();
      }
    }
    ConsiderChildOverflow(aDesiredSize.mOverflowAreas, kidFrame);
    iCoord += aTableFrame.GetColSpacing(cellColIndex);
  }

  // Just set our isize to what was available.
  // The table will calculate the isize and not use our value.
  aDesiredSize.ISize(wm) = aReflowInput.AvailableISize();

  if (aReflowInput.mFlags.mSpecialBSizeReflow) {
    aDesiredSize.BSize(wm) = BSize(wm);
  } else if (NS_UNCONSTRAINEDSIZE == aReflowInput.AvailableBSize()) {
    aDesiredSize.BSize(wm) = CalcBSize(aReflowInput);
    if (GetPrevInFlow()) {
      nscoord bsize = CalcBSizeFromUnpaginatedBSize(*this, wm);
      aDesiredSize.BSize(wm) = std::max(aDesiredSize.BSize(wm), bsize);
    } else {
      if (isPaginated && HasStyleBSize()) {
        // set the unpaginated bsize so next in flows can try to honor it
        SetHasUnpaginatedBSize(true);
        SetUnpaginatedBSize(aPresContext, aDesiredSize.BSize(wm));
      }
      if (isPaginated && HasUnpaginatedBSize()) {
        aDesiredSize.BSize(wm) = std::max(aDesiredSize.BSize(wm),
                                          GetUnpaginatedBSize());
      }
    }
  } else { // constrained bsize, paginated
    // Compute the bsize we should have from style (subtracting the
    // bsize from our prev-in-flows from the style bsize)
    nscoord styleBSize = CalcBSizeFromUnpaginatedBSize(*this, wm);
    if (styleBSize > aReflowInput.AvailableBSize()) {
      styleBSize = aReflowInput.AvailableBSize();
      aStatus.SetIncomplete();
    }
    aDesiredSize.BSize(wm) = std::max(cellMaxBSize, styleBSize);
  }

  if (wm.IsVerticalRL()) {
    // Any children whose width was not the same as our final
    // aDesiredSize.BSize will have been misplaced earlier at the
    // FinishReflowChild stage. So fix them up now.
    for (nsIFrame* kidFrame : mFrames) {
      nsTableCellFrame *cellFrame = do_QueryFrame(kidFrame);
      if (!cellFrame) {
        continue;
      }
      if (kidFrame->BSize(wm) != aDesiredSize.BSize(wm)) {
        kidFrame->MovePositionBy(wm,
          LogicalPoint(wm, 0, kidFrame->BSize(wm) - aDesiredSize.BSize(wm)));
        nsTableFrame::RePositionViews(kidFrame);
        // Do we need to InvalidateFrameSubtree() here?
      }
    }
  }

  aDesiredSize.UnionOverflowAreasWithDesiredBounds();
  FinishAndStoreOverflow(&aDesiredSize);
}

/** Layout the entire row.
  * This method stacks cells in the inline dir according to HTML 4.0 rules.
  */
void
nsTableRowFrame::Reflow(nsPresContext*           aPresContext,
                        ReflowOutput&     aDesiredSize,
                        const ReflowInput& aReflowInput,
                        nsReflowStatus&          aStatus)
{
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsTableRowFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aDesiredSize, aStatus);

  WritingMode wm = aReflowInput.GetWritingMode();

  nsTableFrame* tableFrame = GetTableFrame();
  const nsStyleVisibility* rowVis = StyleVisibility();
  bool collapseRow = (NS_STYLE_VISIBILITY_COLLAPSE == rowVis->mVisible);
  if (collapseRow) {
    tableFrame->SetNeedToCollapse(true);
  }

  // see if a special bsize reflow needs to occur due to having a pct bsize
  nsTableFrame::CheckRequestSpecialBSizeReflow(aReflowInput);

  // See if we have a cell with specified/pct bsize
  InitHasCellWithStyleBSize(tableFrame);

  ReflowChildren(aPresContext, aDesiredSize, aReflowInput, *tableFrame, aStatus);

  if (aPresContext->IsPaginated() && !aStatus.IsFullyComplete() &&
      ShouldAvoidBreakInside(aReflowInput)) {
    aStatus.SetInlineLineBreakBeforeAndReset();
  }

  // Just set our isize to what was available.
  // The table will calculate the isize and not use our value.
  aDesiredSize.ISize(wm) = aReflowInput.AvailableISize();

  // If our parent is in initial reflow, it'll handle invalidating our
  // entire overflow rect.
  if (!GetParent()->HasAnyStateBits(NS_FRAME_FIRST_REFLOW) &&
      nsSize(aDesiredSize.Width(), aDesiredSize.Height()) != mRect.Size()) {
    InvalidateFrame();
  }

  // Any absolutely-positioned children will get reflowed in
  // nsFrame::FixupPositionedTableParts in another pass, so propagate our
  // dirtiness to them before our parent clears our dirty bits.
  PushDirtyBitToAbsoluteFrames();

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowInput, aDesiredSize);
}

/**
 * This function is called by the row group frame's SplitRowGroup() code when
 * pushing a row frame that has cell frames that span into it. The cell frame
 * should be reflowed with the specified height
 */
nscoord
nsTableRowFrame::ReflowCellFrame(nsPresContext*           aPresContext,
                                 const ReflowInput& aReflowInput,
                                 bool                     aIsTopOfPage,
                                 nsTableCellFrame*        aCellFrame,
                                 nscoord                  aAvailableBSize,
                                 nsReflowStatus&          aStatus)
{
  WritingMode wm = aReflowInput.GetWritingMode();

  // Reflow the cell frame with the specified height. Use the existing width
  nsSize containerSize = aCellFrame->GetSize();
  LogicalRect cellRect = aCellFrame->GetLogicalRect(wm, containerSize);
  nsRect cellVisualOverflow = aCellFrame->GetVisualOverflowRect();

  LogicalSize cellSize = cellRect.Size(wm);
  LogicalSize availSize(wm, cellRect.ISize(wm), aAvailableBSize);
  bool borderCollapse = GetTableFrame()->IsBorderCollapse();
  NS_ASSERTION(aCellFrame->GetWritingMode() == wm,
               "expected consistent writing-mode within table");
  TableCellReflowInput
    cellReflowInput(aPresContext, aReflowInput, aCellFrame, availSize,
                    ReflowInput::CALLER_WILL_INIT);
  InitChildReflowInput(*aPresContext, availSize, borderCollapse, cellReflowInput);
  cellReflowInput.mFlags.mIsTopOfPage = aIsTopOfPage;

  ReflowOutput desiredSize(aReflowInput);

  ReflowChild(aCellFrame, aPresContext, desiredSize, cellReflowInput,
              0, 0, NS_FRAME_NO_MOVE_FRAME, aStatus);
  bool fullyComplete = aStatus.IsComplete() && !aStatus.IsTruncated();
  if (fullyComplete) {
    desiredSize.BSize(wm) = aAvailableBSize;
  }
  aCellFrame->SetSize(wm, LogicalSize(wm, cellSize.ISize(wm),
                                      desiredSize.BSize(wm)));

  // Note: BlockDirAlignChild can affect the overflow rect.
  // XXX What happens if this cell has 'vertical-align: baseline' ?
  // XXX Why is it assumed that the cell's ascent hasn't changed ?
  if (fullyComplete) {
    aCellFrame->BlockDirAlignChild(wm, mMaxCellAscent);
  }

  nsTableFrame::InvalidateTableFrame(aCellFrame,
                                     cellRect.GetPhysicalRect(wm, containerSize),
                                     cellVisualOverflow,
                                     aCellFrame->
                                       HasAnyStateBits(NS_FRAME_FIRST_REFLOW));

  aCellFrame->DidReflow(aPresContext, nullptr, nsDidReflowStatus::FINISHED);

  return desiredSize.BSize(wm);
}

nscoord
nsTableRowFrame::CollapseRowIfNecessary(nscoord aRowOffset,
                                        nscoord aISize,
                                        bool    aCollapseGroup,
                                        bool&   aDidCollapse)
{
  const nsStyleVisibility* rowVis = StyleVisibility();
  bool collapseRow = (NS_STYLE_VISIBILITY_COLLAPSE == rowVis->mVisible);
  nsTableFrame* tableFrame =
    static_cast<nsTableFrame*>(GetTableFrame()->FirstInFlow());
  if (collapseRow) {
    tableFrame->SetNeedToCollapse(true);
  }

  if (aRowOffset != 0) {
    // We're moving, so invalidate our old position
    InvalidateFrameSubtree();
  }

  WritingMode wm = GetWritingMode();

  nsSize parentSize = GetParent()->GetSize();
  LogicalRect rowRect = GetLogicalRect(wm, parentSize);
  nsRect oldRect = mRect;
  nsRect oldVisualOverflow = GetVisualOverflowRect();

  rowRect.BStart(wm) -= aRowOffset;
  rowRect.ISize(wm)  = aISize;
  nsOverflowAreas overflow;
  nscoord shift = 0;
  nsSize containerSize = mRect.Size();

  if (aCollapseGroup || collapseRow) {
    aDidCollapse = true;
    shift = rowRect.BSize(wm);
    nsTableCellFrame* cellFrame = GetFirstCell();
    if (cellFrame) {
      int32_t rowIndex;
      cellFrame->GetRowIndex(rowIndex);
      shift += tableFrame->GetRowSpacing(rowIndex);
      while (cellFrame) {
        LogicalRect cRect = cellFrame->GetLogicalRect(wm, containerSize);
        // If aRowOffset != 0, there's no point in invalidating the cells, since
        // we've already invalidated our overflow area.  Note that we _do_ still
        // need to invalidate if our row is not moving, because the cell might
        // span out of this row, so invalidating our row rect won't do enough.
        if (aRowOffset == 0) {
          InvalidateFrame();
        }
        cRect.BSize(wm) = 0;
        cellFrame->SetRect(wm, cRect, containerSize);
        cellFrame = cellFrame->GetNextCell();
      }
    } else {
      shift += tableFrame->GetRowSpacing(GetRowIndex());
    }
    rowRect.BSize(wm) = 0;
  }
  else { // row is not collapsed
    // remember the col index of the previous cell to handle rowspans into this
    // row
    int32_t prevColIndex = -1;
    nscoord iPos = 0; // running total of children inline-axis offset
    nsTableFrame* fifTable =
      static_cast<nsTableFrame*>(tableFrame->FirstInFlow());

    for (nsIFrame* kidFrame : mFrames) {
      nsTableCellFrame *cellFrame = do_QueryFrame(kidFrame);
      if (cellFrame) {
        int32_t cellColIndex;
        cellFrame->GetColIndex(cellColIndex);
        int32_t cellColSpan = tableFrame->GetEffectiveColSpan(*cellFrame);

        // If the adjacent cell is in a prior row (because of a rowspan) add in
        // the space
        if (prevColIndex != (cellColIndex - 1)) {
          iPos += GetSpaceBetween(prevColIndex, cellColIndex, cellColSpan,
                                  *tableFrame, true);
        }
        LogicalRect cRect(wm, iPos, 0, 0, rowRect.BSize(wm));

        // remember the last (iend-wards-most) column this cell spans into
        prevColIndex = cellColIndex + cellColSpan - 1;
        int32_t actualColSpan = cellColSpan;
        bool isVisible = false;
        for (int32_t colIdx = cellColIndex; actualColSpan > 0;
             colIdx++, actualColSpan--) {

          nsTableColFrame* colFrame = tableFrame->GetColFrame(colIdx);
          const nsStyleVisibility* colVis = colFrame->StyleVisibility();
          bool collapseCol = (NS_STYLE_VISIBILITY_COLLAPSE ==
                                colVis->mVisible);
          nsIFrame* cgFrame = colFrame->GetParent();
          const nsStyleVisibility* groupVis = cgFrame->StyleVisibility();
          bool collapseGroup = (NS_STYLE_VISIBILITY_COLLAPSE ==
                                  groupVis->mVisible);
          bool isCollapsed = collapseCol || collapseGroup;
          if (!isCollapsed) {
            cRect.ISize(wm) += fifTable->GetColumnISizeFromFirstInFlow(colIdx);
            isVisible = true;
            if ((actualColSpan > 1)) {
              nsTableColFrame* nextColFrame =
                tableFrame->GetColFrame(colIdx + 1);
              const nsStyleVisibility* nextColVis =
              nextColFrame->StyleVisibility();
              if ( (NS_STYLE_VISIBILITY_COLLAPSE != nextColVis->mVisible) &&
                  tableFrame->ColumnHasCellSpacingBefore(colIdx + 1)) {
                cRect.ISize(wm) += tableFrame->GetColSpacing(cellColIndex);
              }
            }
          }
        }
        iPos += cRect.ISize(wm);
        if (isVisible) {
          iPos += tableFrame->GetColSpacing(cellColIndex);
        }
        int32_t actualRowSpan = tableFrame->GetEffectiveRowSpan(*cellFrame);
        nsTableRowFrame* rowFrame = GetNextRow();
        for (actualRowSpan--; actualRowSpan > 0 && rowFrame; actualRowSpan--) {
          const nsStyleVisibility* nextRowVis = rowFrame->StyleVisibility();
          bool collapseNextRow = (NS_STYLE_VISIBILITY_COLLAPSE ==
                                    nextRowVis->mVisible);
          if (!collapseNextRow) {
            LogicalRect nextRect = rowFrame->GetLogicalRect(wm,
                                                            containerSize);
            cRect.BSize(wm) +=
              nextRect.BSize(wm) +
              tableFrame->GetRowSpacing(rowFrame->GetRowIndex());
          }
          rowFrame = rowFrame->GetNextRow();
        }

        nsRect oldCellRect = cellFrame->GetRect();
        LogicalPoint oldCellNormalPos =
          cellFrame->GetLogicalNormalPosition(wm, containerSize);

        nsRect oldCellVisualOverflow = cellFrame->GetVisualOverflowRect();

        if (aRowOffset == 0 && cRect.Origin(wm) != oldCellNormalPos) {
          // We're moving the cell.  Invalidate the old overflow area
          cellFrame->InvalidateFrameSubtree();
        }

        cellFrame->MovePositionBy(wm, cRect.Origin(wm) - oldCellNormalPos);
        cellFrame->SetSize(wm, cRect.Size(wm));

        // XXXbz This looks completely bogus in the cases when we didn't
        // collapse the cell!
        LogicalRect cellBounds(wm, 0, 0, cRect.ISize(wm), cRect.BSize(wm));
        nsRect cellPhysicalBounds =
          cellBounds.GetPhysicalRect(wm, containerSize);
        nsOverflowAreas cellOverflow(cellPhysicalBounds, cellPhysicalBounds);
        cellFrame->FinishAndStoreOverflow(cellOverflow,
                                          cRect.Size(wm).GetPhysicalSize(wm));
        nsTableFrame::RePositionViews(cellFrame);
        ConsiderChildOverflow(overflow, cellFrame);

        if (aRowOffset == 0) {
          nsTableFrame::InvalidateTableFrame(cellFrame, oldCellRect,
                                             oldCellVisualOverflow, false);
        }
      }
    }
  }

  SetRect(wm, rowRect, containerSize);
  overflow.UnionAllWith(nsRect(0, 0, rowRect.Width(wm), rowRect.Height(wm)));
  FinishAndStoreOverflow(overflow, rowRect.Size(wm).GetPhysicalSize(wm));

  nsTableFrame::RePositionViews(this);
  nsTableFrame::InvalidateTableFrame(this, oldRect, oldVisualOverflow, false);
  return shift;
}

/*
 * The following method is called by the row group frame's SplitRowGroup()
 * when it creates a continuing cell frame and wants to insert it into the
 * row's child list.
 */
void
nsTableRowFrame::InsertCellFrame(nsTableCellFrame* aFrame,
                                 int32_t           aColIndex)
{
  // Find the cell frame where col index < aColIndex
  nsTableCellFrame* priorCell = nullptr;
  for (nsIFrame* child : mFrames) {
    nsTableCellFrame *cellFrame = do_QueryFrame(child);
    if (cellFrame) {
      int32_t colIndex;
      cellFrame->GetColIndex(colIndex);
      if (colIndex < aColIndex) {
        priorCell = cellFrame;
      }
      else break;
    }
  }
  mFrames.InsertFrame(this, priorCell, aFrame);
}

nsTableRowFrame*
nsTableRowFrame::GetNextRow() const
{
  nsIFrame* childFrame = GetNextSibling();
  while (childFrame) {
    nsTableRowFrame *rowFrame = do_QueryFrame(childFrame);
    if (rowFrame) {
	  NS_ASSERTION(mozilla::StyleDisplay::TableRow == childFrame->StyleDisplay()->mDisplay,
                 "wrong display type on rowframe");
      return rowFrame;
    }
    childFrame = childFrame->GetNextSibling();
  }
  return nullptr;
}

NS_DECLARE_FRAME_PROPERTY_SMALL_VALUE(RowUnpaginatedHeightProperty, nscoord)

void
nsTableRowFrame::SetUnpaginatedBSize(nsPresContext* aPresContext,
                                     nscoord        aValue)
{
  NS_ASSERTION(!GetPrevInFlow(), "program error");
  // Get the property
  aPresContext->PropertyTable()->
    Set(this, RowUnpaginatedHeightProperty(), aValue);
}

nscoord
nsTableRowFrame::GetUnpaginatedBSize()
{
  FrameProperties props = FirstInFlow()->Properties();
  return props.Get(RowUnpaginatedHeightProperty());
}

void nsTableRowFrame::SetContinuousBCBorderWidth(LogicalSide aForSide,
                                                 BCPixelSize aPixelValue)
{
  switch (aForSide) {
    case eLogicalSideIEnd:
      mIEndContBorderWidth = aPixelValue;
      return;
    case eLogicalSideBStart:
      mBStartContBorderWidth = aPixelValue;
      return;
    case eLogicalSideIStart:
      mIStartContBorderWidth = aPixelValue;
      return;
    default:
      NS_ERROR("invalid LogicalSide arg");
  }
}
#ifdef ACCESSIBILITY
a11y::AccType
nsTableRowFrame::AccessibleType()
{
  return a11y::eHTMLTableRowType;
}
#endif
/**
 * Sets the NS_ROW_HAS_CELL_WITH_STYLE_BSIZE bit to indicate whether
 * this row has any cells that have non-auto-bsize.  (Row-spanning
 * cells are ignored.)
 */
void nsTableRowFrame::InitHasCellWithStyleBSize(nsTableFrame* aTableFrame)
{
  WritingMode wm = GetWritingMode();

  for (nsIFrame* kidFrame : mFrames) {
    nsTableCellFrame *cellFrame = do_QueryFrame(kidFrame);
    if (!cellFrame) {
      NS_NOTREACHED("Table row has a non-cell child.");
      continue;
    }
    // Ignore row-spanning cells
    const nsStyleCoord &cellBSize = cellFrame->StylePosition()->BSize(wm);
    if (aTableFrame->GetEffectiveRowSpan(*cellFrame) == 1 &&
        cellBSize.GetUnit() != eStyleUnit_Auto &&
         /* calc() with percentages treated like 'auto' */
        (!cellBSize.IsCalcUnit() || !cellBSize.HasPercent())) {
      AddStateBits(NS_ROW_HAS_CELL_WITH_STYLE_BSIZE);
      return;
    }
  }
  RemoveStateBits(NS_ROW_HAS_CELL_WITH_STYLE_BSIZE);
}

void
nsTableRowFrame::InvalidateFrame(uint32_t aDisplayItemKey)
{
  nsIFrame::InvalidateFrame(aDisplayItemKey);
  GetParent()->InvalidateFrameWithRect(GetVisualOverflowRect() + GetPosition(), aDisplayItemKey);
}

void
nsTableRowFrame::InvalidateFrameWithRect(const nsRect& aRect, uint32_t aDisplayItemKey)
{
  nsIFrame::InvalidateFrameWithRect(aRect, aDisplayItemKey);
  // If we have filters applied that would affects our bounds, then
  // we get an inactive layer created and this is computed
  // within FrameLayerBuilder
  GetParent()->InvalidateFrameWithRect(aRect + GetPosition(), aDisplayItemKey);
}

/* ----- global methods ----- */

nsTableRowFrame*
NS_NewTableRowFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsTableRowFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsTableRowFrame)

#ifdef DEBUG_FRAME_DUMP
nsresult
nsTableRowFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("TableRow"), aResult);
}
#endif
