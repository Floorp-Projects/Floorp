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
 */
#include "nsCOMPtr.h"
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
#include "nsIPtr.h"
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

NS_DEF_PTR(nsIStyleContext);
NS_DEF_PTR(nsIContent);

static NS_DEFINE_IID(kIHTMLElementIID, NS_IDOMHTMLELEMENT_IID);
static NS_DEFINE_IID(kIBodyElementIID, NS_IDOMHTMLBODYELEMENT_IID);
static NS_DEFINE_IID(kITableRowGroupFrameIID, NS_ITABLEROWGROUPFRAME_IID);

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
    availSize.width = aReflowState.availableWidth;
    if (!unconstrainedWidth) {
      availSize.width -= aBorderPadding.left + aBorderPadding.right;
    }

    unconstrainedHeight = PRBool(aReflowState.availableHeight == NS_UNCONSTRAINEDSIZE);
    availSize.height = aReflowState.availableHeight;
    if (!unconstrainedHeight) {
      availSize.height -= aBorderPadding.top + aBorderPadding.bottom;
    }

    footerFrame = nsnull;
    firstBodySection = nsnull;
  }
};


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
    mPercentBasisForRows(0)
{
  mBits.mColumnWidthsSet = PR_FALSE;
  mBits.mColumnWidthsValid = PR_FALSE;
  mBits.mFirstPassValid = PR_FALSE;
  mBits.mColumnCacheValid = PR_FALSE;
  mBits.mCellMapValid = PR_TRUE;
  mBits.mIsInvariantWidth = PR_FALSE;
  mBits.mHasScrollableRowGroup = PR_FALSE;
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
  mCellMap = new nsCellMap(0, 0);
}

NS_IMPL_ADDREF_INHERITED(nsTableFrame, nsHTMLContainerFrame)
NS_IMPL_RELEASE_INHERITED(nsTableFrame, nsHTMLContainerFrame)

nsresult nsTableFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(nsITableLayout::GetIID())) 
  { // note there is no addref here, frames are not addref'd
    *aInstancePtr = (void*)(nsITableLayout*)this;
    return NS_OK;
  } else {
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
    if (PR_TRUE==IsRowGroup(childDisplay->mDisplay))
    {
      if (mFrames.IsEmpty())
        mFrames.SetFrames(childFrame);
      else
        prevMainChild->SetNextSibling(childFrame);
      // If we have a prev-in-flow, then we're a table that has been split and
      // so don't treat this like an append
      if (!mPrevInFlow) {
        rv = DidAppendRowGroup(GetRowGroupFrameFor(childFrame, childDisplay));
      }
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

  if (NS_SUCCEEDED(rv)) {
    PRBool  createdColFrames;
    EnsureColumns(aPresContext, createdColFrames);
  }

  if (HasGroupRules()) {
    ProcessGroupRules(aPresContext);
  }

  return rv;
}

NS_IMETHODIMP nsTableFrame::DidAppendRowGroup(nsTableRowGroupFrame *aRowGroupFrame)
{
  nsresult rv=NS_OK;
  nsIFrame *nextRow=nsnull;
  aRowGroupFrame->FirstChild(nsnull, &nextRow);
  for ( ; nsnull!=nextRow; nextRow->GetNextSibling(&nextRow))
  {
    const nsStyleDisplay *rowDisplay;
    nextRow->GetStyleData(eStyleStruct_Display, (const nsStyleStruct *&)rowDisplay);
    if (NS_STYLE_DISPLAY_TABLE_ROW==rowDisplay->mDisplay) {
      rv = ((nsTableRowFrame *)nextRow)->InitChildren();
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
    else if (NS_STYLE_DISPLAY_TABLE_ROW_GROUP==rowDisplay->mDisplay) {
      rv = DidAppendRowGroup((nsTableRowGroupFrame*)nextRow);
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
  }

  return rv;
}


/* ****** CellMap methods ******* */

/* counts columns in column groups */
PRInt32 nsTableFrame::GetSpecifiedColumnCount ()
{
  PRInt32 colCount = 0;
  nsIFrame * childFrame = mColGroups.FirstChild();
  while (nsnull!=childFrame) {
    colCount += ((nsTableColGroupFrame *)childFrame)->GetColumnCount();
    childFrame->GetNextSibling(&childFrame);
  }    
  return colCount;
}

PRInt32 nsTableFrame::GetRowCount () const
{
  PRInt32 rowCount = 0;
  nsCellMap *cellMap = GetCellMap();
  NS_ASSERTION(nsnull!=cellMap, "GetRowCount null cellmap");
  if (nsnull!=cellMap)
    rowCount = cellMap->GetRowCount();
  return rowCount;
}

/* return the effective col count */
PRInt32 nsTableFrame::GetColCount ()
{
  PRInt32 colCount = 0;
  nsCellMap* cellMap = GetCellMap();
  NS_ASSERTION(nsnull != cellMap, "GetColCount null cellmap");
  if (nsnull != cellMap)
    colCount = cellMap->GetColCount();
  return colCount;
}


nsTableColFrame * nsTableFrame::GetColFrame(PRInt32 aColIndex)
{
  nsTableColFrame *result = nsnull;
  nsCellMap *cellMap = GetCellMap();
  if (nsnull!=cellMap)
  {
    result = cellMap->GetColumnFrame(aColIndex);
  }
  return result;
}

// can return nsnull
nsTableCellFrame* nsTableFrame::GetCellFrameAt(PRInt32 aRowIndex, PRInt32 aColIndex)
{
  nsCellMap* cellMap = GetCellMap();
  if (cellMap) 
    return cellMap->GetCellInfoAt(aRowIndex, aColIndex);
  return nsnull;
}

// return the number of rows spanned by aCell starting at aRowIndex
// note that this is different from just the rowspan of aCell
// (that would be GetEffectiveRowSpan (indexOfRowThatContains_aCell, aCell)
//
// XXX This code should be in the table row group frame instead, and it
// should clip rows spans so they don't extend past a row group rather than
// clip to the table itself. Before that can happen the code that builds the
// cell map needs to take row groups into account
PRInt32 nsTableFrame::GetEffectiveRowSpan (PRInt32 aRowIndex, nsTableCellFrame *aCell)
{
  NS_PRECONDITION (nsnull!=aCell, "bad cell arg");
  NS_PRECONDITION (0<=aRowIndex && aRowIndex<GetRowCount(), "bad row index arg");

  if (!(0<=aRowIndex && aRowIndex<GetRowCount()))
    return 1;

  // XXX I don't think this is correct...
#if 0
  PRInt32 rowSpan = aCell->GetRowSpan();
  PRInt32 rowCount = GetRowCount();
  if (rowCount < (aRowIndex + rowSpan))
    return (rowCount - aRowIndex);
  return rowSpan;
#else
  PRInt32 rowSpan = aCell->GetRowSpan();
  PRInt32 rowCount = GetRowCount();
  PRInt32 startRow;
  aCell->GetRowIndex(startRow);

  // Clip the row span so it doesn't extend past the bottom of the table
  if ((startRow + rowSpan) > rowCount) {
    rowSpan = rowCount - startRow;
  }

  // Check that aRowIndex is in the range startRow..startRow+rowSpan-1
  PRInt32 lastRow = startRow + rowSpan - 1;
  if ((aRowIndex < startRow) || (aRowIndex > lastRow)) {
    return 0;  // cell doesn't span any rows starting at aRowIndex
  } else {
    return lastRow - aRowIndex + 1;
  }
#endif
}

PRInt32 nsTableFrame::GetEffectiveRowSpan(nsTableCellFrame *aCell)
{
  PRInt32 startRow;
  aCell->GetRowIndex(startRow);
  return GetEffectiveRowSpan(startRow, aCell);
}


// Return the number of cols spanned by aCell starting at aColIndex
// This is different from the colspan of aCell. If the cell spans no
// dead cells then the colSpan of the cell would be
// GetEffectiveColSpan (indexOfColThatContains_aCell, aCell)
//
// XXX Should be moved to colgroup, as GetEffectiveRowSpan should be moved to rowgroup?
PRInt32 nsTableFrame::GetEffectiveColSpan(PRInt32 aColIndex, const nsTableCellFrame* aCell) const
{
  NS_PRECONDITION (nsnull != aCell, "bad cell arg");
  nsCellMap* cellMap = GetCellMap();
  NS_PRECONDITION (nsnull != cellMap, "bad call, cellMap not yet allocated.");

  return cellMap->GetEffectiveColSpan(aColIndex, aCell);
}

PRInt32 nsTableFrame::GetEffectiveColSpan(const nsTableCellFrame* aCell) const
{
  NS_PRECONDITION (nsnull != aCell, "bad cell arg");
  nsCellMap* cellMap = GetCellMap();
  NS_PRECONDITION (nsnull != cellMap, "bad call, cellMap not yet allocated.");

  PRInt32 initialColIndex;
  aCell->GetColIndex(initialColIndex);
  return cellMap->GetEffectiveColSpan(initialColIndex, aCell);
}

PRInt32 nsTableFrame::GetEffectiveCOLSAttribute()
{
  nsCellMap *cellMap = GetCellMap();
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


NS_IMETHODIMP nsTableFrame::AdjustRowIndices(nsIFrame* aRowGroup, 
                                             PRInt32 aRowIndex,
                                             PRInt32 anAdjustment)
{
  nsresult rv = NS_OK;
  nsIFrame *rowFrame;
  aRowGroup->FirstChild(nsnull, &rowFrame);
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
      AdjustRowIndices(rowFrame, aRowIndex, anAdjustment);
    }
  }
  return rv;
}

NS_IMETHODIMP nsTableFrame::RemoveRowFromMap(nsTableRowFrame* aRow, PRInt32 aRowIndex)
{
  nsresult rv=NS_OK;

  // Create a new row in the cell map at the specified index.
  mCellMap->RemoveRowFromMap(aRowIndex);

  // Iterate over our row groups and increment the row indices of all rows whose index
  // is >= aRowIndex.
  nsIFrame *rowGroupFrame=mFrames.FirstChild();
  for ( ; nsnull!=rowGroupFrame; rowGroupFrame->GetNextSibling(&rowGroupFrame))
  {
    const nsStyleDisplay *rowGroupDisplay;
    rowGroupFrame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct *&)rowGroupDisplay);
    if (PR_TRUE==IsRowGroup(rowGroupDisplay->mDisplay))
    {
      AdjustRowIndices(rowGroupFrame, aRowIndex, -1);
    }
  }

  return rv;
}

NS_IMETHODIMP nsTableFrame::InsertRowIntoMap(nsTableRowFrame* aRow, PRInt32 aRowIndex)
{
  nsresult rv=NS_OK;

  // Create a new row in the cell map at the specified index.
  mCellMap->InsertRowIntoMap(aRowIndex);

  // Iterate over our row groups and increment the row indices of all rows whose index
  // is >= aRowIndex.
  nsIFrame *rowGroupFrame=mFrames.FirstChild();
  for ( ; nsnull!=rowGroupFrame; rowGroupFrame->GetNextSibling(&rowGroupFrame))
  {
    const nsStyleDisplay *rowGroupDisplay;
    rowGroupFrame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct *&)rowGroupDisplay);
    if (PR_TRUE==IsRowGroup(rowGroupDisplay->mDisplay))
    {
      AdjustRowIndices(rowGroupFrame, aRowIndex, 1);
    }
  }

  // Init the row's index and add its cells to the cell map.
  aRow->InitChildrenWithIndex(aRowIndex);

  return rv;
}

/** sum the columns represented by all nsTableColGroup objects
  * if the cell map says there are more columns than this, 
  * add extra implicit columns to the content tree.
  */
void nsTableFrame::EnsureColumns(nsIPresContext* aPresContext,
                                 PRBool&         aCreatedColFrames)
{
  NS_PRECONDITION(nsnull!=mCellMap, "bad state:  null cellmap");
  
  aCreatedColFrames = PR_FALSE;  // initialize OUT parameter
  if (nsnull == mCellMap)
    return; // no info yet, so nothing useful to do

  // make sure we've accounted for the COLS attribute
  AdjustColumnsForCOLSAttribute();

  // count the number of column frames we already have
  PRInt32 actualColumns = 0;
  nsTableColGroupFrame *lastColGroupFrame = nsnull;
  nsIFrame* childFrame = mColGroups.FirstChild();
  while (nsnull!=childFrame) {
    ((nsTableColGroupFrame*)childFrame)->SetStartColumnIndex(actualColumns);
    PRInt32 numCols = ((nsTableColGroupFrame*)childFrame)->GetColumnCount();
    actualColumns += numCols;
    lastColGroupFrame = (nsTableColGroupFrame *)childFrame;
    childFrame->GetNextSibling(&childFrame);
  }

  // if we have fewer column frames than we need, create some implicit column frames
  PRInt32 colCount = mCellMap->GetColCount();
  if (actualColumns < colCount) {
    nsIContent *lastColGroupElement = nsnull;
    if (nsnull==lastColGroupFrame) { // there are no col groups, so create an implicit colgroup frame 
      // Resolve style for the colgroup frame
      // first, need to get the nearest containing content object
      GetContent(&lastColGroupElement);                                          // ADDREF a: lastColGroupElement++  (either here or in the loop below)
      nsIFrame *parentFrame;
      GetParent(&parentFrame);
      while (nsnull==lastColGroupElement) {
        parentFrame->GetContent(&lastColGroupElement);
        if (nsnull==lastColGroupElement)
          parentFrame->GetParent(&parentFrame);
      }
      // now we have a ref-counted "lastColGroupElement" content object
      nsIStyleContext* colGroupStyleContext;
      aPresContext->ResolvePseudoStyleContextFor (lastColGroupElement, 
                                                 nsHTMLAtoms::tableColGroupPseudo,
                                                 mStyleContext,
                                                 PR_FALSE,
                                                 &colGroupStyleContext);        // colGroupStyleContext: REFCNT++
      // Create a col group frame
      nsIFrame* newFrame;
      NS_NewTableColGroupFrame(&newFrame);
      newFrame->Init(aPresContext, lastColGroupElement, this, colGroupStyleContext,
                     nsnull);
      lastColGroupFrame = (nsTableColGroupFrame*)newFrame;
      NS_RELEASE(colGroupStyleContext);                                         // kidStyleContenxt: REFCNT--

      // hook lastColGroupFrame into child list
      mColGroups.SetFrames(lastColGroupFrame);
    }
    else {
      lastColGroupFrame->GetContent(&lastColGroupElement);  // ADDREF b: lastColGroupElement++
    }

    // XXX It would be better to do this in the style code while constructing
    // the table's frames.  But we don't know how many columns we have at that point.
    nsAutoString colTag;
    nsHTMLAtoms::col->ToString(colTag);
    PRInt32 excessColumns = colCount - actualColumns;
    nsIFrame* firstNewColFrame = nsnull;
    nsIFrame* lastNewColFrame = nsnull;
    nsIStyleContextPtr  lastColGroupStyle;
    lastColGroupFrame->GetStyleContext(lastColGroupStyle.AssignPtr());
    for ( ; excessColumns > 0; excessColumns--) {
      // Create a new col frame
      nsIFrame* colFrame;
      // note we pass in PR_TRUE here to force unique style contexts.
      nsIStyleContext* colStyleContext;
      aPresContext->ResolvePseudoStyleContextFor (lastColGroupElement, 
                                                 nsHTMLAtoms::tableColPseudo,
                                                 lastColGroupStyle,
                                                 PR_TRUE,
                                                 &colStyleContext);             // colStyleContext: REFCNT++
      aCreatedColFrames = PR_TRUE;  // remember that we're creating implicit col frames
      NS_NewTableColFrame(&colFrame);
      colFrame->Init(aPresContext, lastColGroupElement, lastColGroupFrame,
                     colStyleContext, nsnull);
      NS_RELEASE(colStyleContext);
      colFrame->SetInitialChildList(aPresContext, nsnull, nsnull);
      ((nsTableColFrame *)colFrame)->SetIsAnonymous(PR_TRUE);

      // Add it to our list
      if (nsnull == lastNewColFrame) {
        firstNewColFrame = colFrame;
      } else {
        lastNewColFrame->SetNextSibling(colFrame);
      }
      lastNewColFrame = colFrame;
    }
    lastColGroupFrame->SetInitialChildList(aPresContext, nsnull, firstNewColFrame);
    NS_RELEASE(lastColGroupElement);                       // ADDREF: lastColGroupElement--
  }
  else if (actualColumns > colCount) { // the cell map needs to grow to accomodate extra cols
    nsCellMap* cellMap = GetCellMap();
    if (cellMap) {
      cellMap->AddColsAtEnd(actualColumns - colCount);
    }
  }

}


void nsTableFrame::AddColumnFrame (nsTableColFrame *aColFrame)
{
  nsCellMap *cellMap = GetCellMap();
  NS_PRECONDITION (nsnull!=cellMap, "null cellMap.");
  if (nsnull!=cellMap)
  {
    cellMap->AppendColumnFrame(aColFrame);
  }
}

/** return the index of the next row that is not yet assigned */
PRInt32 nsTableFrame::GetNextAvailRowIndex() const
{
  PRInt32 result=0;
  nsCellMap *cellMap = GetCellMap();
  NS_PRECONDITION (nsnull!=cellMap, "null cellMap.");
  if (nsnull!=cellMap) {
    result = cellMap->GetNextAvailRowIndex(); 
  }
  return result;
}

/** Get the cell map for this table frame.  It is not always mCellMap.
  * Only the firstInFlow has a legit cell map
  */
nsCellMap * nsTableFrame::GetCellMap() const
{
  nsTableFrame * firstInFlow = (nsTableFrame *)GetFirstInFlow();
  if (this!=firstInFlow)
  {
    return firstInFlow->GetCellMap();
  }
  return mCellMap;
}

PRInt32 nsTableFrame::AddCellToTable(nsTableCellFrame* aCellFrame,
                                     PRInt32           aRowIndex)
{
  NS_ASSERTION(nsnull != aCellFrame, "bad aCellFrame arg");
  NS_ASSERTION(nsnull != mCellMap,   "bad cellMap");

  // XXX: must be called only on first-in-flow!
  return mCellMap->AppendCell(aCellFrame, aRowIndex);
}

void nsTableFrame::RemoveCellFromTable(nsTableCellFrame* aCellFrame,
                                       PRInt32           aRowIndex)
{
  nsCellMap* cellMap = GetCellMap();
  PRInt32 numCols = cellMap->GetColCount();
  cellMap->RemoveCell(aCellFrame, aRowIndex);
  if (numCols != cellMap->GetColCount()) {
    // XXX need to remove the anonymous col frames at the end
    //nsTableColFrame* colFrame = nsnull;
    //GetColumnFrame(colIndex, colFrame);
    //if (colFrame && colFrame->IsAnonymous()) {
    //  nsTableColGroupFrame* colGroupFrame = nsnull;
    //colFrame->GetParent((nsIFrame **)&colGroupFrame);
    //if (colGroupFrame) {
    //  colGroupFrame->DeleteColFrame(aPresContext, colFrame);
  }
}

static nsresult BuildCellMapForRowGroup(nsIFrame* rowGroupFrame)
{
  nsresult rv = NS_OK;
  nsIFrame *rowFrame;
  rowGroupFrame->FirstChild(nsnull, &rowFrame);
  for ( ; nsnull!=rowFrame; rowFrame->GetNextSibling(&rowFrame))
  {
    const nsStyleDisplay *rowDisplay;
    rowFrame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct *&)rowDisplay);
    if (NS_STYLE_DISPLAY_TABLE_ROW==rowDisplay->mDisplay)
    {
      rv = ((nsTableRowFrame *)rowFrame)->InitChildren();
      if (NS_FAILED(rv))
        return rv;
    }
    else if (NS_STYLE_DISPLAY_TABLE_ROW_GROUP==rowDisplay->mDisplay)
    {
      BuildCellMapForRowGroup(rowFrame);
    }
  }
  return rv;
}

NS_METHOD nsTableFrame::ReBuildCellMap()
{
  nsresult rv=NS_OK;
  nsIFrame *rowGroupFrame=mFrames.FirstChild();
  for ( ; nsnull!=rowGroupFrame; rowGroupFrame->GetNextSibling(&rowGroupFrame))
  {
    const nsStyleDisplay *rowGroupDisplay;
    rowGroupFrame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct *&)rowGroupDisplay);
    if (PR_TRUE==IsRowGroup(rowGroupDisplay->mDisplay))
    {
      BuildCellMapForRowGroup(rowGroupFrame);
    }
  }
  mBits.mCellMapValid=PR_TRUE;
  return rv;
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

  nsCellMap *cellMap = GetCellMap();
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
nsTableFrame::FirstChild(nsIAtom* aListName, nsIFrame** aFirstChild) const
{
  if (nsnull == aListName) {
    *aFirstChild = mFrames.FirstChild();
    return NS_OK;
  }
  else if (aListName == nsLayoutAtoms::colGroupList) {
    *aFirstChild = mColGroups.FirstChild();
    return NS_OK;
  }
  *aFirstChild = nsnull;
  return NS_ERROR_INVALID_ARG;
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
    if (disp->mVisible) {
      const nsStyleSpacing* spacing =
        (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);
      const nsStyleColor* color =
        (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);

      nsRect  rect(0, 0, mRect.width, mRect.height);
        
      nsCompatibility mode;
      aPresContext->GetCompatibilityMode(&mode);
      if (eCompatibility_Standard == mode) {
        nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                        aDirtyRect, rect, *color, *spacing, 0, 0);
        // paint the column groups and columns
        nsIFrame* colGroupFrame = mColGroups.FirstChild();
        while (nsnull != colGroupFrame) {
          PaintChild(aPresContext, aRenderingContext, aDirtyRect, colGroupFrame, aWhichLayer);
          colGroupFrame->GetNextSibling(&colGroupFrame);
        }
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
    result = !(IsCellMapValid() && IsFirstPassValid() &&
               IsColumnCacheValid() && IsColumnWidthsValid());
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

  // If it's the footer that was reflowed, then we don't need to adjust any of
  // the frames, because the footer is the bottom most frame
  if (aKidFrame != aReflowState.footerFrame) {
    nsIFrame* kidFrame;
    nsRect    kidRect;
    PRBool    movedFooter = PR_FALSE;
  
    // Move the frames that follow aKidFrame by aDeltaY, and update the running
    // y-offset
    for (aKidFrame->GetNextSibling(&kidFrame); kidFrame; kidFrame->GetNextSibling(&kidFrame)) {
      // See if it's the footer we're moving
      if (kidFrame == aReflowState.footerFrame) {
        movedFooter = PR_TRUE;
      }
  
      // Get the frame's bounding rect
      kidFrame->GetRect(kidRect);
  
      // Adjust the running y-offset
      aReflowState.y += kidRect.height;

      // Update the max element size
      //XXX: this should call into layout strategy to get the width field
      if (aMaxElementSize) 
      {
        const nsStyleSpacing* tableSpacing;
        GetStyleData(eStyleStruct_Spacing , ((const nsStyleStruct *&)tableSpacing));
        nsMargin borderPadding;
        GetTableBorder (borderPadding); // gets the max border thickness for each edge
        nsMargin padding;
        tableSpacing->GetPadding(padding);
        borderPadding += padding;
        nscoord cellSpacing = GetCellSpacingX();
        nsSize  kidMaxElementSize;
        ((nsTableRowGroupFrame*)kidFrame)->GetMaxElementSize(kidMaxElementSize);
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
  
    // We also need to move the footer if there is one and we haven't already
    // moved it
    if (aReflowState.footerFrame && !movedFooter) {
      aReflowState.footerFrame->GetRect(kidRect);
      
      // Adjust the running y-offset
      aReflowState.y += kidRect.height;
  
      if (aDeltaY != 0) {
        kidRect.y += aDeltaY;
        aReflowState.footerFrame->MoveTo(aPresContext, kidRect.x, kidRect.y);
      }
    }

    // Invalidate the area we offset. Note that we only repaint within
    // our existing frame bounds.
    // XXX It would be better to bitblt the row frames and not repaint,
    // but we don't have such a view manager function yet...
    aKidFrame->GetRect(kidRect);
    if (kidRect.YMost() < mRect.height) {
      nsRect  dirtyRect(0, kidRect.YMost(),
                        mRect.width, mRect.height - kidRect.YMost());
      Invalidate(aPresContext, dirtyRect);
    }
  }

  return NS_OK;
}

void nsTableFrame::SetColumnDimensions(nsIPresContext* aPresContext, nscoord aHeight)
{
  const nsStyleSpacing* spacing =
    (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);
  nsMargin borderPadding;
  spacing->CalcBorderPaddingFor(this, borderPadding);
  nscoord colHeight = aHeight -= borderPadding.top + borderPadding.bottom;
  nscoord cellSpacingX = GetCellSpacingX();
  nscoord halfCellSpacingX = NSToCoordRound(((float)cellSpacingX) / (float)2);

  nsIFrame* colGroupFrame = mColGroups.FirstChild();
  PRInt32 colX = 0;
  nsPoint colGroupOrigin(borderPadding.left, borderPadding.top);
  PRInt32 numCols = GetColCount();
  while (nsnull != colGroupFrame) {
    nscoord colGroupWidth = 0;
    nsIFrame* colFrame = nsnull;
    colGroupFrame->FirstChild(nsnull, &colFrame);
    nsPoint colOrigin(0, 0);
    while (nsnull != colFrame) {
      const nsStyleDisplay* colDisplay;
      colFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)colDisplay));
      if (NS_STYLE_DISPLAY_TABLE_COLUMN == colDisplay->mDisplay) {
        NS_ASSERTION(colX < numCols, "invalid number of columns");
        nscoord colWidth = mColumnWidths[colX];
        if (numCols == 1) {
          colWidth += cellSpacingX + cellSpacingX;
        }
        else if ((0 == colX) || (numCols - 1 == colX)) {
          colWidth += cellSpacingX + halfCellSpacingX;
        }
        else if (GetNumCellsOriginatingInCol(colX) > 0) {
          colWidth += cellSpacingX;
        }

        colGroupWidth += colWidth;
        nsRect colRect(colOrigin.x, colOrigin.y, colWidth, colHeight);
        colFrame->SetRect(aPresContext, colRect);
        colOrigin.x += colWidth;
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
  if (nsDebugTable::gRflTable) nsTableFrame::DebugReflow("T::Rfl en", this, &aReflowState, nsnull);

  // this is a temporary fix to prevent a recent crash due to incremental content 
  // sink changes. this will go away when the col cache and cell map are synchronized
  if (!IsColumnCacheValid()) {
    CacheColFrames(aPresContext, PR_TRUE);
  }

  // Initialize out parameter
  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width = 0;
    aDesiredSize.maxElementSize->height = 0;
  }
  aStatus = NS_FRAME_COMPLETE;
  if ((NS_UNCONSTRAINEDSIZE == aReflowState.availableWidth) &&
      (this == (nsTableFrame *)GetFirstInFlow())) {
    InvalidateFirstPassCache();
  }

  nsresult rv = NS_OK;
  nsSize* pass1MaxElementSize = aDesiredSize.maxElementSize;

  if (eReflowReason_Incremental == aReflowState.reason) {
    rv = IncrementalReflow(aPresContext, aDesiredSize, aReflowState, aStatus);
  }

  // NeedsReflow and IsFirstPassValid take into account reflow type = Initial_Reflow
  if (PR_TRUE==NeedsReflow(aReflowState))
  {
    PRBool needsRecalc=PR_FALSE;
    if (eReflowReason_Initial!=aReflowState.reason && PR_FALSE==IsCellMapValid())
    {
      if (nsnull!=mCellMap)
        delete mCellMap;
      mCellMap = new nsCellMap(0,0);
      ReBuildCellMap();
      needsRecalc=PR_TRUE;
    }
    if ((NS_UNCONSTRAINEDSIZE == aReflowState.availableWidth) || 
        (PR_FALSE==IsFirstPassValid()))
    {
      nsReflowReason reason = aReflowState.reason;
      if (eReflowReason_Initial!=reason)
        reason = eReflowReason_Resize;
      if (mBorderCollapser) {
        mBorderCollapser->ComputeVerticalBorders(aPresContext, 0, -1);
      }
      rv = ResizeReflowPass1(aPresContext, aDesiredSize, aReflowState, aStatus, nsnull, reason, PR_TRUE);
      if (NS_FAILED(rv))
        return rv;
      needsRecalc=PR_TRUE;
    }
    if (PR_FALSE==IsColumnCacheValid())
    {
      needsRecalc=PR_TRUE;
    }
    if (PR_TRUE==needsRecalc)
    {
      CacheColFrames(aPresContext, PR_TRUE);
      // if we needed to rebuild the column cache, the data stored in the layout strategy is invalid
      if (nsnull!=mTableLayoutStrategy)
      {
        mTableLayoutStrategy->Initialize(aDesiredSize.maxElementSize, GetColCount(), aReflowState.mComputedWidth);
        mBits.mColumnWidthsValid=PR_TRUE; //so we don't do this a second time below
      }
    }
    if (PR_FALSE==IsColumnWidthsValid())
    {
      if (nsnull!=mTableLayoutStrategy)
      {
        mTableLayoutStrategy->Initialize(aDesiredSize.maxElementSize, GetColCount(), aReflowState.mComputedWidth);
        mBits.mColumnWidthsValid=PR_TRUE;
      }
    }

    if (nsnull==mPrevInFlow)
    { // only do this for a first-in-flow table frame
      // assign column widths, and assign aMaxElementSize->width
      BalanceColumnWidths(aPresContext, aReflowState, nsSize(aReflowState.availableWidth, aReflowState.availableHeight),
                          aDesiredSize.maxElementSize);

      // assign table width
      SetTableWidth(aPresContext);
    }

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
    if (NS_FAILED(rv)) {
      return rv;
    }
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
  else
  {
    // set aDesiredSize and aMaxElementSize
  }

  SetColumnDimensions(aPresContext, aDesiredSize.height);
  if (pass1MaxElementSize) {
    aDesiredSize.maxElementSize = pass1MaxElementSize;
  }

  if (nsDebugTable::gRflTable) nsTableFrame::DebugReflow("T::Rfl ex", this, nsnull, &aDesiredSize);
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
                                          nsTableRowGroupFrame *   aStartingFrame,
                                          nsReflowReason           aReason,
                                          PRBool                   aDoSiblingFrames)
{
  NS_PRECONDITION(aReflowState.frame == this, "bad reflow state");
  NS_PRECONDITION(aReflowState.parentReflowState->frame == mParent,
                  "bad parent reflow state");
  NS_ASSERTION(nsnull==mPrevInFlow, "illegal call, cannot call pass 1 on a continuing frame.");
  NS_ASSERTION(nsnull != mContent, "null content");

  nsresult rv=NS_OK;
  // set out params
  aStatus = NS_FRAME_COMPLETE;

  nsSize availSize(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE); // availSize is the space available at any given time in the process
  nsSize maxSize(0, 0);       // maxSize is the size of the largest child so far in the process
  nsSize kidMaxSize(0,0);
  nsHTMLReflowMetrics kidSize(&kidMaxSize);
  nscoord y = 0;

  // Compute the insets (sum of border and padding)
  // XXX: since this is pass1 reflow and where we place the rowgroup frames is irrelevant, insets are probably a waste

  if (PR_TRUE==RequiresPass1Layout())
  {
    nsIFrame* kidFrame = aStartingFrame;
    if (nsnull==kidFrame)
      kidFrame=mFrames.FirstChild();   
    for ( ; nsnull != kidFrame; kidFrame->GetNextSibling(&kidFrame)) 
    {
      const nsStyleDisplay *childDisplay;
      kidFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)childDisplay));
      if ((NS_STYLE_DISPLAY_TABLE_HEADER_GROUP != childDisplay->mDisplay) &&
          (NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP != childDisplay->mDisplay) &&
          (NS_STYLE_DISPLAY_TABLE_ROW_GROUP    != childDisplay->mDisplay) )
      { // it's an unknown frame type, give it a generic reflow and ignore the results
        nsHTMLReflowState kidReflowState(aPresContext, aReflowState, kidFrame, 
                                         availSize, aReason);
        // rv intentionally not set here
        ReflowChild(kidFrame, aPresContext, kidSize, kidReflowState, 0, 0, 0, aStatus);
        kidFrame->DidReflow(aPresContext, NS_FRAME_REFLOW_FINISHED);
        continue;
      }
      nsHTMLReflowState kidReflowState(aPresContext, aReflowState, kidFrame,
                                       availSize, aReason);
      // Note: we don't bother checking here for whether we should clear the
      // isTopOfPage reflow state flag, because we're dealing with an unconstrained
      // height and it isn't an issue...
      ReflowChild(kidFrame, aPresContext, kidSize, kidReflowState, 0, 0, 0, aStatus);

      // Place the child since some of its content fit in us.
      FinishReflowChild(kidFrame, aPresContext, kidSize, 0, 0, 0);
      if (NS_UNCONSTRAINEDSIZE==kidSize.height)
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
      if (PR_FALSE==aDoSiblingFrames)
        break;
    }

    // if required, give the colgroups their initial reflows
    if (PR_TRUE==aDoSiblingFrames)
    {
      kidFrame=mColGroups.FirstChild();   
      for ( ; nsnull != kidFrame; kidFrame->GetNextSibling(&kidFrame)) 
      {
        nsHTMLReflowState kidReflowState(aPresContext, aReflowState, kidFrame,
                                         availSize, aReason);
        ReflowChild(kidFrame, aPresContext, kidSize, kidReflowState, 0, 0, 0, aStatus);
        FinishReflowChild(kidFrame, aPresContext, kidSize, 0, 0, 0);
      }
    }
  }

  aDesiredSize.width = kidSize.width;
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

  const nsStyleSpacing* mySpacing = (const nsStyleSpacing*)
    mStyleContext->GetStyleData(eStyleStruct_Spacing);
  nsMargin borderPadding;
  GetTableBorder (borderPadding);   // this gets the max border thickness at each edge
  nsMargin padding;
  mySpacing->GetPadding(padding);
  borderPadding += padding;

  InnerTableReflowState state(aPresContext, aReflowState, borderPadding);

  // now that we've computed the column  width information, reflow all children

#ifdef NS_DEBUG
  //PreReflowCheck();
#endif

  // Check for an overflow list, and append any row group frames being
  // pushed
  MoveOverflowToChildList(aPresContext);

  // Reflow the existing frames
  if (mFrames.NotEmpty()) {
    ComputePercentBasisForRows(aReflowState);
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
  nscoord defaultHeight = state.y + borderPadding.top + borderPadding.bottom;
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

void nsTableFrame::ComputePercentBasisForRows(const nsHTMLReflowState& aReflowState)
{
  nscoord height;
  GetTableSpecifiedHeight(height, aReflowState);
  if ((height > 0) && (height < NS_UNCONSTRAINEDSIZE)) {
    // exclude our border and padding
    const nsStyleSpacing* spacing =
      (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);
	  nsMargin borderPadding(0,0,0,0);
    // XXX handle percentages
    if (spacing->GetBorderPadding(borderPadding)) {
     height -= borderPadding.top + borderPadding.bottom;
    }
    // exclude cell spacing for all rows
    height -= (1 + GetRowCount()) * GetCellSpacingY();
    height = PR_MAX(0, height);
  }
  else {
    height = 0;
  }
  mPercentBasisForRows = height;
}

// collapsing row groups, rows, col groups and cols are accounted for after both passes of
// reflow so that it has no effect on the calculations of reflow.
NS_METHOD nsTableFrame::AdjustForCollapsingRowGroup(nsIFrame* aRowGroupFrame, 
                                                    PRInt32& aRowX)
{
  nsCellMap* cellMap = GetCellMap(); // XXX is this right
  const nsStyleDisplay* groupDisplay;
  aRowGroupFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)groupDisplay));
  PRBool groupIsCollapsed = (NS_STYLE_VISIBILITY_COLLAPSE == groupDisplay->mVisible);

  nsTableRowFrame* rowFrame = nsnull;
  aRowGroupFrame->FirstChild(nsnull, (nsIFrame**)&rowFrame);
  while (nsnull != rowFrame) { 
    const nsStyleDisplay *rowDisplay;
    rowFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)rowDisplay));
    if (NS_STYLE_DISPLAY_TABLE_ROW == rowDisplay->mDisplay) {
      if (groupIsCollapsed || (NS_STYLE_VISIBILITY_COLLAPSE == rowDisplay->mVisible)) {
        cellMap->SetRowCollapsedAt(aRowX, PR_TRUE);
      }
      aRowX++;
    }
    else if (NS_STYLE_DISPLAY_TABLE_ROW_GROUP == rowDisplay->mDisplay) {
      AdjustForCollapsingRowGroup(rowFrame, aRowX);
    }
    
    rowFrame->GetNextSibling((nsIFrame**)&rowFrame);
  }

  return NS_OK;
}

NS_METHOD nsTableFrame::CollapseRowGroup(nsIPresContext* aPresContext,
                                         nsIFrame* aRowGroupFrame,
                                         const nscoord& aYTotalOffset,
                                         nscoord& aYGroupOffset, PRInt32& aRowX)
{
  const nsStyleDisplay* groupDisplay;
  aRowGroupFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)groupDisplay));
  
  PRBool collapseGroup = (NS_STYLE_VISIBILITY_COLLAPSE == groupDisplay->mVisible);
  nsIFrame* rowFrame;
  aRowGroupFrame->FirstChild(nsnull, &rowFrame);

  while (nsnull != rowFrame) {
    const nsStyleDisplay* rowDisplay;
    rowFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)rowDisplay));
    if (NS_STYLE_DISPLAY_TABLE_ROW_GROUP == rowDisplay->mDisplay) {
      CollapseRowGroup(aPresContext, rowFrame, aYTotalOffset, aYGroupOffset, aRowX);
    }
    else if (NS_STYLE_DISPLAY_TABLE_ROW == rowDisplay->mDisplay) {
      nsRect rowRect;
      rowFrame->GetRect(rowRect);
      if (collapseGroup || (NS_STYLE_VISIBILITY_COLLAPSE == rowDisplay->mVisible)) {
        aYGroupOffset += rowRect.height;
        rowRect.height = 0;
        rowFrame->SetRect(aPresContext, rowRect);
        nsIFrame* cellFrame;
        rowFrame->FirstChild(nsnull, &cellFrame);
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
         //if (!collapseGroup) {
          PRInt32 numCols = mCellMap->GetColCount();
          nsTableCellFrame* lastCell = nsnull;
          for (int colX = 0; colX < numCols; colX++) {
            CellData* cellData = mCellMap->GetCellAt(aRowX, colX);
            if (cellData && !cellData->mOrigCell) { // a cell above is spanning into here
              // adjust the real cell's rect only once
              nsTableCellFrame* realCell = nsnull;
              if (cellData->mRowSpanData)
                realCell = cellData->mRowSpanData->mOrigCell;
              if (realCell != lastCell) {
                nsRect realRect;
                realCell->GetRect(realRect);
                realRect.height -= rowRect.height;
                realCell->SetRect(aPresContext, realRect);
              }
              lastCell = realCell;
            }
          }
        //}
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

NS_METHOD nsTableFrame::AdjustForCollapsingRows(nsIPresContext* aPresContext, 
                                                nscoord&        aHeight)
{
  // determine which row groups and rows are collapsed
  PRInt32 rowX = 0;
  nsIFrame* childFrame;
  FirstChild(nsnull, &childFrame);
  while (nsnull != childFrame) { 
    const nsStyleDisplay* groupDisplay;
    childFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)groupDisplay));
    if (IsRowGroup(groupDisplay->mDisplay)) {
      AdjustForCollapsingRowGroup(childFrame, rowX);
    }
    childFrame->GetNextSibling(&childFrame);
  }

  if (mCellMap->GetNumCollapsedRows() <= 0) { 
    return NS_OK; // no collapsed rows, we're done
  }

  // collapse the rows and/or row groups
  nsIFrame* groupFrame = mFrames.FirstChild(); 
  nscoord yGroupOffset = 0; // total offset among rows within a single row group
  nscoord yTotalOffset = 0; // total offset among all rows in all row groups
  rowX = 0;
  
  while (nsnull != groupFrame) {
    const nsStyleDisplay* groupDisplay;
    groupFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)groupDisplay));
    if (IsRowGroup(groupDisplay->mDisplay)) {
      CollapseRowGroup(aPresContext, groupFrame, yTotalOffset, yGroupOffset, rowX);
    }
    yTotalOffset += yGroupOffset;
    yGroupOffset = 0;
    groupFrame->GetNextSibling(&groupFrame);
  } // end group frame while

  aHeight -= yTotalOffset;
 
  return NS_OK;
}

NS_METHOD nsTableFrame::AdjustForCollapsingCols(nsIPresContext* aPresContext, 
                                                nscoord&        aWidth)
{
  // determine which col groups and cols are collapsed
  nsIFrame* childFrame = mColGroups.FirstChild();
  PRInt32 numCols = 0;
  while (nsnull != childFrame) { 
    const nsStyleDisplay* groupDisplay;
    GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)groupDisplay));
    PRBool groupIsCollapsed = (NS_STYLE_VISIBILITY_COLLAPSE == groupDisplay->mVisible);

    nsTableColFrame* colFrame = nsnull;
    childFrame->FirstChild(nsnull, (nsIFrame**)&colFrame);
    while (nsnull != colFrame) { 
      const nsStyleDisplay *colDisplay;
      colFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)colDisplay));
      if (NS_STYLE_DISPLAY_TABLE_COLUMN == colDisplay->mDisplay) {
        if (groupIsCollapsed || (NS_STYLE_VISIBILITY_COLLAPSE == colDisplay->mVisible)) {
          mCellMap->SetColCollapsedAt(numCols, PR_TRUE);
        }
        numCols++;
      }
      colFrame->GetNextSibling((nsIFrame**)&colFrame);
    }
    childFrame->GetNextSibling(&childFrame);
  }

  if (mCellMap->GetNumCollapsedCols() <= 0) { 
    return NS_OK; // no collapsed cols, we're done
  }

  // collapse the cols and/or col groups
  PRInt32 numRows = mCellMap->GetRowCount();
  nsTableIterator groupIter(mColGroups, eTableDIR);
  nsIFrame* groupFrame = groupIter.First(); 
  nscoord cellSpacingX = GetCellSpacingX();
  nscoord xOffset = 0;
  PRInt32 colX = (groupIter.IsLeftToRight()) ? 0 : numCols - 1; 
  PRInt32 direction = (groupIter.IsLeftToRight()) ? 1 : -1; 
  while (nsnull != groupFrame) {
    const nsStyleDisplay* groupDisplay;
    groupFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)groupDisplay));
    PRBool collapseGroup = (NS_STYLE_VISIBILITY_COLLAPSE == groupDisplay->mVisible);
    nsTableIterator colIter(*groupFrame, eTableDIR);
    nsIFrame* colFrame = colIter.First();
    while (nsnull != colFrame) {
      const nsStyleDisplay* colDisplay;
      colFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)colDisplay));
      if (NS_STYLE_DISPLAY_TABLE_COLUMN == colDisplay->mDisplay) {
        PRBool collapseCol = (NS_STYLE_VISIBILITY_COLLAPSE == colDisplay->mVisible);
        PRInt32 colSpan = ((nsTableColFrame*)colFrame)->GetSpan();
        for (PRInt32 spanX = 0; spanX < colSpan; spanX++) {
          PRInt32 col2X = colX + (direction * spanX);
          PRInt32 colWidth = GetColumnWidth(col2X);
          if (collapseGroup || collapseCol) {
            xOffset += colWidth + cellSpacingX;
          }
          nsTableCellFrame* lastCell  = nsnull;
          nsTableCellFrame* cellFrame = nsnull;
          for (PRInt32 rowX = 0; rowX < numRows; rowX++) {
            CellData* cellData = mCellMap->GetCellAt(rowX, col2X);
            nsRect cellRect;
            if (cellData) {
              cellFrame = cellData->mOrigCell;
              if (cellFrame) { // the cell originates at (rowX, colX)
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
                if (cellData->mColSpanData)
                  cellFrame = cellData->mColSpanData->mOrigCell;
                if ((cellFrame) && (lastCell != cellFrame)) {
                  cellFrame->GetRect(cellRect);
                  cellRect.width -= colWidth + cellSpacingX;
                  cellFrame->SetRect(aPresContext, cellRect);
                }
              }
            }
            lastCell = cellFrame;
          }
        }
        colX += direction * colSpan;
      }
      colFrame = colIter.Next();
    } // inner while
    groupFrame = groupIter.Next();
  } // outer while

  aWidth -= xOffset;
 
  return NS_OK;
}

NS_METHOD nsTableFrame::GetBorderPlusMarginPadding(nsMargin& aResult)
{
  const nsStyleSpacing* mySpacing = (const nsStyleSpacing*)
    mStyleContext->GetStyleData(eStyleStruct_Spacing);
  nsMargin borderPadding;
  GetTableBorder (borderPadding);
  nsMargin padding;
  mySpacing->GetPadding(padding);
  borderPadding += padding;
  aResult = borderPadding;
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
    index += colGroupFrame->GetColumnCount();
  }

  return index;
}

NS_IMETHODIMP
nsTableFrame::AppendFrames(nsIPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aFrameList)
{
  PRInt32 startingColIndex = -1;

  // Because we actually have two child lists, one for col group frames and one
  // for everything else, we need to look at each frame individually
  nsIFrame* f = aFrameList;
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

      // Set its starting column index
      if (-1 == startingColIndex) {
        startingColIndex = CalculateStartingColumnIndexFor((nsTableColGroupFrame*)f);
      }
      ((nsTableColGroupFrame*)f)->SetStartColumnIndex(startingColIndex);
      startingColIndex += ((nsTableColGroupFrame *)f)->GetColumnCount();
    
    } else if (IsRowGroup(display->mDisplay)) {
      // Append the new row group frame
      mFrames.AppendFrame(nsnull, f);
      
      // Add the content of the row group to the cell map
      DidAppendRowGroup((nsTableRowGroupFrame*)f);

    } else {
      // Nothing special to do, just add the frame to our child list
      mFrames.AppendFrame(nsnull, f);
    }

    // Move to the next frame
    f = next;
  }

  // We'll need to do a pass-1 layout of all cells in all the rows of the
  // rowgroup
  InvalidateFirstPassCache();
  
  // If we've added any columns, we need to rebuild the column cache.
  InvalidateColumnCache();
  
  // Because the number of columns may have changed invalidate the column
  // cache. Note that this has the side effect of recomputing the column
  // widths, so we don't need to call InvalidateColumnWidths()
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

  return rv;
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
    mColGroups.InsertFrame(nsnull, aPrevFrame, aFrameList);

    // Set its starting column index and adjust the index of the
    // col group frames that follow
    SetStartingColumnIndexFor((nsTableColGroupFrame*)aFrameList,
                              CalculateStartingColumnIndexFor((nsTableColGroupFrame*)aFrameList));
  
  } else if (IsRowGroup(display->mDisplay)) {
    // Insert the frame
    mFrames.InsertFrame(nsnull, aPrevFrame, aFrameList);

    // We'll need to do a pass-1 layout of all cells in all the rows of the
    // rowgroup
    InvalidateFirstPassCache();

    // We need to rebuild the cell map, because currently we can't insert
    // new frames except at the end (append)
    InvalidateCellMap();

  } else {
    // Just insert the frame and don't worry about reflowing it
    mFrames.InsertFrame(nsnull, aPrevFrame, aFrameList);
    return NS_OK;
  }

  // If we've added any columns, we need to rebuild the column cache.
  InvalidateColumnCache();
  
  // Because the number of columns may have changed invalidate the column
  // cache. Note that this has the side effect of recomputing the column
  // widths, so we don't need to call InvalidateColumnWidths()
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
  return rv;
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

    // Remove the col group frame
    mColGroups.DestroyFrame(aPresContext, aOldFrame);

    // Adjust the starting column index of the frames that follow
    SetStartingColumnIndexFor((nsTableColGroupFrame*)nextColGroupFrame,
                              CalculateStartingColumnIndexFor((nsTableColGroupFrame*)nextColGroupFrame));

  } else if (IsRowGroup(display->mDisplay)) {
    mFrames.DestroyFrame(aPresContext, aOldFrame);

    // We need to rebuild the cell map, because currently we can't incrementally
    // remove rows
    InvalidateCellMap();

  } else {
    // Just remove the frame
    mFrames.DestroyFrame(aPresContext, aOldFrame);
    return NS_OK;
  }

  // Because the number of columns may have changed invalidate the column
  // cache. Note that this has the side effect of recomputing the column
  // widths, so we don't need to call InvalidateColumnWidths()
  InvalidateColumnCache();
  
  // Because the number of columns may have changed invalidate the column
  // cache. Note that this has the side effect of recomputing the column
  // widths, so we don't need to call InvalidateColumnWidths()
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

  return rv;
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

  nsMargin borderPadding;
  GetBorderPlusMarginPadding(borderPadding);
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

      // Recover our reflow state
      rv = IR_TargetIsChild(aPresContext, aDesiredSize, state, aStatus, nextFrame);
    }
  }
  return rv;
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
    // Problem is we don't know has changed, so assume the worst
    InvalidateCellMap();
    InvalidateFirstPassCache();
    InvalidateColumnCache();
    InvalidateColumnWidths();
    rv = NS_OK;
    break;
  
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
// In the case of the footer frame the y-offset is set to its current
// y-offset. Note that this is different from resize reflow when the
// footer is positioned higher up and then moves down as each row
// group frame is relowed
//
// XXX This is kind of klunky because the InnerTableReflowState::y
// data member does not include the table's border/padding...
nsresult
nsTableFrame::RecoverState(InnerTableReflowState& aReflowState,
                           nsIFrame*              aKidFrame,
                           nsSize*                aMaxElementSize)
{
  // Walk the list of children looking for aKidFrame
  for (nsIFrame* frame = mFrames.FirstChild(); frame; frame->GetNextSibling(&frame)) {
    // If this is a footer row group, remember it
    const nsStyleDisplay *display;
    frame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)display));

    // We only allow a single footer frame, and the footer frame must occur before
    // any body section row groups
    if ((NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP == display->mDisplay) &&
        !aReflowState.footerFrame && !aReflowState.firstBodySection) {
      aReflowState.footerFrame = frame;
    
    } else if ((NS_STYLE_DISPLAY_TABLE_ROW_GROUP == display->mDisplay) &&
               !aReflowState.firstBodySection) {
      aReflowState.firstBodySection = frame;
    }
    
    // See if this is the frame we're looking for
    if (frame == aKidFrame) {
      // If it's the footer, then keep going because the footer is at the
      // very bottom
      if (frame != aReflowState.footerFrame) {
        break;
      }
    }

    // Get the frame's height
    nsSize  kidSize;
    frame->GetSize(kidSize);
    
    // If our height is constrained then update the available height. Do
    // this for all frames including the footer frame
    if (PR_FALSE == aReflowState.unconstrainedHeight) {
      aReflowState.availSize.height -= kidSize.height;
    }

    // Update the running y-offset. Don't do this for the footer frame
    if (frame != aReflowState.footerFrame) {
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
      nsMargin padding;
      tableSpacing->GetPadding(padding);
      borderPadding += padding;
      nscoord cellSpacing = GetCellSpacingX();
      nsSize  kidMaxElementSize;
      ((nsTableRowGroupFrame*)frame)->GetMaxElementSize(kidMaxElementSize);
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
  nsHTMLReflowMetrics desiredSize(aDesiredSize.maxElementSize ? &kidMaxElementSize
                                                              : nsnull);
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
  aReflowState.y += desiredSize.height;

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
    nsMargin padding;
    tableSpacing->GetPadding(padding);
    borderPadding += padding;
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
  nscoord desiredWidth=aReflowState.availableWidth;
  // this is the biggest hack in the world.  But there's no other rational way to handle nested percent tables
  const nsStylePosition* position;
  PRBool isNested=IsNested(aReflowState, position);
  if((eReflowReason_Initial==aReflowState.reason) && 
     (PR_TRUE==isNested) && (eStyleUnit_Percent==position->mWidth.GetUnit()))
  {
    nsITableLayoutStrategy* tableLayoutStrategy = mTableLayoutStrategy;
    if (mPrevInFlow) {
      // Get the table layout strategy from the first-in-flow
      nsTableFrame* table = (nsTableFrame*)GetFirstInFlow();
      tableLayoutStrategy = table->mTableLayoutStrategy;
    }
    desiredWidth = tableLayoutStrategy->GetTableMaxContentWidth();
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

  // If this is a footer row group, remember it
  const nsStyleDisplay *childDisplay;
  aKidFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)childDisplay));

  // We only allow a single footer frame, and the footer frame must occur before
  // any body section row groups
  if ((NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP == childDisplay->mDisplay) &&
      !aReflowState.footerFrame && !aReflowState.firstBodySection)
  {
    aReflowState.footerFrame = aKidFrame;
  }
  else if (aReflowState.footerFrame)
  {
    // Place the row group frame
    nsSize  footerSize;
    nsPoint origin;
    aKidFrame->GetOrigin(origin);
    aReflowState.footerFrame->GetSize(footerSize);
    origin.y -= footerSize.height;
    aKidFrame->MoveTo(aPresContext, origin.x, origin.y);
    
    // Move the footer below the body row group frame
    aReflowState.footerFrame->GetOrigin(origin);
    origin.y += aDesiredSize.height;
    aReflowState.footerFrame->MoveTo(aPresContext, origin.x, origin.y);
  }

  //XXX: this should call into layout strategy to get the width field
  if (nsnull != aMaxElementSize) 
  {
    const nsStyleSpacing* tableSpacing;
    GetStyleData(eStyleStruct_Spacing , ((const nsStyleStruct *&)tableSpacing));
    nsMargin borderPadding;
    GetTableBorder (borderPadding); // gets the max border thickness for each edge
    nsMargin padding;
    tableSpacing->GetPadding(padding);
    borderPadding += padding;
    nscoord cellSpacing = GetCellSpacingX();
    nscoord kidWidth = aKidMaxElementSize.width + borderPadding.left + borderPadding.right + cellSpacing*2;
    aMaxElementSize->width = PR_MAX(aMaxElementSize->width, kidWidth); 
    aMaxElementSize->height += aKidMaxElementSize.height;
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
NS_METHOD nsTableFrame::ReflowMappedChildren(nsIPresContext* aPresContext,
                                             nsHTMLReflowMetrics& aDesiredSize,
                                             InnerTableReflowState& aReflowState,
                                             nsReflowStatus& aStatus)
{
  NS_PRECONDITION(mFrames.NotEmpty(), "no children");

  PRInt32   childCount = 0;
  nsIFrame* prevKidFrame = nsnull;
  nsSize    kidMaxElementSize(0,0);
  nsSize*   pKidMaxElementSize = (nsnull != aDesiredSize.maxElementSize) ? &kidMaxElementSize : nsnull;
  nsresult  rv = NS_OK;

  nsReflowReason reason;
  if (PR_FALSE==RequiresPass1Layout())
  {
    reason = aReflowState.reflowState.reason;
    if (eReflowReason_Incremental==reason) {
      reason = eReflowReason_Resize;
      if (aDesiredSize.maxElementSize) {
        aDesiredSize.maxElementSize->width = 0;
        aDesiredSize.maxElementSize->height = 0;
      }
    }
  }
  else
    reason = eReflowReason_Resize;

  // this never passes reflows down to colgroups
  for (nsIFrame* kidFrame = mFrames.FirstChild(); nsnull != kidFrame; ) 
  {
    nsSize              kidAvailSize(aReflowState.availSize);
    nsHTMLReflowMetrics desiredSize(pKidMaxElementSize);
    desiredSize.width=desiredSize.height=desiredSize.ascent=desiredSize.descent=0;

    const nsStyleDisplay *childDisplay;
    kidFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)childDisplay));
    if (PR_TRUE==IsRowGroup(childDisplay->mDisplay))
    {
      // Keep track of the first body section row group
      if (nsnull == aReflowState.firstBodySection) {
        if (NS_STYLE_DISPLAY_TABLE_ROW_GROUP == childDisplay->mDisplay) {
          aReflowState.firstBodySection = kidFrame;
        }
      }

      nsMargin borderPadding;
      GetTableBorderForRowGroup(GetRowGroupFrameFor(kidFrame, childDisplay), borderPadding);
      const nsStyleSpacing* tableSpacing;
      GetStyleData(eStyleStruct_Spacing, ((const nsStyleStruct *&)tableSpacing));
      nsMargin padding;
      tableSpacing->GetPadding(padding);
      borderPadding += padding;

      // Reflow the child into the available space
      nsHTMLReflowState  kidReflowState(aPresContext, aReflowState.reflowState,
                                        kidFrame, kidAvailSize, reason);
      if (aReflowState.firstBodySection && (kidFrame != aReflowState.firstBodySection)) {
        // If this isn't the first row group frame or the header or footer, then
        // we can't be at the top of the page anymore...
        kidReflowState.isTopOfPage = PR_FALSE;
      }

      nscoord x = borderPadding.left;
      nscoord y = borderPadding.top + aReflowState.y;
      
      if (RowGroupsShouldBeConstrained()) {
        // Only applies to the tree widget.
        nscoord tableSpecifiedHeight;
        GetTableSpecifiedHeight(tableSpecifiedHeight, aReflowState.reflowState);
        if (tableSpecifiedHeight != -1) {
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
      if (PR_TRUE==IsRowGroup(childDisplay->mDisplay))
      {
        // we don't want to adjust the maxElementSize if this is an initial reflow
        // it was set by the TableLayoutStrategy and shouldn't be changed.
        nsSize *requestedMaxElementSize = nsnull;
        if (eReflowReason_Initial != aReflowState.reflowState.reason)
          requestedMaxElementSize = aDesiredSize.maxElementSize;
        PlaceChild(aPresContext, aReflowState, kidFrame, desiredSize,
                   x, y, requestedMaxElementSize, kidMaxElementSize);
      } else {
        kidFrame->DidReflow(aPresContext, NS_FRAME_REFLOW_FINISHED);
      }
      childCount++;

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
    else
    {// it's an unknown frame type, give it a generic reflow and ignore the results
        nsHTMLReflowState kidReflowState(aPresContext,
                                         aReflowState.reflowState, kidFrame,
                                         nsSize(0,0), eReflowReason_Resize);
        nsHTMLReflowMetrics unusedDesiredSize(nsnull);
        ReflowChild(kidFrame, aPresContext, unusedDesiredSize, kidReflowState,
                    0, 0, 0, aStatus);
        kidFrame->DidReflow(aPresContext, NS_FRAME_REFLOW_FINISHED);
    }

    // Get the next child
    kidFrame->GetNextSibling(&kidFrame);
  }

  // Update the child count
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
  NS_ASSERTION(nsnull!=mCellMap, "never ever call me until the cell map is built!");

  PRInt32 numCols = mCellMap->GetColCount();
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

  PRInt32 maxWidth = aMaxSize.width;
  const nsStylePosition* position =
    (const nsStylePosition*)mStyleContext->GetStyleData(eStyleStruct_Position);
  if (eStyleUnit_Coord==position->mWidth.GetUnit()) 
  {
    nscoord coordWidth=0;
    coordWidth = position->mWidth.GetCoordValue();
    // NAV4 compatibility:  0-coord-width == auto-width
    if (0!=coordWidth)
      maxWidth = coordWidth;
  }

  if (0>maxWidth)  // nonsense style specification
    maxWidth = 0;

  // based on the compatibility mode, create a table layout strategy
  if (nsnull == mTableLayoutStrategy) {
    nsCompatibility mode;
    aPresContext->GetCompatibilityMode(&mode);
    if (PR_FALSE==RequiresPass1Layout())
      mTableLayoutStrategy = new FixedTableLayoutStrategy(this);
    else
      mTableLayoutStrategy = new BasicTableLayoutStrategy(this, eCompatibility_NavQuirks == mode);
    mTableLayoutStrategy->Initialize(aMaxElementSize, GetColCount(), aReflowState.mComputedWidth);
    mBits.mColumnWidthsValid=PR_TRUE;
  }
  // fixed-layout tables need to reinitialize the layout strategy. When there are scroll bars
  // reflow gets called twice and the 2nd time has the correct space available.
  else if (!RequiresPass1Layout()) {
    mTableLayoutStrategy->Initialize(aMaxElementSize, GetColCount(), aReflowState.mComputedWidth);
  }

  mTableLayoutStrategy->BalanceColumnWidths(mStyleContext, aReflowState, maxWidth);
  //Dump(PR_TRUE, PR_TRUE);
  mBits.mColumnWidthsSet=PR_TRUE;

  // if collapsing borders, compute the top and bottom edges now that we have column widths
  if ((NS_STYLE_BORDER_COLLAPSE == GetBorderCollapseStyle()) && mBorderCollapser) {
    mBorderCollapser->ComputeHorizontalBorders(aPresContext, 0, mCellMap->GetRowCount()-1);
  }
}

/**
  sum the width of each column
  add in table insets
  set rect
  */
void nsTableFrame::SetTableWidth(nsIPresContext* aPresContext)
{
  NS_ASSERTION(nsnull==mPrevInFlow, "never ever call me on a continuing frame!");
  NS_ASSERTION(nsnull!=mCellMap, "never ever call me until the cell map is built!");

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
  const nsStyleSpacing* spacing =
    (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);
  nsMargin borderPadding;
  GetTableBorder (borderPadding); // this gets the max border value at every edge
  nsMargin padding;
  spacing->GetPadding(padding);
  borderPadding += padding;

  nscoord rightInset = borderPadding.right;
  nscoord leftInset = borderPadding.left;
  tableWidth += (leftInset + rightInset);
  nsRect tableSize = mRect;
  tableSize.width = tableWidth;
    
  // account for scroll bars. XXX needs optimization/caching
  if (mBits.mHasScrollableRowGroup) {
    float sbWidth, sbHeight;
    nsCOMPtr<nsIDeviceContext> dc;
    aPresContext->GetDeviceContext(getter_AddRefs(dc));
    dc->GetScrollBarDimensions(sbWidth, sbHeight);
    tableSize.width += NSToCoordRound(sbWidth);
  }
  SetRect(aPresContext, tableSize);
}

// XXX percentage based margin/border/padding
nscoord GetVerticalMarginBorderPadding(nsIFrame* aFrame, const nsIID& aIID)
{
  nscoord result = 0;
  if (!aFrame) {
    return result;
  }
  nsCOMPtr<nsIContent> iContent;
  nsresult rv = aFrame->GetContent(getter_AddRefs(iContent));
  if (NS_SUCCEEDED(rv)) {
    nsIHTMLContent* htmlContent = nsnull;
    rv = iContent->QueryInterface(aIID, (void **)&htmlContent);  
    if (htmlContent && NS_SUCCEEDED(rv)) { 
      nsIStyleContext* styleContext;
      aFrame->GetStyleContext(&styleContext);
      const nsStyleSpacing* spacing =
        (const nsStyleSpacing*)styleContext->GetStyleData(eStyleStruct_Spacing);
	    nsMargin margin(0,0,0,0);
      if (spacing->GetMargin(margin)) {
        result += margin.top + margin.bottom;
      }
      if (spacing->GetBorderPadding(margin)) {
        result += margin.top + margin.bottom;
      }
      NS_RELEASE(htmlContent);
    }
  }
  return result;
}

/* Get the height of the nearest ancestor of this table which has a height other than
 * auto, except when there is an ancestor which is a table and that table does not have
 * a coord height. It can be the case that the nearest such ancestor is a scroll frame
 * or viewport frame; this provides backwards compatibility with Nav4.X and IE.
 */
nscoord nsTableFrame::GetEffectiveContainerHeight(const nsHTMLReflowState& aReflowState)
{
  nsIFrame* lastArea  = nsnull;
  nsIFrame* lastBlock = nsnull;
  nsIAtom*  frameType = nsnull;
  nscoord result = -1;
  const nsHTMLReflowState* rs = &aReflowState;

  while (rs) {
    const nsStyleDisplay* display;
    rs->frame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct *&)display);
    if (NS_STYLE_DISPLAY_TABLE == display->mDisplay) {
      const nsStylePosition* position;
      rs->frame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct *&)position);
      nsStyleUnit unit = position->mHeight.GetUnit();
      if ((eStyleUnit_Null == unit) || (eStyleUnit_Auto == unit)) {
        result = 0;
        break;
      }
    }
    if (NS_AUTOHEIGHT != rs->mComputedHeight) {
      result = rs->mComputedHeight;
      // if we get to the scroll frame or viewport frame, then subtract out 
      // margin/border/padding for the HTML and BODY elements
      rs->frame->GetFrameType(&frameType);
      if ((nsLayoutAtoms::viewportFrame == frameType) || 
          (nsLayoutAtoms::scrollFrame   == frameType)) {
        result -= GetVerticalMarginBorderPadding(lastArea,  kIHTMLElementIID); 
        result -= GetVerticalMarginBorderPadding(lastBlock, kIBodyElementIID);
      }
      NS_IF_RELEASE(frameType);
      break;
    }
    // keep track of the area and block frame on the way up because they could
    // be the HTML and BODY elements
    rs->frame->GetFrameType(&frameType);
    if (nsLayoutAtoms::areaFrame == frameType) {
      lastArea = rs->frame;
    } 
    else if (nsLayoutAtoms::blockFrame == frameType) {
      lastBlock = rs->frame;
    }
    NS_IF_RELEASE(frameType);
    
    // XXX: evil cast!
    rs = (nsHTMLReflowState *)(rs->parentReflowState);
  }
  NS_ASSERTION(-1 != result, "bad state:  no constrained height in reflow chain");
  return result;
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
                                  const nscoord&           aSumOfRowHeights,
                                  const nscoord&           aExcess, 
                                  const nsStyleTable*      aTableStyle, 
                                  nscoord&                 aExcessForRowGroup, 
                                  nscoord&                 aRowGroupYPos)
{
  // the rows in rowGroupFrame need to be expanded by rowHeightDelta[i]
  // and the rowgroup itself needs to be expanded by SUM(row height deltas)
  nsTableRowGroupFrame* rowGroupFrame = (nsTableRowGroupFrame*)aRowGroupFrame;
  nsIFrame * rowFrame = rowGroupFrame->GetFirstFrame();
  nscoord y = 0;
  while (nsnull!=rowFrame)
  {
    const nsStyleDisplay *rowDisplay;
    rowFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)rowDisplay));
    if (NS_STYLE_DISPLAY_TABLE_ROW_GROUP == rowDisplay->mDisplay) {
      DistributeSpaceToRows(aPresContext, aReflowState, rowFrame, aSumOfRowHeights, 
                            aExcess, aTableStyle, aExcessForRowGroup, y);
    }
    else if (NS_STYLE_DISPLAY_TABLE_ROW == rowDisplay->mDisplay)
    { // the row needs to be expanded by the proportion this row contributed to the original height
      nsRect rowRect;
      rowFrame->GetRect(rowRect);
      float percent = ((float)(rowRect.height)) / ((float)(aSumOfRowHeights));
      nscoord excessForRow = NSToCoordRound((float)aExcess*percent);

      if (rowGroupFrame->RowsDesireExcessSpace()) {
        nsRect newRowRect(rowRect.x, y, rowRect.width, excessForRow+rowRect.height);
        rowFrame->SetRect(aPresContext, newRowRect);
        if ((NS_STYLE_BORDER_COLLAPSE == GetBorderCollapseStyle()) && mBorderCollapser) {
          PRInt32 rowIndex = ((nsTableRowFrame*)rowFrame)->GetRowIndex();
          mBorderCollapser->SetBorderEdgeLength(NS_SIDE_LEFT,  rowIndex, newRowRect.height);
          mBorderCollapser->SetBorderEdgeLength(NS_SIDE_RIGHT, rowIndex, newRowRect.height);
        }
        // better if this were part of an overloaded row::SetRect
        y += excessForRow+rowRect.height;
      }

      aExcessForRowGroup += excessForRow;
    }
    else
    {
      nsRect rowRect;
      rowFrame->GetRect(rowRect);
      y += rowRect.height;
    }

    rowGroupFrame->GetNextFrame(rowFrame, &rowFrame);
  }

  nsRect rowGroupRect;
  aRowGroupFrame->GetRect(rowGroupRect);  
  if (rowGroupFrame->RowGroupDesiresExcessSpace()) {
    nsRect newRowGroupRect(rowGroupRect.x, aRowGroupYPos, rowGroupRect.width, aExcessForRowGroup+rowGroupRect.height);
    aRowGroupFrame->SetRect(aPresContext, newRowGroupRect);
    aRowGroupYPos += aExcessForRowGroup + rowGroupRect.height;
  }
  else aRowGroupYPos += rowGroupRect.height;

  DistributeSpaceToCells(aPresContext, aReflowState, aRowGroupFrame);
}

NS_IMETHODIMP nsTableFrame::GetTableSpecifiedHeight(nscoord&                 aResult, 
                                                    const nsHTMLReflowState& aReflowState)
{
  const nsStylePosition* tablePosition;
  GetStyleData(eStyleStruct_Position, (const nsStyleStruct *&)tablePosition);
  
  const nsStyleTable* tableStyle;
  GetStyleData(eStyleStruct_Table, (const nsStyleStruct *&)tableStyle);
  nscoord tableSpecifiedHeight = -1;
  if (aReflowState.mComputedHeight != NS_UNCONSTRAINEDSIZE &&
      aReflowState.mComputedHeight > 0)
    tableSpecifiedHeight = aReflowState.mComputedHeight;
  else if (eStyleUnit_Coord == tablePosition->mHeight.GetUnit())
    tableSpecifiedHeight = tablePosition->mHeight.GetCoordValue();
  else if (eStyleUnit_Percent == tablePosition->mHeight.GetUnit()) {
    float percent = tablePosition->mHeight.GetPercentValue();
    nscoord parentHeight = GetEffectiveContainerHeight(aReflowState);
    if ((NS_UNCONSTRAINEDSIZE != parentHeight) && (0 != parentHeight))
      tableSpecifiedHeight = NSToCoordRound((float)parentHeight * percent);
  }
  aResult = tableSpecifiedHeight;
  return NS_OK;
}

nscoord nsTableFrame::ComputeDesiredHeight(nsIPresContext*          aPresContext,
                                           const nsHTMLReflowState& aReflowState, 
                                           nscoord                  aDefaultHeight) 
{
  NS_ASSERTION(mCellMap, "never ever call me until the cell map is built!");
  nscoord result = aDefaultHeight;

  nscoord tableSpecifiedHeight;
  GetTableSpecifiedHeight(tableSpecifiedHeight, aReflowState);
  if (-1 != tableSpecifiedHeight) {
    if (tableSpecifiedHeight > aDefaultHeight) { 
      // proportionately distribute the excess height to each row
      result = tableSpecifiedHeight;
      nscoord excess = tableSpecifiedHeight - aDefaultHeight;
      nscoord sumOfRowHeights = 0;
      nscoord rowGroupYPos = 0;
      nsIFrame* childFrame = mFrames.FirstChild();
      nsIFrame* firstRowGroupFrame = nsnull;
      while (nsnull != childFrame) {
        const nsStyleDisplay* childDisplay;
        childFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)childDisplay));
        if (IsRowGroup(childDisplay->mDisplay)) {
          if (((nsTableRowGroupFrame*)childFrame)->RowGroupReceivesExcessSpace()) { 
            ((nsTableRowGroupFrame*)childFrame)->GetHeightOfRows(sumOfRowHeights);
          }
          if (!firstRowGroupFrame) {
            // the first row group's y position starts inside our padding
            const nsStyleSpacing* spacing =
              (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);
	          nsMargin margin(0,0,0,0);
            if (spacing->GetBorder(margin)) { // XXX see bug 10636 and handle percentages
              rowGroupYPos = margin.top;
            }
            if (spacing->GetPadding(margin)) { // XXX see bug 10636 and handle percentages
              rowGroupYPos += margin.top;
            }
            firstRowGroupFrame = childFrame;
          }
        }
        childFrame->GetNextSibling(&childFrame);
      }

      childFrame = mFrames.FirstChild();
      while (childFrame) {
        const nsStyleDisplay* childDisplay;
        childFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)childDisplay));
        if (IsRowGroup(childDisplay->mDisplay)) {
          if (((nsTableRowGroupFrame*)childFrame)->RowGroupReceivesExcessSpace()) {
            nscoord excessForGroup = 0;
            const nsStyleTable* tableStyle;
            GetStyleData(eStyleStruct_Table, (const nsStyleStruct *&)tableStyle);
            DistributeSpaceToRows(aPresContext, aReflowState, childFrame, sumOfRowHeights, 
                                  excess, tableStyle, excessForGroup, rowGroupYPos);
          }
          else {
            nsRect rowGroupRect;
            childFrame->GetRect(rowGroupRect);
            rowGroupYPos += rowGroupRect.height;
          }
        }
        childFrame->GetNextSibling(&childFrame);
      }
    }
  }
  return result;
}

void nsTableFrame::AdjustColumnsForCOLSAttribute()
{
// XXX this is not right, 
#if 0
  nsCellMap *cellMap = GetCellMap();
  NS_ASSERTION(nsnull!=cellMap, "bad cell map");
  
  // any specified-width column turns off COLS attribute
  nsStyleTable* tableStyle = (nsStyleTable *)mStyleContext->GetMutableStyleData(eStyleStruct_Table);
  if (tableStyle->mCols != NS_STYLE_TABLE_COLS_NONE)
  {
    PRInt32 numCols = cellMap->GetColCount();
    PRInt32 numRows = cellMap->GetRowCount();
    for (PRInt32 rowIndex=0; rowIndex<numRows; rowIndex++)
    {
      for (PRInt32 colIndex=0; colIndex<numCols; colIndex++)
      {
        nsTableCellFrame *cellFrame = cellMap->GetCellFrameAt(rowIndex, colIndex);
        // get the cell style info
        const nsStylePosition* cellPosition;
        if (nsnull!=cellFrame)
        {
          cellFrame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct *&)cellPosition);
          if ((eStyleUnit_Coord == cellPosition->mWidth.GetUnit()) ||
               (eStyleUnit_Percent==cellPosition->mWidth.GetUnit())) 
          {
            tableStyle->mCols = NS_STYLE_TABLE_COLS_NONE;
            break;
          }
        }
      }
    }
  }
#endif 
}


/* there's an easy way and a hard way.  The easy way is to look in our
 * cache and pull the frame from there.
 * If the cache isn't built yet, then we have to go hunting.
 */
NS_METHOD nsTableFrame::GetColumnFrame(PRInt32 aColIndex, nsTableColFrame *&aColFrame)
{
  aColFrame = nsnull; // initialize out parameter
  nsCellMap *cellMap = GetCellMap();
  if (nsnull!=cellMap)
  { // hooray, we get to do this the easy way because the info is cached
    aColFrame = cellMap->GetColumnFrame(aColIndex);
  }
  else
  { // ah shucks, we have to go hunt for the column frame brute-force style
    nsIFrame *childFrame = mColGroups.FirstChild();
    for (;;)
    {
      if (nsnull==childFrame)
      {
        NS_ASSERTION (PR_FALSE, "scanned the frame hierarchy and no column frame could be found.");
        break;
      }
      PRInt32 colGroupStartingIndex = ((nsTableColGroupFrame *)childFrame)->GetStartColumnIndex();
      if (aColIndex >= colGroupStartingIndex)
      { // the cell's col might be in this col group
        PRInt32 colCount = ((nsTableColGroupFrame *)childFrame)->GetColumnCount();
        if (aColIndex < colGroupStartingIndex + colCount)
        { // yep, we've found it.  GetColumnAt gives us the column at the offset colCount, not the absolute colIndex for the whole table
          aColFrame = ((nsTableColGroupFrame *)childFrame)->GetColumnAt(colCount);
          break;
        }
      }
      childFrame->GetNextSibling(&childFrame);
    }
  }
  return NS_OK;
}

PRBool nsTableFrame::IsColumnWidthsSet()
{ 
  nsTableFrame * firstInFlow = (nsTableFrame *)GetFirstInFlow();
  NS_ASSERTION(nsnull!=firstInFlow, "illegal state -- no first in flow");
  return (PRBool)firstInFlow->mBits.mColumnWidthsSet; 
}


void nsTableFrame::CacheColFrames(nsIPresContext* aPresContext,
								  PRBool          aReset)
{
  NS_ASSERTION(nsnull==mPrevInFlow, "never ever call me on a continuing frame!");
  NS_ASSERTION(nsnull!=mCellMap, "never ever call me until the cell map is built!");

  if (aReset) {
    PRBool createdColFrames;
    EnsureColumns(aPresContext, createdColFrames);
    mCellMap->ClearColumnCache();
  }
 
  nsIFrame * childFrame = mColGroups.FirstChild();
  while (nsnull != childFrame) { // in this loop, we cache column info 
    nsTableColFrame* colFrame = nsnull;
    childFrame->FirstChild(nsnull, (nsIFrame **)&colFrame);
    while (nsnull != colFrame) {
      const nsStyleDisplay* colDisplay;
      colFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)colDisplay));
      if (NS_STYLE_DISPLAY_TABLE_COLUMN == colDisplay->mDisplay) {
        PRInt32 colIndex = colFrame->GetColumnIndex();
        PRInt32 repeat   = colFrame->GetSpan();
        for (PRInt32 i=0; i<repeat; i++) {
          nsTableColFrame* cachedColFrame = mCellMap->GetColumnFrame(colIndex+i);
          if (nsnull==cachedColFrame) {
            mCellMap->AppendColumnFrame(colFrame);
          }
          colIndex++;
        }
      }
      colFrame->GetNextSibling((nsIFrame **)&colFrame);
    }
    childFrame->GetNextSibling(&childFrame);
  }

  mBits.mColumnCacheValid=PR_TRUE;
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

PRBool nsTableFrame::IsColumnCacheValid() const
{
  nsTableFrame * firstInFlow = (nsTableFrame *)GetFirstInFlow();
  NS_ASSERTION(nsnull!=firstInFlow, "illegal state -- no first in flow");
  return (PRBool)firstInFlow->mBits.mColumnCacheValid;
}

void nsTableFrame::InvalidateColumnCache()
{
  nsTableFrame * firstInFlow = (nsTableFrame *)GetFirstInFlow();
  NS_ASSERTION(nsnull!=firstInFlow, "illegal state -- no first in flow");
  firstInFlow->mBits.mColumnCacheValid=PR_FALSE;
}

PRBool nsTableFrame::IsCellMapValid() const
{
  nsTableFrame * firstInFlow = (nsTableFrame *)GetFirstInFlow();
  NS_ASSERTION(nsnull!=firstInFlow, "illegal state -- no first in flow");
  return (PRBool)firstInFlow->mBits.mCellMapValid;
}

static void InvalidateCellMapForRowGroup(nsIFrame* aRowGroupFrame)
{
  nsIFrame *rowFrame;
  aRowGroupFrame->FirstChild(nsnull, &rowFrame);
  for ( ; nsnull!=rowFrame; rowFrame->GetNextSibling(&rowFrame))
  {
    const nsStyleDisplay *rowDisplay;
    rowFrame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct *&)rowDisplay);
    if (NS_STYLE_DISPLAY_TABLE_ROW==rowDisplay->mDisplay)
    {
      ((nsTableRowFrame *)rowFrame)->ResetInitChildren();
    }
    else if (NS_STYLE_DISPLAY_TABLE_ROW_GROUP==rowDisplay->mDisplay)
    {
      InvalidateCellMapForRowGroup(rowFrame);
    }
  }

}

void nsTableFrame::InvalidateCellMap()
{
  nsTableFrame * firstInFlow = (nsTableFrame *)GetFirstInFlow();
  NS_ASSERTION(nsnull!=firstInFlow, "illegal state -- no first in flow");
  firstInFlow->mBits.mCellMapValid=PR_FALSE;
  // reset the state in each row
  nsIFrame *rowGroupFrame=mFrames.FirstChild();
  for ( ; nsnull!=rowGroupFrame; rowGroupFrame->GetNextSibling(&rowGroupFrame))
  {
    const nsStyleDisplay *rowGroupDisplay;
    rowGroupFrame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct *&)rowGroupDisplay);
    if (PR_TRUE==IsRowGroup(rowGroupDisplay->mDisplay))
    {
      InvalidateCellMapForRowGroup(rowGroupFrame);
    }
  }
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
    NS_ASSERTION(nsnull!=mCellMap, "no cell map");
    PRInt32 numCols = mCellMap->GetColCount();
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
      mColumnWidthsLength = mCellMap->GetColCount();
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


NS_METHOD nsTableFrame::GetCellMarginData(nsTableCellFrame* aKidFrame, nsMargin& aMargin)
{
  nsresult result = NS_ERROR_NOT_INITIALIZED;

  if (aKidFrame) {
    nscoord spacingX = GetCellSpacingX();
    nscoord spacingY = GetCellSpacingY();
    PRInt32 rowIndex, colIndex;
    aKidFrame->GetRowIndex(rowIndex);
    aKidFrame->GetColIndex(colIndex);
    // left/top margins only apply if the cell is a left/top border cell
    // there used to be more complicated logic (rev 3.326) dealing with rowspans 
    // but failure to produce reasonable tests cases does not justify it.
    aMargin.left   = (0 == colIndex) ? spacingX : 0;
    aMargin.top    = (0 == rowIndex) ? spacingY : 0;
    aMargin.right  = spacingX;
    aMargin.bottom = spacingY;
    result = NS_OK;
  }

  return result;
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
        nsCellMap* cellMap = GetCellMap();
        PRInt32 colCount = cellMap->GetColCount();
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
  const nsStyleTable* tableStyle;
  GetStyleData(eStyleStruct_Table, (const nsStyleStruct *&)tableStyle);
  return tableStyle->mBorderCollapse;
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
NS_NewTableFrame(nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsTableFrame* it = new nsTableFrame;
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
PRBool nsTableFrame::IsNested(const nsHTMLReflowState& aReflowState, const nsStylePosition *& aPosition) const
{
  PRBool result = PR_FALSE;
  // Walk up the reflow state chain until we find a cell or the root
  const nsHTMLReflowState* rs = aReflowState.parentReflowState; // this is for the outer frame
  if (rs)
    rs = rs->parentReflowState;  // and this is the parent of the outer frame
  while (nsnull != rs) 
  {
    const nsStyleDisplay *display;
    rs->frame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct *&)display);
    if (NS_STYLE_DISPLAY_TABLE==display->mDisplay)
    {
      result = PR_TRUE;
      rs->frame->GetStyleData(eStyleStruct_Position, ((const nsStyleStruct *&)aPosition));
      break;
    }
    rs = rs->parentReflowState;
  }
  return result;
}


// aSpecifiedTableWidth is filled if the table witdth is not auto
PRBool nsTableFrame::IsAutoWidth(const nsHTMLReflowState& aReflowState,
                                 nscoord&                 aSpecifiedTableWidth)
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
    if ((aReflowState.mComputedWidth > 0) &&
        (aReflowState.mComputedWidth != NS_UNCONSTRAINEDSIZE)) {
      aSpecifiedTableWidth = aReflowState.mComputedWidth;
    }
    isAuto = PR_FALSE;
    break;
  default:
    break;
  }

  return isAuto; 
}


nscoord nsTableFrame::GetMinCaptionWidth()
{
  nsIFrame *outerTableFrame=nsnull;
  GetParent(&outerTableFrame);
  return (((nsTableOuterFrame *)outerTableFrame)->GetMinCaptionWidth());
}

/** return the minimum width of the table.  Return 0 if the min width is unknown. */
nscoord nsTableFrame::GetMinTableContentWidth()
{
  nscoord result = 0;
  if (nsnull!=mTableLayoutStrategy)
    result = mTableLayoutStrategy->GetTableMinContentWidth();
  return result;
}

/** return the maximum width of the table.  Return 0 if the max width is unknown. */
nscoord nsTableFrame::GetMaxTableContentWidth()
{
  nscoord result = 0;
  if (nsnull!=mTableLayoutStrategy)
    result = mTableLayoutStrategy->GetTableMaxContentWidth();
  return result;
}

void nsTableFrame::SetMaxElementSize(nsSize* aMaxElementSize)
{
  if (nsnull!=mTableLayoutStrategy)
    mTableLayoutStrategy->SetMaxElementSize(aMaxElementSize);
}


PRBool nsTableFrame::RequiresPass1Layout()
{
  const nsStyleTable* tableStyle;
  GetStyleData(eStyleStruct_Table, (const nsStyleStruct *&)tableStyle);
  return (PRBool)(NS_STYLE_TABLE_LAYOUT_FIXED!=tableStyle->mLayoutStrategy);
}

#ifdef DEBUG
NS_IMETHODIMP
nsTableFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("Table", aResult);
}
#endif

// This assumes that aFrame is a scroll frame if  
// XXX make this a macro if it becomes an issue
// XXX it has the side effect of setting mHasScrollableRowGroup
nsTableRowGroupFrame*
nsTableFrame::GetRowGroupFrameFor(nsIFrame* aFrame, const nsStyleDisplay* aDisplay) 
{
  nsIFrame* result = nsnull;
  if (IsRowGroup(aDisplay->mDisplay)) {
    nsresult rv = aFrame->QueryInterface(kITableRowGroupFrameIID, (void **)&result);
    if (NS_SUCCEEDED(rv) && (nsnull != result)) {
      ;
    } else { // it is a scroll frame that contains the row group frame
      aFrame->FirstChild(nsnull, &result);
      mBits.mHasScrollableRowGroup = PR_TRUE;
    }
  }

  return (nsTableRowGroupFrame*)result;
}

PRBool
nsTableFrame::IsFinalPass(const nsHTMLReflowState& aState) 
{
  return (NS_UNCONSTRAINEDSIZE != aState.availableWidth) ||
         (NS_UNCONSTRAINEDSIZE != aState.availableHeight);
}

void nsTableFrame::Dump(PRBool aDumpCols, PRBool aDumpCellMap)
{
  printf("***START TABLE DUMP***, \n colWidths=");
  PRInt32 colX;
  PRInt32 numCols = GetColCount();
  for (colX = 0; colX < numCols; colX++) {
    printf("%d ", mColumnWidths[colX]);
  }
  if (aDumpCols) {
    for (colX = 0; colX < numCols; colX++) {
      printf("\n");
      nsTableColFrame* colFrame = GetColFrame(colX);
      colFrame->Dump(1);
    }
  }
  if (aDumpCellMap) {
    printf("\n");
    nsCellMap* cellMap = GetCellMap();
#ifdef NS_DEBUG
    cellMap->Dump();
#endif
  }
  printf(" ***END TABLE DUMP*** \n");
}

// nsTableIterator
nsTableIterator::nsTableIterator(nsIFrame&        aSource,
                                 nsTableIteration aType)
{
  nsIFrame* firstChild;
  aSource.FirstChild(nsnull, &firstChild);
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
  nsCellMap* cellMap = GetCellMap();
  return cellMap->GetCellInfoAt(aRowX, aColX, aOriginates, aColSpan);
}

/*------------------ nsITableLayout methods ------------------------------*/
NS_IMETHODIMP 
nsTableFrame::GetCellDataAt(PRInt32 aRowIndex, PRInt32 aColIndex,
                            nsIDOMElement* &aCell,   //out params
                            PRInt32& aStartRowIndex, PRInt32& aStartColIndex, 
                            PRInt32& aRowSpan, PRInt32& aColSpan,
                            PRBool& aIsSelected)
{
  nsresult result;
  nsCellMap* cellMap = GetCellMap();
  // Initialize out params
  aCell = nsnull;
  aStartRowIndex = 0;
  aStartColIndex = 0;
  aRowSpan = 0;
  aColSpan = 0;
  aIsSelected = PR_FALSE;

  if (!cellMap) { return NS_ERROR_NOT_INITIALIZED;}

  // Return a special error value if an index is out of bounds
  // This will pass the NS_SUCCEEDED() test
  // Thus we can iterate indexes to get all cells in a row or col
  //   and stop when aCell is returned null.
  PRInt32 rowCount = cellMap->GetRowCount();
  PRInt32 colCount = cellMap->GetColCount();

  if (aRowIndex >= rowCount || aColIndex >= colCount)
  {
    return NS_TABLELAYOUT_CELL_NOT_FOUND;
  }
  nsTableCellFrame *cellFrame = cellMap->GetCellFrameOriginatingAt(aRowIndex, aColIndex);
  if (!cellFrame)
  { 
    PRInt32 rowSpan, colSpan;
    PRInt32 row = aStartRowIndex;
    PRInt32 col = aStartColIndex;

    // We didn't find a cell at requested location,
    //  most probably because of ROWSPAN and/or COLSPAN > 1
    // Find the cell that extends into the location we requested,
    //  starting at the most likely indexes supplied by the caller
    //  in aStartRowIndex and aStartColIndex;
    cellFrame = cellMap->GetCellFrameOriginatingAt(row, col);
    if (cellFrame)
    {
      rowSpan = cellFrame->GetRowSpan();
      colSpan = cellFrame->GetColSpan();

      // Check if this extends into the location we want
      if( aRowIndex >= row && aRowIndex < row+rowSpan && 
          aColIndex >= col && aColIndex < col+colSpan) 
      {
CELL_FOUND:
        aStartRowIndex = row;
        aStartColIndex = col;
        aRowSpan = rowSpan;
        aColSpan = colSpan;
        // I know jumps aren't cool, but it's efficient!
        goto TEST_IF_SELECTED;
      } else {
        // Suggested indexes didn't work,
        // Scan through entire table to find the spanned cell
        for (row = 0; row < rowCount; row++ )
        {
          for (col = 0; col < colCount; col++)
          {
            cellFrame = cellMap->GetCellFrameOriginatingAt(row, col);
            if (cellFrame)
            {
              rowSpan = cellFrame->GetRowSpan();
              colSpan = cellFrame->GetColSpan();
              if( aRowIndex >= row && aRowIndex < row+rowSpan && 
                  aColIndex >= col && aColIndex < col+colSpan) 
              {
                goto CELL_FOUND;
              }
            }
          }
          col = 0;
        }
      }
      return NS_ERROR_FAILURE; // Some other error 
    }
  }

  result = cellFrame->GetRowIndex(aStartRowIndex);
  if (NS_FAILED(result)) return result;
  result = cellFrame->GetColIndex(aStartColIndex);
  if (NS_FAILED(result)) return result;
  aRowSpan = cellFrame->GetRowSpan();
  aColSpan = cellFrame->GetColSpan();
  result = cellFrame->GetSelected(&aIsSelected);
  if (NS_FAILED(result)) return result;

TEST_IF_SELECTED:
  // do this last, because it addrefs, 
  // and we don't want the caller leaking it on error
  nsCOMPtr<nsIContent>content;
  result = cellFrame->GetContent(getter_AddRefs(content));  
  if (NS_SUCCEEDED(result) && content)
  {
    content->QueryInterface(nsIDOMElement::GetIID(), (void**)(&aCell));
  }   
                                         
  return result;
}

NS_IMETHODIMP nsTableFrame::GetTableSize(PRInt32& aRowCount, PRInt32& aColCount)
{
  nsCellMap* cellMap = GetCellMap();
  // Initialize out params
  aRowCount = 0;
  aColCount = 0;
  if (!cellMap) { return NS_ERROR_NOT_INITIALIZED;}

  aRowCount = cellMap->GetRowCount();
  aColCount = cellMap->GetColCount();
  return NS_OK;
}

/*---------------- end of nsITableLayout implementation ------------------*/

PRInt32 nsTableFrame::GetNumCellsOriginatingInRow(PRInt32 aRowIndex) const
{
  nsCellMap* cellMap = GetCellMap();
  return cellMap->GetNumCellsOriginatingInRow(aRowIndex);
}

PRInt32 nsTableFrame::GetNumCellsOriginatingInCol(PRInt32 aColIndex) const
{
  nsCellMap* cellMap = GetCellMap();
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
                               const nsHTMLReflowMetrics* aMetrics)
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
    printf("comp=(%s,%s) \n", width, height);
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
    printf("\n");
  }
}

PRBool nsTableFrame::RowHasSpanningCells(PRInt32 aRowIndex)
{
  PRBool result = PR_FALSE;
  nsCellMap* cellMap = GetCellMap();
  NS_PRECONDITION (cellMap, "bad call, cellMap not yet allocated.");
  if (cellMap) {
		result = cellMap->RowHasSpanningCells(aRowIndex);
  }
  return result;
}

PRBool nsTableFrame::RowIsSpannedInto(PRInt32 aRowIndex)
{
  PRBool result = PR_FALSE;
  nsCellMap* cellMap = GetCellMap();
  NS_PRECONDITION (cellMap, "bad call, cellMap not yet allocated.");
  if (cellMap) {
		result = cellMap->RowIsSpannedInto(aRowIndex);
  }
  return result;
}

PRBool nsTableFrame::ColIsSpannedInto(PRInt32 aColIndex)
{
  PRBool result = PR_FALSE;
  nsCellMap * cellMap = GetCellMap();
  NS_PRECONDITION (cellMap, "bad call, cellMap not yet allocated.");
  if (cellMap) {
		result = cellMap->ColIsSpannedInto(aColIndex);
  }
  return result;
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

  // Add in size of table layout strategy
  PRUint32 strategySize;
  mTableLayoutStrategy->SizeOf(aHandler, &strategySize);
  aHandler->AddSize(nsLayoutAtoms::tableStrategy, strategySize);

  *aResult = sum;
  return NS_OK;
}
#endif
