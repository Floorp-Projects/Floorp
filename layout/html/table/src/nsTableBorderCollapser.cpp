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
#if 0
#include "nsIPresContext.h"
#include "nsTableBorderCollapser.h"
#include "nsTableFrame.h"
#include "nsTableCellFrame.h"
#include "nsTableColFrame.h"
#include "nsCellMap.h"
#include "celldata.h"

nsTableBorderCollapser::nsTableBorderCollapser(nsTableFrame& aTableFrame)
:mTableFrame(aTableFrame)
{
}

nsTableBorderCollapser::~nsTableBorderCollapser()
{
  for (PRInt32 sideX = 0; sideX < 4; sideX++) {
	  nsBorderEdge* border = (nsBorderEdge *)(mBorderEdges.mEdges[sideX].ElementAt(0));
		while (border) {
		  delete border;
			mBorderEdges.mEdges[sideX].RemoveElementAt(0);
			border = (nsBorderEdge *)(mBorderEdges.mEdges[sideX].ElementAt(0));
		}
  }
}

void nsTableBorderCollapser::SetBorderEdgeLength(PRUint8 aSide, 
                                                 PRInt32 aIndex, 
                                                 nscoord aLength)
{
  nsBorderEdge* border = (nsBorderEdge *)(mBorderEdges.mEdges[aSide].ElementAt(aIndex));
  if (border) {
    border->mLength = aLength;
  }
}

void nsTableBorderCollapser::DidComputeHorizontalBorders(nsIPresContext* aPresContext,
                                                         PRInt32         aStartRowIndex,
                                                         PRInt32         aEndRowIndex)
{
  // XXX: for now, this only does table edges.  May need to do interior edges also?  Probably not.
  PRInt32 lastRowIndex = mTableFrame.GetRowCount() - 1;
  PRInt32 lastColIndex = mTableFrame.GetColCount() - 1;
  if (0 == aStartRowIndex) {
    nsTableCellFrame* cellFrame = mTableFrame.GetCellInfoAt(0, 0);
    nsRect rowRect(0,0,0,0);
    if (cellFrame) {
      nsIFrame* rowFrame;
      cellFrame->GetParent(&rowFrame);
      rowFrame->GetRect(rowRect);
      nsBorderEdge* leftBorder  = (nsBorderEdge *)(mBorderEdges.mEdges[NS_SIDE_LEFT].ElementAt(0));
      nsBorderEdge* rightBorder = (nsBorderEdge *)(mBorderEdges.mEdges[NS_SIDE_RIGHT].ElementAt(0));
      nsBorderEdge* topBorder   = (nsBorderEdge *)(mBorderEdges.mEdges[NS_SIDE_TOP].ElementAt(0));
      if (leftBorder)
        leftBorder->mLength = rowRect.height + topBorder->mWidth;
      if (topBorder)
        topBorder = (nsBorderEdge *)(mBorderEdges.mEdges[NS_SIDE_TOP].ElementAt(lastColIndex));
      if (rightBorder)
        rightBorder->mLength = rowRect.height + topBorder->mWidth;
    }
  }

  if (lastRowIndex <= aEndRowIndex) {
    nsTableCellFrame* cellFrame = mTableFrame.GetCellInfoAt(lastRowIndex, 0);
    nsRect rowRect(0,0,0,0);
    if (cellFrame) {
      nsIFrame* rowFrame;
      cellFrame->GetParent(&rowFrame);
      rowFrame->GetRect(rowRect);
      nsBorderEdge* leftBorder   = (nsBorderEdge *)(mBorderEdges.mEdges[NS_SIDE_LEFT].ElementAt(lastRowIndex));
      nsBorderEdge* rightBorder  = (nsBorderEdge *)(mBorderEdges.mEdges[NS_SIDE_RIGHT].ElementAt(lastRowIndex));
      nsBorderEdge* bottomBorder = (nsBorderEdge *)(mBorderEdges.mEdges[NS_SIDE_BOTTOM].ElementAt(0));
      if (leftBorder)
        leftBorder->mLength = rowRect.height + bottomBorder->mWidth;
      if (bottomBorder)
        bottomBorder = (nsBorderEdge *)(mBorderEdges.mEdges[NS_SIDE_BOTTOM].ElementAt(lastColIndex));
      if (rightBorder)
        rightBorder->mLength = rowRect.height + bottomBorder->mWidth;
    }
  }

  for (PRInt32 borderX = NS_SIDE_TOP; borderX <= NS_SIDE_LEFT; borderX++) {
    if (!mBorderEdges.mEdges[borderX].ElementAt(0)) {
      mBorderEdges.mEdges[borderX].AppendElement(new nsBorderEdge());
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
void nsTableBorderCollapser::ComputeHorizontalBorders(nsIPresContext* aPresContext,
                                                      PRInt32         aStartRowIndex,
                                                      PRInt32         aEndRowIndex)
{
  PRInt32 colCount = mTableFrame.GetColCount();
  PRInt32 rowCount = mTableFrame.GetRowCount();
  if (aStartRowIndex >= rowCount) {
    return; // we don't have the requested row yet
  }

  for (PRInt32 rowIndex = aStartRowIndex; (rowIndex < rowCount) && (rowIndex <= aEndRowIndex); rowIndex++) {
    for (PRInt32 colIndex = 0; colIndex < colCount; colIndex++) {
      if (0 == rowIndex) { // table is top neighbor
        ComputeTopBorderForEdgeAt(aPresContext, rowIndex, colIndex);
      }
      ComputeBottomBorderForEdgeAt(aPresContext, rowIndex, colIndex);
    }
  }
}

/* compute the left and right collapsed borders between aStartRowIndex and aEndRowIndex, inclusive */
void nsTableBorderCollapser::ComputeVerticalBorders(nsIPresContext* aPresContext,
                                                    PRInt32         aStartRowIndex,
                                                    PRInt32         aEndRowIndex)
{
  //nsCellMap* cellMap = mTableFrame.GetCellMap();
  //if (nsnull == cellMap)
  //  return; // no info yet, so nothing useful to do
  
  // compute all the collapsing border values for the entire table
  // XXX: we have to make this more incremental!
  
  PRInt32 colCount = mTableFrame.GetColCount();
  PRInt32 rowCount = mTableFrame.GetRowCount();
  PRInt32 endRowIndex = aEndRowIndex;
  if (-1 == endRowIndex)
    endRowIndex = rowCount-1;
  if (aStartRowIndex >= rowCount) {
    return; // we don't have the requested row yet
  }

  for (PRInt32 rowX = aStartRowIndex; (rowX < rowCount) && (rowX <= endRowIndex); rowX++) {
    for (PRInt32 colX = 0; colX < colCount; colX++) {
      if (0 == colX) { // table is left neighbor
        ComputeLeftBorderForEdgeAt(aPresContext, rowX, colX);
      }
      ComputeRightBorderForEdgeAt(aPresContext, rowX, colX);
    }
  }
}

void nsTableBorderCollapser::ComputeLeftBorderForEdgeAt(nsIPresContext* aPresContext,
                                                        PRInt32         aRowIndex, 
                                                        PRInt32         aColIndex)
{
  PRInt32 numSegments = mBorderEdges.mEdges[NS_SIDE_LEFT].Count();
  while (numSegments <= aRowIndex) {
    nsBorderEdge* borderToAdd = new nsBorderEdge();
    mBorderEdges.mEdges[NS_SIDE_LEFT].AppendElement(borderToAdd);
    numSegments++;
  }
  // "border" is the border segment we are going to set
  nsBorderEdge *border = (nsBorderEdge *)(mBorderEdges.mEdges[NS_SIDE_LEFT].ElementAt(aRowIndex));
  if (!border) 
    return;

  // collect all the incident frames and compute the dominant border 
  nsVoidArray styles;
  // styles are added to the array in the order least dominant -> most dominant
  //    1. table
  const nsStyleBorder *borderStyleData;
  mTableFrame.GetStyleData(eStyleStruct_Border, ((const nsStyleStruct *&)borderStyleData));
  styles.AppendElement((void*)borderStyleData);
  //    2. colgroup
  nsTableColFrame* colFrame = mTableFrame.GetColFrame(aColIndex);
  nsIFrame* colGroupFrame;
  colFrame->GetParent(&colGroupFrame);
  colGroupFrame->GetStyleData(eStyleStruct_Border, ((const nsStyleStruct *&)borderStyleData));
  styles.AppendElement((void*)borderStyleData);
  //    3. col
  colFrame->GetStyleData(eStyleStruct_Border, ((const nsStyleStruct *&)borderStyleData));
  styles.AppendElement((void*)borderStyleData);
  //    4. rowgroup
  nsTableCellFrame* cellFrame = mTableFrame.GetCellInfoAt(aRowIndex, aColIndex);
  nsRect rowRect(0,0,0,0);
  if (cellFrame) {
    nsIFrame* rowFrame;
    cellFrame->GetParent(&rowFrame);
    rowFrame->GetRect(rowRect);
    nsIFrame* rowGroupFrame;
    rowFrame->GetParent(&rowGroupFrame);
    rowGroupFrame->GetStyleData(eStyleStruct_Border, ((const nsStyleStruct *&)borderStyleData));
    styles.AppendElement((void*)borderStyleData);
    //    5. row
    rowFrame->GetStyleData(eStyleStruct_Border, ((const nsStyleStruct *&)borderStyleData));
    styles.AppendElement((void*)borderStyleData);
    //    6. cell (need to do something smart for rowspanner with row frame)
    cellFrame->GetStyleData(eStyleStruct_Border, ((const nsStyleStruct *&)borderStyleData));
    styles.AppendElement((void*)borderStyleData);
  }
  ComputeBorderSegment(NS_SIDE_LEFT, &styles, *border, PR_FALSE);
  // now give half the computed border to the table segment, and half to the cell
  // to avoid rounding errors, we convert up to pixels, divide by 2, and 
  // we give the odd pixel to the table border
  float t2p;
  aPresContext->GetTwipsToPixels(&t2p);
  float p2t;
  aPresContext->GetPixelsToTwips(&p2t);
  nscoord widthAsPixels = NSToCoordRound((float)(border->mWidth)*t2p);
  nscoord widthToAdd = 0;
  border->mWidth = widthAsPixels/2;
  if ((border->mWidth*2)!=widthAsPixels)
    widthToAdd = NSToCoordCeil(p2t);
  border->mWidth *= NSToCoordCeil(p2t);
  border->mLength = rowRect.height;
  // we need to factor in the table's horizontal borders.
  // but we can't compute that length here because we don't know how thick top and bottom borders are
  // see DidComputeHorizontalCollapsingBorders
  if (nsnull!=cellFrame)
  {
    border->mInsideNeighbor = cellFrame->mBorderEdges;
    cellFrame->SetBorderEdge(NS_SIDE_LEFT, aRowIndex, aColIndex, border, 0);  // set the left edge of the cell frame
  }
  border->mWidth += widthToAdd;
  mBorderEdges.mMaxBorderWidth.left = PR_MAX(border->mWidth, mBorderEdges.mMaxBorderWidth.left);
}

void nsTableBorderCollapser::ComputeRightBorderForEdgeAt(nsIPresContext* aPresContext,
                                                         PRInt32         aRowIndex, 
                                                         PRInt32         aColIndex)
{
  nsTableCellMap* cellMap = mTableFrame.GetCellMap();
  if (!cellMap) {
    return;
  }

  PRInt32 colCount = mTableFrame.GetColCount();
  PRInt32 numSegments = mBorderEdges.mEdges[NS_SIDE_RIGHT].Count();
  while (numSegments <= aRowIndex) {
    nsBorderEdge* borderToAdd = new nsBorderEdge();
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
  nsTableCellFrame* rightNeighborFrame=nsnull; 
 
  if ((colCount - 1) != aColIndex) {
    for (PRInt32 colIndex = aColIndex + 1; colIndex < colCount; colIndex++) {
		  CellData* cd = cellMap->GetCellAt(aRowIndex, colIndex);
		  if (cd) { // there's really a cell at (aRowIndex, colIndex)
			  if (cd->IsSpan()) { // the cell at (aRowIndex, colIndex) is the result of a span
				  nsTableCellFrame* cell = nsnull;
          cell = cellMap->GetCellFrame(aRowIndex, colIndex, *cd, PR_TRUE);
				  NS_ASSERTION(cell, "bad cell map state, missing real cell");
				  PRInt32 realRowIndex;
          cell->GetRowIndex (realRowIndex);
				  if (realRowIndex!=aRowIndex) { // the span is caused by a rowspan
					  rightNeighborFrame = cell;
					  break;
				  }
			  }
        else {
          rightNeighborFrame = cd->GetCellFrame();
          break;
        }
		  }
    }
  }
  const nsStyleBorder *borderStyleData;
  if (!rightNeighborFrame) { 
    // if rightNeighborFrame is null, our right neighbor is the table 
    mTableFrame.GetStyleData(eStyleStruct_Border, ((const nsStyleStruct *&)borderStyleData));
    styles.AppendElement((void*)borderStyleData);
  }
  //    2. colgroup //XXX: need to test if we're really on a colgroup border
  nsTableColFrame* colFrame = mTableFrame.GetColFrame(aColIndex);
  nsIFrame* colGroupFrame;
  colFrame->GetParent(&colGroupFrame);
  colGroupFrame->GetStyleData(eStyleStruct_Border, ((const nsStyleStruct *&)borderStyleData));
  styles.AppendElement((void*)borderStyleData);
  //    3. col
  colFrame->GetStyleData(eStyleStruct_Border, ((const nsStyleStruct *&)borderStyleData));
  styles.AppendElement((void*)borderStyleData);
  //    4. rowgroup
  nsTableCellFrame* cellFrame = cellMap->GetCellInfoAt(aRowIndex, aColIndex);
  nsRect rowRect(0,0,0,0);
  if (cellFrame) {
    nsIFrame* rowFrame;
    cellFrame->GetParent(&rowFrame);
    rowFrame->GetRect(rowRect);
    nsIFrame* rowGroupFrame;
    rowFrame->GetParent(&rowGroupFrame);
    if (!rightNeighborFrame) {
      // if rightNeighborFrame is null, our right neighbor is the table so we include the rowgroup and row
      rowGroupFrame->GetStyleData(eStyleStruct_Border, ((const nsStyleStruct *&)borderStyleData));
      styles.AppendElement((void*)borderStyleData);
      //    5. row
      rowFrame->GetStyleData(eStyleStruct_Border, ((const nsStyleStruct *&)borderStyleData));
      styles.AppendElement((void*)borderStyleData);
    }
    //    6. cell (need to do something smart for rowspanner with row frame)
    cellFrame->GetStyleData(eStyleStruct_Border, ((const nsStyleStruct *&)borderStyleData));
    styles.AppendElement((void*)borderStyleData);
  }
  //    7. left edge of rightNeighborCell, if there is one
  if (rightNeighborFrame) {
    rightNeighborFrame->GetStyleData(eStyleStruct_Border, ((const nsStyleStruct *&)borderStyleData));
    styles.AppendElement((void*)borderStyleData);
  }
  ComputeBorderSegment(NS_SIDE_RIGHT, &styles, border, (nsnull != rightNeighborFrame));
  // now give half the computed border to each of the two neighbors 
  // (the 2 cells, or the cell and the table)
  // to avoid rounding errors, we convert up to pixels, divide by 2, and 
  // we give the odd pixel to the right cell border
  float t2p;
  aPresContext->GetTwipsToPixels(&t2p);
  float p2t;
  aPresContext->GetPixelsToTwips(&p2t);
  nscoord widthAsPixels = NSToCoordRound((float)(border.mWidth)*t2p);
  nscoord widthToAdd = 0;
  border.mWidth = widthAsPixels/2;
  if ((border.mWidth*2) != widthAsPixels)
    widthToAdd = NSToCoordCeil(p2t);
  border.mWidth *= NSToCoordCeil(p2t);
  border.mLength = rowRect.height;
  if (cellFrame) {
    cellFrame->SetBorderEdge(NS_SIDE_RIGHT, aRowIndex, aColIndex, &border, widthToAdd);
  }
  if (!rightNeighborFrame) {
    nsBorderEdge * tableBorder = (nsBorderEdge *)(mBorderEdges.mEdges[NS_SIDE_RIGHT].ElementAt(aRowIndex));
    *tableBorder = border;
    if (cellFrame) {
      tableBorder->mInsideNeighbor = cellFrame->mBorderEdges;
    }
    mBorderEdges.mMaxBorderWidth.right = PR_MAX(border.mWidth, mBorderEdges.mMaxBorderWidth.right);
    // since the table is our right neightbor, we need to factor in the table's horizontal borders.
    // can't compute that length here because we don't know how thick top and bottom borders are
    // see DidComputeHorizontalCollapsingBorders
  }
  else {
    rightNeighborFrame->SetBorderEdge(NS_SIDE_LEFT, aRowIndex, aColIndex, &border, 0);
  }
}

void nsTableBorderCollapser::ComputeTopBorderForEdgeAt(nsIPresContext* aPresContext,
                                                       PRInt32         aRowIndex, 
                                                       PRInt32         aColIndex)
{
  nsTableCellMap* cellMap = mTableFrame.GetCellMap();
  if (!cellMap) {
    return;
  }

  PRInt32 numSegments = mBorderEdges.mEdges[NS_SIDE_TOP].Count();
  while (numSegments<=aColIndex) {
    nsBorderEdge* borderToAdd = new nsBorderEdge();
    mBorderEdges.mEdges[NS_SIDE_TOP].AppendElement(borderToAdd);
    numSegments++;
  }
  // "border" is the border segment we are going to set
  nsBorderEdge *border = (nsBorderEdge *)(mBorderEdges.mEdges[NS_SIDE_TOP].ElementAt(aColIndex));
  if (!border) 
    return;

  // collect all the incident frames and compute the dominant border 
  nsVoidArray styles;
  // styles are added to the array in the order least dominant -> most dominant
  //    1. table
  const nsStyleBorder *borderStyleData;
  mTableFrame.GetStyleData(eStyleStruct_Border, ((const nsStyleStruct *&)borderStyleData));
  styles.AppendElement((void*)borderStyleData);
  //    2. colgroup
  nsTableColFrame* colFrame = mTableFrame.GetColFrame(aColIndex);
  nsIFrame* colGroupFrame;
  colFrame->GetParent(&colGroupFrame);
  colGroupFrame->GetStyleData(eStyleStruct_Border, ((const nsStyleStruct *&)borderStyleData));
  styles.AppendElement((void*)borderStyleData);
  //    3. col
  colFrame->GetStyleData(eStyleStruct_Border, ((const nsStyleStruct *&)borderStyleData));
  styles.AppendElement((void*)borderStyleData);
  //    4. rowgroup
  nsTableCellFrame* cellFrame = cellMap->GetCellInfoAt(aRowIndex, aColIndex);
  if (cellFrame) {
    nsIFrame* rowFrame;
    cellFrame->GetParent(&rowFrame);
    nsIFrame* rowGroupFrame;
    rowFrame->GetParent(&rowGroupFrame);
    rowGroupFrame->GetStyleData(eStyleStruct_Border, ((const nsStyleStruct *&)borderStyleData));
    styles.AppendElement((void*)borderStyleData);
    //    5. row
    rowFrame->GetStyleData(eStyleStruct_Border, ((const nsStyleStruct *&)borderStyleData));
    styles.AppendElement((void*)borderStyleData);
    //    6. cell (need to do something smart for rowspanner with row frame)
    cellFrame->GetStyleData(eStyleStruct_Border, ((const nsStyleStruct *&)borderStyleData));
    styles.AppendElement((void*)borderStyleData);
  }
  ComputeBorderSegment(NS_SIDE_TOP, &styles, *border, PR_FALSE);
  // now give half the computed border to the table segment, and half to the cell
  // to avoid rounding errors, we convert up to pixels, divide by 2, and 
  // we give the odd pixel to the right border
  float t2p;
  aPresContext->GetTwipsToPixels(&t2p);
  float p2t;
  aPresContext->GetPixelsToTwips(&p2t);
  nscoord widthAsPixels = NSToCoordRound((float)(border->mWidth)*t2p);
  nscoord widthToAdd = 0;
  border->mWidth = widthAsPixels/2;
  if ((border->mWidth*2) != widthAsPixels)
    widthToAdd = NSToCoordCeil(p2t);
  border->mWidth *= NSToCoordCeil(p2t);
  border->mLength = mTableFrame.GetColumnWidth(aColIndex);
  if (cellFrame) {
    border->mInsideNeighbor = cellFrame->mBorderEdges;
  }
  if (0 == aColIndex) {
    // if we're the first column, factor in the thickness of the left table border
    nsBorderEdge *leftBorder = (nsBorderEdge *)(mBorderEdges.mEdges[NS_SIDE_LEFT].ElementAt(0));
    if (leftBorder) 
      border->mLength += leftBorder->mWidth;
  }
  if ((cellMap->GetColCount() - 1) == aColIndex) {
    // if we're the last column, factor in the thickness of the right table border
    nsBorderEdge* rightBorder = (nsBorderEdge *)(mBorderEdges.mEdges[NS_SIDE_RIGHT].ElementAt(0));
    if (rightBorder) 
      border->mLength += rightBorder->mWidth;
  }
  if (cellFrame) {
    cellFrame->SetBorderEdge(NS_SIDE_TOP, aRowIndex, aColIndex, border, 0);  // set the top edge of the cell frame
  }
  border->mWidth += widthToAdd;
  mBorderEdges.mMaxBorderWidth.top = PR_MAX(border->mWidth, mBorderEdges.mMaxBorderWidth.top);
}

void nsTableBorderCollapser::ComputeBottomBorderForEdgeAt(nsIPresContext* aPresContext,
                                                          PRInt32         aRowIndex, 
                                                          PRInt32         aColIndex)
{
  nsTableCellMap* cellMap = mTableFrame.GetCellMap();
  if (!cellMap) {
    return;
  }

  PRInt32 rowCount = cellMap->GetRowCount();
  PRInt32 numSegments = mBorderEdges.mEdges[NS_SIDE_BOTTOM].Count();
  while (numSegments <= aColIndex) {
    nsBorderEdge* borderToAdd = new nsBorderEdge();
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
  nsTableCellFrame* bottomNeighborFrame=nsnull;  
  if ((rowCount-1) != aRowIndex) {
    for (PRInt32 rowIndex = aRowIndex + 1; rowIndex < rowCount; rowIndex++) {
		  CellData* cd = cellMap->GetCellAt(rowIndex, aColIndex);
		  if (cd) { // there's really a cell at (rowIndex, aColIndex)
        if (cd->IsSpan()) {
			    // the cell at (rowIndex, aColIndex) is the result of a span
				  nsTableCellFrame* cell = cellMap->GetCellFrame(rowIndex, aColIndex, *cd, PR_FALSE);
				  NS_ASSERTION( cell, "bad cell map state, missing real cell");
				  PRInt32 realColIndex;
          cell->GetColIndex (realColIndex);
				  if (realColIndex!=aColIndex) { // the span is caused by a colspan
					  bottomNeighborFrame = cell;
					  break;
				  }
			  }
        else {
          bottomNeighborFrame = cd->GetCellFrame();
          break;
        }
		  }
    }
  }
  const nsStyleBorder *borderStyleData;
  if (!bottomNeighborFrame) {
    // if bottomNeighborFrame is null, our bottom neighbor is the table 
    mTableFrame.GetStyleData(eStyleStruct_Border, ((const nsStyleStruct *&)borderStyleData));
    styles.AppendElement((void*)borderStyleData);

    //    2. colgroup   // XXX: need to deterine if we're on a colgroup boundary
    nsTableColFrame* colFrame = mTableFrame.GetColFrame(aColIndex);
    nsIFrame* colGroupFrame;
    colFrame->GetParent(&colGroupFrame);
    colGroupFrame->GetStyleData(eStyleStruct_Border, ((const nsStyleStruct *&)borderStyleData));
    styles.AppendElement((void*)borderStyleData);
    //    3. col
    colFrame->GetStyleData(eStyleStruct_Border, ((const nsStyleStruct *&)borderStyleData));
    styles.AppendElement((void*)borderStyleData);
  }
  //    4. rowgroup // XXX: use rowgroup only if we're on a table edge
  nsTableCellFrame* cellFrame = cellMap->GetCellInfoAt(aRowIndex, aColIndex);
  nsRect rowRect(0,0,0,0);
  if (cellFrame) {
    nsIFrame* rowFrame;
    cellFrame->GetParent(&rowFrame);
    rowFrame->GetRect(rowRect);
    nsIFrame* rowGroupFrame;
    rowFrame->GetParent(&rowGroupFrame);
    rowGroupFrame->GetStyleData(eStyleStruct_Border, ((const nsStyleStruct *&)borderStyleData));
    styles.AppendElement((void*)borderStyleData);
    //    5. row
    rowFrame->GetStyleData(eStyleStruct_Border, ((const nsStyleStruct *&)borderStyleData));
    styles.AppendElement((void*)borderStyleData);
    //    6. cell (need to do something smart for rowspanner with row frame)
    cellFrame->GetStyleData(eStyleStruct_Border, ((const nsStyleStruct *&)borderStyleData));
    styles.AppendElement((void*)borderStyleData);
  }
  //    7. top edge of bottomNeighborCell, if there is one
  if (bottomNeighborFrame) {
    bottomNeighborFrame->GetStyleData(eStyleStruct_Border, ((const nsStyleStruct *&)borderStyleData));
    styles.AppendElement((void*)borderStyleData);
  }
  ComputeBorderSegment(NS_SIDE_BOTTOM, &styles, border, (nsnull != bottomNeighborFrame));
  // now give half the computed border to each of the two neighbors 
  // (the 2 cells, or the cell and the table)
  // to avoid rounding errors, we convert up to pixels, divide by 2, and 
  // we give the odd pixel to the right cell border
  float t2p;
  aPresContext->GetTwipsToPixels(&t2p);
  float p2t;
  aPresContext->GetPixelsToTwips(&p2t);
  nscoord widthAsPixels = NSToCoordRound((float)(border.mWidth)*t2p);
  nscoord widthToAdd = 0;
  border.mWidth = widthAsPixels/2;
  if ((border.mWidth*2) != widthAsPixels)
    widthToAdd = NSToCoordCeil(p2t);
  border.mWidth *= NSToCoordCeil(p2t);
  border.mLength = mTableFrame.GetColumnWidth(aColIndex);
  if (cellFrame) {
    cellFrame->SetBorderEdge(NS_SIDE_BOTTOM, aRowIndex, aColIndex, &border, widthToAdd);
  }
  if (!bottomNeighborFrame) {
    nsBorderEdge* tableBorder = (nsBorderEdge *)(mBorderEdges.mEdges[NS_SIDE_BOTTOM].ElementAt(aColIndex));
    *tableBorder = border;
    if (cellFrame) {
      tableBorder->mInsideNeighbor = cellFrame->mBorderEdges;
    }
    mBorderEdges.mMaxBorderWidth.bottom = PR_MAX(border.mWidth, mBorderEdges.mMaxBorderWidth.bottom);
    // since the table is our bottom neightbor, we need to factor in the table's vertical borders.
    PRInt32 lastColIndex = cellMap->GetColCount()-1;
    if (0 == aColIndex) {
      // if we're the first column, factor in the thickness of the left table border
      nsBorderEdge *leftBorder = (nsBorderEdge *)(mBorderEdges.mEdges[NS_SIDE_LEFT].ElementAt(rowCount-1));
      if (leftBorder) 
        tableBorder->mLength += leftBorder->mWidth;
    }
    if (lastColIndex == aColIndex) {
      // if we're the last column, factor in the thickness of the right table border
      nsBorderEdge *rightBorder = (nsBorderEdge *)(mBorderEdges.mEdges[NS_SIDE_RIGHT].ElementAt(rowCount-1));
      if (rightBorder) 
        tableBorder->mLength += rightBorder->mWidth;
    }
  }
  else
  {
    bottomNeighborFrame->SetBorderEdge(NS_SIDE_TOP, aRowIndex, aColIndex, &border, 0);
  }
}

nscoord nsTableBorderCollapser::GetWidthForSide(const nsMargin& aBorder, 
                                                PRUint8         aSide)
{
  if (NS_SIDE_LEFT == aSide) 
    return aBorder.left;
  else if (NS_SIDE_RIGHT == aSide) 
    return aBorder.right;
  else if (NS_SIDE_TOP == aSide) 
    return aBorder.top;
  else 
    return aBorder.bottom;
}

/* Given an Edge, find the opposing edge (top<-->bottom, left<-->right) */
PRUint8 nsTableBorderCollapser::GetOpposingEdge(PRUint8 aEdge)
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
PRUint8 nsTableBorderCollapser::CompareBorderStyles(PRUint8 aStyle1, 
                                                    PRUint8 aStyle2)
{
  PRUint8 result=BORDER_PRECEDENT_HIGHER; // if we get illegal types for table borders, HIGHER is the default
  if (aStyle1 == aStyle2)
    result = BORDER_PRECEDENT_EQUAL;
  else if (NS_STYLE_BORDER_STYLE_HIDDEN==aStyle1)
    result = BORDER_PRECEDENT_HIGHER;
  else if (NS_STYLE_BORDER_STYLE_NONE==aStyle1)
    result = BORDER_PRECEDENT_LOWER;
  else if (NS_STYLE_BORDER_STYLE_NONE==aStyle2)
    result = BORDER_PRECEDENT_HIGHER;
  else if (NS_STYLE_BORDER_STYLE_HIDDEN==aStyle2)
    result = BORDER_PRECEDENT_LOWER;
  else {
    switch (aStyle1) {
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
void nsTableBorderCollapser::ComputeBorderSegment(PRUint8      aSide, 
                                                  nsVoidArray* aStyles, 
                                                  nsBorderEdge &aBorder,
                                                  PRBool       aFlipLastSide)
{
  if (aStyles) {
    PRInt32 styleCount = aStyles->Count();
    if (0 != styleCount) {
      nsVoidArray sameWidthBorders;
      nsStyleBorder * borderStyleData;
      nsStyleBorder * lastBorderStyleData=nsnull;
      nsMargin border;
      PRInt32 maxWidth=0;
      PRUint8 side = aSide;
      PRInt32 i;
      for (i = 0; i < styleCount; i++) {
        borderStyleData = (nsStyleBorder *)(aStyles->ElementAt(i));
        if (aFlipLastSide && (i == styleCount-1)) {
          side = GetOpposingEdge(aSide);
          lastBorderStyleData = borderStyleData;
        }
        if (borderStyleData->GetBorderStyle(side) == NS_STYLE_BORDER_STYLE_HIDDEN) {
          aBorder.mStyle=NS_STYLE_BORDER_STYLE_HIDDEN;
          aBorder.mWidth=0;
          return;
        }
        else if (borderStyleData->GetBorderStyle(side)!=NS_STYLE_BORDER_STYLE_NONE) {
          if (borderStyleData->GetBorder(border)) {
            nscoord borderWidth = GetWidthForSide(border, side);
            if (borderWidth == maxWidth) {
              sameWidthBorders.AppendElement(borderStyleData);
            }
            else if (borderWidth > maxWidth) {
              maxWidth = borderWidth;
              sameWidthBorders.Clear();
              sameWidthBorders.AppendElement(borderStyleData);
            }
          }
        }
      }
      aBorder.mWidth = maxWidth;
      // now we've gone through each overlapping border once, and we have a list
      // of all styles with the same width.  If there's more than one, resolve the
      // conflict based on border style
      styleCount = sameWidthBorders.Count();
      if (0 == styleCount) {  // all borders were of style NONE
        aBorder.mWidth=0;
        aBorder.mStyle=NS_STYLE_BORDER_STYLE_NONE;
        return;
      }
      else if (1 == styleCount) { // there was just one border of the largest width
        borderStyleData = (nsStyleBorder *)(sameWidthBorders.ElementAt(0));
        side = aSide;
        if (borderStyleData == lastBorderStyleData)
          side = GetOpposingEdge(aSide);
        if (!borderStyleData->GetBorderColor(side, aBorder.mColor)) {
          // XXX EEEK handle transparent border color somehow...
        }
        aBorder.mStyle = borderStyleData->GetBorderStyle(side);
        return;
      }
      else {
        nsStyleBorder* winningStyleBorder;
        PRUint8 winningStyle=NS_STYLE_BORDER_STYLE_NONE;
        for (i = 0; i < styleCount; i++) {
          borderStyleData = (nsStyleBorder *)(sameWidthBorders.ElementAt(i));
          side = aSide;
          if (borderStyleData == lastBorderStyleData)
            side = GetOpposingEdge(aSide);
          PRUint8 thisStyle = borderStyleData->GetBorderStyle(side);
          PRUint8 borderCompare = CompareBorderStyles(thisStyle, winningStyle);
          if (BORDER_PRECEDENT_HIGHER == borderCompare) {
            winningStyle = thisStyle;
            winningStyleBorder = borderStyleData;
          }
          else if (BORDER_PRECEDENT_EQUAL == borderCompare) {
            // we're in lowest-to-highest precedence order, so later border styles win
            winningStyleBorder=borderStyleData;
          }          
        }
        aBorder.mStyle = winningStyle;
        side = aSide;
        if (winningStyleBorder == lastBorderStyleData)
          side = GetOpposingEdge(aSide);
        if (!winningStyleBorder->GetBorderColor(side, aBorder.mColor)) {
          // XXX handle transparent border colors somehow
        }
      }
    }
  }

}

void nsTableBorderCollapser::GetBorder(nsMargin& aBorder)
{
  aBorder = mBorderEdges.mMaxBorderWidth;
}

void nsTableBorderCollapser::GetBorderAt(PRInt32   aRowIndex,
                                         PRInt32   aColIndex,
                                         nsMargin& aBorder)
{
  nsBorderEdge* border = (nsBorderEdge *)(mBorderEdges.mEdges[NS_SIDE_LEFT].ElementAt(aRowIndex));
  if (border) {
	  aBorder.left = border->mWidth;
  }
  border = (nsBorderEdge *)(mBorderEdges.mEdges[NS_SIDE_RIGHT].ElementAt(aRowIndex));
  if (border) {
	  aBorder.right = border->mWidth;
  }
  border = (nsBorderEdge *)(mBorderEdges.mEdges[NS_SIDE_TOP].ElementAt(aColIndex));
  if (border) {
	  aBorder.top = border->mWidth;
  }
  border = (nsBorderEdge *)(mBorderEdges.mEdges[NS_SIDE_BOTTOM].ElementAt(aColIndex));
  if (border) {
	  aBorder.bottom = border->mWidth;
  }
}

void nsTableBorderCollapser::GetMaxBorder(PRInt32  aStartRowIndex,
                                          PRInt32  aEndRowIndex,
                                          PRInt32  aStartColIndex,
                                          PRInt32  aEndColIndex,
                                          nsMargin aBorder)
{
  aBorder.top = aBorder.right = aBorder.bottom = aBorder.left = 0;
  for (PRInt32 rowX = aStartRowIndex; rowX <= aEndRowIndex; rowX++) {
    for (PRInt32 colX = aStartColIndex; colX <= aEndColIndex; colX++) {
      nsMargin border;
      GetBorderAt(rowX, colX, border);
      aBorder.top    = PR_MAX(aBorder.top,    border.top);
      aBorder.right  = PR_MAX(aBorder.right,  border.right);
      aBorder.bottom = PR_MAX(aBorder.bottom, border.bottom);
      aBorder.left   = PR_MAX(aBorder.left,   border.left);
    }
  }
}
#endif
