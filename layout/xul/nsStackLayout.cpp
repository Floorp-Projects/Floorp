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

#include "nsStackLayout.h"
#include "nsCOMPtr.h"
#include "nsBoxLayoutState.h"
#include "nsBoxFrame.h"
#include "nsGkAtoms.h"
#include "nsIContent.h"
#include "nsNameSpaceManager.h"

using namespace mozilla;

nsBoxLayout* nsStackLayout::gInstance = nullptr;

nsresult NS_NewStackLayout(nsCOMPtr<nsBoxLayout>& aNewLayout) {
  if (!nsStackLayout::gInstance) {
    nsStackLayout::gInstance = new nsStackLayout();
    NS_IF_ADDREF(nsStackLayout::gInstance);
  }
  // we have not instance variables so just return our static one.
  aNewLayout = nsStackLayout::gInstance;
  return NS_OK;
}

/*static*/
void nsStackLayout::Shutdown() { NS_IF_RELEASE(gInstance); }

nsStackLayout::nsStackLayout() = default;

/*
 * Sizing: we are as wide as the widest child
 * we are tall as the tallest child.
 */

nsSize nsStackLayout::GetXULPrefSize(nsIFrame* aBox, nsBoxLayoutState& aState) {
  nsSize prefSize(0, 0);

  nsIFrame* child = nsIFrame::GetChildXULBox(aBox);
  while (child) {
    nsSize pref = child->GetXULPrefSize(aState);

    AddXULMargin(child, pref);

    if (pref.width > prefSize.width) {
      prefSize.width = pref.width;
    }
    if (pref.height > prefSize.height) {
      prefSize.height = pref.height;
    }

    child = nsIFrame::GetNextXULBox(child);
  }

  AddXULBorderAndPadding(aBox, prefSize);

  return prefSize;
}

nsSize nsStackLayout::GetXULMinSize(nsIFrame* aBox, nsBoxLayoutState& aState) {
  nsSize minSize(0, 0);

  nsIFrame* child = nsIFrame::GetChildXULBox(aBox);
  while (child) {
    nsSize min = child->GetXULMinSize(aState);

    AddXULMargin(child, min);

    if (min.width > minSize.width) {
      minSize.width = min.width;
    }
    if (min.height > minSize.height) {
      minSize.height = min.height;
    }

    child = nsIFrame::GetNextXULBox(child);
  }

  AddXULBorderAndPadding(aBox, minSize);

  return minSize;
}

nsSize nsStackLayout::GetXULMaxSize(nsIFrame* aBox, nsBoxLayoutState& aState) {
  nsSize maxSize(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);

  nsIFrame* child = nsIFrame::GetChildXULBox(aBox);
  while (child) {
    nsSize min = child->GetXULMinSize(aState);
    nsSize max = child->GetXULMaxSize(aState);

    max = nsIFrame::XULBoundsCheckMinMax(min, max);

    AddXULMargin(child, max);

    if (max.width < maxSize.width) {
      maxSize.width = max.width;
    }
    if (max.height < maxSize.height) {
      maxSize.height = max.height;
    }

    child = nsIFrame::GetNextXULBox(child);
  }

  AddXULBorderAndPadding(aBox, maxSize);

  return maxSize;
}

nscoord nsStackLayout::GetAscent(nsIFrame* aBox, nsBoxLayoutState& aState) {
  nscoord vAscent = 0;

  nsIFrame* child = nsIFrame::GetChildXULBox(aBox);
  while (child) {
    nscoord ascent = child->GetXULBoxAscent(aState);
    nsMargin margin;
    child->GetXULMargin(margin);
    ascent += margin.top;
    if (ascent > vAscent) vAscent = ascent;

    child = nsIFrame::GetNextXULBox(child);
  }

  return vAscent;
}

NS_IMETHODIMP
nsStackLayout::XULLayout(nsIFrame* aBox, nsBoxLayoutState& aState) {
  nsRect clientRect;
  aBox->GetXULClientRect(clientRect);

  bool grow;

  do {
    nsIFrame* child = nsIFrame::GetChildXULBox(aBox);
    grow = false;

    while (child) {
      nsMargin margin;
      child->GetXULMargin(margin);
      nsRect childRect(clientRect);
      childRect.Deflate(margin);

      if (childRect.width < 0) childRect.width = 0;

      if (childRect.height < 0) childRect.height = 0;

      nsRect oldRect(child->GetRect());
      bool sizeChanged = !oldRect.IsEqualEdges(childRect);

      // only lay out dirty children or children whose sizes have changed
      if (sizeChanged || child->IsSubtreeDirty()) {
        // add in the child's margin
        nsMargin margin;
        child->GetXULMargin(margin);

        // Now place the child.
        child->SetXULBounds(aState, childRect);

        // Flow the child.
        child->XULLayout(aState);

        // Get the child's new rect.
        childRect = child->GetRect();
        childRect.Inflate(margin);

        // Did the child push back on us and get bigger?
        if (childRect.width > clientRect.width) {
          clientRect.width = childRect.width;
          grow = true;
        }

        if (childRect.height > clientRect.height) {
          clientRect.height = childRect.height;
          grow = true;
        }
      }

      child = nsIFrame::GetNextXULBox(child);
    }
  } while (grow);

  // if some HTML inside us got bigger we need to force ourselves to
  // get bigger
  nsRect bounds(aBox->GetRect());
  nsMargin bp;
  aBox->GetXULBorderAndPadding(bp);
  clientRect.Inflate(bp);

  if (clientRect.width > bounds.width || clientRect.height > bounds.height) {
    if (clientRect.width > bounds.width) bounds.width = clientRect.width;
    if (clientRect.height > bounds.height) bounds.height = clientRect.height;

    aBox->SetXULBounds(aState, bounds);
  }

  return NS_OK;
}
