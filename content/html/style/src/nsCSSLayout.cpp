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
#if 0
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
      switch (kidPosition->mOffset.GetLeftUnit()) {
      case eStyleUnit_Percent:
        printf("XXX: not yet implemented: % relative position\n");
      case eStyleUnit_Auto:
        break;
      case eStyleUnit_Coord:
        dx = kidPosition->mOffset.GetLeft().GetCoordValue();
        break;
      }
      nscoord dy = 0;
      switch (kidPosition->mOffset.GetTopUnit()) {
      case eStyleUnit_Percent:
        printf("XXX: not yet implemented: % relative position\n");
      case eStyleUnit_Auto:
        break;
      case eStyleUnit_Coord:
        dy = kidPosition->mOffset.GetTop().GetCoordValue();
        break;
      }
      kid->MoveTo(origin.x + dx, origin.y + dy);
    }
    kid->GetNextSibling(kid);
  }
}
#endif
