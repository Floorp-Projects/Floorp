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
#include "nsLeafFrame.h"
#include "nsIStyleContext.h"
#include "nsCSSRendering.h"
#include "nsHTMLParts.h"
#include "nsIPresShell.h"

nsLeafFrame::nsLeafFrame(nsIContent* aContent, nsIFrame* aParentFrame)
  : nsFrame(aContent, aParentFrame)
{
}

nsLeafFrame::~nsLeafFrame()
{
}

NS_METHOD nsLeafFrame::Paint(nsIPresContext& aPresContext,
                             nsIRenderingContext& aRenderingContext,
                             const nsRect& aDirtyRect)
{
  const nsStyleDisplay* disp =
    (const nsStyleDisplay*)mStyleContext->GetStyleData(eStyleStruct_Display);

  if (disp->mVisible) {
    const nsStyleColor* myColor =
      (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);
    const nsStyleSpacing* mySpacing =
      (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);
    nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                    aDirtyRect, mRect, *myColor);
    nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                aDirtyRect, mRect, *mySpacing, 0);
  }
  return NS_OK;
}

NS_METHOD nsLeafFrame::Reflow(nsIPresContext*      aPresContext,
                              nsReflowMetrics&     aDesiredSize,
                              const nsReflowState& aReflowState,
                              nsReflowStatus&      aStatus)
{
  NS_PRECONDITION(mState & NS_FRAME_IN_REFLOW, "frame is not in reflow");

  // XXX add in code to check for width/height being set via css
  // and if set use them instead of calling GetDesiredSize.

  GetDesiredSize(aPresContext, aReflowState, aDesiredSize);
  AddBordersAndPadding(aPresContext, aDesiredSize);
  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width = aDesiredSize.width;
    aDesiredSize.maxElementSize->height = aDesiredSize.height;
  }
  aStatus = NS_FRAME_COMPLETE;
  return NS_OK;
}

// XXX how should border&padding effect baseline alignment?
// => descent = borderPadding.bottom for example
void nsLeafFrame::AddBordersAndPadding(nsIPresContext* aPresContext,
                                       nsReflowMetrics& aDesiredSize)
{
  const nsStyleSpacing* space =
    (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);
  nsMargin  borderPadding;
  space->CalcBorderPaddingFor(this, borderPadding);
  aDesiredSize.width += borderPadding.left + borderPadding.right;
  aDesiredSize.height += borderPadding.top + borderPadding.bottom;
  aDesiredSize.ascent = aDesiredSize.height;
  aDesiredSize.descent = 0;
}

void nsLeafFrame::GetInnerArea(nsIPresContext* aPresContext,
                               nsRect& aInnerArea) const
{
  const nsStyleSpacing* space =
    (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);
  nsMargin  borderPadding;
  space->CalcBorderPaddingFor(this, borderPadding);
  aInnerArea.x = borderPadding.left;
  aInnerArea.y = borderPadding.top;
  aInnerArea.width = mRect.width -
    (borderPadding.left + borderPadding.right);
  aInnerArea.height = mRect.height -
    (borderPadding.top + borderPadding.bottom);
}

NS_METHOD nsLeafFrame::ContentChanged(nsIPresShell*   aShell,
                                      nsIPresContext* aPresContext,
                                      nsIContent*     aChild,
                                      nsISupports*    aSubContent)
{
  // Generate a reflow command with this frame as the target frame
  nsIReflowCommand* cmd;
  nsresult          result;
                                                
  result = NS_NewHTMLReflowCommand(&cmd, this, nsIReflowCommand::ContentChanged);
  if (NS_OK == result) {
    aShell->AppendReflowCommand(cmd);
    NS_RELEASE(cmd);
  }

  return result;
}

