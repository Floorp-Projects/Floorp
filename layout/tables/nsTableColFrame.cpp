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
#include "nsTableColFrame.h"
#include "nsContainerFrame.h"
#include "nsIReflowCommand.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsHTMLIIDs.h"
#include "nsHTMLAtoms.h"
#include "nsCSSRendering.h"

nsTableColFrame::nsTableColFrame()
  : nsFrame(), mIsAnonymous(PR_FALSE), mProportion(WIDTH_NOT_SET)
{
  // note that all fields are initialized to 0 by nsFrame::operator new
  ResetSizingInfo();
}

nsTableColFrame::~nsTableColFrame()
{
}

void nsTableColFrame::ResetSizingInfo()
{
  nsCRT::memset(mWidths, WIDTH_NOT_SET, NUM_WIDTHS * sizeof(PRInt32));
  mProportion = 0;
  mConstraint = eNoConstraint;
  mConstrainingCell = nsnull;
}

NS_METHOD nsTableColFrame::Paint(nsIPresContext& aPresContext,
                                 nsIRenderingContext& aRenderingContext,
                                 const nsRect& aDirtyRect,
                                 nsFramePaintLayer aWhichLayer)
{
  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
    nsCompatibility mode;
    aPresContext.GetCompatibilityMode(&mode);
    if (eCompatibility_Standard == mode) {
      const nsStyleDisplay* disp =
        (const nsStyleDisplay*)mStyleContext->GetStyleData(eStyleStruct_Display);
      if (disp->mVisible) {
        const nsStyleSpacing* spacing =
          (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);
        const nsStyleColor* color =
          (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);
        nsRect rect(0, 0, mRect.width, mRect.height);
        nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                        aDirtyRect, rect, *color, *spacing, 0, 0);
      }
    }
  }
  return NS_OK;
}


NS_METHOD nsTableColFrame::Reflow(nsIPresContext&      aPresContext,
                                  nsHTMLReflowMetrics& aDesiredSize,
                                  const nsHTMLReflowState& aReflowState,
                                  nsReflowStatus&      aStatus)
{
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
  for (PRInt32 i = 0; i < aIndent + 1; i++) {
    indent[i] = ' ';
  }
  indent[aIndent] = 0;

  printf("%s**START COL DUMP** colIndex=%d isAnonymous=%d constraint=%d",
    indent, mColIndex, mIsAnonymous, mConstraint);
  printf("\n%s widths=", indent);
  for (PRInt32 widthX = 0; widthX < NUM_WIDTHS; widthX++) {
    printf("%d ", mWidths[widthX]);
  }
  printf(" **END COL DUMP** ");
  delete [] indent;
}

/* ----- global methods ----- */

nsresult 
NS_NewTableColFrame(nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsTableColFrame* it = new nsTableColFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

NS_IMETHODIMP
nsTableColFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("TableCol", aResult);
}

#ifdef DEBUG
NS_IMETHODIMP
nsTableColFrame::SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const
{
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  PRUint32 sum = sizeof(*this);
  // XXX write me
  *aResult = sum;
  return NS_OK;
}
#endif
