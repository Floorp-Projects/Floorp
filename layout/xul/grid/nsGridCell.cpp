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

#include "nsGridCell.h"
#include "nsFrame.h"
#include "nsBox.h"
#include "nsGridLayout2.h"


nsGridCell::nsGridCell():mBoxInColumn(nullptr),mBoxInRow(nullptr)
{
    MOZ_COUNT_CTOR(nsGridCell);
}                                               
        
nsGridCell::~nsGridCell()
{
    MOZ_COUNT_DTOR(nsGridCell);
}

nsSize
nsGridCell::GetXULPrefSize(nsBoxLayoutState& aState)
{
  nsSize sum(0,0);

  // take our 2 children and add them up.
  // we are as wide as the widest child plus its left offset
  // we are tall as the tallest child plus its top offset

  if (mBoxInColumn) {
    nsSize pref = mBoxInColumn->GetXULPrefSize(aState);

    nsBox::AddMargin(mBoxInColumn, pref);
    nsGridLayout2::AddOffset(mBoxInColumn, pref);

    nsBoxLayout::AddLargestSize(sum, pref);
  }

  if (mBoxInRow) {
    nsSize pref = mBoxInRow->GetXULPrefSize(aState);

    nsBox::AddMargin(mBoxInRow, pref);
    nsGridLayout2::AddOffset(mBoxInRow, pref);

    nsBoxLayout::AddLargestSize(sum, pref);
  }

  return sum;
}

nsSize
nsGridCell::GetXULMinSize(nsBoxLayoutState& aState)
{
  nsSize sum(0, 0);

  // take our 2 children and add them up.
  // we are as wide as the widest child plus its left offset
  // we are tall as the tallest child plus its top offset

  if (mBoxInColumn) {
    nsSize min = mBoxInColumn->GetXULMinSize(aState);

    nsBox::AddMargin(mBoxInColumn, min);
    nsGridLayout2::AddOffset(mBoxInColumn, min);

    nsBoxLayout::AddLargestSize(sum, min);
  }

  if (mBoxInRow) {
    nsSize min = mBoxInRow->GetXULMinSize(aState);

    nsBox::AddMargin(mBoxInRow, min);
    nsGridLayout2::AddOffset(mBoxInRow, min);

    nsBoxLayout::AddLargestSize(sum, min);
  }

  return sum;
}

nsSize
nsGridCell::GetXULMaxSize(nsBoxLayoutState& aState)
{
  nsSize sum(NS_INTRINSICSIZE, NS_INTRINSICSIZE);

  // take our 2 children and add them up.
  // we are as wide as the smallest child plus its left offset
  // we are tall as the shortest child plus its top offset

  if (mBoxInColumn) {
    nsSize max = mBoxInColumn->GetXULMaxSize(aState);
 
    nsBox::AddMargin(mBoxInColumn, max);
    nsGridLayout2::AddOffset(mBoxInColumn, max);

    nsBoxLayout::AddSmallestSize(sum, max);
  }

  if (mBoxInRow) {
    nsSize max = mBoxInRow->GetXULMaxSize(aState);

    nsBox::AddMargin(mBoxInRow, max);
    nsGridLayout2::AddOffset(mBoxInRow, max);

    nsBoxLayout::AddSmallestSize(sum, max);
  }

  return sum;
}


bool
nsGridCell::IsXULCollapsed()
{
  return ((mBoxInColumn && mBoxInColumn->IsXULCollapsed()) ||
          (mBoxInRow && mBoxInRow->IsXULCollapsed()));
}


