/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsCOMPtr.h"
#include "nsTableColFrame.h"
#include "nsTableFrame.h"
#include "nsContainerFrame.h"
#include "nsStyleContext.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsGkAtoms.h"
#include "nsCSSRendering.h"
#include "nsIContent.h"

#define COL_TYPE_BITS                 (NS_FRAME_STATE_BIT(28) | \
                                       NS_FRAME_STATE_BIT(29) | \
                                       NS_FRAME_STATE_BIT(30) | \
                                       NS_FRAME_STATE_BIT(31))
#define COL_TYPE_OFFSET               28

nsTableColFrame::nsTableColFrame(nsStyleContext* aContext) :
  nsSplittableFrame(aContext)
{
  SetColType(eColContent);
  ResetIntrinsics();
  ResetSpanIntrinsics();
  ResetFinalWidth();
}

nsTableColFrame::~nsTableColFrame()
{
}

nsTableColType 
nsTableColFrame::GetColType() const 
{
  return (nsTableColType)((mState & COL_TYPE_BITS) >> COL_TYPE_OFFSET);
}

void 
nsTableColFrame::SetColType(nsTableColType aType) 
{
  NS_ASSERTION(aType != eColAnonymousCol ||
               (GetPrevContinuation() &&
                GetPrevContinuation()->GetNextContinuation() == this &&
                GetPrevContinuation()->GetNextSibling() == this),
               "spanned content cols must be continuations");
  uint32_t type = aType - eColContent;
  RemoveStateBits(COL_TYPE_BITS);
  AddStateBits(nsFrameState(type << COL_TYPE_OFFSET));
}

/* virtual */ void
nsTableColFrame::DidSetStyleContext(nsStyleContext* aOldStyleContext)
{
  nsSplittableFrame::DidSetStyleContext(aOldStyleContext);

  if (!aOldStyleContext) //avoid this on init
    return;
     
  nsTableFrame* tableFrame = nsTableFrame::GetTableFrame(this);
  if (tableFrame->IsBorderCollapse() &&
      tableFrame->BCRecalcNeeded(aOldStyleContext, StyleContext())) {
    nsIntRect damageArea(GetColIndex(), 0, 1, tableFrame->GetRowCount());
    tableFrame->AddBCDamageArea(damageArea);
  }
}

void nsTableColFrame::SetContinuousBCBorderWidth(uint8_t     aForSide,
                                                 BCPixelSize aPixelValue)
{
  switch (aForSide) {
    case NS_SIDE_TOP:
      mTopContBorderWidth = aPixelValue;
      return;
    case NS_SIDE_RIGHT:
      mRightContBorderWidth = aPixelValue;
      return;
    case NS_SIDE_BOTTOM:
      mBottomContBorderWidth = aPixelValue;
      return;
    default:
      NS_ERROR("invalid side arg");
  }
}

void
nsTableColFrame::Reflow(nsPresContext*          aPresContext,
                                  nsHTMLReflowMetrics&     aDesiredSize,
                                  const nsHTMLReflowState& aReflowState,
                                  nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsTableColFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);
  aDesiredSize.ClearSize();
  const nsStyleVisibility* colVis = StyleVisibility();
  bool collapseCol = (NS_STYLE_VISIBILITY_COLLAPSE == colVis->mVisible);
  if (collapseCol) {
    nsTableFrame* tableFrame = nsTableFrame::GetTableFrame(this);
    tableFrame->SetNeedToCollapse(true);
  }
  aStatus = NS_FRAME_COMPLETE;
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
}

int32_t nsTableColFrame::GetSpan()
{
  return StyleTable()->mSpan;
}

#ifdef DEBUG
void nsTableColFrame::Dump(int32_t aIndent)
{
  char* indent = new char[aIndent + 1];
  if (!indent) return;
  for (int32_t i = 0; i < aIndent + 1; i++) {
    indent[i] = ' ';
  }
  indent[aIndent] = 0;

  printf("%s**START COL DUMP**\n%s colIndex=%d coltype=",
    indent, indent, mColIndex);
  nsTableColType colType = GetColType();
  switch (colType) {
  case eColContent:
    printf(" content ");
    break;
  case eColAnonymousCol: 
    printf(" anonymous-column ");
    break;
  case eColAnonymousColGroup:
    printf(" anonymous-colgroup ");
    break;
  case eColAnonymousCell: 
    printf(" anonymous-cell ");
    break;
  }
  printf("\nm:%d c:%d(%c) p:%f sm:%d sc:%d sp:%f f:%d",
         int32_t(mMinCoord), int32_t(mPrefCoord),
         mHasSpecifiedCoord ? 's' : 'u', mPrefPercent,
         int32_t(mSpanMinCoord), int32_t(mSpanPrefCoord),
         mSpanPrefPercent,
         int32_t(GetFinalWidth()));
  printf("\n%s**END COL DUMP** ", indent);
  delete [] indent;
}
#endif
/* ----- global methods ----- */

nsTableColFrame* 
NS_NewTableColFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsTableColFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsTableColFrame)

nsTableColFrame*  
nsTableColFrame::GetNextCol() const
{
  nsIFrame* childFrame = GetNextSibling();
  while (childFrame) {
    if (nsGkAtoms::tableColFrame == childFrame->GetType()) {
      return (nsTableColFrame*)childFrame;
    }
    childFrame = childFrame->GetNextSibling();
  }
  return nullptr;
}

nsIAtom*
nsTableColFrame::GetType() const
{
  return nsGkAtoms::tableColFrame;
}

#ifdef DEBUG_FRAME_DUMP
nsresult
nsTableColFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("TableCol"), aResult);
}
#endif

nsSplittableType
nsTableColFrame::GetSplittableType() const
{
  return NS_FRAME_NOT_SPLITTABLE;
}

void
nsTableColFrame::InvalidateFrame(uint32_t aDisplayItemKey)
{
  nsIFrame::InvalidateFrame(aDisplayItemKey);
  GetParent()->InvalidateFrameWithRect(GetVisualOverflowRect() + GetPosition(), aDisplayItemKey);
}

void
nsTableColFrame::InvalidateFrameWithRect(const nsRect& aRect, uint32_t aDisplayItemKey)
{
  nsIFrame::InvalidateFrameWithRect(aRect, aDisplayItemKey);

  // If we have filters applied that would affects our bounds, then
  // we get an inactive layer created and this is computed
  // within FrameLayerBuilder
  GetParent()->InvalidateFrameWithRect(aRect + GetPosition(), aDisplayItemKey);
}

