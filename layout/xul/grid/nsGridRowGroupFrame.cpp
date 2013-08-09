/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//
// Eric Vaughan
// Netscape Communications
//
// See documentation in associated header file
//

#include "nsGridRowGroupFrame.h"
#include "nsGridRowLeafLayout.h"
#include "nsGridRow.h"
#include "nsBoxLayoutState.h"
#include "nsGridLayout2.h"

already_AddRefed<nsBoxLayout> NS_NewGridRowGroupLayout();

nsIFrame*
NS_NewGridRowGroupFrame(nsIPresShell* aPresShell,
                        nsStyleContext* aContext)
{
  nsCOMPtr<nsBoxLayout> layout = NS_NewGridRowGroupLayout();
  return new (aPresShell) nsGridRowGroupFrame(aPresShell, aContext, layout);
}

NS_IMPL_FRAMEARENA_HELPERS(nsGridRowGroupFrame)


/**
 * This is redefined because row groups have a funny property. If they are flexible
 * then their flex must be equal to the sum of their children's flexes.
 */
nscoord
nsGridRowGroupFrame::GetFlex(nsBoxLayoutState& aState)
{
  // if we are flexible out flexibility is determined by our columns.
  // so first get the our flex. If not 0 then our flex is the sum of
  // our columns flexes.

  if (!DoesNeedRecalc(mFlex))
     return mFlex;

  if (nsBoxFrame::GetFlex(aState) == 0)
    return 0;

  // ok we are flexible add up our children
  nscoord totalFlex = 0;
  nsIFrame* child = GetChildBox();
  while (child)
  {
    totalFlex += child->GetFlex(aState);
    child = child->GetNextBox();
  }

  mFlex = totalFlex;

  return totalFlex;
}


