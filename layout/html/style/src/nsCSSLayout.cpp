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
#include "nsCSSLayout.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsIHTMLReflow.h"
#include "nsIFontMetrics.h"
#include "nsIPresContext.h"
#include "nsRect.h"
#include "nsIPtr.h"
#include "nsHTMLIIDs.h"

NS_DEF_PTR(nsIStyleContext);
NS_DEF_PTR(nsIContent);

/**
 * Horizontally place the children in the container frame.
 */
void
nsCSSLayout::HorizontallyPlaceChildren(nsIPresContext* aCX,
                                       nsIFrame* aContainer,
                                       PRInt32 aTextAlign,
                                       PRInt32 aDirection,
                                       nsIFrame* aFirstChild,
                                       PRInt32 aChildCount,
                                       nscoord aLineWidth,
                                       nscoord aMaxWidth)
{
  if (aLineWidth < aMaxWidth) {
    nscoord dx = 0;
    switch (aTextAlign) {
    case NS_STYLE_TEXT_ALIGN_DEFAULT:
      if (NS_STYLE_DIRECTION_LTR == aDirection) {
        // default alignment for left-to-right is left so do nothing
        return;
      }
      // Fall through to align right case for default alignment
      // used when the direction is right-to-left.

    case NS_STYLE_TEXT_ALIGN_RIGHT:
      dx = aMaxWidth - aLineWidth;
      break;

    case NS_STYLE_TEXT_ALIGN_LEFT:
    case NS_STYLE_TEXT_ALIGN_JUSTIFY:
      // Default layout has everything aligned left
      return;

    case NS_STYLE_TEXT_ALIGN_CENTER:
      dx = (aMaxWidth - aLineWidth) / 2;
      break;
    }

    // Position children
    nsPoint origin;
    nsIFrame* kid = aFirstChild;
    while (--aChildCount >= 0) {
      kid->GetOrigin(origin);
      kid->MoveTo(origin.x + dx, origin.y);
      kid->GetNextSibling(kid);
    }
  }
}

/**
 * Apply css relative positioning to any child that requires it.
 */
void
nsCSSLayout::RelativePositionChildren(nsIPresContext* aCX,
                                      nsIFrame* aContainer,
                                      nsIFrame* aFirstChild,
                                      PRInt32 aChildCount)
{
  nsPoint origin;
  nsIFrame* kid = aFirstChild;
  while (--aChildCount >= 0) {
    const nsStylePosition* kidPosition;
    kid->GetStyleData(eStyleStruct_Position,
                      (const nsStyleStruct*&)kidPosition);
    if (NS_STYLE_POSITION_RELATIVE == kidPosition->mPosition) {
      kid->GetOrigin(origin);
      nscoord dx = 0;
      switch (kidPosition->mLeftOffset.GetUnit()) {
      case eStyleUnit_Percent:
        printf("XXX: not yet implemented: % relative position\n");
      case eStyleUnit_Auto:
        break;
      case eStyleUnit_Coord:
        dx = kidPosition->mLeftOffset.GetCoordValue();
        break;
      }
      nscoord dy = 0;
      switch (kidPosition->mTopOffset.GetUnit()) {
      case eStyleUnit_Percent:
        printf("XXX: not yet implemented: % relative position\n");
      case eStyleUnit_Auto:
        break;
      case eStyleUnit_Coord:
        dx = kidPosition->mTopOffset.GetCoordValue();
        break;
      }
      kid->MoveTo(origin.x + dx, origin.y + dy);
    }
    kid->GetNextSibling(kid);
  }
}

// XXX if this can handle proportional widths then do so
// XXX check against other possible values and update
static PRBool
GetStyleDimension(nsIPresContext* aPresContext,
                  const nsHTMLReflowState& aReflowState,
                  const nsStylePosition* aStylePos,
                  const nsStyleCoord& aCoord,
                  nscoord& aResult)
{
  PRBool rv = PR_FALSE;

  PRIntn unit = aCoord.GetUnit();
  if (eStyleUnit_Coord == unit) {
    aResult = aCoord.GetCoordValue();
    rv = PR_TRUE;
  }
  // XXX This isn't correct to use the containg block's width for a percentage
  // height. According to the CSS2 spec: "The percentage is calculated with respect
  // to the height of the generated box's containing block. If the height of the
  // containing block is not specified explicitly (i.e., it depends on content
  // height), the value is interpreted like 'auto'".
  else if (eStyleUnit_Percent == unit) {
    // CSS2 has specified that percentage width/height values are basd
    // on the containing block's <B>width</B>.
    // XXX need to subtract out padding, also this needs
    // to be synced with nsFrame's IsPercentageBase
    const nsReflowState* rs = &aReflowState;
    while (nsnull != rs) {
      nsIFrame* block = nsnull;
      rs->frame->QueryInterface(kBlockFrameCID, (void**) &block);
      if (nsnull != block) {
        if (NS_UNCONSTRAINEDSIZE == rs->maxSize.width) {
          // When we find an unconstrained block it means that pass1
          // table reflow is occuring. In this case the percentage
          // value is unknown so assume it's epsilon for now.
          aResult = 1;
        }
        else {
          // We found the nearest containing block which defines what a
          // percentage size is relative to. Use the width that it will
          // reflow to as the basis for computing our width.
          aResult = nscoord(rs->maxSize.width * aCoord.GetPercentValue());
        }
        rv = PR_TRUE;
        break;
      }
      aResult = 0;
      rs = rs->parentReflowState;
    }
  }
  else {
    aResult = 0;
  }

  // Negative width's are ignored
  if (aResult <= 0) {
    rv = PR_FALSE;
  }
  return rv;
}

PRIntn
nsCSSLayout::GetStyleSize(nsIPresContext* aPresContext,
                          const nsHTMLReflowState& aReflowState,
                          nsSize& aStyleSize)
{
  // XXX if display == row || rowspan ignore width
  // XXX if display == col || colspan ignore height

  PRIntn rv = NS_SIZE_HAS_NONE;
  const nsStylePosition* pos;
  nsresult  result = aReflowState.frame->GetStyleData(eStyleStruct_Position,
                                                      (const nsStyleStruct*&)pos);
  if (NS_OK == result) {
    if (GetStyleDimension(aPresContext, aReflowState, pos, pos->mWidth,
                          aStyleSize.width)) {
NS_ASSERTION(aStyleSize.width < 100000, "bad % result");
      rv |= NS_SIZE_HAS_WIDTH;
    }
    if (GetStyleDimension(aPresContext, aReflowState, pos, pos->mHeight,
                          aStyleSize.height)) {
NS_ASSERTION(aStyleSize.height < 100000, "bad % result");
      rv |= NS_SIZE_HAS_HEIGHT;
    }
  }
  return rv;
}
