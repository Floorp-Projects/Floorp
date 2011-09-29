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
    nsRect damageArea = nsRect(GetColIndex(), 0, 1, tableFrame->GetRowCount());
    tableFrame->SetBCDamageArea(damageArea);
  }
  return;
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
    if (tableFrame)  {
      tableFrame->SetNeedToCollapse(PR_TRUE);
    }    
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
         mMinCoord, mPrefCoord, mHasSpecifiedCoord ? 's' : 'u', mPrefPercent,
         mSpanMinCoord, mSpanPrefCoord,
         mSpanPrefPercent,
         GetFinalWidth());
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
  return nsnull;
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

