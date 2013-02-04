/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**

  Eric D Vaughan
  A frame that can have multiple children. Only one child may be displayed at one time. So the
  can be flipped though like a deck of cards.
 
**/

#ifndef nsGridRowLeafFrame_h___
#define nsGridRowLeafFrame_h___

#include "mozilla/Attributes.h"
#include "nsBoxFrame.h"

/**
 * A frame representing a grid row (or column).  Grid row (and column)
 * elements are the children of row group (or column group) elements,
 * and their children are placed one to a cell.
 */
// XXXldb This needs a better name that indicates that it's for any grid
// row.
class nsGridRowLeafFrame : public nsBoxFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS

  friend nsIFrame* NS_NewGridRowLeafFrame(nsIPresShell* aPresShell,
                                          nsStyleContext* aContext);

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const MOZ_OVERRIDE
  {
      return MakeFrameName(NS_LITERAL_STRING("nsGridRowLeaf"), aResult);
  }
#endif

  nsGridRowLeafFrame(nsIPresShell* aPresShell,
                     nsStyleContext* aContext,
                     bool aIsRoot,
                     nsBoxLayout* aLayoutManager):
    nsBoxFrame(aPresShell, aContext, aIsRoot, aLayoutManager) {}

  NS_IMETHOD GetBorderAndPadding(nsMargin& aBorderAndPadding);

}; // class nsGridRowLeafFrame



#endif

