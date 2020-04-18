/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
#include "nsGridLayout2.h"

nsGridCell::nsGridCell() : mBoxInColumn(nullptr), mBoxInRow(nullptr) {
  MOZ_COUNT_CTOR(nsGridCell);
}

nsGridCell::~nsGridCell() { MOZ_COUNT_DTOR(nsGridCell); }

nsSize nsGridCell::GetXULPrefSize(nsBoxLayoutState& aState) {
  nsSize sum(0, 0);

  // take our 2 children and add them up.
  // we are as wide as the widest child plus its left offset
  // we are tall as the tallest child plus its top offset

  if (mBoxInColumn) {
    nsSize pref = mBoxInColumn->GetXULPrefSize(aState);

    nsIFrame::AddXULMargin(mBoxInColumn, pref);
    nsGridLayout2::AddOffset(mBoxInColumn, pref);

    nsBoxLayout::AddLargestSize(sum, pref);
  }

  if (mBoxInRow) {
    nsSize pref = mBoxInRow->GetXULPrefSize(aState);

    nsIFrame::AddXULMargin(mBoxInRow, pref);
    nsGridLayout2::AddOffset(mBoxInRow, pref);

    nsBoxLayout::AddLargestSize(sum, pref);
  }

  return sum;
}

nsSize nsGridCell::GetXULMinSize(nsBoxLayoutState& aState) {
  nsSize sum(0, 0);

  // take our 2 children and add them up.
  // we are as wide as the widest child plus its left offset
  // we are tall as the tallest child plus its top offset

  if (mBoxInColumn) {
    nsSize min = mBoxInColumn->GetXULMinSize(aState);

    nsIFrame::AddXULMargin(mBoxInColumn, min);
    nsGridLayout2::AddOffset(mBoxInColumn, min);

    nsBoxLayout::AddLargestSize(sum, min);
  }

  if (mBoxInRow) {
    nsSize min = mBoxInRow->GetXULMinSize(aState);

    nsIFrame::AddXULMargin(mBoxInRow, min);
    nsGridLayout2::AddOffset(mBoxInRow, min);

    nsBoxLayout::AddLargestSize(sum, min);
  }

  return sum;
}

nsSize nsGridCell::GetXULMaxSize(nsBoxLayoutState& aState) {
  nsSize sum(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);

  // take our 2 children and add them up.
  // we are as wide as the smallest child plus its left offset
  // we are tall as the shortest child plus its top offset

  if (mBoxInColumn) {
    nsSize max = mBoxInColumn->GetXULMaxSize(aState);

    nsIFrame::AddXULMargin(mBoxInColumn, max);
    nsGridLayout2::AddOffset(mBoxInColumn, max);

    nsBoxLayout::AddSmallestSize(sum, max);
  }

  if (mBoxInRow) {
    nsSize max = mBoxInRow->GetXULMaxSize(aState);

    nsIFrame::AddXULMargin(mBoxInRow, max);
    nsGridLayout2::AddOffset(mBoxInRow, max);

    nsBoxLayout::AddSmallestSize(sum, max);
  }

  return sum;
}

bool nsGridCell::IsXULCollapsed() {
  return ((mBoxInColumn && mBoxInColumn->IsXULCollapsed()) ||
          (mBoxInRow && mBoxInRow->IsXULCollapsed()));
}
