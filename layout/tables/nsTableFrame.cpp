/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Likely.h"
#include "mozilla/MathAlgorithms.h"

#include "nsCOMPtr.h"
#include "nsTableFrame.h"
#include "nsRenderingContext.h"
#include "nsStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIContent.h"
#include "nsCellMap.h"
#include "nsTableCellFrame.h"
#include "nsHTMLParts.h"
#include "nsTableColFrame.h"
#include "nsTableColGroupFrame.h"
#include "nsTableRowFrame.h"
#include "nsTableRowGroupFrame.h"
#include "nsTableOuterFrame.h"
#include "nsTablePainter.h"

#include "BasicTableLayoutStrategy.h"
#include "FixedTableLayoutStrategy.h"

#include "nsPresContext.h"
#include "nsContentUtils.h"
#include "nsCSSRendering.h"
#include "nsGkAtoms.h"
#include "nsCSSAnonBoxes.h"
#include "nsIPresShell.h"
#include "nsIDOMElement.h"
#include "nsIDOMHTMLElement.h"
#include "nsIScriptError.h"
#include "nsFrameManager.h"
#include "nsError.h"
#include "nsAutoPtr.h"
#include "nsCSSFrameConstructor.h"
#include "nsStyleSet.h"
#include "nsDisplayList.h"
#include "nsIScrollableFrame.h"
#include "nsCSSProps.h"
#include "RestyleTracker.h"
#include <algorithm>

using namespace mozilla;
using namespace mozilla::image;
using namespace mozilla::layout;

/********************************************************************************
 ** nsTableReflowState                                                         **
 ********************************************************************************/

struct nsTableReflowState {

  // the real reflow state
  const nsHTMLReflowState& reflowState;

  // The table's available size
  nsSize availSize;

  // Stationary x-offset
  nscoord x;

  // Running y-offset
  nscoord y;

  nsTableReflowState(nsPresContext&           aPresContext,
                     const nsHTMLReflowState& aReflowState,
                     nsTableFrame&            aTableFrame,
                     nscoord                  aAvailWidth,
                     nscoord                  aAvailHeight)
    : reflowState(aReflowState)
  {
    Init(aPresContext, aTableFrame, aAvailWidth, aAvailHeight);
  }

  void Init(nsPresContext&  aPresContext,
            nsTableFrame&   aTableFrame,
            nscoord         aAvailWidth,
            nscoord         aAvailHeight)
  {
    nsTableFrame* table = static_cast<nsTableFrame*>(aTableFrame.FirstInFlow());
    nsMargin borderPadding = table->GetChildAreaOffset(&reflowState);

    x = borderPadding.left + table->GetColSpacing(-1);
    y = borderPadding.top; //cellspacing added during reflow

    availSize.width  = aAvailWidth;
    if (NS_UNCONSTRAINEDSIZE != availSize.width) {
      int32_t colCount = table->GetColCount();
      availSize.width -= borderPadding.left + borderPadding.right
                         + table->GetColSpacing(-1)
                         + table->GetColSpacing(colCount);
      availSize.width = std::max(0, availSize.width);
    }

    availSize.height = aAvailHeight;
    if (NS_UNCONSTRAINEDSIZE != availSize.height) {
      availSize.height -= borderPadding.top + borderPadding.bottom
                          + table->GetRowSpacing(-1)
                          + table->GetRowSpacing(table->GetRowCount());
      availSize.height = std::max(0, availSize.height);
    }
  }

  nsTableReflowState(nsPresContext&           aPresContext,
                     const nsHTMLReflowState& aReflowState,
                     nsTableFrame&            aTableFrame)
    : reflowState(aReflowState)
  {
    Init(aPresContext, aTableFrame, aReflowState.AvailableWidth(), aReflowState.AvailableHeight());
  }

};

/********************************************************************************
 ** nsTableFrame                                                               **
 ********************************************************************************/

struct BCPropertyData
{
  BCPropertyData() : mTopBorderWidth(0), mRightBorderWidth(0),
                     mBottomBorderWidth(0), mLeftBorderWidth(0),
                     mLeftCellBorderWidth(0), mRightCellBorderWidth(0) {}
  nsIntRect mDamageArea;
  BCPixelSize mTopBorderWidth;
  BCPixelSize mRightBorderWidth;
  BCPixelSize mBottomBorderWidth;
  BCPixelSize mLeftBorderWidth;
  BCPixelSize mLeftCellBorderWidth;
  BCPixelSize mRightCellBorderWidth;
};

nsStyleContext*
nsTableFrame::GetParentStyleContext(nsIFrame** aProviderFrame) const
{
  // Since our parent, the table outer frame, returned this frame, we
  // must return whatever our parent would normally have returned.

  NS_PRECONDITION(GetParent(), "table constructed without outer table");
  if (!mContent->GetParent() && !StyleContext()->GetPseudo()) {
    // We're the root.  We have no style context parent.
    *aProviderFrame = nullptr;
    return nullptr;
  }

  return GetParent()->DoGetParentStyleContext(aProviderFrame);
}


nsIAtom*
nsTableFrame::GetType() const
{
  return nsGkAtoms::tableFrame;
}


nsTableFrame::nsTableFrame(nsStyleContext* aContext)
  : nsContainerFrame(aContext),
    mCellMap(nullptr),
    mTableLayoutStrategy(nullptr)
{
  memset(&mBits, 0, sizeof(mBits));
}

void
nsTableFrame::Init(nsIContent*       aContent,
                   nsContainerFrame* aParent,
                   nsIFrame*         aPrevInFlow)
{
  NS_PRECONDITION(!mCellMap, "Init called twice");
  NS_PRECONDITION(!aPrevInFlow ||
                  aPrevInFlow->GetType() == nsGkAtoms::tableFrame,
                  "prev-in-flow must be of same type");

  // Let the base class do its processing
  nsContainerFrame::Init(aContent, aParent, aPrevInFlow);

  // see if border collapse is on, if so set it
  const nsStyleTableBorder* tableStyle = StyleTableBorder();
  bool borderCollapse = (NS_STYLE_BORDER_COLLAPSE == tableStyle->mBorderCollapse);
  SetBorderCollapse(borderCollapse);

  // Create the cell map if this frame is the first-in-flow.
  if (!aPrevInFlow) {
    mCellMap = new nsTableCellMap(*this, borderCollapse);
  }

  if (aPrevInFlow) {
    // set my width, because all frames in a table flow are the same width and
    // code in nsTableOuterFrame depends on this being set
    mRect.width = aPrevInFlow->GetSize().width;
  }
  else {
    NS_ASSERTION(!mTableLayoutStrategy, "strategy was created before Init was called");
    // create the strategy
    if (IsAutoLayout())
      mTableLayoutStrategy = new BasicTableLayoutStrategy(this);
    else
      mTableLayoutStrategy = new FixedTableLayoutStrategy(this);
  }
}

nsTableFrame::~nsTableFrame()
{
  delete mCellMap;
  delete mTableLayoutStrategy;
}

void
nsTableFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  mColGroups.DestroyFramesFrom(aDestructRoot);
  nsContainerFrame::DestroyFrom(aDestructRoot);
}

// Make sure any views are positioned properly
void
nsTableFrame::RePositionViews(nsIFrame* aFrame)
{
  nsContainerFrame::PositionFrameView(aFrame);
  nsContainerFrame::PositionChildViews(aFrame);
}

static bool
IsRepeatedFrame(nsIFrame* kidFrame)
{
  return (kidFrame->GetType() == nsGkAtoms::tableRowFrame ||
          kidFrame->GetType() == nsGkAtoms::tableRowGroupFrame) &&
         (kidFrame->GetStateBits() & NS_REPEATED_ROW_OR_ROWGROUP);
}

bool
nsTableFrame::PageBreakAfter(nsIFrame* aSourceFrame,
                             nsIFrame* aNextFrame)
{
  const nsStyleDisplay* display = aSourceFrame->StyleDisplay();
  nsTableRowGroupFrame* prevRg = do_QueryFrame(aSourceFrame);
  // don't allow a page break after a repeated element ...
  if ((display->mBreakAfter || (prevRg && prevRg->HasInternalBreakAfter())) &&
      !IsRepeatedFrame(aSourceFrame)) {
    return !(aNextFrame && IsRepeatedFrame(aNextFrame)); // or before
  }

  if (aNextFrame) {
    display = aNextFrame->StyleDisplay();
    // don't allow a page break before a repeated element ...
     nsTableRowGroupFrame* nextRg = do_QueryFrame(aNextFrame);
    if ((display->mBreakBefore ||
        (nextRg && nextRg->HasInternalBreakBefore())) &&
        !IsRepeatedFrame(aNextFrame)) {
      return !IsRepeatedFrame(aSourceFrame); // or after
    }
  }
  return false;
}

typedef nsTArray<nsIFrame*> FrameTArray;

/* static */ void
nsTableFrame::RegisterPositionedTablePart(nsIFrame* aFrame)
{
  // Supporting relative positioning for table parts other than table cells has
  // the potential to break sites that apply 'position: relative' to those
  // parts, expecting nothing to happen. We warn at the console to make tracking
  // down the issue easy.
  if (!IS_TABLE_CELL(aFrame->GetType())) {
    nsIContent* content = aFrame->GetContent();
    nsPresContext* presContext = aFrame->PresContext();
    if (content && !presContext->HasWarnedAboutPositionedTableParts()) {
      presContext->SetHasWarnedAboutPositionedTableParts();
      nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                      NS_LITERAL_CSTRING("Layout: Tables"),
                                      content->OwnerDoc(),
                                      nsContentUtils::eLAYOUT_PROPERTIES,
                                      "TablePartRelPosWarning");
    }
  }

  nsTableFrame* tableFrame = nsTableFrame::GetTableFrame(aFrame);
  MOZ_ASSERT(tableFrame, "Should have a table frame here");
  tableFrame = static_cast<nsTableFrame*>(tableFrame->FirstContinuation());

  // Retrieve the positioned parts array for this table.
  FrameProperties props = tableFrame->Properties();
  auto positionedParts =
    static_cast<FrameTArray*>(props.Get(PositionedTablePartArray()));

  // Lazily create the array if it doesn't exist yet.
  if (!positionedParts) {
    positionedParts = new FrameTArray;
    props.Set(PositionedTablePartArray(), positionedParts);
  }

  // Add this frame to the list.
  positionedParts->AppendElement(aFrame);
}

/* static */ void
nsTableFrame::UnregisterPositionedTablePart(nsIFrame* aFrame,
                                            nsIFrame* aDestructRoot)
{
  // Retrieve the table frame, and ensure that we hit aDestructRoot on the way.
  // If we don't, that means that the table frame will be destroyed, so we don't
  // need to bother with unregistering this frame.
  nsTableFrame* tableFrame = GetTableFramePassingThrough(aDestructRoot, aFrame);
  if (!tableFrame) {
    return;
  }
  tableFrame = static_cast<nsTableFrame*>(tableFrame->FirstContinuation());

  // Retrieve the positioned parts array for this table.
  FrameProperties props = tableFrame->Properties();
  auto positionedParts =
    static_cast<FrameTArray*>(props.Get(PositionedTablePartArray()));

  // Remove the frame.
  MOZ_ASSERT(positionedParts &&
             positionedParts->IndexOf(aFrame) != FrameTArray::NoIndex,
             "Asked to unregister a positioned table part that wasn't registered");
  if (positionedParts) {
    positionedParts->RemoveElement(aFrame);
  }
}

// XXX this needs to be cleaned up so that the frame constructor breaks out col group
// frames into a separate child list, bug 343048.
void
nsTableFrame::SetInitialChildList(ChildListID     aListID,
                                  nsFrameList&    aChildList)
{
  MOZ_ASSERT(mFrames.IsEmpty() && mColGroups.IsEmpty(),
             "unexpected second call to SetInitialChildList");
  MOZ_ASSERT(aListID == kPrincipalList, "unexpected child list");

  // XXXbz the below code is an icky cesspit that's only needed in its current
  // form for two reasons:
  // 1) Both rowgroups and column groups come in on the principal child list.
  while (aChildList.NotEmpty()) {
    nsIFrame* childFrame = aChildList.FirstChild();
    aChildList.RemoveFirstChild();
    const nsStyleDisplay* childDisplay = childFrame->StyleDisplay();

    if (NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP == childDisplay->mDisplay) {
      NS_ASSERTION(nsGkAtoms::tableColGroupFrame == childFrame->GetType(),
                   "This is not a colgroup");
      mColGroups.AppendFrame(nullptr, childFrame);
    }
    else { // row groups and unknown frames go on the main list for now
      mFrames.AppendFrame(nullptr, childFrame);
    }
  }

  // If we have a prev-in-flow, then we're a table that has been split and
  // so don't treat this like an append
  if (!GetPrevInFlow()) {
    // process col groups first so that real cols get constructed before
    // anonymous ones due to cells in rows.
    InsertColGroups(0, mColGroups);
    InsertRowGroups(mFrames);
    // calc collapsing borders
    if (IsBorderCollapse()) {
      SetFullBCDamageArea();
    }
  }
}

void nsTableFrame::AttributeChangedFor(nsIFrame*       aFrame,
                                       nsIContent*     aContent,
                                       nsIAtom*        aAttribute)
{
  nsTableCellFrame *cellFrame = do_QueryFrame(aFrame);
  if (cellFrame) {
    if ((nsGkAtoms::rowspan == aAttribute) ||
        (nsGkAtoms::colspan == aAttribute)) {
      nsTableCellMap* cellMap = GetCellMap();
      if (cellMap) {
        // for now just remove the cell from the map and reinsert it
        int32_t rowIndex, colIndex;
        cellFrame->GetRowIndex(rowIndex);
        cellFrame->GetColIndex(colIndex);
        RemoveCell(cellFrame, rowIndex);
        nsAutoTArray<nsTableCellFrame*, 1> cells;
        cells.AppendElement(cellFrame);
        InsertCells(cells, rowIndex, colIndex - 1);

        // XXX Should this use eStyleChange?  It currently doesn't need
        // to, but it might given more optimization.
        PresContext()->PresShell()->
          FrameNeedsReflow(this, nsIPresShell::eTreeChange, NS_FRAME_IS_DIRTY);
      }
    }
  }
}


/* ****** CellMap methods ******* */

/* return the effective col count */
int32_t nsTableFrame::GetEffectiveColCount() const
{
  int32_t colCount = GetColCount();
  if (LayoutStrategy()->GetType() == nsITableLayoutStrategy::Auto) {
    nsTableCellMap* cellMap = GetCellMap();
    if (!cellMap) {
      return 0;
    }
    // don't count cols at the end that don't have originating cells
    for (int32_t colX = colCount - 1; colX >= 0; colX--) {
      if (cellMap->GetNumCellsOriginatingInCol(colX) > 0) {
        break;
      }
      colCount--;
    }
  }
  return colCount;
}

int32_t nsTableFrame::GetIndexOfLastRealCol()
{
  int32_t numCols = mColFrames.Length();
  if (numCols > 0) {
    for (int32_t colX = numCols - 1; colX >= 0; colX--) {
      nsTableColFrame* colFrame = GetColFrame(colX);
      if (colFrame) {
        if (eColAnonymousCell != colFrame->GetColType()) {
          return colX;
        }
      }
    }
  }
  return -1;
}

nsTableColFrame*
nsTableFrame::GetColFrame(int32_t aColIndex) const
{
  NS_ASSERTION(!GetPrevInFlow(), "GetColFrame called on next in flow");
  int32_t numCols = mColFrames.Length();
  if ((aColIndex >= 0) && (aColIndex < numCols)) {
    return mColFrames.ElementAt(aColIndex);
  }
  else {
    NS_ERROR("invalid col index");
    return nullptr;
  }
}

int32_t nsTableFrame::GetEffectiveRowSpan(int32_t                 aRowIndex,
                                          const nsTableCellFrame& aCell) const
{
  nsTableCellMap* cellMap = GetCellMap();
  NS_PRECONDITION (nullptr != cellMap, "bad call, cellMap not yet allocated.");

  int32_t colIndex;
  aCell.GetColIndex(colIndex);
  return cellMap->GetEffectiveRowSpan(aRowIndex, colIndex);
}

int32_t nsTableFrame::GetEffectiveRowSpan(const nsTableCellFrame& aCell,
                                          nsCellMap*              aCellMap)
{
  nsTableCellMap* tableCellMap = GetCellMap(); if (!tableCellMap) ABORT1(1);

  int32_t colIndex, rowIndex;
  aCell.GetColIndex(colIndex);
  aCell.GetRowIndex(rowIndex);

  if (aCellMap)
    return aCellMap->GetRowSpan(rowIndex, colIndex, true);
  else
    return tableCellMap->GetEffectiveRowSpan(rowIndex, colIndex);
}

int32_t nsTableFrame::GetEffectiveColSpan(const nsTableCellFrame& aCell,
                                          nsCellMap*              aCellMap) const
{
  nsTableCellMap* tableCellMap = GetCellMap(); if (!tableCellMap) ABORT1(1);

  int32_t colIndex, rowIndex;
  aCell.GetColIndex(colIndex);
  aCell.GetRowIndex(rowIndex);
  bool ignore;

  if (aCellMap)
    return aCellMap->GetEffectiveColSpan(*tableCellMap, rowIndex, colIndex, ignore);
  else
    return tableCellMap->GetEffectiveColSpan(rowIndex, colIndex);
}

bool nsTableFrame::HasMoreThanOneCell(int32_t aRowIndex) const
{
  nsTableCellMap* tableCellMap = GetCellMap(); if (!tableCellMap) ABORT1(1);
  return tableCellMap->HasMoreThanOneCell(aRowIndex);
}

void nsTableFrame::AdjustRowIndices(int32_t         aRowIndex,
                                    int32_t         aAdjustment)
{
  // Iterate over the row groups and adjust the row indices of all rows
  // whose index is >= aRowIndex.
  RowGroupArray rowGroups;
  OrderRowGroups(rowGroups);

  for (uint32_t rgX = 0; rgX < rowGroups.Length(); rgX++) {
    rowGroups[rgX]->AdjustRowIndices(aRowIndex, aAdjustment);
  }
}


void nsTableFrame::ResetRowIndices(const nsFrameList::Slice& aRowGroupsToExclude)
{
  // Iterate over the row groups and adjust the row indices of all rows
  // omit the rowgroups that will be inserted later
  RowGroupArray rowGroups;
  OrderRowGroups(rowGroups);

  int32_t rowIndex = 0;
  nsTHashtable<nsPtrHashKey<nsTableRowGroupFrame> > excludeRowGroups;
  nsFrameList::Enumerator excludeRowGroupsEnumerator(aRowGroupsToExclude);
  while (!excludeRowGroupsEnumerator.AtEnd()) {
    excludeRowGroups.PutEntry(static_cast<nsTableRowGroupFrame*>(excludeRowGroupsEnumerator.get()));
    excludeRowGroupsEnumerator.Next();
  }

  for (uint32_t rgX = 0; rgX < rowGroups.Length(); rgX++) {
    nsTableRowGroupFrame* rgFrame = rowGroups[rgX];
    if (!excludeRowGroups.GetEntry(rgFrame)) {
      const nsFrameList& rowFrames = rgFrame->PrincipalChildList();
      for (nsFrameList::Enumerator rows(rowFrames); !rows.AtEnd(); rows.Next()) {
        if (NS_STYLE_DISPLAY_TABLE_ROW==rows.get()->StyleDisplay()->mDisplay) {
          ((nsTableRowFrame *)rows.get())->SetRowIndex(rowIndex);
          rowIndex++;
        }
      }
    }
  }
}
void nsTableFrame::InsertColGroups(int32_t                   aStartColIndex,
                                   const nsFrameList::Slice& aColGroups)
{
  int32_t colIndex = aStartColIndex;
  nsFrameList::Enumerator colGroups(aColGroups);
  for (; !colGroups.AtEnd(); colGroups.Next()) {
    MOZ_ASSERT(colGroups.get()->GetType() == nsGkAtoms::tableColGroupFrame);
    nsTableColGroupFrame* cgFrame =
      static_cast<nsTableColGroupFrame*>(colGroups.get());
    cgFrame->SetStartColumnIndex(colIndex);
    // XXXbz this sucks.  AddColsToTable will actually remove colgroups from
    // the list we're traversing!  Need to fix things here.  :( I guess this is
    // why the old code used pointer-to-last-frame as opposed to
    // pointer-to-frame-after-last....

    // How about dealing with this by storing a const reference to the
    // mNextSibling of the framelist's last frame, instead of storing a pointer
    // to the first-after-next frame?  Will involve making nsFrameList friend
    // of nsIFrame, but it's time for that anyway.
    cgFrame->AddColsToTable(colIndex, false,
                              colGroups.get()->PrincipalChildList());
    int32_t numCols = cgFrame->GetColCount();
    colIndex += numCols;
  }

  nsFrameList::Enumerator remainingColgroups = colGroups.GetUnlimitedEnumerator();
  if (!remainingColgroups.AtEnd()) {
    nsTableColGroupFrame::ResetColIndices(
      static_cast<nsTableColGroupFrame*>(remainingColgroups.get()), colIndex);
  }
}

void nsTableFrame::InsertCol(nsTableColFrame& aColFrame,
                             int32_t          aColIndex)
{
  mColFrames.InsertElementAt(aColIndex, &aColFrame);
  nsTableColType insertedColType = aColFrame.GetColType();
  int32_t numCacheCols = mColFrames.Length();
  nsTableCellMap* cellMap = GetCellMap();
  if (cellMap) {
    int32_t numMapCols = cellMap->GetColCount();
    if (numCacheCols > numMapCols) {
      bool removedFromCache = false;
      if (eColAnonymousCell != insertedColType) {
        nsTableColFrame* lastCol = mColFrames.ElementAt(numCacheCols - 1);
        if (lastCol) {
          nsTableColType lastColType = lastCol->GetColType();
          if (eColAnonymousCell == lastColType) {
            // remove the col from the cache
            mColFrames.RemoveElementAt(numCacheCols - 1);
            // remove the col from the eColGroupAnonymousCell col group
            nsTableColGroupFrame* lastColGroup = (nsTableColGroupFrame *)mColGroups.LastChild();
            if (lastColGroup) {
              lastColGroup->RemoveChild(*lastCol, false);

              // remove the col group if it is empty
              if (lastColGroup->GetColCount() <= 0) {
                mColGroups.DestroyFrame((nsIFrame*)lastColGroup);
              }
            }
            removedFromCache = true;
          }
        }
      }
      if (!removedFromCache) {
        cellMap->AddColsAtEnd(1);
      }
    }
  }
  // for now, just bail and recalc all of the collapsing borders
  if (IsBorderCollapse()) {
    nsIntRect damageArea(aColIndex, 0, 1, GetRowCount());
    AddBCDamageArea(damageArea);
  }
}

void nsTableFrame::RemoveCol(nsTableColGroupFrame* aColGroupFrame,
                             int32_t               aColIndex,
                             bool                  aRemoveFromCache,
                             bool                  aRemoveFromCellMap)
{
  if (aRemoveFromCache) {
    mColFrames.RemoveElementAt(aColIndex);
  }
  if (aRemoveFromCellMap) {
    nsTableCellMap* cellMap = GetCellMap();
    if (cellMap) {
      AppendAnonymousColFrames(1);
    }
  }
  // for now, just bail and recalc all of the collapsing borders
  if (IsBorderCollapse()) {
    nsIntRect damageArea(0, 0, GetColCount(), GetRowCount());
    AddBCDamageArea(damageArea);
  }
}

/** Get the cell map for this table frame.  It is not always mCellMap.
  * Only the first-in-flow has a legit cell map.
  */
nsTableCellMap*
nsTableFrame::GetCellMap() const
{
  return static_cast<nsTableFrame*>(FirstInFlow())->mCellMap;
}

// XXX this needs to be moved to nsCSSFrameConstructor
nsTableColGroupFrame*
nsTableFrame::CreateAnonymousColGroupFrame(nsTableColGroupType aColGroupType)
{
  nsIContent* colGroupContent = GetContent();
  nsPresContext* presContext = PresContext();
  nsIPresShell *shell = presContext->PresShell();

  nsRefPtr<nsStyleContext> colGroupStyle;
  colGroupStyle = shell->StyleSet()->
    ResolveAnonymousBoxStyle(nsCSSAnonBoxes::tableColGroup, mStyleContext);
  // Create a col group frame
  nsIFrame* newFrame = NS_NewTableColGroupFrame(shell, colGroupStyle);
  ((nsTableColGroupFrame *)newFrame)->SetColType(aColGroupType);
  newFrame->Init(colGroupContent, this, nullptr);
  return (nsTableColGroupFrame *)newFrame;
}

void
nsTableFrame::AppendAnonymousColFrames(int32_t aNumColsToAdd)
{
  // get the last col group frame
  nsTableColGroupFrame* colGroupFrame =
    static_cast<nsTableColGroupFrame*>(mColGroups.LastChild());

  if (!colGroupFrame ||
      (colGroupFrame->GetColType() != eColGroupAnonymousCell)) {
    int32_t colIndex = (colGroupFrame) ?
                        colGroupFrame->GetStartColumnIndex() +
                        colGroupFrame->GetColCount() : 0;
    colGroupFrame = CreateAnonymousColGroupFrame(eColGroupAnonymousCell);
    if (!colGroupFrame) {
      return;
    }
    // add the new frame to the child list
    mColGroups.AppendFrame(this, colGroupFrame);
    colGroupFrame->SetStartColumnIndex(colIndex);
  }
  AppendAnonymousColFrames(colGroupFrame, aNumColsToAdd, eColAnonymousCell,
                           true);

}

// XXX this needs to be moved to nsCSSFrameConstructor
// Right now it only creates the col frames at the end
void
nsTableFrame::AppendAnonymousColFrames(nsTableColGroupFrame* aColGroupFrame,
                                       int32_t               aNumColsToAdd,
                                       nsTableColType        aColType,
                                       bool                  aAddToTable)
{
  NS_PRECONDITION(aColGroupFrame, "null frame");
  NS_PRECONDITION(aColType != eColAnonymousCol, "Shouldn't happen");

  nsIPresShell *shell = PresContext()->PresShell();

  // Get the last col frame
  nsFrameList newColFrames;

  int32_t startIndex = mColFrames.Length();
  int32_t lastIndex  = startIndex + aNumColsToAdd - 1;

  for (int32_t childX = startIndex; childX <= lastIndex; childX++) {
    nsIContent* iContent;
    nsRefPtr<nsStyleContext> styleContext;
    nsStyleContext* parentStyleContext;

    // all anonymous cols that we create here use a pseudo style context of the
    // col group
    iContent = aColGroupFrame->GetContent();
    parentStyleContext = aColGroupFrame->StyleContext();
    styleContext = shell->StyleSet()->
      ResolveAnonymousBoxStyle(nsCSSAnonBoxes::tableCol, parentStyleContext);
    // ASSERTION to check for bug 54454 sneaking back in...
    NS_ASSERTION(iContent, "null content in CreateAnonymousColFrames");

    // create the new col frame
    nsIFrame* colFrame = NS_NewTableColFrame(shell, styleContext);
    ((nsTableColFrame *) colFrame)->SetColType(aColType);
    colFrame->Init(iContent, aColGroupFrame, nullptr);

    newColFrames.AppendFrame(nullptr, colFrame);
  }
  nsFrameList& cols = aColGroupFrame->GetWritableChildList();
  nsIFrame* oldLastCol = cols.LastChild();
  const nsFrameList::Slice& newCols =
    cols.InsertFrames(nullptr, oldLastCol, newColFrames);
  if (aAddToTable) {
    // get the starting col index in the cache
    int32_t startColIndex;
    if (oldLastCol) {
      startColIndex =
        static_cast<nsTableColFrame*>(oldLastCol)->GetColIndex() + 1;
    } else {
      startColIndex = aColGroupFrame->GetStartColumnIndex();
    }

    aColGroupFrame->AddColsToTable(startColIndex, true, newCols);
  }
}

void
nsTableFrame::MatchCellMapToColCache(nsTableCellMap* aCellMap)
{
  int32_t numColsInMap   = GetColCount();
  int32_t numColsInCache = mColFrames.Length();
  int32_t numColsToAdd = numColsInMap - numColsInCache;
  if (numColsToAdd > 0) {
    // this sets the child list, updates the col cache and cell map
    AppendAnonymousColFrames(numColsToAdd);
  }
  if (numColsToAdd < 0) {
    int32_t numColsNotRemoved = DestroyAnonymousColFrames(-numColsToAdd);
    // if the cell map has fewer cols than the cache, correct it
    if (numColsNotRemoved > 0) {
      aCellMap->AddColsAtEnd(numColsNotRemoved);
    }
  }
  if (numColsToAdd && HasZeroColSpans()) {
    SetNeedColSpanExpansion(true);
  }
  if (NeedColSpanExpansion()) {
    // This flag can be set in two ways -- either by changing
    // the number of columns (that happens in the block above),
    // or by adding a cell with colspan="0" to the cellmap.  To
    // handle the latter case we need to explicitly check the
    // flag here -- it may be set even if the number of columns
    // did not change.
    //
    // @see nsCellMap::AppendCell

    aCellMap->ExpandZeroColSpans();
  }
}

void
nsTableFrame::DidResizeColumns()
{
  NS_PRECONDITION(!GetPrevInFlow(),
                  "should only be called on first-in-flow");
  if (mBits.mResizedColumns)
    return; // already marked

  for (nsTableFrame *f = this; f;
       f = static_cast<nsTableFrame*>(f->GetNextInFlow()))
    f->mBits.mResizedColumns = true;
}

void
nsTableFrame::AppendCell(nsTableCellFrame& aCellFrame,
                         int32_t           aRowIndex)
{
  nsTableCellMap* cellMap = GetCellMap();
  if (cellMap) {
    nsIntRect damageArea(0,0,0,0);
    cellMap->AppendCell(aCellFrame, aRowIndex, true, damageArea);
    MatchCellMapToColCache(cellMap);
    if (IsBorderCollapse()) {
      AddBCDamageArea(damageArea);
    }
  }
}

void nsTableFrame::InsertCells(nsTArray<nsTableCellFrame*>& aCellFrames,
                               int32_t                      aRowIndex,
                               int32_t                      aColIndexBefore)
{
  nsTableCellMap* cellMap = GetCellMap();
  if (cellMap) {
    nsIntRect damageArea(0,0,0,0);
    cellMap->InsertCells(aCellFrames, aRowIndex, aColIndexBefore, damageArea);
    MatchCellMapToColCache(cellMap);
    if (IsBorderCollapse()) {
      AddBCDamageArea(damageArea);
    }
  }
}

// this removes the frames from the col group and table, but not the cell map
int32_t
nsTableFrame::DestroyAnonymousColFrames(int32_t aNumFrames)
{
  // only remove cols that are of type eTypeAnonymous cell (they are at the end)
  int32_t endIndex   = mColFrames.Length() - 1;
  int32_t startIndex = (endIndex - aNumFrames) + 1;
  int32_t numColsRemoved = 0;
  for (int32_t colX = endIndex; colX >= startIndex; colX--) {
    nsTableColFrame* colFrame = GetColFrame(colX);
    if (colFrame && (eColAnonymousCell == colFrame->GetColType())) {
      nsTableColGroupFrame* cgFrame =
        static_cast<nsTableColGroupFrame*>(colFrame->GetParent());
      // remove the frame from the colgroup
      cgFrame->RemoveChild(*colFrame, false);
      // remove the frame from the cache, but not the cell map
      RemoveCol(nullptr, colX, true, false);
      numColsRemoved++;
    }
    else {
      break;
    }
  }
  return (aNumFrames - numColsRemoved);
}

void nsTableFrame::RemoveCell(nsTableCellFrame* aCellFrame,
                              int32_t           aRowIndex)
{
  nsTableCellMap* cellMap = GetCellMap();
  if (cellMap) {
    nsIntRect damageArea(0,0,0,0);
    cellMap->RemoveCell(aCellFrame, aRowIndex, damageArea);
    MatchCellMapToColCache(cellMap);
    if (IsBorderCollapse()) {
      AddBCDamageArea(damageArea);
    }
  }
}

int32_t
nsTableFrame::GetStartRowIndex(nsTableRowGroupFrame* aRowGroupFrame)
{
  RowGroupArray orderedRowGroups;
  OrderRowGroups(orderedRowGroups);

  int32_t rowIndex = 0;
  for (uint32_t rgIndex = 0; rgIndex < orderedRowGroups.Length(); rgIndex++) {
    nsTableRowGroupFrame* rgFrame = orderedRowGroups[rgIndex];
    if (rgFrame == aRowGroupFrame) {
      break;
    }
    int32_t numRows = rgFrame->GetRowCount();
    rowIndex += numRows;
  }
  return rowIndex;
}

// this cannot extend beyond a single row group
void nsTableFrame::AppendRows(nsTableRowGroupFrame*       aRowGroupFrame,
                              int32_t                     aRowIndex,
                              nsTArray<nsTableRowFrame*>& aRowFrames)
{
  nsTableCellMap* cellMap = GetCellMap();
  if (cellMap) {
    int32_t absRowIndex = GetStartRowIndex(aRowGroupFrame) + aRowIndex;
    InsertRows(aRowGroupFrame, aRowFrames, absRowIndex, true);
  }
}

// this cannot extend beyond a single row group
int32_t
nsTableFrame::InsertRows(nsTableRowGroupFrame*       aRowGroupFrame,
                         nsTArray<nsTableRowFrame*>& aRowFrames,
                         int32_t                     aRowIndex,
                         bool                        aConsiderSpans)
{
#ifdef DEBUG_TABLE_CELLMAP
  printf("=== insertRowsBefore firstRow=%d \n", aRowIndex);
  Dump(true, false, true);
#endif

  int32_t numColsToAdd = 0;
  nsTableCellMap* cellMap = GetCellMap();
  if (cellMap) {
    nsIntRect damageArea(0,0,0,0);
    int32_t origNumRows = cellMap->GetRowCount();
    int32_t numNewRows = aRowFrames.Length();
    cellMap->InsertRows(aRowGroupFrame, aRowFrames, aRowIndex, aConsiderSpans, damageArea);
    MatchCellMapToColCache(cellMap);
    if (aRowIndex < origNumRows) {
      AdjustRowIndices(aRowIndex, numNewRows);
    }
    // assign the correct row indices to the new rows. If they were adjusted above
    // it may not have been done correctly because each row is constructed with index 0
    for (int32_t rowY = 0; rowY < numNewRows; rowY++) {
      nsTableRowFrame* rowFrame = aRowFrames.ElementAt(rowY);
      rowFrame->SetRowIndex(aRowIndex + rowY);
    }
    if (IsBorderCollapse()) {
      AddBCDamageArea(damageArea);
    }
  }
#ifdef DEBUG_TABLE_CELLMAP
  printf("=== insertRowsAfter \n");
  Dump(true, false, true);
#endif

  return numColsToAdd;
}

// this cannot extend beyond a single row group
void nsTableFrame::RemoveRows(nsTableRowFrame& aFirstRowFrame,
                              int32_t          aNumRowsToRemove,
                              bool             aConsiderSpans)
{
#ifdef TBD_OPTIMIZATION
  // decide if we need to rebalance. we have to do this here because the row group
  // cannot do it when it gets the dirty reflow corresponding to the frame being destroyed
  bool stopTelling = false;
  for (nsIFrame* kidFrame = aFirstFrame.FirstChild(); (kidFrame && !stopAsking);
       kidFrame = kidFrame->GetNextSibling()) {
    nsTableCellFrame *cellFrame = do_QueryFrame(kidFrame);
    if (cellFrame) {
      stopTelling = tableFrame->CellChangedWidth(*cellFrame, cellFrame->GetPass1MaxElementWidth(),
                                                 cellFrame->GetMaximumWidth(), true);
    }
  }
  // XXX need to consider what happens if there are cells that have rowspans
  // into the deleted row. Need to consider moving rows if a rebalance doesn't happen
#endif

  int32_t firstRowIndex = aFirstRowFrame.GetRowIndex();
#ifdef DEBUG_TABLE_CELLMAP
  printf("=== removeRowsBefore firstRow=%d numRows=%d\n", firstRowIndex, aNumRowsToRemove);
  Dump(true, false, true);
#endif
  nsTableCellMap* cellMap = GetCellMap();
  if (cellMap) {
    nsIntRect damageArea(0,0,0,0);
    cellMap->RemoveRows(firstRowIndex, aNumRowsToRemove, aConsiderSpans, damageArea);
    MatchCellMapToColCache(cellMap);
    if (IsBorderCollapse()) {
      AddBCDamageArea(damageArea);
    }
  }
  AdjustRowIndices(firstRowIndex, -aNumRowsToRemove);
#ifdef DEBUG_TABLE_CELLMAP
  printf("=== removeRowsAfter\n");
  Dump(true, true, true);
#endif
}

// collect the rows ancestors of aFrame
int32_t
nsTableFrame::CollectRows(nsIFrame*                   aFrame,
                          nsTArray<nsTableRowFrame*>& aCollection)
{
  NS_PRECONDITION(aFrame, "null frame");
  int32_t numRows = 0;
  nsIFrame* childFrame = aFrame->GetFirstPrincipalChild();
  while (childFrame) {
    aCollection.AppendElement(static_cast<nsTableRowFrame*>(childFrame));
    numRows++;
    childFrame = childFrame->GetNextSibling();
  }
  return numRows;
}

void
nsTableFrame::InsertRowGroups(const nsFrameList::Slice& aRowGroups)
{
#ifdef DEBUG_TABLE_CELLMAP
  printf("=== insertRowGroupsBefore\n");
  Dump(true, false, true);
#endif
  nsTableCellMap* cellMap = GetCellMap();
  if (cellMap) {
    RowGroupArray orderedRowGroups;
    OrderRowGroups(orderedRowGroups);

    nsAutoTArray<nsTableRowFrame*, 8> rows;
    // Loop over the rowgroups and check if some of them are new, if they are
    // insert cellmaps in the order that is predefined by OrderRowGroups,
    // XXXbz this code is O(N*M) where N is number of new rowgroups
    // and M is number of rowgroups we have!
    uint32_t rgIndex;
    for (rgIndex = 0; rgIndex < orderedRowGroups.Length(); rgIndex++) {
      for (nsFrameList::Enumerator rowgroups(aRowGroups); !rowgroups.AtEnd();
           rowgroups.Next()) {
        if (orderedRowGroups[rgIndex] == rowgroups.get()) {
          nsTableRowGroupFrame* priorRG =
            (0 == rgIndex) ? nullptr : orderedRowGroups[rgIndex - 1];
          // create and add the cell map for the row group
          cellMap->InsertGroupCellMap(orderedRowGroups[rgIndex], priorRG);

          break;
        }
      }
    }
    cellMap->Synchronize(this);
    ResetRowIndices(aRowGroups);

    //now that the cellmaps are reordered too insert the rows
    for (rgIndex = 0; rgIndex < orderedRowGroups.Length(); rgIndex++) {
      for (nsFrameList::Enumerator rowgroups(aRowGroups); !rowgroups.AtEnd();
           rowgroups.Next()) {
        if (orderedRowGroups[rgIndex] == rowgroups.get()) {
          nsTableRowGroupFrame* priorRG =
            (0 == rgIndex) ? nullptr : orderedRowGroups[rgIndex - 1];
          // collect the new row frames in an array and add them to the table
          int32_t numRows = CollectRows(rowgroups.get(), rows);
          if (numRows > 0) {
            int32_t rowIndex = 0;
            if (priorRG) {
              int32_t priorNumRows = priorRG->GetRowCount();
              rowIndex = priorRG->GetStartRowIndex() + priorNumRows;
            }
            InsertRows(orderedRowGroups[rgIndex], rows, rowIndex, true);
            rows.Clear();
          }
          break;
        }
      }
    }

  }
#ifdef DEBUG_TABLE_CELLMAP
  printf("=== insertRowGroupsAfter\n");
  Dump(true, true, true);
#endif
}


/////////////////////////////////////////////////////////////////////////////
// Child frame enumeration

const nsFrameList&
nsTableFrame::GetChildList(ChildListID aListID) const
{
  if (aListID == kColGroupList) {
    return mColGroups;
  }
  return nsContainerFrame::GetChildList(aListID);
}

void
nsTableFrame::GetChildLists(nsTArray<ChildList>* aLists) const
{
  nsContainerFrame::GetChildLists(aLists);
  mColGroups.AppendIfNonempty(aLists, kColGroupList);
}

nsRect
nsDisplayTableItem::GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) {
  *aSnap = false;
  return mFrame->GetVisualOverflowRectRelativeToSelf() + ToReferenceFrame();
}

void
nsDisplayTableItem::UpdateForFrameBackground(nsIFrame* aFrame)
{
  nsStyleContext *bgSC;
  if (!nsCSSRendering::FindBackground(aFrame, &bgSC))
    return;
  if (!bgSC->StyleBackground()->HasFixedBackground())
    return;

  mPartHasFixedBackground = true;
}

nsDisplayItemGeometry*
nsDisplayTableItem::AllocateGeometry(nsDisplayListBuilder* aBuilder)
{
  return new nsDisplayTableItemGeometry(this, aBuilder,
      mFrame->GetOffsetTo(mFrame->PresContext()->PresShell()->GetRootFrame()));
}

void
nsDisplayTableItem::ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                              const nsDisplayItemGeometry* aGeometry,
                                              nsRegion *aInvalidRegion)
{
  auto geometry =
    static_cast<const nsDisplayTableItemGeometry*>(aGeometry);

  bool invalidateForAttachmentFixed = false;
  if (mPartHasFixedBackground) {
    nsPoint frameOffsetToViewport = mFrame->GetOffsetTo(
        mFrame->PresContext()->PresShell()->GetRootFrame());
    invalidateForAttachmentFixed =
        frameOffsetToViewport != geometry->mFrameOffsetToViewport;
  }

  if (invalidateForAttachmentFixed ||
      (aBuilder->ShouldSyncDecodeImages() &&
       geometry->ShouldInvalidateToSyncDecodeImages())) {
    bool snap;
    aInvalidRegion->Or(*aInvalidRegion, GetBounds(aBuilder, &snap));
  }

  nsDisplayItem::ComputeInvalidationRegion(aBuilder, aGeometry, aInvalidRegion);
}

class nsDisplayTableBorderBackground : public nsDisplayTableItem {
public:
  nsDisplayTableBorderBackground(nsDisplayListBuilder* aBuilder,
                                 nsTableFrame* aFrame) :
    nsDisplayTableItem(aBuilder, aFrame) {
    MOZ_COUNT_CTOR(nsDisplayTableBorderBackground);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayTableBorderBackground() {
    MOZ_COUNT_DTOR(nsDisplayTableBorderBackground);
  }
#endif

  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     nsRenderingContext* aCtx) override;
  NS_DISPLAY_DECL_NAME("TableBorderBackground", TYPE_TABLE_BORDER_BACKGROUND)
};

#ifdef DEBUG
static bool
IsFrameAllowedInTable(nsIAtom* aType)
{
  return IS_TABLE_CELL(aType) ||
         nsGkAtoms::tableRowFrame == aType ||
         nsGkAtoms::tableRowGroupFrame == aType ||
         nsGkAtoms::scrollFrame == aType ||
         nsGkAtoms::tableFrame == aType ||
         nsGkAtoms::tableColFrame == aType ||
         nsGkAtoms::tableColGroupFrame == aType;
}
#endif

void
nsDisplayTableBorderBackground::Paint(nsDisplayListBuilder* aBuilder,
                                      nsRenderingContext* aCtx)
{
  DrawResult result = static_cast<nsTableFrame*>(mFrame)->
    PaintTableBorderBackground(*aCtx, mVisibleRect,
                               ToReferenceFrame(),
                               aBuilder->GetBackgroundPaintFlags());

  nsDisplayTableItemGeometry::UpdateDrawResult(this, result);
}

static int32_t GetTablePartRank(nsDisplayItem* aItem)
{
  nsIAtom* type = aItem->Frame()->GetType();
  if (type == nsGkAtoms::tableFrame)
    return 0;
  if (type == nsGkAtoms::tableRowGroupFrame)
    return 1;
  if (type == nsGkAtoms::tableRowFrame)
    return 2;
  return 3;
}

static bool CompareByTablePartRank(nsDisplayItem* aItem1, nsDisplayItem* aItem2,
                                     void* aClosure)
{
  return GetTablePartRank(aItem1) <= GetTablePartRank(aItem2);
}

/* static */ void
nsTableFrame::GenericTraversal(nsDisplayListBuilder* aBuilder, nsFrame* aFrame,
                               const nsRect& aDirtyRect, const nsDisplayListSet& aLists)
{
  // This is similar to what nsContainerFrame::BuildDisplayListForNonBlockChildren
  // does, except that we allow the children's background and borders to go
  // in our BorderBackground list. This doesn't really affect background
  // painting --- the children won't actually draw their own backgrounds
  // because the nsTableFrame already drew them, unless a child has its own
  // stacking context, in which case the child won't use its passed-in
  // BorderBackground list anyway. It does affect cell borders though; this
  // lets us get cell borders into the nsTableFrame's BorderBackground list.
  nsIFrame* kid = aFrame->GetFirstPrincipalChild();
  while (kid) {
    aFrame->BuildDisplayListForChild(aBuilder, kid, aDirtyRect, aLists);
    kid = kid->GetNextSibling();
  }
}

/* static */ void
nsTableFrame::DisplayGenericTablePart(nsDisplayListBuilder* aBuilder,
                                      nsFrame* aFrame,
                                      const nsRect& aDirtyRect,
                                      const nsDisplayListSet& aLists,
                                      nsDisplayTableItem* aDisplayItem,
                                      DisplayGenericTablePartTraversal aTraversal)
{
  nsDisplayList eventsBorderBackground;
  // If we need to sort the event backgrounds, then we'll put descendants'
  // display items into their own set of lists.
  bool sortEventBackgrounds = aDisplayItem && aBuilder->IsForEventDelivery();
  nsDisplayListCollection separatedCollection;
  const nsDisplayListSet* lists = sortEventBackgrounds ? &separatedCollection : &aLists;

  nsAutoPushCurrentTableItem pushTableItem;
  if (aDisplayItem) {
    pushTableItem.Push(aBuilder, aDisplayItem);
  }

  if (aFrame->IsVisibleForPainting(aBuilder)) {
    nsDisplayTableItem* currentItem = aBuilder->GetCurrentTableItem();
    // currentItem may be null, when none of the table parts have a
    // background or border
    if (currentItem) {
      currentItem->UpdateForFrameBackground(aFrame);
    }

    // Paint the outset box-shadows for the table frames
    bool hasBoxShadow = aFrame->StyleBorder()->mBoxShadow != nullptr;
    if (hasBoxShadow) {
      lists->BorderBackground()->AppendNewToTop(
        new (aBuilder) nsDisplayBoxShadowOuter(aBuilder, aFrame));
    }

    // Create dedicated background display items per-frame when we're
    // handling events.
    // XXX how to handle collapsed borders?
    if (aBuilder->IsForEventDelivery()) {
      nsDisplayBackgroundImage::AppendBackgroundItemsToTop(aBuilder, aFrame,
                                                           lists->BorderBackground());
    }

    // Paint the inset box-shadows for the table frames
    if (hasBoxShadow) {
      lists->BorderBackground()->AppendNewToTop(
        new (aBuilder) nsDisplayBoxShadowInner(aBuilder, aFrame));
    }
  }

  aTraversal(aBuilder, aFrame, aDirtyRect, *lists);

  if (sortEventBackgrounds) {
    // Ensure that the table frame event background goes before the
    // table rowgroups event backgrounds, before the table row event backgrounds,
    // before everything else (cells and their blocks)
    separatedCollection.BorderBackground()->Sort(aBuilder, CompareByTablePartRank, nullptr);
    separatedCollection.MoveTo(aLists);
  }

  aFrame->DisplayOutline(aBuilder, aLists);
}

static bool
AnyTablePartHasBorderOrBackground(nsIFrame* aStart, nsIFrame* aEnd)
{
  for (nsIFrame* f = aStart; f != aEnd; f = f->GetNextSibling()) {
    NS_ASSERTION(IsFrameAllowedInTable(f->GetType()), "unexpected frame type");

    if (FrameHasBorderOrBackground(f))
      return true;

    nsTableCellFrame *cellFrame = do_QueryFrame(f);
    if (cellFrame)
      continue;

    if (AnyTablePartHasBorderOrBackground(f->PrincipalChildList().FirstChild(), nullptr))
      return true;
  }

  return false;
}

static void
UpdateItemForColGroupBackgrounds(nsDisplayTableItem* item,
                                 const nsFrameList& aFrames) {
  for (nsFrameList::Enumerator e(aFrames); !e.AtEnd(); e.Next()) {
    nsTableColGroupFrame* cg = static_cast<nsTableColGroupFrame*>(e.get());
    item->UpdateForFrameBackground(cg);
    for (nsTableColFrame* colFrame = cg->GetFirstColumn(); colFrame;
         colFrame = colFrame->GetNextCol()) {
      item->UpdateForFrameBackground(colFrame);
    }
  }
}

// table paint code is concerned primarily with borders and bg color
// SEC: TODO: adjust the rect for captions
void
nsTableFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                               const nsRect&           aDirtyRect,
                               const nsDisplayListSet& aLists)
{
  DO_GLOBAL_REFLOW_COUNT_DSP_COLOR("nsTableFrame", NS_RGB(255,128,255));

  nsDisplayTableItem* item = nullptr;
  if (IsVisibleInSelection(aBuilder)) {
    if (StyleVisibility()->IsVisible()) {
      nsMargin deflate = GetDeflationForBackground(PresContext());
      // If 'deflate' is (0,0,0,0) then we can paint the table background
      // in its own display item, so do that to take advantage of
      // opacity and visibility optimizations
      if (deflate == nsMargin(0, 0, 0, 0)) {
        DisplayBackgroundUnconditional(aBuilder, aLists, false);
      }
    }

    // This background is created if any of the table parts are visible,
    // or if we're doing event handling (since DisplayGenericTablePart
    // needs the item for the |sortEventBackgrounds|-dependent code).
    // Specific visibility decisions are delegated to the table background
    // painter, which handles borders and backgrounds for the table.
    if (aBuilder->IsForEventDelivery() ||
        AnyTablePartHasBorderOrBackground(this, GetNextSibling()) ||
        AnyTablePartHasBorderOrBackground(mColGroups.FirstChild(), nullptr)) {
      item = new (aBuilder) nsDisplayTableBorderBackground(aBuilder, this);
      aLists.BorderBackground()->AppendNewToTop(item);
    }
  }
  DisplayGenericTablePart(aBuilder, this, aDirtyRect, aLists, item);
  if (item) {
    UpdateItemForColGroupBackgrounds(item, mColGroups);
  }
}

nsMargin
nsTableFrame::GetDeflationForBackground(nsPresContext* aPresContext) const
{
  if (eCompatibility_NavQuirks != aPresContext->CompatibilityMode() ||
      !IsBorderCollapse())
    return nsMargin(0,0,0,0);

  return GetOuterBCBorder();
}

// XXX We don't put the borders and backgrounds in tree order like we should.
// That requires some major surgery which we aren't going to do right now.
DrawResult
nsTableFrame::PaintTableBorderBackground(nsRenderingContext& aRenderingContext,
                                         const nsRect& aDirtyRect,
                                         nsPoint aPt, uint32_t aBGPaintFlags)
{
  nsPresContext* presContext = PresContext();

  TableBackgroundPainter painter(this, TableBackgroundPainter::eOrigin_Table,
                                 presContext, aRenderingContext,
                                 aDirtyRect, aPt, aBGPaintFlags);
  nsMargin deflate = GetDeflationForBackground(presContext);
  // If 'deflate' is (0,0,0,0) then we'll paint the table background
  // in a separate display item, so don't do it here.
  DrawResult result =
    painter.PaintTable(this, deflate, deflate != nsMargin(0, 0, 0, 0));

  if (StyleVisibility()->IsVisible()) {
    if (!IsBorderCollapse()) {
      Sides skipSides = GetSkipSides();
      nsRect rect(aPt, mRect.Size());
      nsCSSRendering::PaintBorder(presContext, aRenderingContext, this,
                                  aDirtyRect, rect, mStyleContext, skipSides);
    }
    else {
      gfxContext* ctx = aRenderingContext.ThebesContext();

      gfxPoint devPixelOffset =
        nsLayoutUtils::PointToGfxPoint(aPt,
                                       PresContext()->AppUnitsPerDevPixel());

      // XXX we should probably get rid of this translation at some stage
      // But that would mean modifying PaintBCBorders, ugh
      gfxContextMatrixAutoSaveRestore autoSR(ctx);
      ctx->SetMatrix(ctx->CurrentMatrix().Translate(devPixelOffset));

      PaintBCBorders(aRenderingContext, aDirtyRect - aPt);
    }
  }

  return result;
}

nsIFrame::LogicalSides
nsTableFrame::GetLogicalSkipSides(const nsHTMLReflowState* aReflowState) const
{
  if (MOZ_UNLIKELY(StyleBorder()->mBoxDecorationBreak ==
                     NS_STYLE_BOX_DECORATION_BREAK_CLONE)) {
    return LogicalSides();
  }

  LogicalSides skip;
  // frame attribute was accounted for in nsHTMLTableElement::MapTableBorderInto
  // account for pagination
  if (nullptr != GetPrevInFlow()) {
    skip |= eLogicalSideBitsBStart;
  }
  if (nullptr != GetNextInFlow()) {
    skip |= eLogicalSideBitsBEnd;
  }
  return skip;
}

void
nsTableFrame::SetColumnDimensions(nscoord         aHeight,
                                  const nsMargin& aBorderPadding)
{
  nscoord colHeight = aHeight -= aBorderPadding.top + aBorderPadding.bottom +
                                 GetRowSpacing(-1) +
                                 GetRowSpacing(GetRowCount());

  nsTableIterator iter(mColGroups);
  nsIFrame* colGroupFrame = iter.First();
  bool tableIsLTR = StyleVisibility()->mDirection == NS_STYLE_DIRECTION_LTR;
  int32_t colX =tableIsLTR ? 0 : std::max(0, GetColCount() - 1);
  nscoord cellSpacingX = GetColSpacing(colX);
  int32_t tableColIncr = tableIsLTR ? 1 : -1;
  nsPoint colGroupOrigin(aBorderPadding.left + GetColSpacing(-1),
                         aBorderPadding.top + GetRowSpacing(-1));
  while (colGroupFrame) {
    MOZ_ASSERT(colGroupFrame->GetType() == nsGkAtoms::tableColGroupFrame);
    nscoord colGroupWidth = 0;
    nsTableIterator iterCol(*colGroupFrame);
    nsIFrame* colFrame = iterCol.First();
    nsPoint colOrigin(0,0);
    while (colFrame) {
      if (NS_STYLE_DISPLAY_TABLE_COLUMN ==
          colFrame->StyleDisplay()->mDisplay) {
        NS_ASSERTION(colX < GetColCount(), "invalid number of columns");
        nscoord colWidth = GetColumnWidth(colX);
        nsRect colRect(colOrigin.x, colOrigin.y, colWidth, colHeight);
        colFrame->SetRect(colRect);
        cellSpacingX = GetColSpacing(colX);
        colOrigin.x += colWidth + cellSpacingX;
        colGroupWidth += colWidth + cellSpacingX;
        colX += tableColIncr;
      }
      colFrame = iterCol.Next();
    }
    if (colGroupWidth) {
      colGroupWidth -= cellSpacingX;
    }

    nsRect colGroupRect(colGroupOrigin.x, colGroupOrigin.y, colGroupWidth, colHeight);
    colGroupFrame->SetRect(colGroupRect);
    colGroupFrame = iter.Next();
    colGroupOrigin.x += colGroupWidth + cellSpacingX;
  }
}

// SEC: TODO need to worry about continuing frames prev/next in flow for splitting across pages.

// XXX this could be made more general to handle row modifications that change the
// table height, but first we need to scrutinize every Invalidate
void
nsTableFrame::ProcessRowInserted(nscoord aNewHeight)
{
  SetRowInserted(false); // reset the bit that got us here
  nsTableFrame::RowGroupArray rowGroups;
  OrderRowGroups(rowGroups);
  // find the row group containing the inserted row
  for (uint32_t rgX = 0; rgX < rowGroups.Length(); rgX++) {
    nsTableRowGroupFrame* rgFrame = rowGroups[rgX];
    NS_ASSERTION(rgFrame, "Must have rgFrame here");
    nsIFrame* childFrame = rgFrame->GetFirstPrincipalChild();
    // find the row that was inserted first
    while (childFrame) {
      nsTableRowFrame *rowFrame = do_QueryFrame(childFrame);
      if (rowFrame) {
        if (rowFrame->IsFirstInserted()) {
          rowFrame->SetFirstInserted(false);
          // damage the table from the 1st row inserted to the end of the table
          nsIFrame::InvalidateFrame();
          // XXXbz didn't we do this up front?  Why do we need to do it again?
          SetRowInserted(false);
          return; // found it, so leave
        }
      }
      childFrame = childFrame->GetNextSibling();
    }
  }
}

/* virtual */ void
nsTableFrame::MarkIntrinsicISizesDirty()
{
  nsITableLayoutStrategy* tls = LayoutStrategy();
  if (MOZ_UNLIKELY(!tls)) {
    // This is a FrameNeedsReflow() from nsBlockFrame::RemoveFrame()
    // walking up the ancestor chain in a table next-in-flow.  In this case
    // our original first-in-flow (which owns the TableLayoutStrategy) has
    // already been destroyed and unhooked from the flow chain and thusly
    // LayoutStrategy() returns null.  All the frames in the flow will be
    // destroyed so no need to mark anything dirty here.  See bug 595758.
    return;
  }
  tls->MarkIntrinsicISizesDirty();

  // XXXldb Call SetBCDamageArea?

  nsContainerFrame::MarkIntrinsicISizesDirty();
}

/* virtual */ nscoord
nsTableFrame::GetMinISize(nsRenderingContext *aRenderingContext)
{
  if (NeedToCalcBCBorders())
    CalcBCBorders();

  ReflowColGroups(aRenderingContext);

  return LayoutStrategy()->GetMinISize(aRenderingContext);
}

/* virtual */ nscoord
nsTableFrame::GetPrefISize(nsRenderingContext *aRenderingContext)
{
  if (NeedToCalcBCBorders())
    CalcBCBorders();

  ReflowColGroups(aRenderingContext);

  return LayoutStrategy()->GetPrefISize(aRenderingContext, false);
}

/* virtual */ nsIFrame::IntrinsicISizeOffsetData
nsTableFrame::IntrinsicISizeOffsets(nsRenderingContext* aRenderingContext)
{
  IntrinsicISizeOffsetData result =
    nsContainerFrame::IntrinsicISizeOffsets(aRenderingContext);

  result.hMargin = 0;
  result.hPctMargin = 0;

  if (IsBorderCollapse()) {
    result.hPadding = 0;
    result.hPctPadding = 0;

    nsMargin outerBC = GetIncludedOuterBCBorder();
    result.hBorder = outerBC.LeftRight();
  }

  return result;
}

/* virtual */
LogicalSize
nsTableFrame::ComputeSize(nsRenderingContext *aRenderingContext,
                          WritingMode aWM,
                          const LogicalSize& aCBSize,
                          nscoord aAvailableISize,
                          const LogicalSize& aMargin,
                          const LogicalSize& aBorder,
                          const LogicalSize& aPadding,
                          ComputeSizeFlags aFlags)
{
  LogicalSize result =
    nsContainerFrame::ComputeSize(aRenderingContext, aWM,
                                  aCBSize, aAvailableISize,
                                  aMargin, aBorder, aPadding, aFlags);

  // XXX The code below doesn't make sense if the caller's writing mode
  // is orthogonal to this frame's. Not sure yet what should happen then;
  // for now, just bail out.
  if (aWM.IsVertical() != GetWritingMode().IsVertical()) {
    return result;
  }

  // If we're a container for font size inflation, then shrink
  // wrapping inside of us should not apply font size inflation.
  AutoMaybeDisableFontInflation an(this);

  // Tables never shrink below their min inline-size.
  nscoord minISize = GetMinISize(aRenderingContext);
  if (minISize > result.ISize(aWM)) {
    result.ISize(aWM) = minISize;
  }

  return result;
}

nscoord
nsTableFrame::TableShrinkWidthToFit(nsRenderingContext *aRenderingContext,
                                    nscoord aWidthInCB)
{
  // If we're a container for font size inflation, then shrink
  // wrapping inside of us should not apply font size inflation.
  AutoMaybeDisableFontInflation an(this);

  nscoord result;
  nscoord minWidth = GetMinISize(aRenderingContext);
  if (minWidth > aWidthInCB) {
    result = minWidth;
  } else {
    // Tables shrink width to fit with a slightly different algorithm
    // from the one they use for their intrinsic widths (the difference
    // relates to handling of percentage widths on columns).  So this
    // function differs from nsFrame::ShrinkWidthToFit by only the
    // following line.
    // Since we've already called GetMinISize, we don't need to do any
    // of the other stuff GetPrefISize does.
    nscoord prefWidth =
      LayoutStrategy()->GetPrefISize(aRenderingContext, true);
    if (prefWidth > aWidthInCB) {
      result = aWidthInCB;
    } else {
      result = prefWidth;
    }
  }
  return result;
}

/* virtual */
LogicalSize
nsTableFrame::ComputeAutoSize(nsRenderingContext *aRenderingContext,
                              WritingMode aWM,
                              const LogicalSize& aCBSize,
                              nscoord aAvailableISize,
                              const LogicalSize& aMargin,
                              const LogicalSize& aBorder,
                              const LogicalSize& aPadding,
                              bool aShrinkWrap)
{
  // Tables always shrink-wrap.
  nscoord cbBased = aAvailableISize - aMargin.ISize(aWM) - aBorder.ISize(aWM) -
                    aPadding.ISize(aWM);
  return LogicalSize(aWM, TableShrinkWidthToFit(aRenderingContext, cbBased),
                     NS_UNCONSTRAINEDSIZE);
}

// Return true if aParentReflowState.frame or any of its ancestors within
// the containing table have non-auto height. (e.g. pct or fixed height)
bool
nsTableFrame::AncestorsHaveStyleHeight(const nsHTMLReflowState& aParentReflowState)
{
  for (const nsHTMLReflowState* rs = &aParentReflowState;
       rs && rs->frame; rs = rs->parentReflowState) {
    nsIAtom* frameType = rs->frame->GetType();
    if (IS_TABLE_CELL(frameType)                     ||
        (nsGkAtoms::tableRowFrame      == frameType) ||
        (nsGkAtoms::tableRowGroupFrame == frameType)) {
      const nsStyleCoord &height = rs->mStylePosition->mHeight;
      // calc() with percentages treated like 'auto' on internal table elements
      if (height.GetUnit() != eStyleUnit_Auto &&
          (!height.IsCalcUnit() || !height.HasPercent())) {
        return true;
      }
    }
    else if (nsGkAtoms::tableFrame == frameType) {
      // we reached the containing table, so always return
      return rs->mStylePosition->mHeight.GetUnit() != eStyleUnit_Auto;
    }
  }
  return false;
}

// See if a special height reflow needs to occur and if so, call RequestSpecialHeightReflow
void
nsTableFrame::CheckRequestSpecialHeightReflow(const nsHTMLReflowState& aReflowState)
{
  NS_ASSERTION(IS_TABLE_CELL(aReflowState.frame->GetType()) ||
               aReflowState.frame->GetType() == nsGkAtoms::tableRowFrame ||
               aReflowState.frame->GetType() == nsGkAtoms::tableRowGroupFrame ||
               aReflowState.frame->GetType() == nsGkAtoms::tableFrame,
               "unexpected frame type");
  if (!aReflowState.frame->GetPrevInFlow() &&  // 1st in flow
      (NS_UNCONSTRAINEDSIZE == aReflowState.ComputedHeight() ||  // no computed height
       0                    == aReflowState.ComputedHeight()) &&
      eStyleUnit_Percent == aReflowState.mStylePosition->mHeight.GetUnit() && // pct height
      nsTableFrame::AncestorsHaveStyleHeight(*aReflowState.parentReflowState)) {
    nsTableFrame::RequestSpecialHeightReflow(aReflowState);
  }
}

// Notify the frame and its ancestors (up to the containing table) that a special
// height reflow will occur. During a special height reflow, a table, row group,
// row, or cell returns the last size it was reflowed at. However, the table may
// change the height of row groups, rows, cells in DistributeHeightToRows after.
// And the row group can change the height of rows, cells in CalculateRowHeights.
void
nsTableFrame::RequestSpecialHeightReflow(const nsHTMLReflowState& aReflowState)
{
  // notify the frame and its ancestors of the special reflow, stopping at the containing table
  for (const nsHTMLReflowState* rs = &aReflowState; rs && rs->frame; rs = rs->parentReflowState) {
    nsIAtom* frameType = rs->frame->GetType();
    NS_ASSERTION(IS_TABLE_CELL(frameType) ||
                 nsGkAtoms::tableRowFrame == frameType ||
                 nsGkAtoms::tableRowGroupFrame == frameType ||
                 nsGkAtoms::tableFrame == frameType,
                 "unexpected frame type");

    rs->frame->AddStateBits(NS_FRAME_CONTAINS_RELATIVE_HEIGHT);
    if (nsGkAtoms::tableFrame == frameType) {
      NS_ASSERTION(rs != &aReflowState,
                   "should not request special height reflow for table");
      // always stop when we reach a table
      break;
    }
  }
}

/******************************************************************************************
 * Before reflow, intrinsic width calculation is done using GetMinISize
 * and GetPrefISize.  This used to be known as pass 1 reflow.
 *
 * After the intrinsic width calculation, the table determines the
 * column widths using BalanceColumnWidths() and
 * then reflows each child again with a constrained avail width. This reflow is referred to
 * as the pass 2 reflow.
 *
 * A special height reflow (pass 3 reflow) can occur during an initial or resize reflow
 * if (a) a row group, row, cell, or a frame inside a cell has a percent height but no computed
 * height or (b) in paginated mode, a table has a height. (a) supports percent nested tables
 * contained inside cells whose heights aren't known until after the pass 2 reflow. (b) is
 * necessary because the table cannot split until after the pass 2 reflow. The mechanics of
 * the special height reflow (variety a) are as follows:
 *
 * 1) Each table related frame (table, row group, row, cell) implements NeedsSpecialReflow()
 *    to indicate that it should get the reflow. It does this when it has a percent height but
 *    no computed height by calling CheckRequestSpecialHeightReflow(). This method calls
 *    RequestSpecialHeightReflow() which calls SetNeedSpecialReflow() on its ancestors until
 *    it reaches the containing table and calls SetNeedToInitiateSpecialReflow() on it. For
 *    percent height frames inside cells, during DidReflow(), the cell's NotifyPercentHeight()
 *    is called (the cell is the reflow state's mPercentHeightObserver in this case).
 *    NotifyPercentHeight() calls RequestSpecialHeightReflow().
 *
 * 2) After the pass 2 reflow, if the table's NeedToInitiateSpecialReflow(true) was called, it
 *    will do the special height reflow, setting the reflow state's mFlags.mSpecialHeightReflow
 *    to true and mSpecialHeightInitiator to itself. It won't do this if IsPrematureSpecialHeightReflow()
 *    returns true because in that case another special height reflow will be coming along with the
 *    containing table as the mSpecialHeightInitiator. It is only relevant to do the reflow when
 *    the mSpecialHeightInitiator is the containing table, because if it is a remote ancestor, then
 *    appropriate heights will not be known.
 *
 * 3) Since the heights of the table, row groups, rows, and cells was determined during the pass 2
 *    reflow, they return their last desired sizes during the special height reflow. The reflow only
 *    permits percent height frames inside the cells to resize based on the cells height and that height
 *    was determined during the pass 2 reflow.
 *
 * So, in the case of deeply nested tables, all of the tables that were told to initiate a special
 * reflow will do so, but if a table is already in a special reflow, it won't inititate the reflow
 * until the current initiator is its containing table. Since these reflows are only received by
 * frames that need them and they don't cause any rebalancing of tables, the extra overhead is minimal.
 *
 * The type of special reflow that occurs during printing (variety b) follows the same mechanism except
 * that all frames will receive the reflow even if they don't really need them.
 *
 * Open issues with the special height reflow:
 *
 * 1) At some point there should be 2 kinds of special height reflows because (a) and (b) above are
 *    really quite different. This would avoid unnecessary reflows during printing.
 * 2) When a cell contains frames whose percent heights > 100%, there is data loss (see bug 115245).
 *    However, this can also occur if a cell has a fixed height and there is no special height reflow.
 *
 * XXXldb Special height reflow should really be its own method, not
 * part of nsIFrame::Reflow.  It should then call nsIFrame::Reflow on
 * the contents of the cells to do the necessary vertical resizing.
 *
 ******************************************************************************************/

/* Layout the entire inner table. */
void
nsTableFrame::Reflow(nsPresContext*           aPresContext,
                     nsHTMLReflowMetrics&     aDesiredSize,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus)
{
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsTableFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);
  bool isPaginated = aPresContext->IsPaginated();

  aStatus = NS_FRAME_COMPLETE;
  if (!GetPrevInFlow() && !mTableLayoutStrategy) {
    NS_ASSERTION(false, "strategy should have been created in Init");
    return;
  }

  // see if collapsing borders need to be calculated
  if (!GetPrevInFlow() && IsBorderCollapse() && NeedToCalcBCBorders()) {
    CalcBCBorders();
  }

  aDesiredSize.Width() = aReflowState.AvailableWidth();

  // Check for an overflow list, and append any row group frames being pushed
  MoveOverflowToChildList();

  bool haveDesiredHeight = false;
  SetHaveReflowedColGroups(false);

  // Reflow the entire table (pass 2 and possibly pass 3). This phase is necessary during a
  // constrained initial reflow and other reflows which require either a strategy init or balance.
  // This isn't done during an unconstrained reflow, because it will occur later when the parent
  // reflows with a constrained width.
  if (NS_SUBTREE_DIRTY(this) ||
      aReflowState.ShouldReflowAllKids() ||
      IsGeometryDirty() ||
      aReflowState.IsVResize()) {

    if (aReflowState.ComputedHeight() != NS_UNCONSTRAINEDSIZE ||
        // Also check mVResize, to handle the first Reflow preceding a
        // special height Reflow, when we've already had a special height
        // Reflow (where mComputedHeight would not be
        // NS_UNCONSTRAINEDSIZE, but without a style change in between).
        aReflowState.IsVResize()) {
      // XXX Eventually, we should modify DistributeHeightToRows to use
      // nsTableRowFrame::GetHeight instead of nsIFrame::GetSize().height.
      // That way, it will make its calculations based on internal table
      // frame heights as they are before they ever had any extra height
      // distributed to them.  In the meantime, this reflows all the
      // internal table frames, which restores them to their state before
      // DistributeHeightToRows was called.
      SetGeometryDirty();
    }

    bool needToInitiateSpecialReflow =
      !!(GetStateBits() & NS_FRAME_CONTAINS_RELATIVE_HEIGHT);
    // see if an extra reflow will be necessary in pagination mode when there is a specified table height
    if (isPaginated && !GetPrevInFlow() && (NS_UNCONSTRAINEDSIZE != aReflowState.AvailableHeight())) {
      nscoord tableSpecifiedHeight = CalcBorderBoxHeight(aReflowState);
      if ((tableSpecifiedHeight > 0) &&
          (tableSpecifiedHeight != NS_UNCONSTRAINEDSIZE)) {
        needToInitiateSpecialReflow = true;
      }
    }
    nsIFrame* lastChildReflowed = nullptr;

    NS_ASSERTION(!aReflowState.mFlags.mSpecialHeightReflow,
                 "Shouldn't be in special height reflow here!");

    // do the pass 2 reflow unless this is a special height reflow and we will be
    // initiating a special height reflow
    // XXXldb I changed this.  Should I change it back?

    // if we need to initiate a special height reflow, then don't constrain the
    // height of the reflow before that
    nscoord availHeight = needToInitiateSpecialReflow
                          ? NS_UNCONSTRAINEDSIZE : aReflowState.AvailableHeight();

    ReflowTable(aDesiredSize, aReflowState, availHeight,
                lastChildReflowed, aStatus);

    // reevaluate special height reflow conditions
    if (GetStateBits() & NS_FRAME_CONTAINS_RELATIVE_HEIGHT)
      needToInitiateSpecialReflow = true;

    // XXXldb Are all these conditions correct?
    if (needToInitiateSpecialReflow && NS_FRAME_IS_COMPLETE(aStatus)) {
      // XXXldb Do we need to set the mVResize flag on any reflow states?

      nsHTMLReflowState &mutable_rs =
        const_cast<nsHTMLReflowState&>(aReflowState);

      // distribute extra vertical space to rows
      CalcDesiredHeight(aReflowState, aDesiredSize);
      mutable_rs.mFlags.mSpecialHeightReflow = true;

      ReflowTable(aDesiredSize, aReflowState, aReflowState.AvailableHeight(),
                  lastChildReflowed, aStatus);

      if (lastChildReflowed && NS_FRAME_IS_NOT_COMPLETE(aStatus)) {
        // if there is an incomplete child, then set the desired height to include it but not the next one
        nsMargin borderPadding = GetChildAreaOffset(&aReflowState);
        aDesiredSize.Height() = borderPadding.bottom + GetRowSpacing(GetRowCount()) +
                              lastChildReflowed->GetNormalRect().YMost();
      }
      haveDesiredHeight = true;

      mutable_rs.mFlags.mSpecialHeightReflow = false;
    }
  }
  else {
    // Calculate the overflow area contribution from our children.
    for (nsIFrame* kid = GetFirstPrincipalChild(); kid; kid = kid->GetNextSibling()) {
      ConsiderChildOverflow(aDesiredSize.mOverflowAreas, kid);
    }
  }

  aDesiredSize.Width() = aReflowState.ComputedWidth() +
                       aReflowState.ComputedPhysicalBorderPadding().LeftRight();
  if (!haveDesiredHeight) {
    CalcDesiredHeight(aReflowState, aDesiredSize);
  }
  if (IsRowInserted()) {
    ProcessRowInserted(aDesiredSize.Height());
  }

  nsMargin borderPadding = GetChildAreaOffset(&aReflowState);
  SetColumnDimensions(aDesiredSize.Height(), borderPadding);
  if (NeedToCollapse() &&
      (NS_UNCONSTRAINEDSIZE != aReflowState.AvailableWidth())) {
    AdjustForCollapsingRowsCols(aDesiredSize, borderPadding);
  }

  // If there are any relatively-positioned table parts, we need to reflow their
  // absolutely-positioned descendants now that their dimensions are final.
  FixupPositionedTableParts(aPresContext, aDesiredSize, aReflowState);

  // make sure the table overflow area does include the table rect.
  nsRect tableRect(0, 0, aDesiredSize.Width(), aDesiredSize.Height()) ;

  if (!ShouldApplyOverflowClipping(this, aReflowState.mStyleDisplay)) {
    // collapsed border may leak out
    nsMargin bcMargin = GetExcludedOuterBCBorder();
    tableRect.Inflate(bcMargin);
  }
  aDesiredSize.mOverflowAreas.UnionAllWith(tableRect);

  if ((GetStateBits() & NS_FRAME_FIRST_REFLOW) ||
      nsSize(aDesiredSize.Width(), aDesiredSize.Height()) != mRect.Size()) {
      nsIFrame::InvalidateFrame();
  }

  FinishAndStoreOverflow(&aDesiredSize);
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
}

void
nsTableFrame::FixupPositionedTableParts(nsPresContext*           aPresContext,
                                        nsHTMLReflowMetrics&     aDesiredSize,
                                        const nsHTMLReflowState& aReflowState)
{
  auto positionedParts =
    static_cast<FrameTArray*>(Properties().Get(PositionedTablePartArray()));
  if (!positionedParts) {
    return;
  }

  OverflowChangedTracker overflowTracker;
  overflowTracker.SetSubtreeRoot(this);

  for (size_t i = 0; i < positionedParts->Length(); ++i) {
    nsIFrame* positionedPart = positionedParts->ElementAt(i);

    // As we've already finished reflow, positionedParts's size and overflow
    // areas have already been assigned, so we just pull them back out.
    nsSize size(positionedPart->GetSize());
    nsHTMLReflowMetrics desiredSize(aReflowState.GetWritingMode());
    desiredSize.Width() = size.width;
    desiredSize.Height() = size.height;
    desiredSize.mOverflowAreas = positionedPart->GetOverflowAreasRelativeToSelf();

    // Construct a dummy reflow state and reflow status.
    // XXX(seth): Note that the dummy reflow state doesn't have a correct
    // chain of parent reflow states. It also doesn't necessarily have a
    // correct containing block.
    WritingMode wm = positionedPart->GetWritingMode();
    LogicalSize availSize(wm, size);
    availSize.BSize(wm) = NS_UNCONSTRAINEDSIZE;
    nsHTMLReflowState reflowState(aPresContext, positionedPart,
                                  aReflowState.rendContext, availSize,
                                  nsHTMLReflowState::DUMMY_PARENT_REFLOW_STATE);
    nsReflowStatus reflowStatus = NS_FRAME_COMPLETE;

    // Reflow absolutely-positioned descendants of the positioned part.
    // FIXME: Unconditionally using NS_UNCONSTRAINEDSIZE for the height and
    // ignoring any change to the reflow status aren't correct. We'll never
    // paginate absolutely positioned frames.
    nsFrame* positionedFrame = static_cast<nsFrame*>(positionedPart);
    positionedFrame->FinishReflowWithAbsoluteFrames(PresContext(),
                                                    desiredSize,
                                                    reflowState,
                                                    reflowStatus,
                                                    true);

    // FinishReflowWithAbsoluteFrames has updated overflow on
    // |positionedPart|.  We need to make sure that update propagates
    // through the intermediate frames between it and this frame.
    nsIFrame* positionedFrameParent = positionedPart->GetParent();
    if (positionedFrameParent != this) {
      overflowTracker.AddFrame(positionedFrameParent,
        OverflowChangedTracker::CHILDREN_CHANGED);
    }
  }

  // Propagate updated overflow areas up the tree.
  overflowTracker.Flush();

  // Update our own overflow areas. (OverflowChangedTracker doesn't update the
  // subtree root itself.)
  aDesiredSize.SetOverflowAreasToDesiredBounds();
  nsLayoutUtils::UnionChildOverflow(this, aDesiredSize.mOverflowAreas);
}

bool
nsTableFrame::UpdateOverflow()
{
  nsRect bounds(nsPoint(0, 0), GetSize());

  // As above in Reflow, make sure the table overflow area includes the table
  // rect, and check for collapsed borders leaking out.
  if (!ShouldApplyOverflowClipping(this, StyleDisplay())) {
    nsMargin bcMargin = GetExcludedOuterBCBorder();
    bounds.Inflate(bcMargin);
  }

  nsOverflowAreas overflowAreas(bounds, bounds);
  nsLayoutUtils::UnionChildOverflow(this, overflowAreas);

  return FinishAndStoreOverflow(overflowAreas, GetSize());
}

void
nsTableFrame::ReflowTable(nsHTMLReflowMetrics&     aDesiredSize,
                          const nsHTMLReflowState& aReflowState,
                          nscoord                  aAvailHeight,
                          nsIFrame*&               aLastChildReflowed,
                          nsReflowStatus&          aStatus)
{
  aLastChildReflowed = nullptr;

  if (!GetPrevInFlow()) {
    mTableLayoutStrategy->ComputeColumnISizes(aReflowState);
  }
  // Constrain our reflow width to the computed table width (of the 1st in flow).
  // and our reflow height to our avail height minus border, padding, cellspacing
  aDesiredSize.Width() = aReflowState.ComputedWidth() +
                       aReflowState.ComputedPhysicalBorderPadding().LeftRight();
  nsTableReflowState reflowState(*PresContext(), aReflowState, *this,
                                 aDesiredSize.Width(), aAvailHeight);
  ReflowChildren(reflowState, aStatus, aLastChildReflowed,
                 aDesiredSize.mOverflowAreas);

  ReflowColGroups(aReflowState.rendContext);
}

nsIFrame*
nsTableFrame::GetFirstBodyRowGroupFrame()
{
  nsIFrame* headerFrame = nullptr;
  nsIFrame* footerFrame = nullptr;

  for (nsIFrame* kidFrame = mFrames.FirstChild(); nullptr != kidFrame; ) {
    const nsStyleDisplay* childDisplay = kidFrame->StyleDisplay();

    // We expect the header and footer row group frames to be first, and we only
    // allow one header and one footer
    if (NS_STYLE_DISPLAY_TABLE_HEADER_GROUP == childDisplay->mDisplay) {
      if (headerFrame) {
        // We already have a header frame and so this header frame is treated
        // like an ordinary body row group frame
        return kidFrame;
      }
      headerFrame = kidFrame;

    } else if (NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP == childDisplay->mDisplay) {
      if (footerFrame) {
        // We already have a footer frame and so this footer frame is treated
        // like an ordinary body row group frame
        return kidFrame;
      }
      footerFrame = kidFrame;

    } else if (NS_STYLE_DISPLAY_TABLE_ROW_GROUP == childDisplay->mDisplay) {
      return kidFrame;
    }

    // Get the next child
    kidFrame = kidFrame->GetNextSibling();
  }

  return nullptr;
}

// Table specific version that takes into account repeated header and footer
// frames when continuing table frames
void
nsTableFrame::PushChildren(const RowGroupArray& aRowGroups,
                           int32_t aPushFrom)
{
  NS_PRECONDITION(aPushFrom > 0, "pushing first child");

  // extract the frames from the array into a sibling list
  nsFrameList frames;
  uint32_t childX;
  for (childX = aPushFrom; childX < aRowGroups.Length(); ++childX) {
    nsTableRowGroupFrame* rgFrame = aRowGroups[childX];
    if (!rgFrame->IsRepeatable()) {
      mFrames.RemoveFrame(rgFrame);
      frames.AppendFrame(nullptr, rgFrame);
    }
  }

  if (frames.IsEmpty()) {
    return;
  }

  nsTableFrame* nextInFlow = static_cast<nsTableFrame*>(GetNextInFlow());
  if (nextInFlow) {
    // Insert the frames after any repeated header and footer frames.
    nsIFrame* firstBodyFrame = nextInFlow->GetFirstBodyRowGroupFrame();
    nsIFrame* prevSibling = nullptr;
    if (firstBodyFrame) {
      prevSibling = firstBodyFrame->GetPrevSibling();
    }
    // When pushing and pulling frames we need to check for whether any
    // views need to be reparented.
    ReparentFrameViewList(frames, this, nextInFlow);
    nextInFlow->mFrames.InsertFrames(nextInFlow, prevSibling,
                                     frames);
  }
  else {
    // Add the frames to our overflow list.
    SetOverflowFrames(frames);
  }
}

// collapsing row groups, rows, col groups and cols are accounted for after both passes of
// reflow so that it has no effect on the calculations of reflow.
void
nsTableFrame::AdjustForCollapsingRowsCols(nsHTMLReflowMetrics& aDesiredSize,
                                          nsMargin             aBorderPadding)
{
  nscoord yTotalOffset = 0; // total offset among all rows in all row groups

  // reset the bit, it will be set again if row/rowgroup or col/colgroup are
  // collapsed
  SetNeedToCollapse(false);

  // collapse the rows and/or row groups as necessary
  // Get the ordered children
  RowGroupArray rowGroups;
  OrderRowGroups(rowGroups);

  nsTableFrame* firstInFlow = static_cast<nsTableFrame*>(FirstInFlow());
  nscoord width = firstInFlow->GetCollapsedWidth(aBorderPadding);
  nscoord rgWidth = width - GetColSpacing(-1) -
                    GetColSpacing(GetColCount());
  nsOverflowAreas overflow;
  // Walk the list of children
  for (uint32_t childX = 0; childX < rowGroups.Length(); childX++) {
    nsTableRowGroupFrame* rgFrame = rowGroups[childX];
    NS_ASSERTION(rgFrame, "Must have row group frame here");
    yTotalOffset += rgFrame->CollapseRowGroupIfNecessary(yTotalOffset, rgWidth);
    ConsiderChildOverflow(overflow, rgFrame);
  }

  aDesiredSize.Height() -= yTotalOffset;
  aDesiredSize.Width() = width;
  overflow.UnionAllWith(nsRect(0, 0, aDesiredSize.Width(), aDesiredSize.Height()));
  FinishAndStoreOverflow(overflow,
                         nsSize(aDesiredSize.Width(), aDesiredSize.Height()));
}


nscoord
nsTableFrame::GetCollapsedWidth(nsMargin aBorderPadding)
{
  NS_ASSERTION(!GetPrevInFlow(), "GetCollapsedWidth called on next in flow");
  nscoord width = GetColSpacing(GetColCount());
  width += aBorderPadding.left + aBorderPadding.right;
  for (nsIFrame* groupFrame = mColGroups.FirstChild(); groupFrame;
         groupFrame = groupFrame->GetNextSibling()) {
    const nsStyleVisibility* groupVis = groupFrame->StyleVisibility();
    bool collapseGroup = (NS_STYLE_VISIBILITY_COLLAPSE == groupVis->mVisible);
    nsTableColGroupFrame* cgFrame = (nsTableColGroupFrame*)groupFrame;
    for (nsTableColFrame* colFrame = cgFrame->GetFirstColumn(); colFrame;
         colFrame = colFrame->GetNextCol()) {
      const nsStyleDisplay* colDisplay = colFrame->StyleDisplay();
      int32_t colX = colFrame->GetColIndex();
      if (NS_STYLE_DISPLAY_TABLE_COLUMN == colDisplay->mDisplay) {
        const nsStyleVisibility* colVis = colFrame->StyleVisibility();
        bool collapseCol = (NS_STYLE_VISIBILITY_COLLAPSE == colVis->mVisible);
        int32_t colWidth = GetColumnWidth(colX);
        if (!collapseGroup && !collapseCol) {
          width += colWidth;
          if (ColumnHasCellSpacingBefore(colX))
            width += GetColSpacing(colX-1);
        }
        else {
          SetNeedToCollapse(true);
        }
      }
    }
  }
  return width;
}

/* virtual */ void
nsTableFrame::DidSetStyleContext(nsStyleContext* aOldStyleContext)
{
  nsContainerFrame::DidSetStyleContext(aOldStyleContext);

  if (!aOldStyleContext) //avoid this on init
    return;

  if (IsBorderCollapse() &&
      BCRecalcNeeded(aOldStyleContext, StyleContext())) {
    SetFullBCDamageArea();
  }

  //avoid this on init or nextinflow
  if (!mTableLayoutStrategy || GetPrevInFlow())
    return;

  bool isAuto = IsAutoLayout();
  if (isAuto != (LayoutStrategy()->GetType() == nsITableLayoutStrategy::Auto)) {
    nsITableLayoutStrategy* temp;
    if (isAuto)
      temp = new BasicTableLayoutStrategy(this);
    else
      temp = new FixedTableLayoutStrategy(this);

    if (temp) {
      delete mTableLayoutStrategy;
      mTableLayoutStrategy = temp;
    }
  }
}



void
nsTableFrame::AppendFrames(ChildListID     aListID,
                           nsFrameList&    aFrameList)
{
  NS_ASSERTION(aListID == kPrincipalList || aListID == kColGroupList,
               "unexpected child list");

  // Because we actually have two child lists, one for col group frames and one
  // for everything else, we need to look at each frame individually
  // XXX The frame construction code should be separating out child frames
  // based on the type, bug 343048.
  while (!aFrameList.IsEmpty()) {
    nsIFrame* f = aFrameList.FirstChild();
    aFrameList.RemoveFrame(f);

    // See what kind of frame we have
    const nsStyleDisplay* display = f->StyleDisplay();

    if (NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP == display->mDisplay) {
      nsTableColGroupFrame* lastColGroup =
        nsTableColGroupFrame::GetLastRealColGroup(this);
      int32_t startColIndex = (lastColGroup)
        ? lastColGroup->GetStartColumnIndex() + lastColGroup->GetColCount() : 0;
      mColGroups.InsertFrame(nullptr, lastColGroup, f);
      // Insert the colgroup and its cols into the table
      InsertColGroups(startColIndex,
                      nsFrameList::Slice(mColGroups, f, f->GetNextSibling()));
    } else if (IsRowGroup(display->mDisplay)) {
      DrainSelfOverflowList(); // ensure the last frame is in mFrames
      // Append the new row group frame to the sibling chain
      mFrames.AppendFrame(nullptr, f);

      // insert the row group and its rows into the table
      InsertRowGroups(nsFrameList::Slice(mFrames, f, nullptr));
    } else {
      // Nothing special to do, just add the frame to our child list
      NS_NOTREACHED("How did we get here?  Frame construction screwed up");
      mFrames.AppendFrame(nullptr, f);
    }
  }

#ifdef DEBUG_TABLE_CELLMAP
  printf("=== TableFrame::AppendFrames\n");
  Dump(true, true, true);
#endif
  PresContext()->PresShell()->FrameNeedsReflow(this, nsIPresShell::eTreeChange,
                                               NS_FRAME_HAS_DIRTY_CHILDREN);
  SetGeometryDirty();
}

// Needs to be at file scope or ArrayLength fails to compile.
struct ChildListInsertions {
  nsIFrame::ChildListID mID;
  nsFrameList mList;
};

void
nsTableFrame::InsertFrames(ChildListID     aListID,
                           nsIFrame*       aPrevFrame,
                           nsFrameList&    aFrameList)
{
  // The frames in aFrameList can be a mix of row group frames and col group
  // frames. The problem is that they should go in separate child lists so
  // we need to deal with that here...
  // XXX The frame construction code should be separating out child frames
  // based on the type, bug 343048.

  NS_ASSERTION(!aPrevFrame || aPrevFrame->GetParent() == this,
               "inserting after sibling frame with different parent");

  if ((aPrevFrame && !aPrevFrame->GetNextSibling()) ||
      (!aPrevFrame && GetChildList(aListID).IsEmpty())) {
    // Treat this like an append; still a workaround for bug 343048.
    AppendFrames(aListID, aFrameList);
    return;
  }

  // Collect ColGroupFrames into a separate list and insert those separately
  // from the other frames (bug 759249).
  ChildListInsertions insertions[2]; // ColGroup, other
  const nsStyleDisplay* display = aFrameList.FirstChild()->StyleDisplay();
  nsFrameList::FrameLinkEnumerator e(aFrameList);
  for (; !aFrameList.IsEmpty(); e.Next()) {
    nsIFrame* next = e.NextFrame();
    if (!next || next->StyleDisplay()->mDisplay != display->mDisplay) {
      nsFrameList head = aFrameList.ExtractHead(e);
      if (display->mDisplay == NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP) {
        insertions[0].mID = kColGroupList;
        insertions[0].mList.AppendFrames(nullptr, head);
      } else {
        insertions[1].mID = kPrincipalList;
        insertions[1].mList.AppendFrames(nullptr, head);
      }
      if (!next) {
        break;
      }
      display = next->StyleDisplay();
    }
  }
  for (uint32_t i = 0; i < ArrayLength(insertions); ++i) {
    // We pass aPrevFrame for both ColGroup and other frames since
    // HomogenousInsertFrames will only use it if it's a suitable
    // prev-sibling for the frames in the frame list.
    if (!insertions[i].mList.IsEmpty()) {
      HomogenousInsertFrames(insertions[i].mID, aPrevFrame,
                             insertions[i].mList);
    }
  }
}

void
nsTableFrame::HomogenousInsertFrames(ChildListID     aListID,
                                     nsIFrame*       aPrevFrame,
                                     nsFrameList&    aFrameList)
{
  // See what kind of frame we have
  const nsStyleDisplay* display = aFrameList.FirstChild()->StyleDisplay();
#ifdef DEBUG
  // Verify that either all siblings have display:table-column-group, or they
  // all have display values different from table-column-group.
  for (nsFrameList::Enumerator e(aFrameList); !e.AtEnd(); e.Next()) {
    const nsStyleDisplay* nextDisplay = e.get()->StyleDisplay();
    MOZ_ASSERT((display->mDisplay == NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP) ==
               (nextDisplay->mDisplay == NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP),
               "heterogenous childlist");
  }
#endif
  if (aPrevFrame) {
    const nsStyleDisplay* prevDisplay = aPrevFrame->StyleDisplay();
    // Make sure they belong on the same frame list
    if ((display->mDisplay == NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP) !=
        (prevDisplay->mDisplay == NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP)) {
      // the previous frame is not valid, see comment at ::AppendFrames
      // XXXbz Using content indices here means XBL will get screwed
      // over...  Oh, well.
      nsIFrame* pseudoFrame = aFrameList.FirstChild();
      nsIContent* parentContent = GetContent();
      nsIContent* content;
      aPrevFrame = nullptr;
      while (pseudoFrame  && (parentContent ==
                              (content = pseudoFrame->GetContent()))) {
        pseudoFrame = pseudoFrame->GetFirstPrincipalChild();
      }
      nsCOMPtr<nsIContent> container = content->GetParent();
      if (MOZ_LIKELY(container)) { // XXX need this null-check, see bug 411823.
        int32_t newIndex = container->IndexOf(content);
        nsIFrame* kidFrame;
        bool isColGroup = (NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP ==
                             display->mDisplay);
        nsTableColGroupFrame* lastColGroup;
        if (isColGroup) {
          kidFrame = mColGroups.FirstChild();
          lastColGroup = nsTableColGroupFrame::GetLastRealColGroup(this);
        }
        else {
          kidFrame = mFrames.FirstChild();
        }
        // Important: need to start at a value smaller than all valid indices
        int32_t lastIndex = -1;
        while (kidFrame) {
          if (isColGroup) {
            if (kidFrame == lastColGroup) {
              aPrevFrame = kidFrame; // there is no real colgroup after this one
              break;
            }
          }
          pseudoFrame = kidFrame;
          while (pseudoFrame  && (parentContent ==
                                  (content = pseudoFrame->GetContent()))) {
            pseudoFrame = pseudoFrame->GetFirstPrincipalChild();
          }
          int32_t index = container->IndexOf(content);
          if (index > lastIndex && index < newIndex) {
            lastIndex = index;
            aPrevFrame = kidFrame;
          }
          kidFrame = kidFrame->GetNextSibling();
        }
      }
    }
  }
  if (NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP == display->mDisplay) {
    NS_ASSERTION(aListID == kColGroupList, "unexpected child list");
    // Insert the column group frames
    const nsFrameList::Slice& newColgroups =
      mColGroups.InsertFrames(nullptr, aPrevFrame, aFrameList);
    // find the starting col index for the first new col group
    int32_t startColIndex = 0;
    if (aPrevFrame) {
      nsTableColGroupFrame* prevColGroup =
        (nsTableColGroupFrame*)GetFrameAtOrBefore(this, aPrevFrame,
                                                  nsGkAtoms::tableColGroupFrame);
      if (prevColGroup) {
        startColIndex = prevColGroup->GetStartColumnIndex() + prevColGroup->GetColCount();
      }
    }
    InsertColGroups(startColIndex, newColgroups);
  } else if (IsRowGroup(display->mDisplay)) {
    NS_ASSERTION(aListID == kPrincipalList, "unexpected child list");
    DrainSelfOverflowList(); // ensure aPrevFrame is in mFrames
    // Insert the frames in the sibling chain
    const nsFrameList::Slice& newRowGroups =
      mFrames.InsertFrames(nullptr, aPrevFrame, aFrameList);

    InsertRowGroups(newRowGroups);
  } else {
    NS_ASSERTION(aListID == kPrincipalList, "unexpected child list");
    NS_NOTREACHED("How did we even get here?");
    // Just insert the frame and don't worry about reflowing it
    mFrames.InsertFrames(nullptr, aPrevFrame, aFrameList);
    return;
  }

  PresContext()->PresShell()->FrameNeedsReflow(this, nsIPresShell::eTreeChange,
                                               NS_FRAME_HAS_DIRTY_CHILDREN);
  SetGeometryDirty();
#ifdef DEBUG_TABLE_CELLMAP
  printf("=== TableFrame::InsertFrames\n");
  Dump(true, true, true);
#endif
  return;
}

void
nsTableFrame::DoRemoveFrame(ChildListID     aListID,
                            nsIFrame*       aOldFrame)
{
  if (aListID == kColGroupList) {
    nsIFrame* nextColGroupFrame = aOldFrame->GetNextSibling();
    nsTableColGroupFrame* colGroup = (nsTableColGroupFrame*)aOldFrame;
    int32_t firstColIndex = colGroup->GetStartColumnIndex();
    int32_t lastColIndex  = firstColIndex + colGroup->GetColCount() - 1;
    mColGroups.DestroyFrame(aOldFrame);
    nsTableColGroupFrame::ResetColIndices(nextColGroupFrame, firstColIndex);
    // remove the cols from the table
    int32_t colX;
    for (colX = lastColIndex; colX >= firstColIndex; colX--) {
      nsTableColFrame* colFrame = mColFrames.SafeElementAt(colX);
      if (colFrame) {
        RemoveCol(colGroup, colX, true, false);
      }
    }

    int32_t numAnonymousColsToAdd = GetColCount() - mColFrames.Length();
    if (numAnonymousColsToAdd > 0) {
      // this sets the child list, updates the col cache and cell map
      AppendAnonymousColFrames(numAnonymousColsToAdd);
    }

  } else {
    NS_ASSERTION(aListID == kPrincipalList, "unexpected child list");
    nsTableRowGroupFrame* rgFrame =
      static_cast<nsTableRowGroupFrame*>(aOldFrame);
    // remove the row group from the cell map
    nsTableCellMap* cellMap = GetCellMap();
    if (cellMap) {
      cellMap->RemoveGroupCellMap(rgFrame);
    }

    // remove the row group frame from the sibling chain
    mFrames.DestroyFrame(aOldFrame);

    // the removal of a row group changes the cellmap, the columns might change
    if (cellMap) {
      cellMap->Synchronize(this);
      // Create an empty slice
      ResetRowIndices(nsFrameList::Slice(mFrames, nullptr, nullptr));
      nsIntRect damageArea;
      cellMap->RebuildConsideringCells(nullptr, nullptr, 0, 0, false, damageArea);

      static_cast<nsTableFrame*>(FirstInFlow())->MatchCellMapToColCache(cellMap);
    }
  }
}

void
nsTableFrame::RemoveFrame(ChildListID     aListID,
                          nsIFrame*       aOldFrame)
{
  NS_ASSERTION(aListID == kColGroupList ||
               NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP !=
                 aOldFrame->StyleDisplay()->mDisplay,
               "Wrong list name; use kColGroupList iff colgroup");
  nsIPresShell* shell = PresContext()->PresShell();
  nsTableFrame* lastParent = nullptr;
  while (aOldFrame) {
    nsIFrame* oldFrameNextContinuation = aOldFrame->GetNextContinuation();
    nsTableFrame* parent = static_cast<nsTableFrame*>(aOldFrame->GetParent());
    if (parent != lastParent) {
      parent->DrainSelfOverflowList();
    }
    parent->DoRemoveFrame(aListID, aOldFrame);
    aOldFrame = oldFrameNextContinuation;
    if (parent != lastParent) {
      // for now, just bail and recalc all of the collapsing borders
      // as the cellmap changes we need to recalc
      if (parent->IsBorderCollapse()) {
        parent->SetFullBCDamageArea();
      }
      parent->SetGeometryDirty();
      shell->FrameNeedsReflow(parent, nsIPresShell::eTreeChange,
                              NS_FRAME_HAS_DIRTY_CHILDREN);
      lastParent = parent;
    }
  }
#ifdef DEBUG_TABLE_CELLMAP
  printf("=== TableFrame::RemoveFrame\n");
  Dump(true, true, true);
#endif
}

/* virtual */ nsMargin
nsTableFrame::GetUsedBorder() const
{
  if (!IsBorderCollapse())
    return nsContainerFrame::GetUsedBorder();

  return GetIncludedOuterBCBorder();
}

/* virtual */ nsMargin
nsTableFrame::GetUsedPadding() const
{
  if (!IsBorderCollapse())
    return nsContainerFrame::GetUsedPadding();

  return nsMargin(0,0,0,0);
}

/* virtual */ nsMargin
nsTableFrame::GetUsedMargin() const
{
  // The margin is inherited to the outer table frame via
  // the ::-moz-table-outer rule in ua.css.
  return nsMargin(0, 0, 0, 0);
}

NS_DECLARE_FRAME_PROPERTY(TableBCProperty, DeleteValue<BCPropertyData>)

BCPropertyData*
nsTableFrame::GetBCProperty(bool aCreateIfNecessary) const
{
  FrameProperties props = Properties();
  BCPropertyData* value = static_cast<BCPropertyData*>
                          (props.Get(TableBCProperty()));
  if (!value && aCreateIfNecessary) {
    value = new BCPropertyData();
    props.Set(TableBCProperty(), value);
  }

  return value;
}

static void
DivideBCBorderSize(BCPixelSize  aPixelSize,
                   BCPixelSize& aSmallHalf,
                   BCPixelSize& aLargeHalf)
{
  aSmallHalf = aPixelSize / 2;
  aLargeHalf = aPixelSize - aSmallHalf;
}

nsMargin
nsTableFrame::GetOuterBCBorder() const
{
  if (NeedToCalcBCBorders())
    const_cast<nsTableFrame*>(this)->CalcBCBorders();

  nsMargin border(0, 0, 0, 0);
  int32_t p2t = nsPresContext::AppUnitsPerCSSPixel();
  BCPropertyData* propData = GetBCProperty();
  if (propData) {
    border.top    = BC_BORDER_TOP_HALF_COORD(p2t, propData->mTopBorderWidth);
    border.right  = BC_BORDER_RIGHT_HALF_COORD(p2t, propData->mRightBorderWidth);
    border.bottom = BC_BORDER_BOTTOM_HALF_COORD(p2t, propData->mBottomBorderWidth);
    border.left   = BC_BORDER_LEFT_HALF_COORD(p2t, propData->mLeftBorderWidth);
  }
  return border;
}

nsMargin
nsTableFrame::GetIncludedOuterBCBorder() const
{
  if (NeedToCalcBCBorders())
    const_cast<nsTableFrame*>(this)->CalcBCBorders();

  nsMargin border(0, 0, 0, 0);
  int32_t p2t = nsPresContext::AppUnitsPerCSSPixel();
  BCPropertyData* propData = GetBCProperty();
  if (propData) {
    border.top += BC_BORDER_TOP_HALF_COORD(p2t, propData->mTopBorderWidth);
    border.right += BC_BORDER_RIGHT_HALF_COORD(p2t, propData->mRightCellBorderWidth);
    border.bottom += BC_BORDER_BOTTOM_HALF_COORD(p2t, propData->mBottomBorderWidth);
    border.left += BC_BORDER_LEFT_HALF_COORD(p2t, propData->mLeftCellBorderWidth);
  }
  return border;
}

nsMargin
nsTableFrame::GetExcludedOuterBCBorder() const
{
  return GetOuterBCBorder() - GetIncludedOuterBCBorder();
}

static
void GetSeparateModelBorderPadding(const nsHTMLReflowState* aReflowState,
                                   nsStyleContext&          aStyleContext,
                                   nsMargin&                aBorderPadding)
{
  // XXXbz Either we _do_ have a reflow state and then we can use its
  // mComputedBorderPadding or we don't and then we get the padding
  // wrong!
  const nsStyleBorder* border = aStyleContext.StyleBorder();
  aBorderPadding = border->GetComputedBorder();
  if (aReflowState) {
    aBorderPadding += aReflowState->ComputedPhysicalPadding();
  }
}

nsMargin
nsTableFrame::GetChildAreaOffset(const nsHTMLReflowState* aReflowState) const
{
  nsMargin offset(0,0,0,0);
  if (IsBorderCollapse()) {
    offset = GetIncludedOuterBCBorder();
  }
  else {
    GetSeparateModelBorderPadding(aReflowState, *mStyleContext, offset);
  }
  return offset;
}

void
nsTableFrame::InitChildReflowState(nsHTMLReflowState& aReflowState)
{
  nsMargin collapseBorder;
  nsMargin padding(0,0,0,0);
  nsMargin* pCollapseBorder = nullptr;
  nsPresContext* presContext = PresContext();
  if (IsBorderCollapse()) {
    nsTableRowGroupFrame* rgFrame =
       static_cast<nsTableRowGroupFrame*>(aReflowState.frame);
    pCollapseBorder = rgFrame->GetBCBorderWidth(collapseBorder);
  }
  aReflowState.Init(presContext, -1, -1, pCollapseBorder, &padding);

  NS_ASSERTION(!mBits.mResizedColumns ||
               !aReflowState.parentReflowState->mFlags.mSpecialHeightReflow,
               "should not resize columns on special height reflow");
  if (mBits.mResizedColumns) {
    aReflowState.SetHResize(true);
  }
}

// Position and size aKidFrame and update our reflow state. The origin of
// aKidRect is relative to the upper-left origin of our frame
void nsTableFrame::PlaceChild(nsTableReflowState&  aReflowState,
                              nsIFrame*            aKidFrame,
                              nsPoint              aKidPosition,
                              nsHTMLReflowMetrics& aKidDesiredSize,
                              const nsRect&        aOriginalKidRect,
                              const nsRect&        aOriginalKidVisualOverflow)
{
  bool isFirstReflow =
    (aKidFrame->GetStateBits() & NS_FRAME_FIRST_REFLOW) != 0;

  // Place and size the child
  FinishReflowChild(aKidFrame, PresContext(), aKidDesiredSize, nullptr,
                    aKidPosition.x, aKidPosition.y, 0);

  InvalidateTableFrame(aKidFrame, aOriginalKidRect, aOriginalKidVisualOverflow,
                       isFirstReflow);

  // Adjust the running y-offset
  aReflowState.y += aKidDesiredSize.Height();

  // If our height is constrained, then update the available height
  if (NS_UNCONSTRAINEDSIZE != aReflowState.availSize.height) {
    aReflowState.availSize.height -= aKidDesiredSize.Height();
  }
}

void
nsTableFrame::OrderRowGroups(RowGroupArray& aChildren,
                             nsTableRowGroupFrame** aHead,
                             nsTableRowGroupFrame** aFoot) const
{
  aChildren.Clear();
  nsTableRowGroupFrame* head = nullptr;
  nsTableRowGroupFrame* foot = nullptr;

  nsIFrame* kidFrame = mFrames.FirstChild();
  while (kidFrame) {
    const nsStyleDisplay* kidDisplay = kidFrame->StyleDisplay();
    nsTableRowGroupFrame* rowGroup =
      static_cast<nsTableRowGroupFrame*>(kidFrame);

    switch (kidDisplay->mDisplay) {
    case NS_STYLE_DISPLAY_TABLE_HEADER_GROUP:
      if (head) { // treat additional thead like tbody
        aChildren.AppendElement(rowGroup);
      }
      else {
        head = rowGroup;
      }
      break;
    case NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP:
      if (foot) { // treat additional tfoot like tbody
        aChildren.AppendElement(rowGroup);
      }
      else {
        foot = rowGroup;
      }
      break;
    case NS_STYLE_DISPLAY_TABLE_ROW_GROUP:
      aChildren.AppendElement(rowGroup);
      break;
    default:
      NS_NOTREACHED("How did this produce an nsTableRowGroupFrame?");
      // Just ignore it
      break;
    }
    // Get the next sibling but skip it if it's also the next-in-flow, since
    // a next-in-flow will not be part of the current table.
    while (kidFrame) {
      nsIFrame* nif = kidFrame->GetNextInFlow();
      kidFrame = kidFrame->GetNextSibling();
      if (kidFrame != nif)
        break;
    }
  }

  // put the thead first
  if (head) {
    aChildren.InsertElementAt(0, head);
  }
  if (aHead)
    *aHead = head;
  // put the tfoot after the last tbody
  if (foot) {
    aChildren.AppendElement(foot);
  }
  if (aFoot)
    *aFoot = foot;
}

nsTableRowGroupFrame*
nsTableFrame::GetTHead() const
{
  nsIFrame* kidFrame = mFrames.FirstChild();
  while (kidFrame) {
    if (kidFrame->StyleDisplay()->mDisplay ==
          NS_STYLE_DISPLAY_TABLE_HEADER_GROUP) {
      return static_cast<nsTableRowGroupFrame*>(kidFrame);
    }

    // Get the next sibling but skip it if it's also the next-in-flow, since
    // a next-in-flow will not be part of the current table.
    while (kidFrame) {
      nsIFrame* nif = kidFrame->GetNextInFlow();
      kidFrame = kidFrame->GetNextSibling();
      if (kidFrame != nif)
        break;
    }
  }

  return nullptr;
}

nsTableRowGroupFrame*
nsTableFrame::GetTFoot() const
{
  nsIFrame* kidFrame = mFrames.FirstChild();
  while (kidFrame) {
    if (kidFrame->StyleDisplay()->mDisplay ==
          NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP) {
      return static_cast<nsTableRowGroupFrame*>(kidFrame);
    }

    // Get the next sibling but skip it if it's also the next-in-flow, since
    // a next-in-flow will not be part of the current table.
    while (kidFrame) {
      nsIFrame* nif = kidFrame->GetNextInFlow();
      kidFrame = kidFrame->GetNextSibling();
      if (kidFrame != nif)
        break;
    }
  }

  return nullptr;
}

static bool
IsRepeatable(nscoord aFrameHeight, nscoord aPageHeight)
{
  return aFrameHeight < (aPageHeight / 4);
}

nsresult
nsTableFrame::SetupHeaderFooterChild(const nsTableReflowState& aReflowState,
                                     nsTableRowGroupFrame* aFrame,
                                     nscoord* aDesiredHeight)
{
  nsPresContext* presContext = PresContext();
  nscoord pageHeight = presContext->GetPageSize().height;

  // Reflow the child with unconstrained height
  WritingMode wm = aFrame->GetWritingMode();
  LogicalSize availSize = aReflowState.reflowState.AvailableSize(wm);
  availSize.BSize(wm) = NS_UNCONSTRAINEDSIZE;
  nsHTMLReflowState kidReflowState(presContext, aReflowState.reflowState,
                                   aFrame, availSize,
                                   -1, -1, nsHTMLReflowState::CALLER_WILL_INIT);
  InitChildReflowState(kidReflowState);
  kidReflowState.mFlags.mIsTopOfPage = true;
  nsHTMLReflowMetrics desiredSize(aReflowState.reflowState);
  desiredSize.ClearSize();
  nsReflowStatus status;
  ReflowChild(aFrame, presContext, desiredSize, kidReflowState,
              aReflowState.x, aReflowState.y, 0, status);
  // The child will be reflowed again "for real" so no need to place it now

  aFrame->SetRepeatable(IsRepeatable(desiredSize.Height(), pageHeight));
  *aDesiredHeight = desiredSize.Height();
  return NS_OK;
}

void
nsTableFrame::PlaceRepeatedFooter(nsTableReflowState& aReflowState,
                                  nsTableRowGroupFrame *aTfoot,
                                  nscoord aFooterHeight)
{
  nsPresContext* presContext = PresContext();
  WritingMode wm = aTfoot->GetWritingMode();
  LogicalSize kidAvailSize(wm, aReflowState.availSize);
  kidAvailSize.BSize(wm) = aFooterHeight;
  nsHTMLReflowState footerReflowState(presContext,
                                      aReflowState.reflowState,
                                      aTfoot, kidAvailSize,
                                      -1, -1,
                                      nsHTMLReflowState::CALLER_WILL_INIT);
  InitChildReflowState(footerReflowState);
  aReflowState.y += GetRowSpacing(GetRowCount());

  nsRect origTfootRect = aTfoot->GetRect();
  nsRect origTfootVisualOverflow = aTfoot->GetVisualOverflowRect();

  nsReflowStatus footerStatus;
  nsHTMLReflowMetrics desiredSize(aReflowState.reflowState);
  desiredSize.ClearSize();
  ReflowChild(aTfoot, presContext, desiredSize, footerReflowState,
              aReflowState.x, aReflowState.y, 0, footerStatus);
  nsPoint kidPosition(aReflowState.x, aReflowState.y);
  footerReflowState.ApplyRelativePositioning(&kidPosition);

  PlaceChild(aReflowState, aTfoot, kidPosition, desiredSize, origTfootRect,
             origTfootVisualOverflow);
}

// Reflow the children based on the avail size and reason in aReflowState
// update aReflowMetrics a aStatus
void
nsTableFrame::ReflowChildren(nsTableReflowState& aReflowState,
                             nsReflowStatus&     aStatus,
                             nsIFrame*&          aLastChildReflowed,
                             nsOverflowAreas&    aOverflowAreas)
{
  aStatus = NS_FRAME_COMPLETE;
  aLastChildReflowed = nullptr;

  nsIFrame* prevKidFrame = nullptr;

  nsPresContext* presContext = PresContext();
  // XXXldb Should we be checking constrained height instead?
  // tables are not able to pull back children from its next inflow, so even
  // under paginated contexts tables are should not paginate if they are inside
  // column set
  bool isPaginated = presContext->IsPaginated() &&
                       NS_UNCONSTRAINEDSIZE != aReflowState.availSize.height &&
                       aReflowState.reflowState.mFlags.mTableIsSplittable;

  aOverflowAreas.Clear();

  bool reflowAllKids = aReflowState.reflowState.ShouldReflowAllKids() ||
                         mBits.mResizedColumns ||
                         IsGeometryDirty();

  RowGroupArray rowGroups;
  nsTableRowGroupFrame *thead, *tfoot;
  OrderRowGroups(rowGroups, &thead, &tfoot);
  bool pageBreak = false;
  nscoord footerHeight = 0;

  // Determine the repeatablility of headers and footers, and also the desired
  // height of any repeatable footer.
  // The repeatability of headers on continued tables is handled
  // when they are created in nsCSSFrameConstructor::CreateContinuingTableFrame.
  // We handle the repeatability of footers again here because we need to
  // determine the footer's height anyway. We could perhaps optimize by
  // using the footer's prev-in-flow's height instead of reflowing it again,
  // but there's no real need.
  if (isPaginated) {
    if (thead && !GetPrevInFlow()) {
      nscoord desiredHeight;
      nsresult rv = SetupHeaderFooterChild(aReflowState, thead, &desiredHeight);
      if (NS_FAILED(rv))
        return;
    }
    if (tfoot) {
      nsresult rv = SetupHeaderFooterChild(aReflowState, tfoot, &footerHeight);
      if (NS_FAILED(rv))
        return;
    }
  }
   // if the child is a tbody in paginated mode reduce the height by a repeated footer
  bool allowRepeatedFooter = false;
  for (size_t childX = 0; childX < rowGroups.Length(); childX++) {
    nsIFrame* kidFrame = rowGroups[childX];
    nsTableRowGroupFrame* rowGroupFrame = rowGroups[childX];
    nscoord cellSpacingY = GetRowSpacing(rowGroupFrame->GetStartRowIndex()+
                                         rowGroupFrame->GetRowCount());
    // Get the frame state bits
    // See if we should only reflow the dirty child frames
    if (reflowAllKids ||
        NS_SUBTREE_DIRTY(kidFrame) ||
        (aReflowState.reflowState.mFlags.mSpecialHeightReflow &&
         (isPaginated || (kidFrame->GetStateBits() &
                          NS_FRAME_CONTAINS_RELATIVE_HEIGHT)))) {
      if (pageBreak) {
        if (allowRepeatedFooter) {
          PlaceRepeatedFooter(aReflowState, tfoot, footerHeight);
        }
        else if (tfoot && tfoot->IsRepeatable()) {
          tfoot->SetRepeatable(false);
        }
        PushChildren(rowGroups, childX);
        aStatus = NS_FRAME_NOT_COMPLETE;
        break;
      }

      nsSize kidAvailSize(aReflowState.availSize);
      allowRepeatedFooter = false;
      if (isPaginated && (NS_UNCONSTRAINEDSIZE != kidAvailSize.height)) {
        nsTableRowGroupFrame* kidRG =
          static_cast<nsTableRowGroupFrame*>(kidFrame);
        if (kidRG != thead && kidRG != tfoot && tfoot && tfoot->IsRepeatable()) {
          // the child is a tbody and there is a repeatable footer
          NS_ASSERTION(tfoot == rowGroups[rowGroups.Length() - 1], "Missing footer!");
          if (footerHeight + cellSpacingY < kidAvailSize.height) {
            allowRepeatedFooter = true;
            kidAvailSize.height -= footerHeight + cellSpacingY;
          }
        }
      }

      nsRect oldKidRect = kidFrame->GetRect();
      nsRect oldKidVisualOverflow = kidFrame->GetVisualOverflowRect();

      nsHTMLReflowMetrics desiredSize(aReflowState.reflowState);
      desiredSize.ClearSize();

      // Reflow the child into the available space
      nsHTMLReflowState kidReflowState(presContext, aReflowState.reflowState,
                                       kidFrame,
                                       LogicalSize(kidFrame->GetWritingMode(),
                                                   kidAvailSize),
                                       -1, -1,
                                       nsHTMLReflowState::CALLER_WILL_INIT);
      InitChildReflowState(kidReflowState);

      // If this isn't the first row group, and the previous row group has a
      // nonzero YMost, then we can't be at the top of the page.
      // We ignore a repeated head row group in this check to avoid causing
      // infinite loops in some circumstances - see bug 344883.
      if (childX > ((thead && IsRepeatedFrame(thead)) ? 1u : 0u) &&
          (rowGroups[childX - 1]->GetNormalRect().YMost() > 0)) {
        kidReflowState.mFlags.mIsTopOfPage = false;
      }
      aReflowState.y += cellSpacingY;
      if (NS_UNCONSTRAINEDSIZE != aReflowState.availSize.height) {
        aReflowState.availSize.height -= cellSpacingY;
      }
      // record the presence of a next in flow, it might get destroyed so we
      // need to reorder the row group array
      bool reorder = false;
      if (kidFrame->GetNextInFlow())
        reorder = true;

      ReflowChild(kidFrame, presContext, desiredSize, kidReflowState,
                  aReflowState.x, aReflowState.y, 0, aStatus);
      nsPoint kidPosition(aReflowState.x, aReflowState.y);
      kidReflowState.ApplyRelativePositioning(&kidPosition);

      if (reorder) {
        // reorder row groups the reflow may have changed the nextinflows
        OrderRowGroups(rowGroups, &thead, &tfoot);
        childX = rowGroups.IndexOf(kidFrame);
        if (childX == RowGroupArray::NoIndex) {
          // XXXbz can this happen?
          childX = rowGroups.Length();
        }
      }
      if (isPaginated && !NS_FRAME_IS_FULLY_COMPLETE(aStatus) &&
          ShouldAvoidBreakInside(aReflowState.reflowState)) {
        aStatus = NS_INLINE_LINE_BREAK_BEFORE();
        break;
      }
      // see if the rowgroup did not fit on this page might be pushed on
      // the next page
      if (isPaginated &&
          (NS_INLINE_IS_BREAK_BEFORE(aStatus) ||
           (NS_FRAME_IS_COMPLETE(aStatus) &&
            (NS_UNCONSTRAINEDSIZE != kidReflowState.AvailableHeight()) &&
            kidReflowState.AvailableHeight() < desiredSize.Height()))) {
        if (ShouldAvoidBreakInside(aReflowState.reflowState)) {
          aStatus = NS_INLINE_LINE_BREAK_BEFORE();
          break;
        }
        // if we are on top of the page place with dataloss
        if (kidReflowState.mFlags.mIsTopOfPage) {
          if (childX+1 < rowGroups.Length()) {
            nsIFrame* nextRowGroupFrame = rowGroups[childX + 1];
            if (nextRowGroupFrame) {
              PlaceChild(aReflowState, kidFrame, kidPosition, desiredSize,
                         oldKidRect, oldKidVisualOverflow);
              if (allowRepeatedFooter) {
                PlaceRepeatedFooter(aReflowState, tfoot, footerHeight);
              }
              else if (tfoot && tfoot->IsRepeatable()) {
                tfoot->SetRepeatable(false);
              }
              aStatus = NS_FRAME_NOT_COMPLETE;
              PushChildren(rowGroups, childX + 1);
              aLastChildReflowed = kidFrame;
              break;
            }
          }
        }
        else { // we are not on top, push this rowgroup onto the next page
          if (prevKidFrame) { // we had a rowgroup before so push this
            if (allowRepeatedFooter) {
              PlaceRepeatedFooter(aReflowState, tfoot, footerHeight);
            }
            else if (tfoot && tfoot->IsRepeatable()) {
              tfoot->SetRepeatable(false);
            }
            aStatus = NS_FRAME_NOT_COMPLETE;
            PushChildren(rowGroups, childX);
            aLastChildReflowed = prevKidFrame;
            break;
          }
          else { // we can't push so lets make clear how much space we need
            PlaceChild(aReflowState, kidFrame, kidPosition, desiredSize,
                       oldKidRect, oldKidVisualOverflow);
            aLastChildReflowed = kidFrame;
            if (allowRepeatedFooter) {
              PlaceRepeatedFooter(aReflowState, tfoot, footerHeight);
              aLastChildReflowed = tfoot;
            }
            break;
          }
        }
      }

      aLastChildReflowed   = kidFrame;

      pageBreak = false;
      // see if there is a page break after this row group or before the next one
      if (NS_FRAME_IS_COMPLETE(aStatus) && isPaginated &&
          (NS_UNCONSTRAINEDSIZE != kidReflowState.AvailableHeight())) {
        nsIFrame* nextKid =
          (childX + 1 < rowGroups.Length()) ? rowGroups[childX + 1] : nullptr;
        pageBreak = PageBreakAfter(kidFrame, nextKid);
      }

      // Place the child
      PlaceChild(aReflowState, kidFrame, kidPosition, desiredSize, oldKidRect,
                 oldKidVisualOverflow);

      // Remember where we just were in case we end up pushing children
      prevKidFrame = kidFrame;

      // Special handling for incomplete children
      if (NS_FRAME_IS_NOT_COMPLETE(aStatus)) {
        nsIFrame* kidNextInFlow = kidFrame->GetNextInFlow();
        if (!kidNextInFlow) {
          // The child doesn't have a next-in-flow so create a continuing
          // frame. This hooks the child into the flow
          kidNextInFlow = presContext->PresShell()->FrameConstructor()->
            CreateContinuingFrame(presContext, kidFrame, this);

          // Insert the kid's new next-in-flow into our sibling list...
          mFrames.InsertFrame(nullptr, kidFrame, kidNextInFlow);
          // and in rowGroups after childX so that it will get pushed below.
          rowGroups.InsertElementAt(childX + 1,
                      static_cast<nsTableRowGroupFrame*>(kidNextInFlow));
        } else if (kidNextInFlow == kidFrame->GetNextSibling()) {
          // OrderRowGroups excludes NIFs in the child list from 'rowGroups'
          // so we deal with that here to make sure they get pushed.
          MOZ_ASSERT(!rowGroups.Contains(kidNextInFlow),
                     "OrderRowGroups must not put our NIF in 'rowGroups'");
          rowGroups.InsertElementAt(childX + 1,
                      static_cast<nsTableRowGroupFrame*>(kidNextInFlow));
        }

        // We've used up all of our available space so push the remaining
        // children.
        if (allowRepeatedFooter) {
          PlaceRepeatedFooter(aReflowState, tfoot, footerHeight);
        }
        else if (tfoot && tfoot->IsRepeatable()) {
          tfoot->SetRepeatable(false);
        }

        nsIFrame* nextSibling = kidFrame->GetNextSibling();
        if (nextSibling) {
          PushChildren(rowGroups, childX + 1);
        }
        break;
      }
    }
    else { // it isn't being reflowed
      aReflowState.y += cellSpacingY;
      nsRect kidRect = kidFrame->GetNormalRect();
      if (kidRect.y != aReflowState.y) {
        // invalidate the old position
        kidFrame->InvalidateFrameSubtree();
        // move to the new position
        kidFrame->MovePositionBy(nsPoint(0, aReflowState.y - kidRect.y));
        RePositionViews(kidFrame);
        // invalidate the new position
        kidFrame->InvalidateFrameSubtree();
      }
      aReflowState.y += kidRect.height;

      // If our height is constrained then update the available height.
      if (NS_UNCONSTRAINEDSIZE != aReflowState.availSize.height) {
        aReflowState.availSize.height -= cellSpacingY + kidRect.height;
      }
    }
    ConsiderChildOverflow(aOverflowAreas, kidFrame);
  }

  // We've now propagated the column resizes and geometry changes to all
  // the children.
  mBits.mResizedColumns = false;
  ClearGeometryDirty();
}

void
nsTableFrame::ReflowColGroups(nsRenderingContext *aRenderingContext)
{
  if (!GetPrevInFlow() && !HaveReflowedColGroups()) {
    nsHTMLReflowMetrics kidMet(GetWritingMode());
    nsPresContext *presContext = PresContext();
    for (nsIFrame* kidFrame = mColGroups.FirstChild(); kidFrame;
         kidFrame = kidFrame->GetNextSibling()) {
      if (NS_SUBTREE_DIRTY(kidFrame)) {
        // The column groups don't care about dimensions or reflow states.
        nsHTMLReflowState
          kidReflowState(presContext, kidFrame, aRenderingContext,
                         LogicalSize(kidFrame->GetWritingMode()));
        nsReflowStatus cgStatus;
        ReflowChild(kidFrame, presContext, kidMet, kidReflowState, 0, 0, 0,
                    cgStatus);
        FinishReflowChild(kidFrame, presContext, kidMet, nullptr, 0, 0, 0);
      }
    }
    SetHaveReflowedColGroups(true);
  }
}

void
nsTableFrame::CalcDesiredHeight(const nsHTMLReflowState& aReflowState, nsHTMLReflowMetrics& aDesiredSize)
{
  nsTableCellMap* cellMap = GetCellMap();
  if (!cellMap) {
    NS_ASSERTION(false, "never ever call me until the cell map is built!");
    aDesiredSize.Height() = 0;
    return;
  }
  nsMargin borderPadding = GetChildAreaOffset(&aReflowState);

  // get the natural height based on the last child's (row group) rect
  RowGroupArray rowGroups;
  OrderRowGroups(rowGroups);
  if (rowGroups.IsEmpty()) {
    // tables can be used as rectangular items without content
    nscoord tableSpecifiedHeight = CalcBorderBoxHeight(aReflowState);
    if ((NS_UNCONSTRAINEDSIZE != tableSpecifiedHeight) &&
        (tableSpecifiedHeight > 0) &&
        eCompatibility_NavQuirks != PresContext()->CompatibilityMode()) {
          // empty tables should not have a size in quirks mode
      aDesiredSize.Height() = tableSpecifiedHeight;
    }
    else
      aDesiredSize.Height() = 0;
    return;
  }
  int32_t rowCount = cellMap->GetRowCount();
  int32_t colCount = cellMap->GetColCount();
  nscoord desiredHeight = borderPadding.top + borderPadding.bottom;
  if (rowCount > 0 && colCount > 0) {
    desiredHeight += GetRowSpacing(-1);
    for (uint32_t rgX = 0; rgX < rowGroups.Length(); rgX++) {
      desiredHeight += rowGroups[rgX]->GetSize().height +
                       GetRowSpacing(rowGroups[rgX]->GetRowCount() +
                                     rowGroups[rgX]->GetStartRowIndex());
    }
  }

  // see if a specified table height requires dividing additional space to rows
  if (!GetPrevInFlow()) {
    nscoord tableSpecifiedHeight = CalcBorderBoxHeight(aReflowState);
    if ((tableSpecifiedHeight > 0) &&
        (tableSpecifiedHeight != NS_UNCONSTRAINEDSIZE) &&
        (tableSpecifiedHeight > desiredHeight)) {
      // proportionately distribute the excess height to unconstrained rows in each
      // unconstrained row group.
      DistributeHeightToRows(aReflowState, tableSpecifiedHeight - desiredHeight);
      // this might have changed the overflow area incorporate the childframe overflow area.
      for (nsIFrame* kidFrame = mFrames.FirstChild(); kidFrame; kidFrame = kidFrame->GetNextSibling()) {
        ConsiderChildOverflow(aDesiredSize.mOverflowAreas, kidFrame);
      }
      desiredHeight = tableSpecifiedHeight;
    }
  }
  aDesiredSize.Height() = desiredHeight;
}

static
void ResizeCells(nsTableFrame& aTableFrame)
{
  nsTableFrame::RowGroupArray rowGroups;
  aTableFrame.OrderRowGroups(rowGroups);
  WritingMode wm = aTableFrame.GetWritingMode();
  nsHTMLReflowMetrics tableDesiredSize(wm);
  tableDesiredSize.SetSize(wm, aTableFrame.GetLogicalSize(wm));
  tableDesiredSize.SetOverflowAreasToDesiredBounds();

  for (uint32_t rgX = 0; rgX < rowGroups.Length(); rgX++) {
    nsTableRowGroupFrame* rgFrame = rowGroups[rgX];

    nsHTMLReflowMetrics groupDesiredSize(wm);
    groupDesiredSize.SetSize(wm, rgFrame->GetLogicalSize(wm));
    groupDesiredSize.SetOverflowAreasToDesiredBounds();

    nsTableRowFrame* rowFrame = rgFrame->GetFirstRow();
    while (rowFrame) {
      rowFrame->DidResize();
      rgFrame->ConsiderChildOverflow(groupDesiredSize.mOverflowAreas, rowFrame);
      rowFrame = rowFrame->GetNextRow();
    }
    rgFrame->FinishAndStoreOverflow(&groupDesiredSize);
    tableDesiredSize.mOverflowAreas.UnionWith(groupDesiredSize.mOverflowAreas +
                                              rgFrame->GetPosition());
  }
  aTableFrame.FinishAndStoreOverflow(&tableDesiredSize);
}

void
nsTableFrame::DistributeHeightToRows(const nsHTMLReflowState& aReflowState,
                                     nscoord                  aAmount)
{
  nsMargin borderPadding = GetChildAreaOffset(&aReflowState);

  RowGroupArray rowGroups;
  OrderRowGroups(rowGroups);

  nscoord amountUsed = 0;
  // distribute space to each pct height row whose row group doesn't have a computed
  // height, and base the pct on the table height. If the row group had a computed
  // height, then this was already done in nsTableRowGroupFrame::CalculateRowHeights
  nscoord pctBasis = aReflowState.ComputedHeight() - GetRowSpacing(-1, GetRowCount());
  nscoord yOriginRG = borderPadding.top + GetRowSpacing(0);
  nscoord yEndRG = yOriginRG;
  uint32_t rgX;
  for (rgX = 0; rgX < rowGroups.Length(); rgX++) {
    nsTableRowGroupFrame* rgFrame = rowGroups[rgX];
    nscoord amountUsedByRG = 0;
    nscoord yOriginRow = 0;
    nsRect rgNormalRect = rgFrame->GetNormalRect();
    if (!rgFrame->HasStyleHeight()) {
      nsTableRowFrame* rowFrame = rgFrame->GetFirstRow();
      while (rowFrame) {
        nsRect rowNormalRect = rowFrame->GetNormalRect();
        nscoord cellSpacingY = GetRowSpacing(rowFrame->GetRowIndex());
        if ((amountUsed < aAmount) && rowFrame->HasPctHeight()) {
          nscoord pctHeight = rowFrame->GetHeight(pctBasis);
          nscoord amountForRow = std::min(aAmount - amountUsed,
                                          pctHeight - rowNormalRect.height);
          if (amountForRow > 0) {
            // XXXbz we don't need to move the row's y position to yOriginRow?
            nsRect origRowRect = rowFrame->GetRect();
            nscoord newRowHeight = rowNormalRect.height + amountForRow;
            rowFrame->SetSize(nsSize(rowNormalRect.width, newRowHeight));
            yOriginRow += newRowHeight + cellSpacingY;
            yEndRG += newRowHeight + cellSpacingY;
            amountUsed += amountForRow;
            amountUsedByRG += amountForRow;
            //rowFrame->DidResize();
            nsTableFrame::RePositionViews(rowFrame);

            rgFrame->InvalidateFrameWithRect(origRowRect);
            rgFrame->InvalidateFrame();
          }
        }
        else {
          if (amountUsed > 0 && yOriginRow != rowNormalRect.y &&
              !(GetStateBits() & NS_FRAME_FIRST_REFLOW)) {
            rowFrame->InvalidateFrameSubtree();
            rowFrame->MovePositionBy(nsPoint(0, yOriginRow - rowNormalRect.y));
            nsTableFrame::RePositionViews(rowFrame);
            rowFrame->InvalidateFrameSubtree();
          }
          yOriginRow += rowNormalRect.height + cellSpacingY;
          yEndRG += rowNormalRect.height + cellSpacingY;
        }
        rowFrame = rowFrame->GetNextRow();
      }
      if (amountUsed > 0) {
        if (rgNormalRect.y != yOriginRG) {
          rgFrame->InvalidateFrameSubtree();
        }

        nsRect origRgNormalRect = rgFrame->GetRect();
        nsRect origRgVisualOverflow = rgFrame->GetVisualOverflowRect();

        rgFrame->MovePositionBy(nsPoint(0, yOriginRG - rgNormalRect.y));
        rgFrame->SetSize(nsSize(rgNormalRect.width,
                                rgNormalRect.height + amountUsedByRG));

        nsTableFrame::InvalidateTableFrame(rgFrame, origRgNormalRect,
                                           origRgVisualOverflow, false);
      }
    }
    else if (amountUsed > 0 && yOriginRG != rgNormalRect.y) {
      rgFrame->InvalidateFrameSubtree();
      rgFrame->MovePositionBy(nsPoint(0, yOriginRG - rgNormalRect.y));
      // Make sure child views are properly positioned
      nsTableFrame::RePositionViews(rgFrame);
      rgFrame->InvalidateFrameSubtree();
    }
    yOriginRG = yEndRG;
  }

  if (amountUsed >= aAmount) {
    ResizeCells(*this);
    return;
  }

  // get the first row without a style height where its row group has an
  // unconstrained height
  nsTableRowGroupFrame* firstUnStyledRG  = nullptr;
  nsTableRowFrame*      firstUnStyledRow = nullptr;
  for (rgX = 0; rgX < rowGroups.Length() && !firstUnStyledRG; rgX++) {
    nsTableRowGroupFrame* rgFrame = rowGroups[rgX];
    if (!rgFrame->HasStyleHeight()) {
      nsTableRowFrame* rowFrame = rgFrame->GetFirstRow();
      while (rowFrame) {
        if (!rowFrame->HasStyleHeight()) {
          firstUnStyledRG = rgFrame;
          firstUnStyledRow = rowFrame;
          break;
        }
        rowFrame = rowFrame->GetNextRow();
      }
    }
  }

  nsTableRowFrame* lastEligibleRow = nullptr;
  // Accumulate the correct divisor. This will be the total total height of all
  // unstyled rows inside unstyled row groups, unless there are none, in which
  // case, it will be number of all rows. If the unstyled rows don't have a
  // height, divide the space equally among them.
  nscoord divisor = 0;
  int32_t eligibleRows = 0;
  bool expandEmptyRows = false;

  if (!firstUnStyledRow) {
    // there is no unstyled row
    divisor = GetRowCount();
  }
  else {
    for (rgX = 0; rgX < rowGroups.Length(); rgX++) {
      nsTableRowGroupFrame* rgFrame = rowGroups[rgX];
      if (!firstUnStyledRG || !rgFrame->HasStyleHeight()) {
        nsTableRowFrame* rowFrame = rgFrame->GetFirstRow();
        while (rowFrame) {
          if (!firstUnStyledRG || !rowFrame->HasStyleHeight()) {
            NS_ASSERTION(rowFrame->GetSize().height >= 0,
                         "negative row frame height");
            divisor += rowFrame->GetSize().height;
            eligibleRows++;
            lastEligibleRow = rowFrame;
          }
          rowFrame = rowFrame->GetNextRow();
        }
      }
    }
    if (divisor <= 0) {
      if (eligibleRows > 0) {
        expandEmptyRows = true;
      }
      else {
        NS_ERROR("invalid divisor");
        return;
      }
    }
  }
  // allocate the extra height to the unstyled row groups and rows
  nscoord heightToDistribute = aAmount - amountUsed;
  yOriginRG = borderPadding.top + GetRowSpacing(-1);
  yEndRG = yOriginRG;
  for (rgX = 0; rgX < rowGroups.Length(); rgX++) {
    nsTableRowGroupFrame* rgFrame = rowGroups[rgX];
    nscoord amountUsedByRG = 0;
    nscoord yOriginRow = 0;
    nsRect rgNormalRect = rgFrame->GetNormalRect();
    nsRect rgVisualOverflow = rgFrame->GetVisualOverflowRect();
    // see if there is an eligible row group or we distribute to all rows
    if (!firstUnStyledRG || !rgFrame->HasStyleHeight() || !eligibleRows) {
      nsTableRowFrame* rowFrame = rgFrame->GetFirstRow();
      while (rowFrame) {
        nscoord cellSpacingY = GetRowSpacing(rowFrame->GetRowIndex());
        nsRect rowNormalRect = rowFrame->GetNormalRect();
        nsRect rowVisualOverflow = rowFrame->GetVisualOverflowRect();
        // see if there is an eligible row or we distribute to all rows
        if (!firstUnStyledRow || !rowFrame->HasStyleHeight() || !eligibleRows) {
          float ratio;
          if (eligibleRows) {
            if (!expandEmptyRows) {
              // The amount of additional space each row gets is proportional to
              // its height
              ratio = float(rowNormalRect.height) / float(divisor);
            } else {
              // empty rows get all the same additional space
              ratio = 1.0f / float(eligibleRows);
            }
          }
          else {
            // all rows get the same additional space
            ratio = 1.0f / float(divisor);
          }
          // give rows their additional space, except for the last row which
          // gets the remainder
          nscoord amountForRow = (rowFrame == lastEligibleRow)
                                 ? aAmount - amountUsed : NSToCoordRound(((float)(heightToDistribute)) * ratio);
          amountForRow = std::min(amountForRow, aAmount - amountUsed);

          if (yOriginRow != rowNormalRect.y) {
            rowFrame->InvalidateFrameSubtree();
          }

          // update the row height
          nsRect origRowRect = rowFrame->GetRect();
          nscoord newRowHeight = rowNormalRect.height + amountForRow;
          rowFrame->MovePositionBy(nsPoint(0, yOriginRow - rowNormalRect.y));
          rowFrame->SetSize(nsSize(rowNormalRect.width, newRowHeight));

          yOriginRow += newRowHeight + cellSpacingY;
          yEndRG += newRowHeight + cellSpacingY;

          amountUsed += amountForRow;
          amountUsedByRG += amountForRow;
          NS_ASSERTION((amountUsed <= aAmount), "invalid row allocation");
          //rowFrame->DidResize();
          nsTableFrame::RePositionViews(rowFrame);

          nsTableFrame::InvalidateTableFrame(rowFrame, origRowRect,
                                             rowVisualOverflow, false);
        }
        else {
          if (amountUsed > 0 && yOriginRow != rowNormalRect.y) {
            rowFrame->InvalidateFrameSubtree();
            rowFrame->MovePositionBy(nsPoint(0, yOriginRow - rowNormalRect.y));
            nsTableFrame::RePositionViews(rowFrame);
            rowFrame->InvalidateFrameSubtree();
          }
          yOriginRow += rowNormalRect.height + cellSpacingY;
          yEndRG += rowNormalRect.height + cellSpacingY;
        }
        rowFrame = rowFrame->GetNextRow();
      }
      if (amountUsed > 0) {
        if (rgNormalRect.y != yOriginRG) {
          rgFrame->InvalidateFrameSubtree();
        }

        nsRect origRgNormalRect = rgFrame->GetRect();
        rgFrame->MovePositionBy(nsPoint(0, yOriginRG - rgNormalRect.y));
        rgFrame->SetSize(nsSize(rgNormalRect.width,
                                rgNormalRect.height + amountUsedByRG));

        nsTableFrame::InvalidateTableFrame(rgFrame, origRgNormalRect,
                                           rgVisualOverflow, false);
      }
      // Make sure child views are properly positioned
    }
    else if (amountUsed > 0 && yOriginRG != rgNormalRect.y) {
      rgFrame->InvalidateFrameSubtree();
      rgFrame->MovePositionBy(nsPoint(0, yOriginRG - rgNormalRect.y));
      // Make sure child views are properly positioned
      nsTableFrame::RePositionViews(rgFrame);
      rgFrame->InvalidateFrameSubtree();
    }
    yOriginRG = yEndRG;
  }

  ResizeCells(*this);
}

int32_t nsTableFrame::GetColumnWidth(int32_t aColIndex)
{
  nsTableFrame* firstInFlow = static_cast<nsTableFrame*>(FirstInFlow());
  if (this == firstInFlow) {
    nsTableColFrame* colFrame = GetColFrame(aColIndex);
    return colFrame ? colFrame->GetFinalISize() : 0;
  }
  return firstInFlow->GetColumnWidth(aColIndex);
}

nscoord nsTableFrame::GetColSpacing()
{
  if (IsBorderCollapse())
    return 0;

  return StyleTableBorder()->mBorderSpacingCol;
}

// XXX: could cache this.  But be sure to check style changes if you do!
nscoord nsTableFrame::GetColSpacing(int32_t aColIndex)
{
  NS_ASSERTION(aColIndex >= -1 && aColIndex <= GetColCount(),
               "Column index exceeds the bounds of the table");
  // Index is irrelevant for ordinary tables.  We check that it falls within
  // appropriate bounds to increase confidence of correctness in situations
  // where it does matter.
  return GetColSpacing();
}

nscoord nsTableFrame::GetColSpacing(int32_t aStartColIndex,
                                      int32_t aEndColIndex)
{
  NS_ASSERTION(aStartColIndex >= -1 && aStartColIndex <= GetColCount(),
               "Start column index exceeds the bounds of the table");
  NS_ASSERTION(aEndColIndex >= -1 && aEndColIndex <= GetColCount(),
               "End column index exceeds the bounds of the table");
  NS_ASSERTION(aStartColIndex <= aEndColIndex,
               "End index must not be less than start index");
  // Only one possible value so just multiply it out. Tables where index
  // matters will override this function
  return GetColSpacing() * (aEndColIndex - aStartColIndex);
}

nscoord nsTableFrame::GetRowSpacing()
{
  if (IsBorderCollapse())
    return 0;

  return StyleTableBorder()->mBorderSpacingRow;
}

// XXX: could cache this. But be sure to check style changes if you do!
nscoord nsTableFrame::GetRowSpacing(int32_t aRowIndex)
{
  NS_ASSERTION(aRowIndex >= -1 && aRowIndex <= GetRowCount(),
               "Row index exceeds the bounds of the table");
  // Index is irrelevant for ordinary tables.  We check that it falls within
  // appropriate bounds to increase confidence of correctness in situations
  // where it does matter.
  return GetRowSpacing();
}

nscoord nsTableFrame::GetRowSpacing(int32_t aStartRowIndex,
                                      int32_t aEndRowIndex)
{
  NS_ASSERTION(aStartRowIndex >= -1 && aStartRowIndex <= GetRowCount(),
               "Start row index exceeds the bounds of the table");
  NS_ASSERTION(aEndRowIndex >= -1 && aEndRowIndex <= GetRowCount(),
               "End row index exceeds the bounds of the table");
  NS_ASSERTION(aStartRowIndex <= aEndRowIndex,
               "End index must not be less than start index");
  // Only one possible value so just multiply it out. Tables where index
  // matters will override this function
  return GetRowSpacing() * (aEndRowIndex - aStartRowIndex);
}

/* virtual */ nscoord
nsTableFrame::GetLogicalBaseline(WritingMode aWritingMode) const
{
  nscoord ascent = 0;
  RowGroupArray orderedRowGroups;
  OrderRowGroups(orderedRowGroups);
  nsTableRowFrame* firstRow = nullptr;
  // XXX not sure if this should be the width of the containing block instead.
  nscoord containerWidth = mRect.width;
  for (uint32_t rgIndex = 0; rgIndex < orderedRowGroups.Length(); rgIndex++) {
    nsTableRowGroupFrame* rgFrame = orderedRowGroups[rgIndex];
    if (rgFrame->GetRowCount()) {
      firstRow = rgFrame->GetFirstRow();

      nscoord rgNormalBStart =
        LogicalRect(aWritingMode, rgFrame->GetNormalRect(), containerWidth)
        .Origin(aWritingMode).B(aWritingMode);
      nscoord firstRowNormalBStart =
        LogicalRect(aWritingMode, firstRow->GetNormalRect(), containerWidth)
        .Origin(aWritingMode).B(aWritingMode);

      ascent = rgNormalBStart + firstRowNormalBStart +
               firstRow->GetRowBaseline(aWritingMode);
      break;
    }
  }
  if (!firstRow)
    ascent = BSize(aWritingMode);
  return ascent;
}
/* ----- global methods ----- */

nsTableFrame*
NS_NewTableFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsTableFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsTableFrame)

nsTableFrame*
nsTableFrame::GetTableFrame(nsIFrame* aFrame)
{
  for (nsIFrame* ancestor = aFrame->GetParent(); ancestor;
       ancestor = ancestor->GetParent()) {
    if (nsGkAtoms::tableFrame == ancestor->GetType()) {
      return static_cast<nsTableFrame*>(ancestor);
    }
  }
  NS_RUNTIMEABORT("unable to find table parent");
  return nullptr;
}

nsTableFrame*
nsTableFrame::GetTableFramePassingThrough(nsIFrame* aMustPassThrough,
                                          nsIFrame* aFrame)
{
  MOZ_ASSERT(aMustPassThrough == aFrame ||
             nsLayoutUtils::IsProperAncestorFrame(aMustPassThrough, aFrame),
             "aMustPassThrough should be an ancestor");

  // Retrieve the table frame, and ensure that we hit aMustPassThrough on the
  // way.  If we don't, just return null.
  bool hitPassThroughFrame = false;
  nsTableFrame* tableFrame = nullptr;
  for (nsIFrame* ancestor = aFrame; ancestor; ancestor = ancestor->GetParent()) {
    if (ancestor == aMustPassThrough) {
      hitPassThroughFrame = true;
    }
    if (nsGkAtoms::tableFrame == ancestor->GetType()) {
      tableFrame = static_cast<nsTableFrame*>(ancestor);
      break;
    }
  }

  MOZ_ASSERT(tableFrame, "Should have a table frame here");
  return hitPassThroughFrame ? tableFrame : nullptr;
}

bool
nsTableFrame::IsAutoHeight()
{
  const nsStyleCoord &height = StylePosition()->mHeight;
  // Don't consider calc() here like this quirk for percent.
  return height.GetUnit() == eStyleUnit_Auto ||
         (height.GetUnit() == eStyleUnit_Percent &&
          height.GetPercentValue() <= 0.0f);
}

nscoord
nsTableFrame::CalcBorderBoxHeight(const nsHTMLReflowState& aState)
{
  nscoord height = aState.ComputedHeight();
  if (NS_AUTOHEIGHT != height) {
    nsMargin borderPadding = GetChildAreaOffset(&aState);
    height += borderPadding.top + borderPadding.bottom;
  }
  height = std::max(0, height);

  return height;
}

bool
nsTableFrame::IsAutoLayout()
{
  if (StyleTable()->mLayoutStrategy == NS_STYLE_TABLE_LAYOUT_AUTO)
    return true;
  // a fixed-layout inline-table must have a width
  // and tables with 'width: -moz-max-content' must be auto-layout
  // (at least as long as FixedTableLayoutStrategy::GetPrefISize returns
  // nscoord_MAX)
  const nsStyleCoord &width = StylePosition()->mWidth;
  return (width.GetUnit() == eStyleUnit_Auto) ||
         (width.GetUnit() == eStyleUnit_Enumerated &&
          width.GetIntValue() == NS_STYLE_WIDTH_MAX_CONTENT);
}

#ifdef DEBUG_FRAME_DUMP
nsresult
nsTableFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("Table"), aResult);
}
#endif

// Find the closet sibling before aPriorChildFrame (including aPriorChildFrame) that
// is of type aChildType
nsIFrame*
nsTableFrame::GetFrameAtOrBefore(nsIFrame*       aParentFrame,
                                 nsIFrame*       aPriorChildFrame,
                                 nsIAtom*        aChildType)
{
  nsIFrame* result = nullptr;
  if (!aPriorChildFrame) {
    return result;
  }
  if (aChildType == aPriorChildFrame->GetType()) {
    return aPriorChildFrame;
  }

  // aPriorChildFrame is not of type aChildType, so we need start from
  // the beginnng and find the closest one
  nsIFrame* lastMatchingFrame = nullptr;
  nsIFrame* childFrame = aParentFrame->GetFirstPrincipalChild();
  while (childFrame && (childFrame != aPriorChildFrame)) {
    if (aChildType == childFrame->GetType()) {
      lastMatchingFrame = childFrame;
    }
    childFrame = childFrame->GetNextSibling();
  }
  return lastMatchingFrame;
}

#ifdef DEBUG
void
nsTableFrame::DumpRowGroup(nsIFrame* aKidFrame)
{
  if (!aKidFrame)
    return;

  nsIFrame* cFrame = aKidFrame->GetFirstPrincipalChild();
  while (cFrame) {
    nsTableRowFrame *rowFrame = do_QueryFrame(cFrame);
    if (rowFrame) {
      printf("row(%d)=%p ", rowFrame->GetRowIndex(),
             static_cast<void*>(rowFrame));
      nsIFrame* childFrame = cFrame->GetFirstPrincipalChild();
      while (childFrame) {
        nsTableCellFrame *cellFrame = do_QueryFrame(childFrame);
        if (cellFrame) {
          int32_t colIndex;
          cellFrame->GetColIndex(colIndex);
          printf("cell(%d)=%p ", colIndex, static_cast<void*>(childFrame));
        }
        childFrame = childFrame->GetNextSibling();
      }
      printf("\n");
    }
    else {
      DumpRowGroup(rowFrame);
    }
    cFrame = cFrame->GetNextSibling();
  }
}

void
nsTableFrame::Dump(bool            aDumpRows,
                   bool            aDumpCols,
                   bool            aDumpCellMap)
{
  printf("***START TABLE DUMP*** \n");
  // dump the columns widths array
  printf("mColWidths=");
  int32_t numCols = GetColCount();
  int32_t colX;
  for (colX = 0; colX < numCols; colX++) {
    printf("%d ", GetColumnWidth(colX));
  }
  printf("\n");

  if (aDumpRows) {
    nsIFrame* kidFrame = mFrames.FirstChild();
    while (kidFrame) {
      DumpRowGroup(kidFrame);
      kidFrame = kidFrame->GetNextSibling();
    }
  }

  if (aDumpCols) {
	  // output col frame cache
    printf("\n col frame cache ->");
	   for (colX = 0; colX < numCols; colX++) {
      nsTableColFrame* colFrame = mColFrames.ElementAt(colX);
      if (0 == (colX % 8)) {
        printf("\n");
      }
      printf ("%d=%p ", colX, static_cast<void*>(colFrame));
      nsTableColType colType = colFrame->GetColType();
      switch (colType) {
      case eColContent:
        printf(" content ");
        break;
      case eColAnonymousCol:
        printf(" anonymous-column ");
        break;
      case eColAnonymousColGroup:
        printf(" anonymous-colgroup ");
        break;
      case eColAnonymousCell:
        printf(" anonymous-cell ");
        break;
      }
    }
    printf("\n colgroups->");
    for (nsIFrame* childFrame = mColGroups.FirstChild(); childFrame;
         childFrame = childFrame->GetNextSibling()) {
      if (nsGkAtoms::tableColGroupFrame == childFrame->GetType()) {
        nsTableColGroupFrame* colGroupFrame = (nsTableColGroupFrame *)childFrame;
        colGroupFrame->Dump(1);
      }
    }
    for (colX = 0; colX < numCols; colX++) {
      printf("\n");
      nsTableColFrame* colFrame = GetColFrame(colX);
      colFrame->Dump(1);
    }
  }
  if (aDumpCellMap) {
    nsTableCellMap* cellMap = GetCellMap();
    cellMap->Dump();
  }
  printf(" ***END TABLE DUMP*** \n");
}
#endif

// nsTableIterator
nsTableIterator::nsTableIterator(nsIFrame& aSource)
{
  nsIFrame* firstChild = aSource.GetFirstPrincipalChild();
  Init(firstChild);
}

nsTableIterator::nsTableIterator(nsFrameList& aSource)
{
  nsIFrame* firstChild = aSource.FirstChild();
  Init(firstChild);
}

void nsTableIterator::Init(nsIFrame* aFirstChild)
{
  mFirstListChild = aFirstChild;
  mFirstChild     = aFirstChild;
  mCurrentChild   = nullptr;
  mLeftToRight    = true;
  mCount          = -1;

  if (!mFirstChild) {
    return;
  }

  nsTableFrame* table = nsTableFrame::GetTableFrame(mFirstChild);
  mLeftToRight = (NS_STYLE_DIRECTION_LTR ==
                  table->StyleVisibility()->mDirection);

  if (!mLeftToRight) {
    mCount = 0;
    nsIFrame* nextChild = mFirstChild->GetNextSibling();
    while (nullptr != nextChild) {
      mCount++;
      mFirstChild = nextChild;
      nextChild = nextChild->GetNextSibling();
    }
  }
}

nsIFrame* nsTableIterator::First()
{
  mCurrentChild = mFirstChild;
  return mCurrentChild;
}

nsIFrame* nsTableIterator::Next()
{
  if (!mCurrentChild) {
    return nullptr;
  }

  if (mLeftToRight) {
    mCurrentChild = mCurrentChild->GetNextSibling();
    return mCurrentChild;
  }
  else {
    nsIFrame* targetChild = mCurrentChild;
    mCurrentChild = nullptr;
    nsIFrame* child = mFirstListChild;
    while (child && (child != targetChild)) {
      mCurrentChild = child;
      child = child->GetNextSibling();
    }
    return mCurrentChild;
  }
}

bool nsTableIterator::IsLeftToRight()
{
  return mLeftToRight;
}

int32_t nsTableIterator::Count()
{
  if (-1 == mCount) {
    mCount = 0;
    nsIFrame* child = mFirstListChild;
    while (nullptr != child) {
      mCount++;
      child = child->GetNextSibling();
    }
  }
  return mCount;
}

bool
nsTableFrame::ColumnHasCellSpacingBefore(int32_t aColIndex) const
{
  // Since fixed-layout tables should not have their column sizes change
  // as they load, we assume that all columns are significant.
  if (LayoutStrategy()->GetType() == nsITableLayoutStrategy::Fixed)
    return true;
  // the first column is always significant
  if (aColIndex == 0)
    return true;
  nsTableCellMap* cellMap = GetCellMap();
  if (!cellMap)
    return false;
  return cellMap->GetNumCellsOriginatingInCol(aColIndex) > 0;
}

/********************************************************************************
 * Collapsing Borders
 *
 *  The CSS spec says to resolve border conflicts in this order:
 *  1) any border with the style HIDDEN wins
 *  2) the widest border with a style that is not NONE wins
 *  3) the border styles are ranked in this order, highest to lowest precedence:
 *     double, solid, dashed, dotted, ridge, outset, groove, inset
 *  4) borders that are of equal width and style (differ only in color) have this precedence:
 *     cell, row, rowgroup, col, colgroup, table
 *  5) if all border styles are NONE, then that's the computed border style.
 *******************************************************************************/

#ifdef DEBUG
#define VerifyNonNegativeDamageRect(r)                                  \
  NS_ASSERTION((r).x >= 0, "negative col index");                       \
  NS_ASSERTION((r).y >= 0, "negative row index");                       \
  NS_ASSERTION((r).width >= 0, "negative horizontal damage");           \
  NS_ASSERTION((r).height >= 0, "negative vertical damage");
#define VerifyDamageRect(r)                                             \
  VerifyNonNegativeDamageRect(r);                                       \
  NS_ASSERTION((r).XMost() <= GetColCount(),                            \
               "horizontal damage extends outside table");              \
  NS_ASSERTION((r).YMost() <= GetRowCount(),                            \
               "vertical damage extends outside table");
#endif

void
nsTableFrame::AddBCDamageArea(const nsIntRect& aValue)
{
  NS_ASSERTION(IsBorderCollapse(), "invalid AddBCDamageArea call");
#ifdef DEBUG
  VerifyDamageRect(aValue);
#endif

  SetNeedToCalcBCBorders(true);
  // Get the property
  BCPropertyData* value = GetBCProperty(true);
  if (value) {
#ifdef DEBUG
    VerifyNonNegativeDamageRect(value->mDamageArea);
#endif
    // Clamp the old damage area to the current table area in case it shrunk.
    int32_t cols = GetColCount();
    if (value->mDamageArea.XMost() > cols) {
      if (value->mDamageArea.x > cols) {
        value->mDamageArea.x = cols;
        value->mDamageArea.width = 0;
      }
      else {
        value->mDamageArea.width = cols - value->mDamageArea.x;
      }
    }
    int32_t rows = GetRowCount();
    if (value->mDamageArea.YMost() > rows) {
      if (value->mDamageArea.y > rows) {
        value->mDamageArea.y = rows;
        value->mDamageArea.height = 0;
      }
      else {
        value->mDamageArea.height = rows - value->mDamageArea.y;
      }
    }

    // Construct a union of the new and old damage areas.
    value->mDamageArea.UnionRect(value->mDamageArea, aValue);
  }
}


void
nsTableFrame::SetFullBCDamageArea()
{
  NS_ASSERTION(IsBorderCollapse(), "invalid SetFullBCDamageArea call");

  SetNeedToCalcBCBorders(true);

  BCPropertyData* value = GetBCProperty(true);
  if (value) {
    value->mDamageArea = nsIntRect(0, 0, GetColCount(), GetRowCount());
  }
}


/* BCCellBorder represents a border segment which can be either a horizontal
 * or a vertical segment. For each segment we need to know the color, width,
 * style, who owns it and how long it is in cellmap coordinates.
 * Ownership of these segments is important to calculate which corners should
 * be bevelled. This structure has dual use, its used first to compute the
 * dominant border for horizontal and vertical segments and to store the
 * preliminary computed border results in the BCCellBorders structure.
 * This temporary storage is not symmetric with respect to horizontal and
 * vertical border segments, its always column oriented. For each column in
 * the cellmap there is a temporary stored vertical and horizontal segment.
 * XXX_Bernd this asymmetry is the root of those rowspan bc border errors
 */
struct BCCellBorder
{
  BCCellBorder() { Reset(0, 1); }
  void Reset(uint32_t aRowIndex, uint32_t aRowSpan);
  nscolor       color;    // border segment color
  BCPixelSize   width;    // border segment width in pixel coordinates !!
  uint8_t       style;    // border segment style, possible values are defined
                          // in nsStyleConsts.h as NS_STYLE_BORDER_STYLE_*
  BCBorderOwner owner;    // border segment owner, possible values are defined
                          // in celldata.h. In the cellmap for each border
                          // segment we store the owner and later when
                          // painting we know the owner and can retrieve the
                          // style info from the corresponding frame
  int32_t       rowIndex; // rowIndex of temporary stored horizontal border
                          // segments relative to the table
  int32_t       rowSpan;  // row span of temporary stored horizontal border
                          // segments
};

void
BCCellBorder::Reset(uint32_t aRowIndex,
                    uint32_t aRowSpan)
{
  style = NS_STYLE_BORDER_STYLE_NONE;
  color = 0;
  width = 0;
  owner = eTableOwner;
  rowIndex = aRowIndex;
  rowSpan  = aRowSpan;
}

class BCMapCellIterator;

/*****************************************************************
 *  BCMapCellInfo
 * This structure stores information about the cellmap and all involved
 * table related frames that are used during the computation of winning borders
 * in CalcBCBorders so that they do need to be looked up again and again when
 * iterating over the cells.
 ****************************************************************/
struct BCMapCellInfo
{
  explicit BCMapCellInfo(nsTableFrame* aTableFrame);
  void ResetCellInfo();
  void SetInfo(nsTableRowFrame*   aNewRow,
               int32_t            aColIndex,
               BCCellData*        aCellData,
               BCMapCellIterator* aIter,
               nsCellMap*         aCellMap = nullptr);
  // The BCMapCellInfo has functions to set the continous
  // border widths (see nsTablePainter.cpp for a description of the continous
  // borders concept). The widths are computed inside these functions based on
  // the current position inside the table and the cached frames that correspond
  // to this position. The widths are stored in member variables of the internal
  // table frames.
  void SetTableTopLeftContBCBorder();
  void SetRowGroupLeftContBCBorder();
  void SetRowGroupRightContBCBorder();
  void SetRowGroupBottomContBCBorder();
  void SetRowLeftContBCBorder();
  void SetRowRightContBCBorder();
  void SetColumnTopRightContBCBorder();
  void SetColumnBottomContBCBorder();
  void SetColGroupBottomContBCBorder();
  void SetInnerRowGroupBottomContBCBorder(const nsIFrame* aNextRowGroup,
                                          nsTableRowFrame* aNextRow);

  // functions to set the border widths on the table related frames, where the
  // knowledge about the current position in the table is used.
  void SetTableTopBorderWidth(BCPixelSize aWidth);
  void SetTableLeftBorderWidth(int32_t aRowY, BCPixelSize aWidth);
  void SetTableRightBorderWidth(int32_t aRowY, BCPixelSize aWidth);
  void SetTableBottomBorderWidth(BCPixelSize aWidth);
  void SetLeftBorderWidths(BCPixelSize aWidth);
  void SetRightBorderWidths(BCPixelSize aWidth);
  void SetTopBorderWidths(BCPixelSize aWidth);
  void SetBottomBorderWidths(BCPixelSize aWidth);

  // functions to compute the borders; they depend on the
  // knowledge about the current position in the table. The edge functions
  // should be called if a table edge is involved, otherwise the internal
  // functions should be called.
  BCCellBorder GetTopEdgeBorder();
  BCCellBorder GetBottomEdgeBorder();
  BCCellBorder GetLeftEdgeBorder();
  BCCellBorder GetRightEdgeBorder();
  BCCellBorder GetRightInternalBorder();
  BCCellBorder GetLeftInternalBorder();
  BCCellBorder GetTopInternalBorder();
  BCCellBorder GetBottomInternalBorder();

  // functions to set the interal position information
  void SetColumn(int32_t aColX);
  // Increment the row as we loop over the rows of a rowspan
  void IncrementRow(bool aResetToTopRowOfCell = false);

  // Helper functions to get extent of the cell
  int32_t GetCellEndRowIndex() const;
  int32_t GetCellEndColIndex() const;

  // storage of table information
  nsTableFrame*         mTableFrame;
  int32_t               mNumTableRows;
  int32_t               mNumTableCols;
  BCPropertyData*       mTableBCData;

  // storage of table ltr information, the border collapse code swaps the sides
  // to account for rtl tables, this is done through mStartSide and mEndSide
  bool                  mTableIsLTR;
  mozilla::css::Side    mStartSide;
  mozilla::css::Side    mEndSide;

  // a cell can only belong to one rowgroup
  nsTableRowGroupFrame* mRowGroup;

  // a cell with a rowspan has a top and a bottom row, and rows in between
  nsTableRowFrame*      mTopRow;
  nsTableRowFrame*      mBottomRow;
  nsTableRowFrame*      mCurrentRowFrame;

  // a cell with a colspan has a left and right column and columns in between
  // they can belong to different colgroups
  nsTableColGroupFrame* mColGroup;
  nsTableColGroupFrame* mCurrentColGroupFrame;

  nsTableColFrame*      mLeftCol;
  nsTableColFrame*      mRightCol;
  nsTableColFrame*      mCurrentColFrame;

  // cell information
  BCCellData*           mCellData;
  nsBCTableCellFrame*   mCell;

  int32_t               mRowIndex;
  int32_t               mRowSpan;
  int32_t               mColIndex;
  int32_t               mColSpan;

  // flags to describe the position of the cell with respect to the row- and
  // colgroups, for instance mRgAtTop documents that the top cell border hits
  // a rowgroup border
  bool                  mRgAtTop;
  bool                  mRgAtBottom;
  bool                  mCgAtLeft;
  bool                  mCgAtRight;

};


BCMapCellInfo::BCMapCellInfo(nsTableFrame* aTableFrame)
{
  mTableFrame = aTableFrame;
  mTableIsLTR =
    aTableFrame->StyleVisibility()->mDirection == NS_STYLE_DIRECTION_LTR;
  if (mTableIsLTR) {
    mStartSide = NS_SIDE_LEFT;
    mEndSide = NS_SIDE_RIGHT;
  }
  else {
    mStartSide = NS_SIDE_RIGHT;
    mEndSide = NS_SIDE_LEFT;
  }
  mNumTableRows = mTableFrame->GetRowCount();
  mNumTableCols = mTableFrame->GetColCount();
  mTableBCData = static_cast<BCPropertyData*>
    (mTableFrame->Properties().Get(TableBCProperty()));

  ResetCellInfo();
}

void BCMapCellInfo::ResetCellInfo()
{
  mCellData  = nullptr;
  mRowGroup  = nullptr;
  mTopRow    = nullptr;
  mBottomRow = nullptr;
  mColGroup  = nullptr;
  mLeftCol   = nullptr;
  mRightCol  = nullptr;
  mCell      = nullptr;
  mRowIndex  = mRowSpan = mColIndex = mColSpan = 0;
  mRgAtTop = mRgAtBottom = mCgAtLeft = mCgAtRight = false;
}

inline int32_t BCMapCellInfo::GetCellEndRowIndex() const
{
  return mRowIndex + mRowSpan - 1;
}

inline int32_t BCMapCellInfo::GetCellEndColIndex() const
{
  return mColIndex + mColSpan - 1;
}


class BCMapCellIterator
{
public:
  BCMapCellIterator(nsTableFrame* aTableFrame,
                    const nsIntRect& aDamageArea);

  void First(BCMapCellInfo& aMapCellInfo);

  void Next(BCMapCellInfo& aMapCellInfo);

  void PeekRight(BCMapCellInfo& aRefInfo,
                 uint32_t     aRowIndex,
                 BCMapCellInfo& aAjaInfo);

  void PeekBottom(BCMapCellInfo& aRefInfo,
                  uint32_t     aColIndex,
                  BCMapCellInfo& aAjaInfo);

  bool IsNewRow() { return mIsNewRow; }

  nsTableRowFrame* GetPrevRow() const { return mPrevRow; }
  nsTableRowFrame* GetCurrentRow() const { return mRow; }
  nsTableRowGroupFrame* GetCurrentRowGroup() const { return mRowGroup;}

  int32_t    mRowGroupStart;
  int32_t    mRowGroupEnd;
  bool       mAtEnd;
  nsCellMap* mCellMap;

private:
  bool SetNewRow(nsTableRowFrame* row = nullptr);
  bool SetNewRowGroup(bool aFindFirstDamagedRow);

  nsTableFrame*         mTableFrame;
  nsTableCellMap*       mTableCellMap;
  nsTableFrame::RowGroupArray mRowGroups;
  nsTableRowGroupFrame* mRowGroup;
  int32_t               mRowGroupIndex;
  uint32_t              mNumTableRows;
  nsTableRowFrame*      mRow;
  nsTableRowFrame*      mPrevRow;
  bool                  mIsNewRow;
  int32_t               mRowIndex;
  uint32_t              mNumTableCols;
  int32_t               mColIndex;
  nsPoint               mAreaStart;
  nsPoint               mAreaEnd;
};

BCMapCellIterator::BCMapCellIterator(nsTableFrame* aTableFrame,
                                     const nsIntRect& aDamageArea)
:mTableFrame(aTableFrame)
{
  mTableCellMap  = aTableFrame->GetCellMap();

  mAreaStart.x   = aDamageArea.x;
  mAreaStart.y   = aDamageArea.y;
  mAreaEnd.y     = aDamageArea.y + aDamageArea.height - 1;
  mAreaEnd.x     = aDamageArea.x + aDamageArea.width - 1;

  mNumTableRows  = mTableFrame->GetRowCount();
  mRow           = nullptr;
  mRowIndex      = 0;
  mNumTableCols  = mTableFrame->GetColCount();
  mColIndex      = 0;
  mRowGroupIndex = -1;

  // Get the ordered row groups
  aTableFrame->OrderRowGroups(mRowGroups);

  mAtEnd = true; // gets reset when First() is called
}

// fill fields that we need for border collapse computation on a given cell
void
BCMapCellInfo::SetInfo(nsTableRowFrame*   aNewRow,
                       int32_t            aColIndex,
                       BCCellData*        aCellData,
                       BCMapCellIterator* aIter,
                       nsCellMap*         aCellMap)
{
  // fill the cell information
  mCellData = aCellData;
  mColIndex = aColIndex;

  // initialize the row information if it was not previously set for cells in
  // this row
  mRowIndex = 0;
  if (aNewRow) {
    mTopRow = aNewRow;
    mRowIndex = aNewRow->GetRowIndex();
  }

  // fill cell frame info and row information
  mCell      = nullptr;
  mRowSpan   = 1;
  mColSpan   = 1;
  if (aCellData) {
    mCell = static_cast<nsBCTableCellFrame*>(aCellData->GetCellFrame());
    if (mCell) {
      if (!mTopRow) {
        mTopRow = static_cast<nsTableRowFrame*>(mCell->GetParent());
        if (!mTopRow) ABORT0();
        mRowIndex = mTopRow->GetRowIndex();
      }
      mColSpan = mTableFrame->GetEffectiveColSpan(*mCell, aCellMap);
      mRowSpan = mTableFrame->GetEffectiveRowSpan(*mCell, aCellMap);
    }
  }

  if (!mTopRow) {
    mTopRow = aIter->GetCurrentRow();
  }
  if (1 == mRowSpan) {
    mBottomRow = mTopRow;
  }
  else {
    mBottomRow = mTopRow->GetNextRow();
    if (mBottomRow) {
      for (int32_t spanY = 2; mBottomRow && (spanY < mRowSpan); spanY++) {
        mBottomRow = mBottomRow->GetNextRow();
      }
      NS_ASSERTION(mBottomRow, "spanned row not found");
    }
    else {
      NS_ASSERTION(false, "error in cell map");
      mRowSpan = 1;
      mBottomRow = mTopRow;
    }
  }
  // row group frame info
  // try to reuse the rgStart and rgEnd from the iterator as calls to
  // GetRowCount() are computationally expensive and should be avoided if
  // possible
  uint32_t rgStart  = aIter->mRowGroupStart;
  uint32_t rgEnd    = aIter->mRowGroupEnd;
  mRowGroup = static_cast<nsTableRowGroupFrame*>(mTopRow->GetParent());
  if (mRowGroup != aIter->GetCurrentRowGroup()) {
    rgStart = mRowGroup->GetStartRowIndex();
    rgEnd   = rgStart + mRowGroup->GetRowCount() - 1;
  }
  uint32_t rowIndex = mTopRow->GetRowIndex();
  mRgAtTop    = (rgStart == rowIndex);
  mRgAtBottom = (rgEnd == rowIndex + mRowSpan - 1);

   // col frame info
  mLeftCol = mTableFrame->GetColFrame(aColIndex);
  if (!mLeftCol) ABORT0();

  mRightCol = mLeftCol;
  if (mColSpan > 1) {
    nsTableColFrame* colFrame = mTableFrame->GetColFrame(aColIndex +
                                                         mColSpan -1);
    if (!colFrame) ABORT0();
    mRightCol = colFrame;
  }

  // col group frame info
  mColGroup = static_cast<nsTableColGroupFrame*>(mLeftCol->GetParent());
  int32_t cgStart = mColGroup->GetStartColumnIndex();
  int32_t cgEnd = std::max(0, cgStart + mColGroup->GetColCount() - 1);
  mCgAtLeft  = (cgStart == aColIndex);
  mCgAtRight = (cgEnd == aColIndex + mColSpan - 1);
}

bool
BCMapCellIterator::SetNewRow(nsTableRowFrame* aRow)
{
  mAtEnd   = true;
  mPrevRow = mRow;
  if (aRow) {
    mRow = aRow;
  }
  else if (mRow) {
    mRow = mRow->GetNextRow();
  }
  if (mRow) {
    mRowIndex = mRow->GetRowIndex();
    // get to the first entry with an originating cell
    int32_t rgRowIndex = mRowIndex - mRowGroupStart;
    if (uint32_t(rgRowIndex) >= mCellMap->mRows.Length())
      ABORT1(false);
    const nsCellMap::CellDataArray& row = mCellMap->mRows[rgRowIndex];

    for (mColIndex = mAreaStart.x; mColIndex <= mAreaEnd.x; mColIndex++) {
      CellData* cellData = row.SafeElementAt(mColIndex);
      if (!cellData) { // add a dead cell data
        nsIntRect damageArea;
        cellData = mCellMap->AppendCell(*mTableCellMap, nullptr, rgRowIndex,
                                        false, 0, damageArea);
        if (!cellData) ABORT1(false);
      }
      if (cellData && (cellData->IsOrig() || cellData->IsDead())) {
        break;
      }
    }
    mIsNewRow = true;
    mAtEnd    = false;
  }
  else ABORT1(false);

  return !mAtEnd;
}

bool
BCMapCellIterator::SetNewRowGroup(bool aFindFirstDamagedRow)
{
   mAtEnd = true;
  int32_t numRowGroups = mRowGroups.Length();
  mCellMap = nullptr;
  for (mRowGroupIndex++; mRowGroupIndex < numRowGroups; mRowGroupIndex++) {
    mRowGroup = mRowGroups[mRowGroupIndex];
    int32_t rowCount = mRowGroup->GetRowCount();
    mRowGroupStart = mRowGroup->GetStartRowIndex();
    mRowGroupEnd   = mRowGroupStart + rowCount - 1;
    if (rowCount > 0) {
      mCellMap = mTableCellMap->GetMapFor(mRowGroup, mCellMap);
      if (!mCellMap) ABORT1(false);
      nsTableRowFrame* firstRow = mRowGroup->GetFirstRow();
      if (aFindFirstDamagedRow) {
        if ((mAreaStart.y >= mRowGroupStart) && (mAreaStart.y <= mRowGroupEnd)) {
          // the damage area starts in the row group
          if (aFindFirstDamagedRow) {
            // find the correct first damaged row
            int32_t numRows = mAreaStart.y - mRowGroupStart;
            for (int32_t i = 0; i < numRows; i++) {
              firstRow = firstRow->GetNextRow();
              if (!firstRow) ABORT1(false);
            }
          }
        }
        else {
          continue;
        }
      }
      if (SetNewRow(firstRow)) { // sets mAtEnd
        break;
      }
    }
  }

  return !mAtEnd;
}

void
BCMapCellIterator::First(BCMapCellInfo& aMapInfo)
{
  aMapInfo.ResetCellInfo();

  SetNewRowGroup(true); // sets mAtEnd
  while (!mAtEnd) {
    if ((mAreaStart.y >= mRowGroupStart) && (mAreaStart.y <= mRowGroupEnd)) {
      BCCellData* cellData =
        static_cast<BCCellData*>(mCellMap->GetDataAt(mAreaStart.y -
                                                      mRowGroupStart,
                                                      mAreaStart.x));
      if (cellData && (cellData->IsOrig() || cellData->IsDead())) {
        aMapInfo.SetInfo(mRow, mAreaStart.x, cellData, this);
        return;
      }
      else {
        NS_ASSERTION(((0 == mAreaStart.x) && (mRowGroupStart == mAreaStart.y)) ,
                     "damage area expanded incorrectly");
      }
    }
    SetNewRowGroup(true); // sets mAtEnd
  }
}

void
BCMapCellIterator::Next(BCMapCellInfo& aMapInfo)
{
  if (mAtEnd) ABORT0();
  aMapInfo.ResetCellInfo();

  mIsNewRow = false;
  mColIndex++;
  while ((mRowIndex <= mAreaEnd.y) && !mAtEnd) {
    for (; mColIndex <= mAreaEnd.x; mColIndex++) {
      int32_t rgRowIndex = mRowIndex - mRowGroupStart;
      BCCellData* cellData =
         static_cast<BCCellData*>(mCellMap->GetDataAt(rgRowIndex, mColIndex));
      if (!cellData) { // add a dead cell data
        nsIntRect damageArea;
        cellData =
          static_cast<BCCellData*>(mCellMap->AppendCell(*mTableCellMap, nullptr,
                                                         rgRowIndex, false, 0,
                                                         damageArea));
        if (!cellData) ABORT0();
      }
      if (cellData && (cellData->IsOrig() || cellData->IsDead())) {
        aMapInfo.SetInfo(mRow, mColIndex, cellData, this);
        return;
      }
    }
    if (mRowIndex >= mRowGroupEnd) {
      SetNewRowGroup(false); // could set mAtEnd
    }
    else {
      SetNewRow(); // could set mAtEnd
    }
  }
  mAtEnd = true;
}

void
BCMapCellIterator::PeekRight(BCMapCellInfo&   aRefInfo,
                             uint32_t         aRowIndex,
                             BCMapCellInfo&   aAjaInfo)
{
  aAjaInfo.ResetCellInfo();
  int32_t colIndex = aRefInfo.mColIndex + aRefInfo.mColSpan;
  uint32_t rgRowIndex = aRowIndex - mRowGroupStart;

  BCCellData* cellData =
    static_cast<BCCellData*>(mCellMap->GetDataAt(rgRowIndex, colIndex));
  if (!cellData) { // add a dead cell data
    NS_ASSERTION(colIndex < mTableCellMap->GetColCount(), "program error");
    nsIntRect damageArea;
    cellData =
      static_cast<BCCellData*>(mCellMap->AppendCell(*mTableCellMap, nullptr,
                                                     rgRowIndex, false, 0,
                                                     damageArea));
    if (!cellData) ABORT0();
  }
  nsTableRowFrame* row = nullptr;
  if (cellData->IsRowSpan()) {
    rgRowIndex -= cellData->GetRowSpanOffset();
    cellData =
      static_cast<BCCellData*>(mCellMap->GetDataAt(rgRowIndex, colIndex));
    if (!cellData)
      ABORT0();
  }
  else {
    row = mRow;
  }
  aAjaInfo.SetInfo(row, colIndex, cellData, this);
}

void
BCMapCellIterator::PeekBottom(BCMapCellInfo&   aRefInfo,
                              uint32_t         aColIndex,
                              BCMapCellInfo&   aAjaInfo)
{
  aAjaInfo.ResetCellInfo();
  int32_t rowIndex = aRefInfo.mRowIndex + aRefInfo.mRowSpan;
  int32_t rgRowIndex = rowIndex - mRowGroupStart;
  nsTableRowGroupFrame* rg = mRowGroup;
  nsCellMap* cellMap = mCellMap;
  nsTableRowFrame* nextRow = nullptr;
  if (rowIndex > mRowGroupEnd) {
    int32_t nextRgIndex = mRowGroupIndex;
    do {
      nextRgIndex++;
      rg = mRowGroups.SafeElementAt(nextRgIndex);
      if (rg) {
        cellMap = mTableCellMap->GetMapFor(rg, cellMap); if (!cellMap) ABORT0();
        rgRowIndex = 0;
        nextRow = rg->GetFirstRow();
      }
    }
    while (rg && !nextRow);
    if(!rg) return;
  }
  else {
    // get the row within the same row group
    nextRow = mRow;
    for (int32_t i = 0; i < aRefInfo.mRowSpan; i++) {
      nextRow = nextRow->GetNextRow(); if (!nextRow) ABORT0();
    }
  }

  BCCellData* cellData =
    static_cast<BCCellData*>(cellMap->GetDataAt(rgRowIndex, aColIndex));
  if (!cellData) { // add a dead cell data
    NS_ASSERTION(rgRowIndex < cellMap->GetRowCount(), "program error");
    nsIntRect damageArea;
    cellData =
      static_cast<BCCellData*>(cellMap->AppendCell(*mTableCellMap, nullptr,
                                                    rgRowIndex, false, 0,
                                                    damageArea));
    if (!cellData) ABORT0();
  }
  if (cellData->IsColSpan()) {
    aColIndex -= cellData->GetColSpanOffset();
    cellData =
      static_cast<BCCellData*>(cellMap->GetDataAt(rgRowIndex, aColIndex));
  }
  aAjaInfo.SetInfo(nextRow, aColIndex, cellData, this, cellMap);
}

// Assign priorities to border styles. For example, styleToPriority(NS_STYLE_BORDER_STYLE_SOLID)
// will return the priority of NS_STYLE_BORDER_STYLE_SOLID.
static uint8_t styleToPriority[13] = { 0,  // NS_STYLE_BORDER_STYLE_NONE
                                       2,  // NS_STYLE_BORDER_STYLE_GROOVE
                                       4,  // NS_STYLE_BORDER_STYLE_RIDGE
                                       5,  // NS_STYLE_BORDER_STYLE_DOTTED
                                       6,  // NS_STYLE_BORDER_STYLE_DASHED
                                       7,  // NS_STYLE_BORDER_STYLE_SOLID
                                       8,  // NS_STYLE_BORDER_STYLE_DOUBLE
                                       1,  // NS_STYLE_BORDER_STYLE_INSET
                                       3,  // NS_STYLE_BORDER_STYLE_OUTSET
                                       9 };// NS_STYLE_BORDER_STYLE_HIDDEN
// priority rules follow CSS 2.1 spec
// 'hidden', 'double', 'solid', 'dashed', 'dotted', 'ridge', 'outset', 'groove',
// and the lowest: 'inset'. none is even weaker
#define CELL_CORNER true

/** return the border style, border color for a given frame and side
  * @param aFrame           - query the info for this frame
  * @param aSide            - the side of the frame
  * @param aStyle           - the border style
  * @param aColor           - the border color
  * @param aTableIsLTR      - table direction is LTR
  */
static void
GetColorAndStyle(const nsIFrame*  aFrame,
                 mozilla::css::Side aSide,
                 uint8_t&         aStyle,
                 nscolor&         aColor,
                 bool             aTableIsLTR)
{
  NS_PRECONDITION(aFrame, "null frame");
  // initialize out arg
  aColor = 0;
  const nsStyleBorder* styleData = aFrame->StyleBorder();
  if(!aTableIsLTR) { // revert the directions
    if (NS_SIDE_RIGHT == aSide) {
      aSide = NS_SIDE_LEFT;
    }
    else if (NS_SIDE_LEFT == aSide) {
      aSide = NS_SIDE_RIGHT;
    }
  }
  aStyle = styleData->GetBorderStyle(aSide);

  if ((NS_STYLE_BORDER_STYLE_NONE == aStyle) ||
      (NS_STYLE_BORDER_STYLE_HIDDEN == aStyle)) {
    return;
  }
  aColor = aFrame->StyleContext()->GetVisitedDependentColor(
             nsCSSProps::SubpropertyEntryFor(eCSSProperty_border_color)[aSide]);
}

/** coerce the paint style as required by CSS2.1
  * @param aFrame           - query the info for this frame
  * @param aSide            - the side of the frame
  * @param aStyle           - the border style
  * @param aColor           - the border color
  * @param aTableIsLTR      - table direction is LTR
  */
static void
GetPaintStyleInfo(const nsIFrame*  aFrame,
                  mozilla::css::Side aSide,
                  uint8_t&         aStyle,
                  nscolor&         aColor,
                  bool             aTableIsLTR)
{
  GetColorAndStyle(aFrame, aSide, aStyle, aColor, aTableIsLTR);
  if (NS_STYLE_BORDER_STYLE_INSET    == aStyle) {
    aStyle = NS_STYLE_BORDER_STYLE_RIDGE;
  }
  else if (NS_STYLE_BORDER_STYLE_OUTSET    == aStyle) {
    aStyle = NS_STYLE_BORDER_STYLE_GROOVE;
  }
}

/** return the border style, border color and the width in pixel for a given
  * frame and side
  * @param aFrame           - query the info for this frame
  * @param aSide            - the side of the frame
  * @param aStyle           - the border style
  * @param aColor           - the border color
  * @param aTableIsLTR      - table direction is LTR
  * @param aWidth           - the border width in px.
  * @param aTwipsToPixels   - conversion factor from twips to pixel
  */
static void
GetColorAndStyle(const nsIFrame*  aFrame,
                 mozilla::css::Side aSide,
                 uint8_t&         aStyle,
                 nscolor&         aColor,
                 bool             aTableIsLTR,
                 BCPixelSize&     aWidth)
{
  GetColorAndStyle(aFrame, aSide, aStyle, aColor, aTableIsLTR);
  if ((NS_STYLE_BORDER_STYLE_NONE == aStyle) ||
      (NS_STYLE_BORDER_STYLE_HIDDEN == aStyle)) {
    aWidth = 0;
    return;
  }
  const nsStyleBorder* styleData = aFrame->StyleBorder();
  nscoord width;
  if(!aTableIsLTR) { // revert the directions
    if (NS_SIDE_RIGHT == aSide) {
      aSide = NS_SIDE_LEFT;
    }
    else if (NS_SIDE_LEFT == aSide) {
      aSide = NS_SIDE_RIGHT;
    }
  }
  width = styleData->GetComputedBorderWidth(aSide);
  aWidth = nsPresContext::AppUnitsToIntCSSPixels(width);
}

class nsDelayedCalcBCBorders : public nsRunnable {
public:
  explicit nsDelayedCalcBCBorders(nsIFrame* aFrame) :
    mFrame(aFrame) {}

  NS_IMETHOD Run() override {
    if (mFrame) {
      nsTableFrame* tableFrame = static_cast <nsTableFrame*>(mFrame.GetFrame());
      if (tableFrame->NeedToCalcBCBorders()) {
        tableFrame->CalcBCBorders();
      }
    }
    return NS_OK;
  }
private:
  nsWeakFrame mFrame;
};

bool
nsTableFrame::BCRecalcNeeded(nsStyleContext* aOldStyleContext,
                             nsStyleContext* aNewStyleContext)
{
  // Attention: the old style context is the one we're forgetting,
  // and hence possibly completely bogus for GetStyle* purposes.
  // We use PeekStyleData instead.

  const nsStyleBorder* oldStyleData = aOldStyleContext->PeekStyleBorder();
  if (!oldStyleData)
    return false;

  const nsStyleBorder* newStyleData = aNewStyleContext->StyleBorder();
  nsChangeHint change = newStyleData->CalcDifference(*oldStyleData);
  if (!change)
    return false;
  if (change & nsChangeHint_NeedReflow)
    return true; // the caller only needs to mark the bc damage area
  if (change & nsChangeHint_RepaintFrame) {
    // we need to recompute the borders and the caller needs to mark
    // the bc damage area
    // XXX In principle this should only be necessary for border style changes
    // However the bc painting code tries to maximize the drawn border segments
    // so it stores in the cellmap where a new border segment starts and this
    // introduces a unwanted cellmap data dependence on color
    nsCOMPtr<nsIRunnable> evt = new nsDelayedCalcBCBorders(this);
    NS_DispatchToCurrentThread(evt);
    return true;
  }
  return false;
}


// Compare two border segments, this comparison depends whether the two
// segments meet at a corner and whether the second segment is horizontal.
// The return value is whichever of aBorder1 or aBorder2 dominates.
static const BCCellBorder&
CompareBorders(bool                aIsCorner, // Pass true for corner calculations
               const BCCellBorder& aBorder1,
               const BCCellBorder& aBorder2,
               bool                aSecondIsHorizontal,
               bool*             aFirstDominates = nullptr)
{
  bool firstDominates = true;

  if (NS_STYLE_BORDER_STYLE_HIDDEN == aBorder1.style) {
    firstDominates = (aIsCorner) ? false : true;
  }
  else if (NS_STYLE_BORDER_STYLE_HIDDEN == aBorder2.style) {
    firstDominates = (aIsCorner) ? true : false;
  }
  else if (aBorder1.width < aBorder2.width) {
    firstDominates = false;
  }
  else if (aBorder1.width == aBorder2.width) {
    if (styleToPriority[aBorder1.style] < styleToPriority[aBorder2.style]) {
      firstDominates = false;
    }
    else if (styleToPriority[aBorder1.style] == styleToPriority[aBorder2.style]) {
      if (aBorder1.owner == aBorder2.owner) {
        firstDominates = !aSecondIsHorizontal;
      }
      else if (aBorder1.owner < aBorder2.owner) {
        firstDominates = false;
      }
    }
  }

  if (aFirstDominates)
    *aFirstDominates = firstDominates;

  if (firstDominates)
    return aBorder1;
  return aBorder2;
}

/** calc the dominant border by considering the table, row/col group, row/col,
  * cell.
  * Depending on whether the side is vertical or horizontal and whether
  * adjacent frames are taken into account the ownership of a single border
  * segment is defined. The return value is the dominating border
  * The cellmap stores only top and left borders for each cellmap position.
  * If the cell border is owned by the cell that is left of the border
  * it will be an adjacent owner aka eAjaCellOwner. See celldata.h for the other
  * scenarios with a adjacent owner.
  * @param xxxFrame         - the frame for style information, might be zero if
  *                           it should not be considered
  * @param aSide            - side of the frames that should be considered
  * @param aAja             - the border comparison takes place from the point of
  *                           a frame that is adjacent to the cellmap entry, for
  *                           when a cell owns its lower border it will be the
  *                           adjacent owner as in the cellmap only top and left
  *                           borders are stored.
  * @param aTwipsToPixels   - conversion factor as borders need to be drawn pixel
  *                           aligned.
  */
static BCCellBorder
CompareBorders(const nsIFrame*  aTableFrame,
               const nsIFrame*  aColGroupFrame,
               const nsIFrame*  aColFrame,
               const nsIFrame*  aRowGroupFrame,
               const nsIFrame*  aRowFrame,
               const nsIFrame*  aCellFrame,
               bool             aTableIsLTR,
               mozilla::css::Side aSide,
               bool             aAja)
{
  BCCellBorder border, tempBorder;
  bool horizontal = (NS_SIDE_TOP == aSide) || (NS_SIDE_BOTTOM == aSide);

  // start with the table as dominant if present
  if (aTableFrame) {
    GetColorAndStyle(aTableFrame, aSide, border.style, border.color, aTableIsLTR, border.width);
    border.owner = eTableOwner;
    if (NS_STYLE_BORDER_STYLE_HIDDEN == border.style) {
      return border;
    }
  }
  // see if the colgroup is dominant
  if (aColGroupFrame) {
    GetColorAndStyle(aColGroupFrame, aSide, tempBorder.style, tempBorder.color, aTableIsLTR, tempBorder.width);
    tempBorder.owner = (aAja && !horizontal) ? eAjaColGroupOwner : eColGroupOwner;
    // pass here and below false for aSecondIsHorizontal as it is only used for corner calculations.
    border = CompareBorders(!CELL_CORNER, border, tempBorder, false);
    if (NS_STYLE_BORDER_STYLE_HIDDEN == border.style) {
      return border;
    }
  }
  // see if the col is dominant
  if (aColFrame) {
    GetColorAndStyle(aColFrame, aSide, tempBorder.style, tempBorder.color, aTableIsLTR, tempBorder.width);
    tempBorder.owner = (aAja && !horizontal) ? eAjaColOwner : eColOwner;
    border = CompareBorders(!CELL_CORNER, border, tempBorder, false);
    if (NS_STYLE_BORDER_STYLE_HIDDEN == border.style) {
      return border;
    }
  }
  // see if the rowgroup is dominant
  if (aRowGroupFrame) {
    GetColorAndStyle(aRowGroupFrame, aSide, tempBorder.style, tempBorder.color, aTableIsLTR, tempBorder.width);
    tempBorder.owner = (aAja && horizontal) ? eAjaRowGroupOwner : eRowGroupOwner;
    border = CompareBorders(!CELL_CORNER, border, tempBorder, false);
    if (NS_STYLE_BORDER_STYLE_HIDDEN == border.style) {
      return border;
    }
  }
  // see if the row is dominant
  if (aRowFrame) {
    GetColorAndStyle(aRowFrame, aSide, tempBorder.style, tempBorder.color, aTableIsLTR, tempBorder.width);
    tempBorder.owner = (aAja && horizontal) ? eAjaRowOwner : eRowOwner;
    border = CompareBorders(!CELL_CORNER, border, tempBorder, false);
    if (NS_STYLE_BORDER_STYLE_HIDDEN == border.style) {
      return border;
    }
  }
  // see if the cell is dominant
  if (aCellFrame) {
    GetColorAndStyle(aCellFrame, aSide, tempBorder.style, tempBorder.color, aTableIsLTR, tempBorder.width);
    tempBorder.owner = (aAja) ? eAjaCellOwner : eCellOwner;
    border = CompareBorders(!CELL_CORNER, border, tempBorder, false);
  }
  return border;
}

static bool
Perpendicular(mozilla::css::Side aSide1,
              mozilla::css::Side aSide2)
{
  switch (aSide1) {
  case NS_SIDE_TOP:
    return (NS_SIDE_BOTTOM != aSide2);
  case NS_SIDE_RIGHT:
    return (NS_SIDE_LEFT != aSide2);
  case NS_SIDE_BOTTOM:
    return (NS_SIDE_TOP != aSide2);
  default: // NS_SIDE_LEFT
    return (NS_SIDE_RIGHT != aSide2);
  }
}

// XXX allocate this as number-of-cols+1 instead of number-of-cols+1 * number-of-rows+1
struct BCCornerInfo
{
  BCCornerInfo() { ownerColor = 0; ownerWidth = subWidth = ownerElem = subSide =
                   subElem = hasDashDot = numSegs = bevel = 0; ownerSide = NS_SIDE_TOP;
                   ownerStyle = 0xFF; subStyle = NS_STYLE_BORDER_STYLE_SOLID;  }
  void Set(mozilla::css::Side aSide,
           BCCellBorder  border);

  void Update(mozilla::css::Side aSide,
              BCCellBorder  border);

  nscolor   ownerColor;     // color of borderOwner
  uint16_t  ownerWidth;     // pixel width of borderOwner
  uint16_t  subWidth;       // pixel width of the largest border intersecting the border perpendicular
                            // to ownerSide
  uint32_t  ownerSide:2;    // mozilla::css::Side (e.g NS_SIDE_TOP, NS_SIDE_RIGHT, etc) of the border
                            // owning the corner relative to the corner
  uint32_t  ownerElem:3;    // elem type (e.g. eTable, eGroup, etc) owning the corner
  uint32_t  ownerStyle:8;   // border style of ownerElem
  uint32_t  subSide:2;      // side of border with subWidth relative to the corner
  uint32_t  subElem:3;      // elem type (e.g. eTable, eGroup, etc) of sub owner
  uint32_t  subStyle:8;     // border style of subElem
  uint32_t  hasDashDot:1;   // does a dashed, dotted segment enter the corner, they cannot be beveled
  uint32_t  numSegs:3;      // number of segments entering corner
  uint32_t  bevel:1;        // is the corner beveled (uses the above two fields together with subWidth)
  uint32_t  unused:1;
};

void
BCCornerInfo::Set(mozilla::css::Side aSide,
                  BCCellBorder  aBorder)
{
  ownerElem  = aBorder.owner;
  ownerStyle = aBorder.style;
  ownerWidth = aBorder.width;
  ownerColor = aBorder.color;
  ownerSide  = aSide;
  hasDashDot = 0;
  numSegs    = 0;
  if (aBorder.width > 0) {
    numSegs++;
    hasDashDot = (NS_STYLE_BORDER_STYLE_DASHED == aBorder.style) ||
                 (NS_STYLE_BORDER_STYLE_DOTTED == aBorder.style);
  }
  bevel      = 0;
  subWidth   = 0;
  // the following will get set later
  subSide    = ((aSide == NS_SIDE_LEFT) || (aSide == NS_SIDE_RIGHT)) ? NS_SIDE_TOP : NS_SIDE_LEFT;
  subElem    = eTableOwner;
  subStyle   = NS_STYLE_BORDER_STYLE_SOLID;
}

void
BCCornerInfo::Update(mozilla::css::Side aSide,
                     BCCellBorder  aBorder)
{
  bool existingWins = false;
  if (0xFF == ownerStyle) { // initial value indiating that it hasn't been set yet
    Set(aSide, aBorder);
  }
  else {
    bool horizontal = (NS_SIDE_LEFT == aSide) || (NS_SIDE_RIGHT == aSide); // relative to the corner
    BCCellBorder oldBorder, tempBorder;
    oldBorder.owner  = (BCBorderOwner) ownerElem;
    oldBorder.style =  ownerStyle;
    oldBorder.width =  ownerWidth;
    oldBorder.color =  ownerColor;

    mozilla::css::Side oldSide  = mozilla::css::Side(ownerSide);

    tempBorder = CompareBorders(CELL_CORNER, oldBorder, aBorder, horizontal, &existingWins);

    ownerElem  = tempBorder.owner;
    ownerStyle = tempBorder.style;
    ownerWidth = tempBorder.width;
    ownerColor = tempBorder.color;
    if (existingWins) { // existing corner is dominant
      if (::Perpendicular(mozilla::css::Side(ownerSide), aSide)) {
        // see if the new sub info replaces the old
        BCCellBorder subBorder;
        subBorder.owner = (BCBorderOwner) subElem;
        subBorder.style =  subStyle;
        subBorder.width =  subWidth;
        subBorder.color = 0; // we are not interested in subBorder color
        bool firstWins;

        tempBorder = CompareBorders(CELL_CORNER, subBorder, aBorder, horizontal, &firstWins);

        subElem  = tempBorder.owner;
        subStyle = tempBorder.style;
        subWidth = tempBorder.width;
        if (!firstWins) {
          subSide = aSide;
        }
      }
    }
    else { // input args are dominant
      ownerSide = aSide;
      if (::Perpendicular(oldSide, mozilla::css::Side(ownerSide))) {
        subElem  = oldBorder.owner;
        subStyle = oldBorder.style;
        subWidth = oldBorder.width;
        subSide  = oldSide;
      }
    }
    if (aBorder.width > 0) {
      numSegs++;
      if (!hasDashDot && ((NS_STYLE_BORDER_STYLE_DASHED == aBorder.style) ||
                          (NS_STYLE_BORDER_STYLE_DOTTED == aBorder.style))) {
        hasDashDot = 1;
      }
    }

    // bevel the corner if only two perpendicular non dashed/dotted segments enter the corner
    bevel = (2 == numSegs) && (subWidth > 1) && (0 == hasDashDot);
  }
}

struct BCCorners
{
  BCCorners(int32_t aNumCorners,
            int32_t aStartIndex);

  ~BCCorners() { delete [] corners; }

  BCCornerInfo& operator [](int32_t i) const
  { NS_ASSERTION((i >= startIndex) && (i <= endIndex), "program error");
    return corners[clamped(i, startIndex, endIndex) - startIndex]; }

  int32_t       startIndex;
  int32_t       endIndex;
  BCCornerInfo* corners;
};

BCCorners::BCCorners(int32_t aNumCorners,
                     int32_t aStartIndex)
{
  NS_ASSERTION((aNumCorners > 0) && (aStartIndex >= 0), "program error");
  startIndex = aStartIndex;
  endIndex   = aStartIndex + aNumCorners - 1;
  corners    = new BCCornerInfo[aNumCorners];
}


struct BCCellBorders
{
  BCCellBorders(int32_t aNumBorders,
                int32_t aStartIndex);

  ~BCCellBorders() { delete [] borders; }

  BCCellBorder& operator [](int32_t i) const
  { NS_ASSERTION((i >= startIndex) && (i <= endIndex), "program error");
    return borders[clamped(i, startIndex, endIndex) - startIndex]; }

  int32_t       startIndex;
  int32_t       endIndex;
  BCCellBorder* borders;
};

BCCellBorders::BCCellBorders(int32_t aNumBorders,
                             int32_t aStartIndex)
{
  NS_ASSERTION((aNumBorders > 0) && (aStartIndex >= 0), "program error");
  startIndex = aStartIndex;
  endIndex   = aStartIndex + aNumBorders - 1;
  borders    = new BCCellBorder[aNumBorders];
}

// this function sets the new border properties and returns true if the border
// segment will start a new segment and not be accumulated into the previous
// segment.
static bool
SetBorder(const BCCellBorder&   aNewBorder,
          BCCellBorder&         aBorder)
{
  bool changed = (aNewBorder.style != aBorder.style) ||
                   (aNewBorder.width != aBorder.width) ||
                   (aNewBorder.color != aBorder.color);
  aBorder.color        = aNewBorder.color;
  aBorder.width        = aNewBorder.width;
  aBorder.style        = aNewBorder.style;
  aBorder.owner        = aNewBorder.owner;

  return changed;
}

// this function will set the horizontal border. It will return true if the
// existing segment will not be continued. Having a vertical owner of a corner
// should also start a new segment.
static bool
SetHorBorder(const BCCellBorder& aNewBorder,
             const BCCornerInfo& aCorner,
             BCCellBorder&       aBorder)
{
  bool startSeg = ::SetBorder(aNewBorder, aBorder);
  if (!startSeg) {
    startSeg = ((NS_SIDE_LEFT != aCorner.ownerSide) && (NS_SIDE_RIGHT != aCorner.ownerSide));
  }
  return startSeg;
}

// Make the damage area larger on the top and bottom by at least one row and on the left and right
// at least one column. This is done so that adjacent elements are part of the border calculations.
// The extra segments and borders outside the actual damage area will not be updated in the cell map,
// because they in turn would need info from adjacent segments outside the damage area to be accurate.
void
nsTableFrame::ExpandBCDamageArea(nsIntRect& aRect) const
{
  int32_t numRows = GetRowCount();
  int32_t numCols = GetColCount();

  int32_t dStartX = aRect.x;
  int32_t dEndX   = aRect.XMost() - 1;
  int32_t dStartY = aRect.y;
  int32_t dEndY   = aRect.YMost() - 1;

  // expand the damage area in each direction
  if (dStartX > 0) {
    dStartX--;
  }
  if (dEndX < (numCols - 1)) {
    dEndX++;
  }
  if (dStartY > 0) {
    dStartY--;
  }
  if (dEndY < (numRows - 1)) {
    dEndY++;
  }
  // Check the damage area so that there are no cells spanning in or out. If there are any then
  // make the damage area as big as the table, similarly to the way the cell map decides whether
  // to rebuild versus expand. This could be optimized to expand to the smallest area that contains
  // no spanners, but it may not be worth the effort in general, and it would need to be done in the
  // cell map as well.
  bool haveSpanner = false;
  if ((dStartX > 0) || (dEndX < (numCols - 1)) || (dStartY > 0) || (dEndY < (numRows - 1))) {
    nsTableCellMap* tableCellMap = GetCellMap(); if (!tableCellMap) ABORT0();
    // Get the ordered row groups
    RowGroupArray rowGroups;
    OrderRowGroups(rowGroups);

    // Scope outside loop to be used as hint.
    nsCellMap* cellMap = nullptr;
    for (uint32_t rgX = 0; rgX < rowGroups.Length(); rgX++) {
      nsTableRowGroupFrame* rgFrame = rowGroups[rgX];
      int32_t rgStartY = rgFrame->GetStartRowIndex();
      int32_t rgEndY   = rgStartY + rgFrame->GetRowCount() - 1;
      if (dEndY < rgStartY)
        break;
      cellMap = tableCellMap->GetMapFor(rgFrame, cellMap);
      if (!cellMap) ABORT0();
      // check for spanners from above and below
      if ((dStartY > 0) && (dStartY >= rgStartY) && (dStartY <= rgEndY)) {
        if (uint32_t(dStartY - rgStartY) >= cellMap->mRows.Length())
          ABORT0();
        const nsCellMap::CellDataArray& row =
          cellMap->mRows[dStartY - rgStartY];
        for (int32_t x = dStartX; x <= dEndX; x++) {
          CellData* cellData = row.SafeElementAt(x);
          if (cellData && (cellData->IsRowSpan())) {
             haveSpanner = true;
             break;
          }
        }
        if (dEndY < rgEndY) {
          if (uint32_t(dEndY + 1 - rgStartY) >= cellMap->mRows.Length())
            ABORT0();
          const nsCellMap::CellDataArray& row2 =
            cellMap->mRows[dEndY + 1 - rgStartY];
          for (int32_t x = dStartX; x <= dEndX; x++) {
            CellData* cellData = row2.SafeElementAt(x);
            if (cellData && (cellData->IsRowSpan())) {
              haveSpanner = true;
              break;
            }
          }
        }
      }
      // check for spanners on the left and right
      int32_t iterStartY = -1;
      int32_t iterEndY   = -1;
      if ((dStartY >= rgStartY) && (dStartY <= rgEndY)) {
        // the damage area starts in the row group
        iterStartY = dStartY;
        iterEndY   = std::min(dEndY, rgEndY);
      }
      else if ((dEndY >= rgStartY) && (dEndY <= rgEndY)) {
        // the damage area ends in the row group
        iterStartY = rgStartY;
        iterEndY   = dEndY;
      }
      else if ((rgStartY >= dStartY) && (rgEndY <= dEndY)) {
        // the damage area contains the row group
        iterStartY = rgStartY;
        iterEndY   = rgEndY;
      }
      if ((iterStartY >= 0) && (iterEndY >= 0)) {
        for (int32_t y = iterStartY; y <= iterEndY; y++) {
          if (uint32_t(y - rgStartY) >= cellMap->mRows.Length())
            ABORT0();
          const nsCellMap::CellDataArray& row =
            cellMap->mRows[y - rgStartY];
          CellData* cellData = row.SafeElementAt(dStartX);
          if (cellData && (cellData->IsColSpan())) {
            haveSpanner = true;
            break;
          }
          if (dEndX < (numCols - 1)) {
            cellData = row.SafeElementAt(dEndX + 1);
            if (cellData && (cellData->IsColSpan())) {
              haveSpanner = true;
              break;
            }
          }
        }
      }
    }
  }
  if (haveSpanner) {
    // make the damage area the whole table
    aRect.x      = 0;
    aRect.y      = 0;
    aRect.width  = numCols;
    aRect.height = numRows;
  }
  else {
    aRect.x      = dStartX;
    aRect.y      = dStartY;
    aRect.width  = 1 + dEndX - dStartX;
    aRect.height = 1 + dEndY - dStartY;
  }
}


#define ADJACENT    true
#define HORIZONTAL  true

void
BCMapCellInfo::SetTableTopLeftContBCBorder()
{
  BCCellBorder currentBorder;
  //calculate continuous top first row & rowgroup border: special case
  //because it must include the table in the collapse
  if (mTopRow) {
    currentBorder = CompareBorders(mTableFrame, nullptr, nullptr, mRowGroup,
                                   mTopRow, nullptr, mTableIsLTR,
                                   NS_SIDE_TOP, !ADJACENT);
    mTopRow->SetContinuousBCBorderWidth(NS_SIDE_TOP, currentBorder.width);
  }
  if (mCgAtRight && mColGroup) {
    //calculate continuous top colgroup border once per colgroup
    currentBorder = CompareBorders(mTableFrame, mColGroup, nullptr, mRowGroup,
                                   mTopRow, nullptr, mTableIsLTR,
                                   NS_SIDE_TOP, !ADJACENT);
    mColGroup->SetContinuousBCBorderWidth(NS_SIDE_TOP, currentBorder.width);
  }
  if (0 == mColIndex) {
    currentBorder = CompareBorders(mTableFrame, mColGroup, mLeftCol, nullptr,
                                   nullptr, nullptr, mTableIsLTR, NS_SIDE_LEFT,
                                   !ADJACENT);
    mTableFrame->SetContinuousLeftBCBorderWidth(currentBorder.width);
  }
}

void
BCMapCellInfo::SetRowGroupLeftContBCBorder()
{
  BCCellBorder currentBorder;
  //get row group continuous borders
  if (mRgAtBottom && mRowGroup) { //once per row group, so check for bottom
     currentBorder = CompareBorders(mTableFrame, mColGroup, mLeftCol, mRowGroup,
                                    nullptr, nullptr, mTableIsLTR, NS_SIDE_LEFT,
                                    !ADJACENT);
     mRowGroup->SetContinuousBCBorderWidth(mStartSide, currentBorder.width);
  }
}

void
BCMapCellInfo::SetRowGroupRightContBCBorder()
{
  BCCellBorder currentBorder;
  //get row group continuous borders
  if (mRgAtBottom && mRowGroup) { //once per mRowGroup, so check for bottom
    currentBorder = CompareBorders(mTableFrame, mColGroup, mRightCol, mRowGroup,
                                   nullptr, nullptr, mTableIsLTR, NS_SIDE_RIGHT,
                                   ADJACENT);
    mRowGroup->SetContinuousBCBorderWidth(mEndSide, currentBorder.width);
  }
}

void
BCMapCellInfo::SetColumnTopRightContBCBorder()
{
  BCCellBorder currentBorder;
  //calculate column continuous borders
  //we only need to do this once, so we'll do it only on the first row
  currentBorder = CompareBorders(mTableFrame, mCurrentColGroupFrame,
                                 mCurrentColFrame, mRowGroup, mTopRow, nullptr,
                                 mTableIsLTR, NS_SIDE_TOP, !ADJACENT);
  ((nsTableColFrame*) mCurrentColFrame)->SetContinuousBCBorderWidth(NS_SIDE_TOP,
                                                           currentBorder.width);
  if (mNumTableCols == GetCellEndColIndex() + 1) {
    currentBorder = CompareBorders(mTableFrame, mCurrentColGroupFrame,
                                   mCurrentColFrame, nullptr, nullptr, nullptr,
                                   mTableIsLTR, NS_SIDE_RIGHT, !ADJACENT);
  }
  else {
    currentBorder = CompareBorders(nullptr, mCurrentColGroupFrame,
                                   mCurrentColFrame, nullptr,nullptr, nullptr,
                                   mTableIsLTR, NS_SIDE_RIGHT, !ADJACENT);
  }
  mCurrentColFrame->SetContinuousBCBorderWidth(NS_SIDE_RIGHT,
                                               currentBorder.width);
}

void
BCMapCellInfo::SetColumnBottomContBCBorder()
{
  BCCellBorder currentBorder;
  //get col continuous border
  currentBorder = CompareBorders(mTableFrame, mCurrentColGroupFrame,
                                 mCurrentColFrame, mRowGroup, mBottomRow,
                                 nullptr, mTableIsLTR, NS_SIDE_BOTTOM, ADJACENT);
  mCurrentColFrame->SetContinuousBCBorderWidth(NS_SIDE_BOTTOM,
                                               currentBorder.width);
}

void
BCMapCellInfo::SetColGroupBottomContBCBorder()
{
  BCCellBorder currentBorder;
  if (mColGroup) {
    currentBorder = CompareBorders(mTableFrame, mColGroup, nullptr, mRowGroup,
                                   mBottomRow, nullptr, mTableIsLTR,
                                   NS_SIDE_BOTTOM, ADJACENT);
    mColGroup->SetContinuousBCBorderWidth(NS_SIDE_BOTTOM, currentBorder.width);
  }
}

void
BCMapCellInfo::SetRowGroupBottomContBCBorder()
{
  BCCellBorder currentBorder;
  if (mRowGroup) {
    currentBorder = CompareBorders(mTableFrame, nullptr, nullptr, mRowGroup,
                                   mBottomRow, nullptr, mTableIsLTR,
                                   NS_SIDE_BOTTOM, ADJACENT);
    mRowGroup->SetContinuousBCBorderWidth(NS_SIDE_BOTTOM, currentBorder.width);
  }
}

void
BCMapCellInfo::SetInnerRowGroupBottomContBCBorder(const nsIFrame* aNextRowGroup,
                                                  nsTableRowFrame* aNextRow)
{
  BCCellBorder currentBorder, adjacentBorder;

  const nsIFrame* rowgroup = (mRgAtBottom) ? mRowGroup : nullptr;
  currentBorder = CompareBorders(nullptr, nullptr, nullptr, rowgroup, mBottomRow,
                                 nullptr, mTableIsLTR, NS_SIDE_BOTTOM, ADJACENT);

  adjacentBorder = CompareBorders(nullptr, nullptr, nullptr, aNextRowGroup,
                                  aNextRow, nullptr, mTableIsLTR, NS_SIDE_TOP,
                                  !ADJACENT);
  currentBorder = CompareBorders(false, currentBorder, adjacentBorder,
                                 HORIZONTAL);
  if (aNextRow) {
    aNextRow->SetContinuousBCBorderWidth(NS_SIDE_TOP, currentBorder.width);
  }
  if (mRgAtBottom && mRowGroup) {
    mRowGroup->SetContinuousBCBorderWidth(NS_SIDE_BOTTOM, currentBorder.width);
  }
}

void
BCMapCellInfo::SetRowLeftContBCBorder()
{
  //get row continuous borders
  if (mCurrentRowFrame) {
    BCCellBorder currentBorder;
    currentBorder = CompareBorders(mTableFrame, mColGroup, mLeftCol, mRowGroup,
                                   mCurrentRowFrame, nullptr, mTableIsLTR,
                                   NS_SIDE_LEFT, !ADJACENT);
    mCurrentRowFrame->SetContinuousBCBorderWidth(mStartSide,
                                                 currentBorder.width);
  }
}

void
BCMapCellInfo::SetRowRightContBCBorder()
{
  if (mCurrentRowFrame) {
    BCCellBorder currentBorder;
    currentBorder = CompareBorders(mTableFrame, mColGroup, mRightCol, mRowGroup,
                                   mCurrentRowFrame, nullptr, mTableIsLTR,
                                   NS_SIDE_RIGHT, ADJACENT);
    mCurrentRowFrame->SetContinuousBCBorderWidth(mEndSide,
                                                 currentBorder.width);
  }
}
void
BCMapCellInfo::SetTableTopBorderWidth(BCPixelSize aWidth)
{
  mTableBCData->mTopBorderWidth = std::max(mTableBCData->mTopBorderWidth, aWidth);
}

void
BCMapCellInfo::SetTableLeftBorderWidth(int32_t aRowY, BCPixelSize aWidth)
{
  // update the left/right first cell border
  if (aRowY == 0) {
    if (mTableIsLTR) {
      mTableBCData->mLeftCellBorderWidth = aWidth;
    }
    else {
      mTableBCData->mRightCellBorderWidth = aWidth;
    }
  }
  mTableBCData->mLeftBorderWidth = std::max(mTableBCData->mLeftBorderWidth,
                                          aWidth);
}

void
BCMapCellInfo::SetTableRightBorderWidth(int32_t aRowY, BCPixelSize aWidth)
{
  // update the left/right first cell border
  if (aRowY == 0) {
    if (mTableIsLTR) {
      mTableBCData->mRightCellBorderWidth = aWidth;
    }
    else {
      mTableBCData->mLeftCellBorderWidth = aWidth;
    }
  }
  mTableBCData->mRightBorderWidth = std::max(mTableBCData->mRightBorderWidth,
                                           aWidth);
}

void
BCMapCellInfo::SetRightBorderWidths(BCPixelSize aWidth)
{
   // update the borders of the cells and cols affected
  if (mCell) {
    mCell->SetBorderWidth(mEndSide, std::max(aWidth,
                          mCell->GetBorderWidth(mEndSide)));
  }
  if (mRightCol) {
    BCPixelSize half = BC_BORDER_LEFT_HALF(aWidth);
    mRightCol->SetRightBorderWidth(std::max(nscoord(half),
                                   mRightCol->GetRightBorderWidth()));
  }
}

void
BCMapCellInfo::SetBottomBorderWidths(BCPixelSize aWidth)
{
  // update the borders of the affected cells and rows
  if (mCell) {
    mCell->SetBorderWidth(NS_SIDE_BOTTOM, std::max(aWidth,
                          mCell->GetBorderWidth(NS_SIDE_BOTTOM)));
  }
  if (mBottomRow) {
    BCPixelSize half = BC_BORDER_TOP_HALF(aWidth);
    mBottomRow->SetBottomBCBorderWidth(std::max(nscoord(half),
                                       mBottomRow->GetBottomBCBorderWidth()));
  }
}
void
BCMapCellInfo::SetTopBorderWidths(BCPixelSize aWidth)
{
 if (mCell) {
     mCell->SetBorderWidth(NS_SIDE_TOP, std::max(aWidth,
                           mCell->GetBorderWidth(NS_SIDE_TOP)));
  }
  if (mTopRow) {
    BCPixelSize half = BC_BORDER_BOTTOM_HALF(aWidth);
    mTopRow->SetTopBCBorderWidth(std::max(nscoord(half),
                                        mTopRow->GetTopBCBorderWidth()));
  }
}
void
BCMapCellInfo::SetLeftBorderWidths(BCPixelSize aWidth)
{
  if (mCell) {
    mCell->SetBorderWidth(mStartSide, std::max(aWidth,
                          mCell->GetBorderWidth(mStartSide)));
  }
  if (mLeftCol) {
    BCPixelSize half = BC_BORDER_RIGHT_HALF(aWidth);
    mLeftCol->SetLeftBorderWidth(std::max(nscoord(half),
                                        mLeftCol->GetLeftBorderWidth()));
  }
}

void
BCMapCellInfo::SetTableBottomBorderWidth(BCPixelSize aWidth)
{
  mTableBCData->mBottomBorderWidth = std::max(mTableBCData->mBottomBorderWidth,
                                            aWidth);
}

void
BCMapCellInfo::SetColumn(int32_t aColX)
{
  mCurrentColFrame = mTableFrame->GetColFrame(aColX);
  if (!mCurrentColFrame) {
    NS_ERROR("null mCurrentColFrame");
  }
  mCurrentColGroupFrame = static_cast<nsTableColGroupFrame*>
                            (mCurrentColFrame->GetParent());
  if (!mCurrentColGroupFrame) {
    NS_ERROR("null mCurrentColGroupFrame");
  }
}

void
BCMapCellInfo::IncrementRow(bool aResetToTopRowOfCell)
{
  mCurrentRowFrame = (aResetToTopRowOfCell) ? mTopRow :
                                                mCurrentRowFrame->GetNextRow();
}

BCCellBorder
BCMapCellInfo::GetTopEdgeBorder()
{
  return CompareBorders(mTableFrame, mCurrentColGroupFrame, mCurrentColFrame,
                        mRowGroup, mTopRow, mCell, mTableIsLTR, NS_SIDE_TOP,
                        !ADJACENT);
}

BCCellBorder
BCMapCellInfo::GetBottomEdgeBorder()
{
  return CompareBorders(mTableFrame, mCurrentColGroupFrame, mCurrentColFrame,
                        mRowGroup, mBottomRow, mCell, mTableIsLTR,
                        NS_SIDE_BOTTOM, ADJACENT);
}
BCCellBorder
BCMapCellInfo::GetLeftEdgeBorder()
{
  return CompareBorders(mTableFrame, mColGroup, mLeftCol, mRowGroup,
                        mCurrentRowFrame, mCell, mTableIsLTR, NS_SIDE_LEFT,
                        !ADJACENT);
}
BCCellBorder
BCMapCellInfo::GetRightEdgeBorder()
{
  return CompareBorders(mTableFrame, mColGroup, mRightCol, mRowGroup,
                        mCurrentRowFrame, mCell, mTableIsLTR, NS_SIDE_RIGHT,
                        ADJACENT);
}
BCCellBorder
BCMapCellInfo::GetRightInternalBorder()
{
  const nsIFrame* cg = (mCgAtRight) ? mColGroup : nullptr;
  return CompareBorders(nullptr, cg, mRightCol, nullptr, nullptr, mCell,
                        mTableIsLTR, NS_SIDE_RIGHT, ADJACENT);
}

BCCellBorder
BCMapCellInfo::GetLeftInternalBorder()
{
  const nsIFrame* cg = (mCgAtLeft) ? mColGroup : nullptr;
  return CompareBorders(nullptr, cg, mLeftCol, nullptr, nullptr, mCell,
                        mTableIsLTR, NS_SIDE_LEFT, !ADJACENT);
}

BCCellBorder
BCMapCellInfo::GetBottomInternalBorder()
{
  const nsIFrame* rg = (mRgAtBottom) ? mRowGroup : nullptr;
  return CompareBorders(nullptr, nullptr, nullptr, rg, mBottomRow, mCell,
                        mTableIsLTR, NS_SIDE_BOTTOM, ADJACENT);
}

BCCellBorder
BCMapCellInfo::GetTopInternalBorder()
{
  const nsIFrame* rg = (mRgAtTop) ? mRowGroup : nullptr;
  return CompareBorders(nullptr, nullptr, nullptr, rg, mTopRow, mCell,
                        mTableIsLTR, NS_SIDE_TOP, !ADJACENT);
}

/* Here is the order for storing border edges in the cell map as a cell is processed. There are
   n=colspan top and bottom border edges per cell and n=rowspan left and right border edges per cell.

   1) On the top edge of the table, store the top edge. Never store the top edge otherwise, since
      a bottom edge from a cell above will take care of it.
   2) On the left edge of the table, store the left edge. Never store the left edge othewise, since
      a right edge from a cell to the left will take care of it.
   3) Store the right edge (or edges if a row span)
   4) Store the bottom edge (or edges if a col span)

   Since corners are computed with only an array of BCCornerInfo indexed by the number-of-cols, corner
   calculations are somewhat complicated. Using an array with number-of-rows * number-of-col entries
   would simplify this, but at an extra in memory cost of nearly 12 bytes per cell map entry. Collapsing
   borders already have about an extra 8 byte per cell map entry overhead (this could be
   reduced to 4 bytes if we are willing to not store border widths in nsTableCellFrame), Here are the
   rules in priority order for storing cornes in the cell map as a cell is processed. top-left means the
   left endpoint of the border edge on the top of the cell. There are n=colspan top and bottom border
   edges per cell and n=rowspan left and right border edges per cell.

   1) On the top edge of the table, store the top-left corner, unless on the left edge of the table.
      Never store the top-right corner, since it will get stored as a right-top corner.
   2) On the left edge of the table, store the left-top corner. Never store the left-bottom corner,
      since it will get stored as a bottom-left corner.
   3) Store the right-top corner if (a) it is the top right corner of the table or (b) it is not on
      the top edge of the table. Never store the right-bottom corner since it will get stored as a
      bottom-right corner.
   4) Store the bottom-right corner, if it is the bottom right corner of the table. Never store it
      otherwise, since it will get stored as either a right-top corner by a cell below or
      a bottom-left corner from a cell to the right.
   5) Store the bottom-left corner, if (a) on the bottom edge of the table or (b) if the left edge hits
      the top side of a colspan in its interior. Never store the corner otherwise, since it will
      get stored as a right-top corner by a cell from below.

   XXX the BC-RTL hack - The correct fix would be a rewrite as described in bug 203686.
   In order to draw borders in rtl conditions somehow correct, the existing structure which relies
   heavily on the assumption that the next cell sibling will be on the right side, has been modified.
   We flip the border during painting and during style lookup. Look for tableIsLTR for places where
   the flipping is done.
 */



// Calc the dominant border at every cell edge and corner within the current damage area
void
nsTableFrame::CalcBCBorders()
{
  NS_ASSERTION(IsBorderCollapse(),
               "calling CalcBCBorders on separated-border table");
  nsTableCellMap* tableCellMap = GetCellMap(); if (!tableCellMap) ABORT0();
  int32_t numRows = GetRowCount();
  int32_t numCols = GetColCount();
  if (!numRows || !numCols)
    return; // nothing to do

  // Get the property holding the table damage area and border widths
  BCPropertyData* propData = GetBCProperty();
  if (!propData) ABORT0();



  // calculate an expanded damage area
  nsIntRect damageArea(propData->mDamageArea);
  ExpandBCDamageArea(damageArea);

  // segments that are on the table border edges need
  // to be initialized only once
  bool tableBorderReset[4];
  for (uint32_t sideX = NS_SIDE_TOP; sideX <= NS_SIDE_LEFT; sideX++) {
    tableBorderReset[sideX] = false;
  }

  // vertical borders indexed in x-direction (cols)
  BCCellBorders lastVerBorders(damageArea.width + 1, damageArea.x);
  if (!lastVerBorders.borders) ABORT0();
  BCCellBorder  lastTopBorder, lastBottomBorder;
  // horizontal borders indexed in x-direction (cols)
  BCCellBorders lastBottomBorders(damageArea.width + 1, damageArea.x);
  if (!lastBottomBorders.borders) ABORT0();
  bool startSeg;
  bool gotRowBorder = false;

  BCMapCellInfo  info(this), ajaInfo(this);

  BCCellBorder currentBorder, adjacentBorder;
  BCCorners topCorners(damageArea.width + 1, damageArea.x);
  if (!topCorners.corners) ABORT0();
  BCCorners bottomCorners(damageArea.width + 1, damageArea.x);
  if (!bottomCorners.corners) ABORT0();

  BCMapCellIterator iter(this, damageArea);
  for (iter.First(info); !iter.mAtEnd; iter.Next(info)) {
    // see if lastTopBorder, lastBottomBorder need to be reset
    if (iter.IsNewRow()) {
      gotRowBorder = false;
      lastTopBorder.Reset(info.mRowIndex, info.mRowSpan);
      lastBottomBorder.Reset(info.GetCellEndRowIndex() + 1, info.mRowSpan);
    }
    else if (info.mColIndex > damageArea.x) {
      lastBottomBorder = lastBottomBorders[info.mColIndex - 1];
      if (info.mRowIndex >
          (lastBottomBorder.rowIndex - lastBottomBorder.rowSpan)) {
        // the top border's left edge butts against the middle of a rowspan
        lastTopBorder.Reset(info.mRowIndex, info.mRowSpan);
      }
      if (lastBottomBorder.rowIndex > (info.GetCellEndRowIndex() + 1)) {
        // the bottom border's left edge butts against the middle of a rowspan
        lastBottomBorder.Reset(info.GetCellEndRowIndex() + 1, info.mRowSpan);
      }
    }

    // find the dominant border considering the cell's top border and the table,
    // row group, row if the border is at the top of the table, otherwise it was
    // processed in a previous row
    if (0 == info.mRowIndex) {
      if (!tableBorderReset[NS_SIDE_TOP]) {
        propData->mTopBorderWidth = 0;
        tableBorderReset[NS_SIDE_TOP] = true;
      }
      for (int32_t colX = info.mColIndex; colX <= info.GetCellEndColIndex();
           colX++) {
        info.SetColumn(colX);
        currentBorder = info.GetTopEdgeBorder();
        // update/store the top left & top right corners of the seg
        BCCornerInfo& tlCorner = topCorners[colX]; // top left
        if (0 == colX) {
          // we are on right hand side of the corner
          tlCorner.Set(NS_SIDE_RIGHT, currentBorder);
        }
        else {
          tlCorner.Update(NS_SIDE_RIGHT, currentBorder);
          tableCellMap->SetBCBorderCorner(eTopLeft, *iter.mCellMap, 0, 0, colX,
                                          mozilla::css::Side(tlCorner.ownerSide),
                                          tlCorner.subWidth,
                                          tlCorner.bevel);
        }
        topCorners[colX + 1].Set(NS_SIDE_LEFT, currentBorder); // top right
        // update lastTopBorder and see if a new segment starts
        startSeg = SetHorBorder(currentBorder, tlCorner, lastTopBorder);
        // store the border segment in the cell map
        tableCellMap->SetBCBorderEdge(NS_SIDE_TOP, *iter.mCellMap, 0, 0, colX,
                                      1, currentBorder.owner,
                                      currentBorder.width, startSeg);

        info.SetTableTopBorderWidth(currentBorder.width);
        info.SetTopBorderWidths(currentBorder.width);
        info.SetColumnTopRightContBCBorder();
      }
      info.SetTableTopLeftContBCBorder();
    }
    else {
      // see if the top border needs to be the start of a segment due to a
      // vertical border owning the corner
      if (info.mColIndex > 0) {
        BCData& data = info.mCellData->mData;
        if (!data.IsTopStart()) {
          mozilla::css::Side cornerSide;
          bool bevel;
          data.GetCorner(cornerSide, bevel);
          if ((NS_SIDE_TOP == cornerSide) || (NS_SIDE_BOTTOM == cornerSide)) {
            data.SetTopStart(true);
          }
        }
      }
    }

    // find the dominant border considering the cell's left border and the
    // table, col group, col if the border is at the left of the table,
    // otherwise it was processed in a previous col
    if (0 == info.mColIndex) {
      if (!tableBorderReset[NS_SIDE_LEFT]) {
        propData->mLeftBorderWidth = 0;
        tableBorderReset[NS_SIDE_LEFT] = true;
      }
      info.mCurrentRowFrame = nullptr;
      for (int32_t rowY = info.mRowIndex; rowY <= info.GetCellEndRowIndex();
           rowY++) {
        info.IncrementRow(rowY == info.mRowIndex);
        currentBorder = info.GetLeftEdgeBorder();
        BCCornerInfo& tlCorner = (0 == rowY) ? topCorners[0] : bottomCorners[0];
        tlCorner.Update(NS_SIDE_BOTTOM, currentBorder);
        tableCellMap->SetBCBorderCorner(eTopLeft, *iter.mCellMap,
                                        iter.mRowGroupStart, rowY, 0,
                                        mozilla::css::Side(tlCorner.ownerSide),
                                        tlCorner.subWidth,
                                        tlCorner.bevel);
        bottomCorners[0].Set(NS_SIDE_TOP, currentBorder); // bottom left

        // update lastVerBordersBorder and see if a new segment starts
        startSeg = SetBorder(currentBorder, lastVerBorders[0]);
        // store the border segment in the cell map
        tableCellMap->SetBCBorderEdge(NS_SIDE_LEFT, *iter.mCellMap,
                                      iter.mRowGroupStart, rowY, info.mColIndex,
                                      1, currentBorder.owner,
                                      currentBorder.width, startSeg);
        info.SetTableLeftBorderWidth(rowY , currentBorder.width);
        info.SetLeftBorderWidths(currentBorder.width);
        info.SetRowLeftContBCBorder();
      }
      info.SetRowGroupLeftContBCBorder();
    }

    // find the dominant border considering the cell's right border, adjacent
    // cells and the table, row group, row
    if (info.mNumTableCols == info.GetCellEndColIndex() + 1) {
      // touches right edge of table
      if (!tableBorderReset[NS_SIDE_RIGHT]) {
        propData->mRightBorderWidth = 0;
        tableBorderReset[NS_SIDE_RIGHT] = true;
      }
      info.mCurrentRowFrame = nullptr;
      for (int32_t rowY = info.mRowIndex; rowY <= info.GetCellEndRowIndex();
           rowY++) {
        info.IncrementRow(rowY == info.mRowIndex);
        currentBorder = info.GetRightEdgeBorder();
        // update/store the top right & bottom right corners
        BCCornerInfo& trCorner = (0 == rowY) ?
                                 topCorners[info.GetCellEndColIndex() + 1] :
                                 bottomCorners[info.GetCellEndColIndex() + 1];
        trCorner.Update(NS_SIDE_BOTTOM, currentBorder);   // top right
        tableCellMap->SetBCBorderCorner(eTopRight, *iter.mCellMap,
                                        iter.mRowGroupStart, rowY,
                                        info.GetCellEndColIndex(),
                                        mozilla::css::Side(trCorner.ownerSide),
                                        trCorner.subWidth,
                                        trCorner.bevel);
        BCCornerInfo& brCorner = bottomCorners[info.GetCellEndColIndex() + 1];
        brCorner.Set(NS_SIDE_TOP, currentBorder); // bottom right
        tableCellMap->SetBCBorderCorner(eBottomRight, *iter.mCellMap,
                                        iter.mRowGroupStart, rowY,
                                        info.GetCellEndColIndex(),
                                        mozilla::css::Side(brCorner.ownerSide),
                                        brCorner.subWidth,
                                        brCorner.bevel);
        // update lastVerBorders and see if a new segment starts
        startSeg = SetBorder(currentBorder,
                             lastVerBorders[info.GetCellEndColIndex() + 1]);
        // store the border segment in the cell map and update cellBorders
        tableCellMap->SetBCBorderEdge(NS_SIDE_RIGHT, *iter.mCellMap,
                                      iter.mRowGroupStart, rowY,
                                      info.GetCellEndColIndex(), 1,
                                      currentBorder.owner, currentBorder.width,
                                      startSeg);
        info.SetTableRightBorderWidth(rowY, currentBorder.width);
        info.SetRightBorderWidths(currentBorder.width);
        info.SetRowRightContBCBorder();
      }
      info.SetRowGroupRightContBCBorder();
    }
    else {
      int32_t segLength = 0;
      BCMapCellInfo priorAjaInfo(this);
      for (int32_t rowY = info.mRowIndex; rowY <= info.GetCellEndRowIndex();
           rowY += segLength) {
        iter.PeekRight(info, rowY, ajaInfo);
        currentBorder  = info.GetRightInternalBorder();
        adjacentBorder = ajaInfo.GetLeftInternalBorder();
        currentBorder = CompareBorders(!CELL_CORNER, currentBorder,
                                        adjacentBorder, !HORIZONTAL);

        segLength = std::max(1, ajaInfo.mRowIndex + ajaInfo.mRowSpan - rowY);
        segLength = std::min(segLength, info.mRowIndex + info.mRowSpan - rowY);

        // update lastVerBorders and see if a new segment starts
        startSeg = SetBorder(currentBorder,
                             lastVerBorders[info.GetCellEndColIndex() + 1]);
        // store the border segment in the cell map and update cellBorders
        if (info.GetCellEndColIndex() < damageArea.XMost() &&
            rowY >= damageArea.y && rowY < damageArea.YMost()) {
          tableCellMap->SetBCBorderEdge(NS_SIDE_RIGHT, *iter.mCellMap,
                                        iter.mRowGroupStart, rowY,
                                        info.GetCellEndColIndex(), segLength,
                                        currentBorder.owner,
                                        currentBorder.width, startSeg);
          info.SetRightBorderWidths(currentBorder.width);
          ajaInfo.SetLeftBorderWidths(currentBorder.width);
        }
        // update the top right corner
        bool hitsSpanOnRight = (rowY > ajaInfo.mRowIndex) &&
                                  (rowY < ajaInfo.mRowIndex + ajaInfo.mRowSpan);
        BCCornerInfo* trCorner = ((0 == rowY) || hitsSpanOnRight) ?
                                 &topCorners[info.GetCellEndColIndex() + 1] :
                                 &bottomCorners[info.GetCellEndColIndex() + 1];
        trCorner->Update(NS_SIDE_BOTTOM, currentBorder);
        // if this is not the first time through,
        // consider the segment to the right
        if (rowY != info.mRowIndex) {
          currentBorder  = priorAjaInfo.GetBottomInternalBorder();
          adjacentBorder = ajaInfo.GetTopInternalBorder();
          currentBorder = CompareBorders(!CELL_CORNER, currentBorder,
                                          adjacentBorder, HORIZONTAL);
          trCorner->Update(NS_SIDE_RIGHT, currentBorder);
        }
        // store the top right corner in the cell map
        if (info.GetCellEndColIndex() < damageArea.XMost() &&
            rowY >= damageArea.y) {
          if (0 != rowY) {
            tableCellMap->SetBCBorderCorner(eTopRight, *iter.mCellMap,
                                            iter.mRowGroupStart, rowY,
                                            info.GetCellEndColIndex(),
                                            mozilla::css::Side(trCorner->ownerSide),
                                            trCorner->subWidth,
                                            trCorner->bevel);
          }
          // store any corners this cell spans together with the aja cell
          for (int32_t rX = rowY + 1; rX < rowY + segLength; rX++) {
            tableCellMap->SetBCBorderCorner(eBottomRight, *iter.mCellMap,
                                            iter.mRowGroupStart, rX,
                                            info.GetCellEndColIndex(),
                                            mozilla::css::Side(trCorner->ownerSide),
                                            trCorner->subWidth, false);
          }
        }
        // update bottom right corner, topCorners, bottomCorners
        hitsSpanOnRight = (rowY + segLength <
                           ajaInfo.mRowIndex + ajaInfo.mRowSpan);
        BCCornerInfo& brCorner = (hitsSpanOnRight) ?
                                 topCorners[info.GetCellEndColIndex() + 1] :
                                 bottomCorners[info.GetCellEndColIndex() + 1];
        brCorner.Set(NS_SIDE_TOP, currentBorder);
        priorAjaInfo = ajaInfo;
      }
    }
    for (int32_t colX = info.mColIndex + 1; colX <= info.GetCellEndColIndex();
         colX++) {
      lastVerBorders[colX].Reset(0,1);
    }

    // find the dominant border considering the cell's bottom border, adjacent
    // cells and the table, row group, row
    if (info.mNumTableRows == info.GetCellEndRowIndex() + 1) {
      // touches bottom edge of table
      if (!tableBorderReset[NS_SIDE_BOTTOM]) {
        propData->mBottomBorderWidth = 0;
        tableBorderReset[NS_SIDE_BOTTOM] = true;
      }
      for (int32_t colX = info.mColIndex; colX <= info.GetCellEndColIndex();
           colX++) {
        info.SetColumn(colX);
        currentBorder = info.GetBottomEdgeBorder();
        // update/store the bottom left & bottom right corners
        BCCornerInfo& blCorner = bottomCorners[colX]; // bottom left
        blCorner.Update(NS_SIDE_RIGHT, currentBorder);
        tableCellMap->SetBCBorderCorner(eBottomLeft, *iter.mCellMap,
                                        iter.mRowGroupStart,
                                        info.GetCellEndRowIndex(),
                                        colX,
                                        mozilla::css::Side(blCorner.ownerSide),
                                        blCorner.subWidth, blCorner.bevel);
        BCCornerInfo& brCorner = bottomCorners[colX + 1]; // bottom right
        brCorner.Update(NS_SIDE_LEFT, currentBorder);
        if (info.mNumTableCols == colX + 1) { // lower right corner of the table
          tableCellMap->SetBCBorderCorner(eBottomRight, *iter.mCellMap,
                                          iter.mRowGroupStart,
                                          info.GetCellEndRowIndex(),colX,
                                          mozilla::css::Side(brCorner.ownerSide),
                                          brCorner.subWidth,
                                          brCorner.bevel, true);
        }
        // update lastBottomBorder and see if a new segment starts
        startSeg = SetHorBorder(currentBorder, blCorner, lastBottomBorder);
        if (!startSeg) {
           // make sure that we did not compare apples to oranges i.e. the
           // current border should be a continuation of the lastBottomBorder,
           // as it is a bottom border
           // add 1 to the info.GetCellEndRowIndex()
           startSeg = (lastBottomBorder.rowIndex !=
                       (info.GetCellEndRowIndex() + 1));
        }
        // store the border segment in the cell map and update cellBorders
        tableCellMap->SetBCBorderEdge(NS_SIDE_BOTTOM, *iter.mCellMap,
                                      iter.mRowGroupStart,
                                      info.GetCellEndRowIndex(),
                                      colX, 1, currentBorder.owner,
                                      currentBorder.width, startSeg);
        // update lastBottomBorders
        lastBottomBorder.rowIndex = info.GetCellEndRowIndex() + 1;
        lastBottomBorder.rowSpan = info.mRowSpan;
        lastBottomBorders[colX] = lastBottomBorder;

        info.SetBottomBorderWidths(currentBorder.width);
        info.SetTableBottomBorderWidth(currentBorder.width);
        info.SetColumnBottomContBCBorder();
      }
      info.SetRowGroupBottomContBCBorder();
      info.SetColGroupBottomContBCBorder();
    }
    else {
      int32_t segLength = 0;
      for (int32_t colX = info.mColIndex; colX <= info.GetCellEndColIndex();
           colX += segLength) {
        iter.PeekBottom(info, colX, ajaInfo);
        currentBorder  = info.GetBottomInternalBorder();
        adjacentBorder = ajaInfo.GetTopInternalBorder();
        currentBorder = CompareBorders(!CELL_CORNER, currentBorder,
                                        adjacentBorder, HORIZONTAL);
        segLength = std::max(1, ajaInfo.mColIndex + ajaInfo.mColSpan - colX);
        segLength = std::min(segLength, info.mColIndex + info.mColSpan - colX);

        // update, store the bottom left corner
        BCCornerInfo& blCorner = bottomCorners[colX]; // bottom left
        bool hitsSpanBelow = (colX > ajaInfo.mColIndex) &&
                               (colX < ajaInfo.mColIndex + ajaInfo.mColSpan);
        bool update = true;
        if ((colX == info.mColIndex) && (colX > damageArea.x)) {
          int32_t prevRowIndex = lastBottomBorders[colX - 1].rowIndex;
          if (prevRowIndex > info.GetCellEndRowIndex() + 1) {
            // hits a rowspan on the right
            update = false;
            // the corner was taken care of during the cell on the left
          }
          else if (prevRowIndex < info.GetCellEndRowIndex() + 1) {
            // spans below the cell to the left
            topCorners[colX] = blCorner;
            blCorner.Set(NS_SIDE_RIGHT, currentBorder);
            update = false;
          }
        }
        if (update) {
          blCorner.Update(NS_SIDE_RIGHT, currentBorder);
        }
        if (info.GetCellEndRowIndex() < damageArea.YMost() &&
            (colX >= damageArea.x)) {
          if (hitsSpanBelow) {
            tableCellMap->SetBCBorderCorner(eBottomLeft, *iter.mCellMap,
                                            iter.mRowGroupStart,
                                            info.GetCellEndRowIndex(), colX,
                                            mozilla::css::Side(blCorner.ownerSide),
                                            blCorner.subWidth, blCorner.bevel);
          }
          // store any corners this cell spans together with the aja cell
          for (int32_t cX = colX + 1; cX < colX + segLength; cX++) {
            BCCornerInfo& corner = bottomCorners[cX];
            corner.Set(NS_SIDE_RIGHT, currentBorder);
            tableCellMap->SetBCBorderCorner(eBottomLeft, *iter.mCellMap,
                                            iter.mRowGroupStart,
                                            info.GetCellEndRowIndex(), cX,
                                            mozilla::css::Side(corner.ownerSide),
                                            corner.subWidth,
                                            false);
          }
        }
        // update lastBottomBorders and see if a new segment starts
        startSeg = SetHorBorder(currentBorder, blCorner, lastBottomBorder);
        if (!startSeg) {
           // make sure that we did not compare apples to oranges i.e. the
           // current border should be a continuation of the lastBottomBorder,
           // as it is a bottom border
           // add 1 to the info.GetCellEndRowIndex()
           startSeg = (lastBottomBorder.rowIndex !=
                       info.GetCellEndRowIndex() + 1);
        }
        lastBottomBorder.rowIndex = info.GetCellEndRowIndex() + 1;
        lastBottomBorder.rowSpan = info.mRowSpan;
        for (int32_t cX = colX; cX < colX + segLength; cX++) {
          lastBottomBorders[cX] = lastBottomBorder;
        }

        // store the border segment the cell map and update cellBorders
        if (info.GetCellEndRowIndex() < damageArea.YMost() &&
            (colX >= damageArea.x) &&
            (colX < damageArea.XMost())) {
          tableCellMap->SetBCBorderEdge(NS_SIDE_BOTTOM, *iter.mCellMap,
                                        iter.mRowGroupStart,
                                        info.GetCellEndRowIndex(),
                                        colX, segLength, currentBorder.owner,
                                        currentBorder.width, startSeg);
          info.SetBottomBorderWidths(currentBorder.width);
          ajaInfo.SetTopBorderWidths(currentBorder.width);
        }
        // update bottom right corner
        BCCornerInfo& brCorner = bottomCorners[colX + segLength];
        brCorner.Update(NS_SIDE_LEFT, currentBorder);
      }
      if (!gotRowBorder && 1 == info.mRowSpan &&
          (ajaInfo.mTopRow || info.mRgAtBottom)) {
        //get continuous row/row group border
        //we need to check the row group's bottom border if this is
        //the last row in the row group, but only a cell with rowspan=1
        //will know whether *this* row is at the bottom
        const nsIFrame* nextRowGroup = (ajaInfo.mRgAtTop) ? ajaInfo.mRowGroup :
                                                             nullptr;
        info.SetInnerRowGroupBottomContBCBorder(nextRowGroup, ajaInfo.mTopRow);
        gotRowBorder = true;
      }
    }

    // see if the cell to the right had a rowspan and its lower left border
    // needs be joined with this one's bottom
    // if  there is a cell to the right and the cell to right was a rowspan
    if ((info.mNumTableCols != info.GetCellEndColIndex() + 1) &&
        (lastBottomBorders[info.GetCellEndColIndex() + 1].rowSpan > 1)) {
      BCCornerInfo& corner = bottomCorners[info.GetCellEndColIndex() + 1];
      if ((NS_SIDE_TOP != corner.ownerSide) &&
          (NS_SIDE_BOTTOM != corner.ownerSide)) {
        // not a vertical owner
        BCCellBorder& thisBorder = lastBottomBorder;
        BCCellBorder& nextBorder = lastBottomBorders[info.mColIndex + 1];
        if ((thisBorder.color == nextBorder.color) &&
            (thisBorder.width == nextBorder.width) &&
            (thisBorder.style == nextBorder.style)) {
          // set the flag on the next border indicating it is not the start of a
          // new segment
          if (iter.mCellMap) {
            tableCellMap->ResetTopStart(NS_SIDE_BOTTOM, *iter.mCellMap,
                                        info.GetCellEndRowIndex(),
                                        info.GetCellEndColIndex() + 1);
          }
        }
      }
    }
  } // for (iter.First(info); info.mCell; iter.Next(info)) {
  // reset the bc flag and damage area
  SetNeedToCalcBCBorders(false);
  propData->mDamageArea = nsIntRect(0,0,0,0);
#ifdef DEBUG_TABLE_CELLMAP
  mCellMap->Dump();
#endif
}

class BCPaintBorderIterator;

struct BCVerticalSeg
{
  BCVerticalSeg();

  void Start(BCPaintBorderIterator& aIter,
             BCBorderOwner          aBorderOwner,
             BCPixelSize            aVerSegWidth,
             BCPixelSize            aHorSegHeight);

  void Initialize(BCPaintBorderIterator& aIter);
  void GetBottomCorner(BCPaintBorderIterator& aIter,
                       BCPixelSize            aHorSegHeight);


   void Paint(BCPaintBorderIterator& aIter,
              nsRenderingContext&   aRenderingContext,
              BCPixelSize            aHorSegHeight);
  void AdvanceOffsetY();
  void IncludeCurrentBorder(BCPaintBorderIterator& aIter);


  union {
    nsTableColFrame*  mCol;
    int32_t           mColWidth;
  };
  nscoord               mOffsetX;    // x-offset with respect to the table edge
  nscoord               mOffsetY;    // y-offset with respect to the table edge
  nscoord               mLength;     // vertical length including corners
  BCPixelSize           mWidth;      // width in pixels

  nsTableCellFrame*     mAjaCell;       // previous sibling to the first cell
                                        // where the segment starts, it can be
                                        // the owner of a segment
  nsTableCellFrame*     mFirstCell;     // cell at the start of the segment
  nsTableRowGroupFrame* mFirstRowGroup; // row group at the start of the segment
  nsTableRowFrame*      mFirstRow;      // row at the start of the segment
  nsTableCellFrame*     mLastCell;      // cell at the current end of the
                                        // segment


  uint8_t               mOwner;         // owner of the border, defines the
                                        // style
  mozilla::css::Side    mTopBevelSide;  // direction to bevel at the top
  nscoord               mTopBevelOffset; // how much to bevel at the top
  BCPixelSize           mBottomHorSegHeight; // height of the crossing
                                        //horizontal border
  nscoord               mBottomOffset;  // how much longer is the segment due
                                        // to the horizontal border, by this
                                        // amount the next segment needs to be
                                        // shifted.
  bool                  mIsBottomBevel; // should we bevel at the bottom
};

struct BCHorizontalSeg
{
  BCHorizontalSeg();

  void Start(BCPaintBorderIterator& aIter,
             BCBorderOwner          aBorderOwner,
             BCPixelSize            aBottomVerSegWidth,
             BCPixelSize            aHorSegHeight);
   void GetRightCorner(BCPaintBorderIterator& aIter,
                       BCPixelSize            aLeftSegWidth);
   void AdvanceOffsetX(int32_t aIncrement);
   void IncludeCurrentBorder(BCPaintBorderIterator& aIter);
   void Paint(BCPaintBorderIterator& aIter,
              nsRenderingContext&   aRenderingContext);

  nscoord            mOffsetX;       // x-offset with respect to the table edge
  nscoord            mOffsetY;       // y-offset with respect to the table edge
  nscoord            mLength;        // horizontal length including corners
  BCPixelSize        mWidth;         // border width in pixels
  nscoord            mLeftBevelOffset;   // how much to bevel at the left
  mozilla::css::Side mLeftBevelSide;     // direction to bevel at the left
  bool               mIsRightBevel;      // should we bevel at the right end
  nscoord            mRightBevelOffset;  // how much to bevel at the right
  mozilla::css::Side mRightBevelSide;    // direction to bevel at the right
  nscoord            mEndOffset;         // how much longer is the segment due
                                         // to the vertical border, by this
                                         // amount the next segment needs to be
                                         // shifted.
  uint8_t            mOwner;             // owner of the border, defines the
                                         // style
  nsTableCellFrame*  mFirstCell;         // cell at the start of the segment
  nsTableCellFrame*  mAjaCell;           // neighboring cell to the first cell
                                         // where the segment starts, it can be
                                         // the owner of a segment
};

// Iterates over borders (left border, corner, top border) in the cell map within a damage area
// from left to right, top to bottom. All members are in terms of the 1st in flow frames, except
// where suffixed by InFlow.
class BCPaintBorderIterator
{
public:


  explicit BCPaintBorderIterator(nsTableFrame* aTable);
  ~BCPaintBorderIterator() { if (mVerInfo) {
                              delete [] mVerInfo;
                           }}
  void Reset();

  /**
   * Determine the damage area in terms of rows and columns and finalize
   * mInitialOffsetX and mInitialOffsetY.
   * @param aDirtyRect - dirty rect in table coordinates
   * @return - true if we need to paint something given dirty rect
   */
  bool SetDamageArea(const nsRect& aDamageRect);
  void First();
  void Next();
  void AccumulateOrPaintHorizontalSegment(nsRenderingContext& aRenderingContext);
  void AccumulateOrPaintVerticalSegment(nsRenderingContext& aRenderingContext);
  void ResetVerInfo();
  void StoreColumnWidth(int32_t aIndex);
  bool VerticalSegmentOwnsCorner();

  nsTableFrame*         mTable;
  nsTableFrame*         mTableFirstInFlow;
  nsTableCellMap*       mTableCellMap;
  nsCellMap*            mCellMap;
  bool                  mTableIsLTR;
  int32_t               mColInc;            // +1 for ltr -1 for rtl
  const nsStyleBackground* mTableBgColor;
  nsTableFrame::RowGroupArray mRowGroups;

  nsTableRowGroupFrame* mPrevRg;
  nsTableRowGroupFrame* mRg;
  bool                  mIsRepeatedHeader;
  bool                  mIsRepeatedFooter;
  nsTableRowGroupFrame* mStartRg; // first row group in the damagearea
  int32_t               mRgIndex; // current row group index in the
                                        // mRowgroups array
  int32_t               mFifRgFirstRowIndex; // start row index of the first in
                                           // flow of the row group
  int32_t               mRgFirstRowIndex; // row index of the first row in the
                                          // row group
  int32_t               mRgLastRowIndex; // row index of the last row in the row
                                         // group
  int32_t               mNumTableRows;   // number of rows in the table and all
                                         // continuations
  int32_t               mNumTableCols;   // number of columns in the table
  int32_t               mColIndex;       // with respect to the table
  int32_t               mRowIndex;       // with respect to the table
  int32_t               mRepeatedHeaderRowIndex; // row index in a repeated
                                            //header, it's equivalent to
                                            // mRowIndex when we're in a repeated
                                            // header, and set to the last row
                                            // index of a repeated header when
                                            // we're not
  bool                  mIsNewRow;
  bool                  mAtEnd;             // the iterator cycled over all
                                             // borders
  nsTableRowFrame*      mPrevRow;
  nsTableRowFrame*      mRow;
  nsTableRowFrame*      mStartRow;    //first row in a inside the damagearea


  // cell properties
  nsTableCellFrame*     mPrevCell;
  nsTableCellFrame*     mCell;
  BCCellData*           mPrevCellData;
  BCCellData*           mCellData;
  BCData*               mBCData;

  bool                  IsTableTopMost()    {return (mRowIndex == 0) && !mTable->GetPrevInFlow();}
  bool                  IsTableRightMost()  {return (mColIndex >= mNumTableCols);}
  bool                  IsTableBottomMost() {return (mRowIndex >= mNumTableRows) && !mTable->GetNextInFlow();}
  bool                  IsTableLeftMost()   {return (mColIndex == 0);}
  bool                  IsDamageAreaTopMost()    {return (mRowIndex == mDamageArea.y);}
  bool                  IsDamageAreaRightMost()  {return (mColIndex >= mDamageArea.XMost());}
  bool                  IsDamageAreaBottomMost() {return (mRowIndex >= mDamageArea.YMost());}
  bool                  IsDamageAreaLeftMost()   {return (mColIndex == mDamageArea.x);}
  int32_t               GetRelativeColIndex() {return (mColIndex - mDamageArea.x);}

  nsIntRect             mDamageArea;        // damageArea in cellmap coordinates
  bool                  IsAfterRepeatedHeader() { return !mIsRepeatedHeader && (mRowIndex == (mRepeatedHeaderRowIndex + 1));}
  bool                  StartRepeatedFooter() {return mIsRepeatedFooter && (mRowIndex == mRgFirstRowIndex) && (mRowIndex != mDamageArea.y);}
  nscoord               mInitialOffsetX;  // offsetX of the first border with
                                            // respect to the table
  nscoord               mInitialOffsetY;    // offsetY of the first border with
                                            // respect to the table
  nscoord               mNextOffsetY;       // offsetY of the next segment
  BCVerticalSeg*        mVerInfo; // this array is used differently when
                                  // horizontal and vertical borders are drawn
                                  // When horizontal border are drawn we cache
                                  // the column widths and the width of the
                                  // vertical borders that arrive from top
                                  // When we draw vertical borders we store
                                  // lengths and width for vertical borders
                                  // before they are drawn while we  move over
                                  // the columns in the damage area
                                  // It has one more elements than columns are
                                  //in the table.
  BCHorizontalSeg       mHorSeg;            // the horizontal segment while we
                                            // move over the colums
  BCPixelSize           mPrevHorSegHeight;  // the height of the previous
                                            // horizontal border

private:

  bool SetNewRow(nsTableRowFrame* aRow = nullptr);
  bool SetNewRowGroup();
  void   SetNewData(int32_t aRowIndex, int32_t aColIndex);

};



BCPaintBorderIterator::BCPaintBorderIterator(nsTableFrame* aTable)
{
  mTable      = aTable;
  mVerInfo    = nullptr;
  nsMargin childAreaOffset = mTable->GetChildAreaOffset(nullptr);
  mTableFirstInFlow    = static_cast<nsTableFrame*>(mTable->FirstInFlow());
  mTableCellMap        = mTable->GetCellMap();
  // y position of first row in damage area
  mInitialOffsetY = mTable->GetPrevInFlow() ? 0 : childAreaOffset.top;
  mNumTableRows  = mTable->GetRowCount();
  mNumTableCols  = mTable->GetColCount();

  // Get the ordered row groups
  mTable->OrderRowGroups(mRowGroups);
  // initialize to a non existing index
  mRepeatedHeaderRowIndex = -99;

  mTableIsLTR = mTable->StyleVisibility()->mDirection ==
                   NS_STYLE_DIRECTION_LTR;
  mColInc = (mTableIsLTR) ? 1 : -1;

  nsIFrame* bgFrame =
    nsCSSRendering::FindNonTransparentBackgroundFrame(aTable);
  mTableBgColor = bgFrame->StyleBackground();
}

bool
BCPaintBorderIterator::SetDamageArea(const nsRect& aDirtyRect)
{

  uint32_t startRowIndex, endRowIndex, startColIndex, endColIndex;
  startRowIndex = endRowIndex = startColIndex = endColIndex = 0;
  bool done = false;
  bool haveIntersect = false;
  // find startRowIndex, endRowIndex
  nscoord rowY = mInitialOffsetY;
  for (uint32_t rgX = 0; rgX < mRowGroups.Length() && !done; rgX++) {
    nsTableRowGroupFrame* rgFrame = mRowGroups[rgX];
    for (nsTableRowFrame* rowFrame = rgFrame->GetFirstRow(); rowFrame;
         rowFrame = rowFrame->GetNextRow()) {
      // conservatively estimate the half border widths outside the row
      nscoord topBorderHalf    = (mTable->GetPrevInFlow()) ? 0 :
       nsPresContext::CSSPixelsToAppUnits(rowFrame->GetTopBCBorderWidth() + 1);
      nscoord bottomBorderHalf = (mTable->GetNextInFlow()) ? 0 :
        nsPresContext::CSSPixelsToAppUnits(rowFrame->GetBottomBCBorderWidth() + 1);
      // get the row rect relative to the table rather than the row group
      nsSize rowSize = rowFrame->GetSize();
      if (haveIntersect) {
        if (aDirtyRect.YMost() >= (rowY - topBorderHalf)) {
          nsTableRowFrame* fifRow =
            static_cast<nsTableRowFrame*>(rowFrame->FirstInFlow());
          endRowIndex = fifRow->GetRowIndex();
        }
        else done = true;
      }
      else {
        if ((rowY + rowSize.height + bottomBorderHalf) >= aDirtyRect.y) {
          mStartRg  = rgFrame;
          mStartRow = rowFrame;
          nsTableRowFrame* fifRow =
            static_cast<nsTableRowFrame*>(rowFrame->FirstInFlow());
          startRowIndex = endRowIndex = fifRow->GetRowIndex();
          haveIntersect = true;
        }
        else {
          mInitialOffsetY += rowSize.height;
        }
      }
      rowY += rowSize.height;
    }
  }
  mNextOffsetY = mInitialOffsetY;

  // XXX comment refers to the obsolete NS_FRAME_OUTSIDE_CHILDREN flag
  // XXX but I don't understand it, so not changing it for now
  // outer table borders overflow the table, so the table might be
  // target to other areas as the NS_FRAME_OUTSIDE_CHILDREN is set
  // on the table
  if (!haveIntersect)
    return false;
  // find startColIndex, endColIndex, startColX
  haveIntersect = false;
  if (0 == mNumTableCols)
    return false;
  int32_t leftCol, rightCol; // columns are in the range [leftCol, rightCol)

  nsMargin childAreaOffset = mTable->GetChildAreaOffset(nullptr);
  if (mTableIsLTR) {
    mInitialOffsetX = childAreaOffset.left; // x position of first col in
                                            // damage area
    leftCol = 0;
    rightCol = mNumTableCols;
  } else {
    // x position of first col in damage area
    mInitialOffsetX = mTable->GetRect().width - childAreaOffset.right;
    leftCol = mNumTableCols-1;
    rightCol = -1;
  }
  nscoord x = 0;
  int32_t colX;
  for (colX = leftCol; colX != rightCol; colX += mColInc) {
    nsTableColFrame* colFrame = mTableFirstInFlow->GetColFrame(colX);
    if (!colFrame) ABORT1(false);
    // get the col rect relative to the table rather than the col group
    nsSize size = colFrame->GetSize();
    if (haveIntersect) {
      // conservatively estimate the left half border width outside the col
      nscoord leftBorderHalf =
        nsPresContext::CSSPixelsToAppUnits(colFrame->GetLeftBorderWidth() + 1);
      if (aDirtyRect.XMost() >= (x - leftBorderHalf)) {
        endColIndex = colX;
      }
      else break;
    }
    else {
      // conservatively estimate the right half border width outside the col
      nscoord rightBorderHalf =
        nsPresContext::CSSPixelsToAppUnits(colFrame->GetRightBorderWidth() + 1);
      if ((x + size.width + rightBorderHalf) >= aDirtyRect.x) {
        startColIndex = endColIndex = colX;
        haveIntersect = true;
      }
      else {
        mInitialOffsetX += mColInc * size.width;
      }
    }
    x += size.width;
  }
  if (!mTableIsLTR) {
    uint32_t temp;
    mInitialOffsetX = mTable->GetRect().width - childAreaOffset.right;
    temp = startColIndex; startColIndex = endColIndex; endColIndex = temp;
    for (uint32_t column = 0; column < startColIndex; column++) {
      nsTableColFrame* colFrame = mTableFirstInFlow->GetColFrame(column);
      if (!colFrame) ABORT1(false);
      nsSize size = colFrame->GetSize();
      mInitialOffsetX += mColInc * size.width;
    }
  }
  if (!haveIntersect)
    return false;
  mDamageArea = nsIntRect(startColIndex, startRowIndex,
                          1 + DeprecatedAbs<int32_t>(endColIndex - startColIndex),
                          1 + endRowIndex - startRowIndex);

  Reset();
  mVerInfo = new BCVerticalSeg[mDamageArea.width + 1];
  if (!mVerInfo)
    return false;
  return true;
}

void
BCPaintBorderIterator::Reset()
{
  mAtEnd = true; // gets reset when First() is called
  mRg = mStartRg;
  mPrevRow  = nullptr;
  mRow      = mStartRow;
  mRowIndex      = 0;
  mColIndex      = 0;
  mRgIndex       = -1;
  mPrevCell      = nullptr;
  mCell          = nullptr;
  mPrevCellData  = nullptr;
  mCellData      = nullptr;
  mBCData        = nullptr;
  ResetVerInfo();
}

/**
 * Set the iterator data to a new cellmap coordinate
 * @param aRowIndex - the row index
 * @param aColIndex - the col index
 */
void
BCPaintBorderIterator::SetNewData(int32_t aY,
                                int32_t aX)
{
  if (!mTableCellMap || !mTableCellMap->mBCInfo) ABORT0();

  mColIndex    = aX;
  mRowIndex    = aY;
  mPrevCellData = mCellData;
  if (IsTableRightMost() && IsTableBottomMost()) {
   mCell = nullptr;
   mBCData = &mTableCellMap->mBCInfo->mLowerRightCorner;
  }
  else if (IsTableRightMost()) {
    mCellData = nullptr;
    mBCData = &mTableCellMap->mBCInfo->mRightBorders.ElementAt(aY);
  }
  else if (IsTableBottomMost()) {
    mCellData = nullptr;
    mBCData = &mTableCellMap->mBCInfo->mBottomBorders.ElementAt(aX);
  }
  else {
    if (uint32_t(mRowIndex - mFifRgFirstRowIndex) < mCellMap->mRows.Length()) {
      mBCData = nullptr;
      mCellData =
        (BCCellData*)mCellMap->mRows[mRowIndex - mFifRgFirstRowIndex].SafeElementAt(mColIndex);
      if (mCellData) {
        mBCData = &mCellData->mData;
        if (!mCellData->IsOrig()) {
          if (mCellData->IsRowSpan()) {
            aY -= mCellData->GetRowSpanOffset();
          }
          if (mCellData->IsColSpan()) {
            aX -= mCellData->GetColSpanOffset();
          }
          if ((aX >= 0) && (aY >= 0)) {
            mCellData = (BCCellData*)mCellMap->mRows[aY - mFifRgFirstRowIndex][aX];
          }
        }
        if (mCellData->IsOrig()) {
          mPrevCell = mCell;
          mCell = mCellData->GetCellFrame();
        }
      }
    }
  }
}

/**
 * Set the iterator to a new row
 * @param aRow - the new row frame, if null the iterator will advance to the
 *               next row
 */
bool
BCPaintBorderIterator::SetNewRow(nsTableRowFrame* aRow)
{
  mPrevRow = mRow;
  mRow     = (aRow) ? aRow : mRow->GetNextRow();
  if (mRow) {
    mIsNewRow = true;
    mRowIndex = mRow->GetRowIndex();
    mColIndex = mDamageArea.x;
    mPrevHorSegHeight = 0;
    if (mIsRepeatedHeader) {
      mRepeatedHeaderRowIndex = mRowIndex;
    }
  }
  else {
    mAtEnd = true;
  }
  return !mAtEnd;
}

/**
 * Advance the iterator to the next row group
 */
bool
BCPaintBorderIterator::SetNewRowGroup()
{

  mRgIndex++;

  mIsRepeatedHeader = false;
  mIsRepeatedFooter = false;

  NS_ASSERTION(mRgIndex >= 0, "mRgIndex out of bounds");
  if (uint32_t(mRgIndex) < mRowGroups.Length()) {
    mPrevRg = mRg;
    mRg = mRowGroups[mRgIndex];
    nsTableRowGroupFrame* fifRg =
      static_cast<nsTableRowGroupFrame*>(mRg->FirstInFlow());
    mFifRgFirstRowIndex = fifRg->GetStartRowIndex();
    mRgFirstRowIndex    = mRg->GetStartRowIndex();
    mRgLastRowIndex     = mRgFirstRowIndex + mRg->GetRowCount() - 1;

    if (SetNewRow(mRg->GetFirstRow())) {
      mCellMap = mTableCellMap->GetMapFor(fifRg, nullptr);
      if (!mCellMap) ABORT1(false);
    }
    if (mRg && mTable->GetPrevInFlow() && !mRg->GetPrevInFlow()) {
      // if mRowGroup doesn't have a prev in flow, then it may be a repeated
      // header or footer
      const nsStyleDisplay* display = mRg->StyleDisplay();
      if (mRowIndex == mDamageArea.y) {
        mIsRepeatedHeader = (NS_STYLE_DISPLAY_TABLE_HEADER_GROUP == display->mDisplay);
      }
      else {
        mIsRepeatedFooter = (NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP == display->mDisplay);
      }
    }
  }
  else {
    mAtEnd = true;
  }
  return !mAtEnd;
}

/**
 *  Move the iterator to the first position in the damageArea
 */
void
BCPaintBorderIterator::First()
{
  if (!mTable || (mDamageArea.x >= mNumTableCols) ||
      (mDamageArea.y >= mNumTableRows)) ABORT0();

  mAtEnd = false;

  uint32_t numRowGroups = mRowGroups.Length();
  for (uint32_t rgY = 0; rgY < numRowGroups; rgY++) {
    nsTableRowGroupFrame* rowG = mRowGroups[rgY];
    int32_t start = rowG->GetStartRowIndex();
    int32_t end   = start + rowG->GetRowCount() - 1;
    if ((mDamageArea.y >= start) && (mDamageArea.y <= end)) {
      mRgIndex = rgY - 1; // SetNewRowGroup increments rowGroupIndex
      if (SetNewRowGroup()) {
        while ((mRowIndex < mDamageArea.y) && !mAtEnd) {
          SetNewRow();
        }
        if (!mAtEnd) {
          SetNewData(mDamageArea.y, mDamageArea.x);
        }
      }
      return;
    }
  }
  mAtEnd = true;
}

/**
 * Advance the iterator to the next position
 */
void
BCPaintBorderIterator::Next()
{
  if (mAtEnd) ABORT0();
  mIsNewRow = false;

  mColIndex++;
  if (mColIndex > mDamageArea.XMost()) {
    mRowIndex++;
    if (mRowIndex == mDamageArea.YMost()) {
      mColIndex = mDamageArea.x;
    }
    else if (mRowIndex < mDamageArea.YMost()) {
      if (mRowIndex <= mRgLastRowIndex) {
        SetNewRow();
      }
      else {
        SetNewRowGroup();
      }
    }
    else {
      mAtEnd = true;
    }
  }
  if (!mAtEnd) {
    SetNewData(mRowIndex, mColIndex);
  }
}

// XXX if CalcVerCornerOffset and CalcHorCornerOffset remain similar, combine
// them
/** Compute the vertical offset of a vertical border segment
  * @param aCornerOwnerSide - which side owns the corner
  * @param aCornerSubWidth  - how wide is the nonwinning side of the corner
  * @param aHorWidth        - how wide is the horizontal edge of the corner
  * @param aIsStartOfSeg    - does this corner start a new segment
  * @param aIsBevel         - is this corner beveled
  * @return                 - offset in twips
  */
static nscoord
CalcVerCornerOffset(mozilla::css::Side aCornerOwnerSide,
                    BCPixelSize aCornerSubWidth,
                    BCPixelSize aHorWidth,
                    bool        aIsStartOfSeg,
                    bool        aIsBevel)
{
  nscoord offset = 0;
  // XXX These should be replaced with appropriate side-specific macros (which?)
  BCPixelSize smallHalf, largeHalf;
  if ((NS_SIDE_TOP == aCornerOwnerSide) ||
      (NS_SIDE_BOTTOM == aCornerOwnerSide)) {
    DivideBCBorderSize(aCornerSubWidth, smallHalf, largeHalf);
    if (aIsBevel) {
      offset = (aIsStartOfSeg) ? -largeHalf : smallHalf;
    }
    else {
      offset = (NS_SIDE_TOP == aCornerOwnerSide) ? smallHalf : -largeHalf;
    }
  }
  else {
    DivideBCBorderSize(aHorWidth, smallHalf, largeHalf);
    if (aIsBevel) {
      offset = (aIsStartOfSeg) ? -largeHalf : smallHalf;
    }
    else {
      offset = (aIsStartOfSeg) ? smallHalf : -largeHalf;
    }
  }
  return nsPresContext::CSSPixelsToAppUnits(offset);
}

/** Compute the horizontal offset of a horizontal border segment
  * @param aCornerOwnerSide - which side owns the corner
  * @param aCornerSubWidth  - how wide is the nonwinning side of the corner
  * @param aVerWidth        - how wide is the vertical edge of the corner
  * @param aIsStartOfSeg    - does this corner start a new segment
  * @param aIsBevel         - is this corner beveled
  * @param aTableIsLTR      - direction, the computation depends on ltr or rtl
  * @return                 - offset in twips
  */
static nscoord
CalcHorCornerOffset(mozilla::css::Side aCornerOwnerSide,
                    BCPixelSize aCornerSubWidth,
                    BCPixelSize aVerWidth,
                    bool        aIsStartOfSeg,
                    bool        aIsBevel,
                    bool        aTableIsLTR)
{
  nscoord offset = 0;
  // XXX These should be replaced with appropriate side-specific macros (which?)
  BCPixelSize smallHalf, largeHalf;
  if ((NS_SIDE_LEFT == aCornerOwnerSide) ||
      (NS_SIDE_RIGHT == aCornerOwnerSide)) {
    if (aTableIsLTR) {
      DivideBCBorderSize(aCornerSubWidth, smallHalf, largeHalf);
    }
    else {
      DivideBCBorderSize(aCornerSubWidth, largeHalf, smallHalf);
    }
    if (aIsBevel) {
      offset = (aIsStartOfSeg) ? -largeHalf : smallHalf;
    }
    else {
      offset = (NS_SIDE_LEFT == aCornerOwnerSide) ? smallHalf : -largeHalf;
    }
  }
  else {
    if (aTableIsLTR) {
      DivideBCBorderSize(aVerWidth, smallHalf, largeHalf);
    }
    else {
      DivideBCBorderSize(aVerWidth, largeHalf, smallHalf);
    }
    if (aIsBevel) {
      offset = (aIsStartOfSeg) ? -largeHalf : smallHalf;
    }
    else {
      offset = (aIsStartOfSeg) ? smallHalf : -largeHalf;
    }
  }
  return nsPresContext::CSSPixelsToAppUnits(offset);
}

BCVerticalSeg::BCVerticalSeg()
{
  mCol = nullptr;
  mFirstCell = mLastCell = mAjaCell = nullptr;
  mOffsetX = mOffsetY = mLength = mWidth = mTopBevelOffset = 0;
  mTopBevelSide = NS_SIDE_TOP;
  mOwner = eCellOwner;
}

/**
 * Start a new vertical segment
 * @param aIter         - iterator containing the structural information
 * @param aBorderOwner  - determines the border style
 * @param aVerSegWidth  - the width of segment in pixel
 * @param aHorSegHeight - the width of the horizontal segment joining the corner
 *                        at the start
 */
void
BCVerticalSeg::Start(BCPaintBorderIterator& aIter,
                     BCBorderOwner          aBorderOwner,
                     BCPixelSize            aVerSegWidth,
                     BCPixelSize            aHorSegHeight)
{
  mozilla::css::Side ownerSide   = NS_SIDE_TOP;
  bool bevel       = false;


  nscoord cornerSubWidth  = (aIter.mBCData) ?
                               aIter.mBCData->GetCorner(ownerSide, bevel) : 0;

  bool    topBevel        = (aVerSegWidth > 0) ? bevel : false;
  BCPixelSize maxHorSegHeight = std::max(aIter.mPrevHorSegHeight, aHorSegHeight);
  nscoord offset          = CalcVerCornerOffset(ownerSide, cornerSubWidth,
                                                maxHorSegHeight, true,
                                                topBevel);

  mTopBevelOffset = topBevel ?
    nsPresContext::CSSPixelsToAppUnits(maxHorSegHeight): 0;
  // XXX this assumes that only corners where 2 segments join can be beveled
  mTopBevelSide     = (aHorSegHeight > 0) ? NS_SIDE_RIGHT : NS_SIDE_LEFT;
  mOffsetY      += offset;
  mLength        = -offset;
  mWidth         = aVerSegWidth;
  mOwner         = aBorderOwner;
  mFirstCell     = aIter.mCell;
  mFirstRowGroup = aIter.mRg;
  mFirstRow      = aIter.mRow;
  if (aIter.GetRelativeColIndex() > 0) {
    mAjaCell = aIter.mVerInfo[aIter.GetRelativeColIndex() - 1].mLastCell;
  }
}

/**
 * Initialize the vertical segments with information that will persist for any
 * vertical segment in this column
 * @param aIter - iterator containing the structural information
 */
void
BCVerticalSeg::Initialize(BCPaintBorderIterator& aIter)
{
  int32_t relColIndex = aIter.GetRelativeColIndex();
  mCol = aIter.IsTableRightMost() ? aIter.mVerInfo[relColIndex - 1].mCol :
           aIter.mTableFirstInFlow->GetColFrame(aIter.mColIndex);
  if (!mCol) ABORT0();
  if (0 == relColIndex) {
    mOffsetX = aIter.mInitialOffsetX;
  }
  // set colX for the next column
  if (!aIter.IsDamageAreaRightMost()) {
    aIter.mVerInfo[relColIndex + 1].mOffsetX = mOffsetX +
                                         aIter.mColInc * mCol->GetSize().width;
  }
  mOffsetY = aIter.mInitialOffsetY;
  mLastCell = aIter.mCell;
}

/**
 * Compute the offsets for the bottom corner of a vertical segment
 * @param aIter         - iterator containing the structural information
 * @param aHorSegHeight - the width of the horizontal segment joining the corner
 *                        at the start
 */
void
BCVerticalSeg::GetBottomCorner(BCPaintBorderIterator& aIter,
                               BCPixelSize            aHorSegHeight)
{
   mozilla::css::Side ownerSide = NS_SIDE_TOP;
   nscoord cornerSubWidth = 0;
   bool bevel = false;
   if (aIter.mBCData) {
     cornerSubWidth = aIter.mBCData->GetCorner(ownerSide, bevel);
   }
   mIsBottomBevel = (mWidth > 0) ? bevel : false;
   mBottomHorSegHeight = std::max(aIter.mPrevHorSegHeight, aHorSegHeight);
   mBottomOffset = CalcVerCornerOffset(ownerSide, cornerSubWidth,
                                    mBottomHorSegHeight,
                                    false, mIsBottomBevel);
   mLength += mBottomOffset;
}

/**
 * Paint the vertical segment
 * @param aIter         - iterator containing the structural information
 * @param aRenderingContext - the rendering context
 * @param aHorSegHeight - the width of the horizontal segment joining the corner
 *                        at the start
 */
void
BCVerticalSeg::Paint(BCPaintBorderIterator& aIter,
                     nsRenderingContext&   aRenderingContext,
                     BCPixelSize            aHorSegHeight)
{
  // get the border style, color and paint the segment
  mozilla::css::Side side = (aIter.IsDamageAreaRightMost()) ? NS_SIDE_RIGHT :
                                                    NS_SIDE_LEFT;
  int32_t relColIndex = aIter.GetRelativeColIndex();
  nsTableColFrame* col           = mCol; if (!col) ABORT0();
  nsTableCellFrame* cell         = mFirstCell; // ???
  nsIFrame* owner = nullptr;
  uint8_t style = NS_STYLE_BORDER_STYLE_SOLID;
  nscolor color = 0xFFFFFFFF;

  // All the tables frames have the same presContext, so we just use any one
  // that exists here:
  int32_t appUnitsPerDevPixel = col->PresContext()->AppUnitsPerDevPixel();

  switch (mOwner) {
    case eTableOwner:
      owner = aIter.mTable;
      break;
    case eAjaColGroupOwner:
      side = NS_SIDE_RIGHT;
      if (!aIter.IsTableRightMost() && (relColIndex > 0)) {
        col = aIter.mVerInfo[relColIndex - 1].mCol;
      } // and fall through
    case eColGroupOwner:
      if (col) {
        owner = col->GetParent();
      }
      break;
    case eAjaColOwner:
      side = NS_SIDE_RIGHT;
      if (!aIter.IsTableRightMost() && (relColIndex > 0)) {
        col = aIter.mVerInfo[relColIndex - 1].mCol;
      } // and fall through
    case eColOwner:
      owner = col;
      break;
    case eAjaRowGroupOwner:
      NS_ERROR("a neighboring rowgroup can never own a vertical border");
      // and fall through
    case eRowGroupOwner:
      NS_ASSERTION(aIter.IsTableLeftMost() || aIter.IsTableRightMost(),
                  "row group can own border only at table edge");
      owner = mFirstRowGroup;
      break;
    case eAjaRowOwner:
      NS_ASSERTION(false, "program error"); // and fall through
    case eRowOwner:
      NS_ASSERTION(aIter.IsTableLeftMost() || aIter.IsTableRightMost(),
                   "row can own border only at table edge");
      owner = mFirstRow;
      break;
    case eAjaCellOwner:
      side = NS_SIDE_RIGHT;
      cell = mAjaCell; // and fall through
    case eCellOwner:
      owner = cell;
      break;
  }
  if (owner) {
    ::GetPaintStyleInfo(owner, side, style, color, aIter.mTableIsLTR);
  }
  BCPixelSize smallHalf, largeHalf;
  DivideBCBorderSize(mWidth, smallHalf, largeHalf);
  nsRect segRect(mOffsetX - nsPresContext::CSSPixelsToAppUnits(largeHalf),
                 mOffsetY,
                 nsPresContext::CSSPixelsToAppUnits(mWidth), mLength);
  nscoord bottomBevelOffset = (mIsBottomBevel) ?
                  nsPresContext::CSSPixelsToAppUnits(mBottomHorSegHeight) : 0;
  mozilla::css::Side bottomBevelSide = ((aHorSegHeight > 0) ^ !aIter.mTableIsLTR) ?
                            NS_SIDE_RIGHT : NS_SIDE_LEFT;
  mozilla::css::Side topBevelSide = ((mTopBevelSide == NS_SIDE_RIGHT) ^ !aIter.mTableIsLTR)?
                         NS_SIDE_RIGHT : NS_SIDE_LEFT;
  nsCSSRendering::DrawTableBorderSegment(aRenderingContext, style, color,
                                         aIter.mTableBgColor, segRect,
                                         appUnitsPerDevPixel,
                                         nsPresContext::AppUnitsPerCSSPixel(),
                                         topBevelSide, mTopBevelOffset,
                                         bottomBevelSide, bottomBevelOffset);
}

/**
 * Advance the start point of a segment
 */
void
BCVerticalSeg::AdvanceOffsetY()
{
  mOffsetY +=  mLength - mBottomOffset;
}

/**
 * Accumulate the current segment
 */
void
BCVerticalSeg::IncludeCurrentBorder(BCPaintBorderIterator& aIter)
{
  mLastCell = aIter.mCell;
  mLength  += aIter.mRow->GetRect().height;
}

BCHorizontalSeg::BCHorizontalSeg()
{
  mOffsetX = mOffsetY = mLength = mWidth =  mLeftBevelOffset = 0;
  mLeftBevelSide = NS_SIDE_TOP;
  mFirstCell = mAjaCell = nullptr;
}

/** Initialize a horizontal border segment for painting
  * @param aIter              - iterator storing the current and adjacent frames
  * @param aBorderOwner       - which frame owns the border
  * @param aBottomVerSegWidth - vertical segment width coming from up
  * @param aHorSegHeight      - the height of the segment
  +  */
void
BCHorizontalSeg::Start(BCPaintBorderIterator& aIter,
                       BCBorderOwner        aBorderOwner,
                       BCPixelSize          aBottomVerSegWidth,
                       BCPixelSize          aHorSegHeight)
{
  mozilla::css::Side cornerOwnerSide = NS_SIDE_TOP;
  bool bevel     = false;

  mOwner = aBorderOwner;
  nscoord cornerSubWidth  = (aIter.mBCData) ?
                             aIter.mBCData->GetCorner(cornerOwnerSide,
                                                       bevel) : 0;

  bool    leftBevel = (aHorSegHeight > 0) ? bevel : false;
  int32_t relColIndex = aIter.GetRelativeColIndex();
  nscoord maxVerSegWidth = std::max(aIter.mVerInfo[relColIndex].mWidth,
                                  aBottomVerSegWidth);
  nscoord offset = CalcHorCornerOffset(cornerOwnerSide, cornerSubWidth,
                                       maxVerSegWidth, true, leftBevel,
                                       aIter.mTableIsLTR);
  mLeftBevelOffset = (leftBevel && (aHorSegHeight > 0)) ? maxVerSegWidth : 0;
  // XXX this assumes that only corners where 2 segments join can be beveled
  mLeftBevelSide   = (aBottomVerSegWidth > 0) ? NS_SIDE_BOTTOM : NS_SIDE_TOP;
  if (aIter.mTableIsLTR) {
    mOffsetX += offset;
  }
  else {
    mOffsetX -= offset;
  }
  mLength          = -offset;
  mWidth           = aHorSegHeight;
  mFirstCell       = aIter.mCell;
  mAjaCell         = (aIter.IsDamageAreaTopMost()) ? nullptr :
                     aIter.mVerInfo[relColIndex].mLastCell;
}

/**
 * Compute the offsets for the right corner of a horizontal segment
 * @param aIter         - iterator containing the structural information
 * @param aLeftSegWidth - the width of the vertical segment joining the corner
 *                        at the start
 */
void
BCHorizontalSeg::GetRightCorner(BCPaintBorderIterator& aIter,
                                BCPixelSize            aLeftSegWidth)
{
  mozilla::css::Side ownerSide = NS_SIDE_TOP;
  nscoord cornerSubWidth = 0;
  bool bevel = false;
  if (aIter.mBCData) {
    cornerSubWidth = aIter.mBCData->GetCorner(ownerSide, bevel);
  }

  mIsRightBevel = (mWidth > 0) ? bevel : 0;
  int32_t relColIndex = aIter.GetRelativeColIndex();
  nscoord verWidth = std::max(aIter.mVerInfo[relColIndex].mWidth, aLeftSegWidth);
  mEndOffset = CalcHorCornerOffset(ownerSide, cornerSubWidth, verWidth,
                                   false, mIsRightBevel, aIter.mTableIsLTR);
  mLength += mEndOffset;
  mRightBevelOffset = (mIsRightBevel) ?
                       nsPresContext::CSSPixelsToAppUnits(verWidth) : 0;
  mRightBevelSide = (aLeftSegWidth > 0) ? NS_SIDE_BOTTOM : NS_SIDE_TOP;
}

/**
 * Paint the horizontal segment
 * @param aIter         - iterator containing the structural information
 * @param aRenderingContext - the rendering context
 */
void
BCHorizontalSeg::Paint(BCPaintBorderIterator& aIter,
                       nsRenderingContext&   aRenderingContext)
{
  // get the border style, color and paint the segment
  mozilla::css::Side side = (aIter.IsDamageAreaBottomMost()) ? NS_SIDE_BOTTOM :
                                                     NS_SIDE_TOP;
  nsIFrame* rg   = aIter.mRg;  if (!rg) ABORT0();
  nsIFrame* row  = aIter.mRow; if (!row) ABORT0();
  nsIFrame* cell = mFirstCell;
  nsIFrame* col;
  nsIFrame* owner = nullptr;

  // All the tables frames have the same presContext, so we just use any one
  // that exists here:
  int32_t appUnitsPerDevPixel = row->PresContext()->AppUnitsPerDevPixel();

  uint8_t style = NS_STYLE_BORDER_STYLE_SOLID;
  nscolor color = 0xFFFFFFFF;


  switch (mOwner) {
    case eTableOwner:
      owner = aIter.mTable;
      break;
    case eAjaColGroupOwner:
      NS_ERROR("neighboring colgroups can never own a horizontal border");
      // and fall through
    case eColGroupOwner:
      NS_ASSERTION(aIter.IsTableTopMost() || aIter.IsTableBottomMost(),
                   "col group can own border only at the table edge");
      col = aIter.mTableFirstInFlow->GetColFrame(aIter.mColIndex - 1);
      if (!col) ABORT0();
      owner = col->GetParent();
      break;
    case eAjaColOwner:
      NS_ERROR("neighboring column can never own a horizontal border");
      // and fall through
    case eColOwner:
      NS_ASSERTION(aIter.IsTableTopMost() || aIter.IsTableBottomMost(),
                   "col can own border only at the table edge");
      owner = aIter.mTableFirstInFlow->GetColFrame(aIter.mColIndex - 1);
      break;
    case eAjaRowGroupOwner:
      side = NS_SIDE_BOTTOM;
      rg = (aIter.IsTableBottomMost()) ? aIter.mRg : aIter.mPrevRg;
      // and fall through
    case eRowGroupOwner:
      owner = rg;
      break;
    case eAjaRowOwner:
      side = NS_SIDE_BOTTOM;
      row = (aIter.IsTableBottomMost()) ? aIter.mRow : aIter.mPrevRow;
      // and fall through
      case eRowOwner:
      owner = row;
      break;
    case eAjaCellOwner:
      side = NS_SIDE_BOTTOM;
      // if this is null due to the damage area origin-y > 0, then the border
      // won't show up anyway
      cell = mAjaCell;
      // and fall through
    case eCellOwner:
      owner = cell;
      break;
  }
  if (owner) {
    ::GetPaintStyleInfo(owner, side, style, color, aIter.mTableIsLTR);
  }
  BCPixelSize smallHalf, largeHalf;
  DivideBCBorderSize(mWidth, smallHalf, largeHalf);
  nsRect segRect(mOffsetX,
                 mOffsetY - nsPresContext::CSSPixelsToAppUnits(largeHalf),
                 mLength,
                 nsPresContext::CSSPixelsToAppUnits(mWidth));
  if (aIter.mTableIsLTR) {
    nsCSSRendering::DrawTableBorderSegment(aRenderingContext, style, color,
                                           aIter.mTableBgColor, segRect,
                                           appUnitsPerDevPixel,
                                           nsPresContext::AppUnitsPerCSSPixel(),
                                           mLeftBevelSide,
                                           nsPresContext::CSSPixelsToAppUnits(mLeftBevelOffset),
                                           mRightBevelSide, mRightBevelOffset);
  }
  else {
    segRect.x -= segRect.width;
    nsCSSRendering::DrawTableBorderSegment(aRenderingContext, style, color,
                                           aIter.mTableBgColor, segRect,
                                           appUnitsPerDevPixel,
                                           nsPresContext::AppUnitsPerCSSPixel(),
                                           mRightBevelSide, mRightBevelOffset,
                                           mLeftBevelSide,
                                           nsPresContext::CSSPixelsToAppUnits(mLeftBevelOffset));
  }
}

/**
 * Advance the start point of a segment
 */
void
BCHorizontalSeg::AdvanceOffsetX(int32_t aIncrement)
{
  mOffsetX += aIncrement * (mLength - mEndOffset);
}

/**
 * Accumulate the current segment
 */
void
BCHorizontalSeg::IncludeCurrentBorder(BCPaintBorderIterator& aIter)
{
  mLength += aIter.mVerInfo[aIter.GetRelativeColIndex()].mColWidth;
}

/**
 * store the column width information while painting horizontal segment
 */
void
BCPaintBorderIterator::StoreColumnWidth(int32_t aIndex)
{
  if (IsTableRightMost()) {
      mVerInfo[aIndex].mColWidth = mVerInfo[aIndex - 1].mColWidth;
  }
  else {
    nsTableColFrame* col = mTableFirstInFlow->GetColFrame(mColIndex);
    if (!col) ABORT0();
    mVerInfo[aIndex].mColWidth = col->GetSize().width;
  }
}
/**
 * Determine if a vertical segment owns the corder
 */
bool
BCPaintBorderIterator::VerticalSegmentOwnsCorner()
{
  mozilla::css::Side cornerOwnerSide = NS_SIDE_TOP;
  bool bevel = false;
  if (mBCData) {
    mBCData->GetCorner(cornerOwnerSide, bevel);
  }
  // unitialized ownerside, bevel
  return  (NS_SIDE_TOP == cornerOwnerSide) ||
          (NS_SIDE_BOTTOM == cornerOwnerSide);
}

/**
 * Paint if necessary a horizontal segment, otherwise accumulate it
 * @param aRenderingContext - the rendering context
 */
void
BCPaintBorderIterator::AccumulateOrPaintHorizontalSegment(nsRenderingContext& aRenderingContext)
{

  int32_t relColIndex = GetRelativeColIndex();
  // store the current col width if it hasn't been already
  if (mVerInfo[relColIndex].mColWidth < 0) {
    StoreColumnWidth(relColIndex);
  }

  BCBorderOwner borderOwner = eCellOwner;
  BCBorderOwner ignoreBorderOwner;
  bool isSegStart = true;
  bool ignoreSegStart;

  nscoord leftSegWidth =
    mBCData ? mBCData->GetLeftEdge(ignoreBorderOwner, ignoreSegStart) : 0;
  nscoord topSegHeight =
    mBCData ? mBCData->GetTopEdge(borderOwner, isSegStart) : 0;

  if (mIsNewRow || (IsDamageAreaLeftMost() && IsDamageAreaBottomMost())) {
    // reset for every new row and on the bottom of the last row
    mHorSeg.mOffsetY = mNextOffsetY;
    mNextOffsetY     = mNextOffsetY + mRow->GetSize().height;
    mHorSeg.mOffsetX = mInitialOffsetX;
    mHorSeg.Start(*this, borderOwner, leftSegWidth, topSegHeight);
  }

  if (!IsDamageAreaLeftMost() && (isSegStart || IsDamageAreaRightMost() ||
                                  VerticalSegmentOwnsCorner())) {
    // paint the previous seg or the current one if IsDamageAreaRightMost()
    if (mHorSeg.mLength > 0) {
      mHorSeg.GetRightCorner(*this, leftSegWidth);
      if (mHorSeg.mWidth > 0) {
        mHorSeg.Paint(*this, aRenderingContext);
      }
      mHorSeg.AdvanceOffsetX(mColInc);
    }
    mHorSeg.Start(*this, borderOwner, leftSegWidth, topSegHeight);
  }
  mHorSeg.IncludeCurrentBorder(*this);
  mVerInfo[relColIndex].mWidth = leftSegWidth;
  mVerInfo[relColIndex].mLastCell = mCell;
}
/**
 * Paint if necessary a vertical segment, otherwise  it
 * @param aRenderingContext - the rendering context
 */
void
BCPaintBorderIterator::AccumulateOrPaintVerticalSegment(nsRenderingContext& aRenderingContext)
{
  BCBorderOwner borderOwner = eCellOwner;
  BCBorderOwner ignoreBorderOwner;
  bool isSegStart = true;
  bool ignoreSegStart;

  nscoord verSegWidth  =
    mBCData ? mBCData->GetLeftEdge(borderOwner, isSegStart) : 0;
  nscoord horSegHeight =
    mBCData ? mBCData->GetTopEdge(ignoreBorderOwner, ignoreSegStart) : 0;

  int32_t relColIndex = GetRelativeColIndex();
  BCVerticalSeg& verSeg = mVerInfo[relColIndex];
  if (!verSeg.mCol) { // on the first damaged row and the first segment in the
                      // col
    verSeg.Initialize(*this);
    verSeg.Start(*this, borderOwner, verSegWidth, horSegHeight);
  }

  if (!IsDamageAreaTopMost() && (isSegStart || IsDamageAreaBottomMost() ||
                                 IsAfterRepeatedHeader() ||
                                 StartRepeatedFooter())) {
    // paint the previous seg or the current one if IsDamageAreaBottomMost()
    if (verSeg.mLength > 0) {
      verSeg.GetBottomCorner(*this, horSegHeight);
      if (verSeg.mWidth > 0) {
        verSeg.Paint(*this, aRenderingContext, horSegHeight);
      }
      verSeg.AdvanceOffsetY();
    }
    verSeg.Start(*this, borderOwner, verSegWidth, horSegHeight);
  }
  verSeg.IncludeCurrentBorder(*this);
  mPrevHorSegHeight = horSegHeight;
}

/**
 * Reset the vertical information cache
 */
void
BCPaintBorderIterator::ResetVerInfo()
{
  if (mVerInfo) {
    memset(mVerInfo, 0, mDamageArea.width * sizeof(BCVerticalSeg));
    // XXX reinitialize properly
    for (int32_t xIndex = 0; xIndex < mDamageArea.width; xIndex++) {
      mVerInfo[xIndex].mColWidth = -1;
    }
  }
}

/**
 * Method to paint BCBorders, this does not use currently display lists although
 * it will do this in future
 * @param aRenderingContext - the rendering context
 * @param aDirtyRect        - inside this rectangle the BC Borders will redrawn
 */
void
nsTableFrame::PaintBCBorders(nsRenderingContext& aRenderingContext,
                             const nsRect&        aDirtyRect)
{
  // We first transfer the aDirtyRect into cellmap coordinates to compute which
  // cell borders need to be painted
  BCPaintBorderIterator iter(this);
  if (!iter.SetDamageArea(aDirtyRect))
    return;

  // First, paint all of the vertical borders from top to bottom and left to
  // right as they become complete. They are painted first, since they are less
  // efficient to paint than horizontal segments. They were stored with as few
  // segments as possible (since horizontal borders are painted last and
  // possibly over them). For every cell in a row that fails in the damage are
  // we look up if the current border would start a new segment, if so we paint
  // the previously stored vertical segment and start a new segment. After
  // this we  the now active segment with the current border. These
  // segments are stored in mVerInfo to be used on the next row
  for (iter.First(); !iter.mAtEnd; iter.Next()) {
    iter.AccumulateOrPaintVerticalSegment(aRenderingContext);
  }

  // Next, paint all of the horizontal border segments from top to bottom reuse
  // the mVerInfo array to keep track of col widths and vertical segments for
  // corner calculations
  iter.Reset();
  for (iter.First(); !iter.mAtEnd; iter.Next()) {
    iter.AccumulateOrPaintHorizontalSegment(aRenderingContext);
  }
}

bool nsTableFrame::RowHasSpanningCells(int32_t aRowIndex, int32_t aNumEffCols)
{
  bool result = false;
  nsTableCellMap* cellMap = GetCellMap();
  NS_PRECONDITION (cellMap, "bad call, cellMap not yet allocated.");
  if (cellMap) {
    result = cellMap->RowHasSpanningCells(aRowIndex, aNumEffCols);
  }
  return result;
}

bool nsTableFrame::RowIsSpannedInto(int32_t aRowIndex, int32_t aNumEffCols)
{
  bool result = false;
  nsTableCellMap* cellMap = GetCellMap();
  NS_PRECONDITION (cellMap, "bad call, cellMap not yet allocated.");
  if (cellMap) {
    result = cellMap->RowIsSpannedInto(aRowIndex, aNumEffCols);
  }
  return result;
}

/* static */
void
nsTableFrame::InvalidateTableFrame(nsIFrame* aFrame,
                                   const nsRect& aOrigRect,
                                   const nsRect& aOrigVisualOverflow,
                                   bool aIsFirstReflow)
{
  nsIFrame* parent = aFrame->GetParent();
  NS_ASSERTION(parent, "What happened here?");

  if (parent->GetStateBits() & NS_FRAME_FIRST_REFLOW) {
    // Don't bother; we'll invalidate the parent's overflow rect when
    // we finish reflowing it.
    return;
  }

  // The part that looks at both the rect and the overflow rect is a
  // bit of a hack.  See nsBlockFrame::ReflowLine for an eloquent
  // description of its hackishness.
  //
  // This doesn't really make sense now that we have DLBI.
  // This code can probably be simplified a fair bit.
  nsRect visualOverflow = aFrame->GetVisualOverflowRect();
  if (aIsFirstReflow ||
      aOrigRect.TopLeft() != aFrame->GetPosition() ||
      aOrigVisualOverflow.TopLeft() != visualOverflow.TopLeft()) {
    // Invalidate the old and new overflow rects.  Note that if the
    // frame moved, we can't just use aOrigVisualOverflow, since it's in
    // coordinates relative to the old position.  So invalidate via
    // aFrame's parent, and reposition that overflow rect to the right
    // place.
    // XXXbz this doesn't handle outlines, does it?
    aFrame->InvalidateFrame();
    parent->InvalidateFrameWithRect(aOrigVisualOverflow + aOrigRect.TopLeft());
  } else if (aOrigRect.Size() != aFrame->GetSize() ||
             aOrigVisualOverflow.Size() != visualOverflow.Size()){
    aFrame->InvalidateFrameWithRect(aOrigVisualOverflow);
    aFrame->InvalidateFrame();
    parent->InvalidateFrameWithRect(aOrigRect);;
    parent->InvalidateFrame();
  }
}
