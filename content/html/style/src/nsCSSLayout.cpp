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
#include "nsBlockFrame.h"
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
                                     nsStyleFont* aContainerFont,
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
    nsIStyleContextPtr kidSC;

    kid->GetStyleContext(aCX, kidSC.AssignRef());
    nsStyleText* textStyle = (nsStyleText*)kidSC->GetData(eStyleStruct_Text);
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
      nsIStyleContextPtr kidSC;

      kid->GetStyleContext(aCX, kidSC.AssignRef());
      nsStyleText* textStyle = (nsStyleText*)kidSC->GetData(eStyleStruct_Text);
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
                                       nsStyleText* aContainerStyle,
                                       nsIFrame* aFirstChild,
                                       PRInt32 aChildCount,
                                       nscoord aLineWidth,
                                       nscoord aMaxWidth)
{
  if (aLineWidth < aMaxWidth) {
    PRIntn textAlign = aContainerStyle->mTextAlign;
    nscoord dx = 0;
    switch (textAlign) {
    case NS_STYLE_TEXT_ALIGN_LEFT:
    case NS_STYLE_TEXT_ALIGN_JUSTIFY:
      // Default layout has everything aligned left
      return;

    case NS_STYLE_TEXT_ALIGN_RIGHT:
      dx = aMaxWidth - aLineWidth;
      break;

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
    nsIStyleContextPtr kidSC;

    kid->GetStyleContext(aCX, kidSC.AssignRef());
    nsStylePosition* kidPosition = (nsStylePosition*)
      kidSC->GetData(eStyleStruct_Position);
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
                  nsIFrame* aFrame,
                  nsStylePosition* aStylePos,
                  nsStyleCoord& aCoord,
                  nscoord& aResult)
{
  nsIFrame* parentFrame;
  PRBool rv = PR_FALSE;

  switch (aCoord.GetUnit()) {
  case eStyleUnit_Coord:
    aResult = aCoord.GetCoordValue();
    rv = PR_TRUE;
    break;

  case eStyleUnit_Percent:
    // CSS2 has specified that percentage width/height values are basd
    // on the containing block's <B>width</B>.
    // XXX need to subtract out padding, also this needs
    // to be synced with nsFrame's IsPercentageBase
    aFrame->GetContentParent(parentFrame);
    while (nsnull != parentFrame) {
      nsBlockFrame* block;
      nsresult status =
        parentFrame->QueryInterface(kBlockFrameCID, (void**) &block);
      if (NS_OK == status) {
        nsRect blockRect;
        block->GetRect(blockRect);
        aResult = nscoord(blockRect.width * aCoord.GetPercentValue());
        rv = PR_TRUE;
        break;
      }
    }
    break;
  default:
    aResult = 0;
  }
  if (aResult < 0) {
    rv = PR_FALSE;
  }

  return rv;
}

PRIntn
nsCSSLayout::GetStyleSize(nsIPresContext* aPresContext,
                          nsIFrame* aFrame,
                          nsSize& aStyleSize)
{
  // XXX if display == row || rowspan ignore width
  // XXX if display == col || colspan ignore height

  PRIntn rv = NS_SIZE_HAS_NONE;
  nsIStyleContext* sc = nsnull;
  aFrame->GetStyleContext(aPresContext, sc);
  if (nsnull != sc) {
    nsStylePosition* pos = (nsStylePosition*) sc->GetData(eStyleStruct_Position);
    if (GetStyleDimension(aPresContext, aFrame, pos, pos->mWidth,
                          aStyleSize.width)) {
      rv |= NS_SIZE_HAS_WIDTH;
    }
    if (GetStyleDimension(aPresContext, aFrame, pos, pos->mHeight,
                          aStyleSize.height)) {
      rv |= NS_SIZE_HAS_HEIGHT;
    }
    NS_RELEASE(sc);
  }
  return rv;
}
