/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */
#include "nsCOMPtr.h"
#include "nsVoidArray.h"
#include "nsTableFrame.h"
#include "nsTableBorderCollapser.h"
#include "nsIRenderingContext.h"
#include "nsIStyleContext.h"
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
#include "nsIHTMLContent.h"

#include "BasicTableLayoutStrategy.h"
#include "FixedTableLayoutStrategy.h"

#include "nsIPresContext.h"
#include "nsCSSRendering.h"
#include "nsStyleConsts.h"
#include "nsVoidArray.h"
#include "nsIView.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIReflowCommand.h"
#include "nsLayoutAtoms.h"
#include "nsIDeviceContext.h"
#include "nsIStyleSet.h"
#include "nsIPresShell.h"
#include "nsIDOMElement.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDOMHTMLBodyElement.h"
#include "nsISizeOfHandler.h"
#include "nsIScrollableFrame.h"
#include "nsHTMLReflowCommand.h"
#include "nsIFrameManager.h"

static NS_DEFINE_IID(kIHTMLElementIID, NS_IDOMHTMLELEMENT_IID);
static NS_DEFINE_IID(kIBodyElementIID, NS_IDOMHTMLBODYELEMENT_IID);
static NS_DEFINE_IID(kITableRowGroupFrameIID, NS_ITABLEROWGROUPFRAME_IID);
static NS_DEFINE_IID(kIScrollableFrameIID, NS_ISCROLLABLE_FRAME_IID);

// helper function for dealing with placeholder for positioned/floated table
static void GetPlaceholderFor(nsIPresContext& aPresContext, nsIFrame& aFrame, nsIFrame** aPlaceholder);

static const PRInt32 kColumnWidthIncrement=10;

#if 1
PRBool nsDebugTable::gRflTableOuter = PR_FALSE;
PRBool nsDebugTable::gRflTable      = PR_FALSE;
PRBool nsDebugTable::gRflRowGrp     = PR_FALSE;
PRBool nsDebugTable::gRflRow        = PR_FALSE;
PRBool nsDebugTable::gRflCell       = PR_FALSE;
PRBool nsDebugTable::gRflArea       = PR_FALSE;
#else
PRBool nsDebugTable::gRflTableOuter = PR_TRUE;
PRBool nsDebugTable::gRflTable      = PR_TRUE;
PRBool nsDebugTable::gRflRowGrp     = PR_TRUE;
PRBool nsDebugTable::gRflRow        = PR_TRUE;
PRBool nsDebugTable::gRflCell       = PR_TRUE;
PRBool nsDebugTable::gRflArea       = PR_TRUE;
#endif
static PRInt32 gRflCount = 0;

/* ----------- InnerTableReflowState ---------- */

struct InnerTableReflowState {

  // Our reflow state
  const nsHTMLReflowState& reflowState;

  // The body's available size (computed from the body's parent)
  nsSize availSize;

  // Flags for whether the max size is unconstrained
  PRBool  unconstrainedWidth;
  PRBool  unconstrainedHeight;

  // Running y-offset
  nscoord y;

  // Pointer to the footer in the table
  nsIFrame* footerFrame;

  // The first body section row group frame, i.e. not a header or footer
  nsIFrame* firstBodySection;

  // border/padding
  nsMargin  mBorderPadding;

  InnerTableReflowState(nsIPresContext*          aPresContext,
                        const nsHTMLReflowState& aReflowState,
                        const nsMargin&          aBorderPadding)
    : reflowState(aReflowState), mBorderPadding(aBorderPadding)
  {
    y=0;  // border/padding???

    unconstrainedWidth = PRBool(aReflowState.availableWidth == NS_UNCONSTRAINEDSIZE);
    unconstrainedHeight = PRBool(aReflowState.availableHeight == NS_UNCONSTRAINEDSIZE);

    availSize.width  = aReflowState.availableWidth;
    if (!unconstrainedWidth) {
      availSize.width -= aBorderPadding.left + aBorderPadding.right;
    }

    availSize.height = aReflowState.availableHeight;
    if (!unconstrainedHeight) {
      availSize.height -= aBorderPadding.top + aBorderPadding.bottom;
    }

    footerFrame = nsnull;
    firstBodySection = nsnull;
  }
};

const nsHTMLReflowState*
GetGrandParentReflowState(const nsHTMLReflowState& aInnerRS)
{
  const nsHTMLReflowState* rs = nsnull;
  if (aInnerRS.parentReflowState) {
    rs = aInnerRS.parentReflowState->parentReflowState;  
  }
  return rs;
}


NS_IMETHODIMP
nsTableFrame::GetFrameType(nsIAtom** aType) const
{
  NS_PRECONDITION(nsnull != aType, "null OUT parameter pointer");
  *aType = nsLayoutAtoms::tableFrame; 
  NS_ADDREF(*aType);
  return NS_OK;
}


/* --------------------- nsTableFrame -------------------- */

nsTableFrame::nsTableFrame()
  : nsHTMLContainerFrame(),
    mCellMap(nsnull),
    mTableLayoutStrategy(nsnull),
    mPercentBasisForRows(0),
    mPreferredWidth(0)
{
  mBits.mColumnWidthsSet   = PR_FALSE;
  mBits.mColumnWidthsValid = PR_FALSE;
  mBits.mFirstPassValid    = PR_FALSE;
  mBits.mIsInvariantWidth  = PR_FALSE;
  mBits.mMaximumWidthValid = PR_FALSE;
  mBits.mCellSpansPctCol   = PR_FALSE;
  // XXX We really shouldn't do this, but if we don't then we'll have a
  // problem with the tree control...
#if 0
  mColumnWidthsLength = 0;  
  mColumnWidths = nsnull;
#else
  mColumnWidthsLength = 5;  
  mColumnWidths = new PRInt32[mColumnWidthsLength];
  nsCRT::memset (mColumnWidths, 0, mColumnWidthsLength*sizeof(PRInt32));
#endif
}

NS_IMPL_ADDREF_INHERITED(nsTableFrame, nsHTMLContainerFrame)
NS_IMPL_RELEASE_INHERITED(nsTableFrame, nsHTMLContainerFrame)

nsresult nsTableFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(NS_GET_IID(nsITableLayout))) 
  { // note there is no addref here, frames are not addref'd
    *aInstancePtr = (void*)(nsITableLayout*)this;
    return NS_OK;
  }
  else {
    return nsHTMLContainerFrame::QueryInterface(aIID, aInstancePtr);
  }
}

NS_IMETHODIMP
nsTableFrame::Init(nsIPresContext*  aPresContext,
                   nsIContent*      aContent,
                   nsIFrame*        aParent,
                   nsIStyleContext* aContext,
                   nsIFrame*        aPrevInFlow)
{
  nsresult  rv;

  // Create the cell map
  // XXX Why do we do this for continuing frames?
  mCellMap = new nsTableCellMap(aPresContext, *this);

  // Let the base class do its processing
  rv = nsHTMLContainerFrame::Init(aPresContext, aContent, aParent, aContext,
                                  aPrevInFlow);

  if (aPrevInFlow) {
    // set my width, because all frames in a table flow are the same width and
    // code in nsTableOuterFrame depends on this being set
    nsSize  size;
    aPrevInFlow->GetSize(size);
    mRect.width = size.width;
  }

  if ((NS_STYLE_BORDER_COLLAPSE == GetBorderCollapseStyle()) && !mBorderCollapser) {
    mBorderCollapser = new nsTableBorderCollapser(*this);
    // if new fails then we don't get collapsing borders
  }

  return rv;
}


nsTableFrame::~nsTableFrame()
{
  if ((NS_STYLE_BORDER_COLLAPSE == GetBorderCollapseStyle()) && mBorderCollapser) {
    delete mBorderCollapser;
  }

  if (nsnull!=mCellMap) {
    delete mCellMap; 
    mCellMap = nsnull;
  } 

  if (nsnull!=mColumnWidths) {
    delete [] mColumnWidths;
    mColumnWidths = nsnull;
  }

  if (nsnull!=mTableLayoutStrategy) {
    delete mTableLayoutStrategy;
    mTableLayoutStrategy = nsnull;
  }
}

NS_IMETHODIMP
nsTableFrame::Destroy(nsIPresContext* aPresContext)
{
  mColGroups.DestroyFrames(aPresContext);
  return nsHTMLContainerFrame::Destroy(aPresContext);
}

nscoord 
nsTableFrame::RoundToPixel(nscoord aValue,
                           float   aPixelToTwips)
{
  nscoord halfPixel = NSToCoordRound(aPixelToTwips / 2.0f);
  nscoord fullPixel = NSToCoordRound(aPixelToTwips);
  PRInt32 excess = aValue % fullPixel;
  if (0 == excess) {
    return aValue;
  }
  else {
    if (excess > halfPixel) {
      return aValue + (fullPixel - excess);
    }
    else {
      return aValue - excess;
    }
  }
}

// Helper function. It marks the table frame as dirty and generates
// a reflow command
nsresult
nsTableFrame::AddTableDirtyReflowCommand(nsIPresContext* aPresContext,
                                         nsIFrame*       aTableFrame)
{
  nsFrameState      frameState;
  nsIFrame*         tableParentFrame;
  nsIReflowCommand* reflowCmd;
  nsresult          rv;
  nsIPresShell*     presShell;

  aPresContext->GetShell(&presShell);

  // Mark the table frame as dirty
  aTableFrame->GetFrameState(&frameState);
  frameState |= NS_FRAME_IS_DIRTY;
  aTableFrame->SetFrameState(frameState);

  // Target the reflow comamnd at its parent frame
  aTableFrame->GetParent(&tableParentFrame);
  rv = NS_NewHTMLReflowCommand(&reflowCmd, tableParentFrame,
                               nsIReflowCommand::ReflowDirty);
  if (NS_SUCCEEDED(rv)) {
    // Add the reflow command
    rv = presShell->AppendReflowCommand(reflowCmd);
    NS_RELEASE(reflowCmd);
  }

  NS_RELEASE(presShell);
  return rv;
}

// XXX this needs to be cleaned up so that the frame constructor breaks out col group
// frames into a separate child list.
NS_IMETHODIMP
nsTableFrame::SetInitialChildList(nsIPresContext* aPresContext,
                                  nsIAtom*        aListName,
                                  nsIFrame*       aChildList)
{
  nsresult rv=NS_OK;

  // I know now that I have all my children, so build the cell map
  nsIFrame *childFrame = aChildList;
  nsIFrame *prevMainChild = nsnull;
  nsIFrame *prevColGroupChild = nsnull;
  for ( ; nsnull!=childFrame; )
  {
    const nsStyleDisplay *childDisplay;
    childFrame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct *&)childDisplay);
    // XXX this if should go away
    if (PR_TRUE==IsRowGroup(childDisplay->mDisplay))
    {
      if (mFrames.IsEmpty()) 
        mFrames.SetFrames(childFrame);
      else
        prevMainChild->SetNextSibling(childFrame);
      prevMainChild = childFrame;
    }
    else if (NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP == childDisplay->mDisplay)
    {
      if (mColGroups.IsEmpty())
        mColGroups.SetFrames(childFrame);
      else
        prevColGroupChild->SetNextSibling(childFrame);
      prevColGroupChild = childFrame;
    }
    else
    { // unknown frames go on the main list for now
      if (mFrames.IsEmpty())
        mFrames.SetFrames(childFrame);
      else
        prevMainChild->SetNextSibling(childFrame);
      prevMainChild = childFrame;
    }
    nsIFrame *prevChild = childFrame;
    childFrame->GetNextSibling(&childFrame);
    prevChild->SetNextSibling(nsnull);
  }
  if (nsnull!=prevMainChild)
    prevMainChild->SetNextSibling(nsnull);
  if (nsnull!=prevColGroupChild)
    prevColGroupChild->SetNextSibling(nsnull);

  // If we have a prev-in-flow, then we're a table that has been split and
  // so don't treat this like an append
  if (!mPrevInFlow) {
    // process col groups first so that real cols get constructed before
    // anonymous ones due to cells in rows.
    InsertColGroups(*aPresContext, 0, mColGroups.FirstChild());
    AppendRowGroups(*aPresContext, mFrames.FirstChild());
  }

  if (HasGroupRules()) {
    ProcessGroupRules(aPresContext);
  }

  return rv;
}

NS_IMETHODIMP
nsTableFrame::IsPercentageBase(PRBool& aBase) const
{
  aBase = PR_TRUE;
  return NS_OK;
}

void nsTableFrame::AttributeChangedFor(nsIPresContext* aPresContext, 
                                       nsIFrame*       aFrame,
                                       nsIContent*     aContent, 
                                       nsIAtom*        aAttribute)
{
  nsIAtom* frameType;
  aFrame->GetFrameType(&frameType);
  if (nsLayoutAtoms::tableCellFrame == frameType) {
    if ((nsHTMLAtoms::rowspan == aAttribute) || 
        (nsHTMLAtoms::colspan == aAttribute)) {
      nsTableCellMap* cellMap = GetCellMap();
      if (cellMap) {
        // for now just remove the cell from the map and reinsert it
        nsTableCellFrame* cellFrame = (nsTableCellFrame*)aFrame;
        PRInt32 rowIndex, colIndex;
        cellFrame->GetRowIndex(rowIndex);
        cellFrame->GetColIndex(colIndex);
        RemoveCell(*aPresContext, cellFrame, rowIndex);
        nsVoidArray cells;
        cells.AppendElement(cellFrame);
        InsertCells(*aPresContext, cells, rowIndex, colIndex - 1);
        // invalidate the column widths and generate a reflow command
        InvalidateColumnWidths(); 
        AddTableDirtyReflowCommand(aPresContext, this);
      }
    }
  }
  NS_IF_RELEASE(frameType);
}


/* ****** CellMap methods ******* */

/* counts columns in column groups */
PRInt32 nsTableFrame::GetSpecifiedColumnCount ()
{
  PRInt32 colCount = 0;
  nsIFrame * childFrame = mColGroups.FirstChild();
  while (nsnull!=childFrame) {
    colCount += ((nsTableColGroupFrame *)childFrame)->GetColCount();
    childFrame->GetNextSibling(&childFrame);
  }    
  return colCount;
}

PRInt32 nsTableFrame::GetRowCount () const
{
  PRInt32 rowCount = 0;
  nsTableCellMap *cellMap = GetCellMap();
  NS_ASSERTION(nsnull!=cellMap, "GetRowCount null cellmap");
  if (nsnull!=cellMap)
    rowCount = cellMap->GetRowCount();
  return rowCount;
}

/* return the effective col count */
PRInt32 nsTableFrame::GetColCount ()
{
  PRInt32 colCount = 0;
  nsTableCellMap* cellMap = GetCellMap();
  NS_ASSERTION(nsnull != cellMap, "GetColCount null cellmap");
  if (nsnull != cellMap)
    colCount = cellMap->GetColCount();
  return colCount;
}


nsTableColFrame* nsTableFrame::GetColFrame(PRInt32 aColIndex)
{
  return (nsTableColFrame *)mColFrames.ElementAt(aColIndex);
}

// can return nsnull
nsTableCellFrame* nsTableFrame::GetCellFrameAt(PRInt32 aRowIndex, PRInt32 aColIndex)
{
  nsTableCellMap* cellMap = GetCellMap();
  if (cellMap) 
    return cellMap->GetCellInfoAt(aRowIndex, aColIndex);
  return nsnull;
}


PRInt32 nsTableFrame::GetEffectiveRowSpan(PRInt32                 aRowIndex,
                                          const nsTableCellFrame& aCell) const
{
  nsTableCellMap* cellMap = GetCellMap();
  NS_PRECONDITION (nsnull != cellMap, "bad call, cellMap not yet allocated.");

  PRInt32 colIndex;
  aCell.GetColIndex(colIndex);
  return cellMap->GetEffectiveRowSpan(aRowIndex, colIndex);
}

PRInt32 nsTableFrame::GetEffectiveRowSpan(const nsTableCellFrame& aCell) const
{
  nsTableCellMap* cellMap = GetCellMap();
  NS_PRECONDITION (nsnull != cellMap, "bad call, cellMap not yet allocated.");

  PRInt32 colIndex, rowIndex;
  aCell.GetColIndex(colIndex);
  aCell.GetRowIndex(rowIndex);
  return cellMap->GetEffectiveRowSpan(rowIndex, colIndex);
}

PRInt32 nsTableFrame::GetEffectiveColSpan(const nsTableCellFrame& aCell) const
{
  nsTableCellMap* cellMap = GetCellMap();
  NS_PRECONDITION (nsnull != cellMap, "bad call, cellMap not yet allocated.");

  PRInt32 colIndex, rowIndex;
  aCell.GetColIndex(colIndex);
  aCell.GetRowIndex(rowIndex);
  return cellMap->GetEffectiveColSpan(rowIndex, colIndex);
}

PRInt32 nsTableFrame::GetEffectiveCOLSAttribute()
{
  nsTableCellMap *cellMap = GetCellMap();
  NS_PRECONDITION (nsnull!=cellMap, "null cellMap.");
  PRInt32 result;
  const nsStyleTable *tableStyle=nsnull;
  GetStyleData(eStyleStruct_Table, (const nsStyleStruct *&)tableStyle);
  result = tableStyle->mCols;
  PRInt32 numCols = GetColCount();
  if (result>numCols)
    result = numCols;
  return result;
}

PRBool nsTableFrame::HasGroupRules() const
{
  const nsStyleTable* tableStyle = nsnull;
  GetStyleData(eStyleStruct_Table, (const nsStyleStruct *&)tableStyle);
  if (NS_STYLE_TABLE_RULES_GROUPS == tableStyle->mRules) { 
    return PR_TRUE;
  }
  return PR_FALSE;
}

// this won't work until bug 12948 is resolved and col groups are considered 
void nsTableFrame::ProcessGroupRules(nsIPresContext* aPresContext)
{
  PRInt32 numCols = GetColCount();

  // process row groups
  nsIFrame* iFrame;
  for (iFrame = mFrames.FirstChild(); iFrame; iFrame->GetNextSibling(&iFrame)) {
    nsIAtom* frameType;
    iFrame->GetFrameType(&frameType);
    if (nsLayoutAtoms::tableRowGroupFrame == frameType) {
      nsTableRowGroupFrame* rgFrame = (nsTableRowGroupFrame *)iFrame;
      PRInt32 startRow = rgFrame->GetStartRowIndex();
      PRInt32 numGroupRows;
      rgFrame->GetRowCount(numGroupRows, PR_FALSE);
      PRInt32 endRow = startRow + numGroupRows - 1;
      if (startRow == endRow) {
        continue;
      }
      for (PRInt32 rowX = startRow; rowX <= endRow; rowX++) {
        for (PRInt32 colX = 0; colX < numCols; colX++) {
          PRBool originates;
          nsTableCellFrame* cell = GetCellInfoAt(rowX, colX, &originates);
          if (originates) {
            nsCOMPtr<nsIStyleContext> styleContext;
            cell->GetStyleContext(getter_AddRefs(styleContext));
            nsStyleSpacing* spacing = (nsStyleSpacing*)styleContext->GetMutableStyleData(eStyleStruct_Spacing);
            if (rowX == startRow) { 
              spacing->SetBorderStyle(NS_SIDE_BOTTOM, NS_STYLE_BORDER_STYLE_NONE);
            }
            else if (rowX == endRow) { 
              spacing->SetBorderStyle(NS_SIDE_TOP, NS_STYLE_BORDER_STYLE_NONE);
            }
            else {
              spacing->SetBorderStyle(NS_SIDE_TOP, NS_STYLE_BORDER_STYLE_NONE);
              spacing->SetBorderStyle(NS_SIDE_BOTTOM, NS_STYLE_BORDER_STYLE_NONE);
            }
            styleContext->RecalcAutomaticData(aPresContext);
          }
        }
      }
    }
    NS_IF_RELEASE(frameType);
  }
}


void nsTableFrame::AdjustRowIndices(nsIPresContext* aPresContext,
                                    PRInt32         aRowIndex,
                                    PRInt32         aAdjustment)
{
  // Iterate over the row groups and adjust the row indices of all rows 
  // whose index is >= aRowIndex.
  nsVoidArray rowGroups;
  PRUint32 numRowGroups;
  OrderRowGroups(rowGroups, numRowGroups, nsnull);

  for (PRUint32 rgX = 0; rgX < numRowGroups; rgX++) {
    nsIFrame* kidFrame = (nsIFrame*)rowGroups.ElementAt(rgX);
    nsTableRowGroupFrame* rgFrame = GetRowGroupFrame(kidFrame);
    AdjustRowIndices(aPresContext, rgFrame, aRowIndex, aAdjustment);
  }
}

NS_IMETHODIMP nsTableFrame::AdjustRowIndices(nsIPresContext* aPresContext,
                                             nsIFrame*       aRowGroup, 
                                             PRInt32         aRowIndex,
                                             PRInt32         anAdjustment)
{
  nsresult rv = NS_OK;
  nsIFrame *rowFrame;
  aRowGroup->FirstChild(aPresContext, nsnull, &rowFrame);
  for ( ; nsnull!=rowFrame; rowFrame->GetNextSibling(&rowFrame))
  {
    const nsStyleDisplay *rowDisplay;
    rowFrame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct *&)rowDisplay);
    if (NS_STYLE_DISPLAY_TABLE_ROW==rowDisplay->mDisplay)
    {
      PRInt32 index = ((nsTableRowFrame*)rowFrame)->GetRowIndex();
      if (index >= aRowIndex)
        ((nsTableRowFrame *)rowFrame)->SetRowIndex(index+anAdjustment);
    }
    else if (NS_STYLE_DISPLAY_TABLE_ROW_GROUP==rowDisplay->mDisplay)
    {
      AdjustRowIndices(aPresContext, rowFrame, aRowIndex, anAdjustment);
    }
  }
  return rv;
}


void nsTableFrame::InsertColGroups(nsIPresContext& aPresContext,
                                   PRInt32         aStartColIndex,
                                   nsIFrame*       aFirstFrame,
                                   nsIFrame*       aLastFrame)
{
  PRInt32 colIndex = aStartColIndex;
  nsTableColGroupFrame* firstColGroupToReset = nsnull;
  nsIFrame* kidFrame = aFirstFrame;
  PRBool didLastFrame = PR_FALSE;
  while (kidFrame) {
    nsIAtom* kidType;
    kidFrame->GetFrameType(&kidType);
    if (nsLayoutAtoms::tableColGroupFrame == kidType) {
      if (didLastFrame) {
        firstColGroupToReset = (nsTableColGroupFrame*)kidFrame;
        break;
      }
      else {
        nsTableColGroupFrame* cgFrame = (nsTableColGroupFrame*)kidFrame;
        cgFrame->SetStartColumnIndex(colIndex);
        nsIFrame* firstCol;
        kidFrame->FirstChild(&aPresContext, nsnull, &firstCol);
        cgFrame->AddColsToTable(aPresContext, colIndex, PR_FALSE, firstCol);
        PRInt32 numCols = cgFrame->GetColCount();
        colIndex += numCols;
      }
    }
    NS_IF_RELEASE(kidType);
    if (kidFrame == aLastFrame) {
      didLastFrame = PR_TRUE;
    }
    kidFrame->GetNextSibling(&kidFrame);
  }

  if (firstColGroupToReset) {
    nsTableColGroupFrame::ResetColIndices(&aPresContext, firstColGroupToReset, aStartColIndex);
  }
}

void nsTableFrame::InsertCol(nsIPresContext&  aPresContext,
                             nsTableColFrame& aColFrame,
                             PRInt32          aColIndex)
{
  mColFrames.InsertElementAt(&aColFrame, aColIndex);
  nsTableColType insertedColType = aColFrame.GetType();
  PRInt32 numCacheCols = mColFrames.Count();
  nsTableCellMap* cellMap = GetCellMap();
  if (cellMap) {
    PRInt32 numMapCols = cellMap->GetColCount();
    if (numCacheCols > numMapCols) {
      PRBool removedFromCache = PR_FALSE;
      if (eColAnonymousCell != insertedColType) {
        nsTableColFrame* lastCol = (nsTableColFrame *)mColFrames.ElementAt(numCacheCols - 1);
        if (lastCol) {
          nsTableColType lastColType = lastCol->GetType();
          if (eColAnonymousCell == lastColType) {
            // remove the col from the cache
            mColFrames.RemoveElementAt(numCacheCols - 1);
            // remove the col from the eColGroupAnonymousCell col group
            nsTableColGroupFrame* lastColGroup = (nsTableColGroupFrame *)mColGroups.LastChild();
            if (lastColGroup) {
              lastColGroup->RemoveChild(aPresContext, *lastCol, PR_FALSE);
            }
            // remove the col group if it is empty
            if (lastColGroup->GetColCount() <= 0) {
              mColGroups.DestroyFrame(&aPresContext, (nsIFrame*)lastColGroup);
            }
            removedFromCache = PR_TRUE;
          }
        }
      }
      if (!removedFromCache) {
        cellMap->AddColsAtEnd(1);
      }
    }
  }
}

void nsTableFrame::RemoveCol(nsIPresContext&       aPresContext,
                             nsTableColGroupFrame* aColGroupFrame,
                             PRInt32               aColIndex,
                             PRBool                aRemoveFromCache,
                             PRBool                aRemoveFromCellMap)
{
  if (aRemoveFromCache) {
    mColFrames.RemoveElementAt(aColIndex);
  }
  if (aRemoveFromCellMap) {
    nsTableCellMap* cellMap = GetCellMap();
    if (cellMap) {
      // check to see if the cell map can remove the col
      if (cellMap->RemoveUnusedCols(1) < 1) { 
        // it couldn't be removed so we need a new anonymous col frame
        CreateAnonymousColFrames(aPresContext, 1, eColAnonymousCell, PR_TRUE);
      }
    }
  }
}

/** Get the cell map for this table frame.  It is not always mCellMap.
  * Only the firstInFlow has a legit cell map
  */
nsTableCellMap * nsTableFrame::GetCellMap() const
{
  nsTableFrame * firstInFlow = (nsTableFrame *)GetFirstInFlow();
  if (this!=firstInFlow)
  {
    return firstInFlow->GetCellMap();
  }
  return mCellMap;
}

// XXX this needs to be moved to nsCSSFrameConstructor
nsTableColGroupFrame*
nsTableFrame::CreateAnonymousColGroupFrame(nsIPresContext&     aPresContext,
                                           nsTableColGroupType aColGroupType)
{
  nsCOMPtr<nsIContent> colGroupContent;
  GetContent(getter_AddRefs(colGroupContent));

  nsCOMPtr<nsIStyleContext> colGroupStyle;
  aPresContext.ResolvePseudoStyleContextFor(colGroupContent,
                                            nsHTMLAtoms::tableColGroupPseudo,
                                            mStyleContext,
                                            PR_FALSE,
                                            getter_AddRefs(colGroupStyle));        
  // Create a col group frame
  nsIFrame* newFrame;
  nsCOMPtr<nsIPresShell> presShell;
  aPresContext.GetShell(getter_AddRefs(presShell));
  nsresult result = NS_NewTableColGroupFrame(presShell, &newFrame);
  if (NS_SUCCEEDED(result) && newFrame) {
    ((nsTableColGroupFrame *)newFrame)->SetType(aColGroupType);
    newFrame->Init(&aPresContext, colGroupContent, this, colGroupStyle, nsnull);
  }
  return (nsTableColGroupFrame *)newFrame;
}

void
nsTableFrame::CreateAnonymousColFrames(nsIPresContext& aPresContext,
                                       PRInt32         aNumColsToAdd,
                                       nsTableColType  aColType,
                                       PRBool          aDoAppend,
                                       nsIFrame*       aPrevColIn)
{
  // get the last col group frame
  nsTableColGroupFrame* colGroupFrame = nsnull;
  nsIFrame* childFrame = mColGroups.FirstChild();
  while (childFrame) {
    nsIAtom* frameType = nsnull;
    childFrame->GetFrameType(&frameType);
    if (nsLayoutAtoms::tableColGroupFrame == frameType) {
      colGroupFrame = (nsTableColGroupFrame *)childFrame;
    }
    childFrame->GetNextSibling(&childFrame);
    NS_IF_RELEASE(frameType);
  }

  nsTableColGroupType lastColGroupType = eColGroupContent; 
  nsTableColGroupType newColGroupType  = eColGroupContent; 
  if (colGroupFrame) {
    lastColGroupType = colGroupFrame->GetType();
  }
  if (eColAnonymousCell == aColType) {
    if (eColGroupAnonymousCell != lastColGroupType) {
      newColGroupType = eColGroupAnonymousCell;
    }
  }
  else if (eColAnonymousCol == aColType) {
    if (eColGroupAnonymousCol != lastColGroupType) {
      newColGroupType = eColGroupAnonymousCol;
    }
  }
  else {
    NS_ASSERTION(PR_FALSE, "CreateAnonymousColFrames called incorrectly");
    return;
  }

  if (eColGroupContent != newColGroupType) {
    PRInt32 colIndex = (colGroupFrame) ? colGroupFrame->GetStartColumnIndex() + colGroupFrame->GetColCount()
                                       : 0;
    colGroupFrame = CreateAnonymousColGroupFrame(aPresContext, newColGroupType);
    if (!colGroupFrame) {
      return;
    }
    mColGroups.AppendFrame(this, colGroupFrame); // add the new frame to the child list
    colGroupFrame->SetStartColumnIndex(colIndex);
  }

  nsIFrame* prevCol = (aDoAppend) ? colGroupFrame->GetChildList().LastChild() : aPrevColIn;

  nsIFrame* firstNewFrame;
  CreateAnonymousColFrames(aPresContext, *colGroupFrame, aNumColsToAdd, 
                           aColType, PR_TRUE, prevCol, &firstNewFrame);
}

// XXX this needs to be moved to nsCSSFrameConstructor
// Right now it only creates the col frames at the end 
void
nsTableFrame::CreateAnonymousColFrames(nsIPresContext&       aPresContext,
                                       nsTableColGroupFrame& aColGroupFrame,
                                       PRInt32               aNumColsToAdd,
                                       nsTableColType        aColType,
                                       PRBool                aAddToColGroupAndTable,         
                                       nsIFrame*             aPrevFrameIn,
                                       nsIFrame**            aFirstNewFrame)
{
  *aFirstNewFrame = nsnull;
  nsIFrame* lastColFrame = nsnull;
  nsIFrame* childFrame;

  // Get the last col frame
  aColGroupFrame.FirstChild(&aPresContext, nsnull, &childFrame);
  while (childFrame) {
    nsIAtom* frameType = nsnull;
    childFrame->GetFrameType(&frameType);
    if (nsLayoutAtoms::tableColFrame == frameType) {
      lastColFrame = (nsTableColGroupFrame *)childFrame;
    }
    NS_IF_RELEASE(frameType);
    childFrame->GetNextSibling(&childFrame);
  }

  PRInt32 startIndex = mColFrames.Count();
  PRInt32 lastIndex  = startIndex + aNumColsToAdd - 1; 

  for (PRInt32 childX = startIndex; childX <= lastIndex; childX++) {
    nsCOMPtr<nsIContent> iContent;
    nsCOMPtr<nsIStyleContext> styleContext;
    nsCOMPtr<nsIStyleContext> parentStyleContext;

    if ((aColType == eColAnonymousCol) && aPrevFrameIn) {
      // a col due to a span in a previous col uses the style context of the col
      aPrevFrameIn->GetStyleContext(getter_AddRefs(styleContext)); 
      // fix for bugzilla bug 54454: get the content from the prevFrame 
      aPrevFrameIn->GetContent(getter_AddRefs(iContent));
    }
    else {
      // all other anonymous cols use a pseudo style context of the col group
      aColGroupFrame.GetContent(getter_AddRefs(iContent));
      aColGroupFrame.GetStyleContext(getter_AddRefs(parentStyleContext));
      aPresContext.ResolvePseudoStyleContextFor(iContent, nsHTMLAtoms::tableColPseudo,
                                                parentStyleContext, PR_FALSE, getter_AddRefs(styleContext));
    }
    // ASSERTION to check for bug 54454 sneaking back in...
    NS_ASSERTION(iContent, "null content in CreateAnonymousColFrames");

    // create the new col frame
    nsIFrame* colFrame;
    nsCOMPtr<nsIPresShell> presShell;
    aPresContext.GetShell(getter_AddRefs(presShell));
    NS_NewTableColFrame(presShell, &colFrame);
    ((nsTableColFrame *) colFrame)->SetType(aColType);
    colFrame->Init(&aPresContext, iContent, &aColGroupFrame,
                   styleContext, nsnull);
    colFrame->SetInitialChildList(&aPresContext, nsnull, nsnull);
    ((nsTableColFrame *)colFrame)->SetType(aColType);

    // Add the col to the sibling chain
    if (lastColFrame) {
      lastColFrame->SetNextSibling(colFrame);
    }
    lastColFrame = colFrame;
    if (childX == startIndex) {
      *aFirstNewFrame = colFrame;
    }
  }
  if (aAddToColGroupAndTable) {
    nsFrameList& cols = aColGroupFrame.GetChildList();
    // the chain already exists, now add it to the col group child list
    if (!aPrevFrameIn) {
      cols.AppendFrames(&aColGroupFrame, *aFirstNewFrame);
    }
    // get the starting col index in the cache
    PRInt32 startColIndex = aColGroupFrame.GetStartColumnIndex();
    if (aPrevFrameIn) {
      nsTableColFrame* colFrame = 
        (nsTableColFrame*)nsTableFrame::GetFrameAtOrBefore(&aPresContext,
                                                           (nsIFrame*)&aColGroupFrame, aPrevFrameIn, 
                                                           nsLayoutAtoms::tableColFrame);
      if (colFrame) {
        startColIndex = colFrame->GetColIndex();
      }
    }
    aColGroupFrame.AddColsToTable(aPresContext, startColIndex, PR_TRUE, 
                                  *aFirstNewFrame, lastColFrame);
  }
}

PRInt32 nsTableFrame::AppendCell(nsIPresContext&   aPresContext,
                                 nsTableCellFrame& aCellFrame,
                                 PRInt32           aRowIndex)
{
  PRInt32 colIndex = 0;
  nsTableCellMap* cellMap = GetCellMap();
  if (cellMap) {
    colIndex = cellMap->AppendCell(aCellFrame, aRowIndex, PR_TRUE);
    PRInt32 numColsInMap   = GetColCount();
    PRInt32 numColsInCache = mColFrames.Count();
    PRInt32 numColsToAdd = numColsInMap - numColsInCache;
    if (numColsToAdd > 0) {
      // this sets the child list, updates the col cache and cell map
      CreateAnonymousColFrames(aPresContext, numColsToAdd, eColAnonymousCell, PR_TRUE); 
    }
  }
  return colIndex;
}

void nsTableFrame::InsertCells(nsIPresContext& aPresContext,
                               nsVoidArray&    aCellFrames, 
                               PRInt32         aRowIndex, 
                               PRInt32         aColIndexBefore)
{
  nsTableCellMap* cellMap = GetCellMap();
  if (cellMap) {
    cellMap->InsertCells(aCellFrames, aRowIndex, aColIndexBefore);
    PRInt32 numColsInMap = GetColCount();
    PRInt32 numColsInCache = mColFrames.Count();
    PRInt32 numColsToAdd = numColsInMap - numColsInCache;
    if (numColsToAdd > 0) {
      // this sets the child list, updates the col cache and cell map
      CreateAnonymousColFrames(aPresContext, numColsToAdd, eColAnonymousCell, PR_TRUE);
    }
  }
}

// this removes the frames from the col group and table, but not the cell map
PRInt32 
nsTableFrame::DestroyAnonymousColFrames(nsIPresContext& aPresContext,
                                        PRInt32 aNumFrames)
{
  // only remove cols that are of type eTypeAnonymous cell (they are at the end)
  PRInt32 endIndex   = mColFrames.Count() - 1;
  PRInt32 startIndex = (endIndex - aNumFrames) + 1;
  PRInt32 numColsRemoved = 0;
  for (PRInt32 colX = endIndex; colX >= startIndex; colX--) {
    nsTableColFrame* colFrame = GetColFrame(colX);
    if (colFrame && (eColAnonymousCell == colFrame->GetType())) {
      nsTableColGroupFrame* cgFrame;
      colFrame->GetParent((nsIFrame**)&cgFrame);
      // remove the frame from the colgroup
      cgFrame->RemoveChild(aPresContext, *colFrame, PR_FALSE);
      // remove the frame from the cache, but not the cell map 
      RemoveCol(aPresContext, nsnull, colX, PR_TRUE, PR_FALSE);
      numColsRemoved++;
    }
    else {
      break; 
    }
  }
  return (aNumFrames - numColsRemoved);
}

void nsTableFrame::RemoveCell(nsIPresContext&   aPresContext,
                              nsTableCellFrame* aCellFrame,
                              PRInt32           aRowIndex)
{
  nsTableCellMap* cellMap = GetCellMap();
  if (cellMap) {
    cellMap->RemoveCell(aCellFrame, aRowIndex);
    PRInt32 numColsInMap = GetColCount(); // cell map's notion of num cols
    PRInt32 numColsInCache = mColFrames.Count();
    PRInt32 numColsNotRemoved = DestroyAnonymousColFrames(aPresContext, numColsInCache - numColsInMap);
    // if the cell map has fewer cols than the cache, correct it
    if (numColsNotRemoved > 0) {
      cellMap->AddColsAtEnd(numColsNotRemoved);
    }
  }
}

PRInt32
nsTableFrame::GetStartRowIndex(nsTableRowGroupFrame& aRowGroupFrame)
{
  nsVoidArray orderedRowGroups;
  PRUint32 numRowGroups;
  OrderRowGroups(orderedRowGroups, numRowGroups);

  PRInt32 rowIndex = 0;
  for (PRUint32 rgIndex = 0; rgIndex < numRowGroups; rgIndex++) {
    nsTableRowGroupFrame* rgFrame = GetRowGroupFrame((nsIFrame*)orderedRowGroups.ElementAt(rgIndex));
    if (rgFrame == &aRowGroupFrame) {
      break;
    }
    PRInt32 numRows;
    rgFrame->GetRowCount(numRows);
    rowIndex += numRows;
  }
  return rowIndex;
}

// this cannot extend beyond a single row group
void nsTableFrame::AppendRows(nsIPresContext&       aPresContext,
                              nsTableRowGroupFrame& aRowGroupFrame,
                              PRInt32               aRowIndex,
                              nsVoidArray&          aRowFrames)
{
  nsTableCellMap* cellMap = GetCellMap();
  if (cellMap) {
    PRInt32 absRowIndex = GetStartRowIndex(aRowGroupFrame) + aRowIndex;
    InsertRows(aPresContext, aRowGroupFrame, aRowFrames, absRowIndex, PR_TRUE);
  }
}

PRInt32
nsTableFrame::InsertRow(nsIPresContext&       aPresContext,
                        nsTableRowGroupFrame& aRowGroupFrame,
                        nsIFrame&             aRowFrame,
                        PRInt32               aRowIndex,
                        PRBool                aConsiderSpans)
{
  nsVoidArray rows;
  rows.AppendElement(&aRowFrame);
  return InsertRows(aPresContext, aRowGroupFrame, rows, aRowIndex, aConsiderSpans);
}

// this cannot extend beyond a single row group
PRInt32
nsTableFrame::InsertRows(nsIPresContext&       aPresContext,
                         nsTableRowGroupFrame& aRowGroupFrame,
                         nsVoidArray&          aRowFrames,
                         PRInt32               aRowIndex,
                         PRBool                aConsiderSpans)
{
  //printf("insertRowsBefore firstRow=%d \n", aRowIndex);
  //Dump(PR_TRUE, PR_FALSE, PR_TRUE);

  PRInt32 numColsToAdd = 0;
  nsTableCellMap* cellMap = GetCellMap();
  if (cellMap) {
    PRInt32 origNumRows = cellMap->GetRowCount();
    PRInt32 numNewRows = aRowFrames.Count();
    cellMap->InsertRows(&aPresContext, aRowGroupFrame, aRowFrames, aRowIndex, aConsiderSpans);
    PRInt32 numColsInMap = GetColCount(); // cell map's notion of num cols
    PRInt32 numColsInCache = mColFrames.Count();
    numColsToAdd = numColsInMap - numColsInCache;
    if (numColsToAdd > 0) {
      // this sets the child list, updates the col cache and cell map
      CreateAnonymousColFrames(aPresContext, numColsToAdd, eColAnonymousCell, 
                               PR_TRUE);
    }
    if (aRowIndex < origNumRows) {
      AdjustRowIndices(&aPresContext, aRowIndex, numNewRows);
    }
    // assign the correct row indices to the new rows. If they were adjusted above
    // it may not have been done correctly because each row is constructed with index 0
    for (PRInt32 rowX = 0; rowX < numNewRows; rowX++) {
      nsTableRowFrame* rowFrame = (nsTableRowFrame *) aRowFrames.ElementAt(rowX);
      rowFrame->SetRowIndex(aRowIndex + rowX);
    }
  }

  //printf("insertRowsAfter \n");
  //Dump(PR_TRUE, PR_FALSE, PR_TRUE);

  return numColsToAdd;
}

// this cannot extend beyond a single row group
void nsTableFrame::RemoveRows(nsIPresContext&  aPresContext,
                              PRInt32          aFirstRowIndex,
                              PRInt32          aNumRowsToRemove,
                              PRBool           aConsiderSpans)
{
  //printf("removeRowsBefore firstRow=%d numRows=%d\n", aFirstRowIndex, aNumRowsToRemove);
  //Dump(PR_TRUE, PR_FALSE, PR_TRUE);

  nsTableCellMap* cellMap = GetCellMap();
  if (cellMap) {
    cellMap->RemoveRows(&aPresContext, aFirstRowIndex, aNumRowsToRemove, aConsiderSpans);
    // only remove cols that are of type eTypeAnonymous cell (they are at the end)
    PRInt32 numColsInMap = GetColCount(); // cell map's notion of num cols
    PRInt32 numColsInCache = mColFrames.Count();
    PRInt32 numColsNotRemoved = DestroyAnonymousColFrames(aPresContext, numColsInCache - numColsInMap);
    // if the cell map has fewer cols than the cache, correct it
    if (numColsNotRemoved > 0) {
      cellMap->AddColsAtEnd(numColsNotRemoved);
    }
  }
  AdjustRowIndices(&aPresContext, aFirstRowIndex, -aNumRowsToRemove);
  //printf("removeRowsAfter\n");
  //Dump(PR_TRUE, PR_FALSE, PR_TRUE);
}

void nsTableFrame::AppendRowGroups(nsIPresContext& aPresContext,
                                   nsIFrame*       aFirstRowGroupFrame)
{
  if (aFirstRowGroupFrame) {
    nsTableCellMap* cellMap = GetCellMap();
    if (cellMap) {
      nsFrameList newList(aFirstRowGroupFrame);
      InsertRowGroups(aPresContext, aFirstRowGroupFrame, newList.LastChild());
    }
  }
}

nsTableRowGroupFrame*
nsTableFrame::GetRowGroupFrame(nsIFrame* aFrame,
                               nsIAtom*  aFrameTypeIn)
{
  nsIFrame* rgFrame = nsnull;
  nsIAtom* frameType = aFrameTypeIn;
  if (!aFrameTypeIn) {
    aFrame->GetFrameType(&frameType);
  }
  if (nsLayoutAtoms::tableRowGroupFrame == frameType) {
    rgFrame = aFrame;
  }
  else if (nsLayoutAtoms::scrollFrame == frameType) {
    nsIScrollableFrame* scrollable = nsnull;
    nsresult rv = aFrame->QueryInterface(kIScrollableFrameIID, (void **)&scrollable);
    if (NS_SUCCEEDED(rv) && (scrollable)) {
      nsIFrame* scrolledFrame;
      scrollable->GetScrolledFrame(nsnull, scrolledFrame);
      if (scrolledFrame) {
        nsIAtom* scrolledType;
        scrolledFrame->GetFrameType(&scrolledType);
        if (nsLayoutAtoms::tableRowGroupFrame == scrolledType) {
          rgFrame = scrolledFrame;
        }
        NS_IF_RELEASE(scrolledType);
      }
    }
  }
  if (!aFrameTypeIn) {
    NS_IF_RELEASE(frameType);
  }
  return (nsTableRowGroupFrame*)rgFrame;
}

// collect the rows ancestors of aFrame
PRInt32
nsTableFrame::CollectRows(nsIPresContext* aPresContext,
                          nsIFrame*       aFrame,
                          nsVoidArray&    aCollection)
{
  if (!aFrame) return 0;
  PRInt32 numRows = 0;
  nsTableRowGroupFrame* rgFrame = GetRowGroupFrame(aFrame);
  if (rgFrame) {
    nsIFrame* childFrame = nsnull;
    rgFrame->FirstChild(aPresContext, nsnull, &childFrame);
    while (childFrame) {
      nsIAtom* childType;
      childFrame->GetFrameType(&childType);
      if (nsLayoutAtoms::tableRowFrame == childType) {
        aCollection.AppendElement(childFrame);
        numRows++;
      }
      else {
        numRows += CollectRows(aPresContext, childFrame, aCollection);
      }
      NS_IF_RELEASE(childType);
      childFrame->GetNextSibling(&childFrame);
    }
  }
  return numRows;
}

void
nsTableFrame::InsertRowGroups(nsIPresContext&  aPresContext,
                              nsIFrame*        aFirstRowGroupFrame,
                              nsIFrame*        aLastRowGroupFrame)
{
  nsTableCellMap* cellMap = GetCellMap();
  if (cellMap) {
    nsVoidArray orderedRowGroups;
    PRUint32 numRowGroups;
    OrderRowGroups(orderedRowGroups, numRowGroups);

    nsVoidArray rows;
    for (nsIFrame* kidFrame = aFirstRowGroupFrame; kidFrame; kidFrame->GetNextSibling(&kidFrame)) {
      nsTableRowGroupFrame* rgFrame = GetRowGroupFrame(kidFrame);
      if (rgFrame) {
        // get the prior row group in display order
        PRUint32 rgIndex;
        for (rgIndex = 0; rgIndex < numRowGroups; rgIndex++) {
          if (GetRowGroupFrame((nsIFrame*)orderedRowGroups.ElementAt(rgIndex)) == rgFrame) {
            break;
          }
        }
        nsTableRowGroupFrame* priorRG = (0 == rgIndex) 
          ? nsnull : GetRowGroupFrame((nsIFrame*)orderedRowGroups.ElementAt(rgIndex - 1));
          
        // create and add the cell map for the row group
        cellMap->InsertGroupCellMap(*rgFrame, priorRG);
        // collect the new row frames in an array and add them to the table
        PRInt32 numRows = CollectRows(&aPresContext, kidFrame, rows);
        if (numRows > 0) {
          PRInt32 rowIndex = 0;
          if (priorRG) {
            PRInt32 priorNumRows;
            priorRG->GetRowCount(priorNumRows);
            rowIndex = priorRG->GetStartRowIndex() + priorNumRows;
          }
          InsertRows(aPresContext, *rgFrame, rows, rowIndex, PR_TRUE);
          rows.Clear();
        }
      }
      if (kidFrame == aLastRowGroupFrame) {
        break;
      }
    }
  }
}

/* ***** Column Layout Data methods ***** */

/*
 * Lists the column layout data which turns
 * around and lists the cell layout data.
 * This is for debugging purposes only.
 */
#ifdef NS_DEBUG
void nsTableFrame::ListColumnLayoutData(FILE* out, PRInt32 aIndent) 
{
  nsTableFrame * firstInFlow = (nsTableFrame *)GetFirstInFlow();
  if (this!=firstInFlow)
  {
    firstInFlow->ListColumnLayoutData(out, aIndent);
    return;
  }

  nsTableCellMap *cellMap = GetCellMap();
  if (nsnull!=cellMap)
  {
    fprintf(out,"Column Layout Data \n");
    
    PRInt32 numCols = cellMap->GetColCount();
    PRInt32 numRows = cellMap->GetRowCount();
    for (PRInt32 colIndex = 0; colIndex<numCols; colIndex++)
    {
      for (PRInt32 indent = aIndent; --indent >= 0; ) 
        fputs("  ", out);
      fprintf(out,"Column Data [%d] \n",colIndex);
      for (PRInt32 rowIndex = 0; rowIndex < numRows; rowIndex++)
      {
//        nsTableCellFrame* cellFrame = cellMap->GetCellInfoAt(rowIndex, colIndex);
        PRInt32 rowIndent;
        for (rowIndent = aIndent+2; --rowIndent >= 0; ) fputs("  ", out);
        fprintf(out,"Cell Data [%d] \n",rowIndex);
        for (rowIndent = aIndent+2; --rowIndent >= 0; ) fputs("  ", out);      
      }
    }
  }
}
#endif


/////////////////////////////////////////////////////////////////////////////
// Child frame enumeration

NS_IMETHODIMP
nsTableFrame::FirstChild(nsIPresContext* aPresContext,
                         nsIAtom*        aListName,
                         nsIFrame**      aFirstChild) const
{
  NS_PRECONDITION(nsnull != aFirstChild, "null OUT parameter pointer");
  if (aListName == nsLayoutAtoms::colGroupList) {
    *aFirstChild = mColGroups.FirstChild();
    return NS_OK;
  }

  return nsHTMLContainerFrame::FirstChild(aPresContext, aListName, aFirstChild);
}

NS_IMETHODIMP
nsTableFrame::GetAdditionalChildListName(PRInt32   aIndex,
                                         nsIAtom** aListName) const
{
  NS_PRECONDITION(nsnull != aListName, "null OUT parameter pointer");
  if (aIndex < 0) {
    return NS_ERROR_INVALID_ARG;
  }
  *aListName = nsnull;
  switch (aIndex) {
  case NS_TABLE_FRAME_COLGROUP_LIST_INDEX:
    *aListName = nsLayoutAtoms::colGroupList;
    NS_ADDREF(*aListName);
    break;
  }
  return NS_OK;
}

/* SEC: TODO: adjust the rect for captions */
NS_METHOD nsTableFrame::Paint(nsIPresContext* aPresContext,
                              nsIRenderingContext& aRenderingContext,
                              const nsRect& aDirtyRect,
                              nsFramePaintLayer aWhichLayer)
{
  // table paint code is concerned primarily with borders and bg color
  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
    const nsStyleDisplay* disp =
      (const nsStyleDisplay*)mStyleContext->GetStyleData(eStyleStruct_Display);
    if (disp->IsVisibleOrCollapsed()) {
      const nsStyleSpacing* spacing =
        (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);
      const nsStyleColor* color =
        (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);

      nsRect  rect(0, 0, mRect.width, mRect.height);
        
      nsCompatibility mode;
      aPresContext->GetCompatibilityMode(&mode);
      if (eCompatibility_NavQuirks != mode) {
        nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                        aDirtyRect, rect, *color, *spacing, 0, 0);
      }
      // paint the column groups and columns
      nsIFrame* colGroupFrame = mColGroups.FirstChild();
      while (nsnull != colGroupFrame) {
        PaintChild(aPresContext, aRenderingContext, aDirtyRect, colGroupFrame, aWhichLayer);
        colGroupFrame->GetNextSibling(&colGroupFrame);
      }

      PRIntn skipSides = GetSkipSides();
      if (NS_STYLE_BORDER_SEPARATE == GetBorderCollapseStyle())
      {
        nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                    aDirtyRect, rect, *spacing, mStyleContext, skipSides);
      }
      else
      {
        //printf("paint table frame\n");
        nsBorderEdges* edges = nsnull;
        if (mBorderCollapser) {
          edges = mBorderCollapser->GetEdges();
        }
        nsCSSRendering::PaintBorderEdges(aPresContext, aRenderingContext, this,
                                         aDirtyRect, rect,  edges, mStyleContext, skipSides);
      }
    }
  }

#ifdef DEBUG
  // for debug...
  if ((NS_FRAME_PAINT_LAYER_DEBUG == aWhichLayer) && GetShowFrameBorders()) {
    aRenderingContext.SetColor(NS_RGB(0,255,0));
    aRenderingContext.DrawRect(0, 0, mRect.width, mRect.height);
  }
#endif

  PaintChildren(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
  return NS_OK;
  /*nsFrame::Paint(aPresContext,
                        aRenderingContext,
                        aDirtyRect,
                        aWhichLayer);*/
}

NS_IMETHODIMP
nsTableFrame::GetFrameForPoint(nsIPresContext* aPresContext,
                                   const nsPoint& aPoint, 
                                   nsFramePaintLayer aWhichLayer,
                                   nsIFrame**     aFrame)
{
  // this should act like a block, so we need to override
  return GetFrameForPointUsing(aPresContext, aPoint, nsnull, aWhichLayer, (aWhichLayer == NS_FRAME_PAINT_LAYER_BACKGROUND), aFrame);
}


//null range means the whole thing
NS_IMETHODIMP
nsTableFrame::SetSelected(nsIPresContext* aPresContext,
                          nsIDOMRange *aRange,
                          PRBool aSelected,
                          nsSpread aSpread)
{
#if 0
  //traverse through children unselect tables
  if ((aSpread == eSpreadDown)){
    nsIFrame* kid;
    nsresult rv = FirstChild(nsnull, &kid);
    while (NS_SUCCEEDED(rv) && nsnull != kid) {
      kid->SetSelected(nsnull,aSelected,eSpreadDown);

      rv = kid->GetNextSibling(&kid);
    }
  }
#endif
  // Must call base class to set mSelected state and trigger repaint of frame
  // Note that in current version, aRange and aSpread are ignored,
  //   only this frame is considered
  nsFrame::SetSelected(aPresContext, aRange, aSelected, aSpread);
  return NS_OK;//return nsFrame::SetSelected(aRange,aSelected,eSpreadNone);
  
}

PRBool nsTableFrame::ParentDisablesSelection() const //override default behavior
{
  PRBool returnval;
  if (NS_FAILED(GetSelected(&returnval)))
    return PR_FALSE;
  if (returnval)
    return PR_TRUE;
  return nsFrame::ParentDisablesSelection();
}

PRIntn
nsTableFrame::GetSkipSides() const
{
  PRIntn skip = 0;
  // frame attribute was accounted for in nsHTMLTableElement::MapTableBorderInto
  // account for pagination
  if (nsnull != mPrevInFlow) {
    skip |= 1 << NS_SIDE_TOP;
  }
  if (nsnull != mNextInFlow) {
    skip |= 1 << NS_SIDE_BOTTOM;
  }
  return skip;
}

PRBool nsTableFrame::NeedsReflow(const nsHTMLReflowState& aReflowState)
{
  PRBool result = PR_TRUE;

  if (mBits.mIsInvariantWidth) {
    result = PR_FALSE;
  } else if ((eReflowReason_Incremental == aReflowState.reason) &&
             (NS_UNCONSTRAINEDSIZE == aReflowState.availableHeight)) {
    // If it's an incremental reflow and we're in galley mode, then only
    // do a full reflow if we need to. We need to if the cell map is invalid,
    // or the column info is invalid, or if we need to do a pass-1 reflow
    result = !(IsFirstPassValid() && IsColumnWidthsValid());
  }
  return result;
}

// Called by IR_TargetIsChild() after an incremental reflow of
// aKidFrame. Only called if we don't need a full reflow, e.g., the
// column widths haven't changed. Not used for paginated mode, so
// we don't need to worry about split row group frames
//
// Slides all the row groups following aKidFrame by the specified
// amount
//
// XXX This is kind of klunky because the InnerTableReflowState::y
// data member does not include the table's border/padding...
nsresult nsTableFrame::AdjustSiblingsAfterReflow(nsIPresContext*        aPresContext,
                                                 InnerTableReflowState& aReflowState,
                                                 nsIFrame*              aKidFrame,
                                                 nsSize*                aMaxElementSize,
                                                 nscoord                aDeltaY)
{
  NS_PRECONDITION(NS_UNCONSTRAINEDSIZE == aReflowState.reflowState.availableHeight,
                  "we're not in galley mode");

  nscoord yInvalid = NS_UNCONSTRAINEDSIZE;

  // Get the ordered children and find aKidFrame in the list
  nsVoidArray rowGroups;
  PRUint32 numRowGroups;
  OrderRowGroups(rowGroups, numRowGroups, nsnull);
  PRUint32 changeIndex;
  for (changeIndex = 0; changeIndex < numRowGroups; changeIndex++) {
    if (aKidFrame == rowGroups.ElementAt(changeIndex)) {
      break;
    }
  }
  changeIndex++; // set it to the next sibling

  for (PRUint32 rgX = changeIndex; rgX < numRowGroups; rgX++) {
    nsIFrame* kidFrame = (nsIFrame*)rowGroups.ElementAt(rgX);
    nsRect    kidRect;
    // Move the frames that follow aKidFrame by aDeltaY, and update the running
    // y-offset
    nsTableRowGroupFrame* rgFrame = GetRowGroupFrame(kidFrame);
    if (!rgFrame) continue; // skip foreign frames

    // Get the frame's bounding rect
    kidFrame->GetRect(kidRect);
    yInvalid = PR_MIN(yInvalid, kidRect.y);
  
    // Adjust the running y-offset
    aReflowState.y += kidRect.height;

    // Update the max element size
    //XXX: this should call into layout strategy to get the width field
    if (aMaxElementSize) {
      const nsStyleSpacing* tableSpacing;
      GetStyleData(eStyleStruct_Spacing , ((const nsStyleStruct *&)tableSpacing));
      nsMargin borderPadding;
      GetTableBorder (borderPadding); // gets the max border thickness for each edge
      borderPadding += aReflowState.reflowState.mComputedPadding;
      nscoord cellSpacing = GetCellSpacingX();
      nsSize  kidMaxElementSize;
      rgFrame->GetMaxElementSize(kidMaxElementSize);
      nscoord kidWidth = kidMaxElementSize.width + borderPadding.left + borderPadding.right + cellSpacing*2;
      aMaxElementSize->width = PR_MAX(aMaxElementSize->width, kidWidth); 
      aMaxElementSize->height += kidMaxElementSize.height;
    }
  
    // Adjust the y-origin if its position actually changed
    if (aDeltaY != 0) {
      kidRect.y += aDeltaY;
      kidFrame->MoveTo(aPresContext, kidRect.x, kidRect.y);
    }
  }
  
  // Invalidate the area we offset. Note that we only repaint within
  // XXX It would be better to bitblt the row frames and not repaint,
  // but we don't have such a view manager function yet...
  if (NS_UNCONSTRAINEDSIZE != yInvalid) {
    nsRect  dirtyRect(0, yInvalid, mRect.width, mRect.height - yInvalid);
    Invalidate(aPresContext, dirtyRect);
  }

  return NS_OK;
}

void 
nsTableFrame::SetColumnDimensions(nsIPresContext* aPresContext, 
                                  nscoord         aHeight,
                                  const nsMargin& aBorderPadding)
{
  nscoord colHeight = aHeight -= aBorderPadding.top + aBorderPadding.bottom;
  nscoord cellSpacingX = GetCellSpacingX();

  nsIFrame* colGroupFrame = mColGroups.FirstChild();
  PRInt32 colX = 0;
  nsPoint colGroupOrigin(aBorderPadding.left + cellSpacingX, aBorderPadding.top);
  PRInt32 numCols = GetColCount();
  while (nsnull != colGroupFrame) {
    nscoord colGroupWidth = 0;
    nsIFrame* colFrame = nsnull;
    colGroupFrame->FirstChild(aPresContext, nsnull, &colFrame);
    nsPoint colOrigin(0,0);
    while (nsnull != colFrame) {
      const nsStyleDisplay* colDisplay;
      colFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)colDisplay));
      if (NS_STYLE_DISPLAY_TABLE_COLUMN == colDisplay->mDisplay) {
        NS_ASSERTION(colX < numCols, "invalid number of columns");
        nscoord colWidth = mColumnWidths[colX];
        nsRect colRect(colOrigin.x, colOrigin.y, colWidth, colHeight);
        colFrame->SetRect(aPresContext, colRect);
        colOrigin.x += colWidth + cellSpacingX;

        colGroupWidth += colWidth;
        if (numCols - 1 != colX) {
          colGroupWidth += cellSpacingX;
        }
        colX++;
      }
      colFrame->GetNextSibling(&colFrame);
    }
    nsRect colGroupRect(colGroupOrigin.x, colGroupOrigin.y, colGroupWidth, colHeight);
    colGroupFrame->SetRect(aPresContext, colGroupRect);
    colGroupFrame->GetNextSibling(&colGroupFrame);
    colGroupOrigin.x += colGroupWidth;
  }
}

// SEC: TODO need to worry about continuing frames prev/next in flow for splitting across pages.

// GetParentStyleContextProvider:
//  The grandparent frame is he parent style context provider
//  That is, the parent of the outerTableFrame is the frame that has our parent style context
//  NOTE: if ther is a placeholder frame for the outer table, then we use its grandparent frame.
//
NS_IMETHODIMP 
nsTableFrame::GetParentStyleContextProvider(nsIPresContext* aPresContext,
                                            nsIFrame** aProviderFrame, 
                                            nsContextProviderRelationship& aRelationship)
{
  NS_ASSERTION(aProviderFrame, "null argument aProviderFrame");
  if (aProviderFrame) {
    nsIFrame* f;
    GetParent(&f);
    NS_ASSERTION(f,"TableFrame has no parent frame");
    if (f) {
      // see if there is a placeholder frame representing our parent's place in the frame tree
      // if there is, we get the parent of that frame as our surrogate grandparent
      nsIFrame* placeholder = nsnull;
      GetPlaceholderFor(*aPresContext,*f,&placeholder);
      if (placeholder) {
        f = placeholder;
      } 
      f->GetParent(&f);
      NS_ASSERTION(f,"TableFrame has no grandparent frame");
    }
    // parent context provider is the grandparent frame
    *aProviderFrame = f;
    aRelationship = eContextProvider_Ancestor;
  }
  return ((aProviderFrame != nsnull) && (*aProviderFrame != nsnull)) ? NS_OK : NS_ERROR_FAILURE;
}


/* overview:
  if mFirstPassValid is false, this is our first time through since content was last changed
    do pass 1
      get min/max info for all cells in an infinite space
  do column balancing
  do pass 2
  use column widths to size table and ResizeReflow rowgroups (and therefore rows and cells)
*/

/* Layout the entire inner table. */
NS_METHOD nsTableFrame::Reflow(nsIPresContext* aPresContext,
                               nsHTMLReflowMetrics& aDesiredSize,
                               const nsHTMLReflowState& aReflowState,
                               nsReflowStatus& aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsTableFrame", aReflowState.reason);
  if (nsDebugTable::gRflTable) nsTableFrame::DebugReflow("T::Rfl en", this, &aReflowState, nsnull);

  // Initialize out parameter
  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width = 0;
    aDesiredSize.maxElementSize->height = 0;
  }
  aStatus = NS_FRAME_COMPLETE;
  // XXX yank this nasty code and see what happens, and then yank other
  // similar calls to InvalidateFirstPassCache.
  if ((NS_UNCONSTRAINEDSIZE == aReflowState.availableWidth) &&
      (this == (nsTableFrame *)GetFirstInFlow())) {
    InvalidateFirstPassCache();
  }

  nsresult rv = NS_OK;
  nsSize* pass1MaxElementSize = aDesiredSize.maxElementSize;

  if (eReflowReason_Incremental == aReflowState.reason) {
    rv = IncrementalReflow(aPresContext, aDesiredSize, aReflowState, aStatus);
  }

  // If this is a style change reflow, then invalidate the pass 1 cache
  if (eReflowReason_StyleChange == aReflowState.reason) {
    InvalidateFirstPassCache();
  }

  // NeedsReflow and IsFirstPassValid take into account reflow type = Initial_Reflow
  if (NeedsReflow(aReflowState)) {
    PRBool needsRecalc = PR_FALSE;
    if ((NS_UNCONSTRAINEDSIZE == aReflowState.availableWidth) || 
        (PR_FALSE==IsFirstPassValid())) {
      nsReflowReason reason = aReflowState.reason;
      if ((eReflowReason_Initial != reason) &&
          (eReflowReason_StyleChange != reason)) {
        reason = eReflowReason_Resize;
      }
      if (mBorderCollapser) {
        mBorderCollapser->ComputeVerticalBorders(aPresContext, 0, -1);
      }
      rv = ResizeReflowPass1(aPresContext, aDesiredSize, aReflowState, aStatus, reason, PR_TRUE);
      if (NS_FAILED(rv))
        return rv;
      needsRecalc = PR_TRUE;
    }
    if (mTableLayoutStrategy && (needsRecalc || !IsColumnWidthsValid())) {
      nscoord boxWidth = CalcBorderBoxWidth(aReflowState);
      mTableLayoutStrategy->Initialize(aPresContext, aDesiredSize.maxElementSize, 
                                       boxWidth, aReflowState);
      mBits.mColumnWidthsValid = PR_TRUE; //so we don't do this a second time below
    }

    if (!mPrevInFlow) { 
      // only do this for a first-in-flow table frame
      // assign column widths, and assign aMaxElementSize->width
      BalanceColumnWidths(aPresContext, aReflowState, nsSize(aReflowState.availableWidth, aReflowState.availableHeight), 
                          aDesiredSize.maxElementSize);

      // assign table width
      SetTableWidth(aPresContext, aReflowState);
    }

    if ((eReflowReason_Initial == aReflowState.reason) &&
        (NS_UNCONSTRAINEDSIZE == aReflowState.availableWidth)) {
      // Don't bother doing a pass2 reflow. Use our pass1 width as our
      // desired width
      aDesiredSize.width = mRect.width;

    } else {
      // Constrain our reflow width to the computed table width. Note: this is based
      // on the width of the first-in-flow
      nsHTMLReflowState reflowState(aReflowState);
      PRInt32 pass1Width = mRect.width;
      if (mPrevInFlow) {
        nsTableFrame* table = (nsTableFrame*)GetFirstInFlow();
        pass1Width = table->mRect.width;
      }
      reflowState.availableWidth = pass1Width;
      if (pass1MaxElementSize) { 
        // we already have the max element size, so don't request it during pass2
        aDesiredSize.maxElementSize = nsnull;
      }
      rv = ResizeReflowPass2(aPresContext, aDesiredSize, reflowState, aStatus);
      if (NS_FAILED(rv)) return rv;

      aDesiredSize.width = PR_MIN(aDesiredSize.width, pass1Width);
  
      // If this is an incremental reflow and we're here that means we had to
      // reflow all the rows, e.g., the column widths changed. We need to make
      // sure that any damaged areas are repainted
      if (eReflowReason_Incremental == aReflowState.reason) {
        nsRect  damageRect;
  
        damageRect.x = 0;
        damageRect.y = 0;
        damageRect.width = mRect.width;
        damageRect.height = mRect.height;
        Invalidate(aPresContext, damageRect);
      }
    }
  }
  else {
    // set aDesiredSize and aMaxElementSize
  }

  SetColumnDimensions(aPresContext, aDesiredSize.height, aReflowState.mComputedBorderPadding);
  if (pass1MaxElementSize) {
    aDesiredSize.maxElementSize = pass1MaxElementSize;
  }

  // See if we are supposed to compute our maximum width
  if (aDesiredSize.mFlags & NS_REFLOW_CALC_MAX_WIDTH) {
    PRBool  isAutoOrPctWidth = IsAutoLayout(&aReflowState) &&
                               ((eStyleUnit_Auto == aReflowState.mStylePosition->mWidth.GetUnit() ||
                                (eStyleUnit_Percent == aReflowState.mStylePosition->mWidth.GetUnit())));
    
    // See if the pass1 maximum width is no longer valid because one of the
    // cell maximum widths changed
    if (mPrevInFlow) {
      // a next in flow just uses the preferred width of the 1st in flow.
      nsTableFrame* firstInFlow = (nsTableFrame*)GetFirstInFlow();
      if (firstInFlow) {
        aDesiredSize.mMaximumWidth = firstInFlow->GetPreferredWidth();
      }
    }
    else if (!IsMaximumWidthValid()) {
      // Initialize the strategy and have it compute the natural size of
      // the table
      mTableLayoutStrategy->Initialize(aPresContext, nsnull, NS_UNCONSTRAINEDSIZE, aReflowState);

      // Ask the strategy for the natural width of the content area 
      aDesiredSize.mMaximumWidth = mTableLayoutStrategy->GetTableMaxWidth();
     
      // Add in space for border
      nsMargin border;
      GetTableBorder (border); // this gets the max border value at every edge
      aDesiredSize.mMaximumWidth += border.left + border.right;

      // Add in space for padding
      aDesiredSize.mMaximumWidth += aReflowState.mComputedPadding.left +
                                    aReflowState.mComputedPadding.right;

      SetPreferredWidth(aDesiredSize.mMaximumWidth); // cache the value

      // Initializing the table layout strategy assigns preliminary column
      // widths. We can't leave the column widths this way, and so we need to
      // balance the column widths to get them back to what we had previously.
      // XXX It would be nice to have a cleaner way to calculate the updated
      // maximum width
      BalanceColumnWidths(aPresContext, aReflowState,
                          nsSize(aReflowState.availableWidth, aReflowState.availableHeight), 
                          aDesiredSize.maxElementSize);
      // Now the maximum width is valid
      mBits.mMaximumWidthValid = PR_TRUE;
    } else {
      aDesiredSize.mMaximumWidth = GetPreferredWidth();
    }
  }

  if (nsDebugTable::gRflTable) nsTableFrame::DebugReflow("T::Rfl ex", this, nsnull, &aDesiredSize, aStatus);
  return rv;
}

/** the first of 2 reflow passes
  * lay out the captions and row groups in an infinite space (NS_UNCONSTRAINEDSIZE)
  * cache the results for each caption and cell.
  * if successful, set mFirstPassValid=PR_TRUE, so we know we can skip this step 
  * next time.  mFirstPassValid is set to PR_FALSE when content is changed.
  * NOTE: should never get called on a continuing frame!  All cached pass1 state
  *       is stored in the inner table first-in-flow.
  */
NS_METHOD nsTableFrame::ResizeReflowPass1(nsIPresContext*          aPresContext,
                                          nsHTMLReflowMetrics&     aDesiredSize,
                                          const nsHTMLReflowState& aReflowState,
                                          nsReflowStatus&          aStatus,
                                          nsReflowReason           aReason,
                                          PRBool                   aDoSiblingFrames)
{
  NS_PRECONDITION(aReflowState.frame == this, "bad reflow state");
  NS_PRECONDITION(aReflowState.parentReflowState->frame == mParent,
                  "bad parent reflow state");
  NS_ASSERTION(nsnull==mPrevInFlow, "illegal call, cannot call pass 1 on a continuing frame.");
  NS_ASSERTION(nsnull != mContent, "null content");

  nsresult rv = NS_OK;
  // set out params
  aStatus = NS_FRAME_COMPLETE;

  nsSize availSize(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE); // availSize is the space available at any given time in the process
  nsSize maxSize(0, 0);       // maxSize is the size of the largest child so far in the process
  nsSize kidMaxSize(0,0);
  nsHTMLReflowMetrics kidSize(&kidMaxSize);
  nscoord y = 0;
  nscoord cellSpacingY = GetCellSpacingY();

  // Compute the insets (sum of border and padding)
  // XXX: since this is pass1 reflow and where we place the rowgroup frames is irrelevant, insets are probably a waste

  if (IsAutoLayout(&aReflowState)) {
    nsVoidArray rowGroups;
    PRUint32 numRowGroups;
    OrderRowGroups(rowGroups, numRowGroups, nsnull);
    PRUint32 numChildren = rowGroups.Count();

    nsIFrame* kidFrame;
    for (PRUint32 childX = 0; childX < numChildren; childX++) {
      kidFrame = (nsIFrame*)rowGroups.ElementAt(childX);
      if (childX >= numRowGroups) { 
        // it's an unknown frame type, give it a generic reflow and ignore the results
        nsHTMLReflowState kidReflowState(aPresContext, aReflowState, kidFrame, 
                                         availSize, aReason);
        // rv intentionally not set here
        ReflowChild(kidFrame, aPresContext, kidSize, kidReflowState, 0, 0, 0, aStatus);
        kidFrame->DidReflow(aPresContext, NS_FRAME_REFLOW_FINISHED);
        continue;
      }

      // Get the table's border padding
      nsMargin borderPadding;
      GetTableBorderForRowGroup(GetRowGroupFrame(kidFrame), borderPadding);
      const nsStyleSpacing* tableSpacing;
      GetStyleData(eStyleStruct_Spacing, ((const nsStyleStruct *&)tableSpacing));
      nsMargin padding;
      tableSpacing->GetPadding(padding);
      borderPadding += padding;

      y += cellSpacingY;
      if (0 == childX) {
        y += borderPadding.top;
      }
      
      // Reflow the child
      nsHTMLReflowState kidReflowState(aPresContext, aReflowState, kidFrame,
                                       availSize, aReason);
      // Note: we don't bother checking here for whether we should clear the
      // isTopOfPage reflow state flag, because we're dealing with an unconstrained
      // height and it isn't an issue...
      ReflowChild(kidFrame, aPresContext, kidSize, kidReflowState, borderPadding.left,
                  y, 0, aStatus);

      // Place the child since some of its content fit in us.
      FinishReflowChild(kidFrame, aPresContext, kidSize, borderPadding.left,
                        borderPadding.top + y, 0);
      if (NS_UNCONSTRAINEDSIZE == kidSize.height)
        // XXX This is very suspicious. Why would a row group frame want
        // such a large height?
        y = NS_UNCONSTRAINEDSIZE;
      else
        y += kidSize.height;
      if (kidMaxSize.width > maxSize.width) {
        maxSize.width = kidMaxSize.width;
      }
      if (kidMaxSize.height > maxSize.height) {
        maxSize.height = kidMaxSize.height;
      }

      if (NS_FRAME_IS_NOT_COMPLETE(aStatus)) {
        // If the child didn't finish layout then it means that it used
        // up all of our available space (or needs us to split).
        break;
      }
      if (!aDoSiblingFrames)
        break;
    }

    // if required, give the colgroups their initial reflows
    if (aDoSiblingFrames) {
      kidFrame=mColGroups.FirstChild();   
      for ( ; nsnull != kidFrame; kidFrame->GetNextSibling(&kidFrame)) {
        nsHTMLReflowState kidReflowState(aPresContext, aReflowState, kidFrame,
                                         availSize, aReason);
        ReflowChild(kidFrame, aPresContext, kidSize, kidReflowState, 0, 0, 0, aStatus);
        FinishReflowChild(kidFrame, aPresContext, kidSize, 0, 0, 0);
      }
    }
  }

  // Get the table's border/padding
  const nsStyleSpacing* mySpacing = (const nsStyleSpacing*)
    mStyleContext->GetStyleData(eStyleStruct_Spacing);
  nsMargin tableBorderPadding;
  GetTableBorder (tableBorderPadding);   // this gets the max border thickness at each edge
  nsMargin tablePadding;
  mySpacing->GetPadding(tablePadding);
  tableBorderPadding += tablePadding;

  aDesiredSize.width = kidSize.width;
  nscoord defaultHeight = y + tableBorderPadding.top + tableBorderPadding.bottom;
  aDesiredSize.height = ComputeDesiredHeight(aPresContext, aReflowState, defaultHeight);
  mBits.mFirstPassValid = PR_TRUE;

  return rv;
}
  
nsIFrame*
nsTableFrame::GetFirstBodyRowGroupFrame()
{
  nsIFrame* headerFrame = nsnull;
  nsIFrame* footerFrame = nsnull;

  for (nsIFrame* kidFrame = mFrames.FirstChild(); nsnull != kidFrame; ) {
    const nsStyleDisplay *childDisplay;
    kidFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)childDisplay));

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
    kidFrame->GetNextSibling(&kidFrame);
  }

  return nsnull;
}

// Table specific version that takes into account repeated header and footer
// frames when continuing table frames
void
nsTableFrame::PushChildren(nsIPresContext* aPresContext,
                           nsIFrame*       aFromChild,
                           nsIFrame*       aPrevSibling)
{
  NS_PRECONDITION(nsnull != aFromChild, "null pointer");
  NS_PRECONDITION(nsnull != aPrevSibling, "pushing first child");
#ifdef NS_DEBUG
  nsIFrame* prevNextSibling;
  aPrevSibling->GetNextSibling(&prevNextSibling);
  NS_PRECONDITION(prevNextSibling == aFromChild, "bad prev sibling");
#endif

  // Disconnect aFromChild from its previous sibling
  aPrevSibling->SetNextSibling(nsnull);

  if (nsnull != mNextInFlow) {
    nsTableFrame* nextInFlow = (nsTableFrame*)mNextInFlow;

    // Insert the frames after any repeated header and footer frames
    nsIFrame* firstBodyFrame = nextInFlow->GetFirstBodyRowGroupFrame();
    nsIFrame* prevSibling = nsnull;
    if (firstBodyFrame) {
      prevSibling = nextInFlow->mFrames.GetPrevSiblingFor(firstBodyFrame);
    }
    // When pushing and pulling frames we need to check for whether any
    // views need to be reparented.
    for (nsIFrame* f = aFromChild; f; f->GetNextSibling(&f)) {
      nsHTMLContainerFrame::ReparentFrameView(aPresContext, f, this, nextInFlow);
    }
    nextInFlow->mFrames.InsertFrames(mNextInFlow, prevSibling, aFromChild);
  }
  else {
    // Add the frames to our overflow list
    SetOverflowFrames(aPresContext, aFromChild);
  }
}

// Table specific version that takes into account header and footer row group
// frames that are repeated for continuing table frames
//
// Appends the overflow frames to the end of the child list, just like the
// nsContainerFrame version does, except that there are no assertions that
// the child list is empty (it may not be empty, because there may be repeated
// header/footer frames)
PRBool
nsTableFrame::MoveOverflowToChildList(nsIPresContext* aPresContext)
{
  PRBool result = PR_FALSE;

  // Check for an overflow list with our prev-in-flow
  nsTableFrame* prevInFlow = (nsTableFrame*)mPrevInFlow;
  if (nsnull != prevInFlow) {
    nsIFrame* prevOverflowFrames = prevInFlow->GetOverflowFrames(aPresContext, PR_TRUE);
    if (prevOverflowFrames) {
      // When pushing and pulling frames we need to check for whether any
      // views need to be reparented.
      for (nsIFrame* f = prevOverflowFrames; f; f->GetNextSibling(&f)) {
        nsHTMLContainerFrame::ReparentFrameView(aPresContext, f, prevInFlow, this);
      }
      mFrames.InsertFrames(this, nsnull, prevOverflowFrames);
      result = PR_TRUE;
    }
  }

  // It's also possible that we have an overflow list for ourselves
  nsIFrame* overflowFrames = GetOverflowFrames(aPresContext, PR_TRUE);
  if (overflowFrames) {
    mFrames.AppendFrames(nsnull, overflowFrames);
    result = PR_TRUE;
  }
  return result;
}

/** the second of 2 reflow passes
  */
NS_METHOD nsTableFrame::ResizeReflowPass2(nsIPresContext*          aPresContext,
                                          nsHTMLReflowMetrics&     aDesiredSize,
                                          const nsHTMLReflowState& aReflowState,
                                          nsReflowStatus&          aStatus)
{
  NS_PRECONDITION(aReflowState.frame == this, "bad reflow state");
  NS_PRECONDITION(aReflowState.parentReflowState->frame == mParent,
                  "bad parent reflow state");
  nsresult rv = NS_OK;
  // set out param
  aStatus = NS_FRAME_COMPLETE;

  nsMargin borderPadding;
  GetTableBorder (borderPadding);   // this gets the max border thickness at each edge
  borderPadding += aReflowState.mComputedPadding;

  InnerTableReflowState state(aPresContext, aReflowState, borderPadding);
  state.y = borderPadding.top; // XXX this should be set in the constructor, but incremental code is affected.
  // now that we've computed the column  width information, reflow all children

#ifdef NS_DEBUG
  //PreReflowCheck();
#endif

  // Check for an overflow list, and append any row group frames being
  // pushed
  MoveOverflowToChildList(aPresContext);

  // Reflow the existing frames
  if (mFrames.NotEmpty()) {
    ComputePercentBasisForRows(aPresContext, aReflowState);
    rv = ReflowMappedChildren(aPresContext, aDesiredSize, state, aStatus);
  }

  // Did we successfully reflow our mapped children?
  if (NS_FRAME_COMPLETE == aStatus) {
    // Any space left?
    if (state.availSize.height > 0) {
      // Try and pull-up some children from a next-in-flow
      rv = PullUpChildren(aPresContext, aDesiredSize, state, aStatus);
    }
  }

  // Return our size and our status
  aDesiredSize.width = ComputeDesiredWidth(aReflowState);
  nscoord defaultHeight = state.y + GetCellSpacingY() + borderPadding.bottom;
  aDesiredSize.height = ComputeDesiredHeight(aPresContext, aReflowState, defaultHeight);
 
  AdjustForCollapsingRows(aPresContext, aDesiredSize.height);
  AdjustForCollapsingCols(aPresContext, aDesiredSize.width);

  // once horizontal borders are computed and all row heights are set, 
  // we need to fix up length of vertical edges
  // XXX need to figure start row and end row correctly
  if ((NS_STYLE_BORDER_COLLAPSE == GetBorderCollapseStyle()) && mBorderCollapser) {
    mBorderCollapser->DidComputeHorizontalBorders(aPresContext, 0, 10000);
  }

#ifdef NS_DEBUG
  //PostReflowCheck(aStatus);
#endif

  return rv;

}

void nsTableFrame::ComputePercentBasisForRows(nsIPresContext*          aPresContext,
                                              const nsHTMLReflowState& aReflowState)
{
  nscoord height = CalcBorderBoxHeight(aPresContext, aReflowState);
  if ((height > 0) && (height != NS_UNCONSTRAINEDSIZE)) {
    // exclude our border and padding
    nsMargin borderPadding = aReflowState.mComputedBorderPadding;
    height -= borderPadding.top + borderPadding.bottom;

    height = PR_MAX(0, height);
  }
  else {
    height = 0;
  }
  mPercentBasisForRows = height;
}

NS_METHOD 
nsTableFrame::CollapseRowGroupIfNecessary(nsIPresContext* aPresContext,
                                          nsIFrame* aRowGroupFrame,
                                          const nscoord& aYTotalOffset,
                                          nscoord& aYGroupOffset, PRInt32& aRowX)
{
  const nsStyleDisplay* groupDisplay;
  aRowGroupFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)groupDisplay));
  
  PRBool collapseGroup = (NS_STYLE_VISIBILITY_COLLAPSE == groupDisplay->mVisible);
  nsIFrame* rowFrame;
  aRowGroupFrame->FirstChild(aPresContext, nsnull, &rowFrame);

  while (nsnull != rowFrame) {
    const nsStyleDisplay* rowDisplay;
    rowFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)rowDisplay));
    if (NS_STYLE_DISPLAY_TABLE_ROW_GROUP == rowDisplay->mDisplay) {
      CollapseRowGroupIfNecessary(aPresContext, rowFrame, aYTotalOffset, aYGroupOffset, aRowX);
    }
    else if (NS_STYLE_DISPLAY_TABLE_ROW == rowDisplay->mDisplay) {
      nsRect rowRect;
      rowFrame->GetRect(rowRect);
      if (collapseGroup || (NS_STYLE_VISIBILITY_COLLAPSE == rowDisplay->mVisible)) {
        aYGroupOffset += rowRect.height;
        rowRect.height = 0;
        rowFrame->SetRect(aPresContext, rowRect);
        nsIFrame* cellFrame;
        rowFrame->FirstChild(aPresContext, nsnull, &cellFrame);
        while (nsnull != cellFrame) {
          const nsStyleDisplay* cellDisplay;
          cellFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)cellDisplay));
          if (NS_STYLE_DISPLAY_TABLE_CELL == cellDisplay->mDisplay) {
            nsTableCellFrame* cFrame = (nsTableCellFrame*)cellFrame;
            nsRect cRect;
            cFrame->GetRect(cRect);
            cRect.height -= rowRect.height;
            cFrame->SetCollapseOffsetY(aPresContext, -aYGroupOffset);
            cFrame->SetRect(aPresContext, cRect);
          }
          cellFrame->GetNextSibling(&cellFrame);
        }
        // check if a cell above spans into here
        nsTableCellMap* cellMap = GetCellMap();
        if (cellMap) {
          PRInt32 numCols = cellMap->GetColCount();
          nsTableCellFrame* lastCell = nsnull;
          for (int colX = 0; colX < numCols; colX++) {
            CellData* cellData = cellMap->GetCellAt(aRowX, colX);
            if (cellData && cellData->IsSpan()) { // a cell above is spanning into here
              // adjust the real cell's rect only once
              nsTableCellFrame* realCell = nsnull;
              if (cellData->IsRowSpan())
                realCell = cellMap->GetCellFrame(aRowX, colX, *cellData, PR_TRUE);
              if (realCell != lastCell) {
                nsRect realRect;
                realCell->GetRect(realRect);
                realRect.height -= rowRect.height;
                realCell->SetRect(aPresContext, realRect);
              }
              lastCell = realCell;
            }
          }
        }
      } else { // row is not collapsed but needs to be adjusted by those that are
        rowRect.y -= aYGroupOffset;
        rowFrame->SetRect(aPresContext, rowRect);
      }
      aRowX++;
    }
    rowFrame->GetNextSibling(&rowFrame);
  } // end row frame while

  nsRect groupRect;
  aRowGroupFrame->GetRect(groupRect);
  groupRect.height -= aYGroupOffset;
  groupRect.y -= aYTotalOffset;
  aRowGroupFrame->SetRect(aPresContext, groupRect);

  return NS_OK;
}

// collapsing row groups, rows, col groups and cols are accounted for after both passes of
// reflow so that it has no effect on the calculations of reflow.
NS_METHOD nsTableFrame::AdjustForCollapsingRows(nsIPresContext* aPresContext, 
                                                nscoord&        aHeight)
{
  nsIFrame* groupFrame = mFrames.FirstChild(); 
  nscoord yGroupOffset = 0; // total offset among rows within a single row group
  nscoord yTotalOffset = 0; // total offset among all rows in all row groups
  PRInt32 rowIndex = 0;

  // collapse the rows and/or row groups as necessary
  while (nsnull != groupFrame) {
    const nsStyleDisplay* groupDisplay;
    groupFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)groupDisplay));
    if (IsRowGroup(groupDisplay->mDisplay)) {
      CollapseRowGroupIfNecessary(aPresContext, groupFrame, yTotalOffset, yGroupOffset, rowIndex);
    }
    yTotalOffset += yGroupOffset;
    yGroupOffset = 0;
    groupFrame->GetNextSibling(&groupFrame);
  } 

  aHeight -= yTotalOffset;
 
  return NS_OK;
}

NS_METHOD nsTableFrame::AdjustForCollapsingCols(nsIPresContext* aPresContext, 
                                                nscoord&        aWidth)
{
  nsTableCellMap* cellMap = GetCellMap();
  if (!cellMap) return NS_OK;

  PRInt32 numRows = cellMap->GetRowCount();
  nsTableIterator groupIter(mColGroups, eTableDIR);
  nsIFrame* groupFrame = groupIter.First(); 
  nscoord cellSpacingX = GetCellSpacingX();
  nscoord xOffset = 0;
  PRInt32 colX = (groupIter.IsLeftToRight()) ? 0 : GetColCount() - 1; 
  PRInt32 direction = (groupIter.IsLeftToRight()) ? 1 : -1; 
  // iterate over the col groups
  while (nsnull != groupFrame) {
    const nsStyleDisplay* groupDisplay;
    groupFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)groupDisplay));
    PRBool collapseGroup = (NS_STYLE_VISIBILITY_COLLAPSE == groupDisplay->mVisible);
    nsTableIterator colIter(aPresContext, *groupFrame, eTableDIR);
    nsIFrame* colFrame = colIter.First();
    // iterate over the cols in the col group
    while (nsnull != colFrame) {
      const nsStyleDisplay* colDisplay;
      colFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)colDisplay));
      if (NS_STYLE_DISPLAY_TABLE_COLUMN == colDisplay->mDisplay) {
        PRBool collapseCol = (NS_STYLE_VISIBILITY_COLLAPSE == colDisplay->mVisible);
        PRInt32 colWidth = GetColumnWidth(colX);
        if (collapseGroup || collapseCol) {
          xOffset += colWidth + cellSpacingX;
        }
        nsTableCellFrame* lastCell  = nsnull;
        nsTableCellFrame* cellFrame = nsnull;
        for (PRInt32 rowX = 0; rowX < numRows; rowX++) {
          CellData* cellData = cellMap->GetCellAt(rowX, colX);
          nsRect cellRect;
          if (cellData) {
            if (cellData->IsOrig()) { // the cell originates at (rowX, colX)
              cellFrame = cellData->GetCellFrame();
              // reset the collapse offsets since they may have been collapsed previously
              cellFrame->SetCollapseOffsetX(aPresContext, 0);
              cellFrame->SetCollapseOffsetY(aPresContext, 0);
              cellFrame->GetRect(cellRect);
              if (collapseGroup || collapseCol) {
                if (lastCell != cellFrame) { // do it only once if there is a row span
                  cellRect.width -= colWidth;
                  cellFrame->SetCollapseOffsetX(aPresContext, -xOffset);
                }
              } else { // the cell is not in a collapsed col but needs to move
                cellRect.x -= xOffset;
              }
              cellFrame->SetRect(aPresContext, cellRect);
              // if the cell does not originate at (rowX, colX), adjust the real cells width
            } else if (collapseGroup || collapseCol) { 
              if (cellData->IsColSpan()) {
                cellFrame = cellMap->GetCellFrame(rowX, colX, *cellData, PR_FALSE);
              }
              if ((cellFrame) && (lastCell != cellFrame)) {
                cellFrame->GetRect(cellRect);
                cellRect.width -= colWidth + cellSpacingX;
                cellFrame->SetRect(aPresContext, cellRect);
              }
            }
          }
          lastCell = cellFrame;
        }
        colX += direction;
      }
      colFrame = colIter.Next();
    } // inner while
    groupFrame = groupIter.Next();
  } // outer while

  aWidth -= xOffset;
 
  return NS_OK;
}

// Sets the starting column index for aColGroupFrame and the siblings frames that
// follow
void
nsTableFrame::SetStartingColumnIndexFor(nsTableColGroupFrame* aColGroupFrame,
                                        PRInt32 aIndex)
{
  while (aColGroupFrame) {
    aIndex += aColGroupFrame->SetStartColumnIndex(aIndex);
    aColGroupFrame->GetNextSibling((nsIFrame**)&aColGroupFrame);
  }
}

// Calculate the starting column index to use for the specified col group frame
PRInt32
nsTableFrame::CalculateStartingColumnIndexFor(nsTableColGroupFrame* aColGroupFrame)
{
  PRInt32 index = 0;
  for (nsTableColGroupFrame* colGroupFrame = (nsTableColGroupFrame*)mColGroups.FirstChild();
       colGroupFrame && (colGroupFrame != aColGroupFrame);
       colGroupFrame->GetNextSibling((nsIFrame**)&colGroupFrame))
  {
    index += colGroupFrame->GetColCount();
  }

  return index;
}

NS_IMETHODIMP
nsTableFrame::AppendFrames(nsIPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aFrameList)
{
  PRInt32 startColIndex = 0;
  // Because we actually have two child lists, one for col group frames and one
  // for everything else, we need to look at each frame individually
  nsIFrame* f = aFrameList;
  nsIFrame* firstAppendedColGroup = nsnull;
  while (f) {
    nsIFrame* next;

    // Get the next frame and disconnect this frame from its sibling
    f->GetNextSibling(&next);
    f->SetNextSibling(nsnull);

    // See what kind of frame we have
    const nsStyleDisplay *display;
    f->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)display));

    if (NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP == display->mDisplay) {
      // Append the new col group frame
      mColGroups.AppendFrame(nsnull, f);
      if (!firstAppendedColGroup) {
        firstAppendedColGroup = f;
        nsIFrame* lastChild = mFrames.LastChild();
        nsTableColGroupFrame* lastColGroup = 
          (nsTableColGroupFrame*)GetFrameAtOrBefore(aPresContext, this, lastChild,
                                                    nsLayoutAtoms::tableColGroupFrame);
        startColIndex = (lastColGroup) 
          ? lastColGroup->GetStartColumnIndex() + lastColGroup->GetColCount() : 0;
      }
    } else if (IsRowGroup(display->mDisplay)) {
      nsIFrame* prevSibling = mFrames.LastChild();
      // Append the new row group frame to the sibling chain
      mFrames.AppendFrame(nsnull, f);

      // insert the row group and its rows into the table
      InsertRowGroups(*aPresContext, f, f);
    } else {
      // Nothing special to do, just add the frame to our child list
      mFrames.AppendFrame(nsnull, f);
    }

    // Move to the next frame
    f = next;
  }

  if (firstAppendedColGroup) {
    InsertColGroups(*aPresContext, startColIndex, firstAppendedColGroup);
  }

  // We'll need to do a pass-1 layout of all cells in all the rows of the
  // rowgroup. XXX - fix this
  InvalidateFirstPassCache();

  // Because the number of columns may have changed invalidate them
  InvalidateColumnWidths(); 
  
  // Mark the table as dirty and generate a reflow command targeted at the
  // outer table frame
  nsIReflowCommand* reflowCmd;
  nsresult          rv;
  
  mState |= NS_FRAME_IS_DIRTY;
  rv = NS_NewHTMLReflowCommand(&reflowCmd, mParent, nsIReflowCommand::ReflowDirty);
  if (NS_SUCCEEDED(rv)) {
    // Add the reflow command
    rv = aPresShell.AppendReflowCommand(reflowCmd);
    NS_RELEASE(reflowCmd);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTableFrame::InsertFrames(nsIPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aPrevFrame,
                           nsIFrame*       aFrameList)
{
  // Asssume there's only one frame being inserted. The problem is that
  // row group frames and col group frames go in separate child lists and
  // so if there's more than one this gets messy...
  // XXX The frame construction code should be separating out child frames
  // based on the type...
  nsIFrame* nextSibling;
  aFrameList->GetNextSibling(&nextSibling);
  NS_PRECONDITION(!nextSibling, "expected only one child frame");

  // See what kind of frame we have
  const nsStyleDisplay *display=nsnull;
  aFrameList->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)display));

  if (NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP == display->mDisplay) {
    // Insert the column group frame
    nsFrameList frames(aFrameList); // convience for getting last frame
    nsIFrame* lastFrame = frames.LastChild();
    mColGroups.InsertFrame(nsnull, aPrevFrame, aFrameList);
    // find the starting col index for the first new col group
    PRInt32 startColIndex = 0;
    if (aPrevFrame) {
      nsTableColGroupFrame* prevColGroup = 
        (nsTableColGroupFrame*)GetFrameAtOrBefore(aPresContext, this, aPrevFrame,
                                                  nsLayoutAtoms::tableColGroupFrame);
      if (prevColGroup) {
        startColIndex = prevColGroup->GetStartColumnIndex() + prevColGroup->GetColCount();
      }
    }
    InsertColGroups(*aPresContext, startColIndex, aFrameList, lastFrame);
  } else if (IsRowGroup(display->mDisplay)) {
    // get the starting row index of the new rows and insert them into the table
    PRInt32 rowIndex = 0;
    nsTableRowGroupFrame* prevRowGroup = (nsTableRowGroupFrame *)nsTableFrame::GetFrameAtOrBefore(aPresContext, this, aPrevFrame, nsLayoutAtoms::tableRowGroupFrame);
    nsFrameList newList(aFrameList);
    nsIFrame* lastSibling = newList.LastChild();
    //if (prevRowGroup) {
    //  PRInt32 numRows;
    //  prevRowGroup->GetRowCount(numRows);
    //  rowIndex = prevRowGroup->GetStartRowIndex() + numRows;
    //}
    // Insert the frames in the sibling chain
    mFrames.InsertFrame(nsnull, aPrevFrame, aFrameList);

    InsertRowGroups(*aPresContext, aFrameList, lastSibling);
  } else {
    // Just insert the frame and don't worry about reflowing it
    mFrames.InsertFrame(nsnull, aPrevFrame, aFrameList);
    return NS_OK;
  }

  // Because the number of columns may have changed invalidate them
  InvalidateColumnWidths();
  
  InvalidateFirstPassCache(); // XXX fix this
  
  // Mark the table as dirty and generate a reflow command targeted at the
  // outer table frame
  nsIReflowCommand* reflowCmd;
  nsresult          rv;
  
  mState |= NS_FRAME_IS_DIRTY;
  rv = NS_NewHTMLReflowCommand(&reflowCmd, mParent, nsIReflowCommand::ReflowDirty);
  if (NS_SUCCEEDED(rv)) {
    // Add the reflow command
    rv = aPresShell.AppendReflowCommand(reflowCmd);
    NS_RELEASE(reflowCmd);
  }
  return rv;

  return NS_OK;
}

NS_IMETHODIMP
nsTableFrame::RemoveFrame(nsIPresContext* aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aOldFrame)
{
  // See what kind of frame we have
  const nsStyleDisplay *display=nsnull;
  aOldFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)display));

  if (NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP == display->mDisplay) {
    nsIFrame* nextColGroupFrame;
    aOldFrame->GetNextSibling(&nextColGroupFrame);
    nsTableColGroupFrame* colGroup = (nsTableColGroupFrame*)aOldFrame;
    PRInt32 firstColIndex = colGroup->GetStartColumnIndex();
    PRInt32 lastColIndex  = firstColIndex + colGroup->GetColCount() - 1;
    // remove the col frames, the colGroup frame and reset col indices
    colGroup->RemoveChildrenAtEnd(*aPresContext, colGroup->GetColCount());
    mColGroups.DestroyFrame(aPresContext, aOldFrame);
    nsTableColGroupFrame::ResetColIndices(aPresContext, nextColGroupFrame, firstColIndex); 
    // remove the cols from the table
    PRInt32 colX;
    for (colX = lastColIndex; colX >= firstColIndex; colX--) {
      nsTableColFrame* colFrame = (nsTableColFrame*)mColFrames.ElementAt(colX);
      if (colFrame) {
        RemoveCol(*aPresContext, colGroup, colX, PR_TRUE, PR_FALSE);
      }
    }
    for (colX = lastColIndex; colX >= firstColIndex; colX--) {
      nsTableColFrame* colFrame = (nsTableColFrame*)mColFrames.ElementAt(colX);
      if (colFrame) {
        RemoveCol(*aPresContext, colGroup, colX, PR_FALSE, PR_TRUE);
      }
    }

    PRInt32 numAnonymousColsToAdd = GetColCount() - mColFrames.Count();
    if (numAnonymousColsToAdd > 0) {
      // this sets the child list, updates the col cache and cell map
      CreateAnonymousColFrames(*aPresContext, numAnonymousColsToAdd,
                               eColAnonymousCell, PR_TRUE);
    }

    InvalidateColumnWidths();
    AddTableDirtyReflowCommand(aPresContext, this);
  } else {
    nsTableRowGroupFrame* rgFrame = GetRowGroupFrame(aOldFrame);
    if (rgFrame) {
      PRInt32 startRowIndex = rgFrame->GetStartRowIndex();
      PRInt32 numRows;
      rgFrame->GetRowCount(numRows, PR_TRUE);
      // remove the row group from the cell map
      nsTableCellMap* cellMap = GetCellMap();
      if (cellMap) {
        cellMap->RemoveGroupCellMap(rgFrame);
      }
      // only remove cols that are of type eTypeAnonymous cell (they are at the end)
      PRInt32 numColsInMap = GetColCount(); // cell map's notion of num cols
      PRInt32 numColsInCache = mColFrames.Count();
      PRInt32 numColsNotRemoved = DestroyAnonymousColFrames(*aPresContext, numColsInCache - numColsInMap);
      // if the cell map has fewer cols than the cache, correct it
      if (numColsNotRemoved > 0) {
        cellMap->AddColsAtEnd(numColsNotRemoved);
      }
      AdjustRowIndices(aPresContext, startRowIndex, -numRows);
      // remove the row group frame from the sibling chain
      mFrames.DestroyFrame(aPresContext, aOldFrame);

      InvalidateColumnWidths();
      AddTableDirtyReflowCommand(aPresContext, this);
    } else {
      // Just remove the frame
      mFrames.DestroyFrame(aPresContext, aOldFrame);
      return NS_OK;
    }
  }
  return NS_OK;
}

NS_METHOD nsTableFrame::IncrementalReflow(nsIPresContext* aPresContext,
                                          nsHTMLReflowMetrics& aDesiredSize,
                                          const nsHTMLReflowState& aReflowState,
                                          nsReflowStatus& aStatus)
{
  nsresult  rv = NS_OK;

  // Constrain our reflow width to the computed table width. Note: this is
  // based on the width of the first-in-flow
  nsHTMLReflowState reflowState(aReflowState);
  PRInt32 pass1Width = mRect.width;
  if (mPrevInFlow) {
    nsTableFrame* table = (nsTableFrame*)GetFirstInFlow();
    pass1Width = table->mRect.width;
  }
  reflowState.availableWidth = pass1Width;

  // get border + padding
  nsMargin borderPadding;
  GetTableBorder (borderPadding);
  borderPadding += aReflowState.mComputedPadding;

  InnerTableReflowState state(aPresContext, reflowState, borderPadding);

  // determine if this frame is the target or not
  nsIFrame *target=nsnull;
  rv = reflowState.reflowCommand->GetTarget(target);
  if ((PR_TRUE==NS_SUCCEEDED(rv)) && (nsnull!=target))
  {
    // this is the target if target is either this or the outer table frame containing this inner frame
    nsIFrame *outerTableFrame=nsnull;
    GetParent(&outerTableFrame);
    if ((this==target) || (outerTableFrame==target))
      rv = IR_TargetIsMe(aPresContext, aDesiredSize, state, aStatus);
    else
    {
      // Get the next frame in the reflow chain
      nsIFrame* nextFrame;
      reflowState.reflowCommand->GetNext(nextFrame);
      NS_ASSERTION(nextFrame, "next frame in reflow command is null"); 

      // Recover our reflow state
      rv = IR_TargetIsChild(aPresContext, aDesiredSize, state, aStatus, nextFrame);
    }
  }
  return rv;
}

// this assumes the dirty children are contiguous 
// XXX if this is ever used again, it probably needs to call OrderRowGroups
PRBool GetDirtyChildren(nsIPresContext* aPresContext,
                        nsIFrame*       aFrame,
                        nsIFrame**      aFirstDirty,
                        PRInt32&        aNumDirty)
{
  *aFirstDirty = nsnull;
  aNumDirty = 0;

  nsIFrame* kidFrame;
  aFrame->FirstChild(aPresContext, nsnull, &kidFrame);
  while (kidFrame) {
    nsFrameState  frameState;
    kidFrame->GetFrameState(&frameState);
    if (frameState & NS_FRAME_IS_DIRTY) {
      if (!*aFirstDirty) {
        *aFirstDirty = kidFrame;
      }
      aNumDirty++;
    }
  }
  return (aNumDirty > 0);
}

NS_METHOD nsTableFrame::IR_TargetIsMe(nsIPresContext*        aPresContext,
                                      nsHTMLReflowMetrics&   aDesiredSize,
                                      InnerTableReflowState& aReflowState,
                                      nsReflowStatus&        aStatus)
{
  nsresult rv = NS_OK;
  aStatus = NS_FRAME_COMPLETE;
  nsIReflowCommand::ReflowType type;
  aReflowState.reflowState.reflowCommand->GetType(type);
  nsIFrame *objectFrame;
  aReflowState.reflowState.reflowCommand->GetChildFrame(objectFrame); 
  const nsStyleDisplay *childDisplay=nsnull;
  if (nsnull!=objectFrame)
    objectFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)childDisplay));
  switch (type)
  {
  case nsIReflowCommand::StyleChanged :
    rv = IR_StyleChanged(aPresContext, aDesiredSize, aReflowState, aStatus);
    break;

  case nsIReflowCommand::ContentChanged :
    NS_ASSERTION(PR_FALSE, "illegal reflow type: ContentChanged");
    rv = NS_ERROR_ILLEGAL_VALUE;
    break;
  
  case nsIReflowCommand::ReflowDirty:
    {
      //nsIFrame* dirtyChild;
      //PRInt32 numDirty;
      //if (GetDirtyChildren(this, &dirtyChild, numDirty)) {
      //  for (PRInt32 dirtyX = 0; dirtyX < numDirty; dirtyX++) {
      //    ResizeReflowPass1(aPresContext, aDesiredSize, aReflowState, aStatus, 
      //                      dirtyChild, reason, PR_FALSE);
      //    dirtyChild->GetNextSibling(&dirtyChild);
      //  }
      //}
      //else {
        // Problem is we don't know has changed, so assume the worst
        InvalidateFirstPassCache();
      //}
      InvalidateColumnWidths();
      rv = NS_OK;
      break;
    }
  
  default:
    NS_NOTYETIMPLEMENTED("unexpected reflow command type");
    rv = NS_ERROR_NOT_IMPLEMENTED;
    break;
  }

  return rv;
}

NS_METHOD nsTableFrame::IR_StyleChanged(nsIPresContext*        aPresContext,
                                        nsHTMLReflowMetrics&   aDesiredSize,
                                        InnerTableReflowState& aReflowState,
                                        nsReflowStatus&        aStatus)
{
  nsresult rv = NS_OK;
  // we presume that all the easy optimizations were done in the nsHTMLStyleSheet before we were called here
  // XXX: we can optimize this when we know which style attribute changed
  //      if something like border changes, we need to do pass1 again
  //      but if something like width changes from 100 to 200, we just need to do pass2
  InvalidateFirstPassCache();
  return rv;
}

// Recovers the reflow state to what it should be if aKidFrame is about
// to be reflowed. Restores the following:
// - availSize
// - y
// - footerFrame
// - firstBodySection
//
// XXX This is kind of klunky because the InnerTableReflowState::y
// data member does not include the table's border/padding...
nsresult
nsTableFrame::RecoverState(InnerTableReflowState& aReflowState,
                           nsIFrame*              aKidFrame,
                           nsSize*                aMaxElementSize)
{
  nscoord cellSpacingY = GetCellSpacingY();
  // Get the ordered children and find aKidFrame in the list
  nsVoidArray rowGroups;
  PRUint32 numRowGroups;
  OrderRowGroups(rowGroups, numRowGroups, &aReflowState.firstBodySection);
  
  // Walk the list of children looking for aKidFrame
  for (PRUint32 childX = 0; childX < numRowGroups; childX++) {
    nsIFrame* childFrame = (nsIFrame*)rowGroups.ElementAt(childX);
    nsTableRowGroupFrame* rgFrame = GetRowGroupFrame(childFrame);
    if (!rgFrame) continue; // skip foreign frame types
   
    // If this is a footer row group, remember it
    const nsStyleDisplay *display;
    rgFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)display));

    // We only allow a single footer frame
    if ((NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP == display->mDisplay) &&
        !aReflowState.footerFrame) {
      aReflowState.footerFrame = childFrame;    
    } 
    else {
      if ((NS_STYLE_DISPLAY_TABLE_ROW_GROUP == display->mDisplay) &&
          !aReflowState.firstBodySection) {
        aReflowState.firstBodySection = childFrame;
      }
    }
    aReflowState.y += cellSpacingY;
    
    // See if this is the frame we're looking for
    if (childFrame == aKidFrame) {
      break;
    }

    // Get the frame's height
    nsSize  kidSize;
    childFrame->GetSize(kidSize);
    
    // If our height is constrained then update the available height. Do
    // this for all frames including the footer frame
    if (PR_FALSE == aReflowState.unconstrainedHeight) {
      aReflowState.availSize.height -= kidSize.height;
    }

    // Update the running y-offset. Don't do this for the footer frame
    if (childFrame != aReflowState.footerFrame) {
      aReflowState.y += kidSize.height;
    }

    // Update the max element size
    //XXX: this should call into layout strategy to get the width field
    if (nsnull != aMaxElementSize) 
    {
      const nsStyleSpacing* tableSpacing;
      GetStyleData(eStyleStruct_Spacing , ((const nsStyleStruct *&)tableSpacing));
      nsMargin borderPadding;
      GetTableBorder (borderPadding); // gets the max border thickness for each edge
      borderPadding += aReflowState.reflowState.mComputedPadding;
      nscoord cellSpacing = GetCellSpacingX();
      nsSize  kidMaxElementSize;
      rgFrame->GetMaxElementSize(kidMaxElementSize);
      nscoord kidWidth = kidMaxElementSize.width + borderPadding.left + borderPadding.right + cellSpacing*2;
      aMaxElementSize->width = PR_MAX(aMaxElementSize->width, kidWidth); 
      aMaxElementSize->height += kidMaxElementSize.height;
    }
  }

  return NS_OK;
}

NS_METHOD nsTableFrame::IR_TargetIsChild(nsIPresContext*        aPresContext,
                                         nsHTMLReflowMetrics&   aDesiredSize,
                                         InnerTableReflowState& aReflowState,
                                         nsReflowStatus&        aStatus,
                                         nsIFrame *             aNextFrame)

{
  nsresult rv;
  // Recover the state as if aNextFrame is about to be reflowed
  RecoverState(aReflowState, aNextFrame, aDesiredSize.maxElementSize);

  // Remember the old rect
  nsRect  oldKidRect;
  aNextFrame->GetRect(oldKidRect);

  // Pass along the reflow command
  nsSize               kidMaxElementSize;
  nsHTMLReflowMetrics desiredSize(aDesiredSize.maxElementSize ? &kidMaxElementSize : nsnull,
                                  aDesiredSize.mFlags);
  nsHTMLReflowState kidReflowState(aPresContext, aReflowState.reflowState,
                                   aNextFrame, aReflowState.availSize);

  nscoord x = aReflowState.mBorderPadding.left;
  nscoord y = aReflowState.mBorderPadding.top + aReflowState.y;
  rv = ReflowChild(aNextFrame, aPresContext, desiredSize, kidReflowState,
                   x, y, 0, aStatus);

  // Place the row group frame. Don't use PlaceChild(), because it moves
  // the footer frame as well. We'll adjust the footer frame later on in
  // AdjustSiblingsAfterReflow()
  nsRect  kidRect(x, y, desiredSize.width, desiredSize.height);
  FinishReflowChild(aNextFrame, aPresContext, desiredSize, x, y, 0);

  // Adjust the running y-offset
  aReflowState.y += desiredSize.height + GetCellSpacingY();

  // If our height is constrained, then update the available height
  if (PR_FALSE == aReflowState.unconstrainedHeight) {
    aReflowState.availSize.height -= desiredSize.height;
  }

  // Update the max element size
  //XXX: this should call into layout strategy to get the width field
  if (nsnull != aDesiredSize.maxElementSize) 
  {
    const nsStyleSpacing* tableSpacing;
    GetStyleData(eStyleStruct_Spacing , ((const nsStyleStruct *&)tableSpacing));
    nsMargin borderPadding;
    GetTableBorder (borderPadding); // gets the max border thickness for each edge
    borderPadding += aReflowState.reflowState.mComputedPadding;
    nscoord cellSpacing = GetCellSpacingX();
    nscoord kidWidth = kidMaxElementSize.width + borderPadding.left + borderPadding.right + cellSpacing*2;
    aDesiredSize.maxElementSize->width = PR_MAX(aDesiredSize.maxElementSize->width, kidWidth); 
    aDesiredSize.maxElementSize->height += kidMaxElementSize.height;
  }

  // If the column width info is valid, then adjust the row group frames
  // that follow. Otherwise, return and we'll recompute the column widths
  // and reflow all the row group frames
  if (!NeedsReflow(aReflowState.reflowState)) {
    // If the row group frame changed height, then damage the horizontal strip
    // that was either added or went away
    if (desiredSize.height != oldKidRect.height) {
      nsRect  dirtyRect;

      dirtyRect.x = 0;
      dirtyRect.y = PR_MIN(oldKidRect.YMost(), kidRect.YMost());
      dirtyRect.width = mRect.width;
      dirtyRect.height = PR_MAX(oldKidRect.YMost(), kidRect.YMost()) -
                         dirtyRect.y;
      Invalidate(aPresContext, dirtyRect);
    }

    // Adjust the row groups that follow
    AdjustSiblingsAfterReflow(aPresContext, aReflowState, aNextFrame,
                              aDesiredSize.maxElementSize, desiredSize.height -
                              oldKidRect.height);

    // Return our size and our status
    aDesiredSize.width = ComputeDesiredWidth(aReflowState.reflowState);
    nscoord defaultHeight = aReflowState.y + aReflowState.mBorderPadding.top +
                            aReflowState.mBorderPadding.bottom;
    aDesiredSize.height = ComputeDesiredHeight(aPresContext, aReflowState.reflowState,
                                               defaultHeight);

    // XXX Is this needed?
#if 0
    AdjustForCollapsingRows(aPresContext, aDesiredSize.height);
    AdjustForCollapsingCols(aPresContext, aDesiredSize.width);

    // once horizontal borders are computed and all row heights are set, 
    // we need to fix up length of vertical edges
    // XXX need to figure start row and end row correctly
    if ((NS_STYLE_BORDER_COLLAPSE == GetBorderCollapseStyle()) && mBorderCollapser) {
      mBorderCollapser->DidComputeHorizontalBorders(aPresContext, 0, 10000);
    }
#endif
  }

  return rv;
}

nscoord nsTableFrame::ComputeDesiredWidth(const nsHTMLReflowState& aReflowState) const
{
  nscoord desiredWidth = aReflowState.availableWidth;
  if (NS_UNCONSTRAINEDSIZE == desiredWidth) { 
    // XXX this code path is not used, but if it is needed, then borders and padding must be added
    NS_ASSERTION(PR_FALSE, "code path in ComputeDesiredWidth is not complete");
    nsITableLayoutStrategy* tableLayoutStrategy = mTableLayoutStrategy;
    if (mPrevInFlow) {
      // Get the table layout strategy from the first-in-flow
      nsTableFrame* table = (nsTableFrame*)GetFirstInFlow();
      tableLayoutStrategy = table->mTableLayoutStrategy;
    }
    desiredWidth = tableLayoutStrategy->GetTableMaxWidth();
  }
  return desiredWidth;
}

// Position and size aKidFrame and update our reflow state. The origin of
// aKidRect is relative to the upper-left origin of our frame
void nsTableFrame::PlaceChild(nsIPresContext*        aPresContext,
                              InnerTableReflowState& aReflowState,
                              nsIFrame*              aKidFrame,
                              nsHTMLReflowMetrics&   aDesiredSize,
                              nscoord                aX,
                              nscoord                aY,
                              nsSize*                aMaxElementSize,
                              nsSize&                aKidMaxElementSize)
{
  // Place and size the child
  FinishReflowChild(aKidFrame, aPresContext, aDesiredSize, aX, aY, 0);

  // Adjust the running y-offset
  aReflowState.y += aDesiredSize.height;

  // If our height is constrained, then update the available height
  if (PR_FALSE == aReflowState.unconstrainedHeight) {
    aReflowState.availSize.height -= aDesiredSize.height;
  }

  const nsStyleDisplay *childDisplay;
  aKidFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)childDisplay));

  // We only allow a single footer frame, and the footer frame must occur before
  // any body section row groups
  if ((NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP == childDisplay->mDisplay) &&
      !aReflowState.footerFrame && !aReflowState.firstBodySection) {
    aReflowState.footerFrame = aKidFrame;
  }
  else if (aReflowState.footerFrame) {
    // put the non footer where the footer was
    nsPoint origin;
    aReflowState.footerFrame->GetOrigin(origin);
    aKidFrame->MoveTo(aPresContext, origin.x, origin.y);

    // put the footer below the non footer
    nsSize size;
    aReflowState.footerFrame->GetSize(size);
    origin.y = aReflowState.y - size.height;
    aReflowState.footerFrame->MoveTo(aPresContext, origin.x, origin.y);
  }

  //XXX: this should call into layout strategy to get the width field
  if (nsnull != aMaxElementSize) {
    const nsStyleSpacing* tableSpacing;
    GetStyleData(eStyleStruct_Spacing , ((const nsStyleStruct *&)tableSpacing));
    nsMargin borderPadding;
    GetTableBorder (borderPadding); // gets the max border thickness for each edge
    borderPadding += aReflowState.reflowState.mComputedPadding;
    nscoord cellSpacing = GetCellSpacingX();
    nscoord kidWidth = aKidMaxElementSize.width + borderPadding.left + borderPadding.right + cellSpacing*2;
    aMaxElementSize->width = PR_MAX(aMaxElementSize->width, kidWidth); 
    aMaxElementSize->height += aKidMaxElementSize.height;
  }
}

void
nsTableFrame::OrderRowGroups(nsVoidArray& aChildren,
                             PRUint32&    aNumRowGroups,
                             nsIFrame**   aFirstBody)
{
  aChildren.Clear();
  nsIFrame* head = nsnull;
  nsIFrame* foot = nsnull;
  if (aFirstBody) {
    aFirstBody = nsnull;
  }
  nsIFrame* kidFrame = mFrames.FirstChild();
  nsVoidArray nonRowGroups;
  // put the tbodies first, and the non row groups last
  while (kidFrame) {
    const nsStyleDisplay* kidDisplay;
    kidFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)kidDisplay));
    if (IsRowGroup(kidDisplay->mDisplay)) {
      switch(kidDisplay->mDisplay) {
      case NS_STYLE_DISPLAY_TABLE_HEADER_GROUP:
        if (head) { // treat additional thead like tbody
          aChildren.AppendElement(kidFrame);
        }
        else {
          head = kidFrame;
        }
        break;
      case NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP:
        if (foot) {
          aChildren.AppendElement(kidFrame);
        }
        else {
          foot = kidFrame;
        }
        break;
      default:
        aChildren.AppendElement(kidFrame);
        if (aFirstBody && !*aFirstBody) {
          *aFirstBody = kidFrame;
        }
      }
    }
    else {
      nonRowGroups.AppendElement(kidFrame);
    }

    kidFrame->GetNextSibling(&kidFrame);
  }
  aNumRowGroups = aChildren.Count();
  // put the thead first
  if (head) {
    aChildren.InsertElementAt(head, 0);
    aNumRowGroups++;
  }
  // put the tfoot after the last tbody
  if (foot) {
    aChildren.InsertElementAt(foot, aNumRowGroups);
    aNumRowGroups++;
  }
  // put the non row groups at the end
  PRInt32 numNonRowGroups = nonRowGroups.Count();
  for (PRInt32 i = 0; i < numNonRowGroups; i++) {
    aChildren.AppendElement(nonRowGroups.ElementAt(i));
  }
}

/**
 * Reflow the frames we've already created
 *
 * @param   aPresContext presentation context to use
 * @param   aReflowState current inline state
 * @return  true if we successfully reflowed all the mapped children and false
 *            otherwise, e.g. we pushed children to the next in flow
 */
NS_METHOD nsTableFrame::ReflowMappedChildren(nsIPresContext*        aPresContext,
                                             nsHTMLReflowMetrics&   aDesiredSize,
                                             InnerTableReflowState& aReflowState,
                                             nsReflowStatus&        aStatus)
{
  NS_PRECONDITION(mFrames.NotEmpty(), "no children");

  nsIFrame* prevKidFrame = nsnull;
  nsSize    kidMaxElementSize(0,0);
  nsSize*   pKidMaxElementSize = (nsnull != aDesiredSize.maxElementSize) ? &kidMaxElementSize : nsnull;
  nsresult  rv = NS_OK;
  nscoord   cellSpacingY = GetCellSpacingY();

  nsReflowReason reason;
  if (!IsAutoLayout(&aReflowState.reflowState)) {
    reason = aReflowState.reflowState.reason;
    if (eReflowReason_Incremental==reason) {
      reason = eReflowReason_Resize;
      if (aDesiredSize.maxElementSize) {
        aDesiredSize.maxElementSize->width = 0;
        aDesiredSize.maxElementSize->height = 0;
      }
    }
  }
  else {
    reason = eReflowReason_Resize;
  }

  // this never passes reflows down to colgroups
  nsVoidArray rowGroups;
  PRUint32 numRowGroups;
  OrderRowGroups(rowGroups, numRowGroups, &aReflowState.firstBodySection);
  PRUint32 numChildren = rowGroups.Count();

  for (PRUint32 childX = 0; childX < numChildren; childX++) {
    nsIFrame* kidFrame = (nsIFrame*)rowGroups.ElementAt(childX);
    nsSize              kidAvailSize(aReflowState.availSize);
    nsHTMLReflowMetrics desiredSize(pKidMaxElementSize);
    desiredSize.width = desiredSize.height = desiredSize.ascent = desiredSize.descent = 0;

    PRBool didFirstBody = PR_FALSE;
    if (childX < numRowGroups) {
      // Keep track of the first body section row group
      if (kidFrame == aReflowState.firstBodySection) {
        didFirstBody = PR_TRUE;
      }

      nsMargin borderPadding;
      GetTableBorderForRowGroup(GetRowGroupFrame(kidFrame), borderPadding);
      const nsStyleSpacing* tableSpacing;
      GetStyleData(eStyleStruct_Spacing, ((const nsStyleStruct *&)tableSpacing));
      borderPadding += aReflowState.reflowState.mComputedPadding;

      // Reflow the child into the available space
      nsHTMLReflowState  kidReflowState(aPresContext, aReflowState.reflowState,
                                        kidFrame, kidAvailSize, reason);
      // XXX fix up bad mComputedWidth for scroll frame
      kidReflowState.mComputedWidth = PR_MAX(kidReflowState.mComputedWidth, 0);

      if (didFirstBody && (kidFrame != aReflowState.firstBodySection)) {
        // If this isn't the first row group frame or the header or footer, then
        // we can't be at the top of the page anymore...
        kidReflowState.isTopOfPage = PR_FALSE;
      }

      aReflowState.y += cellSpacingY;
      nscoord x = borderPadding.left;
      nscoord y = aReflowState.y;
      
      if (RowGroupsShouldBeConstrained()) {
        // Only applies to the tree widget.
        nscoord tableSpecifiedHeight = CalcBorderBoxHeight(aPresContext, aReflowState.reflowState);
        if ((tableSpecifiedHeight != NS_UNCONSTRAINEDSIZE)) {
          kidReflowState.availableHeight = tableSpecifiedHeight - y;
          if (kidReflowState.availableHeight < 0)
            kidReflowState.availableHeight = 0;
        }
      }

      rv = ReflowChild(kidFrame, aPresContext, desiredSize, kidReflowState,
                       x, y, 0, aStatus);
      // Did the child fit?
      if (desiredSize.height > kidAvailSize.height) {
        if (aReflowState.firstBodySection && (kidFrame != aReflowState.firstBodySection)) {
          // The child is too tall to fit at all in the available space, and it's
          // not a header/footer or our first row group frame
          PushChildren(aPresContext, kidFrame, prevKidFrame);
          aStatus = NS_FRAME_NOT_COMPLETE;
          break;
        }
      }

      // Place the child
      // we don't want to adjust the maxElementSize if this is an initial reflow
      // it was set by the TableLayoutStrategy and shouldn't be changed.
      nsSize* requestedMaxElementSize = nsnull;
      if (eReflowReason_Initial != aReflowState.reflowState.reason)
        requestedMaxElementSize = aDesiredSize.maxElementSize;
      PlaceChild(aPresContext, aReflowState, kidFrame, desiredSize,
                 x, y, requestedMaxElementSize, kidMaxElementSize);

      // Remember where we just were in case we end up pushing children
      prevKidFrame = kidFrame;

      // Special handling for incomplete children
      if (NS_FRAME_IS_NOT_COMPLETE(aStatus)) {
        nsIFrame* kidNextInFlow;
         
        kidFrame->GetNextInFlow(&kidNextInFlow);
        if (nsnull == kidNextInFlow) {
          // The child doesn't have a next-in-flow so create a continuing
          // frame. This hooks the child into the flow
          nsIFrame*     continuingFrame;
          nsIPresShell* presShell;
          nsIStyleSet*  styleSet;

          aPresContext->GetShell(&presShell);
          presShell->GetStyleSet(&styleSet);
          NS_RELEASE(presShell);
          styleSet->CreateContinuingFrame(aPresContext, kidFrame, this, &continuingFrame);
          NS_RELEASE(styleSet);

          // Add the continuing frame to the sibling list
          nsIFrame* nextSib;
           
          kidFrame->GetNextSibling(&nextSib);
          continuingFrame->SetNextSibling(nextSib);
          kidFrame->SetNextSibling(continuingFrame);
        }
        // We've used up all of our available space so push the remaining
        // children to the next-in-flow
        nsIFrame* nextSibling;
         
        kidFrame->GetNextSibling(&nextSibling);
        if (nsnull != nextSibling) {
          PushChildren(aPresContext, nextSibling, kidFrame);
        }
        break;
      }
    }
    else {// it's an unknown frame type, give it a generic reflow and ignore the results
      nsHTMLReflowState kidReflowState(aPresContext, aReflowState.reflowState, kidFrame,
                                       nsSize(0,0), eReflowReason_Resize);
      nsHTMLReflowMetrics unusedDesiredSize(nsnull);
      nsReflowStatus status;
      ReflowChild(kidFrame, aPresContext, unusedDesiredSize, kidReflowState,
                  0, 0, 0, status);
      kidFrame->DidReflow(aPresContext, NS_FRAME_REFLOW_FINISHED);
    }
  }

  return rv;
}

/**
 * Try and pull-up frames from our next-in-flow
 *
 * @param   aPresContext presentation context to use
 * @param   aReflowState current inline state
 * @return  true if we successfully pulled-up all the children and false
 *            otherwise, e.g. child didn't fit
 */
NS_METHOD nsTableFrame::PullUpChildren(nsIPresContext* aPresContext,
                                       nsHTMLReflowMetrics& aDesiredSize,
                                       InnerTableReflowState& aReflowState,
                                       nsReflowStatus& aStatus)
{
  nsTableFrame*  nextInFlow = (nsTableFrame*)mNextInFlow;
  nsSize         kidMaxElementSize(0,0);
  nsSize*        pKidMaxElementSize = (nsnull != aDesiredSize.maxElementSize) ? &kidMaxElementSize : nsnull;
  nsIFrame*      prevKidFrame = mFrames.LastChild();
  nsresult       rv = NS_OK;

  while (nsnull != nextInFlow) {
    nsHTMLReflowMetrics kidSize(pKidMaxElementSize);
    kidSize.width=kidSize.height=kidSize.ascent=kidSize.descent=0;

    // XXX change to use nsFrameList::PullFrame

    // Get the next child
    nsIFrame* kidFrame = nextInFlow->mFrames.FirstChild();

    // Any more child frames?
    if (nsnull == kidFrame) {
      // No. Any frames on its overflow list?
      nsIFrame* nextOverflowFrames = nextInFlow->GetOverflowFrames(aPresContext, PR_TRUE);
      if (nextOverflowFrames) {
        // Move the overflow list to become the child list
        nextInFlow->mFrames.AppendFrames(nsnull, nextOverflowFrames);
        kidFrame = nextInFlow->mFrames.FirstChild();
      } else {
        // We've pulled up all the children, so move to the next-in-flow.
        nextInFlow->GetNextInFlow((nsIFrame**)&nextInFlow);
        continue;
      }
    }

    // See if the child fits in the available space. If it fits or
    // it's splittable then reflow it. The reason we can't just move
    // it is that we still need ascent/descent information
    nsSize            kidFrameSize(0,0);
    nsSplittableType  kidIsSplittable;

    kidFrame->GetSize(kidFrameSize);
    kidFrame->IsSplittable(kidIsSplittable);
    if ((kidFrameSize.height > aReflowState.availSize.height) &&
        NS_FRAME_IS_NOT_SPLITTABLE(kidIsSplittable)) {
      //XXX: Troy
      aStatus = NS_FRAME_NOT_COMPLETE;
      break;
    }
    nsHTMLReflowState  kidReflowState(aPresContext, aReflowState.reflowState,
                                      kidFrame, aReflowState.availSize,
                                      eReflowReason_Resize);

    rv = ReflowChild(kidFrame, aPresContext, kidSize, kidReflowState,
                     0, aReflowState.y, 0, aStatus);

    // Did the child fit?
    if ((kidSize.height > aReflowState.availSize.height) && mFrames.NotEmpty()) {
      // The child is too wide to fit in the available space, and it's
      // not our first child
      //XXX: Troy
      aStatus = NS_FRAME_NOT_COMPLETE;
      break;
    }

    const nsStyleDisplay *childDisplay;
    kidFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)childDisplay));
    if (PR_TRUE==IsRowGroup(childDisplay->mDisplay))
    {
      PlaceChild(aPresContext, aReflowState, kidFrame, kidSize, 0,
                 aReflowState.y, aDesiredSize.maxElementSize, *pKidMaxElementSize);
    } else {
      kidFrame->DidReflow(aPresContext, NS_FRAME_REFLOW_FINISHED);
    }

    // Remove the frame from its current parent
    nextInFlow->mFrames.RemoveFirstChild();

    // Link the frame into our list of children
    kidFrame->SetParent(this);

    if (nsnull == prevKidFrame) {
      mFrames.SetFrames(kidFrame);
    } else {
      prevKidFrame->SetNextSibling(kidFrame);
    }
    kidFrame->SetNextSibling(nsnull);

    // Remember where we just were in case we end up pushing children
    prevKidFrame = kidFrame;
    if (NS_FRAME_IS_NOT_COMPLETE(aStatus)) {
      // No the child isn't complete
      nsIFrame* kidNextInFlow;
       
      kidFrame->GetNextInFlow(&kidNextInFlow);
      if (nsnull == kidNextInFlow) {
        // The child doesn't have a next-in-flow so create a
        // continuing frame. The creation appends it to the flow and
        // prepares it for reflow.
        nsIFrame*     continuingFrame;
        nsIPresShell* presShell;
        nsIStyleSet*  styleSet;
        aPresContext->GetShell(&presShell);
        presShell->GetStyleSet(&styleSet);
        NS_RELEASE(presShell);
        styleSet->CreateContinuingFrame(aPresContext, kidFrame, this, &continuingFrame);
        NS_RELEASE(styleSet);

        // Add the continuing frame to our sibling list and then push
        // it to the next-in-flow. This ensures the next-in-flow's
        // content offsets and child count are set properly. Note that
        // we can safely assume that the continuation is complete so
        // we pass PR_TRUE into PushChidren
        kidFrame->SetNextSibling(continuingFrame);

        PushChildren(aPresContext, continuingFrame, kidFrame);
      }
      break;
    }
  }

  return rv;
}

/**
  Now I've got all the cells laid out in an infinite space.
  For each column, use the min size for each cell in that column
  along with the attributes of the table, column group, and column
  to assign widths to each column.
  */
// use the cell map to determine which cell is in which column.
void nsTableFrame::BalanceColumnWidths(nsIPresContext* aPresContext, 
                                       const nsHTMLReflowState& aReflowState,
                                       const nsSize& aMaxSize, 
                                       nsSize* aMaxElementSize)
{
  NS_ASSERTION(nsnull==mPrevInFlow, "never ever call me on a continuing frame!");
  nsTableCellMap* cellMap = GetCellMap();
  if (!cellMap) {
    NS_ASSERTION(PR_FALSE, "never ever call me until the cell map is built!");
    return;
  }

  PRInt32 numCols = cellMap->GetColCount();
  if (numCols>mColumnWidthsLength)
  {
    PRInt32 priorColumnWidthsLength=mColumnWidthsLength;
    if (0 == priorColumnWidthsLength) {
      mColumnWidthsLength = numCols;
    } else {
      while (numCols>mColumnWidthsLength)
        mColumnWidthsLength += kColumnWidthIncrement;
    }
    PRInt32 * newColumnWidthsArray = new PRInt32[mColumnWidthsLength];
    nsCRT::memset (newColumnWidthsArray, 0, mColumnWidthsLength*sizeof(PRInt32));
    if (mColumnWidths) {
      nsCRT::memcpy (newColumnWidthsArray, mColumnWidths, priorColumnWidthsLength*sizeof(PRInt32));
      delete [] mColumnWidths;
    }
    mColumnWidths = newColumnWidthsArray;
  }

  // need to figure out the overall table width constraint
  // default case, get 100% of available space

  PRInt32 maxWidth = CalcBorderBoxWidth(aReflowState);

  // based on the compatibility mode, create a table layout strategy
  nscoord boxWidth = CalcBorderBoxWidth(aReflowState);
  if (nsnull == mTableLayoutStrategy) {
    nsCompatibility mode;
    aPresContext->GetCompatibilityMode(&mode);
    if (!IsAutoLayout(&aReflowState))
      mTableLayoutStrategy = new FixedTableLayoutStrategy(this);
    else
      mTableLayoutStrategy = new BasicTableLayoutStrategy(this, eCompatibility_NavQuirks == mode);
    mTableLayoutStrategy->Initialize(aPresContext, aMaxElementSize, boxWidth, aReflowState);
    mBits.mColumnWidthsValid=PR_TRUE;
  }
  // fixed-layout tables need to reinitialize the layout strategy. When there are scroll bars
  // reflow gets called twice and the 2nd time has the correct space available.
  else if (!IsAutoLayout(&aReflowState)) {
    mTableLayoutStrategy->Initialize(aPresContext, aMaxElementSize, boxWidth, aReflowState);
  }

  mTableLayoutStrategy->BalanceColumnWidths(aPresContext, mStyleContext, aReflowState, maxWidth);
  //Dump(PR_TRUE, PR_TRUE);
  mBits.mColumnWidthsSet=PR_TRUE;

  // if collapsing borders, compute the top and bottom edges now that we have column widths
  if ((NS_STYLE_BORDER_COLLAPSE == GetBorderCollapseStyle()) && mBorderCollapser) {
    mBorderCollapser->ComputeHorizontalBorders(aPresContext, 0, cellMap->GetRowCount()-1);
  }
}

/**
  sum the width of each column
  add in table insets
  set rect
  */
void nsTableFrame::SetTableWidth(nsIPresContext*          aPresContext,
                                 const nsHTMLReflowState& aReflowState)
{
  NS_ASSERTION(nsnull==mPrevInFlow, "never ever call me on a continuing frame!");
  nsTableCellMap* cellMap = GetCellMap();
  if (!cellMap) {
    NS_ASSERTION(PR_FALSE, "never ever call me until the cell map is built!");
    return;
  }

  nscoord cellSpacing = GetCellSpacingX();
  PRInt32 tableWidth = 0;

  PRInt32 numCols = GetColCount();
  for (PRInt32 colIndex = 0; colIndex < numCols; colIndex++) {
    nscoord totalColWidth = mColumnWidths[colIndex];
    if (GetNumCellsOriginatingInCol(colIndex) > 0) { // skip degenerate cols
      totalColWidth += cellSpacing;           // add cell spacing to left of col
    }
    tableWidth += totalColWidth;
  }

  if (numCols > 0) {
    tableWidth += cellSpacing; // add last cellspacing
  } 
  else if (0 == tableWidth)  {
    nsRect tableRect = mRect;
    tableRect.width = 0;
    SetRect(aPresContext, tableRect);
    return;
  }

  // Compute the insets (sum of border and padding)
  nsMargin borderPadding;
  GetTableBorder (borderPadding); // this gets the max border value at every edge
  borderPadding += aReflowState.mComputedPadding;

  nscoord rightInset = borderPadding.right;
  nscoord leftInset = borderPadding.left;
  tableWidth += (leftInset + rightInset);
  nsRect tableSize = mRect;
  tableSize.width = tableWidth;
  SetRect(aPresContext, tableSize);
}

/**
  get the table height attribute
  if it is auto, table height = SUM(height of rowgroups)
  else if (resolved table height attribute > SUM(height of rowgroups))
    proportionately distribute extra height to each row
  we assume we are passed in the default table height==the sum of the heights of the table's rowgroups
  in aDesiredSize.height.
  */
void nsTableFrame::DistributeSpaceToCells(nsIPresContext* aPresContext, 
                                   const nsHTMLReflowState& aReflowState,
                                   nsIFrame* aRowGroupFrame)
{
  // now that all of the rows have been resized, resize the cells       
  nsTableRowGroupFrame* rowGroupFrame = (nsTableRowGroupFrame*)aRowGroupFrame;
  nsIFrame * rowFrame = rowGroupFrame->GetFirstFrame();
  while (nsnull!=rowFrame) {
    const nsStyleDisplay *rowDisplay;
    rowFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)rowDisplay));
    if (NS_STYLE_DISPLAY_TABLE_ROW == rowDisplay->mDisplay) { 
      ((nsTableRowFrame *)rowFrame)->DidResize(aPresContext, aReflowState);
    }
    // The calling function, DistributeSpaceToRows, takes care of the recursive row
    // group descent, which is why there's no NS_STYLE_DISPLAY_TABLE_ROW_GROUP case
    // here.
    rowGroupFrame->GetNextFrame(rowFrame, &rowFrame);
  }
}

void nsTableFrame::DistributeSpaceToRows(nsIPresContext*   aPresContext,
                                  const nsHTMLReflowState& aReflowState,
                                  nsIFrame*                aRowGroupFrame, 
                                  nscoord                  aSumOfRowHeights,
                                  nscoord                  aExcess, 
                                  nscoord&                 aExcessForRowGroup, 
                                  nscoord&                 aRowGroupYPos)
{
  // the rows in rowGroupFrame need to be expanded by rowHeightDelta[i]
  // and the rowgroup itself needs to be expanded by SUM(row height deltas)
  nscoord cellSpacingY = GetCellSpacingY();
  nsTableRowGroupFrame* rowGroupFrame = (nsTableRowGroupFrame*)aRowGroupFrame;
  nsIFrame* rowFrame = rowGroupFrame->GetFirstFrame();
  nscoord y = 0;
  while (rowFrame) {
    const nsStyleDisplay *rowDisplay;
    rowFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)rowDisplay));
    if (NS_STYLE_DISPLAY_TABLE_ROW_GROUP == rowDisplay->mDisplay) {
      DistributeSpaceToRows(aPresContext, aReflowState, rowFrame, aSumOfRowHeights, 
                            aExcess, aExcessForRowGroup, y);
    }
    else if (NS_STYLE_DISPLAY_TABLE_ROW == rowDisplay->mDisplay) { 
      // the row needs to be expanded by the proportion this row contributed to the original height
      nsRect rowRect;
      rowFrame->GetRect(rowRect);
      float percent = ((float)(rowRect.height)) / (float)aSumOfRowHeights;
      nscoord excessForRow = NSToCoordRound((float)aExcess * percent);

      if (rowGroupFrame->RowsDesireExcessSpace()) {
        nsRect newRowRect(rowRect.x, y, rowRect.width, excessForRow + rowRect.height);
        rowFrame->SetRect(aPresContext, newRowRect);
        if ((NS_STYLE_BORDER_COLLAPSE == GetBorderCollapseStyle()) && mBorderCollapser) {
          PRInt32 rowIndex = ((nsTableRowFrame*)rowFrame)->GetRowIndex();
          mBorderCollapser->SetBorderEdgeLength(NS_SIDE_LEFT,  rowIndex, newRowRect.height);
          mBorderCollapser->SetBorderEdgeLength(NS_SIDE_RIGHT, rowIndex, newRowRect.height);
        }
        // better if this were part of an overloaded row::SetRect
        y += excessForRow + rowRect.height;
      }
      y += cellSpacingY;

      aExcessForRowGroup += excessForRow;
    }
    else { // XXX why
      nsRect rowRect;
      rowFrame->GetRect(rowRect);
      y += rowRect.height;
    }

    rowGroupFrame->GetNextFrame(rowFrame, &rowFrame);
  }

  nsRect rowGroupRect;
  aRowGroupFrame->GetRect(rowGroupRect);  
  if (rowGroupFrame->RowGroupDesiresExcessSpace()) {
    nsRect newRowGroupRect(rowGroupRect.x, aRowGroupYPos, rowGroupRect.width, 
                           aExcessForRowGroup + rowGroupRect.height);
    aRowGroupFrame->SetRect(aPresContext, newRowGroupRect);
    aRowGroupYPos += aExcessForRowGroup + rowGroupRect.height;
  }
  else {
    aRowGroupYPos += rowGroupRect.height;
  }

  DistributeSpaceToCells(aPresContext, aReflowState, aRowGroupFrame);
}

nscoord nsTableFrame::ComputeDesiredHeight(nsIPresContext*          aPresContext,
                                           const nsHTMLReflowState& aReflowState, 
                                           nscoord                  aDefaultHeight) 
{
  nsTableCellMap* cellMap = GetCellMap();
  if (!cellMap) {
    NS_ASSERTION(PR_FALSE, "never ever call me until the cell map is built!");
    return 0;
  }
  nscoord result = aDefaultHeight;
  nscoord cellSpacingY = GetCellSpacingY();

  nscoord tableSpecifiedHeight = CalcBorderBoxHeight(aPresContext, aReflowState);
  if ((tableSpecifiedHeight > 0) && (tableSpecifiedHeight != NS_UNCONSTRAINEDSIZE)) {
    if (tableSpecifiedHeight > aDefaultHeight) {
      result = tableSpecifiedHeight;

      if (NS_UNCONSTRAINEDSIZE != aReflowState.availableWidth) { 
        // proportionately distribute the excess height to each row. Note that we
        // don't need to do this if it's an unconstrained reflow
        nscoord excess = tableSpecifiedHeight - aDefaultHeight;
        nscoord sumOfRowHeights = 0;
        nscoord rowGroupYPos = aReflowState.mComputedBorderPadding.top + cellSpacingY;

        nsVoidArray rowGroups;
        PRUint32 numRowGroups;
        OrderRowGroups(rowGroups, numRowGroups, nsnull);

        PRUint32 rgX;
        for (rgX = 0; rgX < numRowGroups; rgX++) {
          nsTableRowGroupFrame* rgFrame = 
            GetRowGroupFrame((nsIFrame*)rowGroups.ElementAt(rgX));
          if (rgFrame) {
            if (rgFrame->RowGroupReceivesExcessSpace()) { 
              rgFrame->GetHeightOfRows(aPresContext, sumOfRowHeights);
            }
          }
        }
  
        for (rgX = 0; rgX < numRowGroups; rgX++) {
          nsTableRowGroupFrame* rgFrame = 
            GetRowGroupFrame((nsIFrame*)rowGroups.ElementAt(rgX));
          if (rgFrame) {
            if (rgFrame->RowGroupReceivesExcessSpace()) {
              nscoord excessForGroup = 0;
              const nsStyleTable* tableStyle;
              GetStyleData(eStyleStruct_Table, (const nsStyleStruct *&)tableStyle);
              DistributeSpaceToRows(aPresContext, aReflowState, rgFrame, sumOfRowHeights, 
                                    excess, excessForGroup, rowGroupYPos);
  
              // Make sure child views are properly positioned
              // XXX what happens if childFrame is a scroll frame and this gets skipped?
              nsIView*  view;
              rgFrame->GetView(aPresContext, &view);
              if (view) {
                nsContainerFrame::PositionFrameView(aPresContext, rgFrame, view);
              } else {
                nsContainerFrame::PositionChildViews(aPresContext, rgFrame);
              }
            }
            else {
              nsRect rowGroupRect;
              rgFrame->GetRect(rowGroupRect);
              rowGroupYPos += rowGroupRect.height;
            }
            rowGroupYPos += cellSpacingY;
          }
        }
      }
    }
  }
  return result;
}


NS_METHOD nsTableFrame::GetColumnFrame(PRInt32 aColIndex, nsTableColFrame *&aColFrame)
{
  aColFrame = (nsTableColFrame *)mColFrames.ElementAt(aColIndex);
  return NS_OK;
}

PRBool nsTableFrame::IsColumnWidthsSet()
{ 
  nsTableFrame * firstInFlow = (nsTableFrame *)GetFirstInFlow();
  NS_ASSERTION(nsnull!=firstInFlow, "illegal state -- no first in flow");
  return (PRBool)firstInFlow->mBits.mColumnWidthsSet; 
}

PRBool nsTableFrame::ColumnsCanBeInvalidatedBy(nsStyleCoord*           aPrevStyleWidth,
                                               const nsTableCellFrame& aCellFrame) const
{
  if (mTableLayoutStrategy) {
    return mTableLayoutStrategy->ColumnsCanBeInvalidatedBy(aPrevStyleWidth, aCellFrame);
  }
  return PR_FALSE;
}

PRBool nsTableFrame::ColumnsCanBeInvalidatedBy(const nsTableCellFrame& aCellFrame,
                                               PRBool                  aConsiderMinWidth) const

{
  if (mTableLayoutStrategy) {
    return mTableLayoutStrategy->ColumnsCanBeInvalidatedBy(aCellFrame, aConsiderMinWidth);
  }
  return PR_FALSE;
}

PRBool nsTableFrame::ColumnsAreValidFor(const nsTableCellFrame& aCellFrame,
                                        nscoord                 aPrevCellMin,
                                        nscoord                 aPrevCellDes) const
{
  if (mTableLayoutStrategy) {
    return mTableLayoutStrategy->ColumnsAreValidFor(aCellFrame, aPrevCellMin, aPrevCellDes);
  }
  return PR_FALSE;
}

// XXX This could be more incremental  
void nsTableFrame::InvalidateColumnWidths()
{
  nsTableFrame * firstInFlow = (nsTableFrame *)GetFirstInFlow();
  NS_ASSERTION(nsnull!=firstInFlow, "illegal state -- no first in flow");
  firstInFlow->mBits.mColumnWidthsValid=PR_FALSE;
}

PRBool nsTableFrame::IsColumnWidthsValid() const
{
  nsTableFrame * firstInFlow = (nsTableFrame *)GetFirstInFlow();
  NS_ASSERTION(nsnull!=firstInFlow, "illegal state -- no first in flow");
  return (PRBool)firstInFlow->mBits.mColumnWidthsValid;
}

PRBool nsTableFrame::IsFirstPassValid() const
{
  nsTableFrame * firstInFlow = (nsTableFrame *)GetFirstInFlow();
  NS_ASSERTION(nsnull!=firstInFlow, "illegal state -- no first in flow");
  return (PRBool)firstInFlow->mBits.mFirstPassValid;
}

void nsTableFrame::InvalidateFirstPassCache()
{
  nsTableFrame * firstInFlow = (nsTableFrame *)GetFirstInFlow();
  NS_ASSERTION(nsnull!=firstInFlow, "illegal state -- no first in flow");
  firstInFlow->mBits.mFirstPassValid=PR_FALSE;
}

PRBool nsTableFrame::IsMaximumWidthValid() const
{
  nsTableFrame * firstInFlow = (nsTableFrame *)GetFirstInFlow();
  NS_ASSERTION(nsnull!=firstInFlow, "illegal state -- no first in flow");
  return (PRBool)firstInFlow->mBits.mMaximumWidthValid;
}

void nsTableFrame::InvalidateMaximumWidth()
{
  nsTableFrame * firstInFlow = (nsTableFrame *)GetFirstInFlow();
  NS_ASSERTION(nsnull!=firstInFlow, "illegal state -- no first in flow");
  firstInFlow->mBits.mMaximumWidthValid=PR_FALSE;
}

PRInt32 nsTableFrame::GetColumnWidth(PRInt32 aColIndex)
{
  nsTableFrame * firstInFlow = (nsTableFrame *)GetFirstInFlow();
  NS_ASSERTION(nsnull!=firstInFlow, "illegal state -- no first in flow");
  PRInt32 result = 0;
  if (this!=firstInFlow)
    result = firstInFlow->GetColumnWidth(aColIndex);
  else
  {
    // Because we lazily allocate the column widths when balancing the
    // columns, it may not be allocated yet. For example, if this is
    // an incremental reflow. That's okay, just return 0 for the column
    // width
#ifdef NS_DEBUG
    nsTableCellMap* cellMap = GetCellMap();
    if (!cellMap) {
      NS_ASSERTION(PR_FALSE, "no cell map");
      return 0;
    }
    PRInt32 numCols = cellMap->GetColCount();
    NS_ASSERTION (numCols > aColIndex, "bad arg, col index out of bounds");
#endif
    if (nsnull!=mColumnWidths)
     result = mColumnWidths[aColIndex];
  }

  return result;
}

void  nsTableFrame::SetColumnWidth(PRInt32 aColIndex, nscoord aWidth)
{
  nsTableFrame * firstInFlow = (nsTableFrame *)GetFirstInFlow();
  NS_ASSERTION(nsnull!=firstInFlow, "illegal state -- no first in flow");

  if (this!=firstInFlow)
    firstInFlow->SetColumnWidth(aColIndex, aWidth);
  else
  {
    // Note: in the case of incremental reflow sometimes the table layout
    // strategy will call to set a column width before we've allocated the
    // column width array
    if (!mColumnWidths) {
      mColumnWidthsLength = mCellMap->GetColCount(); // mCellMap is valid since first inflow
      mColumnWidths = new PRInt32[mColumnWidthsLength];
      nsCRT::memset (mColumnWidths, 0, mColumnWidthsLength*sizeof(PRInt32));
    }
    
    if (nsnull!=mColumnWidths && aColIndex<mColumnWidthsLength) {
      mColumnWidths[aColIndex] = aWidth;
    }
  }
}

/**
  *
  * Update the border style to map to the HTML border style
  *
  */
void nsTableFrame::MapHTMLBorderStyle(nsStyleSpacing& aSpacingStyle, nscoord aBorderWidth)
{
  nsStyleCoord  width;
  width.SetCoordValue(aBorderWidth);
  aSpacingStyle.mBorder.SetTop(width);
  aSpacingStyle.mBorder.SetLeft(width);
  aSpacingStyle.mBorder.SetBottom(width);
  aSpacingStyle.mBorder.SetRight(width);

  aSpacingStyle.SetBorderStyle(NS_SIDE_TOP, NS_STYLE_BORDER_STYLE_BG_OUTSET);
  aSpacingStyle.SetBorderStyle(NS_SIDE_LEFT, NS_STYLE_BORDER_STYLE_BG_OUTSET);
  aSpacingStyle.SetBorderStyle(NS_SIDE_BOTTOM, NS_STYLE_BORDER_STYLE_BG_OUTSET);
  aSpacingStyle.SetBorderStyle(NS_SIDE_RIGHT, NS_STYLE_BORDER_STYLE_BG_OUTSET);

  nsIStyleContext* styleContext = mStyleContext; 
  const nsStyleColor* colorData = (const nsStyleColor*)
    styleContext->GetStyleData(eStyleStruct_Color);

  // Look until we find a style context with a NON-transparent background color
  while (styleContext)
  {
    if ((colorData->mBackgroundFlags & NS_STYLE_BG_COLOR_TRANSPARENT)!=0)
    {
      nsIStyleContext* temp = styleContext;
      styleContext = styleContext->GetParent();
      if (temp != mStyleContext)
        NS_RELEASE(temp);
      colorData = (const nsStyleColor*)styleContext->GetStyleData(eStyleStruct_Color);
    }
    else
    {
      break;
    }
  }

  // Yaahoo, we found a style context which has a background color 
  
  nscolor borderColor = 0xFFC0C0C0;

  if (styleContext != nsnull)
  {
    borderColor = colorData->mBackgroundColor;
    if (styleContext != mStyleContext)
      NS_RELEASE(styleContext);
  }

  // if the border color is white, then shift to grey
  if (borderColor == 0xFFFFFFFF)
    borderColor = 0xFFC0C0C0;

  aSpacingStyle.SetBorderColor(NS_SIDE_TOP, borderColor);
  aSpacingStyle.SetBorderColor(NS_SIDE_LEFT, borderColor);
  aSpacingStyle.SetBorderColor(NS_SIDE_BOTTOM, borderColor);
  aSpacingStyle.SetBorderColor(NS_SIDE_RIGHT, borderColor);

}



PRBool nsTableFrame::ConvertToPixelValue(nsHTMLValue& aValue, PRInt32 aDefault, PRInt32& aResult)
{
  if (aValue.GetUnit() == eHTMLUnit_Pixel)
    aResult = aValue.GetPixelValue();
  else if (aValue.GetUnit() == eHTMLUnit_Empty)
    aResult = aDefault;
  else
  {
    NS_ERROR("Unit must be pixel or empty");
    return PR_FALSE;
  }
  return PR_TRUE;
}

void nsTableFrame::MapBorderMarginPadding(nsIPresContext* aPresContext)
{
#if 0
  // Check to see if the table has either cell padding or 
  // Cell spacing defined for the table. If true, then
  // this setting overrides any specific border, margin or 
  // padding information in the cell. If these attributes
  // are not defined, the the cells attributes are used
  
  nsHTMLValue padding_value;
  nsHTMLValue spacing_value;
  nsHTMLValue border_value;


  nsresult border_result;

  nscoord   padding = 0;
  nscoord   spacing = 0;
  nscoord   border  = 1;

  float     p2t = aPresContext->GetPixelsToTwips();

  nsIHTMLContent*  table = (nsIHTMLContent*)mContent;

  NS_ASSERTION(table,"Table Must not be null");
  if (!table)
    return;

  nsStyleSpacing* spacingData = (nsStyleSpacing*)mStyleContext->GetMutableStyleData(eStyleStruct_Spacing);

  border_result = table->GetAttribute(nsHTMLAtoms::border,border_value);
  if (border_result == NS_CONTENT_ATTR_HAS_VALUE)
  {
    PRInt32 intValue = 0;

    if (ConvertToPixelValue(border_value,1,intValue)) //XXX this is busted if this code is ever used again. MMP
      border = NSIntPixelsToTwips(intValue, p2t); 
  }
  MapHTMLBorderStyle(*spacingData,border);
#endif
}

nscoord 
CalcPercentPadding(nscoord      aBasis,
                   nsStyleCoord aStyleCoord)
{
  float percent = (NS_UNCONSTRAINEDSIZE == aBasis)
                  ? 0 : aStyleCoord.GetPercentValue();
  return NSToCoordRound(((float)aBasis) * percent);
}

void 
GetPaddingFor(const nsSize&         aBasis, 
              const nsStyleSpacing& aSpacing, 
              nsMargin&             aPadding)
{
  nsStyleCoord styleCoord;
  aSpacing.mPadding.GetTop(styleCoord);
  if (eStyleUnit_Percent == aSpacing.mPadding.GetTopUnit()) {
    aPadding.top = CalcPercentPadding(aBasis.height, styleCoord);
  }
  else if (eStyleUnit_Coord == aSpacing.mPadding.GetTopUnit()) {
    aPadding.top = styleCoord.GetCoordValue();
  }

  aSpacing.mPadding.GetRight(styleCoord);
  if (eStyleUnit_Percent == aSpacing.mPadding.GetRightUnit()) {
    aPadding.right = CalcPercentPadding(aBasis.width, styleCoord);
  }
  else if (eStyleUnit_Coord == aSpacing.mPadding.GetTopUnit()) {
    aPadding.right = styleCoord.GetCoordValue();
  }

  aSpacing.mPadding.GetBottom(styleCoord);
  if (eStyleUnit_Percent == aSpacing.mPadding.GetBottomUnit()) {
    aPadding.bottom = CalcPercentPadding(aBasis.height, styleCoord);
  }
  else if (eStyleUnit_Coord == aSpacing.mPadding.GetTopUnit()) {
    aPadding.bottom = styleCoord.GetCoordValue();
  }

  aSpacing.mPadding.GetLeft(styleCoord);
  if (eStyleUnit_Percent == aSpacing.mPadding.GetLeftUnit()) {
    aPadding.left = CalcPercentPadding(aBasis.width, styleCoord);
  }
  else if (eStyleUnit_Coord == aSpacing.mPadding.GetTopUnit()) {
    aPadding.left = styleCoord.GetCoordValue();
  }
}

nsMargin
nsTableFrame::GetPadding(const nsHTMLReflowState& aReflowState,
                         const nsTableCellFrame*  aCellFrame)
{
  const nsStyleSpacing* spacing;
  aCellFrame->GetStyleData(eStyleStruct_Spacing,(const nsStyleStruct *&)spacing);
  nsMargin padding(0,0,0,0);
  if (!spacing->GetPadding(padding)) {
    const nsHTMLReflowState* parentRS = aReflowState.parentReflowState;
    while (parentRS) {
      if (parentRS->frame) {
        nsCOMPtr<nsIAtom> frameType;
        parentRS->frame->GetFrameType(getter_AddRefs(frameType));
        if (nsLayoutAtoms::tableFrame == frameType.get()) {
          nsSize basis(parentRS->mComputedWidth, parentRS->mComputedHeight);
          GetPaddingFor(basis, *spacing, padding);
          break;
        }
      }
      parentRS = parentRS->parentReflowState;
    }
  }
  return padding;
}

nsMargin
nsTableFrame::GetPadding(const nsSize&           aBasis,
                         const nsTableCellFrame* aCellFrame)
{
  const nsStyleSpacing* spacing;
  aCellFrame->GetStyleData(eStyleStruct_Spacing,(const nsStyleStruct *&)spacing);
  nsMargin padding(0,0,0,0);
  if (!spacing->GetPadding(padding)) {
    GetPaddingFor(aBasis, *spacing, padding);
  }
  return padding;
}

//XXX: ok, this looks dumb now.  but in a very short time this will get filled in
void nsTableFrame::GetTableBorder(nsMargin &aBorder)
{
  if (NS_STYLE_BORDER_COLLAPSE == GetBorderCollapseStyle()) {
    mBorderCollapser->GetBorder(aBorder);
  }
  else {
    const nsStyleSpacing* spacing =
      (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);
    spacing->GetBorder(aBorder);
  }
}

/*
now I need to actually use gettableborderat, instead of assuming that the table border is homogenous
across rows and columns.  in tableFrame, LayoutStrategy, and cellFrame, maybe rowFrame
need something similar for cell (for those with spans?)
*/

void nsTableFrame::GetTableBorderAt(PRInt32   aRowIndex, 
                                    PRInt32   aColIndex,
                                    nsMargin& aBorder)
{
  if (NS_STYLE_BORDER_COLLAPSE==GetBorderCollapseStyle()) {
    if (mBorderCollapser) {
      mBorderCollapser->GetBorderAt(aRowIndex, aColIndex, aBorder);
    }
  }
  else {
    const nsStyleSpacing* spacing =
      (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);
    spacing->GetBorder(aBorder);
  }
}

void nsTableFrame::SetBorderEdgeLength(PRUint8 aSide, 
                                       PRInt32 aIndex, 
                                       nscoord aLength)
{
  if (mBorderCollapser) {
    mBorderCollapser->SetBorderEdgeLength(aSide, aIndex, aLength);
  }
}

void nsTableFrame::GetTableBorderForRowGroup(nsTableRowGroupFrame* aRowGroupFrame, 
                                             nsMargin&             aBorder)
{
  aBorder.SizeTo(0,0,0,0);
  if (aRowGroupFrame) {
    if (NS_STYLE_BORDER_COLLAPSE == GetBorderCollapseStyle()) {
      if (mBorderCollapser) {
        PRInt32 rowCount;
        aRowGroupFrame->GetRowCount(rowCount);
        nsTableCellMap* cellMap = GetCellMap();
        //PRInt32 colCount = cellMap->GetColCount();
        mBorderCollapser->GetMaxBorder(aRowGroupFrame->GetStartRowIndex(), rowCount - 1,
                                       0, cellMap->GetColCount() - 1, aBorder);
      }
    }
    else
    {
      GetTableBorder (aBorder);
    }
  }
}

PRUint8 nsTableFrame::GetBorderCollapseStyle()
{
  /* the following has been commented out to turn off collapsing borders
  const nsStyleTable* tableStyle;
  GetStyleData(eStyleStruct_Table, (const nsStyleStruct *&)tableStyle);
  return tableStyle->mBorderCollapse;*/
  return NS_STYLE_BORDER_SEPARATE;
}


// XXX: could cache this.  But be sure to check style changes if you do!
nscoord nsTableFrame::GetCellSpacingX()
{
  const nsStyleTable* tableStyle;
  GetStyleData(eStyleStruct_Table, (const nsStyleStruct *&)tableStyle);
  nscoord cellSpacing = 0;
  PRUint8 borderCollapseStyle = GetBorderCollapseStyle();
  if (NS_STYLE_BORDER_COLLAPSE != borderCollapseStyle) {
    if (tableStyle->mBorderSpacingX.GetUnit() == eStyleUnit_Coord) {
      cellSpacing = tableStyle->mBorderSpacingX.GetCoordValue();
    }
  }
  return cellSpacing;
}

// XXX: could cache this. But be sure to check style changes if you do!
nscoord nsTableFrame::GetCellSpacingY()
{
  const nsStyleTable* tableStyle;
  GetStyleData(eStyleStruct_Table, (const nsStyleStruct *&)tableStyle);
  nscoord cellSpacing = 0;
  PRUint8 borderCollapseStyle = GetBorderCollapseStyle();
  if (NS_STYLE_BORDER_COLLAPSE != borderCollapseStyle) {
    if (tableStyle->mBorderSpacingY.GetUnit() == eStyleUnit_Coord) {
      cellSpacing = tableStyle->mBorderSpacingY.GetCoordValue();
    }
  }
  return cellSpacing;
}

// Get the cellpadding defined on the table. Each cell can override this with style
nscoord nsTableFrame::GetCellPadding()
{
  const nsStyleTable* tableStyle;
  GetStyleData(eStyleStruct_Table, (const nsStyleStruct *&)tableStyle);
  nscoord cellPadding = -1;
  if (tableStyle->mCellPadding.GetUnit() == eStyleUnit_Coord) {
    cellPadding = tableStyle->mCellPadding.GetCoordValue();
  }
  return cellPadding;
}


/* ----- global methods ----- */

nsresult 
NS_NewTableFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsTableFrame* it = new (aPresShell) nsTableFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

NS_METHOD nsTableFrame::GetTableFrame(nsIFrame *aSourceFrame, nsTableFrame *& aTableFrame)
{
  nsresult rv = NS_ERROR_UNEXPECTED;  // the value returned
  aTableFrame = nsnull;               // initialize out-param
  nsIFrame *parentFrame=nsnull;
  if (nsnull!=aSourceFrame)
  {
    // "result" is the result of intermediate calls, not the result we return from this method
    nsresult result = aSourceFrame->GetParent((nsIFrame **)&parentFrame); 
    while ((NS_OK==result) && (nsnull!=parentFrame))
    {
      const nsStyleDisplay *display;
      parentFrame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct *&)display);
      if (NS_STYLE_DISPLAY_TABLE == display->mDisplay)
      {
        aTableFrame = (nsTableFrame *)parentFrame;
        rv = NS_OK; // only set if we found the table frame
        break;
      }
      result = parentFrame->GetParent((nsIFrame **)&parentFrame);
    }
  }
  NS_POSTCONDITION(nsnull!=aTableFrame, "unable to find table parent. aTableFrame null.");
  NS_POSTCONDITION(NS_OK==rv, "unable to find table parent. result!=NS_OK");
  return rv;
}

/* helper method for determining if this is a nested table or not */
// aReflowState must be the reflow state for this inner table frame, should have an assertion here for that
PRBool 
nsTableFrame::IsNested(const nsHTMLReflowState& aReflowState,
                       const nsStylePosition*&  aPosition) const
{
  aPosition = nsnull;
  PRBool result = PR_FALSE;
  // Walk up the reflow state chain until we find a cell or the root
  const nsHTMLReflowState* rs = GetGrandParentReflowState(aReflowState);
  while (rs) {
    nsCOMPtr<nsIAtom> frameType;
    rs->frame->GetFrameType(getter_AddRefs(frameType));
    if (nsLayoutAtoms::tableFrame == frameType.get()) {
      result = PR_TRUE;
      rs->frame->GetStyleData(eStyleStruct_Position, ((const nsStyleStruct *&)aPosition));
      break;
    }
    rs = rs->parentReflowState;
  }
  return result;
}

PRBool nsTableFrame::IsAutoWidth()
{
  PRBool isAuto = PR_TRUE;  // the default

  nsStylePosition* tablePosition = (nsStylePosition*)mStyleContext->GetStyleData(eStyleStruct_Position);
  switch (tablePosition->mWidth.GetUnit()) {

  case eStyleUnit_Auto:         // specified auto width
  case eStyleUnit_Proportional: // illegal for table, so ignored
    break;
  case eStyleUnit_Inherit:
    // get width of parent and see if it is a specified value or not
    // XXX for now, just return true
    break;
  case eStyleUnit_Coord:
  case eStyleUnit_Percent:
    isAuto = PR_FALSE;
    break;
  default:
    break;
  }

  return isAuto; 
}

nscoord nsTableFrame::CalcBorderBoxWidth(const nsHTMLReflowState& aState)
{
  nscoord width = aState.mComputedWidth;

  if (eStyleUnit_Auto == aState.mStylePosition->mWidth.GetUnit()) {
    if (0 == width) {
      width = NS_UNCONSTRAINEDSIZE;
    }
    if (NS_UNCONSTRAINEDSIZE != aState.availableWidth) {
      nsMargin margin(0,0,0,0);
      aState.mStyleSpacing->GetMargin(margin);
      width = aState.availableWidth - margin.left - margin.right;
    }
  }
  else if (width != NS_UNCONSTRAINEDSIZE) {
    nsMargin borderPadding(0,0,0,0);
    aState.mStyleSpacing->GetBorderPadding(borderPadding);
    width += borderPadding.left + borderPadding.right;
  }
  width = PR_MAX(width, 0);

  return width;
}

nscoord nsTableFrame::CalcBorderBoxHeight(nsIPresContext*  aPresContext,
                                          const            nsHTMLReflowState& aState)
{
  nscoord height = aState.mComputedHeight;
  if (NS_AUTOHEIGHT != height) {
    nsMargin borderPadding = aState.mComputedBorderPadding;
    height += borderPadding.top + borderPadding.bottom;
  }
  height = PR_MAX(0, height);

  return height;
}

nscoord nsTableFrame::GetMinCaptionWidth()
{
  nsIFrame *outerTableFrame=nsnull;
  GetParent(&outerTableFrame);
  return (((nsTableOuterFrame *)outerTableFrame)->GetMinCaptionWidth());
}

/** return the minimum width of the table.  Return 0 if the min width is unknown. */
nscoord nsTableFrame::GetMinTableWidth()
{
  nscoord result = 0;
  if (nsnull!=mTableLayoutStrategy)
    result = mTableLayoutStrategy->GetTableMinWidth();
  return result;
}

/** return the maximum width of the table.  Return 0 if the max width is unknown. */
nscoord nsTableFrame::GetMaxTableWidth(const nsHTMLReflowState& aState)
{
  nscoord result = 0;
  if (mTableLayoutStrategy) {
    if (eStyleUnit_Coord == aState.mStylePosition->mWidth.GetUnit()) {
      // only fixed width values are returned as the preferred width
      result = PR_MAX(aState.mComputedWidth, mTableLayoutStrategy->GetTableMinWidth());
    }
    else {
      result = mTableLayoutStrategy->GetTableMaxWidth();
    }
  }
  return result;
}

void nsTableFrame::SetMaxElementSize(nsSize*         aMaxElementSize,
                                     const nsMargin& aPadding)
{
  if (nsnull!=mTableLayoutStrategy) {
    mTableLayoutStrategy->SetMaxElementSize(aMaxElementSize, aPadding);
  }
}


PRBool nsTableFrame::IsAutoLayout(const nsHTMLReflowState* aReflowState)
{
  const nsStyleTable* tableStyle;
  GetStyleData(eStyleStruct_Table, (const nsStyleStruct *&)tableStyle);
  // a fixed table-layout table with an auto width is not considered as such
  // for purposes of requiring pass1 reflow and assigning a layout strategy
  if (NS_STYLE_TABLE_LAYOUT_FIXED == tableStyle->mLayoutStrategy) {
    const nsStylePosition* position;
    if (aReflowState) {
      position = aReflowState->mStylePosition;
    }
    else {
      GetStyleData(eStyleStruct_Position, (const nsStyleStruct *&)position);
    }
    if (eStyleUnit_Auto != position->mWidth.GetUnit()) {
      return PR_FALSE;
    }
  }
  return PR_TRUE;
}

#ifdef DEBUG
NS_IMETHODIMP
nsTableFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("Table", aResult);
}
#endif


PRBool
nsTableFrame::IsFinalPass(const nsHTMLReflowState& aState) 
{
  return (NS_UNCONSTRAINEDSIZE != aState.availableWidth) ||
         (NS_UNCONSTRAINEDSIZE != aState.availableHeight);
}

// Find the closet sibling before aPriorChildFrame (including aPriorChildFrame) that
// is of type aChildType
nsIFrame* 
nsTableFrame::GetFrameAtOrBefore(nsIPresContext* aPresContext,
                                 nsIFrame*       aParentFrame,
                                 nsIFrame*       aPriorChildFrame,
                                 nsIAtom*        aChildType)
{
  nsIFrame* result = nsnull;
  if (!aPriorChildFrame) {
    return result;
  }
  nsIAtom* frameType;
  aPriorChildFrame->GetFrameType(&frameType);
  if (aChildType == frameType) {
    NS_RELEASE(frameType);
    return (nsTableCellFrame*)aPriorChildFrame;
  }
  NS_IF_RELEASE(frameType);

  // aPriorChildFrame is not of type aChildType, so we need start from 
  // the beginnng and find the closest one 
  nsIFrame* childFrame;
  nsIFrame* lastMatchingFrame = nsnull;
  aParentFrame->FirstChild(aPresContext, nsnull, &childFrame);
  while (childFrame != aPriorChildFrame) {
    childFrame->GetFrameType(&frameType);
    if (aChildType == frameType) {
      lastMatchingFrame = childFrame;
    }
    NS_IF_RELEASE(frameType);
    childFrame->GetNextSibling(&childFrame);
  }
  return (nsTableCellFrame*)lastMatchingFrame;
}

void 
nsTableFrame::DumpRowGroup(nsIPresContext* aPresContext, nsIFrame* aKidFrame)
{
  nsTableRowGroupFrame* rgFrame = GetRowGroupFrame(aKidFrame);
  if (rgFrame) {
    nsIFrame* rowFrame;
    rgFrame->FirstChild(aPresContext, nsnull, &rowFrame);
    while (rowFrame) {
      nsIAtom* rowType;
      rowFrame->GetFrameType(&rowType);
      if (nsLayoutAtoms::tableRowFrame == rowType) {
        printf("row(%d)=%p ", ((nsTableRowFrame*)rowFrame)->GetRowIndex(), rowFrame);
        nsIFrame* cellFrame;
        rowFrame->FirstChild(aPresContext, nsnull, &cellFrame);
        while (cellFrame) {
          nsIAtom* cellType;
          cellFrame->GetFrameType(&cellType);
          if (nsLayoutAtoms::tableCellFrame == cellType) {
            PRInt32 colIndex;
            ((nsTableCellFrame*)cellFrame)->GetColIndex(colIndex);
            printf("cell(%d)=%p ", colIndex, cellFrame);
          }
          NS_IF_RELEASE(cellType);
          cellFrame->GetNextSibling(&cellFrame);
        }
        printf("\n");
      }
      else {
        DumpRowGroup(aPresContext, rowFrame);
      }
      NS_IF_RELEASE(rowType);
      rowFrame->GetNextSibling(&rowFrame);
    }
  }
}

void nsTableFrame::Dump(nsIPresContext* aPresContext,
                        PRBool          aDumpRows,
                        PRBool          aDumpCols, 
                        PRBool          aDumpCellMap)
{
  printf("***START TABLE DUMP*** \n");
  // dump the columns widths array
  printf("mColWidths=");
  PRInt32 numCols = GetColCount();
  PRInt32 colX;
  for (colX = 0; colX < numCols; colX++) {
    printf("%d ", mColumnWidths[colX]);
  }
  printf("\n");

  if (aDumpRows) {
    nsIFrame* kidFrame = mFrames.FirstChild();
    while (kidFrame) {
      DumpRowGroup(aPresContext, kidFrame);
      kidFrame->GetNextSibling(&kidFrame);
    }
  }

  if (aDumpCols) {
	  // output col frame cache
    printf("\n col frame cache ->");
	  for (colX = 0; colX < numCols; colX++) {
      nsTableColFrame* colFrame = (nsTableColFrame *)mColFrames.ElementAt(colX);
      if (0 == (colX % 8)) {
        printf("\n");
      }
      printf ("%d=%p ", colX, colFrame);
    }
    for (colX = 0; colX < numCols; colX++) {
      printf("\n");
      nsTableColFrame* colFrame = GetColFrame(colX);
      colFrame->Dump(1);
    }
  }
  if (aDumpCellMap) {
    nsTableCellMap* cellMap = GetCellMap();
#ifdef NS_DEBUG
    cellMap->Dump();
#endif
  }
  printf(" ***END TABLE DUMP*** \n");
}

// nsTableIterator
nsTableIterator::nsTableIterator(nsIPresContext*  aPresContext,
                                 nsIFrame&        aSource,
                                 nsTableIteration aType)
{
  nsIFrame* firstChild;
  aSource.FirstChild(aPresContext, nsnull, &firstChild);
  Init(firstChild, aType);
}

nsTableIterator::nsTableIterator(nsFrameList&     aSource,
                                 nsTableIteration aType)
{
  nsIFrame* firstChild = aSource.FirstChild();
  Init(firstChild, aType);
}

void nsTableIterator::Init(nsIFrame*        aFirstChild,
                           nsTableIteration aType)
{
  mFirstListChild = aFirstChild;
  mFirstChild     = aFirstChild;
  mCurrentChild   = nsnull;
  mLeftToRight    = (eTableRTL == aType) ? PR_FALSE : PR_TRUE; 
  mCount          = -1;

  if (!mFirstChild) {
    return;
  }
  if (eTableDIR == aType) {
    nsTableFrame* table = nsnull;
    nsresult rv = nsTableFrame::GetTableFrame(mFirstChild, table);
    if (NS_SUCCEEDED(rv) && (table != nsnull)) {
      const nsStyleDisplay* display;
      table->GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)display);
      mLeftToRight = (NS_STYLE_DIRECTION_LTR == display->mDirection);
    }
    else {
      NS_ASSERTION(PR_FALSE, "source of table iterator is not part of a table");
      return;
    }
  }
  if (!mLeftToRight) {
    mCount = 0;
    nsIFrame* nextChild;
    mFirstChild->GetNextSibling(&nextChild);
    while (nsnull != nextChild) {
      mCount++;
      mFirstChild = nextChild;
      nextChild->GetNextSibling(&nextChild);
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
    return nsnull;
  }

  if (mLeftToRight) {
    mCurrentChild->GetNextSibling(&mCurrentChild);
    return mCurrentChild;
  }
  else {
    nsIFrame* targetChild = mCurrentChild;
    mCurrentChild = nsnull;
    nsIFrame* child = mFirstListChild;
    while (child && (child != targetChild)) {
      mCurrentChild = child;
      child->GetNextSibling(&child);
    }
    return mCurrentChild;
  }
}

PRBool nsTableIterator::IsLeftToRight()
{
  return mLeftToRight;
}

PRInt32 nsTableIterator::Count()
{
  if (-1 == mCount) {
    mCount = 0;
    nsIFrame* child = mFirstListChild;
    while (nsnull != child) {
      mCount++;
      child->GetNextSibling(&child);
    }
  }
  return mCount;
}

nsTableCellFrame* nsTableFrame::GetCellInfoAt(PRInt32            aRowX, 
                                              PRInt32            aColX, 
                                              PRBool*            aOriginates, 
                                              PRInt32*           aColSpan)
{
  nsTableCellMap* cellMap = GetCellMap();
  return cellMap->GetCellInfoAt(aRowX, aColX, aOriginates, aColSpan);
}

/*------------------ nsITableLayout methods ------------------------------*/
NS_IMETHODIMP 
nsTableFrame::GetCellDataAt(PRInt32        aRowIndex, 
                            PRInt32        aColIndex,
                            nsIDOMElement* &aCell,   //out params
                            PRInt32&       aStartRowIndex, 
                            PRInt32&       aStartColIndex, 
                            PRInt32&       aRowSpan, 
                            PRInt32&       aColSpan,
                            PRInt32&       aActualRowSpan, 
                            PRInt32&       aActualColSpan,
                            PRBool&        aIsSelected)
{
  // Initialize out params
  aCell = nsnull;
  aStartRowIndex = 0;
  aStartColIndex = 0;
  aRowSpan = 0;
  aColSpan = 0;
  aIsSelected = PR_FALSE;

  nsTableCellMap* cellMap = GetCellMap();
  if (!cellMap) { return NS_ERROR_NOT_INITIALIZED;}

  PRBool originates;
  PRInt32 colSpan; // Is this the "effective" or "html" value?

  nsTableCellFrame *cellFrame = cellMap->GetCellInfoAt(aRowIndex, aColIndex, &originates, &colSpan);
  if (!cellFrame) return NS_TABLELAYOUT_CELL_NOT_FOUND;

  nsresult result= cellFrame->GetRowIndex(aStartRowIndex);
  if (NS_FAILED(result)) return result;
  result = cellFrame->GetColIndex(aStartColIndex);
  if (NS_FAILED(result)) return result;
  //This returns HTML value, which may be 0
  aRowSpan = cellFrame->GetRowSpan();
  aColSpan = cellFrame->GetColSpan();
  aActualRowSpan = GetEffectiveRowSpan(*cellFrame);
  aActualColSpan = GetEffectiveColSpan(*cellFrame);
  result = cellFrame->GetSelected(&aIsSelected);
  if (NS_FAILED(result)) return result;

  // do this last, because it addrefs, 
  // and we don't want the caller leaking it on error
  nsCOMPtr<nsIContent>content;
  result = cellFrame->GetContent(getter_AddRefs(content));  
  if (NS_SUCCEEDED(result) && content)
  {
    content->QueryInterface(NS_GET_IID(nsIDOMElement), (void**)(&aCell));
  }   
                                        
  return result;
}

NS_IMETHODIMP nsTableFrame::GetTableSize(PRInt32& aRowCount, PRInt32& aColCount)
{
  nsTableCellMap* cellMap = GetCellMap();
  // Initialize out params
  aRowCount = 0;
  aColCount = 0;
  if (!cellMap) { return NS_ERROR_NOT_INITIALIZED;}

  aRowCount = cellMap->GetRowCount();
  aColCount = cellMap->GetColCount();
  return NS_OK;
}

/*---------------- end of nsITableLayout implementation ------------------*/

PRInt32 nsTableFrame::GetNumCellsOriginatingInCol(PRInt32 aColIndex) const
{
  nsTableCellMap* cellMap = GetCellMap();
  return cellMap->GetNumCellsOriginatingInCol(aColIndex);
}

#define INDENT_PER_LEVEL 2

void nsTableFrame::DebugGetIndent(const nsIFrame* aFrame, 
                                  char*           aBuf)
{
  PRInt32 numLevels = 0;
  nsIFrame* parent = nsnull;
  aFrame->GetParent(&parent);
  while (parent) {
    nsIAtom* frameType = nsnull;
    parent->GetFrameType(&frameType);
    if ((nsDebugTable::gRflTableOuter && (nsLayoutAtoms::tableOuterFrame    == frameType)) ||
        (nsDebugTable::gRflTable      && (nsLayoutAtoms::tableFrame         == frameType)) ||
        (nsDebugTable::gRflRowGrp     && (nsLayoutAtoms::tableRowGroupFrame == frameType)) ||
        (nsDebugTable::gRflRow        && (nsLayoutAtoms::tableRowFrame      == frameType)) ||
        (nsDebugTable::gRflCell       && (nsLayoutAtoms::tableCellFrame     == frameType)) ||
        (nsDebugTable::gRflArea       && (nsLayoutAtoms::areaFrame          == frameType))) {
      numLevels++;
    }
    NS_IF_RELEASE(frameType);
    parent->GetParent(&parent);
  }
  PRInt32 indent = INDENT_PER_LEVEL * numLevels;
  nsCRT::memset (aBuf, ' ', indent);
  aBuf[indent] = 0;
}

void PrettyUC(nscoord aSize,
              char*   aBuf)
{
  if (NS_UNCONSTRAINEDSIZE == aSize) {
    strcpy(aBuf, "UC");
  }
  else {
    sprintf(aBuf, "%d", aSize);
  }
}

void nsTableFrame::DebugReflow(char*                      aMessage,
                               const nsIFrame*            aFrame,
                               const nsHTMLReflowState*   aState, 
                               const nsHTMLReflowMetrics* aMetrics,
                               const nsReflowStatus       aStatus)
{
  char indent[256];
  nsTableFrame::DebugGetIndent(aFrame, indent);
  printf("%s%s %p ", indent, aMessage, aFrame);
  char width[32];
  char height[32];
  if (aState) {
    PrettyUC(aState->availableWidth, width);
    PrettyUC(aState->availableHeight, height);
    printf("rea=%d av=(%s,%s) ", aState->reason, width, height); 
    PrettyUC(aState->mComputedWidth, width);
    PrettyUC(aState->mComputedHeight, height);
    printf("comp=(%s,%s) count=%d \n ", width, height, gRflCount);
    gRflCount++;
    //if (32 == gRflCount) {
    //  NS_ASSERTION(PR_FALSE, "stop");
    //}
  }
  if (aMetrics) {
    if (aState) {
      printf("%s", indent);
    }
    PrettyUC(aMetrics->width, width);
    PrettyUC(aMetrics->height, height);
    printf("des=(%s,%s) ", width, height);
    if (aMetrics->maxElementSize) {
      PrettyUC(aMetrics->maxElementSize->width, width);
      PrettyUC(aMetrics->maxElementSize->height, height);
      printf("maxElem=(%s,%s)", width, height);
    }
    if (aMetrics->mFlags & NS_REFLOW_CALC_MAX_WIDTH) {
      printf("max=%d ", aMetrics->mMaximumWidth);
    }
    if (NS_FRAME_COMPLETE != aStatus) {
      printf("status=%d", aStatus);
    }
    printf("\n");
  }
}

PRBool nsTableFrame::RowHasSpanningCells(PRInt32 aRowIndex)
{
  PRBool result = PR_FALSE;
  nsTableCellMap* cellMap = GetCellMap();
  NS_PRECONDITION (cellMap, "bad call, cellMap not yet allocated.");
  if (cellMap) {
		result = cellMap->RowHasSpanningCells(aRowIndex);
  }
  return result;
}

PRBool nsTableFrame::RowIsSpannedInto(PRInt32 aRowIndex)
{
  PRBool result = PR_FALSE;
  nsTableCellMap* cellMap = GetCellMap();
  NS_PRECONDITION (cellMap, "bad call, cellMap not yet allocated.");
  if (cellMap) {
		result = cellMap->RowIsSpannedInto(aRowIndex);
  }
  return result;
}

PRBool nsTableFrame::ColIsSpannedInto(PRInt32 aColIndex)
{
  PRBool result = PR_FALSE;
  nsTableCellMap * cellMap = GetCellMap();
  NS_PRECONDITION (cellMap, "bad call, cellMap not yet allocated.");
  if (cellMap) {
		result = cellMap->ColIsSpannedInto(aColIndex);
  }
  return result;
}

void GetPlaceholderFor(nsIPresContext& aPresContext, nsIFrame& aFrame, nsIFrame** aPlaceholder)
{
  NS_ASSERTION(aPlaceholder, "null placeholder argument is illegal");
  if (aPlaceholder) {
    *aPlaceholder = nsnull;
    nsCOMPtr<nsIPresShell> shell;
    aPresContext.GetShell(getter_AddRefs(shell));
    if(shell) {
      nsCOMPtr<nsIFrameManager> frameManager;
      shell->GetFrameManager(getter_AddRefs(frameManager));
      if (frameManager) {
        frameManager->GetPlaceholderFrameFor(&aFrame,
                                             aPlaceholder);
      }
    }
  }
}

#ifdef DEBUG
NS_IMETHODIMP
nsTableFrame::SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const
{
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  PRUint32 sum = sizeof(*this);

  // Add in the amount of space for the column width array
  sum += mColumnWidthsLength * sizeof(PRInt32);

  // Add in size of cell map
  PRUint32 cellMapSize;
  mCellMap->SizeOf(aHandler, &cellMapSize);
  aHandler->AddSize(nsLayoutAtoms::cellMap, cellMapSize);

  // Add in size of col cache
  PRUint32 voidArraySize;
  mColFrames.SizeOf(aHandler, &voidArraySize);
  sum += voidArraySize - sizeof(mColFrames);

  // Add in size of table layout strategy
  PRUint32 strategySize;
  mTableLayoutStrategy->SizeOf(aHandler, &strategySize);
  aHandler->AddSize(nsLayoutAtoms::tableStrategy, strategySize);

  *aResult = sum;
  return NS_OK;
}
#endif


