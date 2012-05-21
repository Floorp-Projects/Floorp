/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 
  Author:
  Eric D Vaughan

**/

#ifndef nsGridCell_h___
#define nsGridCell_h___

#include "nsIFrame.h"

class nsBoxLayoutState;
struct nsSize;

/*
 * Grid cell is what makes up the cellmap in the grid. Each GridCell contains
 * 2 pointers. One to the matching box in the columns and one to the matching box
 * in the rows. Remember that you can put content in both rows and columns.
 * When asked for preferred/min/max sizes it works like a stack and takes the 
 * biggest sizes.
 */

class nsGridCell
{
public:
    nsGridCell();
    virtual ~nsGridCell();

    nsSize      GetPrefSize(nsBoxLayoutState& aBoxLayoutState);
    nsSize      GetMinSize(nsBoxLayoutState& aBoxLayoutState);
    nsSize      GetMaxSize(nsBoxLayoutState& aBoxLayoutState);
    bool        IsCollapsed();

// accessors
    nsIBox*     GetBoxInColumn()             { return mBoxInColumn; }
    nsIBox*     GetBoxInRow()                { return mBoxInRow; }
    void        SetBoxInRow(nsIBox* aBox)    { mBoxInRow = aBox; }
    void        SetBoxInColumn(nsIBox* aBox) { mBoxInColumn = aBox; }

private:
    nsIBox* mBoxInColumn;
    nsIBox* mBoxInRow;
};

#endif

