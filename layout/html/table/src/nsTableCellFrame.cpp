/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsTableFrame.h"
#include "nsTableColFrame.h"
#include "nsTableCellFrame.h"
#include "nsTableFrame.h"
#include "nsTableRowGroupFrame.h"
#include "nsTablePainter.h"
#include "nsReflowPath.h"
#include "nsStyleContext.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsIRenderingContext.h"
#include "nsCSSRendering.h"
#include "nsIContent.h"
#include "nsIHTMLContent.h"
#include "nsHTMLParts.h"
#include "nsHTMLValue.h"
#include "nsHTMLAtoms.h"
#include "nsVoidArray.h"
#include "nsIView.h"
#include "nsLayoutAtoms.h"
#include "nsIPresShell.h"
#include "nsCOMPtr.h"
#include "nsIDOMHTMLTableCellElement.h"
#ifdef ACCESSIBILITY
#include "nsIAccessibilityService.h"
#endif
#include "nsIServiceManager.h"
#include "nsIDOMNode.h"
#include "nsINameSpaceManager.h"


//TABLECELL SELECTION
#include "nsIFrameSelection.h"
#include "nsILookAndFeel.h"


nsTableCellFrame::nsTableCellFrame()
{
  mBits.mColIndex  = 0;
  mPriorAvailWidth = 0;

  SetContentEmpty(PR_FALSE);
  SetNeedSpecialReflow(PR_FALSE);
  SetHadSpecialReflow(PR_FALSE);
  SetHasPctOverHeight(PR_FALSE);
  SetNeedPass2Reflow(PR_TRUE);

#ifdef DEBUG_TABLE_REFLOW_TIMING
  mTimer = new nsReflowTimer(this);
  mBlockTimer = new nsReflowTimer(this);
#endif
}

nsTableCellFrame::~nsTableCellFrame()
{
#ifdef DEBUG_TABLE_REFLOW_TIMING
  nsTableFrame::DebugReflowDone(this);
#endif
}

nsTableCellFrame*  
nsTableCellFrame::GetNextCell() const
{
  nsIFrame* childFrame = GetNextSibling();
  while (childFrame) {
    if (IS_TABLE_CELL(childFrame->GetType())) {
      return (nsTableCellFrame*)childFrame;
    }
    childFrame = childFrame->GetNextSibling();
  }
  return nsnull;
}

NS_IMETHODIMP
nsTableCellFrame::Init(nsPresContext*  aPresContext,
                       nsIContent*      aContent,
                       nsIFrame*        aParent,
                       nsStyleContext*  aContext,
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
    SetColIndex(colIndex);
  }

  return rv;
}

// nsIPercentHeightObserver methods

void
nsTableCellFrame::NotifyPercentHeight(const nsHTMLReflowState& aReflowState)
{
  if (!NeedSpecialReflow()) {
    // Only initiate a special reflow if we will be able to construct a computed height 
    // on the cell that will result in the frame getting a computed height. This can only 
    // happen (but not sufficient) if there is no computed height already set between the 
    // initiating frame and the cell.
    for (const nsHTMLReflowState* rs = aReflowState.parentReflowState; rs; rs = rs->parentReflowState) {
      if ((NS_UNCONSTRAINEDSIZE != rs->mComputedHeight) && (0 != rs->mComputedHeight)) {
        return;
      }
      // stop when we reach the cell frame
      if (rs->frame == this) {
        nsTableFrame::RequestSpecialHeightReflow(*rs);
        return;
      }
    }
    NS_ASSERTION(PR_FALSE, "program error in NotifyPercentHeight");
  }
}

// The cell needs to observe its block and things inside its block but nothing below that
PRBool 
nsTableCellFrame::NeedsToObserve(const nsHTMLReflowState& aReflowState)
{
  PRBool result = PR_FALSE;
  const nsHTMLReflowState* parentRS = aReflowState.parentReflowState;
  if (parentRS && (parentRS->mPercentHeightObserver == this)) { // cell observes the parent
    result = PR_TRUE;
    parentRS = parentRS->parentReflowState;
    if (parentRS && (parentRS->mPercentHeightObserver == this)) { // cell observers the grand parent
      parentRS = parentRS->parentReflowState;
      if (parentRS && (parentRS->mPercentHeightObserver == this)) { 
        // cell observes the great grand parent, so we have gone too deep
        result = PR_FALSE;
      }
    }
  }
  return result;
}

nsresult 
nsTableCellFrame::GetRowIndex(PRInt32 &aRowIndex) const
{
  nsresult result;
  nsTableRowFrame* row = NS_STATIC_CAST(nsTableRowFrame*, GetParent());
  if (row) {
    aRowIndex = row->GetRowIndex();
    result = NS_OK;
  }
  else {
    aRowIndex = 0;
    result = NS_ERROR_NOT_INITIALIZED;
  }
  return result;
}

nsresult 
nsTableCellFrame::GetColIndex(PRInt32 &aColIndex) const
{  
  if (mPrevInFlow) {
    return ((nsTableCellFrame*)GetFirstInFlow())->GetColIndex(aColIndex);
  }
  else {
    aColIndex = mBits.mColIndex;
    return  NS_OK;
  }
}

NS_IMETHODIMP
nsTableCellFrame::AttributeChanged(nsPresContext* aPresContext,
                                   nsIContent*     aChild,
                                   PRInt32         aNameSpaceID,
                                   nsIAtom*        aAttribute,
                                   PRInt32         aModType)
{
  // let the table frame decide what to do
  nsTableFrame* tableFrame = nsnull; 
  nsresult rv = nsTableFrame::GetTableFrame(this, tableFrame);
  if ((NS_SUCCEEDED(rv)) && (tableFrame)) {
    tableFrame->AttributeChangedFor(aPresContext, this, aChild, aAttribute); 
  }
  return NS_OK;
}

void nsTableCellFrame::SetPass1MaxElementWidth(nscoord aMaxWidth,
                                               nscoord aMaxElementWidth)
{ 
  nscoord maxElemWidth = aMaxElementWidth;
  // check for fixed width and not nowrap and not pre
  const nsStylePosition* stylePosition = GetStylePosition();
  const nsStyleText* styleText = GetStyleText();
  if (stylePosition->mWidth.GetUnit() == eStyleUnit_Coord &&
      styleText->mWhiteSpace != NS_STYLE_WHITESPACE_NOWRAP &&
      styleText->mWhiteSpace != NS_STYLE_WHITESPACE_PRE) {
    // has fixed width, check the content for nowrap
    nsAutoString nowrap;
    nsresult result = GetContent()->GetAttr(kNameSpaceID_None, nsHTMLAtoms::nowrap, nowrap);
    if(NS_CONTENT_ATTR_NOT_THERE != result) {
      // content has nowrap (is not mapped to style be cause it has width)
      // set the max element size to the value of the fixed width (NAV/IE quirk)
      maxElemWidth = NS_MAX(maxElemWidth, stylePosition->mWidth.GetCoordValue());
    }
  }
  mPass1MaxElementWidth = maxElemWidth;
}

NS_IMETHODIMP
nsTableCellFrame::AppendFrames(nsPresContext* aPresContext,
                               nsIPresShell&   aPresShell,
                               nsIAtom*        aListName,
                               nsIFrame*       aFrameList)
{
  NS_PRECONDITION(PR_FALSE, "unsupported operation");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsTableCellFrame::InsertFrames(nsPresContext* aPresContext,
                               nsIPresShell&   aPresShell,
                               nsIAtom*        aListName,
                               nsIFrame*       aPrevFrame,
                               nsIFrame*       aFrameList)
{
  NS_PRECONDITION(PR_FALSE, "unsupported operation");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsTableCellFrame::RemoveFrame(nsPresContext* aPresContext,
                              nsIPresShell&   aPresShell,
                              nsIAtom*        aListName,
                              nsIFrame*       aOldFrame)
{
  NS_PRECONDITION(PR_FALSE, "unsupported operation");
  return NS_ERROR_NOT_IMPLEMENTED;
}

void nsTableCellFrame::SetColIndex(PRInt32 aColIndex)
{  
  mBits.mColIndex = aColIndex;
}


//ASSURE DIFFERENT COLORS for selection
inline nscolor EnsureDifferentColors(nscolor colorA, nscolor colorB)
{
    if (colorA == colorB)
    {
      nscolor res;
      res = NS_RGB(NS_GET_R(colorA) ^ 0xff,
                   NS_GET_G(colorA) ^ 0xff,
                   NS_GET_B(colorA) ^ 0xff);
      return res;
    }
    return colorA;
}


nsresult
nsTableCellFrame::DecorateForSelection(nsPresContext* aPresContext,
                                       nsIRenderingContext& aRenderingContext,
                                       const nsStyleBackground *aStyleColor)
{
  PRInt16 displaySelection;
  displaySelection = DisplaySelection(aPresContext);
  if (displaySelection) {
    PRBool isSelected =
      (GetStateBits() & NS_FRAME_SELECTED_CONTENT) == NS_FRAME_SELECTED_CONTENT;
    if (isSelected) {
      nsIFrameSelection *frameSelection =
        aPresContext->PresShell()->FrameSelection();

      PRBool tableCellSelectionMode;
      nsresult result =
        frameSelection->GetTableCellSelection(&tableCellSelectionMode);
      if (NS_SUCCEEDED(result) && tableCellSelectionMode) {
        nscolor       bordercolor;
        if (displaySelection == nsISelectionController::SELECTION_DISABLED) {
          bordercolor = NS_RGB(176,176,176);// disabled color
        }
        else {
          aPresContext->LookAndFeel()->
            GetColor(nsILookAndFeel::eColor_TextSelectBackground,
                     bordercolor);
        }
        PRInt16 t2p = (PRInt16) aPresContext->PixelsToTwips();
        if ((mRect.width >(3*t2p)) && (mRect.height > (3*t2p)))
        {
          //compare bordercolor to ((nsStyleColor *)myColor)->mBackgroundColor)
          bordercolor = EnsureDifferentColors(bordercolor, aStyleColor->mBackgroundColor);
          //outerrounded
          aRenderingContext.SetColor(bordercolor);
          aRenderingContext.DrawLine(t2p, 0, mRect.width, 0);
          aRenderingContext.DrawLine(0, t2p, 0, mRect.height);
          aRenderingContext.DrawLine(t2p, mRect.height, mRect.width, mRect.height);
          aRenderingContext.DrawLine(mRect.width, t2p, mRect.width, mRect.height);
          //middle
          aRenderingContext.DrawRect(t2p, t2p, mRect.width-t2p, mRect.height-t2p);
          //shading
          aRenderingContext.DrawLine(2*t2p, mRect.height-2*t2p, mRect.width-t2p, mRect.height- (2*t2p));
          aRenderingContext.DrawLine(mRect.width - (2*t2p), 2*t2p, mRect.width - (2*t2p), mRect.height-t2p);
        }
      }
    }
  }
  return NS_OK;
}

void
nsTableCellFrame::PaintUnderlay(nsPresContext&           aPresContext,
                                nsIRenderingContext&      aRenderingContext,
                                const nsRect&             aDirtyRect,
                                PRUint32&                 aFlags,
                                const nsStyleBorder&      aStyleBorder,
                                const nsStylePadding&     aStylePadding,
                                const nsStyleTableBorder& aCellTableStyle)
{
  nsRect rect(0, 0, mRect.width, mRect.height);
  nsCSSRendering::PaintBackground(&aPresContext, aRenderingContext, this,
                                  aDirtyRect, rect, aStyleBorder, aStylePadding,
                                  PR_TRUE);
  PRIntn skipSides = GetSkipSides();
  if (NS_STYLE_TABLE_EMPTY_CELLS_SHOW == aCellTableStyle.mEmptyCells ||
      !GetContentEmpty()) {
    nsCSSRendering::PaintBorder(&aPresContext, aRenderingContext, this,
                                aDirtyRect, rect, aStyleBorder, mStyleContext, skipSides);
  }
}

NS_IMETHODIMP
nsTableCellFrame::Paint(nsPresContext*      aPresContext,
                        nsIRenderingContext& aRenderingContext,
                        const nsRect&        aDirtyRect,
                        nsFramePaintLayer    aWhichLayer,
                        PRUint32             aFlags)
{
  NS_ENSURE_TRUE(aPresContext, NS_ERROR_NULL_POINTER);
  PRBool isVisible;
  if (NS_SUCCEEDED(IsVisibleForPainting(aPresContext, aRenderingContext, PR_FALSE, &isVisible)) && !isVisible) {
    return NS_OK;
  }

  PRBool paintChildren = PR_TRUE;

  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
    const nsStyleBorder*      myBorder       = nsnull;
    const nsStylePadding*     myPadding      = nsnull;
    const nsStyleTableBorder* cellTableStyle = nsnull;
    const nsStyleVisibility* vis = GetStyleVisibility();
    if (vis->IsVisible()) {
      myBorder = GetStyleBorder();
      myPadding = GetStylePadding();
      cellTableStyle = GetStyleTableBorder();

      // draw the border & background only when there is content or showing empty cells
      if (NS_STYLE_TABLE_EMPTY_CELLS_HIDE != cellTableStyle->mEmptyCells ||
          !GetContentEmpty()) {
        PaintUnderlay(*aPresContext, aRenderingContext, aDirtyRect, aFlags,
                      *myBorder, *myPadding, *cellTableStyle);
      }

      const nsStyleBackground* myColor = GetStyleBackground();
      DecorateForSelection(aPresContext, aRenderingContext,myColor); //ignore return value
    }

    paintChildren = !(aFlags & NS_PAINT_FLAG_TABLE_CELL_BG_PASS);
    //flags were for us; remove them for our children
    aFlags &= ~ (NS_PAINT_FLAG_TABLE_CELL_BG_PASS | NS_PAINT_FLAG_TABLE_BG_PAINT);
  }

#ifdef DEBUG
  // for debug...
  if ((NS_FRAME_PAINT_LAYER_DEBUG == aWhichLayer) && GetShowFrameBorders()) {
    aRenderingContext.SetColor(NS_RGB(0, 0, 128));
    aRenderingContext.DrawRect(0, 0, mRect.width, mRect.height);
  }
#endif

  // paint the children unless we've been told not to
  if (paintChildren) {
    const nsStyleDisplay* disp = GetStyleDisplay();
    // if the cell originates in a row and/or col that is collapsed, the
    // bottom and/or right portion of the cell is painted by translating
    // the rendering context.
    nsPoint offset;
    GetCollapseOffset(aPresContext, offset);
    PRBool pushed = PR_FALSE;
    if ((0 != offset.x) || (0 != offset.y)) {
      aRenderingContext.PushState();
      pushed = PR_TRUE;
      aRenderingContext.Translate(offset.x, offset.y);
      aRenderingContext.SetClipRect(nsRect(-offset.x, -offset.y, mRect.width, mRect.height),
                                    nsClipCombine_kIntersect);
    }
    else {
      // XXXldb HIDDEN should really create a scrollframe,
      // but use |IsTableClip| here since it doesn't.
      if (disp->IsTableClip() ||
          HasPctOverHeight()) {
        aRenderingContext.PushState();
        pushed = PR_TRUE;
        SetOverflowClipRect(aRenderingContext);
      }    
    }

    PaintChildren(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer, aFlags);

    if (pushed) {
      aRenderingContext.PopState();
    }
  } 
  
  DO_GLOBAL_REFLOW_COUNT_DSP_J("nsTableCellFrame", &aRenderingContext, 0);
  return NS_OK;
  /*nsFrame::Paint(aPresContext,
                        aRenderingContext,
                        aDirtyRect,
                        aWhichLayer);*/
}

NS_IMETHODIMP
nsTableCellFrame::GetFrameForPoint(nsPresContext*   aPresContext,
                                   const nsPoint&    aPoint, 
                                   nsFramePaintLayer aWhichLayer,
                                   nsIFrame**        aFrame)
{
  // this should act like a block, so we need to override
  return GetFrameForPointUsing(aPresContext, aPoint, nsnull, aWhichLayer, (aWhichLayer == NS_FRAME_PAINT_LAYER_BACKGROUND), aFrame);
}

//null range means the whole thing
NS_IMETHODIMP
nsTableCellFrame::SetSelected(nsPresContext* aPresContext,
                              nsIDOMRange*    aRange,
                              PRBool          aSelected,
                              nsSpread        aSpread)
{
  //traverse through children unselect tables
#if 0
  if ((aSpread == eSpreadDown)){
    nsIFrame* kid = GetFirstChild(nsnull);
    while (nsnull != kid) {
      kid->SetSelected(nsnull, aSelected, eSpreadDown);
      kid = kid->GetNextSibling();
    }
  }
  //return nsFrame::SetSelected(aRange,aSelected,eSpreadNone);
#endif
  // Must call base class to set mSelected state and trigger repaint of frame
  // Note that in current version, aRange and aSpread are ignored,
  //   only this frame is considered
  nsFrame::SetSelected(aPresContext, aRange, aSelected, aSpread);

  PRBool tableCellSelectionMode;
  nsresult result = aPresContext->PresShell()->FrameSelection()->GetTableCellSelection(&tableCellSelectionMode);
  if (NS_SUCCEEDED(result) && tableCellSelectionMode) {
    // Selection can affect content, border and outline
    Invalidate(GetOverflowRect(), PR_FALSE);
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


// Align the cell's child frame within the cell

void nsTableCellFrame::VerticallyAlignChild(nsPresContext*          aPresContext,
                                            const nsHTMLReflowState& aReflowState,
                                            nscoord                  aMaxAscent)
{
  const nsStyleTextReset* textStyle = GetStyleTextReset();
  /* XXX: remove tableFrame when border-collapse inherits */
  GET_PIXELS_TO_TWIPS(aPresContext, p2t);
  nsMargin borderPadding = nsTableFrame::GetBorderPadding(aReflowState, p2t, this);
  
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
  nsIFrame* firstKid = mFrames.FirstChild();
  nsRect kidRect = firstKid->GetRect();
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
      kidYTop = nsTableFrame::RoundToPixel(kidYTop,
                                           aPresContext->ScaledPixelsToTwips(),
                                           eAlwaysRoundDown);
  }
  // if the content is larger than the cell height align from top
  kidYTop = PR_MAX(0, kidYTop);

  firstKid->SetPosition(nsPoint(kidRect.x, kidYTop));
  nsHTMLReflowMetrics desiredSize(PR_FALSE);
  desiredSize.width = mRect.width;
  desiredSize.height = mRect.height;
  desiredSize.mOverflowArea = nsRect(0, 0, mRect.width, mRect.height);
  ConsiderChildOverflow(aPresContext, desiredSize.mOverflowArea, firstKid);
  FinishAndStoreOverflow(&desiredSize);
  if (kidYTop != kidRect.y) {
    // Make sure any child views are correctly positioned. We know the inner table
    // cell won't have a view
    nsContainerFrame::PositionChildViews(aPresContext, firstKid);
  }
  if (HasView()) {
    nsContainerFrame::SyncFrameViewAfterReflow(aPresContext, this, GetView(), &desiredSize.mOverflowArea, 0);
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
  const nsStyleTextReset* textStyle = GetStyleTextReset();
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
  nsCOMPtr<nsIHTMLContent> hc(do_QueryInterface(mContent));

  if (hc) {
    nsHTMLValue val;
    hc->GetHTMLAttribute(nsHTMLAtoms::rowspan, val); 
    if (eHTMLUnit_Integer == val.GetUnit()) { 
       rowSpan=val.GetIntValue(); 
    }
  }
  return rowSpan;
}

PRInt32 nsTableCellFrame::GetColSpan()
{  
  PRInt32 colSpan=1;
  nsCOMPtr<nsIHTMLContent> hc(do_QueryInterface(mContent));

  if (hc) {
    nsHTMLValue val;
    hc->GetHTMLAttribute(nsHTMLAtoms::colspan, val); 
    if (eHTMLUnit_Integer == val.GetUnit()) { 
       colSpan=val.GetIntValue(); 
    }
  }
  return colSpan;
}

#ifdef DEBUG
#define PROBABLY_TOO_LARGE 1000000
static
void DebugCheckChildSize(nsIFrame*            aChild, 
                         nsHTMLReflowMetrics& aMet, 
                         nsSize&              aAvailSize,
                         PRBool               aIsPass2Reflow)
{
  if (aIsPass2Reflow) {
    if ((aMet.width < 0) || (aMet.width > PROBABLY_TOO_LARGE)) {
      printf("WARNING: cell content %p has large width %d \n", aChild, aMet.width);
    }
  }
  if (aMet.mComputeMEW) {
    nscoord tmp = aMet.mMaxElementWidth;
    if ((tmp < 0) || (tmp > PROBABLY_TOO_LARGE)) {
      printf("WARNING: cell content %p has large max element width %d \n", aChild, tmp);
    }
  }
}
#endif

// the computed height for the cell, which descendents use for percent height calculations
// it is the height (minus border, padding) of the cell's first in flow during its final 
// reflow without an unconstrained height.
static nscoord
CalcUnpaginagedHeight(nsPresContext*       aPresContext,
                      nsTableCellFrame&     aCellFrame, 
                      nsTableFrame&         aTableFrame,
                      nscoord               aVerticalBorderPadding)
{
  const nsTableCellFrame* firstCellInFlow   = (nsTableCellFrame*)aCellFrame.GetFirstInFlow();
  nsTableFrame*           firstTableInFlow  = (nsTableFrame*)aTableFrame.GetFirstInFlow();
  nsTableRowFrame*        row
    = NS_STATIC_CAST(nsTableRowFrame*, firstCellInFlow->GetParent());
  nsTableRowGroupFrame*   firstRGInFlow
    = NS_STATIC_CAST(nsTableRowGroupFrame*, row->GetParent());

  PRInt32 rowIndex;
  firstCellInFlow->GetRowIndex(rowIndex);
  PRInt32 rowSpan = aTableFrame.GetEffectiveRowSpan(*firstCellInFlow);
  nscoord cellSpacing = firstTableInFlow->GetCellSpacingX();

  nscoord computedHeight = ((rowSpan - 1) * cellSpacing) - aVerticalBorderPadding;
  PRInt32 rowX;
  for (row = firstRGInFlow->GetFirstRow(), rowX = 0; row; row = row->GetNextRow(), rowX++) {
    if (rowX > rowIndex + rowSpan - 1) {
      break;
    }
    else if (rowX >= rowIndex) {
      computedHeight += row->GetUnpaginatedHeight(aPresContext);
    }
  }
  return computedHeight;
}

NS_METHOD nsTableCellFrame::Reflow(nsPresContext*          aPresContext,
                                   nsHTMLReflowMetrics&     aDesiredSize,
                                   const nsHTMLReflowState& aReflowState,
                                   nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsTableCellFrame", aReflowState.reason);
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);
#if defined DEBUG_TABLE_REFLOW_TIMING
  nsTableFrame::DebugReflow(this, (nsHTMLReflowState&)aReflowState);
#endif
  float p2t = aPresContext->ScaledPixelsToTwips();

  // work around pixel rounding errors, round down to ensure we don't exceed the avail height in
  nscoord availHeight = aReflowState.availableHeight;
  if (NS_UNCONSTRAINEDSIZE != availHeight) {
    availHeight = nsTableFrame::RoundToPixel(availHeight, p2t, eAlwaysRoundDown);
  }

  nsresult rv = NS_OK;

  // see if a special height reflow needs to occur due to having a pct height
  if (!NeedSpecialReflow()) 
    nsTableFrame::CheckRequestSpecialHeightReflow(aReflowState);

  // this should probably be cached somewhere
  nsCompatibility compatMode = aPresContext->CompatibilityMode();

  // Initialize out parameter
  if (aDesiredSize.mComputeMEW) {
    aDesiredSize.mMaxElementWidth = 0;
  }

  aStatus = NS_FRAME_COMPLETE;
  nsSize availSize(aReflowState.availableWidth, availHeight);

  PRBool contentEmptyBeforeReflow = GetContentEmpty();
  /* XXX: remove tableFrame when border-collapse inherits */
  nsTableFrame* tableFrame = nsnull;
  rv = nsTableFrame::GetTableFrame(this, tableFrame); if (!tableFrame) ABORT1(NS_ERROR_NULL_POINTER);

  nsMargin borderPadding = aReflowState.mComputedPadding;
  nsMargin border;
  GetBorderWidth(p2t, border);
  if ((NS_UNCONSTRAINEDSIZE == availSize.width) || !contentEmptyBeforeReflow) {
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
  if (eReflowReason_Incremental == aReflowState.reason) {
    // if the path has a reflow command then the cell must be the target of a style change 
    nsHTMLReflowCommand* command = aReflowState.path->mReflowCommand;
    if (command) {
      // if there are other reflow commands targeted at the cell's block, these will
      // be subsumed by the style change reflow
      nsReflowType type;
      command->GetType(type);
      if (eReflowType_StyleChanged == type) {
        isStyleChanged = PR_TRUE;
      }
      else NS_ASSERTION(PR_FALSE, "table cell target of illegal incremental reflow type");
    }
    // else the reflow command will be passed down to the child
  }

  // Try to reflow the child into the available space. It might not
  // fit or might need continuing.
  if (availSize.height < 0)
    availSize.height = 1;

  nsHTMLReflowMetrics kidSize(NS_UNCONSTRAINEDSIZE == aReflowState.availableWidth ||
                              aDesiredSize.mComputeMEW,
                              aDesiredSize.mFlags);
  kidSize.width=kidSize.height=kidSize.ascent=kidSize.descent=0;
  SetPriorAvailWidth(aReflowState.availableWidth);
  nsIFrame* firstKid = mFrames.FirstChild();

  nscoord computedPaginatedHeight = 0;

  if (aReflowState.mFlags.mSpecialHeightReflow || 
      (HadSpecialReflow() && (eReflowReason_Incremental == aReflowState.reason))) {
    ((nsHTMLReflowState&)aReflowState).mComputedHeight = mRect.height - topInset - bottomInset;
    DISPLAY_REFLOW_CHANGE();
  }
  else if (aPresContext->IsPaginated()) {
    computedPaginatedHeight = CalcUnpaginagedHeight(aPresContext, (nsTableCellFrame&)*this, *tableFrame, topInset + bottomInset);
    if (computedPaginatedHeight > 0) {
      ((nsHTMLReflowState&)aReflowState).mComputedHeight = computedPaginatedHeight;
      DISPLAY_REFLOW_CHANGE();
    }
  }      
  else {
    SetHasPctOverHeight(PR_FALSE);
  }

  // If it was a style change targeted at us, then reflow the child with a style change reason
  nsReflowReason reason = aReflowState.reason;
  if (isStyleChanged) {
    reason = eReflowReason_StyleChange;
    // the following could be optimized with a fair amount of effort
    tableFrame->SetNeedStrategyInit(PR_TRUE);
  }

  nsHTMLReflowState kidReflowState(aPresContext, aReflowState, firstKid, availSize, reason);
  // mIPercentHeightObserver is for non table related frames inside cells
  kidReflowState.mPercentHeightObserver = (nsIPercentHeightObserver *)this;

  // Assume the inner child will stay positioned exactly where it is. Later in
  // VerticallyAlignChild() we'll move it if it turns out to be wrong. This
  // avoids excessive movement and is more stable
  nsPoint kidOrigin;
  if (isStyleChanged || 
      (eReflowReason_Initial == aReflowState.reason) ||
      (eReflowReason_StyleChange == aReflowState.reason)) {
    kidOrigin.MoveTo(leftInset, topInset);
  } else {
    // handle percent padding-left which was 0 during initial reflow
    if (eStyleUnit_Percent == aReflowState.mStylePadding->mPadding.GetLeftUnit()) {
      nsRect kidRect = firstKid->GetRect();
      // only move in the x direction for the same reason as above
      kidOrigin.MoveTo(leftInset, kidRect.y);
      firstKid->SetPosition(nsPoint(leftInset, kidRect.y));
    }
    kidOrigin = firstKid->GetPosition();
  }

#if defined DEBUG_TABLE_REFLOW_TIMING
  nsTableFrame::DebugReflow(firstKid, (nsHTMLReflowState&)kidReflowState);
#endif
  nscoord priorBlockHeight = GetLastBlockHeight();
  ReflowChild(firstKid, aPresContext, kidSize, kidReflowState,
              kidOrigin.x, kidOrigin.y, 0, aStatus);
  SetLastBlockHeight(kidSize.height);
  if (isStyleChanged) {
    Invalidate(GetOverflowRect(), PR_FALSE);
  }

#if defined DEBUG_TABLE_REFLOW_TIMING
  nsTableFrame::DebugReflow(firstKid, (nsHTMLReflowState&)kidReflowState, &kidSize, aStatus);
#endif

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
    if ((eReflowReason_Incremental == aReflowState.reason) && contentEmptyBeforeReflow) {
      // need to consider borders, since they were factored out above
      leftInset   += border.left;
      rightInset  += border.right;
      topInset    += border.top;
      bottomInset += border.bottom;
      kidOrigin.MoveTo(leftInset, topInset);
    }
  }

  const nsStylePosition* pos = GetStylePosition();

  // calculate the min cell width
  nscoord onePixel = NSIntPixelsToTwips(1, p2t);
  nscoord smallestMinWidth = 0;
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
      PRInt32 pixHeight = (eCompatibility_NavQuirks == compatMode) ? 1 : 0;
      kidSize.height = NSIntPixelsToTwips(pixHeight, p2t);
    }
  }
  // end 0 dimensioned cells

  kidSize.width = PR_MAX(kidSize.width, smallestMinWidth); 
  if (!tableFrame->IsAutoLayout()) {
    // a cell in a fixed layout table is constrained to the avail width
    kidSize.width = PR_MIN(kidSize.width, availSize.width);
  }
  //if (eReflowReason_Resize == aReflowState.reason) {
  //  NS_ASSERTION(kidSize.width <= availSize.width, "child needed more space during resize reflow");
  //}
  // Place the child
  FinishReflowChild(firstKid, aPresContext, &kidReflowState, kidSize,
                    kidOrigin.x, kidOrigin.y, 0);
    
  // first, compute the height which can be set w/o being restricted by aMaxSize.height
  nscoord cellHeight = kidSize.height;

  if (NS_UNCONSTRAINEDSIZE != cellHeight) {
    cellHeight += topInset + bottomInset;
    // work around block rounding errors, round down to ensure we don't exceed the avail height in
    nsPixelRound roundMethod = (NS_UNCONSTRAINEDSIZE == availHeight) ? eAlwaysRoundUp : eAlwaysRoundDown;
    cellHeight = nsTableFrame::RoundToPixel(cellHeight, p2t, roundMethod); 
  }

  // next determine the cell's width
  nscoord cellWidth = kidSize.width;      // at this point, we've factored in the cell's style attributes

  // factor in border and padding
  if (NS_UNCONSTRAINEDSIZE != cellWidth) {
    cellWidth += leftInset + rightInset;    
  }
  cellWidth = nsTableFrame::RoundToPixel(cellWidth, p2t); // work around block rounding errors

  // set the cell's desired size and max element size
  aDesiredSize.width   = cellWidth;
  aDesiredSize.height  = cellHeight;
  aDesiredSize.ascent  = topInset;
  aDesiredSize.descent = bottomInset;

  aDesiredSize.ascent  += kidSize.ascent;
  aDesiredSize.descent += kidSize.descent;
  
  // the overflow area will be computed when the child will be vertically aligned

  if (aDesiredSize.mComputeMEW) {
    aDesiredSize.mMaxElementWidth =
      PR_MAX(smallestMinWidth, kidSize.mMaxElementWidth);
    if (NS_UNCONSTRAINEDSIZE != aDesiredSize.mMaxElementWidth) {
      aDesiredSize.mMaxElementWidth = nsTableFrame::RoundToPixel(
                  aDesiredSize.mMaxElementWidth + leftInset + rightInset, p2t);
    }
  }
  if (aDesiredSize.mFlags & NS_REFLOW_CALC_MAX_WIDTH) {
    aDesiredSize.mMaximumWidth = kidSize.mMaximumWidth;
    if (NS_UNCONSTRAINEDSIZE != aDesiredSize.mMaximumWidth) {
      aDesiredSize.mMaximumWidth += leftInset + rightInset;
      aDesiredSize.mMaximumWidth = nsTableFrame::RoundToPixel(aDesiredSize.mMaximumWidth, p2t);
    }
    // make sure the preferred width is at least as big as the max element width
    if (aDesiredSize.mComputeMEW) {
      aDesiredSize.mMaximumWidth = PR_MAX(aDesiredSize.mMaximumWidth, aDesiredSize.mMaxElementWidth);
    }
  }

  if (aReflowState.mFlags.mSpecialHeightReflow) {
    if (aDesiredSize.height > mRect.height) {
      // set a bit indicating that the pct height contents exceeded 
      // the height that they could honor in the pass 2 reflow
      SetHasPctOverHeight(PR_TRUE);
    }
    if (NS_UNCONSTRAINEDSIZE == aReflowState.availableHeight) {
      aDesiredSize.height = mRect.height;
    }
    SetNeedSpecialReflow(PR_FALSE);
    SetHadSpecialReflow(PR_TRUE);
  }
  else if (HadSpecialReflow()) {
    if (eReflowReason_Incremental == aReflowState.reason) {
      // with an unconstrained height, if the block height value hasn't changed, 
      // use the last height of the cell.
      if ((NS_UNCONSTRAINEDSIZE == aReflowState.availableHeight) && 
          (GetLastBlockHeight() == priorBlockHeight)) {
        aDesiredSize.height = mRect.height;
      }
    }
    // XXX should probably call SetHadSpecialReflow(PR_FALSE) when things change so that
    // nothing inside the cell has a percent height, but it is not easy determining this 
  }

  // remember the desired size for this reflow
  SetDesiredSize(aDesiredSize);

#if defined DEBUG_TABLE_REFLOW_TIMING
  nsTableFrame::DebugReflow(this, (nsHTMLReflowState&)aReflowState, &aDesiredSize, aStatus);
#endif

  if (NS_UNCONSTRAINEDSIZE == aReflowState.availableWidth) {
    SetNeedPass2Reflow(PR_TRUE);
  }
  else if ((eReflowReason_Initial == aReflowState.reason) || 
           (eReflowReason_Resize  == aReflowState.reason)) { 
    SetNeedPass2Reflow(PR_FALSE);
  }

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
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
  }
  if (aIID.Equals(NS_GET_IID(nsIPercentHeightObserver))) {
    *aInstancePtr = (void*) (nsIPercentHeightObserver *)this;
    return NS_OK;
  }

  return nsHTMLContainerFrame::QueryInterface(aIID, aInstancePtr);
}

#ifdef ACCESSIBILITY
NS_IMETHODIMP nsTableCellFrame::GetAccessible(nsIAccessible** aAccessible)
{
  nsCOMPtr<nsIAccessibilityService> accService = do_GetService("@mozilla.org/accessibilityService;1");

  if (accService) {
    return accService->CreateHTMLTableCellAccessible(NS_STATIC_CAST(nsIFrame*, this), aAccessible);
  }

  return NS_ERROR_FAILURE;
}
#endif

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
  aColIndex = mBits.mColIndex;
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
    if (!cellFrame) return NS_ERROR_FAILURE;
    return CallQueryInterface(cellFrame, aCellLayout);
  }
  return NS_ERROR_FAILURE;
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
  if (!cellFrame) return NS_ERROR_FAILURE;
  return CallQueryInterface(cellFrame, aCellLayout);
}

nsresult 
NS_NewTableCellFrame(nsIPresShell* aPresShell, 
                     PRBool        aIsBorderCollapse,
                     nsIFrame**    aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsTableCellFrame* it = (aIsBorderCollapse) ? new (aPresShell) nsBCTableCellFrame 
                                             : new (aPresShell) nsTableCellFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsMargin* 
nsTableCellFrame::GetBorderWidth(float      aPixelsToTwips,
                                 nsMargin&  aBorder) const
{
  aBorder.left = aBorder.right = aBorder.top = aBorder.bottom = 0;

  const nsStyleBorder* borderData = GetStyleBorder();
  borderData->GetBorder(aBorder);
  return &aBorder;
}

nsIAtom*
nsTableCellFrame::GetType() const
{
  return nsLayoutAtoms::tableCellFrame;
}

#ifdef DEBUG
NS_IMETHODIMP
nsTableCellFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("TableCell"), aResult);
}
#endif

void nsTableCellFrame::SetCollapseOffsetX(nsPresContext* aPresContext,
                                          nscoord         aXOffset)
{
  // Get the frame property (creating a point struct if necessary)
  nsPoint* offset = (nsPoint*)nsTableFrame::GetProperty(aPresContext, this, nsLayoutAtoms::collapseOffsetProperty, aXOffset != 0);

  if (offset) {
    offset->x = aXOffset;
  }
}

void nsTableCellFrame::SetCollapseOffsetY(nsPresContext* aPresContext,
                                          nscoord         aYOffset)
{
  // Get the property (creating a point struct if necessary)
  nsPoint* offset = (nsPoint*)nsTableFrame::GetProperty(aPresContext, this, nsLayoutAtoms::collapseOffsetProperty, aYOffset != 0);

  if (offset) {
    offset->y = aYOffset;
  }
}

void nsTableCellFrame::GetCollapseOffset(nsPresContext* aPresContext,
                                         nsPoint&        aOffset)
{
  // See if the property is set
  nsPoint* offset = (nsPoint*)nsTableFrame::GetProperty(aPresContext, this, nsLayoutAtoms::collapseOffsetProperty);

  if (offset) {
    aOffset = *offset;
  } else {
    aOffset.MoveTo(0, 0);
  }
}

// nsBCTableCellFrame

nsBCTableCellFrame::nsBCTableCellFrame()
:nsTableCellFrame()
{
  mTopBorder = mRightBorder = mBottomBorder = mLeftBorder = 0;
}

nsBCTableCellFrame::~nsBCTableCellFrame()
{
}

nsIAtom*
nsBCTableCellFrame::GetType() const
{
  return nsLayoutAtoms::bcTableCellFrame;
}

#ifdef DEBUG
NS_IMETHODIMP
nsBCTableCellFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("BCTableCell"), aResult);
}
#endif

void 
nsBCTableCellFrame::SetBorderWidth(const nsMargin& aBorder)
{
  mTopBorder    = aBorder.top;
  mRightBorder  = aBorder.right;
  mBottomBorder = aBorder.bottom;
  mLeftBorder   = aBorder.left;
}

nsMargin* 
nsBCTableCellFrame::GetBorderWidth(float      aPixelsToTwips,
                                   nsMargin&  aBorder) const
{
  aBorder.top    = (aPixelsToTwips) ? NSToCoordRound(aPixelsToTwips * mTopBorder) :   mTopBorder;
  aBorder.right  = (aPixelsToTwips) ? NSToCoordRound(aPixelsToTwips * mRightBorder) : mRightBorder;
  aBorder.bottom = (aPixelsToTwips) ? NSToCoordRound(aPixelsToTwips * mBottomBorder): mBottomBorder;
  aBorder.left   = (aPixelsToTwips) ? NSToCoordRound(aPixelsToTwips * mLeftBorder):   mLeftBorder;
  return &aBorder;
}

nscoord 
nsBCTableCellFrame::GetBorderWidth(PRUint8 aSide) const
{
  switch(aSide) {
  case NS_SIDE_TOP:
    return (PRUint8)mTopBorder;
  case NS_SIDE_RIGHT:
    return (PRUint8)mRightBorder;
  case NS_SIDE_BOTTOM:
    return (PRUint8)mBottomBorder;
  default:
    return (PRUint8)mLeftBorder;
  }
}

void 
nsBCTableCellFrame::SetBorderWidth(PRUint8 aSide,
                                   nscoord aValue)
{
  switch(aSide) {
  case NS_SIDE_TOP:
    mTopBorder = aValue;
    break;
  case NS_SIDE_RIGHT:
    mRightBorder = aValue;
    break;
  case NS_SIDE_BOTTOM:
    mBottomBorder = aValue;
    break;
  default:
    mLeftBorder = aValue;
  }
}

void
nsBCTableCellFrame::PaintUnderlay(nsPresContext&           aPresContext,
                                  nsIRenderingContext&      aRenderingContext,
                                  const nsRect&             aDirtyRect,
                                  PRUint32&                 aFlags,
                                  const nsStyleBorder&      aStyleBorder,
                                  const nsStylePadding&     aStylePadding,
                                  const nsStyleTableBorder& aCellTableStyle)
{
  if (!(aFlags & NS_PAINT_FLAG_TABLE_BG_PAINT)
      /*direct call; not table-based paint*/ ||
      (aFlags & NS_PAINT_FLAG_TABLE_CELL_BG_PASS)
      /*table cell background only pass*/) {
    // make border-width reflect border-collapse assigned border
    GET_PIXELS_TO_TWIPS(&aPresContext, p2t);
    nsMargin borderWidth;
    GetBorderWidth(p2t, borderWidth);

    nsStyleBorder myBorder = aStyleBorder;

    nsStyleCoord coord(borderWidth.top);
    myBorder.mBorder.SetTop(coord);
    coord.SetCoordValue(borderWidth.right);
    myBorder.mBorder.SetRight(coord);
    coord.SetCoordValue(borderWidth.bottom);
    myBorder.mBorder.SetBottom(coord);
    coord.SetCoordValue(borderWidth.left);
    myBorder.mBorder.SetLeft(coord);
    myBorder.RecalcData();

    nsRect rect(0, 0, mRect.width, mRect.height);
    nsCSSRendering::PaintBackground(&aPresContext, aRenderingContext, this,
                                    aDirtyRect, rect, myBorder, aStylePadding,
                                    PR_TRUE);
    // borders are painted by nsTableFrame
  }
}
