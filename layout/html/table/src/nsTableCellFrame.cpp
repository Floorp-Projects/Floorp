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
#include "nsTableFrame.h"
#include "nsTableColFrame.h"
#include "nsTableCellFrame.h"
#include "nsTableFrame.h"
#include "nsIReflowCommand.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIRenderingContext.h"
#include "nsCSSRendering.h"
#include "nsIContent.h"
#include "nsIHTMLContent.h"
#include "nsHTMLIIDs.h"
#include "nsHTMLParts.h"
#include "nsHTMLValue.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsVoidArray.h"
#include "nsIView.h"
#include "nsStyleUtil.h"
#include "nsLayoutAtoms.h"
#include "nsIFrameManager.h"
#include "nsIPresShell.h"
#include "nsCOMPtr.h"
#include "nsIHTMLTableCellElement.h"
#include "nsIDOMHTMLTableCellElement.h"

//TABLECELL SELECTION
#include "nsIFrameSelection.h"
#include "nsILookAndFeel.h"

static NS_DEFINE_IID(kIHTMLTableCellElementIID, NS_IHTMLTABLECELLELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLTableCellElementIID, NS_IDOMHTMLTABLECELLELEMENT_IID);

nsTableCellFrame::nsTableCellFrame()
{
  mColIndex        = 0;
  mPriorAvailWidth = 0;
  mBorderEdges     = nsnull;
}

nsTableCellFrame::~nsTableCellFrame()
{
  delete mBorderEdges;
}

NS_IMETHODIMP
nsTableCellFrame::Init(nsIPresContext*  aPresContext,
                       nsIContent*      aContent,
                       nsIFrame*        aParent,
                       nsIStyleContext* aContext,
                       nsIFrame*        aPrevInFlow)
{
  nsresult  rv;
  
  // Let the base class do its initialization
  rv = nsHTMLContainerFrame::Init(aPresContext, aContent, aParent, aContext,
                                  aPrevInFlow);

  if (aPrevInFlow) {
    // Set the column index
    nsTableCellFrame* cellFrame = (nsTableCellFrame*)aPrevInFlow;
    PRInt32           colIndex;
    cellFrame->GetColIndex(colIndex);
    InitCellFrame(colIndex);
  }

  return rv;
}

NS_IMETHODIMP
nsTableCellFrame::AttributeChanged(nsIPresContext* aPresContext,
                                   nsIContent*     aChild,
                                   PRInt32         aNameSpaceID,
                                   nsIAtom*        aAttribute,
                                   PRInt32         aHint)
{
  // let the table frame decide what to do
  nsTableFrame* tableFrame = nsnull; 
  nsresult rv = nsTableFrame::GetTableFrame(this, tableFrame);
  if ((NS_SUCCEEDED(rv)) && (tableFrame)) {
    tableFrame->AttributeChangedFor(aPresContext, this, aChild, aAttribute); 
  }
  return NS_OK;
}

void nsTableCellFrame::SetPass1MaxElementSize(const nsSize& aMaxElementSize)
{ 
  mPass1MaxElementSize.height = aMaxElementSize.height;
  nscoord maxElemWidth = aMaxElementSize.width;
  // the max elem width needs to be set to the max of aMaxElementSize.width and 
  // the width attribute if both nowrap and width are present on the cell.
  const nsStylePosition* stylePosition;
  GetStyleData(eStyleStruct_Position, ((const nsStyleStruct *&)stylePosition));
  if (stylePosition->mWidth.GetUnit() == eStyleUnit_Coord) {
    nscoord styleWidth = stylePosition->mWidth.GetCoordValue();
    if (styleWidth > 0) {
      nsIDOMHTMLTableCellElement* cellContent = nsnull;
      nsresult rv = mContent->QueryInterface(kIDOMHTMLTableCellElementIID, (void **)&cellContent);  
      if (cellContent && NS_SUCCEEDED(rv)) {
        PRBool nowrap = PR_FALSE;
        cellContent->GetNoWrap(&nowrap);
        if (nowrap) {
          maxElemWidth = PR_MAX(maxElemWidth, styleWidth);
        }
        NS_RELEASE(cellContent);
      }
    }
  }
  mPass1MaxElementSize.width = maxElemWidth;
}

NS_IMETHODIMP
nsTableCellFrame::AppendFrames(nsIPresContext* aPresContext,
                               nsIPresShell&   aPresShell,
                               nsIAtom*        aListName,
                               nsIFrame*       aFrameList)
{
  NS_PRECONDITION(PR_FALSE, "unsupported operation");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsTableCellFrame::InsertFrames(nsIPresContext* aPresContext,
                               nsIPresShell&   aPresShell,
                               nsIAtom*        aListName,
                               nsIFrame*       aPrevFrame,
                               nsIFrame*       aFrameList)
{
  NS_PRECONDITION(PR_FALSE, "unsupported operation");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsTableCellFrame::RemoveFrame(nsIPresContext* aPresContext,
                              nsIPresShell&   aPresShell,
                              nsIAtom*        aListName,
                              nsIFrame*       aOldFrame)
{
  NS_PRECONDITION(PR_FALSE, "unsupported operation");
  return NS_ERROR_NOT_IMPLEMENTED;
}

void nsTableCellFrame::InitCellFrame(PRInt32 aColIndex)
{
  nsTableFrame* tableFrame=nsnull;  // I should be checking my own style context, but border-collapse isn't inheriting correctly
  nsresult rv = nsTableFrame::GetTableFrame(this, tableFrame);
  if ((NS_SUCCEEDED(rv)) && (nsnull!=tableFrame)) {
    SetColIndex(aColIndex);
    if (NS_STYLE_BORDER_COLLAPSE == tableFrame->GetBorderCollapseStyle()) {
      if (mBorderEdges) delete mBorderEdges; // this could be non null during a reinitialization
      mBorderEdges = new nsBorderEdges;
      mBorderEdges->mOutsideEdge=PR_FALSE;
      
      PRInt32 rowspan = tableFrame->GetEffectiveRowSpan(*this);
      PRInt32 i;
      for (i=0; i<rowspan; i++) {
        nsBorderEdge *borderToAdd = new nsBorderEdge();
        mBorderEdges->mEdges[NS_SIDE_LEFT].AppendElement(borderToAdd);
        borderToAdd = new nsBorderEdge();
        mBorderEdges->mEdges[NS_SIDE_RIGHT].AppendElement(borderToAdd);
      }
      PRInt32 colspan = tableFrame->GetEffectiveColSpan(*this);
      for (i=0; i<colspan; i++) {
        nsBorderEdge *borderToAdd = new nsBorderEdge();
        mBorderEdges->mEdges[NS_SIDE_TOP].AppendElement(borderToAdd);
        borderToAdd = new nsBorderEdge();
        mBorderEdges->mEdges[NS_SIDE_BOTTOM].AppendElement(borderToAdd);
      }
    }
  }
}

nsresult nsTableCellFrame::SetColIndex(PRInt32 aColIndex)
{  
  mColIndex = aColIndex;
  // for style context optimization, set the content's column index if possible.
  // this can only be done if we really have an nsTableCell.  
  // other tags mapped to table cell display won't benefit from this optimization
  // see nsHTMLStyleSheet::RulesMatching

  //nsIContent* cell;
  //kidFrame->GetContent(&cell);
  nsCOMPtr<nsIContent> cell;
  nsresult rv = GetContent(getter_AddRefs(cell));
  if (NS_FAILED(rv) || !cell)
    return rv;

  nsIHTMLTableCellElement* cellContent = nsnull;
  rv = cell->QueryInterface(kIHTMLTableCellElementIID, 
                            (void **)&cellContent);  // cellContent: REFCNT++
  if (cellContent && NS_SUCCEEDED(rv)) { // it's a table cell
    cellContent->SetColIndex(aColIndex);
    NS_RELEASE(cellContent);
  }
  return rv;
}

      
void nsTableCellFrame::SetBorderEdgeLength(PRUint8 aSide, 
                                           PRInt32 aIndex, 
                                           nscoord aLength)
{
  NS_PRECONDITION(mBorderEdges, "haven't allocated border edges struct");
  if ((NS_SIDE_LEFT==aSide) || (NS_SIDE_RIGHT==aSide))
  {
    PRInt32 baseRowIndex;
    GetRowIndex(baseRowIndex);
    PRInt32 rowIndex = aIndex-baseRowIndex;
    nsBorderEdge *border = (nsBorderEdge *)(mBorderEdges->mEdges[aSide].ElementAt(rowIndex));
    if (border) {
      border->mLength = aLength;
    }
  }
  else {
    NS_ASSERTION(PR_FALSE, "bad arg aSide passed to SetBorderEdgeLength");
  }
}


NS_METHOD nsTableCellFrame::Paint(nsIPresContext* aPresContext,
                                  nsIRenderingContext& aRenderingContext,
                                  const nsRect& aDirtyRect,
                                  nsFramePaintLayer aWhichLayer)
{
  const nsStyleDisplay* disp =
    (const nsStyleDisplay*)mStyleContext->GetStyleData(eStyleStruct_Display);

  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
    if (disp->IsVisibleOrCollapsed()) {
      const nsStyleColor* myColor =
        (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);


      //TABLECELL SELECTION
      PRInt16 displaySelection;
      displaySelection = DisplaySelection(aPresContext);
      if (displaySelection)
      {
        nsFrameState  frameState;
        PRBool        isSelected;
        GetFrameState(&frameState);
        isSelected = (frameState & NS_FRAME_SELECTED_CONTENT) == NS_FRAME_SELECTED_CONTENT;
        if (isSelected)
        {
          nsCOMPtr<nsIPresShell> shell;
          nsresult result = aPresContext->GetShell(getter_AddRefs(shell));
          if (NS_FAILED(result))
            return result;
          nsCOMPtr<nsIFrameSelection> frameSelection;
          result = shell->GetFrameSelection(getter_AddRefs(frameSelection));
          if (NS_SUCCEEDED(result))
          {
            PRBool tableCellSelectionMode;
            result = frameSelection->GetTableCellSelection(&tableCellSelectionMode);
            if (NS_SUCCEEDED(result) && tableCellSelectionMode)
            {
              frameSelection->GetTableCellSelectionStyleColor(&myColor); 
              if(displaySelection==nsISelectionController::SELECTION_DISABLED)
              {
                ((nsStyleColor *)myColor)->mBackgroundColor = NS_RGB(176,176,176);// disabled color
              }
              else
              {
  	            nsILookAndFeel* look = nsnull;
	              if (NS_SUCCEEDED(aPresContext->GetLookAndFeel(&look)) && look) {
	                look->GetColor(nsILookAndFeel::eColor_TextSelectBackground, ((nsStyleColor *)myColor)->mBackgroundColor);//VERY BAD CAST..TEMPORARY
	                NS_RELEASE(look);
	              }
              }
            }
          }
        }
      }
//END SELECTION

      const nsStyleSpacing* mySpacing =
        (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);
      NS_ASSERTION(nsnull!=myColor, "bad style color");
      NS_ASSERTION(nsnull!=mySpacing, "bad style spacing");

      const nsStyleTable* cellTableStyle;
      GetStyleData(eStyleStruct_Table, ((const nsStyleStruct *&)cellTableStyle)); 
      nsRect  rect(0, 0, mRect.width, mRect.height);

      // only non empty cells render their background
      if (PR_FALSE == GetContentEmpty()) {
        nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                        aDirtyRect, rect, *myColor, *mySpacing, 0, 0);
      }
    
      // empty cells do not render their border
      PRBool renderBorder = PR_TRUE;
      if (PR_TRUE==GetContentEmpty())
      {
        if (NS_STYLE_TABLE_EMPTY_CELLS_HIDE==cellTableStyle->mEmptyCells)
          renderBorder=PR_FALSE;
      }
      if (PR_TRUE==renderBorder)
      {
        PRIntn skipSides = GetSkipSides();
        nsTableFrame* tableFrame=nsnull;  // I should be checking my own style context, but border-collapse isn't inheriting correctly
        nsresult rv = nsTableFrame::GetTableFrame(this, tableFrame);
        if ((NS_SUCCEEDED(rv)) && (nsnull!=tableFrame))
        {
          const nsStyleTable* tableStyle;
          tableFrame->GetStyleData(eStyleStruct_Table, ((const nsStyleStruct *&)tableStyle)); 
          if (NS_STYLE_BORDER_SEPARATE == tableFrame->GetBorderCollapseStyle())
          {
            nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                        aDirtyRect, rect, *mySpacing, mStyleContext, skipSides);
          }
          else
          {
            nsCSSRendering::PaintBorderEdges(aPresContext, aRenderingContext, this,
                                             aDirtyRect, rect, mBorderEdges, mStyleContext, skipSides);
          }
        }
      }
    }
  }

#ifdef DEBUG
  // for debug...
  if ((NS_FRAME_PAINT_LAYER_DEBUG == aWhichLayer) && GetShowFrameBorders()) {
    aRenderingContext.SetColor(NS_RGB(0, 0, 128));
    aRenderingContext.DrawRect(0, 0, mRect.width, mRect.height);
  }
#endif

  // if the cell originates in a row and/or col that is collapsed, the
  // bottom and/or right portion of the cell is painted by translating
  // the rendering context.
  PRBool clipState;
  nsPoint offset;
  GetCollapseOffset(aPresContext, offset);
  if ((0 != offset.x) || (0 != offset.y)) {
    aRenderingContext.PushState();
    aRenderingContext.Translate(offset.x, offset.y);
    aRenderingContext.SetClipRect(nsRect(-offset.x, -offset.y, mRect.width, mRect.height),
                                nsClipCombine_kIntersect, clipState);
  }
  PaintChildren(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
  if ((0 != offset.x) || (0 != offset.y)) {
    aRenderingContext.PopState(clipState);
  }
  
  return NS_OK;
  /*nsFrame::Paint(aPresContext,
                        aRenderingContext,
                        aDirtyRect,
                        aWhichLayer);*/
}

NS_IMETHODIMP
nsTableCellFrame::GetFrameForPoint(nsIPresContext* aPresContext,
                                   const nsPoint& aPoint, 
                                   nsFramePaintLayer aWhichLayer,
                                   nsIFrame**     aFrame)
{
  // this should act like a block, so we need to override
  return GetFrameForPointUsing(aPresContext, aPoint, nsnull, aWhichLayer, (aWhichLayer == NS_FRAME_PAINT_LAYER_BACKGROUND), aFrame);
}

//null range means the whole thing
NS_IMETHODIMP
nsTableCellFrame::SetSelected(nsIPresContext* aPresContext,
                              nsIDOMRange *aRange,
                              PRBool aSelected,
                              nsSpread aSpread)
{
  //traverse through children unselect tables
#if 0
  if ((aSpread == eSpreadDown)){
    nsIFrame* kid;
    FirstChild(nsnull, &kid);
    while (nsnull != kid) {
      kid->SetSelected(nsnull,aSelected,eSpreadDown);

      kid->GetNextSibling(&kid);
    }
  }
  //return nsFrame::SetSelected(aRange,aSelected,eSpreadNone);
#endif
  // Must call base class to set mSelected state and trigger repaint of frame
  // Note that in current version, aRange and aSpread are ignored,
  //   only this frame is considered
  nsFrame::SetSelected(aPresContext, aRange, aSelected, aSpread);

  nsCOMPtr<nsIPresShell> shell;
  nsresult result = aPresContext->GetShell(getter_AddRefs(shell));
  if (NS_FAILED(result))
    return result;
  nsCOMPtr<nsIFrameSelection> frameSelection;
  result = shell->GetFrameSelection(getter_AddRefs(frameSelection));
  if (NS_SUCCEEDED(result) && frameSelection)
  {
    PRBool tableCellSelectionMode;
    result = frameSelection->GetTableCellSelection(&tableCellSelectionMode);
    if (NS_SUCCEEDED(result) && tableCellSelectionMode)
    {
      nsRect frameRect;
      GetRect(frameRect);
      nsRect rect(0, 0, frameRect.width, frameRect.height);
      Invalidate(aPresContext, rect, PR_FALSE);
    }
  }
  return NS_OK;
}

PRIntn
nsTableCellFrame::GetSkipSides() const
{
  PRIntn skip = 0;
  if (nsnull != mPrevInFlow) {
    skip |= 1 << NS_SIDE_TOP;
  }
  if (nsnull != mNextInFlow) {
    skip |= 1 << NS_SIDE_BOTTOM;
  }
  return skip;
}

PRBool nsTableCellFrame::ParentDisablesSelection() const //override default behavior
{
  PRBool returnval;
  if (NS_FAILED(GetSelected(&returnval)))
    return PR_FALSE;
  if (returnval)
    return PR_TRUE;
  return nsFrame::ParentDisablesSelection();
}

void nsTableCellFrame::SetBorderEdge(PRUint8       aSide, 
                                     PRInt32       aRowIndex, 
                                     PRInt32       aColIndex, 
                                     nsBorderEdge *aBorder,
                                     nscoord       aOddAmountToAdd)
{
  NS_PRECONDITION(mBorderEdges, "haven't allocated border edges struct");
  nsBorderEdge *border = nsnull;
  switch (aSide)
  {
    case NS_SIDE_TOP:
    {
      PRInt32 baseColIndex;
      GetColIndex(baseColIndex);
      PRInt32 colIndex = aColIndex-baseColIndex;
      border = (nsBorderEdge *)(mBorderEdges->mEdges[aSide].ElementAt(colIndex));
      mBorderEdges->mMaxBorderWidth.top = PR_MAX(aBorder->mWidth+aOddAmountToAdd, mBorderEdges->mMaxBorderWidth.top);
      break;
    }

    case NS_SIDE_BOTTOM:
    {
      PRInt32 baseColIndex;
      GetColIndex(baseColIndex);
      PRInt32 colIndex = aColIndex-baseColIndex;
      border = (nsBorderEdge *)(mBorderEdges->mEdges[aSide].ElementAt(colIndex));
      mBorderEdges->mMaxBorderWidth.bottom = PR_MAX(aBorder->mWidth+aOddAmountToAdd, mBorderEdges->mMaxBorderWidth.bottom);
      break;
    }
  
    case NS_SIDE_LEFT:
    {
      PRInt32 baseRowIndex;
      GetRowIndex(baseRowIndex);
      PRInt32 rowIndex = aRowIndex-baseRowIndex;
      border = (nsBorderEdge *)(mBorderEdges->mEdges[aSide].ElementAt(rowIndex));
      mBorderEdges->mMaxBorderWidth.left = PR_MAX(aBorder->mWidth+aOddAmountToAdd, mBorderEdges->mMaxBorderWidth.left);
      break;
    }
      
    case NS_SIDE_RIGHT:  
    {
      PRInt32 baseRowIndex;
      GetRowIndex(baseRowIndex);
      PRInt32 rowIndex = aRowIndex-baseRowIndex;
      border = (nsBorderEdge *)(mBorderEdges->mEdges[aSide].ElementAt(rowIndex));
      mBorderEdges->mMaxBorderWidth.right = PR_MAX(aBorder->mWidth+aOddAmountToAdd, mBorderEdges->mMaxBorderWidth.right);
      break;
    }
  }
  if (nsnull!=border) {
    *border=*aBorder;
    border->mWidth += aOddAmountToAdd;
  }
  else {
    //XXX determine why this was asserting (after beta)
    //NS_ASSERTION(PR_FALSE, "bad border edge state");
  }
}

/**
  *
  * Align the cell's child frame within the cell
  *
  */
void nsTableCellFrame::VerticallyAlignChild(nsIPresContext*          aPresContext,
                                            const nsHTMLReflowState& aReflowState,
                                            nscoord                  aMaxAscent)
{
  const nsStyleText* textStyle =
      (const nsStyleText*)mStyleContext->GetStyleData(eStyleStruct_Text);
  /* XXX: remove tableFrame when border-collapse inherits */
  nsTableFrame* tableFrame = nsnull;
  (void) nsTableFrame::GetTableFrame(this, tableFrame);
  nsMargin borderPadding;
  GetCellBorder (borderPadding, tableFrame);
  nsMargin padding = nsTableFrame::GetPadding(aReflowState, this);
  borderPadding += padding;
  
  nscoord topInset = borderPadding.top;
  nscoord bottomInset = borderPadding.bottom;

  // As per bug 10207, we map 'sub', 'super', 'text-top', 'text-bottom', 
  // length and percentage values to 'baseline'
  // XXX It seems that we don't get to see length and percentage values here
  //     because the Style System has already fixed the error and mapped them
  //     to whatever is inherited from the parent, i.e, 'middle' in most cases.
  PRUint8 verticalAlignFlags = NS_STYLE_VERTICAL_ALIGN_BASELINE;
  if (textStyle->mVerticalAlign.GetUnit() == eStyleUnit_Enumerated) {
    verticalAlignFlags = textStyle->mVerticalAlign.GetIntValue();
    if (verticalAlignFlags != NS_STYLE_VERTICAL_ALIGN_TOP &&
        verticalAlignFlags != NS_STYLE_VERTICAL_ALIGN_MIDDLE &&
        verticalAlignFlags != NS_STYLE_VERTICAL_ALIGN_BOTTOM)
    {
      verticalAlignFlags = NS_STYLE_VERTICAL_ALIGN_BASELINE;
    }
  }

  nscoord height = mRect.height;
  nsRect kidRect;  
  nsIFrame* firstKid = mFrames.FirstChild();
  firstKid->GetRect(kidRect);
  nscoord childHeight = kidRect.height;
  

  // Vertically align the child
  nscoord kidYTop = 0;
  switch (verticalAlignFlags) 
  {
    case NS_STYLE_VERTICAL_ALIGN_BASELINE:
      // Align the baselines of the child frame with the baselines of 
      // other children in the same row which have 'vertical-align: baseline'
      kidYTop = topInset + aMaxAscent - GetDesiredAscent();
    break;

    case NS_STYLE_VERTICAL_ALIGN_TOP:
      // Align the top of the child frame with the top of the content area, 
      kidYTop = topInset;
    break;

    case NS_STYLE_VERTICAL_ALIGN_BOTTOM:
      // Align the bottom of the child frame with the bottom of the content area, 
      kidYTop = height - childHeight - bottomInset;
    break;

    default:
    case NS_STYLE_VERTICAL_ALIGN_MIDDLE:
      // Align the middle of the child frame with the middle of the content area, 
      kidYTop = (height - childHeight - bottomInset + topInset) / 2;
  }
  firstKid->MoveTo(aPresContext, kidRect.x, kidYTop);
  if (kidYTop != kidRect.y) {
    // Make sure any child views are correctly positioned. We know the inner table
    // cell won't have a view
    nsContainerFrame::PositionChildViews(aPresContext, firstKid);
  }
}

// As per bug 10207, we map 'sub', 'super', 'text-top', 'text-bottom', 
// length and percentage values to 'baseline'
// XXX It seems that we don't get to see length and percentage values here
//     because the Style System has already fixed the error and mapped them
//     to whatever is inherited from the parent, i.e, 'middle' in most cases.
PRBool
nsTableCellFrame::HasVerticalAlignBaseline()
{
  const nsStyleText* textStyle;
  GetStyleData(eStyleStruct_Text, (const nsStyleStruct*&)textStyle);
  if (textStyle->mVerticalAlign.GetUnit() == eStyleUnit_Enumerated) {
    PRUint8 verticalAlignFlags = textStyle->mVerticalAlign.GetIntValue();
    if (verticalAlignFlags == NS_STYLE_VERTICAL_ALIGN_TOP ||
        verticalAlignFlags == NS_STYLE_VERTICAL_ALIGN_MIDDLE ||
        verticalAlignFlags == NS_STYLE_VERTICAL_ALIGN_BOTTOM)
    {
      return PR_FALSE;
    }
  }
  return PR_TRUE;
}

PRInt32 nsTableCellFrame::GetRowSpan()
{  
  PRInt32 rowSpan=1;
  nsIHTMLContent *hc=nsnull;
  nsresult rv = mContent->QueryInterface(kIHTMLContentIID, (void**) &hc);
  if (NS_OK==rv)
  {
    nsHTMLValue val;
    hc->GetHTMLAttribute(nsHTMLAtoms::rowspan, val); 
    if (eHTMLUnit_Integer == val.GetUnit()) { 
       rowSpan=val.GetIntValue(); 
    }
    NS_RELEASE(hc);
  }
  return rowSpan;
}

PRInt32 nsTableCellFrame::GetColSpan()
{  
  PRInt32 colSpan=1;
  nsIHTMLContent *hc=nsnull;
  nsresult rv = mContent->QueryInterface(kIHTMLContentIID, (void**) &hc);
  if (NS_OK==rv)
  {
    nsHTMLValue val;
    hc->GetHTMLAttribute(nsHTMLAtoms::colspan, val); 
    if (eHTMLUnit_Integer == val.GetUnit()) { 
       colSpan=val.GetIntValue(); 
    }
    NS_RELEASE(hc);
  }
  return colSpan;
}


static
void DebugCheckChildSize(nsIFrame*            aChild, 
                         nsHTMLReflowMetrics& aMet, 
                         nsSize&              aAvailSize,
                         PRBool               aIsPass2Reflow)
{

/* approved for commenting out by rickg
  if (aMet.width > aAvailSize.width) {
    nsAutoString tmp;
    aChild->GetFrameName(tmp);
    printf("WARNING: cell ");
    fputs(tmp, stdout);
    printf(" content has desired width %d given avail width %d\n",
            aMet.width, aAvailSize.width);
  }
*/
  if (aIsPass2Reflow) {
    if ((aMet.width < 0) || (aMet.width > 60000)) {
      printf("WARNING: cell content %p has large width %d \n", aChild, aMet.width);
    }
    if ((aMet.height < 0) || (aMet.height > 60000)) {
      printf("WARNING: cell content %p has large height %d \n", aChild, aMet.height);
    }
  }
  if (aMet.maxElementSize) {
    nscoord tmp = aMet.maxElementSize->width;
    if ((tmp < 0) || (tmp > 60000)) {
      printf("WARNING: cell content %p has large max element width %d \n", aChild, tmp);
    }
    tmp = aMet.maxElementSize->height;
    if ((tmp < 0) || (tmp > 60000)) {
      printf("WARNING: cell content %p has large max element height %d \n", aChild, tmp);
    }
  }
}

/**
  */
NS_METHOD nsTableCellFrame::Reflow(nsIPresContext*          aPresContext,
                                   nsHTMLReflowMetrics&     aDesiredSize,
                                   const nsHTMLReflowState& aReflowState,
                                   nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsTableCellFrame", aReflowState.reason);
  if (nsDebugTable::gRflCell) nsTableFrame::DebugReflow("TC::Rfl", this, &aReflowState, nsnull);

  nsresult rv = NS_OK;
  // this should probably be cached somewhere
  nsCompatibility compatMode;
  aPresContext->GetCompatibilityMode(&compatMode);

  // Initialize out parameter
  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width = 0;
    aDesiredSize.maxElementSize->height = 0;
  }

  aStatus = NS_FRAME_COMPLETE;
  nsSize availSize(aReflowState.availableWidth, aReflowState.availableHeight);
  nsSize maxElementSize;
  nsSize *pMaxElementSize = aDesiredSize.maxElementSize;
  if (NS_UNCONSTRAINEDSIZE==aReflowState.availableWidth)
    pMaxElementSize = &maxElementSize;

  /* XXX: remove tableFrame when border-collapse inherits */
  nsTableFrame* tableFrame=nsnull;
  rv = nsTableFrame::GetTableFrame(this, tableFrame);
  nsMargin borderPadding = aReflowState.mComputedPadding;
  nsMargin border;
  GetCellBorder(border, tableFrame);
  if ((NS_UNCONSTRAINEDSIZE == availSize.width) || !GetContentEmpty()) {
    borderPadding += border;
  }
  
  nscoord topInset    = borderPadding.top;
  nscoord rightInset  = borderPadding.right;
  nscoord bottomInset = borderPadding.bottom;
  nscoord leftInset   = borderPadding.left;

  // reduce available space by insets, if we're in a constrained situation
  if (NS_UNCONSTRAINEDSIZE!=availSize.width)
    availSize.width -= leftInset+rightInset;
  if (NS_UNCONSTRAINEDSIZE!=availSize.height)
    availSize.height -= topInset+bottomInset;

  PRBool  isStyleChanged = PR_FALSE;
  if (eReflowReason_Incremental == aReflowState.reason) 
  {
    // We *must* do this otherwise incremental reflow that's
    // passing through will not work right.
    nsIFrame* next;
    aReflowState.reflowCommand->GetNext(next);

    // if it is a StyleChanged reflow targeted at this cell frame,
    // handle that here
    // first determine if this frame is the target or not
    nsIFrame *target=nsnull;
    rv = aReflowState.reflowCommand->GetTarget(target);
    if ((PR_TRUE==NS_SUCCEEDED(rv)) && (nsnull!=target))
    {
      if (this==target)
      {
        nsIReflowCommand::ReflowType type;
        aReflowState.reflowCommand->GetType(type);
        if (nsIReflowCommand::StyleChanged==type)
        {
          isStyleChanged = PR_TRUE;
        }
        else {
          NS_ASSERTION(PR_FALSE, "table cell target of illegal incremental reflow type");
        }
      }
    }
    // if any of these conditions are not true, we just pass the reflow command down
  }

  // Try to reflow the child into the available space. It might not
  // fit or might need continuing.
  if (availSize.height < 0)
    availSize.height = 1;

  nsHTMLReflowMetrics kidSize(pMaxElementSize, aDesiredSize.mFlags);
  kidSize.width=kidSize.height=kidSize.ascent=kidSize.descent=0;
  SetPriorAvailWidth(aReflowState.availableWidth);
  nsIFrame* firstKid = mFrames.FirstChild();
  nsHTMLReflowState kidReflowState(aPresContext, aReflowState, firstKid,
                                   availSize);

  // If it was a style change targeted at us, then reflow the child using
  // the special reflow reason
  if (isStyleChanged) {
    kidReflowState.reason = eReflowReason_StyleChange;
    kidReflowState.reflowCommand = nsnull;
  }

  // Assume the inner child will stay positioned exactly where it is. Later in
  // VerticallyAlignChild() we'll move it if it turns out to be wrong. This
  // avoids excessive movement and is more stable
  nsPoint kidOrigin;
  if (eReflowReason_Initial == aReflowState.reason) {
    kidOrigin.MoveTo(leftInset, topInset);
  } else {
    // handle percent padding-left which was 0 during initial reflow
    if (eStyleUnit_Percent == aReflowState.mStyleSpacing->mPadding.GetLeftUnit()) {
      nsRect kidRect;
      firstKid->GetRect(kidRect);
      // only move in the x direction for the same reason as above
      kidOrigin.MoveTo(leftInset, kidRect.y);
      firstKid->MoveTo(aPresContext, leftInset, kidRect.y);
    }
    firstKid->GetOrigin(kidOrigin);
  }

  if (nsDebugTable::gRflArea) nsTableFrame::DebugReflow("Area::Rfl en", firstKid, &kidReflowState, nsnull);
  ReflowChild(firstKid, aPresContext, kidSize, kidReflowState,
              kidOrigin.x, kidOrigin.y, 0, aStatus);
  if (isStyleChanged) {
    Invalidate(aPresContext, mRect);
  }
  if (nsDebugTable::gRflArea) nsTableFrame::DebugReflow("Area::Rfl ex", firstKid, nsnull, &kidSize, aStatus);

#ifdef NS_DEBUG
  DebugCheckChildSize(firstKid, kidSize, availSize, (NS_UNCONSTRAINEDSIZE != aReflowState.availableWidth));
#endif

  // 0 dimensioned cells need to be treated specially in Standard/NavQuirks mode 
  // see testcase "emptyCells.html"
  if ((0 == kidSize.width) || (0 == kidSize.height)) { // XXX why was this &&
    SetContentEmpty(PR_TRUE);
    if (NS_UNCONSTRAINEDSIZE == kidReflowState.availableWidth) {
      // need to reduce the insets by border if the cell is empty
      leftInset   -= border.left;
      rightInset  -= border.right;
      topInset    -= border.top;
      bottomInset -= border.bottom;
    }
  }
  else {
    SetContentEmpty(PR_FALSE);
  }

  const nsStylePosition* pos;
  GetStyleData(eStyleStruct_Position, ((const nsStyleStruct *&)pos));

  // calculate the min cell width
  float p2t;
  aPresContext->GetScaledPixelsToTwips(&p2t);
  nscoord onePixel = NSIntPixelsToTwips(1, p2t); 
  nscoord smallestMinWidth = onePixel;
  if (eCompatibility_NavQuirks == compatMode) {
    if ((pos->mWidth.GetUnit() != eStyleUnit_Coord)   &&
        (pos->mWidth.GetUnit() != eStyleUnit_Percent)) {
      if (PR_TRUE == GetContentEmpty()) {
        if (border.left > 0) 
          smallestMinWidth += onePixel;
        if (border.right > 0) 
          smallestMinWidth += onePixel;
      }
    }
  }
  PRInt32 colspan = tableFrame->GetEffectiveColSpan(*this);
  if (colspan > 1) {
    smallestMinWidth = PR_MAX(smallestMinWidth, colspan * onePixel);
    nscoord spacingX = tableFrame->GetCellSpacingX();
    nscoord spacingExtra = spacingX * (colspan - 1);
    smallestMinWidth += spacingExtra;
    if (aReflowState.mComputedPadding.left > 0) {
      smallestMinWidth -= onePixel;
    }
  }
 
  if ((0 == kidSize.width) && (NS_UNCONSTRAINEDSIZE != kidReflowState.availableWidth)) {
    // empty content has to be forced to the assigned width for resize or incremental reflow
    kidSize.width = kidReflowState.availableWidth;
  }
  if (0 == kidSize.height) {
    if ((pos->mHeight.GetUnit() != eStyleUnit_Coord) &&
        (pos->mHeight.GetUnit() != eStyleUnit_Percent)) {
      // Standard mode should probably be 0 pixels high instead of 1
      PRInt32 pixHeight = (eCompatibility_Standard == compatMode) ? 1 : 2;
      kidSize.height = NSIntPixelsToTwips(pixHeight, p2t);
      if ((nsnull != aDesiredSize.maxElementSize) && (0 == pMaxElementSize->height))
        pMaxElementSize->height = kidSize.height;
    }
  }
  // end 0 dimensioned cells

  kidSize.width = PR_MAX(kidSize.width, smallestMinWidth); 

  // Place the child
  //////////////////////////////// HACK //////////////////////////////
  kidSize.width = PR_MIN(kidSize.width, availSize.width);
  ///////////////////////////// END HACK /////////////////////////////
  FinishReflowChild(firstKid, aPresContext, kidSize,
                    kidOrigin.x, kidOrigin.y, 0);
    
  // first, compute the height which can be set w/o being restricted by aMaxSize.height
  nscoord cellHeight = kidSize.height;
  if (NS_UNCONSTRAINEDSIZE != cellHeight) {
    cellHeight += topInset + bottomInset;
  }

  // next determine the cell's width
  nscoord cellWidth = kidSize.width;      // at this point, we've factored in the cell's style attributes

  // factor in border and padding
  if (NS_UNCONSTRAINEDSIZE != cellWidth) {
    cellWidth += leftInset + rightInset;    
  }

  // set the cell's desired size and max element size
  aDesiredSize.width   = cellWidth;
  aDesiredSize.height  = cellHeight;
  aDesiredSize.ascent  = topInset;
  aDesiredSize.descent = bottomInset;

  aDesiredSize.ascent  += kidSize.ascent;
  aDesiredSize.descent += kidSize.descent;

  if (aDesiredSize.mFlags & NS_REFLOW_CALC_MAX_WIDTH) {
    aDesiredSize.mMaximumWidth = kidSize.mMaximumWidth + leftInset + rightInset;
  }
  if (nsnull!=aDesiredSize.maxElementSize) {
    *aDesiredSize.maxElementSize = *pMaxElementSize;
    if (0!=pMaxElementSize->height) {
      aDesiredSize.maxElementSize->height += topInset + bottomInset;
    }
    aDesiredSize.maxElementSize->width = PR_MAX(smallestMinWidth, aDesiredSize.maxElementSize->width); 
    aDesiredSize.maxElementSize->width += leftInset + rightInset;
  }
  // remember my desired size for this reflow
  SetDesiredSize(aDesiredSize);

  if (nsDebugTable::gRflCell) nsTableFrame::DebugReflow("TC::Rfl ex", this, nsnull, &aDesiredSize, aStatus);

  return NS_OK;
}

/**
  *
  * Update the border style to map to the HTML border style
  *
  */
void nsTableCellFrame::MapHTMLBorderStyle(nsIPresContext* aPresContext, 
                                          nsStyleSpacing& aSpacingStyle,
                                          nsTableFrame*   aTableFrame)
{
  //adjust the border style based on the table rules attribute

  /* The RULES code below has been disabled because collapsing borders have been disabled 
     and RULES depend on collapsing borders

  const nsStyleTable* tableStyle;
  aTableFrame->GetStyleData(eStyleStruct_Table, (const nsStyleStruct *&)tableStyle);

  switch (tableStyle->mRules)
  {
  case NS_STYLE_TABLE_RULES_NONE:
    aSpacingStyle.SetBorderStyle(NS_SIDE_TOP, NS_STYLE_BORDER_STYLE_NONE);
    aSpacingStyle.SetBorderStyle(NS_SIDE_LEFT, NS_STYLE_BORDER_STYLE_NONE);
    aSpacingStyle.SetBorderStyle(NS_SIDE_BOTTOM, NS_STYLE_BORDER_STYLE_NONE);
    aSpacingStyle.SetBorderStyle(NS_SIDE_RIGHT, NS_STYLE_BORDER_STYLE_NONE);
    break;

  case NS_STYLE_TABLE_RULES_COLS:
	  aSpacingStyle.SetBorderStyle(NS_SIDE_TOP, NS_STYLE_BORDER_STYLE_NONE);
    aSpacingStyle.SetBorderStyle(NS_SIDE_BOTTOM, NS_STYLE_BORDER_STYLE_NONE);
    break;

  case NS_STYLE_TABLE_RULES_ROWS:
    aSpacingStyle.SetBorderStyle(NS_SIDE_LEFT, NS_STYLE_BORDER_STYLE_NONE);
    aSpacingStyle.SetBorderStyle(NS_SIDE_RIGHT, NS_STYLE_BORDER_STYLE_NONE);
    break;

  default:
    // do nothing for "GROUPS" or "ALL" or for any illegal value
    // "GROUPS" will be handled in nsTableFrame::ProcessGroupRules
    break;
  }
  */
}


PRBool nsTableCellFrame::ConvertToPixelValue(nsHTMLValue& aValue, PRInt32 aDefault, PRInt32& aResult)
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

void nsTableCellFrame::MapBorderPadding(nsIPresContext* aPresContext)
{
  // Check to see if the table has cell padding or defined for the table. If true, 
  // then this setting overrides any specific border or padding information in the 
  // cell. If these attributes are not defined, the the cells attributes are used

  nsTableFrame* tableFrame;
  nsTableFrame::GetTableFrame(this, tableFrame);
  NS_ASSERTION(tableFrame,"Table must not be null");
  if (!tableFrame)
    return;

  // get the table frame style context, and from it get cellpadding, cellspacing, and border info
  const nsStyleTable* tableStyle;
  tableFrame->GetStyleData(eStyleStruct_Table, (const nsStyleStruct *&)tableStyle);
  const nsStyleSpacing* tableSpacingStyle;
  tableFrame->GetStyleData(eStyleStruct_Spacing,(const nsStyleStruct *&)tableSpacingStyle);
  nsStyleSpacing* spacingData = (nsStyleSpacing*)mStyleContext->GetMutableStyleData(eStyleStruct_Spacing);

  float p2t;
  aPresContext->GetPixelsToTwips(&p2t);

  // Get the table's cellpadding or use 2 pixels as the default if it is not set.
  // This assumes that ua.css does not set padding for the cell.
  nscoord defaultPadding = tableFrame->GetCellPadding();
  if (-1 == defaultPadding) { // not set in table
    defaultPadding = NSIntPixelsToTwips(1, p2t);
  }

  // if the padding is not already set, set it to the table's cellpadding
  if (eStyleUnit_Null == spacingData->mPadding.GetTopUnit()) 
    spacingData->mPadding.SetTop(defaultPadding);
  if (eStyleUnit_Null == spacingData->mPadding.GetRightUnit()) 
    spacingData->mPadding.SetRight(defaultPadding); 
  if (eStyleUnit_Null == spacingData->mPadding.GetBottomUnit())
    spacingData->mPadding.SetBottom(defaultPadding);
  if (eStyleUnit_Null == spacingData->mPadding.GetLeftUnit()) 
    spacingData->mPadding.SetLeft(defaultPadding);

  MapHTMLBorderStyle(aPresContext, *spacingData, tableFrame);

  MapVAlignAttribute(aPresContext, tableFrame);
  MapHAlignAttribute(aPresContext, tableFrame);
  
}

/* XXX: this code will not work properly until the style and layout code has been updated 
 * as outlined in Bugzilla bug report 1802 and 915 */
void nsTableCellFrame::MapVAlignAttribute(nsIPresContext* aPresContext, nsTableFrame *aTableFrame)
{
  const nsStyleText* textStyle;
  GetStyleData(eStyleStruct_Text,(const nsStyleStruct *&)textStyle);
  // check if valign is set on the cell
  // this condition will also be true if we inherited valign from the row or rowgroup
  if (textStyle->mVerticalAlign.GetUnit() == eStyleUnit_Enumerated) {
    return; // valign is already set on this cell
  }

  // check if valign is set on the cell's COL (or COLGROUP by inheritance)
  nsTableColFrame* colFrame;
  PRInt32 colIndex;
  GetColIndex(colIndex);
  aTableFrame->GetColumnFrame(colIndex, colFrame);
  if (colFrame) {
    const nsStyleText* colTextStyle;
    colFrame->GetStyleData(eStyleStruct_Text,(const nsStyleStruct *&)colTextStyle);
    if (colTextStyle->mVerticalAlign.GetUnit() == eStyleUnit_Enumerated) {
      nsStyleText* cellTextStyle = (nsStyleText*)mStyleContext->GetMutableStyleData(eStyleStruct_Text);
      cellTextStyle->mVerticalAlign.SetIntValue(colTextStyle->mVerticalAlign.GetIntValue(), eStyleUnit_Enumerated);
      return; // valign set from COL info
    }
  }

  // otherwise, set the vertical align attribute to the HTML default
  nsStyleText* cellTextStyle = (nsStyleText*)mStyleContext->GetMutableStyleData(eStyleStruct_Text);
  cellTextStyle->mVerticalAlign.SetIntValue(NS_STYLE_VERTICAL_ALIGN_MIDDLE, eStyleUnit_Enumerated);
}

/* XXX: this code will not work properly until the style and layout code has been updated 
 * as outlined in Bugzilla bug report 1802 and 915.
 * In particular, mTextAlign has to be an nsStyleCoord, not just an int */
void nsTableCellFrame::MapHAlignAttribute(nsIPresContext* aPresContext, 
                                          nsTableFrame*   aTableFrame)
{
#if 0
  const nsStyleText* textStyle;
  GetStyleData(eStyleStruct_Text,(const nsStyleStruct *&)textStyle);
  // check if halign is set on the cell
  // cells do not inherited halign from the row or rowgroup
  if (NS_STYLE_TEXT_ALIGN_DEFAULT != textStyle->mTextAlign) {
    return; // text align is already set on this cell
  }

  // check if halign is set on the cell's ROW (or ROWGROUP by inheritance)
  nsIFrame* rowFrame;
  GetParent(&rowFrame);
  const nsStyleText* rowTextStyle;
  rowFrame->GetStyleData(eStyleStruct_Text,(const nsStyleStruct *&)rowTextStyle);
  if (NS_STYLE_TEXT_ALIGN_DEFAULT != rowTextStyle->mTextAlign) {
    nsStyleText* cellTextStyle = (nsStyleText*)mStyleContext->GetMutableStyleData(eStyleStruct_Text);
    cellTextStyle->mTextAlign = rowTextStyle->mTextAlign;
    return; // halign set from ROW info
  }

  // check if halign is set on the cell's COL (or COLGROUP by inheritance)
  nsTableColFrame* colFrame;
  PRInt32 colIndex;
  GetColIndex(colIndex);
  aTableFrame->GetColumnFrame(colIndex, colFrame);
  if (colFrame) {
    const nsStyleText* colTextStyle;
    colFrame->GetStyleData(eStyleStruct_Text,(const nsStyleStruct *&)colTextStyle);
    if (NS_STYLE_TEXT_ALIGN_DEFAULT != colTextStyle->mTextAlign) {
      nsStyleText* cellTextStyle = (nsStyleText*)mStyleContext->GetMutableStyleData(eStyleStruct_Text);
      cellTextStyle->mTextAlign = colTextStyle->mTextAlign;
      return; // halign set from COL info
    }
  }

  // otherwise, set the text align to the HTML default (center for TH, left for TD)
  nsStyleText* cellTextStyle = (nsStyleText*)mStyleContext->GetMutableStyleData(eStyleStruct_Text);
  nsIAtom* tag;
  if (mContent) {
    mContent->GetTag(tag);
    if (tag) {
      cellTextStyle->mTextAlign = (nsHTMLAtoms::th == tag)
                                  ? NS_STYLE_TEXT_ALIGN_CENTER 
                                  : NS_STYLE_TEXT_ALIGN_LEFT;
    }
    NS_IF_RELEASE(tag);
  }
#endif
}  

// Subclass hook for style post processing
NS_METHOD nsTableCellFrame::DidSetStyleContext(nsIPresContext* aPresContext)
{
#ifdef NOISY_STYLE
  printf("nsTableCellFrame::DidSetStyleContext \n");
#endif

  MapBorderPadding(aPresContext);
  mStyleContext->RecalcAutomaticData(aPresContext);
  return NS_OK;
}

/* ----- global methods ----- */

NS_IMPL_ADDREF_INHERITED(nsTableCellFrame, nsHTMLContainerFrame)
NS_IMPL_RELEASE_INHERITED(nsTableCellFrame, nsHTMLContainerFrame)

nsresult nsTableCellFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(NS_GET_IID(nsITableCellLayout))) {
    *aInstancePtr = (void*) (nsITableCellLayout *)this;
    return NS_OK;
  } else {
    return nsHTMLContainerFrame::QueryInterface(aIID, aInstancePtr);
  }
}

/* This is primarily for editor access via nsITableLayout */
NS_IMETHODIMP
nsTableCellFrame::GetCellIndexes(PRInt32 &aRowIndex, PRInt32 &aColIndex)
{
  nsresult res = GetRowIndex(aRowIndex);
  if (NS_FAILED(res))
  {
    aColIndex = 0;
    return res;
  }
  aColIndex = mColIndex;
  return  NS_OK;
}

NS_IMETHODIMP
nsTableCellFrame::GetPreviousCellInColumn(nsITableCellLayout **aCellLayout)
{
  if (!aCellLayout) return NS_ERROR_NULL_POINTER;
  *aCellLayout = nsnull;

  nsTableFrame* tableFrame = nsnull; 
  nsresult rv = nsTableFrame::GetTableFrame(this, tableFrame);
  if (NS_FAILED(rv)) return rv;
  if (!tableFrame) return NS_ERROR_FAILURE;

  // Get current cell location
  PRInt32 rowIndex, colIndex;
  GetCellIndexes(rowIndex, colIndex);
  if (colIndex > 0)
  {
    // Get the cellframe at previous colIndex
    nsTableCellFrame *cellFrame = tableFrame->GetCellFrameAt(rowIndex, colIndex-1);
    if (cellFrame)
      cellFrame->QueryInterface(NS_GET_IID(nsITableCellLayout), (void **)aCellLayout);  
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTableCellFrame::GetNextCellInColumn(nsITableCellLayout **aCellLayout)
{
  if (!aCellLayout) return NS_ERROR_NULL_POINTER;
  *aCellLayout = nsnull;

  nsTableFrame* tableFrame = nsnull; 
  nsresult rv = nsTableFrame::GetTableFrame(this, tableFrame);
  if (NS_FAILED(rv)) return rv;
  if (!tableFrame) return NS_ERROR_FAILURE;

  // Get current cell location
  PRInt32 rowIndex, colIndex;
  GetCellIndexes(rowIndex, colIndex);

  // Get the cellframe at next colIndex
  nsTableCellFrame *cellFrame = tableFrame->GetCellFrameAt(rowIndex, colIndex+1);
  if (cellFrame)
    cellFrame->QueryInterface(NS_GET_IID(nsITableCellLayout), (void **)aCellLayout);  

  return NS_OK;
}

nsresult 
NS_NewTableCellFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsTableCellFrame* it = new (aPresShell) nsTableCellFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}



/* ----- methods from CellLayoutData ----- */

/**
  * Given an Edge, find the opposing edge (top<-->bottom, left<-->right)
  *
  **/
PRUint8 nsTableCellFrame::GetOpposingEdge(PRUint8 aEdge)
{
   PRUint8 result;

   switch (aEdge)
    {
   case NS_SIDE_LEFT:
        result = NS_SIDE_RIGHT;
        break;

   case NS_SIDE_RIGHT:
        result = NS_SIDE_LEFT;
        break;

   case NS_SIDE_TOP:
        result = NS_SIDE_BOTTOM;
        break;

   case NS_SIDE_BOTTOM:
        result = NS_SIDE_TOP;
        break;

      default:
        result = NS_SIDE_TOP;
     }
  return result;
}

void 
nsTableCellFrame::GetCellBorder(nsMargin&     aBorder, 
                                nsTableFrame* aTableFrame)
{
  aBorder.left = aBorder.right = aBorder.top = aBorder.bottom = 0;
  if (nsnull==aTableFrame) {
    return;
  }

  if (NS_STYLE_BORDER_COLLAPSE==aTableFrame->GetBorderCollapseStyle()) {
    NS_PRECONDITION(mBorderEdges, "haven't allocated border edges struct");
    aBorder = mBorderEdges->mMaxBorderWidth;
  } else {
    const nsStyleSpacing* spacing;
    GetStyleData(eStyleStruct_Spacing, (const nsStyleStruct*&)spacing);
    spacing->GetBorder(aBorder);
  }
}

NS_IMETHODIMP
nsTableCellFrame::GetFrameType(nsIAtom** aType) const
{
  NS_PRECONDITION(nsnull != aType, "null OUT parameter pointer");
  *aType = nsLayoutAtoms::tableCellFrame; 
  NS_ADDREF(*aType);
  return NS_OK;
}

#ifdef DEBUG
NS_IMETHODIMP
nsTableCellFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("TableCell", aResult);
}
#endif

// Destructor function for the collapse offset frame property
static void
DestroyPointFunc(nsIPresContext* aPresContext,
                 nsIFrame*       aFrame,
                 nsIAtom*        aPropertyName,
                 void*           aPropertyValue)
{
  delete (nsPoint*)aPropertyValue;
}

static nsPoint*
GetCollapseOffsetProperty(nsIPresContext* aPresContext,
                          nsIFrame*       aFrame,
                          PRBool          aCreateIfNecessary = PR_FALSE)
{
  nsCOMPtr<nsIPresShell>     presShell;
  aPresContext->GetShell(getter_AddRefs(presShell));

  if (presShell) {
    nsCOMPtr<nsIFrameManager>  frameManager;
    presShell->GetFrameManager(getter_AddRefs(frameManager));
  
    if (frameManager) {
      void* value;
  
      frameManager->GetFrameProperty(aFrame, nsLayoutAtoms::collapseOffsetProperty,
                                     0, &value);
      if (value) {
        return (nsPoint*)value;  // the property already exists

      } else if (aCreateIfNecessary) {
        // The property isn't set yet, so allocate a new point, set the property,
        // and return the newly allocated point
        nsPoint*  offset = new nsPoint(0, 0);

        frameManager->SetFrameProperty(aFrame, nsLayoutAtoms::collapseOffsetProperty,
                                       offset, DestroyPointFunc);
        return offset;
      }
    }
  }

  return nsnull;
}

void nsTableCellFrame::SetCollapseOffsetX(nsIPresContext* aPresContext,
                                          nscoord         aXOffset)
{
  // Get the frame property (creating a point struct if necessary)
  nsPoint*  offset = ::GetCollapseOffsetProperty(aPresContext, this, PR_TRUE);

  if (offset) {
    offset->x = aXOffset;
  }
}

void nsTableCellFrame::SetCollapseOffsetY(nsIPresContext* aPresContext,
                                          nscoord         aYOffset)
{
  // Get the property (creating a point struct if necessary)
  nsPoint*  offset = ::GetCollapseOffsetProperty(aPresContext, this, PR_TRUE);

  if (offset) {
    offset->y = aYOffset;
  }
}

void nsTableCellFrame::GetCollapseOffset(nsIPresContext* aPresContext,
                                         nsPoint&        aOffset)
{
  // See if the property is set
  nsPoint*  offset = ::GetCollapseOffsetProperty(aPresContext, this);

  if (offset) {
    aOffset = *offset;
  } else {
    aOffset.MoveTo(0, 0);
  }
}

#ifdef DEBUG
NS_IMETHODIMP
nsTableCellFrame::SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const
{
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  PRUint32 sum = sizeof(*this);
  *aResult = sum;
  return NS_OK;
}
#endif
