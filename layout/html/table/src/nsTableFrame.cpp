/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "nsCOMPtr.h"
#include "nsTableFrame.h"
#include "nsIRenderingContext.h"
#include "nsIStyleContext.h"
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

#ifdef NS_DEBUG
static PRBool gsDebug = PR_FALSE;
//static PRBool gsDebugCLD = PR_FALSE;
static PRBool gsDebugNT = PR_FALSE;
static PRBool gsDebugIR = PR_FALSE;
#else
static const PRBool gsDebug = PR_FALSE;
//static const PRBool gsDebugCLD = PR_FALSE;
static const PRBool gsDebugNT = PR_FALSE;
static const PRBool gsDebugIR = PR_FALSE;
#endif

#ifndef max
#define max(x, y) ((x) > (y) ? (x) : (y))
#endif

NS_DEF_PTR(nsIStyleContext);
NS_DEF_PTR(nsIContent);

static NS_DEFINE_IID(kITableRowGroupFrameIID, NS_ITABLEROWGROUPFRAME_IID);

static const PRInt32 kColumnWidthIncrement=100;

/* ----------- CellData ---------- */

/* CellData is the info stored in the cell map */
CellData::CellData()
{
  mCell = nsnull;
  mRealCell = nsnull;
  mOverlap = nsnull;
}

CellData::~CellData()
{}

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

  InnerTableReflowState(nsIPresContext&          aPresContext,
                        const nsHTMLReflowState& aReflowState,
                        const nsMargin&          aBorderPadding)
    : reflowState(aReflowState)
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

/* ----------- ColumnInfoCache ---------- */

static const PRInt32 NUM_COL_WIDTH_TYPES=4;

class ColumnInfoCache
{
public:
  ColumnInfoCache(PRInt32 aNumColumns);
  ~ColumnInfoCache();

  void AddColumnInfo(const nsStyleUnit aType, 
                     PRInt32 aColumnIndex);

  void GetColumnsByType(const nsStyleUnit aType, 
                        PRInt32& aOutNumColumns,
                        PRInt32 *& aOutColumnIndexes);
  enum ColWidthType {
    eColWidthType_Auto         = 0,      // width based on contents
    eColWidthType_Percent      = 1,      // (float) 1.0 == 100%
    eColWidthType_Coord        = 2,      // (nscoord) value is twips
    eColWidthType_Proportional = 3       // (int) value has proportional meaning
  };

private:
  PRInt32  mColCounts [4];
  PRInt32 *mColIndexes[4];
  PRInt32  mNumColumns;
};

ColumnInfoCache::ColumnInfoCache(PRInt32 aNumColumns)
{
  if (PR_TRUE==gsDebug || PR_TRUE==gsDebugIR) printf("CIC constructor: aNumColumns %d\n", aNumColumns);
  mNumColumns = aNumColumns;
  for (PRInt32 i=0; i<NUM_COL_WIDTH_TYPES; i++)
  {
    mColCounts[i] = 0;
    mColIndexes[i] = nsnull;
  }
}

ColumnInfoCache::~ColumnInfoCache()
{
  for (PRInt32 i=0; i<NUM_COL_WIDTH_TYPES; i++)
  {
    if (nsnull!=mColIndexes[i])
    {
      delete [] mColIndexes[i];
    }
  }
}

void ColumnInfoCache::AddColumnInfo(const nsStyleUnit aType, 
                                    PRInt32 aColumnIndex)
{
  if (PR_TRUE==gsDebug || PR_TRUE==gsDebugIR) printf("CIC AddColumnInfo: adding col index %d of type %d\n", aColumnIndex, aType);
  // a table may have more COLs than actual columns, so we guard against that here
  if (aColumnIndex<mNumColumns)
  {
    switch (aType)
    {
      case eStyleUnit_Auto:
        if (nsnull==mColIndexes[eColWidthType_Auto])
          mColIndexes[eColWidthType_Auto] = new PRInt32[mNumColumns];     // TODO : be much more efficient
        mColIndexes[eColWidthType_Auto][mColCounts[eColWidthType_Auto]] = aColumnIndex;
        mColCounts[eColWidthType_Auto]++;
        break;

      case eStyleUnit_Percent:
        if (nsnull==mColIndexes[eColWidthType_Percent])
          mColIndexes[eColWidthType_Percent] = new PRInt32[mNumColumns];     // TODO : be much more efficient
        mColIndexes[eColWidthType_Percent][mColCounts[eColWidthType_Percent]] = aColumnIndex;
        mColCounts[eColWidthType_Percent]++;
        break;

      case eStyleUnit_Coord:
        if (nsnull==mColIndexes[eColWidthType_Coord])
          mColIndexes[eColWidthType_Coord] = new PRInt32[mNumColumns];     // TODO : be much more efficient
        mColIndexes[eColWidthType_Coord][mColCounts[eColWidthType_Coord]] = aColumnIndex;
        mColCounts[eColWidthType_Coord]++;
        break;

      case eStyleUnit_Proportional:
        if (nsnull==mColIndexes[eColWidthType_Proportional])
          mColIndexes[eColWidthType_Proportional] = new PRInt32[mNumColumns];     // TODO : be much more efficient
        mColIndexes[eColWidthType_Proportional][mColCounts[eColWidthType_Proportional]] = aColumnIndex;
        mColCounts[eColWidthType_Proportional]++;
        break;

      default:
        break;
    }
  }
}


void ColumnInfoCache::GetColumnsByType(const nsStyleUnit aType, 
                                       PRInt32& aOutNumColumns,
                                       PRInt32 *& aOutColumnIndexes)
{
  // initialize out-params
  aOutNumColumns=0;
  aOutColumnIndexes=nsnull;
  
  // fill out params with column info based on aType
  switch (aType)
  {
    case eStyleUnit_Auto:
      aOutNumColumns = mColCounts[eColWidthType_Auto];
      aOutColumnIndexes = mColIndexes[eColWidthType_Auto];
      break;
    case eStyleUnit_Percent:
      aOutNumColumns = mColCounts[eColWidthType_Percent];
      aOutColumnIndexes = mColIndexes[eColWidthType_Percent];
      break;
    case eStyleUnit_Coord:
      aOutNumColumns = mColCounts[eColWidthType_Coord];
      aOutColumnIndexes = mColIndexes[eColWidthType_Coord];
      break;
    case eStyleUnit_Proportional:
      aOutNumColumns = mColCounts[eColWidthType_Proportional];
      aOutColumnIndexes = mColIndexes[eColWidthType_Proportional];
      break;

    default:
      break;
  }
  if (PR_TRUE==gsDebug || PR_TRUE==gsDebugIR) printf("CIC GetColumnsByType: found %d of type %d\n", aOutNumColumns, aType);
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
    mColumnWidthsValid(PR_FALSE),
    mFirstPassValid(PR_FALSE),
    mColumnCacheValid(PR_FALSE),
    mCellMapValid(PR_TRUE),
    mIsInvariantWidth(PR_FALSE),
    mHasScrollableRowGroup(PR_FALSE),
    mCellMap(nsnull),
    mColCache(nsnull),
    mTableLayoutStrategy(nsnull)
{
  mEffectiveColCount = -1;  // -1 means uninitialized
  mColumnWidthsSet=PR_FALSE;
  mColumnWidthsLength = kColumnWidthIncrement;  
  mColumnWidths = new PRInt32[mColumnWidthsLength];
  nsCRT::memset (mColumnWidths, 0, mColumnWidthsLength*sizeof(PRInt32));
  mCellMap = new nsCellMap(0, 0);
  mDefaultCellSpacingX=0;
  mDefaultCellSpacingY=0;
  mDefaultCellPadding=0;
  mBorderEdges.mOutsideEdge=PR_TRUE;
}

NS_IMETHODIMP
nsTableFrame::Init(nsIPresContext&  aPresContext,
                   nsIContent*      aContent,
                   nsIFrame*        aParent,
                   nsIStyleContext* aContext,
                   nsIFrame*        aPrevInFlow)
{
  nsresult  rv;
  float     p2t;

  aPresContext.GetPixelsToTwips(&p2t);
  mDefaultCellSpacingX = NSIntPixelsToTwips(2, p2t);
  mDefaultCellSpacingY = NSIntPixelsToTwips(2, p2t);
  mDefaultCellPadding = NSIntPixelsToTwips(1, p2t);

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

  return rv;
}


nsTableFrame::~nsTableFrame()
{
  if (nsnull!=mCellMap)
    delete mCellMap;

  if (nsnull!=mColumnWidths)
    delete [] mColumnWidths;

  if (nsnull!=mTableLayoutStrategy)
    delete mTableLayoutStrategy;

  if (nsnull!=mColCache)
    delete mColCache;
}

NS_IMETHODIMP
nsTableFrame::DeleteFrame(nsIPresContext& aPresContext)
{
  mColGroups.DeleteFrames(aPresContext);
  return nsHTMLContainerFrame::DeleteFrame(aPresContext);
}

NS_IMETHODIMP
nsTableFrame::SetInitialChildList(nsIPresContext& aPresContext,
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

  if (NS_SUCCEEDED(rv))
    EnsureColumns(aPresContext);

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
  }

  return rv;
}


/* ****** CellMap methods ******* */

/* counts columns in column groups */
PRInt32 nsTableFrame::GetSpecifiedColumnCount ()
{
  mColCount=0;
  nsIFrame * childFrame = mColGroups.FirstChild();
  while (nsnull!=childFrame)
  {
    mColCount += ((nsTableColGroupFrame *)childFrame)->GetColumnCount();
    childFrame->GetNextSibling(&childFrame);
  }    
  if (PR_TRUE==gsDebug) printf("TIF GetSpecifiedColumnCount: returning %d\n", mColCount);
  return mColCount;
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
  nsCellMap *cellMap = GetCellMap();
  NS_ASSERTION(nsnull!=cellMap, "GetColCount null cellmap");

  if (nsnull!=cellMap)
  {
    //if (-1==mEffectiveColCount)
      SetEffectiveColCount();
  }
  return mEffectiveColCount;
}

void nsTableFrame::SetEffectiveColCount()
{
  nsCellMap *cellMap = GetCellMap();
  NS_ASSERTION(nsnull!=cellMap, "SetEffectiveColCount null cellmap");
  if (nsnull!=cellMap)
  {
    PRInt32 colCount = cellMap->GetColCount();
    mEffectiveColCount = colCount;
    PRInt32 rowCount = cellMap->GetRowCount();
    for (PRInt32 colIndex=colCount-1; colIndex>0; colIndex--)
    {
      PRBool deleteCol=PR_TRUE;
      for (PRInt32 rowIndex=0; rowIndex<rowCount; rowIndex++)
      {
        CellData *cell = cellMap->GetCellAt(rowIndex, colIndex);
        if ((nsnull!=cell) && (cell->mCell != nsnull))
        { // found a real cell, so we're done
          deleteCol = PR_FALSE;
          break;
        }
      }
      if (PR_TRUE==deleteCol)
        mEffectiveColCount--;
      else
        break;
    }
  }
  if (PR_TRUE==gsDebug) printf("TIF SetEffectiveColumnCount: returning %d\n", mEffectiveColCount);
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
nsTableCellFrame * nsTableFrame::GetCellFrameAt(PRInt32 aRowIndex, PRInt32 aColIndex)
{
  nsTableCellFrame *result = nsnull;
  nsCellMap *cellMap = GetCellMap();
  if (nsnull!=cellMap)
  {
    CellData * cellData = cellMap->GetCellAt(aRowIndex, aColIndex);
    if (nsnull!=cellData)
    {
      result = cellData->mCell;
      if (nsnull==result)
        result = cellData->mRealCell->mCell;
    }
  }
  return result;
}

/** returns PR_TRUE if the row at aRowIndex has any cells that are the result
  * of a row-spanning cell.  
  * @see nsCellMap::RowIsSpannedInto
  */
PRBool nsTableFrame::RowIsSpannedInto(PRInt32 aRowIndex)
{
  NS_PRECONDITION (0<=aRowIndex && aRowIndex<GetRowCount(), "bad row index arg");
  PRBool result = PR_FALSE;
  nsCellMap * cellMap = GetCellMap();
  NS_PRECONDITION (nsnull!=cellMap, "bad call, cellMap not yet allocated.");
  if (nsnull!=cellMap)
  {
		result = cellMap->RowIsSpannedInto(aRowIndex);
  }
  return result;
}

/** returns PR_TRUE if the row at aRowIndex has any cells that have a rowspan>1
  * @see nsCellMap::RowHasSpanningCells
  */
PRBool nsTableFrame::RowHasSpanningCells(PRInt32 aRowIndex)
{
  NS_PRECONDITION (0<=aRowIndex && aRowIndex<GetRowCount(), "bad row index arg");
  PRBool result = PR_FALSE;
  nsCellMap * cellMap = GetCellMap();
  NS_PRECONDITION (nsnull!=cellMap, "bad call, cellMap not yet allocated.");
  if (nsnull!=cellMap)
  {
		result = cellMap->RowHasSpanningCells(aRowIndex);
  }
  return result;
}


/** returns PR_TRUE if the col at aColIndex has any cells that are the result
  * of a col-spanning cell.  
  * @see nsCellMap::ColIsSpannedInto
  */
PRBool nsTableFrame::ColIsSpannedInto(PRInt32 aColIndex)
{
  PRBool result = PR_FALSE;
  nsCellMap * cellMap = GetCellMap();
  NS_PRECONDITION (nsnull!=cellMap, "bad call, cellMap not yet allocated.");
  if (nsnull!=cellMap)
  {
		result = cellMap->ColIsSpannedInto(aColIndex);
  }
  return result;
}

/** returns PR_TRUE if the row at aColIndex has any cells that have a colspan>1
  * @see nsCellMap::ColHasSpanningCells
  */
PRBool nsTableFrame::ColHasSpanningCells(PRInt32 aColIndex)
{
  PRBool result = PR_FALSE;
  nsCellMap * cellMap = GetCellMap();
  NS_PRECONDITION (nsnull!=cellMap, "bad call, cellMap not yet allocated.");
  if (nsnull!=cellMap)
  {
		result = cellMap->ColHasSpanningCells(aColIndex);
  }
  return result;
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

// return the number of cols spanned by aCell starting at aColIndex
// note that this is different from just the colspan of aCell
// (that would be GetEffectiveColSpan (indexOfColThatContains_aCell, aCell)
//
// XXX Should be moved to colgroup, as GetEffectiveRowSpan should be moved to rowgroup?
PRInt32 nsTableFrame::GetEffectiveColSpan (PRInt32 aColIndex, nsTableCellFrame *aCell)
{
  NS_PRECONDITION (nsnull!=aCell, "bad cell arg");
  nsCellMap *cellMap = GetCellMap();
  NS_PRECONDITION (nsnull!=cellMap, "bad call, cellMap not yet allocated.");
  PRInt32 colCount = GetColCount();
  NS_PRECONDITION (0<=aColIndex && aColIndex<colCount, "bad col index arg");

  PRInt32 result;
  /*  XXX: it kind of makes sense to ignore colspans when there's only one row, but it doesn't quite work that way  :(
           it would be good to find out why
   */
  /*
  if (cellMap->GetRowCount()==1)
    return 1;
  */
  PRInt32 colSpan = aCell->GetColSpan();
  if (colCount < (aColIndex + colSpan))
    result =  colCount - aColIndex;
  else
  {
    result = colSpan;
    // check for case where all cells in a column have a colspan
    PRInt32 initialColIndex;
    aCell->GetColIndex(initialColIndex);
    PRInt32 minColSpanForCol = cellMap->GetMinColSpan(initialColIndex);
    result -= (minColSpanForCol - 1); // minColSpanForCol is always at least 1
                                      // and we want to treat default as 0 (no effect)
  }
#ifdef NS_DEBUG
  if (0>=result)
  {
    printf("ERROR!\n");
    DumpCellMap();
    PRInt32 initialColIndex;
    aCell->GetColIndex(initialColIndex);
    printf("aColIndex=%d, cell->colIndex=%d\n", aColIndex, initialColIndex);
    printf("aCell->colSpan=%d\n", aCell->GetColSpan());
    printf("colCount=%d\n", mCellMap->GetColCount());
  }
#endif
NS_ASSERTION(0<result, "bad effective col span");
  return result;
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

/** sum the columns represented by all nsTableColGroup objects
  * if the cell map says there are more columns than this, 
  * add extra implicit columns to the content tree.
  */
void nsTableFrame::EnsureColumns(nsIPresContext& aPresContext)
{
  if (PR_TRUE==gsDebug) printf("TIF EnsureColumns\n");
  NS_PRECONDITION(nsnull!=mCellMap, "bad state:  null cellmap");
  // XXX sec should only be called on firstInFlow
  SetMinColSpanForTable();
  if (nsnull==mCellMap)
    return; // no info yet, so nothing useful to do

  // make sure we've accounted for the COLS attribute
  AdjustColumnsForCOLSAttribute();

  // find the first row group
  nsIFrame * firstRowGroupFrame=mFrames.FirstChild();
  nsIFrame * childFrame=firstRowGroupFrame;
  while (nsnull!=childFrame)
  {
    const nsStyleDisplay *childDisplay;
    childFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)childDisplay));
    if (PR_TRUE==IsRowGroup(childDisplay->mDisplay))
    {
      if (nsnull==firstRowGroupFrame)
      {
        firstRowGroupFrame = childFrame;
        if (PR_TRUE==gsDebug) printf("EC: found a row group %p\n", firstRowGroupFrame);
        break;
      }
    }
    childFrame->GetNextSibling(&childFrame);
  }

  // count the number of column frames we already have
  PRInt32 actualColumns = 0;
  nsTableColGroupFrame *lastColGroupFrame = nsnull;
  childFrame = mColGroups.FirstChild();
  while (nsnull!=childFrame)
  {
    ((nsTableColGroupFrame*)childFrame)->SetStartColumnIndex(actualColumns);
    PRInt32 numCols = ((nsTableColGroupFrame*)childFrame)->GetColumnCount();
    actualColumns += numCols;
    lastColGroupFrame = (nsTableColGroupFrame *)childFrame;
    if (PR_TRUE==gsDebug) printf("EC: found a col group %p\n", lastColGroupFrame);
    childFrame->GetNextSibling(&childFrame);
  }

  // if we have fewer column frames than we need, create some implicit column frames
  PRInt32 colCount = mCellMap->GetColCount();
  if (PR_TRUE==gsDebug) printf("EC: actual = %d, colCount=%d\n", actualColumns, colCount);
  if (actualColumns < colCount)
  {
    if (PR_TRUE==gsDebug) printf("TIF EnsureColumns: actual %d < colCount %d\n", actualColumns, colCount);
    nsIContent *lastColGroupElement = nsnull;
    if (nsnull==lastColGroupFrame)
    { // there are no col groups, so create an implicit colgroup frame 
      if (PR_TRUE==gsDebug) printf("EnsureColumns:creating colgroup frame\n");
      // Resolve style for the colgroup frame
      // first, need to get the nearest containing content object
      GetContent(&lastColGroupElement);                                          // ADDREF a: lastColGroupElement++  (either here or in the loop below)
      nsIFrame *parentFrame;
      GetParent(&parentFrame);
      while (nsnull==lastColGroupElement)
      {
        parentFrame->GetContent(&lastColGroupElement);
        if (nsnull==lastColGroupElement)
          parentFrame->GetParent(&parentFrame);
      }
      // now we have a ref-counted "lastColGroupElement" content object
      nsIStyleContext* colGroupStyleContext;
      aPresContext.ResolvePseudoStyleContextFor (lastColGroupElement, 
                                                 nsHTMLAtoms::tableColGroupPseudo,
                                                 mStyleContext,
                                                 PR_FALSE,
                                                 &colGroupStyleContext);        // colGroupStyleContext: REFCNT++
      // Create a col group frame
      nsIFrame* newFrame;
      NS_NewTableColGroupFrame(newFrame);
      newFrame->Init(aPresContext, lastColGroupElement, this, colGroupStyleContext,
                     nsnull);
      lastColGroupFrame = (nsTableColGroupFrame*)newFrame;
      NS_RELEASE(colGroupStyleContext);                                         // kidStyleContenxt: REFCNT--

      // hook lastColGroupFrame into child list
      mColGroups.SetFrames(lastColGroupFrame);
    }
    else
    {
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
    for ( ; excessColumns > 0; excessColumns--)
    {
      // Create a new col frame
      nsIFrame* colFrame;
      // note we pass in PR_TRUE here to force unique style contexts.
      nsIStyleContext* colStyleContext;
      aPresContext.ResolvePseudoStyleContextFor (lastColGroupElement, 
                                                 nsHTMLAtoms::tableColPseudo,
                                                 lastColGroupStyle,
                                                 PR_TRUE,
                                                 &colStyleContext);             // colStyleContext: REFCNT++
      NS_NewTableColFrame(colFrame);
      colFrame->Init(aPresContext, lastColGroupElement, lastColGroupFrame,
                     colStyleContext, nsnull);
      NS_RELEASE(colStyleContext);
      colFrame->SetInitialChildList(aPresContext, nsnull, nsnull);

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
}


void nsTableFrame::AddColumnFrame (nsTableColFrame *aColFrame)
{
  if (gsDebug==PR_TRUE) printf("TIF: AddColumnFrame %p\n", aColFrame);
  nsCellMap *cellMap = GetCellMap();
  NS_PRECONDITION (nsnull!=cellMap, "null cellMap.");
  if (nsnull!=cellMap)
  {
    if (gsDebug) printf("TIF AddColumnFrame: adding column frame %p\n", aColFrame);
    cellMap->AppendColumnFrame(aColFrame);
  }
}

/** return the index of the next row that is not yet assigned */
PRInt32 nsTableFrame::GetNextAvailRowIndex() const
{
  PRInt32 result=0;
  nsCellMap *cellMap = GetCellMap();
  NS_PRECONDITION (nsnull!=cellMap, "null cellMap.");
  if (nsnull!=cellMap)
  {
    result = cellMap->GetRowCount(); // the next index is the current count
    cellMap->GrowToRow(result+1);    // expand the cell map to include this new row
  }
  return result;
}

/** return the index of the next column in aRowIndex that does not have a cell assigned to it */
PRInt32 nsTableFrame::GetNextAvailColIndex(PRInt32 aRowIndex, PRInt32 aColIndex) const
{
  PRInt32 result=0;
  nsCellMap *cellMap = GetCellMap();
  NS_PRECONDITION (nsnull!=cellMap, "null cellMap.");
  if (nsnull!=cellMap)
    result = cellMap->GetNextAvailColIndex(aRowIndex, aColIndex);
  if (gsDebug==PR_TRUE) printf("TIF: GetNextAvailColIndex returning %d\n", result);
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

void nsTableFrame::SetMinColSpanForTable()
{ // XXX: must be called ONLY on first-in-flow
  // set the minColSpan for each column
  PRInt32 rowCount = mCellMap->GetRowCount();
  PRInt32 colCount = mCellMap->GetColCount();
  for (PRInt32 colIndex=0; colIndex<colCount; colIndex++)
  {
    PRInt32 minColSpan=0;
    for (PRInt32 rowIndex=0; rowIndex<rowCount; rowIndex++)
    {
      nsTableCellFrame *cellFrame = mCellMap->GetCellFrameAt(rowIndex, colIndex);
      if (nsnull!=cellFrame)
      {
        PRInt32 colSpan = cellFrame->GetColSpan();
        if (0==minColSpan)
          minColSpan = colSpan;
        else
          minColSpan = PR_MIN(minColSpan, colSpan);
      }
    }
    if (1<minColSpan)
    {
      mCellMap->SetMinColSpan(colIndex, minColSpan);
#ifdef NS_DEBUG
      if (gsDebug==PR_TRUE)
      {
        printf("minColSpan for col %d set to %d\n", colIndex, minColSpan);
        DumpCellMap();
      }
#endif
    }
  }
}

void nsTableFrame::AddCellToTable (nsTableRowFrame *aRowFrame, 
                                   nsTableCellFrame *aCellFrame,
                                   PRBool aAddRow)
{
  NS_ASSERTION(nsnull!=aRowFrame, "bad aRowFrame arg");
  NS_ASSERTION(nsnull!=aCellFrame, "bad aCellFrame arg");
  NS_PRECONDITION(nsnull!=mCellMap, "bad cellMap");

  // XXX: must be called only on first-in-flow!
  if (gsDebug==PR_TRUE) printf("TIF AddCellToTable: frame %p\n", aCellFrame);

  // Make an educated guess as to how many columns we have. It's
  // only a guess because we can't know exactly until we have
  // processed the last row.
  if (0 == mColCount)
    mColCount = GetSpecifiedColumnCount();
  if (0 == mColCount) // no column parts
  {
    mColCount = aRowFrame->GetMaxColumns();
  }

  PRInt32 rowIndex;
  // also determine the index of aRowFrame and set it if necessary
  if (0==mCellMap->GetRowCount())
  { // this is the first time we've ever been called
    rowIndex = 0;
    if (gsDebug==PR_TRUE) printf("rowFrame %p set to index %d\n", aRowFrame, rowIndex);
  }
  else
  {
    rowIndex = mCellMap->GetRowCount() - 1;   // rowIndex is 0-indexed, rowCount is 1-indexed
  }

  PRInt32 colIndex=0;
  while (PR_TRUE)
  {
    CellData *data = mCellMap->GetCellAt(rowIndex, colIndex);
    if (nsnull == data)
    {
      BuildCellIntoMap(aCellFrame, rowIndex, colIndex);
      break;
    }
    colIndex++;
  }

#ifdef NS_DEBUG
  if (gsDebug==PR_TRUE)
    DumpCellMap ();
#endif
}

NS_METHOD nsTableFrame::ReBuildCellMap()
{
  if (PR_TRUE==gsDebugIR) printf("TIF: ReBuildCellMap.\n");
  nsresult rv=NS_OK;
  nsIFrame *rowGroupFrame=mFrames.FirstChild();
  for ( ; nsnull!=rowGroupFrame; rowGroupFrame->GetNextSibling(&rowGroupFrame))
  {
    const nsStyleDisplay *rowGroupDisplay;
    rowGroupFrame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct *&)rowGroupDisplay);
    if (PR_TRUE==IsRowGroup(rowGroupDisplay->mDisplay))
    {
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
      }
    }
  }
  mCellMapValid=PR_TRUE;
  return rv;
}

#ifdef NS_DEBUG
void nsTableFrame::DumpCellMap () 
{
  printf("dumping CellMap:\n");
  if (nsnull != mCellMap) {
    PRInt32 colIndex;
    PRInt32 rowCount = mCellMap->GetRowCount();
    PRInt32 colCount = mCellMap->GetColCount();
    printf("rowCount=%d, colCount=%d\n", rowCount, colCount);

    for (PRInt32 rowIndex = 0; rowIndex < rowCount; rowIndex++) {
      printf("row %d : ", rowIndex);
      for (colIndex = 0; colIndex < colCount; colIndex++) {
        CellData* cd = mCellMap->GetCellAt(rowIndex, colIndex);
        if (cd != nsnull) {
          if (cd->mCell != nsnull) {
            printf("C%d,%d  ", rowIndex, colIndex);
          } else {
            nsTableCellFrame* cell = cd->mRealCell->mCell;
            nsTableRowFrame* row;
            cell->GetParent((nsIFrame**)&row);
            int rr = row->GetRowIndex();
            int cc;
            cell->GetColIndex(cc);
            printf("S%d,%d  ", rr, cc);
            if (cd->mOverlap != nsnull){
              cell = cd->mOverlap->mCell;
              nsTableRowFrame* row2;
              cell->GetParent((nsIFrame**)&row2);
              rr = row2->GetRowIndex();
              cell->GetColIndex(cc);
              printf("O%d,%c ", rr, cc);
            } 
          }
        } else {
          printf("----      ");
        }
      }
      PRBool spanners = RowHasSpanningCells(rowIndex);
      PRBool spannedInto = RowIsSpannedInto(rowIndex);
      printf ("  spanners=%s spannedInto=%s\n", spanners?"T":"F", spannedInto?"T":"F");
    }

    // output info mapping Ci,j to cell address
    PRInt32 cellCount = 0;
    for (PRInt32 rIndex = 0; rIndex < rowCount; rIndex++) {
      for (colIndex = 0; colIndex < colCount; colIndex++) {
        CellData* cd = mCellMap->GetCellAt(rIndex, colIndex);
        if (cd != nsnull) {
          if (cd->mCell != nsnull) {
            printf("C%d,%d=%p  ", rIndex, colIndex, cd->mCell);
            cellCount++;
          }
        }
        if (0 == (cellCount % 4)) {
          printf("\n");
        }
      }
    }

		// output colspan info
		for (colIndex=0; colIndex<colCount; colIndex++) {
			PRBool colSpanners = ColHasSpanningCells(colIndex);
			PRBool colSpannedInto = ColIsSpannedInto(colIndex);
			printf ("%d colSpanners=%s colSpannedInto=%s\n", 
				      colIndex, colSpanners?"T":"F", colSpannedInto?"T":"F");
		}
		// output col frame info
		for (colIndex=0; colIndex<colCount; colIndex++) {
      nsTableColFrame *colFrame = mCellMap->GetColumnFrame(colIndex);
			printf ("col index %d has frame=%p\n", colIndex, colFrame);
		}
  } else {
    printf ("[nsnull]");
  }
}
#endif

void nsTableFrame::BuildCellIntoMap (nsTableCellFrame *aCell, PRInt32 aRowIndex, PRInt32 aColIndex)
{
  NS_PRECONDITION (nsnull!=aCell, "bad cell arg");
  NS_PRECONDITION (0 <= aColIndex, "bad column index arg");
  NS_PRECONDITION (0 <= aRowIndex, "bad row index arg");

  // Setup CellMap for this cell
  int rowSpan = aCell->GetRowSpan();
  int colSpan = aCell->GetColSpan();
  if (gsDebug==PR_TRUE) printf("BCIM: rowSpan = %d, colSpan = %d\n", rowSpan, colSpan);

  // Grow the mCellMap array if we will end up addressing
  // some new columns.
  if (mCellMap->GetColCount() < (aColIndex + colSpan))
  {
    if (gsDebug==PR_TRUE) 
      printf("BCIM: calling GrowCellMap(%d)\n", aColIndex+colSpan);
    GrowCellMap (aColIndex + colSpan);
  }

  if (mCellMap->GetRowCount() < (aRowIndex+1))
  {
    printf("BCIM: calling GrowToRow(%d)\n", aRowIndex+1);
    mCellMap->GrowToRow(aRowIndex+1);
  }

  // Setup CellMap for this cell in the table
  CellData *data = new CellData ();
  data->mCell = aCell;
  data->mRealCell = data;
  if (gsDebug==PR_TRUE) printf("BCIM: calling mCellMap->SetCellAt(data, %d, %d)\n", aRowIndex, aColIndex);
  mCellMap->SetCellAt(data, aRowIndex, aColIndex);

  // Create CellData objects for the rows that this cell spans. Set
  // their mCell to nsnull and their mRealCell to point to data. If
  // there were no column overlaps then we could use the same
  // CellData object for each row that we span...
  if ((1 < rowSpan) || (1 < colSpan))
  {
    if (gsDebug==PR_TRUE) printf("BCIM: spans\n");
    for (int rowIndex = 0; rowIndex < rowSpan; rowIndex++)
    {
      if (gsDebug==PR_TRUE) printf("BCIM: rowIndex = %d\n", rowIndex);
      int workRow = aRowIndex + rowIndex;
      if (gsDebug==PR_TRUE) printf("BCIM: workRow = %d\n", workRow);
      for (int colIndex = 0; colIndex < colSpan; colIndex++)
      {
        if (gsDebug==PR_TRUE) printf("BCIM: colIndex = %d\n", colIndex);
        int workCol = aColIndex + colIndex;
        if (gsDebug==PR_TRUE) printf("BCIM: workCol = %d\n", workCol);
        CellData *testData = mCellMap->GetCellAt(workRow, workCol);
        if (nsnull == testData)
        {
          CellData *spanData = new CellData ();
          spanData->mRealCell = data;
          if (gsDebug==PR_TRUE) printf("BCIM: null GetCellFrameAt(%d, %d) so setting to spanData\n", workRow, workCol);
          mCellMap->SetCellAt(spanData, workRow, workCol);
        }
        else if ((0 < rowIndex) || (0 < colIndex))
        { // we overlap, replace existing data, it might be shared
          if (gsDebug==PR_TRUE) printf("BCIM: overlapping Cell from GetCellFrameAt(%d, %d) so setting to spanData\n", workRow, workCol);
          CellData *overlap = new CellData ();
          overlap->mCell = testData->mCell;
          overlap->mRealCell = testData->mRealCell;
          overlap->mOverlap = data;
          mCellMap->SetCellAt(overlap, workRow, workCol);
        }
      }
    }
  }
}

void nsTableFrame::GrowCellMap (PRInt32 aColCount)
{
  if (nsnull!=mCellMap)
  {
    mCellMap->GrowToCol(aColCount);
    mColCount = aColCount;
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
        nsTableCellFrame *cellFrame = cellMap->GetCellFrameAt(rowIndex, colIndex);
        PRInt32 rowIndent;
        for (rowIndent = aIndent+2; --rowIndent >= 0; ) fputs("  ", out);
        fprintf(out,"Cell Data [%d] \n",rowIndex);
        for (rowIndent = aIndent+2; --rowIndent >= 0; ) fputs("  ", out);
        nsMargin margin;
        cellFrame->GetMargin(margin);
        fprintf(out,"Margin -- Top: %d Left: %d Bottom: %d Right: %d \n",  
                NSTwipsToIntPoints(margin.top),
                NSTwipsToIntPoints(margin.left),
                NSTwipsToIntPoints(margin.bottom),
                NSTwipsToIntPoints(margin.right));
      
        for (rowIndent = aIndent+2; --rowIndent >= 0; ) fputs("  ", out);
      }
    }
  }
}
#endif

/**
  * For the TableCell in CellData, add it to the list
  */
void nsTableFrame::AppendLayoutData(nsVoidArray* aList, nsTableCellFrame* aTableCell)
{
  if (aTableCell != nsnull)
  {
    aList->AppendElement((void*)aTableCell);
  }
}




void nsTableFrame::SetBorderEdgeLength(PRUint8 aSide, PRInt32 aIndex, nscoord aLength)
{
  nsBorderEdge *border = (nsBorderEdge *)(mBorderEdges.mEdges[aSide].ElementAt(aIndex));
  border->mLength = aLength;
}

void nsTableFrame::DidComputeHorizontalCollapsingBorders(nsIPresContext& aPresContext,
                                                         PRInt32 aStartRowIndex,
                                                         PRInt32 aEndRowIndex)
{
  // XXX: for now, this only does table edges.  May need to do interior edges also?  Probably not.
  nsCellMap *cellMap = GetCellMap();
  PRInt32 lastRowIndex = cellMap->GetRowCount()-1;
  PRInt32 lastColIndex = cellMap->GetColCount()-1;
  if (0==aStartRowIndex)
  {
    nsTableCellFrame *cellFrame = mCellMap->GetCellFrameAt(0, 0);
    if (nsnull==cellFrame)
    {
      CellData *cellData = mCellMap->GetCellAt(0, 0);
      if (nsnull!=cellData)
        cellFrame = cellData->mRealCell->mCell;
    }
    nsRect rowRect(0,0,0,0);
    if (nsnull!=cellFrame)
    {
      nsIFrame *rowFrame;
      cellFrame->GetParent(&rowFrame);
      rowFrame->GetRect(rowRect);
      nsBorderEdge *leftBorder = (nsBorderEdge *)(mBorderEdges.mEdges[NS_SIDE_LEFT].ElementAt(0));
      nsBorderEdge *rightBorder = (nsBorderEdge *)(mBorderEdges.mEdges[NS_SIDE_RIGHT].ElementAt(0));
      nsBorderEdge *topBorder = (nsBorderEdge *)(mBorderEdges.mEdges[NS_SIDE_TOP].ElementAt(0));
      leftBorder->mLength = rowRect.height + topBorder->mWidth;
      topBorder = (nsBorderEdge *)(mBorderEdges.mEdges[NS_SIDE_TOP].ElementAt(lastColIndex));
      rightBorder->mLength = rowRect.height + topBorder->mWidth;
    }
  }

  if (lastRowIndex<=aEndRowIndex)
  {
    nsTableCellFrame *cellFrame = mCellMap->GetCellFrameAt(lastRowIndex, 0);
    if (nsnull==cellFrame)
    {
      CellData *cellData = mCellMap->GetCellAt(lastRowIndex, 0);
      if (nsnull!=cellData)
        cellFrame = cellData->mRealCell->mCell;
    }
    nsRect rowRect(0,0,0,0);
    if (nsnull!=cellFrame)
    {
      nsIFrame *rowFrame;
      cellFrame->GetParent(&rowFrame);
      rowFrame->GetRect(rowRect);
      nsBorderEdge *leftBorder = (nsBorderEdge *)(mBorderEdges.mEdges[NS_SIDE_LEFT].ElementAt(lastRowIndex));
      nsBorderEdge *rightBorder = (nsBorderEdge *)(mBorderEdges.mEdges[NS_SIDE_RIGHT].ElementAt(lastRowIndex));
      nsBorderEdge *bottomBorder = (nsBorderEdge *)(mBorderEdges.mEdges[NS_SIDE_BOTTOM].ElementAt(0));
      leftBorder->mLength = rowRect.height + bottomBorder->mWidth;
      bottomBorder = (nsBorderEdge *)(mBorderEdges.mEdges[NS_SIDE_BOTTOM].ElementAt(lastColIndex));
      rightBorder->mLength = rowRect.height + bottomBorder->mWidth;
    }
  }

}


  // For every row between aStartRowIndex and aEndRowIndex (-1 == the end of the table),
  // walk across every edge and compute the border at that edge.
  // We compute each edge only once, arbitrarily choosing to compute right and bottom edges, 
  // except for exterior cells that share a left or top edge with the table itself.
  // Distribute half the computed border to the appropriate adjacent objects
  // (always a cell frame or the table frame.)  In the case of odd width, 
  // the object on the right/bottom gets the extra portion

/* compute the top and bottom collapsed borders between aStartRowIndex and aEndRowIndex, inclusive */
void nsTableFrame::ComputeHorizontalCollapsingBorders(nsIPresContext& aPresContext,
                                                      PRInt32 aStartRowIndex,
                                                      PRInt32 aEndRowIndex)
{
  // this method just uses mCellMap, because it can't get called unless nCellMap!=nsnull
  PRInt32 colCount = mCellMap->GetColCount();
  PRInt32 rowCount = mCellMap->GetRowCount();
  if (aStartRowIndex>=rowCount)
  {
    NS_ASSERTION(PR_FALSE, "aStartRowIndex>=rowCount in ComputeHorizontalCollapsingBorders");
    return; // we don't have the requested row yet
  }

  PRInt32 rowIndex = aStartRowIndex;
  for ( ; rowIndex<rowCount && rowIndex <=aEndRowIndex; rowIndex++)
  {
    PRInt32 colIndex=0;
    for ( ; colIndex<colCount; colIndex++)
    {
      if (0==rowIndex)
      { // table is top neighbor
        ComputeTopBorderForEdgeAt(aPresContext, rowIndex, colIndex);
      }
      ComputeBottomBorderForEdgeAt(aPresContext, rowIndex, colIndex);
    }
  }
}

/* compute the left and right collapsed borders between aStartRowIndex and aEndRowIndex, inclusive */
void nsTableFrame::ComputeVerticalCollapsingBorders(nsIPresContext& aPresContext,
                                                    PRInt32 aStartRowIndex,
                                                    PRInt32 aEndRowIndex)
{
  nsCellMap *cellMap = GetCellMap();
  if (nsnull==cellMap)
    return; // no info yet, so nothing useful to do
  
  CacheColFramesInCellMap();

  // compute all the collapsing border values for the entire table
  // XXX: we have to make this more incremental!
  const nsStyleTable *tableStyle=nsnull;
  GetStyleData(eStyleStruct_Table, (const nsStyleStruct *&)tableStyle);
  if (NS_STYLE_BORDER_COLLAPSE!=tableStyle->mBorderCollapse)
    return;
  
  PRInt32 colCount = mCellMap->GetColCount();
  PRInt32 rowCount = mCellMap->GetRowCount();
  PRInt32 endRowIndex = aEndRowIndex;
  if (-1==endRowIndex)
    endRowIndex=rowCount-1;
  if (aStartRowIndex>=rowCount)
  {
    NS_ASSERTION(PR_FALSE, "aStartRowIndex>=rowCount in ComputeVerticalCollapsingBorders");
    return; // we don't have the requested row yet
  }

  PRInt32 rowIndex = aStartRowIndex;
  for ( ; rowIndex<rowCount && rowIndex <=endRowIndex; rowIndex++)
  {
    PRInt32 colIndex=0;
    for ( ; colIndex<colCount; colIndex++)
    {
      if (0==colIndex)
      { // table is left neighbor
        ComputeLeftBorderForEdgeAt(aPresContext, rowIndex, colIndex);
      }
      ComputeRightBorderForEdgeAt(aPresContext, rowIndex, colIndex);
    }
  }
}

void nsTableFrame::ComputeLeftBorderForEdgeAt(nsIPresContext& aPresContext,
                                              PRInt32 aRowIndex, 
                                              PRInt32 aColIndex)
{
  // this method just uses mCellMap, because it can't get called unless nCellMap!=nsnull
  PRInt32 numSegments = mBorderEdges.mEdges[NS_SIDE_LEFT].Count();
  while (numSegments<=aRowIndex)
  {
    nsBorderEdge *borderToAdd = new nsBorderEdge();
    mBorderEdges.mEdges[NS_SIDE_LEFT].AppendElement(borderToAdd);
    numSegments++;
  }
  // "border" is the border segment we are going to set
  nsBorderEdge *border = (nsBorderEdge *)(mBorderEdges.mEdges[NS_SIDE_LEFT].ElementAt(aRowIndex));

  // collect all the incident frames and compute the dominant border 
  nsVoidArray styles;
  // styles are added to the array in the order least dominant -> most dominant
  //    1. table
  const nsStyleSpacing *spacing;
  GetStyleData(eStyleStruct_Spacing, ((const nsStyleStruct *&)spacing));
  styles.AppendElement((void*)spacing);
  //    2. colgroup
  nsTableColFrame *colFrame = mCellMap->GetColumnFrame(aColIndex);
  nsIFrame *colGroupFrame;
  colFrame->GetParent(&colGroupFrame);
  colGroupFrame->GetStyleData(eStyleStruct_Spacing, ((const nsStyleStruct *&)spacing));
  styles.AppendElement((void*)spacing);
  //    3. col
  colFrame->GetStyleData(eStyleStruct_Spacing, ((const nsStyleStruct *&)spacing));
  styles.AppendElement((void*)spacing);
  //    4. rowgroup
  nsTableCellFrame *cellFrame = mCellMap->GetCellFrameAt(aRowIndex, aColIndex);
  if (nsnull==cellFrame)
  {
    CellData *cellData = mCellMap->GetCellAt(aRowIndex, aColIndex);
    if (nsnull!=cellData)
      cellFrame = cellData->mRealCell->mCell;
  }
  nsRect rowRect(0,0,0,0);
  if (nsnull!=cellFrame)
  {
    nsIFrame *rowFrame;
    cellFrame->GetParent(&rowFrame);
    rowFrame->GetRect(rowRect);
    nsIFrame *rowGroupFrame;
    rowFrame->GetParent(&rowGroupFrame);
    rowGroupFrame->GetStyleData(eStyleStruct_Spacing, ((const nsStyleStruct *&)spacing));
    styles.AppendElement((void*)spacing);
    //    5. row
    rowFrame->GetStyleData(eStyleStruct_Spacing, ((const nsStyleStruct *&)spacing));
    styles.AppendElement((void*)spacing);
    //    6. cell (need to do something smart for rowspanner with row frame)
    cellFrame->GetStyleData(eStyleStruct_Spacing, ((const nsStyleStruct *&)spacing));
    styles.AppendElement((void*)spacing);
  }
  ComputeCollapsedBorderSegment(NS_SIDE_LEFT, &styles, *border, PR_FALSE);
  // now give half the computed border to the table segment, and half to the cell
  // to avoid rounding errors, we convert up to pixels, divide by 2, and 
  // we give the odd pixel to the table border
  float t2p;
  aPresContext.GetTwipsToPixels(&t2p);
  float p2t;
  aPresContext.GetPixelsToTwips(&p2t);
  nscoord widthAsPixels = NSToCoordRound((float)(border->mWidth)*t2p);
  nscoord widthToAdd = 0;
  border->mWidth = widthAsPixels/2;
  if ((border->mWidth*2)!=widthAsPixels)
    widthToAdd = NSToCoordCeil(p2t);
  border->mWidth *= NSToCoordCeil(p2t);
  border->mLength = rowRect.height;
  border->mInsideNeighbor = &cellFrame->mBorderEdges;
  // we need to factor in the table's horizontal borders.
  // but we can't compute that length here because we don't know how thick top and bottom borders are
  // see DidComputeHorizontalCollapsingBorders
  if (nsnull!=cellFrame)
  {
    cellFrame->SetBorderEdge(NS_SIDE_LEFT, aRowIndex, aColIndex, border, 0);  // set the left edge of the cell frame
  }
  border->mWidth += widthToAdd;
  mBorderEdges.mMaxBorderWidth.left = PR_MAX(border->mWidth, mBorderEdges.mMaxBorderWidth.left);
}

void nsTableFrame::ComputeRightBorderForEdgeAt(nsIPresContext& aPresContext,
                                               PRInt32 aRowIndex, 
                                               PRInt32 aColIndex)
{
  // this method just uses mCellMap, because it can't get called unless nCellMap!=nsnull
  PRInt32 colCount = mCellMap->GetColCount();
  PRInt32 numSegments = mBorderEdges.mEdges[NS_SIDE_RIGHT].Count();
  while (numSegments<=aRowIndex)
  {
    nsBorderEdge *borderToAdd = new nsBorderEdge();
    mBorderEdges.mEdges[NS_SIDE_RIGHT].AppendElement(borderToAdd);
    numSegments++;
  }
  // "border" is the border segment we are going to set
  nsBorderEdge border;

  // collect all the incident frames and compute the dominant border 
  nsVoidArray styles;
  // styles are added to the array in the order least dominant -> most dominant
  //    1. table, only if this cell is in the right-most column and no rowspanning cell is
  //       to it's right.  Otherwise, we remember what cell is the right neighbor
  nsTableCellFrame *rightNeighborFrame=nsnull;  
  if ((colCount-1)!=aColIndex)
  {
    PRInt32 colIndex = aColIndex+1;
    for ( ; colIndex<colCount; colIndex++)
    {
		  CellData *cd = mCellMap->GetCellAt(aRowIndex, colIndex);
		  if (cd != nsnull)
		  { // there's really a cell at (aRowIndex, colIndex)
			  if (nsnull==cd->mCell)
			  { // the cell at (aRowIndex, colIndex) is the result of a span
				  nsTableCellFrame *cell = cd->mRealCell->mCell;
				  NS_ASSERTION(nsnull!=cell, "bad cell map state, missing real cell");
				  PRInt32 realRowIndex;
          cell->GetRowIndex (realRowIndex);
				  if (realRowIndex!=aRowIndex)
				  { // the span is caused by a rowspan
					  rightNeighborFrame = cd->mRealCell->mCell;
					  break;
				  }
			  }
        else
        {
          rightNeighborFrame = cd->mCell;
          break;
        }
		  }
    }
  }
  const nsStyleSpacing *spacing;
  if (nsnull==rightNeighborFrame)
  { // if rightNeighborFrame is null, our right neighbor is the table 
    GetStyleData(eStyleStruct_Spacing, ((const nsStyleStruct *&)spacing));
    styles.AppendElement((void*)spacing);
  }
  //    2. colgroup //XXX: need to test if we're really on a colgroup border
  nsTableColFrame *colFrame = mCellMap->GetColumnFrame(aColIndex);
  nsIFrame *colGroupFrame;
  colFrame->GetParent(&colGroupFrame);
  colGroupFrame->GetStyleData(eStyleStruct_Spacing, ((const nsStyleStruct *&)spacing));
  styles.AppendElement((void*)spacing);
  //    3. col
  colFrame->GetStyleData(eStyleStruct_Spacing, ((const nsStyleStruct *&)spacing));
  styles.AppendElement((void*)spacing);
  //    4. rowgroup
  nsTableCellFrame *cellFrame = mCellMap->GetCellFrameAt(aRowIndex, aColIndex);
  if (nsnull==cellFrame)
  {
    CellData *cellData = mCellMap->GetCellAt(aRowIndex, aColIndex);
    if (nsnull!=cellData)
      cellFrame = cellData->mRealCell->mCell;
  }
  nsRect rowRect(0,0,0,0);
  if (nsnull!=cellFrame)
  {
    nsIFrame *rowFrame;
    cellFrame->GetParent(&rowFrame);
    rowFrame->GetRect(rowRect);
    nsIFrame *rowGroupFrame;
    rowFrame->GetParent(&rowGroupFrame);
    if (nsnull==rightNeighborFrame)
    { // if rightNeighborFrame is null, our right neighbor is the table so we include the rowgroup and row
      rowGroupFrame->GetStyleData(eStyleStruct_Spacing, ((const nsStyleStruct *&)spacing));
      styles.AppendElement((void*)spacing);
      //    5. row
      rowFrame->GetStyleData(eStyleStruct_Spacing, ((const nsStyleStruct *&)spacing));
      styles.AppendElement((void*)spacing);
    }
    //    6. cell (need to do something smart for rowspanner with row frame)
    cellFrame->GetStyleData(eStyleStruct_Spacing, ((const nsStyleStruct *&)spacing));
    styles.AppendElement((void*)spacing);
  }
  //    7. left edge of rightNeighborCell, if there is one
  if (nsnull!=rightNeighborFrame)
  {
    rightNeighborFrame->GetStyleData(eStyleStruct_Spacing, ((const nsStyleStruct *&)spacing));
    styles.AppendElement((void*)spacing);
  }
  ComputeCollapsedBorderSegment(NS_SIDE_RIGHT, &styles, border, PRBool(nsnull!=rightNeighborFrame));
  // now give half the computed border to each of the two neighbors 
  // (the 2 cells, or the cell and the table)
  // to avoid rounding errors, we convert up to pixels, divide by 2, and 
  // we give the odd pixel to the right cell border
  float t2p;
  aPresContext.GetTwipsToPixels(&t2p);
  float p2t;
  aPresContext.GetPixelsToTwips(&p2t);
  nscoord widthAsPixels = NSToCoordRound((float)(border.mWidth)*t2p);
  nscoord widthToAdd = 0;
  border.mWidth = widthAsPixels/2;
  if ((border.mWidth*2)!=widthAsPixels)
    widthToAdd = NSToCoordCeil(p2t);
  border.mWidth *= NSToCoordCeil(p2t);
  border.mLength = rowRect.height;
  if (nsnull!=cellFrame)
  {
    cellFrame->SetBorderEdge(NS_SIDE_RIGHT, aRowIndex, aColIndex, &border, widthToAdd);
  }
  if (nsnull==rightNeighborFrame)
  {
    nsBorderEdge * tableBorder = (nsBorderEdge *)(mBorderEdges.mEdges[NS_SIDE_RIGHT].ElementAt(aRowIndex));
    *tableBorder = border;
    tableBorder->mInsideNeighbor = &cellFrame->mBorderEdges;
    mBorderEdges.mMaxBorderWidth.right = PR_MAX(border.mWidth, mBorderEdges.mMaxBorderWidth.right);
    // since the table is our right neightbor, we need to factor in the table's horizontal borders.
    // can't compute that length here because we don't know how thick top and bottom borders are
    // see DidComputeHorizontalCollapsingBorders
  }
  else
  {
    rightNeighborFrame->SetBorderEdge(NS_SIDE_LEFT, aRowIndex, aColIndex, &border, 0);
  }
}

void nsTableFrame::ComputeTopBorderForEdgeAt(nsIPresContext& aPresContext,
                                             PRInt32 aRowIndex, 
                                             PRInt32 aColIndex)
{
  // this method just uses mCellMap, because it can't get called unless nCellMap!=nsnull
  PRInt32 numSegments = mBorderEdges.mEdges[NS_SIDE_TOP].Count();
  while (numSegments<=aColIndex)
  {
    nsBorderEdge *borderToAdd = new nsBorderEdge();
    mBorderEdges.mEdges[NS_SIDE_TOP].AppendElement(borderToAdd);
    numSegments++;
  }
  // "border" is the border segment we are going to set
  nsBorderEdge *border = (nsBorderEdge *)(mBorderEdges.mEdges[NS_SIDE_TOP].ElementAt(aColIndex));

  // collect all the incident frames and compute the dominant border 
  nsVoidArray styles;
  // styles are added to the array in the order least dominant -> most dominant
  //    1. table
  const nsStyleSpacing *spacing;
  GetStyleData(eStyleStruct_Spacing, ((const nsStyleStruct *&)spacing));
  styles.AppendElement((void*)spacing);
  //    2. colgroup
  nsTableColFrame *colFrame = mCellMap->GetColumnFrame(aColIndex);
  nsIFrame *colGroupFrame;
  colFrame->GetParent(&colGroupFrame);
  colGroupFrame->GetStyleData(eStyleStruct_Spacing, ((const nsStyleStruct *&)spacing));
  styles.AppendElement((void*)spacing);
  //    3. col
  colFrame->GetStyleData(eStyleStruct_Spacing, ((const nsStyleStruct *&)spacing));
  styles.AppendElement((void*)spacing);
  //    4. rowgroup
  nsTableCellFrame *cellFrame = mCellMap->GetCellFrameAt(aRowIndex, aColIndex);
  if (nsnull==cellFrame)
  {
    CellData *cellData = mCellMap->GetCellAt(aRowIndex, aColIndex);
    if (nsnull!=cellData)
      cellFrame = cellData->mRealCell->mCell;
  }
  if (nsnull!=cellFrame)
  {
    nsIFrame *rowFrame;
    cellFrame->GetParent(&rowFrame);
    nsIFrame *rowGroupFrame;
    rowFrame->GetParent(&rowGroupFrame);
    rowGroupFrame->GetStyleData(eStyleStruct_Spacing, ((const nsStyleStruct *&)spacing));
    styles.AppendElement((void*)spacing);
    //    5. row
    rowFrame->GetStyleData(eStyleStruct_Spacing, ((const nsStyleStruct *&)spacing));
    styles.AppendElement((void*)spacing);
    //    6. cell (need to do something smart for rowspanner with row frame)
    cellFrame->GetStyleData(eStyleStruct_Spacing, ((const nsStyleStruct *&)spacing));
    styles.AppendElement((void*)spacing);
  }
  ComputeCollapsedBorderSegment(NS_SIDE_TOP, &styles, *border, PR_FALSE);
  // now give half the computed border to the table segment, and half to the cell
  // to avoid rounding errors, we convert up to pixels, divide by 2, and 
  // we give the odd pixel to the right border
  float t2p;
  aPresContext.GetTwipsToPixels(&t2p);
  float p2t;
  aPresContext.GetPixelsToTwips(&p2t);
  nscoord widthAsPixels = NSToCoordRound((float)(border->mWidth)*t2p);
  nscoord widthToAdd = 0;
  border->mWidth = widthAsPixels/2;
  if ((border->mWidth*2)!=widthAsPixels)
    widthToAdd = NSToCoordCeil(p2t);
  border->mWidth *= NSToCoordCeil(p2t);
  border->mLength = GetColumnWidth(aColIndex);
  border->mInsideNeighbor = &cellFrame->mBorderEdges;
  if (0==aColIndex)
  { // if we're the first column, factor in the thickness of the left table border
    nsBorderEdge *leftBorder = (nsBorderEdge *)(mBorderEdges.mEdges[NS_SIDE_LEFT].ElementAt(0));
    border->mLength += leftBorder->mWidth;
  }
  if ((mCellMap->GetColCount()-1)==aColIndex)
  { // if we're the last column, factor in the thickness of the right table border
    nsBorderEdge *rightBorder = (nsBorderEdge *)(mBorderEdges.mEdges[NS_SIDE_RIGHT].ElementAt(0));
    border->mLength += rightBorder->mWidth;
  }
  if (nsnull!=cellFrame)
  {
    cellFrame->SetBorderEdge(NS_SIDE_TOP, aRowIndex, aColIndex, border, 0);  // set the top edge of the cell frame
  }
  border->mWidth += widthToAdd;
  mBorderEdges.mMaxBorderWidth.top = PR_MAX(border->mWidth, mBorderEdges.mMaxBorderWidth.top);
}

void nsTableFrame::ComputeBottomBorderForEdgeAt(nsIPresContext& aPresContext,
                                                PRInt32 aRowIndex, 
                                                PRInt32 aColIndex)
{
  // this method just uses mCellMap, because it can't get called unless nCellMap!=nsnull
  PRInt32 rowCount = mCellMap->GetRowCount();
  PRInt32 numSegments = mBorderEdges.mEdges[NS_SIDE_BOTTOM].Count();
  while (numSegments<=aColIndex)
  {
    nsBorderEdge *borderToAdd = new nsBorderEdge();
    mBorderEdges.mEdges[NS_SIDE_BOTTOM].AppendElement(borderToAdd);
    numSegments++;
  }
  // "border" is the border segment we are going to set
  nsBorderEdge border;

  // collect all the incident frames and compute the dominant border 
  nsVoidArray styles;
  // styles are added to the array in the order least dominant -> most dominant
  //    1. table, only if this cell is in the bottom-most row and no colspanning cell is
  //       beneath it.  Otherwise, we remember what cell is the bottom neighbor
  nsTableCellFrame *bottomNeighborFrame=nsnull;  
  if ((rowCount-1)!=aRowIndex)
  {
    PRInt32 rowIndex = aRowIndex+1;
    for ( ; rowIndex<rowCount; rowIndex++)
    {
		  CellData *cd = mCellMap->GetCellAt(rowIndex, aColIndex);
		  if (cd != nsnull)
		  { // there's really a cell at (rowIndex, aColIndex)
			  if (nsnull==cd->mCell)
			  { // the cell at (rowIndex, aColIndex) is the result of a span
				  nsTableCellFrame *cell = cd->mRealCell->mCell;
				  NS_ASSERTION(nsnull!=cell, "bad cell map state, missing real cell");
				  PRInt32 realColIndex;
          cell->GetColIndex (realColIndex);
				  if (realColIndex!=aColIndex)
				  { // the span is caused by a colspan
					  bottomNeighborFrame = cd->mRealCell->mCell;
					  break;
				  }
			  }
        else
        {
          bottomNeighborFrame = cd->mCell;
          break;
        }
		  }
    }
  }
  const nsStyleSpacing *spacing;
  if (nsnull==bottomNeighborFrame)
  { // if bottomNeighborFrame is null, our bottom neighbor is the table 
    GetStyleData(eStyleStruct_Spacing, ((const nsStyleStruct *&)spacing));
    styles.AppendElement((void*)spacing);

    //    2. colgroup   // XXX: need to deterine if we're on a colgroup boundary
    nsTableColFrame *colFrame = mCellMap->GetColumnFrame(aColIndex);
    nsIFrame *colGroupFrame;
    colFrame->GetParent(&colGroupFrame);
    colGroupFrame->GetStyleData(eStyleStruct_Spacing, ((const nsStyleStruct *&)spacing));
    styles.AppendElement((void*)spacing);
    //    3. col
    colFrame->GetStyleData(eStyleStruct_Spacing, ((const nsStyleStruct *&)spacing));
    styles.AppendElement((void*)spacing);
  }
  //    4. rowgroup // XXX: use rowgroup only if we're on a table edge
  nsTableCellFrame *cellFrame = mCellMap->GetCellFrameAt(aRowIndex, aColIndex);
  if (nsnull==cellFrame)
  {
    CellData *cellData = mCellMap->GetCellAt(aRowIndex, aColIndex);
    if (nsnull!=cellData)
      cellFrame = cellData->mRealCell->mCell;
  }
  nsRect rowRect(0,0,0,0);
  if (nsnull!=cellFrame)
  {
    nsIFrame *rowFrame;
    cellFrame->GetParent(&rowFrame);
    rowFrame->GetRect(rowRect);
    nsIFrame *rowGroupFrame;
    rowFrame->GetParent(&rowGroupFrame);
    rowGroupFrame->GetStyleData(eStyleStruct_Spacing, ((const nsStyleStruct *&)spacing));
    styles.AppendElement((void*)spacing);
    //    5. row
    rowFrame->GetStyleData(eStyleStruct_Spacing, ((const nsStyleStruct *&)spacing));
    styles.AppendElement((void*)spacing);
    //    6. cell (need to do something smart for rowspanner with row frame)
    cellFrame->GetStyleData(eStyleStruct_Spacing, ((const nsStyleStruct *&)spacing));
    styles.AppendElement((void*)spacing);
  }
  //    7. top edge of bottomNeighborCell, if there is one
  if (nsnull!=bottomNeighborFrame)
  {
    bottomNeighborFrame->GetStyleData(eStyleStruct_Spacing, ((const nsStyleStruct *&)spacing));
    styles.AppendElement((void*)spacing);
  }
  ComputeCollapsedBorderSegment(NS_SIDE_BOTTOM, &styles, border, PRBool(nsnull!=bottomNeighborFrame));
  // now give half the computed border to each of the two neighbors 
  // (the 2 cells, or the cell and the table)
  // to avoid rounding errors, we convert up to pixels, divide by 2, and 
  // we give the odd pixel to the right cell border
  float t2p;
  aPresContext.GetTwipsToPixels(&t2p);
  float p2t;
  aPresContext.GetPixelsToTwips(&p2t);
  nscoord widthAsPixels = NSToCoordRound((float)(border.mWidth)*t2p);
  nscoord widthToAdd = 0;
  border.mWidth = widthAsPixels/2;
  if ((border.mWidth*2)!=widthAsPixels)
    widthToAdd = NSToCoordCeil(p2t);
  border.mWidth *= NSToCoordCeil(p2t);
  border.mLength = GetColumnWidth(aColIndex);
  if (nsnull!=cellFrame)
  {
    cellFrame->SetBorderEdge(NS_SIDE_BOTTOM, aRowIndex, aColIndex, &border, widthToAdd);
  }
  if (nsnull==bottomNeighborFrame)
  {
    nsBorderEdge * tableBorder = (nsBorderEdge *)(mBorderEdges.mEdges[NS_SIDE_BOTTOM].ElementAt(aColIndex));
    *tableBorder = border;
    tableBorder->mInsideNeighbor = &cellFrame->mBorderEdges;
    mBorderEdges.mMaxBorderWidth.bottom = PR_MAX(border.mWidth, mBorderEdges.mMaxBorderWidth.bottom);
    // since the table is our bottom neightbor, we need to factor in the table's vertical borders.
    PRInt32 lastColIndex = mCellMap->GetColCount()-1;
    if (0==aColIndex)
    { // if we're the first column, factor in the thickness of the left table border
      nsBorderEdge *leftBorder = (nsBorderEdge *)(mBorderEdges.mEdges[NS_SIDE_LEFT].ElementAt(rowCount-1));
      tableBorder->mLength += leftBorder->mWidth;
    }
    if (lastColIndex==aColIndex)
    { // if we're the last column, factor in the thickness of the right table border
      nsBorderEdge *rightBorder = (nsBorderEdge *)(mBorderEdges.mEdges[NS_SIDE_RIGHT].ElementAt(rowCount-1));
      tableBorder->mLength += rightBorder->mWidth;
    }
  }
  else
  {
    bottomNeighborFrame->SetBorderEdge(NS_SIDE_TOP, aRowIndex, aColIndex, &border, 0);
  }
}

nscoord nsTableFrame::GetWidthForSide(const nsMargin &aBorder, PRUint8 aSide)
{
  if (NS_SIDE_LEFT == aSide) return aBorder.left;
  else if (NS_SIDE_RIGHT == aSide) return aBorder.right;
  else if (NS_SIDE_TOP == aSide) return aBorder.top;
  else return aBorder.bottom;
}

/* Given an Edge, find the opposing edge (top<-->bottom, left<-->right) */
PRUint8 nsTableFrame::GetOpposingEdge(PRUint8 aEdge)
{
   PRUint8 result;
   switch (aEdge)
   {
   case NS_SIDE_LEFT:
        result = NS_SIDE_RIGHT;  break;
   case NS_SIDE_RIGHT:
        result = NS_SIDE_LEFT;   break;
   case NS_SIDE_TOP:
        result = NS_SIDE_BOTTOM; break;
   case NS_SIDE_BOTTOM:
        result = NS_SIDE_TOP;    break;
   default:
        result = NS_SIDE_TOP;
   }
  return result;
}

/* returns BORDER_PRECEDENT_LOWER if aStyle1 is lower precedent that aStyle2
 *         BORDER_PRECEDENT_HIGHER if aStyle1 is higher precedent that aStyle2
 *         BORDER_PRECEDENT_EQUAL if aStyle1 and aStyle2 have the same precedence
 *         (note, this is not necessarily the same as saying aStyle1==aStyle2)
 * this is a method on nsTableFrame because other objects might define their
 * own border precedence rules.
 */
PRUint8 nsTableFrame::CompareBorderStyles(PRUint8 aStyle1, PRUint8 aStyle2)
{
  PRUint8 result=BORDER_PRECEDENT_HIGHER; // if we get illegal types for table borders, HIGHER is the default
  if (aStyle1==aStyle2)
    result = BORDER_PRECEDENT_EQUAL;
  else if (NS_STYLE_BORDER_STYLE_HIDDEN==aStyle1)
    result = BORDER_PRECEDENT_HIGHER;
  else if (NS_STYLE_BORDER_STYLE_NONE==aStyle1)
    result = BORDER_PRECEDENT_LOWER;
  else if (NS_STYLE_BORDER_STYLE_NONE==aStyle2)
    result = BORDER_PRECEDENT_HIGHER;
  else if (NS_STYLE_BORDER_STYLE_HIDDEN==aStyle2)
    result = BORDER_PRECEDENT_LOWER;
  else
  {
    switch (aStyle1)
    {
    case NS_STYLE_BORDER_STYLE_BG_INSET:
      result = BORDER_PRECEDENT_LOWER;
      break;

    case NS_STYLE_BORDER_STYLE_GROOVE:
      if (NS_STYLE_BORDER_STYLE_BG_INSET==aStyle2)
        result = BORDER_PRECEDENT_HIGHER;
      else
        result = BORDER_PRECEDENT_LOWER;
      break;      

    case NS_STYLE_BORDER_STYLE_BG_OUTSET:
      if (NS_STYLE_BORDER_STYLE_BG_INSET==aStyle2 || 
          NS_STYLE_BORDER_STYLE_GROOVE==aStyle2)
        result = BORDER_PRECEDENT_HIGHER;
      else
        result = BORDER_PRECEDENT_LOWER;
      break;      

    case NS_STYLE_BORDER_STYLE_RIDGE:
      if (NS_STYLE_BORDER_STYLE_BG_INSET==aStyle2  || 
          NS_STYLE_BORDER_STYLE_GROOVE==aStyle2 ||
          NS_STYLE_BORDER_STYLE_BG_OUTSET==aStyle2)
        result = BORDER_PRECEDENT_HIGHER;
      else
        result = BORDER_PRECEDENT_LOWER;
      break;

    case NS_STYLE_BORDER_STYLE_DOTTED:
      if (NS_STYLE_BORDER_STYLE_BG_INSET==aStyle2  || 
          NS_STYLE_BORDER_STYLE_GROOVE==aStyle2 ||
          NS_STYLE_BORDER_STYLE_BG_OUTSET==aStyle2 ||
          NS_STYLE_BORDER_STYLE_RIDGE==aStyle2)
        result = BORDER_PRECEDENT_HIGHER;
      else
        result = BORDER_PRECEDENT_LOWER;
      break;

    case NS_STYLE_BORDER_STYLE_DASHED:
      if (NS_STYLE_BORDER_STYLE_BG_INSET==aStyle2  || 
          NS_STYLE_BORDER_STYLE_GROOVE==aStyle2 ||
          NS_STYLE_BORDER_STYLE_BG_OUTSET==aStyle2 ||
          NS_STYLE_BORDER_STYLE_RIDGE==aStyle2  ||
          NS_STYLE_BORDER_STYLE_DOTTED==aStyle2)
        result = BORDER_PRECEDENT_HIGHER;
      else
        result = BORDER_PRECEDENT_LOWER;
      break;

    case NS_STYLE_BORDER_STYLE_SOLID:
      if (NS_STYLE_BORDER_STYLE_BG_INSET==aStyle2  || 
          NS_STYLE_BORDER_STYLE_GROOVE==aStyle2 ||
          NS_STYLE_BORDER_STYLE_BG_OUTSET==aStyle2 ||
          NS_STYLE_BORDER_STYLE_RIDGE==aStyle2  ||
          NS_STYLE_BORDER_STYLE_DOTTED==aStyle2 ||
          NS_STYLE_BORDER_STYLE_DASHED==aStyle2)
        result = BORDER_PRECEDENT_HIGHER;
      else
        result = BORDER_PRECEDENT_LOWER;
      break;

    case NS_STYLE_BORDER_STYLE_DOUBLE:
        result = BORDER_PRECEDENT_LOWER;
      break;
    }
  }
  return result;
}

/*
  This method is the CSS2 border conflict resolution algorithm
  The spec says to resolve conflicts in this order:
  1. any border with the style HIDDEN wins
  2. the widest border with a style that is not NONE wins
  3. the border styles are ranked in this order, highest to lowest precedence: 
        double, solid, dashed, dotted, ridge, outset, groove, inset
  4. borders that are of equal width and style (differ only in color) have this precedence:
        cell, row, rowgroup, col, colgroup, table
  5. if all border styles are NONE, then that's the computed border style.
  This method assumes that the styles were added to aStyles in the reverse precedence order
  of their frame type, so that styles that come later in the list win over style 
  earlier in the list if the tie-breaker gets down to #4.
  This method sets the out-param aBorder with the resolved border attributes
*/
void nsTableFrame::ComputeCollapsedBorderSegment(PRUint8       aSide, 
                                                 nsVoidArray  *aStyles, 
                                                 nsBorderEdge &aBorder,
                                                 PRBool        aFlipLastSide)
{
  if (nsnull!=aStyles)
  {
    PRInt32 styleCount=aStyles->Count();
    if (0!=styleCount)
    {
      nsVoidArray sameWidthBorders;
      nsStyleSpacing * spacing;
      nsStyleSpacing * lastSpacing=nsnull;
      nsMargin border;
      PRInt32 maxWidth=0;
      PRInt32 i;
      PRUint8 side = aSide;
      for (i=0; i<styleCount; i++)
      {
        spacing = (nsStyleSpacing *)(aStyles->ElementAt(i));
        if ((PR_TRUE==aFlipLastSide) && (i==styleCount-1))
        {
          side = GetOpposingEdge(aSide);
          lastSpacing = spacing;
        }
        if (spacing->GetBorderStyle(side)==NS_STYLE_BORDER_STYLE_HIDDEN)
        {
          aBorder.mStyle=NS_STYLE_BORDER_STYLE_HIDDEN;
          aBorder.mWidth=0;
          return;
        }
        else if (spacing->GetBorderStyle(side)!=NS_STYLE_BORDER_STYLE_NONE)
        {
          spacing->GetBorder(border);
          nscoord borderWidth = GetWidthForSide(border, side);
          if (borderWidth==maxWidth)
            sameWidthBorders.AppendElement(spacing);
          else if (borderWidth>maxWidth)
          {
            maxWidth=borderWidth;
            sameWidthBorders.Clear();
            sameWidthBorders.AppendElement(spacing);
          }
        }
      }
      aBorder.mWidth=maxWidth;
      // now we've gone through each overlapping border once, and we have a list
      // of all styles with the same width.  If there's more than one, resolve the
      // conflict based on border style
      styleCount = sameWidthBorders.Count();
      if (0==styleCount)
      {  // all borders were of style NONE
        aBorder.mWidth=0;
        aBorder.mStyle=NS_STYLE_BORDER_STYLE_NONE;
        return;
      }
      else if (1==styleCount)
      { // there was just one border of the largest width
        spacing = (nsStyleSpacing *)(sameWidthBorders.ElementAt(0));
        side=aSide;
        if (spacing==lastSpacing)
          side=GetOpposingEdge(aSide);
        if (! spacing->GetBorderColor(side, aBorder.mColor)) {
          // XXX EEEK handle transparent border color somehow...
        }
        aBorder.mStyle=spacing->GetBorderStyle(side);
        return;
      }
      else
      {
        nsStyleSpacing *winningStyleBorder;
        PRUint8 winningStyle=NS_STYLE_BORDER_STYLE_NONE;
        for (i=0; i<styleCount; i++)
        {
          spacing = (nsStyleSpacing *)(sameWidthBorders.ElementAt(i));
          side=aSide;
          if (spacing==lastSpacing)
            side=GetOpposingEdge(aSide);
          PRUint8 thisStyle = spacing->GetBorderStyle(side);
          PRUint8 borderCompare = CompareBorderStyles(thisStyle, winningStyle);
          if (BORDER_PRECEDENT_HIGHER==borderCompare)
          {
            winningStyle=thisStyle;
            winningStyleBorder = spacing;
          }
          else if (BORDER_PRECEDENT_EQUAL==borderCompare)
          { // we're in lowest-to-highest precedence order, so later border styles win
            winningStyleBorder=spacing;
          }          
        }
        aBorder.mStyle = winningStyle;
        side=aSide;
        if (winningStyleBorder==lastSpacing)
          side=GetOpposingEdge(aSide);
        if (! winningStyleBorder->GetBorderColor(side, aBorder.mColor)) {
          // XXX handle transparent border colors somehow
        }
      }
    }
  }

}

void nsTableFrame::RecalcLayoutData(nsIPresContext& aPresContext)
{
  nsCellMap *cellMap = GetCellMap();
  if (nsnull==cellMap)
    return; // no info yet, so nothing useful to do
  PRInt32 colCount = cellMap->GetColCount();
  PRInt32 rowCount = cellMap->GetRowCount();
  
  //XXX need to determine how much of what follows is really necessary
  //    it does collapsing margins between table elements
  PRInt32 row = 0;
  PRInt32 col = 0;

  nsTableCellFrame*  above = nsnull;
  nsTableCellFrame*  below = nsnull;
  nsTableCellFrame*  left = nsnull;
  nsTableCellFrame*  right = nsnull;


  PRInt32       edge = 0;
  nsVoidArray*  boundaryCells[4];

  for (edge = 0; edge < 4; edge++)
    boundaryCells[edge] = new nsVoidArray();


  if (colCount != 0 && rowCount != 0)
  {
    for (row = 0; row < rowCount; row++)
    {
      for (col = 0; col < colCount; col++)
      {
        nsTableCellFrame*  cell = nsnull;
        CellData*     cellData = cellMap->GetCellAt(row,col);
        
        if (cellData)
          cell = cellData->mCell;
        
        if (nsnull==cell)
          continue;

        PRInt32 colSpan = cell->GetColSpan();
        PRInt32 rowSpan = cell->GetRowSpan();

        // clear the cells for all for edges
        for (edge = 0; edge < 4; edge++)
          boundaryCells[edge]->Clear();

        // Check to see if the cell data represents the top,left
        // corner of a a table cell

        // Check to see if cell the represents a top edge cell
        if (0 == row)
          above = nsnull;
        else
        {
          cellData = cellMap->GetCellAt(row-1,col);
          if (nsnull != cellData)
            above = cellData->mRealCell->mCell;

          // Does the cell data point to the same cell?
          // If it is, then continue
          if ((nsnull != above) && (above == cell))
            continue;
        }           

        // Check to see if cell the represents a left edge cell
        if (0 == col)
          left = nsnull;
        else
        {
          cellData = cellMap->GetCellAt(row,col-1);
          if (cellData != nsnull)
            left = cellData->mRealCell->mCell;

          if ((nsnull != left) && (left == cell))
            continue;
        }

        // If this is the top,left edged cell
        // Then add the cells on the for edges to the array
        
        // Do the top and bottom edge
        PRInt32   r,c;
        PRInt32   r1,r2;
        PRInt32   c1,c2;
        PRInt32   last;
        

        r1 = row - 1;
        r2 = row + rowSpan;
        c = col;
        last = col + colSpan -1;
        last = PR_MIN(last,colCount-1);
        
        while (c <= last)
        {
          if (r1 != -1)
          {
            // Add top edge cells
            if (c != col)
            {
              cellData = cellMap->GetCellAt(r1,c);
              if ((cellData != nsnull) && (cellData->mCell != above))
              {
                above = cellData->mCell;
                if (above != nsnull)
                  AppendLayoutData(boundaryCells[NS_SIDE_TOP],above);
              }
            }
            else if (above != nsnull)
            {
              AppendLayoutData(boundaryCells[NS_SIDE_TOP],above);
            }
          }

          if (r2 < rowCount)
          {
            // Add bottom edge cells
            cellData = cellMap->GetCellAt(r2,c);
            if ((cellData != nsnull) && cellData->mCell != below)
            {
              below = cellData->mCell;
              if (below != nsnull)
                AppendLayoutData(boundaryCells[NS_SIDE_BOTTOM],below);
            }
          }
          c++;
        }

        // Do the left and right edge
        c1 = col - 1;
        c2 = col + colSpan;
        r = row ;
        last = row + rowSpan-1;
        last = PR_MIN(last,rowCount-1);
        
        while (r <= last)
        {
          // Add left edge cells
          if (c1 != -1)
          {
            if (r != row)
            {
              cellData = cellMap->GetCellAt(r,c1);
              if ((cellData != nsnull) && (cellData->mCell != left))
              {
                left = cellData->mCell;
                if (left != nsnull)
                  AppendLayoutData(boundaryCells[NS_SIDE_LEFT],left);
              }
            }
            else if (left != nsnull)
            {
              AppendLayoutData(boundaryCells[NS_SIDE_LEFT],left);
            }
          }

          if (c2 < colCount)
          {
            // Add right edge cells
            cellData = cellMap->GetCellAt(r,c2);
            if ((cellData != nsnull) && (cellData->mCell != right))
            {
              right = cellData->mCell;
              if (right != nsnull)
                AppendLayoutData(boundaryCells[NS_SIDE_RIGHT],right);
            }
          }
          r++;
        }
        
        cell->RecalcLayoutData(this,boundaryCells);
      }
    }
  }
  for (edge = 0; edge < 4; edge++)
    delete boundaryCells[edge];
}


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
NS_METHOD nsTableFrame::Paint(nsIPresContext& aPresContext,
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
      const nsStyleTable* tableStyle =
        (const nsStyleTable*)mStyleContext->GetStyleData(eStyleStruct_Table);

      nsRect  rect(0, 0, mRect.width, mRect.height);
      nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                      aDirtyRect, rect, *color, *spacing, 0, 0);
      PRIntn skipSides = GetSkipSides();
      if (NS_STYLE_BORDER_SEPARATE==tableStyle->mBorderCollapse)
      {
        nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                    aDirtyRect, rect, *spacing, mStyleContext, skipSides);
      }
      else
      {
        //printf("paint table frame\n");
        nsCSSRendering::PaintBorderEdges(aPresContext, aRenderingContext, this,
                                         aDirtyRect, rect,  &mBorderEdges, mStyleContext, skipSides);
      }
    }
  }
  // for debug...
  if ((NS_FRAME_PAINT_LAYER_DEBUG == aWhichLayer) && GetShowFrameBorders()) {
    aRenderingContext.SetColor(NS_RGB(0,255,0));
    aRenderingContext.DrawRect(0, 0, mRect.width, mRect.height);
  }

  PaintChildren(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
  return NS_OK;
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

PRBool nsTableFrame::NeedsReflow(const nsHTMLReflowState& aReflowState, const nsSize& aMaxSize)
{
  PRBool result = PR_TRUE;
  if (eReflowReason_Incremental != aReflowState.reason) 
  { // incremental reflows always need to be reflowed (for now)
    if (PR_TRUE==mIsInvariantWidth)
      result = PR_FALSE;
    // XXX TODO: other optimization cases...
  }
  return result;
}

nsresult nsTableFrame::AdjustSiblingsAfterReflow(nsIPresContext&        aPresContext,
                                                 InnerTableReflowState& aReflowState,
                                                 nsIFrame*              aKidFrame,
                                                 nscoord                aDeltaY)
{
  nsIFrame* lastKidFrame = aKidFrame;

  if (aDeltaY != 0) {
    // Move the frames that follow aKidFrame by aDeltaY
    nsIFrame* kidFrame;

    aKidFrame->GetNextSibling(&kidFrame);
    while (nsnull != kidFrame) {
      nsPoint        origin;
      nsIHTMLReflow* htmlReflow;
  
      // XXX We can't just slide the child if it has a next-in-flow
      kidFrame->GetOrigin(origin);
      origin.y += aDeltaY;
  
      // XXX We need to send move notifications to the frame...
      if (NS_OK == kidFrame->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow)) {
        htmlReflow->WillReflow(aPresContext);
      }
      kidFrame->MoveTo(origin.x, origin.y);

      // Get the next frame
      lastKidFrame = kidFrame;
      kidFrame->GetNextSibling(&kidFrame);
    }

  } else {
    // Get the last frame
    lastKidFrame = mFrames.LastChild();
  }

  // Update our running y-offset to reflect the bottommost child
  nsRect  rect;
  lastKidFrame->GetRect(rect);
  aReflowState.y = rect.YMost();

  return NS_OK;
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
NS_METHOD nsTableFrame::Reflow(nsIPresContext& aPresContext,
                               nsHTMLReflowMetrics& aDesiredSize,
                               const nsHTMLReflowState& aReflowState,
                               nsReflowStatus& aStatus)
{
  if (gsDebug==PR_TRUE) 
  {
    printf("-----------------------------------------------------------------\n");
    printf("nsTableFrame::Reflow: table %p reason %d given maxSize=%d,%d\n",
            this, aReflowState.reason, aReflowState.availableWidth, aReflowState.availableHeight);
  }

  // Initialize out parameter
  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width = 0;
    aDesiredSize.maxElementSize->height = 0;
  }
  aStatus = NS_FRAME_COMPLETE;

  nsresult rv = NS_OK;

  if (eReflowReason_Incremental == aReflowState.reason) {
    rv = IncrementalReflow(aPresContext, aDesiredSize, aReflowState, aStatus);
  }

  // NeedsReflow and IsFirstPassValid take into account reflow type = Initial_Reflow
  if (PR_TRUE==NeedsReflow(aReflowState, nsSize(aReflowState.availableWidth, aReflowState.availableHeight)))
  {
    PRBool needsRecalc=PR_FALSE;
    if (PR_TRUE==gsDebug || PR_TRUE==gsDebugIR) printf("TIF Reflow: needs reflow\n");
    if (eReflowReason_Initial!=aReflowState.reason && PR_FALSE==IsCellMapValid())
    {
      if (PR_TRUE==gsDebug || PR_TRUE==gsDebugIR) printf("TIF Reflow: cell map invalid, rebuilding...\n");
      if (nsnull!=mCellMap)
        delete mCellMap;
      mCellMap = new nsCellMap(0,0);
      ReBuildCellMap();
#ifdef NS_DEBUG
      if (PR_TRUE==gsDebugIR)
      {
        DumpCellMap();
        printf("tableFrame thinks colCount is %d\n", mEffectiveColCount);
      }
#endif
      needsRecalc=PR_TRUE;
    }
    if (PR_FALSE==IsFirstPassValid())
    {
      if (PR_TRUE==gsDebug || PR_TRUE==gsDebugIR) printf("TIF Reflow: first pass is invalid, rebuilding...\n");
      nsReflowReason reason = aReflowState.reason;
      if (eReflowReason_Initial!=reason)
        reason = eReflowReason_Resize;
      ComputeVerticalCollapsingBorders(aPresContext, 0, -1);
      rv = ResizeReflowPass1(aPresContext, aDesiredSize, aReflowState, aStatus, nsnull, reason, PR_TRUE);
      if (NS_FAILED(rv))
        return rv;
      needsRecalc=PR_TRUE;
    }
    if (PR_FALSE==IsColumnCacheValid())
    {
      if (PR_TRUE==gsDebug || PR_TRUE==gsDebugIR) printf("TIF Reflow: column cache is invalid, rebuilding...\n");
      needsRecalc=PR_TRUE;
    }
    if (PR_TRUE==needsRecalc)
    {
      if (PR_TRUE==gsDebug || PR_TRUE==gsDebugIR) printf("TIF Reflow: needs recalc. Calling BuildColumnCache...\n");
      BuildColumnCache(aPresContext, aDesiredSize, aReflowState, aStatus);
      RecalcLayoutData(aPresContext);  // Recalculate Layout Dependencies
      // if we needed to rebuild the column cache, the data stored in the layout strategy is invalid
      if (nsnull!=mTableLayoutStrategy)
      {
        if (PR_TRUE==gsDebug || PR_TRUE==gsDebugIR) printf("TIF Reflow: Re-init layout strategy\n");
        mTableLayoutStrategy->Initialize(aDesiredSize.maxElementSize, GetColCount());
        mColumnWidthsValid=PR_TRUE; //so we don't do this a second time below
      }
    }
    if (PR_FALSE==IsColumnWidthsValid())
    {
      if (PR_TRUE==gsDebug || PR_TRUE==gsDebugIR) printf("TIF Reflow: Re-init layout strategy\n");
      if (nsnull!=mTableLayoutStrategy)
      {
        mTableLayoutStrategy->Initialize(aDesiredSize.maxElementSize, GetColCount());
        mColumnWidthsValid=PR_TRUE;
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
    nsHTMLReflowState    reflowState(aReflowState);
    if (mPrevInFlow) {
      nsTableFrame* table = (nsTableFrame*)GetFirstInFlow();
      reflowState.availableWidth = table->mRect.width;
    } else {
      reflowState.availableWidth = mRect.width;
    }
    rv = ResizeReflowPass2(aPresContext, aDesiredSize, reflowState, aStatus);
    if (NS_FAILED(rv))
        return rv;
  }
  else
  {
    // set aDesiredSize and aMaxElementSize
  }

  // DumpCellMap is useful for debugging the results of an incremental reflow.  But it's noisy, 
  // so this module should not be checked in with the call enabled.
  //DumpCellMap();  

  if (PR_TRUE==gsDebug || PR_TRUE==gsDebugNT) 
  {
    if (nsnull!=aDesiredSize.maxElementSize)
      printf("%p: Inner table reflow complete, returning aDesiredSize = %d,%d and aMaxElementSize=%d,%d\n",
              this, aDesiredSize.width, aDesiredSize.height, 
              aDesiredSize.maxElementSize->width, aDesiredSize.maxElementSize->height);
    else
      printf("%p: Inner table reflow complete, returning aDesiredSize = %d,%d and NSNULL aMaxElementSize\n",
              this, aDesiredSize.width, aDesiredSize.height);
  }

  if (PR_TRUE==gsDebug) printf("end reflow for table %p\n", this);
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
NS_METHOD nsTableFrame::ResizeReflowPass1(nsIPresContext&          aPresContext,
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

  if (PR_TRUE==gsDebugNT) printf("%p nsTableFrame::ResizeReflow Pass1: maxSize=%d,%d\n",
                               this, aReflowState.availableWidth, aReflowState.availableHeight);
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
        if (PR_TRUE==gsDebugIR) printf("\nTIF IR: Reflow Pass 1 of unknown frame %p of type %d with reason=%d\n", 
                                       kidFrame, childDisplay->mDisplay, aReason);
        // rv intentionally not set here
        ReflowChild(kidFrame, aPresContext, kidSize, kidReflowState, aStatus);
        continue;
      }
      nsHTMLReflowState kidReflowState(aPresContext, aReflowState, kidFrame,
                                       availSize, aReason);
      // Note: we don't bother checking here for whether we should clear the
      // isTopOfPage reflow state flag, because we're dealing with an unconstrained
      // height and it isn't an issue...
      if (PR_TRUE==gsDebugIR) printf("\nTIF IR: Reflow Pass 1 of frame %p with reason=%d\n", kidFrame, aReason);
      ReflowChild(kidFrame, aPresContext, kidSize, kidReflowState, aStatus);

      // Place the child since some of its content fit in us.
      if (PR_TRUE==gsDebugNT) {
        printf ("%p: reflow of row group returned desired=%d,%d, max-element=%d,%d\n",
                this, kidSize.width, kidSize.height, kidMaxSize.width, kidMaxSize.height);
      }
      kidFrame->SetRect(nsRect(0, 0, kidSize.width, kidSize.height));
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
        if (PR_TRUE==gsDebugIR) printf("\nTIF IR: Reflow Pass 1 of colgroup frame %p with reason=%d\n", kidFrame, aReason);
        ReflowChild(kidFrame, aPresContext, kidSize, kidReflowState, aStatus);
        kidFrame->SetRect(nsRect(0, 0, 0, 0));
      }
    }
  }

  aDesiredSize.width = kidSize.width;
  mFirstPassValid = PR_TRUE;

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
nsTableFrame::PushChildren(nsIFrame* aFromChild, nsIFrame* aPrevSibling)
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
    nextInFlow->mFrames.InsertFrames(mNextInFlow, prevSibling, aFromChild);
  }
  else {
    // Add the frames to our overflow list
    NS_ASSERTION(mOverflowFrames.IsEmpty(), "bad overflow list");
    mOverflowFrames.SetFrames(aFromChild);
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
nsTableFrame::MoveOverflowToChildList()
{
  PRBool result = PR_FALSE;

  // Check for an overflow list with our prev-in-flow
  nsTableFrame* prevInFlow = (nsTableFrame*)mPrevInFlow;
  if (nsnull != prevInFlow) {
    if (prevInFlow->mOverflowFrames.NotEmpty()) {
      mFrames.Join(this, prevInFlow->mOverflowFrames);
      result = PR_TRUE;
    }
  }

  // It's also possible that we have an overflow list for ourselves
  if (mOverflowFrames.NotEmpty()) {
    mFrames.Join(nsnull, mOverflowFrames);
    result = PR_TRUE;
  }
  return result;
}

/** the second of 2 reflow passes
  */
NS_METHOD nsTableFrame::ResizeReflowPass2(nsIPresContext&          aPresContext,
                                          nsHTMLReflowMetrics&     aDesiredSize,
                                          const nsHTMLReflowState& aReflowState,
                                          nsReflowStatus&          aStatus)
{
  NS_PRECONDITION(aReflowState.frame == this, "bad reflow state");
  NS_PRECONDITION(aReflowState.parentReflowState->frame == mParent,
                  "bad parent reflow state");
  if (PR_TRUE==gsDebugNT)
    printf("%p nsTableFrame::ResizeReflow Pass2: maxSize=%d,%d\n",
           this, aReflowState.availableWidth, aReflowState.availableHeight);

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
  MoveOverflowToChildList();

  // Reflow the existing frames
  if (mFrames.NotEmpty()) {
    rv = ReflowMappedChildren(aPresContext, aDesiredSize, state, aStatus);
  }

  // Did we successfully reflow our mapped children?
  if (NS_FRAME_COMPLETE == aStatus) {
    // Any space left?
    PRInt32 numKids;
    mContent->ChildCount(numKids);
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
  if (NS_STYLE_BORDER_COLLAPSE==GetBorderCollapseStyle())
    DidComputeHorizontalCollapsingBorders(aPresContext, 0, 10000);

#ifdef NS_DEBUG
  //PostReflowCheck(aStatus);
#endif

  return rv;

}

// collapsing row groups, rows, col groups and cols are accounted for after both passes of
// reflow so that it has no effect on the calculations of reflow.
NS_METHOD nsTableFrame::AdjustForCollapsingRows(nsIPresContext& aPresContext, 
                                                nscoord&        aHeight)
{
  // determine which row groups and rows are collapsed
  nsIFrame* childFrame;
  FirstChild(nsnull, &childFrame);
  while (nsnull != childFrame) { 
    const nsStyleDisplay* groupDisplay;
    childFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)groupDisplay));
    if (IsRowGroup(groupDisplay->mDisplay)) {
      PRBool groupIsCollapsed = (NS_STYLE_VISIBILITY_COLLAPSE == groupDisplay->mVisible);

      nsTableRowFrame* rowFrame = nsnull;
      childFrame->FirstChild(nsnull, (nsIFrame**)&rowFrame);
      PRInt32 rowX = 0;
      while (nsnull != rowFrame) { 
        const nsStyleDisplay *rowDisplay;
        rowFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)rowDisplay));
        if (NS_STYLE_DISPLAY_TABLE_ROW == rowDisplay->mDisplay) {
          if (groupIsCollapsed || (NS_STYLE_VISIBILITY_COLLAPSE == rowDisplay->mVisible)) {
            mCellMap->SetRowCollapsedAt(rowX, PR_TRUE);
          }
        }
        rowFrame->GetNextSibling((nsIFrame**)&rowFrame);
        rowX++;
      }
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
  PRInt32 rowX = 0;
  PRBool collapseGroup;

  while (nsnull != groupFrame) {
    const nsStyleDisplay* groupDisplay;
    groupFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)groupDisplay));
    if (IsRowGroup(groupDisplay->mDisplay)) { 
      collapseGroup = (NS_STYLE_VISIBILITY_COLLAPSE == groupDisplay->mVisible);
      nsIFrame* rowFrame;
      groupFrame->FirstChild(nsnull, &rowFrame);

      while (nsnull != rowFrame) {
        const nsStyleDisplay* rowDisplay;
        rowFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)rowDisplay));
        if (NS_STYLE_DISPLAY_TABLE_ROW == rowDisplay->mDisplay) {
          nsRect rowRect;
          rowFrame->GetRect(rowRect);
          if (collapseGroup || (NS_STYLE_VISIBILITY_COLLAPSE == rowDisplay->mVisible)) {
            yGroupOffset += rowRect.height;
            rowRect.height = 0;
            rowFrame->SetRect(rowRect);
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
                cFrame->SetCollapseOffsetY(-yGroupOffset);
                cFrame->SetRect(cRect);
              }
              cellFrame->GetNextSibling(&cellFrame);
            }
            // check if a cell above spans into here
             //if (!collapseGroup) {
              PRInt32 numCols = mCellMap->GetColCount();
              nsTableCellFrame* lastCell = nsnull;
              for (int colX = 0; colX < numCols; colX++) {
                CellData* cellData = mCellMap->GetCellAt(rowX, colX);
                if (cellData && !cellData->mCell) { // a cell above is spanning into here
                  // adjust the real cell's rect only once
                  nsTableCellFrame* realCell = cellData->mRealCell->mCell;
                  if (realCell != lastCell) {
                    nsRect realRect;
                    realCell->GetRect(realRect);
                    realRect.height -= rowRect.height;
                    realCell->SetRect(realRect);
                  }
                  lastCell = realCell;
                }
              }
            //}
          } else { // row is not collapsed but needs to be adjusted by those that are
            rowRect.y -= yGroupOffset;
            rowFrame->SetRect(rowRect);
          }
          rowX++;
        }
        rowFrame->GetNextSibling(&rowFrame);
      } // end row frame while
    }
    nsRect groupRect;
    groupFrame->GetRect(groupRect);
    groupRect.height -= yGroupOffset;
    groupRect.y -= yTotalOffset;
    groupFrame->SetRect(groupRect);

    yTotalOffset += yGroupOffset;
    yGroupOffset = 0;
    groupFrame->GetNextSibling(&groupFrame);
  } // end group frame while

  aHeight -= yTotalOffset;
 
  return NS_OK;
}

NS_METHOD nsTableFrame::AdjustForCollapsingCols(nsIPresContext& aPresContext, 
                                                nscoord&        aWidth)
{
  // determine which col groups and cols are collapsed
  nsIFrame* childFrame = mColGroups.FirstChild();
  while (nsnull != childFrame) { 
    const nsStyleDisplay* groupDisplay;
    GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)groupDisplay));
    PRBool groupIsCollapsed = (NS_STYLE_VISIBILITY_COLLAPSE == groupDisplay->mVisible);

    nsTableColFrame* colFrame = nsnull;
    childFrame->FirstChild(nsnull, (nsIFrame**)&colFrame);
    PRInt32 colX = 0;
    while (nsnull != colFrame) { 
      const nsStyleDisplay *colDisplay;
      colFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)colDisplay));
      if (NS_STYLE_DISPLAY_TABLE_COLUMN == colDisplay->mDisplay) {
        if (groupIsCollapsed || (NS_STYLE_VISIBILITY_COLLAPSE == colDisplay->mVisible)) {
          mCellMap->SetColCollapsedAt(colX, PR_TRUE);
        }
      }
      colFrame->GetNextSibling((nsIFrame**)&colFrame);
      colX++;
    }
    childFrame->GetNextSibling(&childFrame);
  }

  if (mCellMap->GetNumCollapsedCols() <= 0) { 
    return NS_OK; // no collapsed cols, we're done
  }

  // collapse the cols and/or col groups
  PRInt32 numRows = mCellMap->GetRowCount();
  nsIFrame* groupFrame = mColGroups.FirstChild(); 
  nscoord cellSpacingX = GetCellSpacingX();
  nscoord xOffset = 0;
  PRInt32 colX = 0;
  while (nsnull != groupFrame) {
    const nsStyleDisplay* groupDisplay;
    groupFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)groupDisplay));
    PRBool collapseGroup = (NS_STYLE_VISIBILITY_COLLAPSE == groupDisplay->mVisible);
    nsIFrame* colFrame;
    groupFrame->FirstChild(nsnull, &colFrame);
    while (nsnull != colFrame) {
      const nsStyleDisplay* colDisplay;
      colFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)colDisplay));
      if (NS_STYLE_DISPLAY_TABLE_COLUMN == colDisplay->mDisplay) {
        PRBool collapseCol = (NS_STYLE_VISIBILITY_COLLAPSE == colDisplay->mVisible);
        PRInt32 colSpan = ((nsTableColFrame*)colFrame)->GetSpan();
        for (PRInt32 spanX = 0; spanX < colSpan; spanX++) {
          PRInt32 colWidth = GetColumnWidth(colX+spanX);
          if (collapseGroup || collapseCol) {
            xOffset += colWidth + cellSpacingX;
          }
          nsTableCellFrame* lastCell  = nsnull;
          nsTableCellFrame* cellFrame = nsnull;
          for (PRInt32 rowX = 0; rowX < numRows; rowX++) {
            CellData* cellData = mCellMap->GetCellAt(rowX, colX+spanX);
            nsRect cellRect;
            if (cellData) {
              cellFrame = cellData->mCell;
              if (cellFrame) { // the cell originates at (rowX, colX)
                cellFrame->GetRect(cellRect);
                if (collapseGroup || collapseCol) {
                  if (lastCell != cellFrame) { // do it only once if there is a row span
                    cellRect.width -= colWidth;
                    cellFrame->SetCollapseOffsetX(-xOffset);
                  }
                } else { // the cell is not in a collapsed col but needs to move
                  cellRect.x -= xOffset;
                }
                cellFrame->SetRect(cellRect);
              // if the cell does not originate at (rowX, colX), adjust the real cells width
              } else if (collapseGroup || collapseCol) { 
                cellFrame = cellData->mRealCell->mCell;
                if ((cellFrame) && (lastCell != cellFrame)) {
                  cellFrame->GetRect(cellRect);
                  cellRect.width -= colWidth + cellSpacingX;
                  cellFrame->SetRect(cellRect);
                }
              }
            }
            lastCell = cellFrame;
          }
        }
        colX += colSpan;
      }
      colFrame->GetNextSibling(&colFrame);
    } // inner while
    groupFrame->GetNextSibling(&groupFrame);
  } // outer while

  aWidth -= xOffset;
 
  return NS_OK;
}

NS_METHOD nsTableFrame::IncrementalReflow(nsIPresContext& aPresContext,
                                          nsHTMLReflowMetrics& aDesiredSize,
                                          const nsHTMLReflowState& aReflowState,
                                          nsReflowStatus& aStatus)
{
  if (PR_TRUE==gsDebugIR) printf("\nTIF IR: IncrementalReflow\n");
  nsresult  rv = NS_OK;

  // create an inner table reflow state
  const nsStyleSpacing* mySpacing = (const nsStyleSpacing*)
    mStyleContext->GetStyleData(eStyleStruct_Spacing);
  nsMargin borderPadding;
  GetTableBorder (borderPadding);
  nsMargin padding;
  mySpacing->GetPadding(padding);
  borderPadding += padding;
  InnerTableReflowState state(aPresContext, aReflowState, borderPadding);

  // determine if this frame is the target or not
  nsIFrame *target=nsnull;
  rv = aReflowState.reflowCommand->GetTarget(target);
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
      aReflowState.reflowCommand->GetNext(nextFrame);

      // Recover our reflow state
      //RecoverState(state, nextFrame);
      rv = IR_TargetIsChild(aPresContext, aDesiredSize, state, aStatus, nextFrame);
    }
  }
  return rv;
}

NS_METHOD nsTableFrame::IR_TargetIsMe(nsIPresContext&        aPresContext,
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
  if (PR_TRUE==gsDebugIR) printf("TIF IR: IncrementalReflow_TargetIsMe with type=%d\n", type);
  switch (type)
  {
  case nsIReflowCommand::FrameInserted :
    NS_ASSERTION(nsnull!=objectFrame, "bad objectFrame");
    NS_ASSERTION(nsnull!=childDisplay, "bad childDisplay");
    if (NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP == childDisplay->mDisplay)
    {
      rv = IR_ColGroupInserted(aPresContext, aDesiredSize, aReflowState, aStatus, 
                               (nsTableColGroupFrame*)objectFrame, PR_FALSE);
    }
    else if (IsRowGroup(childDisplay->mDisplay))
    {
      rv = IR_RowGroupInserted(aPresContext, aDesiredSize, aReflowState, aStatus, 
                               GetRowGroupFrameFor(objectFrame, childDisplay), PR_FALSE);
    }
    else
    {
      rv = AddFrame(aReflowState.reflowState, objectFrame);
    }
    break;
  
  case nsIReflowCommand::FrameAppended :
    NS_ASSERTION(nsnull!=objectFrame, "bad objectFrame");
    NS_ASSERTION(nsnull!=childDisplay, "bad childDisplay");
    if (NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP == childDisplay->mDisplay)
    {
      rv = IR_ColGroupAppended(aPresContext, aDesiredSize, aReflowState, aStatus, 
                              (nsTableColGroupFrame*)objectFrame);
    }
    else if (IsRowGroup(childDisplay->mDisplay))
    {
      rv = IR_RowGroupAppended(aPresContext, aDesiredSize, aReflowState, aStatus, 
                              GetRowGroupFrameFor(objectFrame, childDisplay));
    }
    else
    { // no optimization to be done for Unknown frame types, so just reuse the Inserted method
      rv = AddFrame(aReflowState.reflowState, objectFrame);
    }
    break;

  /*
  case nsIReflowCommand::FrameReplaced :
    NS_ASSERTION(nsnull!=objectFrame, "bad objectFrame");
    NS_ASSERTION(nsnull!=childDisplay, "bad childDisplay");

  */

  case nsIReflowCommand::FrameRemoved :
    NS_ASSERTION(nsnull!=objectFrame, "bad objectFrame");
    NS_ASSERTION(nsnull!=childDisplay, "bad childDisplay");
    if (NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP == childDisplay->mDisplay)
    {
      rv = IR_ColGroupRemoved(aPresContext, aDesiredSize, aReflowState, aStatus, 
                              (nsTableColGroupFrame*)objectFrame);
    }
    else if (IsRowGroup(childDisplay->mDisplay))
    {
      rv = IR_RowGroupRemoved(aPresContext, aDesiredSize, aReflowState, aStatus, 
                              GetRowGroupFrameFor(objectFrame, childDisplay));
    }
    else
    {
      rv = RemoveAFrame(objectFrame);
    }
    break;

  case nsIReflowCommand::StyleChanged :
    rv = IR_StyleChanged(aPresContext, aDesiredSize, aReflowState, aStatus);
    break;

  case nsIReflowCommand::ContentChanged :
    NS_ASSERTION(PR_FALSE, "illegal reflow type: ContentChanged");
    rv = NS_ERROR_ILLEGAL_VALUE;
    break;
  
  case nsIReflowCommand::PullupReflow:
  case nsIReflowCommand::PushReflow:
  case nsIReflowCommand::CheckPullupReflow :
  case nsIReflowCommand::UserDefined :
  default:
    NS_NOTYETIMPLEMENTED("unimplemented reflow command type");
    rv = NS_ERROR_NOT_IMPLEMENTED;
    if (PR_TRUE==gsDebugIR) printf("TIF IR: reflow command not implemented.\n");
    break;
  }

  return rv;
}

NS_METHOD nsTableFrame::IR_ColGroupInserted(nsIPresContext&        aPresContext,
                                            nsHTMLReflowMetrics&   aDesiredSize,
                                            InnerTableReflowState& aReflowState,
                                            nsReflowStatus&        aStatus,
                                            nsTableColGroupFrame * aInsertedFrame,
                                            PRBool                 aReplace)
{
  if (PR_TRUE==gsDebugIR) printf("TIF IR: IR_ColGroupInserted for frame %p\n", aInsertedFrame);
  nsresult rv=NS_OK;
  PRBool adjustStartingColIndex=PR_FALSE;
  PRInt32 startingColIndex=0;
  // find out what frame to insert aInsertedFrame after
  nsIFrame *frameToInsertAfter=nsnull;
  rv = aReflowState.reflowState.reflowCommand->GetPrevSiblingFrame(frameToInsertAfter); 
  // insert aInsertedFrame as the first child.  Set its start col index to 0
  if (nsnull==frameToInsertAfter)
  {
    mColGroups.InsertFrame(nsnull, nsnull, aInsertedFrame);
    startingColIndex += aInsertedFrame->SetStartColumnIndex(0);
    adjustStartingColIndex=PR_TRUE;
  }
  nsIFrame *childFrame=mColGroups.FirstChild();
  nsIFrame *prevSib=nsnull;
  while ((NS_SUCCEEDED(rv)) && (nsnull!=childFrame))
  {
    if ((nsnull!=frameToInsertAfter) && (childFrame==frameToInsertAfter))
    {
      nsIFrame *nextSib=nsnull;
      frameToInsertAfter->GetNextSibling(&nextSib);
      aInsertedFrame->SetNextSibling(nextSib);
      frameToInsertAfter->SetNextSibling(aInsertedFrame);
      // account for childFrame being a COLGROUP now
      if (PR_FALSE==adjustStartingColIndex) // we haven't gotten to aDeletedFrame yet
        startingColIndex += ((nsTableColGroupFrame *)childFrame)->GetColumnCount();
      // skip ahead to aInsertedFrame, since we just handled the frame we inserted after
      childFrame=aInsertedFrame;
      adjustStartingColIndex=PR_TRUE; // now that we've inserted aInsertedFrame, 
                                      // start adjusting subsequent col groups' starting col index including aInsertedFrame
    }
    if (PR_FALSE==adjustStartingColIndex) // we haven't gotten to aDeletedFrame yet
      startingColIndex += ((nsTableColGroupFrame *)childFrame)->GetColumnCount();
    else // we've removed aDeletedFrame, now adjust the starting col index of all subsequent col groups
      startingColIndex += ((nsTableColGroupFrame *)childFrame)->SetStartColumnIndex(startingColIndex);
    prevSib=childFrame;
    rv = childFrame->GetNextSibling(&childFrame);
  }

  InvalidateColumnCache();
  //XXX: what we want to do here is determine if the new COL information changes anything about layout
  //     if not, skip invalidating the first passs
  //     if so, and we can fix the first pass info
  return rv;

}

NS_METHOD nsTableFrame::IR_ColGroupAppended(nsIPresContext&        aPresContext,
                                            nsHTMLReflowMetrics&   aDesiredSize,
                                            InnerTableReflowState& aReflowState,
                                            nsReflowStatus&        aStatus,
                                            nsTableColGroupFrame * aAppendedFrame)
{
  if (PR_TRUE==gsDebugIR) printf("TIF IR: IR_ColGroupAppended for frame %p\n", aAppendedFrame);
  nsresult rv=NS_OK;
  PRInt32 startingColIndex=0;
  nsIFrame *childFrame=mColGroups.FirstChild();
  nsIFrame *lastChild=childFrame;
  while ((NS_SUCCEEDED(rv)) && (nsnull!=childFrame))
  {
    startingColIndex += ((nsTableColGroupFrame *)childFrame)->GetColumnCount();
    lastChild=childFrame;
    rv = childFrame->GetNextSibling(&childFrame);
  }

  // append aAppendedFrame
  if (nsnull!=lastChild)
    lastChild->SetNextSibling(aAppendedFrame);
  else
    mColGroups.SetFrames(aAppendedFrame);

  aAppendedFrame->SetStartColumnIndex(startingColIndex);
  
#if 0

we would only want to do this if manufactured col groups were invisible to the DOM.  Since they 
currently are visible, they should behave just as if they were content-backed "real" colgroups
If this decision is changed, the code below is a half-finished attempt to rationalize the situation.
It requires having built a list of the colGroups before we get to this point.

  // look at the last col group.  If it is implicit, and it's cols are implicit, then
  // it and its cols were manufactured for table layout.  
  // Delete it if possible, otherwise move it to the end of the list 
  
  if (0<colGroupCount)
  {
    nsTableColGroupFrame *colGroup = (nsTableColGroupFrame *)(colGroupList.ElementAt(colGroupCount-1));
    if (PR_TRUE==colGroup->IsManufactured())
    { // account for the new COLs that were added in aAppendedFrame
      // first, try to delete the implicit colgroup
      
      // if we couldn't delete it, move the implicit colgroup to the end of the list
      // and adjust it's col indexes
      nsIFrame *colGroupNextSib;
      colGroup->GetNextSibling(colGroupNextSib);
      childFrame=mColGroups.FirstChild();
      nsIFrame * prevSib=nsnull;
      rv = NS_OK;
      while ((NS_SUCCEEDED(rv)) && (nsnull!=childFrame))
      {
        if (childFrame==colGroup)
        {
          if (nsnull!=prevSib) // colGroup is in the middle of the list, remove it
            prevSib->SetNextSibling(colGroupNextSib);
          else  // colGroup was the first child, so set it's next sib to first child
            mColGroups.SetFrames(colGroupNextSib);
          aAppendedFrame->SetNextSibling(colGroup); // place colGroup at the end of the list
          colGroup->SetNextSibling(nsnull);
          break;
        }
        prevSib=childFrame;
        rv = childFrame->GetNextSibling(childFrame);
      }
    }
  }
#endif


  InvalidateColumnCache();
  //XXX: what we want to do here is determine if the new COL information changes anything about layout
  //     if not, skip invalidating the first passs
  //     if so, and we can fix the first pass info
  return rv;
}


NS_METHOD nsTableFrame::IR_ColGroupRemoved(nsIPresContext&        aPresContext,
                                           nsHTMLReflowMetrics&   aDesiredSize,
                                           InnerTableReflowState& aReflowState,
                                           nsReflowStatus&        aStatus,
                                           nsTableColGroupFrame * aDeletedFrame)
{
  if (PR_TRUE==gsDebugIR) printf("TIF IR: IR_ColGroupRemoved for frame %p\n", aDeletedFrame);
  nsresult rv=NS_OK;
  PRBool adjustStartingColIndex=PR_FALSE;
  PRInt32 startingColIndex=0;
  nsIFrame *childFrame=mColGroups.FirstChild();
  nsIFrame *prevSib=nsnull;
  while ((NS_SUCCEEDED(rv)) && (nsnull!=childFrame))
  {
    if (childFrame==aDeletedFrame)
    {
      nsIFrame *deleteFrameNextSib=nsnull;
      aDeletedFrame->GetNextSibling(&deleteFrameNextSib);
      if (nsnull!=prevSib)
        prevSib->SetNextSibling(deleteFrameNextSib);
      else
        mColGroups.SetFrames(deleteFrameNextSib);
      childFrame=deleteFrameNextSib;
      if (nsnull==childFrame)
        break;
      adjustStartingColIndex=PR_TRUE; // now that we've removed aDeletedFrame, start adjusting subsequent col groups' starting col index
    }
    const nsStyleDisplay *display;
    childFrame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct *&)display);
    if (NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP == display->mDisplay)
    {
      if (PR_FALSE==adjustStartingColIndex) // we haven't gotten to aDeletedFrame yet
        startingColIndex += ((nsTableColGroupFrame *)childFrame)->GetColumnCount();
      else // we've removed aDeletedFrame, now adjust the starting col index of all subsequent col groups
        startingColIndex += ((nsTableColGroupFrame *)childFrame)->SetStartColumnIndex(startingColIndex);
    }
    prevSib=childFrame;
    rv = childFrame->GetNextSibling(&childFrame);
  }

  InvalidateColumnCache();
  //XXX: what we want to do here is determine if the new COL information changes anything about layout
  //     if not, skip invalidating the first passs
  //     if so, and we can fix the first pass info
  return rv;
}

NS_METHOD nsTableFrame::IR_StyleChanged(nsIPresContext&        aPresContext,
                                        nsHTMLReflowMetrics&   aDesiredSize,
                                        InnerTableReflowState& aReflowState,
                                        nsReflowStatus&        aStatus)
{
  if (PR_TRUE==gsDebugIR) printf("TIF IR: IR_StyleChanged for frame %p\n", this);
  nsresult rv = NS_OK;
  // we presume that all the easy optimizations were done in the nsHTMLStyleSheet before we were called here
  // XXX: we can optimize this when we know which style attribute changed
  //      if something like border changes, we need to do pass1 again
  //      but if something like width changes from 100 to 200, we just need to do pass2
  InvalidateFirstPassCache();
  return rv;
}

NS_METHOD nsTableFrame::IR_RowGroupInserted(nsIPresContext&        aPresContext,
                                            nsHTMLReflowMetrics&   aDesiredSize,
                                            InnerTableReflowState& aReflowState,
                                            nsReflowStatus&        aStatus,
                                            nsTableRowGroupFrame * aInsertedFrame,
                                            PRBool                 aReplace)
{
  if (PR_TRUE==gsDebugIR) printf("TIF IR: IR_RowGroupInserted for frame %p\n", aInsertedFrame);
  nsresult rv = AddFrame(aReflowState.reflowState, aInsertedFrame);
  if (NS_FAILED(rv))
    return rv;
  
  // do a pass-1 layout of all the cells in all the rows of the rowgroup
  rv = ResizeReflowPass1(aPresContext, aDesiredSize, aReflowState.reflowState, aStatus, 
                         aInsertedFrame, eReflowReason_Initial, PR_FALSE);
  if (NS_FAILED(rv))
    return rv;

  InvalidateCellMap();
  InvalidateColumnCache();

  return rv;
}

// since we know we're doing an append here, we can optimize
NS_METHOD nsTableFrame::IR_RowGroupAppended(nsIPresContext&        aPresContext,
                                            nsHTMLReflowMetrics&   aDesiredSize,
                                            InnerTableReflowState& aReflowState,
                                            nsReflowStatus&        aStatus,
                                            nsTableRowGroupFrame * aAppendedFrame)
{
  if (PR_TRUE==gsDebugIR) printf("TIF IR: IR_RowGroupAppended for frame %p\n", aAppendedFrame);
  // hook aAppendedFrame into the child list
  nsresult rv = AddFrame(aReflowState.reflowState, aAppendedFrame);
  if (NS_FAILED(rv))
    return rv;

  // account for the cells in the rows that are children of aAppendedFrame
  // this will add the content of the rowgroup to the cell map
  rv = DidAppendRowGroup(aAppendedFrame);
  if (NS_FAILED(rv))
    return rv;

  // do a pass-1 layout of all the cells in all the rows of the rowgroup
  rv = ResizeReflowPass1(aPresContext, aDesiredSize, aReflowState.reflowState, aStatus, 
                         aAppendedFrame, eReflowReason_Initial, PR_TRUE);
  if (NS_FAILED(rv))
    return rv;

  // if we've added any columns, we need to rebuild the column cache
  // XXX: it would be nice to have a mechanism to just extend the column cache, rather than rebuild it completely
  InvalidateColumnCache();

  // if any column widths have to change due to this, rebalance column widths
  //XXX need to calculate this, but for now just do it
  InvalidateColumnWidths();  

  return rv;
}

NS_METHOD nsTableFrame::IR_RowGroupRemoved(nsIPresContext&        aPresContext,
                                           nsHTMLReflowMetrics&   aDesiredSize,
                                           InnerTableReflowState& aReflowState,
                                           nsReflowStatus&        aStatus,
                                           nsTableRowGroupFrame * aDeletedFrame)
{
  if (PR_TRUE==gsDebugIR) printf("TIF IR: IR_RowGroupRemoved for frame %p\n", aDeletedFrame);
  nsresult rv = RemoveAFrame(aDeletedFrame);
  InvalidateCellMap();
  InvalidateColumnCache();

  // if any column widths have to change due to this, rebalance column widths
  //XXX need to calculate this, but for now just do it
  InvalidateColumnWidths();

  return rv;
}


NS_METHOD nsTableFrame::IR_TargetIsChild(nsIPresContext&        aPresContext,
                                         nsHTMLReflowMetrics&   aDesiredSize,
                                         InnerTableReflowState& aReflowState,
                                         nsReflowStatus&        aStatus,
                                         nsIFrame *             aNextFrame)

{
  nsresult rv;
  if (PR_TRUE==gsDebugIR) printf("\nTIF IR: IR_TargetIsChild\n");

  // Remember the old rect
  nsRect  oldKidRect;
  aNextFrame->GetRect(oldKidRect);

  // Pass along the reflow command
  nsHTMLReflowMetrics desiredSize(nsnull);
  // XXX Correctly compute the available space...
  nsSize  availSpace(aReflowState.reflowState.availableWidth, aReflowState.reflowState.availableHeight);
  nsHTMLReflowState kidReflowState(aPresContext, aReflowState.reflowState,
                                   aNextFrame, availSpace);

  rv = ReflowChild(aNextFrame, aPresContext, desiredSize, kidReflowState, aStatus);

  // Resize the row group frame
  nsRect  kidRect;
  aNextFrame->GetRect(kidRect);
  aNextFrame->SizeTo(desiredSize.width, desiredSize.height);

#if 1
  // XXX For the time being just fall through and treat it like a
  // pass 2 reflow...
  // calling intialize here resets all the cached info based on new table content 
  InvalidateColumnWidths();
#else
  // XXX Hack...
  AdjustSiblingsAfterReflow(&aPresContext, aReflowState, aNextFrame, desiredSize.height -
                            oldKidRect.height);
  aDesiredSize.width = mRect.width;
  aDesiredSize.height = state.y + myBorderPadding.top + myBorderPadding.bottom;
  return NS_OK;
#endif
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
    desiredWidth =  tableLayoutStrategy->GetTableMaxWidth();
  }
  return desiredWidth;
}

// Position and size aKidFrame and update our reflow state. The origin of
// aKidRect is relative to the upper-left origin of our frame
void nsTableFrame::PlaceChild(nsIPresContext&    aPresContext,
                              InnerTableReflowState& aReflowState,
                              nsIFrame*          aKidFrame,
                              const nsRect&      aKidRect,
                              nsSize*            aMaxElementSize,
                              nsSize&            aKidMaxElementSize)
{
  if (PR_TRUE==gsDebug)
    printf ("table: placing row group at %d, %d, %d, %d\n",
           aKidRect.x, aKidRect.y, aKidRect.width, aKidRect.height);
  // Place and size the child
  aKidFrame->SetRect(aKidRect);

  // Adjust the running y-offset
  aReflowState.y += aKidRect.height;

  // If our height is constrained, then update the available height
  if (PR_FALSE == aReflowState.unconstrainedHeight) {
    aReflowState.availSize.height -= aKidRect.height;
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
    aKidFrame->MoveTo(origin.x, origin.y);
    
    // Move the footer below the body row group frame
    aReflowState.footerFrame->GetOrigin(origin);
    origin.y += aKidRect.height;
    aReflowState.footerFrame->MoveTo(origin.x, origin.y);
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
    if (gsDebug)
      printf("%p placeChild set MES->width to %d\n", 
             this, aMaxElementSize->width);
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
NS_METHOD nsTableFrame::ReflowMappedChildren(nsIPresContext& aPresContext,
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
    if (eReflowReason_Incremental==reason)
      reason = eReflowReason_Resize;
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
      if (PR_TRUE==gsDebugIR) printf("\nTIF IR: Reflow Pass 2 of frame %p with reason=%d\n", kidFrame, reason);
      rv = ReflowChild(kidFrame, aPresContext, desiredSize, kidReflowState, aStatus);
      // Did the child fit?
      if (desiredSize.height > kidAvailSize.height) {
        if (aReflowState.firstBodySection && (kidFrame != aReflowState.firstBodySection)) {
          // The child is too tall to fit at all in the available space, and it's
          // not a header/footer or our first row group frame
          PushChildren(kidFrame, prevKidFrame);
          aStatus = NS_FRAME_NOT_COMPLETE;
          break;
        }
      }

      // Place the child
      nsRect kidRect (x, y, desiredSize.width, desiredSize.height);
      if (PR_TRUE==IsRowGroup(childDisplay->mDisplay))
      {
        // we don't want to adjust the maxElementSize if this is an initial reflow
        // it was set by the TableLayoutStrategy and shouldn't be changed.
        nsSize *requestedMaxElementSize = nsnull;
        if (eReflowReason_Initial != aReflowState.reflowState.reason)
          requestedMaxElementSize = aDesiredSize.maxElementSize;
        PlaceChild(aPresContext, aReflowState, kidFrame, kidRect,
                   requestedMaxElementSize, kidMaxElementSize);
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

          aPresContext.GetShell(&presShell);
          presShell->GetStyleSet(&styleSet);
          NS_RELEASE(presShell);
          styleSet->CreateContinuingFrame(&aPresContext, kidFrame, this, &continuingFrame);
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
          PushChildren(nextSibling, kidFrame);
        }
        break;
      }
    }
    else
    {// it's an unknown frame type, give it a generic reflow and ignore the results
        nsHTMLReflowState kidReflowState(aPresContext,
                                         aReflowState.reflowState, kidFrame,
                                         nsSize(0,0), eReflowReason_Resize);
        nsHTMLReflowMetrics desiredSize(nsnull);
        if (PR_TRUE==gsDebug) printf("\nTIF : Reflow Pass 2 of unknown frame %p of type %d with reason=%d\n", 
                                       kidFrame, childDisplay->mDisplay, eReflowReason_Resize);
        ReflowChild(kidFrame, aPresContext, desiredSize, kidReflowState, aStatus);
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
NS_METHOD nsTableFrame::PullUpChildren(nsIPresContext& aPresContext,
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
      if (nextInFlow->mOverflowFrames.NotEmpty()) {
        // XXX use nsFrameList::Join
        // Move the overflow list to become the child list
        nextInFlow->AppendChildren(nextInFlow->mOverflowFrames.FirstChild(), PR_TRUE);
        nextInFlow->mOverflowFrames.SetFrames(nsnull);
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

    rv = ReflowChild(kidFrame, aPresContext, kidSize, kidReflowState, aStatus);

    // Did the child fit?
    if ((kidSize.height > aReflowState.availSize.height) && mFrames.NotEmpty()) {
      // The child is too wide to fit in the available space, and it's
      // not our first child
      //XXX: Troy
      aStatus = NS_FRAME_NOT_COMPLETE;
      break;
    }

    nsRect kidRect (0, 0, kidSize.width, kidSize.height);
    kidRect.y += aReflowState.y;
    const nsStyleDisplay *childDisplay;
    kidFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)childDisplay));
    if (PR_TRUE==IsRowGroup(childDisplay->mDisplay))
    {
      PlaceChild(aPresContext, aReflowState, kidFrame, kidRect, aDesiredSize.maxElementSize, *pKidMaxElementSize);
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
        aPresContext.GetShell(&presShell);
        presShell->GetStyleSet(&styleSet);
        NS_RELEASE(presShell);
        styleSet->CreateContinuingFrame(&aPresContext, kidFrame, this, &continuingFrame);
        NS_RELEASE(styleSet);

        // Add the continuing frame to our sibling list and then push
        // it to the next-in-flow. This ensures the next-in-flow's
        // content offsets and child count are set properly. Note that
        // we can safely assume that the continuation is complete so
        // we pass PR_TRUE into PushChidren
        kidFrame->SetNextSibling(continuingFrame);

        PushChildren(continuingFrame, kidFrame);
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
void nsTableFrame::BalanceColumnWidths(nsIPresContext& aPresContext, 
                                       const nsHTMLReflowState& aReflowState,
                                       const nsSize& aMaxSize, 
                                       nsSize* aMaxElementSize)
{
  NS_ASSERTION(nsnull==mPrevInFlow, "never ever call me on a continuing frame!");
  NS_ASSERTION(nsnull!=mCellMap, "never ever call me until the cell map is built!");
  NS_ASSERTION(nsnull!=mColumnWidths, "never ever call me until the col widths array is built!");

  PRInt32 numCols = mCellMap->GetColCount();
  if (numCols>mColumnWidthsLength)
  {
    PRInt32 priorColumnWidthsLength=mColumnWidthsLength;
    while (numCols>mColumnWidthsLength)
      mColumnWidthsLength += kColumnWidthIncrement;
    PRInt32 * newColumnWidthsArray = new PRInt32[mColumnWidthsLength];
    nsCRT::memset (newColumnWidthsArray, 0, mColumnWidthsLength*sizeof(PRInt32));
    nsCRT::memcpy (newColumnWidthsArray, mColumnWidths, priorColumnWidthsLength*sizeof(PRInt32));
    delete [] mColumnWidths;
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

  if (PR_TRUE==gsDebug || PR_TRUE==gsDebugNT) 
    printf ("%p: maxWidth=%d from aMaxSize=%d,%d\n", 
            this, maxWidth, aMaxSize.width, aMaxSize.height);

  // based on the compatibility mode, create a table layout strategy
  if (nsnull==mTableLayoutStrategy)
  {
    if (PR_FALSE==RequiresPass1Layout())
      mTableLayoutStrategy = new FixedTableLayoutStrategy(this);
    else
      mTableLayoutStrategy = new BasicTableLayoutStrategy(this);
    mTableLayoutStrategy->Initialize(aMaxElementSize, GetColCount());
    mColumnWidthsValid=PR_TRUE;
  }
  mTableLayoutStrategy->BalanceColumnWidths(mStyleContext, aReflowState, maxWidth);
  mColumnWidthsSet=PR_TRUE;

  // if collapsing borders, compute the top and bottom edges now that we have column widths
  const nsStyleTable* tableStyle =
    (const nsStyleTable*)mStyleContext->GetStyleData(eStyleStruct_Table);
  if (NS_STYLE_BORDER_COLLAPSE==tableStyle->mBorderCollapse)
  {
    ComputeHorizontalCollapsingBorders(aPresContext, 0, mCellMap->GetRowCount()-1);
  }
}

/**
  sum the width of each column
  add in table insets
  set rect
  */
void nsTableFrame::SetTableWidth(nsIPresContext& aPresContext)
{
  NS_ASSERTION(nsnull==mPrevInFlow, "never ever call me on a continuing frame!");
  NS_ASSERTION(nsnull!=mCellMap, "never ever call me until the cell map is built!");

  nscoord cellSpacing = GetCellSpacingX();
  if (gsDebug==PR_TRUE) 
    printf ("SetTableWidth with cellSpacing = %d ", cellSpacing);
  PRInt32 tableWidth = cellSpacing;

  PRInt32 numCols = GetColCount();
  for (PRInt32 colIndex = 0; colIndex<numCols; colIndex++)
  {
    nscoord totalColWidth = mColumnWidths[colIndex];
    totalColWidth += cellSpacing;
    if (gsDebug==PR_TRUE) 
      printf (" += %d ", totalColWidth);
    tableWidth += totalColWidth;
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
  if (PR_TRUE==gsDebug  ||  PR_TRUE==gsDebugNT)
  {
    printf ("%p: setting table rect to %d, %d after adding insets %d, %d\n", 
            this, tableSize.width, tableSize.height, rightInset, leftInset);
  }
    
  // account for scroll bars. XXX needs optimization/caching
  if (mHasScrollableRowGroup) {
    float sbWidth, sbHeight;
    nsCOMPtr<nsIDeviceContext> dc;
    aPresContext.GetDeviceContext(getter_AddRefs(dc));
    dc->GetScrollBarDimensions(sbWidth, sbHeight);
    tableSize.width += NSToCoordRound(sbWidth);
  }
  SetRect(tableSize);
}

/* get the height of this table's container.  ignore all containers who have unconstrained height.
 * take into account the possibility that this table is nested.
 * if nested within an auto-height table, this method returns 0.
 * this method may also return NS_UNCONSTRAINEDSIZE, meaning no container provided any height constraint
 */
nscoord nsTableFrame::GetEffectiveContainerHeight(const nsHTMLReflowState& aReflowState)
{
  nscoord result=-1;
  const nsHTMLReflowState* rs = &aReflowState;
  while (nsnull!=rs)
  {
    const nsStyleDisplay *display;
    rs->frame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct *&)display);
    if (NS_STYLE_DISPLAY_TABLE==display->mDisplay)
    {
      const nsStylePosition *position;
      rs->frame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct *&)position);
      nsStyleUnit unit = position->mHeight.GetUnit();
      if (eStyleUnit_Null==unit || eStyleUnit_Auto==unit)
      {
        result = 0;
        break;
      }
    }
    if (NS_AUTOHEIGHT != rs->computedHeight)
    {
      result = rs->computedHeight;
      break;
    }
    // XXX: evil cast!
    rs = (nsHTMLReflowState *)(rs->parentReflowState);
  }
  NS_ASSERTION(-1!=result, "bad state:  no constrained height in reflow chain");
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
nscoord nsTableFrame::ComputeDesiredHeight(nsIPresContext& aPresContext,
                                           const nsHTMLReflowState& aReflowState, 
                                           nscoord aDefaultHeight) 
{
  NS_ASSERTION(nsnull!=mCellMap, "never ever call me until the cell map is built!");
  nsresult rv = NS_OK;
  nscoord result = aDefaultHeight;
  const nsStylePosition* tablePosition;
  GetStyleData(eStyleStruct_Position, (const nsStyleStruct *&)tablePosition);
  if (eStyleUnit_Auto == tablePosition->mHeight.GetUnit())
    return result; // auto width tables are sized by the row heights, which we already have

  const nsStyleTable* tableStyle;
  GetStyleData(eStyleStruct_Table, (const nsStyleStruct *&)tableStyle);
  nscoord tableSpecifiedHeight=-1;
  if (eStyleUnit_Coord == tablePosition->mHeight.GetUnit())
    tableSpecifiedHeight = tablePosition->mHeight.GetCoordValue();
  else if (eStyleUnit_Percent == tablePosition->mHeight.GetUnit())
  {
    float percent = tablePosition->mHeight.GetPercentValue();
    nscoord parentHeight = GetEffectiveContainerHeight(aReflowState);
    if (NS_UNCONSTRAINEDSIZE!=parentHeight && 0!=parentHeight)
      tableSpecifiedHeight = NSToCoordRound((float)parentHeight * percent);
  }
  if (-1!=tableSpecifiedHeight)
  {
    if (tableSpecifiedHeight>aDefaultHeight)
    { // proportionately distribute the excess height to each row
      result = tableSpecifiedHeight;
      nscoord excess = tableSpecifiedHeight-aDefaultHeight;
      nscoord sumOfRowHeights=0;
      nsIFrame * rowGroupFrame=mFrames.FirstChild();
      while (nsnull!=rowGroupFrame)
      {
        const nsStyleDisplay *rowGroupDisplay;
        rowGroupFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)rowGroupDisplay));
        if (PR_TRUE==IsRowGroup(rowGroupDisplay->mDisplay))
        { // the rows in rowGroupFrame need to be expanded by rowHeightDelta[i]
          // and the rowgroup itself needs to be expanded by SUM(row height deltas)
          nsIFrame * rowFrame=nsnull;
          rv = rowGroupFrame->FirstChild(nsnull, &rowFrame);
          while ((NS_SUCCEEDED(rv)) && (nsnull!=rowFrame))
          {
            const nsStyleDisplay *rowDisplay;
            rowFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)rowDisplay));
            if (NS_STYLE_DISPLAY_TABLE_ROW == rowDisplay->mDisplay)
            { // the row needs to be expanded by the proportion this row contributed to the original height
              nsRect rowRect;
              rowFrame->GetRect(rowRect);
              sumOfRowHeights += rowRect.height;
            }
            rowFrame->GetNextSibling(&rowFrame);
          }
        }
        rowGroupFrame->GetNextSibling(&rowGroupFrame);
      }
      rowGroupFrame=mFrames.FirstChild();
      nscoord y=0;
      while (nsnull!=rowGroupFrame)
      {
        const nsStyleDisplay *rowGroupDisplay;
        rowGroupFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)rowGroupDisplay));
        if (PR_TRUE==IsRowGroup(rowGroupDisplay->mDisplay))
        { // the rows in rowGroupFrame need to be expanded by rowHeightDelta[i]
          // and the rowgroup itself needs to be expanded by SUM(row height deltas)
          nscoord excessForRowGroup=0;
          nsIFrame * rowFrame=nsnull;
          rv = rowGroupFrame->FirstChild(nsnull, &rowFrame);
          while ((NS_SUCCEEDED(rv)) && (nsnull!=rowFrame))
          {
            const nsStyleDisplay *rowDisplay;
            rowFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)rowDisplay));
            if (NS_STYLE_DISPLAY_TABLE_ROW == rowDisplay->mDisplay)
            { // the row needs to be expanded by the proportion this row contributed to the original height
              nsRect rowRect;
              rowFrame->GetRect(rowRect);
              float percent = ((float)(rowRect.height)) / ((float)(sumOfRowHeights));
              nscoord excessForRow = NSToCoordRound((float)excess*percent);
              nsRect newRowRect(rowRect.x, y, rowRect.width, excessForRow+rowRect.height);
              rowFrame->SetRect(newRowRect);
              if (NS_STYLE_BORDER_COLLAPSE==tableStyle->mBorderCollapse)
              {
                nsBorderEdge *border = (nsBorderEdge *)
                  (mBorderEdges.mEdges[NS_SIDE_LEFT].ElementAt(((nsTableRowFrame*)rowFrame)->GetRowIndex()));
                border->mLength=newRowRect.height;
                border = (nsBorderEdge *)
                  (mBorderEdges.mEdges[NS_SIDE_RIGHT].ElementAt(((nsTableRowFrame*)rowFrame)->GetRowIndex()));
                border->mLength=newRowRect.height;
              }
              // resize cells, too
              ((nsTableRowFrame *)rowFrame)->DidResize(aPresContext, aReflowState);
              // better if this were part of an overloaded row::SetRect
              y += excessForRow+rowRect.height;
              excessForRowGroup += excessForRow;
            }
            rowFrame->GetNextSibling(&rowFrame);
          }
          nsRect rowGroupRect;
          rowGroupFrame->GetRect(rowGroupRect);
          nsRect newRowGroupRect(rowGroupRect.x, rowGroupRect.y, rowGroupRect.width, excessForRowGroup+rowGroupRect.height);
          rowGroupFrame->SetRect(newRowGroupRect);
        }
        rowGroupFrame->GetNextSibling(&rowGroupFrame);
      }
    }
  }
  return result;
}

void nsTableFrame::AdjustColumnsForCOLSAttribute()
{
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
}

/*
  The rule is:  use whatever width is greatest among those specified widths
  that span a column.  It doesn't matter what comes first, just what is biggest.
  Specified widths (when colspan==1) and span widths need to be stored separately, 
  because specified widths tell us what proportion of the span width to give to each column 
  (in their absence, we use the desired width of the cell.)                                                                  
*/
NS_METHOD
nsTableFrame::SetColumnStyleFromCell(nsIPresContext &  aPresContext,
                                     nsTableCellFrame* aCellFrame,
                                     nsTableRowFrame * aRowFrame)
{
  // if the cell has a colspan, the width is used provisionally, divided equally among 
  // the spanned columns until the table layout strategy computes the real column width.
  if (PR_TRUE==gsDebug) printf("TIF SetCSFromCell: cell %p in row %p\n", aCellFrame, aRowFrame); 
  if ((nsnull!=aCellFrame) && (nsnull!=aRowFrame))
  {
    // get the cell style info
    const nsStylePosition* cellPosition;
    aCellFrame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct *&)cellPosition);
    if ((eStyleUnit_Coord == cellPosition->mWidth.GetUnit()) ||
         (eStyleUnit_Percent==cellPosition->mWidth.GetUnit())) {
      // compute the width per column spanned
      PRInt32 baseColIndex;
      aCellFrame->GetColIndex(baseColIndex);
      PRInt32 colSpan = GetEffectiveColSpan(baseColIndex, aCellFrame);
      if (PR_TRUE==gsDebug)
        printf("TIF SetCSFromCell: for col %d with colspan %d\n",baseColIndex, colSpan);
      for (PRInt32 i=0; i<colSpan; i++)
      {
        // get the appropriate column frame
        nsTableColFrame *colFrame;
        GetColumnFrame(i+baseColIndex, colFrame);
        if (PR_TRUE==gsDebug)
          printf("TIF SetCSFromCell: for col %d (%p)\n",i+baseColIndex, colFrame);
        // if the colspan is 1 and we already have a cell that set this column's width
        // then ignore this width attribute
        if ((1==colSpan) && (nsTableColFrame::eWIDTH_SOURCE_CELL == colFrame->GetWidthSource()))
        {
          if (PR_TRUE==gsDebug)
            printf("TIF SetCSFromCell: width already set from a cell with colspan=1, no-op this cell's width attr\n");
          break;
        }
        // get the column style
        nsIStyleContext *colSC;
        colFrame->GetStyleContext(&colSC);
        nsStylePosition* colPosition = (nsStylePosition*) colSC->GetMutableStyleData(eStyleStruct_Position);
        // if colSpan==1, then we can just set the column width
        if (1==colSpan)
        { // set the column width attribute
          if (eStyleUnit_Coord == cellPosition->mWidth.GetUnit())
          {
            nscoord width = cellPosition->mWidth.GetCoordValue();
            colPosition->mWidth.SetCoordValue(width);
            if (PR_TRUE==gsDebug)
              printf("TIF SetCSFromCell: col fixed width set to %d from cell", width);
          }
          else
          {
            float width = cellPosition->mWidth.GetPercentValue();
            colPosition->mWidth.SetPercentValue(width);
            if (PR_TRUE==gsDebug)
              printf("TIF SetCSFromCell: col percent width set to %g from cell", width);
          }
          colFrame->SetWidthSource(nsTableColFrame::eWIDTH_SOURCE_CELL);
        }
        else  // we have a colspan > 1. so we need to set the column table style spanWidth
        { // if the cell is a coord width...
          nsStyleTable* colTableStyle = (nsStyleTable*) colSC->GetMutableStyleData(eStyleStruct_Table);
          if (eStyleUnit_Coord == cellPosition->mWidth.GetUnit())
          {
            // set the column width attribute iff this span's contribution to this column
            // is greater than any previous information
            nscoord cellWidth = cellPosition->mWidth.GetCoordValue();
            nscoord widthPerColumn = cellWidth/colSpan;
            nscoord widthForThisColumn = widthPerColumn;
            if (eStyleUnit_Coord == colTableStyle->mSpanWidth.GetUnit())
              widthForThisColumn = PR_MAX(widthForThisColumn, colTableStyle->mSpanWidth.GetCoordValue());
            colTableStyle->mSpanWidth.SetCoordValue(widthForThisColumn);
            if (PR_TRUE==gsDebug)
              printf("TIF SetCSFromCell: col span width set to %d from spanning cell", widthForThisColumn);
          }
          // else if the cell has a percent width...
          else if (eStyleUnit_Percent == cellPosition->mWidth.GetUnit())
          {
            // set the column width attribute iff this span's contribution to this column
            // is greater than any previous information
            float cellWidth = cellPosition->mWidth.GetPercentValue();
            float percentPerColumn = cellWidth/(float)colSpan;
            float percentForThisColumn = percentPerColumn;
            if (eStyleUnit_Percent == colTableStyle->mSpanWidth.GetUnit())
              percentForThisColumn = PR_MAX(percentForThisColumn, colTableStyle->mSpanWidth.GetPercentValue());
            colTableStyle->mSpanWidth.SetPercentValue(percentForThisColumn);
            if (PR_TRUE==gsDebug)
              printf("TIF SetCSFromCell: col span width set to %d from spanning cell", (nscoord)percentForThisColumn);
          }
          colFrame->SetWidthSource(nsTableColFrame::eWIDTH_SOURCE_CELL_WITH_SPAN);
        }
        NS_RELEASE(colSC);
      }
    }
  }
  return NS_OK;
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
  NS_POSTCONDITION(nsnull!=aColFrame, "no column frame could be found.");
  return NS_OK;
}

PRBool nsTableFrame::IsColumnWidthsSet()
{ 
  nsTableFrame * firstInFlow = (nsTableFrame *)GetFirstInFlow();
  NS_ASSERTION(nsnull!=firstInFlow, "illegal state -- no first in flow");
  return firstInFlow->mColumnWidthsSet; 
}

/* We have to go through our child list twice.
 * The first time, we scan until we find the first row.  
 * We set column style from the cells in the first row.
 * Then we terminate that loop and start a second pass.
 * In the second pass, we build column and cell cache info.
 */
void nsTableFrame::BuildColumnCache( nsIPresContext&          aPresContext,
                                     nsHTMLReflowMetrics&     aDesiredSize,
                                     const nsHTMLReflowState& aReflowState,
                                     nsReflowStatus&          aStatus)
{
  NS_ASSERTION(nsnull==mPrevInFlow, "never ever call me on a continuing frame!");
  NS_ASSERTION(nsnull!=mCellMap, "never ever call me until the cell map is built!");
  PRInt32 colIndex=0;
  const nsStyleTable* tableStyle;
  GetStyleData(eStyleStruct_Table, (const nsStyleStruct *&)tableStyle);
  EnsureColumns(aPresContext);
  if (nsnull!=mColCache)
  {
    if (PR_TRUE==gsDebugIR) printf("TIF BCC: clearing column cache and cell map column frame cache.\n");
    mCellMap->ClearColumnCache();
    delete mColCache;
  }

  mColCache = new ColumnInfoCache(GetColCount());
  CacheColFramesInCellMap();

  // handle rowgroups
  nsIFrame * childFrame = mFrames.FirstChild();
  while (nsnull!=childFrame)
  { // in this loop, set column style info from cells
    const nsStyleDisplay *childDisplay;
    childFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)childDisplay));
    if (PR_TRUE==IsRowGroup(childDisplay->mDisplay))
    { // if it's a row group, get the cells and set the column style if appropriate
      if (PR_TRUE==RequiresPass1Layout())
      {
        nsIFrame *rowFrame;
        childFrame->FirstChild(nsnull, &rowFrame);
        while (nsnull!=rowFrame)
        {
          const nsStyleDisplay *rowDisplay;
          rowFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)rowDisplay));
          if (NS_STYLE_DISPLAY_TABLE_ROW == rowDisplay->mDisplay)
          {
            nsIFrame *cellFrame;
            rowFrame->FirstChild(nsnull, &cellFrame);
            while (nsnull!=cellFrame)
            {
              /* this is the first time we are guaranteed to have both the cell frames
               * and the column frames, so it's a good time to 
               * set the column style from the cell's width attribute (if this is the first row)
               */
              const nsStyleDisplay *cellDisplay;
              cellFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)cellDisplay));
              if (NS_STYLE_DISPLAY_TABLE_CELL == cellDisplay->mDisplay)
                SetColumnStyleFromCell(aPresContext, (nsTableCellFrame *)cellFrame, (nsTableRowFrame *)rowFrame);
              cellFrame->GetNextSibling(&cellFrame);
            }
          }
          rowFrame->GetNextSibling(&rowFrame);
        }
      }
    }
    childFrame->GetNextSibling(&childFrame);
  }

  // second time through, set column cache info for each column
  // we can't do this until the loop above has set the column style info from the cells
  childFrame = mColGroups.FirstChild();
  while (nsnull!=childFrame)
  { // for every child, if it's a col group then get the columns
    nsTableColFrame *colFrame=nsnull;
    childFrame->FirstChild(nsnull, (nsIFrame **)&colFrame);
    while (nsnull!=colFrame)
    { // for every column, create an entry in the column cache
      // assumes that the col style has been twiddled to account for first cell width attribute
      const nsStyleDisplay *colDisplay;
      colFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)colDisplay));
      if (NS_STYLE_DISPLAY_TABLE_COLUMN == colDisplay->mDisplay)
      {
        const nsStylePosition* colPosition;
        colFrame->GetStyleData(eStyleStruct_Position, ((const nsStyleStruct *&)colPosition));
        PRInt32 repeat = colFrame->GetSpan();
        colIndex = colFrame->GetColumnIndex();
        for (PRInt32 i=0; i<repeat; i++)
        {
          mColCache->AddColumnInfo(colPosition->mWidth.GetUnit(), colIndex+i);
        }
      }
      colFrame->GetNextSibling((nsIFrame **)&colFrame);
    }
    childFrame->GetNextSibling(&childFrame);
  }
  if (PR_TRUE==gsDebugIR) printf("TIF BCC: mColumnCacheValid=PR_TRUE.\n");
  mColumnCacheValid=PR_TRUE;
}

void nsTableFrame::CacheColFramesInCellMap()
{
  nsIFrame * childFrame = mColGroups.FirstChild();
  while (nsnull!=childFrame)
  { // in this loop, we cache column info 
    nsTableColFrame *colFrame=nsnull;
    childFrame->FirstChild(nsnull, (nsIFrame **)&colFrame);
    while (nsnull!=colFrame)
    {
      PRInt32 colIndex = colFrame->GetColumnIndex();
      PRInt32 repeat   = colFrame->GetSpan();
      for (PRInt32 i=0; i<repeat; i++)
      {
        nsTableColFrame *cachedColFrame = mCellMap->GetColumnFrame(colIndex+i);
        if (nsnull==cachedColFrame)
        {
          if (gsDebug) 
            printf("TIF BCB: adding column frame %p\n", colFrame);
          mCellMap->AppendColumnFrame(colFrame);
        }
        colIndex++;
      }
      colFrame->GetNextSibling((nsIFrame **)&colFrame);
    }
    childFrame->GetNextSibling(&childFrame);
  }
}

void nsTableFrame::InvalidateColumnWidths()
{
  nsTableFrame * firstInFlow = (nsTableFrame *)GetFirstInFlow();
  NS_ASSERTION(nsnull!=firstInFlow, "illegal state -- no first in flow");
  firstInFlow->mColumnWidthsValid=PR_FALSE;
  if (PR_TRUE==gsDebugIR) printf("TIF: ColWidths invalidated.\n");
}

PRBool nsTableFrame::IsColumnWidthsValid() const
{
  nsTableFrame * firstInFlow = (nsTableFrame *)GetFirstInFlow();
  NS_ASSERTION(nsnull!=firstInFlow, "illegal state -- no first in flow");
  return firstInFlow->mColumnWidthsValid;
}

PRBool nsTableFrame::IsFirstPassValid() const
{
  nsTableFrame * firstInFlow = (nsTableFrame *)GetFirstInFlow();
  NS_ASSERTION(nsnull!=firstInFlow, "illegal state -- no first in flow");
  return firstInFlow->mFirstPassValid;
}

void nsTableFrame::InvalidateFirstPassCache()
{
  nsTableFrame * firstInFlow = (nsTableFrame *)GetFirstInFlow();
  NS_ASSERTION(nsnull!=firstInFlow, "illegal state -- no first in flow");
  firstInFlow->mFirstPassValid=PR_FALSE;
  if (PR_TRUE==gsDebugIR) printf("TIF: FirstPass invalidated.\n");
}

PRBool nsTableFrame::IsColumnCacheValid() const
{
  nsTableFrame * firstInFlow = (nsTableFrame *)GetFirstInFlow();
  NS_ASSERTION(nsnull!=firstInFlow, "illegal state -- no first in flow");
  return firstInFlow->mColumnCacheValid;
}

void nsTableFrame::InvalidateColumnCache()
{
  nsTableFrame * firstInFlow = (nsTableFrame *)GetFirstInFlow();
  NS_ASSERTION(nsnull!=firstInFlow, "illegal state -- no first in flow");
  firstInFlow->mColumnCacheValid=PR_FALSE;
  if (PR_TRUE==gsDebugIR) printf("TIF: ColCache invalidated.\n");
}

PRBool nsTableFrame::IsCellMapValid() const
{
  nsTableFrame * firstInFlow = (nsTableFrame *)GetFirstInFlow();
  NS_ASSERTION(nsnull!=firstInFlow, "illegal state -- no first in flow");
  return firstInFlow->mCellMapValid;
}

void nsTableFrame::InvalidateCellMap()
{
  nsTableFrame * firstInFlow = (nsTableFrame *)GetFirstInFlow();
  NS_ASSERTION(nsnull!=firstInFlow, "illegal state -- no first in flow");
  firstInFlow->mCellMapValid=PR_FALSE;
  // reset the state in each row
  nsIFrame *rowGroupFrame=mFrames.FirstChild();
  for ( ; nsnull!=rowGroupFrame; rowGroupFrame->GetNextSibling(&rowGroupFrame))
  {
    const nsStyleDisplay *rowGroupDisplay;
    rowGroupFrame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct *&)rowGroupDisplay);
    if (PR_TRUE==IsRowGroup(rowGroupDisplay->mDisplay))
    {
      nsIFrame *rowFrame;
      rowGroupFrame->FirstChild(nsnull, &rowFrame);
      for ( ; nsnull!=rowFrame; rowFrame->GetNextSibling(&rowFrame))
      {
        const nsStyleDisplay *rowDisplay;
        rowFrame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct *&)rowDisplay);
        if (NS_STYLE_DISPLAY_TABLE_ROW==rowDisplay->mDisplay)
        {
          ((nsTableRowFrame *)rowFrame)->ResetInitChildren();
        }
      }
    }
  }
  if (PR_TRUE==gsDebugIR) printf("TIF: CellMap invalidated.\n");
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
    NS_ASSERTION(nsnull!=mColumnWidths, "illegal state");
    // can't assert on IsColumnWidthsSet because we might want to call this
    // while we're in the process of setting column widths, and we don't
    // want to complicate IsColumnWidthsSet by making it a multiple state return value
    // (like eNotSet, eSetting, eIsSet)
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
    NS_ASSERTION(nsnull!=mColumnWidths, "illegal state, nsnull mColumnWidths");
    NS_ASSERTION(aColIndex<mColumnWidthsLength, "illegal state, aColIndex<mColumnWidthsLength");
    if (nsnull!=mColumnWidths && aColIndex<mColumnWidthsLength)
      mColumnWidths[aColIndex] = aWidth;
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

void nsTableFrame::MapBorderMarginPadding(nsIPresContext& aPresContext)
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

  float     p2t = aPresContext.GetPixelsToTwips();

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

  if (nsnull != aKidFrame)
  {
    result = aKidFrame->GetMargin(aMargin);
  }

  return result;
}

//XXX: ok, this looks dumb now.  but in a very short time this will get filled in
void nsTableFrame::GetTableBorder(nsMargin &aBorder)
{
  if (NS_STYLE_BORDER_COLLAPSE==GetBorderCollapseStyle())
  {
    aBorder = mBorderEdges.mMaxBorderWidth;
  }
  else
  {
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

void nsTableFrame::GetTableBorderAt(nsMargin &aBorder, PRInt32 aRowIndex, PRInt32 aColIndex)
{
  if (NS_STYLE_BORDER_COLLAPSE==GetBorderCollapseStyle())
  {
    nsBorderEdge *border = (nsBorderEdge *)(mBorderEdges.mEdges[NS_SIDE_LEFT].ElementAt(aRowIndex));
	if (border) {
    aBorder.left = border->mWidth;
    border = (nsBorderEdge *)(mBorderEdges.mEdges[NS_SIDE_RIGHT].ElementAt(aRowIndex));
    aBorder.right = border->mWidth;
    border = (nsBorderEdge *)(mBorderEdges.mEdges[NS_SIDE_TOP].ElementAt(aColIndex));
    aBorder.top = border->mWidth;
    border = (nsBorderEdge *)(mBorderEdges.mEdges[NS_SIDE_TOP].ElementAt(aColIndex));
    aBorder.bottom = border->mWidth;
	}
  }
  else
  {
    const nsStyleSpacing* spacing =
      (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);
    spacing->GetBorder(aBorder);
  }
}

void nsTableFrame::GetTableBorderForRowGroup(nsTableRowGroupFrame * aRowGroupFrame, nsMargin &aBorder)
{
  aBorder.SizeTo(0,0,0,0);
  if (nsnull!=aRowGroupFrame)
  {
    if (NS_STYLE_BORDER_COLLAPSE==GetBorderCollapseStyle())
    {
      PRInt32 rowIndex = aRowGroupFrame->GetStartRowIndex();
      PRInt32 rowCount;
      aRowGroupFrame->GetRowCount(rowCount);
      for ( ; rowIndex<rowCount; rowIndex++)
      {
        PRInt32 colIndex = 0;
        nsCellMap *cellMap = GetCellMap();
        PRInt32 colCount = cellMap->GetColCount();
        for ( ; colIndex<colCount; colIndex++)
        {
          nsMargin border;
          GetTableBorderAt (border, rowIndex, colIndex);
          aBorder.top = PR_MAX(aBorder.top, border.top);
          aBorder.right = PR_MAX(aBorder.right, border.right);
          aBorder.bottom = PR_MAX(aBorder.bottom, border.bottom);
          aBorder.left = PR_MAX(aBorder.left, border.left);
        }
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
  if (NS_STYLE_BORDER_COLLAPSE!=borderCollapseStyle)
  {
    if (tableStyle->mBorderSpacingX.GetUnit() == eStyleUnit_Coord) {
      cellSpacing = tableStyle->mBorderSpacingX.GetCoordValue();
    }
    else {
      cellSpacing = mDefaultCellSpacingX;
    }
  }
  return cellSpacing;
}

// XXX: could cache this.  But be sure to check style changes if you do!
nscoord nsTableFrame::GetCellSpacingY()
{
  const nsStyleTable* tableStyle;
  GetStyleData(eStyleStruct_Table, (const nsStyleStruct *&)tableStyle);
  nscoord cellSpacing = 0;
  PRUint8 borderCollapseStyle = GetBorderCollapseStyle();
  if (NS_STYLE_BORDER_COLLAPSE!=borderCollapseStyle)
  {
    if (tableStyle->mBorderSpacingY.GetUnit() == eStyleUnit_Coord) {
      cellSpacing = tableStyle->mBorderSpacingY.GetCoordValue();
    }
    else {
      cellSpacing = mDefaultCellSpacingY;
    }
  }
  return cellSpacing;
}


// XXX: could cache this.  But be sure to check style changes if you do!
nscoord nsTableFrame::GetCellPadding()
{
  const nsStyleTable* tableStyle;
  GetStyleData(eStyleStruct_Table, (const nsStyleStruct *&)tableStyle);
  nscoord cellPadding = 0;
  if (tableStyle->mCellPadding.GetUnit() == eStyleUnit_Coord) {
    cellPadding = tableStyle->mCellPadding.GetCoordValue();
  }
  else  {
    cellPadding = mDefaultCellPadding;
  }
  return cellPadding;
}


void nsTableFrame::GetColumnsByType(const nsStyleUnit aType, 
                                    PRInt32& aOutNumColumns,
                                    PRInt32 *& aOutColumnIndexes)
{
  mColCache->GetColumnsByType(aType, aOutNumColumns, aOutColumnIndexes);
}



/* ----- global methods ----- */

nsresult 
NS_NewTableFrame(nsIFrame*& aResult)
{
  nsIFrame* it = new nsTableFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  aResult = it;
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
  const nsReflowState* rs = aReflowState.parentReflowState; // this is for the outer frame
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


/* helper method for getting the width of the table's containing block */
nscoord nsTableFrame::GetTableContainerWidth(const nsHTMLReflowState& aReflowState)
{
  const nsStyleDisplay *display;
  nscoord parentWidth = aReflowState.availableWidth;

  // Walk up the reflow state chain until we find a block
  // frame. Our width is computed relative to there.
  const nsReflowState* rs = &aReflowState;
  nsTableCellFrame *lastCellFrame=nsnull;
  while (nsnull != rs) 
  {
    // if it's a block, use its max width
    rs->frame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct *&)display);
    if (NS_STYLE_DISPLAY_BLOCK==display->mDisplay) 
    { // we found a block, see if it's really a table cell (which means we're a nested table)
      PRBool skipThisBlock=PR_FALSE;
      const nsReflowState* parentRS = rs->parentReflowState;
      if (nsnull!=parentRS)
      {
        parentRS = parentRS->parentReflowState;
        if (nsnull!=parentRS)
        {
          parentRS->frame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct *&)display);
          if (NS_STYLE_DISPLAY_TABLE_CELL==display->mDisplay) {
            if (PR_TRUE==gsDebugNT)
              printf("%p: found a block pframe %p in a cell, skipping it.\n", aReflowState.frame, rs->frame);
            skipThisBlock = PR_TRUE;
          }
        }
      }
      // at this point, we know we have a block.  If we're sure it's not a table cell pframe,
      // then we can use it
      if (PR_FALSE==skipThisBlock)
      {
        if (NS_UNCONSTRAINEDSIZE!=rs->availableWidth)
        {
          parentWidth = rs->availableWidth;
          if (PR_TRUE==gsDebugNT)
            printf("%p: found a block frame %p, returning width %d\n", 
                   aReflowState.frame, rs->frame, parentWidth);
          break;
        }
      }
    }
    // or if it's another table (we're nested) use its computed width
    if (rs->frame!=aReflowState.frame)
    {
      nsMargin borderPadding;
      const nsStylePosition* tablePosition;
      const nsStyleSpacing* spacing;
      rs->frame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct *&)display);
      if (NS_STYLE_DISPLAY_TABLE_CELL==display->mDisplay) 
      { // if it's a cell and the cell has a specified width, use it
        // Compute and subtract out the insets (sum of border and padding) for the table
        lastCellFrame = (nsTableCellFrame*)(rs->frame);
        const nsStylePosition* cellPosition;
        lastCellFrame->GetStyleData(eStyleStruct_Position, ((const nsStyleStruct *&)cellPosition));
        if (eStyleUnit_Coord == cellPosition->mWidth.GetUnit())
        {
          nsTableFrame *tableParent;
          nsresult rv=GetTableFrame(lastCellFrame, tableParent);
          if ((NS_OK==rv) && (nsnull!=tableParent) && (nsnull!=tableParent->mColumnWidths))
          {
            parentWidth=0;
            PRInt32 colIndex;
            lastCellFrame->GetColIndex(colIndex);
            PRInt32 colSpan = tableParent->GetEffectiveColSpan(colIndex, lastCellFrame);
            for (PRInt32 i=0; i<colSpan; i++)
              parentWidth += tableParent->GetColumnWidth(colIndex);
            break;
          }
          // if the column width of this cell is already computed, it overrides the attribute
          // otherwise, use the attribute becauase the actual column width has not yet been computed
          else
          {
            parentWidth = cellPosition->mWidth.GetCoordValue();
            // subtract out cell border and padding
            lastCellFrame->GetStyleData(eStyleStruct_Spacing, (const nsStyleStruct *&)spacing);
            spacing->CalcBorderPaddingFor(lastCellFrame, borderPadding);  //XXX: COLLAPSE
            parentWidth -= (borderPadding.right + borderPadding.left);
            if (PR_TRUE==gsDebugNT)
              printf("%p: found a cell frame %p with fixed coord width %d, returning parentWidth %d\n", 
                     aReflowState.frame, lastCellFrame, cellPosition->mWidth.GetCoordValue(), parentWidth);
            break;
          }
        }
      }
      else
      {
        rs->frame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct *&)display);
        if (NS_STYLE_DISPLAY_TABLE==display->mDisplay) 
        {
          // we only want to do this for inner table frames, so check the frame's parent to make sure it is an outer table frame
          // we know that if both the frame and it's parent map to NS_STYLE_DISPLAY_TABLE, then we have an inner table frame 
          nsIFrame * tableFrameParent;
          rs->frame->GetParent(&tableFrameParent);
          tableFrameParent->GetStyleData(eStyleStruct_Display, (const nsStyleStruct *&)display);
          if (NS_STYLE_DISPLAY_TABLE==display->mDisplay)
          {
            /* We found the nearest containing table (actually, the inner table).  
               This defines what our percentage size is relative to. Use its desired width 
               as the basis for computing our width.
               **********************************************************************************
               Nav4 compatibility code:  if the inner table has a percent width and the outer
               table has an auto width, the parentWidth is the width the containing cell would be 
               without the inner table.
               **********************************************************************************
             */
            // Compute and subtract out the insets (sum of border and padding) for the table
            rs->frame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct *&)tablePosition);
            rs->frame->GetStyleData(eStyleStruct_Spacing, (const nsStyleStruct *&)spacing);
            if (eStyleUnit_Auto == tablePosition->mWidth.GetUnit())
            {
              parentWidth = NS_UNCONSTRAINEDSIZE;
              if (nsnull != ((nsTableFrame*)(rs->frame))->mColumnWidths)
              {
                parentWidth=0;
                PRInt32 colIndex;
                lastCellFrame->GetColIndex(colIndex);
                PRInt32 colSpan = ((nsTableFrame*)(rs->frame))->GetEffectiveColSpan(colIndex, lastCellFrame);
                for (PRInt32 i = 0; i<colSpan; i++)
                  parentWidth += ((nsTableFrame*)(rs->frame))->GetColumnWidth(i+colIndex);
                // subtract out cell border and padding
                lastCellFrame->GetStyleData(eStyleStruct_Spacing, (const nsStyleStruct *&)spacing);
                spacing->CalcBorderPaddingFor(lastCellFrame, borderPadding); //XXX: COLLAPSE
                parentWidth -= (borderPadding.right + borderPadding.left);
                if (PR_TRUE==gsDebugNT)
                  printf("%p: found a table frame %p with auto width, returning parentWidth %d from cell in col %d with span %d\n", 
                         aReflowState.frame, rs->frame, parentWidth, colIndex, colSpan);
              }
              else
              {
                if (PR_TRUE==gsDebugNT)
                  printf("%p: found a table frame %p with auto width, returning parentWidth %d because parent has no info yet.\n", 
                         aReflowState.frame, rs->frame, parentWidth);
              }
            }
            else
            {
              if (PR_TRUE==((nsTableFrame*)(rs->frame))->IsColumnWidthsSet())
              {
                parentWidth=0;
                PRInt32 colIndex;
                lastCellFrame->GetColIndex(colIndex);
                PRInt32 colSpan = ((nsTableFrame*)(rs->frame))->GetEffectiveColSpan(colIndex, lastCellFrame);
                for (PRInt32 i = 0; i<colSpan; i++)
                  parentWidth += ((nsTableFrame*)(rs->frame))->GetColumnWidth(i+colIndex);
                // factor in the cell
                lastCellFrame->GetStyleData(eStyleStruct_Spacing, (const nsStyleStruct *&)spacing);
                spacing->CalcBorderPaddingFor(lastCellFrame, borderPadding); //XXX: COLLAPSE
                parentWidth -= (borderPadding.right + borderPadding.left);
              }
              else
              {
                nsSize tableSize;
                rs->frame->GetSize(tableSize);
                parentWidth = tableSize.width;
                if (0!=tableSize.width)
                { // the table has been sized, so we can compute the available space for the child
                  ((nsTableFrame *)(rs->frame))->GetTableBorder (borderPadding);
                  nsMargin padding;
                  spacing->GetPadding(padding);
                  borderPadding += padding;
                  parentWidth -= (borderPadding.right + borderPadding.left);
                  // factor in the cell
                  lastCellFrame->GetStyleData(eStyleStruct_Spacing, (const nsStyleStruct *&)spacing);
                  spacing->CalcBorderPaddingFor(lastCellFrame, borderPadding); //XXX: COLLAPSE
                  parentWidth -= (borderPadding.right + borderPadding.left);
                }
                else
                {
                  // the table has not yet been sized, so we need to infer the available space
                  parentWidth = rs->availableWidth;
                  if (eStyleUnit_Percent == tablePosition->mWidth.GetUnit())
                  {
                    float percent = tablePosition->mWidth.GetPercentValue();
                    parentWidth = (nscoord)(percent*((float)parentWidth));
                  }
                }
              }
              if (PR_TRUE==gsDebugNT)
                printf("%p: found a table frame %p, returning parentWidth %d \n", 
                       aReflowState.frame, rs->frame, parentWidth);
            }
            break;
          }
        }
      }
    }

    rs = rs->parentReflowState;
  }
  return parentWidth;
}

// aSpecifiedTableWidth is filled if the table witdth is not auto
PRBool nsTableFrame::TableIsAutoWidth(nsTableFrame *aTableFrame,
                                      nsIStyleContext *aTableStyle, 
                                      const nsHTMLReflowState& aReflowState,
                                      nscoord& aSpecifiedTableWidth)
{
  NS_ASSERTION(nsnull!=aTableStyle, "bad arg - aTableStyle");
  PRBool result = PR_TRUE;  // the default
  if (nsnull!=aTableStyle)
  {
    nsStylePosition* tablePosition = (nsStylePosition*)aTableStyle->GetStyleData(eStyleStruct_Position);
    nsMargin borderPadding;
    const nsStyleSpacing* spacing;
    switch (tablePosition->mWidth.GetUnit()) {
    case eStyleUnit_Auto:         // specified auto width
    case eStyleUnit_Proportional: // illegal for table, so ignored
      break;

    case eStyleUnit_Inherit:
      // get width of parent and see if it is a specified value or not
      // XXX for now, just return true
      break;

    case eStyleUnit_Coord:
    {
      nscoord coordWidth = tablePosition->mWidth.GetCoordValue();
      // NAV4 compatibility.  If coord width is 0, do nothing so we get same result as "auto"
      if (0!=coordWidth)
      {
        aSpecifiedTableWidth = coordWidth;
        aReflowState.frame->GetStyleData(eStyleStruct_Spacing, (const nsStyleStruct *&)spacing);
        spacing->CalcBorderPaddingFor(aReflowState.frame, borderPadding); //XXX: COLLAPSE
        aSpecifiedTableWidth -= (borderPadding.right + borderPadding.left);
        result = PR_FALSE;
      }
    }
    break;

    case eStyleUnit_Percent:
    {
      // set aSpecifiedTableWidth to be the given percent of the parent.
      // first, get the effective parent width (parent width - insets)
      nscoord parentWidth = nsTableFrame::GetTableContainerWidth(aReflowState);
      if (NS_UNCONSTRAINEDSIZE!=parentWidth)
      {
        aReflowState.frame->GetStyleData(eStyleStruct_Spacing, (const nsStyleStruct *&)spacing);
        spacing->CalcBorderPaddingFor(aReflowState.frame, borderPadding); //XXX: COLLAPSE
        parentWidth -= (borderPadding.right + borderPadding.left);

        // then set aSpecifiedTableWidth to the given percent of the computed parent width
        float percent = tablePosition->mWidth.GetPercentValue();
        aSpecifiedTableWidth = (PRInt32)(parentWidth*percent);
        if (PR_TRUE==gsDebug || PR_TRUE==gsDebugNT) 
          printf("%p: TableIsAutoWidth setting aSpecifiedTableWidth = %d with parentWidth = %d and percent = %f\n", 
                 aTableFrame, aSpecifiedTableWidth, parentWidth, percent);
      }
      else
      {
        aSpecifiedTableWidth=parentWidth;
        if (PR_TRUE==gsDebug || PR_TRUE==gsDebugNT) 
          printf("%p: TableIsAutoWidth setting aSpecifiedTableWidth = %d with parentWidth = %d\n", 
                 aTableFrame, aSpecifiedTableWidth, parentWidth);
      }
      result = PR_FALSE;
    }
    break;

    default:
      break;
    }
  }

  return result; 
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
nscoord nsTableFrame::GetMaxTableWidth()
{
  nscoord result = 0;
  if (nsnull!=mTableLayoutStrategy)
    result = mTableLayoutStrategy->GetTableMaxWidth();
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

NS_IMETHODIMP
nsTableFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("Table", aResult);
}

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
      mHasScrollableRowGroup = PR_TRUE;
    }
  }

  return (nsTableRowGroupFrame*)result;
}

PRBool
nsTableFrame::IsFinalPass(const nsReflowState& aState) 
{
  return (NS_UNCONSTRAINEDSIZE != aState.availableWidth) ||
         (NS_UNCONSTRAINEDSIZE != aState.availableHeight);
}
