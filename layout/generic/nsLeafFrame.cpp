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

static NS_DEFINE_IID(kStyleMoleculeSID, NS_STYLEMOLECULE_SID);
static NS_DEFINE_IID(kStyleColorSID, NS_STYLECOLOR_SID);

nsLeafFrame::nsLeafFrame(nsIContent* aContent,
                         PRInt32 aIndexInParent,
                         nsIFrame* aParentFrame)
  : nsFrame(aContent, aIndexInParent, aParentFrame)
{
}

nsLeafFrame::~nsLeafFrame()
{
}

NS_METHOD nsLeafFrame::Paint(nsIPresContext& aPresContext,
                             nsIRenderingContext& aRenderingContext,
                             const nsRect& aDirtyRect)
{
  nsStyleColor* myColor =
    (nsStyleColor*)mStyleContext->GetData(kStyleColorSID);
  nsStyleMolecule* myMol =
    (nsStyleMolecule*)mStyleContext->GetData(kStyleMoleculeSID);
  nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                  aDirtyRect, mRect, *myColor);
  nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                              aDirtyRect, mRect, *myMol, 0);
  return NS_OK;
}

NS_METHOD nsLeafFrame::ResizeReflow(nsIPresContext* aPresContext,
                                    nsReflowMetrics& aDesiredSize,
                                    const nsSize& aMaxSize,
                                    nsSize* aMaxElementSize,
                                    ReflowStatus& aStatus)
{
  // XXX add in code to check for width/height being set via css
  // and if set use them instead of calling GetDesiredSize.

  GetDesiredSize(aPresContext, aDesiredSize, aMaxSize);
  AddBordersAndPadding(aPresContext, aDesiredSize);
  if (nsnull != aMaxElementSize) {
    aMaxElementSize->width = aDesiredSize.width;
    aMaxElementSize->height = aDesiredSize.height;
  }
  aStatus = frComplete;
  return NS_OK;
}

NS_METHOD nsLeafFrame::IncrementalReflow(nsIPresContext* aPresContext,
                                         nsReflowMetrics& aDesiredSize,
                                         const nsSize& aMaxSize,
                                         nsReflowCommand& aReflowCommand,
                                         ReflowStatus& aStatus)
{
  // XXX Unless the reflow command is a style change, we should
  // just return the current size, otherwise we should invoke
  // GetDesiredSize

  // XXX add in code to check for width/height being set via css
  // and if set use them instead of calling GetDesiredSize.
  GetDesiredSize(aPresContext, aDesiredSize, aMaxSize);
  AddBordersAndPadding(aPresContext, aDesiredSize);

  aStatus = frComplete;
  return NS_OK;
}

// XXX how should border&padding effect baseline alignment?
// => descent = borderPadding.bottom for example
void nsLeafFrame::AddBordersAndPadding(nsIPresContext* aPresContext,
                                       nsReflowMetrics& aDesiredSize)
{
  nsStyleMolecule* mol =
    (nsStyleMolecule*)mStyleContext->GetData(kStyleMoleculeSID);
  aDesiredSize.width += mol->borderPadding.left + mol->borderPadding.right;
  aDesiredSize.height += mol->borderPadding.top + mol->borderPadding.bottom;
  aDesiredSize.ascent = aDesiredSize.height;
  aDesiredSize.descent = 0;
}

void nsLeafFrame::GetInnerArea(nsIPresContext* aPresContext,
                               nsRect& aInnerArea) const
{
  nsStyleMolecule* mol =
    (nsStyleMolecule*)mStyleContext->GetData(kStyleMoleculeSID);
  aInnerArea.x = mol->borderPadding.left;
  aInnerArea.y = mol->borderPadding.top;
  aInnerArea.width = mRect.width -
    (mol->borderPadding.left + mol->borderPadding.right);
  aInnerArea.height = mRect.height -
    (mol->borderPadding.top + mol->borderPadding.bottom);
}

NS_METHOD nsLeafFrame::CreateContinuingFrame(nsIPresContext* aPresContext,
                                             nsIFrame*       aParent,
                                             nsIFrame*&      aContinuingFrame)
{
  NS_NOTREACHED("Attempt to split the unsplittable");
  aContinuingFrame = nsnull;
  return NS_OK;
}
