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

static NS_DEFINE_IID(kStyleMoleculeSID, NS_STYLEMOLECULE_SID);

// XXX what about ebina's center vs. ncsa-center?
/*
 * Notes: It's a known fact that this doesn't do what ebina's layout
 * engine does because his code did vertical alignment on the
 * fly. Hopefully that's ok because his code generated surprising
 * results in a number of unusual cases.
 */
nscoord nsCSSLayout::VerticallyAlignChildren(nsIPresContext* aCX,
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
  PRIntn bottomAlignKids = 0;
  PRIntn kidCount = aChildCount;
  while (--kidCount >= 0) {
    nscoord kidAscent = *aAscents++;
    nsIStyleContext* kidSC;
    nsIContent* kidContent;

    kid->GetStyleContext(aCX, kidSC);
    kid->GetContent(kidContent);
    nsStyleMolecule* kidMol =
      (nsStyleMolecule*)kidSC->GetData(kStyleMoleculeSID);
    PRIntn verticalAlign = kidMol->verticalAlign;
    float verticalAlignPct = kidMol->verticalAlignPct;
    NS_RELEASE(kidSC);
    NS_RELEASE(kidContent);

    kid->GetRect(kidRect);

    // Vertically align the child
    nscoord kidYTop = 0;
    switch (verticalAlign) {
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
      bottomAlignKids++;
      break;

    case NS_STYLE_VERTICAL_ALIGN_MIDDLE:
      kidYTop = aMaxAscent - (fm->GetHeight(/*'x'XXX */) / 2) - kidRect.height/2;
      break;

    case NS_STYLE_VERTICAL_ALIGN_TEXT_BOTTOM:
      kidYTop = aMaxAscent + fm->GetMaxDescent() - kidRect.height;
      break;

    case NS_STYLE_VERTICAL_ALIGN_TEXT_TOP:
      kidYTop = aMaxAscent - fm->GetMaxAscent();
      break;

    case NS_STYLE_VERTICAL_ALIGN_LENGTH:
      // XXX css2 hasn't yet specified what this means!
      break;

    case NS_STYLE_VERTICAL_ALIGN_PCT:
      // XXX need kidMol->lineHeight in translated to a value form...
      break;
    }

    // Place kid and update min and max Y values
    nscoord y = aY0 + kidYTop;
    if (y < minY) minY = y;
    kid->MoveTo(kidRect.x, y);
    y += kidRect.height;
    if (y > maxY) maxY = y;

    kid->GetNextSibling(kid);
  }

  nscoord lineHeight = maxY - minY;

  if (0 != bottomAlignKids) {
    // Position all of the bottom aligned children
    kidCount = aChildCount;
    kid = aFirstChild;
    while (--kidCount >= 0) {
      // Get kid's vertical align style data
      nsIStyleContext* kidSC;
      nsIContent* kidContent;

      kid->GetStyleContext(aCX, kidSC);
      kid->GetContent(kidContent);
      nsStyleMolecule* kidMol =
        (nsStyleMolecule*)kidSC->GetData(kStyleMoleculeSID);
      PRIntn verticalAlign = kidMol->verticalAlign;
      NS_RELEASE(kidSC);
      NS_RELEASE(kidContent);

      // Vertically align the child
      if (verticalAlign == NS_STYLE_VERTICAL_ALIGN_BOTTOM) {
        // Place kid along the bottom
        kid->GetRect(kidRect);
        kid->MoveTo(kidRect.x, aY0 + lineHeight - kidRect.height);
        if (--bottomAlignKids == 0) {
          // Stop on lost bottom aligned kid
          break;
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
void nsCSSLayout::HorizontallyPlaceChildren(nsIPresContext* aCX,
                                            nsIFrame* aContainer,
                                            nsStyleMolecule* aContainerStyle,
                                            nsIFrame* aFirstChild,
                                            PRInt32 aChildCount,
                                            nscoord aLineWidth,
                                            nscoord aMaxWidth)
{
  PRIntn textAlign = aContainerStyle->textAlign;

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

/**
 * Apply css relative positioning to any child that requires it.
 */
void nsCSSLayout::RelativePositionChildren(nsIPresContext* aCX,
                                           nsIFrame* aContainer,
                                           nsStyleMolecule* aContainerStyle,
                                           nsIFrame* aFirstChild,
                                           PRInt32 aChildCount)
{
  nsPoint origin;
  nsIFrame* kid = aFirstChild;
  while (--aChildCount >= 0) {
    nsIContent* kidContent;
    nsIStyleContext* kidSC;

    kid->GetContent(kidContent);
    kid->GetStyleContext(aCX, kidSC);
    nsStyleMolecule* kidMol = (nsStyleMolecule*)kidSC->GetData(kStyleMoleculeSID);
    if (NS_STYLE_POSITION_RELATIVE == kidMol->positionFlags) {
      kid->GetOrigin(origin);
      nscoord dx = kidMol->left;
      nscoord dy = kidMol->top;
      kid->MoveTo(origin.x + dx, origin.y + dy);
    }
    NS_RELEASE(kidContent);
    NS_RELEASE(kidSC);
    kid->GetNextSibling(kid);
  }
}
