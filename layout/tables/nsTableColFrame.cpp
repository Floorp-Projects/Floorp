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
#include "nsCOMPtr.h"
#include "nsTableColFrame.h"
#include "nsContainerFrame.h"
#include "nsIReflowCommand.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsHTMLIIDs.h"
#include "nsHTMLAtoms.h"
#include "nsCSSRendering.h"
#include "nsLayoutAtoms.h"
#include "nsIContent.h"
#include "nsIDOMHTMLTableColElement.h"

#define COL_TYPE_CONTENT              0x0
#define COL_TYPE_ANONYMOUS_COL        0x1
#define COL_TYPE_ANONYMOUS_COLGROUP   0x2
#define COL_TYPE_ANONYMOUS_CELL       0x3


nsTableColFrame::nsTableColFrame()
  : nsFrame()
{
  // note that all fields are initialized to 0 by nsFrame::operator new
  mBits.mIsAnonymous = PR_FALSE;
  ResetSizingInfo();
  SetType(eColContent);
}

nsTableColFrame::~nsTableColFrame()
{
}

nsTableColType nsTableColFrame::GetType() const {
  switch(mBits.mType) {
  case COL_TYPE_ANONYMOUS_COL:
    return eColAnonymousCol;
  case COL_TYPE_ANONYMOUS_COLGROUP:
    return eColAnonymousColGroup;
  case COL_TYPE_ANONYMOUS_CELL:
    return eColAnonymousCell;
  default:
    return eColContent;
  }
}

void nsTableColFrame::SetType(nsTableColType aType) {
  mBits.mType = aType - eColContent;
}


// XXX what about other style besides width
nsStyleCoord nsTableColFrame::GetStyleWidth() const
{
  nsStylePosition* position = nsnull;
  position = (nsStylePosition*)mStyleContext->GetStyleData(eStyleStruct_Position);
  nsStyleCoord styleWidth = position->mWidth;
  // the following should not be necessary since html.css defines table-col and
  // :table-col to inherit. However, :table-col is not inheriting properly
  if (eStyleUnit_Auto == styleWidth.GetUnit()) {
    nsIFrame* parent;
    GetParent(&parent);
    nsCOMPtr<nsIStyleContext> styleContext;
    parent->GetStyleContext(getter_AddRefs(styleContext)); 
    if (styleContext) {
      position = (nsStylePosition*)styleContext->GetStyleData(eStyleStruct_Position);
      styleWidth = position->mWidth;
    }
  }

  nsStyleCoord returnWidth;
  returnWidth.mUnit  = styleWidth.mUnit;
  returnWidth.mValue = styleWidth.mValue;
  return returnWidth;
}

void nsTableColFrame::ResetSizingInfo()
{
  nsCRT::memset(mWidths, WIDTH_NOT_SET, NUM_WIDTHS * sizeof(PRInt32));
  mConstraint = eNoConstraint;
  mConstrainingCell = nsnull;
}

NS_METHOD 
nsTableColFrame::Paint(nsIPresContext*      aPresContext,
                       nsIRenderingContext& aRenderingContext,
                       const nsRect&        aDirtyRect,
                       nsFramePaintLayer    aWhichLayer,
                       PRUint32             aFlags)
{
  PRBool isVisible;
  if (NS_SUCCEEDED(IsVisibleForPainting(aPresContext, aRenderingContext, PR_FALSE, &isVisible)) && !isVisible) {
    return NS_OK;
  }
  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
    nsCompatibility mode;
    aPresContext->GetCompatibilityMode(&mode);
    if (eCompatibility_Standard == mode) {
      const nsStyleVisibility* vis = 
      (const nsStyleVisibility*)mStyleContext->GetStyleData(eStyleStruct_Visibility);
      if (vis->IsVisibleOrCollapsed()) {
        const nsStyleBorder* border =
          (const nsStyleBorder*)mStyleContext->GetStyleData(eStyleStruct_Border);
        const nsStyleBackground* color =
          (const nsStyleBackground*)mStyleContext->GetStyleData(eStyleStruct_Background);
        nsRect rect(0, 0, mRect.width, mRect.height);
        nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                        aDirtyRect, rect, *color, *border, 0, 0);
      }
    }
  }
  return NS_OK;
}

// override, since we want to act like a block
NS_IMETHODIMP
nsTableColFrame::GetFrameForPoint(nsIPresContext* aPresContext,
                          const nsPoint& aPoint,
                          nsFramePaintLayer aWhichLayer,
                          nsIFrame** aFrame)
{
  if ((aWhichLayer == NS_FRAME_PAINT_LAYER_BACKGROUND) &&
      (mRect.Contains(aPoint))) {
    const nsStyleVisibility* vis = 
      (const nsStyleVisibility*)mStyleContext->GetStyleData(eStyleStruct_Visibility);
    if (vis->IsVisible()) {
      *aFrame = this;
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}

NS_METHOD nsTableColFrame::Reflow(nsIPresContext*          aPresContext,
                                  nsHTMLReflowMetrics&     aDesiredSize,
                                  const nsHTMLReflowState& aReflowState,
                                  nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsTableColFrame", aReflowState.reason);
  DISPLAY_REFLOW(this, aReflowState, aDesiredSize, aStatus);
  aDesiredSize.width=0;
  aDesiredSize.height=0;
  if (nsnull!=aDesiredSize.maxElementSize)
  {
    aDesiredSize.maxElementSize->width=0;
    aDesiredSize.maxElementSize->height=0;
  }
  aStatus = NS_FRAME_COMPLETE;
  return NS_OK;
}

PRInt32 nsTableColFrame::GetSpan()
{  
  const nsStyleTable* tableStyle;
  GetStyleData(eStyleStruct_Table, (const nsStyleStruct *&)tableStyle);
  return tableStyle->mSpan;
}

nscoord nsTableColFrame::GetWidth(PRUint32 aWidthType)
{
  NS_ASSERTION(aWidthType < NUM_WIDTHS, "GetWidth: bad width type");
  return mWidths[aWidthType];
}

void nsTableColFrame::SetWidth(PRUint32 aWidthType,
                               nscoord  aWidth)
{
  NS_ASSERTION(aWidthType < NUM_WIDTHS, "SetWidth: bad width type");
  mWidths[aWidthType] = aWidth;
#ifdef MY_DEBUG
  if (aWidth > 0) {
    nscoord minWidth = GetMinWidth();
    if ((MIN_CON != aWidthType) && (aWidth < minWidth)) {
      printf("non min width set to lower than min \n");
    }
  }
#endif
}

nscoord nsTableColFrame::GetMinWidth()
{
  return PR_MAX(mWidths[MIN_CON], mWidths[MIN_ADJ]);
}

nscoord nsTableColFrame::GetDesWidth()
{
  return PR_MAX(mWidths[DES_CON], mWidths[DES_ADJ]);
}

nscoord nsTableColFrame::GetFixWidth()
{
  return PR_MAX(mWidths[FIX], mWidths[FIX_ADJ]);
}

nscoord nsTableColFrame::GetPctWidth()
{
  return PR_MAX(mWidths[PCT], mWidths[PCT_ADJ]);
}

void nsTableColFrame::Dump(PRInt32 aIndent)
{
  char* indent = new char[aIndent + 1];
  if (!indent) return;
  for (PRInt32 i = 0; i < aIndent + 1; i++) {
    indent[i] = ' ';
  }
  indent[aIndent] = 0;

  printf("%s**START COL DUMP** colIndex=%d isAnonymous=%d constraint=%d",
    indent, mColIndex, mBits.mIsAnonymous, mConstraint);
  printf("\n%s widths=", indent);
  for (PRInt32 widthX = 0; widthX < NUM_WIDTHS; widthX++) {
    printf("%d ", mWidths[widthX]);
  }
  printf(" **END COL DUMP** ");
  delete [] indent;
}

/* ----- global methods ----- */

nsresult 
NS_NewTableColFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsTableColFrame* it = new (aPresShell) nsTableColFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

NS_IMETHODIMP
nsTableColFrame::Init(nsIPresContext*  aPresContext,
                      nsIContent*      aContent,
                      nsIFrame*        aParent,
                      nsIStyleContext* aContext,
                      nsIFrame*        aPrevInFlow)
{
  nsresult  rv;

  // Let the the base class do its initialization
  rv = nsFrame::Init(aPresContext, aContent, aParent, aContext,
                     aPrevInFlow);

  // record that children that are ignorable whitespace should be excluded 
  mState |= NS_FRAME_EXCLUDE_IGNORABLE_WHITESPACE;

  return rv;
}

NS_IMETHODIMP
nsTableColFrame::GetFrameType(nsIAtom** aType) const
{
  NS_PRECONDITION(nsnull != aType, "null OUT parameter pointer");
  *aType = nsLayoutAtoms::tableColFrame; 
  NS_ADDREF(*aType);
  return NS_OK;
}

#ifdef DEBUG
NS_IMETHODIMP
nsTableColFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("TableCol"), aResult);
}

NS_IMETHODIMP
nsTableColFrame::SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const
{
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  PRUint32 sum = sizeof(*this);
  *aResult = sum;
  return NS_OK;
}
#endif
