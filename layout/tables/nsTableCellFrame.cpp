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
#include "nsStyleContext.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsIRenderingContext.h"
#include "nsCSSRendering.h"
#include "nsIContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLParts.h"
#include "nsGkAtoms.h"
#include "nsIView.h"
#include "nsIPresShell.h"
#include "nsCOMPtr.h"
#include "nsIDOMHTMLTableCellElement.h"
#ifdef ACCESSIBILITY
#include "nsIAccessibilityService.h"
#endif
#include "nsIServiceManager.h"
#include "nsIDOMNode.h"
#include "nsINameSpaceManager.h"
#include "nsDisplayList.h"
#include "nsLayoutUtils.h"

//TABLECELL SELECTION
#include "nsFrameSelection.h"
#include "nsILookAndFeel.h"


nsTableCellFrame::nsTableCellFrame(nsStyleContext* aContext) :
  nsHTMLContainerFrame(aContext)
{
  mColIndex = 0;
  mPriorAvailWidth = 0;

  SetContentEmpty(PR_FALSE);
  SetHasPctOverHeight(PR_FALSE);
}

nsTableCellFrame::~nsTableCellFrame()
{
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
nsTableCellFrame::Init(nsIContent*      aContent,
                       nsIFrame*        aParent,
                       nsIFrame*        aPrevInFlow)
{
  // Let the base class do its initialization
  nsresult rv = nsHTMLContainerFrame::Init(aContent, aParent, aPrevInFlow);

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
  // nsHTMLReflowState ensures the mCBReflowState of blocks inside a
  // cell is the cell frame, not the inner-cell block, and that the
  // containing block of an inner table is the containing block of its
  // outer table.
  // XXXldb Given the now-stricter |NeedsToObserve|, many if not all of
  // these tests are probably unnecessary.

  // Maybe the cell reflow state; we sure if we're inside the |if|.
  const nsHTMLReflowState *cellRS = aReflowState.mCBReflowState;

  if (cellRS && cellRS->frame == this &&
      (cellRS->mComputedHeight == NS_UNCONSTRAINEDSIZE ||
       cellRS->mComputedHeight == 0)) { // XXXldb Why 0?
    // This is a percentage height on a frame whose percentage heights
    // are based on the height of the cell, since its containing block
    // is the inner cell frame.

    for (const nsHTMLReflowState *rs = aReflowState.parentReflowState;
         rs != cellRS;
         rs = rs->parentReflowState) {
      rs->frame->AddStateBits(NS_FRAME_CONTAINS_RELATIVE_HEIGHT);
    }

    nsTableFrame::RequestSpecialHeightReflow(*cellRS);
  }
}

// The cell needs to observe its block and things inside its block but nothing below that
PRBool 
nsTableCellFrame::NeedsToObserve(const nsHTMLReflowState& aReflowState)
{
  const nsHTMLReflowState *rs = aReflowState.parentReflowState;
  if (!rs)
    return PR_FALSE;
  if (rs->frame == this) {
    // We always observe the child block.  It will never send any
    // notifications, but we need this so that the observer gets
    // propagated to its kids.
    return PR_TRUE;
  }
  rs = rs->parentReflowState;
  if (!rs) {
    return PR_FALSE;
  }

  // We always need to let the percent height observer be propagated
  // from an outer table frame to an inner table frame.
  nsIAtom *fType = aReflowState.frame->GetType();
  if (fType == nsGkAtoms::tableFrame) {
    return PR_TRUE;
  }

  // We need the observer to be propagated to all children of the cell
  // (i.e., children of the child block) in quirks mode, but only to
  // tables in standards mode.
  return rs->frame == this &&
         (GetPresContext()->CompatibilityMode() == eCompatibility_NavQuirks ||
          fType == nsGkAtoms::tableOuterFrame);
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
  if (GetPrevInFlow()) {
    return ((nsTableCellFrame*)GetFirstInFlow())->GetColIndex(aColIndex);
  }
  else {
    aColIndex = mColIndex;
    return  NS_OK;
  }
}

NS_IMETHODIMP
nsTableCellFrame::AttributeChanged(PRInt32         aNameSpaceID,
                                   nsIAtom*        aAttribute,
                                   PRInt32         aModType)
{
  // let the table frame decide what to do
  nsTableFrame* tableFrame = nsTableFrame::GetTableFrame(this);
  if (tableFrame) {
    tableFrame->AttributeChangedFor(this, mContent, aAttribute); 
  }
  return NS_OK;
}

NS_IMETHODIMP
nsTableCellFrame::AppendFrames(nsIAtom*        aListName,
                               nsIFrame*       aFrameList)
{
  NS_PRECONDITION(PR_FALSE, "unsupported operation");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsTableCellFrame::InsertFrames(nsIAtom*        aListName,
                               nsIFrame*       aPrevFrame,
                               nsIFrame*       aFrameList)
{
  NS_PRECONDITION(PR_FALSE, "unsupported operation");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsTableCellFrame::RemoveFrame(nsIAtom*        aListName,
                              nsIFrame*       aOldFrame)
{
  NS_PRECONDITION(PR_FALSE, "unsupported operation");
  return NS_ERROR_NOT_IMPLEMENTED;
}

void nsTableCellFrame::SetColIndex(PRInt32 aColIndex)
{  
  mColIndex = aColIndex;
}

/* virtual */ nsMargin
nsTableCellFrame::GetUsedMargin() const
{
  return nsMargin(0,0,0,0);
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

void
nsTableCellFrame::DecorateForSelection(nsIRenderingContext& aRenderingContext,
                                       nsPoint aPt)
{
  NS_ASSERTION(GetStateBits() & NS_FRAME_SELECTED_CONTENT,
               "Should only be called for selected cells");
  PRInt16 displaySelection;
  nsPresContext* presContext = GetPresContext();
  displaySelection = DisplaySelection(presContext);
  if (displaySelection) {
    nsFrameSelection *frameSelection = presContext->PresShell()->FrameSelection();

    if (frameSelection->GetTableCellSelection()) {
      nscolor       bordercolor;
      if (displaySelection == nsISelectionController::SELECTION_DISABLED) {
        bordercolor = NS_RGB(176,176,176);// disabled color
      }
      else {
        presContext->LookAndFeel()->
          GetColor(nsILookAndFeel::eColor_TextSelectBackground,
                   bordercolor);
      }
      GET_PIXELS_TO_TWIPS(presContext, p2t);
      if ((mRect.width >(3*p2t)) && (mRect.height > (3*p2t)))
      {
        //compare bordercolor to ((nsStyleColor *)myColor)->mBackgroundColor)
        bordercolor = EnsureDifferentColors(bordercolor,
                                            GetStyleBackground()->mBackgroundColor);
        nsIRenderingContext::AutoPushTranslation
            translate(&aRenderingContext, aPt.x, aPt.y);
        nscoord onePixel = NSToCoordRound(p2t);

        aRenderingContext.SetColor(bordercolor);
        aRenderingContext.DrawLine(onePixel, 0, mRect.width, 0);
        aRenderingContext.DrawLine(0, onePixel, 0, mRect.height);
        aRenderingContext.DrawLine(onePixel, mRect.height, mRect.width, mRect.height);
        aRenderingContext.DrawLine(mRect.width, onePixel, mRect.width, mRect.height);
        //middle
        aRenderingContext.DrawRect(onePixel, onePixel, mRect.width-onePixel,
                                   mRect.height-onePixel);
        //shading
        aRenderingContext.DrawLine(2*onePixel, mRect.height-2*onePixel,
                                   mRect.width-onePixel, mRect.height- (2*onePixel));
        aRenderingContext.DrawLine(mRect.width - (2*onePixel), 2*onePixel,
                                   mRect.width - (2*onePixel), mRect.height-onePixel);
      }
    }
  }
}

void
nsTableCellFrame::PaintBackground(nsIRenderingContext& aRenderingContext,
                                  const nsRect&        aDirtyRect,
                                  nsPoint              aPt)
{
  nsRect rect(aPt, GetSize());
  nsCSSRendering::PaintBackground(GetPresContext(), aRenderingContext, this,
                                  aDirtyRect, rect, *GetStyleBorder(),
                                  *GetStylePadding(), PR_TRUE);
}

// Called by nsTablePainter
void
nsTableCellFrame::PaintCellBackground(nsIRenderingContext& aRenderingContext,
                                      const nsRect& aDirtyRect, nsPoint aPt)
{
  if (!GetStyleVisibility()->IsVisible())
    return;
  if (GetContentEmpty() &&
      NS_STYLE_TABLE_EMPTY_CELLS_HIDE == GetStyleTableBorder()->mEmptyCells)
    return;

  PaintBackground(aRenderingContext, aDirtyRect, aPt);
}

class nsDisplayTableCellBackground : public nsDisplayItem {
public:
  nsDisplayTableCellBackground(nsTableCellFrame* aFrame) : nsDisplayItem(aFrame) {
    MOZ_COUNT_CTOR(nsDisplayTableCellBackground);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayTableCellBackground() {
    MOZ_COUNT_DTOR(nsDisplayTableCellBackground);
  }
#endif

  virtual nsIFrame* HitTest(nsDisplayListBuilder* aBuilder, nsPoint aPt) { return mFrame; }
  virtual void Paint(nsDisplayListBuilder* aBuilder, nsIRenderingContext* aCtx,
     const nsRect& aDirtyRect);
  NS_DISPLAY_DECL_NAME("TableCellBackground")
};

void nsDisplayTableCellBackground::Paint(nsDisplayListBuilder* aBuilder,
     nsIRenderingContext* aCtx, const nsRect& aDirtyRect)
{
  NS_STATIC_CAST(nsTableCellFrame*, mFrame)->
    PaintBackground(*aCtx, aDirtyRect, aBuilder->ToReferenceFrame(mFrame));
}

static void
PaintTableCellSelection(nsIFrame* aFrame, nsIRenderingContext* aCtx,
                        const nsRect& aRect, nsPoint aPt)
{
  NS_STATIC_CAST(nsTableCellFrame*, aFrame)->DecorateForSelection(*aCtx, aPt);
}

NS_IMETHODIMP
nsTableCellFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                   const nsRect&           aDirtyRect,
                                   const nsDisplayListSet& aLists)
{
  if (!IsVisibleInSelection(aBuilder))
    return NS_OK;

  DO_GLOBAL_REFLOW_COUNT_DSP("nsTableCellFrame");

  PRInt32 emptyCellStyle = GetContentEmpty() ? GetStyleTableBorder()->mEmptyCells
      : NS_STYLE_TABLE_EMPTY_CELLS_SHOW;
  // take account of 'empty-cells'
  if (GetStyleVisibility()->IsVisible() &&
      (NS_STYLE_TABLE_EMPTY_CELLS_HIDE != emptyCellStyle)) {
    nsTableFrame* tableFrame = nsTableFrame::GetTableFrame(this);

    // display background if we need to.
    if (aBuilder->IsForEventDelivery() ||
        (((!tableFrame->IsBorderCollapse() || aBuilder->IsAtRootOfPseudoStackingContext()) &&
        (!GetStyleBackground()->IsTransparent() || GetStyleDisplay()->mAppearance)))) {
      // The cell background was not painted by the nsTablePainter,
      // so we need to do it. We have special background processing here
      // so we need to duplicate some code from nsFrame::DisplayBorderBackgroundOutline
      nsresult rv = aLists.BorderBackground()->AppendNewToTop(new (aBuilder)
             nsDisplayTableCellBackground(this));
      NS_ENSURE_SUCCESS(rv, rv);
    }
    
    // display borders if we need to
    if (!tableFrame->IsBorderCollapse() && HasBorder() &&
        emptyCellStyle == NS_STYLE_TABLE_EMPTY_CELLS_SHOW) {
      nsresult rv = aLists.BorderBackground()->AppendNewToTop(new (aBuilder)
          nsDisplayBorder(this));
      NS_ENSURE_SUCCESS(rv, rv);
    }

    // and display the selection border if we need to
    PRBool isSelected =
      (GetStateBits() & NS_FRAME_SELECTED_CONTENT) == NS_FRAME_SELECTED_CONTENT;
    if (isSelected) {
      nsresult rv = aLists.BorderBackground()->AppendNewToTop(new (aBuilder)
          nsDisplayGeneric(this, ::PaintTableCellSelection, "TableCellSelection"));
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // the 'empty-cells' property has no effect on 'outline'
  nsresult rv = DisplayOutline(aBuilder, aLists);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool quirkyClip = HasPctOverHeight() &&
    eCompatibility_NavQuirks == GetPresContext()->CompatibilityMode();
  nsIFrame* kid = mFrames.FirstChild();
  NS_ASSERTION(kid && !kid->GetNextSibling(), "Table cells should have just one child");
  if (!quirkyClip) {
    // The child's background will go in our BorderBackground() list.
    // This isn't a problem since it won't have a real background except for
    // event handling. We do not call BuildDisplayListForNonBlockChildren
    // because that/ would put the child's background in the Content() list
    // which isn't right (e.g., would end up on top of our child floats for
    // event handling).
    return BuildDisplayListForChild(aBuilder, kid, aDirtyRect, aLists);
  }
    
  // Unfortunately there is some wacky clipping to do
  nsDisplayListCollection set;
  rv = BuildDisplayListForChild(aBuilder, kid, aDirtyRect, set);
  NS_ENSURE_SUCCESS(rv, rv);

  nsRect clip = GetOverflowRect();
  if (quirkyClip) {
    clip = nsRect(nsPoint(0, 0), GetSize());
  }
  return OverflowClip(aBuilder, set, aLists, clip + aBuilder->ToReferenceFrame(this));
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

  if (aPresContext->PresShell()->FrameSelection()->GetTableCellSelection()) {
    // Selection can affect content, border and outline
    Invalidate(GetOverflowRect(), PR_FALSE);
  }
  return NS_OK;
}

PRIntn
nsTableCellFrame::GetSkipSides() const
{
  PRIntn skip = 0;
  if (nsnull != GetPrevInFlow()) {
    skip |= 1 << NS_SIDE_TOP;
  }
  if (nsnull != GetNextInFlow()) {
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

/* virtual */ void
nsTableCellFrame::GetSelfOverflow(nsRect& aOverflowArea)
{
  aOverflowArea = nsRect(nsPoint(0,0), GetSize());
}

// Align the cell's child frame within the cell

void nsTableCellFrame::VerticallyAlignChild(nscoord aMaxAscent)
{
  const nsStyleTextReset* textStyle = GetStyleTextReset();
  /* It's the 'border-collapse' on the table that matters */
  nsPresContext* presContext = GetPresContext();
  GET_PIXELS_TO_TWIPS(presContext, p2t);
  nsMargin borderPadding = GetUsedBorderAndPadding();
  
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
  NS_ASSERTION(firstKid, "Frame construction error, a table cell always has an inner cell frame");
  nsRect kidRect = firstKid->GetRect();
  nscoord childHeight = kidRect.height;

  // Vertically align the child
  nscoord kidYTop = 0;
  switch (verticalAlignFlags) 
  {
    case NS_STYLE_VERTICAL_ALIGN_BASELINE:
      // Align the baselines of the child frame with the baselines of 
      // other children in the same row which have 'vertical-align: baseline'
      kidYTop = topInset + aMaxAscent - GetCellBaseline();
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
      kidYTop = nsTableFrame::RoundToPixel(kidYTop, p2t, eAlwaysRoundDown);
  }
  // if the content is larger than the cell height align from top
  kidYTop = PR_MAX(0, kidYTop);

  firstKid->SetPosition(nsPoint(kidRect.x, kidYTop));
  nsHTMLReflowMetrics desiredSize;
  desiredSize.width = mRect.width;
  desiredSize.height = mRect.height;
  GetSelfOverflow(desiredSize.mOverflowArea);
  ConsiderChildOverflow(desiredSize.mOverflowArea, firstKid);
  FinishAndStoreOverflow(&desiredSize);
  if (kidYTop != kidRect.y) {
    // Make sure any child views are correctly positioned. We know the inner table
    // cell won't have a view
    nsContainerFrame::PositionChildViews(firstKid);
  }
  if (HasView()) {
    nsContainerFrame::SyncFrameViewAfterReflow(presContext, this,
                                               GetView(),
                                               &desiredSize.mOverflowArea, 0);
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

nscoord
nsTableCellFrame::GetCellBaseline() const
{
  // Ignore the position of the inner frame relative to the cell frame
  // since we want the position as though the inner were top-aligned.
  nsIFrame *inner = mFrames.FirstChild();
  nscoord borderPadding = GetUsedBorderAndPadding().top;
  nscoord result;
  if (nsLayoutUtils::GetFirstLineBaseline(inner, &result))
    return result + borderPadding;
  return inner->GetContentRect().YMost() - inner->GetPosition().y +
         borderPadding;
}

PRInt32 nsTableCellFrame::GetRowSpan()
{  
  PRInt32 rowSpan=1;
  nsGenericHTMLElement *hc = nsGenericHTMLElement::FromContent(mContent);

  if (hc) {
    const nsAttrValue* attr = hc->GetParsedAttr(nsGkAtoms::rowspan); 
    if (attr && attr->Type() == nsAttrValue::eInteger) { 
       rowSpan = attr->GetIntegerValue(); 
    }
  }
  return rowSpan;
}

PRInt32 nsTableCellFrame::GetColSpan()
{  
  PRInt32 colSpan=1;
  nsGenericHTMLElement *hc = nsGenericHTMLElement::FromContent(mContent);

  if (hc) {
    const nsAttrValue* attr = hc->GetParsedAttr(nsGkAtoms::colspan); 
    if (attr && attr->Type() == nsAttrValue::eInteger) { 
       colSpan = attr->GetIntegerValue(); 
    }
  }
  return colSpan;
}

/* virtual */ nscoord
nsTableCellFrame::GetMinWidth(nsIRenderingContext *aRenderingContext)
{
  nscoord result = 0;
  DISPLAY_MIN_WIDTH(this, result);

  nsIFrame *inner = mFrames.FirstChild();
  result = nsLayoutUtils::IntrinsicForContainer(aRenderingContext, inner,
                                                    nsLayoutUtils::MIN_WIDTH);
  return result;
}

/* virtual */ nscoord
nsTableCellFrame::GetPrefWidth(nsIRenderingContext *aRenderingContext)
{
  nscoord result = 0;
  DISPLAY_PREF_WIDTH(this, result);

  nsIFrame *inner = mFrames.FirstChild();
  result = nsLayoutUtils::IntrinsicForContainer(aRenderingContext, inner,
                                                nsLayoutUtils::PREF_WIDTH);
  return result;
}

/* virtual */ nsIFrame::IntrinsicWidthOffsetData
nsTableCellFrame::IntrinsicWidthOffsets(nsIRenderingContext* aRenderingContext)
{
  IntrinsicWidthOffsetData result =
    nsHTMLContainerFrame::IntrinsicWidthOffsets(aRenderingContext);

  result.hMargin = 0;
  result.hPctMargin = 0;

  GET_PIXELS_TO_TWIPS(GetPresContext(), p2t);
  nsMargin border;
  GetBorderWidth(p2t, border);
  result.hBorder = border.LeftRight();

  return result;
}

#ifdef DEBUG
#define PROBABLY_TOO_LARGE 1000000
static
void DebugCheckChildSize(nsIFrame*            aChild, 
                         nsHTMLReflowMetrics& aMet, 
                         nsSize&              aAvailSize)
{
  if ((aMet.width < 0) || (aMet.width > PROBABLY_TOO_LARGE)) {
    printf("WARNING: cell content %p has large width %d \n", aChild, aMet.width);
  }
}
#endif

// the computed height for the cell, which descendants use for percent height calculations
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
  DO_GLOBAL_REFLOW_COUNT("nsTableCellFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);
  GET_PIXELS_TO_TWIPS(aPresContext, p2t);

  // work around pixel rounding errors, round down to ensure we don't exceed the avail height in
  nscoord availHeight = aReflowState.availableHeight;
  if (NS_UNCONSTRAINEDSIZE != availHeight) {
    availHeight = nsTableFrame::RoundToPixel(availHeight, p2t, eAlwaysRoundDown);
  }

  // see if a special height reflow needs to occur due to having a pct height
  nsTableFrame::CheckRequestSpecialHeightReflow(aReflowState);

  aStatus = NS_FRAME_COMPLETE;
  nsSize availSize(aReflowState.availableWidth, availHeight);

  /* It's the 'border-collapse' on the table that matters */
  nsTableFrame* tableFrame = nsTableFrame::GetTableFrame(this);
  if (!tableFrame)
    ABORT1(NS_ERROR_NULL_POINTER);

  nsMargin borderPadding = aReflowState.mComputedPadding;
  nsMargin border;
  GetBorderWidth(p2t, border);
  borderPadding += border;
  
  nscoord topInset    = borderPadding.top;
  nscoord rightInset  = borderPadding.right;
  nscoord bottomInset = borderPadding.bottom;
  nscoord leftInset   = borderPadding.left;

  // reduce available space by insets, if we're in a constrained situation
  availSize.width -= leftInset+rightInset;
  if (NS_UNCONSTRAINEDSIZE!=availSize.height)
    availSize.height -= topInset+bottomInset;

  // Try to reflow the child into the available space. It might not
  // fit or might need continuing.
  if (availSize.height < 0)
    availSize.height = 1;

  nsHTMLReflowMetrics kidSize(aDesiredSize.mFlags);
  kidSize.width = kidSize.height = 0;
  SetPriorAvailWidth(aReflowState.availableWidth);
  nsIFrame* firstKid = mFrames.FirstChild();
  NS_ASSERTION(firstKid, "Frame construction error, a table cell always has an inner cell frame");

  nscoord computedPaginatedHeight = 0;

  if (aReflowState.mFlags.mSpecialHeightReflow) {
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

  nsHTMLReflowState kidReflowState(aPresContext, aReflowState, firstKid,
                                   availSize);
  // mIPercentHeightObserver is for children of cells in quirks mode,
  // but only those than are tables in standards mode.  NeedsToObserve
  // will determine how far this is propagated to descendants.
  kidReflowState.mPercentHeightObserver = this;
  if (aReflowState.mFlags.mSpecialHeightReflow) {
    kidReflowState.mFlags.mVResize = PR_TRUE;
  }

  nsPoint kidOrigin(leftInset, topInset);

  ReflowChild(firstKid, aPresContext, kidSize, kidReflowState,
              kidOrigin.x, kidOrigin.y, 0, aStatus);
  if (GetStateBits() & NS_FRAME_IS_DIRTY) {
    Invalidate(GetOverflowRect(), PR_FALSE);
  }

#ifdef NS_DEBUG
  DebugCheckChildSize(firstKid, kidSize, availSize);
#endif

  // 0 dimensioned cells need to be treated specially in Standard/NavQuirks mode 
  // see testcase "emptyCells.html"
  SetContentEmpty(0 == kidSize.height);

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

  // the overflow area will be computed when the child will be vertically aligned

  if (aReflowState.mFlags.mSpecialHeightReflow) {
    if (aDesiredSize.height > mRect.height) {
      // set a bit indicating that the pct height contents exceeded 
      // the height that they could honor in the pass 2 reflow
      SetHasPctOverHeight(PR_TRUE);
    }
    if (NS_UNCONSTRAINEDSIZE == aReflowState.availableHeight) {
      aDesiredSize.height = mRect.height;
    }
  }

  // remember the desired size for this reflow
  SetDesiredSize(aDesiredSize);

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
  aColIndex = mColIndex;
  return  NS_OK;
}

nsIFrame*
NS_NewTableCellFrame(nsIPresShell*   aPresShell,
                     nsStyleContext* aContext,
                     PRBool          aIsBorderCollapse)
{
  if (aIsBorderCollapse)
    return new (aPresShell) nsBCTableCellFrame(aContext);
  else
    return new (aPresShell) nsTableCellFrame(aContext);
}

nsMargin* 
nsTableCellFrame::GetBorderWidth(float      aPixelsToTwips,
                                 nsMargin&  aBorder) const
{
  aBorder = GetStyleBorder()->GetBorder();
  return &aBorder;
}

nsIAtom*
nsTableCellFrame::GetType() const
{
  return nsGkAtoms::tableCellFrame;
}

/* virtual */ PRBool
nsTableCellFrame::IsContainingBlock() const
{
  return PR_TRUE;
}

#ifdef DEBUG
NS_IMETHODIMP
nsTableCellFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("TableCell"), aResult);
}
#endif

// nsBCTableCellFrame

nsBCTableCellFrame::nsBCTableCellFrame(nsStyleContext* aContext)
:nsTableCellFrame(aContext)
{
  mTopBorder = mRightBorder = mBottomBorder = mLeftBorder = 0;
}

nsBCTableCellFrame::~nsBCTableCellFrame()
{
}

nsIAtom*
nsBCTableCellFrame::GetType() const
{
  return nsGkAtoms::bcTableCellFrame;
}

/* virtual */ nsMargin
nsBCTableCellFrame::GetUsedBorder() const
{
  nsMargin result;
  GetBorderWidth(GetPresContext()->PixelsToTwips(), result);
  return result;
}

#ifdef DEBUG
NS_IMETHODIMP
nsBCTableCellFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("BCTableCell"), aResult);
}
#endif

nsMargin* 
nsBCTableCellFrame::GetBorderWidth(float      aPixelsToTwips,
                                   nsMargin&  aBorder) const
{
  aBorder.top    = BC_BORDER_BOTTOM_HALF_COORD(aPixelsToTwips, mTopBorder);
  aBorder.right  = BC_BORDER_LEFT_HALF_COORD(aPixelsToTwips, mRightBorder);
  aBorder.bottom = BC_BORDER_TOP_HALF_COORD(aPixelsToTwips, mBottomBorder);
  aBorder.left   = BC_BORDER_RIGHT_HALF_COORD(aPixelsToTwips, mLeftBorder);
  return &aBorder;
}

BCPixelSize
nsBCTableCellFrame::GetBorderWidth(PRUint8 aSide) const
{
  switch(aSide) {
  case NS_SIDE_TOP:
    return BC_BORDER_BOTTOM_HALF(mTopBorder);
  case NS_SIDE_RIGHT:
    return BC_BORDER_LEFT_HALF(mRightBorder);
  case NS_SIDE_BOTTOM:
    return BC_BORDER_TOP_HALF(mBottomBorder);
  default:
    return BC_BORDER_RIGHT_HALF(mLeftBorder);
  }
}

void 
nsBCTableCellFrame::SetBorderWidth(PRUint8 aSide,
                                   BCPixelSize aValue)
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

/* virtual */ void
nsBCTableCellFrame::GetSelfOverflow(nsRect& aOverflowArea)
{
  nsMargin halfBorder;
  GET_PIXELS_TO_TWIPS(GetPresContext(), p2t);
  halfBorder.top = BC_BORDER_TOP_HALF_COORD(p2t, mTopBorder);
  halfBorder.right = BC_BORDER_RIGHT_HALF_COORD(p2t, mRightBorder);
  halfBorder.bottom = BC_BORDER_BOTTOM_HALF_COORD(p2t, mBottomBorder);
  halfBorder.left = BC_BORDER_LEFT_HALF_COORD(p2t, mLeftBorder);

  nsRect overflow(nsPoint(0,0), GetSize());
  overflow.Inflate(halfBorder);
  aOverflowArea = overflow;
}


void
nsBCTableCellFrame::PaintBackground(nsIRenderingContext& aRenderingContext,
                                    const nsRect&        aDirtyRect,
                                    nsPoint              aPt)
{
  // make border-width reflect the half of the border-collapse
  // assigned border that's inside the cell
  nsPresContext* presContext = GetPresContext();
  GET_PIXELS_TO_TWIPS(presContext, p2t);
  nsMargin borderWidth;
  GetBorderWidth(p2t, borderWidth);

  nsStyleBorder myBorder(*GetStyleBorder());

  NS_FOR_CSS_SIDES(side) {
    myBorder.SetBorderWidth(side, borderWidth.side(side));
  }

  nsRect rect(aPt, GetSize());
  nsCSSRendering::PaintBackground(presContext, aRenderingContext, this,
                                  aDirtyRect, rect, myBorder, *GetStylePadding(),
                                  PR_TRUE);
}
