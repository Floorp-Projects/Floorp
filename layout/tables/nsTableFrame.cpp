/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsCOMPtr.h"
#include "nsVoidArray.h"
#include "nsTableFrame.h"
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
#include "nsHTMLReflowCommand.h"
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
#include "nsCSSRendering.h"



/********************************************************************************
 ** nsTableReflowState                                                         **
 ********************************************************************************/

struct nsTableReflowState {

  // the real reflow state
  const nsHTMLReflowState& reflowState;

  nsReflowReason reason;

  // The table's available size 
  nsSize availSize;

  // Stationary x-offset
  nscoord x;

  // Running y-offset
  nscoord y;

  // Pointer to the footer in the table
  nsIFrame* footerFrame;

  // The first body section row group frame, i.e. not a header or footer
  nsIFrame* firstBodySection;

  nsTableReflowState(nsIPresContext&          aPresContext,
                     const nsHTMLReflowState& aReflowState,
                     nsTableFrame&            aTableFrame,
                     nsReflowReason           aReason,
                     nscoord                  aAvailWidth,
                     nscoord                  aAvailHeight)
    : reflowState(aReflowState)
  {
    Init(aPresContext, aTableFrame, aReason, aAvailWidth, aAvailHeight);
  }

  void Init(nsIPresContext& aPresContext,
            nsTableFrame&   aTableFrame,
            nsReflowReason  aReason,
            nscoord         aAvailWidth,
            nscoord         aAvailHeight)
  {
    reason = aReason;

    nsTableFrame* table = (nsTableFrame*)aTableFrame.GetFirstInFlow();
    nsMargin borderPadding = table->GetChildAreaOffset(aPresContext, &reflowState);

    x = borderPadding.left;
    y = borderPadding.top;

    availSize.width  = aAvailWidth;
    if (NS_UNCONSTRAINEDSIZE != availSize.width) {
      availSize.width -= borderPadding.left + borderPadding.right;
    }

    availSize.height = aAvailHeight;
    if (NS_UNCONSTRAINEDSIZE != availSize.height) {
      availSize.height -= borderPadding.top + borderPadding.bottom + (2 * table->GetCellSpacingY());
    }

    footerFrame      = nsnull;
    firstBodySection = nsnull;
  }

  nsTableReflowState(nsIPresContext&          aPresContext,
                     const nsHTMLReflowState& aReflowState,
                     nsTableFrame&            aTableFrame)
    : reflowState(aReflowState)
  {
    Init(aPresContext, aTableFrame, aReflowState.reason, aReflowState.availableWidth, aReflowState.availableHeight);
  }

};

/********************************************************************************
 ** nsTableFrame                                                               **
 ********************************************************************************/
#if defined DEBUG_TABLE_REFLOW_TIMING
static PRInt32 gRflCount = 0;
#endif

struct BCPropertyData
{
  BCPropertyData() { mDamageArea.x = mDamageArea.y = mDamageArea.width = mDamageArea.height =
                     mTopBorderWidth = mRightBorderWidth = mBottomBorderWidth = mLeftBorderWidth = 0; }
  nsRect  mDamageArea;
  PRUint8 mTopBorderWidth;
  PRUint8 mRightBorderWidth;
  PRUint8 mBottomBorderWidth;
  PRUint8 mLeftBorderWidth;
};

NS_IMETHODIMP 
nsTableFrame::GetParentStyleContextFrame(nsIPresContext* aPresContext,
                                         nsIFrame**      aProviderFrame,
                                         PRBool*         aIsChild)
{
  // Since our parent, the table outer frame, returned this frame, we
  // must return whatever our parent would normally have returned.

  NS_PRECONDITION(mParent, "table constructed without outer table");
  return NS_STATIC_CAST(nsFrame*, mParent)->
          DoGetParentStyleContextFrame(aPresContext, aProviderFrame, aIsChild);
}


NS_IMETHODIMP
nsTableFrame::GetFrameType(nsIAtom** aType) const
{
  NS_PRECONDITION(nsnull != aType, "null OUT parameter pointer");
  *aType = nsLayoutAtoms::tableFrame; 
  NS_ADDREF(*aType);
  return NS_OK;
}


nsTableFrame::nsTableFrame()
  : nsHTMLContainerFrame(),
    mCellMap(nsnull),
    mTableLayoutStrategy(nsnull),
    mPreferredWidth(0)
{
  mBits.mHadInitialReflow       = PR_FALSE;
  mBits.mHaveReflowedColGroups  = PR_FALSE;
  mBits.mNeedStrategyInit       = PR_TRUE;
  mBits.mNeedStrategyBalance    = PR_TRUE;
  mBits.mCellSpansPctCol        = PR_FALSE;
  mBits.mNeedToCalcBCBorders    = PR_FALSE;
  mBits.mIsBorderCollapse       = PR_FALSE;

#ifdef DEBUG_TABLE_REFLOW_TIMING
  mTimer = new nsReflowTimer(this);
  nsReflowTimer* timer = new nsReflowTimer(this);
  mTimer->mNextSibling = timer;
  timer = new nsReflowTimer(this);
  mTimer->mNextSibling->mNextSibling = timer;
  timer = new nsReflowTimer(this);
  mTimer->mNextSibling->mNextSibling->mNextSibling = timer;
  timer = new nsReflowTimer(this);
  mTimer->mNextSibling->mNextSibling->mNextSibling->mNextSibling = timer;
  timer = new nsReflowTimer(this);
  mTimer->mNextSibling->mNextSibling->mNextSibling->mNextSibling->mNextSibling = timer;
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

  // Let the base class do its processing
  rv = nsHTMLContainerFrame::Init(aPresContext, aContent, aParent, aContext,
                                  aPrevInFlow);

  // record that children that are ignorable whitespace should be excluded 
  mState |= NS_FRAME_EXCLUDE_IGNORABLE_WHITESPACE;

  // see if border collapse is on, if so set it
  const nsStyleTableBorder* tableStyle =
    (const nsStyleTableBorder*)mStyleContext->GetStyleData(eStyleStruct_TableBorder);
  PRBool borderCollapse = (NS_STYLE_BORDER_COLLAPSE == tableStyle->mBorderCollapse);
  SetBorderCollapse(borderCollapse);
  // Create the cell map
  // XXX Why do we do this for continuing frames?
  mCellMap = new nsTableCellMap(aPresContext, *this, borderCollapse);
  if (!mCellMap) return NS_ERROR_OUT_OF_MEMORY;

  if (aPrevInFlow) {
    // set my width, because all frames in a table flow are the same width and
    // code in nsTableOuterFrame depends on this being set
    nsSize  size;
    aPrevInFlow->GetSize(size);
    mRect.width = size.width;
  }
  else {
    NS_ASSERTION(!mTableLayoutStrategy, "strategy was created before Init was called");
    nsCompatibility mode;
    aPresContext->GetCompatibilityMode(&mode);
    // create the strategy
    mTableLayoutStrategy = (IsAutoLayout()) ? new BasicTableLayoutStrategy(this, eCompatibility_NavQuirks == mode)
                                            : new FixedTableLayoutStrategy(this);
  }

  return rv;
}


nsTableFrame::~nsTableFrame()
{
  if (nsnull!=mCellMap) {
    delete mCellMap; 
    mCellMap = nsnull;
  } 

  if (nsnull!=mTableLayoutStrategy) {
    delete mTableLayoutStrategy;
    mTableLayoutStrategy = nsnull;
  }
#ifdef DEBUG_TABLE_REFLOW_TIMING
  nsTableFrame::DebugReflowDone(this);
#endif
}

NS_IMETHODIMP
nsTableFrame::Destroy(nsIPresContext* aPresContext)
{
  mColGroups.DestroyFrames(aPresContext);
  return nsHTMLContainerFrame::Destroy(aPresContext);
}

nscoord 
nsTableFrame::RoundToPixel(nscoord       aValue,
                           float         aPixelToTwips,
                           nsPixelRound  aRound)
{
  nscoord fullPixel = NSToCoordRound(aPixelToTwips);
  if (fullPixel <= 0) 
    // We must be rendering to a device that has a resolution greater than Twips! 
    // In that case, aValue is as accurate as it's going to get.
    return aValue;
  
  PRInt32 excess = aValue % fullPixel;
  if (0 == excess) 
    return aValue;

  nscoord halfPixel = NSToCoordRound(aPixelToTwips / 2.0f);
  switch(aRound) {
  case eRoundUpIfHalfOrMore:
    if (excess >= halfPixel) { // eRoundUpIfHalfOrMore
      return aValue + (fullPixel - excess);
    }
  case eAlwaysRoundDown:
    return aValue - excess;
  default: // eAlwaysRoundUp
    return aValue + (fullPixel - excess);
  }
}

// Helper function which marks aFrame as dirty and generates a reflow command
nsresult
nsTableFrame::AppendDirtyReflowCommand(nsIPresShell* aPresShell,
                                       nsIFrame*     aFrame)
{
  if (!aPresShell) return NS_ERROR_NULL_POINTER;

  nsFrameState frameState;
  aFrame->GetFrameState(&frameState);
  
  frameState |= NS_FRAME_IS_DIRTY;  // mark the table frame as dirty  
  aFrame->SetFrameState(frameState);

  nsHTMLReflowCommand* reflowCmd;
  nsresult rv = NS_NewHTMLReflowCommand(&reflowCmd, aFrame,
                                        eReflowType_ReflowDirty);
  if (NS_SUCCEEDED(rv)) {
    // Add the reflow command
    rv = aPresShell->AppendReflowCommand(reflowCmd);
  }

  return rv;
}

// Make sure any views are positioned properly
void
nsTableFrame::RePositionViews(nsIPresContext* aPresContext,
                              nsIFrame*       aFrame)
{
  nsContainerFrame::PositionFrameView(aPresContext, aFrame);
  nsContainerFrame::PositionChildViews(aPresContext, aFrame);
}

PRBool
nsTableFrame::PageBreakAfter(nsIFrame& aSourceFrame,
                             nsIFrame* aNextFrame)
{
  nsCOMPtr<nsIStyleContext> sourceContext;
  aSourceFrame.GetStyleContext(getter_AddRefs(sourceContext)); NS_ENSURE_TRUE(sourceContext, PR_FALSE);
  const nsStyleDisplay* display = (const nsStyleDisplay*)
    sourceContext->GetStyleData(eStyleStruct_Display);         NS_ENSURE_TRUE(display, PR_FALSE); 
  // don't allow a page break after a repeated header
  if (display->mBreakAfter && (NS_STYLE_DISPLAY_TABLE_HEADER_GROUP != display->mDisplay)) {
    return PR_TRUE;
  }

  if (aNextFrame) {
    nsCOMPtr<nsIStyleContext> nextContext;
    aNextFrame->GetStyleContext(getter_AddRefs(nextContext));  NS_ENSURE_TRUE(nextContext, PR_FALSE);
    display = (const nsStyleDisplay*)
      nextContext->GetStyleData(eStyleStruct_Display);         NS_ENSURE_TRUE(display, PR_FALSE); 
    // don't allow a page break before a repeated footer
    if (display->mBreakBefore && (NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP != display->mDisplay)) {
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

nsIPresShell*
nsTableFrame::GetPresShellNoAddref(nsIPresContext* aPresContext)
{
  nsIPresShell* tempShell;
  nsIPresShell* presShell;
  aPresContext->GetShell(&tempShell);
  presShell = tempShell;
  NS_RELEASE(tempShell); // presShell is needed because this sets tempShell to nsnull

  return presShell;
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
    // calc collapsing borders if this is the default (row group, col group, child list)
    if (!aChildList && IsBorderCollapse()) {
      nsRect damageArea(0, 0, GetColCount(), GetRowCount());
      SetBCDamageArea(*aPresContext, damageArea);
    }
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
  if (IS_TABLE_CELL(frameType)) {
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
        nsAutoVoidArray cells;
        cells.AppendElement(cellFrame);
        InsertCells(*aPresContext, cells, rowIndex, colIndex - 1);

        // XXX This could probably be optimized with some effort
        SetNeedStrategyInit(PR_TRUE);
        AppendDirtyReflowCommand(GetPresShellNoAddref(aPresContext), this);
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

/* return the col count including dead cols */
PRInt32 nsTableFrame::GetColCount () const
{
  PRInt32 colCount = 0;
  nsTableCellMap* cellMap = GetCellMap();
  NS_ASSERTION(nsnull != cellMap, "GetColCount null cellmap");
  if (nsnull != cellMap) {
    colCount = cellMap->GetColCount();
  }
  return colCount;
}

/* return the effective col count */
PRInt32 nsTableFrame::GetEffectiveColCount() const
{
  PRInt32 colCount = GetColCount();
  // don't count cols at the end that don't have originating cells
  for (PRInt32 colX = colCount - 1; colX >= 0; colX--) {
    if (GetNumCellsOriginatingInCol(colX) <= 0) { 
      colCount--;
    }
    else break;
  }
  return colCount;
}

PRInt32 nsTableFrame::GetIndexOfLastRealCol()
{
  PRInt32 numCols = mColFrames.Count();
  for (PRInt32 colX = numCols; colX >= 0; colX--) { 
    nsTableColFrame* colFrame = GetColFrame(colX);
    if (colFrame) {
      if (eColAnonymousCell != colFrame->GetType()) {
        return colX;
      }
    }
  }
  return -1; 
}

nsTableColFrame* nsTableFrame::GetColFrame(PRInt32 aColIndex)
{
  NS_ASSERTION(!mPrevInFlow, "GetColFrame called on next in flow");
  PRInt32 numCols = mColFrames.Count();
  if ((aColIndex >= 0) && (aColIndex < numCols)) {
    return (nsTableColFrame *)mColFrames.ElementAt(aColIndex);
  }
  else {
    //NS_ASSERTION(PR_FALSE, "invalid col index");
    return nsnull;
  }
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

PRInt32 nsTableFrame::GetEffectiveRowSpan(const nsTableCellFrame& aCell,
                                          nsCellMap*              aCellMap)
{
  nsTableCellMap* tableCellMap = GetCellMap(); if (!tableCellMap) ABORT1(1);

  PRInt32 colIndex, rowIndex;
  aCell.GetColIndex(colIndex);
  aCell.GetRowIndex(rowIndex);
  PRBool ignore;

  if (aCellMap) 
    return aCellMap->GetRowSpan(*tableCellMap, rowIndex, colIndex, PR_TRUE, ignore);
  else
    return tableCellMap->GetEffectiveRowSpan(rowIndex, colIndex);
}

PRInt32 nsTableFrame::GetEffectiveColSpan(const nsTableCellFrame& aCell,
                                          nsCellMap*              aCellMap) const
{
  nsTableCellMap* tableCellMap = GetCellMap(); if (!tableCellMap) ABORT1(1);

  PRInt32 colIndex, rowIndex;
  aCell.GetColIndex(colIndex);
  aCell.GetRowIndex(rowIndex);
  PRBool ignore;

  if (aCellMap) 
    return aCellMap->GetEffectiveColSpan(*tableCellMap, rowIndex, colIndex, ignore);
  else
    return tableCellMap->GetEffectiveColSpan(rowIndex, colIndex);
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

void nsTableFrame::AdjustRowIndices(nsIPresContext* aPresContext,
                                    PRInt32         aRowIndex,
                                    PRInt32         aAdjustment)
{
  // Iterate over the row groups and adjust the row indices of all rows 
  // whose index is >= aRowIndex.
  nsAutoVoidArray rowGroups;
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
  nsIFrame* rowFrame;
  aRowGroup->FirstChild(aPresContext, nsnull, &rowFrame);
  for ( ; rowFrame; rowFrame->GetNextSibling(&rowFrame)) {
    const nsStyleDisplay* rowDisplay;
    rowFrame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct *&)rowDisplay);
    if (NS_STYLE_DISPLAY_TABLE_ROW==rowDisplay->mDisplay) {
      PRInt32 index = ((nsTableRowFrame*)rowFrame)->GetRowIndex();
      if (index >= aRowIndex)
        ((nsTableRowFrame *)rowFrame)->SetRowIndex(index+anAdjustment);
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
  // for now, just bail and recalc all of the collapsing borders
  if (IsBorderCollapse()) {
    nsRect damageArea(0, 0, PR_MAX(1, GetColCount()), PR_MAX(1, GetRowCount()));
    SetBCDamageArea(aPresContext, damageArea);
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
      CreateAnonymousColFrames(aPresContext, 1, eColAnonymousCell, PR_TRUE);
    }
  }
  // for now, just bail and recalc all of the collapsing borders
  if (IsBorderCollapse()) {
    nsRect damageArea(0, 0, GetColCount(), GetRowCount());
    SetBCDamageArea(aPresContext, damageArea);
  }
}

/** Get the cell map for this table frame.  It is not always mCellMap.
  * Only the firstInFlow has a legit cell map
  */
nsTableCellMap* nsTableFrame::GetCellMap() const
{
  nsTableFrame* firstInFlow = (nsTableFrame *)GetFirstInFlow();
  if (this == firstInFlow) {
    return mCellMap;
  }
  else {
    return firstInFlow->GetCellMap();
  }
}

nscoord nsTableFrame::GetMinWidth() const
{
  nsTableFrame* firstInFlow = (nsTableFrame *)GetFirstInFlow();
  if (this == firstInFlow) {
    return mMinWidth;
  }
  else {
    return firstInFlow->GetMinWidth();
  }
}

nscoord nsTableFrame::GetDesiredWidth() const
{
  nsTableFrame* firstInFlow = (nsTableFrame *)GetFirstInFlow();
  if (this == firstInFlow) {
    return mDesiredWidth;
  }
  else {
    return firstInFlow->GetDesiredWidth();
  }
}

nscoord nsTableFrame::GetPreferredWidth() const
{
  nsTableFrame* firstInFlow = (nsTableFrame *)GetFirstInFlow();
  if (this == firstInFlow) {
    return mPreferredWidth;
  }
  else {
    return firstInFlow->GetPreferredWidth();
  }
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
                                                parentStyleContext, getter_AddRefs(styleContext));
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
        startColIndex = colFrame->GetColIndex() + 1;
      }
    }
    aColGroupFrame.AddColsToTable(aPresContext, startColIndex, PR_TRUE, 
                                  *aFirstNewFrame, lastColFrame);
  }
}

void
nsTableFrame::AppendCell(nsIPresContext&   aPresContext,
                         nsTableCellFrame& aCellFrame,
                         PRInt32           aRowIndex)
{
  nsTableCellMap* cellMap = GetCellMap();
  if (cellMap) {
    nsRect damageArea(0,0,0,0);
    cellMap->AppendCell(aCellFrame, aRowIndex, PR_TRUE, damageArea);
    PRInt32 numColsInMap   = GetColCount();
    PRInt32 numColsInCache = mColFrames.Count();
    PRInt32 numColsToAdd = numColsInMap - numColsInCache;
    if (numColsToAdd > 0) {
      // this sets the child list, updates the col cache and cell map
      CreateAnonymousColFrames(aPresContext, numColsToAdd, eColAnonymousCell, PR_TRUE); 
    }
    if (IsBorderCollapse()) {
      SetBCDamageArea(aPresContext, damageArea);
    }
  }
}

void nsTableFrame::InsertCells(nsIPresContext& aPresContext,
                               nsVoidArray&    aCellFrames, 
                               PRInt32         aRowIndex, 
                               PRInt32         aColIndexBefore)
{
  nsTableCellMap* cellMap = GetCellMap();
  if (cellMap) {
    nsRect damageArea(0,0,0,0);
    cellMap->InsertCells(aCellFrames, aRowIndex, aColIndexBefore, damageArea);
    PRInt32 numColsInMap = GetColCount();
    PRInt32 numColsInCache = mColFrames.Count();
    PRInt32 numColsToAdd = numColsInMap - numColsInCache;
    if (numColsToAdd > 0) {
      // this sets the child list, updates the col cache and cell map
      CreateAnonymousColFrames(aPresContext, numColsToAdd, eColAnonymousCell, PR_TRUE);
    }
    if (IsBorderCollapse()) {
      SetBCDamageArea(aPresContext, damageArea);
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
    nsRect damageArea(0,0,0,0);
    cellMap->RemoveCell(aCellFrame, aRowIndex, damageArea);
    PRInt32 numColsInMap = GetColCount(); // cell map's notion of num cols
    PRInt32 numColsInCache = mColFrames.Count();
    if (numColsInCache > numColsInMap) {
      PRInt32 numColsNotRemoved = DestroyAnonymousColFrames(aPresContext, numColsInCache - numColsInMap);
      // if the cell map has fewer cols than the cache, correct it
      if (numColsNotRemoved > 0) {
        cellMap->AddColsAtEnd(numColsNotRemoved);
      }
    }
    else NS_ASSERTION(numColsInCache == numColsInMap, "cell map has too many cols");

    if (IsBorderCollapse()) {
      SetBCDamageArea(aPresContext, damageArea);
    }
  }
}

PRInt32
nsTableFrame::GetStartRowIndex(nsTableRowGroupFrame& aRowGroupFrame)
{
  nsAutoVoidArray orderedRowGroups;
  PRUint32 numRowGroups;
  OrderRowGroups(orderedRowGroups, numRowGroups);

  PRInt32 rowIndex = 0;
  for (PRUint32 rgIndex = 0; rgIndex < numRowGroups; rgIndex++) {
    nsTableRowGroupFrame* rgFrame = GetRowGroupFrame((nsIFrame*)orderedRowGroups.ElementAt(rgIndex));
    if (rgFrame == &aRowGroupFrame) {
      break;
    }
    PRInt32 numRows = rgFrame->GetRowCount();
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
  nsAutoVoidArray rows;
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
    nsRect damageArea(0,0,0,0);
    PRInt32 origNumRows = cellMap->GetRowCount();
    PRInt32 numNewRows = aRowFrames.Count();
    cellMap->InsertRows(&aPresContext, aRowGroupFrame, aRowFrames, aRowIndex, aConsiderSpans, damageArea);
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
    if (IsBorderCollapse()) {
      SetBCDamageArea(aPresContext, damageArea);
    }
  }

  //printf("insertRowsAfter \n");
  //Dump(PR_TRUE, PR_FALSE, PR_TRUE);

  return numColsToAdd;
}

// this cannot extend beyond a single row group
void nsTableFrame::RemoveRows(nsIPresContext&  aPresContext,
                              nsTableRowFrame& aFirstRowFrame,
                              PRInt32          aNumRowsToRemove,
                              PRBool           aConsiderSpans)
{
  //printf("removeRowsBefore firstRow=%d numRows=%d\n", aFirstRowIndex, aNumRowsToRemove);
  //Dump(PR_TRUE, PR_FALSE, PR_TRUE);

  
#ifdef TBD_OPTIMIZATION
  // decide if we need to rebalance. we have to do this here because the row group 
  // cannot do it when it gets the dirty reflow corresponding to the frame being destroyed
  PRBool stopTelling = PR_FALSE;
  for (nsIFrame* kidFrame = aFirstFrame.FirstChild(); (kidFrame && !stopAsking); kidFrame = kidFrame->GetNextSibling()) {
    nsCOMPtr<nsIAtom> kidType;
    kidFrame->GetFrameType(getter_AddRefs(frameType));
    if (IS_TABLE_CELL(kidType.get())) {
      nsTableCellFrame* cellFrame = (nsTableCellFrame*)kidFrame;
      stopTelling = tableFrame->CellChangedWidth(*cellFrame, cellFrame->GetPass1MaxElementWidth(), 
                                                 cellFrame->GetMaximumWidth(), PR_TRUE);
    }
  }
  // XXX need to consider what happens if there are cells that have rowspans 
  // into the deleted row. Need to consider moving rows if a rebalance doesn't happen
#endif

  PRInt32 firstRowIndex = aFirstRowFrame.GetRowIndex();
  nsTableCellMap* cellMap = GetCellMap();
  if (cellMap) {
    nsRect damageArea(0,0,0,0);
    cellMap->RemoveRows(&aPresContext, firstRowIndex, aNumRowsToRemove, aConsiderSpans, damageArea);
    // only remove cols that are of type eTypeAnonymous cell (they are at the end)
    PRInt32 numColsInMap = GetColCount(); // cell map's notion of num cols
    PRInt32 numColsInCache = mColFrames.Count();
    if (numColsInCache > numColsInMap) {
      PRInt32 numColsNotRemoved = DestroyAnonymousColFrames(aPresContext, numColsInCache - numColsInMap);
      // if the cell map has fewer cols than the cache, correct it
      if (numColsNotRemoved > 0) {
        cellMap->AddColsAtEnd(numColsNotRemoved);
      }
    }
    else NS_ASSERTION(numColsInCache == numColsInMap, "cell map has too many cols");

    if (IsBorderCollapse()) {
      SetBCDamageArea(aPresContext, damageArea);
    }
  }
  AdjustRowIndices(&aPresContext, firstRowIndex, -aNumRowsToRemove);
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
                               nsIAtom*  aFrameTypeIn) const
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
    nsresult rv = aFrame->QueryInterface(NS_GET_IID(nsIScrollableFrame), (void **)&scrollable);
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
    nsAutoVoidArray orderedRowGroups;
    PRUint32 numRowGroups;
    OrderRowGroups(orderedRowGroups, numRowGroups);

    nsAutoVoidArray rows;
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
            PRInt32 priorNumRows = priorRG->GetRowCount();
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

void 
nsTableFrame::PaintChildren(nsIPresContext*      aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            const nsRect&        aDirtyRect,
                            nsFramePaintLayer    aWhichLayer,
                            PRUint32             aFlags)

{
  const nsStyleDisplay* disp = (const nsStyleDisplay*)
    mStyleContext->GetStyleData(eStyleStruct_Display);
  // If overflow is hidden then set the clip rect so that children don't
  // leak out of us. Note that because overflow'-clip' only applies to
  // the content area we do this after painting the border and background
  if (NS_STYLE_OVERFLOW_HIDDEN == disp->mOverflow) {
    aRenderingContext.PushState();
    SetOverflowClipRect(aRenderingContext);
  }

  nsHTMLContainerFrame::PaintChildren(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer, aFlags);

  if (NS_STYLE_OVERFLOW_HIDDEN == disp->mOverflow) {
    PRBool clipState;
    aRenderingContext.PopState(clipState);
  }
}

// table paint code is concerned primarily with borders and bg color
// SEC: TODO: adjust the rect for captions 
NS_METHOD 
nsTableFrame::Paint(nsIPresContext*      aPresContext,
                    nsIRenderingContext& aRenderingContext,
                    const nsRect&        aDirtyRect,
                    nsFramePaintLayer    aWhichLayer,
                    PRUint32             aFlags)
{
  PRBool visibleBCBorders = PR_FALSE;
  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
    const nsStyleVisibility* vis = 
      (const nsStyleVisibility*)mStyleContext->GetStyleData(eStyleStruct_Visibility);
    if (vis && vis->IsVisibleOrCollapsed()) {
      const nsStyleBorder* border =
        (const nsStyleBorder*)mStyleContext->GetStyleData(eStyleStruct_Border);
      const nsStylePadding* padding =
        (const nsStylePadding*)mStyleContext->GetStyleData(eStyleStruct_Padding);
      nsRect  rect(0, 0, mRect.width, mRect.height);

      nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                      aDirtyRect, rect, *border, *padding,
                                      0, 0, PR_TRUE);
      
      // paint the border here only for separate borders
      if (!IsBorderCollapse()) {
        PRIntn skipSides = GetSkipSides();
        nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                    aDirtyRect, rect, *border, mStyleContext, skipSides);
      }
      else {
        visibleBCBorders = PR_TRUE;
      }
    }
  }

  // for collapsed borders paint the backgrounds of cells, but not their contents (that happens below)
  PRUint32 flags = aFlags;
  if (visibleBCBorders) {
    flags &= ~BORDER_COLLAPSE_BACKGROUNDS; // set bit to 0
  }
  PaintChildren(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer, flags);

  if (visibleBCBorders) {
    // for collapsed borders, paint the borders and then the backgrounds of cell
    // contents but not the backgrounds of the cells
    PaintBCBorders(aPresContext, aRenderingContext, aDirtyRect);
    flags |= BORDER_COLLAPSE_BACKGROUNDS; // set bit to 1
    PaintChildren(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer, flags);
  }

#ifdef DEBUG
  // for debug...
  if ((NS_FRAME_PAINT_LAYER_DEBUG == aWhichLayer) && GetShowFrameBorders()) {
    aRenderingContext.SetColor(NS_RGB(0,255,0));
    aRenderingContext.DrawRect(0, 0, mRect.width, mRect.height);
  }
#endif

  DO_GLOBAL_REFLOW_COUNT_DSP_J("nsTableFrame", &aRenderingContext, NS_RGB(255,128,255));
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
  if (eReflowReason_Resize == aReflowState.reason) {
    if (aReflowState.mFlags.mSpecialHeightReflow &&
        !NeedSpecialReflow()                   &&
        !NeedToInitiateSpecialReflow()) {
      result = PR_FALSE;
    }
  }
  else if ((eReflowReason_Incremental == aReflowState.reason) &&
           (NS_UNCONSTRAINEDSIZE == aReflowState.availableHeight)) {
    // It's an incremental reflow and we're in galley mode. Only
    // do a full reflow if we need to.
    result = NeedStrategyInit() || NeedStrategyBalance();
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
nsresult 
nsTableFrame::AdjustSiblingsAfterReflow(nsIPresContext*     aPresContext,
                                        nsTableReflowState& aReflowState,
                                        nsIFrame*           aKidFrame,
                                        nscoord             aDeltaY)
{
  NS_PRECONDITION(NS_UNCONSTRAINEDSIZE == aReflowState.reflowState.availableHeight,
                  "we're not in galley mode");

  nscoord yInvalid = NS_UNCONSTRAINEDSIZE;

  // Get the ordered children and find aKidFrame in the list
  nsAutoVoidArray rowGroups;
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
 
    // Adjust the y-origin if its position actually changed
    if (aDeltaY != 0) {
      kidRect.y += aDeltaY;
      kidFrame->MoveTo(aPresContext, kidRect.x, kidRect.y);
      RePositionViews(aPresContext, kidFrame);
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
  if (!aPresContext) ABORT0();
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
        nscoord colWidth = GetColumnWidth(colX);
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

// XXX this could be made more general to handle row modifications that change the
// table height, but first we need to scrutinize every Invalidate
static void
ProcessRowInserted(nsIPresContext* aPresContext,
                   nsTableFrame&   aTableFrame,
                   PRBool          aInvalidate,
                   nscoord         aNewHeight)
{
  aTableFrame.SetRowInserted(PR_FALSE); // reset the bit that got us here
  nsAutoVoidArray rowGroups;
  PRUint32 numRowGroups;
  aTableFrame.OrderRowGroups(rowGroups, numRowGroups);
  // find the row group containing the inserted row
  for (PRUint32 rgX = 0; rgX < numRowGroups; rgX++) {
    nsTableRowGroupFrame* rgFrame = (nsTableRowGroupFrame*)rowGroups.ElementAt(rgX);
    if (!rgFrame) continue; // should never happen
    nsIFrame* childFrame = nsnull;
    rgFrame->FirstChild(aPresContext, nsnull, &childFrame);
    // find the row that was inserted first
    while (childFrame) {
      nsCOMPtr<nsIAtom> childType;
      childFrame->GetFrameType(getter_AddRefs(childType));
      if (nsLayoutAtoms::tableRowFrame == childType.get()) {
        nsTableRowFrame* rowFrame = (nsTableRowFrame*)childFrame;
        if (rowFrame->IsFirstInserted()) {
          rowFrame->SetFirstInserted(PR_FALSE);
          if (aInvalidate) {
            // damage the table from the 1st row inserted to the end of the table
            nsRect damageRect, rgRect, rowRect;

            aTableFrame.GetRect(damageRect);
            rgFrame->GetRect(rgRect);
            rowFrame->GetRect(rowRect);

            damageRect.y += rgRect.y + rowRect.y; 
            damageRect.height = aNewHeight - damageRect.y;

            aTableFrame.Invalidate(aPresContext, damageRect);
            aTableFrame.SetRowInserted(PR_FALSE);
          }
          return; // found it, so leave
        }
      }
      childFrame->GetNextSibling(&childFrame);
    }
  }
}

// Return true if aStylePosition has a pct height
static PRBool 
IsPctStyleHeight(const nsStylePosition* aStylePosition)
{
  return (aStylePosition && 
          (eStyleUnit_Percent == aStylePosition->mHeight.GetUnit()));
}

// Return true if aStylePosition has a coord height
static PRBool 
IsFixedStyleHeight(const nsStylePosition* aStylePosition)
{
  return (aStylePosition && 
          (eStyleUnit_Coord == aStylePosition->mHeight.GetUnit()));
}

// Return true if any of aReflowState.frame's ancestors within the containing table
// have a pct or fixed height
static PRBool
AncestorsHaveStyleHeight(const nsHTMLReflowState& aReflowState)
{
  for (const nsHTMLReflowState* parentRS = aReflowState.parentReflowState;
       parentRS && parentRS->frame; 
       parentRS = parentRS->parentReflowState) {
    nsCOMPtr<nsIAtom> frameType;
    parentRS->frame->GetFrameType(getter_AddRefs(frameType));
    if (IS_TABLE_CELL(frameType.get())                         ||
        (nsLayoutAtoms::tableRowFrame      == frameType.get()) ||
        (nsLayoutAtoms::tableRowGroupFrame == frameType.get())) {
      if (::IsPctStyleHeight(parentRS->mStylePosition) || ::IsFixedStyleHeight(parentRS->mStylePosition)) {
        return PR_TRUE;
      }
    }
    else if (nsLayoutAtoms::tableFrame == frameType.get()) {
      // we reached the containing table, so always return
      if (::IsPctStyleHeight(parentRS->mStylePosition) || ::IsFixedStyleHeight(parentRS->mStylePosition)) {
        return PR_TRUE;
      }
      else return PR_FALSE;
    }
  }
  return PR_FALSE;
}

// See if a special height reflow needs to occur and if so, call RequestSpecialHeightReflow
void
nsTableFrame::CheckRequestSpecialHeightReflow(const nsHTMLReflowState& aReflowState)
{
  if (!aReflowState.frame) ABORT0();
  nsIFrame* prevInFlow;
  aReflowState.frame->GetPrevInFlow(&prevInFlow);

  if (!prevInFlow                                             &&   // 1st in flow                                            && // 1st in flow
      ((NS_UNCONSTRAINEDSIZE == aReflowState.mComputedHeight) ||   // no computed height
       (0                    == aReflowState.mComputedHeight))  && 
      ::IsPctStyleHeight(aReflowState.mStylePosition)) {           // pct height

    if (::AncestorsHaveStyleHeight(aReflowState)) {
      nsTableFrame::RequestSpecialHeightReflow(aReflowState);
    }
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
    nsCOMPtr<nsIAtom> frameType;
    rs->frame->GetFrameType(getter_AddRefs(frameType));
    if (IS_TABLE_CELL(frameType.get())) {
      ((nsTableCellFrame*)rs->frame)->SetNeedSpecialReflow(PR_TRUE);
    }
    else if (nsLayoutAtoms::tableRowFrame == frameType.get()) {
      ((nsTableRowFrame*)rs->frame)->SetNeedSpecialReflow(PR_TRUE);
    }
    else if (nsLayoutAtoms::tableRowGroupFrame == frameType.get()) {
      ((nsTableRowGroupFrame*)rs->frame)->SetNeedSpecialReflow(PR_TRUE);
    }
    else if (nsLayoutAtoms::tableFrame == frameType.get()) {
      if (rs == &aReflowState) {
        // don't stop because we started with this table 
        ((nsTableFrame*)rs->frame)->SetNeedSpecialReflow(PR_TRUE);
      }
      else {
        ((nsTableFrame*)rs->frame)->SetNeedToInitiateSpecialReflow(PR_TRUE);
        // always stop when we reach a table that we didn't start with
        break;
      }
    }
  }
}

// Return true (and set aMetrics's desiredSize to aRect) if the special height reflow
// was initiated by an ancestor of aReflowState.frame's containing table. In that case, 
// aFrame's containing table will eventually initiate a special height reflow which 
// will cause this method to return false. 
PRBool
nsTableFrame::IsPrematureSpecialHeightReflow(const nsHTMLReflowState& aReflowState,
                                             const nsRect&            aRect,
                                             PRBool                   aNeedSpecialHeightReflow,
                                             nsHTMLReflowMetrics&     aMetrics)
{
  PRBool premature = PR_FALSE; 
  if (aReflowState.mFlags.mSpecialHeightReflow) { 
    if (aNeedSpecialHeightReflow) { 
      nsTableFrame* tableFrame; 
      nsTableFrame::GetTableFrame(aReflowState.frame, tableFrame); 
      if (tableFrame && (tableFrame != aReflowState.mPercentHeightReflowInitiator)) { 
        premature = PR_TRUE; 
      } 
    } 
    else { 
      premature = PR_TRUE; 
    } 
    if (premature) { 
      aMetrics.width  = aRect.width; 
      aMetrics.height = aRect.height; 
    } 
  }
  return premature;
}

/******************************************************************************************
 * During the initial reflow the table reflows each child with an unconstrained avail width
 * to get its max element width and maximum width. This is referred to as the pass 1 reflow.
 *
 * After the 1st pass reflow, the table determines the column widths using BalanceColumnWidths()
 * then reflows each child again with a constrained avail width. This reflow is referred to
 * as the pass 2 reflow. 
 *
 * A special height reflow (pass 3 reflow) can occur during an intitial or resize reflow
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
 *    will do the special height reflow, setting the reflow state's mFlages.mSpecialHeightReflow
 *    to true and mSpecialHeightInitiator to itself. It won't do this if IsPrematureSpecialHeightReflow()
 *    returns true because in that case another special height reflow will be comming along with the
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
 ******************************************************************************************/

/* Layout the entire inner table. */
NS_METHOD nsTableFrame::Reflow(nsIPresContext*          aPresContext,
                               nsHTMLReflowMetrics&     aDesiredSize,
                               const nsHTMLReflowState& aReflowState,
                               nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsTableFrame", aReflowState.reason);
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);
#if defined DEBUG_TABLE_REFLOW_TIMING
  nsTableFrame::DebugReflow(this, (nsHTMLReflowState&)aReflowState);
#endif
  PRBool isPaginated;
  aPresContext->IsPaginated(&isPaginated);

  // If this is a special height reflow, set our desired size to what is was previously and return
  // if we will be getting another special height reflow. In paginated mode, SetNeedSpecialReflow(PR_TRUE) 
  // may not have been called if reflow was a result of having a height on the containing table
  if (IsPrematureSpecialHeightReflow(aReflowState, mRect, NeedSpecialReflow() || isPaginated, aDesiredSize)) 
    return NS_OK;

  aStatus = NS_FRAME_COMPLETE; 
  if (!mPrevInFlow && !mTableLayoutStrategy) {
    NS_ASSERTION(PR_FALSE, "strategy should have been created in Init");
    return NS_ERROR_NULL_POINTER;
  }
  nsresult rv = NS_OK;

  // see if a special height reflow needs to occur due to having a pct height
  if (!NeedSpecialReflow()) 
    nsTableFrame::CheckRequestSpecialHeightReflow(aReflowState);

  // see if collapsing borders need to be calculated
  if (!mPrevInFlow && IsBorderCollapse() && NeedToCalcBCBorders()) {
    GET_TWIPS_TO_PIXELS(aPresContext, p2t);
    CalcBCBorders(*aPresContext);
  }
  PRBool doCollapse = PR_FALSE; // collapsing rows, cols, etc.

  aDesiredSize.width = aReflowState.availableWidth;

  nsReflowReason nextReason = aReflowState.reason;

  // Check for an overflow list, and append any row group frames being pushed
  MoveOverflowToChildList(aPresContext);

  // Processes an initial (except when there is mPrevInFlow), incremental, or style 
  // change reflow 1st. resize reflows are processed in the next phase.
  switch (aReflowState.reason) {
    case eReflowReason_Initial: 
    case eReflowReason_StyleChange: {
      if ((eReflowReason_Initial == aReflowState.reason) && HadInitialReflow()) {
        // XXX put this back in when bug 70150 is fixed
        // NS_ASSERTION(PR_FALSE, "intial reflow called twice");
      }
      else {
        if (!mPrevInFlow) { // only do pass1 on a first in flow
          if (IsAutoLayout()) {     
            // only do pass1 reflow on an auto layout table
            nsReflowReason reason = (eReflowReason_Initial == aReflowState.reason)
                                    ? eReflowReason_Initial : eReflowReason_StyleChange;
            nsTableReflowState reflowState(*aPresContext, aReflowState, *this, reason,
                                           NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE); 
            // reflow the children
            nsIFrame *lastReflowed;
            ReflowChildren(aPresContext, reflowState, !HaveReflowedColGroups(), PR_FALSE, aStatus, lastReflowed);
          }
          mTableLayoutStrategy->Initialize(aPresContext, aReflowState);
        }
      }
      if (!mPrevInFlow) {
        SetHadInitialReflow(PR_TRUE);
        SetNeedStrategyBalance(PR_TRUE); // force a balance and then a pass2 reflow 
        if ((nextReason != eReflowReason_StyleChange) || IsAutoLayout()) 
          nextReason = eReflowReason_Resize;
      }
      else {
        nextReason = eReflowReason_Initial;
      }
      break; 
    }
    case eReflowReason_Incremental:
      NS_ASSERTION(HadInitialReflow(), "intial reflow not called");
      rv = IncrementalReflow(aPresContext, aReflowState, aStatus);
      nextReason = eReflowReason_Resize;
      break;
    case eReflowReason_Resize:
      // do the resize reflow below
      if (!HadInitialReflow()) {
        // NS_ASSERTION(HadInitialReflow(), "intial reflow not called");
        nextReason = eReflowReason_Initial;
      }
      //NS_ASSERTION(NS_UNCONSTRAINEDSIZE != aReflowState.availableWidth, "this doesn't do anything");
      SetNeedStrategyBalance(PR_TRUE); 
      break; 
    default:
      break;
  }

  if (NS_FAILED(rv)) return rv;

  PRBool haveDesiredHeight = PR_FALSE;
  PRBool balanced = PR_FALSE;

  // Reflow the entire table (pass 2 and possibly pass 3). This phase is necessary during a 
  // constrained initial reflow and other reflows which require either a strategy init or balance. 
  // This isn't done during an unconstrained reflow, because it will occur later when the parent 
  // reflows with a constrained width.
  if (NeedsReflow(aReflowState) && (NS_UNCONSTRAINEDSIZE != aReflowState.availableWidth)) {
    // see if an extra reflow will be necessary in pagination mode when there is a specified table height 
    if (isPaginated && !mPrevInFlow && (NS_UNCONSTRAINEDSIZE != aReflowState.availableHeight)) {
      nscoord tableSpecifiedHeight = CalcBorderBoxHeight(aPresContext, aReflowState);
      if ((tableSpecifiedHeight > 0) && 
          (tableSpecifiedHeight != NS_UNCONSTRAINEDSIZE)) {
        SetNeedToInitiateSpecialReflow(PR_TRUE);
      }
    }
    nsIFrame* lastChildReflowed = nsnull;
    PRBool willInitiateSpecialReflow = 
      ((NeedToInitiateSpecialReflow() || InitiatedSpecialReflow()) && 
       (aReflowState.mFlags.mSpecialHeightReflow || !NeedSpecialReflow()));

    // do the pass 2 reflow unless this is a special height reflow and we will be 
    // initiating a special height reflow
    if (!(aReflowState.mFlags.mSpecialHeightReflow && willInitiateSpecialReflow)) {
      // if we need to initiate a special height reflow, then don't constrain the 
      // height of the reflow before that
      nscoord availHeight = (willInitiateSpecialReflow)
                            ? NS_UNCONSTRAINEDSIZE : aReflowState.availableHeight;

      ReflowTable(aPresContext, aDesiredSize, aReflowState, availHeight, nextReason, 
                  lastChildReflowed, doCollapse, balanced, aStatus);
    }
    if (willInitiateSpecialReflow && NS_FRAME_IS_COMPLETE(aStatus)) {
      // distribute extra vertical space to rows
      aDesiredSize.height = CalcDesiredHeight(aPresContext, aReflowState); 
      ((nsHTMLReflowState::ReflowStateFlags&)aReflowState.mFlags).mSpecialHeightReflow = PR_TRUE;
      // save the previous special height reflow initiator, install us as the new one
      nsIFrame* specialReflowInitiator = aReflowState.mPercentHeightReflowInitiator;
      ((nsHTMLReflowState&)aReflowState).mPercentHeightReflowInitiator = this;

      ((nsHTMLReflowState::ReflowStateFlags&)aReflowState.mFlags).mSpecialHeightReflow = PR_TRUE;
      ReflowTable(aPresContext, aDesiredSize, aReflowState, aReflowState.availableHeight, 
                  nextReason, lastChildReflowed, doCollapse, balanced, aStatus);
      // restore the previous special height reflow initiator
      ((nsHTMLReflowState&)aReflowState).mPercentHeightReflowInitiator = specialReflowInitiator;
      // XXX We should call SetInitiatedSpecialReflow(PR_FALSE) at some point, but it is difficult to tell when
      SetInitiatedSpecialReflow(PR_TRUE);
      
      if (lastChildReflowed && NS_FRAME_IS_NOT_COMPLETE(aStatus)) {
        // if there is an incomplete child, then set the desired height to include it but not the next one
        nsRect childRect;
        lastChildReflowed->GetRect(childRect);
        nsMargin borderPadding = GetChildAreaOffset(*aPresContext, &aReflowState);
        aDesiredSize.height = borderPadding.top + GetCellSpacingY() + childRect.height;
      }
      haveDesiredHeight = PR_TRUE;
    }
  }
  else if (aReflowState.mFlags.mSpecialHeightReflow) {
    aDesiredSize.width  = mRect.width;
    aDesiredSize.height = mRect.height;
#if defined DEBUG_TABLE_REFLOW_TIMING
    nsTableFrame::DebugReflow(this, (nsHTMLReflowState&)aReflowState, &aDesiredSize, aStatus);
#endif
    SetNeedSpecialReflow(PR_FALSE);
    SetNeedToInitiateSpecialReflow(PR_FALSE);
    return NS_OK;
  }

  aDesiredSize.width = GetDesiredWidth();
  if (!haveDesiredHeight) {
    aDesiredSize.height = CalcDesiredHeight(aPresContext, aReflowState); 
  }
  if (IsRowInserted()) {
    ProcessRowInserted(aPresContext, *this, PR_TRUE, aDesiredSize.height);
  }

  nsMargin borderPadding = GetChildAreaOffset(*aPresContext, &aReflowState);
  SetColumnDimensions(aPresContext, aDesiredSize.height, borderPadding);
  if (doCollapse) {
    AdjustForCollapsingRows(aPresContext, aDesiredSize.height);
    AdjustForCollapsingCols(aPresContext, aDesiredSize.width);
  }

  // See if we need to calc max elem and/or preferred widths. This isn't done on 
  // continuations or if we have balanced (since it was done then) 
  if ((aDesiredSize.maxElementSize || (aDesiredSize.mFlags & NS_REFLOW_CALC_MAX_WIDTH)) &&
      !mPrevInFlow && !balanced) {
    // Since the calculation has some cost, avoid doing it for an unconstrained initial 
    // reflow (it was done when the strategy was initialized in pass 1 above) and most
    // unconstrained resize reflows. XXX The latter optimization could be a problem if the
    // parent of a nested table starts doing unconstrained resize reflows to get max elem/preferred 
    if ((NS_UNCONSTRAINEDSIZE != aReflowState.availableWidth) ||
        (eReflowReason_Incremental == aReflowState.reason)    || 
        (eReflowReason_StyleChange == aReflowState.reason)    ||
        ((eReflowReason_Resize == aReflowState.reason) &&
         HasPctCol() && IsAutoWidth())) {
      nscoord minWidth, prefWidth;
      CalcMinAndPreferredWidths(aPresContext, aReflowState, PR_TRUE, minWidth, prefWidth);
      SetMinWidth(minWidth);
      SetPreferredWidth(prefWidth);
    }
  }
  // See if we need to return our max element size
  if (aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width  = GetMinWidth();
    aDesiredSize.maxElementSize->height = 0;
  }
  // See if we need to return our maximum width
  if (aDesiredSize.mFlags & NS_REFLOW_CALC_MAX_WIDTH) {
    aDesiredSize.mMaximumWidth = GetPreferredWidth();
  }

  if (aReflowState.mFlags.mSpecialHeightReflow) {
    SetNeedSpecialReflow(PR_FALSE);
    SetNeedToInitiateSpecialReflow(PR_FALSE);
  }
#if defined DEBUG_TABLE_REFLOW_TIMING
  nsTableFrame::DebugReflow(this, (nsHTMLReflowState&)aReflowState, &aDesiredSize, aStatus);
#endif

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return rv;
}

nsresult 
nsTableFrame::ReflowTable(nsIPresContext*          aPresContext,
                          nsHTMLReflowMetrics&     aDesiredSize,
                          const nsHTMLReflowState& aReflowState,
                          nscoord                  aAvailHeight,
                          nsReflowReason           aReason,
                          nsIFrame*&               aLastChildReflowed,
                          PRBool&                  aDoCollapse,
                          PRBool&                  aDidBalance,
                          nsReflowStatus&          aStatus)
{
  nsresult rv = NS_OK;
  aDoCollapse = PR_FALSE;
  aDidBalance = PR_FALSE;
  aLastChildReflowed = nsnull;

  PRBool isPaginated;
  aPresContext->IsPaginated(&isPaginated);

  PRBool haveReflowedColGroups = PR_TRUE;
  if (!mPrevInFlow) {
    if (NeedStrategyInit()) {
      mTableLayoutStrategy->Initialize(aPresContext, aReflowState);
      BalanceColumnWidths(aPresContext, aReflowState); 
      aDidBalance = PR_TRUE;
    }
    if (NeedStrategyBalance()) {
      BalanceColumnWidths(aPresContext, aReflowState);
      aDidBalance = PR_TRUE;
    }
    haveReflowedColGroups = HaveReflowedColGroups();
  }
  // Constrain our reflow width to the computed table width (of the 1st in flow).
  // and our reflow height to our avail height minus border, padding, cellspacing
  aDesiredSize.width = GetDesiredWidth();
  nsTableReflowState reflowState(*aPresContext, aReflowState, *this, aReason, 
                                 aDesiredSize.width, aAvailHeight);
  ReflowChildren(aPresContext, reflowState, haveReflowedColGroups, PR_FALSE, aStatus, aLastChildReflowed);

  // If we're here that means we had to reflow all the rows, e.g., the column widths 
  // changed. We need to make sure that any damaged areas are repainted
  if (!mRect.IsEmpty()) {
    Invalidate(aPresContext, mRect);
  }

  if (eReflowReason_Resize == aReflowState.reason) {
    if (!DidResizeReflow()) {
      // XXX we need to do this in other cases as well, but it needs to be made more incremental
      aDoCollapse = PR_TRUE;
      SetResizeReflow(PR_TRUE);
    }
  }
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
  if (prevInFlow) {
    nsIFrame* prevOverflowFrames = prevInFlow->GetOverflowFrames(aPresContext, PR_TRUE);
    if (prevOverflowFrames) {
      // When pushing and pulling frames we need to check for whether any
      // views need to be reparented.
      for (nsIFrame* f = prevOverflowFrames; f; f->GetNextSibling(&f)) {
        nsHTMLContainerFrame::ReparentFrameView(aPresContext, f, prevInFlow, this);
      }
      mFrames.AppendFrames(this, prevOverflowFrames);
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

NS_METHOD 
nsTableFrame::CollapseRowGroupIfNecessary(nsIPresContext* aPresContext,
                                          nsIFrame* aRowGroupFrame,
                                          const nscoord& aYTotalOffset,
                                          nscoord& aYGroupOffset, PRInt32& aRowX)
{
  const nsStyleVisibility* groupVis;
  aRowGroupFrame->GetStyleData(eStyleStruct_Visibility, ((const nsStyleStruct *&)groupVis));
  
  PRBool collapseGroup = (NS_STYLE_VISIBILITY_COLLAPSE == groupVis->mVisible);
  nsIFrame* rowFrame;
  aRowGroupFrame->FirstChild(aPresContext, nsnull, &rowFrame);

  while (nsnull != rowFrame) {
    const nsStyleDisplay* rowDisplay;
    rowFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)rowDisplay));
    if (NS_STYLE_DISPLAY_TABLE_ROW == rowDisplay->mDisplay) {
      const nsStyleVisibility* rowVis;
      rowFrame->GetStyleData(eStyleStruct_Visibility, ((const nsStyleStruct *&)rowVis));
      nsRect rowRect;
      rowFrame->GetRect(rowRect);
      if (collapseGroup || (NS_STYLE_VISIBILITY_COLLAPSE == rowVis->mVisible)) {
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
            CellData* cellData = cellMap->GetDataAt(aRowX, colX);
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
    const nsStyleVisibility* groupVis;
    groupFrame->GetStyleData(eStyleStruct_Visibility, ((const nsStyleStruct *&)groupVis));
    
    PRBool collapseGroup = (NS_STYLE_VISIBILITY_COLLAPSE == groupVis->mVisible);
    nsTableIterator colIter(aPresContext, *groupFrame, eTableDIR);
    nsIFrame* colFrame = colIter.First();
    // iterate over the cols in the col group
    while (nsnull != colFrame) {
      const nsStyleDisplay* colDisplay;
      colFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)colDisplay));
      if (NS_STYLE_DISPLAY_TABLE_COLUMN == colDisplay->mDisplay) {
        const nsStyleVisibility* colVis;
        colFrame->GetStyleData(eStyleStruct_Visibility, ((const nsStyleStruct *&)colVis));
        PRBool collapseCol = (NS_STYLE_VISIBILITY_COLLAPSE == colVis->mVisible);
        PRInt32 colWidth = GetColumnWidth(colX);
        if (collapseGroup || collapseCol) {
          xOffset += colWidth + cellSpacingX;
        }
        nsTableCellFrame* lastCell  = nsnull;
        nsTableCellFrame* cellFrame = nsnull;
        for (PRInt32 rowX = 0; rowX < numRows; rowX++) {
          CellData* cellData = cellMap->GetDataAt(rowX, colX);
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
      if (!firstAppendedColGroup) {
        firstAppendedColGroup = f;
        nsTableColGroupFrame* lastColGroup = (nsTableColGroupFrame *)mColGroups.LastChild();
        startColIndex = (lastColGroup) 
          ? lastColGroup->GetStartColumnIndex() + lastColGroup->GetColCount() : 0;
      }
      // Append the new col group frame
      mColGroups.AppendFrame(nsnull, f);
    } else if (IsRowGroup(display->mDisplay)) {
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

  SetNeedStrategyInit(PR_TRUE); // XXX assume the worse
  AppendDirtyReflowCommand(&aPresShell, this);

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
    SetNeedStrategyInit(PR_TRUE);
  } else if (IsRowGroup(display->mDisplay)) {
    nsFrameList newList(aFrameList);
    nsIFrame* lastSibling = newList.LastChild();
    // Insert the frames in the sibling chain
    mFrames.InsertFrame(nsnull, aPrevFrame, aFrameList);

    InsertRowGroups(*aPresContext, aFrameList, lastSibling);
    SetNeedStrategyInit(PR_TRUE);
  } else {
    // Just insert the frame and don't worry about reflowing it
    mFrames.InsertFrame(nsnull, aPrevFrame, aFrameList);
    return NS_OK;
  }

  AppendDirtyReflowCommand(&aPresShell, this);

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
      nsTableColFrame* colFrame = (nsTableColFrame*)mColFrames.SafeElementAt(colX);
      if (colFrame) {
        RemoveCol(*aPresContext, colGroup, colX, PR_TRUE, PR_FALSE);
      }
    }

    PRInt32 numAnonymousColsToAdd = GetColCount() - mColFrames.Count();
    if (numAnonymousColsToAdd > 0) {
      // this sets the child list, updates the col cache and cell map
      CreateAnonymousColFrames(*aPresContext, numAnonymousColsToAdd,
                               eColAnonymousCell, PR_TRUE);
    }

    // XXX This could probably be optimized with much effort
    SetNeedStrategyInit(PR_TRUE);
    AppendDirtyReflowCommand(GetPresShellNoAddref(aPresContext), this);
  } else {
    nsTableRowGroupFrame* rgFrame = GetRowGroupFrame(aOldFrame);
    if (rgFrame) {
      PRInt32 startRowIndex = rgFrame->GetStartRowIndex();
      PRInt32 numRows = rgFrame->GetRowCount();
      // remove the row group from the cell map
      nsTableCellMap* cellMap = GetCellMap();
      if (cellMap) {
        cellMap->RemoveGroupCellMap(rgFrame);
      }
      // only remove cols that are of type eTypeAnonymous cell (they are at the end)
      PRInt32 numColsInMap = GetColCount(); // cell map's notion of num cols
      PRInt32 numColsInCache = mColFrames.Count();
      if (numColsInCache > numColsInMap) {
        PRInt32 numColsNotRemoved = DestroyAnonymousColFrames(*aPresContext, numColsInCache - numColsInMap);
        // if the cell map has fewer cols than the cache, correct it
        if (numColsNotRemoved > 0) {
          cellMap->AddColsAtEnd(numColsNotRemoved);
        }
      }
      else NS_ASSERTION(numColsInCache == numColsInMap, "cell map has too many cols");

      AdjustRowIndices(aPresContext, startRowIndex, -numRows);
      // remove the row group frame from the sibling chain
      mFrames.DestroyFrame(aPresContext, aOldFrame);

      // XXX This could probably be optimized with much effort
      SetNeedStrategyInit(PR_TRUE);
      AppendDirtyReflowCommand(GetPresShellNoAddref(aPresContext), this);
    } else {
      // Just remove the frame
      mFrames.DestroyFrame(aPresContext, aOldFrame);
      return NS_OK;
    }
  }
  return NS_OK;
}

NS_METHOD 
nsTableFrame::IncrementalReflow(nsIPresContext*          aPresContext,
                                const nsHTMLReflowState& aReflowState,
                                nsReflowStatus&          aStatus)
{
  // Constrain our reflow width to the computed table width. Note: this is
  // based on the width of the first-in-flow
  PRInt32 lastWidth = mRect.width;
  if (mPrevInFlow) {
    nsTableFrame* table = (nsTableFrame*)GetFirstInFlow();
    lastWidth = table->mRect.width;
  }
  nsTableReflowState state(*aPresContext, aReflowState, *this, eReflowReason_Incremental,
                           lastWidth, aReflowState.availableHeight); 

  // the table is a target if its path has a reflow command
  nsHTMLReflowCommand* command = aReflowState.path->mReflowCommand;
  if (command)
    IR_TargetIsMe(aPresContext, state, aStatus);

  // see if the chidren are targets as well
  nsReflowPath::iterator iter = aReflowState.path->FirstChild();
  nsReflowPath::iterator end  = aReflowState.path->EndChildren();
  for (; iter != end; ++iter)
    IR_TargetIsChild(aPresContext, state, aStatus, *iter);

  return NS_OK;
}

NS_METHOD 
nsTableFrame::IR_TargetIsMe(nsIPresContext*      aPresContext,
                            nsTableReflowState&  aReflowState,
                            nsReflowStatus&      aStatus)
{
  nsresult rv = NS_OK;
  aStatus = NS_FRAME_COMPLETE;

  nsReflowType type;
  aReflowState.reflowState.path->mReflowCommand->GetType(type);

  switch (type) {
    case eReflowType_StyleChanged :
      rv = IR_StyleChanged(aPresContext, aReflowState, aStatus);
      break;
    case eReflowType_ContentChanged :
      NS_ASSERTION(PR_FALSE, "illegal reflow type: ContentChanged");
      rv = NS_ERROR_ILLEGAL_VALUE;
      break;
    case eReflowType_ReflowDirty: {
      // reflow the dirty children
      nsTableReflowState reflowState(*aPresContext, aReflowState.reflowState, *this, eReflowReason_Initial,
                                     aReflowState.availSize.width, aReflowState.availSize.height); 
      nsIFrame* lastReflowed;
      PRBool reflowedAtLeastOne; 
      ReflowChildren(aPresContext, reflowState, PR_FALSE, PR_TRUE, aStatus, lastReflowed, &reflowedAtLeastOne);
      if (!reflowedAtLeastOne)
        // XXX For now assume the worse
        SetNeedStrategyInit(PR_TRUE);
      }
      break;
    default:
      NS_NOTYETIMPLEMENTED("unexpected reflow command type");
      rv = NS_ERROR_NOT_IMPLEMENTED;
      break;
  }

  return rv;
}

NS_METHOD nsTableFrame::IR_StyleChanged(nsIPresContext*      aPresContext,
                                        nsTableReflowState&  aReflowState,
                                        nsReflowStatus&      aStatus)
{
  // we presume that all the easy optimizations were done in the nsHTMLStyleSheet 
  // before we were called here
  SetNeedStrategyInit(PR_TRUE);
  return NS_OK;
}

static void
DivideBCBorderSize(nscoord  aPixelSize,
                   nscoord& aSmallHalf,
                   nscoord& aLargeHalf)
{
  aSmallHalf = aPixelSize / 2;
  aLargeHalf = ((aSmallHalf + aSmallHalf) < aPixelSize) ? aSmallHalf + 1 : aSmallHalf;
}

nsMargin*  
nsTableFrame::GetBCBorder(nsIPresContext& aPresContext,
                          PRBool          aInnerBorderOnly,
                          nsMargin&       aBorder) const
{
  aBorder.top = aBorder.right = aBorder.bottom = aBorder.left = 0;

  GET_PIXELS_TO_TWIPS(&aPresContext, p2t);
  BCPropertyData* propData = 
    (BCPropertyData*)nsTableFrame::GetProperty(&aPresContext, (nsIFrame*)this, nsLayoutAtoms::tableBCProperty, PR_FALSE);
  if (propData) {
    nsCompatibility mode;
    aPresContext.GetCompatibilityMode(&mode);
    if ((eCompatibility_NavQuirks != mode) || aInnerBorderOnly) {
      nscoord smallHalf, largeHalf;

      DivideBCBorderSize(propData->mTopBorderWidth, smallHalf, largeHalf);
      aBorder.top += NSToCoordRound(p2t * (float)smallHalf); 

      DivideBCBorderSize(propData->mRightBorderWidth, smallHalf, largeHalf);
      aBorder.right += NSToCoordRound(p2t * (float)largeHalf); 

      DivideBCBorderSize(propData->mBottomBorderWidth, smallHalf, largeHalf);
      aBorder.bottom += NSToCoordRound(p2t * (float)largeHalf); 

      DivideBCBorderSize(propData->mLeftBorderWidth, smallHalf, largeHalf);
      aBorder.left += NSToCoordRound(p2t * (float)smallHalf); 
    }
    else {
      aBorder.top    += NSToCoordRound(p2t * (float)propData->mTopBorderWidth);
      aBorder.right  += NSToCoordRound(p2t * (float)propData->mRightBorderWidth);
      aBorder.bottom += NSToCoordRound(p2t * (float)propData->mBottomBorderWidth);
      aBorder.left   += NSToCoordRound(p2t * (float)propData->mLeftBorderWidth);
    }
  }
  return &aBorder;
}

static
void GetSeparateModelBorderPadding(nsIPresContext&          aPresContext,
                                   const nsHTMLReflowState* aReflowState,
                                   nsIStyleContext&         aStyleContext,
                                   nsMargin&                aBorderPadding)
{
  const nsStyleBorder* border =
    (const nsStyleBorder*)aStyleContext.GetStyleData(eStyleStruct_Border);
  border->GetBorder(aBorderPadding);
  if (aReflowState) {
    aBorderPadding += aReflowState->mComputedPadding;
  }
}

nsMargin 
nsTableFrame::GetChildAreaOffset(nsIPresContext&          aPresContext,
                                 const nsHTMLReflowState* aReflowState) const
{
  nsMargin offset(0,0,0,0);
  if (IsBorderCollapse()) {
    nsCompatibility mode;
    aPresContext.GetCompatibilityMode(&mode);
    if (eCompatibility_NavQuirks == mode) {
      nsTableFrame* firstInFlow = (nsTableFrame*)GetFirstInFlow(); if (!firstInFlow) ABORT1(offset);
      nscoord smallHalf, largeHalf;
      GET_PIXELS_TO_TWIPS(&aPresContext, p2t);
      BCPropertyData* propData = 
        (BCPropertyData*)nsTableFrame::GetProperty(&aPresContext, (nsIFrame*)firstInFlow, nsLayoutAtoms::tableBCProperty, PR_FALSE);
      if (!propData) ABORT1(offset);

      DivideBCBorderSize(propData->mTopBorderWidth, smallHalf, largeHalf);
      offset.top += NSToCoordRound(p2t * (float)largeHalf);

      DivideBCBorderSize(propData->mRightBorderWidth, smallHalf, largeHalf);
      offset.right += NSToCoordRound(p2t * (float)smallHalf);

      DivideBCBorderSize(propData->mBottomBorderWidth, smallHalf, largeHalf);
      offset.bottom += NSToCoordRound(p2t * (float)smallHalf);

      DivideBCBorderSize(propData->mLeftBorderWidth, smallHalf, largeHalf);
      offset.left += NSToCoordRound(p2t * (float)largeHalf);
    }
  }
  else {
    if (!mStyleContext) ABORT1(offset);
    GetSeparateModelBorderPadding(aPresContext, aReflowState, *mStyleContext, offset);
  }
  return offset;
}

nsMargin 
nsTableFrame::GetContentAreaOffset(nsIPresContext&          aPresContext,
                                   const nsHTMLReflowState* aReflowState) const
{
  nsMargin offset(0,0,0,0);
  if (IsBorderCollapse()) {
    GetBCBorder(aPresContext, PR_FALSE, offset);
  }
  else {
    if (!mStyleContext) ABORT1(offset);
    GetSeparateModelBorderPadding(aPresContext, aReflowState, *mStyleContext, offset);
  }
  return offset;
}

// Recovers the reflow state to what it should be if aKidFrame is about to be 
// reflowed. Restores y, footerFrame, firstBodySection and availSize.height (if
// the height is constrained)
nsresult
nsTableFrame::RecoverState(nsIPresContext&     aPresContext,
                           nsTableReflowState& aReflowState,
                           nsIFrame*           aKidFrame)
{
  nsMargin borderPadding = GetChildAreaOffset(aPresContext, &aReflowState.reflowState);
  aReflowState.y = borderPadding.top;

  nscoord cellSpacingY = GetCellSpacingY();
  // Get the ordered children and find aKidFrame in the list
  nsAutoVoidArray rowGroups;
  PRUint32 numRowGroups;
  OrderRowGroups(rowGroups, numRowGroups, &aReflowState.firstBodySection);
  
  // Walk the list of children looking for aKidFrame
  for (PRUint32 childX = 0; childX < numRowGroups; childX++) {
    nsIFrame* childFrame = (nsIFrame*)rowGroups.ElementAt(childX);
    nsTableRowGroupFrame* rgFrame = GetRowGroupFrame(childFrame);
    if (!rgFrame) continue; // skip foreign frame types
   
    // If this is a footer row group, remember it
    const nsStyleDisplay* display;
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
    if (NS_UNCONSTRAINEDSIZE != aReflowState.availSize.height) {
      aReflowState.availSize.height -= kidSize.height;
    }

    // Update the running y-offset. Don't do this for the footer frame
    if (childFrame != aReflowState.footerFrame) {
      aReflowState.y += kidSize.height;
    }
  }

  return NS_OK;
}

void
nsTableFrame::InitChildReflowState(nsIPresContext&    aPresContext,                     
                                   nsHTMLReflowState& aReflowState)                                    
{
  nsMargin collapseBorder;
  nsMargin padding(0,0,0,0);
  nsMargin* pCollapseBorder = nsnull;
  if (IsBorderCollapse()) {
    nsTableRowGroupFrame* rgFrame = GetRowGroupFrame(aReflowState.frame);
    if (rgFrame) {
      GET_PIXELS_TO_TWIPS(&aPresContext, p2t);
      pCollapseBorder = rgFrame->GetBCBorderWidth(p2t, collapseBorder);
    }
  }
  aReflowState.Init(&aPresContext, -1, -1, pCollapseBorder, &padding);
}

NS_METHOD 
nsTableFrame::IR_TargetIsChild(nsIPresContext*      aPresContext,
                               nsTableReflowState&  aReflowState,
                               nsReflowStatus&      aStatus,
                               nsIFrame*            aNextFrame)

{
  if (!aPresContext) ABORT1(NS_ERROR_NULL_POINTER);
  nsresult rv;
  // Recover the state as if aNextFrame is about to be reflowed
  RecoverState(*aPresContext, aReflowState, aNextFrame);

  // Remember the old rect
  nsRect  oldKidRect;
  aNextFrame->GetRect(oldKidRect);

  // Pass along the reflow command, don't request a max element size, rows will do that
  nsHTMLReflowMetrics desiredSize(nsnull);
  nsSize kidAvailSize(aReflowState.availSize);
  nsHTMLReflowState kidReflowState(aPresContext, aReflowState.reflowState, aNextFrame, 
                                   kidAvailSize, aReflowState.reason);
  InitChildReflowState(*aPresContext, kidReflowState);

  rv = ReflowChild(aNextFrame, aPresContext, desiredSize, kidReflowState,
                   aReflowState.x, aReflowState.y, 0, aStatus);

  // Place the row group frame. Don't use PlaceChild(), because it moves
  // the footer frame as well. We'll adjust the footer frame later on in
  // AdjustSiblingsAfterReflow()
  nsRect  kidRect(aReflowState.x, aReflowState.y, desiredSize.width, desiredSize.height);
  FinishReflowChild(aNextFrame, aPresContext, nsnull, desiredSize, aReflowState.x, aReflowState.y, 0);

  // Adjust the running y-offset
  aReflowState.y += desiredSize.height + GetCellSpacingY();

  // If our height is constrained, then update the available height
  if (NS_UNCONSTRAINEDSIZE != aReflowState.availSize.height) {
    aReflowState.availSize.height -= desiredSize.height;
  }

  // If the column width info is valid, then adjust the row group frames
  // that follow. Otherwise, return and we'll recompute the column widths
  // and reflow all the row group frames
  if (!NeedsReflow(aReflowState.reflowState)) {
    // If the row group frame changed height, then damage the horizontal strip
    // that was either added or went away
    if (desiredSize.height != oldKidRect.height) {
      nsRect dirtyRect;
      dirtyRect.x = 0;
      dirtyRect.y = PR_MIN(oldKidRect.YMost(), kidRect.YMost());
      dirtyRect.width = mRect.width;
      dirtyRect.height = PR_MAX(oldKidRect.YMost(), kidRect.YMost()) - dirtyRect.y;
      Invalidate(aPresContext, dirtyRect);
    }

    // Adjust the row groups that follow
    AdjustSiblingsAfterReflow(aPresContext, aReflowState, aNextFrame, 
                              desiredSize.height - oldKidRect.height);

    // XXX Is this needed?
#if 0
    AdjustForCollapsingRows(aPresContext, aDesiredSize.height);
    AdjustForCollapsingCols(aPresContext, aDesiredSize.width);
#endif
  }

  return rv;
}

// Position and size aKidFrame and update our reflow state. The origin of
// aKidRect is relative to the upper-left origin of our frame
void nsTableFrame::PlaceChild(nsIPresContext*      aPresContext,
                              nsTableReflowState&  aReflowState,
                              nsIFrame*            aKidFrame,
                              nsHTMLReflowMetrics& aDesiredSize)
{
  // Place and size the child
  FinishReflowChild(aKidFrame, aPresContext, nsnull, aDesiredSize, aReflowState.x, aReflowState.y, 0);

  // Adjust the running y-offset
  aReflowState.y += aDesiredSize.height;

  // If our height is constrained, then update the available height
  if (NS_UNCONSTRAINEDSIZE != aReflowState.availSize.height) {
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
}

void
nsTableFrame::OrderRowGroups(nsVoidArray&           aChildren,
                             PRUint32&              aNumRowGroups,
                             nsIFrame**             aFirstBody,
                             nsTableRowGroupFrame** aHead,
                             nsTableRowGroupFrame** aFoot) const
{
  aChildren.Clear();
  nsIFrame* head = nsnull;
  nsIFrame* foot = nsnull;
  // initialize out parameters, if present
  if (aFirstBody) *aFirstBody = nsnull;
  if (aHead)      *aHead      = nsnull;
  if (aFoot)      *aFoot      = nsnull;
  
  nsIFrame* kidFrame = mFrames.FirstChild();
  nsAutoVoidArray nonRowGroups;
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
          if (aHead) {
            *aHead = (nsTableRowGroupFrame*)head;
          }
        }
        break;
      case NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP:
        if (foot) {
          aChildren.AppendElement(kidFrame);
        }
        else {
          foot = kidFrame;
          if (aFoot) {
            *aFoot = (nsTableRowGroupFrame*)foot;
          }
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
    // Get the next sibling but skip it if it's also the next-in-flow, since
    // a next-in-flow will not be part of the current table.
    while (kidFrame) {
      nsIFrame* nif;
      kidFrame->GetNextInFlow(&nif);
      kidFrame->GetNextSibling(&kidFrame);
      if (kidFrame != nif) 
        break;
    }
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

static PRBool
IsRepeatable(nsTableRowGroupFrame& aHeaderOrFooter,
             nscoord               aPageHeight)
{
  nsRect rect;
  aHeaderOrFooter.GetRect(rect);

  return (rect.height < (aPageHeight / 4));
}

// Reflow the children based on the avail size and reason in aReflowState
// update aReflowMetrics a aStatus
NS_METHOD 
nsTableFrame::ReflowChildren(nsIPresContext*     aPresContext,
                             nsTableReflowState& aReflowState,
                             PRBool              aDoColGroups,
                             PRBool              aDirtyOnly,
                             nsReflowStatus&     aStatus,
                             nsIFrame*&          aLastChildReflowed,
                             PRBool*             aReflowedAtLeastOne)
{
  aStatus = NS_FRAME_COMPLETE;
  aLastChildReflowed = nsnull;

  nsIFrame* prevKidFrame = nsnull;
  nsresult  rv = NS_OK;
  nscoord   cellSpacingY = GetCellSpacingY();

  PRBool isPaginated;
  aPresContext->IsPaginated(&isPaginated);

  nsAutoVoidArray rowGroups;
  PRUint32 numRowGroups;
  nsTableRowGroupFrame *thead, *tfoot;
  OrderRowGroups(rowGroups, numRowGroups, &aReflowState.firstBodySection, &thead, &tfoot);
  PRBool haveReflowedRowGroup = PR_FALSE;
  PRBool pageBreak = PR_FALSE;
  for (PRUint32 childX = 0; ((PRInt32)childX) < rowGroups.Count(); childX++) {
    nsIFrame* kidFrame = (nsIFrame*)rowGroups.ElementAt(childX);
    // Get the frame state bits
    nsFrameState  frameState;
    kidFrame->GetFrameState(&frameState);

    // See if we should only reflow the dirty child frames
    PRBool doReflowChild = PR_TRUE;
    if (aDirtyOnly && ((frameState & NS_FRAME_IS_DIRTY) == 0)) {
      doReflowChild = PR_FALSE;
    }

    if (doReflowChild) {
      if (pageBreak) {
        PushChildren(aPresContext, kidFrame, prevKidFrame);
        aStatus = NS_FRAME_NOT_COMPLETE;
        break;
      }

      nsSize kidAvailSize(aReflowState.availSize);
      // if the child is a tbody in paginated mode reduce the height by a repeated footer
      nsIFrame* repeatedFooter = nsnull;
      nscoord repeatedFooterHeight = 0;
      if (isPaginated && (NS_UNCONSTRAINEDSIZE != kidAvailSize.height)) {
        const nsStyleDisplay* display;
        kidFrame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct *&)display);
        if (NS_STYLE_DISPLAY_TABLE_ROW_GROUP == display->mDisplay) { // the child is a tbody
          nsIFrame* lastChild = (nsIFrame*)rowGroups.ElementAt(numRowGroups - 1);
          lastChild->GetStyleData(eStyleStruct_Display, (const nsStyleStruct *&)display);
          if (NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP == display->mDisplay) { // the last child is a tfoot
            if (((nsTableRowGroupFrame*)lastChild)->IsRepeatable()) {
              nsRect rect;
              lastChild->GetRect(rect);
              repeatedFooterHeight = rect.height;
              if (repeatedFooterHeight + cellSpacingY < kidAvailSize.height) {
                repeatedFooter = lastChild;
                kidAvailSize.height -= repeatedFooterHeight + cellSpacingY;
              }
            }
          }
        }
      }

      nsHTMLReflowMetrics desiredSize(nsnull);
      desiredSize.width = desiredSize.height = desiredSize.ascent = desiredSize.descent = 0;
  
      if (childX < numRowGroups) {  
        // Reflow the child into the available space
        nsHTMLReflowState  kidReflowState(aPresContext, aReflowState.reflowState, kidFrame, 
                                          kidAvailSize, aReflowState.reason);
        InitChildReflowState(*aPresContext, kidReflowState);
        // XXX fix up bad mComputedWidth for scroll frame
        kidReflowState.mComputedWidth = PR_MAX(kidReflowState.mComputedWidth, 0);
  
        // If this isn't the first row group, then we can't be at the top of the page
        if (childX > 0) {
          kidReflowState.mFlags.mIsTopOfPage = PR_FALSE;
        }
        aReflowState.y += cellSpacingY;
        
        // record the next in flow in case it gets destroyed and the row group array
        // needs to be recomputed.
        nsIFrame* kidNextInFlow;
        kidFrame->GetNextInFlow(&kidNextInFlow);
  
        rv = ReflowChild(kidFrame, aPresContext, desiredSize, kidReflowState,
                         aReflowState.x, aReflowState.y, 0, aStatus);
        haveReflowedRowGroup = PR_TRUE;
        aLastChildReflowed   = kidFrame;

        pageBreak = PR_FALSE;
        // see if there is a page break after this row group or before the next one
        if (NS_FRAME_IS_COMPLETE(aStatus) && isPaginated && 
            (NS_UNCONSTRAINEDSIZE != kidReflowState.availableHeight)) {
          nsIFrame* nextKid = (childX + 1 < numRowGroups) ? (nsIFrame*)rowGroups.ElementAt(childX + 1) : nsnull;
          pageBreak = PageBreakAfter(*kidFrame, nextKid);
        }
        // Place the child
        PlaceChild(aPresContext, aReflowState, kidFrame, desiredSize);
  
        // Remember where we just were in case we end up pushing children
        prevKidFrame = kidFrame;
 
        // Special handling for incomplete children
        if (NS_FRAME_IS_NOT_COMPLETE(aStatus)) {         
          kidFrame->GetNextInFlow(&kidNextInFlow);
          if (!kidNextInFlow) {
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
          if (repeatedFooter) {
            kidAvailSize.height = repeatedFooterHeight;
            nsHTMLReflowState footerReflowState(aPresContext, aReflowState.reflowState, repeatedFooter, 
                                                kidAvailSize, aReflowState.reason);
            InitChildReflowState(*aPresContext, footerReflowState);
            aReflowState.y += cellSpacingY;
            nsReflowStatus footerStatus;
            rv = ReflowChild(repeatedFooter, aPresContext, desiredSize, footerReflowState,
                             aReflowState.x, aReflowState.y, 0, footerStatus);
            PlaceChild(aPresContext, aReflowState, repeatedFooter, desiredSize);
          }
          break;
        }
      }
    }
    else if (childX < numRowGroups) { // it is a row group but isn't being reflowed
      nsRect kidRect;
      kidFrame->GetRect(kidRect);
      if (haveReflowedRowGroup) { 
        if (kidRect.y != aReflowState.y) {
          Invalidate(aPresContext, kidRect); // invalidate the old position
          kidRect.y = aReflowState.y;
          kidFrame->SetRect(aPresContext, kidRect);        // move to the new position
          Invalidate(aPresContext, kidRect); // invalidate the new position
        }
      }
      aReflowState.y += cellSpacingY + kidRect.height;
    }
  }

  // if required, give the colgroups their initial reflows
  if (aDoColGroups) {
    nsHTMLReflowMetrics kidMet(nsnull);
    for (nsIFrame* kidFrame = mColGroups.FirstChild(); kidFrame; kidFrame->GetNextSibling(&kidFrame)) {
      nsHTMLReflowState kidReflowState(aPresContext, aReflowState.reflowState, kidFrame,
                                       aReflowState.availSize, aReflowState.reason);
      nsReflowStatus cgStatus;
      ReflowChild(kidFrame, aPresContext, kidMet, kidReflowState, 0, 0, 0, cgStatus);
      FinishReflowChild(kidFrame, aPresContext, nsnull, kidMet, 0, 0, 0);
    }
    SetHaveReflowedColGroups(PR_TRUE);
  }

  // set the repeatablility of headers and footers in the original table during its first reflow
  // the repeatability of header and footers on continued tables is handled when they are created
  if (isPaginated && !mPrevInFlow && (NS_UNCONSTRAINEDSIZE == aReflowState.availSize.height)) {
    nsRect actualRect;
    nsRect adjRect;
    aPresContext->GetPageDim(&actualRect, &adjRect);
    // don't repeat the thead or tfoot unless it is < 25% of the page height
    if (thead) {
      thead->SetRepeatable(IsRepeatable(*thead, actualRect.height));
    }
    if (tfoot) {
      tfoot->SetRepeatable(IsRepeatable(*tfoot, actualRect.height));
    }
  }

  if (aReflowedAtLeastOne) {
    *aReflowedAtLeastOne = haveReflowedRowGroup;
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
void nsTableFrame::BalanceColumnWidths(nsIPresContext*          aPresContext, 
                                       const nsHTMLReflowState& aReflowState)
{
  NS_ASSERTION(!mPrevInFlow, "never ever call me on a continuing frame!");

  // fixed-layout tables need to reinitialize the layout strategy. When there are scroll bars
  // reflow gets called twice and the 2nd time has the correct space available.
  // XXX this is very bad and needs to be changed
  if (!IsAutoLayout()) {
    mTableLayoutStrategy->Initialize(aPresContext, aReflowState);
  }

  // need to figure out the overall table width constraint
  // default case, get 100% of available space

  mTableLayoutStrategy->BalanceColumnWidths(aPresContext, aReflowState);
  //Dump(PR_TRUE, PR_TRUE);
  SetNeedStrategyBalance(PR_FALSE);                    // we have just balanced
  // cache the min, desired, and preferred widths
  nscoord minWidth, prefWidth;
  CalcMinAndPreferredWidths(aPresContext, aReflowState, PR_FALSE, minWidth, prefWidth);
  SetMinWidth(minWidth); 
  nscoord desWidth = CalcDesiredWidth(*aPresContext, aReflowState);
  SetDesiredWidth(desWidth);          
  SetPreferredWidth(prefWidth); 

}

// This width is based on the column widths array of the table.
// sum the width of each column and add in table insets
nscoord 
nsTableFrame::CalcDesiredWidth(nsIPresContext&          aPresContext,
                               const nsHTMLReflowState& aReflowState)
{
  NS_ASSERTION(!mPrevInFlow, "never ever call me on a continuing frame!");
  nsTableCellMap* cellMap = GetCellMap();
  if (!cellMap) {
    NS_ASSERTION(PR_FALSE, "never ever call me until the cell map is built!");
    return 0;
  }

  nscoord cellSpacing = GetCellSpacingX();
  PRInt32 tableWidth  = 0;

  PRInt32 numCols = GetColCount();
  for (PRInt32 colIndex = 0; colIndex < numCols; colIndex++) {
    nscoord totalColWidth = GetColumnWidth(colIndex);
    if (GetNumCellsOriginatingInCol(colIndex) > 0) { // skip degenerate cols
      totalColWidth += cellSpacing;           // add cell spacing to left of col
    }
    tableWidth += totalColWidth;
  }

  if (numCols > 0) {
    tableWidth += cellSpacing; // add last cellspacing
    // Add the width between the border edge and the child area
    nsMargin childOffset = GetChildAreaOffset(aPresContext, &aReflowState);
    tableWidth += childOffset.left + childOffset.right;
  } 

  return tableWidth;
}


nscoord 
nsTableFrame::CalcDesiredHeight(nsIPresContext*          aPresContext,
                                const nsHTMLReflowState& aReflowState) 
{
  nsTableCellMap* cellMap = GetCellMap();
  if (!cellMap) {
    NS_ASSERTION(PR_FALSE, "never ever call me until the cell map is built!");
    return 0;
  }
  nscoord  cellSpacingY = GetCellSpacingY();
  nsMargin borderPadding = GetChildAreaOffset(*aPresContext, &aReflowState);

  // get the natural height based on the last child's (row group or scroll frame) rect
  nsAutoVoidArray rowGroups;
  PRUint32 numRowGroups;
  OrderRowGroups(rowGroups, numRowGroups, nsnull);
  if (numRowGroups <= 0) return 0;

  nscoord desiredHeight = borderPadding.top + cellSpacingY + borderPadding.bottom;
  for (PRUint32 rgX = 0; rgX < numRowGroups; rgX++) {
    nsIFrame* rg = (nsIFrame*)rowGroups.ElementAt(rgX);
    if (rg) {
      nsRect rgRect;
      rg->GetRect(rgRect);
      desiredHeight += rgRect.height + cellSpacingY;
    }
  }

  // see if a specified table height requires dividing additional space to rows
  if (!mPrevInFlow) {
    nscoord tableSpecifiedHeight = CalcBorderBoxHeight(aPresContext, aReflowState);
    if ((tableSpecifiedHeight > 0) && 
        (tableSpecifiedHeight != NS_UNCONSTRAINEDSIZE) &&
        (tableSpecifiedHeight > desiredHeight)) {
      // proportionately distribute the excess height to unconstrained rows in each
      // unconstrained row group.We don't need to do this if it's an unconstrained reflow
      if (NS_UNCONSTRAINEDSIZE != aReflowState.availableWidth) { 
        DistributeHeightToRows(aPresContext, aReflowState, tableSpecifiedHeight - desiredHeight);
      }
      desiredHeight = tableSpecifiedHeight;
    }
  }
  return desiredHeight;
}

static
void ResizeCells(nsTableFrame&            aTableFrame,
                 nsIPresContext*          aPresContext,
                 const nsHTMLReflowState& aReflowState)
{
  nsAutoVoidArray rowGroups;
  PRUint32 numRowGroups;
  aTableFrame.OrderRowGroups(rowGroups, numRowGroups, nsnull);

  for (PRUint32 rgX = 0; (rgX < numRowGroups); rgX++) {
    nsTableRowGroupFrame* rgFrame = aTableFrame.GetRowGroupFrame((nsIFrame*)rowGroups.ElementAt(rgX));
    nsTableRowFrame* rowFrame = rgFrame->GetFirstRow();
    while (rowFrame) {
      rowFrame->DidResize(aPresContext, aReflowState);
      rowFrame = rowFrame->GetNextRow();
    }
  }
}

void
nsTableFrame::DistributeHeightToRows(nsIPresContext*          aPresContext,
                                     const nsHTMLReflowState& aReflowState,
                                     nscoord                  aAmount)
{
  float p2t;
  aPresContext->GetPixelsToTwips(&p2t);

  nscoord cellSpacingY = GetCellSpacingY();

  nsMargin borderPadding = GetChildAreaOffset(*aPresContext, &aReflowState);
  
  nsVoidArray rowGroups;
  PRUint32 numRowGroups;
  OrderRowGroups(rowGroups, numRowGroups, nsnull);

  nscoord amountUsed = 0;
  // distribute space to each pct height row whose row group doesn't have a computed 
  // height, and base the pct on the table height. If the row group had a computed 
  // height, then this was already done in nsTableRowGroupFrame::CalculateRowHeights
  nscoord pctBasis = aReflowState.mComputedHeight - (GetCellSpacingY() * (GetRowCount() + 1));
  nscoord yOriginRG = borderPadding.top + GetCellSpacingY();
  nscoord yEndRG = yOriginRG;
  PRUint32 rgX;
  for (rgX = 0; (rgX < numRowGroups); rgX++) {
    nsTableRowGroupFrame* rgFrame = GetRowGroupFrame((nsIFrame*)rowGroups.ElementAt(rgX));
    nscoord amountUsedByRG = 0;
    nscoord yOriginRow = 0;
    nsRect rgRect;
    rgFrame->GetRect(rgRect);
    if (rgFrame && !rgFrame->HasStyleHeight()) {
      nsTableRowFrame* rowFrame = rgFrame->GetFirstRow();
      while (rowFrame) {
        nsRect rowRect;
        rowFrame->GetRect(rowRect);
        if ((amountUsed < aAmount) && rowFrame->HasPctHeight()) {
          nscoord pctHeight = nsTableFrame::RoundToPixel(rowFrame->GetHeight(pctBasis), p2t);
          nscoord amountForRow = PR_MIN(aAmount - amountUsed, pctHeight - rowRect.height);
          if (amountForRow > 0) {
            rowRect.height += amountForRow;
            rowFrame->SetRect(aPresContext, rowRect);
            yOriginRow += rowRect.height + cellSpacingY;
            yEndRG += rowRect.height + cellSpacingY;
            amountUsed += amountForRow;
            amountUsedByRG += amountForRow;
            //rowFrame->DidResize(aPresContext, aReflowState);        
            nsTableFrame::RePositionViews(aPresContext, rowFrame);
          }
        }
        else {
          if (amountUsed > 0) {
            rowFrame->MoveTo(aPresContext, rowRect.x, yOriginRow);
            nsTableFrame::RePositionViews(aPresContext, rowFrame);
          }
          yOriginRow += rowRect.height + cellSpacingY;
          yEndRG += rowRect.height + cellSpacingY;
        }
        rowFrame = rowFrame->GetNextRow();
      }
      if (amountUsed > 0) {
        rgRect.y = yOriginRG;
        rgRect.height += amountUsedByRG;
        rgFrame->SetRect(aPresContext, rgRect);
      }
    }
    else if (amountUsed > 0) {
      rgFrame->MoveTo(aPresContext, 0, yOriginRG);
      // Make sure child views are properly positioned
      nsTableFrame::RePositionViews(aPresContext, rgFrame);
    }
    yOriginRG = yEndRG;
  }

  if (amountUsed >= aAmount) {
    ResizeCells(*this, aPresContext, aReflowState);
    return;
  }

  // get the first row without a style height where its row group has an unconstrianed height
  nsTableRowGroupFrame* firstUnStyledRG  = nsnull;
  nsTableRowFrame*      firstUnStyledRow = nsnull;
  for (rgX = 0; (rgX < numRowGroups) && !firstUnStyledRG; rgX++) {
    nsTableRowGroupFrame* rgFrame = GetRowGroupFrame((nsIFrame*)rowGroups.ElementAt(rgX));
    if (rgFrame && !rgFrame->HasStyleHeight()) {
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

  nsTableRowFrame* lastElligibleRow = nsnull;
  // accumulate the correct divisor. This will be the total of all unstyled rows inside 
  // unstyled row groups, unless there are none, in which case, it will be all rows
  nscoord divisor = 0;
  for (rgX = 0; rgX < numRowGroups; rgX++) {
    nsTableRowGroupFrame* rgFrame = GetRowGroupFrame((nsIFrame*)rowGroups.ElementAt(rgX));
    if (rgFrame && (!firstUnStyledRG || !rgFrame->HasStyleHeight())) {
      nsTableRowFrame* rowFrame = rgFrame->GetFirstRow();
      while (rowFrame) {
        if (!firstUnStyledRG || !rowFrame->HasStyleHeight()) {
          nsRect rowRect;
          rowFrame->GetRect(rowRect);
          divisor += rowRect.height;
          lastElligibleRow = rowFrame;
        }
        rowFrame = rowFrame->GetNextRow();
      }
    }
  }
  if (divisor <= 0) {
    NS_ASSERTION(PR_FALSE, "invlaid divisor");
    return;
  }

  // allocate the extra height to the unstyled row groups and rows
  pctBasis = aAmount - amountUsed;
  yOriginRG = borderPadding.top + cellSpacingY;
  yEndRG = yOriginRG;
  for (rgX = 0; rgX < numRowGroups; rgX++) {
    nsTableRowGroupFrame* rgFrame = GetRowGroupFrame((nsIFrame*)rowGroups.ElementAt(rgX));
    if (!rgFrame) continue; 
    nscoord amountUsedByRG = 0;
    nscoord yOriginRow = 0;
    nsRect rgRect;
    rgFrame->GetRect(rgRect);
    // see if there is an eligible row group
    if (!firstUnStyledRG || !rgFrame->HasStyleHeight()) {
      nsTableRowFrame* rowFrame = rgFrame->GetFirstRow();
      while (rowFrame) {
        nsRect rowRect;
        rowFrame->GetRect(rowRect);
        // see if there is an eligible row
        if (!firstUnStyledRow || !rowFrame->HasStyleHeight()) {
          // The amount of additional space each row gets is proportional to its height
          float percent = rowRect.height / ((float)divisor);
          // give rows their percentage, except for the last row which gets the remainder
          nscoord amountForRow = (rowFrame == lastElligibleRow) 
                                 ? aAmount - amountUsed : NSToCoordRound(((float)(pctBasis)) * percent);
          amountForRow = PR_MIN(nsTableFrame::RoundToPixel(amountForRow, p2t), aAmount - amountUsed);
          // update the row height
          nsRect newRowRect(rowRect.x, yOriginRow, rowRect.width, rowRect.height + amountForRow);
          rowFrame->SetRect(aPresContext, newRowRect);
          yOriginRow += newRowRect.height + cellSpacingY;
          yEndRG += newRowRect.height + cellSpacingY;

          amountUsed += amountForRow;
          amountUsedByRG += amountForRow;
          NS_ASSERTION((amountUsed <= aAmount), "invalid row allocation");
          //rowFrame->DidResize(aPresContext, aReflowState);        
          nsTableFrame::RePositionViews(aPresContext, rowFrame);
        }
        else {
          if (amountUsed > 0) {
            rowFrame->MoveTo(aPresContext, rowRect.x, yOriginRow);
            nsTableFrame::RePositionViews(aPresContext, rowFrame);
          }
          yOriginRow += rowRect.height + cellSpacingY;
          yEndRG += rowRect.height + cellSpacingY;
        }          
        rowFrame = rowFrame->GetNextRow();
      }
      if (amountUsed > 0) {
        rgRect.y = yOriginRG;
        rgRect.height += amountUsedByRG;
        rgFrame->SetRect(aPresContext, rgRect);
      }
      // Make sure child views are properly positioned
      // XXX what happens if childFrame is a scroll frame and this gets skipped? see also below
    }
    else if (amountUsed > 0) {
      rgFrame->MoveTo(aPresContext, 0, yOriginRG);
      // Make sure child views are properly positioned
      nsTableFrame::RePositionViews(aPresContext, rgFrame);
    }
    yOriginRG = yEndRG;
  }

  ResizeCells(*this, aPresContext, aReflowState);
}

static void
UpdateCol(nsTableFrame&           aTableFrame,
          nsTableColFrame&        aColFrame,
          const nsTableCellFrame& aCellFrame,
          nscoord                 aColMaxWidth,
          PRBool                  aColMaxGetsBigger)
{
  if (aColMaxGetsBigger) {
    // update the columns's new min width
    aColFrame.SetWidth(DES_CON, aColMaxWidth);
  }
  else {
    // determine the new max width
    PRInt32 numRows = aTableFrame.GetRowCount();
    PRInt32 colIndex = aColFrame.GetColIndex();
    PRBool originates;
    PRInt32 colSpan;
    nscoord maxWidth = 0;
    for (PRInt32 rowX = 0; rowX < numRows; rowX++) {
      nsTableCellFrame* cellFrame = aTableFrame.GetCellInfoAt(rowX, colIndex, &originates, &colSpan);
      if (cellFrame && originates && (1 == colSpan)) {
        maxWidth = PR_MAX(maxWidth, cellFrame->GetMaximumWidth());
      }
    }
    // update the columns's new max width
    aColFrame.SetWidth(DES_CON, maxWidth);
  }
}

PRBool 
nsTableFrame::IsPctHeight(nsIStyleContext* aStyleContext) 
{
  PRBool result = PR_FALSE;
  if (aStyleContext) {
    nsStylePosition* position = (nsStylePosition*)aStyleContext->GetStyleData(eStyleStruct_Position);
    result = (eStyleUnit_Percent == position->mHeight.GetUnit());
  }
  return result;
}

PRBool 
nsTableFrame::CellChangedWidth(const nsTableCellFrame& aCellFrame,
                               nscoord                 aPrevCellMin,
                               nscoord                 aPrevCellMax,
                               PRBool                  aCellWasDestroyed)
{
  if (NeedStrategyInit() || !IsAutoLayout()) {
    // if the strategy needs to be initialized, all of the col info will be updated later
    // fixed layout tables do not cause any rebalancing
    return PR_TRUE;
  }

  nscoord colSpan = GetEffectiveColSpan(aCellFrame);
  if (colSpan > 1) {
    // colspans are too complicated to optimize, so just bail out
    SetNeedStrategyInit(PR_TRUE);
    return PR_TRUE;
  }

  PRInt32 rowX, colIndex, numRows;
  aCellFrame.GetColIndex(colIndex);
  
  PRBool originates;

  nsTableColFrame* colFrame = GetColFrame(colIndex);
  if (!colFrame) return PR_TRUE; // should never happen

  nscoord cellMin = (aCellWasDestroyed) ? 0 : aCellFrame.GetPass1MaxElementWidth();
  nscoord cellMax = (aCellWasDestroyed) ? 0 : aCellFrame.GetMaximumWidth();
  nscoord colMin  = colFrame->GetWidth(MIN_CON);
  nscoord colMax  = colFrame->GetWidth(DES_CON);

  PRBool colMinGetsBigger  = (cellMin > colMin);
  PRBool colMinGetsSmaller = (cellMin < colMin) && (colMin == aPrevCellMin);

  if (colMinGetsBigger || colMinGetsSmaller) {
    if (ColIsSpannedInto(colIndex)) {
      // bail out if a colspan is involved
      SetNeedStrategyInit(PR_TRUE);
      return PR_TRUE;
    }
    if (colMinGetsBigger) {
      // update the columns's min width
      colFrame->SetWidth(MIN_CON, cellMin);
    }
    else if (colMinGetsSmaller) {
      // determine the new min width
      numRows = GetRowCount();
      nscoord minWidth = 0;
      for (rowX = 0; rowX < numRows; rowX++) {
        nsTableCellFrame* cellFrame = GetCellInfoAt(rowX, colIndex, &originates, &colSpan);
        if (cellFrame && originates && (1 == colSpan)) {
          minWidth = PR_MAX(minWidth, cellFrame->GetPass1MaxElementWidth());
        }
      }
      // update the columns's new min width
      colFrame->SetWidth(MIN_CON, minWidth);
    }
    // we should rebalance in case the min width determines the column width
    SetNeedStrategyBalance(PR_TRUE);
  }

  PRBool colMaxGetsBigger  = (cellMax > colMax);
  PRBool colMaxGetsSmaller = (cellMax < colMax) && (colMax == aPrevCellMax);

  if (colMaxGetsBigger || colMaxGetsSmaller) {
    if (ColIsSpannedInto(colIndex)) {
      // bail out if a colspan is involved
      SetNeedStrategyInit(PR_TRUE);
      return PR_TRUE;
    }
    // see if the max width will be not be overshadowed by a pct, fix, or proportional width
    if ((colFrame->GetWidth(PCT) <= 0) && (colFrame->GetWidth(FIX) <= 0) &&
        (colFrame->GetWidth(MIN_PRO) <= 0)) {
      // see if the doesn't have a pct width
      const nsStylePosition* cellPosition;
      aCellFrame.GetStyleData(eStyleStruct_Position, (const nsStyleStruct *&)cellPosition);
      // see if there isn't a pct width on the cell
      PRBool havePct = PR_FALSE;
      if (eStyleUnit_Percent == cellPosition->mWidth.GetUnit()) {
        float percent = cellPosition->mWidth.GetPercentValue();
        if (percent > 0.0f) {
          havePct = PR_TRUE;
        }
      }
      if (!havePct) {
        // see if there isn't a fix width on the cell
        PRBool haveFix = PR_FALSE;
        if (eStyleUnit_Coord == cellPosition->mWidth.GetUnit()) {
          nscoord coordValue = cellPosition->mWidth.GetCoordValue();
          if (coordValue > 0) { 
            haveFix = PR_TRUE;
          }
        }
        if (!haveFix) {
          // see if there isn't a prop width on the cell
          PRBool haveProp = PR_FALSE;
          if (eStyleUnit_Proportional == cellPosition->mWidth.GetUnit()) {
            nscoord intValue = cellPosition->mWidth.GetIntValue();
            if (intValue > 0) { 
              haveProp = PR_TRUE;
            }
          }
          if (!haveProp) {
            UpdateCol(*this, *colFrame, aCellFrame, cellMax, colMaxGetsBigger);
            // we should rebalance in case the max width determines the column width
            SetNeedStrategyBalance(PR_TRUE);
          }
        }
      }
    }
    else {
      UpdateCol(*this, *colFrame, aCellFrame, cellMax, colMaxGetsBigger);
    }
  }
  return PR_FALSE;
}

void nsTableFrame::SetNeedStrategyBalance(PRBool aValue)
{
  nsTableFrame* firstInFlow = (nsTableFrame *)GetFirstInFlow();
  NS_ASSERTION(firstInFlow, "illegal state -- no first in flow");
  firstInFlow->mBits.mNeedStrategyBalance = aValue;
}

PRBool nsTableFrame::NeedStrategyBalance() const
{
  nsTableFrame* firstInFlow = (nsTableFrame *)GetFirstInFlow();
  NS_ASSERTION(firstInFlow, "illegal state -- no first in flow");
  return (PRBool)firstInFlow->mBits.mNeedStrategyBalance;
}

void nsTableFrame::SetNeedStrategyInit(PRBool aValue)
{
  nsTableFrame* firstInFlow = (nsTableFrame *)GetFirstInFlow();
  NS_ASSERTION(firstInFlow, "illegal state -- no first in flow");
  firstInFlow->mBits.mNeedStrategyInit = aValue;
}

PRBool nsTableFrame::NeedStrategyInit() const
{
  nsTableFrame* firstInFlow = (nsTableFrame *)GetFirstInFlow();
  NS_ASSERTION(firstInFlow, "illegal state -- no first in flow");
  return (PRBool)firstInFlow->mBits.mNeedStrategyInit;
}

void nsTableFrame::SetResizeReflow(PRBool aValue)
{
  nsTableFrame* firstInFlow = (nsTableFrame *)GetFirstInFlow();
  NS_ASSERTION(firstInFlow, "illegal state -- no first in flow");
  firstInFlow->mBits.mDidResizeReflow = aValue;
}

PRBool nsTableFrame::DidResizeReflow() const
{
  nsTableFrame* firstInFlow = (nsTableFrame *)GetFirstInFlow();
  NS_ASSERTION(firstInFlow, "illegal state -- no first in flow");
  return (PRBool)firstInFlow->mBits.mDidResizeReflow;
}

PRInt32 nsTableFrame::GetColumnWidth(PRInt32 aColIndex)
{
  nsTableFrame * firstInFlow = (nsTableFrame *)GetFirstInFlow();
  NS_ASSERTION(firstInFlow, "illegal state -- no first in flow");
  PRInt32 result = 0;
  if (this == firstInFlow) {
    nsTableColFrame* colFrame = GetColFrame(aColIndex);
    if (colFrame) {
      result = colFrame->GetWidth(FINAL);
    }
  }
  else {
    result = firstInFlow->GetColumnWidth(aColIndex);
  }

  return result;
}

void nsTableFrame::SetColumnWidth(PRInt32 aColIndex, nscoord aWidth)
{
  nsTableFrame* firstInFlow = (nsTableFrame *)GetFirstInFlow();
  NS_ASSERTION(firstInFlow, "illegal state -- no first in flow");

  if (this == firstInFlow) {
    nsTableColFrame* colFrame = GetColFrame(aColIndex);
    if (colFrame) {
      colFrame->SetWidth(FINAL, aWidth);
    }
    else {
      NS_ASSERTION(PR_FALSE, "null col frame");
    }
  }
  else {
    firstInFlow->SetColumnWidth(aColIndex, aWidth);
  }
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
              const nsStylePadding& aPaddingData, 
              nsMargin&             aPadding)
{
  nsStyleCoord styleCoord;
  aPaddingData.mPadding.GetTop(styleCoord);
  if (eStyleUnit_Percent == aPaddingData.mPadding.GetTopUnit()) {
    aPadding.top = CalcPercentPadding(aBasis.height, styleCoord);
  }
  else if (eStyleUnit_Coord == aPaddingData.mPadding.GetTopUnit()) {
    aPadding.top = styleCoord.GetCoordValue();
  }

  aPaddingData.mPadding.GetRight(styleCoord);
  if (eStyleUnit_Percent == aPaddingData.mPadding.GetRightUnit()) {
    aPadding.right = CalcPercentPadding(aBasis.width, styleCoord);
  }
  else if (eStyleUnit_Coord == aPaddingData.mPadding.GetTopUnit()) {
    aPadding.right = styleCoord.GetCoordValue();
  }

  aPaddingData.mPadding.GetBottom(styleCoord);
  if (eStyleUnit_Percent == aPaddingData.mPadding.GetBottomUnit()) {
    aPadding.bottom = CalcPercentPadding(aBasis.height, styleCoord);
  }
  else if (eStyleUnit_Coord == aPaddingData.mPadding.GetTopUnit()) {
    aPadding.bottom = styleCoord.GetCoordValue();
  }

  aPaddingData.mPadding.GetLeft(styleCoord);
  if (eStyleUnit_Percent == aPaddingData.mPadding.GetLeftUnit()) {
    aPadding.left = CalcPercentPadding(aBasis.width, styleCoord);
  }
  else if (eStyleUnit_Coord == aPaddingData.mPadding.GetTopUnit()) {
    aPadding.left = styleCoord.GetCoordValue();
  }
}

nsMargin
nsTableFrame::GetPadding(const nsHTMLReflowState& aReflowState,
                         const nsTableCellFrame*  aCellFrame)
{
  const nsStylePadding* paddingData;
  aCellFrame->GetStyleData(eStyleStruct_Padding,(const nsStyleStruct *&)paddingData);
  nsMargin padding(0,0,0,0);
  if (!paddingData->GetPadding(padding)) {
    const nsHTMLReflowState* parentRS = aReflowState.parentReflowState;
    while (parentRS) {
      if (parentRS->frame) {
        nsCOMPtr<nsIAtom> frameType;
        parentRS->frame->GetFrameType(getter_AddRefs(frameType));
        if (nsLayoutAtoms::tableFrame == frameType.get()) {
          nsSize basis(parentRS->mComputedWidth, parentRS->mComputedHeight);
          GetPaddingFor(basis, *paddingData, padding);
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
  const nsStylePadding* paddingData;
  aCellFrame->GetStyleData(eStyleStruct_Padding,(const nsStyleStruct *&)paddingData);
  nsMargin padding(0,0,0,0);
  if (!paddingData->GetPadding(padding)) {
    GetPaddingFor(aBasis, *paddingData, padding);
  }
  return padding;
}

// XXX: could cache this.  But be sure to check style changes if you do!
nscoord nsTableFrame::GetCellSpacingX()
{
  nscoord cellSpacing = 0;
  if (!IsBorderCollapse()) {
    const nsStyleTableBorder* tableStyle;
    GetStyleData(eStyleStruct_TableBorder, (const nsStyleStruct *&)tableStyle);
    if (tableStyle->mBorderSpacingX.GetUnit() == eStyleUnit_Coord) {
      cellSpacing = tableStyle->mBorderSpacingX.GetCoordValue();
    }
  }
  return cellSpacing;
}

// XXX: could cache this. But be sure to check style changes if you do!
nscoord nsTableFrame::GetCellSpacingY()
{
  nscoord cellSpacing = 0;
  if (!IsBorderCollapse()) {
    const nsStyleTableBorder* tableStyle;
    GetStyleData(eStyleStruct_TableBorder, (const nsStyleStruct *&)tableStyle);
    if (tableStyle->mBorderSpacingY.GetUnit() == eStyleUnit_Coord) {
      cellSpacing = tableStyle->mBorderSpacingY.GetCoordValue();
    }
  }
  return cellSpacing;
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

NS_METHOD 
nsTableFrame::GetTableFrame(nsIFrame*      aSourceFrame, 
                            nsTableFrame*& aTableFrame)
{
  nsresult rv = NS_ERROR_UNEXPECTED;  // the value returned
  aTableFrame = nsnull;               // initialize out-param
  nsIFrame* parentFrame = nsnull;
  if (aSourceFrame) {
    // "result" is the result of intermediate calls, not the result we return from this method
    nsresult result = aSourceFrame->GetParent((nsIFrame **)&parentFrame); 
    while ((NS_OK == result) && parentFrame) {
      nsCOMPtr<nsIAtom> frameType;
      parentFrame->GetFrameType(getter_AddRefs(frameType));
      if (nsLayoutAtoms::tableFrame == frameType.get()) {
        aTableFrame = (nsTableFrame*)parentFrame;
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

PRBool 
nsTableFrame::IsAutoWidth(PRBool* aIsPctWidth)
{
  return nsTableOuterFrame::IsAutoWidth(*this, aIsPctWidth);
}

PRBool 
nsTableFrame::IsAutoHeight()
{
  PRBool isAuto = PR_TRUE;  // the default
  nsCOMPtr<nsIStyleContext> styleContext;
  GetStyleContext(getter_AddRefs(styleContext)); 

  nsStylePosition* position = (nsStylePosition*)styleContext->GetStyleData(eStyleStruct_Position);

  switch (position->mHeight.GetUnit()) {
    case eStyleUnit_Auto:         // specified auto width
    case eStyleUnit_Proportional: // illegal for table, so ignored
      break;
    case eStyleUnit_Inherit:
      // get width of parent and see if it is a specified value or not
      // XXX for now, just return true
      break;
    case eStyleUnit_Coord:
      isAuto = PR_FALSE;
      break;
    case eStyleUnit_Percent:
      if (position->mHeight.GetPercentValue() > 0.0f) {
        isAuto = PR_FALSE;
      }
      break;
    default:
      break;
  }

  return isAuto; 
}

nscoord 
nsTableFrame::CalcBorderBoxWidth(nsIPresContext*          aPresContext,
                                 const nsHTMLReflowState& aState)
{
  nscoord width = aState.mComputedWidth;

  if (eStyleUnit_Auto == aState.mStylePosition->mWidth.GetUnit()) {
    if (0 == width) {
      width = aState.availableWidth;
    }
    if (NS_UNCONSTRAINEDSIZE != aState.availableWidth) {
      width = aState.availableWidth;
    }
  }
  else if (width != NS_UNCONSTRAINEDSIZE) {
    nsMargin borderPadding = GetContentAreaOffset(*aPresContext, &aState);
    width += borderPadding.left + borderPadding.right;
  }
  width = PR_MAX(width, 0);

  if (NS_UNCONSTRAINEDSIZE != width) {
    float p2t;
    aPresContext->GetPixelsToTwips(&p2t);
    width = RoundToPixel(width, p2t);
  }

  return width;
}

nscoord 
nsTableFrame::CalcBorderBoxHeight(nsIPresContext*          aPresContext,
                                  const nsHTMLReflowState& aState)
{
  nscoord height = aState.mComputedHeight;
  if (NS_AUTOHEIGHT != height) {
    nsMargin borderPadding = GetContentAreaOffset(*aPresContext, &aState);
    height += borderPadding.top + borderPadding.bottom;
  }
  height = PR_MAX(0, height);

  return height;
}

nscoord 
nsTableFrame::GetMinCaptionWidth()
{
  nsIFrame *outerTableFrame=nsnull;
  GetParent(&outerTableFrame);
  return (((nsTableOuterFrame *)outerTableFrame)->GetMinCaptionWidth());
}

PRBool 
nsTableFrame::IsAutoLayout()
{
  const nsStyleTable* tableStyle;
  GetStyleData(eStyleStruct_Table, (const nsStyleStruct *&)tableStyle);
  if (NS_STYLE_TABLE_LAYOUT_FIXED == tableStyle->mLayoutStrategy) {
    const nsStylePosition* position;
    GetStyleData(eStyleStruct_Position, (const nsStyleStruct *&)position);
    // a fixed-layout table must have a width
    if (eStyleUnit_Auto != position->mWidth.GetUnit()) {
      return PR_FALSE;
    }
  }
  return PR_TRUE;
}

#ifdef DEBUG
NS_IMETHODIMP
nsTableFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("Table"), aResult);
}
#endif


void 
nsTableFrame::CalcMinAndPreferredWidths(nsIPresContext* aPresContext,
                                        const           nsHTMLReflowState& aReflowState,
                                        PRBool          aCalcPrefWidthIfAutoWithPctCol,
                                        nscoord&        aMinWidth,
                                        nscoord&        aPrefWidth) 
{
  if (!aPresContext) ABORT0();
  aMinWidth = aPrefWidth = 0;

  nscoord spacingX = GetCellSpacingX();
  PRInt32 numCols = GetColCount();

  for (PRInt32 colX = 0; colX < numCols; colX++) { 
    nsTableColFrame* colFrame = GetColFrame(colX);
    if (!colFrame) continue;
    aMinWidth += PR_MAX(colFrame->GetMinWidth(), colFrame->GetWidth(MIN_ADJ));
    nscoord width = colFrame->GetFixWidth();
    if (width <= 0) {
      width = colFrame->GetDesWidth();
    }
    aPrefWidth += width;
    if (GetNumCellsOriginatingInCol(colX) > 0) {
      aMinWidth  += spacingX;
      aPrefWidth += spacingX;
    }
  }
  // if it is not a degenerate table, add the last spacing on the right and the borderPadding
  if (numCols > 0) {
    nsMargin childAreaOffset = GetChildAreaOffset(*aPresContext, &aReflowState);
    nscoord extra = spacingX + childAreaOffset.left + childAreaOffset.right;
    aMinWidth  += extra;
    aPrefWidth += extra;
  }
  aPrefWidth = PR_MAX(aMinWidth, aPrefWidth);

  PRBool isPctWidth = PR_FALSE;
  if (IsAutoWidth(&isPctWidth)) {
    if (HasPctCol() && aCalcPrefWidthIfAutoWithPctCol && 
        (NS_UNCONSTRAINEDSIZE != aReflowState.availableWidth)) {
      // for an auto table with a pct cell, use the strategy's CalcPctAdjTableWidth
      nscoord availWidth = CalcBorderBoxWidth(aPresContext, aReflowState);
      availWidth = PR_MIN(availWidth, aReflowState.availableWidth);
      if (mTableLayoutStrategy && IsAutoLayout()) {
        float p2t;
        aPresContext->GetPixelsToTwips(&p2t);
        aPrefWidth = mTableLayoutStrategy->CalcPctAdjTableWidth(*aPresContext, aReflowState, availWidth, p2t);
      }
    }
  }
  else { // a specified fix width becomes the min or preferred width
    nscoord compWidth = aReflowState.mComputedWidth;
    if ((NS_UNCONSTRAINEDSIZE != compWidth) && (0 != compWidth) && !isPctWidth) {
      nsMargin contentOffset = GetContentAreaOffset(*aPresContext, &aReflowState);
      compWidth += contentOffset.left + contentOffset.right;
      aMinWidth = PR_MAX(aMinWidth, compWidth);
      aPrefWidth = PR_MAX(aMinWidth, compWidth);
    }
  }
  if (0 == numCols) { // degenerate case
    aMinWidth = aPrefWidth = 0;
  }
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
  while (childFrame && (childFrame != aPriorChildFrame)) {
    childFrame->GetFrameType(&frameType);
    if (aChildType == frameType) {
      lastMatchingFrame = childFrame;
    }
    NS_IF_RELEASE(frameType);
    childFrame->GetNextSibling(&childFrame);
  }
  return (nsTableCellFrame*)lastMatchingFrame;
}

#ifdef DEBUG
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
          if (IS_TABLE_CELL(cellType)) {
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

void 
nsTableFrame::Dump(nsIPresContext* aPresContext,
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
    printf("%d ", GetColumnWidth(colX));
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
    cellMap->Dump();
  }
  printf(" ***END TABLE DUMP*** \n");
}
#endif

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
      const nsStyleVisibility* vis;
      table->GetStyleData(eStyleStruct_Visibility, (const nsStyleStruct*&)vis);
      mLeftToRight = (NS_STYLE_DIRECTION_LTR == vis->mDirection);
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

  // If these aren't at least 1, we have a cellmap error
  if (aActualRowSpan == 0 || aActualColSpan == 0)
    return NS_ERROR_FAILURE;

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
  if (cellMap) 
    return cellMap->GetNumCellsOriginatingInCol(aColIndex);
  else
    return 0;
}

PRInt32 nsTableFrame::GetNumCellsOriginatingInRow(PRInt32 aRowIndex) const
{
  nsTableCellMap* cellMap = GetCellMap();
  if (cellMap) 
    return cellMap->GetNumCellsOriginatingInRow(aRowIndex);
  else
    return 0;
}

static void
CheckFixDamageArea(PRInt32 aNumRows,
                   PRInt32 aNumCols,
                   nsRect& aDamageArea)
{
  if (((aDamageArea.XMost() > aNumCols) && (aDamageArea.width  != 1) & (aNumCols != 0)) || 
      ((aDamageArea.YMost() > aNumRows) && (aDamageArea.height != 1) & (aNumRows != 0))) {
    // the damage area was set incorrectly, just be safe and make it the entire table
    NS_ASSERTION(PR_FALSE, "invalid BC damage area");
    aDamageArea.x      = 0;
    aDamageArea.y      = 0;
    aDamageArea.width  = aNumCols;
    aDamageArea.height = aNumRows;
  }
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

void 
nsTableFrame::SetBCDamageArea(nsIPresContext& aPresContext,
                              const nsRect&   aValue)
{
  nsRect newRect(aValue);
  newRect.width  = PR_MAX(1, newRect.width);
  newRect.height = PR_MAX(1, newRect.height);

  if (!IsBorderCollapse()) {
    NS_ASSERTION(PR_FALSE, "invalid call - not border collapse model");
    return;
  }
  SetNeedToCalcBCBorders(PR_TRUE);
  // Get the property 
  BCPropertyData* value = (BCPropertyData*)nsTableFrame::GetProperty(&aPresContext, this, nsLayoutAtoms::tableBCProperty, PR_TRUE);
  if (value) {
    // for now just construct a union of the new and old damage areas
    value->mDamageArea.UnionRect(value->mDamageArea, newRect);
    CheckFixDamageArea(GetRowCount(), GetColCount(), value->mDamageArea);
  }
}
/*****************************************************************
 *  BCMapCellIterator
 ****************************************************************/
struct BCMapCellInfo 
{
  BCMapCellInfo();
  void Reset();

  CellData*             cellData;
  nsCellMap*            cellMap;

  nsTableRowGroupFrame* rg;

  nsTableRowFrame*      topRow;
  nsTableRowFrame*      bottomRow;

  nsTableColGroupFrame* cg;
 
  nsTableColFrame*      leftCol;
  nsTableColFrame*      rightCol;

  nsBCTableCellFrame*   cell;

  PRInt32               rowIndex;
  PRInt32               rowSpan;
  PRInt32               colIndex;
  PRInt32               colSpan;

  PRPackedBool          rgTop;
  PRPackedBool          rgBottom;
  PRPackedBool          cgLeft;
  PRPackedBool          cgRight;
};

BCMapCellInfo::BCMapCellInfo()
{
  Reset();
}

void BCMapCellInfo::Reset()
{
  cellData  = nsnull;
  rg        = nsnull;
  topRow    = nsnull;
  bottomRow = nsnull;
  cg        = nsnull;
  leftCol   = nsnull;
  rightCol  = nsnull;
  cell      = nsnull;
  rowIndex = rowSpan = colIndex = colSpan = 0;
  rgTop = rgBottom = cgLeft = cgRight = PR_FALSE;
}

class BCMapCellIterator
{
public:
  BCMapCellIterator(nsTableFrame& aTableFrame,
                  const nsRect& aDamageArea);

  void First(BCMapCellInfo& aMapCellInfo);

  void Next(BCMapCellInfo& aMapCellInfo);

  void PeekRight(BCMapCellInfo& aRefInfo,
                 PRUint32     aRowIndex,
                 BCMapCellInfo& aAjaInfo);

  void PeekBottom(BCMapCellInfo& aRefInfo,
                  PRUint32     aColIndex,
                  BCMapCellInfo& aAjaInfo);

  PRBool IsNewRow() { return mIsNewRow; }

  nsTableRowFrame* GetPrevRow() const { return mPrevRow; }

  PRInt32    mRowGroupStart;
  PRInt32    mRowGroupEnd;
  PRBool     mAtEnd;
  nsCellMap* mCellMap;

private:
  void SetInfo(nsTableRowFrame* aRow,
               PRInt32          aColIndex,
               CellData*        aCellData,
               BCMapCellInfo&   aMapInfo,
               nsCellMap*       aCellMap = nsnull);

  PRBool SetNewRow(nsTableRowFrame* row = nsnull);
  PRBool SetNewRowGroup(PRBool aFindFirstDamagedRow);

  nsTableFrame&         mTableFrame;
  nsTableCellMap*       mTableCellMap;
  nsVoidArray           mRowGroups;
  nsTableRowGroupFrame* mRowGroup;
  PRInt32               mRowGroupIndex;
  PRUint32              mNumRows;
  nsTableRowFrame*      mRow;
  nsTableRowFrame*      mPrevRow;
  PRBool                mIsNewRow;
  PRInt32               mRowIndex;
  PRUint32              mNumCols;
  PRInt32               mColIndex;
  nsPoint               mAreaStart;
  nsPoint               mAreaEnd;
};

BCMapCellIterator::BCMapCellIterator(nsTableFrame& aTableFrame,
                                     const nsRect& aDamageArea)
:mTableFrame(aTableFrame)
{
  mTableCellMap  = aTableFrame.GetCellMap();

  mAreaStart.x   = aDamageArea.x;
  mAreaStart.y   = aDamageArea.y;
  mAreaEnd.y     = aDamageArea.y + aDamageArea.height - 1;
  mAreaEnd.x     = aDamageArea.x + aDamageArea.width - 1;

  mNumRows       = mTableFrame.GetRowCount();
  mRow           = nsnull;
  mRowIndex      = 0;
  mNumCols       = mTableFrame.GetColCount();
  mColIndex      = 0;
  mRowGroupIndex = -1;

  // Get the ordered row groups 
  PRUint32 numRowGroups;
  aTableFrame.OrderRowGroups(mRowGroups, numRowGroups, nsnull);

  mAtEnd = PR_TRUE; // gets reset when First() is called
}

void 
BCMapCellIterator::SetInfo(nsTableRowFrame* aRow,
                           PRInt32          aColIndex,
                           CellData*        aCellData,
                           BCMapCellInfo&   aCellInfo,
                           nsCellMap*       aCellMap)
{
  aCellInfo.cellData = aCellData;
  aCellInfo.cellMap = (aCellMap) ? aCellMap : mCellMap;
  aCellInfo.colIndex = aColIndex;

  // row frame info
  aCellInfo.rowIndex = 0;
  if (aRow) {
    aCellInfo.topRow = aRow; 
    aCellInfo.rowIndex = aRow->GetRowIndex();
  }

  // cell frame info
  aCellInfo.cell      = nsnull;
  aCellInfo.rowSpan   = 1;
  aCellInfo.colSpan  = 1;
  if (aCellData) {
    aCellInfo.cell = (nsBCTableCellFrame*)aCellData->GetCellFrame(); 
    if (aCellInfo.cell) {
      if (!aCellInfo.topRow) {
        aCellInfo.cell->GetParent((nsIFrame**)&aCellInfo.topRow); if (!aCellInfo.topRow) ABORT0();
        aCellInfo.rowIndex = aCellInfo.topRow->GetRowIndex();
      }
      aCellInfo.colSpan = mTableFrame.GetEffectiveColSpan(*aCellInfo.cell, aCellMap); 
      aCellInfo.rowSpan = mTableFrame.GetEffectiveRowSpan(*aCellInfo.cell, aCellMap);
    }
  }
  if (!aCellInfo.topRow) {
    aCellInfo.topRow = mRow;
  }

  if (1 == aCellInfo.rowSpan) {
    aCellInfo.bottomRow = aCellInfo.topRow;
  }
  else {
    aCellInfo.bottomRow = aCellInfo.topRow->GetNextRow();
    if (aCellInfo.bottomRow) {
      for (PRInt32 spanX = 2; aCellInfo.bottomRow && (spanX < aCellInfo.rowSpan); spanX++) {
        aCellInfo.bottomRow = aCellInfo.bottomRow->GetNextRow();
      }
      NS_ASSERTION(aCellInfo.bottomRow, "program error");
    }
    else {
      NS_ASSERTION(PR_FALSE, "error in cell map");
      aCellInfo.rowSpan = 1;
      aCellInfo.bottomRow = aCellInfo.topRow;
    }
  }

  // row group frame info
  PRUint32 rgStart  = mRowGroupStart;
  PRUint32 rgEnd    = mRowGroupEnd;
  nsIFrame* parentFrame = nsnull;
  aCellInfo.topRow->GetParent((nsIFrame**)&parentFrame);
  aCellInfo.rg = mTableFrame.GetRowGroupFrame(parentFrame);
  if (aCellInfo.rg != mRowGroup) {
    rgStart = aCellInfo.rg->GetStartRowIndex();
    rgEnd   = rgStart + aCellInfo.rg->GetRowCount() - 1;
  }
  PRUint32 rowIndex  = aCellInfo.topRow->GetRowIndex();
  aCellInfo.rgTop    = (rgStart == rowIndex);
  aCellInfo.rgBottom = (rgEnd == rowIndex + aCellInfo.rowSpan - 1);

  // col frame info
  aCellInfo.leftCol = mTableFrame.GetColFrame(aColIndex); if (!aCellInfo.leftCol) ABORT0();

  aCellInfo.rightCol = aCellInfo.leftCol;
  if (aCellInfo.colSpan > 1) {
    for (PRInt32 spanX = 1; spanX < aCellInfo.colSpan; spanX++) {
      nsTableColFrame* colFrame = mTableFrame.GetColFrame(aColIndex + spanX); if (!colFrame) ABORT0();
      aCellInfo.rightCol = colFrame;
    }
  }

  // col group frame info
  aCellInfo.leftCol->GetParent((nsIFrame**)&aCellInfo.cg);
  PRInt32 cgStart  = aCellInfo.cg->GetStartColumnIndex();
  PRInt32 cgEnd    = PR_MAX(0, cgStart + aCellInfo.cg->GetColCount() - 1);
  aCellInfo.cgLeft  = (cgStart == aColIndex);
  aCellInfo.cgRight = (cgEnd == aColIndex + (PRInt32)aCellInfo.colSpan - 1);
}

PRBool
BCMapCellIterator::SetNewRow(nsTableRowFrame* aRow)
{
  mAtEnd   = PR_TRUE;
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
    PRInt32 rgRowIndex = mRowIndex - mRowGroupStart;
    nsVoidArray* row = (nsVoidArray*)mCellMap->mRows.ElementAt(rgRowIndex); if (!row) ABORT1(PR_FALSE);
    PRInt32 rowSize = row->Count();
    for (mColIndex = mAreaStart.x; mColIndex <= mAreaEnd.x; mColIndex++) {
      CellData* cellData = (mColIndex < rowSize) ? (CellData*)row->ElementAt(mColIndex) : nsnull;
      if (!cellData) { // add a dead cell data
        nsRect damageArea;
        cellData = mCellMap->AppendCell(*mTableCellMap, nsnull, rgRowIndex, PR_FALSE, damageArea); if (!cellData) ABORT1(PR_FALSE);
      }
      if (cellData && (cellData->IsOrig() || cellData->IsDead())) {
        break;
      }
    }
    mIsNewRow = PR_TRUE;
    mAtEnd    = PR_FALSE;
  }
  else ABORT1(PR_FALSE);

  return !mAtEnd;
}

PRBool
BCMapCellIterator::SetNewRowGroup(PRBool aFindFirstDamagedRow)
{
  mAtEnd = PR_TRUE;
  mRowGroupIndex++;
  PRInt32 numRowGroups = mRowGroups.Count();
  for (PRInt32 rgX = mRowGroupIndex; rgX < numRowGroups; rgX++) {
    nsIFrame* frame = (nsTableRowGroupFrame*)mRowGroups.ElementAt(mRowGroupIndex); if (!frame) ABORT1(PR_FALSE);
    mRowGroup = mTableFrame.GetRowGroupFrame(frame); if (!mRowGroup) ABORT1(PR_FALSE);
    mRowGroupStart = mRowGroup->GetStartRowIndex();
    mRowGroupEnd   = mRowGroupStart + mRowGroup->GetRowCount() - 1;
    if (mRowGroupEnd >= 0) {
      mCellMap = mTableCellMap->GetMapFor(*mRowGroup); if (!mCellMap) ABORT1(PR_FALSE);
      nsTableRowFrame* firstRow = mRowGroup->GetFirstRow();
      if (aFindFirstDamagedRow) {
        if ((mAreaStart.y >= mRowGroupStart) && (mAreaStart.y <= mRowGroupEnd)) {
          // the damage area starts in the row group 
          if (aFindFirstDamagedRow) {
            // find the correct first damaged row
            PRInt32 numRows = mAreaStart.y - mRowGroupStart;
            for (PRInt32 i = 0; i < numRows; i++) {
              firstRow = firstRow->GetNextRow(); if (!frame) ABORT1(PR_FALSE);
            }
          }
        }
        else {
          mRowGroupIndex++;
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
  aMapInfo.Reset();

  SetNewRowGroup(PR_TRUE); // sets mAtEnd
  while (!mAtEnd) {
    if ((mAreaStart.y >= mRowGroupStart) && (mAreaStart.y <= mRowGroupEnd)) {
      CellData* cellData = mCellMap->GetDataAt(*mTableCellMap, mAreaStart.y - mRowGroupStart, mAreaStart.x, PR_FALSE);
      if (cellData && cellData->IsOrig()) {
        SetInfo(mRow, mAreaStart.x, cellData, aMapInfo);
      }
      else {
        NS_ASSERTION(((0 == mAreaStart.x) && (mRowGroupStart == mAreaStart.y)) , "damage area expanded incorrectly");
        mAtEnd = PR_TRUE;
      }
      break;
    }
    SetNewRowGroup(PR_TRUE); // sets mAtEnd
  } 
}

void 
BCMapCellIterator::Next(BCMapCellInfo& aMapInfo)
{
  if (mAtEnd) ABORT0();
  aMapInfo.Reset();

  mIsNewRow = PR_FALSE;
  mColIndex++;
  while ((mRowIndex <= mAreaEnd.y) && !mAtEnd) {
    for (; mColIndex <= mAreaEnd.x; mColIndex++) {
      PRInt32 rgRowIndex = mRowIndex - mRowGroupStart;
      CellData* cellData = mCellMap->GetDataAt(*mTableCellMap, rgRowIndex, mColIndex, PR_FALSE);
      if (!cellData) { // add a dead cell data
        nsRect damageArea;
        cellData = mCellMap->AppendCell(*mTableCellMap, nsnull, rgRowIndex, PR_FALSE, damageArea); if (!cellData) ABORT0();
      }
      if (cellData && (cellData->IsOrig() || cellData->IsDead())) {
        SetInfo(mRow, mColIndex, cellData, aMapInfo);
        return;
      }
    }
    if (mRowIndex >= mRowGroupEnd) {
      SetNewRowGroup(PR_FALSE); // could set mAtEnd
    }
    else {
      SetNewRow(); // could set mAtEnd
    }
  }
  mAtEnd = PR_TRUE;
}

void 
BCMapCellIterator::PeekRight(BCMapCellInfo&   aRefInfo,
                             PRUint32         aRowIndex,
                             BCMapCellInfo&   aAjaInfo)
{
  aAjaInfo.Reset();
  PRInt32 colIndex = aRefInfo.colIndex + aRefInfo.colSpan;
  PRUint32 rgRowIndex = aRowIndex - mRowGroupStart;

  CellData* cellData = mCellMap->GetDataAt(*mTableCellMap, rgRowIndex, colIndex, PR_FALSE);
  if (!cellData) { // add a dead cell data
    NS_ASSERTION(colIndex < mTableCellMap->GetColCount(), "program error");
    nsRect damageArea;
    cellData = mCellMap->AppendCell(*mTableCellMap, nsnull, rgRowIndex, PR_FALSE, damageArea); if (!cellData) ABORT0();
  }
  nsTableRowFrame* row = nsnull;
  if (cellData->IsRowSpan()) {
    rgRowIndex -= cellData->GetRowSpanOffset();
    cellData = mCellMap->GetDataAt(*mTableCellMap, rgRowIndex, colIndex, PR_FALSE); if (!cellData) ABORT0();
  }
  else {
    row = mRow;
  }
  SetInfo(row, colIndex, cellData, aAjaInfo);
}

void 
BCMapCellIterator::PeekBottom(BCMapCellInfo&   aRefInfo,
                              PRUint32         aColIndex,
                              BCMapCellInfo&   aAjaInfo)
{
  aAjaInfo.Reset();
  PRInt32 rowIndex = aRefInfo.rowIndex + aRefInfo.rowSpan;
  PRInt32 rgRowIndex = rowIndex - mRowGroupStart;
  nsTableRowGroupFrame* rg = mRowGroup;
  nsCellMap* cellMap = mCellMap;
  nsTableRowFrame* nextRow = nsnull;
  if (rowIndex > mRowGroupEnd) {
    nsIFrame* frame = (nsTableRowGroupFrame*)mRowGroups.ElementAt(mRowGroupIndex + 1); if (!frame) ABORT0();
    rg = mTableFrame.GetRowGroupFrame(frame);
    if (rg) {
      cellMap = mTableCellMap->GetMapFor(*rg); if (!cellMap) ABORT0();
      rgRowIndex = 0;
      nextRow = rg->GetFirstRow();
    }
    else return;
  }
  else {
    // get the row within the same row group
    nextRow = mRow;
    for (PRInt32 i = 0; i < aRefInfo.rowSpan; i++) {
      nextRow = nextRow->GetNextRow(); if (!nextRow) ABORT0();
    }
  }

  CellData* cellData = cellMap->GetDataAt(*mTableCellMap, rgRowIndex, aColIndex, PR_FALSE);
  if (!cellData) { // add a dead cell data
    NS_ASSERTION(rgRowIndex < cellMap->GetRowCount(), "program error");
    nsRect damageArea;
    cellData = cellMap->AppendCell(*mTableCellMap, nsnull, rgRowIndex, PR_FALSE, damageArea); if (!cellData) ABORT0();
  }
  if (cellData->IsColSpan()) {
    aColIndex -= cellData->GetColSpanOffset();
    cellData = cellMap->GetDataAt(*mTableCellMap, rowIndex, aColIndex, PR_FALSE);
  }
  SetInfo(nextRow, aColIndex, cellData, aAjaInfo, cellMap);
}

// Assign priorities to border styles. For example, styleToPriority(NS_STYLE_BORDER_STYLE_SOLID)
// will return the priority of NS_STYLE_BORDER_STYLE_SOLID. 
static PRUint8 styleToPriority[13] = { 0, 1, 2, 3, 4, 5, 6, 0, 1, 2, 7, 1, 2 }; 

// Set aStyle and aWidth given aStyleBorder and aSide
static void 
GetStyleInfo(const nsIFrame&  aFrame,
             PRUint8          aSide,
             PRUint8&         aStyle,
             nscolor&         aColor,
             PRBool           aIgnoreIfRules = PR_FALSE,
             nscoord*         aWidth = nsnull,
             float*           aTwipsToPixels = nsnull)
{
  const nsStyleBorder* styleData; 
  aFrame.GetStyleData(eStyleStruct_Border, (const nsStyleStruct *&)styleData); if (!styleData) ABORT0();

  aStyle = styleData->GetBorderStyle(aSide);

  // if the rules mask is set, set the style either to none or remove the mask
  if (NS_STYLE_BORDER_STYLE_RULES_MASK & aStyle) {
    if (aIgnoreIfRules) {
      aStyle = NS_STYLE_BORDER_STYLE_NONE;
      aColor = 0;
      if (aWidth) {
        *aWidth = 0;
      }
      return;
    }
    else {
      aStyle &= ~NS_STYLE_BORDER_STYLE_RULES_MASK;
    }
  }

  if ((NS_STYLE_BORDER_STYLE_NONE == aStyle) || (NS_STYLE_BORDER_STYLE_HIDDEN == aStyle)) {
    if (aWidth) {
      *aWidth = 0;
      aColor = 0;
    }
    return;
  }

  if ((NS_STYLE_BORDER_STYLE_INSET    == aStyle) || 
      (NS_STYLE_BORDER_STYLE_BG_INSET == aStyle)) {
    aStyle = NS_STYLE_BORDER_STYLE_GROOVE;
  }
  else if ((NS_STYLE_BORDER_STYLE_OUTSET    == aStyle) || 
           (NS_STYLE_BORDER_STYLE_BG_OUTSET == aStyle)) {
    aStyle = NS_STYLE_BORDER_STYLE_RIDGE;
  }
  PRBool transparent, foreground;
  styleData->GetBorderColor(aSide, aColor, transparent, foreground);
  if (foreground) {
    nsCOMPtr<nsIStyleContext> styleContext;
    aFrame.GetStyleContext(getter_AddRefs(styleContext)); if(!styleContext) ABORT0();
    const nsStyleColor* colorStyle = (const nsStyleColor*)styleContext->GetStyleData(eStyleStruct_Color);
    aColor = colorStyle->mColor;
  }
  if (aWidth && aTwipsToPixels) {
    *aWidth = 0;
    nscoord width;
    styleData->CalcBorderFor(&aFrame, aSide, width);
    *aWidth = NSToCoordRound(*aTwipsToPixels * (float)width);
  }
}

static PRBool
CalcDominateBorder(PRBool          aIsCorner,
                   BCBorderOwner   aOwner1,
                   PRUint8         aStyle1,
                   PRUint16        aWidth1,
                   nscolor         aColor1,
                   BCBorderOwner   aOwner2,
                   PRUint8         aStyle2,
                   PRUint16        aWidth2,
                   nscolor         aColor2,
                   BCBorderOwner&  aDomOwner,
                   PRUint8&        aDomStyle,
                   PRUint16&       aDomWidth,
                   nscolor&        aDomColor,
                   PRBool          aSecondIsHorizontal)
{
  PRBool firstDominates = PR_TRUE;
  if (NS_STYLE_BORDER_STYLE_HIDDEN == aStyle1) {
    firstDominates = (aIsCorner) ? PR_FALSE : PR_TRUE;
  }
  else if (NS_STYLE_BORDER_STYLE_HIDDEN == aStyle2) { 
    firstDominates = (aIsCorner) ? PR_TRUE : PR_FALSE;
  }
  else if (aWidth1 < aWidth2) {
    firstDominates = PR_FALSE;
  }
  else if (aWidth1 == aWidth2) {
    if (styleToPriority[aStyle1] < styleToPriority[aStyle2]) {
      firstDominates = PR_FALSE;
    }
    else if (styleToPriority[aStyle1] == styleToPriority[aStyle2]) {
      if (aOwner1 == aOwner2) {
        firstDominates = !aSecondIsHorizontal;
      }
      else if (aOwner1 < aOwner2) {
        firstDominates = PR_FALSE;
      }
    }
  }
  if (firstDominates) {
    aDomOwner = aOwner1;
    aDomStyle = aStyle1;
    aDomWidth = aWidth1;
    aDomColor = aColor1;
  }
  else {
    aDomOwner = aOwner2;
    aDomStyle = aStyle2;
    aDomWidth = aWidth2;
    aDomColor = aColor2;
  }
  return firstDominates;
}

// calc the dominate border by considering the table, row/col group, row/col, cell, 
static void 
CalcDominateBorder(const nsIFrame*  aTableFrame,
                   const nsIFrame*  aColGroupFrame,
                   const nsIFrame*  aColFrame,
                   const nsIFrame*  aRowGroupFrame,
                   const nsIFrame*  aRowFrame,
                   const nsIFrame*  aCellFrame,
                   PRBool           aIgnoreIfRules,
                   PRUint8          aSide,
                   PRBool           aAja,
                   float            aTwipsToPixels,
                   BCBorderOwner&   aDomElem,
                   PRUint8&         aDomStyle,
                   PRUint16&        aDomWidth,
                   nscolor&         aDomColor)
{
  PRUint8 style;
  nscolor color;
  aDomStyle = NS_STYLE_BORDER_STYLE_NONE;
  nscoord width;
  aDomWidth = 0;
  PRBool horizontal = (NS_SIDE_TOP == aSide) || (NS_SIDE_BOTTOM == aSide);

  // start with the table as dominate if present
  if (aTableFrame) {
    GetStyleInfo(*aTableFrame, aSide, style, color, aIgnoreIfRules, &width, &aTwipsToPixels);
    aDomStyle = style;
    aDomWidth = width;
    aDomColor = color;
    aDomElem  = eTableOwner;
    if (NS_STYLE_BORDER_STYLE_HIDDEN == style) {
      return;
    }
  }
  // see if the col row group is dominate
  if (aColGroupFrame) {
    GetStyleInfo(*aColGroupFrame, aSide, style, color, aIgnoreIfRules, &width, &aTwipsToPixels);
    if ((NS_STYLE_BORDER_STYLE_HIDDEN == style) ||
        (width > aDomWidth) || 
        ((width == aDomWidth) && 
         (styleToPriority[style] >= styleToPriority[aDomStyle]))) {
      aDomStyle = style;
      aDomWidth = width;
      aDomColor = color;
      aDomElem  = (aAja && !horizontal) ? eAjaColGroupOwner : eColGroupOwner;
      if (NS_STYLE_BORDER_STYLE_HIDDEN == style) {
        return;
      }
    }
  }
  // see if the col is dominate
  if (aColFrame) {
    GetStyleInfo(*aColFrame, aSide, style, color, aIgnoreIfRules, &width, &aTwipsToPixels);
    if ((NS_STYLE_BORDER_STYLE_HIDDEN == style) ||
        (width > aDomWidth) || 
        ((width == aDomWidth) && 
         (styleToPriority[style] >= styleToPriority[aDomStyle]))) {
      aDomStyle = style;
      aDomWidth = width;
      aDomColor = color;
      aDomElem  = (aAja && !horizontal) ? eAjaColOwner : eColOwner;
      if (NS_STYLE_BORDER_STYLE_HIDDEN == style) {
        return;
      }
    }
  }
  // see if the row row group is dominate
  if (aRowGroupFrame) {
    GetStyleInfo(*aRowGroupFrame, aSide, style, color, aIgnoreIfRules, &width, &aTwipsToPixels);
    if ((NS_STYLE_BORDER_STYLE_HIDDEN == style) ||
        (width > aDomWidth) || 
        ((width == aDomWidth) && 
         (styleToPriority[style] >= styleToPriority[aDomStyle]))) {
      aDomStyle = style;
      aDomWidth = width;
      aDomColor = color;
      aDomElem  = (aAja && horizontal) ? eAjaRowGroupOwner : eRowGroupOwner;
      if (NS_STYLE_BORDER_STYLE_HIDDEN == style) {
        return;
      }
    }
  }
  // see if the row is dominate
  if (aRowFrame) {
    GetStyleInfo(*aRowFrame, aSide, style, color, aIgnoreIfRules, &width, &aTwipsToPixels);
    if ((NS_STYLE_BORDER_STYLE_HIDDEN == style) ||
        (width > aDomWidth) || 
        ((width == aDomWidth) && 
         (styleToPriority[style] >= styleToPriority[aDomStyle]))) {
      aDomStyle = style;
      aDomWidth = width;
      aDomColor = color;
      aDomElem  = (aAja && horizontal) ? eAjaRowOwner : eRowOwner;
      if (NS_STYLE_BORDER_STYLE_HIDDEN == style) {
        return;
      }
    }
  }
  // see if the cell is dominate
  if (aCellFrame) {
    GetStyleInfo(*aCellFrame, aSide, style, color, aIgnoreIfRules, &width, &aTwipsToPixels);
    if ((NS_STYLE_BORDER_STYLE_HIDDEN == style) ||
        (width > aDomWidth) || 
        ((width == aDomWidth) && 
         (styleToPriority[style] >= styleToPriority[aDomStyle]))) {
      aDomStyle = style;
      aDomWidth = width;
      aDomColor = color;
      aDomElem  = (aAja) ? eAjaCellOwner : eCellOwner;
    }
  }
}

static PRBool 
Perpendicular(PRUint8 aSide1, 
              PRUint8 aSide2)
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
  BCCornerInfo() { ownerColor = 0; ownerWidth = subWidth = ownerSide = ownerElem = subSide = 
                   subElem = hasDashDot = numSegs = bevel = 0;
                   ownerStyle = 0xFF; subStyle = NS_STYLE_BORDER_STYLE_SOLID;  }
  void Set(PRUint8       aSide,
           BCBorderOwner aBorderOwner,
           PRUint8       aOwnerBStyle, 
           nscoord       aOwnerWidth,
           nscolor       aOwnerColor);

  void Update(PRUint8       aSide,
              BCBorderOwner aBorderOwner,
              PRUint8       aOwnerBStyle, 
              nscoord       aOwnerWidth,
              nscolor       aOwnerColor);

  nscolor   ownerColor;     // color of borderOwner
  PRUint16  ownerWidth;     // pixel width of borderOwner 
  PRUint16  subWidth;       // pixel width of the largest border intersecting the border perpendicular 
                            // to ownerSide
  PRUint32  ownerSide:2;    // side (e.g NS_SIDE_TOP, NS_SIDE_RIGHT, etc) of the border owning 
                            // the corner relative to the corner
  PRUint32  ownerElem:3;    // elem type (e.g. eTable, eGroup, etc) owning the corner
  PRUint32  ownerStyle:8;   // border style of ownerElem
  PRUint32  subSide:2;      // side of border with subWidth relative to the corner
  PRUint32  subElem:3;      // elem type (e.g. eTable, eGroup, etc) of sub owner
  PRUint32  subStyle:8;     // border style of subElem
  PRUint32  hasDashDot:1;   // does a dashed, dotted segment enter the corner, they cannot be beveled
  PRUint32  numSegs:3;      // number of segments entering corner
  PRUint32  bevel:1;        // is the corner beveled (uses the above two fields together with subWidth)
  PRUint32  unused:1;
};

void 
BCCornerInfo::Set(PRUint8       aSide,
                  BCBorderOwner aBorderOwner,
                  PRUint8       aOwnerBStyle, 
                  nscoord       aOwnerWidth,
                  nscolor       aOwnerColor)
{
  ownerElem  = aBorderOwner;
  ownerStyle = aOwnerBStyle;
  ownerWidth = aOwnerWidth;
  ownerColor = aOwnerColor;
  ownerSide  = aSide;
  hasDashDot = 0;
  numSegs    = 0;
  if (aOwnerWidth > 0) {
    numSegs++;
    hasDashDot = (NS_STYLE_BORDER_STYLE_DASHED == aOwnerBStyle) ||
                 (NS_STYLE_BORDER_STYLE_DOTTED == aOwnerBStyle);
  }
  bevel      = 0;
  subWidth   = 0;
  // the following will get set later
  subSide    = ((aSide == NS_SIDE_LEFT) || (aSide == NS_SIDE_RIGHT)) ? NS_SIDE_TOP : NS_SIDE_LEFT; 
  subElem    = eTableOwner;
  subStyle   = NS_STYLE_BORDER_STYLE_SOLID; 
}

void 
BCCornerInfo::Update(PRUint8       aSide,
                     BCBorderOwner aBorderOwner,
                     PRUint8       aOwnerBStyle, 
                     nscoord       aOwnerWidth,
                     nscolor       aOwnerColor)
{
  PRBool existingWins = PR_FALSE;
  if (0xFF == ownerStyle) { // initial value indiating that it hasn't been set yet
    Set(aSide, aBorderOwner, aOwnerBStyle, aOwnerWidth, aOwnerColor);
  }
  else {
    PRBool horizontal = (NS_SIDE_LEFT == aSide) || (NS_SIDE_RIGHT == aSide); // relative to the corner
    PRUint8 oldElem  = ownerElem;
    PRUint8 oldSide  = ownerSide;
    PRUint8 oldStyle = ownerStyle;
    nscoord oldWidth = ownerWidth;
    BCBorderOwner tempBorderOwner = (BCBorderOwner)ownerElem;
    PRUint8 tempStyle = ownerStyle;
    existingWins = 
      CalcDominateBorder(PR_TRUE, (BCBorderOwner)ownerElem, ownerStyle, ownerWidth, ownerColor, 
                         (BCBorderOwner)aBorderOwner, aOwnerBStyle, aOwnerWidth, aOwnerColor, 
                         tempBorderOwner, tempStyle, ownerWidth, ownerColor, horizontal);
    ownerElem = tempBorderOwner;
    ownerStyle = tempStyle;
    if (existingWins) { // existing corner is dominate
      if (::Perpendicular(ownerSide, aSide)) {
        // see if the new sub info replaces the old
        nscolor color;
        tempBorderOwner = (BCBorderOwner)ownerElem;
        PRUint8 tempStyle = subStyle;
        PRBool firstWins = 
          CalcDominateBorder(PR_TRUE, (BCBorderOwner)subElem, subStyle, subWidth, color, 
                             (BCBorderOwner)aBorderOwner, aOwnerBStyle, aOwnerWidth, aOwnerColor, 
                             tempBorderOwner, tempStyle, subWidth, color, horizontal);
        subElem = tempBorderOwner;
        subStyle = tempStyle;
        if (firstWins) {
          subSide = aSide; 
        }
      }
    }
    else { // input args are dominate
      ownerSide = aSide;
      if (::Perpendicular(oldSide, ownerSide)) {
        subElem  = oldElem; 
        subSide  = oldSide; 
        subStyle = oldStyle; 
        subWidth = oldWidth;
      }
    }
    if (aOwnerWidth > 0) {
      numSegs++;
      if (!hasDashDot && ((NS_STYLE_BORDER_STYLE_DASHED == aOwnerBStyle) ||
                          (NS_STYLE_BORDER_STYLE_DOTTED == aOwnerBStyle))) {
        hasDashDot = 1;
      }
    }
  
    // bevel the corner if only two perpendicular non dashed/dotted segments enter the corner
    bevel = (2 == numSegs) && (subWidth > 1) && (0 == hasDashDot);
  }
}

struct BCCorners
{
  BCCorners(PRInt32 aNumCorners,
            PRInt32 aStartIndex);

  ~BCCorners() { delete [] corners; }
  
  BCCornerInfo& operator [](PRInt32 i) const
  { NS_ASSERTION((i >= startIndex) && (i <= endIndex), "program error");
    return corners[PR_MAX(PR_MIN(i, endIndex), startIndex) - startIndex]; }

  PRInt32       startIndex;
  PRInt32       endIndex;
  BCCornerInfo* corners;
};
  
BCCorners::BCCorners(PRInt32 aNumCorners,
                     PRInt32 aStartIndex)
{
  NS_ASSERTION((aNumCorners > 0) && (aStartIndex >= 0), "program error");
  startIndex = aStartIndex;
  endIndex   = aStartIndex + aNumCorners - 1;
  corners    = new BCCornerInfo[aNumCorners]; 
}

struct BCCellBorder
{
  BCCellBorder() { Reset(); }
  void Reset(PRUint32 aIndex = 0, PRUint32 aSpan = 1);
  nscolor      color;
  PRUint16     index;   // y index, not used for vertical borders
  PRUint16     span;    // row span
  PRUint16     width;
  PRUint8      style;
};

void 
BCCellBorder::Reset(PRUint32 aIndex,
                    PRUint32 aSpan) 
{ 
  style = color = width = -1; 
  index = (PRUint16)aIndex;
  span  = (PRUint16)aSpan;
}

struct BCCellBorders
{
  BCCellBorders(PRInt32 aNumBorders,
                PRInt32 aStartIndex);

  ~BCCellBorders() { delete [] borders; }
  
  BCCellBorder& operator [](PRInt32 i) const
  { NS_ASSERTION((i >= startIndex) && (i <= endIndex), "program error");
    return borders[PR_MAX(PR_MIN(i, endIndex), startIndex) - startIndex]; }

  PRInt32       startIndex;
  PRInt32       endIndex;
  BCCellBorder* borders;
};
  
BCCellBorders::BCCellBorders(PRInt32 aNumBorders,
                             PRInt32 aStartIndex)
{
  NS_ASSERTION((aNumBorders > 0) && (aStartIndex >= 0), "program error");
  startIndex = aStartIndex;
  endIndex   = aStartIndex + aNumBorders - 1;
  borders    = new BCCellBorder[aNumBorders]; 
}

static PRBool
SetBorder(PRUint8        aOwnerBStyle, 
          PRUint16       aOwnerWidth, 
          nscolor        aOwnerColor, 
          BCCellBorder&  aBorder)
{
  PRBool changed = (aOwnerBStyle != aBorder.style) || (aOwnerWidth != aBorder.width) || 
                   (aOwnerColor != aBorder.color);
  aBorder.color        = aOwnerColor;
  aBorder.width        = aOwnerWidth;
  aBorder.style        = aOwnerBStyle;

  return changed;
}

static PRBool
SetHorBorder(PRUint8             aOwnerBStyle, 
             PRUint16            aOwnerWidth, 
             PRUint32            aOwnerColor, 
             const BCCornerInfo& aCorner,
             BCCellBorder&       aBorder)
{
  PRBool startSeg = ::SetBorder(aOwnerBStyle, aOwnerWidth, aOwnerColor, aBorder);
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
nsTableFrame::ExpandBCDamageArea(nsRect& aRect) const
{
  PRInt32 numRows = GetRowCount();
  PRInt32 numCols = GetColCount();

  PRInt32 dStartX = aRect.x;
  PRInt32 dEndX   = aRect.XMost() - 1;
  PRInt32 dStartY = aRect.y;
  PRInt32 dEndY   = aRect.YMost() - 1;

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
  PRBool haveSpanner = PR_FALSE;
  if ((dStartX > 0) || (dEndX < (numCols - 1)) || (dStartY > 0) || (dEndY < (numRows - 1))) {
    nsTableCellMap* tableCellMap = GetCellMap(); if (!tableCellMap) ABORT0();
    // Get the ordered row groups 
    PRUint32 numRowGroups;
    nsVoidArray rowGroups;
    OrderRowGroups(rowGroups, numRowGroups, nsnull);
    for (PRUint32 rgX = 0; rgX < numRowGroups; rgX++) {
      nsIFrame* kidFrame = (nsIFrame*)rowGroups.ElementAt(rgX);
      nsTableRowGroupFrame* rgFrame = GetRowGroupFrame(kidFrame); if (!rgFrame) ABORT0();
      PRInt32 rgStartY = rgFrame->GetStartRowIndex();
      PRInt32 rgEndY   = rgStartY + rgFrame->GetRowCount() - 1;
      if (dEndY < rgStartY) 
        break;
      nsCellMap* cellMap = tableCellMap->GetMapFor(*rgFrame); if (!cellMap) ABORT0();
      // check for spanners from above and below
      if ((dStartY > 0) && (dStartY >= rgStartY) && (dStartY <= rgEndY)) {
        nsVoidArray* row = (nsVoidArray*)cellMap->mRows.ElementAt(dStartY - rgStartY); if (!row) ABORT0();
        for (PRInt32 x = dStartX; x <= dEndX; x++) {
          CellData* cellData = (row->Count() > x) ? (CellData*)row->ElementAt(x) : nsnull;
          if (cellData && (cellData->IsRowSpan())) {
             haveSpanner = PR_TRUE;
             break;
          }
        }
        if (dEndY < rgEndY) {
          row = (nsVoidArray*)cellMap->mRows.ElementAt(dEndY + 1 - rgStartY); if (!row) ABORT0();
          for (PRInt32 x = dStartX; x <= dEndX; x++) {
            CellData* cellData = (row->Count() > x) ? (CellData*)row->ElementAt(x) : nsnull;
            if (cellData && (cellData->IsRowSpan())) {
              haveSpanner = PR_TRUE;
              break;
            }
          }
        }
      }
      // check for spanners on the left and right
      PRInt32 iterStartY = -1;
      PRInt32 iterEndY   = -1;
      if ((dStartY >= rgStartY) && (dStartY <= rgEndY)) {
        // the damage area starts in the row group
        iterStartY = dStartY;
        iterEndY   = PR_MIN(dEndY, rgEndY);
      }
      else if ((dEndY >= rgStartY) && (dEndY <= rgEndY)) {
        // the damage area ends in the row group
        iterStartY = rgStartY;
        iterEndY   = PR_MIN(dEndY, rgStartY);
      }
      else if ((rgStartY >= dStartY) && (rgEndY <= dEndY)) {
        // the damage area contains the row group
        iterStartY = rgStartY;
        iterEndY   = rgEndY;
      }
      if ((iterStartY >= 0) && (iterEndY >= 0)) {
        for (PRInt32 y = iterStartY; y <= iterEndY; y++) {
          nsVoidArray* row = (nsVoidArray*)cellMap->mRows.ElementAt(y - rgStartY); if (!row) ABORT0();
          CellData* cellData = (CellData*)row->ElementAt(dStartX);
          if (cellData && (cellData->IsColSpan())) {
            haveSpanner = PR_TRUE;
            break;
          }
          if (dEndX < (numCols - 1)) {
            cellData = (row->Count() > dEndX) ? (CellData*)row->ElementAt(dEndX + 1) : nsnull;
            if (cellData && (cellData->IsColSpan())) {
              haveSpanner = PR_TRUE;
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

#define MAX_TABLE_BORDER_WIDTH 256
static PRUint8
LimitBorderWidth(PRUint16 aWidth)
{
  return PR_MIN(MAX_TABLE_BORDER_WIDTH, aWidth);
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
 */

#define TOP_DAMAGED(aRowIndex)    ((aRowIndex) >= propData->mDamageArea.y) 
#define RIGHT_DAMAGED(aColIndex)  ((aColIndex) <  propData->mDamageArea.XMost()) 
#define BOTTOM_DAMAGED(aRowIndex) ((aRowIndex) <  propData->mDamageArea.YMost()) 
#define LEFT_DAMAGED(aColIndex)   ((aColIndex) >= propData->mDamageArea.x) 

// Calc the dominate border at every cell edge and corner within the current damage area
void 
nsTableFrame::CalcBCBorders(nsIPresContext& aPresContext)
{
  nsTableCellMap* tableCellMap = GetCellMap(); if (!tableCellMap) ABORT0();
  PRInt32 numRows = GetRowCount();
  PRInt32 numCols = GetColCount();

  // Get the property holding the table damage area and border widths
  BCPropertyData* propData = 
    (BCPropertyData*)nsTableFrame::GetProperty(&aPresContext, this, nsLayoutAtoms::tableBCProperty, PR_FALSE);
  if (!propData) ABORT0();

  CheckFixDamageArea(numRows, numCols, propData->mDamageArea);
  // calculate an expanded damage area 
  nsRect damageArea(propData->mDamageArea);
  ExpandBCDamageArea(damageArea);

  // segments that are on the table border edges need to be initialized only once
  PRBool tableBorderReset[4];
  for (PRUint32 sideX = NS_SIDE_TOP; sideX <= NS_SIDE_LEFT; sideX++) {
    tableBorderReset[sideX] = PR_FALSE;
  }
  GET_TWIPS_TO_PIXELS(&aPresContext, t2p);

  // vertical borders indexed in x-direction (cols)
  BCCellBorders lastVerBorders(damageArea.width + 1, damageArea.x); if (!lastVerBorders.borders) ABORT0();
  BCCellBorder  lastTopBorder, lastBottomBorder;
  // horizontal borders indexed in x-direction (cols)
  BCCellBorders lastBottomBorders(damageArea.width + 1, damageArea.x); if (!lastBottomBorders.borders) ABORT0();
  PRBool startSeg;

  BCMapCellInfo  info, ajaInfo;
  BCBorderOwner  owner, ajaOwner;
  nscolor   ownerColor, ajaColor;
  PRUint8   ownerBStyle, ajaBStyle;
  PRUint16  ownerWidth, ajaWidth;
  PRInt32   cellEndRowIndex = -1;
  PRInt32   cellEndColIndex = -1;
  nscoord   smallHalf, largeHalf;
  BCCorners topCorners(damageArea.width + 1, damageArea.x); if (!topCorners.corners) ABORT0();
  BCCorners bottomCorners(damageArea.width + 1, damageArea.x); if (!bottomCorners.corners) ABORT0();

  BCMapCellIterator iter(*this, damageArea);
  for (iter.First(info); !iter.mAtEnd; iter.Next(info)) {

    cellEndRowIndex = info.rowIndex + info.rowSpan - 1;
    cellEndColIndex = info.colIndex + info.colSpan - 1;
    PRBool isBottomRight = (info.rowIndex == (numRows - 1)) && (info.colIndex == (numCols - 1));

    PRBool bottomRowSpan = PR_FALSE;
    // see if lastTopBorder, lastBottomBorder need to be reset
    if (iter.IsNewRow()) { 
      lastTopBorder.Reset(info.rowIndex, info.rowSpan);
      lastBottomBorder.Reset(cellEndRowIndex + 1, info.rowSpan);
    }
    else if (info.colIndex > damageArea.x) {
      BCCellBorder& prevBorder = lastBottomBorders[info.colIndex - 1];
      if (info.rowIndex > prevBorder.index - prevBorder.span) { 
        // the top border's left edge butts against the middle of a rowspan
        lastTopBorder.Reset(info.rowIndex, info.rowSpan);
      }
      if (prevBorder.index > (cellEndRowIndex + 1)) { 
        // the bottom border's left edge butts against the middle of a rowspan
        lastBottomBorder.Reset(cellEndRowIndex + 1, info.rowSpan);
        bottomRowSpan = PR_TRUE;
      }
    }

    // find the dominate border considernig the cell's top border and the table, row group, row   
    // if the border is at the top of the table, otherwise it was processed in a previous row
    if (0 == info.rowIndex) {
      if (!tableBorderReset[NS_SIDE_TOP]) {
        propData->mTopBorderWidth = 0;
        tableBorderReset[NS_SIDE_TOP] = PR_TRUE;
      }
      for (PRInt32 colX = info.colIndex; colX <= cellEndColIndex; colX++) {
        nsIFrame* colFrame = GetColFrame(colX); if (!colFrame) ABORT0();
        nsIFrame* cgFrame;
        colFrame->GetParent(&cgFrame); if (!cgFrame) ABORT0();
        CalcDominateBorder(this, cgFrame, colFrame, info.rg, info.topRow, info.cell, PR_TRUE, NS_SIDE_TOP, 
                           PR_FALSE, t2p, owner, ownerBStyle, ownerWidth, ownerColor);
        // update/store the top left & top right corners of the seg 
        BCCornerInfo& tlCorner = topCorners[colX]; // top left
        if (0 == colX) {
          tlCorner.Set(NS_SIDE_RIGHT, owner, ownerBStyle, ownerWidth, ownerColor);    
        }
        else {
          tlCorner.Update(NS_SIDE_RIGHT, owner, ownerBStyle, ownerWidth, ownerColor); 
          tableCellMap->SetBCBorderCorner(eTopLeft, *info.cellMap, 0, 0, colX, 
                                          tlCorner.ownerSide, tlCorner.subWidth, tlCorner.bevel);
        }
        topCorners[colX + 1].Set(NS_SIDE_LEFT, owner, ownerBStyle, ownerWidth, ownerColor); // top right
        // update lastTopBorder and see if a new segment starts
        startSeg = SetHorBorder(ownerBStyle, ownerWidth, ownerColor, tlCorner, lastTopBorder);
        // store the border segment in the cell map 
        tableCellMap->SetBCBorderEdge(NS_SIDE_TOP, *info.cellMap, 0, 0, colX, 
                                      1, owner, ownerWidth, startSeg);
        // update the affected borders of the cell, row, and table
        DivideBCBorderSize(ownerWidth, smallHalf, largeHalf);
        if (info.cell) {
          info.cell->SetBorderWidth(NS_SIDE_TOP, PR_MAX(smallHalf, info.cell->GetBorderWidth(NS_SIDE_TOP)));
        }
        if (info.topRow) {
          info.topRow->SetTopBCBorderWidth(PR_MAX(smallHalf, info.topRow->GetTopBCBorderWidth()));
        }
        propData->mTopBorderWidth = LimitBorderWidth(PR_MAX(propData->mTopBorderWidth, (PRUint8)ownerWidth));
      }
    }
    else {
      // see if the top border needs to be the start of a segment due to a vertical border owning the corner
      if (info.colIndex > 0) {
        BCData& data = ((BCCellData*)info.cellData)->mData;
        if (!data.IsTopStart()) {
          PRUint8 cornerSide;
          PRPackedBool bevel;
          data.GetCorner(cornerSide, bevel);
          if ((NS_SIDE_TOP == cornerSide) || (NS_SIDE_BOTTOM == cornerSide)) {
            data.SetTopStart(PR_TRUE);
          }
        }
      }  
    }

    // find the dominate border considernig the cell's left border and the table, col group, col  
    // if the border is at the left of the table, otherwise it was processed in a previous col
    if (0 == info.colIndex) {
      if (!tableBorderReset[NS_SIDE_LEFT]) {
        propData->mLeftBorderWidth = 0;
        tableBorderReset[NS_SIDE_LEFT] = PR_TRUE;
      }
      nsTableRowFrame* rowFrame = nsnull;
      for (PRInt32 rowX = info.rowIndex; rowX <= cellEndRowIndex; rowX++) {
        rowFrame = (rowX == info.rowIndex) ? info.topRow : rowFrame->GetNextRow();
        CalcDominateBorder(this, info.cg, info.leftCol, info.rg, rowFrame, info.cell, PR_TRUE, NS_SIDE_LEFT, 
                           PR_FALSE, t2p, owner, ownerBStyle, ownerWidth, ownerColor);
        BCCornerInfo& tlCorner = (0 == rowX) ? topCorners[0] : bottomCorners[0]; // top left
        tlCorner.Update(NS_SIDE_BOTTOM, owner, ownerBStyle, ownerWidth, ownerColor); 
        tableCellMap->SetBCBorderCorner(eTopLeft, *info.cellMap, iter.mRowGroupStart, rowX, 
                                        0, tlCorner.ownerSide, tlCorner.subWidth, tlCorner.bevel);
        bottomCorners[0].Set(NS_SIDE_TOP, owner, ownerBStyle, ownerWidth, ownerColor); // bottom left             
        // update lastVerBordersBorder and see if a new segment starts
        startSeg = SetBorder(ownerBStyle, ownerWidth, ownerColor, lastVerBorders[0]);
        // store the border segment in the cell map 
        tableCellMap->SetBCBorderEdge(NS_SIDE_LEFT, *info.cellMap, iter.mRowGroupStart, rowX, 
                                      info.colIndex, 1, owner, ownerWidth, startSeg);
        // update the left border of the cell, col and table
        DivideBCBorderSize(ownerWidth, smallHalf, largeHalf);
        if (info.cell) {
          info.cell->SetBorderWidth(NS_SIDE_LEFT, PR_MAX(smallHalf, info.cell->GetBorderWidth(NS_SIDE_LEFT)));
        }
        if (info.leftCol) {
          info.leftCol->SetLeftBorderWidth(PR_MAX(smallHalf, info.leftCol->GetLeftBorderWidth()));
        }
        propData->mLeftBorderWidth = LimitBorderWidth(PR_MAX(propData->mLeftBorderWidth, ownerWidth));
      }
    }

    // find the dominate border considernig the cell's right border, adjacent cells and the table, row group, row
    if (numCols == cellEndColIndex + 1) { // touches right edge of table
      if (!tableBorderReset[NS_SIDE_RIGHT]) {
        propData->mRightBorderWidth = 0;
        tableBorderReset[NS_SIDE_RIGHT] = PR_TRUE;
      }
      nsTableRowFrame* rowFrame = nsnull;
      for (PRInt32 rowX = info.rowIndex; rowX <= cellEndRowIndex; rowX++) {
        rowFrame = (rowX == info.rowIndex) ? info.topRow : rowFrame->GetNextRow();
        CalcDominateBorder(this, info.cg, info.rightCol, info.rg, rowFrame, info.cell, PR_TRUE, NS_SIDE_RIGHT, 
                           PR_TRUE, t2p, owner, ownerBStyle, ownerWidth, ownerColor);
        // update/store the top right & bottom right corners 
        BCCornerInfo& trCorner = (0 == rowX) ? topCorners[cellEndColIndex + 1] : bottomCorners[cellEndColIndex + 1]; 
        trCorner.Update(NS_SIDE_BOTTOM, owner, ownerBStyle, ownerWidth, ownerColor);   // top right
        tableCellMap->SetBCBorderCorner(eTopRight, *info.cellMap, iter.mRowGroupStart, rowX, 
                                        cellEndColIndex, trCorner.ownerSide, trCorner.subWidth, trCorner.bevel);
        BCCornerInfo& brCorner = bottomCorners[cellEndColIndex + 1]; 
        brCorner.Set(NS_SIDE_TOP, owner, ownerBStyle, ownerWidth, ownerColor); // bottom right
        tableCellMap->SetBCBorderCorner(eBottomRight, *info.cellMap, iter.mRowGroupStart, rowX, 
                                        cellEndColIndex, brCorner.ownerSide, brCorner.subWidth, brCorner.bevel);
        // update lastVerBorders and see if a new segment starts
        startSeg = SetBorder(ownerBStyle, ownerWidth, ownerColor, lastVerBorders[cellEndColIndex + 1]);
        // store the border segment in the cell map and update cellBorders
        tableCellMap->SetBCBorderEdge(NS_SIDE_RIGHT, *info.cellMap, iter.mRowGroupStart, rowX, 
                                      cellEndColIndex, 1, owner, ownerWidth, startSeg);
        // update the affected borders of the cell, col, and table
        DivideBCBorderSize(ownerWidth, smallHalf, largeHalf);
        if (info.cell) {
          info.cell->SetBorderWidth(NS_SIDE_RIGHT, PR_MAX(largeHalf, info.cell->GetBorderWidth(NS_SIDE_RIGHT)));
        }
        if (info.rightCol) {
          info.rightCol->SetRightBorderWidth(PR_MAX(largeHalf, info.rightCol->GetRightBorderWidth()));
        }
        propData->mRightBorderWidth = LimitBorderWidth(PR_MAX(propData->mRightBorderWidth, ownerWidth));
      }
    }
    else {
      PRInt32 segLength = 0;
      BCMapCellInfo priorAjaInfo;
      for (PRInt32 rowX = info.rowIndex; rowX <= cellEndRowIndex; rowX += segLength) {
        iter.PeekRight(info, rowX, ajaInfo);
        const nsIFrame* cg = (info.cgRight) ? info.cg : nsnull;
        CalcDominateBorder(nsnull, cg, info.rightCol, nsnull, nsnull, info.cell, PR_FALSE, NS_SIDE_RIGHT, 
                           PR_TRUE, t2p, owner, ownerBStyle, ownerWidth, ownerColor);
        cg = (ajaInfo.cgLeft) ? ajaInfo.cg : nsnull;
        CalcDominateBorder(nsnull, cg, ajaInfo.leftCol, nsnull, nsnull, ajaInfo.cell, PR_FALSE, NS_SIDE_LEFT, 
                           PR_FALSE, t2p, ajaOwner, ajaBStyle, ajaWidth, ajaColor);
        CalcDominateBorder(PR_FALSE, owner, ownerBStyle, ownerWidth, ownerColor, ajaOwner, ajaBStyle, 
                           ajaWidth, ajaColor, owner, ownerBStyle, ownerWidth, ownerColor, PR_FALSE);
        segLength = PR_MAX(1, ajaInfo.rowIndex + ajaInfo.rowSpan - rowX);
        segLength = PR_MIN(segLength, info.rowIndex + info.rowSpan - rowX);

        // update lastVerBorders and see if a new segment starts
        startSeg = SetBorder(ownerBStyle, ownerWidth, ownerColor, lastVerBorders[cellEndColIndex + 1]);
        // store the border segment in the cell map and update cellBorders
        if (RIGHT_DAMAGED(cellEndColIndex) && TOP_DAMAGED(rowX) && BOTTOM_DAMAGED(rowX)) {
          tableCellMap->SetBCBorderEdge(NS_SIDE_RIGHT, *info.cellMap, iter.mRowGroupStart, rowX, 
                                        cellEndColIndex, segLength, owner, ownerWidth, startSeg);
          // update the borders of the cells and cols affected 
          DivideBCBorderSize(ownerWidth, smallHalf, largeHalf);
          if (info.cell) {
            info.cell->SetBorderWidth(NS_SIDE_RIGHT, PR_MAX(largeHalf, info.cell->GetBorderWidth(NS_SIDE_RIGHT)));
          }
          if (info.rightCol) {
            info.rightCol->SetRightBorderWidth(PR_MAX(largeHalf, info.rightCol->GetRightBorderWidth()));
          }
          if (ajaInfo.cell) {
            ajaInfo.cell->SetBorderWidth(NS_SIDE_LEFT, PR_MAX(smallHalf, ajaInfo.cell->GetBorderWidth(NS_SIDE_LEFT)));
          }
          if (ajaInfo.leftCol) {
            ajaInfo.leftCol->SetLeftBorderWidth(PR_MAX(smallHalf, ajaInfo.leftCol->GetLeftBorderWidth()));
          }
        }
        // update the top right corner
        PRBool hitsSpanOnRight = (rowX > ajaInfo.rowIndex) && (rowX < ajaInfo.rowIndex + ajaInfo.rowSpan);
        BCCornerInfo* trCorner = ((0 == rowX) || hitsSpanOnRight) 
                                 ? &topCorners[cellEndColIndex + 1] : &bottomCorners[cellEndColIndex + 1]; 
        trCorner->Update(NS_SIDE_BOTTOM, owner, ownerBStyle, ownerWidth, ownerColor);   
        // if this is not the first time through, consider the segment to the right
        if (rowX != info.rowIndex) {
          const nsIFrame* rg = (priorAjaInfo.rgBottom) ? priorAjaInfo.rg : nsnull;
          CalcDominateBorder(nsnull, nsnull, nsnull, rg, priorAjaInfo.bottomRow, priorAjaInfo.cell, PR_FALSE, 
                             NS_SIDE_BOTTOM, PR_TRUE, t2p, owner, ownerBStyle, ownerWidth, ownerColor);
          rg = (ajaInfo.rgTop) ? ajaInfo.rg : nsnull;
          CalcDominateBorder(nsnull, nsnull, nsnull, rg, ajaInfo.topRow, ajaInfo.cell, PR_FALSE, NS_SIDE_TOP, 
                             PR_FALSE, t2p, ajaOwner, ajaBStyle, ajaWidth, ajaColor);
          CalcDominateBorder(PR_FALSE, owner, ownerBStyle, ownerWidth, ownerColor, ajaOwner, ajaBStyle, 
                             ajaWidth, ajaColor, owner, ownerBStyle, ownerWidth, ownerColor, PR_TRUE);
          trCorner->Update(NS_SIDE_RIGHT, owner, ownerBStyle, ownerWidth, ownerColor);   
        }
        // store the top right corner in the cell map 
        if (RIGHT_DAMAGED(cellEndColIndex) && TOP_DAMAGED(rowX)) {
          if (0 != rowX) {
            tableCellMap->SetBCBorderCorner(eTopRight, *info.cellMap, iter.mRowGroupStart, rowX, cellEndColIndex, 
                                            trCorner->ownerSide, trCorner->subWidth, trCorner->bevel);
          }
          // store any corners this cell spans together with the aja cell
          for (PRInt32 rX = rowX + 1; rX < rowX + segLength; rX++) {
            tableCellMap->SetBCBorderCorner(eBottomRight, *info.cellMap, iter.mRowGroupStart, rX, 
                                            cellEndColIndex, trCorner->ownerSide, trCorner->subWidth, PR_FALSE);
          }
        }
        // update bottom right corner, topCorners, bottomCorners
        hitsSpanOnRight = (rowX + segLength < ajaInfo.rowIndex + ajaInfo.rowSpan);
        BCCornerInfo& brCorner = (hitsSpanOnRight) ? topCorners[cellEndColIndex + 1] 
                                                   : bottomCorners[cellEndColIndex + 1];
        brCorner.Set(NS_SIDE_TOP, owner, ownerBStyle, ownerWidth, ownerColor);
        priorAjaInfo = ajaInfo;
      }
    }
    for (PRInt32 colX = info.colIndex + 1; colX <= cellEndColIndex; colX++) {
      lastVerBorders[colX].Reset();
    }

    // find the dominate border considernig the cell's bottom border, adjacent cells and the table, row group, row
    if (numRows == cellEndRowIndex + 1) { // touches botom edge of table
      if (!tableBorderReset[NS_SIDE_BOTTOM]) {
        propData->mBottomBorderWidth = 0;
        tableBorderReset[NS_SIDE_BOTTOM] = PR_TRUE;
      }
      for (PRInt32 colX = info.colIndex; colX <= cellEndColIndex; colX++) {
        nsIFrame* colFrame = GetColFrame(colX); if (!colFrame) ABORT0();
        nsIFrame* cgFrame;
        colFrame->GetParent(&cgFrame); if (!cgFrame) ABORT0();
        CalcDominateBorder(this, cgFrame, colFrame, info.rg, info.bottomRow, info.cell, PR_TRUE, NS_SIDE_BOTTOM, 
                           PR_TRUE, t2p, owner, ownerBStyle, ownerWidth, ownerColor);
        // update/store the bottom left & bottom right corners 
        BCCornerInfo& blCorner = bottomCorners[colX]; // bottom left
        blCorner.Update(NS_SIDE_RIGHT, owner, ownerBStyle, ownerWidth, ownerColor);     
        tableCellMap->SetBCBorderCorner(eBottomLeft, *info.cellMap, iter.mRowGroupStart, cellEndRowIndex,                
                                        colX, blCorner.ownerSide, blCorner.subWidth, blCorner.bevel); 
        BCCornerInfo& brCorner = bottomCorners[colX + 1]; // bottom right
        brCorner.Update(NS_SIDE_LEFT, owner, ownerBStyle, ownerWidth, ownerColor); 
        if (numCols == colX + 1) { // lower right corner of the table
          tableCellMap->SetBCBorderCorner(eBottomRight, *info.cellMap, iter.mRowGroupStart, cellEndRowIndex,               
                                          colX, brCorner.ownerSide, brCorner.subWidth, brCorner.bevel, isBottomRight);  
        }
        // update lastBottomBorder and see if a new segment starts
        startSeg = SetHorBorder(ownerBStyle, ownerWidth, ownerColor, blCorner, lastBottomBorder);
        // store the border segment in the cell map and update cellBorders
        tableCellMap->SetBCBorderEdge(NS_SIDE_BOTTOM, *info.cellMap, iter.mRowGroupStart, cellEndRowIndex, 
                                      colX, 1, owner, ownerWidth, startSeg);
        // update the bottom borders of the cell, the bottom row, and the table 
        DivideBCBorderSize(ownerWidth, smallHalf, largeHalf);
        if (info.cell) {
          info.cell->SetBorderWidth(NS_SIDE_BOTTOM, PR_MAX(largeHalf, info.cell->GetBorderWidth(NS_SIDE_BOTTOM)));
        }
        if (info.bottomRow) {
          info.bottomRow->SetBottomBCBorderWidth(PR_MAX(largeHalf, info.bottomRow->GetBottomBCBorderWidth()));
        }
        propData->mBottomBorderWidth = LimitBorderWidth(PR_MAX(propData->mBottomBorderWidth, ownerWidth));
        // update lastBottomBorders
        lastBottomBorder.index = cellEndRowIndex + 1;
        lastBottomBorder.span = info.rowSpan;
        lastBottomBorders[colX] = lastBottomBorder;
      }
    }
    else {
      PRInt32 segLength = 0;
      for (PRInt32 colX = info.colIndex; colX <= cellEndColIndex; colX += segLength) {
        iter.PeekBottom(info, colX, ajaInfo);
        const nsIFrame* rg = (info.rgBottom) ? info.rg : nsnull;
        CalcDominateBorder(nsnull, nsnull, nsnull, rg, info.bottomRow, info.cell, PR_FALSE, NS_SIDE_BOTTOM, 
                           PR_TRUE, t2p, owner, ownerBStyle, ownerWidth, ownerColor);
        rg = (ajaInfo.rgTop) ? ajaInfo.rg : nsnull;
        CalcDominateBorder(nsnull, nsnull, nsnull, rg, ajaInfo.topRow, ajaInfo.cell, PR_FALSE, NS_SIDE_TOP, 
                           PR_FALSE, t2p, ajaOwner, ajaBStyle, ajaWidth, ajaColor);
        CalcDominateBorder(PR_FALSE, owner, ownerBStyle, ownerWidth, ownerColor, ajaOwner, ajaBStyle, ajaWidth,
                           ajaColor, owner, ownerBStyle, ownerWidth, ownerColor, PR_TRUE);
        segLength = PR_MAX(1, ajaInfo.colIndex + ajaInfo.colSpan - colX);
        segLength = PR_MIN(segLength, info.colIndex + info.colSpan - colX);

        // update, store the bottom left corner
        BCCornerInfo& blCorner = bottomCorners[colX]; // bottom left
        PRBool hitsSpanBelow = (colX > ajaInfo.colIndex) && (colX < ajaInfo.colIndex + ajaInfo.colSpan);
        PRBool update = PR_TRUE;
        if ((colX == info.colIndex) && (colX > damageArea.x)) {
          PRInt32 prevRowIndex = lastBottomBorders[colX - 1].index;
          if (prevRowIndex > cellEndRowIndex + 1) { // hits a rowspan on the right
            update = PR_FALSE; // the corner was taken care of during the cell on the left
          }
          else if (prevRowIndex < cellEndRowIndex) { // spans below the cell to the left
            topCorners[colX] = blCorner;
            blCorner.Set(NS_SIDE_RIGHT, owner, ownerBStyle, ownerWidth, ownerColor); 
            update = PR_FALSE;
          }
        }
        if (update) {
          blCorner.Update(NS_SIDE_RIGHT, owner, ownerBStyle, ownerWidth, ownerColor); 
        }
        if (BOTTOM_DAMAGED(cellEndRowIndex) && LEFT_DAMAGED(colX)) {
          if (hitsSpanBelow) {
            tableCellMap->SetBCBorderCorner(eBottomLeft, *info.cellMap, iter.mRowGroupStart, cellEndRowIndex, colX, 
                                            blCorner.ownerSide, blCorner.subWidth, blCorner.bevel);
          }
          // store any corners this cell spans together with the aja cell
          for (PRInt32 cX = colX + 1; cX < colX + segLength; cX++) {
            BCCornerInfo& corner = bottomCorners[cX]; 
            corner.Set(NS_SIDE_RIGHT, owner, ownerBStyle, ownerWidth, ownerColor);
            tableCellMap->SetBCBorderCorner(eBottomLeft, *info.cellMap, iter.mRowGroupStart, cellEndRowIndex, 
                                            cX, corner.ownerSide, corner.subWidth, PR_FALSE);
          }
        }
        // update lastBottomBorders and see if a new segment starts
        startSeg = SetHorBorder(ownerBStyle, ownerWidth, ownerColor, blCorner, lastBottomBorder);
        lastBottomBorder.index = cellEndRowIndex + 1;
        lastBottomBorder.span = info.rowSpan;
        for (PRInt32 cX = colX; cX < colX + segLength; cX++) {
          lastBottomBorders[cX] = lastBottomBorder;
        }

        // store the border segment the cell map and update cellBorders
        if (BOTTOM_DAMAGED(cellEndRowIndex) && LEFT_DAMAGED(colX) && RIGHT_DAMAGED(colX)) {
          tableCellMap->SetBCBorderEdge(NS_SIDE_BOTTOM, *info.cellMap, iter.mRowGroupStart, cellEndRowIndex, 
                                        colX, segLength, owner, ownerWidth, startSeg);
          // update the borders of the affected cells and rows
          DivideBCBorderSize(ownerWidth, smallHalf, largeHalf);
          if (info.cell) {
            info.cell->SetBorderWidth(NS_SIDE_BOTTOM, PR_MAX(largeHalf, info.cell->GetBorderWidth(NS_SIDE_BOTTOM)));
          }
          if (info.bottomRow) {
            info.bottomRow->SetBottomBCBorderWidth(PR_MAX(largeHalf, info.bottomRow->GetBottomBCBorderWidth()));
          }
          if (ajaInfo.cell) {
            ajaInfo.cell->SetBorderWidth(NS_SIDE_TOP, PR_MAX(smallHalf, ajaInfo.cell->GetBorderWidth(NS_SIDE_TOP)));
          }
          if (ajaInfo.topRow) {
            ajaInfo.topRow->SetTopBCBorderWidth(PR_MAX(smallHalf, ajaInfo.topRow->GetTopBCBorderWidth()));
          }
        }
        // update bottom right corner
        BCCornerInfo& brCorner = bottomCorners[colX + segLength];
        brCorner.Update(NS_SIDE_LEFT, owner, ownerBStyle, ownerWidth, ownerColor);        
      }
    }

    // see if the cell to the right had a rowspan and its lower left border needs be joined with this one's bottom
    if ((numCols != cellEndColIndex + 1) &&                                 // there is a cell to the right 
        (lastBottomBorders[cellEndColIndex + 1].span > 1)) { // cell to right was a rowspan
      BCCornerInfo& corner = bottomCorners[cellEndColIndex + 1];
      if ((NS_SIDE_TOP != corner.ownerSide) && (NS_SIDE_BOTTOM != corner.ownerSide)) { // not a vertical owner
        BCCellBorder& thisBorder = lastBottomBorder;  
        BCCellBorder& nextBorder = lastBottomBorders[info.colIndex + 1];
        if ((thisBorder.color == nextBorder.color) && (thisBorder.width == nextBorder.width) &&
            (thisBorder.style == nextBorder.style)) {
          // set the flag on the next border indicating it is not the start of a new segment
          if (iter.mCellMap) {
            BCData* bcData = tableCellMap->GetBCData(NS_SIDE_BOTTOM, *iter.mCellMap, cellEndRowIndex, 
                                                     cellEndColIndex + 1);
            if (bcData) {
              bcData->SetTopStart(PR_FALSE);
            }
          }
        }
      }
    }
  } // for (iter.First(info); info.cell; iter.Next(info)) {

  // reset the bc flag and damage area
  SetNeedToCalcBCBorders(PR_FALSE);
  propData->mDamageArea.x = propData->mDamageArea.y = propData->mDamageArea.width = propData->mDamageArea.height = 0;
  //mCellMap->Dump();
}

// Iterates over borders (left border, corner, top border) in the cell map within a damage area
// from left to right, top to bottom. All members are in terms of the 1st in flow frames, except 
// where suffixed by InFlow.  
class BCMapBorderIterator
{
public:
  BCMapBorderIterator(nsTableFrame&         aTableFrame,
                      nsTableRowGroupFrame& aRowGroupFrame,
                      nsTableRowFrame&      aRowFrame,
                      const nsRect&         aDamageArea);
  void Reset(nsTableFrame&         aTableFrame,
             nsTableRowGroupFrame& aRowGroupFrame,
             nsTableRowFrame&      aRowFrame,
             const nsRect&         aDamageArea);
  void First();
  void Next();

  nsTableFrame*         table;
  nsTableCellMap*       tableCellMap;
  nsCellMap*            cellMap;

  nsVoidArray           rowGroups;
  nsTableRowGroupFrame* prevRg;
  nsTableRowGroupFrame* rg;
  PRInt32               rowGroupIndex;
  PRInt32               fifRowGroupStart;
  PRInt32               rowGroupStart;
  PRInt32               rowGroupEnd;
  PRInt32               numRows; // number of rows in the table and all continuations

  nsTableRowFrame*      prevRow;
  nsTableRowFrame*      row;
  PRInt32               numCols;
  PRInt32               x;
  PRInt32               y;

  nsTableCellFrame*     prevCell;
  nsTableCellFrame*     cell;  
  BCCellData*           prevCellData;
  BCCellData*           cellData;
  BCData*               bcData;

  PRBool                IsTopMostTable()    { return (y == 0) && !table->GetPrevInFlow(); }
  PRBool                IsRightMostTable()  { return (x >= numCols); }
  PRBool                IsBottomMostTable() { return (y >= numRows) && !table->GetNextInFlow(); }
  PRBool                IsLeftMostTable()   { return (x == 0); }
  PRBool                IsTopMost()    { return (y == startY); }
  PRBool                IsRightMost()  { return (x >= endX); }
  PRBool                IsBottomMost() { return (y >= endY); }
  PRBool                IsLeftMost()   { return (x == startX); }
  PRBool                isNewRow;

  PRInt32               startX;
  PRInt32               startY;
  PRInt32               endX;
  PRInt32               endY;
  PRBool                isRepeatedHeader;
  PRBool                isRepeatedFooter;
  PRBool                atEnd;

private:

  PRBool SetNewRow(nsTableRowFrame* aRow = nsnull);
  PRBool SetNewRowGroup();
  void   SetNewData(PRInt32 aY, PRInt32 aX);

};

BCMapBorderIterator::BCMapBorderIterator(nsTableFrame&         aTable,
                                         nsTableRowGroupFrame& aRowGroup,
                                         nsTableRowFrame&      aRow,
                                         const nsRect&         aDamageArea)
{
  Reset(aTable, aRowGroup, aRow, aDamageArea);
}

void
BCMapBorderIterator::Reset(nsTableFrame&         aTable,
                           nsTableRowGroupFrame& aRowGroup,
                           nsTableRowFrame&      aRow,
                           const nsRect&         aDamageArea)
{
  atEnd = PR_TRUE; // gets reset when First() is called

  table = &aTable;
  rg    = &aRowGroup;                
  row   = &aRow;                     

  nsTableFrame* tableFif = (nsTableFrame*)table->GetFirstInFlow(); if (!tableFif) ABORT0();
  tableCellMap = tableFif->GetCellMap();

  startX   = aDamageArea.x;
  startY   = aDamageArea.y;
  endY     = aDamageArea.y + aDamageArea.height;
  endX     = aDamageArea.x + aDamageArea.width;

  numRows       = tableFif->GetRowCount();
  y             = 0;
  numCols       = tableFif->GetColCount();
  x             = 0;
  rowGroupIndex = -1;
  cell          = nsnull;
  cellData      = nsnull;

  // Get the ordered row groups 
  PRUint32 numRowGroups;
  table->OrderRowGroups(rowGroups, numRowGroups, nsnull);
}

void 
BCMapBorderIterator::SetNewData(PRInt32 aY,
                                PRInt32 aX)
{
  if (!tableCellMap || !tableCellMap->mBCInfo) ABORT0();

  x            = aX;
  y            = aY;
  prevCellData = cellData;
  if (IsRightMost() && IsBottomMost()) {
    cell = nsnull;
    bcData = &tableCellMap->mBCInfo->mLowerRightCorner;
  }
  else if (IsRightMost()) {
    cellData = nsnull;
    bcData = (BCData*)tableCellMap->mBCInfo->mRightBorders.ElementAt(aY);
  }
  else if (IsBottomMost()) {
    cellData = nsnull;
    bcData = (BCData*)tableCellMap->mBCInfo->mBottomBorders.ElementAt(aX);
  }
  else {
    bcData = nsnull;
    nsVoidArray* row = (nsVoidArray*)cellMap->mRows.ElementAt(y - fifRowGroupStart);
    if (row) {
      cellData = (row->Count() > x) ? (BCCellData*)row->ElementAt(x) : nsnull;
      if (cellData) {
        bcData = &cellData->mData;
        if (!cellData->IsOrig()) {
          if (cellData->IsRowSpan()) {
            aY -= cellData->GetRowSpanOffset();
          }
          if (cellData->IsColSpan()) {
            aX -= cellData->GetColSpanOffset();
          }
          if ((aX >= 0) && (aY >= 0)) {
            row = (nsVoidArray*)cellMap->mRows.ElementAt(aY - fifRowGroupStart);
            if (row) {
              cellData = (BCCellData*)row->ElementAt(aX);
            }
          }
        }
        if (cellData->IsOrig()) {
          prevCell = cell;
          cell = cellData->GetCellFrame();
        }
      }
    }
  }
}

PRBool
BCMapBorderIterator::SetNewRow(nsTableRowFrame* aRow)
{
  prevRow = row;
  row      = (aRow) ? aRow : row->GetNextRow();
 
  if (row) {
    isNewRow = PR_TRUE;
    y = row->GetRowIndex();
    x = startX;
  }
  else {
    atEnd = PR_TRUE;
  }
  return !atEnd;
}


PRBool
BCMapBorderIterator::SetNewRowGroup()
{
  rowGroupIndex++;

  isRepeatedHeader = PR_FALSE;
  isRepeatedFooter = PR_FALSE;

  if (rowGroupIndex < rowGroups.Count()) {
    prevRg = rg;
    nsIFrame* frame = (nsTableRowGroupFrame*)rowGroups.ElementAt(rowGroupIndex); if (!frame) ABORT1(PR_FALSE);
    rg = table->GetRowGroupFrame(frame); if (!rg) ABORT1(PR_FALSE);
    fifRowGroupStart = ((nsTableRowGroupFrame*)rg->GetFirstInFlow())->GetStartRowIndex();
    rowGroupStart    = rg->GetStartRowIndex(); 
    rowGroupEnd      = rowGroupStart + rg->GetRowCount() - 1;

    if (SetNewRow(rg->GetFirstRow())) {
      cellMap = tableCellMap->GetMapFor(*(nsTableRowGroupFrame*)rg->GetFirstInFlow()); if (!cellMap) ABORT1(PR_FALSE);
    }
    if (rg && table->GetPrevInFlow() && !rg->GetPrevInFlow()) {
      // if rg doesn't have a prev in flow, then it may be a repeated header or footer
      const nsStyleDisplay* display;
      rg->GetStyleData(eStyleStruct_Display, (const nsStyleStruct *&)display);
      if (y == startY) {
        isRepeatedHeader = (NS_STYLE_DISPLAY_TABLE_HEADER_GROUP == display->mDisplay);
      }
      else {
        isRepeatedFooter = (NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP == display->mDisplay);
      }
    }
  }
  else {
    atEnd = PR_TRUE;
  }
  return !atEnd;
}

void 
BCMapBorderIterator::First()
{
  if (!table || (startX >= numCols) || (startY >= numRows)) ABORT0();

  atEnd = PR_FALSE;

  PRUint32 numRowGroups = rowGroups.Count();
  for (PRUint32 rgX = 0; rgX < numRowGroups; rgX++) { 
    nsIFrame* frame = (nsIFrame*)rowGroups.ElementAt(rgX);
    nsTableRowGroupFrame* rowG = table->GetRowGroupFrame(frame);
    if (rowG) {
      PRInt32 start = rowG->GetStartRowIndex();
      PRInt32 end   = start + rowG->GetRowCount() - 1;
      if ((startY >= start) && (startY <= end)) {
        rowGroupIndex = rgX - 1; // SetNewRowGroup increments rowGroupIndex
        if (SetNewRowGroup()) { 
          while ((y < startY) && !atEnd) {
            SetNewRow();
          }
          if (!atEnd) {
            SetNewData(startY, startX);
          }
        }
        return;
      }
    }
  }
  atEnd = PR_TRUE;
}

void 
BCMapBorderIterator::Next()
{
  if (atEnd) ABORT0();
  isNewRow = PR_FALSE;

  x++;
  if (x > endX) {
    y++;
    if (y == endY) {
      x = startX;
    }
    else if (y < endY) {
      if (y <= rowGroupEnd) {
        SetNewRow();
      }
      else {
        SetNewRowGroup();
      }
    }
    else {
      atEnd = PR_TRUE;
    }
  }
  if (!atEnd) {
    SetNewData(y, x);
  }
}

// XXX if CalcVerCornerOffset and CalcHorCornerOffset remain similar, combine them
static nscoord
CalcVerCornerOffset(PRUint8 aCornerOwnerSide,
                    nscoord aCornerSubWidth,
                    nscoord aHorWidth,
                    PRBool  aIsStartOfSeg,
                    PRBool  aIsBevel,
                    float   aPixelsToTwips)
{
  nscoord offset = 0;
  nscoord smallHalf, largeHalf;
  if ((NS_SIDE_TOP == aCornerOwnerSide) || (NS_SIDE_BOTTOM == aCornerOwnerSide)) {
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
  return NSToCoordRound(aPixelsToTwips * (float)offset);
}

static nscoord
CalcHorCornerOffset(PRUint8 aCornerOwnerSide,
                    nscoord aCornerSubWidth,
                    nscoord aVerWidth,
                    PRBool  aIsStartOfSeg,
                    PRBool  aIsBevel,
                    float   aPixelsToTwips)
{
  nscoord offset = 0;
  nscoord smallHalf, largeHalf;
  if ((NS_SIDE_LEFT == aCornerOwnerSide) || (NS_SIDE_RIGHT == aCornerOwnerSide)) {
    DivideBCBorderSize(aCornerSubWidth, smallHalf, largeHalf);
    if (aIsBevel) {
      offset = (aIsStartOfSeg) ? -largeHalf : smallHalf;
    }
    else {
      offset = (NS_SIDE_LEFT == aCornerOwnerSide) ? smallHalf : -largeHalf;
    }
  }
  else {
    DivideBCBorderSize(aVerWidth, smallHalf, largeHalf);
    if (aIsBevel) {
      offset = (aIsStartOfSeg) ? -largeHalf : smallHalf;
    }
    else {
      offset = (aIsStartOfSeg) ? smallHalf : -largeHalf;
    }
  }
  return NSToCoordRound(aPixelsToTwips * (float)offset);
}

struct BCVerticalSeg
{
  BCVerticalSeg();
 
  void Start(BCMapBorderIterator& aIter,
             BCBorderOwner        aBorderOwner,
             nscoord              aVerSegWidth,
             nscoord              aPrevHorSegHeight,
             nscoord              aHorSegHeight,
             float                aPixelsToTwips,
             BCVerticalSeg*       aVerInfoArray);
  
  union {
    nsTableColFrame*  col;
    PRInt32           colWidth;
  };
  PRInt32            colX;
  nsTableCellFrame*  ajaCell;
  nsTableCellFrame*  firstCell;  // cell at the start of the segment
  nsTableCellFrame*  lastCell;   // cell at the current end of the segment
  PRInt32            segY;
  PRInt32            segHeight;
  PRInt16            segWidth;   // width in pixels
  PRUint8            owner;
  PRUint8            bevelSide;
  PRUint16           bevelOffset;
};

BCVerticalSeg::BCVerticalSeg() 
{ 
  col = nsnull; firstCell = lastCell = ajaCell = nsnull; colX = segY = segHeight = 0;
  segWidth = bevelOffset = 0; bevelSide = 0; owner = eCellOwner; 
}
 
void
BCVerticalSeg::Start(BCMapBorderIterator& aIter,
                     BCBorderOwner        aBorderOwner,
                     nscoord              aVerSegWidth,
                     nscoord              aPrevHorSegHeight,
                     nscoord              aHorSegHeight,
                     float                aPixelsToTwips,
                     BCVerticalSeg*       aVerInfoArray)
{
  PRUint8      ownerSide = 0;
  PRPackedBool bevel     = PR_FALSE;
  PRInt32      xAdj      = aIter.x - aIter.startX;

  nscoord cornerSubWidth  = (aIter.bcData) ? aIter.bcData->GetCorner(ownerSide, bevel) : 0;
  PRBool  topBevel        = (aVerSegWidth > 0) ? bevel : PR_FALSE;
  nscoord maxHorSegHeight = PR_MAX(aPrevHorSegHeight, aHorSegHeight);
  nscoord offset          = CalcVerCornerOffset(ownerSide, cornerSubWidth, maxHorSegHeight, 
                                                PR_TRUE, topBevel, aPixelsToTwips);

  bevelOffset = (topBevel) ? maxHorSegHeight : 0;
  bevelSide   = (aHorSegHeight > 0) ? NS_SIDE_RIGHT : NS_SIDE_LEFT;
  segY       += offset;
  segHeight   = -offset;
  segWidth    = aVerSegWidth;
  owner       = aBorderOwner;
  firstCell   = aIter.cell;
  if (xAdj > 0) {
    ajaCell = aVerInfoArray[xAdj - 1].lastCell;
  }
}

struct BCHorizontalSeg
{
  BCHorizontalSeg();

  void Start(BCMapBorderIterator& aIter,
             BCBorderOwner        aBorderOwner,
             PRUint8              aCornerOwnerSide,
             nscoord              aSubWidth,
             PRBool               aBevel,
             nscoord              aTopVerSegWidth,
             nscoord              aBottomVerSegWidth,
             nscoord              aHorSegHeight,
             nsTableCellFrame*    aLastCell,
             float                aPixelsToTwips);
  
  nscoord            x;
  nscoord            y;
  nscoord            width;
  nscoord            height;
  PRBool             leftBevel;
  nscoord            leftBevelOffset;
  PRUint8            leftBevelSide;
  PRUint8            owner;
  nsTableCellFrame*  firstCell; // cell at the start of the segment
  nsTableCellFrame*  ajaCell;
};

BCHorizontalSeg::BCHorizontalSeg() 
{ 
  x = y = width = height = leftBevel = leftBevelOffset = leftBevelSide = 0; 
  firstCell = ajaCell = nsnull;
}
  
  
void
BCHorizontalSeg::Start(BCMapBorderIterator& aIter,
                       BCBorderOwner        aBorderOwner,
                       PRUint8              aCornerOwnerSide,
                       nscoord              aSubWidth,
                       PRBool               aBevel,
                       nscoord              aTopVerSegWidth,
                       nscoord              aBottomVerSegWidth,
                       nscoord              aHorSegHeight,
                       nsTableCellFrame*    aLastCell,
                       float                aPixelsToTwips)
{
  owner = aBorderOwner;
  leftBevel = (aHorSegHeight > 0) ? aBevel : PR_FALSE;
  nscoord maxVerSegWidth = PR_MAX(aTopVerSegWidth, aBottomVerSegWidth);
  nscoord offset = CalcHorCornerOffset(aCornerOwnerSide, aSubWidth, maxVerSegWidth, 
                                       PR_TRUE, leftBevel, aPixelsToTwips);
  leftBevelOffset = (leftBevel && (aHorSegHeight > 0)) ? maxVerSegWidth : 0;
  leftBevelSide   = (aBottomVerSegWidth > 0) ? NS_SIDE_BOTTOM : NS_SIDE_TOP;
  x              += offset;
  width           = -offset;
  height          = aHorSegHeight;
  firstCell       = aIter.cell;
  ajaCell         = (aIter.IsTopMost()) ? nsnull : aLastCell; 
}

void 
nsTableFrame::PaintBCBorders(nsIPresContext*      aPresContext,
                             nsIRenderingContext& aRenderingContext,
                             const nsRect&        aDirtyRect)
{
  nsMargin childAreaOffset = GetChildAreaOffset(*aPresContext, nsnull); 
  nsTableFrame* firstInFlow = (nsTableFrame*)GetFirstInFlow(); if (!firstInFlow) ABORT0();
  GET_PIXELS_TO_TWIPS(aPresContext, p2t);

  PRInt32 startColX = childAreaOffset.left;                    // x position of first col in damage area
  PRInt32 startRowY = (mPrevInFlow) ? 0 : childAreaOffset.top; // y position of first row in damage area

  const nsStyleBackground* bgColor = nsCSSRendering::FindNonTransparentBackground(mStyleContext);
  // determine the damage area in terms of rows and columns and finalize startColX and startRowY
  PRUint32 startRowIndex, endRowIndex, startColIndex, endColIndex;
  startRowIndex = endRowIndex = startColIndex = endColIndex = 0;

  nsAutoVoidArray rowGroups;
  PRUint32 numRowGroups;
  OrderRowGroups(rowGroups, numRowGroups, nsnull);
  PRBool done = PR_FALSE;
  PRBool haveIntersect = PR_FALSE;
  nsTableRowGroupFrame* inFlowRG  = nsnull;
  nsTableRowFrame*      inFlowRow = nsnull;
  // find startRowIndex, endRowIndex, startRowY
  nscoord onePixel = NSToCoordRound(p2t);
  PRInt32 rowY = startRowY;
  for (PRUint32 rgX = 0; (rgX < numRowGroups) && !done; rgX++) {
    nsIFrame* kidFrame = (nsIFrame*)rowGroups.ElementAt(rgX);
    nsTableRowGroupFrame* rgFrame = GetRowGroupFrame(kidFrame); if (!rgFrame) ABORT0();
    nsRect rowRect;
    for (nsTableRowFrame* rowFrame = rgFrame->GetFirstRow(); rowFrame; rowFrame = rowFrame->GetNextRow()) {
      // conservatively estimate the half border widths outside the row
      nscoord topBorderHalf    = (mPrevInFlow) ? 0 : rowFrame->GetTopBCBorderWidth(&p2t) + onePixel; 
      nscoord bottomBorderHalf = (mNextInFlow) ? 0 : rowFrame->GetBottomBCBorderWidth(&p2t) + onePixel;
      // get the row rect relative to the table rather than the row group
      rowFrame->GetRect(rowRect);
      if (haveIntersect) {
        if (aDirtyRect.YMost() >= (rowY - topBorderHalf)) {
          nsTableRowFrame* fifRow = (nsTableRowFrame*)rowFrame->GetFirstInFlow(); if (!fifRow) ABORT0();
          endRowIndex = fifRow->GetRowIndex();
        }
        else done = PR_TRUE;
      }
      else {
        if ((rowY + rowRect.height + bottomBorderHalf) >= aDirtyRect.y) {
          inFlowRG  = rgFrame;
          inFlowRow = rowFrame;
          nsTableRowFrame* fifRow = (nsTableRowFrame*)rowFrame->GetFirstInFlow(); if (!fifRow) ABORT0();
          startRowIndex = endRowIndex = fifRow->GetRowIndex();
          haveIntersect = PR_TRUE;
        }
        else {
          startRowY += rowRect.height;
        }
      }
      rowY += rowRect.height; 
    }
  }
  if (!inFlowRG || !inFlowRow) ABORT0();

  // find startColIndex, endColIndex, startColX
  haveIntersect = PR_FALSE;
  PRUint32 numCols = GetColCount();
  if (0 == numCols) return;

  nscoord x = 0;
  for (PRUint32 colX = 0; colX < numCols; colX++) {
    nsTableColFrame* colFrame = firstInFlow->GetColFrame(colX); if (!colFrame) ABORT0();
    // conservatively estimate the half border widths outside the col
    nscoord leftBorderHalf    = colFrame->GetLeftBorderWidth(&p2t) + onePixel; 
    nscoord rightBorderHalf   = colFrame->GetRightBorderWidth(&p2t) + onePixel;
    // get the col rect relative to the table rather than the col group
    nsRect rect;
    colFrame->GetRect(rect);
    if (haveIntersect) {
      if (aDirtyRect.XMost() >= (x - leftBorderHalf)) {
        endColIndex = colX;
      }
      else break;
    }
    else {
      if ((x + rect.width + rightBorderHalf) >= aDirtyRect.x) {
        startColIndex = endColIndex = colX;
        haveIntersect = PR_TRUE;
      }
      else {
        startColX += rect.width;
      }
    }
    x += rect.width;
  }

  // iterate the cell map and build up border segments
  nsRect damageArea(startColIndex, startRowIndex, 1 + endColIndex - startColIndex, 
                    1 + endRowIndex - startRowIndex);
  BCVerticalSeg* verInfo = new BCVerticalSeg[damageArea.width + 1]; if (!verInfo) ABORT0();

  BCBorderOwner borderOwner, ignoreBorderOwner;
  PRUint8 ownerSide;
  nscoord cornerSubWidth, smallHalf, largeHalf;
  nsRect rowRect(0,0,0,0);
  PRBool isSegStart, ignoreSegStart;
  nscoord prevHorSegHeight = 0;
  PRPackedBool bevel;
  PRInt32 repeatedHeaderY = -99;
  PRBool  afterRepeatedHeader = PR_FALSE;
  PRBool  startRepeatedFooter = PR_FALSE;

  // First, paint all of the vertical borders from top to bottom and left to right as they become complete
  // They are painted first, since they are less efficient to paint than horizontal segments. They were 
  // stored with as few segments as possible (since horizontal borders are painted last and possibly over them).
  BCMapBorderIterator iter(*this, *inFlowRG, *inFlowRow, damageArea); 
  for (iter.First(); !iter.atEnd; iter.Next()) {
    nscoord verSegWidth = (iter.bcData) ? iter.bcData->GetLeftEdge(borderOwner, isSegStart) : 0;
    nscoord horSegHeight = (iter.bcData) ? iter.bcData->GetTopEdge(ignoreBorderOwner, ignoreSegStart) : 0;

    PRInt32 xAdj = iter.x - iter.startX;
    if (iter.isNewRow) {
      prevHorSegHeight = 0;
      iter.row->GetRect(rowRect);
      if (iter.isRepeatedHeader) {
        repeatedHeaderY = iter.y;
      }
      afterRepeatedHeader = !iter.isRepeatedHeader && (iter.y == (repeatedHeaderY + 1));
      startRepeatedFooter = iter.isRepeatedFooter && (iter.y == iter.rowGroupStart) && (iter.y != iter.startY);
    }
    BCVerticalSeg& info = verInfo[xAdj];
    if (!info.col) { // on the first damaged row and the first segment in the col
      info.col = iter.IsRightMostTable() ? verInfo[xAdj - 1].col : firstInFlow->GetColFrame(iter.x); if (!info.col) ABORT0();
      if (0 == xAdj) {
        info.colX = startColX;
      }
      // set colX for the next column
      if (!iter.IsRightMost()) {
        nsRect colRect;
        info.col->GetRect(colRect); 
        verInfo[xAdj + 1].colX = info.colX + colRect.width;
      }
      info.segY = startRowY; 
      info.Start(iter, borderOwner, verSegWidth, prevHorSegHeight, horSegHeight, p2t, verInfo);
      info.lastCell = iter.cell;
    }

    if (!iter.IsTopMost() && (isSegStart || iter.IsBottomMost() || afterRepeatedHeader || startRepeatedFooter)) {
      // paint the previous seg or the current one if iter.IsBottomMost()
      if (info.segHeight > 0) {
        cornerSubWidth = (iter.bcData) ? iter.bcData->GetCorner(ownerSide, bevel) : 0;
        PRBool endBevel = (info.segWidth > 0) ? bevel : PR_FALSE; 
        nscoord bottomHorSegHeight = PR_MAX(prevHorSegHeight, horSegHeight); 
        nscoord endOffset = CalcVerCornerOffset(ownerSide, cornerSubWidth, bottomHorSegHeight, 
                                                PR_FALSE, endBevel, p2t);
        info.segHeight += endOffset;
        if (info.segWidth > 0) {     
          // get the border style, color and paint the segment
          PRUint8 side = (iter.IsRightMost()) ? NS_SIDE_RIGHT : NS_SIDE_LEFT;
          nsTableColGroupFrame* cg = nsnull;
          nsTableColFrame* col = info.col; if (!col) ABORT0();
          nsTableCellFrame* cell = info.firstCell; 
          PRUint8 style = NS_STYLE_BORDER_STYLE_SOLID;
          nscolor color = 0xFFFFFFFF;
          PRBool ignoreIfRules = (iter.IsRightMostTable() || iter.IsLeftMostTable());

          switch (info.owner) {
          case eTableOwner:
            ::GetStyleInfo(*this, side, style, color);
            break;
          case eAjaColGroupOwner: 
            side = NS_SIDE_RIGHT;
            if (!iter.IsRightMostTable() && (xAdj > 0)) {
              col = verInfo[xAdj - 1].col; 
            } // and fall through
          case eColGroupOwner:
            if (col) {
              col->GetParent((nsIFrame**)&cg);
              if (cg) {
                ::GetStyleInfo(*cg, side, style, color, ignoreIfRules);
              }
            }
            break;
          case eAjaColOwner: 
            side = NS_SIDE_RIGHT;
            if (!iter.IsRightMostTable() && (xAdj > 0)) {
              col = verInfo[xAdj - 1].col; 
            } // and fall through
          case eColOwner:
            if (col) {
              ::GetStyleInfo(*col, side, style, color, ignoreIfRules);
            }
            break;
          case eAjaRowGroupOwner:
            NS_ASSERTION(PR_FALSE, "program error"); // and fall through
          case eRowGroupOwner:
            NS_ASSERTION(iter.IsLeftMostTable() || iter.IsRightMostTable(), "program error");
            if (iter.rg) {
              ::GetStyleInfo(*iter.rg, side, style, color, ignoreIfRules); 
            }
            break;
          case eAjaRowOwner:
            NS_ASSERTION(PR_FALSE, "program error"); // and fall through
          case eRowOwner: 
            NS_ASSERTION(iter.IsLeftMostTable() || iter.IsRightMostTable(), "program error");
            if (iter.row) {
              ::GetStyleInfo(*iter.row, side, style, color, ignoreIfRules); 
            }
            break;
          case eAjaCellOwner:
            side = NS_SIDE_RIGHT;
            cell = info.ajaCell; // and fall through
          case eCellOwner:
            if (cell) {
              ::GetStyleInfo(*cell, side, style, color);
            }
            break;
          }
          DivideBCBorderSize(info.segWidth, smallHalf, largeHalf);
          nsRect segRect(info.colX - NSToCoordRound(p2t * (float)largeHalf), info.segY, 
                         NSToCoordRound(p2t * (float)info.segWidth), info.segHeight);
          nscoord bottomBevelOffset = (endBevel) ? NSToCoordRound(p2t * (float)bottomHorSegHeight) : 0;
          PRUint8 bottomBevelSide = (horSegHeight > 0) ? NS_SIDE_RIGHT : NS_SIDE_LEFT;
          nsCSSRendering::DrawTableBorderSegment(aRenderingContext, style, color, bgColor, segRect, p2t, 
                                                 info.bevelSide, NSToCoordRound(p2t * (float)info.bevelOffset), 
                                                 bottomBevelSide, bottomBevelOffset);
        } // if (info.segWidth > 0) { 
        info.segY = info.segY + info.segHeight - endOffset;
      } // if (info.segHeight > 0)
      info.Start(iter, borderOwner, verSegWidth, prevHorSegHeight, horSegHeight, p2t, verInfo);
    } // if (!iter.IsTopMost() && (isSegStart || iter.IsBottomMost())) {

    info.lastCell   = iter.cell;
    info.segHeight += rowRect.height;
    prevHorSegHeight = horSegHeight;
  } // for (iter.First(); !iter.atEnd; iter.Next()) {

  // Next, paint all of the horizontal border segments from top to bottom reuse the verInfo 
  // array to keep tract of col widths and vertical segments for corner calculations
  memset(verInfo, 0, damageArea.width * sizeof(BCVerticalSeg)); // XXX reinitialize properly
  for (PRInt32 xIndex = 0; xIndex < damageArea.width; xIndex++) {
    verInfo[xIndex].colWidth = -1;
  }
  PRInt32 nextY = startRowY;
  BCHorizontalSeg horSeg;

  iter.Reset(*this, *inFlowRG, *inFlowRow, damageArea);
  for (iter.First(); !iter.atEnd; iter.Next()) {
    nscoord leftSegWidth = (iter.bcData) ? iter.bcData->GetLeftEdge(ignoreBorderOwner, ignoreSegStart) : 0;
    nscoord topSegHeight = (iter.bcData) ? iter.bcData->GetTopEdge(borderOwner, isSegStart) : 0;

    PRInt32 xAdj = iter.x - iter.startX;
    // store the current col width if it hasn't been already
    if (verInfo[xAdj].colWidth < 0) {
      if (iter.IsRightMostTable()) {
        verInfo[xAdj].colWidth = verInfo[xAdj - 1].colWidth;
      }
      else {
        nsTableColFrame* col = firstInFlow->GetColFrame(iter.x); if (!col) ABORT0();
        nsRect colRect;
        col->GetRect(colRect);
        verInfo[xAdj].colWidth = colRect.width;
      }
    }
    cornerSubWidth = (iter.bcData) ? iter.bcData->GetCorner(ownerSide, bevel) : 0;
    nscoord verWidth = PR_MAX(verInfo[xAdj].segWidth, leftSegWidth);
    if (iter.isNewRow || (iter.IsLeftMost() && iter.IsBottomMostTable())) {
      iter.row->GetRect(rowRect);
      horSeg.y = nextY;
      nextY    = nextY + rowRect.height;
      horSeg.x = startColX;
      horSeg.Start(iter, borderOwner, ownerSide, cornerSubWidth, bevel, verInfo[xAdj].segWidth, 
                   leftSegWidth, topSegHeight, verInfo[xAdj].lastCell, p2t);
    }
    PRBool verOwnsCorner = (NS_SIDE_TOP == ownerSide) || (NS_SIDE_BOTTOM == ownerSide);
    if (!iter.IsLeftMost() && (isSegStart || iter.IsRightMost() || verOwnsCorner)) {
      // paint the previous seg or the current one if iter.IsRightMost()
      if (horSeg.width > 0) {
        PRBool endBevel = (horSeg.height > 0) ? bevel : 0;
        nscoord endOffset = CalcHorCornerOffset(ownerSide, cornerSubWidth, verWidth, PR_FALSE, endBevel, p2t);
        horSeg.width += endOffset;
        if (horSeg.height > 0) {
          // get the border style, color and paint the segment
          PRUint8 side = (iter.IsBottomMost()) ? NS_SIDE_BOTTOM : NS_SIDE_TOP;
          nsIFrame* rg   = iter.rg;          if (!rg) ABORT0();
          nsIFrame* row  = iter.row;         if (!row) ABORT0();
          nsIFrame* cell = horSeg.firstCell; if (!cell) ABORT0();
          nsIFrame* cg;
          nsIFrame* col;

          PRUint8 style = NS_STYLE_BORDER_STYLE_SOLID; 
          nscolor color = 0xFFFFFFFF;
          PRBool ignoreIfRules = (iter.IsTopMostTable() || iter.IsBottomMostTable());

          switch (horSeg.owner) {
          case eTableOwner:
            ::GetStyleInfo(*this, side, style, color);
            break;
          case eAjaColGroupOwner: 
            NS_ASSERTION(PR_FALSE, "program error"); // and fall through
          case eColGroupOwner:
            NS_ASSERTION(iter.IsTopMostTable() || iter.IsBottomMostTable(), "program error");
            col = firstInFlow->GetColFrame(iter.x - 1); if (!col) ABORT0();
            col->GetParent(&cg); if (!cg) ABORT0();
            ::GetStyleInfo(*cg, side, style, color, ignoreIfRules);
            break;
          case eAjaColOwner: 
            NS_ASSERTION(PR_FALSE, "program error"); // and fall through
          case eColOwner:
            NS_ASSERTION(iter.IsTopMostTable() || iter.IsBottomMostTable(), "program error");
            col = firstInFlow->GetColFrame(iter.x - 1); if (!col) ABORT0();
            ::GetStyleInfo(*col, side, style, color, ignoreIfRules);
            break;
          case eAjaRowGroupOwner: 
            side = NS_SIDE_BOTTOM;
            rg = (iter.IsBottomMostTable()) ? iter.rg : iter.prevRg; // and fall through
          case eRowGroupOwner:
            if (rg) {
              ::GetStyleInfo(*rg, side, style, color, ignoreIfRules);
            }
            break;
          case eAjaRowOwner: 
            side = NS_SIDE_BOTTOM;
            row = iter.prevRow; // and fall through
          case eRowOwner:
            if (row) {
              ::GetStyleInfo(*row, side, style, color, iter.IsBottomMostTable());
            }
            break;
          case eAjaCellOwner:
            side = NS_SIDE_BOTTOM;
            // if this is null due to the damage area origin-y > 0, then the border won't show up anyway
            cell = horSeg.ajaCell; 
            // and fall through
          case eCellOwner:
            if (cell) {
              ::GetStyleInfo(*cell, side, style, color);
            }
            break;
          }
          DivideBCBorderSize(horSeg.height, smallHalf, largeHalf);
          nsRect segRec(horSeg.x, horSeg.y - NSToCoordRound(p2t * (float)largeHalf), horSeg.width, 
                        NSToCoordRound(p2t * (float)horSeg.height));
          nscoord rightBevelOffset = (endBevel) ? NSToCoordRound(p2t * (float)verWidth) : 0;
          PRUint8 rightBevelSide = (leftSegWidth > 0) ? NS_SIDE_BOTTOM : NS_SIDE_TOP;
          nsCSSRendering::DrawTableBorderSegment(aRenderingContext, style, color, bgColor, segRec, p2t, horSeg.leftBevelSide, 
                                                 NSToCoordRound(p2t * (float)horSeg.leftBevelOffset), 
                                                 rightBevelSide, rightBevelOffset);
        } // if (horSeg.height > 0) {
        horSeg.x = horSeg.x + horSeg.width - endOffset;
      } // if (horSeg.width > 0) {
      horSeg.Start(iter, borderOwner, ownerSide, cornerSubWidth, bevel, verInfo[xAdj].segWidth, 
                   leftSegWidth, topSegHeight, verInfo[xAdj].lastCell, p2t);
    } // if (!iter.IsLeftMost() && (isSegStart || iter.IsRightMost() || verOwnsCorner)) {
    horSeg.width += verInfo[xAdj].colWidth;
    verInfo[xAdj].segWidth = leftSegWidth;
    verInfo[xAdj].lastCell = iter.cell;
  }
  delete [] verInfo;
}

/********************************************************************************
 ** DEBUG_TABLE_REFLOW_TIMING                                                  **
 ********************************************************************************/

#ifdef DEBUG

static PRBool 
GetFrameTypeName(nsIAtom* aFrameType,
                 char*    aName)
{
  PRBool isTable = PR_FALSE;
  if (nsLayoutAtoms::tableOuterFrame == aFrameType) 
    strcpy(aName, "Tbl");
  else if (nsLayoutAtoms::tableFrame == aFrameType) {
    strcpy(aName, "Tbl");
    isTable = PR_TRUE;
  }
  else if (nsLayoutAtoms::tableRowGroupFrame == aFrameType) 
    strcpy(aName, "RowG");
  else if (nsLayoutAtoms::tableRowFrame == aFrameType) 
    strcpy(aName, "Row");
  else if (IS_TABLE_CELL(aFrameType)) 
    strcpy(aName, "Cell");
  else if (nsLayoutAtoms::blockFrame == aFrameType) 
    strcpy(aName, "Block");
  else 
    NS_ASSERTION(PR_FALSE, "invalid call to GetFrameTypeName");

  return isTable;
}
#endif

#if defined DEBUG_TABLE_REFLOW_TIMING

#define INDENT_PER_LEVEL 1

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

nsReflowTimer* GetFrameTimer(nsIFrame* aFrame,
                             nsIAtom*  aFrameType)
{
  if (nsLayoutAtoms::tableOuterFrame == aFrameType) 
    return ((nsTableOuterFrame*)aFrame)->mTimer;
  else if (nsLayoutAtoms::tableFrame == aFrameType) 
    return ((nsTableFrame*)aFrame)->mTimer;
  else if (nsLayoutAtoms::tableRowGroupFrame == aFrameType) 
    return ((nsTableRowGroupFrame*)aFrame)->mTimer;
  else if (nsLayoutAtoms::tableRowFrame == aFrameType) 
    return ((nsTableRowFrame*)aFrame)->mTimer;
  else if (IS_TABLE_CELL(aFrameType)) 
    return ((nsTableCellFrame*)aFrame)->mTimer;
  else if (nsLayoutAtoms::blockFrame == aFrameType) { 
    nsIFrame* parentFrame;
    aFrame->GetParent(&parentFrame);
    nsCOMPtr<nsIAtom> fType;
    parentFrame->GetFrameType(getter_AddRefs(fType));
    if (IS_TABLE_CELL(fType)) {
      nsTableCellFrame* cellFrame = (nsTableCellFrame*)parentFrame;
      // fix up the block timer, which may be referring to the cell
      if (cellFrame->mBlockTimer->mFrame == parentFrame) {
        cellFrame->mBlockTimer->mFrame = aFrame;
        NS_IF_RELEASE(cellFrame->mBlockTimer->mFrameType);
        cellFrame->mBlockTimer->mFrameType = nsLayoutAtoms::blockFrame;
        NS_ADDREF(cellFrame->mBlockTimer->mFrameType);
      }
      return cellFrame->mBlockTimer;
    }
  }
  return nsnull;
}

void DebugReflowPrintAuxTimer(char*          aMes, 
                              nsReflowTimer* aTimer)
{
  if (aTimer->mNumStarts > 0) {
    printf("%s %dms", aMes, aTimer->Elapsed());
    if (aTimer->mNumStarts > 1) {
      printf(" times=%d", aTimer->mNumStarts);
    }
  }
}

void DebugReflowPrint(nsReflowTimer& aTimer,
                      PRUint32       aLevel,
                      PRBool         aSummary)
{
  // set up the indentation
  char indentChar[128];
  PRInt32 indent = INDENT_PER_LEVEL * aLevel;
  memset (indentChar, ' ', indent);
  indentChar[indent] = 0;

  // get the frame type
  char fName[128];
  PRBool isTable = GetFrameTypeName(aTimer.mFrameType, fName);

  // print the timer
  printf("\n%s%s %dms %p", indentChar, fName, aTimer.Elapsed(), aTimer.mFrame);
  if (aSummary) {
    printf(" times=%d", aTimer.mNumStarts);
    if (isTable) {
      printf("\n%s", indentChar);
      DebugReflowPrintAuxTimer("init", aTimer.mNextSibling);
      DebugReflowPrintAuxTimer(" balanceCols", aTimer.mNextSibling->mNextSibling);
      DebugReflowPrintAuxTimer(" nonPctCols", aTimer.mNextSibling->mNextSibling->mNextSibling);
      DebugReflowPrintAuxTimer(" nonPctColspans", aTimer.mNextSibling->mNextSibling->mNextSibling->mNextSibling);
      DebugReflowPrintAuxTimer(" pctCols", aTimer.mNextSibling->mNextSibling->mNextSibling->mNextSibling->mNextSibling);
    }
  }
  else {
    char avWidth[16];
    char avHeight[16];
    char compWidth[16];
    char compHeight[16];
    char desWidth[16];
    char desHeight[16];
    PrettyUC(aTimer.mAvailWidth, avWidth);
    PrettyUC(aTimer.mAvailWidth, avHeight);
    PrettyUC(aTimer.mComputedWidth, compWidth);
    PrettyUC(aTimer.mComputedHeight, compHeight);
    PrettyUC(aTimer.mDesiredWidth, desWidth);
    PrettyUC(aTimer.mDesiredHeight, desHeight);
    printf(" r=%d", aTimer.mReason); 
    if (aTimer.mReflowType >= 0) {
      printf(",%d", aTimer.mReflowType);
    }
    printf(" a=%s,%s c=%s,%s d=%s,%s", avWidth, avHeight, compWidth, compHeight, desWidth, desHeight); 
    if (aTimer.mMaxElementWidth >= 0) {
      PrettyUC(aTimer.mMaxElementWidth, avWidth);
      printf(" me=%s", avWidth);
    }
    if (aTimer.mMaxWidth >= 0) {
      PrettyUC(aTimer.mMaxWidth, avWidth);
      printf(" m=%s", avWidth);
    }
    if (NS_FRAME_IS_NOT_COMPLETE(aTimer.mStatus)) {
      printf(" status=%d", aTimer.mStatus);
    }
    printf(" cnt=%d", aTimer.mCount);
    if (isTable) {
      printf("\n%s", indentChar);
      DebugReflowPrintAuxTimer("init", aTimer.mNextSibling);
      DebugReflowPrintAuxTimer(" balanceCols", aTimer.mNextSibling->mNextSibling);
      DebugReflowPrintAuxTimer(" nonPctCols", aTimer.mNextSibling->mNextSibling->mNextSibling);
      DebugReflowPrintAuxTimer(" nonPctColspans", aTimer.mNextSibling->mNextSibling->mNextSibling->mNextSibling);
      DebugReflowPrintAuxTimer(" pctCols", aTimer.mNextSibling->mNextSibling->mNextSibling->mNextSibling->mNextSibling);
    }
  }
  // print the timer's children
  nsVoidArray& children = aTimer.mChildren;
  PRInt32 numChildren = children.Count();
  for (PRInt32 childX = 0; childX < numChildren; childX++) {
    nsReflowTimer* child = (nsReflowTimer*)children.ElementAt(childX);
    if (child) {
      DebugReflowPrint(*child, aLevel + 1, aSummary);
    }
    else NS_ASSERTION(PR_FALSE, "bad DebugTimeReflow");
  }
}

void nsTableFrame::DebugReflow(nsIFrame*            aFrame,
                               nsHTMLReflowState&   aState,
                               nsHTMLReflowMetrics* aMetrics,
                               nsReflowStatus       aStatus)
{
#ifdef DEBUG_TABLE_REFLOW_TIMING_DETAIL
  // get the parent timer 
  const nsHTMLReflowState* parentRS = aState.parentReflowState;
  nsReflowTimer* parentTimer = nsnull;
  while (parentRS) {
    parentTimer = (nsReflowTimer *)parentRS->mDebugHook;
    if (parentTimer) break;
    parentRS = parentRS->parentReflowState;
  }
#endif
  // get the the frame summary timer
  nsCOMPtr<nsIAtom> frameType = nsnull;
  aFrame->GetFrameType(getter_AddRefs(frameType));
  nsReflowTimer* frameTimer = GetFrameTimer(aFrame, frameType.get());
  if (!frameTimer) {NS_ASSERTION(PR_FALSE, "no frame timer");return;}
  if (!aMetrics) { // start
#ifdef DEBUG_TABLE_REFLOW_TIMING_DETAIL
    // create the reflow timer
    nsReflowTimer* timer = new nsReflowTimer(aFrame);
    // create the aux table timers if they don't exist
    if ((nsLayoutAtoms::tableFrame == frameType.get()) && !timer->mNextSibling) {
      timer->mNextSibling = new nsReflowTimer(aFrame);
      timer->mNextSibling->mNextSibling = new nsReflowTimer(aFrame);
      timer->mNextSibling->mNextSibling->mNextSibling = new nsReflowTimer(aFrame);
      timer->mNextSibling->mNextSibling->mNextSibling->NextSibling = new nsReflowTimer(aFrame);
      timer->mNextSibling->mNextSibling->mNextSibling->NextSibling->mNextSibling = new nsReflowTimer(aFrame);
    }
    timer->mReason = aState.reason;
    timer->mAvailWidth = aState.availableWidth;
    timer->mComputedWidth = aState.mComputedWidth;
    timer->mComputedHeight = aState.mComputedHeight;
    timer->mCount = gRflCount++; 
    nsHTMLReflowCommand* reflowCommand = aState.reflowCommand;
    if (reflowCommand) {
      nsReflowType reflowType;
      reflowCommand->GetType(reflowType);
      timer->mReflowType = reflowType;
    }
    timer->Start();
    aState.mDebugHook = timer;
    if (parentTimer) {
      parentTimer->mChildren.AppendElement(timer);
    }
#endif
    // start the frame summary timer
    frameTimer->Start();
  }
  else {
#ifdef DEBUG_TABLE_REFLOW_TIMING_DETAIL
    // stop the reflow timer
    nsReflowTimer* timer = (nsReflowTimer *)aState.mDebugHook; 
    if (timer) {
      timer->Stop();
      timer->mDesiredWidth  = aMetrics->width;
      timer->mDesiredHeight = aMetrics->height;
      timer->mMaxElementWidth = (aMetrics->maxElementSize) 
        ? aMetrics->maxElementSize->width : -1;
      timer->mMaxWidth = (aMetrics->mFlags & NS_REFLOW_CALC_MAX_WIDTH) 
        ? aMetrics->mMaximumWidth : -1;
      timer->mStatus = aStatus;
    }
    else {NS_ASSERTION(PR_FALSE, "bad DebugTimeReflow");return;}
    // stop the frame summary timer
#endif
    frameTimer->Stop();
#ifdef DEBUG_TABLE_REFLOW_TIMING_DETAIL
    if (!parentTimer) {
      // print out all of the reflow timers
      DebugReflowPrint(*timer, 0, PR_FALSE);
      timer->Destroy();
    }
#endif
  }
}

void nsTableFrame::DebugTimeMethod(nsMethod           aMethod,
                                   nsTableFrame&      aFrame,
                                   nsHTMLReflowState& aState,
                                   PRBool             aStart)
{
  nsReflowTimer* baseTimer = (nsReflowTimer*)aState.mDebugHook;
  nsReflowTimer* timer;
  PRInt32 offset = aMethod;
  PRInt32 idx;
  if (aStart) {
#ifdef DEBUG_TABLE_REFLOW_TIMING_DETAIL
    timer = baseTimer;
    for (idx = 0; idx <= offset; idx++) {
      timer = timer->mNextSibling;
    }
    timer->Start();
#endif
    timer = aFrame.mTimer;
    for (idx = 0; idx <= offset; idx++) {
      timer = timer->mNextSibling;
    }
    timer->Start();
  }
  else {
#ifdef DEBUG_TABLE_REFLOW_TIMING_DETAIL
    timer = baseTimer;
    for (idx = 0; idx <= offset; idx++) {
      timer = timer->mNextSibling;
    }
    timer->Stop();
#endif
    timer = aFrame.mTimer;
    for (idx = 0; idx <= offset; idx++) {
      timer = timer->mNextSibling;
    }
    timer->Stop();
  }
}

void nsTableFrame::DebugReflowDone(nsIFrame* aFrame)
{
  // get the timer of aFrame
  nsCOMPtr<nsIAtom> frameType = nsnull;
  aFrame->GetFrameType(getter_AddRefs(frameType));
  nsReflowTimer* thisTimer = GetFrameTimer(aFrame, frameType.get());

  // get the nearest ancestor frame with a timer
  nsReflowTimer* ancestorTimer;
  nsIFrame* ancestorFrame;
  aFrame->GetParent(&ancestorFrame);
  while (ancestorFrame) {
    nsCOMPtr<nsIAtom> frameType = nsnull;
    ancestorFrame->GetFrameType(getter_AddRefs(frameType));
    ancestorTimer = GetFrameTimer(ancestorFrame, frameType.get());
    if (ancestorTimer) break;
    ancestorFrame->GetParent(&ancestorFrame);
  }
  if (ancestorTimer) { // add this timer to its parent
    ancestorTimer->mChildren.AppendElement(thisTimer);
    nsCOMPtr<nsIAtom> fType;
    aFrame->GetFrameType(getter_AddRefs(fType));
    if (IS_TABLE_CELL(fType)) {
      // add the cell block timer as a child of the cell timer
      nsTableCellFrame* cellFrame = (nsTableCellFrame*)aFrame;
      cellFrame->mTimer->mChildren.AppendElement(cellFrame->mBlockTimer);
    }
  }
  else { // print out all of the frame timers
    printf("\n\nSUMMARY OF REFLOW BY FRAME\n");
    DebugReflowPrint(*thisTimer, 0, PR_TRUE);
    thisTimer->Destroy();
  }
}

#endif //DEBUG_TABLE_REFLOW_TIMING


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

// Destructor function for nscoord properties
static void
DestroyCoordFunc(nsIPresContext* aPresContext,
                 nsIFrame*       aFrame,
                 nsIAtom*        aPropertyName,
                 void*           aPropertyValue)
{
  delete (nscoord*)aPropertyValue;
}

// Destructor function point properties
static void
DestroyPointFunc(nsIPresContext* aPresContext,
                 nsIFrame*       aFrame,
                 nsIAtom*        aPropertyName,
                 void*           aPropertyValue)
{
  delete (nsPoint*)aPropertyValue;
}

// Destructor function for nscoord properties
static void
DestroyBCPropertyDataFunc(nsIPresContext* aPresContext,
                          nsIFrame*       aFrame,
                          nsIAtom*        aPropertyName,
                          void*           aPropertyValue)
{
  delete (BCPropertyData*)aPropertyValue;
}

void*
nsTableFrame::GetProperty(nsIPresContext*      aPresContext,
                          nsIFrame*            aFrame,
                          nsIAtom*             aPropertyName,
                          PRBool               aCreateIfNecessary)
{
  nsCOMPtr<nsIPresShell>     presShell;
  aPresContext->GetShell(getter_AddRefs(presShell));

  if (presShell) {
    nsCOMPtr<nsIFrameManager>  frameManager;
    presShell->GetFrameManager(getter_AddRefs(frameManager));
  
    if (frameManager) {
      void* value;
  
      frameManager->GetFrameProperty(aFrame, aPropertyName, 0, &value);
      if (value) {
        return (nsPoint*)value;  // the property already exists

      } else if (aCreateIfNecessary) {
        // The property isn't set yet, so allocate a new value, set the property,
        // and return the newly allocated value
        void* value = nsnull;
        NSFMPropertyDtorFunc dtorFunc = nsnull;
        if (aPropertyName == nsLayoutAtoms::collapseOffsetProperty) {
          value = new nsPoint(0, 0);
          dtorFunc = DestroyPointFunc;
        }
        else if (aPropertyName == nsLayoutAtoms::rowUnpaginatedHeightProperty) {
          value = new nscoord;
          dtorFunc = DestroyCoordFunc;
        }
        else if (aPropertyName == nsLayoutAtoms::tableBCProperty) {
          value = new BCPropertyData;
          dtorFunc = DestroyBCPropertyDataFunc;
        }
        if (!value) return nsnull;

        frameManager->SetFrameProperty(aFrame, aPropertyName, value, dtorFunc);
        return value;
      }
    }
  }

  return nsnull;
}

#ifdef DEBUG
NS_IMETHODIMP
nsTableFrame::SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const
{
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  PRUint32 sum = sizeof(*this);

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

#define MAX_SIZE  128
#define MIN_INDENT 30

static 
void DumpTableFramesRecur(nsIPresContext* aPresContext,
                          nsIFrame*       aFrame,
                          PRUint32        aIndent)
{
  char indent[MAX_SIZE + 1];
  memset (indent, ' ', aIndent + MIN_INDENT);
  indent[aIndent + MIN_INDENT] = 0;

  char fName[MAX_SIZE];
  nsCOMPtr<nsIAtom> fType;
  aFrame->GetFrameType(getter_AddRefs(fType));
  GetFrameTypeName(fType, fName);

  printf("%s%s %p", indent, fName, aFrame);
  nsIFrame* flowFrame;
  aFrame->GetPrevInFlow(&flowFrame);
  if (flowFrame) {
    printf(" pif=%p", flowFrame);
  }
  aFrame->GetNextInFlow(&flowFrame);
  if (flowFrame) {
    printf(" nif=%p", flowFrame);
  }
  printf("\n");

  if (nsLayoutAtoms::tableFrame         == fType.get() ||
      nsLayoutAtoms::tableRowGroupFrame == fType.get() ||
      nsLayoutAtoms::tableRowFrame      == fType.get() ||
      IS_TABLE_CELL(fType.get())) {
    nsIFrame* child;
    aFrame->FirstChild(aPresContext, nsnull, &child);
    while(child) {
      DumpTableFramesRecur(aPresContext, child, aIndent+1);
      child->GetNextSibling(&child);
    }
  }
}
  
void
nsTableFrame::DumpTableFrames(nsIPresContext* aPresContext,
                              nsIFrame*       aFrame)
{
  nsTableFrame* tableFrame = nsnull;
  nsCOMPtr<nsIAtom> fType;
  aFrame->GetFrameType(getter_AddRefs(fType));

  if (nsLayoutAtoms::tableFrame == fType.get()) { 
    tableFrame = (nsTableFrame*)aFrame;
  }
  else {
    nsTableFrame::GetTableFrame(aFrame, tableFrame);
  }
  tableFrame = (nsTableFrame*)tableFrame->GetFirstInFlow();
  while (tableFrame) {
    DumpTableFramesRecur(aPresContext, tableFrame, 0);
    tableFrame->GetNextInFlow((nsIFrame**)&tableFrame);
  }
}

#endif




