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

static NS_DEFINE_IID(kITableRowGroupFrameIID, NS_ITABLEROWGROUPFRAME_IID);

// helper function for dealing with placeholder for positioned/floated table
static void GetPlaceholderFor(nsIPresContext& aPresContext, nsIFrame& aFrame, nsIFrame** aPlaceholder);

static const PRInt32 kColumnWidthIncrement=10;


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

  nsTableReflowState(const nsHTMLReflowState& aReflowState,
                     nsTableFrame&            aTableFrame,
                     nsReflowReason           aReason,
                     nscoord                  aAvailWidth,
                     nscoord                  aAvailHeight)
    : reflowState(aReflowState)
  {
    Init(aTableFrame, aReason, aAvailWidth, aAvailHeight);
  }

  void Init(nsTableFrame&   aTableFrame,
            nsReflowReason  aReason,
            nscoord         aAvailWidth,
            nscoord         aAvailHeight)
  {
    reason = aReason;

    nsTableFrame* table = (nsTableFrame*)aTableFrame.GetFirstInFlow();
    nsMargin borderPadding = table->GetBorderPadding(reflowState);

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

  nsTableReflowState(const nsHTMLReflowState& aReflowState,
                     nsTableFrame&            aTableFrame)
    : reflowState(aReflowState)
  {
    Init(aTableFrame, aReflowState.reason, aReflowState.availableWidth, aReflowState.availableHeight);
  }

};

nscoord 
GetHorBorderPaddingWidth(const nsHTMLReflowState& aReflowState,
                         nsTableFrame*            aTableFrame)
{
  nsMargin borderPadding = aTableFrame->GetBorderPadding(aReflowState);
  return borderPadding.left + borderPadding.right;
}

/********************************************************************************
 ** nsTableFrame                                                               **
 ********************************************************************************/
#if defined DEBUG_TABLE_REFLOW | DEBUG_TABLE_REFLOW_TIMING
static PRInt32 gRflCount = 0;
#endif

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
    mPercentBasisForRows(0),
    mPreferredWidth(0),
    mNumDescendantReflowsPending(0),
    mNumDescendantTimeoutReflowsPending(0)
{
  mBits.mHadInitialReflow       = PR_FALSE;
  mBits.mHaveReflowedColGroups  = PR_FALSE;
  mBits.mNeedStrategyInit       = PR_TRUE;
  mBits.mNeedStrategyBalance    = PR_TRUE;
  mBits.mCellSpansPctCol        = PR_FALSE;
  mBits.mRequestedTimeoutReflow = PR_FALSE;

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

  // Create the cell map
  // XXX Why do we do this for continuing frames?
  mCellMap = new nsTableCellMap(aPresContext, *this);
  if (!mCellMap) return NS_ERROR_OUT_OF_MEMORY;

  // Let the base class do its processing
  rv = nsHTMLContainerFrame::Init(aPresContext, aContent, aParent, aContext,
                                  aPrevInFlow);

  // record that children that are ignorable whitespace should be excluded 
  mState |= NS_FRAME_EXCLUDE_IGNORABLE_WHITESPACE;

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

NS_IMETHODIMP
nsTableFrame::ReflowCommandNotify(nsIPresShell*     aShell,
                                  nsIReflowCommand* aRC,
                                  PRBool            aCommandAdded)

{
  if (!aShell || !aRC) {
    NS_ASSERTION(PR_FALSE, "invalid call to ReflowCommandNotify");
    return NS_ERROR_NULL_POINTER;
  }

#ifndef TABLE_REFLOW_COALESCING_OFF
  nsIReflowCommand::ReflowType type;
  aRC->GetType(type);
  if ((type == nsIReflowCommand::ContentChanged) ||
      (type == nsIReflowCommand::StyleChanged)   ||
      (type == nsIReflowCommand::ReflowDirty)) {
    mNumDescendantReflowsPending += (aCommandAdded) ? 1 : -1;
  }
  else if (type == nsIReflowCommand::Timeout) {
    if (aCommandAdded) {
      mNumDescendantTimeoutReflowsPending++;
      if (RequestedTimeoutReflow()) {
        // since we can use the descendants timeout reflow, remove ours 
        aShell->CancelInterruptNotificationTo(this, nsIPresShell::Timeout);
        SetRequestedTimeoutReflow(PR_FALSE);
      }
    }
    else {
      mNumDescendantTimeoutReflowsPending--;
      if ((mNumDescendantTimeoutReflowsPending <= 0) && !RequestedTimeoutReflow() &&
           DescendantReflowedNotTimeout()) {
        // a descendant caused us to cancel our timeout request but we really need one 
        aShell->SendInterruptNotificationTo(this, nsIPresShell::Timeout);
        SetRequestedTimeoutReflow(PR_TRUE);
      }
    }
  }
#endif
  return NS_OK;
}

void
nsTableFrame::InterruptNotification(nsIPresContext* aPresContext,
                                    PRBool          aIsRequest)
{
  nsCOMPtr<nsIPresShell> presShell;
  aPresContext->GetShell(getter_AddRefs(presShell));
  if (aIsRequest) {
    presShell->SendInterruptNotificationTo(this, nsIPresShell::Timeout);
    SetRequestedTimeoutReflow(PR_TRUE);
  }
  else {
    presShell->CancelInterruptNotificationTo(this, nsIPresShell::Timeout);
    SetRequestedTimeoutReflow(PR_FALSE);
  }
}

nscoord 
nsTableFrame::RoundToPixel(nscoord       aValue,
                           float         aPixelToTwips,
                           nsPixelRound  aRound)
{
  nscoord halfPixel = NSToCoordRound(aPixelToTwips / 2.0f);
  nscoord fullPixel = NSToCoordRound(aPixelToTwips);
  PRInt32 excess = aValue % fullPixel;
  if (0 == excess) {
    return aValue;
  }
  else {
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

  nsIReflowCommand* reflowCmd;
  nsresult rv = NS_NewHTMLReflowCommand(&reflowCmd, aFrame,
                                        nsIReflowCommand::ReflowDirty);
  if (NS_SUCCEEDED(rv)) {
    // Add the reflow command
    rv = aPresShell->AppendReflowCommand(reflowCmd);
    NS_RELEASE(reflowCmd);
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
PRInt32 nsTableFrame::GetColCount ()
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
PRInt32 nsTableFrame::GetEffectiveColCount ()
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
#if 0
  // The RULES code below has been disabled because collapsing borders have been disabled 
  // and RULES depend on collapsing borders
  PRInt32 numCols = GetColCount();

  // process row groups
  nsIFrame* iFrame;
  for (iFrame = mFrames.FirstChild(); iFrame; iFrame->GetNextSibling(&iFrame)) {
    nsIAtom* frameType;
    iFrame->GetFrameType(&frameType);
    if (nsLayoutAtoms::tableRowGroupFrame == frameType) {
      nsTableRowGroupFrame* rgFrame = (nsTableRowGroupFrame *)iFrame;
      PRInt32 startRow = rgFrame->GetStartRowIndex();
      PRInt32 numGroupRows = rgFrame->GetRowCount();
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
            nsStyleBorder* border = (nsStyleBorder*)styleContext->GetMutableStyleData(eStyleStruct_Border);
            if (rowX == startRow) { 
              border->SetBorderStyle(NS_SIDE_BOTTOM, NS_STYLE_BORDER_STYLE_NONE);
            }
            else if (rowX == endRow) { 
              border->SetBorderStyle(NS_SIDE_TOP, NS_STYLE_BORDER_STYLE_NONE);
            }
            else {
              border->SetBorderStyle(NS_SIDE_TOP, NS_STYLE_BORDER_STYLE_NONE);
              border->SetBorderStyle(NS_SIDE_BOTTOM, NS_STYLE_BORDER_STYLE_NONE);
            }
            styleContext->RecalcAutomaticData(aPresContext);
          }
        }
      }
    }
    NS_IF_RELEASE(frameType);
  }
#endif
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
    if (numColsInCache > numColsInMap) {
      PRInt32 numColsNotRemoved = DestroyAnonymousColFrames(aPresContext, numColsInCache - numColsInMap);
      // if the cell map has fewer cols than the cache, correct it
      if (numColsNotRemoved > 0) {
        cellMap->AddColsAtEnd(numColsNotRemoved);
      }
    }
    else NS_ASSERTION(numColsInCache == numColsInMap, "cell map has too many cols");
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
    if (nsLayoutAtoms::tableCellFrame == kidType.get()) {
      nsTableCellFrame* cellFrame = (nsTableCellFrame*)kidFrame;
      stopTelling = tableFrame->CellChangedWidth(*cellFrame, cellFrame->GetPass1MaxElementSize(), 
                                                 cellFrame->GetMaximumWidth(), PR_TRUE);
    }
  }
  // XXX need to consider what happens if there are cells that have rowspans 
  // into the deleted row. Need to consider moving rows if a rebalance doesn't happen
#endif

  PRInt32 firstRowIndex = aFirstRowFrame.GetRowIndex();
  nsTableCellMap* cellMap = GetCellMap();
  if (cellMap) {
    cellMap->RemoveRows(&aPresContext, firstRowIndex, aNumRowsToRemove, aConsiderSpans);
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

/* SEC: TODO: adjust the rect for captions */
NS_METHOD 
nsTableFrame::Paint(nsIPresContext*      aPresContext,
                    nsIRenderingContext& aRenderingContext,
                    const nsRect&        aDirtyRect,
                    nsFramePaintLayer    aWhichLayer,
                    PRUint32             aFlags)
{
  // table paint code is concerned primarily with borders and bg color
  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
    const nsStyleVisibility* vis = 
      (const nsStyleVisibility*)mStyleContext->GetStyleData(eStyleStruct_Visibility);
    if (vis->IsVisibleOrCollapsed()) {
      const nsStyleBorder* border =
        (const nsStyleBorder*)mStyleContext->GetStyleData(eStyleStruct_Border);
      const nsStyleBackground* color =
        (const nsStyleBackground*)mStyleContext->GetStyleData(eStyleStruct_Background);

      nsRect  rect(0, 0, mRect.width, mRect.height);

      nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                      aDirtyRect, rect, *color, *border, 0, 0);
      
      // paint the column groups and columns
      nsIFrame* colGroupFrame = mColGroups.FirstChild();
      while (nsnull != colGroupFrame) {
        PaintChild(aPresContext, aRenderingContext, aDirtyRect, colGroupFrame, aWhichLayer);
        colGroupFrame->GetNextSibling(&colGroupFrame);
      }

      PRIntn skipSides = GetSkipSides();
      if (NS_STYLE_BORDER_SEPARATE == GetBorderCollapseStyle()) {
        nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                    aDirtyRect, rect, *border, mStyleContext, skipSides);
      }
      else {
        // tbd
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

// this assumes that mNumDescendantReflowsPending and mNumDescendantTimeoutReflowsPending
// have already been updated for the current reflow
PRBool nsTableFrame::NeedsReflow(const nsHTMLReflowState& aReflowState)
{
  PRBool result = PR_TRUE;
  if ((eReflowReason_Incremental == aReflowState.reason) &&
      (NS_UNCONSTRAINEDSIZE == aReflowState.availableHeight)) {
    // It's an incremental reflow and we're in galley mode. Only
    // do a full reflow if we need to.
#ifndef TABLE_REFLOW_COALESCING_OFF
    nsIFrame* reflowTarget;
    aReflowState.reflowCommand->GetTarget(reflowTarget);
    nsIReflowCommand::ReflowType reflowType;
    aReflowState.reflowCommand->GetType(reflowType);
    if (reflowType == nsIReflowCommand::Timeout) {
      result = PR_FALSE;
      if (this == reflowTarget) {
        if (mNumDescendantTimeoutReflowsPending <= 0) {
          // no more timeout reflows are coming targeted below
          result = NeedStrategyInit() || NeedStrategyBalance();
        }
      }
      else if ((mNumDescendantTimeoutReflowsPending <= 0) && !RequestedTimeoutReflow()) {
        // no more timeout reflows are coming targeted below or here
        result = NeedStrategyInit() || NeedStrategyBalance();
      }
    }
    else {
      if ((mNumDescendantTimeoutReflowsPending <= 0) &&
          !RequestedTimeoutReflow() &&
          (mNumDescendantReflowsPending <= 0)) { 
        // no more timeout reflows are coming targeted below or here, 
        // and no more normal reflows are coming targeted below
        result = NeedStrategyInit() || NeedStrategyBalance();
      }
      else result = PR_FALSE;
    }
#else
    result = NeedStrategyInit() || NeedStrategyBalance();
#endif
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

            nscoord tableY = damageRect.y;
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

/* overview:
  if mFirstPassValid is false, this is our first time through since content was last changed
    do pass 1
      get min/max info for all cells in an infinite space
  do column balancing
  do pass 2
  use column widths to size table and ResizeReflow rowgroups (and therefore rows and cells)
*/

/* Layout the entire inner table. */
NS_METHOD nsTableFrame::Reflow(nsIPresContext*          aPresContext,
                               nsHTMLReflowMetrics&     aDesiredSize,
                               const nsHTMLReflowState& aReflowState,
                               nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsTableFrame", aReflowState.reason);
  DISPLAY_REFLOW(this, aReflowState, aDesiredSize, aStatus);
#if defined DEBUG_TABLE_REFLOW_TIMING
  nsTableFrame::DebugReflow(this, (nsHTMLReflowState&)aReflowState);
#endif

  aStatus = NS_FRAME_COMPLETE; 
  if (!mPrevInFlow && !mTableLayoutStrategy) {
    NS_ASSERTION(PR_FALSE, "strategy should have been created in Init");
    return NS_ERROR_NULL_POINTER;
  }
  nsresult rv = NS_OK;

  PRBool isPaginated;
  aPresContext->IsPaginated(&isPaginated);
 
  PRBool doCollapse = PR_FALSE;

  ComputePercentBasisForRows(aReflowState);

  aDesiredSize.width = aReflowState.availableWidth;

  nsReflowReason nextReason = aReflowState.reason;

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
        // Check for an overflow list, and append any row group frames being pushed
        MoveOverflowToChildList(aPresContext);

        if (!mPrevInFlow) { // only do pass1 on a first in flow
          if (IsAutoLayout()) {     
            // only do pass1 reflow on an auto layout table
            nsReflowReason reason = (eReflowReason_Initial == aReflowState.reason)
                                    ? eReflowReason_Initial : eReflowReason_StyleChange;
            nsTableReflowState reflowState(aReflowState, *this, reason,
                                           NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE); 
            // reflow the children
            ReflowChildren(aPresContext, reflowState, !HaveReflowedColGroups(), PR_FALSE, aStatus);
          }
          mTableLayoutStrategy->Initialize(aPresContext, aReflowState);
        }
      }
      if (!mPrevInFlow) {
        SetHadInitialReflow(PR_TRUE);
        SetNeedStrategyBalance(PR_TRUE); // force a balance and then a pass2 reflow 
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
      NS_ASSERTION(NS_UNCONSTRAINEDSIZE != aReflowState.availableWidth, "this doesn't do anything");
      SetNeedStrategyBalance(PR_TRUE); 
      break; 
    default:
      break;
  }

  if (NS_FAILED(rv)) return rv;

  PRBool needPass3Reflow = PR_FALSE;
  PRBool balanced = PR_FALSE;

  // Reflow the entire table. This phase is necessary during a constrained initial reflow 
  // and other reflows which require either a strategy init or balance. This isn't done 
  // during an unconstrained reflow because another reflow will be processed later.
  if (NeedsReflow(aReflowState) && (NS_UNCONSTRAINEDSIZE != aReflowState.availableWidth)) {
    // see if an extra (3rd) reflow will be necessary in pagination mode when there is a specified table height 
    if (isPaginated && !mPrevInFlow && (NS_UNCONSTRAINEDSIZE != aReflowState.availableHeight)) {
      nscoord tableSpecifiedHeight = CalcBorderBoxHeight(aReflowState);
      if ((tableSpecifiedHeight > 0) && 
          (tableSpecifiedHeight != NS_UNCONSTRAINEDSIZE)) {
        needPass3Reflow = PR_TRUE;
      }
    }
    // if we need to reflow the table an extra time, then don't constrain the height of the previous reflow
    nscoord availHeight = (needPass3Reflow) ? NS_UNCONSTRAINEDSIZE : aReflowState.availableHeight;

    ReflowTable(aPresContext, aDesiredSize, aReflowState, availHeight, nextReason, doCollapse, balanced, aStatus);

    if (needPass3Reflow) {
      aDesiredSize.height = CalcDesiredHeight(aPresContext, aReflowState); // distributes extra vertical space to rows
      SetThirdPassReflow(PR_TRUE); // set it and leave it set for frames that may split
      ReflowTable(aPresContext, aDesiredSize, aReflowState, aReflowState.availableHeight, 
                  nextReason, doCollapse, balanced, aStatus);
    }
  }

  aDesiredSize.width  = GetDesiredWidth();
  if (!needPass3Reflow) {
    aDesiredSize.height = CalcDesiredHeight(aPresContext, aReflowState); 
  }

  if (IsRowInserted()) {
    ProcessRowInserted(aPresContext, *this, PR_TRUE, aDesiredSize.height);
  }

#ifndef TABLE_REFLOW_COALESCING_OFF
  // determine if we need to reset DescendantReflowedNotTimeout and/or
  // RequestedTimeoutReflow after a timeout reflow.
  if (aReflowState.reflowCommand) {
    nsIReflowCommand::ReflowType type;
    aReflowState.reflowCommand->GetType(type);
    if (nsIReflowCommand::Timeout == type) {
      nsIFrame* target = nsnull;
      aReflowState.reflowCommand->GetTarget(target);
      if (target == this) { // target is me
        NS_ASSERTION(0 == mNumDescendantTimeoutReflowsPending, "was incorrectly target of timeout reflow"); 
        SetDescendantReflowedNotTimeout(PR_FALSE);
        SetRequestedTimeoutReflow(PR_FALSE);
      }
      else if (target) {    // target is descendant        
        if (0 >= mNumDescendantTimeoutReflowsPending) {
          NS_ASSERTION(!RequestedTimeoutReflow(), "invalid timeout request");
          SetDescendantReflowedNotTimeout(PR_FALSE);
        }
      }
    }
  }
#endif

  SetColumnDimensions(aPresContext, aDesiredSize.height, aReflowState.mComputedBorderPadding);
  if (doCollapse) {
    AdjustForCollapsingRows(aPresContext, aDesiredSize.height);
    AdjustForCollapsingCols(aPresContext, aDesiredSize.width);
  }

  // See if we are supposed to return our max element size
  if (aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width  = GetMinWidth();
    aDesiredSize.maxElementSize->height = 0;
  }
  // See if we are supposed to compute our maximum width
  if (aDesiredSize.mFlags & NS_REFLOW_CALC_MAX_WIDTH) {
    aDesiredSize.mMaximumWidth = GetPreferredWidth();
    if (!mPrevInFlow && HasPctCol() && IsAutoWidth() && !balanced) {
      nscoord minWidth;
      CalcMinAndPreferredWidths(aPresContext, aReflowState, PR_TRUE, minWidth, aDesiredSize.mMaximumWidth);
      SetPreferredWidth(aDesiredSize.mMaximumWidth);
    }
  }

#if defined DEBUG_TABLE_REFLOW_TIMING
  nsTableFrame::DebugReflow(this, (nsHTMLReflowState&)aReflowState, &aDesiredSize, aStatus);
#endif
  return rv;
}

nsresult 
nsTableFrame::ReflowTable(nsIPresContext*          aPresContext,
                          nsHTMLReflowMetrics&     aDesiredSize,
                          const nsHTMLReflowState& aReflowState,
                          nscoord                  aAvailHeight,
                          nsReflowReason           aReason,
                          PRBool&                  aDoCollapse,
                          PRBool&                  aDidBalance,
                          nsReflowStatus&          aStatus)
{
  nsresult rv = NS_OK;
  aDoCollapse = PR_FALSE;
  aDidBalance = PR_FALSE;

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
  nsTableReflowState reflowState(aReflowState, *this, aReason, 
                                 aDesiredSize.width, aAvailHeight);
  ReflowChildren(aPresContext, reflowState, haveReflowedColGroups, PR_FALSE, aStatus);

  // If we're here that means we had to reflow all the rows, e.g., the column widths 
  // changed. We need to make sure that any damaged areas are repainted
  Invalidate(aPresContext, mRect);

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


void nsTableFrame::ComputePercentBasisForRows(const nsHTMLReflowState& aReflowState)
{
  nscoord height = CalcBorderBoxHeight(aReflowState);
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
    // get the starting row index of the new rows and insert them into the table
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
      nsTableColFrame* colFrame = (nsTableColFrame*)mColFrames.ElementAt(colX);
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
  nsresult  rv = NS_OK;

  // Constrain our reflow width to the computed table width. Note: this is
  // based on the width of the first-in-flow
  PRInt32 lastWidth = mRect.width;
  if (mPrevInFlow) {
    nsTableFrame* table = (nsTableFrame*)GetFirstInFlow();
    lastWidth = table->mRect.width;
  }
  nsTableReflowState state(aReflowState, *this, eReflowReason_Incremental,
                           lastWidth, aReflowState.availableHeight); 

  // determine if this frame is the target or not
  nsIFrame* target = nsnull;
  rv = aReflowState.reflowCommand->GetTarget(target);
  if (NS_SUCCEEDED(rv) && target) {
    nsIReflowCommand::ReflowType type;
    aReflowState.reflowCommand->GetType(type);
    // this is the target if target is either this or the outer table frame containing this inner frame
    nsIFrame* outerTableFrame = nsnull;
    GetParent(&outerTableFrame);
    if ((this == target) || (outerTableFrame == target)) {
      rv = IR_TargetIsMe(aPresContext, state, aStatus);
    }
    else {
      // Get the next frame in the reflow chain
      nsIFrame* nextFrame;
      aReflowState.reflowCommand->GetNext(nextFrame);
      if (nextFrame) {
        rv = IR_TargetIsChild(aPresContext, state, aStatus, nextFrame);
      }
      else {
        NS_ASSERTION(PR_FALSE, "next frame in reflow command is null"); 
      }
    }
  }
  return rv;
}

NS_METHOD 
nsTableFrame::IR_TargetIsMe(nsIPresContext*      aPresContext,
                            nsTableReflowState&  aReflowState,
                            nsReflowStatus&      aStatus)
{
  nsresult rv = NS_OK;
  aStatus = NS_FRAME_COMPLETE;

  nsIReflowCommand::ReflowType type;
  aReflowState.reflowState.reflowCommand->GetType(type);

  switch (type) {
    case nsIReflowCommand::StyleChanged :
      rv = IR_StyleChanged(aPresContext, aReflowState, aStatus);
      break;
    case nsIReflowCommand::ContentChanged :
      NS_ASSERTION(PR_FALSE, "illegal reflow type: ContentChanged");
      rv = NS_ERROR_ILLEGAL_VALUE;
      break;
    case nsIReflowCommand::ReflowDirty: {
      // reflow the dirty children
      nsTableReflowState reflowState(aReflowState.reflowState, *this, eReflowReason_Initial,
                                     aReflowState.availSize.width, aReflowState.availSize.height); 
      PRBool reflowedAtLeastOne; 
      ReflowChildren(aPresContext, reflowState, PR_FALSE, PR_TRUE, aStatus, &reflowedAtLeastOne);
      if (!reflowedAtLeastOne)
        // XXX For now assume the worse
        SetNeedStrategyInit(PR_TRUE);
      }
      break;
    case nsIReflowCommand::Timeout: {
      // for a timeout reflow, don't do anything here
      break;
    }
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

nsMargin 
nsTableFrame::GetBorderPadding(const nsHTMLReflowState& aReflowState) const
{
  const nsStyleBorder* border =
    (const nsStyleBorder*)mStyleContext->GetStyleData(eStyleStruct_Border);
  nsMargin borderPadding;
  border->GetBorder(borderPadding);
  borderPadding += aReflowState.mComputedPadding;
  return borderPadding;
}

// Recovers the reflow state to what it should be if aKidFrame is about to be 
// reflowed. Restores y, footerFrame, firstBodySection and availSize.height (if
// the height is constrained)
nsresult
nsTableFrame::RecoverState(nsTableReflowState& aReflowState,
                           nsIFrame*           aKidFrame)
{
  nsMargin borderPadding = GetBorderPadding(aReflowState.reflowState);
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

NS_METHOD 
nsTableFrame::IR_TargetIsChild(nsIPresContext*      aPresContext,
                               nsTableReflowState&  aReflowState,
                               nsReflowStatus&      aStatus,
                               nsIFrame*            aNextFrame)

{
  nsresult rv;
  // Recover the state as if aNextFrame is about to be reflowed
  RecoverState(aReflowState, aNextFrame);

  // Remember the old rect
  nsRect  oldKidRect;
  aNextFrame->GetRect(oldKidRect);

  // Pass along the reflow command, don't request a max element size, rows will do that
  nsHTMLReflowMetrics desiredSize(nsnull);
  nsHTMLReflowState kidReflowState(aPresContext, aReflowState.reflowState,
                                   aNextFrame, aReflowState.availSize);

  rv = ReflowChild(aNextFrame, aPresContext, desiredSize, kidReflowState,
                   aReflowState.x, aReflowState.y, 0, aStatus);

  // Place the row group frame. Don't use PlaceChild(), because it moves
  // the footer frame as well. We'll adjust the footer frame later on in
  // AdjustSiblingsAfterReflow()
  nsRect  kidRect(aReflowState.x, aReflowState.y, desiredSize.width, desiredSize.height);
  FinishReflowChild(aNextFrame, aPresContext, desiredSize, aReflowState.x, aReflowState.y, 0);

#ifndef TABLE_REFLOW_COALESCING_OFF
  // update the descendant reflow counts and determine if we need to request a timeout reflow
  PRBool needToRequestTimeoutReflow = PR_FALSE;
  nsIReflowCommand::ReflowType type;
  aReflowState.reflowState.reflowCommand->GetType(type);
  if (nsIReflowCommand::Timeout == type) {
    mNumDescendantTimeoutReflowsPending--;
    NS_ASSERTION(mNumDescendantTimeoutReflowsPending >= 0, "invalid descendant reflow count");
  }
  else {
    mNumDescendantReflowsPending--;
    NS_ASSERTION(mNumDescendantReflowsPending >= 0, "invalid descendant reflow count");
    SetDescendantReflowedNotTimeout(PR_TRUE);
    // request a timeout reflow if there are no more timeout reflows coming, more normal
    // reflows are coming, and we haven't already requested one.
    if ((mNumDescendantTimeoutReflowsPending <= 0) &&
        (mNumDescendantReflowsPending > 0) && 
        !RequestedTimeoutReflow()) {
      InterruptNotification(aPresContext, PR_TRUE);
    }
    // cancel our timeout reflow (if present) if there are no more coming and
    // this is the last child to be reflowed.
    else if ((mNumDescendantTimeoutReflowsPending <= 0) &&
             (mNumDescendantReflowsPending <= 0) && 
             RequestedTimeoutReflow()) {
      InterruptNotification(aPresContext, PR_FALSE);
    }
  }
#endif

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
  FinishReflowChild(aKidFrame, aPresContext, aDesiredSize, aReflowState.x, aReflowState.y, 0);

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
                             nsTableRowGroupFrame** aFoot)
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
nsTableFrame::ReflowChildren(nsIPresContext*      aPresContext,
                             nsTableReflowState&  aReflowState,
                             PRBool               aDoColGroups,
                             PRBool               aDirtyOnly,
                             nsReflowStatus&      aStatus,
                             PRBool*              aReflowedAtLeastOne)
{
  aStatus = NS_FRAME_COMPLETE;

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
        nsHTMLReflowState  kidReflowState(aPresContext, aReflowState.reflowState,
                                          kidFrame, kidAvailSize, aReflowState.reason);
        // XXX fix up bad mComputedWidth for scroll frame
        kidReflowState.mComputedWidth = PR_MAX(kidReflowState.mComputedWidth, 0);
  
        aReflowState.y += cellSpacingY;
        
        // record the next in flow in case it gets destroyed and the row group array
        // needs to be recomputed.
        nsIFrame* kidNextInFlow;
        kidFrame->GetNextInFlow(&kidNextInFlow);
  
        rv = ReflowChild(kidFrame, aPresContext, desiredSize, kidReflowState,
                         aReflowState.x, aReflowState.y, 0, aStatus);
        haveReflowedRowGroup = PR_TRUE;

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
            nsHTMLReflowState footerReflowState(aPresContext, aReflowState.reflowState,
                                                repeatedFooter, kidAvailSize, aReflowState.reason);
            aReflowState.y += cellSpacingY;
            nsReflowStatus footerStatus;
            rv = ReflowChild(repeatedFooter, aPresContext, desiredSize, footerReflowState,
                             aReflowState.x, aReflowState.y, 0, footerStatus);
            PlaceChild(aPresContext, aReflowState, repeatedFooter, desiredSize);
          }
          break;
        }
        else if (kidNextInFlow) {
          // during printing, the unfortunate situation arises where a next in flow can be a
          // next sibling and the next sibling can get destroyed during the reflow. By reordering
          // the row groups, the rowGroups array can be kept in sync.
          OrderRowGroups(rowGroups, numRowGroups, nsnull);
        }
      }
      else { // it's an unknown frame type, give it a generic reflow and ignore the results
        nsHTMLReflowState kidReflowState(aPresContext, aReflowState.reflowState, kidFrame,
                                         nsSize(0,0), eReflowReason_Resize);
        nsHTMLReflowMetrics unusedDesiredSize(nsnull);
        nsReflowStatus status;
        ReflowChild(kidFrame, aPresContext, unusedDesiredSize, kidReflowState,
                    0, 0, 0, status);
        kidFrame->DidReflow(aPresContext, NS_FRAME_REFLOW_FINISHED);
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
      FinishReflowChild(kidFrame, aPresContext, kidMet, 0, 0, 0);
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
  nscoord desWidth = CalcDesiredWidth(aReflowState);
  SetDesiredWidth(desWidth);          
  SetPreferredWidth(prefWidth); 

#ifdef DEBUG_TABLE_REFLOW
  printf("Balanced min=%d des=%d pref=%d cols=", minWidth, desWidth, prefWidth);
  for (PRInt32 colX = 0; colX < GetColCount(); colX++) {
    printf("%d ", GetColumnWidth(colX));
  }
  printf("\n");
#endif

}

// This width is based on the column widths array of the table.
// sum the width of each column and add in table insets
nscoord 
nsTableFrame::CalcDesiredWidth(const nsHTMLReflowState& aReflowState)
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
    // Compute the insets (sum of border and padding)
    tableWidth += GetHorBorderPaddingWidth(aReflowState, this);
  } 

  return tableWidth;
}

/**
  get the table height attribute
  if it is auto, table height = SUM(height of rowgroups)
  else if (resolved table height attribute > SUM(height of rowgroups))
    proportionately distribute extra height to each row
  we assume we are passed in the default table height==the sum of the heights of the table's rowgroups
  in aDesiredSize.height.
  */
void 
nsTableFrame::DistributeSpaceToCells(nsIPresContext*          aPresContext, 
                                     const nsHTMLReflowState& aReflowState,
                                     nsIFrame*                aRowGroupFrame)
{
  // now that all of the rows have been resized, resize the cells       
  nsTableRowGroupFrame* rowGroupFrame = (nsTableRowGroupFrame*)aRowGroupFrame;
  nsIFrame * rowFrame = rowGroupFrame->GetFirstFrame();
  while (rowFrame) {
    const nsStyleDisplay *rowDisplay;
    rowFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)rowDisplay));
    if (NS_STYLE_DISPLAY_TABLE_ROW == rowDisplay->mDisplay) { 
      ((nsTableRowFrame *)rowFrame)->DidResize(aPresContext, aReflowState);
    }
    rowGroupFrame->GetNextFrame(rowFrame, &rowFrame);
  }
}

void 
nsTableFrame::DistributeSpaceToRows(nsIPresContext*          aPresContext,
                                    const nsHTMLReflowState& aReflowState,
                                    nsIFrame*                aRowGroupFrame, 
                                    nscoord                  aSumOfRowHeights,
                                    nscoord                  aExcess, 
                                    nscoord&                 aExcessAllocated,
                                    nscoord&                 aRowGroupYPos)
{
  // the rows in rowGroupFrame need to be expanded by rowHeightDelta[i]
  // and the rowgroup itself needs to be expanded by SUM(row height deltas)
  nscoord cellSpacingY = GetCellSpacingY();
  nsTableRowGroupFrame* rowGroupFrame = (nsTableRowGroupFrame*)aRowGroupFrame;
  nsIFrame* rowFrame = rowGroupFrame->GetFirstFrame();
  nscoord excessForRowGroup = 0;
  nscoord y = 0;
  float p2t;
  aPresContext->GetPixelsToTwips(&p2t);
  while (rowFrame) {
    const nsStyleDisplay *rowDisplay;
    rowFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)rowDisplay));
    if (NS_STYLE_DISPLAY_TABLE_ROW == rowDisplay->mDisplay) { 
      // the row needs to be expanded by the proportion this row contributed to the original height
      nsRect rowRect;
      rowFrame->GetRect(rowRect);
      float percent = ((float)(rowRect.height)) / (float)aSumOfRowHeights;
      nscoord excessForRow = RoundToPixel(NSToCoordRound((float)aExcess * percent), p2t);
      excessForRow = PR_MIN(excessForRow, aExcess - aExcessAllocated);

      nsRect newRowRect(rowRect.x, y, rowRect.width, excessForRow + rowRect.height);
      rowFrame->SetRect(aPresContext, newRowRect);
      y = newRowRect.YMost() + cellSpacingY;

      aExcessAllocated  += excessForRow;
      excessForRowGroup += excessForRow;
    }

    rowGroupFrame->GetNextFrame(rowFrame, &rowFrame);
  }

  nsRect rowGroupRect;
  aRowGroupFrame->GetRect(rowGroupRect);  
  nsRect newRowGroupRect(rowGroupRect.x, aRowGroupYPos, rowGroupRect.width, 
                         excessForRowGroup + rowGroupRect.height);
  aRowGroupFrame->SetRect(aPresContext, newRowGroupRect);
  aRowGroupYPos = newRowGroupRect.YMost();

  DistributeSpaceToCells(aPresContext, aReflowState, aRowGroupFrame);
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
  nsMargin borderPadding = GetBorderPadding(aReflowState);

  // get the natural height based on the last child's (row group or scroll frame) rect
  nsAutoVoidArray rowGroups;
  PRUint32 numRowGroups;
  OrderRowGroups(rowGroups, numRowGroups, nsnull);
  if (numRowGroups <= 0) return 0;

  nscoord naturalHeight = borderPadding.top + cellSpacingY + borderPadding.bottom;
  for (PRUint32 rgX = 0; rgX < numRowGroups; rgX++) {
    nsIFrame* rg = (nsIFrame*)rowGroups.ElementAt(rgX);
    if (rg) {
      nsRect rgRect;
      rg->GetRect(rgRect);
      naturalHeight += rgRect.height + cellSpacingY;
    }
  }

  nscoord desiredHeight = naturalHeight;

  // see if a specified table height requires dividing additional space to rows
  if (!mPrevInFlow) {
    nscoord tableSpecifiedHeight = CalcBorderBoxHeight(aReflowState);
    if ((tableSpecifiedHeight > 0) && 
        (tableSpecifiedHeight != NS_UNCONSTRAINEDSIZE) &&
        (tableSpecifiedHeight > naturalHeight)) {
      desiredHeight = tableSpecifiedHeight;

      if (NS_UNCONSTRAINEDSIZE != aReflowState.availableWidth) { 
        // proportionately distribute the excess height to each row. Note that we
        // don't need to do this if it's an unconstrained reflow
        nscoord excess = tableSpecifiedHeight - naturalHeight;
        nscoord sumOfRowHeights = 0;
        nscoord rowGroupYPos = aReflowState.mComputedBorderPadding.top + cellSpacingY;

        nsAutoVoidArray rowGroups;
        PRUint32 numRowGroups;
        OrderRowGroups(rowGroups, numRowGroups, nsnull);

        PRUint32 rgX;
        for (rgX = 0; rgX < numRowGroups; rgX++) {
          nsTableRowGroupFrame* rgFrame = 
            GetRowGroupFrame((nsIFrame*)rowGroups.ElementAt(rgX));
          if (rgFrame) {
            sumOfRowHeights += rgFrame->GetHeightOfRows(aPresContext);
          }
        }
        nscoord excessAllocated = 0;
        for (rgX = 0; rgX < numRowGroups; rgX++) {
          nsTableRowGroupFrame* rgFrame = 
          GetRowGroupFrame((nsIFrame*)rowGroups.ElementAt(rgX));
          if (rgFrame) {
            DistributeSpaceToRows(aPresContext, aReflowState, rgFrame, sumOfRowHeights, 
                                  excess, excessAllocated, rowGroupYPos);

            // Make sure child views are properly positioned
            // XXX what happens if childFrame is a scroll frame and this gets skipped?
            nsTableFrame::RePositionViews(aPresContext, rgFrame);
          }
          rowGroupYPos += cellSpacingY;
        }
      }
    }
  }
  return desiredHeight;
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

  nscoord cellMin = (aCellWasDestroyed) ? 0 : aCellFrame.GetPass1MaxElementSize().width;
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
          minWidth = PR_MAX(minWidth, cellFrame->GetPass1MaxElementSize().width);
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
    else {
      NS_ASSERTION(PR_FALSE, "null col frame");
      result = 0;
    }
#if 0
    nsTableCellMap* cellMap = GetCellMap();
    if (!cellMap) {
      NS_ASSERTION(PR_FALSE, "no cell map");
      return 0;
    }
    PRInt32 numCols = cellMap->GetColCount();
    NS_ASSERTION (numCols > aColIndex, "bad arg, col index out of bounds");
#endif
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

// Update the border style to map to the HTML border style
void nsTableFrame::MapHTMLBorderStyle(nsStyleBorder& aBorderStyle, nscoord aBorderWidth)
{
  nsStyleCoord  width;
  width.SetCoordValue(aBorderWidth);
  aBorderStyle.mBorder.SetTop(width);
  aBorderStyle.mBorder.SetLeft(width);
  aBorderStyle.mBorder.SetBottom(width);
  aBorderStyle.mBorder.SetRight(width);

  aBorderStyle.SetBorderStyle(NS_SIDE_TOP, NS_STYLE_BORDER_STYLE_BG_OUTSET);
  aBorderStyle.SetBorderStyle(NS_SIDE_LEFT, NS_STYLE_BORDER_STYLE_BG_OUTSET);
  aBorderStyle.SetBorderStyle(NS_SIDE_BOTTOM, NS_STYLE_BORDER_STYLE_BG_OUTSET);
  aBorderStyle.SetBorderStyle(NS_SIDE_RIGHT, NS_STYLE_BORDER_STYLE_BG_OUTSET);

  nsIStyleContext* styleContext = mStyleContext; 
  const nsStyleBackground* colorData = (const nsStyleBackground*)
    styleContext->GetStyleData(eStyleStruct_Background);

  // Look until we find a style context with a NON-transparent background color
  while (styleContext) {
    if ((colorData->mBackgroundFlags & NS_STYLE_BG_COLOR_TRANSPARENT) != 0) {
      nsIStyleContext* temp = styleContext;
      styleContext = styleContext->GetParent();
      if (temp != mStyleContext)
        NS_RELEASE(temp);
      colorData = (const nsStyleBackground*)styleContext->GetStyleData(eStyleStruct_Background);
    }
    else {
      break;
    }
  }

  // Yaahoo, we found a style context which has a background color 
  
  nscolor borderColor = 0xFFC0C0C0;

  if (styleContext) {
    borderColor = colorData->mBackgroundColor;
    if (styleContext != mStyleContext)
      NS_RELEASE(styleContext);
  }

  // if the border color is white, then shift to grey
  if (borderColor == 0xFFFFFFFF)
    borderColor = 0xFFC0C0C0;

  aBorderStyle.SetBorderColor(NS_SIDE_TOP, borderColor);
  aBorderStyle.SetBorderColor(NS_SIDE_LEFT, borderColor);
  aBorderStyle.SetBorderColor(NS_SIDE_BOTTOM, borderColor);
  aBorderStyle.SetBorderColor(NS_SIDE_RIGHT, borderColor);

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
  const nsStyleTableBorder* tableStyle;
  GetStyleData(eStyleStruct_TableBorder, (const nsStyleStruct *&)tableStyle);
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
  const nsStyleTableBorder* tableStyle;
  GetStyleData(eStyleStruct_TableBorder, (const nsStyleStruct *&)tableStyle);
  nscoord cellSpacing = 0;
  PRUint8 borderCollapseStyle = GetBorderCollapseStyle();
  if (NS_STYLE_BORDER_COLLAPSE != borderCollapseStyle) {
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
  nsIFrame* parentFrame=nsnull;
  if (aSourceFrame) {
    // "result" is the result of intermediate calls, not the result we return from this method
    nsresult result = aSourceFrame->GetParent((nsIFrame **)&parentFrame); 
    while ((NS_OK==result) && (nsnull!=parentFrame)) {
      const nsStyleDisplay *display;
      parentFrame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct *&)display);
      if (NS_STYLE_DISPLAY_TABLE == display->mDisplay) {
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
    nsMargin borderPadding = aState.mComputedBorderPadding;
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
nsTableFrame::CalcBorderBoxHeight(const nsHTMLReflowState& aState)
{
  nscoord height = aState.mComputedHeight;
  if (NS_AUTOHEIGHT != height) {
    nsMargin borderPadding = aState.mComputedBorderPadding;
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
nsTableFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("Table", aResult);
}
#endif


void 
nsTableFrame::CalcMinAndPreferredWidths(nsIPresContext* aPresContext,
                                        const           nsHTMLReflowState& aReflowState,
                                        PRBool          aCalcPrefWidthIfAutoWithPctCol,
                                        nscoord&        aMinWidth,
                                        nscoord&        aPrefWidth) 
{
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
    nscoord extra = spacingX + GetHorBorderPaddingWidth(aReflowState, this);
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
        aPrefWidth = mTableLayoutStrategy->CalcPctAdjTableWidth(aReflowState, availWidth, p2t);
      }
    }
  }
  else { // a specified fix width becomes the min or preferred width
    nscoord compWidth = aReflowState.mComputedWidth;
    if ((NS_UNCONSTRAINEDSIZE != compWidth) && (0 != compWidth) && !isPctWidth) {
      compWidth += GetHorBorderPaddingWidth(aReflowState, this);
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
  return cellMap->GetNumCellsOriginatingInCol(aColIndex);
}



/********************************************************************************
 ** DEBUG_TABLE_REFLOW  and  DEBUG_TABLE_REFLOW_TIMING                         **
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
  else if (nsLayoutAtoms::tableCellFrame == aFrameType) 
    strcpy(aName, "Cell");
  else if (nsLayoutAtoms::blockFrame == aFrameType) 
    strcpy(aName, "Block");
  else 
    NS_ASSERTION(PR_FALSE, "invalid call to GetFrameTypeName");

  return isTable;
}
#endif

#if defined DEBUG_TABLE_REFLOW | DEBUG_TABLE_REFLOW_TIMING

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


#ifdef DEBUG_TABLE_REFLOW

void DebugGetIndent(const nsIFrame* aFrame, 
                    char*           aBuf)
{
  PRInt32 numLevels = 0;
  nsIFrame* parent = nsnull;
  aFrame->GetParent(&parent);
  while (parent) {
    nsCOMPtr<nsIAtom> frameType;
    parent->GetFrameType(getter_AddRefs(frameType));
    if ((nsLayoutAtoms::tableOuterFrame    == frameType.get()) ||
        (nsLayoutAtoms::tableFrame         == frameType.get()) ||
        (nsLayoutAtoms::tableRowGroupFrame == frameType.get()) ||
        (nsLayoutAtoms::tableRowFrame      == frameType.get()) ||
        (nsLayoutAtoms::tableCellFrame     == frameType.get())) {
      numLevels++;
    }
    if (nsLayoutAtoms::blockFrame == frameType.get()) {
      // only count blocks that are children of cells
      nsIFrame* grandParent;
      parent->GetParent(&grandParent);
      nsCOMPtr<nsIAtom> gFrameType;
      grandParent->GetFrameType(getter_AddRefs(gFrameType));
      if (nsLayoutAtoms::tableCellFrame == gFrameType.get()) {
        numLevels++;
      }
    }
    parent->GetParent(&parent);
  }
  PRInt32 indent = INDENT_PER_LEVEL * numLevels;
  nsCRT::memset (aBuf, ' ', indent);
  aBuf[indent] = 0;
}

void nsTableFrame::DebugReflow(nsIFrame*            aFrame,
                               nsHTMLReflowState&   aState,
                               nsHTMLReflowMetrics* aMetrics,
                               nsReflowStatus       aStatus)
{
  // get the frame type
  nsCOMPtr<nsIAtom> fType;
  aFrame->GetFrameType(getter_AddRefs(fType));
  char fName[128];
  GetFrameTypeName(fType.get(), fName);

  char indent[256];
  DebugGetIndent(aFrame, indent);
  printf("%s%s %p ", indent, fName, aFrame);
  char width[16];
  char height[16];
  if (!aMetrics) { // start
    PrettyUC(aState.availableWidth, width);
    PrettyUC(aState.availableHeight, height);
    printf("r=%d a=%s,%s ", aState.reason, width, height); 
    PrettyUC(aState.mComputedWidth, width);
    PrettyUC(aState.mComputedHeight, height);
    printf("c=%s,%s ", width, height);
    nsIFrame* inFlow;
    aFrame->GetPrevInFlow(&inFlow);
    if (inFlow) {
      printf("pif=%p ", inFlow);
    }
    aFrame->GetNextInFlow(&inFlow);
    if (inFlow) {
      printf("nif=%p ", inFlow);
    }
    printf("cnt=%d \n", gRflCount);
    gRflCount++;
    //if (32 == gRflCount) {
    //  NS_ASSERTION(PR_FALSE, "stop");
    //}
  }
  if (aMetrics) { // stop
    PrettyUC(aMetrics->width, width);
    PrettyUC(aMetrics->height, height);
    printf("d=%s,%s ", width, height);
    if (aMetrics->maxElementSize) {
      PrettyUC(aMetrics->maxElementSize->width, width);
      printf("me=%s ", width);
    }
    if (aMetrics->mFlags & NS_REFLOW_CALC_MAX_WIDTH) {
      PrettyUC(aMetrics->mMaximumWidth, width);
      printf("m=%s ", width);
    }
    if (NS_FRAME_COMPLETE != aStatus) {
      printf("status=%d", aStatus);
    }
    printf("\n");
  }
}

#else

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
  else if (nsLayoutAtoms::tableCellFrame == aFrameType) 
    return ((nsTableCellFrame*)aFrame)->mTimer;
  else if (nsLayoutAtoms::blockFrame == aFrameType) { 
    nsIFrame* parentFrame;
    aFrame->GetParent(&parentFrame);
    nsCOMPtr<nsIAtom> fType;
    parentFrame->GetFrameType(getter_AddRefs(fType));
    if (nsLayoutAtoms::tableCellFrame == fType) {
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
  nsCRT::memset (indentChar, ' ', indent);
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
    if (NS_FRAME_COMPLETE != aTimer.mStatus) {
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
    nsIReflowCommand* reflowCommand = aState.reflowCommand;
    if (reflowCommand) {
      nsIReflowCommand::ReflowType reflowType;
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
    if (nsLayoutAtoms::tableCellFrame == fType) {
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

#endif

#endif


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
  nsCRT::memset (indent, ' ', aIndent + MIN_INDENT);
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
      nsLayoutAtoms::tableCellFrame     == fType.get()) {
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




