/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
    bool        IsCollapsed(nsBoxLayoutState& aBoxLayoutState);

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

