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
#include "nsTableFrame.h"
#include "nsTablePart.h"
#include "nsIRenderingContext.h"
#include "nsIStyleContext.h"
#include "nsIContent.h"
#include "nsIContentDelegate.h"
#include "nsCellMap.h"
#include "nsTableCellFrame.h"
#include "nsTableCol.h"         // needed for building implicit columns
#include "nsTableColGroup.h"    // needed for building implicit colgroups
#include "nsTableColFrame.h"
#include "nsTableColGroupFrame.h"
#include "nsTableRowFrame.h"
#include "nsTableRowGroupFrame.h"
#include "nsColLayoutData.h"

#include "BasicTableLayoutStrategy.h"

#include "nsIPresContext.h"
#include "nsCSSRendering.h"
#include "nsStyleConsts.h"
#include "nsCellLayoutData.h"
#include "nsVoidArray.h"
#include "prinrval.h"
#include "nsIPtr.h"
#include "nsIView.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIReflowCommand.h"

#ifdef NS_DEBUG
static PRBool gsDebug = PR_FALSE;
static PRBool gsDebugCLD = PR_FALSE;
static PRBool gsTiming = PR_FALSE;
static PRBool gsDebugNT = PR_FALSE;
//#define NOISY
//#define NOISY_FLOW
//#ifdef NOISY_STYLE
#else
static const PRBool gsDebug = PR_FALSE;
static const PRBool gsDebugCLD = PR_FALSE;
static const PRBool gsTiming = PR_FALSE;
static const PRBool gsDebugNT = PR_FALSE;
#endif

#ifndef max
#define max(x, y) ((x) > (y) ? (x) : (y))
#endif

NS_DEF_PTR(nsIStyleContext);
NS_DEF_PTR(nsIContent);

const nsIID kTableFrameCID = NS_TABLEFRAME_CID;

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

  // The body's style molecule

  // Our reflow state
  const nsReflowState& reflowState;

  // The body's available size (computed from the body's parent)
  nsSize availSize;

  nscoord leftInset, topInset;

  // Margin tracking information
  nscoord prevMaxPosBottomMargin;
  nscoord prevMaxNegBottomMargin;

  // Flags for whether the max size is unconstrained
  PRBool  unconstrainedWidth;
  PRBool  unconstrainedHeight;

  // Running y-offset
  nscoord y;

  // Flag for whether we're dealing with the first interior row group
  PRBool firstRowGroup;

  // a list of the footers in this table frame, for quick access when inserting bodies
  nsVoidArray *footerList;

  // cache the total height of the footers for placing body rows
  nscoord footerHeight;

  InnerTableReflowState(nsIPresContext*      aPresContext,
                        const nsReflowState& aReflowState,
                        const nsMargin&      aBorderPadding)
    : reflowState(aReflowState)
  {
    prevMaxPosBottomMargin = 0;
    prevMaxNegBottomMargin = 0;
    y=0;  // border/padding/margin???

    unconstrainedWidth = PRBool(aReflowState.maxSize.width == NS_UNCONSTRAINEDSIZE);
    availSize.width = aReflowState.maxSize.width;
    if (!unconstrainedWidth) {
      availSize.width -= aBorderPadding.left + aBorderPadding.right;
    }
    leftInset = aBorderPadding.left;

    unconstrainedHeight = PRBool(aReflowState.maxSize.height == NS_UNCONSTRAINEDSIZE);
    availSize.height = aReflowState.maxSize.height;
    if (!unconstrainedHeight) {
      availSize.height -= aBorderPadding.top + aBorderPadding.bottom;
    }
    topInset = aBorderPadding.top;

    firstRowGroup = PR_TRUE;
    footerHeight = 0;
    footerList = nsnull;
  }

  ~InnerTableReflowState() {
    if (nsnull!=footerList)
      delete footerList;
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
    eColWidthType_Proportional = 3,      // (int) value has proportional meaning
  };

private:
  PRInt32  mColCounts [4];
  PRInt32 *mColIndexes[4];
  PRInt32  mNumColumns;
};

ColumnInfoCache::ColumnInfoCache(PRInt32 aNumColumns)
{
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
  }
}


void ColumnInfoCache::GetColumnsByType(const nsStyleUnit aType, 
                                        PRInt32& aOutNumColumns,
                                        PRInt32 *& aOutColumnIndexes)
{
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
  }
}



/* --------------------- nsTableFrame -------------------- */


nsTableFrame::nsTableFrame(nsIContent* aContent, nsIFrame* aParentFrame)
  : nsContainerFrame(aContent, aParentFrame),
    mCellMap(nsnull),
    mColumnLayoutData(nsnull),
    mColCache(nsnull),
    mColumnWidths(nsnull),
    mTableLayoutStrategy(nsnull),
    mFirstPassValid(PR_FALSE),
    mPass(kPASS_UNDEFINED),
    mIsInvariantWidth(PR_FALSE)
{
}

/**
 * Method to delete all owned objects assoicated
 * with the ColumnLayoutObject instance variable
 */
void nsTableFrame::DeleteColumnLayoutData()
{
  if (nsnull!=mColumnLayoutData)
  {
    PRInt32 numCols = mColumnLayoutData->Count();
    for (PRInt32 i = 0; i<numCols; i++)
    {
      nsColLayoutData *colData = (nsColLayoutData *)(mColumnLayoutData->ElementAt(i));
      delete colData;
    }
    delete mColumnLayoutData;
    mColumnLayoutData = nsnull;
  }
}

nsTableFrame::~nsTableFrame()
{
  if (nsnull!=mCellMap)
    delete mCellMap;

  DeleteColumnLayoutData();

  if (nsnull!=mColumnWidths)
    delete [] mColumnWidths;

  if (nsnull!=mTableLayoutStrategy)
    delete mTableLayoutStrategy;

  if (nsnull!=mColCache)
    delete mColCache;
}

/* ****** CellMap methods ******* */

/* return the index of the first row group after aStartIndex */
PRInt32 nsTableFrame::NextRowGroup (PRInt32 aStartIndex)
{
  int index = aStartIndex;
  int count;
  ChildCount(count);
  nsIFrame * child;
  ChildAt (index+1, child);

  while (++index < count)
  {
    const nsStyleDisplay *childDisplay;
    child->GetStyleData(eStyleStruct_Display, ((nsStyleStruct *&)childDisplay));
    if (NS_STYLE_DISPLAY_TABLE_ROW_GROUP == childDisplay->mDisplay ||
        NS_STYLE_DISPLAY_TABLE_HEADER_GROUP == childDisplay->mDisplay ||
        NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP == childDisplay->mDisplay )
      return index;
    child->GetNextSibling(child);
  }
  return count;
}

/* counts columns in column groups */
PRInt32 nsTableFrame::GetSpecifiedColumnCount ()
{
  mColCount=0;
  nsIFrame * colGroup;
  ChildAt (0, (nsIFrame *&)colGroup);
  while (nsnull!=colGroup)
  {
    const nsStyleDisplay *childDisplay;
    colGroup->GetStyleData(eStyleStruct_Display, ((nsStyleStruct *&)childDisplay));
    if (NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP == childDisplay->mDisplay)
    {
      mColCount += ((nsTableColGroupFrame *)colGroup)->GetColumnCount();
    }
    else if (NS_STYLE_DISPLAY_TABLE_ROW_GROUP == childDisplay->mDisplay ||
             NS_STYLE_DISPLAY_TABLE_HEADER_GROUP == childDisplay->mDisplay ||
             NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP == childDisplay->mDisplay )
    {
      break;
    }
    colGroup->GetNextSibling(colGroup);
  }    
  return mColCount;
}

PRInt32 nsTableFrame::GetRowCount ()
{
  PRInt32 rowCount = 0;

  if (nsnull != mCellMap)
    return mCellMap->GetRowCount();

  nsIFrame *child=nsnull;
  ChildAt(0, child);
  while (nsnull!=child)
  {
    const nsStyleDisplay *childDisplay;
    child->GetStyleData(eStyleStruct_Display, ((nsStyleStruct *&)childDisplay));
    if (NS_STYLE_DISPLAY_TABLE_ROW_GROUP == childDisplay->mDisplay ||
        NS_STYLE_DISPLAY_TABLE_HEADER_GROUP == childDisplay->mDisplay ||
        NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP == childDisplay->mDisplay )
      rowCount += ((nsTableRowGroupFrame *)child)->GetRowCount ();
    child->GetNextSibling(child);
  }
  return rowCount;
}
// return the rows spanned by aCell starting at aRowIndex
// note that this is different from just the rowspan of aCell
// (that would be GetEffectiveRowSpan (indexOfRowThatContains_aCell, aCell)
PRInt32 nsTableFrame::GetEffectiveRowSpan (PRInt32 aRowIndex, nsTableCellFrame *aCell)
{
  NS_PRECONDITION (nsnull!=aCell, "bad cell arg");
  NS_PRECONDITION (0<=aRowIndex && aRowIndex<GetRowCount(), "bad row index arg");

  int rowSpan = aCell->GetRowSpan();
  int rowCount = GetRowCount();
  if (rowCount < (aRowIndex + rowSpan))
    return (rowCount - aRowIndex);
  return rowSpan;
}


// returns the actual cell map, not a copy, so don't mess with it!
nsCellMap* nsTableFrame::GetCellMap() const
{
  return mCellMap;
}

/* call when the cell structure has changed.  mCellMap will be rebuilt on demand. */
void nsTableFrame::ResetCellMap ()
{
  if (nsnull!=mCellMap)
    delete mCellMap;
  mCellMap = nsnull; // for now, will rebuild when needed
}

/* call when column structure has changed. */
void nsTableFrame::ResetColumns ()
{
  EnsureCellMap();
}

/** sum the columns represented by all nsTableColGroup objects
  * if the cell map says there are more columns than this, 
  * add extra implicit columns to the content tree.
  */
void nsTableFrame::EnsureColumns(nsIPresContext*      aPresContext,
                                 nsReflowMetrics&     aDesiredSize,
                                 const nsReflowState& aReflowState,
                                 nsReflowStatus&      aStatus)
{
  EnsureCellMap();

  PRInt32 actualColumns = 0;
  nsTableColGroupFrame *lastColGroupFrame = nsnull;
  nsIFrame * childFrame=nsnull;
  nsIFrame * firstRowGroupFrame=nsnull;
  nsIFrame * prevSibFrame=nsnull;
  ChildAt (0, (nsIFrame *&)childFrame);
  while (nsnull!=childFrame)
  {
    const nsStyleDisplay *childDisplay;
    childFrame->GetStyleData(eStyleStruct_Display, ((nsStyleStruct *&)childDisplay));
    if (NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP == childDisplay->mDisplay)
    {
      PRInt32 numCols = ((nsTableColGroupFrame*)childFrame)->GetColumnCount();
      actualColumns += numCols;
      lastColGroupFrame = (nsTableColGroupFrame *)childFrame;
    }
    else if (NS_STYLE_DISPLAY_TABLE_ROW_GROUP == childDisplay->mDisplay ||
             NS_STYLE_DISPLAY_TABLE_HEADER_GROUP == childDisplay->mDisplay ||
             NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP == childDisplay->mDisplay )
    {
      firstRowGroupFrame = childFrame;
      break;
    }
    prevSibFrame = childFrame;
    childFrame->GetNextSibling(childFrame);
  }    
  if (actualColumns < mCellMap->GetColCount())
  {
    nsTableColGroup *lastColGroup=nsnull;
    if (nsnull==lastColGroupFrame)
    {
      //QQQ
      // need to find the generic way to stamp out this content, and ::AppendChild it
      // this might be ok.  no matter what my mcontent is, I know it needs a colgroup as a kid?

      lastColGroup = new nsTableColGroup (PR_TRUE);
      // XXX: how do I know whether AppendChild should notify or not?
      mContent->AppendChild(lastColGroup, PR_FALSE);  // was AppendColGroup
      NS_ADDREF(lastColGroup);                        // ADDREF a: lastColGroup++
      // Resolve style for the child
      nsIStyleContext* colGroupStyleContext =
        aPresContext->ResolveStyleContextFor(lastColGroup, this, PR_TRUE);      // kidStyleContext: REFCNT++
      nsIContentDelegate* kidDel = nsnull;
      kidDel = lastColGroup->GetDelegate(aPresContext);                         // kidDel: REFCNT++
      nsresult rv = kidDel->CreateFrame(aPresContext, lastColGroup, this,
                                        colGroupStyleContext, (nsIFrame *&)lastColGroupFrame);
      NS_RELEASE(kidDel);                                                       // kidDel: REFCNT--
      NS_RELEASE(colGroupStyleContext);                                         // kidStyleContenxt: REFCNT--

      // hook lastColGroupFrame into child list
      if (nsnull==firstRowGroupFrame)
      { // make lastColGroupFrame the last frame
        nsIFrame *lastChild=nsnull;
        LastChild(lastChild);
        lastChild->SetNextSibling(lastColGroupFrame);
      }
      else
      { // insert lastColGroupFrame before the first row group frame
        if (nsnull!=prevSibFrame)
        { // lastColGroupFrame is inserted between prevSibFrame and lastColGroupFrame
          prevSibFrame->SetNextSibling(lastColGroupFrame);
        }
        else
        { // lastColGroupFrame is inserted as the first child of this table
          mFirstChild = lastColGroupFrame;
        }
        lastColGroupFrame->SetNextSibling(firstRowGroupFrame);
      }
      mChildCount++;
    }
    else
    {
      lastColGroupFrame->GetContent((nsIContent *&)lastColGroup);  // ADDREF b: lastColGroup++
    }

    PRInt32 excessColumns = mCellMap->GetColCount() - actualColumns;
    for ( ; excessColumns > 0; excessColumns--)
    {//QQQ
      // need to find the generic way to stamp out this content, and ::AppendChild it
      nsTableCol *col = new nsTableCol(PR_TRUE);
      lastColGroup->AppendChild (col, PR_FALSE);
    }
    NS_RELEASE(lastColGroup);                       // ADDREF: lastColGroup--
    lastColGroupFrame->Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);
  }
}

void nsTableFrame::EnsureCellMap()
{
  if (mCellMap == nsnull)
    BuildCellMap();
}

void nsTableFrame::BuildCellMap ()
{
  if (gsDebug==PR_TRUE) printf("Build Cell Map...\n");

  int rowCount = GetRowCount ();
  NS_ASSERTION(0!=rowCount, "bad state");
  if (0 == rowCount)
  {
    // until we have some rows, there's nothing useful to do
    return;
  }

  // Make an educated guess as to how many columns we have. It's
  // only a guess because we can't know exactly until we have
  // processed the last row.
  nsTableRowGroupFrame *rowGroup=nsnull;
  PRInt32 childCount;
  ChildCount (childCount);
  PRInt32 groupIndex = NextRowGroup (-1);
  if (0 == mColCount)
    mColCount = GetSpecifiedColumnCount ();
  if (0 == mColCount) // no column parts
  {
    ChildAt (groupIndex, (nsIFrame *&)rowGroup);
    nsTableRowFrame *row;
    rowGroup->ChildAt (0, (nsIFrame *&)row);
    mColCount = row->GetMaxColumns ();
    if (gsDebug==PR_TRUE) 
      printf("mColCount=0 at start.  Guessing col count to be %d from a row.\n", mColCount);
  }
  if (nsnull==mCellMap)
    mCellMap = new nsCellMap(rowCount, mColCount);
  else
    mCellMap->Reset(rowCount, mColCount);
  if (gsDebug==PR_TRUE) printf("mCellMap set to (%d, %d)\n", rowCount, mColCount);
  PRInt32 rowStart = 0;
  if (gsDebug==PR_TRUE) printf("childCount is %d\n", childCount);
  while (groupIndex < childCount)
  {
    if (gsDebug==PR_TRUE) printf("  groupIndex is %d\n", groupIndex);
    if (gsDebug==PR_TRUE) printf("  rowStart is %d\n", rowStart);
    ChildAt (groupIndex, (nsIFrame *&)rowGroup);
    int groupRowCount;
    rowGroup->ChildCount(groupRowCount);
    if (gsDebug==PR_TRUE) printf("  groupRowCount is %d\n", groupRowCount);
    nsTableRowFrame *row;
    rowGroup->ChildAt (0, (nsIFrame *&)row);
    for (PRInt32 rowIndex = 0; rowIndex < groupRowCount; rowIndex++)
    {
      PRInt32 cellCount;
      row->ChildCount (cellCount);
      PRInt32 cellIndex = 0;
      PRInt32 colIndex = 0;
      if (gsDebug==PR_TRUE) 
        DumpCellMap();
      if (gsDebug==PR_TRUE) printf("    rowIndex is %d, row->SetRowIndex(%d)\n", rowIndex, rowIndex + rowStart);
      row->SetRowIndex (rowIndex + rowStart);
      while (colIndex < mColCount)
      {
        if (gsDebug==PR_TRUE) printf("      colIndex = %d, with mColCount = %d\n", colIndex, mColCount);
        CellData *data =mCellMap->GetCellAt(rowIndex + rowStart, colIndex);
        if (nsnull == data)
        {
          if (gsDebug==PR_TRUE) printf("      null data from GetCellAt(%d,%d)\n", rowIndex+rowStart, colIndex);
          if (gsDebug==PR_TRUE) printf("      cellIndex=%d, cellCount=%d)\n", cellIndex, cellCount);
          if (cellIndex < cellCount)
          {
            nsTableCellFrame* cell;
            row->ChildAt (cellIndex, (nsIFrame *&)cell);
            if (gsDebug==PR_TRUE) printf("      calling BuildCellIntoMap(cell, %d, %d), and incrementing cellIndex\n", rowIndex + rowStart, colIndex);
            BuildCellIntoMap (cell, rowIndex + rowStart, colIndex);
            cellIndex++;
          }
        }
        colIndex++;
      }

      if (cellIndex < cellCount)  
      {
        // We didn't use all the cells in this row up. Grow the cell
        // data because we now know that we have more columns than we
        // originally thought we had.
        if (gsDebug==PR_TRUE) printf("   calling GrowCellMap because cellIndex < %d\n", cellIndex, cellCount);
        GrowCellMap (cellCount);
        while (cellIndex < cellCount)
        {
          if (gsDebug==PR_TRUE) printf("     calling GrowCellMap again because cellIndex < %d\n", cellIndex, cellCount);
          GrowCellMap (colIndex + 1); // ensure enough cols in map, may be low due to colspans
          CellData *data =mCellMap->GetCellAt(rowIndex + rowStart, colIndex);
          if (data == nsnull)
          {
            nsTableCellFrame* cell;
            row->ChildAt (cellIndex, (nsIFrame *&)cell);
            BuildCellIntoMap (cell, rowIndex + rowStart, colIndex);
            cellIndex++;
          }
          colIndex++;
        }
      }
      row->GetNextSibling((nsIFrame *&)row);
    }
    rowStart += groupRowCount;
    groupIndex = NextRowGroup (groupIndex);
  }
  if (gsDebug==PR_TRUE)
    DumpCellMap ();
}

/**
  */
void nsTableFrame::DumpCellMap () const
{
  printf("dumping CellMap:\n");
  if (nsnull != mCellMap)
  {
    PRInt32 rowCount = mCellMap->GetRowCount();
    PRInt32 cols = mCellMap->GetColCount();
    for (PRInt32 r = 0; r < rowCount; r++)
    {
      if (gsDebug==PR_TRUE)
      { printf("row %d", r);
        printf(": ");
      }
      for (PRInt32 c = 0; c < cols; c++)
      {
        CellData *cd =mCellMap->GetCellAt(r, c);
        if (cd != nsnull)
        {
          if (cd->mCell != nsnull)
          {
            printf("C%d,%d ", r, c);
            printf("     ");
          }
          else
          {
            nsTableCellFrame *cell = cd->mRealCell->mCell;
            nsTableRowFrame *row;
            cell->GetGeometricParent((nsIFrame *&)row);
            int rr = row->GetRowIndex ();
            int cc = cell->GetColIndex ();
            printf("S%d,%d ", rr, cc);
            if (cd->mOverlap != nsnull)
            {
              cell = cd->mOverlap->mCell;
              nsTableRowFrame* row2;
              cell->GetGeometricParent((nsIFrame *&)row);
              rr = row2->GetRowIndex ();
              cc = cell->GetColIndex ();
              printf("O%d,%c ", rr, cc);
            }
            else
              printf("     ");
          }
        }
        else
          printf("----      ");
      }
      printf("\n");
    }
  }
  else
    printf ("[nsnull]");
}

void nsTableFrame::BuildCellIntoMap (nsTableCellFrame *aCell, PRInt32 aRowIndex, PRInt32 aColIndex)
{
  NS_PRECONDITION (nsnull!=aCell, "bad cell arg");
  NS_PRECONDITION (aColIndex < mColCount, "bad column index arg");
  NS_PRECONDITION (aRowIndex < GetRowCount(), "bad row index arg");

  // Setup CellMap for this cell
  int rowSpan = GetEffectiveRowSpan (aRowIndex, aCell);
  int colSpan = aCell->GetColSpan ();
  if (gsDebug==PR_TRUE) printf("        BuildCellIntoMap. rowSpan = %d, colSpan = %d\n", rowSpan, colSpan);

  // Grow the mCellMap array if we will end up addressing
  // some new columns.
  if (mColCount < (aColIndex + colSpan))
  {
    if (gsDebug==PR_TRUE) printf("        mColCount=%d<aColIndex+colSpan so calling GrowCellMap(%d)\n", mColCount, aColIndex+colSpan);
    GrowCellMap (aColIndex + colSpan);
  }

  // Setup CellMap for this cell in the table
  CellData *data = new CellData ();
  data->mCell = aCell;
  data->mRealCell = data;
  if (gsDebug==PR_TRUE) printf("        calling mCellMap->SetCellAt(data, %d, %d)\n", aRowIndex, aColIndex);
  mCellMap->SetCellAt(data, aRowIndex, aColIndex);
  aCell->SetColIndex (aColIndex);

  // Create CellData objects for the rows that this cell spans. Set
  // their mCell to nsnull and their mRealCell to point to data. If
  // there were no column overlaps then we could use the same
  // CellData object for each row that we span...
  if ((1 < rowSpan) || (1 < colSpan))
  {
    if (gsDebug==PR_TRUE) printf("        spans\n");
    for (int rowIndex = 0; rowIndex < rowSpan; rowIndex++)
    {
      if (gsDebug==PR_TRUE) printf("          rowIndex = %d\n", rowIndex);
      int workRow = aRowIndex + rowIndex;
      if (gsDebug==PR_TRUE) printf("          workRow = %d\n", workRow);
      for (int colIndex = 0; colIndex < colSpan; colIndex++)
      {
        if (gsDebug==PR_TRUE) printf("            colIndex = %d\n", colIndex);
        int workCol = aColIndex + colIndex;
        if (gsDebug==PR_TRUE) printf("            workCol = %d\n", workCol);
        CellData *testData = mCellMap->GetCellAt(workRow, workCol);
        if (nsnull == testData)
        {
          CellData *spanData = new CellData ();
          spanData->mRealCell = data;
          if (gsDebug==PR_TRUE) printf("            null GetCellAt(%d, %d) so setting to spanData\n", workRow, workCol);
          mCellMap->SetCellAt(spanData, workRow, workCol);
        }
        else if ((0 < rowIndex) || (0 < colIndex))
        { // we overlap, replace existing data, it might be shared
          if (gsDebug==PR_TRUE) printf("            overlapping Cell from GetCellAt(%d, %d) so setting to spanData\n", workRow, workCol);
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
    if (mColCount < aColCount)
    {
      mCellMap->GrowTo(aColCount);
    }
    mColCount = aColCount;
  }
}


/* ***** Column Layout Data methods ***** */

/*
 * Lists the column layout data which turns
 * around and lists the cell layout data.
 * This is for debugging purposes only.
 */
void nsTableFrame::ListColumnLayoutData(FILE* out, PRInt32 aIndent) const
{
  // if this is a continuing frame, there will be no output
  if (nsnull!=mColumnLayoutData)
  {
    fprintf(out,"Column Layout Data \n");
    
    PRInt32 numCols = mColumnLayoutData->Count();
    for (PRInt32 i = 0; i<numCols; i++)
    {
      nsColLayoutData *colData = (nsColLayoutData *)(mColumnLayoutData->ElementAt(i));

      for (PRInt32 indent = aIndent; --indent >= 0; ) fputs("  ", out);
      fprintf(out,"Column Data [%d] \n",i);
      colData->List(out,aIndent+2);
    }
  }
}

/**
 * For the TableCell in CellData, find the CellLayoutData assocated
 * and add it to the list
**/
void nsTableFrame::AppendLayoutData(nsVoidArray* aList, nsTableCellFrame* aTableCell)
{

  if (aTableCell != nsnull)
  {
    nsCellLayoutData* layoutData = GetCellLayoutData(aTableCell);
    if (layoutData != nsnull)
      aList->AppendElement((void*)layoutData);
  }
}

void nsTableFrame::RecalcLayoutData()
{
  PRInt32 colCount = mCellMap->GetColCount();
  PRInt32 rowCount = mCellMap->GetRowCount();
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
        CellData*     cellData = mCellMap->GetCellAt(row,col);
        
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
          cellData = mCellMap->GetCellAt(row-1,col);
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
          cellData = mCellMap->GetCellAt(row,col-1);
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
              cellData = mCellMap->GetCellAt(r1,c);
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
            cellData = mCellMap->GetCellAt(r2,c);
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
              cellData = mCellMap->GetCellAt(r,c1);
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
            cellData = mCellMap->GetCellAt(r,c2);
            if ((cellData != nsnull) && (cellData->mCell != right))
            {
              right = cellData->mCell;
              if (right != nsnull)
                AppendLayoutData(boundaryCells[NS_SIDE_RIGHT],right);
            }
          }
          r++;
        }
        
        nsCellLayoutData* cellLayoutData = GetCellLayoutData(cell); 
        if (cellLayoutData != nsnull)
          cellLayoutData->RecalcLayoutData(this,boundaryCells);
      }
    }
  }
  for (edge = 0; edge < 4; edge++)
    delete boundaryCells[edge];
}




/* SEC: TODO: adjust the rect for captions */
NS_METHOD nsTableFrame::Paint(nsIPresContext& aPresContext,
                              nsIRenderingContext& aRenderingContext,
                              const nsRect& aDirtyRect)
{
  // table paint code is concerned primarily with borders and bg color
  const nsStyleDisplay* disp =
    (const nsStyleDisplay*)mStyleContext->GetStyleData(eStyleStruct_Display);

  if (disp->mVisible) {
    const nsStyleColor* myColor =
      (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);
    const nsStyleSpacing* mySpacing =
      (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);
    NS_ASSERTION(nsnull != myColor, "null style color");
    NS_ASSERTION(nsnull != mySpacing, "null style spacing");
    if (nsnull!=mySpacing)
    {
      nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                      aDirtyRect, mRect, *myColor);
      nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                  aDirtyRect, mRect, *mySpacing, 0);
    }
  }

  // for debug...
  if (nsIFrame::GetShowFrameBorders()) {
    aRenderingContext.SetColor(NS_RGB(0,128,0));
    aRenderingContext.DrawRect(0, 0, mRect.width, mRect.height);
  }

  PaintChildren(aPresContext, aRenderingContext, aDirtyRect);
  return NS_OK;
}

PRBool nsTableFrame::NeedsReflow(const nsSize& aMaxSize)
{
  PRBool result = PR_TRUE;
  if (PR_TRUE==mIsInvariantWidth)
    result = PR_FALSE;
  // TODO: other cases...
  return result;
}

// SEC: TODO need to worry about continuing frames prev/next in flow for splitting across pages.
// SEC: TODO need to keep "first pass done" state, update it when ContentChanged notifications come in

/* overview:
  if mFirstPassValid is false, this is our first time through since content was last changed
    set pass to 1
    do pass 1
      get min/max info for all cells in an infinite space
      do column balancing
    set mFirstPassValid to true
    do pass 2
  if pass is 1,
    set pass to 2
    use column widths to ResizeReflow cells
    shrinkWrap Cells in each row to tallest, realigning contents within the cell
*/

/* Layout the entire inner table. */
NS_METHOD nsTableFrame::Reflow(nsIPresContext* aPresContext,
                               nsReflowMetrics& aDesiredSize,
                               const nsReflowState& aReflowState,
                               nsReflowStatus& aStatus)
{
  NS_PRECONDITION(nsnull != aPresContext, "null arg");
  if (gsDebug==PR_TRUE) 
  {
    printf("-----------------------------------------------------------------\n");
    printf("nsTableFrame::Reflow: table %p given maxSize=%d,%d\n",
            this, aReflowState.maxSize.width, aReflowState.maxSize.height);
  }

#ifdef NS_DEBUG
  PreReflowCheck();
#endif

  aStatus = NS_FRAME_COMPLETE;

  PRIntervalTime startTime;
  if (gsTiming) {
    startTime = PR_IntervalNow();
  }

  if (eReflowReason_Incremental == aReflowState.reason) {
    // XXX Deal with the case where the reflow command is targeted at us
    nsIFrame* kidFrame;
    aReflowState.reflowCommand->GetNext(kidFrame);

    // Pass along the reflow command
    nsReflowMetrics desiredSize(nsnull);
    // XXX Correctly compute the available space...
    nsReflowState kidReflowState(kidFrame, aReflowState, aReflowState.maxSize);
    kidFrame->WillReflow(*aPresContext);
    aStatus = ReflowChild(kidFrame, aPresContext, desiredSize, kidReflowState);

    // XXX For the time being just fall through and treat it like a
    // pass 2 reflow...
    mPass = kPASS_SECOND;
  }

  if (PR_TRUE==NeedsReflow(aReflowState.maxSize))
  {
    if (PR_FALSE==IsFirstPassValid())
    { // we treat the table as if we've never seen the layout data before
      mPass = kPASS_FIRST;
      aStatus = ResizeReflowPass1(aPresContext, aDesiredSize, aReflowState, aStatus);
      // check result
    }
    mPass = kPASS_SECOND;

    // assign column widths, and assign aMaxElementSize->width
    BalanceColumnWidths(aPresContext, aReflowState, aReflowState.maxSize,
                        aDesiredSize.maxElementSize);

    // assign table width
    SetTableWidth(aPresContext);

    // Constrain our reflow width to the computed table width
    nsReflowState    reflowState(aReflowState);
    reflowState.maxSize.width = mRect.width;
    aStatus = ResizeReflowPass2(aPresContext, aDesiredSize, reflowState, 0, 0);

    if (gsTiming) {
      PRIntervalTime endTime = PR_IntervalNow();
      printf("Table reflow took %ld ticks for frame %d\n",
             endTime-startTime, this);/* XXX need to use LL_* macros! */
    }

    mPass = kPASS_UNDEFINED;
  }
  else
  {
    // set aDesiredSize and aMaxElementSize
  }

#ifdef NS_DEBUG
  PostReflowCheck(aStatus);
#endif  

  if (PR_TRUE==gsDebug) printf("end reflow for table %p\n", this);
  return NS_OK;
}

/** the first of 2 reflow passes
  * lay out the captions and row groups in an infinite space (NS_UNCONSTRAINEDSIZE)
  * cache the results for each caption and cell.
  * if successful, set mFirstPassValid=PR_TRUE, so we know we can skip this step 
  * next time.  mFirstPassValid is set to PR_FALSE when content is changed.
  * NOTE: should never get called on a continuing frame!  All cached pass1 state
  *       is stored in the inner table first-in-flow.
  */
nsReflowStatus nsTableFrame::ResizeReflowPass1(nsIPresContext* aPresContext,
                                               nsReflowMetrics& aDesiredSize,
                                               const nsReflowState& aReflowState,
                                               nsReflowStatus& aStatus)
{
  NS_PRECONDITION(aReflowState.frame == this, "bad reflow state");
  NS_PRECONDITION(aReflowState.parentReflowState->frame == mGeometricParent,
                  "bad parent reflow state");
  NS_ASSERTION(nsnull!=aPresContext, "bad pres context param");
  NS_ASSERTION(nsnull==mPrevInFlow, "illegal call, cannot call pass 1 on a continuing frame.");
  NS_ASSERTION(nsnull != mContent, "null content");

  if (PR_TRUE==gsDebugNT) printf("%p nsTableFrame::ResizeReflow Pass1: maxSize=%d,%d\n",
                               this, aReflowState.maxSize.width, aReflowState.maxSize.height);
  nsReflowStatus result = NS_FRAME_COMPLETE;

  mChildCount = 0;
  mFirstContentOffset = mLastContentOffset = 0;

  nsSize availSize(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE); // availSize is the space available at any given time in the process
  nsSize maxSize(0, 0);       // maxSize is the size of the largest child so far in the process
  nsSize kidMaxSize(0,0);
  nsReflowMetrics kidSize(&kidMaxSize);
  nscoord y = 0;
  nscoord maxAscent = 0;
  nscoord maxDescent = 0;
  PRInt32 kidIndex = 0;
  PRInt32 lastIndex = mContent->ChildCount();
  PRInt32 contentOffset=0;
  nsIFrame* prevKidFrame = nsnull;/* XXX incremental reflow! */

  // Compute the insets (sum of border and padding)
  const nsStyleSpacing* spacing =
    (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);
  nsMargin borderPadding;
  spacing->CalcBorderPaddingFor(this, borderPadding);
  nscoord topInset = borderPadding.top;
  nscoord rightInset = borderPadding.right;
  nscoord bottomInset = borderPadding.bottom;
  nscoord leftInset = borderPadding.left;
  nsReflowReason  reflowReason = aReflowState.reason;

  /* assumes that Table's children are in the following order:
   *  Captions
   *  ColGroups, in order
   *  THead, in order
   *  TFoot, in order
   *  TBody, in order
   */
  for (;;) {
    nsIContentPtr kid = mContent->ChildAt(kidIndex);   // kid: REFCNT++
    if (kid.IsNull()) {
      result = NS_FRAME_COMPLETE;
      break;
    }

    mLastContentIsComplete = PR_TRUE;

    // Resolve style
    nsIStyleContextPtr kidStyleContext =
      aPresContext->ResolveStyleContextFor(kid, this, PR_TRUE);
    NS_ASSERTION(kidStyleContext.IsNotNull(), "null style context for kid");
    const nsStyleDisplay *childDisplay = (nsStyleDisplay*)kidStyleContext->GetStyleData(eStyleStruct_Display);
    if (NS_STYLE_DISPLAY_TABLE_CAPTION != childDisplay->mDisplay)
    {
      // get next frame, creating one if needed
      nsIFrame* kidFrame=nsnull;
      if (nsnull!=prevKidFrame)
        prevKidFrame->GetNextSibling(kidFrame);  // no need to check for an error, just see if it returned null...
      else
        ChildAt(0, kidFrame);

      // if this is the first time, allocate the frame
      if (nsnull==kidFrame)
      {
        nsIContentDelegate* kidDel;
        kidDel = kid->GetDelegate(aPresContext);
        nsresult rv = kidDel->CreateFrame(aPresContext, kid,
                                          this, kidStyleContext, kidFrame);
        reflowReason = eReflowReason_Initial;
        NS_RELEASE(kidDel);

        // Link child frame into the list of children
        if (nsnull != prevKidFrame) {
          prevKidFrame->SetNextSibling(kidFrame);
        } else {
          // Our first child
          mFirstChild = kidFrame;
          SetFirstContentOffset(kidFrame);
          if (gsDebug) printf("INNER: set first content offset to %d\n", GetFirstContentOffset()); //@@@
        }
        mChildCount++;
      }

      nsSize maxKidElementSize(0,0);
      nsReflowState kidReflowState(kidFrame, aReflowState, availSize,
                                   reflowReason);
      PRInt32 yCoord = y;
      if (NS_UNCONSTRAINEDSIZE!=yCoord)
        yCoord+= topInset;
      kidFrame->WillReflow(*aPresContext);
      kidFrame->MoveTo(leftInset, yCoord);
      result = ReflowChild(kidFrame, aPresContext, kidSize, kidReflowState);

      // Place the child since some of it's content fit in us.
      if (PR_TRUE==gsDebugNT) {
        printf ("%p: reflow of row group returned desired=%d,%d, max-element=%d,%d\n",
                this, kidSize.width, kidSize.height, kidMaxSize.width, kidMaxSize.height);
      }
      kidFrame->SetRect(nsRect(leftInset, yCoord,
                               kidSize.width, kidSize.height));
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

      prevKidFrame = kidFrame;

      if (NS_FRAME_IS_NOT_COMPLETE(result)) {
        // If the child didn't finish layout then it means that it used
        // up all of our available space (or needs us to split).
        mLastContentIsComplete = PR_FALSE;
        break;
      }
    }
    contentOffset++;
    kidIndex++;
    if (NS_FRAME_IS_NOT_COMPLETE(result)) {
      // If the child didn't finish layout then it means that it used
      // up all of our available space (or needs us to split).
      mLastContentIsComplete = PR_FALSE;
      break;
    }
  }

  // BuildColumnCache calls EnsureCellMap. If that ever changes, be sure to call EnsureCellMap
  // here first.
  BuildColumnCache(aPresContext, aDesiredSize, aReflowState, aStatus);
  // Recalculate Layout Dependencies
  RecalcLayoutData();

  if (nsnull != prevKidFrame) {
    NS_ASSERTION(IsLastChild(prevKidFrame), "unexpected last child");
                                           // can't use SetLastContentOffset here
    mLastContentOffset = contentOffset-1;    // takes into account colGroup frame we're not using
    if (gsDebug) printf("INNER: set last content offset to %d\n", GetLastContentOffset()); //@@@
  }

  aDesiredSize.width = kidSize.width;
  mFirstPassValid = PR_TRUE;

  return result;
}

/** the second of 2 reflow passes
  */
nsReflowStatus nsTableFrame::ResizeReflowPass2(nsIPresContext* aPresContext,
                                               nsReflowMetrics& aDesiredSize,
                                               const nsReflowState& aReflowState,
                                               PRInt32 aMinCaptionWidth,
                                               PRInt32 mMaxCaptionWidth)
{
  NS_PRECONDITION(aReflowState.frame == this, "bad reflow state");
  NS_PRECONDITION(aReflowState.parentReflowState->frame == mGeometricParent,
                  "bad parent reflow state");
  if (PR_TRUE==gsDebugNT)
    printf("%p nsTableFrame::ResizeReflow Pass2: maxSize=%d,%d\n",
           this, aReflowState.maxSize.width, aReflowState.maxSize.height);

  nsReflowStatus result = NS_FRAME_COMPLETE;

  // now that we've computed the column  width information, reflow all children
  nsIContent* c = mContent;
  NS_ASSERTION(nsnull != c, "null kid");
  nsSize kidMaxSize(0,0);

  PRInt32 kidIndex = 0;
  PRInt32 lastIndex = c->ChildCount();
  nsIFrame* prevKidFrame = nsnull;/* XXX incremental reflow! */

#ifdef NS_DEBUG
  //PreReflowCheck();
#endif

  // Initialize out parameter
  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width = 0;
    aDesiredSize.maxElementSize->height = 0;
  }

  PRBool        reflowMappedOK = PR_TRUE;
  nsReflowStatus  status = NS_FRAME_COMPLETE;

  // Check for an overflow list
  MoveOverflowToChildList();

  const nsStyleSpacing* mySpacing = (const nsStyleSpacing*)
    mStyleContext->GetStyleData(eStyleStruct_Spacing);
  nsMargin myBorderPadding;
  mySpacing->CalcBorderPaddingFor(this, myBorderPadding);

  InnerTableReflowState state(aPresContext, aReflowState, myBorderPadding);

  // Reflow the existing frames
  if (nsnull != mFirstChild) {
    reflowMappedOK = ReflowMappedChildren(aPresContext, state, aDesiredSize.maxElementSize);
    if (PR_FALSE == reflowMappedOK) {
      status = NS_FRAME_NOT_COMPLETE;
    }
  }

  // Did we successfully reflow our mapped children?
  if (PR_TRUE == reflowMappedOK) {
    // Any space left?
    if (state.availSize.height <= 0) {
      // No space left. Don't try to pull-up children or reflow unmapped
      if (NextChildOffset() < mContent->ChildCount()) {
        status = NS_FRAME_NOT_COMPLETE;
      }
    } else if (NextChildOffset() < mContent->ChildCount()) {
      // Try and pull-up some children from a next-in-flow
      if (PullUpChildren(aPresContext, state, aDesiredSize.maxElementSize)) {
        // If we still have unmapped children then create some new frames
        if (NextChildOffset() < mContent->ChildCount()) {
          status = ReflowUnmappedChildren(aPresContext, state, aDesiredSize.maxElementSize);
        }
      } else {
        // We were unable to pull-up all the existing frames from the
        // next in flow
        status = NS_FRAME_NOT_COMPLETE;
      }
    }
  }

  // Return our size and our status

  if (NS_FRAME_IS_NOT_COMPLETE(status)) {
    // Don't forget to add in the bottom margin from our last child.
    // Only add it in if there's room for it.
    nscoord margin = state.prevMaxPosBottomMargin -
      state.prevMaxNegBottomMargin;
    if (state.availSize.height >= margin) {
      state.y += margin;
    }
  }

  // Return our desired rect
  //NS_ASSERTION(0<state.y, "illegal height after reflow");
  aDesiredSize.width = aReflowState.maxSize.width;
  aDesiredSize.height = state.y;

  // shrink wrap rows to height of tallest cell in that row
  ShrinkWrapChildren(aPresContext, aDesiredSize, aDesiredSize.maxElementSize);

  if (gsDebugNT==PR_TRUE) 
  {
    if (nsnull!=aDesiredSize.maxElementSize)
      printf("%p: Inner table reflow complete, returning aDesiredSize = %d,%d and aMaxElementSize=%d,%d\n",
              this, aDesiredSize.width, aDesiredSize.height, 
              aDesiredSize.maxElementSize->width, aDesiredSize.maxElementSize->height);
    else
      printf("%p: Inner table reflow complete, returning aDesiredSize = %d,%d and NSNULL aMaxElementSize\n",
              this, aDesiredSize.width, aDesiredSize.height);
  }

  // SEC: assign our real width and height based on this reflow step and return

  mPass = kPASS_UNDEFINED;  // we're no longer in-process

#ifdef NS_DEBUG
  //PostReflowCheck(status);
#endif

  return status;

}

// Collapse child's top margin with previous bottom margin
nscoord nsTableFrame::GetTopMarginFor(nsIPresContext*      aCX,
                                      InnerTableReflowState& aState,
                                      const nsMargin& aKidMargin)
{
  nscoord margin;
  nscoord maxNegTopMargin = 0;
  nscoord maxPosTopMargin = 0;
  if ((margin = aKidMargin.top) < 0) {
    maxNegTopMargin = -margin;
  } else {
    maxPosTopMargin = margin;
  }

  nscoord maxPos = PR_MAX(aState.prevMaxPosBottomMargin, maxPosTopMargin);
  nscoord maxNeg = PR_MAX(aState.prevMaxNegBottomMargin, maxNegTopMargin);
  margin = maxPos - maxNeg;

  return margin;
}

// Position and size aKidFrame and update our reflow state. The origin of
// aKidRect is relative to the upper-left origin of our frame, and includes
// any left/top margin.
void nsTableFrame::PlaceChild(nsIPresContext*    aPresContext,
                              InnerTableReflowState& aState,
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
  aState.y += aKidRect.height;

  // If our height is constrained then update the available height
  if (PR_FALSE == aState.unconstrainedHeight) {
    aState.availSize.height -= aKidRect.height;
  }

  // If this is a footer row group, add it to the list of footer row groups
  const nsStyleDisplay *childDisplay;
  aKidFrame->GetStyleData(eStyleStruct_Display, ((nsStyleStruct *&)childDisplay));
  if (NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP == childDisplay->mDisplay)
  {
    if (nsnull==aState.footerList)
      aState.footerList = new nsVoidArray();
    aState.footerList->AppendElement((void *)aKidFrame);
    aState.footerHeight += aKidRect.height;
  }
  // else if this is a body row group, push down all the footer row groups
  else
  {
    // don't bother unless there are footers to push down
    if (nsnull!=aState.footerList  &&  0!=aState.footerList->Count())
    {
      nsPoint origin;
      aKidFrame->GetOrigin(origin);
      origin.y -= aState.footerHeight;
      aKidFrame->MoveTo(origin.x, origin.y);
      // XXX do we need to check for headers here also, or is that implicit?
      if (NS_STYLE_DISPLAY_TABLE_ROW_GROUP == childDisplay->mDisplay)
      {
        PRInt32 numFooters = aState.footerList->Count();
        for (PRInt32 footerIndex = 0; footerIndex < numFooters; footerIndex++)
        {
          nsTableRowGroupFrame * footer = (nsTableRowGroupFrame *)(aState.footerList->ElementAt(footerIndex));
          NS_ASSERTION(nsnull!=footer, "bad footer list in table inner frame.");
          if (nsnull!=footer)
          {
            footer->GetOrigin(origin);
            origin.y += aKidRect.height;
            footer->MoveTo(origin.x, origin.y);
          }
        }
      }
    }
  }

  // Update the maximum element size
  if (PR_TRUE==aState.firstRowGroup)
  {
    aState.firstRowGroup = PR_FALSE;
    if (nsnull != aMaxElementSize) {
      aMaxElementSize->width = aKidMaxElementSize.width;
      aMaxElementSize->height = aKidMaxElementSize.height;
    }
  }
}

/**
 * Reflow the frames we've already created
 *
 * @param   aPresContext presentation context to use
 * @param   aState current inline state
 * @return  true if we successfully reflowed all the mapped children and false
 *            otherwise, e.g. we pushed children to the next in flow
 */
PRBool nsTableFrame::ReflowMappedChildren( nsIPresContext*        aPresContext,
                                           InnerTableReflowState& aState,
                                           nsSize*                aMaxElementSize)
{
#ifdef NS_DEBUG
  VerifyLastIsComplete();
#endif
#ifdef NOISY
  ListTag(stdout);
  printf(": reflow mapped (childCount=%d) [%d,%d,%c]\n",
         mChildCount,
         mFirstContentOffset, mLastContentOffset,
         (mLastContentIsComplete ? 'T' : 'F'));
#ifdef NOISY_FLOW
  {
    nsTableFrame* flow = (nsTableFrame*) mNextInFlow;
    while (flow != 0) {
      printf("  %p: [%d,%d,%c]\n",
             flow, flow->mFirstContentOffset, flow->mLastContentOffset,
             (flow->mLastContentIsComplete ? 'T' : 'F'));
      flow = (nsTableFrame*) flow->mNextInFlow;
    }
  }
#endif
#endif
  NS_PRECONDITION(nsnull != mFirstChild, "no children");

  PRInt32   childCount = 0;
  nsIFrame* prevKidFrame = nsnull;
  mLastContentOffset = 0;
  // Remember our original mLastContentIsComplete so that if we end up
  // having to push children, we have the correct value to hand to
  // PushChildren.
  PRBool    originalLastContentIsComplete = mLastContentIsComplete;

  nsSize    kidMaxElementSize(0,0);
  nsSize*   pKidMaxElementSize = (nsnull != aMaxElementSize) ? &kidMaxElementSize : nsnull;
  PRBool    result = PR_TRUE;

  for (nsIFrame*  kidFrame = mFirstChild; nsnull != kidFrame; ) {
    nsSize            kidAvailSize(aState.availSize);
    nsReflowMetrics   desiredSize(pKidMaxElementSize);
    desiredSize.width=desiredSize.height=desiredSize.ascent=desiredSize.descent=0;
    nsReflowStatus    status;

    const nsStyleDisplay *childDisplay;
    kidFrame->GetStyleData(eStyleStruct_Display, ((nsStyleStruct *&)childDisplay));
    if (NS_STYLE_DISPLAY_TABLE_CAPTION != childDisplay->mDisplay)
    { // for all colgroups and rowgroups...
      const nsStyleSpacing* kidSpacing;
      kidFrame->GetStyleData(eStyleStruct_Spacing, ((nsStyleStruct *&)kidSpacing));
      nsMargin kidMargin;
      kidSpacing->CalcMarginFor(kidFrame, kidMargin);

      nscoord topMargin = GetTopMarginFor(aPresContext, aState, kidMargin);
      nscoord bottomMargin = kidMargin.bottom;

      // Figure out the amount of available size for the child (subtract
      // off the top margin we are going to apply to it)
      if (PR_FALSE == aState.unconstrainedHeight) {
        kidAvailSize.height -= topMargin;
      }
      // Subtract off for left and right margin
      if (PR_FALSE == aState.unconstrainedWidth) {
        kidAvailSize.width -= kidMargin.left + kidMargin.right;
      }

      // Reflow the child into the available space
      nsReflowState kidReflowState(kidFrame, aState.reflowState, kidAvailSize,
                                   eReflowReason_Resize);
      kidFrame->WillReflow(*aPresContext);
      nscoord x = aState.leftInset + kidMargin.left;
      nscoord y = aState.topInset + aState.y + topMargin;
      kidFrame->MoveTo(x, y);
      status = ReflowChild(kidFrame, aPresContext, desiredSize, kidReflowState);
      if (nsnull!=desiredSize.maxElementSize)
        desiredSize.maxElementSize->width = desiredSize.width;
      // Did the child fit?
      if ((kidFrame != mFirstChild) && (desiredSize.height > kidAvailSize.height))
      {
        // The child is too wide to fit in the available space, and it's
        // not our first child

        // Since we are giving the next-in-flow our last child, we
        // give it our original mLastContentIsComplete too (in case we
        // are pushing into an empty next-in-flow)
        PushChildren(kidFrame, prevKidFrame, originalLastContentIsComplete);
        SetLastContentOffset(prevKidFrame);

        result = PR_FALSE;
        break;
      }

      // Place the child after taking into account it's margin
      aState.y += topMargin;
      nsRect kidRect (x, y, desiredSize.width, desiredSize.height);
      if (NS_STYLE_DISPLAY_TABLE_ROW_GROUP == childDisplay->mDisplay ||
          NS_STYLE_DISPLAY_TABLE_HEADER_GROUP == childDisplay->mDisplay ||
          NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP == childDisplay->mDisplay )
      {
        PlaceChild(aPresContext, aState, kidFrame, kidRect,
                   aMaxElementSize, kidMaxElementSize);
        if (bottomMargin < 0) {
          aState.prevMaxNegBottomMargin = -bottomMargin;
        } else {
          aState.prevMaxPosBottomMargin = bottomMargin;
        }
      }
      childCount++;
      kidFrame->GetContentIndex(mLastContentOffset);

      // Remember where we just were in case we end up pushing children
      prevKidFrame = kidFrame;

      // Update mLastContentIsComplete now that this kid fits
      mLastContentIsComplete = PRBool(NS_FRAME_IS_COMPLETE(status));

      // Special handling for incomplete children
      if (NS_FRAME_IS_NOT_COMPLETE(status)) {
        nsIFrame* kidNextInFlow;
         
        kidFrame->GetNextInFlow(kidNextInFlow);
        PRBool lastContentIsComplete = mLastContentIsComplete;
        if (nsnull == kidNextInFlow) {
          // The child doesn't have a next-in-flow so create a continuing
          // frame. This hooks the child into the flow
          nsIFrame* continuingFrame;
           
          nsIStyleContext* kidSC;
          kidFrame->GetStyleContext(aPresContext, kidSC);
          kidFrame->CreateContinuingFrame(aPresContext, this, kidSC, continuingFrame);
          NS_RELEASE(kidSC);
          NS_ASSERTION(nsnull != continuingFrame, "frame creation failed");

          // Add the continuing frame to the sibling list
          nsIFrame* nextSib;
           
          kidFrame->GetNextSibling(nextSib);
          continuingFrame->SetNextSibling(nextSib);
          kidFrame->SetNextSibling(continuingFrame);
          if (nsnull == nextSib) {
            // Assume that the continuation frame we just created is
            // complete, for now. It will get reflowed by our
            // next-in-flow (we are going to push it now)
            lastContentIsComplete = PR_TRUE;
          }
        }
        // We've used up all of our available space so push the remaining
        // children to the next-in-flow
        nsIFrame* nextSibling;
         
        kidFrame->GetNextSibling(nextSibling);
        if (nsnull != nextSibling) {
          PushChildren(nextSibling, kidFrame, lastContentIsComplete);
          SetLastContentOffset(prevKidFrame);
        }
        result = PR_FALSE;
        break;
      }
    }

    // Get the next child
    kidFrame->GetNextSibling(kidFrame);

    // XXX talk with troy about checking for available space here
  }

  // Update the child count
  mChildCount = childCount;
#ifdef NS_DEBUG
  NS_POSTCONDITION(LengthOf(mFirstChild) == mChildCount, "bad child count");

  nsIFrame* lastChild;
  PRInt32   lastIndexInParent;

  LastChild(lastChild);
  lastChild->GetContentIndex(lastIndexInParent);
  NS_POSTCONDITION(lastIndexInParent == mLastContentOffset, "bad last content offset");
#endif

#ifdef NS_DEBUG
  VerifyLastIsComplete();
#endif
#ifdef NOISY
  ListTag(stdout);
  printf(": reflow mapped %sok (childCount=%d) [%d,%d,%c]\n",
         (result ? "" : "NOT "),
         mChildCount,
         mFirstContentOffset, mLastContentOffset,
         (mLastContentIsComplete ? 'T' : 'F'));
#ifdef NOISY_FLOW
  {
    nsTableFrame* flow = (nsTableFrame*) mNextInFlow;
    while (flow != 0) {
      printf("  %p: [%d,%d,%c]\n",
             flow, flow->mFirstContentOffset, flow->mLastContentOffset,
             (flow->mLastContentIsComplete ? 'T' : 'F'));
      flow = (nsTableFrame*) flow->mNextInFlow;
    }
  }
#endif
#endif
  return result;
}

/**
 * Try and pull-up frames from our next-in-flow
 *
 * @param   aPresContext presentation context to use
 * @param   aState current inline state
 * @return  true if we successfully pulled-up all the children and false
 *            otherwise, e.g. child didn't fit
 */
PRBool nsTableFrame::PullUpChildren(nsIPresContext*      aPresContext,
                                    InnerTableReflowState& aState,
                                    nsSize*              aMaxElementSize)
{
#ifdef NS_DEBUG
  VerifyLastIsComplete();
#endif
#ifdef NOISY
  ListTag(stdout);
  printf(": pullup (childCount=%d) [%d,%d,%c]\n",
         mChildCount,
         mFirstContentOffset, mLastContentOffset,
         (mLastContentIsComplete ? 'T' : 'F'));
#ifdef NOISY_FLOW
  {
    nsTableFrame* flow = (nsTableFrame*) mNextInFlow;
    while (flow != 0) {
      printf("  %p: [%d,%d,%c]\n",
             flow, flow->mFirstContentOffset, flow->mLastContentOffset,
             (flow->mLastContentIsComplete ? 'T' : 'F'));
      flow = (nsTableFrame*) flow->mNextInFlow;
    }
  }
#endif
#endif
  nsTableFrame* nextInFlow = (nsTableFrame*)mNextInFlow;
  nsSize         kidMaxElementSize(0,0);
  nsSize*        pKidMaxElementSize = (nsnull != aMaxElementSize) ? &kidMaxElementSize : nsnull;
#ifdef NS_DEBUG
  PRInt32        kidIndex = NextChildOffset();
#endif
  nsIFrame*      prevKidFrame;
   
  LastChild(prevKidFrame);

  // This will hold the prevKidFrame's mLastContentIsComplete
  // status. If we have to push the frame that follows prevKidFrame
  // then this will become our mLastContentIsComplete state. Since
  // prevKidFrame is initially our last frame, it's completion status
  // is our mLastContentIsComplete value.
  PRBool        prevLastContentIsComplete = mLastContentIsComplete;

  PRBool        result = PR_TRUE;

  while (nsnull != nextInFlow) {
    nsReflowMetrics kidSize(pKidMaxElementSize);
    kidSize.width=kidSize.height=kidSize.ascent=kidSize.descent=0;
    nsReflowStatus  status;

    // Get the next child
    nsIFrame* kidFrame = nextInFlow->mFirstChild;

    // Any more child frames?
    if (nsnull == kidFrame) {
      // No. Any frames on its overflow list?
      if (nsnull != nextInFlow->mOverflowList) {
        // Move the overflow list to become the child list
        nextInFlow->AppendChildren(nextInFlow->mOverflowList);
        nextInFlow->mOverflowList = nsnull;
        kidFrame = nextInFlow->mFirstChild;
      } else {
        // We've pulled up all the children, so move to the next-in-flow.
        nextInFlow->GetNextInFlow((nsIFrame*&)nextInFlow);
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
    if ((kidFrameSize.height > aState.availSize.height) &&
        NS_FRAME_IS_NOT_SPLITTABLE(kidIsSplittable)) {
      result = PR_FALSE;
      mLastContentIsComplete = prevLastContentIsComplete;
      break;
    }
    nsReflowState kidReflowState(kidFrame, aState.reflowState, aState.availSize,
                                 eReflowReason_Resize);
    kidFrame->WillReflow(*aPresContext);
    status = ReflowChild(kidFrame, aPresContext, kidSize, kidReflowState);

    // Did the child fit?
    if ((kidSize.height > aState.availSize.height) && (nsnull != mFirstChild)) {
      // The child is too wide to fit in the available space, and it's
      // not our first child
      result = PR_FALSE;
      mLastContentIsComplete = prevLastContentIsComplete;
      break;
    }

    // Advance y by the topMargin between children. Zero out the
    // topMargin in case this frame is continued because
    // continuations do not have a top margin. Update the prev
    // bottom margin state in the body reflow state so that we can
    // apply the bottom margin when we hit the next child (or
    // finish).
    //aState.y += topMargin;
    nsRect kidRect (0, 0, kidSize.width, kidSize.height);
    //kidRect.x += kidMol->margin.left;
    kidRect.y += aState.y;
    const nsStyleDisplay *childDisplay;
    kidFrame->GetStyleData(eStyleStruct_Display, ((nsStyleStruct *&)childDisplay));
    if (NS_STYLE_DISPLAY_TABLE_ROW_GROUP == childDisplay->mDisplay ||
        NS_STYLE_DISPLAY_TABLE_HEADER_GROUP == childDisplay->mDisplay ||
        NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP == childDisplay->mDisplay )
    {
      PlaceChild(aPresContext, aState, kidFrame, kidRect, aMaxElementSize, *pKidMaxElementSize);
    }

    // Remove the frame from its current parent
    kidFrame->GetNextSibling(nextInFlow->mFirstChild);
    nextInFlow->mChildCount--;
    // Update the next-in-flows first content offset
    if (nsnull != nextInFlow->mFirstChild) {
      nextInFlow->SetFirstContentOffset(nextInFlow->mFirstChild);
    }

    // Link the frame into our list of children
    kidFrame->SetGeometricParent(this);
    nsIFrame* contentParent;

    kidFrame->GetContentParent(contentParent);
    if (nextInFlow == contentParent) {
      kidFrame->SetContentParent(this);
    }
    if (nsnull == prevKidFrame) {
      mFirstChild = kidFrame;
      SetFirstContentOffset(kidFrame);
    } else {
      prevKidFrame->SetNextSibling(kidFrame);
    }
    kidFrame->SetNextSibling(nsnull);
    mChildCount++;

    // Remember where we just were in case we end up pushing children
    prevKidFrame = kidFrame;
    prevLastContentIsComplete = mLastContentIsComplete;

    // Is the child we just pulled up complete?
    mLastContentIsComplete = PRBool(NS_FRAME_IS_COMPLETE(status));
    if (NS_FRAME_IS_NOT_COMPLETE(status)) {
      // No the child isn't complete
      nsIFrame* kidNextInFlow;
       
      kidFrame->GetNextInFlow(kidNextInFlow);
      if (nsnull == kidNextInFlow) {
        // The child doesn't have a next-in-flow so create a
        // continuing frame. The creation appends it to the flow and
        // prepares it for reflow.
        nsIFrame* continuingFrame;

        nsIStyleContext* kidSC;
        kidFrame->GetStyleContext(aPresContext, kidSC);
        kidFrame->CreateContinuingFrame(aPresContext, this, kidSC, continuingFrame);
        NS_RELEASE(kidSC);
        NS_ASSERTION(nsnull != continuingFrame, "frame creation failed");

        // Add the continuing frame to our sibling list and then push
        // it to the next-in-flow. This ensures the next-in-flow's
        // content offsets and child count are set properly. Note that
        // we can safely assume that the continuation is complete so
        // we pass PR_TRUE into PushChidren in case our next-in-flow
        // was just drained and now needs to know it's
        // mLastContentIsComplete state.
        kidFrame->SetNextSibling(continuingFrame);

        PushChildren(continuingFrame, kidFrame, PR_TRUE);

        // After we push the continuation frame we don't need to fuss
        // with mLastContentIsComplete beause the continuation frame
        // is no longer on *our* list.
      }

      // If the child isn't complete then it means that we've used up
      // all of our available space.
      result = PR_FALSE;
      break;
    }
  }

  // Update our last content offset
  if (nsnull != prevKidFrame) {
    NS_ASSERTION(IsLastChild(prevKidFrame), "bad last child");
    SetLastContentOffset(prevKidFrame);
  }

  // We need to make sure the first content offset is correct for any empty
  // next-in-flow frames (frames where we pulled up all the child frames)
  nextInFlow = (nsTableFrame*)mNextInFlow;
  if ((nsnull != nextInFlow) && (nsnull == nextInFlow->mFirstChild)) {
    // We have at least one empty frame. Did we succesfully pull up all the
    // child frames?
    if (PR_FALSE == result) {
      // No, so we need to adjust the first content offset of all the empty
      // frames
      AdjustOffsetOfEmptyNextInFlows();
#ifdef NS_DEBUG
    } else {
      // Yes, we successfully pulled up all the child frames which means all
      // the next-in-flows must be empty. Do a sanity check
      while (nsnull != nextInFlow) {
        NS_ASSERTION(nsnull == nextInFlow->mFirstChild, "non-empty next-in-flow");
        nextInFlow->GetNextInFlow((nsIFrame*&)nextInFlow);
      }
#endif
    }
  }

#ifdef NS_DEBUG
  VerifyLastIsComplete();
#endif
  return result;
}

/**
 * Create new frames for content we haven't yet mapped
 *
 * @param   aPresContext presentation context to use
 * @param   aState current inline state
 * @return  frComplete if all content has been mapped and frNotComplete
 *            if we should be continued
 */
nsReflowStatus
nsTableFrame::ReflowUnmappedChildren(nsIPresContext*      aPresContext,
                                     InnerTableReflowState& aState,
                                     nsSize*              aMaxElementSize)
{
#ifdef NS_DEBUG
  VerifyLastIsComplete();
#endif
  nsIFrame*    kidPrevInFlow = nsnull;
  nsReflowStatus result = NS_FRAME_NOT_COMPLETE;

  // If we have no children and we have a prev-in-flow then we need to pick
  // up where it left off. If we have children, e.g. we're being resized, then
  // our content offset should already be set correctly...
  if ((nsnull == mFirstChild) && (nsnull != mPrevInFlow)) {
    nsTableFrame* prev = (nsTableFrame*)mPrevInFlow;
    NS_ASSERTION(prev->mLastContentOffset >= prev->mFirstContentOffset, "bad prevInFlow");

    mFirstContentOffset = prev->NextChildOffset();
    if (!prev->mLastContentIsComplete) {
      // Our prev-in-flow's last child is not complete
      prev->LastChild(kidPrevInFlow);
    }
  }
  mLastContentIsComplete = PR_TRUE;

  // Place our children, one at a time until we are out of children
  nsSize    kidMaxElementSize(0,0);
  nsSize*   pKidMaxElementSize = (nsnull != aMaxElementSize) ? &kidMaxElementSize : nsnull;
  PRInt32   kidIndex = NextChildOffset();
  nsIFrame* prevKidFrame;

  LastChild(prevKidFrame);
  for (;;) {
    // Get the next content object
    nsIContentPtr kid = mContent->ChildAt(kidIndex);
    if (kid.IsNull()) {
      result = NS_FRAME_COMPLETE;
      break;
    }

    // Make sure we still have room left
    if (aState.availSize.height <= 0) {
      // Note: return status was set to frNotComplete above...
      break;
    }

    // Resolve style for the child
    nsIStyleContextPtr kidStyleContext =
      aPresContext->ResolveStyleContextFor(kid, this, PR_TRUE);

    // Figure out how we should treat the child
    nsIFrame*        kidFrame;

    // Create a child frame
    if (nsnull == kidPrevInFlow) {
      nsIContentDelegate* kidDel = nsnull;
      kidDel = kid->GetDelegate(aPresContext);
      nsresult rv = kidDel->CreateFrame(aPresContext, kid, this,
                                        kidStyleContext, kidFrame);
      NS_RELEASE(kidDel);
    } else {
      kidPrevInFlow->CreateContinuingFrame(aPresContext, this, kidStyleContext,
                                           kidFrame);
    }

    // Link child frame into the list of children
    if (nsnull != prevKidFrame) {
      prevKidFrame->SetNextSibling(kidFrame);
    } else {
      mFirstChild = kidFrame;  // our first child
      SetFirstContentOffset(kidFrame);
    }
    mChildCount++;

    // Try to reflow the child into the available space. It might not
    // fit or might need continuing.
    nsReflowMetrics kidSize(pKidMaxElementSize);
    kidSize.width=kidSize.height=kidSize.ascent=kidSize.descent=0;
    nsReflowState   kidReflowState(kidFrame, aState.reflowState, aState.availSize,
                                   eReflowReason_Initial);
    kidFrame->WillReflow(*aPresContext);
    kidFrame->MoveTo(0, aState.y);
    nsReflowStatus status = ReflowChild(kidFrame,aPresContext, kidSize, kidReflowState);

    // Did the child fit?
    if ((kidSize.height > aState.availSize.height) && (nsnull != mFirstChild)) {
      // The child is too wide to fit in the available space, and it's
      // not our first child. Add the frame to our overflow list
      NS_ASSERTION(nsnull == mOverflowList, "bad overflow list");
      mOverflowList = kidFrame;
      prevKidFrame->SetNextSibling(nsnull);
      break;
    }

    // Advance y by the topMargin between children. Zero out the
    // topMargin in case this frame is continued because
    // continuations do not have a top margin. Update the prev
    // bottom margin state in the body reflow state so that we can
    // apply the bottom margin when we hit the next child (or
    // finish).
    //aState.y += topMargin;
    nsRect kidRect (0, aState.y, kidSize.width, kidSize.height);
    const nsStyleDisplay *childDisplay;
    kidFrame->GetStyleData(eStyleStruct_Display, ((nsStyleStruct *&)childDisplay));
    if (NS_STYLE_DISPLAY_TABLE_ROW_GROUP == childDisplay->mDisplay ||
        NS_STYLE_DISPLAY_TABLE_HEADER_GROUP == childDisplay->mDisplay ||
        NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP == childDisplay->mDisplay )
    {
      PlaceChild(aPresContext, aState, kidFrame, kidRect, aMaxElementSize, *pKidMaxElementSize);
    }

    prevKidFrame = kidFrame;
    kidIndex++;

    // Did the child complete?
    if (NS_FRAME_IS_NOT_COMPLETE(status)) {
      // If the child isn't complete then it means that we've used up
      // all of our available space
      mLastContentIsComplete = PR_FALSE;
      break;
    }
    kidPrevInFlow = nsnull;
  }

  // Update the content mapping
  NS_ASSERTION(IsLastChild(prevKidFrame), "bad last child");
  SetLastContentOffset(prevKidFrame);
#ifdef NS_DEBUG
  PRInt32 len = LengthOf(mFirstChild);
  NS_ASSERTION(len == mChildCount, "bad child count");
#endif
#ifdef NS_DEBUG
  VerifyLastIsComplete();
#endif
  return result;
}


/**
  Now I've got all the cells laid out in an infinite space.
  For each column, use the min size for each cell in that column
  along with the attributes of the table, column group, and column
  to assign widths to each column.
  */
// use the cell map to determine which cell is in which column.
void nsTableFrame::BalanceColumnWidths(nsIPresContext* aPresContext, 
                                       const nsReflowState& aReflowState,
                                       const nsSize& aMaxSize, 
                                       nsSize* aMaxElementSize)
{
  NS_ASSERTION(nsnull==mPrevInFlow, "never ever call me on a continuing frame!");

  if (gsDebug)
    printf ("BalanceColumnWidths...\n");

  nsVoidArray *columnLayoutData = GetColumnLayoutData();
  PRInt32 numCols = columnLayoutData->Count();
  if (nsnull==mColumnWidths)
  {
    mColumnWidths = new PRInt32[numCols];
    nsCRT::memset (mColumnWidths, 0, numCols*sizeof(PRInt32));
  }

  // need to track min and max table widths
  PRInt32 minTableWidth = 0;
  PRInt32 maxTableWidth = 0;
  PRInt32 totalFixedWidth = 0;

  const nsStyleSpacing* spacing =
    (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);
  nsMargin borderPadding;
  spacing->CalcBorderPaddingFor(this, borderPadding);

  // need to figure out the overall table width constraint
  // default case, get 100% of available space
  PRInt32 maxWidth;
  const nsStylePosition* position =
    (const nsStylePosition*)mStyleContext->GetStyleData(eStyleStruct_Position);
  switch (position->mWidth.GetUnit()) {
  case eStyleUnit_Coord:
    maxWidth = position->mWidth.GetCoordValue();
    break;
  case eStyleUnit_Percent:
  case eStyleUnit_Proportional:
    // XXX for now these fall through

  default:
  case eStyleUnit_Auto:
  case eStyleUnit_Inherit:
    maxWidth = aMaxSize.width;
    break;
  }

  // now, if maxWidth is not NS_UNCONSTRAINED, subtract out my border
  // and padding
  if (NS_UNCONSTRAINEDSIZE!=maxWidth)
  {
    maxWidth -= borderPadding.left + borderPadding.right;
    if (0>maxWidth)  // nonsense style specification
      maxWidth = 0;
  }

  if (PR_TRUE==gsDebug || PR_TRUE==gsDebugNT) 
    printf ("%p: maxWidth=%d from aMaxSize=%d,%d\n", 
            this, maxWidth, aMaxSize.width, aMaxSize.height);

  // based on the compatibility mode, create a table layout strategy
  if (nsnull==mTableLayoutStrategy)
  { // TODO:  build a different strategy based on the compatibility mode
    mTableLayoutStrategy = new BasicTableLayoutStrategy(this, numCols);
  }
  mTableLayoutStrategy->BalanceColumnWidths(aPresContext, mStyleContext,
                                            aReflowState, maxWidth,
                                            totalFixedWidth, 
                                            minTableWidth, maxTableWidth,
                                            aMaxElementSize);

}

/**
  sum the width of each column
  add in table insets
  set rect
  */
void nsTableFrame::SetTableWidth(nsIPresContext* aPresContext)
{
  NS_ASSERTION(nsnull==mPrevInFlow, "never ever call me on a continuing frame!");

  if (gsDebug==PR_TRUE) printf ("SetTableWidth...");
  PRInt32 tableWidth = 0;
  nsVoidArray *columnLayoutData = GetColumnLayoutData();
  PRInt32 numCols = columnLayoutData->Count();
  for (PRInt32 i = 0; i<numCols; i++)
  {
    tableWidth += mColumnWidths[i];
    if (gsDebug==PR_TRUE) 
      printf (" += %d ", mColumnWidths[i]);
  }

  // Compute the insets (sum of border and padding)
  const nsStyleSpacing* spacing =
    (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);
  nsMargin borderPadding;
  spacing->CalcBorderPaddingFor(this, borderPadding);

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
  SetRect(tableSize);
}

/**
  */
void nsTableFrame::ShrinkWrapChildren(nsIPresContext* aPresContext, 
                                      nsReflowMetrics& aDesiredSize,
                                      nsSize* aMaxElementSize)
{
#ifdef NS_DEBUG
  PRBool gsDebugWas = gsDebug;
  //gsDebug = PR_TRUE;  // turn on debug in this method
#endif
  // iterate children, tell all row groups to ShrinkWrap
  PRBool atLeastOneRowSpanningCell = PR_FALSE;

  PRInt32 rowIndex;
  PRInt32 tableHeight = 0;

  const nsStyleSpacing* spacing = (const nsStyleSpacing*)
    mStyleContext->GetStyleData(eStyleStruct_Spacing);
  nsMargin borderPadding;
  spacing->CalcBorderPaddingFor(this, borderPadding);

  tableHeight += borderPadding.top + borderPadding.bottom;

  PRInt32 childCount = mChildCount;
  nsIFrame * kidFrame;
  for (PRInt32 i = 0; i < childCount; i++)
  {
    PRInt32 childHeight=0;
    // for every child that is a rowFrame, set the row frame height  = sum of row heights

    if (0==i)
      ChildAt(i, kidFrame); // frames are not ref counted
    else
      kidFrame->GetNextSibling(kidFrame);
    NS_ASSERTION(nsnull != kidFrame, "bad kid frame");

    nscoord topInnerMargin = 0;
    nscoord bottomInnerMargin = 0;

    const nsStyleDisplay *childDisplay;
    kidFrame->GetStyleData(eStyleStruct_Display, ((nsStyleStruct *&)childDisplay));
    if (NS_STYLE_DISPLAY_TABLE_ROW_GROUP == childDisplay->mDisplay ||
        NS_STYLE_DISPLAY_TABLE_HEADER_GROUP == childDisplay->mDisplay ||
        NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP == childDisplay->mDisplay )
    {
      /* Step 1:  set the row height to the height of the tallest cell,
       *          and resize all cells in that row to that height (except cells with rowspan>1)
       */
      PRInt32 rowGroupHeight = 0;
      nsTableRowGroupFrame * rowGroupFrame = (nsTableRowGroupFrame *)kidFrame;
      PRInt32 numRows;
      rowGroupFrame->ChildCount(numRows);
      PRInt32 *rowHeights = new PRInt32[numRows];
      nsCRT::memset (rowHeights, 0, numRows*sizeof(PRInt32));
      if (gsDebug==PR_TRUE) printf("Height Step 1...\n");
      for (rowIndex = 0; rowIndex < numRows; rowIndex++)
      {
        // get the height of the tallest cell in the row (excluding cells that span rows)
        nsTableRowFrame *rowFrame;
         
        rowGroupFrame->ChildAt(rowIndex, (nsIFrame*&)rowFrame);
        NS_ASSERTION(nsnull != rowFrame, "bad row frame");

        nscoord maxCellHeight       = rowFrame->GetTallestChild();
        nscoord maxCellTopMargin    = rowFrame->GetChildMaxTopMargin();
        nscoord maxCellBottomMargin = rowFrame->GetChildMaxBottomMargin();
        nscoord maxRowHeight = maxCellHeight + maxCellTopMargin + maxCellBottomMargin;

        rowHeights[rowIndex] = maxRowHeight;
    
        if (rowIndex == 0)
          topInnerMargin = maxCellTopMargin;
        if (rowIndex+1 == numRows)
          bottomInnerMargin = maxCellBottomMargin;


        nsSize  rowFrameSize(0,0);

        rowFrame->GetSize(rowFrameSize);
        rowFrame->SizeTo(rowFrameSize.width, maxRowHeight);
        rowGroupHeight += maxRowHeight;
        // resize all the cells based on the rowHeight
        PRInt32 numCells;

        rowFrame->ChildCount(numCells);
        for (PRInt32 cellIndex = 0; cellIndex < numCells; cellIndex++)
        {
          nsTableCellFrame *cellFrame;

          rowFrame->ChildAt(cellIndex, (nsIFrame*&)cellFrame);
          PRInt32 rowSpan = GetEffectiveRowSpan(rowIndex, cellFrame);//cellFrame->GetRowSpan();
          if (1==rowSpan)
          {
            if (gsDebug==PR_TRUE) printf("  setting cell[%d,%d] height to %d\n", rowIndex, cellIndex, rowHeights[rowIndex]);

            nsSize  cellFrameSize(0,0);

            cellFrame->GetSize(cellFrameSize);
            cellFrame->SizeTo(cellFrameSize.width, maxCellHeight);
            // Realign cell content based on new height
            cellFrame->VerticallyAlignChild(aPresContext);
          }
          else
          {
            if (gsDebug==PR_TRUE) printf("  skipping cell[%d,%d]\n", rowIndex, cellIndex);
            atLeastOneRowSpanningCell = PR_TRUE;
          }
        }
      }

      /* Step 2:  now account for cells that span rows.
       *          a spanning cell's height is the sum of the heights of the rows it spans,
       *          or it's own desired height, whichever is greater.
       *          If the cell's desired height is the larger value, resize the rows and contained
       *          cells by an equal percentage of the additional space.
       */
      /* TODO
       * 1. optimization, if (PR_TRUE==atLeastOneRowSpanningCell) ... otherwise skip this step entirely
       * 2. find cases where spanning cells effect other spanning cells that began in rows above themselves.
       *    I think in this case, we have to make another pass through step 2.
       *    There should be a "rational" check to terminate that kind of loop after n passes, probably 3 or 4.
       */
      if (gsDebug==PR_TRUE) printf("Height Step 2...\n");
      rowGroupHeight=0;
      nsTableRowFrame *rowFrame=nsnull;
      for (rowIndex = 0; rowIndex < numRows; rowIndex++)
      {
        // get the next row
        if (0==rowIndex)
          rowGroupFrame->ChildAt(rowIndex, (nsIFrame*&)rowFrame);
        else
          rowFrame->GetNextSibling((nsIFrame*&)rowFrame);
        PRInt32 numCells;
        rowFrame->ChildCount(numCells);
        // check this row for a cell with rowspans
        for (PRInt32 cellIndex = 0; cellIndex < numCells; cellIndex++)
        {
          // get the next cell
          nsTableCellFrame *cellFrame;
          rowFrame->ChildAt(cellIndex, (nsIFrame*&)cellFrame);
          PRInt32 rowSpan = GetEffectiveRowSpan(rowIndex, cellFrame);//cellFrame->GetRowSpan();
          if (1<rowSpan)
          { // found a cell with rowspan > 1, determine it's height
            if (gsDebug==PR_TRUE) printf("  cell[%d,%d] has a rowspan = %d\n", rowIndex, cellIndex, rowSpan);
            
            nscoord heightOfRowsSpanned = 0;
            for (PRInt32 i=0; i<rowSpan; i++)
              heightOfRowsSpanned += rowHeights[i+rowIndex];
            
            heightOfRowsSpanned -= topInnerMargin + bottomInnerMargin;

            /* if the cell height fits in the rows, expand the spanning cell's height and slap it in */
            nsSize  cellFrameSize(0,0);
            cellFrame->GetSize(cellFrameSize);
            if (heightOfRowsSpanned>cellFrameSize.height)
            {
              if (gsDebug==PR_TRUE) printf("  cell[%d,%d] fits, setting height to %d\n", rowIndex, cellIndex, heightOfRowsSpanned);
              cellFrame->SizeTo(cellFrameSize.width, heightOfRowsSpanned);
              // Realign cell content based on new height
              cellFrame->VerticallyAlignChild(aPresContext);
            }
            /* otherwise, distribute the excess height to the rows effected, and to the cells in those rows
             * push all subsequent rows down by the total change in height of all the rows above it
             */
            else
            {
              PRInt32 excessHeight = cellFrameSize.height - heightOfRowsSpanned;
              PRInt32 excessHeightPerRow = excessHeight/rowSpan;
              if (gsDebug==PR_TRUE) printf("  cell[%d,%d] does not fit, excessHeight = %d, excessHeightPerRow = %d\n", 
                      rowIndex, cellIndex, excessHeight, excessHeightPerRow);
              // for every row starting at the row with the spanning cell...
              for (i=rowIndex; i<numRows; i++)
              {
                nsTableRowFrame *rowFrameToBeResized;

                rowGroupFrame->ChildAt(i, (nsIFrame*&)rowFrameToBeResized);
                // if the row is within the spanned range, resize the row and it's cells
                if (i<rowIndex+rowSpan)
                {
                  rowHeights[i] += excessHeightPerRow;
                  if (gsDebug==PR_TRUE) printf("    rowHeight[%d] set to %d\n", i, rowHeights[i]);

                  nsSize  rowFrameSize(0,0);

                  rowFrameToBeResized->GetSize(rowFrameSize);
                  rowFrameToBeResized->SizeTo(rowFrameSize.width, rowHeights[i]);
                  PRInt32 cellCount;

                  rowFrameToBeResized->ChildCount(cellCount);
                  for (PRInt32 j=0; j<cellCount; j++)
                  {
                    if (i==rowIndex && j==cellIndex)
                    {
                      if (gsDebug==PR_TRUE) printf("      cell[%d, %d] skipping self\n", i, j);
                      continue; // don't do math on myself, only the other cells I effect
                    }
                    nsTableCellFrame *frame;

                    rowFrameToBeResized->ChildAt(j, (nsIFrame*&)frame);
                    PRInt32 frameRowSpan = GetEffectiveRowSpan(i, frame);
                    if (frameRowSpan==1)
                    {
                      nsSize  frameSize(0,0);

                      frame->GetSize(frameSize);
                      if (gsDebug==PR_TRUE) printf("      cell[%d, %d] set height to %d\n", i, j, frameSize.height+excessHeightPerRow);
                      frame->SizeTo(frameSize.width, frameSize.height+excessHeightPerRow);
                      // Realign cell content based on new height
                      frame->VerticallyAlignChild(aPresContext);
                    }
                  }
                }
                // if we're dealing with a row below the row containing the spanning cell, 
                // push that row down by the amount we've expanded the cell heights by
                if (i>=rowIndex && i!=0)
                {
                  nsRect rowRect;
                   
                  rowFrameToBeResized->GetRect(rowRect);
                  nscoord delta = excessHeightPerRow*(i-rowIndex);
                  if (delta > excessHeight)
                    delta = excessHeight;
                  rowFrameToBeResized->MoveTo(rowRect.x, rowRect.y + delta);
                  if (gsDebug==PR_TRUE) printf("      row %d (%p) moved by %d to y-offset %d\n",  
                                                i, rowFrameToBeResized, delta, rowRect.y + delta);
                }
              }
            }
          }
        }
        rowGroupHeight += rowHeights[rowIndex];
      }
      if (gsDebug==PR_TRUE) printf("row group height set to %d\n", rowGroupHeight);

      nsSize  rowGroupFrameSize(0,0);

      rowGroupFrame->GetSize(rowGroupFrameSize);
      rowGroupFrame->SizeTo(rowGroupFrameSize.width, rowGroupHeight);
      tableHeight += rowGroupHeight;
      if (nsnull!=rowHeights)
        delete [] rowHeights;
    }
  }
  if (0!=tableHeight)
  {
    if (gsDebug==PR_TRUE) printf("table desired height set to %d\n", tableHeight);
    aDesiredSize.height = tableHeight;
  }
#ifdef NS_DEBUG
  gsDebug = gsDebugWas;
#endif
}

void nsTableFrame::VerticallyAlignChildren(nsIPresContext* aPresContext,
                                           nscoord* aAscents,
                                           nscoord aMaxAscent,
                                           nscoord aMaxHeight)
{
}

NS_METHOD
nsTableFrame::SetColumnStyleFromCell(nsIPresContext  * aPresContext,
                                     nsTableCellFrame* aCellFrame,
                                     nsTableRowFrame * aRowFrame)
{
  // if this cell is in the first row, then the width attribute
  // also acts as the width attribute for the entire column
  if ((nsnull!=aPresContext) && (nsnull!=aCellFrame) && (nsnull!=aRowFrame))
  {
    if (0==aRowFrame->GetRowIndex())
    {
      // get the cell style info
      const nsStylePosition* cellPosition;
      aCellFrame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct *&)cellPosition);
      if ((eStyleUnit_Coord == cellPosition->mWidth.GetUnit()) ||
           (eStyleUnit_Percent==cellPosition->mWidth.GetUnit())) {
        // compute the width per column spanned
        PRInt32 colSpan = aCellFrame->GetColSpan();
        for (PRInt32 i=0; i<colSpan; i++)
        {
          // get the appropriate column frame
          nsTableColFrame *colFrame;
          GetColumnFrame(i+aCellFrame->GetColIndex(), colFrame);
          // get the column style and set the width attribute
          nsIStyleContext* colSC;
          colFrame->GetStyleContext(aPresContext, colSC);
          nsStylePosition* colPosition = (nsStylePosition*) colSC->GetMutableStyleData(eStyleStruct_Position);
          // set the column width attribute
          if (eStyleUnit_Coord == cellPosition->mWidth.GetUnit())
          {
            nscoord width = cellPosition->mWidth.GetCoordValue();
            colPosition->mWidth.SetCoordValue(width/colSpan);
          }
          else
          {
            float width = cellPosition->mWidth.GetPercentValue();
            colPosition->mWidth.SetPercentValue(width/colSpan);
          }
        }
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
  nsTableFrame * firstInFlow = (nsTableFrame *)GetFirstInFlow();
  if (nsnull!=firstInFlow->mColumnLayoutData)
  { // hooray, we get to do this the easy way because the info is cached
    nsColLayoutData * colData = (nsColLayoutData *)
      (firstInFlow->mColumnLayoutData->ElementAt(aColIndex));
    NS_ASSERTION(nsnull != colData, "bad column data");
    aColFrame = colData->GetColFrame();
    NS_ASSERTION(nsnull!=aColFrame, "bad col frame");
  }
  else
  { // ah shucks, we have to go hunt for the column frame brute-force style
    nsIFrame *childFrame;
    FirstChild(childFrame);
    for (;;)
    {
      if (nsnull==childFrame)
      {
        NS_ASSERTION (PR_FALSE, "scanned the frame hierarchy and no column frame could be found.");
        break;
      }
      const nsStyleDisplay *childDisplay;
      childFrame->GetStyleData(eStyleStruct_Display, ((nsStyleStruct *&)childDisplay));
      if (NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP == childDisplay->mDisplay)
      {
        PRInt32 colGroupStartingIndex = ((nsTableColGroupFrame *)childFrame)->GetStartColumnIndex();
        if (aColIndex >= colGroupStartingIndex)
        { // the cell's col might be in this col group
          PRInt32 childCount;
          childFrame->ChildCount(childCount);
          if (aColIndex < colGroupStartingIndex + childCount)
          { // yep, we've found it
            childFrame->ChildAt(aColIndex-colGroupStartingIndex, (nsIFrame *&)aColFrame);
            break;
          }
        }
      }
      childFrame->GetNextSibling(childFrame);
    }
  }
  return NS_OK;
}

void nsTableFrame::BuildColumnCache( nsIPresContext*      aPresContext,
                                     nsReflowMetrics&     aDesiredSize,
                                     const nsReflowState& aReflowState,
                                     nsReflowStatus&      aStatus
                                    )
{
  EnsureColumns(aPresContext, aDesiredSize, aReflowState, aStatus);
  if (nsnull==mColCache)
  {
    mColCache = new ColumnInfoCache(mColCount);
    nsIFrame * childFrame = mFirstChild;
    while (nsnull!=childFrame)
    { // for every child, if it's a col group then get the columns
      const nsStyleDisplay *childDisplay;
      childFrame->GetStyleData(eStyleStruct_Display, ((nsStyleStruct *&)childDisplay));
      if (NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP == childDisplay->mDisplay)
      {
        nsTableColFrame *colFrame=nsnull;
        childFrame->ChildAt(0, (nsIFrame *&)colFrame);
        while (nsnull!=colFrame)
        { // for every column, create an entry in the column cache
          // assumes that the col style has been twiddled to account for first cell width attribute
          const nsStylePosition* colPosition;
          colFrame->GetStyleData(eStyleStruct_Position, ((nsStyleStruct *&)colPosition));
          mColCache->AddColumnInfo(colPosition->mWidth.GetUnit(), colFrame->GetColumnIndex());
          colFrame->GetNextSibling((nsIFrame *&)colFrame);
        }
      }
      else if (NS_STYLE_DISPLAY_TABLE_ROW_GROUP == childDisplay->mDisplay ||
               NS_STYLE_DISPLAY_TABLE_HEADER_GROUP == childDisplay->mDisplay ||
               NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP == childDisplay->mDisplay )
      { // for every cell in every row, call SetCellLayoutData with the cached info
        // QQQ we can probably just leave the info in the table cell frame, not cache it here
        nsIFrame *rowFrame;
        childFrame->ChildAt(0, rowFrame);
        while (nsnull!=rowFrame)
        {
          nsIFrame *cellFrame;
          rowFrame->ChildAt(0, cellFrame);
          while (nsnull!=cellFrame)
          {
            nsCellLayoutData *cld = ((nsTableCellFrame*)cellFrame)->GetCellLayoutData();
            SetCellLayoutData(aPresContext, cld, (nsTableCellFrame*)cellFrame);
            /* this is the first time we are guaranteed to have both the cell frames
             * and the column frames, so it's a good time to 
             * set the column style from the cell's width attribute (if this is the first row)
             */
            SetColumnStyleFromCell(aPresContext, (nsTableCellFrame *)cellFrame, (nsTableRowFrame *)rowFrame);
            cellFrame->GetNextSibling(cellFrame);
          }
          rowFrame->GetNextSibling(rowFrame);
        }
      }
      childFrame->GetNextSibling(childFrame);
    }
  }
}

nsVoidArray * nsTableFrame::GetColumnLayoutData()
{
  nsTableFrame * firstInFlow = (nsTableFrame *)GetFirstInFlow();
  NS_ASSERTION(nsnull!=firstInFlow, "illegal state -- no first in flow");
  NS_ASSERTION(nsnull!=firstInFlow->mColumnLayoutData, "illegal state -- no column layout data");
  return firstInFlow->mColumnLayoutData;
}

/** Associate aData with the cell at (aRow,aCol)
  * @return PR_TRUE if the data was successfully associated with a Cell
  *         PR_FALSE if there was an error, such as aRow or aCol being invalid
  */
PRBool nsTableFrame::SetCellLayoutData(nsIPresContext* aPresContext,
                                       nsCellLayoutData * aData, nsTableCellFrame *aCell)
{
  NS_ASSERTION(nsnull != aPresContext, "bad arg aPresContext");
  NS_ASSERTION(nsnull != aData, "bad arg aData");
  NS_ASSERTION(nsnull != aCell, "bad arg aCell");

  PRBool result = PR_TRUE;

  nsTableFrame * firstInFlow = (nsTableFrame *)GetFirstInFlow();
  NS_ASSERTION(nsnull!=firstInFlow, "illegal state -- no first in flow");
  if (this!=firstInFlow)
    result = firstInFlow->SetCellLayoutData(aPresContext, aData, aCell);
  else
  {
    if ((kPASS_FIRST==GetReflowPass()) || (kPASS_INCREMENTAL==GetReflowPass()))
    {
      if (nsnull==mColumnLayoutData)
      {
        PRInt32 rows = GetRowCount();
        mColumnLayoutData = new nsVoidArray();
        NS_ASSERTION(nsnull != mColumnLayoutData, "bad alloc");
        PRInt32 tableKidCount = mContent->ChildCount();
        nsIFrame * colGroupFrame = mFirstChild;
        for (PRInt32 i=0; i<tableKidCount; )  // notice increment of i is done just before ChildAt call
        {
          const nsStyleDisplay *childDisplay;
          colGroupFrame->GetStyleData(eStyleStruct_Display, ((nsStyleStruct *&)childDisplay));
          if (NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP == childDisplay->mDisplay)
          {
            nsTableColFrame *colFrame=nsnull;
            colGroupFrame->ChildAt(0, (nsIFrame *&)colFrame);
            while (nsnull!=colFrame)
            {
              // TODO:  unify these 2 kinds of column data
              // TODO:  cache more column data, like the mWidth.GetUnit and what its value
              nsColLayoutData *colData = new nsColLayoutData(colFrame, rows);
              mColumnLayoutData->AppendElement((void *)colData);
              colFrame->GetNextSibling((nsIFrame *&)colFrame);
            }
          }
          // can't have col groups after row groups, so stop if you find a row group
          else if (NS_STYLE_DISPLAY_TABLE_ROW_GROUP == childDisplay->mDisplay ||
                   NS_STYLE_DISPLAY_TABLE_HEADER_GROUP == childDisplay->mDisplay ||
                   NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP == childDisplay->mDisplay )
          {
            break;
          }
          i++;
          ChildAt(i, colGroupFrame);  // can't use colGroupFrame->GetNextSibling because it hasn't been set yet
        }
      }

      PRInt32 firstColIndex = aCell->GetColIndex();
      nsTableRowFrame *row;
      aCell->GetGeometricParent((nsIFrame*&)row);
      PRInt32 rowIndex = row->GetRowIndex();
      PRInt32 colSpan = aCell->GetColSpan();
      nsColLayoutData * colData = (nsColLayoutData *)(mColumnLayoutData->ElementAt(firstColIndex));
      nsVoidArray *col = colData->GetCells();
      if (gsDebugCLD) printf ("     ~ SetCellLayoutData with row = %d, firstCol = %d, colSpan = %d, colData = %ld, col=%ld\n", 
                           rowIndex, firstColIndex, colSpan, colData, col);
      /* this logic looks wrong wrong wrong
         it seems to add an entries in col (the array of cells for a column) for aCell
         based on colspan.  This is weird, because you would expect one entry in each
         column spanned for aCell, not multiple entries in the same col.
       */
      for (PRInt32 i=0; i<colSpan; i++)
      {
        nsSize * cellSize = aData->GetMaxElementSize();
        nsSize partialCellSize(*cellSize);
        partialCellSize.width = (cellSize->width)/colSpan;
        // This method will copy the nsReflowMetrics pointed at by aData->GetDesiredSize()
        nsCellLayoutData * kidLayoutData = new nsCellLayoutData(aData->GetCellFrame(),
                                                                aData->GetDesiredSize(),
                                                                &partialCellSize);
        NS_ASSERTION(col->Count() > rowIndex, "unexpected count");
        if (gsDebugCLD) printf ("     ~ replacing rowIndex = %d\n", rowIndex);
        nsCellLayoutData* data = (nsCellLayoutData*)col->ElementAt(rowIndex);
        col->ReplaceElementAt((void *)kidLayoutData, rowIndex);
        if (data != nsnull) {
          delete data;
        }
      }
    }
    else
      result = PR_FALSE;
  }

  return result;
}

/** Get the layout data associated with the cell at (aRow,aCol)
  * @return nsnull if there was an error, such as aRow or aCol being invalid
  *         otherwise, the data is returned.
  */
nsCellLayoutData * nsTableFrame::GetCellLayoutData(nsTableCellFrame *aCell)
{
  NS_ASSERTION(nsnull != aCell, "bad arg");

  nsTableFrame * firstInFlow = (nsTableFrame *)GetFirstInFlow();
  NS_ASSERTION(nsnull!=firstInFlow, "illegal state -- no first in flow");
  nsCellLayoutData *result = nsnull;
  if (this!=firstInFlow)
    result = firstInFlow->GetCellLayoutData(aCell);
  else
  {
    if (nsnull!=mColumnLayoutData)
    {
      PRInt32 firstColIndex = aCell->GetColIndex();
      nsColLayoutData * colData = (nsColLayoutData *)(mColumnLayoutData->ElementAt(firstColIndex));
      nsTableRowFrame *rowFrame;
      aCell->GetGeometricParent((nsIFrame *&)rowFrame);
      PRInt32 rowIndex = rowFrame->GetRowIndex();
      result = colData->ElementAt(rowIndex);

#ifdef NS_DEBUG
      // Do some sanity checking
      if (nsnull != result) {
        nsIContent* inputContent;
        nsIContent* resultContent;
        result->GetCellFrame()->GetContent(resultContent);
        aCell->GetContent(inputContent);
        NS_ASSERTION(resultContent == inputContent, "unexpected cell");
        NS_IF_RELEASE(inputContent);
        NS_IF_RELEASE(resultContent);
      }
#endif

    }
  }
  return result;
}



PRInt32 nsTableFrame::GetReflowPass() const
{
  nsTableFrame * firstInFlow = (nsTableFrame *)GetFirstInFlow();
  NS_ASSERTION(nsnull!=firstInFlow, "illegal state -- no first in flow");
  return firstInFlow->mPass;
}

void nsTableFrame::SetReflowPass(PRInt32 aReflowPass)
{
  nsTableFrame * firstInFlow = (nsTableFrame *)GetFirstInFlow();
  NS_ASSERTION(nsnull!=firstInFlow, "illegal state -- no first in flow");
  firstInFlow->mPass = aReflowPass;
}

PRBool nsTableFrame::IsFirstPassValid() const
{
  nsTableFrame * firstInFlow = (nsTableFrame *)GetFirstInFlow();
  NS_ASSERTION(nsnull!=firstInFlow, "illegal state -- no first in flow");
  return firstInFlow->mFirstPassValid;
}

NS_METHOD
nsTableFrame::CreateContinuingFrame(nsIPresContext*  aPresContext,
                                    nsIFrame*        aParent,
                                    nsIStyleContext* aStyleContext,
                                    nsIFrame*&       aContinuingFrame)
{
  nsTableFrame* cf = new nsTableFrame(mContent, aParent);
  if (nsnull == cf) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  PrepareContinuingFrame(aPresContext, aParent, aStyleContext, cf);
  if (PR_TRUE==gsDebug) printf("nsTableFrame::CCF parent = %p, this=%p, cf=%p\n", aParent, this, cf);
  // set my width, because all frames in a table flow are the same width
  // code in nsTableOuterFrame depends on this being set
  cf->SetRect(nsRect(0, 0, mRect.width, 0));
  // add headers and footers to cf
  nsTableFrame * firstInFlow = (nsTableFrame *)GetFirstInFlow();
  nsIFrame * rg = nsnull;
  firstInFlow->ChildAt(0, rg);
  NS_ASSERTION (nsnull!=rg, "previous frame has no children");
  nsIAtom * tHeadTag = NS_NewAtom(nsTablePart::kRowGroupHeadTagString);  // tHeadTag: REFCNT++
  nsIAtom * tFootTag = NS_NewAtom(nsTablePart::kRowGroupFootTagString);  // tFootTag: REFCNT++
  PRInt32 index = 0;
  nsIFrame * bodyRowGroupFromOverflow = mOverflowList;
  nsIFrame * lastSib = nsnull;
  for ( ; nsnull!=rg; index++)
  {
    nsIContent *content = nsnull;
    rg->GetContent(content);                                              // content: REFCNT++
    NS_ASSERTION(nsnull!=content, "bad frame, returned null content.");
    nsIAtom * rgTag = content->GetTag();
    // if we've found a header or a footer, replicate it
    if (tHeadTag==rgTag || tFootTag==rgTag)
    {
      printf("found a head or foot in continuing frame\n");
      // Resolve style for the child
      nsIStyleContext* kidStyleContext =
        aPresContext->ResolveStyleContextFor(content, cf);               // kidStyleContext: REFCNT++
      nsIContentDelegate* kidDel = nsnull;
      kidDel = content->GetDelegate(aPresContext);                       // kidDel: REFCNT++
      nsIFrame* duplicateFrame;
      nsresult rv = kidDel->CreateFrame(aPresContext, content, cf,
                                        kidStyleContext, duplicateFrame);
      NS_RELEASE(kidDel);                                                // kidDel: REFCNT--
      NS_RELEASE(kidStyleContext);                                       // kidStyleContenxt: REFCNT--
      
      if (nsnull==lastSib)
      {
        mOverflowList = duplicateFrame;
      }
      else
      {
        lastSib->SetNextSibling(duplicateFrame);
      }
      duplicateFrame->SetNextSibling(bodyRowGroupFromOverflow);
      lastSib = duplicateFrame;
    }
    NS_RELEASE(content);                                                 // content: REFCNT--
    // get the next row group
    rg->GetNextSibling(rg);
  }
  NS_RELEASE(tFootTag);                                                  // tHeadTag: REFCNT --
  NS_RELEASE(tHeadTag);                                                  // tFootTag: REFCNT --
  aContinuingFrame = cf;
  return NS_OK;
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
#ifdef DEBUG
    nsVoidArray *cld = GetColumnLayoutData();
    NS_ASSERTION(nsnull!=cld, "no column layout data");
    PRInt32 numCols = cld->Count();
    NS_ASSERTION (numCols > aColIndex, "bad arg, col index out of bounds");
#endif
    if (nsnull!=mColumnWidths)
     result = mColumnWidths[aColIndex];
  }

  //printf("GET_COL_WIDTH: %p, FIF=%p getting col %d and returning %d\n", this, firstInFlow, aColIndex, result);

  // XXX hack
#if 0
  if (result <= 0) {
    result = 100;
  }
#endif
  return result;
}

void  nsTableFrame::SetColumnWidth(PRInt32 aColIndex, nscoord aWidth)
{
  nsTableFrame * firstInFlow = (nsTableFrame *)GetFirstInFlow();
  NS_ASSERTION(nsnull!=firstInFlow, "illegal state -- no first in flow");
  //printf("SET_COL_WIDTH: %p, FIF=%p setting col %d to %d\n", this, firstInFlow, aColIndex, aWidth);
  if (this!=firstInFlow)
    firstInFlow->SetColumnWidth(aColIndex, aWidth);
  else
  {
    NS_ASSERTION(nsnull!=mColumnWidths, "illegal state");
    if (nsnull!=mColumnWidths)
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

  aSpacingStyle.mBorderStyle[NS_SIDE_TOP] = NS_STYLE_BORDER_STYLE_OUTSET; 
  aSpacingStyle.mBorderStyle[NS_SIDE_LEFT] = NS_STYLE_BORDER_STYLE_OUTSET; 
  aSpacingStyle.mBorderStyle[NS_SIDE_BOTTOM] = NS_STYLE_BORDER_STYLE_OUTSET; 
  aSpacingStyle.mBorderStyle[NS_SIDE_RIGHT] = NS_STYLE_BORDER_STYLE_OUTSET; 
  

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

  aSpacingStyle.mBorderColor[NS_SIDE_TOP]     = borderColor;
  aSpacingStyle.mBorderColor[NS_SIDE_LEFT]    = borderColor;
  aSpacingStyle.mBorderColor[NS_SIDE_BOTTOM]  = borderColor;
  aSpacingStyle.mBorderColor[NS_SIDE_RIGHT]   = borderColor;

}



PRBool nsTableFrame::ConvertToPixelValue(nsHTMLValue& aValue, PRInt32 aDefault, PRInt32& aResult)
{
  PRInt32 result = 0;

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
  // Check to see if the table has either cell padding or 
  // Cell spacing defined for the table. If true, then
  // this setting overrides any specific border, margin or 
  // padding information in the cell. If these attributes
  // are not defined, the the cells attributes are used
  
  nsHTMLValue padding_value;
  nsHTMLValue spacing_value;
  nsHTMLValue border_value;


  nsContentAttr border_result;

  nscoord   padding = 0;
  nscoord   spacing = 0;
  nscoord   border  = 1;

  float     p2t = aPresContext->GetPixelsToTwips();

  nsTablePart*  table = (nsTablePart*)mContent;

  NS_ASSERTION(table,"Table Must not be null");
  if (!table)
    return;

  nsStyleSpacing* spacingData = (nsStyleSpacing*)mStyleContext->GetMutableStyleData(eStyleStruct_Spacing);

  border_result = table->GetAttribute(nsHTMLAtoms::border,border_value);
  if (border_result == eContentAttr_HasValue)
  {
    PRInt32 intValue = 0;

    if (ConvertToPixelValue(border_value,1,intValue))
      border = nscoord(p2t*(float)intValue); 
  }
  MapHTMLBorderStyle(*spacingData,border);
}




// Subclass hook for style post processing
NS_METHOD nsTableFrame::DidSetStyleContext(nsIPresContext* aPresContext)
{
#ifdef NOISY_STYLE
  printf("nsTableFrame::DidSetStyleContext \n");
#endif
  MapBorderMarginPadding(aPresContext);
  return NS_OK;
}

NS_METHOD nsTableFrame::GetCellMarginData(nsTableCellFrame* aKidFrame, nsMargin& aMargin)
{
  nsresult result = NS_ERROR_NOT_INITIALIZED;

  if (nsnull != aKidFrame)
  {
    nsCellLayoutData* layoutData = GetCellLayoutData(aKidFrame);
    if (layoutData)
      result = layoutData->GetMargin(aMargin);
  }

  return result;
}

void nsTableFrame::GetColumnsByType(const nsStyleUnit aType, 
                                    PRInt32& aOutNumColumns,
                                    PRInt32 *& aOutColumnIndexes)
{
  mColCache->GetColumnsByType(aType, aOutNumColumns, aOutColumnIndexes);
}


NS_METHOD
nsTableFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_PRECONDITION(0 != aInstancePtr, "null ptr");
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kTableFrameCID)) {
    *aInstancePtr = (void*) (this);
    return NS_OK;
  }
  return nsContainerFrame::QueryInterface(aIID, aInstancePtr);
}


/* ---------- static methods ---------- */

nsresult nsTableFrame::NewFrame(nsIFrame** aInstancePtrResult,
                                nsIContent* aContent,
                                nsIFrame*   aParent)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIFrame* it = new nsTableFrame(aContent, aParent);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aInstancePtrResult = it;
  return NS_OK;
}

/* helper method for getting the width of the table's containing block */
nscoord nsTableFrame::GetTableContainerWidth(const nsReflowState& aReflowState)
{
//STEVES_WAY is out of synch, because it doesn't handle nested table case.
#ifdef STEVES_WAY // from BasicTableLayoutStrategy::TableIsAutoWidth()
  // get the parent's width (available only from parent frames that claim they can provide it)
  // note that we start with our parent's parent (the outer table frame's parent)
  nscoord parentWidth = 0;
  NS_ASSERTION(nsnull!=aReflowState.parentReflowState, "bad outer table reflow state.");
  NS_ASSERTION(nsnull!=aReflowState.parentReflowState->parentReflowState, "bad table parent reflow state.");
  if ((nsnull!=aReflowState.parentReflowState) &&
      (nsnull!=aReflowState.parentReflowState->parentReflowState))
  {
    const nsReflowState *parentReflowState = aReflowState.parentReflowState->parentReflowState;
    nsIFrame *parentFrame=parentReflowState->frame;
    NS_ASSERTION(nsnull!=parentFrame, "bad parent frame in reflow state struct.");
    while(nsnull!=parentFrame)
    {
      PRBool isPercentageBase=PR_FALSE;
      parentFrame->IsPercentageBase(isPercentageBase);
      if (PR_TRUE==isPercentageBase)
      { // found the ancestor who claims to be the container to base my percentage width on
        parentWidth = parentReflowState->maxSize.width;
        if (PR_TRUE==gsDebug) printf("  ** width for parent frame %p = %d\n", parentFrame, parentWidth);
        break;
      }
      parentReflowState = parentReflowState->parentReflowState; // get next ancestor
      if (nsnull!=parentReflowState)
        parentFrame = parentReflowState->frame;
      else
        parentFrame = nsnull; // terminates loop.  
      // TODO: do we need a backstop in case there are no IsPercentageBase==true frames?
    }
  }

#else

  nscoord parentWidth = aReflowState.maxSize.width;

  // Walk up the reflow state chain until we find a block
  // frame. Our width is computed relative to there.
  const nsReflowState* rs = &aReflowState;
  nsIFrame *childFrame=nsnull;
  nsIFrame *grandchildFrame=nsnull;
  nsIFrame *greatgrandchildFrame=nsnull;
  while (nsnull != rs) 
  {
    // if it's a block, use its max width
    nsIFrame* block = nsnull;
    rs->frame->QueryInterface(kBlockFrameCID, (void**) &block);
    if (nsnull != block) 
    { // we found a block, see if it's really a table cell (which means we're a nested table)
      PRBool skipThisBlock=PR_FALSE;
      if (PR_TRUE==((nsContainerFrame*)block)->IsPseudoFrame())
      {
        const nsReflowState* parentRS = rs->parentReflowState;
        if (nsnull!=parentRS)
        {
          parentRS = parentRS->parentReflowState;
          if (nsnull!=parentRS)
          {
            nsIFrame* cell = nsnull;
            parentRS->frame->QueryInterface(kTableCellFrameCID, (void**) &cell);
            if (nsnull != cell) {
              if (PR_TRUE==gsDebugNT)
                printf("%p: found a block pframe %p in a cell, skipping it.\n", aReflowState.frame, block);
              skipThisBlock = PR_TRUE;
            }
          }
        }
      }
      // at this point, we know we have a block.  If we're sure it's not a table cell pframe,
      // then we can use it
      if (PR_FALSE==skipThisBlock)
      {
        parentWidth = rs->maxSize.width;
        if (PR_TRUE==gsDebugNT)
          printf("%p: found a block frame %p, returning width %d\n", 
                 aReflowState.frame, block, parentWidth);
        break;
      }
    }
    // or if it's another table (we're nested) use its computed width
    if (rs->frame!=aReflowState.frame)
    {
      nsIFrame* table = nsnull;
      rs->frame->QueryInterface(kTableFrameCID, (void**) &table);
      if (nsnull != table) {
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
         nsMargin borderPadding;
        /* the following hack is because the outer table really holds the position info */
        // begin REMOVE_ME_WHEN_TABLE_STYLE_IS_RESOLVED!
        nsIFrame * outerTableFrame = nsnull;
        table->GetGeometricParent(outerTableFrame);
        const nsStylePosition* tablePosition;
        outerTableFrame->GetStyleData(eStyleStruct_Position, ((nsStyleStruct *&)tablePosition));
        const nsStyleSpacing* spacing;
        outerTableFrame->GetStyleData(eStyleStruct_Spacing, (const nsStyleStruct *&)spacing);
        // end REMOVE_ME_WHEN_TABLE_STYLE_IS_RESOLVED!

        if (eStyleUnit_Auto == tablePosition->mWidth.GetUnit())
        {
          parentWidth = 0;
          if (nsnull != ((nsTableFrame*)table)->mColumnWidths)
          {
            PRInt32 colIndex = ((nsTableCellFrame *)greatgrandchildFrame)->GetColIndex();
            PRInt32 colSpan = ((nsTableCellFrame *)greatgrandchildFrame)->GetColSpan();
            for (PRInt32 i = 0; i<colSpan; i++)
              parentWidth += ((nsTableFrame*)table)->GetColumnWidth(i+colIndex);
            // subtract out cell border and padding
            greatgrandchildFrame->GetStyleData(eStyleStruct_Spacing, (const nsStyleStruct *&)spacing);
            spacing->CalcBorderPaddingFor(greatgrandchildFrame, borderPadding);
            parentWidth -= (borderPadding.right + borderPadding.left);
            if (PR_TRUE==gsDebugNT)
              printf("%p: found a table frame %p with auto width, returning parentWidth %d from cell in col %d with span %d\n", 
                     aReflowState.frame, table, parentWidth, colIndex, colSpan);
          }
          else
          {
            if (PR_TRUE==gsDebugNT)
              printf("%p: found a table frame %p with auto width, returning parentWidth %d because parent has no info yet.\n", 
                     aReflowState.frame, table, parentWidth);
          }

        }
        else
        {
          nsSize tableSize;
          table->GetSize(tableSize);
          parentWidth = tableSize.width;
          spacing->CalcBorderPaddingFor(rs->frame, borderPadding);
          parentWidth -= (borderPadding.right + borderPadding.left);
          // same for the row group
          childFrame->GetStyleData(eStyleStruct_Spacing, (const nsStyleStruct *&)spacing);
          spacing->CalcBorderPaddingFor(childFrame, borderPadding);
          parentWidth -= (borderPadding.right + borderPadding.left);
          // same for the row
          grandchildFrame->GetStyleData(eStyleStruct_Spacing, (const nsStyleStruct *&)spacing);
          spacing->CalcBorderPaddingFor(grandchildFrame, borderPadding);
          parentWidth -= (borderPadding.right + borderPadding.left);
          // same for the cell
          greatgrandchildFrame->GetStyleData(eStyleStruct_Spacing, (const nsStyleStruct *&)spacing);
          spacing->CalcBorderPaddingFor(greatgrandchildFrame, borderPadding);
          parentWidth -= (borderPadding.right + borderPadding.left);

          if (PR_TRUE==gsDebugNT)
            printf("%p: found a table frame %p, returning parentWidth %d from frame width %d\n", 
                   aReflowState.frame, table, parentWidth, tableSize.width);
        }
        break;
      }
    }

    if (nsnull==childFrame)
      childFrame = rs->frame;
    else if (nsnull==grandchildFrame)
    {
      grandchildFrame = childFrame;
      childFrame = rs->frame;
    }
    else
    {
      greatgrandchildFrame = grandchildFrame;
      grandchildFrame = childFrame;
      childFrame = rs->frame;
    }
    rs = rs->parentReflowState;
  }

#endif
  return parentWidth;
}

// aSpecifiedTableWidth is filled if the table witdth is not auto
PRBool nsTableFrame::TableIsAutoWidth(nsTableFrame *aTableFrame,
                                      nsIStyleContext *aTableStyle, 
                                      const nsReflowState& aReflowState,
                                      nscoord& aSpecifiedTableWidth)
{
  NS_ASSERTION(nsnull!=aTableStyle, "bad arg - aTableStyle");
  PRBool result = PR_TRUE;  // the default
  if (nsnull!=aTableStyle)
  {
    //nsStylePosition* tablePosition = (nsStylePosition*)aTableStyle->GetData(eStyleStruct_Position);
    /* this is sick and wrong, but what the hell
       we grab the style of our parent (nsTableOuterFrame) and ask it for width info, 
       until the style resolution stuff does the cool stuff about splitting style between outer and inner
     */
    // begin REMOVE_ME_WHEN_TABLE_STYLE_IS_RESOLVED!
    nsIFrame * parent = nsnull;
    aTableFrame->GetGeometricParent(parent);
    const nsStylePosition* tablePosition;
    parent->GetStyleData(eStyleStruct_Position, ((nsStyleStruct *&)tablePosition));
    // end REMOVE_ME_WHEN_TABLE_STYLE_IS_RESOLVED!
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
      // XXX: subtract out this table frame's borderpadding?
      aSpecifiedTableWidth = tablePosition->mWidth.GetCoordValue();
      aReflowState.frame->GetStyleData(eStyleStruct_Spacing, (const nsStyleStruct *&)spacing);
      spacing->CalcBorderPaddingFor(aReflowState.frame, borderPadding);
      aSpecifiedTableWidth -= (borderPadding.right + borderPadding.left);
      result = PR_FALSE;
      break;

    case eStyleUnit_Percent:
      // set aSpecifiedTableWidth to be the given percent of the parent.
      // first, get the effective parent width (parent width - insets)
      nscoord parentWidth = nsTableFrame::GetTableContainerWidth(aReflowState);
      aReflowState.frame->GetStyleData(eStyleStruct_Spacing, (const nsStyleStruct *&)spacing);
      spacing->CalcBorderPaddingFor(aReflowState.frame, borderPadding);
      parentWidth -= (borderPadding.right + borderPadding.left);

      // then set aSpecifiedTableWidth to the given percent of the computed parent width
      float percent = tablePosition->mWidth.GetPercentValue();
      aSpecifiedTableWidth = (PRInt32)(parentWidth*percent);
      if (PR_TRUE==gsDebug || PR_TRUE==gsDebugNT) 
        printf("%p: TableIsAutoWidth setting aSpecifiedTableWidth = %d with parentWidth = %d and percent = %f\n", 
               aTableFrame, aSpecifiedTableWidth, parentWidth, percent);
      result = PR_FALSE;
      break;
    }
  }

  return result; 
}

/* valuable code not yet used anywhere

    // since the table is a specified width, we need to expand the columns to fill the table
    nsVoidArray *columnLayoutData = GetColumnLayoutData();
    PRInt32 numCols = columnLayoutData->Count();
    PRInt32 spaceUsed = 0;
    for (PRInt32 colIndex = 0; colIndex<numCols; colIndex++)
      spaceUsed += mColumnWidths[colIndex];
    PRInt32 spaceRemaining = spaceUsed - aMaxWidth;
    PRInt32 additionalSpaceAdded = 0;
    if (0<spaceRemaining)
    {
      for (colIndex = 0; colIndex<numCols; colIndex++)
      {
        nsColLayoutData * colData = (nsColLayoutData *)(columnLayoutData->ElementAt(colIndex));
        nsTableColPtr col = colData->GetCol();  // col: ADDREF++
        nsStyleMolecule* colStyle =
          (nsStyleMolecule*)mStyleContext->GetData(eStyleStruct_Molecule);
        if (PR_TRUE==IsProportionalWidth(colStyle))
        {
          PRInt32 percentage = (100*mColumnWidths[colIndex]) / aMaxWidth;
          PRInt32 additionalSpace = (spaceRemaining*percentage)/100;
          mColumnWidths[colIndex] += additionalSpace;
          additionalSpaceAdded += additionalSpace;
        }
      }
      if (spaceUsed+additionalSpaceAdded < aMaxTableWidth)
        mColumnWidths[numCols-1] += (aMaxTableWidth - (spaceUsed+additionalSpaceAdded));
    }
*/

// For Debugging ONLY
NS_METHOD nsTableFrame::MoveTo(nscoord aX, nscoord aY)
{
  if ((aX != mRect.x) || (aY != mRect.y)) {
    mRect.x = aX;
    mRect.y = aY;

    nsIView* view;
    GetView(view);

    // Let the view know
    if (nsnull != view) {
      // Position view relative to it's parent, not relative to our
      // parent frame (our parent frame may not have a view).
      nsIView* parentWithView;
      nsPoint origin;
      GetOffsetFromView(origin, parentWithView);
      view->SetPosition(origin.x, origin.y);
      NS_IF_RELEASE(parentWithView);
    }
  }

  return NS_OK;
}

NS_METHOD nsTableFrame::SizeTo(nscoord aWidth, nscoord aHeight)
{
  mRect.width = aWidth;
  mRect.height = aHeight;

  nsIView* view;
  GetView(view);

  // Let the view know
  if (nsnull != view) {
    view->SetDimensions(aWidth, aHeight);
  }
  return NS_OK;
}

