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

#include "BasicTableLayoutStrategy.h"
#include "nsTableFrame.h"
#include "nsTableColFrame.h"
#include "nsTableCellFrame.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsVoidArray.h"
#include "nsHTMLIIDs.h"

static const PRBool gsDebug = PR_FALSE;

const nscoord gBigSpace = 100000; // a fudge constant used when the table is laid out with unconstrained width

// these macros are defined to improve the readability of the main code
#define TDBG_S(str) \
  if (gsDebug) { \
    printf((str)); \
  }

#define TDBG_SP(str,ptr) \
  if (gsDebug) { \
    printf((str),(ptr)); \
  }

#define TDBG_SPD(str,ptr,dec) \
  if (gsDebug) { \
    printf((str),(ptr),(dec)); \
  }

#define TDBG_SPDD(str,ptr,dec1,dec2) \
  if (gsDebug) { \
    printf((str),(ptr),(dec1),(dec2)); \
  }

#define TDBG_SFDD(str,flt,dec1,dec2) \
  if (gsDebug) { \
    printf((str),(flt),(dec1),(dec2)); \
  }

#define TDBG_SDF(str,dec,flt) \
  if (gsDebug) { \
    printf((str),(dec),(flt)); \
  }

#define TDBG_SDFDDD(str,dec1,flt,dec2,dec3,dec4) \
  if (gsDebug) { \
    printf((str),(dec1),(flt),(dec2),(dec3),(dec4)); \
  }

#define TDBG_SD(str,dec) \
  if (gsDebug) { \
    printf((str),(dec)); \
  }

#define TDBG_SDD(str,dec1,dec2) \
  if (gsDebug) { \
    printf((str),(dec1),(dec2)); \
  }

#define TDBG_SDDD(str,dec1,dec2,dec3) \
  if (gsDebug) { \
    printf((str),(dec1),(dec2),(dec3)); \
  }

#define TDBG_SDFD(str,dec1,flt,dec2) \
  if (gsDebug) { \
    printf((str),(dec1),(flt),(dec2)); \
  }

#define TDBG_SDDF(str,dec1,dec2,flt) \
  if (gsDebug) { \
    printf((str),(dec1),(dec2),(flt)); \
  }

#define TDBG_SDDDD(str,dec1,dec2,dec3,dec4) \
  if (gsDebug) { \
    printf((str),(dec1),(dec2),(dec3),(dec4)); \
  }

#define TDBG_SDDDF(str,dec1,dec2,dec3,flt) \
  if (gsDebug) { \
    printf((str),(dec1),(dec2),(dec3),(flt)); \
  }

#define TDBG_SDDDDD(str,dec1,dec2,dec3,dec4,dec5) \
  if (gsDebug) { \
    printf((str),(dec1),(dec2),(dec3),(dec4),(dec5)); \
  }

#define TDBG_SDDDDDD(str,dec1,dec2,dec3,dec4,dec5,dec6) \
  if (gsDebug) { \
    printf((str),(dec1),(dec2),(dec3),(dec4),(dec5),(dec6)); \
  }

#define TDBG_SDD_SDD(str1,dec1a,dec1b,str2,dec2a,dec2b) \
  if (gsDebug) { \
    printf((str1),(dec1a),(dec1b)); \
    printf((str2),(dec2a),(dec2b)); \
  }

#define TDBG_SDS_SDD_SD(str1a,dec1,str1b,str2,dec2a,dec2b,str3,dec3) \
  if (gsDebug) { \
    printf((str1a),(dec1),(str1b)); \
    printf((str2),(dec2a),(dec2b)); \
    printf((str3),(dec3)); \
  }

#define TDBG_SDS_SD(str1a,dec1,str1b,str2,dec2) \
  if (gsDebug) { \
    printf((str1a),(dec1),(str1b)); \
    printf((str2),(dec2)); \
  }

#define TDBG_WIDTHS1() \
  if (PR_TRUE == gsDebug) { \
    printf ("BalanceColumnWidths with aMaxWidth = %d, availWidth = %d\n", aMaxWidth, availWidth); \
    printf ("\t\t specifiedTW = %d, min/maxTW = %d %d\n", specifiedTableWidth, mMinTableWidth, mMaxTableWidth); \
    printf ("\t\t reflow reason = %d\n", aReflowState.reason); \
  } 

#define TDBG_WIDTHS2(string) \
  if (PR_TRUE == gsDebug) { \
    printf("\n%p: %s\n", (string), mTableFrame); \
    for (PRInt32 i=0; i<mNumCols; i++) { \
      printf("  col %d assigned width %d\n", i, mTableFrame->GetColumnWidth(i)); \
    } \
    printf("\n"); \
  } 

#define TDBG_WIDTHS4(string,haveShrinkFit,shrinkFit) \
  if (PR_TRUE == gsDebug) { \
    nscoord tableWidth = 0; \
    printf((string)); \
    for (PRInt32 i = 0; i < mNumCols; i++) { \
      tableWidth += mTableFrame->GetColumnWidth(i); \
      printf(" %d ", mTableFrame->GetColumnWidth(i)); \
    } \
    printf ("\n  computed table width is %d",tableWidth); \
    if ((haveShrinkFit)) { \
      printf(" with aShrinkFixedCols = %s", shrinkFit ? "TRUE" : "FALSE"); \
    } \
    printf("\n"); \
  }

/* ---------- ProportionalColumnLayoutStruct ---------- */  
// TODO: make public so other subclasses can use it

/** useful info about a column for layout of tables with colspans */
struct ProportionalColumnLayoutStruct
{
  ProportionalColumnLayoutStruct(PRInt32 aColIndex, 
                                 nscoord aMinColWidth, 
                                 nscoord aMaxColWidth, 
                                 PRInt32 aProportion)
  {
    mColIndex    = aColIndex;
    mMinColWidth = aMinColWidth;
    mMaxColWidth = aMaxColWidth;
    mProportion  = aProportion;
  };

  PRInt32 mColIndex;
  nscoord mMinColWidth; 
  nscoord mMaxColWidth; 
  PRInt32 mProportion;
};


/* ---------- ColSpanStruct ---------- */
/** useful info about a column for layout of tables with colspans */
struct ColSpanStruct
{
  PRInt32 colIndex;
  PRInt32 colSpan;
  nscoord width;

  ColSpanStruct(PRInt32 aSpan, PRInt32 aIndex, nscoord aWidth)
  {
    colSpan  = aSpan;
    colIndex = aIndex;
    width    = aWidth;
  }
};




/* ---------- BasicTableLayoutStrategy ---------- */

/* return true if the style indicates that the width is fixed 
 * for the purposes of column width determination
 */
inline
PRBool BasicTableLayoutStrategy::IsFixedWidth(const nsStylePosition* aStylePosition,
                                              const nsStyleTable*    aStyleTable)
{
  return PRBool ((eStyleUnit_Coord==aStylePosition->mWidth.GetUnit()) ||
                 (eStyleUnit_Coord==aStyleTable->mSpanWidth.GetUnit()));
}


BasicTableLayoutStrategy::BasicTableLayoutStrategy(nsTableFrame *aFrame)
{
  NS_ASSERTION(nsnull != aFrame, "bad frame arg");

  mTableFrame      = aFrame;
  mMinTableWidth   = 0;
  mMaxTableWidth   = 0;
  mFixedTableWidth = 0;
}

BasicTableLayoutStrategy::~BasicTableLayoutStrategy()
{
}

PRBool BasicTableLayoutStrategy::Initialize(nsSize* aMaxElementSize, 
                                            PRInt32 aNumCols)
{
#ifdef NS_DEBUG 
  nsIFrame* tablePIF = nsnull; 
  mTableFrame->GetPrevInFlow(&tablePIF); 
  NS_ASSERTION(nsnull == tablePIF, "never ever call me on a continuing frame!"); 
#endif

  PRBool result = PR_TRUE;

  // re-init instance variables
  mNumCols         = aNumCols;
  mMinTableWidth   = 0;
  mMaxTableWidth   = 0;
  mFixedTableWidth = 0;
  mCols            = mTableFrame->GetEffectiveCOLSAttribute();

  // Step 1 - assign the width of all fixed-width columns
  AssignPreliminaryColumnWidths();

  // set aMaxElementSize here because we compute mMinTableWidth in AssignPreliminaryColumnWidths
  if (nsnull != aMaxElementSize) {
    SetMaxElementSize(aMaxElementSize);
  }

  return result;
}

void BasicTableLayoutStrategy::SetMaxElementSize(nsSize* aMaxElementSize)
{
  if (nsnull != aMaxElementSize) {
    aMaxElementSize->height = 0;
    nsMargin borderPadding;
    const nsStylePosition* tablePosition;
    const nsStyleSpacing* tableSpacing;
    mTableFrame->GetStyleData(eStyleStruct_Position, ((const nsStyleStruct *&)tablePosition));
    mTableFrame->GetStyleData(eStyleStruct_Spacing , ((const nsStyleStruct *&)tableSpacing));
    mTableFrame->GetTableBorder(borderPadding);
    nsMargin padding;
    tableSpacing->GetPadding(padding);
    borderPadding += padding;
    if (tablePosition->mWidth.GetUnit() == eStyleUnit_Coord) {
      aMaxElementSize->width = tablePosition->mWidth.GetCoordValue();
      aMaxElementSize->width = PR_MAX(aMaxElementSize->width, mMinTableWidth);
      //XXX: need to factor in borderpadding here!
    }   
    else {
      aMaxElementSize->width = mMinTableWidth + borderPadding.left + borderPadding.right;
    }
    TDBG_SPD("%p BTLS::Init setting aMaxElementSize->width = %d\n", mTableFrame, aMaxElementSize->width);
  }
}

PRBool BasicTableLayoutStrategy::BalanceColumnWidths(nsIStyleContext*         aTableStyle,
                                                     const nsHTMLReflowState& aReflowState,
                                                     nscoord                  aMaxWidth)
{
#ifdef NS_DEBUG
  nsIFrame *tablePIF = nsnull;
  mTableFrame->GetPrevInFlow(&tablePIF);
  NS_ASSERTION(nsnull==tablePIF, "never ever call me on a continuing frame!");
#endif

  PRBool result;

  NS_ASSERTION(nsnull != aTableStyle, "bad arg");
  if (nsnull==aTableStyle) {
    return PR_FALSE;
  }

  nscoord specifiedTableWidth = 0; // not cached as a data member because it can vary depending on aMaxWidth
  PRBool tableIsAutoWidth = nsTableFrame::TableIsAutoWidth(mTableFrame, aTableStyle, aReflowState, specifiedTableWidth);
  // HACK!  Fix TableIsAutoWidth to return the right width
  specifiedTableWidth = PR_MIN(specifiedTableWidth,aMaxWidth);
  if (NS_UNCONSTRAINEDSIZE == specifiedTableWidth) {
    specifiedTableWidth = 0;
    tableIsAutoWidth    = PR_TRUE;
  }

  // Step 2 - determine how much space is really available
  nscoord availWidth = aMaxWidth;                         // start with the max width I've been given
  if (NS_UNCONSTRAINEDSIZE != availWidth) {               // if that's not infinite, subtract the fixed columns
    availWidth -= mFixedTableWidth; 
  }                                                       // that have already been accounted for
  if (PR_FALSE == tableIsAutoWidth) {                     // if the table has a specified width
    availWidth = specifiedTableWidth - mFixedTableWidth;  // use it, minus the fixed columns already accounted for
  }
  //if (0!=mMinTableWidth && mMinTableWidth>availWidth) // if the computed available size is too small
  //availWidth = mMinTableWidth;                        // bump it up to the min
  availWidth = PR_MAX(0,availWidth); // avail width can never be negative

  // Step 3 - assign the width of all proportional-width columns in the remaining space
  TDBG_WIDTHS1();
  TDBG_WIDTHS2("BEGIN BALANCE COLUMN WIDTHS\n");

  result = BalanceProportionalColumns(aReflowState, availWidth, aMaxWidth,
                                      specifiedTableWidth, tableIsAutoWidth);

  TDBG_WIDTHS2("END BALANCE COLUMN WIDTHS\n");

#ifdef NS_DEBUG   // sanity check for table column widths
  for (PRInt32 i=0; i<mNumCols; i++) {
    nsTableColFrame *colFrame;
    mTableFrame->GetColumnFrame(i, colFrame);
    nscoord minColWidth = colFrame->GetMinColWidth();
    nscoord assignedColWidth = mTableFrame->GetColumnWidth(i);
    NS_ASSERTION(assignedColWidth >= minColWidth, "illegal width assignment");
  }
#endif

  return result;
}

// Step 1 - assign the width of all fixed-width columns, all other columns get there max, 
//          and calculate min/max table width
PRBool BasicTableLayoutStrategy::AssignPreliminaryColumnWidths()
{
  TDBG_SP("** %p: AssignPreliminaryColumnWidths **\n", mTableFrame);
  nsVoidArray *spanList    = nsnull;
  nsVoidArray *colSpanList = nsnull;

  PRBool hasColsAttribute = (PRBool)(NS_STYLE_TABLE_COLS_NONE != mCols);
  
  PRInt32 *minColWidthArray = nsnull;   // used for computing the effect of COLS attribute
  PRInt32 *maxColWidthArray = nsnull;   // used for computing the effect of COLS attribute
  if (PR_TRUE == hasColsAttribute) { 
    minColWidthArray = new PRInt32[mNumCols];
    maxColWidthArray = new PRInt32[mNumCols];
  }

  nscoord cellPadding = mTableFrame->GetCellPadding();
  TDBG_SD("table cell padding = %d\n", cellPadding);

  PRInt32 numRows = mTableFrame->GetRowCount();
  PRInt32 colIndex, rowIndex;

  // for every column, determine it's min and max width, and keep track of the table width
  for (colIndex = 0; colIndex < mNumCols; colIndex++) { 
    nscoord minColWidth             = 0;  // min col width factoring in table attributes
    nscoord minColContentWidth      = 0;  // min width of the col's contents, does not take into account table attributes
    nscoord maxColWidth             = 0;  // max col width factoring in table attributes
    nscoord effectiveMinColumnWidth = 0;  // min col width ignoring cells with colspans
    nscoord effectiveMaxColumnWidth = 0;  // max col width ignoring cells with colspans
    nscoord specifiedFixedColWidth  = 0;  // the width of the column if given stylistically (or via cell Width attribute)
                                          // only applicable if haveColWidth==PR_TRUE                                           
    PRBool haveColWidth = PR_FALSE;       // if true, the column has a width either from HTML width attribute,
                                          //   from a style rule on the column, 
                                          //   or from a width attr/style on a cell that has colspan==1 
                                          
    // Get column information
    nsTableColFrame* colFrame = mTableFrame->GetColFrame(colIndex);
    TDBG_SDD("BTLS::APCW - got colFrame %p for colIndex %d\n", colFrame, colIndex);
    NS_ASSERTION(nsnull != colFrame, "bad col frame");

    // Get the columns's style
    const nsStylePosition* colPosition;
    colFrame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)colPosition);
    const nsStyleTable* colTableStyle;
    colFrame->GetStyleData(eStyleStruct_Table, (const nsStyleStruct*&)colTableStyle);

    // Get fixed column width if it has one
    if (eStyleUnit_Coord == colPosition->mWidth.GetUnit()) {
      haveColWidth = PR_TRUE;
      specifiedFixedColWidth = colPosition->mWidth.GetCoordValue();
      specifiedFixedColWidth += (cellPadding*2);
      TDBG_SD("BTLS::APCW - got specified col width = %d\n", specifiedFixedColWidth);
    }

    /* Scan the column, simulatneously assigning column widths
     * and computing the min/max column widths
     */
    PRInt32 firstRowIndex = -1;
    PRInt32 maxColSpan = 1;
    PRBool cellGrantingWidth = PR_TRUE;
    for (rowIndex = 0; rowIndex < numRows; rowIndex++) {
      nsTableCellFrame* cellFrame = mTableFrame->GetCellFrameAt(rowIndex, colIndex);
      if (nsnull == cellFrame) { // there is no cell in this row that corresponds to this column
        continue;
      }
      if (-1 == firstRowIndex) {
        firstRowIndex = rowIndex;
      }
      PRInt32 cellRowIndex;
      cellFrame->GetRowIndex(cellRowIndex);
      if (rowIndex != cellRowIndex) {
        // For cells that span rows, we only figure it in once
        NS_ASSERTION(1 != cellFrame->GetRowSpan(), "row index does not match row span");  // sanity check
        continue;
      }
      PRInt32 colSpan = mTableFrame->GetEffectiveColSpan(colIndex, cellFrame);
      maxColSpan = PR_MAX(maxColSpan,colSpan);
      PRInt32 cellColIndex;
      cellFrame->GetColIndex(cellColIndex);
      if (colIndex != cellColIndex) {
        // For cells that span cols, we figure in the row using previously-built SpanInfo 
        NS_ASSERTION(1 != cellFrame->GetColSpan(), "col index does not match col span");  // sanity check
        continue;
      }

      nsSize cellMinSize = cellFrame->GetPass1MaxElementSize();
      nsSize cellDesiredSize = cellFrame->GetPass1DesiredSize();
      nscoord cellDesiredWidth = cellDesiredSize.width;
      nscoord cellMinWidth = cellMinSize.width;

      if (1 == colSpan) {
        if (0 == minColContentWidth) {
          minColContentWidth = cellMinWidth;
        }
        else {
          minColContentWidth = PR_MAX(minColContentWidth, cellMinWidth);
        }
      }
      else {
        minColContentWidth = PR_MAX(minColContentWidth, cellMinWidth/colSpan);  // no need to divide this proportionately
      }
      TDBG_SDDDDDD("for cell %d with colspan=%d, min = %d,%d  and  des = %d,%d\n", 
                    rowIndex, colSpan, cellMinSize.width, cellMinSize.height,
                    cellDesiredSize.width, cellDesiredSize.height);

      if (PR_TRUE == haveColWidth) {
        // This col has a specified coord fixed width, so set the min and max width to the larger of 
        //   (specified width, largest max_element_size of the cells in the column)
        //   factoring in the min width of the prior cells (stored in minColWidth) 
        nscoord widthForThisCell = specifiedFixedColWidth;
        if (0 == specifiedFixedColWidth) { // set to min
          specifiedFixedColWidth = cellMinWidth;
        }
        widthForThisCell = PR_MAX(widthForThisCell, effectiveMinColumnWidth);
        if (1 == colSpan) {
          widthForThisCell = PR_MAX(widthForThisCell, cellMinWidth);
        }
        mTableFrame->SetColumnWidth(colIndex, widthForThisCell);
        maxColWidth = widthForThisCell;
        minColWidth = PR_MAX(minColWidth, cellMinWidth);
        if (1 == colSpan) {
          effectiveMaxColumnWidth = PR_MAX(effectiveMaxColumnWidth, widthForThisCell);
          if (0 == effectiveMinColumnWidth) {
            effectiveMinColumnWidth = cellMinWidth;//widthForThisCell; 
          }
          else {
            //effectiveMinColumnWidth = PR_MIN(effectiveMinColumnWidth, widthForThisCell);
            //above line works for most tables, but it can't be right
            effectiveMinColumnWidth = PR_MAX(effectiveMinColumnWidth, cellMinWidth);
            //above line seems right and works for xT1, but breaks lots of other tables.
          }
        }
      }
      else {
        if (maxColWidth < cellDesiredWidth) {
          maxColWidth = cellDesiredWidth;
        }
        if ((1==colSpan) && (effectiveMaxColumnWidth < cellDesiredWidth)) {
          effectiveMaxColumnWidth = cellDesiredWidth;
        }
        if (minColWidth < cellMinWidth) {
          minColWidth = cellMinWidth;
        }
        // effectiveMinColumnWidth is the min width as if no cells with colspans existed
        if ((1==colSpan) && (effectiveMinColumnWidth < cellMinWidth)) {
          effectiveMinColumnWidth = cellMinWidth;
        }
      }

      if (1 < colSpan) {
        // add the column to our list of post-process columns, if all of the intersected columns are auto
        PRBool okToAdd = PR_TRUE;
        if (numRows == 1 || (PR_FALSE == IsFixedWidth(colPosition, colTableStyle))) {
          okToAdd = PR_FALSE;
        }
        else {
          for (PRInt32 i = 1; i < colSpan; i++) {
            nsTableColFrame* cf;
            mTableFrame->GetColumnFrame(i+colIndex, cf);
            const nsStylePosition* colPos;
            cf->GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)colPos);
            if (colPos->mWidth.GetUnit() != eStyleUnit_Auto &&
               (nsTableColFrame::eWIDTH_SOURCE_CELL_WITH_SPAN!=cf->GetWidthSource())) {
              okToAdd = PR_FALSE;
              break;
            }
          }
        }

        nscoord width = cellDesiredSize.width;  // used below as the cell's "natural" width
        if (eStyleUnit_Coord == colTableStyle->mSpanWidth.GetUnit()) {
          width = colSpan * colTableStyle->mSpanWidth.GetCoordValue();  // "colSpan*" because table frame divided per column spanned
        }
        if (PR_TRUE == okToAdd) {
          nscoord colwidth = PR_MAX(width, cellMinWidth);
          ColSpanStruct* colSpanInfo = new ColSpanStruct(colSpan, colIndex, colwidth);
          if (nsnull == colSpanList) {
            colSpanList = new nsVoidArray();
          }
          colSpanList->AppendElement(colSpanInfo);
        }

        if (PR_TRUE == cellGrantingWidth) {
          // add the cell to our list of spanning cells
          SpanInfo* spanInfo = new SpanInfo(colIndex, colSpan, cellMinWidth, width);
          if (nsnull == spanList) {
            spanList = new nsVoidArray();
          }
          spanList->AppendElement(spanInfo);
        }
      }

      // bookkeeping:  is this the cell that gave the column it's fixed width attribute?
      // must be done after "haveColWidth && cellGrantingWidth" used above
      // since we want the biggest, I don't think this is valid anymore
      // it made sense when the rule was to grab the first cell that contributed a width
      //if (PR_TRUE==cellGrantingWidth)
      //{
      //  const nsStylePosition* cellPosition;
      //  cellFrame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct *&)cellPosition);
      //  if (eStyleUnit_Coord == cellPosition->mWidth.GetUnit())
      //    cellGrantingWidth=PR_FALSE; //I've found the cell that gave the col it's width
      //} 

      // book 'em, Danno
      TDBG_SDDD("    after cell %d, minColWidth = %d and maxColWidth = %d\n",
                rowIndex, minColWidth, maxColWidth);
    } // end for (rowIndex = 0; rowIndex < numRows; rowIndex++)

    // adjust the "fixed" width for content that is too wide
    if (effectiveMinColumnWidth > specifiedFixedColWidth) {
      specifiedFixedColWidth = effectiveMinColumnWidth;
    }
    // do all the global bookkeeping, factoring in margins
    nscoord colInset = mTableFrame->GetCellSpacingX();
    // keep a running total of the amount of space taken up by all fixed-width columns
    if ((PR_TRUE == haveColWidth) && 
        (nsTableColFrame::eWIDTH_SOURCE_CELL == colFrame->GetWidthSource())) {
      mFixedTableWidth += specifiedFixedColWidth + colInset;
      if (0 == colIndex) {
        mFixedTableWidth += colInset;
      }
      TDBG_SD("setting mFixedTableWidth=%d\n", mFixedTableWidth);
    }

    // cache the computed column info
    colFrame->SetMinColWidth(effectiveMinColumnWidth);
    colFrame->SetMaxColWidth(effectiveMaxColumnWidth);
    colFrame->SetEffectiveMinColWidth(effectiveMinColumnWidth);
    colFrame->SetEffectiveMaxColWidth(effectiveMaxColumnWidth);
    // this is the default, the real adjustment happens below where we deal with colspans
    colFrame->SetAdjustedMinColWidth(effectiveMinColumnWidth);  
    if ((PR_TRUE == haveColWidth) && 
      (nsTableColFrame::eWIDTH_SOURCE_CELL_WITH_SPAN!=colFrame->GetWidthSource())) {
      mTableFrame->SetColumnWidth(colIndex, specifiedFixedColWidth);
    }
    else {
      mTableFrame->SetColumnWidth(colIndex, effectiveMaxColumnWidth);
    }
    TDBG_SDD("col %d, set col width to = %d\n", colIndex, mTableFrame->GetColumnWidth(colIndex));

    if (PR_TRUE == hasColsAttribute) {
      minColWidthArray[colIndex] = minColWidth;
      maxColWidthArray[colIndex] = maxColWidth;
    }
    TDBG_SDDDD("after col %d, minColWidth=%d effectiveMinColumnWidth=%d\n\teffectiveMaxColumnWidth = %d\n", 
               colIndex, minColWidth, effectiveMinColumnWidth, effectiveMaxColumnWidth);
  }

  // now, post-process the computed values based on the table attributes

  // first, factor in max values of spanning cells proportionately.
  // We can't do this above in the first loop, because we don't have enough information
  // to determine the proportion each column gets from spanners.
  if (nsnull != spanList) {
    // we only want to do this if there are auto-cells involved
    // or if we have columns that are provisionally fixed-width with colspans
    PRInt32 numAutoColumns=0;
    PRInt32* autoColumns=nsnull;
    mTableFrame->GetColumnsByType(eStyleUnit_Auto, numAutoColumns, autoColumns);
    // for every column, handle spanning cells that impact that column
    for (PRInt32 colIndex=0; colIndex<mNumCols; colIndex++) {
      TDBG_SD("handling span for %d\n", colIndex);
      PRInt32 spanCount = spanList->Count();
      // go through the list backwards so we can delete easily
      for (PRInt32 spanIndex = spanCount-1; 0 <= spanIndex; spanIndex--) { 
        // get each spanInfo struct and see if it impacts this column
        SpanInfo *spanInfo = (SpanInfo *)(spanList->ElementAt(spanIndex));
        // if the spanInfo is about a column before the current column, it effects 
        // the current column (otherwise it would have already been deleted.)
        if (spanInfo->initialColIndex <= colIndex) {
          if (-1 == spanInfo->effectiveMaxWidthOfSpannedCols) { 
            // if we have not yet computed effectiveMaxWidthOfSpannedCols, do it now
            // first, initialize the sums
            spanInfo->effectiveMaxWidthOfSpannedCols = 0;
            spanInfo->effectiveMinWidthOfSpannedCols = 0;
            // then compute the sums
            for (PRInt32 span=0; span < spanInfo->initialColSpan; span++) {
              nsTableColFrame* nextColFrame = mTableFrame->GetColFrame(colIndex+span);
              if (nsnull==nextColFrame) {
                break;
              }
              spanInfo->effectiveMaxWidthOfSpannedCols += nextColFrame->GetEffectiveMaxColWidth();
              spanInfo->effectiveMinWidthOfSpannedCols += nextColFrame->GetEffectiveMinColWidth();
            }
            TDBG_SDD("effective min total = %d, max total = %d\n", 
                     spanInfo->effectiveMinWidthOfSpannedCols, spanInfo->effectiveMaxWidthOfSpannedCols);
          }
          nsTableColFrame* colFrame = mTableFrame->GetColFrame(colIndex);
          nscoord colMinWidth = colFrame->GetMinColWidth();
          
          // compute the spanning cell's contribution to the column min width
          // this is the "adjusted" column width, used in SetTableToMinWidth
          nscoord spanCellMinWidth;
          if (0 != spanInfo->effectiveMinWidthOfSpannedCols) {
            float percent = ((float)(colFrame->GetEffectiveMinColWidth())) /
                             ((float)(spanInfo->effectiveMinWidthOfSpannedCols));
            spanCellMinWidth = NSToCoordRound(((float)(spanInfo->cellMinWidth)) * percent);
            TDBG_SDFDDD("spanCellMinWidth portion = %d from percent=%f, cellMW=%d, effMinColW=%d, sum=%d\n", 
                        spanCellMinWidth, percent, spanInfo->cellMinWidth, colFrame->GetEffectiveMinColWidth(),
                        spanInfo->effectiveMinWidthOfSpannedCols);
            if (colMinWidth < spanCellMinWidth) {
              colFrame->SetAdjustedMinColWidth(spanCellMinWidth); // set the new min width for the col
            }
          }
          else {
            spanCellMinWidth = spanInfo->cellMinWidth / spanInfo->initialColSpan;
            if (colMinWidth < spanCellMinWidth) {
              colFrame->SetAdjustedMinColWidth(spanCellMinWidth);
            }
          }

          // compute the spanning cell's contribution to the column max width
          nscoord colMaxWidth = colFrame->GetMaxColWidth(); 
          nscoord spanCellMaxWidth;
          if (0 != spanInfo->effectiveMaxWidthOfSpannedCols) {
            float percent = ((float)(colFrame->GetEffectiveMaxColWidth())) /
                             ((float)(spanInfo->effectiveMaxWidthOfSpannedCols));
            spanCellMaxWidth = NSToCoordRound(((float)(spanInfo->cellDesiredWidth)) * percent);
            TDBG_SDFDDD("spanCellMaxWidth portion = %d with percent = %f from cellDW = %d, effMaxColW=%d and sum=%d\n", 
                        spanCellMaxWidth, percent, spanInfo->cellDesiredWidth, colFrame->GetEffectiveMaxColWidth(),
                        spanInfo->effectiveMaxWidthOfSpannedCols);
            if (colMaxWidth < spanCellMaxWidth) {
              // make sure we're at least as big as our min
              spanCellMaxWidth = PR_MAX(spanCellMaxWidth, colMinWidth);
              colFrame->SetMaxColWidth(spanCellMaxWidth);     // set the new max width for the col
              mTableFrame->SetColumnWidth(colIndex, spanCellMaxWidth);  // set the column to the new desired max width
              TDBG_SDDDD("for spanning cell into col %d with remaining span=%d, old max = %d, new max = %d\n", 
                         colIndex, spanInfo->span, colMaxWidth, spanCellMaxWidth);
            }
          }
          else {
            spanCellMaxWidth = spanInfo->cellDesiredWidth / spanInfo->initialColSpan;
            nscoord minColWidth = colFrame->GetMinColWidth();
            spanCellMaxWidth = PR_MAX(spanCellMaxWidth, minColWidth);
            colFrame->SetMaxColWidth(spanCellMaxWidth);
            mTableFrame->SetColumnWidth(colIndex, spanCellMaxWidth);
            TDBG_SDDDD("    for spanning cell into col %d with remaining span=%d, old max = %d, new max = %d\n", 
                       colIndex, spanInfo->span, colMaxWidth, spanCellMaxWidth);
          }

          spanInfo->span--;
          if (0==spanInfo->span) {
            spanList->RemoveElementAt(spanIndex);
            delete spanInfo;
          }

          // begin code that respects column width attribute
          /* the code below checks to see if the column has a width attribute (either it's own or a cell's)
           * if so, if it fits within the constraints we computed above, we use it.
           * why go through all that pain above, then?  because the given width attribute might not
           * be rational.  So we need to act as if it doesn't exist and then fit it in if it makes sense.
           * a shortcut that checks for a column width attribute and skips the above computations
           * would not work without lots of extra computation that would lead you down the same path anyway.
           */
          const nsStylePosition* colPosition;
          colFrame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)colPosition);
          // Get fixed column width if it has one
          if (eStyleUnit_Coord == colPosition->mWidth.GetUnit()) {
            if (nsTableColFrame::eWIDTH_SOURCE_CELL_WITH_SPAN!=colFrame->GetWidthSource()) {
              nscoord specifiedFixedColWidth = colPosition->mWidth.GetCoordValue();
              specifiedFixedColWidth += (cellPadding*2);
              if (specifiedFixedColWidth>=colFrame->GetEffectiveMinColWidth()) {
                mTableFrame->SetColumnWidth(colIndex, specifiedFixedColWidth);
                colFrame->SetMaxColWidth(specifiedFixedColWidth);
              }
            }
          }
          // end code that respects column width attribute
        }
      }
    }
  }

  // now set the min and max table widths
  SetMinAndMaxTableWidths();

  // then, handle the COLS attribute (equal column widths)
  // if there is a COLS attribute, fix up mMinTableWidth and mMaxTableWidth
  if (PR_TRUE == hasColsAttribute) {
    TDBG_SD("has COLS attribute = %d\n", mCols);
    // for every effected column, subtract out its prior contribution and add back in the new value
    PRInt32 numColsEffected = mNumCols;
    if (NS_STYLE_TABLE_COLS_ALL != mCols) {
      numColsEffected = mCols;
    }
    PRInt32 maxOfMinColWidths = 0;
    PRInt32 maxOfMaxColWidths = 0;
    PRInt32 effectedColIndex;
    for (effectedColIndex=0; effectedColIndex<numColsEffected; effectedColIndex++) {
      maxOfMinColWidths = PR_MAX(maxOfMinColWidths, minColWidthArray[effectedColIndex]);
      maxOfMaxColWidths = PR_MAX(maxOfMaxColWidths, maxColWidthArray[effectedColIndex]);
    }
    for (effectedColIndex = 0; effectedColIndex<numColsEffected; effectedColIndex++) {
      // subtract out the prior contributions of this column
      // and add back in the adjusted value
      if (NS_UNCONSTRAINEDSIZE != mMinTableWidth) {
        mMinTableWidth -= minColWidthArray[effectedColIndex];
        mMinTableWidth += maxOfMinColWidths;
      }
      if (NS_UNCONSTRAINEDSIZE!=mMaxTableWidth) {
        mMaxTableWidth -= maxColWidthArray[effectedColIndex];
        mMaxTableWidth += maxOfMaxColWidths;
      }
      nsTableColFrame* colFrame;
      mTableFrame->GetColumnFrame(effectedColIndex, colFrame);
      colFrame->SetMaxColWidth(maxOfMaxColWidths);  // cache the new column max width (min width is uneffected)
      colFrame->SetEffectiveMaxColWidth(maxOfMaxColWidths);
      TDBG_SDD("col %d now has max col width %d\n", effectedColIndex, maxOfMaxColWidths);
    }
    delete [] minColWidthArray;
    delete [] maxColWidthArray;
  }
  if (nsnull != colSpanList) {
    DistributeFixedSpace(colSpanList);
  }

  TDBG_SPDD("%p: aMinTW=%d, aMaxTW=%d\n", mTableFrame, mMinTableWidth, mMaxTableWidth);

  // clean up
  if (nsnull != spanList) {
    TDBG_S("BTLS::APCW...space leak, span list not empty\n");
    delete spanList;
  }
  if (nsnull != colSpanList) {
    PRInt32 colSpanListCount = colSpanList->Count();
    for (PRInt32 i = 0; i < colSpanListCount; i++) {
      ColSpanStruct * colSpanInfo = (ColSpanStruct *)(colSpanList->ElementAt(i));
      if (nsnull != colSpanInfo) {
        delete colSpanInfo;
      }
    }
    delete colSpanList;
  }
  return PR_TRUE;
}

void BasicTableLayoutStrategy::SetMinAndMaxTableWidths()
{
  TDBG_S("SetMinAndMaxTableWidths\n");
  PRInt32 colIndex, rowIndex;
  PRInt32 numRows = mTableFrame->GetRowCount();
  nscoord colInset = mTableFrame->GetCellSpacingX();
  for (rowIndex = 0; rowIndex < numRows; rowIndex++) {
    TDBG_SD("  row %d\n", rowIndex);
    nscoord rowMinWidth = colInset;
    nscoord rowMaxWidth = colInset;
    for (colIndex = 0; colIndex < mNumCols; colIndex++) {
      TDBG_SD("    col %d\n", colIndex);
      nsTableCellFrame* cellFrame = mTableFrame->GetCellFrameAt(rowIndex, colIndex);
      rowMinWidth += colInset;
      rowMaxWidth += colInset;
      if (nsnull == cellFrame) { // there is no cell in this row that corresponds to this column
        TDBG_SD("    col %d skipped because there is no cell\n", colIndex);
        continue;
      }
      PRInt32 cellColIndex;
      cellFrame->GetColIndex(cellColIndex);
      if (colIndex != cellColIndex) {
        // For cells that span cols, we figured in the cell the first time we saw it
        TDBG_SD("    col %d skipped because it has colspan so we've already added it in\n", colIndex);
        continue;
      }
      nsTableColFrame* colFrame = mTableFrame->GetColFrame(colIndex);
      nsSize cellMinSize = cellFrame->GetPass1MaxElementSize();
      nscoord cellMinWidth = PR_MAX(cellMinSize.width, colFrame->GetEffectiveMinColWidth());
      nsSize cellMaxSize = cellFrame->GetPass1DesiredSize();
      nscoord cellMaxWidth = PR_MAX(cellMaxSize.width, colFrame->GetEffectiveMaxColWidth());
      PRInt32 colSpan = mTableFrame->GetEffectiveColSpan(colIndex, cellFrame);
      nscoord spanningCellMinWidth = (colSpan-1)*colInset; 
      TDBG_SDDD("    cellMin=%d, cellMax=%d, spanningCellMin=%d\n", 
                cellMinWidth, cellMaxWidth, spanningCellMinWidth);
      // spanning cells must be at least as wide as the columns they span, including the cell spacing spanned.
      if (NS_UNCONSTRAINEDSIZE != rowMinWidth) {
        rowMinWidth += PR_MAX(cellMinWidth, spanningCellMinWidth);
      }
      if (NS_UNCONSTRAINEDSIZE != rowMaxWidth) {
        rowMaxWidth += PR_MAX(cellMaxWidth, spanningCellMinWidth);
      }
      TDBG_SDD("    rowMinWidth=%d, rowMaxWidth=%d\n", rowMinWidth, rowMaxWidth);
    }
    TDBG_SDD("  rowMinWidth=%d, rowMaxWidth=%d\n", rowMinWidth, rowMaxWidth);
    // the largest row widths are the table widths
    mMinTableWidth = PR_MAX(mMinTableWidth, rowMinWidth);
    mMaxTableWidth = PR_MAX(mMaxTableWidth, rowMaxWidth);
    TDBG_SDD("  mMinTableWidth=%d, mMaxTableWidth=%d\n", mMinTableWidth, mMaxTableWidth);
  }
  // verify max of min row widths vs. sum of adjusted column min widths.  bigger one wins
  nscoord sumOfAdjustedColMinWidths = colInset;
  for (colIndex = 0; colIndex < mNumCols; colIndex++) {
    nsTableColFrame* colFrame;
    mTableFrame->GetColumnFrame(colIndex, colFrame);
    sumOfAdjustedColMinWidths += colFrame->GetAdjustedMinColWidth() + colInset;
    TDBG_SDDDD("  col %d has amcw=%d, cellspacing=%d, sum=%d\n", 
               colIndex, colFrame->GetAdjustedMinColWidth(), colInset, sumOfAdjustedColMinWidths);
  }
  TDBG_SD("  sumOfAdjustedColMinWidths=%d\n", sumOfAdjustedColMinWidths);
  /*
  mMinTableWidth = PR_MAX(mMinTableWidth, sumOfAdjustedColMinWidths);
  mMaxTableWidth = PR_MAX(mMinTableWidth, mMaxTableWidth);
  */
  TDBG_SDDD("end SetMinAndMaxTW: minTW=%d, maxTW=%d with DMCW=%d\n", mMinTableWidth, mMaxTableWidth, sumOfAdjustedColMinWidths);
}

// take the fixed space spanned by the columns in aColSpanList 
// and distribute it proportionately (based on desired width)
void BasicTableLayoutStrategy::DistributeFixedSpace(nsVoidArray *aColSpanList)
{
  nscoord excess = 0;
  TDBG_S("** DistributeFixedSpace:\n");
  // for all fixed-width columns, determine the amount of the specified width each column spanned recieves
  PRInt32 numSpanningCells = aColSpanList->Count();
  for (PRInt32 nextSpanningCell = 0; nextSpanningCell < numSpanningCells; nextSpanningCell++) { // proportionately distributed extra space, based on the column's fixed width
    ColSpanStruct* colInfo = (ColSpanStruct *)aColSpanList->ElementAt(nextSpanningCell);
    PRInt32 colIndex = colInfo->colIndex;  
    PRInt32 colSpan = colInfo->colSpan;  
    nscoord totalColWidth = colInfo->width;
    
    // 1. get the sum of the effective widths of the columns in the span
    nscoord totalEffectiveWidth=0;
    nsTableColFrame* colFrame;
    PRInt32 i;
    for (i = 0; i<colSpan; i++) {
      mTableFrame->GetColumnFrame(colIndex+i, colFrame);
      totalEffectiveWidth += colFrame->GetColWidthForComputation();
    }

    // 2. next, compute the proportion to be added to each column, and add it
    for (i = 0; i<colSpan; i++) {
      mTableFrame->GetColumnFrame(colIndex+i, colFrame);
      float percent;
      percent = ((float)(colFrame->GetColWidthForComputation())) / ((float)totalEffectiveWidth);
      nscoord newColWidth = NSToCoordRound(((float)totalColWidth)*percent);
      nscoord minColWidth = colFrame->GetEffectiveMinColWidth();
      nscoord oldColWidth = mTableFrame->GetColumnWidth(colIndex+i);
      if (newColWidth > minColWidth) {
        TDBG_SDD_SDD("  assigning fixed col width for spanning cells:  column %d set to %d\n", 
                     colIndex+i, newColWidth,
                     "  minCW = %d oldCW = %d\n", minColWidth, oldColWidth);
        mTableFrame->SetColumnWidth(colIndex+i, newColWidth);
        colFrame->SetEffectiveMaxColWidth(newColWidth);
      }
    }
  }
}

/* assign column widths to all non-fixed-width columns (adjusting fixed-width columns if absolutely necessary)
 aAvailWidth is the amount of space left in the table to distribute after fixed-width columns are accounted for
 aMaxWidth is the space the parent gave us (minus border & padding) to fit ourselves into
 aTableIsAutoWidth is true if the table is auto-width, false if it is anything else (percent, fixed, etc)
 */
PRBool BasicTableLayoutStrategy::
           BalanceProportionalColumns (const nsHTMLReflowState& aReflowState,
                                             nscoord            aAvailWidth,
                                             nscoord            aMaxWidth,
                                             nscoord            aTableSpecifiedWidth,
                                             PRBool             aTableIsAutoWidth)
{
  PRBool result = PR_TRUE;

  nscoord actualMaxWidth; // the real target width, depends on if we're auto or specified width
  if (PR_TRUE == aTableIsAutoWidth) {
    actualMaxWidth = aMaxWidth;
  } else {
    actualMaxWidth = PR_MIN(aMaxWidth, aTableSpecifiedWidth);
  }
  if (NS_UNCONSTRAINEDSIZE == aMaxWidth  ||  NS_UNCONSTRAINEDSIZE == mMinTableWidth) { 
    // the max width of the table fits comfortably in the available space
    TDBG_S("  * table laying out in NS_UNCONSTRAINEDSIZE, calling BalanceColumnsTableFits\n");
    // nested tables are laid out with unconstrained width.  But the underlying algorithms require a 
    // real width.  So we pick a really big width here.  It doesn't really matter, of course, because
    // eventually the table will be laid out with a constrained width.
    nscoord bigSpace = gBigSpace;
    bigSpace = PR_MAX(bigSpace, mMaxTableWidth);
    result = BalanceColumnsTableFits(aReflowState, bigSpace, bigSpace, 
                                     aTableSpecifiedWidth, aTableIsAutoWidth);
  }
  else if (mMinTableWidth > actualMaxWidth) {
    // the table doesn't fit in the available space
    TDBG_S("  * table minTW does not fit, calling BalanceColumnsTableDoesNotFit\n");
    result = BalanceColumnsTableDoesNotFit();
  }
  else if (mMaxTableWidth <= actualMaxWidth) {
    // the max width of the table fits comfortably in the available space
    TDBG_S("  * table desired size fits, calling BalanceColumnsTableFits\n");
    result = BalanceColumnsTableFits(aReflowState, aAvailWidth, aMaxWidth, 
                                     aTableSpecifiedWidth, aTableIsAutoWidth);
  }
  else {
    // the table fits somewhere between its min and desired size
    TDBG_S("  * table desired size does not fit, calling BalanceColumnsConstrained\n");
    result = BalanceColumnsConstrained(aReflowState, aAvailWidth,
                                       actualMaxWidth, aTableIsAutoWidth);
  }

  return result;
}

// the table doesn't fit, so squeeze every column down to its minimum
PRBool BasicTableLayoutStrategy::BalanceColumnsTableDoesNotFit()
{
  PRBool result = PR_TRUE;

  PRBool hasColsAttribute = (PRBool)(NS_STYLE_TABLE_COLS_NONE!=mCols);
  
  PRInt32* minColWidthArray = nsnull;
  if (PR_TRUE == hasColsAttribute) {
    minColWidthArray = new PRInt32[mNumCols];
  }

  PRInt32 numRows = mTableFrame->GetRowCount();
  for (PRInt32 colIndex = 0; colIndex < mNumCols; colIndex++) { 
    nscoord minAdjustedColWidth = 0;
    TDBG_SD("  for col %d\n", colIndex);

    nsTableColFrame *colFrame = mTableFrame->GetColFrame(colIndex);
    NS_ASSERTION(nsnull != colFrame, "bad col frame");

    // Get the columns's style
    const nsStylePosition* colPosition;
    colFrame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)colPosition);
    const nsStyleTable* colTableStyle;
    colFrame->GetStyleData(eStyleStruct_Table, (const nsStyleStruct*&)colTableStyle);
    //if (PR_FALSE==IsFixedWidth(colPosition, colTableStyle))
    {
      minAdjustedColWidth = colFrame->GetAdjustedMinColWidth();
      mTableFrame->SetColumnWidth(colIndex, minAdjustedColWidth);
      if (PR_TRUE == hasColsAttribute) {
        minColWidthArray[colIndex] = minAdjustedColWidth;
      }
    }
    TDBG_SDD("  2: col %d, set to width = %d\n", colIndex, mTableFrame->GetColumnWidth(colIndex));
  }

  // now, post-process the computed values based on the table attributes
  // if there is a COLS attribute, fix up mMinTableWidth and mMaxTableWidth
  if (PR_TRUE == hasColsAttribute) { // for every effected column, subtract out its prior contribution and add back in the new value
    PRInt32 numColsEffected = mNumCols;
    if (NS_STYLE_TABLE_COLS_ALL != mCols) {
      numColsEffected = mCols;
    }
    nscoord maxOfEffectedColWidths = 0;
    PRInt32 effectedColIndex;
    // XXX need to fix this and all similar code if any fixed-width columns intersect COLS
    for (effectedColIndex = 0; effectedColIndex < numColsEffected; effectedColIndex++) {
      maxOfEffectedColWidths = PR_MAX(maxOfEffectedColWidths, minColWidthArray[effectedColIndex]);
    }
    for (effectedColIndex = 0; effectedColIndex < numColsEffected; effectedColIndex++) {
      // set each effected column to the size of the largest column in the group
      mTableFrame->SetColumnWidth(effectedColIndex, maxOfEffectedColWidths);
      TDBG_SDD("  2 (cols): setting %d to %d\n", effectedColIndex, maxOfEffectedColWidths); 
    }
    // we're guaranteed here that minColWidthArray has been allocated, and that
    // if we don't get here, it was never allocated
    delete [] minColWidthArray;
  }
  return result;
}

/* the table fits in the given space.  Set all columns to their desired width,
 * and if we are not an auto-width table add extra space to fluff out the total width
 */
PRBool BasicTableLayoutStrategy::
                 BalanceColumnsTableFits(const nsHTMLReflowState& aReflowState,
                                               nscoord            aAvailWidth,
                                               nscoord            aMaxWidth,
                                               nscoord            aTableSpecifiedWidth,
                                               PRBool             aTableIsAutoWidth)
{
  PRBool  result = PR_TRUE;
  nscoord tableWidth = 0;               // the width of the table as a result of setting column widths
  nscoord widthOfFixedTableColumns = 0; // the sum of the width of all fixed-width columns plus margins
                                        // tableWidth - widthOfFixedTableColumns is the width of columns computed in this method
  PRInt32 totalSlices = 0;              // the total number of slices the proportional-width columns request
  nsVoidArray* proportionalColumnsList = nsnull; // a list of the columns that are proportional-width
  nsVoidArray* spanList = nsnull;       // a list of the cells that span columns
  PRInt32 numRows = mTableFrame->GetRowCount();
  nscoord colInset = mTableFrame->GetCellSpacingX();
  PRInt32 colIndex;

  for (colIndex = 0; colIndex < mNumCols; colIndex++) { 
    TDBG_SD("for col %d\n", colIndex);
    // Get column information
    nsTableColFrame* colFrame = mTableFrame->GetColFrame(colIndex);
    NS_ASSERTION(nsnull != colFrame, "bad col frame");
    PRInt32 minColWidth = colFrame->GetMinColWidth();
    PRInt32 maxColWidth = 0;
    PRInt32 rowIndex;
    PRInt32 firstRowIndex = -1;

    const nsStylePosition* colPosition;
    colFrame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)colPosition);    
    const nsStyleTable* colTableStyle;
    colFrame->GetStyleData(eStyleStruct_Table, (const nsStyleStruct*&)colTableStyle);

    // first, deal with any cells that span into this column from a pervious column
      // go through the list backwards so we can delete easily
    if (nsnull!=spanList) {
      PRInt32 spanCount = spanList->Count();
      // go through the list backwards so we can delete easily
      for (PRInt32 spanIndex = spanCount-1; 0 <= spanIndex; spanIndex--) {
        SpanInfo* spanInfo = (SpanInfo *)(spanList->ElementAt(spanIndex));
        if (PR_FALSE == IsFixedWidth(colPosition, colTableStyle)) {
          // compute the spanning cell's contribution to the column min width
          nscoord spanCellMinWidth;
          PRBool needsExtraMinWidth = PR_FALSE;
          //if (spanInfo->effectiveMinWidthOfSpannedCols<spanInfo->cellMinWidth)
          //  needsExtraMinWidth = PR_TRUE;
          if (PR_TRUE == needsExtraMinWidth) {
            if (0 != spanInfo->effectiveMinWidthOfSpannedCols) {
              spanCellMinWidth = (spanInfo->cellMinWidth * colFrame->GetEffectiveMinColWidth()) /
                                 (spanInfo->effectiveMinWidthOfSpannedCols);
              TDBG_SDD("    spanlist min: %d of %d\n", spanCellMinWidth, spanInfo->effectiveMaxWidthOfSpannedCols);
              if (minColWidth < spanCellMinWidth) {
                minColWidth = spanCellMinWidth;     // set the new min width for the col
                TDBG_SDDD("    for spanning cell into col %d with remaining span=%d, new min = %d\n", 
                          colIndex, spanInfo->span, minColWidth);
                // if the new min width is greater than the desired width, bump up the desired width
                maxColWidth = PR_MAX(maxColWidth, minColWidth);
                TDBG_SD("   and maxColWidth = %d\n", maxColWidth);
              }
            }
            else {
              spanCellMinWidth = spanInfo->cellMinWidth / spanInfo->initialColSpan;
              if (minColWidth < spanCellMinWidth) {
                minColWidth = spanCellMinWidth;
                TDBG_SDDD("    for spanning cell into col %d with remaining span=%d, new min = %d\n", 
                          colIndex, spanInfo->span, minColWidth);
                // if the new min width is greater than the desired width, bump up the desired width
                maxColWidth = PR_MAX(maxColWidth, minColWidth);
                TDBG_SD("   and maxColWidth = %d\n", maxColWidth);
              }
            }
          }


          // compute the spanning cell's contribution to the column max width
          nscoord spanCellMaxWidth;
          if (0 != spanInfo->effectiveMaxWidthOfSpannedCols) {
            spanCellMaxWidth = (spanInfo->cellDesiredWidth * colFrame->GetEffectiveMaxColWidth()) /
                               (spanInfo->effectiveMaxWidthOfSpannedCols);
            TDBG_SDD("    spanlist max: %d of %d\n", spanCellMaxWidth, spanInfo->effectiveMaxWidthOfSpannedCols);
            if (maxColWidth < spanCellMaxWidth) {
              spanCellMaxWidth = PR_MAX(spanCellMaxWidth, minColWidth);
              maxColWidth = spanCellMaxWidth;                 // set the new max width for the col
              mTableFrame->SetColumnWidth(colIndex, spanCellMaxWidth);  // set the column to the new desired max width
              TDBG_SDDD("    for spanning cell into col %d with remaining span=%d, new max = %d\n", 
                       colIndex, spanInfo->span, maxColWidth);
            }
          }
          else {
            spanCellMaxWidth = spanInfo->cellDesiredWidth / spanInfo->initialColSpan;
            maxColWidth = spanCellMaxWidth;
            mTableFrame->SetColumnWidth(colIndex, spanCellMaxWidth);
            TDBG_SDDD("    for spanning cell into col %d with remaining span=%d, new max = %d\n", 
                      colIndex, spanInfo->span, maxColWidth);
          }
        }
        spanInfo->span--;
        if (0 == spanInfo->span) {
          spanList->RemoveElementAt(spanIndex);
          delete spanInfo;
        }
      }
    }

    // second, process non-fixed-width columns
    if (PR_FALSE==IsFixedWidth(colPosition, colTableStyle)) {
      for (rowIndex = 0; rowIndex < numRows; rowIndex++) { 
        // this col has proportional width, so determine its width requirements
        nsTableCellFrame* cellFrame = mTableFrame->GetCellFrameAt(rowIndex, colIndex);
        if (nsnull == cellFrame) { 
          // there is no cell in this row that corresponds to this column
          continue;
        }
        if (-1 == firstRowIndex) {
          firstRowIndex = rowIndex;
        }
        PRInt32 cellRowIndex;
        cellFrame->GetRowIndex(cellRowIndex);
        if (rowIndex != cellRowIndex) {
          // For cells that span rows, we only figure it in once
          NS_ASSERTION(1 != cellFrame->GetRowSpan(), "row index does not match row span");  // sanity check
          continue;
        }
        PRInt32 cellColIndex;
        cellFrame->GetColIndex(cellColIndex);
        if (colIndex != cellColIndex) {
          // For cells that span cols, we figure in the row using previously-built SpanInfo 
          NS_ASSERTION(1 != cellFrame->GetColSpan(), "col index does not match row span");  // sanity check
          continue;
        }
 
        PRInt32 colSpan = mTableFrame->GetEffectiveColSpan(colIndex, cellFrame);
        nsSize cellMinSize = cellFrame->GetPass1MaxElementSize();
        nsSize cellDesiredSize = cellFrame->GetPass1DesiredSize();
        nscoord cellMinWidth=colFrame->GetMinColWidth();
        nscoord cellDesiredWidth=colFrame->GetMaxColWidth();  
        // then get the desired size info factoring in the cell style attributes
        if (1 == colSpan) {
          cellMinWidth = cellMinSize.width;
          cellDesiredWidth = cellDesiredSize.width;  
          nscoord specifiedCellWidth = -1;
          const nsStylePosition* cellPosition;
          cellFrame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)cellPosition);
          if (eStyleUnit_Percent==cellPosition->mWidth.GetUnit()) {
            if (PR_FALSE == aTableIsAutoWidth) {
              float percent = cellPosition->mWidth.GetPercentValue();
              specifiedCellWidth = (PRInt32)(aTableSpecifiedWidth * percent);
              TDBG_SFDD("specified percent width %f of %d = %d\n", 
                        percent, aTableSpecifiedWidth, specifiedCellWidth);
            }
            // otherwise we need to post-process, set to max for now
            // do we want to set specifiedCellWidth off of aAvailWidth? aMaxWidth?    XXX
          }
          if (-1 != specifiedCellWidth) {
            if (specifiedCellWidth>cellMinWidth) {
              TDBG_SDD("setting cellDesiredWidth from %d to %d\n", cellDesiredWidth, specifiedCellWidth);
              cellDesiredWidth = specifiedCellWidth;
            }
          }
        }
        else { 
          // colSpan>1, get the proportion for this column
          // then get the desired size info factoring in the cell style attributes
          nscoord effectiveMaxWidthOfSpannedCols = colFrame->GetEffectiveMaxColWidth();
          nscoord effectiveMinWidthOfSpannedCols = colFrame->GetEffectiveMinColWidth();
          for (PRInt32 span = 1; span < colSpan; span++) {
            nsTableColFrame* nextColFrame = mTableFrame->GetColFrame(colIndex+span);
            if (nsnull == nextColFrame) {
              break;
            }
            effectiveMaxWidthOfSpannedCols += nextColFrame->GetEffectiveMaxColWidth();
            effectiveMinWidthOfSpannedCols += nextColFrame->GetEffectiveMinColWidth();
          }
          nscoord specifiedCellWidth = -1;
          const nsStylePosition* cellPosition;
          cellFrame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)cellPosition);
          if (eStyleUnit_Percent == cellPosition->mWidth.GetUnit()) { 
            //XXX what if table is auto width?
            float percent = cellPosition->mWidth.GetPercentValue();
            specifiedCellWidth = (PRInt32)(aTableSpecifiedWidth*percent);
            TDBG_SFDD("specified percent width %f of %d = %d\n", 
                      percent, aTableSpecifiedWidth, specifiedCellWidth);
          }
          if (-1 != specifiedCellWidth) {
            float percentForThisCol = (float)(cellDesiredSize.width * colFrame->GetEffectiveMaxColWidth()) /
                                      (float)effectiveMaxWidthOfSpannedCols;
            nscoord cellWidthForThisCol = (nscoord)(specifiedCellWidth * percentForThisCol);
            if (cellWidthForThisCol>cellMinWidth) {
              TDBG_SDD("setting cellDesiredWidth from %d to %d\n", cellDesiredWidth, cellWidthForThisCol);
              cellDesiredWidth = cellWidthForThisCol;
            }
          }
          // otherwise it's already been factored in.
          // now, if this column holds the cell, create a spanInfo struct for the cell
          // so subsequent columns can take a proportion of this cell's space into account
          PRInt32 cellColIndex;
          cellFrame->GetColIndex(cellColIndex);
          if (cellColIndex == colIndex) { 
            // add this cell to span list iff we are currently processing the column the cell starts in
            SpanInfo* spanInfo = new SpanInfo(colIndex, colSpan-1, cellMinSize.width, cellDesiredSize.width);
            spanInfo->effectiveMaxWidthOfSpannedCols = effectiveMaxWidthOfSpannedCols;
            spanInfo->effectiveMinWidthOfSpannedCols = effectiveMinWidthOfSpannedCols;
            if (nsnull == spanList) {
              spanList = new nsVoidArray();
            }
            spanList->AppendElement(spanInfo);
          }
        }

        TDBG_SDDDD("factoring in cell %d with colSpan=%d\n  factoring in min=%d and desired=%d\n", 
                   rowIndex, colSpan, cellMinWidth, cellDesiredWidth);
        maxColWidth = PR_MAX(maxColWidth, cellDesiredWidth);
        TDBG_SDDDDD("    after cell %d, minColWidth=%d  maxColWidth=%d  effColWidth[%d]=%d\n", 
                    rowIndex, minColWidth, maxColWidth, 
                    colIndex, colFrame->GetEffectiveMaxColWidth());
      } // end looping through cells in the column

      TDBG_SDS_SDD_SD("  for determining width of col %d %s:\n",
                      colIndex, !IsFixedWidth(colPosition, colTableStyle)? "(P)":"(A)",
                      "    minColWidth = %d and maxColWidth = %d\n", minColWidth, maxColWidth,
                      "    aAvailWidth = %d\n", aAvailWidth);

      // Get column width if it has one
      nscoord specifiedProportionColumnWidth = -1;
      float specifiedPercentageColWidth = -1.0f;
      nscoord specifiedFixedColumnWidth = -1;
      PRBool isAutoWidth = PR_FALSE;
      switch (colPosition->mWidth.GetUnit()) {
        case eStyleUnit_Percent:
          specifiedPercentageColWidth = colPosition->mWidth.GetPercentValue();
          TDBG_SDF("column %d has specified percent width = %f\n", colIndex, specifiedPercentageColWidth);
          break;
        case eStyleUnit_Proportional:
          specifiedProportionColumnWidth = colPosition->mWidth.GetIntValue();
          TDBG_SDD("column %d has specified percent width = %d\n", colIndex, specifiedProportionColumnWidth);
          break;
        case eStyleUnit_Auto:
          isAutoWidth = PR_TRUE;
          break;
        default:
          break;
      }

      // set the column width, knowing that the table fits in the available space 
      if (0 == specifiedProportionColumnWidth || 0.0==specifiedPercentageColWidth) { 
        // col width is specified to be the minimum
        mTableFrame->SetColumnWidth(colIndex, minColWidth);
        TDBG_SDD("  3 min: col %d set to min width = %d because style set proportionalWidth=0\n", 
                 colIndex, mTableFrame->GetColumnWidth(colIndex));
      }
      else if ((PR_TRUE == isAutoWidth) || 
               ((PR_TRUE==aTableIsAutoWidth) && (-1==specifiedProportionColumnWidth))) { 
        // col width is determined by the cells' content,
         // so give each remaining column it's desired width (because we know we fit.)
         // if there is width left over, we'll factor that in after this loop is complete
         // the OR clause is because we fix up percentage widths as a post-process 
         // in auto-width tables
        mTableFrame->SetColumnWidth(colIndex, maxColWidth);
        TDBG_SDDD("  3 auto: col %d with availWidth %d, set to width = %d\n", 
                  colIndex, aAvailWidth, mTableFrame->GetColumnWidth(colIndex));
      }
      else if (-1 != specifiedProportionColumnWidth)
      { // we need to save these and do them after all other columns have been calculated
        // the calculation will be: 
        //   sum up n, the total number of slices for the columns with proportional width
        //   compute the table "required" width, fixed-width + percentage-width +
        //     the sum of the proportional column's max widths (especially because in this routine I know the table fits)
        //   compute the remaining width: the required width - the used width (fixed + percentage)
        //   compute the width per slice
        //   set the width of each proportional-width column to it's number of slices * width per slice
        mTableFrame->SetColumnWidth(colIndex, 0);   // set the column width to 0, since it isn't computed yet
        if (nsnull == proportionalColumnsList) {
          proportionalColumnsList = new nsVoidArray();
        }
        ProportionalColumnLayoutStruct* info = 
          new ProportionalColumnLayoutStruct(colIndex, minColWidth, 
                                             maxColWidth, specifiedProportionColumnWidth);
        proportionalColumnsList->AppendElement(info);
        totalSlices += specifiedProportionColumnWidth;  // keep track of the total number of proportions
        TDBG_SDDDD("  3 proportional: col %d with availWidth %d, gets %d slices with %d slices so far.\n", 
                   colIndex, aAvailWidth, specifiedProportionColumnWidth, totalSlices);
      }
      else {  // give the column a percentage of the remaining space
        PRInt32 percentage = -1;
        if (NS_UNCONSTRAINEDSIZE == aAvailWidth) { 
          // since the "remaining space" is infinite, give the column it's max requested size
          mTableFrame->SetColumnWidth(colIndex, maxColWidth);
        }
        else {
          if (-1.0f != specifiedPercentageColWidth) {
            percentage = (PRInt32)(specifiedPercentageColWidth * 100.0f); // TODO: rounding errors?
            // base the % on the total specified fixed width of the table
            mTableFrame->SetColumnWidth(colIndex, (percentage*aTableSpecifiedWidth) / 100);
            TDBG_SDDDD("  3 percent specified: col %d given %d percent of aTableSpecifiedWidth %d, set to width = %d\n", 
                       colIndex, percentage, aTableSpecifiedWidth, mTableFrame->GetColumnWidth(colIndex));
          }
          if (-1 == percentage) {
            percentage = 100 / mNumCols;
            // base the % on the remaining available width 
            mTableFrame->SetColumnWidth(colIndex, (percentage*aAvailWidth) / 100);
            TDBG_SDDDD("  3 percent default: col %d given %d percent of aAvailWidth %d, set to width = %d\n", 
                       colIndex, percentage, aAvailWidth, mTableFrame->GetColumnWidth(colIndex));
          }
          // if the column was computed to be too small, enlarge the column
          if (mTableFrame->GetColumnWidth(colIndex) <= minColWidth) {
            mTableFrame->SetColumnWidth(colIndex, minColWidth);
            TDBG_SD("  enlarging column to it's minimum = %d\n", minColWidth);
          }
        }
      }
    }
    else { // need to maintain this so we know how much we have left over at the end
      nscoord maxEffectiveColWidth = colFrame->GetEffectiveMaxColWidth();
      mTableFrame->SetColumnWidth(colIndex, maxEffectiveColWidth);
      widthOfFixedTableColumns += maxEffectiveColWidth + colInset;
    }
    tableWidth += mTableFrame->GetColumnWidth(colIndex) + colInset;
  }
  tableWidth += colInset;

  /* --- post-process if necessary --- */

  // XXX the following implementation does not account for borders, cell spacing, cell padding

  // first, assign column widths in auto-width tables
  PRInt32 numPercentColumns = 0;
  PRInt32* percentColumns=nsnull;
  mTableFrame->GetColumnsByType(eStyleUnit_Percent, numPercentColumns, percentColumns);
  if ((PR_TRUE == aTableIsAutoWidth) && (0 != numPercentColumns)) {
    // variables common to lots of code in this block
    nscoord colWidth;
    float percent;
    const nsStylePosition* colPosition;
    nsTableColFrame* colFrame;
    if (numPercentColumns != mNumCols) {
      TDBG_S("assigning widths to percent colums in auto-width table\n");
      nscoord widthOfPercentCells = 0;  // the total width of those percent-width cells that have been given a width
      nscoord widthOfOtherCells = 0;    // the total width of those non-percent-width cells that have been given a width
      float sumOfPercentColumns = 0.0f; // the total of the percent widths
      for (colIndex = 0; colIndex < mNumCols; colIndex++) {
        // every column contributes to either widthOfOtherCells or widthOfPercentCells, but not both
        if (PR_TRUE == IsColumnInList(colIndex, percentColumns, numPercentColumns)) {
          colFrame = mTableFrame->GetColFrame(colIndex); 
          colFrame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)colPosition);
          percent = colPosition->mWidth.GetPercentValue(); // we know this will work
          sumOfPercentColumns += percent;
          if (sumOfPercentColumns > 1.0f) { // values greater than 100% are meaningless
            sumOfPercentColumns = 1.0f; 
          }
          widthOfPercentCells += mTableFrame->GetColumnWidth(colIndex);
        }
        else {
          widthOfOtherCells += mTableFrame->GetColumnWidth(colIndex);
        }
      }
      if (0 == widthOfOtherCells) {
        widthOfOtherCells = aMaxWidth;
      }
      TDBG_SDDF("  widthOfOtherCells=%d widthOfPercentCells = %d sumOfPercentColumns=%f\n", 
                widthOfOtherCells, widthOfPercentCells, sumOfPercentColumns);
      float remainingPercent = 1.0f - sumOfPercentColumns;
      nscoord newTableWidth;  // the table width after all cells have been resized  
      if (1.0f == sumOfPercentColumns) { // this definately seems like a QUIRK!
        newTableWidth = aMaxWidth;
      }
      else { // the newTableWidth is the larger of the calculation from the percent cells and non-percent cells
        nscoord percentWidth = (nscoord)((1.0f/sumOfPercentColumns)*((float)(widthOfPercentCells)));
        nscoord otherWidth = (nscoord)((1.0f/remainingPercent)*((float)(widthOfOtherCells)));
        TDBG_SDD("    percentWidth=%d otherWidth=%d\n", percentWidth, otherWidth);
        newTableWidth = PR_MAX(percentWidth, otherWidth);     // the table width is the larger of the two computations
        newTableWidth = PR_MIN(newTableWidth, aMaxWidth);     // an auto-width table can't normally be wider than it's parent
        newTableWidth = PR_MIN(newTableWidth, mMaxTableWidth);// an auto-width table can't normally be wider than it's own computed max width 
      }
      nscoord excess = newTableWidth-mFixedTableWidth;      // the amount of new space that needs to be 
                                                            // accounted for in the non-fixed columns
      TDBG_SD("  newTableWidth=%d \n", newTableWidth);
      PRInt32 i;  // just a counter
      for (i = 0; i < numPercentColumns; i++) {
        colIndex = percentColumns[i];
        colFrame = mTableFrame->GetColFrame(colIndex);
        colFrame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)colPosition);
        percent = colPosition->mWidth.GetPercentValue(); // we know this will work
        colWidth = (nscoord)((percent) * ((float)newTableWidth));
        nscoord minColWidth = colFrame->GetEffectiveMinColWidth();
        colWidth = PR_MAX(colWidth, minColWidth);
        mTableFrame->SetColumnWidth(colIndex, colWidth);
        TDBG_SDFD("  col %d with percentwidth=%f set to %d\n", colIndex, percent, colWidth);
        excess -= colWidth;
      }
      if (0 < excess) {
        PRInt32 numAutoColumns=0;
        PRInt32* autoColumns=nsnull;
        mTableFrame->GetColumnsByType(eStyleUnit_Auto, numAutoColumns, autoColumns);
        TDBG_SDD("  excess=%d with %d autoColumns\n", excess, numAutoColumns);
        if (0 < numAutoColumns) {
          nscoord excessPerColumn = excess / numAutoColumns;
          for (i = 0; i < numAutoColumns; i++) {
            colIndex = autoColumns[i];
            nscoord newWidth = PR_MAX(excessPerColumn, mTableFrame->GetColumnWidth(colIndex));
            TDBG_SDDD("    col %d was %d set to %d\n", 
                      colIndex, mTableFrame->GetColumnWidth(colIndex), newWidth);
            mTableFrame->SetColumnWidth(colIndex, newWidth);
            excess -= excessPerColumn;
          }
          // handle division underflow
          if (0 < excess) {
            TDBG_SD("    after first pass through auto-width columns, excess=%d\n", excess);
            for (i = 0; i < numAutoColumns; i++) {
              colIndex = autoColumns[i];
              nscoord newWidth = 1 + mTableFrame->GetColumnWidth(colIndex);
              mTableFrame->SetColumnWidth(colIndex, newWidth);
            }
          }
        }
      }
    }
    else { 
      // all columns have a percent width.  Each already has its desired width.
      // find the smallest percentage and base the widths of the others off that
      PRInt32 indexOfSmallest;
      nscoord colWidthOfSmallest;
      float smallestPercent = 2.0f; // an illegal large number
      for (colIndex = 0; colIndex < mNumCols; colIndex++) {
        colFrame = mTableFrame->GetColFrame(colIndex); 
        colFrame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)colPosition);
        percent = colPosition->mWidth.GetPercentValue(); // we know this will work 
        if (percent < smallestPercent) {
          smallestPercent=percent;
          indexOfSmallest=colIndex;
          colWidthOfSmallest = mTableFrame->GetColumnWidth(colIndex);
        }
        else if (percent==smallestPercent) { 
          // the largest desired size among equal-percent columns wins
          nscoord colWidth = mTableFrame->GetColumnWidth(colIndex);
          if (colWidth > colWidthOfSmallest) {
            indexOfSmallest = colIndex;
            colWidthOfSmallest = colWidth;
          }
        }
      }
      // now that we know which column to base the other columns' widths off of, do it!
      for (colIndex = 0; colIndex < mNumCols; colIndex++) {
        colFrame = mTableFrame->GetColFrame(colIndex); 
        colFrame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)colPosition);
        percent = colPosition->mWidth.GetPercentValue(); // we know this will work 
        if (percent == smallestPercent) { // set col width to the max width of all columns that shared the smallest percent-width
          mTableFrame->SetColumnWidth(colIndex, colWidthOfSmallest);  
        }
        else {
          colWidth = (nscoord)((percent/smallestPercent) * ((float)colWidthOfSmallest));
          nscoord minColWidth = colFrame->GetEffectiveMinColWidth();
          colWidth = PR_MAX(colWidth, minColWidth);
          mTableFrame->SetColumnWidth(colIndex, colWidth); 
        }
        TDBG_SDFD("  col %d with percentwidth=%f set to %d\n", colIndex, percent, colWidth);
      }
    }
  }

  // second, assign column widths to proportional-width columns
  if (nsnull != proportionalColumnsList) {
    // first, figure out the amount of space per slice
    nscoord maxWidthForTable = (0 != aTableSpecifiedWidth) ? aTableSpecifiedWidth : aMaxWidth;
    if (NS_UNCONSTRAINEDSIZE != maxWidthForTable) {
      nscoord widthRemaining = maxWidthForTable - tableWidth;
      nscoord widthPerSlice = widthRemaining/totalSlices;
      PRInt32 numSpecifiedProportionalColumns = proportionalColumnsList->Count();
      for (PRInt32 i = 0; i < numSpecifiedProportionalColumns; i++) {
        // for every proportionally-sized column, set the col width to the computed width
        ProportionalColumnLayoutStruct* info = 
          (ProportionalColumnLayoutStruct *)(proportionalColumnsList->ElementAt(i));
        // verify that the computed width is at least the minimum width
        nscoord computedColWidth = info->mProportion*widthPerSlice;
        // compute the requested proportional width
        nsTableColFrame* colFrame;
        mTableFrame->GetColumnFrame(info->mColIndex, colFrame);
        nscoord minColWidth = colFrame->GetMinColWidth();
        computedColWidth = PR_MAX(computedColWidth, minColWidth);
        mTableFrame->SetColumnWidth(info->mColIndex, computedColWidth);
        TDBG_SDDDD("  3 proportional step 2: col %d given %d proportion of remaining space %d, set to width = %d\n", 
                   info->mColIndex, info->mProportion, widthRemaining, computedColWidth);
        tableWidth += computedColWidth;
        delete info;
      }
    }
    else { 
      // we're in an unconstrained situation, so give each column the max of the max column widths
      PRInt32 numSpecifiedProportionalColumns = proportionalColumnsList->Count();
      PRInt32 maxOfMaxColWidths = 0;
      PRInt32 i;
      for (i = 0; i < numSpecifiedProportionalColumns; i++) {
        ProportionalColumnLayoutStruct* info = 
          (ProportionalColumnLayoutStruct *)(proportionalColumnsList->ElementAt(i));
        maxOfMaxColWidths = PR_MAX(maxOfMaxColWidths, info->mMaxColWidth);
      }
      for (i = 0; i < numSpecifiedProportionalColumns; i++) {
        ProportionalColumnLayoutStruct* info = 
          (ProportionalColumnLayoutStruct *)(proportionalColumnsList->ElementAt(i));
        mTableFrame->SetColumnWidth(info->mColIndex, maxOfMaxColWidths);
        TDBG_SDD("  3 proportional step 2 (unconstrained!): col %d set to width = %d\n", 
                 info->mColIndex, maxOfMaxColWidths);
        tableWidth += maxOfMaxColWidths;
        nsTableColFrame* colFrame;
        mTableFrame->GetColumnFrame(info->mColIndex, colFrame);
        colFrame->SetEffectiveMaxColWidth(maxOfMaxColWidths);
        delete info;
      }
    }
    delete proportionalColumnsList;
  }

  // next, account for table width constraints
  // if the caption min width is wider than than the table, expand the table
  nscoord captionMinWidth = mTableFrame->GetMinCaptionWidth();
  if (captionMinWidth > tableWidth) {
    DistributeExcessSpace(captionMinWidth, tableWidth, widthOfFixedTableColumns);
  }
  else { 
    // else, if the caption min width is not an issue...
    // if the specified width of the table is greater than the table's computed width, expand the
    // table's computed width to match the specified width, giving the extra space to proportionately-sized
    // columns if possible. 
    if ((PR_FALSE == aTableIsAutoWidth) && (aAvailWidth > (tableWidth-widthOfFixedTableColumns)) &&
        (gBigSpace != aAvailWidth)) {
      DistributeExcessSpace(aAvailWidth, tableWidth, widthOfFixedTableColumns);
    }
    // IFF the table is NOT (auto-width && all columns have fixed width)
    PRInt32 numFixedColumns=0;
    PRInt32* fixedColumns=nsnull;
    mTableFrame->GetColumnsByType(eStyleUnit_Coord, numFixedColumns, fixedColumns);
    if (!((PR_TRUE == aTableIsAutoWidth) && (numFixedColumns == mNumCols))) {
      nscoord computedWidth = 0;
      for (PRInt32 i = 0; i < mNumCols; i++) {
        computedWidth += mTableFrame->GetColumnWidth(i) + colInset;
      }
      if (computedWidth > aMaxWidth) {
        AdjustTableThatIsTooWide(computedWidth, aMaxWidth, PR_FALSE);
      }
    }
  }

  // cleanup
  if (nsnull != spanList) {
    TDBG_S("BTLS::BCTFits...space leak, span list not empty");
    delete spanList;
  }
  return result;
}

// take the extra space in the table and distribute it proportionately (based on desired width)
// give extra space to auto-width cells first, or if there are none to all cells
void BasicTableLayoutStrategy::DistributeExcessSpace(nscoord  aAvailWidth,
                                                     nscoord  aTableWidth, 
                                                     nscoord  aWidthOfFixedTableColumns)
{
  nscoord excess = 0;
  TDBG_SDDD("DistributeExcessSpace: fixed width %d > computed table width %d, woftc=%d\n",
            aAvailWidth, aTableWidth, aWidthOfFixedTableColumns);
  nscoord  widthMinusFixedColumns = aTableWidth - aWidthOfFixedTableColumns;
  // if there are auto-sized columns, give them the extra space
  // the trick here is to do the math excluding non-auto width columns
  PRInt32 numAutoColumns=0;
  PRInt32* autoColumns=nsnull;
  GetColumnsThatActLikeAutoWidth(numAutoColumns, autoColumns);  // allocates autoColumns, be sure to delete it
  if (0 != numAutoColumns) {
    // there's at least one auto-width column, so give it (them) the extra space
    // proportionately distributed extra space, based on the column's desired size
    nscoord totalEffectiveWidthOfAutoColumns = 0;
    // 1. first, get the total width of the auto columns
    PRInt32 i;
    for (i = 0; i < numAutoColumns; i++) {
      nsTableColFrame* colFrame;
      mTableFrame->GetColumnFrame(autoColumns[i], colFrame);
      nscoord effectiveColWidth = colFrame->GetEffectiveMaxColWidth();
      if (0 != effectiveColWidth) {
        totalEffectiveWidthOfAutoColumns += effectiveColWidth;
      }
      else {
        totalEffectiveWidthOfAutoColumns += mTableFrame->GetColumnWidth(autoColumns[i]);
      }
    }
    // excess is the amount of space that was available minus the computed available width
    // XXX shouldn't it just be aMaxWidth - aTableWidth???
    TDBG_SDD("  aAvailWidth specified as %d, expanding columns by excess = %d\n", aAvailWidth, excess);
    excess = aAvailWidth - widthMinusFixedColumns;

    // 2. next, compute the proportion to be added to each column, and add it
    nscoord totalAdded=0;
    for (i = 0; i < numAutoColumns; i++) {
      PRInt32 colIndex = autoColumns[i];
      nsTableColFrame* colFrame;
      mTableFrame->GetColumnFrame(colIndex, colFrame);
      nscoord oldColWidth = mTableFrame->GetColumnWidth(colIndex);
      float percent;
      if (0 != totalEffectiveWidthOfAutoColumns) {
        percent = ((float)(colFrame->GetEffectiveMaxColWidth())) / ((float)totalEffectiveWidthOfAutoColumns);
      }
      else {
        percent = ((float)1 )/ ((float)numAutoColumns);
      }
      nscoord excessForThisColumn = (nscoord)(excess * percent);
      totalAdded += excessForThisColumn;
      nscoord colWidth = excessForThisColumn+oldColWidth;
      TDBG_SDDD("  distribute excess to auto columns:  column %d was %d, now set to %d\n", 
                colIndex, oldColWidth, colWidth);
      mTableFrame->SetColumnWidth(colIndex, colWidth);
    }
    TDBG_SDD("lost a few twips due to rounding errors: excess=%d, totalAdded=%d\n", excess, totalAdded);
  }
  // otherwise, distribute the space between all the columns 
  // (they must be all fixed and percentage-width columns, or we would have gone into the block above)
  else {
    excess = aAvailWidth - widthMinusFixedColumns;
    TDBG_SDD("  aAvailWidth specified as %d, expanding columns by excess = %d\n", aAvailWidth, excess);
    nscoord totalAdded=0;
    for (PRInt32 colIndex = 0; colIndex < mNumCols; colIndex++) {
      nscoord oldColWidth=0;
      if (0 == oldColWidth) {
        oldColWidth = mTableFrame->GetColumnWidth(colIndex);
      }
      float percent;
      if (0 != aTableWidth) {
        percent = (float)oldColWidth/(float)aTableWidth;
      }
      else {
        percent = (float)1 / (float)mNumCols;
      }
      nscoord excessForThisColumn = (nscoord)(NSToCoordRound(excess * percent));
      totalAdded += excessForThisColumn;
      nscoord colWidth = excessForThisColumn + oldColWidth;
      TDBG_SDDDF("  distribute excess: column %d was %d, now %d from %=%f\n", 
                 colIndex, oldColWidth, colWidth, percent);
      mTableFrame->SetColumnWidth(colIndex, colWidth);
    }
    TDBG_SDD("lost a few twips due to rounding errors: excess=%d, totalAdded=%d\n", excess, totalAdded);
  }
  if (nsnull != autoColumns) {
    delete [] autoColumns;
  }
}

/* assign columns widths for a table whose max size doesn't fit in the available space
 */
PRBool BasicTableLayoutStrategy::
             BalanceColumnsConstrained(const nsHTMLReflowState& aReflowState,
                                             nscoord            aAvailWidth,
                                             nscoord            aMaxWidth,
                                             PRBool             aTableIsAutoWidth)
{
#ifdef NS_DEBUG
  nsIFrame* tablePIF=nsnull;
  mTableFrame->GetPrevInFlow(&tablePIF);
  NS_ASSERTION(nsnull==tablePIF, "never ever call me on a continuing frame!");
#endif

  PRBool result = PR_TRUE;
  PRInt32 maxOfAllMinColWidths = 0;   // for the case where we have equal column widths, this is the smallest a column can be
  nscoord tableWidth           = 0;   // the width of the table as a result of setting column widths
  PRInt32 totalSlices          = 0;   // the total number of slices the proportional-width columns request
  nsVoidArray* proportionalColumnsList=nsnull; // a list of the columns that are proportional-width
  PRBool equalWidthColumns = PR_TRUE; // remember if we're in the special case where all
                                      // proportional-width columns are equal (if any are anything other than 1)
  PRBool atLeastOneAutoWidthColumn = PR_FALSE;  // true if at least one column is auto-width, requiring us to post-process
  nsVoidArray* spanList=nsnull;       // a list of the cells that span columns
  PRInt32 numRows = mTableFrame->GetRowCount();
  nscoord colInset = mTableFrame->GetCellSpacingX();

  for (PRInt32 colIndex = 0; colIndex < mNumCols; colIndex++) { 
    TDBG_SD("  for col %d\n", colIndex);
    nscoord minColWidth = 0;
    nscoord maxColWidth = 0;
    
    // Get column information
    nsTableColFrame* colFrame = mTableFrame->GetColFrame(colIndex);
    NS_ASSERTION(nsnull != colFrame, "bad col frame");
    // Get the columns's style and margins
    PRInt32 rowIndex;
    PRInt32 firstRowIndex = -1;
    const nsStylePosition* colPosition;
    colFrame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)colPosition);
    const nsStyleTable* colTableStyle;
    colFrame->GetStyleData(eStyleStruct_Table, (const nsStyleStruct*&)colTableStyle);

    // first, deal with any cells that span into this column from a pervious column
      // go through the list backwards so we can delete easily
    if (nsnull != spanList) {
      PRInt32 spanCount = spanList->Count();
      // go through the list backwards so we can delete easily
      for (PRInt32 spanIndex=spanCount-1; 0<=spanIndex; spanIndex--) {
        SpanInfo* spanInfo = (SpanInfo *)(spanList->ElementAt(spanIndex));
        if (PR_FALSE == IsFixedWidth(colPosition, colTableStyle)) {
          // compute the spanning cell's contribution to the column min width
          nscoord spanCellMinWidth;
          PRBool needsExtraMinWidth = PR_FALSE;
          //if (spanInfo->effectiveMinWidthOfSpannedCols<spanInfo->cellMinWidth)
          //  needsExtraMinWidth = PR_TRUE;
          if (PR_TRUE == needsExtraMinWidth) {
            if (0 != spanInfo->effectiveMinWidthOfSpannedCols) {
              spanCellMinWidth = (spanInfo->cellMinWidth * colFrame->GetEffectiveMinColWidth()) /
                                 (spanInfo->effectiveMinWidthOfSpannedCols);
              TDBG_SDD("    spanlist min: %d of %d\n", spanCellMinWidth, spanInfo->effectiveMaxWidthOfSpannedCols);
              if (minColWidth < spanCellMinWidth) {
                minColWidth = spanCellMinWidth;     // set the new min width for the col
                TDBG_SDDD("    for spanning cell into col %d with remaining span=%d, new min = %d\n", 
                          colIndex, spanInfo->span, minColWidth);
                maxColWidth = PR_MAX(maxColWidth, minColWidth);
                TDBG_SD("    maxColWidth = %d\n", maxColWidth);
              }
            }
            else {
              spanCellMinWidth = spanInfo->cellMinWidth / spanInfo->initialColSpan;
              if (minColWidth < spanCellMinWidth) {
                minColWidth = spanCellMinWidth;
                TDBG_SDDD("    for spanning cell into col %d with remaining span=%d, new min = %d\n", 
                          colIndex, spanInfo->span, minColWidth);
                maxColWidth = PR_MAX(maxColWidth, minColWidth);
                TDBG_SD("    maxColWidth = %d\n", maxColWidth);  
              }
            }
          }


          // compute the spanning cell's contribution to the column max width
          nscoord spanCellMaxWidth;
          if (0 != spanInfo->effectiveMaxWidthOfSpannedCols) {
            spanCellMaxWidth = (spanInfo->cellDesiredWidth * colFrame->GetEffectiveMinColWidth()) /
                               (spanInfo->effectiveMaxWidthOfSpannedCols);
            TDBG_SDD("    spanlist max: %d of %d\n", spanCellMaxWidth, spanInfo->effectiveMaxWidthOfSpannedCols);
            if (maxColWidth < spanCellMaxWidth) {
              maxColWidth = spanCellMaxWidth;                 // set the new max width for the col
              mTableFrame->SetColumnWidth(colIndex, spanCellMaxWidth);  // set the column to the new desired max width
              TDBG_SDDD("    for spanning cell into col %d with remaining span=%d, new max = %d\n", 
                        colIndex, spanInfo->span, maxColWidth);
            }
          }
          else {
            spanCellMaxWidth = spanInfo->cellDesiredWidth/spanInfo->initialColSpan;
            maxColWidth = spanCellMaxWidth;
            mTableFrame->SetColumnWidth(colIndex, spanCellMaxWidth);
            TDBG_SDDD("    for spanning cell into col %d with remaining span=%d, new max = %d\n", 
                      colIndex, spanInfo->span, maxColWidth);
          }
        }
        spanInfo->span--;
        if (0 == spanInfo->span) {
          spanList->RemoveElementAt(spanIndex);
          delete spanInfo;
        }
      }
    }

    // second, process non-fixed-width columns
    if (PR_FALSE == IsFixedWidth(colPosition, colTableStyle)) {
      for (rowIndex = 0; rowIndex < numRows; rowIndex++) {
        nsTableCellFrame* cellFrame = mTableFrame->GetCellFrameAt(rowIndex, colIndex);
        if (nsnull == cellFrame) { // there is no cell in this row that corresponds to this column
          continue;
        }
        PRInt32 cellRowIndex;
        cellFrame->GetRowIndex(cellRowIndex);
        if (rowIndex!=cellRowIndex) {
          // For cells that span rows, we only figure it in once
          NS_ASSERTION(1 != cellFrame->GetRowSpan(), "row index does not match row span");  // sanity check
          continue;
        }
        PRInt32 cellColIndex;
        cellFrame->GetColIndex(cellColIndex);
        if (colIndex!=cellColIndex) {
          // For cells that span cols, we figure in the row using previously-built SpanInfo 
          NS_ASSERTION(1 != cellFrame->GetColSpan(), "col index does not match row span");  // sanity check
          continue;
        }

        PRInt32 colSpan = mTableFrame->GetEffectiveColSpan(colIndex, cellFrame);
        nsSize cellMinSize = cellFrame->GetPass1MaxElementSize();
        nsSize cellDesiredSize = cellFrame->GetPass1DesiredSize();

        nscoord cellMinWidth=colFrame->GetAdjustedMinColWidth();
        nscoord cellDesiredWidth=colFrame->GetMaxColWidth(); 
        if (1 == colSpan) {
          cellMinWidth = cellMinSize.width;
          cellDesiredWidth = cellDesiredSize.width;  
          nscoord specifiedCellWidth = -1;
          const nsStylePosition* cellPosition;
          cellFrame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)cellPosition);
          if (eStyleUnit_Percent==cellPosition->mWidth.GetUnit()) {
            float percent = cellPosition->mWidth.GetPercentValue();
            specifiedCellWidth = (PRInt32)(aMaxWidth * percent);
            TDBG_SFDD("specified percent width %f of %d = %d\n", percent, aMaxWidth, specifiedCellWidth);
          }
          if (-1 != specifiedCellWidth) {
            if (specifiedCellWidth>cellMinWidth) {
              TDBG_SDD("setting cellDesiredWidth from %d to %d\n", cellDesiredWidth, specifiedCellWidth);
              cellDesiredWidth = specifiedCellWidth;
            }
          }
        }
        else { // colSpan>1, get the proportion for this column
          // then get the desired size info factoring in the cell style attributes
          nscoord effectiveMaxWidthOfSpannedCols = colFrame->GetEffectiveMaxColWidth();
          nscoord effectiveMinWidthOfSpannedCols = colFrame->GetEffectiveMinColWidth();
          for (PRInt32 span = 1; span < colSpan; span++) {
            nsTableColFrame* nextColFrame = mTableFrame->GetColFrame(colIndex+span);
            if (nsnull == nextColFrame) {
              break;
            }
            effectiveMaxWidthOfSpannedCols += nextColFrame->GetEffectiveMaxColWidth();
            effectiveMinWidthOfSpannedCols += nextColFrame->GetEffectiveMinColWidth();
          }
          nscoord specifiedCellWidth = -1;
          const nsStylePosition* cellPosition;
          cellFrame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)cellPosition);
          if (eStyleUnit_Percent == cellPosition->mWidth.GetUnit()) {
            //XXX what if table is auto width?
            float percent = cellPosition->mWidth.GetPercentValue();
            specifiedCellWidth = (PRInt32)(aMaxWidth * percent);
            TDBG_SFDD("specified percent width %f of %d = %d\n", percent, aMaxWidth, specifiedCellWidth);
          }
          if (-1 != specifiedCellWidth) {
            float percentForThisCol = (float)(cellDesiredSize.width * colFrame->GetEffectiveMaxColWidth()) /
                                      (float)effectiveMaxWidthOfSpannedCols;
            nscoord cellWidthForThisCol = (nscoord)(specifiedCellWidth * percentForThisCol);
            if (cellWidthForThisCol>cellMinWidth) {
              TDBG_SDD("setting cellDesiredWidth from %d to %d\n", cellDesiredWidth, cellWidthForThisCol);
              cellDesiredWidth = cellWidthForThisCol;
            }
          }
          // otherwise it's already been factored in.
          // now, if this column holds the cell, create a spanInfo struct for the cell
          // so subsequent columns can take a proportion of this cell's space into account
          PRInt32 cellColIndex;
          cellFrame->GetColIndex(cellColIndex);
          if (cellColIndex == colIndex) {
            // add this cell to span list iff we are currently processing the column the cell starts in
            SpanInfo* spanInfo = new SpanInfo(colIndex, colSpan-1, cellMinSize.width, cellDesiredSize.width);
            spanInfo->effectiveMaxWidthOfSpannedCols = effectiveMaxWidthOfSpannedCols;
            spanInfo->effectiveMinWidthOfSpannedCols = effectiveMinWidthOfSpannedCols;
            if (nsnull == spanList) {
              spanList = new nsVoidArray();
            }
            spanList->AppendElement(spanInfo);
          }
        }

        TDBG_SDDDD("factoring in cell %d with colSpan=%d\n  factoring in min=%d and desired=%d\n", 
                   rowIndex, colSpan, cellMinWidth, cellDesiredWidth);
        // remember the widest min cell width
        minColWidth = PR_MAX(minColWidth, cellMinWidth);
        // remember the max desired cell width
        maxColWidth = PR_MAX(maxColWidth, cellDesiredWidth);
        // effectiveMaxColumnWidth is the width as if no cells with colspans existed
        // if ((1==colSpan) && (colFrame->GetEffectiveMaxColWidth() < maxColWidth))
        //   colFrame->SetEffectiveMaxColWidth(cellDesiredWidth);
        TDBG_SDDDDDD("    after cell %d, minColWidth=%d  maxColWidth=%d  effColWidth[%d]=%d,%d\n", 
                     rowIndex, minColWidth, maxColWidth, colIndex, 
                     colFrame->GetEffectiveMinColWidth(), colFrame->GetEffectiveMaxColWidth());
      }
      if (PR_TRUE == gsDebug) { 
         printf ("  for determining width of col %d %s:\n", 
            colIndex, !IsFixedWidth(colPosition, colTableStyle)? "(P)":"(A)"); 
         printf ("    minTableWidth = %d and maxTableWidth = %d\n",  
            mMinTableWidth, mMaxTableWidth); 
         printf ("    minColWidth = %d, maxColWidth = %d, eff=%d,%d\n", 
            minColWidth, maxColWidth, colFrame->GetEffectiveMinColWidth(), 
            colFrame->GetEffectiveMaxColWidth()); 
         printf ("    aAvailWidth = %d\n", aAvailWidth); 
      }


      // Get column width if it has one
      nscoord specifiedProportionColumnWidth = -1;
      float specifiedPercentageColWidth = -1.0f;
      PRBool isAutoWidth = PR_FALSE;
      switch (colPosition->mWidth.GetUnit()) {
        case eStyleUnit_Percent:
          specifiedPercentageColWidth = colPosition->mWidth.GetPercentValue();
          TDBG_SDF("column %d has specified percent width = %f\n", colIndex, specifiedPercentageColWidth);
          break;
        case eStyleUnit_Proportional:
          specifiedProportionColumnWidth = colPosition->mWidth.GetIntValue();
          TDBG_SDD("column %d has specified percent width = %d\n", colIndex, specifiedProportionColumnWidth);
          break;
        case eStyleUnit_Auto:
          isAutoWidth = PR_TRUE;
          break;
        default:
          break;
      }

      // set the column width, knowing that the table is constrained  
      if (0 == specifiedProportionColumnWidth || 0.0 == specifiedPercentageColWidth) {
        // col width is specified to be the minimum
        mTableFrame->SetColumnWidth(colIndex, minColWidth);
        TDBG_SDD("  4 (0): col %d set to min width = %d because style set proportionalWidth=0\n", 
                 colIndex, mTableFrame->GetColumnWidth(colIndex));
      }
      else if (1 == mNumCols) { // there is only one column, and we know that it's desired width doesn't fit
        // so the column should be as wide as the available space allows it to be minus cell spacing
        nscoord colWidth = aAvailWidth - (2*colInset);
        mTableFrame->SetColumnWidth(colIndex, colWidth);
        TDBG_SDDDD("  4 one-col:  col %d set to width = %d from available width %d and cell spacing %d\n", 
                   colIndex, mTableFrame->GetColumnWidth(colIndex), aAvailWidth, colInset);
      }
      else if (PR_TRUE==isAutoWidth) { // column's width is determined by its content, done in post-processing
        mTableFrame->SetColumnWidth(colIndex, minColWidth); // reserve the column's min width
        atLeastOneAutoWidthColumn = PR_TRUE;
      }
      else if (-1 != specifiedProportionColumnWidth) {
        // we need to save these and do them after all other columns have been calculated
        // the calculation will be: 
        //    sum up n, the total number of slices for the columns with proportional width
        //    compute the table "required" width, fixed-width + percentage-width +
        //      the sum of the proportional column's max widths (especially because in this routine I know the table fits)
        //    compute the remaining width: the required width - the used width (fixed + percentage)
        //    compute the width per slice
        //    set the width of each proportional-width column to it's number of slices * width per slice
        mTableFrame->SetColumnWidth(colIndex, 0);   // set the column width to 0, since it isn't computed yet
        if (nsnull == proportionalColumnsList) {
          proportionalColumnsList = new nsVoidArray();
        }
        ProportionalColumnLayoutStruct* info = 
          new ProportionalColumnLayoutStruct(colIndex, minColWidth, maxColWidth, specifiedProportionColumnWidth);
        proportionalColumnsList->AppendElement(info);
        totalSlices += specifiedProportionColumnWidth;
        if (1 != specifiedProportionColumnWidth) {
          equalWidthColumns = PR_FALSE;
        }
      }
      else {  // give the column a percentage of the remaining space
        PRInt32 percentage = -1;
        if (NS_UNCONSTRAINEDSIZE == aAvailWidth) { 
          // since the "remaining space" is infinite, give the column it's max requested size
          mTableFrame->SetColumnWidth(colIndex, maxColWidth);
        }
        else {
          if (-1.0f != specifiedPercentageColWidth) {
            percentage = (PRInt32)(specifiedPercentageColWidth * 100.0f); // TODO: rounding errors?
            // base the % on the total specified fixed width of the table
            mTableFrame->SetColumnWidth(colIndex, (percentage*aMaxWidth)/100);
            TDBG_SDDDD("  4 percent specified: col %d given %d percent of aMaxWidth %d, set to width = %d\n", 
                       colIndex, percentage, aMaxWidth, mTableFrame->GetColumnWidth(colIndex));
          }
          if (-1 == percentage) {
            percentage = 100 / mNumCols;
            // base the % on the remaining available width 
            mTableFrame->SetColumnWidth(colIndex, (percentage * aAvailWidth) / 100);
            TDBG_SDDDD("  4 percent default: col %d given %d percent of aAvailWidth %d, set to width = %d\n", 
                       colIndex, percentage, aAvailWidth, mTableFrame->GetColumnWidth(colIndex));
          }
          // if the column was computed to be too small, enlarge the column
          if (mTableFrame->GetColumnWidth(colIndex) <= minColWidth) {
            mTableFrame->SetColumnWidth(colIndex, minColWidth);
            TDBG_SD("  enlarging column to it's minimum = %d\n", minColWidth);
            if (maxOfAllMinColWidths < minColWidth) {
              maxOfAllMinColWidths = minColWidth;
              TDBG_SD("   and setting maxOfAllMins to %d\n", maxOfAllMinColWidths);
            }
          }
        }
      }
    }
    else {
      mTableFrame->SetColumnWidth(colIndex, colFrame->GetMaxColWidth());
    }
    tableWidth += mTableFrame->GetColumnWidth(colIndex) + colInset;
  }
  tableWidth += colInset;
  // --- post-process if necessary --- /
  // first, assign autoWidth columns a width
  if (PR_TRUE == atLeastOneAutoWidthColumn) {
    // proportionately distribute the remaining space to autowidth columns
    // "0" for the last param tells DistributeRemainingSpace that this is the top (non-recursive) call
    PRInt32 topRecursiveControl=0;
    DistributeRemainingSpace(aMaxWidth, tableWidth, aTableIsAutoWidth, topRecursiveControl);
  }
  
  // second, fix up tables where column width attributes give us a table that is too wide or too narrow
  nscoord computedWidth = colInset;
  for (PRInt32 i = 0; i < mNumCols; i++) {
    computedWidth += mTableFrame->GetColumnWidth(i) + colInset;
  }
  if (computedWidth<aMaxWidth)  { 
    // then widen the table because it's too narrow
    // do not widen auto-width tables, they shrinkwrap to their content's width
    if (PR_FALSE == aTableIsAutoWidth) {
      AdjustTableThatIsTooNarrow(computedWidth, aMaxWidth);
    }
  }
  else if (computedWidth>aMaxWidth) {
    // then shrink the table width because its too wide
    AdjustTableThatIsTooWide(computedWidth, aMaxWidth, PR_FALSE);
  }


  // finally, assign a width to proportional-width columns
  if (nsnull!=proportionalColumnsList) {
    // first, figure out the amount of space per slice
    nscoord widthRemaining = aMaxWidth - tableWidth;
    nscoord widthPerSlice = widthRemaining / totalSlices;
    PRInt32 numSpecifiedProportionalColumns = proportionalColumnsList->Count();
    for (PRInt32 i = 0; i < numSpecifiedProportionalColumns; i++) {
      ProportionalColumnLayoutStruct* info = 
        (ProportionalColumnLayoutStruct *)(proportionalColumnsList->ElementAt(i));
      if (PR_TRUE == equalWidthColumns && 0 != maxOfAllMinColWidths) {
        TDBG_SDD("  EqualColWidths specified and some column couldn't fit, so setting col %d width to %d\n", 
                 info->mColIndex, maxOfAllMinColWidths);
        mTableFrame->SetColumnWidth(info->mColIndex, maxOfAllMinColWidths);
      }
      else {
        // compute the requested proportional width
        nscoord computedColWidth = info->mProportion * widthPerSlice;
        // verify that the computed width is at least the minimum width
        nsTableColFrame* colFrame;
        mTableFrame->GetColumnFrame(info->mColIndex, colFrame);
        nscoord minColWidth = colFrame->GetMinColWidth();
        computedColWidth = PR_MAX(computedColWidth, minColWidth);
        mTableFrame->SetColumnWidth(info->mColIndex, computedColWidth);
        TDBG_SDDDD("  4 proportion: col %d given %d proportion of remaining space %d, set to width = %d\n", 
                   info->mColIndex, info->mProportion, widthRemaining, computedColWidth);
      }
      delete info;
    }
    delete proportionalColumnsList;
  }

  // clean up
  if (nsnull != spanList) {
    TDBG_S("BTLS::BCTConstrained...space leak, span list not empty");
    delete spanList;
  }

  return result;
}

static const PRInt32 kRecursionLimit = 10;  // backwards compatible with Nav4

// take the remaining space in the table and distribute it proportionately 
// to the auto-width cells in the table (based on desired width)
void BasicTableLayoutStrategy::DistributeRemainingSpace(nscoord  aTableSpecifiedWidth,
                                                        nscoord& aComputedTableWidth, 
                                                        PRBool   aTableIsAutoWidth,
                                                        PRInt32& aRecursionControl)
{
  aRecursionControl++;
  if (kRecursionLimit <= aRecursionControl) { // only allow kRecursionLimit iterations, as per Nav4. See laytable.c
    return;
  }
  nscoord sumOfMinWidths = 0;   // sum of min widths of each auto column
  nscoord startingComputedTableWidth = aComputedTableWidth;  // remember this so we can see if we're making any progress
  TDBG_SDD("DistributeRemainingSpace: fixed width %d > computed table width %d\n",
           aTableSpecifiedWidth, aComputedTableWidth);
  // if there are auto-sized columns, give them the extra space
  PRInt32 numAutoColumns = 0;
  PRInt32* autoColumns=nsnull;
  mTableFrame->GetColumnsByType(eStyleUnit_Auto, numAutoColumns, autoColumns);
  if (0 != numAutoColumns) {
    PRInt32 numColumnsToBeResized = 0;
    // there's at least one auto-width column, so give it (them) the extra space
    // proportionately distributed extra space, based on the column's desired size
    nscoord totalEffectiveWidthOfAutoColumns = 0;
    // 1. first, get the total width of the auto columns
    PRInt32 i;
    for (i = 0; i < numAutoColumns; i++) {
      PRInt32 colIndex = autoColumns[i];
      nsTableColFrame* colFrame = mTableFrame->GetColFrame(autoColumns[i]);
      nscoord startingColWidth = mTableFrame->GetColumnWidth(colIndex);
      nscoord maxEffectiveColWidth = colFrame->GetEffectiveMaxColWidth();
      if ((PR_FALSE == aTableIsAutoWidth) || (startingColWidth<maxEffectiveColWidth)) {
        numColumnsToBeResized++;
        if (0 != maxEffectiveColWidth) {
          totalEffectiveWidthOfAutoColumns += maxEffectiveColWidth;
        }
        else {
          totalEffectiveWidthOfAutoColumns += mTableFrame->GetColumnWidth(autoColumns[i]);
        }
      }
    }
    // availWidth is the difference between the total available width and the 
    // amount of space already assigned, assuming auto col widths were assigned 0.
    nscoord availWidth;
    availWidth = aTableSpecifiedWidth - aComputedTableWidth;
    TDBG_SDD("  aTableSpecifiedWidth specified as %d, availWidth is = %d\n", aTableSpecifiedWidth, availWidth);
    
    // 2. next, compute the proportion to be added to each column, and add it
    for (i = 0; i < numAutoColumns; i++) {
      PRInt32 colIndex = autoColumns[i];
      nsTableColFrame* colFrame = mTableFrame->GetColFrame(colIndex);
      nscoord startingColWidth = mTableFrame->GetColumnWidth(colIndex);
      nscoord maxEffectiveColWidth = colFrame->GetEffectiveMaxColWidth();
      // if we actually have room to distribute, do it here
      // otherwise, the auto columns already are set to their minimum
      if (0 < availWidth) {
        if ((PR_FALSE == aTableIsAutoWidth) || (startingColWidth<maxEffectiveColWidth)) {
          float percent;
          if (0 != aTableSpecifiedWidth) {
            percent = ((float)maxEffectiveColWidth)/((float)totalEffectiveWidthOfAutoColumns);
          }
          else {
            percent = ((float)1) / ((float)numColumnsToBeResized);
          }
          nscoord colWidth = startingColWidth + NSToCoordRound(((float)(availWidth)) * percent);
          TDBG_SDDDD("  colWidth = %d from startingColWidth %d plus %d percent of availWidth %d\n",
                     colWidth, startingColWidth, (nscoord)((float)100*percent), availWidth);
          // in an auto width table, the column cannot be wider than its max width
          if (PR_TRUE == aTableIsAutoWidth) {
            // since the table shrinks to the content width, don't be wider than the content max width
            colWidth = PR_MIN(colWidth, colFrame->GetMaxColWidth());
          }
          aComputedTableWidth += colWidth - startingColWidth;
          TDBG_SDDD("  distribute width to auto columns:  column %d was %d, now set to %d\n", 
                    colIndex, colFrame->GetEffectiveMaxColWidth(), colWidth);
          mTableFrame->SetColumnWidth(colIndex, colWidth);
        }
      }
    }

    if (aComputedTableWidth!=startingComputedTableWidth) {  
      // othewise we made no progress and shouldn't continue
      if (aComputedTableWidth < aTableSpecifiedWidth) {
        DistributeRemainingSpace(aTableSpecifiedWidth, aComputedTableWidth, aTableIsAutoWidth, aRecursionControl);
      }
    }
  }
  TDBG_WIDTHS4("at end of DistributeRemainingSpace: ",PR_FALSE,PR_FALSE);
}


void BasicTableLayoutStrategy::AdjustTableThatIsTooWide(nscoord aComputedWidth, 
                                                        nscoord aTableWidth, 
                                                        PRBool  aShrinkFixedCols)
{
  TDBG_WIDTHS4("before AdjustTableThatIsTooWide: ",PR_FALSE,PR_FALSE);

  PRInt32 numFixedColumns = 0;
  PRInt32* fixedColumns = nsnull;
  mTableFrame->GetColumnsByType(eStyleUnit_Coord, numFixedColumns, fixedColumns);
  PRInt32 numAutoColumns = 0;
  PRInt32* autoColumns = nsnull;
  mTableFrame->GetColumnsByType(eStyleUnit_Auto, numAutoColumns, autoColumns);
  nscoord excess = aComputedWidth - aTableWidth;
  nscoord minDiff;    // the smallest non-zero delta between a column's current width and its min width
  PRInt32* colsToShrink = new PRInt32[mNumCols];
  // while there is still extra computed space in the table
  while (0 < excess) {
    // reinit state variables
    PRInt32 colIndex;
    for (colIndex = 0; colIndex < mNumCols; colIndex++) {
      colsToShrink[colIndex] = 0;
    }
    minDiff = 0;

    // determine what columns we can remove width from
    PRInt32 numColsToShrink = 0;
    PRBool shrinkAutoOnly = PR_TRUE;
    PRBool keepLooking = PR_TRUE;
    while (PR_TRUE == keepLooking) {
      for (colIndex = 0; colIndex < mNumCols; colIndex++) {
        nscoord currentColWidth = mTableFrame->GetColumnWidth(colIndex);
        nsTableColFrame* colFrame;
        mTableFrame->GetColumnFrame(colIndex, colFrame);
        nscoord minColWidth = colFrame->GetAdjustedMinColWidth();
        if (currentColWidth == minColWidth) {
          continue;
        }
        if ((PR_FALSE==aShrinkFixedCols) && 
            (PR_TRUE==IsColumnInList(colIndex, fixedColumns, numFixedColumns))) {
          continue;
        }
        if ((PR_TRUE==shrinkAutoOnly) && 
            (PR_FALSE==IsColumnInList(colIndex, autoColumns, numAutoColumns))) {
          continue;
        }
        colsToShrink[numColsToShrink] = colIndex;
        numColsToShrink++;
        nscoord diff = currentColWidth - minColWidth;
        if ((0 == minDiff) || (diff < minDiff)) {
          minDiff = diff;
        }
      }
      if (PR_FALSE == shrinkAutoOnly) {
        keepLooking = PR_FALSE; // we've looked everywhere, so bail.  this breaks us out of the loop
      }
      if (0 != numColsToShrink) {
        keepLooking = PR_FALSE; // we found at least one column to shrink, so bail.  this breaks us out of the loop
      }
      shrinkAutoOnly = PR_FALSE;// this guarantees we'll go through this loop only one more time
    }
    // if there are no columns we can remove space from, we're done
    if (0 == numColsToShrink) {
      break;
    }
    
    // determine the amount to remove from each column
    nscoord excessPerColumn;
    if (excess < numColsToShrink) {
      excessPerColumn = 1;
    }
    else {
      excessPerColumn = excess / numColsToShrink;  // guess we can remove as much as we want
    }
    if (excessPerColumn > minDiff) {               // then adjust for minimum col widths
      excessPerColumn = minDiff;
    }
    // remove excessPerColumn from every column we've determined we can remove width from
    for (colIndex = 0; colIndex < mNumCols; colIndex++) {
      if ((PR_TRUE == IsColumnInList(colIndex, colsToShrink, numColsToShrink))) {
        nscoord colWidth = mTableFrame->GetColumnWidth(colIndex);
        colWidth -= excessPerColumn;
        mTableFrame->SetColumnWidth(colIndex, colWidth);
        excess -= excessPerColumn;
        if (0 == excess) {
          break;
        }
      }
    }
  } // end while (0 < excess)
  TDBG_WIDTHS4("after AdjustTableThatIsTooWide: ",PR_TRUE,aShrinkFixedCols);

  delete [] colsToShrink;

  // deal with any excess left over 
  if ((PR_FALSE == aShrinkFixedCols) && (0 != excess)) {
    // if there's any excess left, we know we've shrunk every non-fixed column to its min
    // so we have to shrink fixed width columns if possible
    AdjustTableThatIsTooWide(aComputedWidth, aTableWidth, PR_TRUE);
  }
  // otherwise we've shrunk the table to its min, and that's all we can do
}


void BasicTableLayoutStrategy::AdjustTableThatIsTooNarrow(nscoord aComputedWidth, 
                                                          nscoord aTableWidth)
{
  TDBG_WIDTHS4("before AdjustTableThatIsTooNarrow: ",PR_FALSE,PR_FALSE);

  PRInt32 numFixedColumns=0;
  PRInt32* fixedColumns=nsnull;
  mTableFrame->GetColumnsByType(eStyleUnit_Coord, numFixedColumns, fixedColumns);
  nscoord excess = aTableWidth - aComputedWidth;
  while (0 < excess) {
    PRInt32 colIndex;
    PRInt32* colsToGrow = new PRInt32[mNumCols];
    // determine what columns we can take add width to
    PRInt32 numColsToGrow = 0;
    PRBool expandFixedCols = PRBool(mNumCols == numFixedColumns);
    for (colIndex = 0; colIndex < mNumCols; colIndex++) {
      if ((PR_FALSE == expandFixedCols) && 
        (PR_TRUE == IsColumnInList(colIndex, fixedColumns, numFixedColumns))) {
          // skip fixed-width cells if we're told to
        continue;
      }
      if (PR_TRUE == ColIsSpecifiedAsMinimumWidth(colIndex)) {
        // skip columns that are forced by their attributes to be their minimum width
        continue;
      }
      colsToGrow[numColsToGrow] = colIndex;
      numColsToGrow++;
    }    
    if (0 != numColsToGrow) {
      nscoord excessPerColumn;
      if (excess<numColsToGrow) {
        excessPerColumn = 1;
      }
      else {
        excessPerColumn = excess / numColsToGrow;
      }
      for (colIndex = 0; colIndex < mNumCols; colIndex++) {
        if ((PR_TRUE == IsColumnInList(colIndex, colsToGrow, numColsToGrow))) {
          nsTableColFrame* colFrame = mTableFrame->GetColFrame(colIndex);
          nscoord colWidth = mTableFrame->GetColumnWidth(colIndex);
          colWidth += excessPerColumn;
          if (colWidth > colFrame->GetMinColWidth()) {
            excess -= excessPerColumn;
            mTableFrame->SetColumnWidth(colIndex, colWidth);
          }
          else {
            excess -= mTableFrame->GetColumnWidth(colIndex) - colFrame->GetMinColWidth();   
            mTableFrame->SetColumnWidth(colIndex, colFrame->GetMinColWidth());
          }
          if (0 >= excess) {
            break;
          }
        }
      }
    }
    delete [] colsToGrow;
    if (0 == numColsToGrow) {
      break;
    }
  } // end while (0 < excess)

  TDBG_WIDTHS4("after AdjustTableThatIsTooNarrow: ",PR_FALSE,PR_FALSE);
}

PRBool BasicTableLayoutStrategy::IsColumnInList(const PRInt32 colIndex, 
                                                PRInt32*      colIndexes, 
                                                PRInt32       aNumFixedColumns)
{
  PRBool result = PR_FALSE;
  for (PRInt32 i = 0; i < aNumFixedColumns; i++) {
    if (colIndex == colIndexes[i]) {
      result = PR_TRUE;
      break;
    }
    else if (colIndex<colIndexes[i]) {
      break;
    }
  }
  return result;
}

PRBool BasicTableLayoutStrategy::ColIsSpecifiedAsMinimumWidth(PRInt32 aColIndex)
{
  PRBool result = PR_FALSE;
  nsTableColFrame* colFrame;
  mTableFrame->GetColumnFrame(aColIndex, colFrame);
  const nsStylePosition* colPosition;
  colFrame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)colPosition);
  switch (colPosition->mWidth.GetUnit()) {
  case eStyleUnit_Coord:
    if (0 == colPosition->mWidth.GetCoordValue()) {
      result = PR_TRUE;
    }
    break;
  case eStyleUnit_Percent:
    {
      // total hack for now for 0% and 1% specifications
      // should compare percent to available parent width and see that it is below minimum
      // for this column
      float percent = colPosition->mWidth.GetPercentValue();
      if (0.0f == percent || 0.01f == percent) {  
        result = PR_TRUE;
      }
      break;
    }
  case eStyleUnit_Proportional:
    if (0 == colPosition->mWidth.GetIntValue()) {
      result = PR_TRUE;
    }

  default:
    break;
  }

  return result;
}

// returns a list and count of all columns that behave like they have width=auto
// this includes columns with no width specified, and columns whose fixed width comes from a span
void BasicTableLayoutStrategy::GetColumnsThatActLikeAutoWidth(PRInt32&  aNumCols,  
                                                              PRInt32*& aColList)
{
  // initialize out params
  aNumCols = 0;
  aColList = nsnull;

  // get the auto columns
  PRInt32 numAutoCols=0;
  PRInt32* autoColList=nsnull;
  mTableFrame->GetColumnsByType(eStyleUnit_Auto, numAutoCols, autoColList);

  // XXX: have to do next step for %-width as well?

  // get the fixed columns 
  PRInt32 numFixedCols=0;
  PRInt32* fixedColList=nsnull;
  mTableFrame->GetColumnsByType(eStyleUnit_Coord, numFixedCols, fixedColList);

  if (0 < numAutoCols+numFixedCols) {
    // allocate the out param aColList to the biggest it could be (usually a little wasteful, but it's 
    // temp space and the numbers here are small, even for very big tables
    aColList = new PRInt32[numAutoCols+numFixedCols];

    // transfer the auto columns to aColList
    PRInt32 i;
    for (i = 0; i < numAutoCols; i++) {
      aColList[i] = autoColList[i];
      aNumCols++;
    }

    // get the columns whose fixed width is due to a colspan and transfer them to aColList
    for (i = 0; i < numFixedCols; i++) {
      nsTableColFrame* colFrame;
      mTableFrame->GetColumnFrame(fixedColList[i], colFrame);
      if (nsTableColFrame::eWIDTH_SOURCE_CELL_WITH_SPAN==colFrame->GetWidthSource()) {
        aColList[aNumCols] = fixedColList[i];
        aNumCols++;
      }
    }
  }

  return;
}


