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

#include "nsCOMPtr.h"
#include "nsContainerFrame.h"
#include "nsBoxLayout.h"

void nsBoxLayout::AddXULBorderAndPadding(nsIFrame* aBox, nsSize& aSize) {
  nsIFrame::AddXULBorderAndPadding(aBox, aSize);
}

void nsBoxLayout::AddXULMargin(nsIFrame* aChild, nsSize& aSize) {
  nsIFrame::AddXULMargin(aChild, aSize);
}

void nsBoxLayout::AddXULMargin(nsSize& aSize, const nsMargin& aMargin) {
  nsIFrame::AddXULMargin(aSize, aMargin);
}

nsSize nsBoxLayout::GetXULPrefSize(nsIFrame* aBox,
                                   nsBoxLayoutState& aBoxLayoutState) {
  nsSize pref(0, 0);
  AddXULBorderAndPadding(aBox, pref);

  return pref;
}

nsSize nsBoxLayout::GetXULMinSize(nsIFrame* aBox,
                                  nsBoxLayoutState& aBoxLayoutState) {
  nsSize minSize(0, 0);
  AddXULBorderAndPadding(aBox, minSize);
  return minSize;
}

nsSize nsBoxLayout::GetXULMaxSize(nsIFrame* aBox,
                                  nsBoxLayoutState& aBoxLayoutState) {
  // AddXULBorderAndPadding () never changes maxSize (NS_UNCONSTRAINEDSIZE)
  // AddXULBorderAndPadding(aBox, maxSize);
  return nsSize(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
}

nscoord nsBoxLayout::GetAscent(nsIFrame* aBox,
                               nsBoxLayoutState& aBoxLayoutState) {
  return 0;
}

NS_IMETHODIMP
nsBoxLayout::XULLayout(nsIFrame* aBox, nsBoxLayoutState& aBoxLayoutState) {
  return NS_OK;
}

void nsBoxLayout::AddLargestSize(nsSize& aSize, const nsSize& aSize2) {
  if (aSize2.width > aSize.width) aSize.width = aSize2.width;

  if (aSize2.height > aSize.height) aSize.height = aSize2.height;
}

void nsBoxLayout::AddSmallestSize(nsSize& aSize, const nsSize& aSize2) {
  if (aSize2.width < aSize.width) aSize.width = aSize2.width;

  if (aSize2.height < aSize.height) aSize.height = aSize2.height;
}

NS_IMPL_ISUPPORTS(nsBoxLayout, nsBoxLayout)
