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

#include "nsGridRowLeafFrame.h"
#include "nsGridRowLeafLayout.h"
#include "nsGridRow.h"
#include "nsBoxLayoutState.h"
#include "nsGridLayout2.h"

already_AddRefed<nsBoxLayout> NS_NewGridRowLeafLayout();

nsIFrame*
NS_NewGridRowLeafFrame(nsIPresShell* aPresShell,
                       nsStyleContext* aContext)
{
  nsCOMPtr<nsBoxLayout> layout = NS_NewGridRowLeafLayout();
  return new (aPresShell) nsGridRowLeafFrame(aPresShell, aContext, false,
                                             layout);
}

NS_IMPL_FRAMEARENA_HELPERS(nsGridRowLeafFrame)

/*
 * Our border and padding could be affected by our columns or rows.
 * Let's go check it out.
 */
nsresult
nsGridRowLeafFrame::GetBorderAndPadding(nsMargin& aBorderAndPadding)
{
  // if our columns have made our padding larger add it in.
  nsresult rv = nsBoxFrame::GetBorderAndPadding(aBorderAndPadding);

  nsIGridPart* part = nsGrid::GetPartFromBox(this);
  if (!part)
    return rv;
    
  int32_t index = 0;
  nsGrid* grid = part->GetGrid(this, &index);

  if (!grid) 
    return rv;

  bool isHorizontal = IsHorizontal();

  nsBoxLayoutState state(PresContext());

  int32_t firstIndex = 0;
  int32_t lastIndex = 0;
  nsGridRow* firstRow = nullptr;
  nsGridRow* lastRow = nullptr;
  grid->GetFirstAndLastRow(state, firstIndex, lastIndex, firstRow, lastRow, isHorizontal);

  // only the first and last rows can be affected.
  if (firstRow && firstRow->GetBox() == this) {
    
    nscoord top = 0;
    nscoord bottom = 0;
    grid->GetRowOffsets(state, firstIndex, top, bottom, isHorizontal);

    if (isHorizontal) {
      if (top > aBorderAndPadding.top)
        aBorderAndPadding.top = top;
    } else {
      if (top > aBorderAndPadding.left)
        aBorderAndPadding.left = top;
    } 
  }

  if (lastRow && lastRow->GetBox() == this) {
    
    nscoord top = 0;
    nscoord bottom = 0;
    grid->GetRowOffsets(state, lastIndex, top, bottom, isHorizontal);

    if (isHorizontal) {
      if (bottom > aBorderAndPadding.bottom)
        aBorderAndPadding.bottom = bottom;
    } else {
      if (bottom > aBorderAndPadding.right)
        aBorderAndPadding.right = bottom;
    }
    
  }  
  
  return rv;
}


