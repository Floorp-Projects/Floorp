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
#include "nsIDOMHTMLTableColElement.h"

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
  PRUint32 type = aType - eColContent;
  mState |= (type << COL_TYPE_OFFSET);
}

/* virtual */ void
nsTableColFrame::DidSetStyleContext(nsStyleContext* aOldStyleContext)
{
  if (!aOldStyleContext) //avoid this on init
    return;
     
  nsTableFrame* tableFrame = nsTableFrame::GetTableFrame(this);
  if (tableFrame->IsBorderCollapse() &&
      tableFrame->BCRecalcNeeded(aOldStyleContext, GetStyleContext())) {
    nsIntRect damageArea(GetColIndex(), 0, 1, tableFrame->GetRowCount());
    tableFrame->AddBCDamageArea(damageArea);
  }
}

void nsTableColFrame::SetContinuousBCBorderWidth(PRUint8     aForSide,
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

NS_METHOD nsTableColFrame::Reflow(nsPresContext*          aPresContext,
                                  nsHTMLReflowMetrics&     aDesiredSize,
                                  const nsHTMLReflowState& aReflowState,
                                  nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsTableColFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);
  aDesiredSize.width=0;
  aDesiredSize.height=0;
  const nsStyleVisibility* colVis = GetStyleVisibility();
  bool collapseCol = (NS_STYLE_VISIBILITY_COLLAPSE == colVis->mVisible);
  if (collapseCol) {
    nsTableFrame* tableFrame = nsTableFrame::GetTableFrame(this);
    tableFrame->SetNeedToCollapse(true);
  }
  aStatus = NS_FRAME_COMPLETE;
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return NS_OK;
}

PRInt32 nsTableColFrame::GetSpan()
{
  return GetStyleTable()->mSpan;
}

#ifdef DEBUG
void nsTableColFrame::Dump(PRInt32 aIndent)
{
  char* indent = new char[aIndent + 1];
  if (!indent) return;
  for (PRInt32 i = 0; i < aIndent + 1; i++) {
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
         PRInt32(mMinCoord), PRInt32(mPrefCoord),
         mHasSpecifiedCoord ? 's' : 'u', mPrefPercent,
         PRInt32(mSpanMinCoord), PRInt32(mSpanPrefCoord),
         mSpanPrefPercent,
         PRInt32(GetFinalWidth()));
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

#ifdef DEBUG
NS_IMETHODIMP
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

