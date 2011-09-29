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

//
// Eric Vaughan
// Netscape Communications
//
// See documentation in associated header file
//

#include "nsGridCell.h"
#include "nsFrame.h"
#include "nsBox.h"
#include "nsGridLayout2.h"


nsGridCell::nsGridCell():mBoxInColumn(nsnull),mBoxInRow(nsnull)
{
    MOZ_COUNT_CTOR(nsGridCell);
}                                               
        
nsGridCell::~nsGridCell()
{
    MOZ_COUNT_DTOR(nsGridCell);
}

nsSize
nsGridCell::GetPrefSize(nsBoxLayoutState& aState)
{
  nsSize sum(0,0);

  // take our 2 children and add them up.
  // we are as wide as the widest child plus its left offset
  // we are tall as the tallest child plus its top offset

  if (mBoxInColumn) {
    nsSize pref = mBoxInColumn->GetPrefSize(aState);

    nsBox::AddMargin(mBoxInColumn, pref);
    nsGridLayout2::AddOffset(aState, mBoxInColumn, pref);

    nsBoxLayout::AddLargestSize(sum, pref);
  }

  if (mBoxInRow) {
    nsSize pref = mBoxInRow->GetPrefSize(aState);

    nsBox::AddMargin(mBoxInRow, pref);
    nsGridLayout2::AddOffset(aState, mBoxInRow, pref);

    nsBoxLayout::AddLargestSize(sum, pref);
  }

  return sum;
}

nsSize
nsGridCell::GetMinSize(nsBoxLayoutState& aState)
{
  nsSize sum(0, 0);

  // take our 2 children and add them up.
  // we are as wide as the widest child plus its left offset
  // we are tall as the tallest child plus its top offset

  if (mBoxInColumn) {
    nsSize min = mBoxInColumn->GetMinSize(aState);

    nsBox::AddMargin(mBoxInColumn, min);
    nsGridLayout2::AddOffset(aState, mBoxInColumn, min);

    nsBoxLayout::AddLargestSize(sum, min);
  }

  if (mBoxInRow) {
    nsSize min = mBoxInRow->GetMinSize(aState);

    nsBox::AddMargin(mBoxInRow, min);
    nsGridLayout2::AddOffset(aState, mBoxInRow, min);

    nsBoxLayout::AddLargestSize(sum, min);
  }

  return sum;
}

nsSize
nsGridCell::GetMaxSize(nsBoxLayoutState& aState)
{
  nsSize sum(NS_INTRINSICSIZE, NS_INTRINSICSIZE);

  // take our 2 children and add them up.
  // we are as wide as the smallest child plus its left offset
  // we are tall as the shortest child plus its top offset

  if (mBoxInColumn) {
    nsSize max = mBoxInColumn->GetMaxSize(aState);
 
    nsBox::AddMargin(mBoxInColumn, max);
    nsGridLayout2::AddOffset(aState, mBoxInColumn, max);

    nsBoxLayout::AddSmallestSize(sum, max);
  }

  if (mBoxInRow) {
    nsSize max = mBoxInRow->GetMaxSize(aState);

    nsBox::AddMargin(mBoxInRow, max);
    nsGridLayout2::AddOffset(aState, mBoxInRow, max);

    nsBoxLayout::AddSmallestSize(sum, max);
  }

  return sum;
}


bool
nsGridCell::IsCollapsed(nsBoxLayoutState& aState)
{
  return ((mBoxInColumn && mBoxInColumn->IsCollapsed(aState)) ||
          (mBoxInRow && mBoxInRow->IsCollapsed(aState)));
}


