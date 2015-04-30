/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTableCellFrame.h"

#include "gfxUtils.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Helpers.h"
#include "nsTableFrame.h"
#include "nsTableColFrame.h"
#include "nsTableRowFrame.h"
#include "nsTableRowGroupFrame.h"
#include "nsTablePainter.h"
#include "nsStyleContext.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsRenderingContext.h"
#include "nsCSSRendering.h"
#include "nsIContent.h"
#include "nsGenericHTMLElement.h"
#include "nsAttrValueInlines.h"
#include "nsHTMLParts.h"
#include "nsGkAtoms.h"
#include "nsIPresShell.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsIDOMNode.h"
#include "nsNameSpaceManager.h"
#include "nsDisplayList.h"
#include "nsLayoutUtils.h"
#include "nsTextFrame.h"
#include "FrameLayerBuilder.h"
#include <algorithm>

//TABLECELL SELECTION
#include "nsFrameSelection.h"
#include "mozilla/LookAndFeel.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::image;

nsTableCellFrame::nsTableCellFrame(nsStyleContext* aContext,
                                   nsTableFrame* aTableFrame)
  : nsContainerFrame(aContext)
  , mDesiredSize(aTableFrame->GetWritingMode())
{
  mColIndex = 0;
  mPriorAvailWidth = 0;

  SetContentEmpty(false);
  SetHasPctOverHeight(false);
}

nsTableCellFrame::~nsTableCellFrame()
{
}

NS_IMPL_FRAMEARENA_HELPERS(nsTableCellFrame)

nsTableCellFrame*
nsTableCellFrame::GetNextCell() const
{
  nsIFrame* childFrame = GetNextSibling();
  while (childFrame) {
    nsTableCellFrame *cellFrame = do_QueryFrame(childFrame);
    if (cellFrame) {
      return cellFrame;
    }
    childFrame = childFrame->GetNextSibling();
  }
  return nullptr;
}

void
nsTableCellFrame::Init(nsIContent*       aContent,
                       nsContainerFrame* aParent,
                       nsIFrame*         aPrevInFlow)
{
  // Let the base class do its initialization
  nsContainerFrame::Init(aContent, aParent, aPrevInFlow);

  if (GetStateBits() & NS_FRAME_FONT_INFLATION_CONTAINER) {
    AddStateBits(NS_FRAME_FONT_INFLATION_FLOW_ROOT);
  }

  if (aPrevInFlow) {
    // Set the column index
    nsTableCellFrame* cellFrame = (nsTableCellFrame*)aPrevInFlow;
    int32_t           colIndex;
    cellFrame->GetColIndex(colIndex);
    SetColIndex(colIndex);
  }
}

void
nsTableCellFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  if (GetStateBits() & NS_FRAME_CAN_HAVE_ABSPOS_CHILDREN) {
    nsTableFrame::UnregisterPositionedTablePart(this, aDestructRoot);
  }

  nsContainerFrame::DestroyFrom(aDestructRoot);
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
      (cellRS->ComputedHeight() == NS_UNCONSTRAINEDSIZE ||
       cellRS->ComputedHeight() == 0)) { // XXXldb Why 0?
    // This is a percentage height on a frame whose percentage heights
    // are based on the height of the cell, since its containing block
    // is the inner cell frame.

    // We'll only honor the percent height if sibling-cells/ancestors
    // have specified/pct height. (Also, siblings only count for this if
    // both this cell and the sibling cell span exactly 1 row.)

    if (nsTableFrame::AncestorsHaveStyleHeight(*cellRS) ||
        (GetTableFrame()->GetEffectiveRowSpan(*this) == 1 &&
         (cellRS->parentReflowState->frame->GetStateBits() &
          NS_ROW_HAS_CELL_WITH_STYLE_HEIGHT))) {

      for (const nsHTMLReflowState *rs = aReflowState.parentReflowState;
           rs != cellRS;
           rs = rs->parentReflowState) {
        rs->frame->AddStateBits(NS_FRAME_CONTAINS_RELATIVE_HEIGHT);
      }

      nsTableFrame::RequestSpecialHeightReflow(*cellRS);
    }
  }
}

// The cell needs to observe its block and things inside its block but nothing below that
bool
nsTableCellFrame::NeedsToObserve(const nsHTMLReflowState& aReflowState)
{
  const nsHTMLReflowState *rs = aReflowState.parentReflowState;
  if (!rs)
    return false;
  if (rs->frame == this) {
    // We always observe the child block.  It will never send any
    // notifications, but we need this so that the observer gets
    // propagated to its kids.
    return true;
  }
  rs = rs->parentReflowState;
  if (!rs) {
    return false;
  }

  // We always need to let the percent height observer be propagated
  // from an outer table frame to an inner table frame.
  nsIAtom *fType = aReflowState.frame->GetType();
  if (fType == nsGkAtoms::tableFrame) {
    return true;
  }

  // We need the observer to be propagated to all children of the cell
  // (i.e., children of the child block) in quirks mode, but only to
  // tables in standards mode.
  return rs->frame == this &&
         (PresContext()->CompatibilityMode() == eCompatibility_NavQuirks ||
          fType == nsGkAtoms::tableOuterFrame);
}

nsresult
nsTableCellFrame::GetRowIndex(int32_t &aRowIndex) const
{
  nsresult result;
  nsTableRowFrame* row = static_cast<nsTableRowFrame*>(GetParent());
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
nsTableCellFrame::GetColIndex(int32_t &aColIndex) const
{
  if (GetPrevInFlow()) {
    return static_cast<nsTableCellFrame*>(FirstInFlow())->GetColIndex(aColIndex);
  }
  else {
    aColIndex = mColIndex;
    return  NS_OK;
  }
}

nsresult
nsTableCellFrame::AttributeChanged(int32_t         aNameSpaceID,
                                   nsIAtom*        aAttribute,
                                   int32_t         aModType)
{
  // We need to recalculate in this case because of the nowrap quirk in
  // BasicTableLayoutStrategy
  if (aNameSpaceID == kNameSpaceID_None && aAttribute == nsGkAtoms::nowrap &&
      PresContext()->CompatibilityMode() == eCompatibility_NavQuirks) {
    PresContext()->PresShell()->
      FrameNeedsReflow(this, nsIPresShell::eTreeChange, NS_FRAME_IS_DIRTY);
  }
  // let the table frame decide what to do
  GetTableFrame()->AttributeChangedFor(this, mContent, aAttribute);
  return NS_OK;
}

/* virtual */ void
nsTableCellFrame::DidSetStyleContext(nsStyleContext* aOldStyleContext)
{
  nsContainerFrame::DidSetStyleContext(aOldStyleContext);

  if (!aOldStyleContext) //avoid this on init
    return;

  nsTableFrame* tableFrame = GetTableFrame();
  if (tableFrame->IsBorderCollapse() &&
      tableFrame->BCRecalcNeeded(aOldStyleContext, StyleContext())) {
    int32_t colIndex, rowIndex;
    GetColIndex(colIndex);
    GetRowIndex(rowIndex);
    // row span needs to be clamped as we do not create rows in the cellmap
    // which do not have cells originating in them
    nsIntRect damageArea(colIndex, rowIndex, GetColSpan(),
      std::min(GetRowSpan(), tableFrame->GetRowCount() - rowIndex));
    tableFrame->AddBCDamageArea(damageArea);
  }
}

#ifdef DEBUG
void
nsTableCellFrame::AppendFrames(ChildListID     aListID,
                               nsFrameList&    aFrameList)
{
  MOZ_CRASH("unsupported operation");
}

void
nsTableCellFrame::InsertFrames(ChildListID     aListID,
                               nsIFrame*       aPrevFrame,
                               nsFrameList&    aFrameList)
{
  MOZ_CRASH("unsupported operation");
}

void
nsTableCellFrame::RemoveFrame(ChildListID     aListID,
                              nsIFrame*       aOldFrame)
{
  MOZ_CRASH("unsupported operation");
}
#endif

void nsTableCellFrame::SetColIndex(int32_t aColIndex)
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
nsTableCellFrame::DecorateForSelection(nsRenderingContext& aRenderingContext,
                                       nsPoint aPt)
{
  NS_ASSERTION(IsSelected(), "Should only be called for selected cells");
  int16_t displaySelection;
  nsPresContext* presContext = PresContext();
  displaySelection = DisplaySelection(presContext);
  if (displaySelection) {
    nsRefPtr<nsFrameSelection> frameSelection =
      presContext->PresShell()->FrameSelection();

    if (frameSelection->GetTableCellSelection()) {
      nscolor       bordercolor;
      if (displaySelection == nsISelectionController::SELECTION_DISABLED) {
        bordercolor = NS_RGB(176,176,176);// disabled color
      }
      else {
        bordercolor =
          LookAndFeel::GetColor(LookAndFeel::eColorID_TextSelectBackground);
      }
      nscoord threePx = nsPresContext::CSSPixelsToAppUnits(3);
      if ((mRect.width > threePx) && (mRect.height > threePx))
      {
        //compare bordercolor to ((nsStyleColor *)myColor)->mBackgroundColor)
        bordercolor = EnsureDifferentColors(bordercolor,
                                            StyleBackground()->mBackgroundColor);

        int32_t appUnitsPerDevPixel = PresContext()->AppUnitsPerDevPixel();
        Point devPixelOffset = NSPointToPoint(aPt, appUnitsPerDevPixel);

        DrawTarget* drawTarget = aRenderingContext.GetDrawTarget();
        AutoRestoreTransform autoRestoreTransform(drawTarget);
        drawTarget->SetTransform(
          drawTarget->GetTransform().PreTranslate(devPixelOffset));

        ColorPattern color(ToDeviceColor(bordercolor));

        nscoord onePixel = nsPresContext::CSSPixelsToAppUnits(1);

        StrokeLineWithSnapping(nsPoint(onePixel, 0), nsPoint(mRect.width, 0),
                               appUnitsPerDevPixel, *drawTarget, color);
        StrokeLineWithSnapping(nsPoint(0, onePixel), nsPoint(0, mRect.height),
                               appUnitsPerDevPixel, *drawTarget, color);
        StrokeLineWithSnapping(nsPoint(onePixel, mRect.height),
                               nsPoint(mRect.width, mRect.height),
                               appUnitsPerDevPixel, *drawTarget, color);
        StrokeLineWithSnapping(nsPoint(mRect.width, onePixel),
                               nsPoint(mRect.width, mRect.height),
                               appUnitsPerDevPixel, *drawTarget, color);
        //middle
        nsRect r(onePixel, onePixel,
                 mRect.width - onePixel, mRect.height - onePixel);
        Rect devPixelRect =
          NSRectToSnappedRect(r, appUnitsPerDevPixel, *drawTarget);
        drawTarget->StrokeRect(devPixelRect, color);
        //shading
        StrokeLineWithSnapping(nsPoint(2*onePixel, mRect.height-2*onePixel),
                               nsPoint(mRect.width-onePixel, mRect.height- (2*onePixel)),
                               appUnitsPerDevPixel, *drawTarget, color);
        StrokeLineWithSnapping(nsPoint(mRect.width - (2*onePixel), 2*onePixel),
                               nsPoint(mRect.width - (2*onePixel), mRect.height-onePixel),
                               appUnitsPerDevPixel, *drawTarget, color);
      }
    }
  }
}

DrawResult
nsTableCellFrame::PaintBackground(nsRenderingContext& aRenderingContext,
                                  const nsRect&        aDirtyRect,
                                  nsPoint              aPt,
                                  uint32_t             aFlags)
{
  nsRect rect(aPt, GetSize());
  return nsCSSRendering::PaintBackground(PresContext(), aRenderingContext, this,
                                         aDirtyRect, rect, aFlags);
}

// Called by nsTablePainter
DrawResult
nsTableCellFrame::PaintCellBackground(nsRenderingContext& aRenderingContext,
                                      const nsRect& aDirtyRect, nsPoint aPt,
                                      uint32_t aFlags)
{
  if (!StyleVisibility()->IsVisible()) {
    return DrawResult::SUCCESS;
  }

  return PaintBackground(aRenderingContext, aDirtyRect, aPt, aFlags);
}

nsresult
nsTableCellFrame::ProcessBorders(nsTableFrame* aFrame,
                                 nsDisplayListBuilder* aBuilder,
                                 const nsDisplayListSet& aLists)
{
  const nsStyleBorder* borderStyle = StyleBorder();
  if (aFrame->IsBorderCollapse() || !borderStyle->HasBorder())
    return NS_OK;

  if (!GetContentEmpty() ||
      StyleTableBorder()->mEmptyCells == NS_STYLE_TABLE_EMPTY_CELLS_SHOW) {
    aLists.BorderBackground()->AppendNewToTop(new (aBuilder)
                                              nsDisplayBorder(aBuilder, this));
  }

  return NS_OK;
}

class nsDisplayTableCellBackground : public nsDisplayTableItem {
public:
  nsDisplayTableCellBackground(nsDisplayListBuilder* aBuilder,
                               nsTableCellFrame* aFrame) :
    nsDisplayTableItem(aBuilder, aFrame) {
    MOZ_COUNT_CTOR(nsDisplayTableCellBackground);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayTableCellBackground() {
    MOZ_COUNT_DTOR(nsDisplayTableCellBackground);
  }
#endif

  virtual void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                       HitTestState* aState,
                       nsTArray<nsIFrame*> *aOutFrames) override {
    aOutFrames->AppendElement(mFrame);
  }
  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     nsRenderingContext* aCtx) override;
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) override;
  NS_DISPLAY_DECL_NAME("TableCellBackground", TYPE_TABLE_CELL_BACKGROUND)
};

void nsDisplayTableCellBackground::Paint(nsDisplayListBuilder* aBuilder,
                                         nsRenderingContext* aCtx)
{
  DrawResult result = static_cast<nsTableCellFrame*>(mFrame)->
    PaintBackground(*aCtx, mVisibleRect, ToReferenceFrame(),
                    aBuilder->GetBackgroundPaintFlags());

  nsDisplayTableItemGeometry::UpdateDrawResult(this, result);
}

nsRect
nsDisplayTableCellBackground::GetBounds(nsDisplayListBuilder* aBuilder,
                                        bool* aSnap)
{
  // revert from nsDisplayTableItem's implementation ... cell backgrounds
  // don't overflow the cell
  return nsDisplayItem::GetBounds(aBuilder, aSnap);
}

void nsTableCellFrame::InvalidateFrame(uint32_t aDisplayItemKey)
{
  nsIFrame::InvalidateFrame(aDisplayItemKey);
  GetParent()->InvalidateFrameWithRect(GetVisualOverflowRect() + GetPosition(), aDisplayItemKey);
}

void nsTableCellFrame::InvalidateFrameWithRect(const nsRect& aRect, uint32_t aDisplayItemKey)
{
  nsIFrame::InvalidateFrameWithRect(aRect, aDisplayItemKey);
  // If we have filters applied that would affects our bounds, then
  // we get an inactive layer created and this is computed
  // within FrameLayerBuilder
  GetParent()->InvalidateFrameWithRect(aRect + GetPosition(), aDisplayItemKey);
}

static void
PaintTableCellSelection(nsIFrame* aFrame, nsRenderingContext* aCtx,
                        const nsRect& aRect, nsPoint aPt)
{
  static_cast<nsTableCellFrame*>(aFrame)->DecorateForSelection(*aCtx, aPt);
}

void
nsTableCellFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                   const nsRect&           aDirtyRect,
                                   const nsDisplayListSet& aLists)
{
  DO_GLOBAL_REFLOW_COUNT_DSP("nsTableCellFrame");
  if (IsVisibleInSelection(aBuilder)) {
    nsTableFrame* tableFrame = GetTableFrame();
    int32_t emptyCellStyle = GetContentEmpty() && !tableFrame->IsBorderCollapse() ?
                                StyleTableBorder()->mEmptyCells
                                : NS_STYLE_TABLE_EMPTY_CELLS_SHOW;
    // take account of 'empty-cells'
    if (StyleVisibility()->IsVisible() &&
        (NS_STYLE_TABLE_EMPTY_CELLS_HIDE != emptyCellStyle)) {
      // display outset box-shadows if we need to.
      const nsStyleBorder* borderStyle = StyleBorder();
      bool hasBoxShadow = !!borderStyle->mBoxShadow;
      if (hasBoxShadow) {
        aLists.BorderBackground()->AppendNewToTop(
          new (aBuilder) nsDisplayBoxShadowOuter(aBuilder, this));
      }
    
      // display background if we need to.
      if (aBuilder->IsForEventDelivery() ||
          !StyleBackground()->IsTransparent() || StyleDisplay()->mAppearance) {
        if (!tableFrame->IsBorderCollapse() ||
            aBuilder->IsAtRootOfPseudoStackingContext() ||
            aBuilder->IsForEventDelivery()) {
          // The cell background was not painted by the nsTablePainter,
          // so we need to do it. We have special background processing here
          // so we need to duplicate some code from nsFrame::DisplayBorderBackgroundOutline
          nsDisplayTableItem* item =
            new (aBuilder) nsDisplayTableCellBackground(aBuilder, this);
          aLists.BorderBackground()->AppendNewToTop(item);
          item->UpdateForFrameBackground(this);
        } else {
          // The nsTablePainter will paint our background. Make sure it
          // knows if we're background-attachment:fixed.
          nsDisplayTableItem* currentItem = aBuilder->GetCurrentTableItem();
          if (currentItem) {
            currentItem->UpdateForFrameBackground(this);
          }
        }
      }
    
      // display inset box-shadows if we need to.
      if (hasBoxShadow) {
        aLists.BorderBackground()->AppendNewToTop(
          new (aBuilder) nsDisplayBoxShadowInner(aBuilder, this));
      }
    
      // display borders if we need to
      ProcessBorders(tableFrame, aBuilder, aLists);
    
      // and display the selection border if we need to
      if (IsSelected()) {
        aLists.BorderBackground()->AppendNewToTop(new (aBuilder)
          nsDisplayGeneric(aBuilder, this, ::PaintTableCellSelection,
                           "TableCellSelection",
                           nsDisplayItem::TYPE_TABLE_CELL_SELECTION));
      }
    }
    
    // the 'empty-cells' property has no effect on 'outline'
    DisplayOutline(aBuilder, aLists);
  }

  // Push a null 'current table item' so that descendant tables can't
  // accidentally mess with our table
  nsAutoPushCurrentTableItem pushTableItem;
  pushTableItem.Push(aBuilder, nullptr);

  nsIFrame* kid = mFrames.FirstChild();
  NS_ASSERTION(kid && !kid->GetNextSibling(), "Table cells should have just one child");
  // The child's background will go in our BorderBackground() list.
  // This isn't a problem since it won't have a real background except for
  // event handling. We do not call BuildDisplayListForNonBlockChildren
  // because that/ would put the child's background in the Content() list
  // which isn't right (e.g., would end up on top of our child floats for
  // event handling).
  BuildDisplayListForChild(aBuilder, kid, aDirtyRect, aLists);
}

nsIFrame::LogicalSides
nsTableCellFrame::GetLogicalSkipSides(const nsHTMLReflowState* aReflowState) const
{
  if (MOZ_UNLIKELY(StyleBorder()->mBoxDecorationBreak ==
                     NS_STYLE_BOX_DECORATION_BREAK_CLONE)) {
    return LogicalSides();
  }

  LogicalSides skip;
  if (nullptr != GetPrevInFlow()) {
    skip |= eLogicalSideBitsBStart;
  }
  if (nullptr != GetNextInFlow()) {
    skip |= eLogicalSideBitsBEnd;
  }
  return skip;
}

/* virtual */ nsMargin
nsTableCellFrame::GetBorderOverflow()
{
  return nsMargin(0, 0, 0, 0);
}

// Align the cell's child frame within the cell

void nsTableCellFrame::VerticallyAlignChild(nscoord aMaxAscent)
{
  /* It's the 'border-collapse' on the table that matters */
  nsMargin borderPadding = GetUsedBorderAndPadding();

  nscoord topInset = borderPadding.top;
  nscoord bottomInset = borderPadding.bottom;

  uint8_t verticalAlignFlags = GetVerticalAlign();

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
  }
  // if the content is larger than the cell height align from top
  kidYTop = std::max(0, kidYTop);

  if (kidYTop != kidRect.y) {
    // Invalidate at the old position first
    firstKid->InvalidateFrameSubtree();
  }

  firstKid->SetPosition(nsPoint(kidRect.x, kidYTop));
  WritingMode wm = GetWritingMode();
  nsHTMLReflowMetrics desiredSize(wm);
  desiredSize.SetSize(wm, GetLogicalSize(wm));

  nsRect overflow(nsPoint(0,0), GetSize());
  overflow.Inflate(GetBorderOverflow());
  desiredSize.mOverflowAreas.SetAllTo(overflow);
  ConsiderChildOverflow(desiredSize.mOverflowAreas, firstKid);
  FinishAndStoreOverflow(&desiredSize);
  if (kidYTop != kidRect.y) {
    // Make sure any child views are correctly positioned. We know the inner table
    // cell won't have a view
    nsContainerFrame::PositionChildViews(firstKid);

    // Invalidate new overflow rect
    firstKid->InvalidateFrameSubtree();
  }
  if (HasView()) {
    nsContainerFrame::SyncFrameViewAfterReflow(PresContext(), this,
                                               GetView(),
                                               desiredSize.VisualOverflow(), 0);
  }
}

bool
nsTableCellFrame::UpdateOverflow()
{
  nsRect bounds(nsPoint(0,0), GetSize());
  bounds.Inflate(GetBorderOverflow());
  nsOverflowAreas overflowAreas(bounds, bounds);

  nsLayoutUtils::UnionChildOverflow(this, overflowAreas);

  return FinishAndStoreOverflow(overflowAreas, GetSize());
}

// Per CSS 2.1, we map 'sub', 'super', 'text-top', 'text-bottom',
// length, percentage, and calc() values to 'baseline'.
uint8_t
nsTableCellFrame::GetVerticalAlign() const
{
  const nsStyleCoord& verticalAlign = StyleTextReset()->mVerticalAlign;
  if (verticalAlign.GetUnit() == eStyleUnit_Enumerated) {
    uint8_t value = verticalAlign.GetIntValue();
    if (value == NS_STYLE_VERTICAL_ALIGN_TOP ||
        value == NS_STYLE_VERTICAL_ALIGN_MIDDLE ||
        value == NS_STYLE_VERTICAL_ALIGN_BOTTOM) {
      return value;
    }
  }
  return NS_STYLE_VERTICAL_ALIGN_BASELINE;
}

bool
nsTableCellFrame::CellHasVisibleContent(nscoord       height,
                                        nsTableFrame* tableFrame,
                                        nsIFrame*     kidFrame)
{
  // see  http://www.w3.org/TR/CSS21/tables.html#empty-cells
  if (height > 0)
    return true;
  if (tableFrame->IsBorderCollapse())
    return true;
  nsIFrame* innerFrame = kidFrame->GetFirstPrincipalChild();
  while(innerFrame) {
    nsIAtom* frameType = innerFrame->GetType();
    if (nsGkAtoms::textFrame == frameType) {
       nsTextFrame* textFrame = static_cast<nsTextFrame*>(innerFrame);
       if (textFrame->HasNoncollapsedCharacters())
         return true;
    }
    else if (nsGkAtoms::placeholderFrame != frameType) {
      return true;
    }
    else {
      nsIFrame *floatFrame = nsLayoutUtils::GetFloatFromPlaceholder(innerFrame);
      if (floatFrame)
        return true;
    }
    innerFrame = innerFrame->GetNextSibling();
  }	
  return false;
}

nscoord
nsTableCellFrame::GetCellBaseline() const
{
  // Ignore the position of the inner frame relative to the cell frame
  // since we want the position as though the inner were top-aligned.
  nsIFrame *inner = mFrames.FirstChild();
  nscoord borderPadding = GetUsedBorderAndPadding().top;
  nscoord result;
  if (nsLayoutUtils::GetFirstLineBaseline(GetWritingMode(), inner, &result))
    return result + borderPadding;
  return inner->GetContentRectRelativeToSelf().YMost() +
         borderPadding;
}

int32_t nsTableCellFrame::GetRowSpan()
{
  int32_t rowSpan=1;
  nsGenericHTMLElement *hc = nsGenericHTMLElement::FromContent(mContent);

  // Don't look at the content's rowspan if we're a pseudo cell
  if (hc && !StyleContext()->GetPseudo()) {
    const nsAttrValue* attr = hc->GetParsedAttr(nsGkAtoms::rowspan);
    // Note that we don't need to check the tag name, because only table cells
    // and table headers parse the "rowspan" attribute into an integer.
    if (attr && attr->Type() == nsAttrValue::eInteger) {
       rowSpan = attr->GetIntegerValue();
    }
  }
  return rowSpan;
}

int32_t nsTableCellFrame::GetColSpan()
{
  int32_t colSpan=1;
  nsGenericHTMLElement *hc = nsGenericHTMLElement::FromContent(mContent);

  // Don't look at the content's colspan if we're a pseudo cell
  if (hc && !StyleContext()->GetPseudo()) {
    const nsAttrValue* attr = hc->GetParsedAttr(nsGkAtoms::colspan);
    // Note that we don't need to check the tag name, because only table cells
    // and table headers parse the "colspan" attribute into an integer.
    if (attr && attr->Type() == nsAttrValue::eInteger) {
       colSpan = attr->GetIntegerValue();
    }
  }
  return colSpan;
}

/* virtual */ nscoord
nsTableCellFrame::GetMinISize(nsRenderingContext *aRenderingContext)
{
  nscoord result = 0;
  DISPLAY_MIN_WIDTH(this, result);

  nsIFrame *inner = mFrames.FirstChild();
  result = nsLayoutUtils::IntrinsicForContainer(aRenderingContext, inner,
                                                    nsLayoutUtils::MIN_ISIZE);
  return result;
}

/* virtual */ nscoord
nsTableCellFrame::GetPrefISize(nsRenderingContext *aRenderingContext)
{
  nscoord result = 0;
  DISPLAY_PREF_WIDTH(this, result);

  nsIFrame *inner = mFrames.FirstChild();
  result = nsLayoutUtils::IntrinsicForContainer(aRenderingContext, inner,
                                                nsLayoutUtils::PREF_ISIZE);
  return result;
}

/* virtual */ nsIFrame::IntrinsicISizeOffsetData
nsTableCellFrame::IntrinsicISizeOffsets(nsRenderingContext* aRenderingContext)
{
  IntrinsicISizeOffsetData result =
    nsContainerFrame::IntrinsicISizeOffsets(aRenderingContext);

  result.hMargin = 0;
  result.hPctMargin = 0;

  nsMargin border;
  GetBorderWidth(border);
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
  WritingMode wm = aMet.GetWritingMode();
  if ((aMet.ISize(wm) < 0) || (aMet.ISize(wm) > PROBABLY_TOO_LARGE)) {
    printf("WARNING: cell content %p has large inline size %d \n",
           static_cast<void*>(aChild), int32_t(aMet.ISize(wm)));
  }
}
#endif

// the computed height for the cell, which descendants use for percent height calculations
// it is the height (minus border, padding) of the cell's first in flow during its final
// reflow without an unconstrained height.
static nscoord
CalcUnpaginagedHeight(nsPresContext*        aPresContext,
                      nsTableCellFrame&     aCellFrame,
                      nsTableFrame&         aTableFrame,
                      nscoord               aVerticalBorderPadding)
{
  const nsTableCellFrame* firstCellInFlow =
    static_cast<nsTableCellFrame*>(aCellFrame.FirstInFlow());
  nsTableFrame* firstTableInFlow  =
    static_cast<nsTableFrame*>(aTableFrame.FirstInFlow());
  nsTableRowFrame* row =
    static_cast<nsTableRowFrame*>(firstCellInFlow->GetParent());
  nsTableRowGroupFrame* firstRGInFlow =
    static_cast<nsTableRowGroupFrame*>(row->GetParent());

  int32_t rowIndex;
  firstCellInFlow->GetRowIndex(rowIndex);
  int32_t rowSpan = aTableFrame.GetEffectiveRowSpan(*firstCellInFlow);

  nscoord computedHeight = firstTableInFlow->GetRowSpacing(rowIndex,
                                                           rowIndex + rowSpan - 1);
  computedHeight -= aVerticalBorderPadding;
  int32_t rowX;
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

void
nsTableCellFrame::Reflow(nsPresContext*           aPresContext,
                         nsHTMLReflowMetrics&     aDesiredSize,
                         const nsHTMLReflowState& aReflowState,
                         nsReflowStatus&          aStatus)
{
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsTableCellFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);

  if (aReflowState.mFlags.mSpecialHeightReflow) {
    FirstInFlow()->AddStateBits(NS_TABLE_CELL_HAD_SPECIAL_REFLOW);
  }

  // see if a special height reflow needs to occur due to having a pct height
  nsTableFrame::CheckRequestSpecialHeightReflow(aReflowState);

  aStatus = NS_FRAME_COMPLETE;
  nsSize availSize(aReflowState.AvailableWidth(), aReflowState.AvailableHeight());

  nsMargin borderPadding = aReflowState.ComputedPhysicalPadding();
  nsMargin border;
  GetBorderWidth(border);
  borderPadding += border;

  nscoord topInset    = borderPadding.top;
  nscoord rightInset  = borderPadding.right;
  nscoord bottomInset = borderPadding.bottom;
  nscoord leftInset   = borderPadding.left;

  // reduce available space by insets, if we're in a constrained situation
  availSize.width -= leftInset + rightInset;
  if (NS_UNCONSTRAINEDSIZE != availSize.height)
    availSize.height -= topInset + bottomInset;

  // Try to reflow the child into the available space. It might not
  // fit or might need continuing.
  if (availSize.height < 0)
    availSize.height = 1;

  WritingMode wm = aReflowState.GetWritingMode();
  nsHTMLReflowMetrics kidSize(wm, aDesiredSize.mFlags);
  kidSize.ClearSize();
  SetPriorAvailWidth(aReflowState.AvailableWidth());
  nsIFrame* firstKid = mFrames.FirstChild();
  NS_ASSERTION(firstKid, "Frame construction error, a table cell always has an inner cell frame");
  nsTableFrame* tableFrame = GetTableFrame();

  if (aReflowState.mFlags.mSpecialHeightReflow) {
    const_cast<nsHTMLReflowState&>(aReflowState).SetComputedHeight(mRect.height - topInset - bottomInset);
    DISPLAY_REFLOW_CHANGE();
  }
  else if (aPresContext->IsPaginated()) {
    nscoord computedUnpaginatedHeight =
      CalcUnpaginagedHeight(aPresContext, (nsTableCellFrame&)*this,
                            *tableFrame, topInset + bottomInset);
    if (computedUnpaginatedHeight > 0) {
      const_cast<nsHTMLReflowState&>(aReflowState).SetComputedHeight(computedUnpaginatedHeight);
      DISPLAY_REFLOW_CHANGE();
    }
  }
  else {
    SetHasPctOverHeight(false);
  }

  nsHTMLReflowState kidReflowState(aPresContext, aReflowState, firstKid,
                                   LogicalSize(firstKid->GetWritingMode(),
                                               availSize));

  // Don't be a percent height observer if we're in the middle of
  // special-height reflow, in case we get an accidental NotifyPercentHeight()
  // call (which we shouldn't honor during special-height reflow)
  if (!aReflowState.mFlags.mSpecialHeightReflow) {
    // mPercentHeightObserver is for children of cells in quirks mode,
    // but only those than are tables in standards mode.  NeedsToObserve
    // will determine how far this is propagated to descendants.
    kidReflowState.mPercentHeightObserver = this;
  }
  // Don't propagate special height reflow state to our kids
  kidReflowState.mFlags.mSpecialHeightReflow = false;

  if (aReflowState.mFlags.mSpecialHeightReflow ||
      (FirstInFlow()->GetStateBits() & NS_TABLE_CELL_HAD_SPECIAL_REFLOW)) {
    // We need to force the kid to have mVResize set if we've had a
    // special reflow in the past, since the non-special reflow needs to
    // resize back to what it was without the special height reflow.
    kidReflowState.SetVResize(true);
  }

  nsPoint kidOrigin(leftInset, topInset);
  nsRect origRect = firstKid->GetRect();
  nsRect origVisualOverflow = firstKid->GetVisualOverflowRect();
  bool firstReflow = (firstKid->GetStateBits() & NS_FRAME_FIRST_REFLOW) != 0;

  ReflowChild(firstKid, aPresContext, kidSize, kidReflowState,
              kidOrigin.x, kidOrigin.y, 0, aStatus);
  if (NS_FRAME_OVERFLOW_IS_INCOMPLETE(aStatus)) {
    // Don't pass OVERFLOW_INCOMPLETE through tables until they can actually handle it
    //XXX should paginate overflow as overflow, but not in this patch (bug 379349)
    NS_FRAME_SET_INCOMPLETE(aStatus);
    printf("Set table cell incomplete %p\n", static_cast<void*>(this));
  }

  // XXXbz is this invalidate actually needed, really?
  if (GetStateBits() & NS_FRAME_IS_DIRTY) {
    InvalidateFrameSubtree();
  }

#ifdef DEBUG
  DebugCheckChildSize(firstKid, kidSize, availSize);
#endif

  // 0 dimensioned cells need to be treated specially in Standard/NavQuirks mode
  // see testcase "emptyCells.html"
  nsIFrame* prevInFlow = GetPrevInFlow();
  bool isEmpty;
  if (prevInFlow) {
    isEmpty = static_cast<nsTableCellFrame*>(prevInFlow)->GetContentEmpty();
  } else {
    isEmpty = !CellHasVisibleContent(kidSize.Height(), tableFrame, firstKid);
  }
  SetContentEmpty(isEmpty);

  // Place the child
  FinishReflowChild(firstKid, aPresContext, kidSize, &kidReflowState,
                    kidOrigin.x, kidOrigin.y, 0);

  nsTableFrame::InvalidateTableFrame(firstKid, origRect, origVisualOverflow,
                                     firstReflow);

  // first, compute the height which can be set w/o being restricted by aMaxSize.height
  LogicalSize cellSize(wm);
  LogicalMargin logicalInsets(wm, nsMargin(topInset, rightInset,
                                           bottomInset, leftInset));
  cellSize.BSize(wm) = kidSize.BSize(wm);

  if (NS_UNCONSTRAINEDSIZE != cellSize.BSize(wm)) {
    cellSize.BSize(wm) += logicalInsets.BStartEnd(wm);
  }

  // next determine the cell's width
  cellSize.ISize(wm) = kidSize.ISize(wm);      // at this point, we've factored in the cell's style attributes

  // factor in border and padding
  if (NS_UNCONSTRAINEDSIZE != cellSize.ISize(wm)) {
    cellSize.ISize(wm) += logicalInsets.IStartEnd(wm);
  }

  // set the cell's desired size and max element size
  aDesiredSize.SetSize(wm, cellSize);

  // the overflow area will be computed when the child will be vertically aligned

  if (aReflowState.mFlags.mSpecialHeightReflow) {
    if (aDesiredSize.Height() > mRect.height) {
      // set a bit indicating that the pct height contents exceeded
      // the height that they could honor in the pass 2 reflow
      SetHasPctOverHeight(true);
    }
    if (NS_UNCONSTRAINEDSIZE == aReflowState.AvailableHeight()) {
      aDesiredSize.Height() = mRect.height;
    }
  }

  // If our parent is in initial reflow, it'll handle invalidating our
  // entire overflow rect.
  if (!(GetParent()->GetStateBits() & NS_FRAME_FIRST_REFLOW) &&
      nsSize(aDesiredSize.Width(), aDesiredSize.Height()) != mRect.Size()) {
    InvalidateFrame();
  }

  // remember the desired size for this reflow
  SetDesiredSize(aDesiredSize);

  // Any absolutely-positioned children will get reflowed in
  // nsFrame::FixupPositionedTableParts in another pass, so propagate our
  // dirtiness to them before our parent clears our dirty bits.
  PushDirtyBitToAbsoluteFrames();

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
}

/* ----- global methods ----- */

NS_QUERYFRAME_HEAD(nsTableCellFrame)
  NS_QUERYFRAME_ENTRY(nsTableCellFrame)
  NS_QUERYFRAME_ENTRY(nsITableCellLayout)
  NS_QUERYFRAME_ENTRY(nsIPercentHeightObserver)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

#ifdef ACCESSIBILITY
a11y::AccType
nsTableCellFrame::AccessibleType()
{
  return a11y::eHTMLTableCellType;
}
#endif

/* This is primarily for editor access via nsITableLayout */
NS_IMETHODIMP
nsTableCellFrame::GetCellIndexes(int32_t &aRowIndex, int32_t &aColIndex)
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

nsTableCellFrame*
NS_NewTableCellFrame(nsIPresShell*   aPresShell,
                     nsStyleContext* aContext,
                     nsTableFrame* aTableFrame)
{
  if (aTableFrame->IsBorderCollapse())
    return new (aPresShell) nsBCTableCellFrame(aContext, aTableFrame);
  else
    return new (aPresShell) nsTableCellFrame(aContext, aTableFrame);
}

NS_IMPL_FRAMEARENA_HELPERS(nsBCTableCellFrame)

nsMargin*
nsTableCellFrame::GetBorderWidth(nsMargin&  aBorder) const
{
  aBorder = StyleBorder()->GetComputedBorder();
  return &aBorder;
}

nsIAtom*
nsTableCellFrame::GetType() const
{
  return nsGkAtoms::tableCellFrame;
}

#ifdef DEBUG_FRAME_DUMP
nsresult
nsTableCellFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("TableCell"), aResult);
}
#endif

// nsBCTableCellFrame

nsBCTableCellFrame::nsBCTableCellFrame(nsStyleContext* aContext,
                                       nsTableFrame* aTableFrame)
  : nsTableCellFrame(aContext, aTableFrame)
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
  GetBorderWidth(result);
  return result;
}

/* virtual */ bool
nsBCTableCellFrame::GetBorderRadii(const nsSize& aFrameSize,
                                   const nsSize& aBorderArea,
                                   Sides aSkipSides,
                                   nscoord aRadii[8]) const
{
  NS_FOR_CSS_HALF_CORNERS(corner) {
    aRadii[corner] = 0;
  }
  return false;
}

#ifdef DEBUG_FRAME_DUMP
nsresult
nsBCTableCellFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("BCTableCell"), aResult);
}
#endif

nsMargin*
nsBCTableCellFrame::GetBorderWidth(nsMargin&  aBorder) const
{
  int32_t aPixelsToTwips = nsPresContext::AppUnitsPerCSSPixel();
  aBorder.top    = BC_BORDER_BOTTOM_HALF_COORD(aPixelsToTwips, mTopBorder);
  aBorder.right  = BC_BORDER_LEFT_HALF_COORD(aPixelsToTwips, mRightBorder);
  aBorder.bottom = BC_BORDER_TOP_HALF_COORD(aPixelsToTwips, mBottomBorder);
  aBorder.left   = BC_BORDER_RIGHT_HALF_COORD(aPixelsToTwips, mLeftBorder);
  return &aBorder;
}

BCPixelSize
nsBCTableCellFrame::GetBorderWidth(mozilla::css::Side aSide) const
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
nsBCTableCellFrame::SetBorderWidth(mozilla::css::Side aSide,
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

/* virtual */ nsMargin
nsBCTableCellFrame::GetBorderOverflow()
{
  nsMargin halfBorder;
  int32_t p2t = nsPresContext::AppUnitsPerCSSPixel();
  halfBorder.top = BC_BORDER_TOP_HALF_COORD(p2t, mTopBorder);
  halfBorder.right = BC_BORDER_RIGHT_HALF_COORD(p2t, mRightBorder);
  halfBorder.bottom = BC_BORDER_BOTTOM_HALF_COORD(p2t, mBottomBorder);
  halfBorder.left = BC_BORDER_LEFT_HALF_COORD(p2t, mLeftBorder);
  return halfBorder;
}


DrawResult
nsBCTableCellFrame::PaintBackground(nsRenderingContext& aRenderingContext,
                                    const nsRect&        aDirtyRect,
                                    nsPoint              aPt,
                                    uint32_t             aFlags)
{
  // make border-width reflect the half of the border-collapse
  // assigned border that's inside the cell
  nsMargin borderWidth;
  GetBorderWidth(borderWidth);

  nsStyleBorder myBorder(*StyleBorder());

  NS_FOR_CSS_SIDES(side) {
    myBorder.SetBorderWidth(side, borderWidth.Side(side));
  }

  nsRect rect(aPt, GetSize());
  // bypassing nsCSSRendering::PaintBackground is safe because this kind
  // of frame cannot be used for the root element
  return nsCSSRendering::PaintBackgroundWithSC(PresContext(), aRenderingContext,
                                               this, aDirtyRect, rect,
                                               StyleContext(), myBorder,
                                               aFlags, nullptr);
}
