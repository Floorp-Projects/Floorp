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

#include "nsBox.h"
#include "nsCOMPtr.h"
#include "nsContainerFrame.h"
#include "nsBoxLayout.h"

void
nsBoxLayout::AddBorderAndPadding(nsIFrame* aBox, nsSize& aSize)
{
  nsBox::AddBorderAndPadding(aBox, aSize);
}

void
nsBoxLayout::AddMargin(nsIFrame* aBox, nsSize& aSize)
{
  nsBox::AddMargin(aBox, aSize);
}

void
nsBoxLayout::AddMargin(nsSize& aSize, const nsMargin& aMargin)
{
  nsBox::AddMargin(aSize, aMargin);
}

nsSize
nsBoxLayout::GetPrefSize(nsIFrame* aBox, nsBoxLayoutState& aBoxLayoutState)
{
  nsSize pref (0, 0);
  AddBorderAndPadding(aBox, pref);

  return pref;
}

nsSize
nsBoxLayout::GetMinSize(nsIFrame* aBox, nsBoxLayoutState& aBoxLayoutState)
{
  nsSize minSize (0,0);
  AddBorderAndPadding(aBox, minSize);
  return minSize;
}

nsSize
nsBoxLayout::GetMaxSize(nsIFrame* aBox, nsBoxLayoutState& aBoxLayoutState)
{
  //AddBorderAndPadding () never changes maxSize (NS_INTRINSICSIZE)
  //AddBorderAndPadding(aBox, maxSize);
  return nsSize (NS_INTRINSICSIZE,NS_INTRINSICSIZE);
}


nscoord
nsBoxLayout::GetAscent(nsIFrame* aBox, nsBoxLayoutState& aBoxLayoutState)
{
  return 0;
}

NS_IMETHODIMP
nsBoxLayout::Layout(nsIFrame* aBox, nsBoxLayoutState& aBoxLayoutState)
{
  return NS_OK;
}

void
nsBoxLayout::AddLargestSize(nsSize& aSize, const nsSize& aSize2)
{
  if (aSize2.width > aSize.width)
     aSize.width = aSize2.width;

  if (aSize2.height > aSize.height)
     aSize.height = aSize2.height;
}

void
nsBoxLayout::AddSmallestSize(nsSize& aSize, const nsSize& aSize2)
{
  if (aSize2.width < aSize.width)
     aSize.width = aSize2.width;

  if (aSize2.height < aSize.height)
     aSize.height = aSize2.height;
}

NS_IMPL_ISUPPORTS1(nsBoxLayout, nsBoxLayout)
