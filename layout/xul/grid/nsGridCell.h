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

#include "mozilla/Attributes.h"

class nsBoxLayoutState;
struct nsSize;
class nsIFrame;

/*
 * Grid cell is what makes up the cellmap in the grid. Each GridCell contains
 * 2 pointers. One to the matching box in the columns and one to the matching box
 * in the rows. Remember that you can put content in both rows and columns.
 * When asked for preferred/min/max sizes it works like a stack and takes the
 * biggest sizes.
 */

class nsGridCell final
{
public:
    nsGridCell();
    ~nsGridCell();

    nsSize      GetXULPrefSize(nsBoxLayoutState& aBoxLayoutState);
    nsSize      GetXULMinSize(nsBoxLayoutState& aBoxLayoutState);
    nsSize      GetXULMaxSize(nsBoxLayoutState& aBoxLayoutState);
    bool        IsXULCollapsed();

// accessors
    nsIFrame*   GetBoxInColumn()               { return mBoxInColumn; }
    nsIFrame*   GetBoxInRow()                  { return mBoxInRow; }
    void        SetBoxInRow(nsIFrame* aBox)    { mBoxInRow = aBox; }
    void        SetBoxInColumn(nsIFrame* aBox) { mBoxInColumn = aBox; }

private:
    nsIFrame* mBoxInColumn;
    nsIFrame* mBoxInRow;
};

#endif

