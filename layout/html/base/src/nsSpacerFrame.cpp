/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsHTMLParts.h"
#include "nsIHTMLContent.h"
#include "nsFrame.h"
#include "nsLineLayout.h"
#include "nsHTMLIIDs.h"
#include "nsIPresContext.h"
#include "nsHTMLAtoms.h"
#include "nsUnitConversion.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsINameSpaceManager.h"

// Spacer type's
#define TYPE_WORD  0            // horizontal space
#define TYPE_LINE  1            // line-break + vertical space
#define TYPE_IMAGE 2            // acts like a sized image with nothing to see

class SpacerFrame : public nsFrame {
public:
  friend nsresult NS_NewSpacerFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

  // nsIHTMLReflow
  NS_IMETHOD Reflow(nsIPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  PRUint8 GetType();

protected:
  SpacerFrame();
  virtual ~SpacerFrame();
};

nsresult
NS_NewSpacerFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  SpacerFrame* it = new (aPresShell) SpacerFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

SpacerFrame::SpacerFrame()
{
}

SpacerFrame::~SpacerFrame()
{
}

NS_IMETHODIMP
SpacerFrame::Reflow(nsIPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aMetrics,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("SpacerFrame", aReflowState.reason);
  aStatus = NS_FRAME_COMPLETE;

  // By default, we have no area
  aMetrics.width = 0;
  aMetrics.height = 0;
  aMetrics.ascent = 0;
  aMetrics.descent = 0;

  const nsStylePosition*  position;
  GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)position);

  PRUint8 type = GetType();
  switch (type) {
  case TYPE_WORD:
    break;

  case TYPE_LINE:
    aStatus = NS_INLINE_LINE_BREAK_AFTER(NS_FRAME_COMPLETE);
    if (eStyleUnit_Coord == position->mHeight.GetUnit()) {
      aMetrics.width = position->mHeight.GetCoordValue();
    }
    aMetrics.ascent = aMetrics.height;
    break;

  case TYPE_IMAGE:
    // width
    nsStyleUnit unit = position->mWidth.GetUnit();
    if (eStyleUnit_Coord == unit) {
      aMetrics.width = position->mWidth.GetCoordValue();
    }
    else if (eStyleUnit_Percent == unit) 
    {
      if (NS_UNCONSTRAINEDSIZE != aReflowState.availableWidth)
      {
        float factor = position->mWidth.GetPercentValue();
        aMetrics.width = NSToCoordRound (factor * aReflowState.availableWidth);
      }
    }

    // height
    unit = position->mHeight.GetUnit();
    if (eStyleUnit_Coord == unit) {
      aMetrics.height = position->mHeight.GetCoordValue();
    }
    else if (eStyleUnit_Percent == unit) 
    {
      if (NS_UNCONSTRAINEDSIZE != aReflowState.availableHeight)
      {
        float factor = position->mHeight.GetPercentValue();
        aMetrics.width = NSToCoordRound (factor * aReflowState.availableHeight);
      }
    }
    // accent
    aMetrics.ascent = aMetrics.height;
    break;
  }

  if (aMetrics.width || aMetrics.height) {
    // Make sure that the other dimension is non-zero
    if (!aMetrics.width) aMetrics.width = 1;
    if (!aMetrics.height) aMetrics.height = 1;
  }

  if (nsnull != aMetrics.maxElementSize) {
    aMetrics.maxElementSize->width = aMetrics.width;
    aMetrics.maxElementSize->height = aMetrics.height;
  }

  return NS_OK;
}

PRUint8
SpacerFrame::GetType()
{
  PRUint8 type = TYPE_WORD;
  nsAutoString value;
  if (NS_CONTENT_ATTR_HAS_VALUE ==
      mContent->GetAttr(kNameSpaceID_HTML, nsHTMLAtoms::type, value)) {
    if (value.EqualsIgnoreCase("line") ||
        value.EqualsIgnoreCase("vert") ||
        value.EqualsIgnoreCase("vertical")) {
      return TYPE_LINE;
    }
    if (value.EqualsIgnoreCase("block")) {
      return TYPE_IMAGE;
    }
  }
  return type;
}
