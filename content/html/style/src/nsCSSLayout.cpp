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
#include "nsIFontMetrics.h"
#include "nsIPresContext.h"
#include "nsRect.h"
#include "nsIPtr.h"
#include "nsHTMLIIDs.h"

NS_DEF_PTR(nsIStyleContext);
NS_DEF_PTR(nsIContent);

// XXX what about ebina's center vs. ncsa-center?
/*
 * Notes: It's a known fact that this doesn't do what ebina's layout
 * engine does because his code did vertical alignment on the
 * fly. Hopefully that's ok because his code generated surprising
 * results in a number of unusual cases.
 */
nscoord
nsCSSLayout::VerticallyAlignChildren(nsIPresContext* aCX,
                                     nsIFrame* aContainer,
                                     const nsStyleFont* aContainerFont,
                                     nscoord aY0,
                                     nsIFrame* aFirstChild,
                                     PRIntn aChildCount,
                                     nscoord* aAscents,
                                     nscoord aMaxAscent)
{
  // Determine minimum and maximum y values for the line and
  // perform alignment of all children except those requesting bottom
  // alignment. The second pass will align bottom children (if any)
  nsIFontMetrics* fm = aCX->GetMetricsFor(aContainerFont->mFont);
  nsIFrame* kid = aFirstChild;
  nsRect kidRect;
  nscoord minY = aY0;
  nscoord maxY = aY0;
  PRIntn pass2Kids = 0;
  PRIntn kidCount = aChildCount;
  while (--kidCount >= 0) {
    nscoord kidAscent = *aAscents++;

    const nsStyleText* textStyle;
    kid->GetStyleData(eStyleStruct_Text, (const nsStyleStruct*&)textStyle);
    nsStyleUnit verticalAlignUnit = textStyle->mVerticalAlign.GetUnit();
    PRUint8 verticalAlignEnum = NS_STYLE_VERTICAL_ALIGN_BASELINE;

    kid->GetRect(kidRect);

    // Vertically align the child
    nscoord kidYTop = 0;

    PRBool isPass2Kid = PR_FALSE;
    switch (verticalAlignUnit) {
      case eStyleUnit_Coord:
        kidYTop = aMaxAscent + textStyle->mVerticalAlign.GetCoordValue();
        break;

      case eStyleUnit_Percent:
        pass2Kids++;
        isPass2Kid = PR_TRUE;
        break;

      case eStyleUnit_Enumerated:
        verticalAlignEnum = textStyle->mVerticalAlign.GetIntValue();
        switch (verticalAlignEnum) {
          default:
          case NS_STYLE_VERTICAL_ALIGN_BASELINE:
            // Align the kid's baseline at the max baseline
            kidYTop = aMaxAscent - kidAscent;
            break;

          case NS_STYLE_VERTICAL_ALIGN_TOP:
            // Align the top of the kid with the top of the line box
            break;

          case NS_STYLE_VERTICAL_ALIGN_SUB:
            // Move baseline by 1/2 the ascent of the child.
            // NOTE: CSSx doesn't seem to specify what subscripting does
            // so we are using ebina's logic
            kidYTop = aMaxAscent + (kidAscent/2) - kidAscent;
            break;

          case NS_STYLE_VERTICAL_ALIGN_SUPER:
            // Move baseline by 1/2 the ascent of the child
            // NOTE: CSSx doesn't seem to specify what superscripting does
            // so we are using ebina's logic
            kidYTop = aMaxAscent - (kidAscent/2) - kidAscent;
            break;

          case NS_STYLE_VERTICAL_ALIGN_BOTTOM:
            pass2Kids++;
            isPass2Kid = PR_TRUE;
            break;

          case NS_STYLE_VERTICAL_ALIGN_MIDDLE:
            // XXX spec says use the 'x' height but our font api
            // doesn't give us that information.
            kidYTop = aMaxAscent - (fm->GetHeight() / 2) - kidRect.height/2;
            break;

          case NS_STYLE_VERTICAL_ALIGN_TEXT_BOTTOM:
            kidYTop = aMaxAscent + fm->GetMaxDescent() - kidRect.height;
            break;

          case NS_STYLE_VERTICAL_ALIGN_TEXT_TOP:
            kidYTop = aMaxAscent - fm->GetMaxAscent();
            break;

        }
        break;
      
      default:
        // Align the kid's baseline at the max baseline
        kidYTop = aMaxAscent - kidAscent;
        break;
    }

    // Place kid and update min and max Y values
    if (!isPass2Kid) {
      nscoord y = aY0 + kidYTop;
      if (y < minY) minY = y;
      kid->MoveTo(kidRect.x, y);
      y += kidRect.height;
      if (y > maxY) maxY = y;
    }
    else {
      nscoord y = aY0 + kidRect.height;
      if (y > maxY) maxY = y;
    }

    kid->GetNextSibling(kid);
  }

  // Now compute the final line-height
  nscoord lineHeight = maxY - minY;

  if (0 != pass2Kids) {
    // Position all of the bottom aligned children
    kidCount = aChildCount;
    kid = aFirstChild;
    while (--kidCount >= 0) {
      // Get kid's vertical align style data
      const nsStyleText* textStyle;
      kid->GetStyleData(eStyleStruct_Text, (const nsStyleStruct*&)textStyle);
      nsStyleUnit verticalAlignUnit = textStyle->mVerticalAlign.GetUnit();

      if (eStyleUnit_Percent == verticalAlignUnit) {
        nscoord kidYTop = aMaxAscent +
          nscoord(textStyle->mVerticalAlign.GetPercentValue() * lineHeight);
        kid->GetRect(kidRect);
        kid->MoveTo(kidRect.x, aY0 + kidYTop);
        if (--pass2Kids == 0) {
          // Stop on last pass2 kid
          break;
        }
      }
      else if (verticalAlignUnit == eStyleUnit_Enumerated) {
        PRUint8 verticalAlignEnum = textStyle->mVerticalAlign.GetIntValue();
        // Vertically align the child
        if (NS_STYLE_VERTICAL_ALIGN_BOTTOM == verticalAlignEnum) {
          // Place kid along the bottom
          kid->GetRect(kidRect);
          kid->MoveTo(kidRect.x, aY0 + lineHeight - kidRect.height);
          if (--pass2Kids == 0) {
            // Stop on last pass2 kid
            break;
          }
        }
      }

      kid->GetNextSibling(kid);
    }
  }

  NS_RELEASE(fm);
  return lineHeight;
}

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
    kid->GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)kidPosition);
    if (NS_STYLE_POSITION_RELATIVE == kidPosition->mPosition) {
      kid->GetOrigin(origin);
      // XXX Check the unit: could be auto or percent (not just length)
      nscoord dx = kidPosition->mLeftOffset.GetCoordValue();
      nscoord dy = kidPosition->mTopOffset.GetCoordValue();
      kid->MoveTo(origin.x + dx, origin.y + dy);
    }
    kid->GetNextSibling(kid);
  }
}

// XXX if this can handle proportional widths then do so
// XXX check against other possible values and update
static PRBool
GetStyleDimension(nsIPresContext* aPresContext,
                  const nsReflowState& aReflowState,
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
        // We found the nearest containing block which defines what a
        // percentage size is relative to. Use the width that it will
        // reflow to as the basis for computing our width.
        aResult = nscoord(rs->maxSize.width * aCoord.GetPercentValue());
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
                          const nsReflowState& aReflowState,
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
