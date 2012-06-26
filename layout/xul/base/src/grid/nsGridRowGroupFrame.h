/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**

  Eric D Vaughan
  A frame that can have multiple children. Only one child may be displayed at one time. So the
  can be flipped though like a deck of cards.
 
**/

#ifndef nsGridRowGroupFrame_h___
#define nsGridRowGroupFrame_h___

#include "nsBoxFrame.h"

/**
 * A frame representing a grid row (or column) group, which is usually
 * an element that is a child of a grid and contains all the rows (or
 * all the columns).  However, multiple levels of groups are allowed, so
 * the parent or child could instead be another group.
 */
class nsGridRowGroupFrame : public nsBoxFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
      return MakeFrameName(NS_LITERAL_STRING("nsGridRowGroup"), aResult);
  }
#endif

  nsGridRowGroupFrame(nsIPresShell* aPresShell,
                      nsStyleContext* aContext,
                      nsBoxLayout* aLayoutManager):
    nsBoxFrame(aPresShell, aContext, false, aLayoutManager) {}

  virtual nscoord GetFlex(nsBoxLayoutState& aBoxLayoutState);

}; // class nsGridRowGroupFrame



#endif

