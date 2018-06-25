/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Element.h"
#include "mozilla/dom/ToJSValue.h"
#include "nsCOMPtr.h"
#include "nsIPresShell.h"
#include "nsIContent.h"
#include "nsPresContext.h"
#include "nsBox.h"
#include "nsIScrollableFrame.h"
#include "mozilla/dom/XULScrollElement.h"
#include "mozilla/dom/XULScrollElementBinding.h"

namespace mozilla {
namespace dom {

JSObject*
XULScrollElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return XULScrollElement_Binding::Wrap(aCx, this, aGivenProto);
}

void
XULScrollElement::ScrollByIndex(int32_t aIndex, ErrorResult& aRv)
{
  nsIScrollableFrame* sf = GetScrollFrame();
  if (!sf) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsIFrame* scrolledFrame = sf->GetScrolledFrame();
  if (!scrolledFrame){
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsIFrame* scrolledBox = nsBox::GetChildXULBox(scrolledFrame);
  if (!scrolledBox){
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsRect rect;

  // now get the element's first child
  nsIFrame* child = nsBox::GetChildXULBox(scrolledBox);

  bool horiz = scrolledBox->IsXULHorizontal();
  nsPoint cp = sf->GetScrollPosition();
  nscoord diff = 0;
  int32_t curIndex = 0;
  bool isLTR = scrolledBox->IsXULNormalDirection();

  nscoord frameWidth = 0;
  if (!isLTR && horiz) {
    nsCOMPtr<nsIDocument> doc = GetComposedDoc();

    nsCOMPtr<nsIPresShell> shell = doc->GetShell();
    if (!shell) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return;
    }
    nsRect rcFrame = nsLayoutUtils::GetAllInFlowRectsUnion(GetPrimaryFrame(), shell->GetRootFrame());
    frameWidth = rcFrame.width;
  }

  // first find out what index we are currently at
  while(child) {
    rect = child->GetRect();
    if (horiz) {
      // In the left-to-right case we break from the loop when the center of
      // the current child rect is greater than the scrolled position of
      // the left edge of the scrollbox
      // In the right-to-left case we break when the center of the current
      // child rect is less than the scrolled position of the right edge of
      // the scrollelement.
      diff = rect.x + rect.width/2; // use the center, to avoid rounding errors
      if ((isLTR && diff > cp.x) ||
          (!isLTR && diff < cp.x + frameWidth)) {
        break;
      }
    } else {
      diff = rect.y + rect.height/2;// use the center, to avoid rounding errors
      if (diff > cp.y) {
        break;
      }
    }
    child = nsBox::GetNextXULBox(child);
    curIndex++;
  }

  int32_t count = 0;

  if (aIndex == 0)
    return;

  if (aIndex > 0) {
    while(child) {
      child = nsBox::GetNextXULBox(child);
      if (child) {
        rect = child->GetRect();
      }
      count++;
      if (count >= aIndex) {
        break;
      }
    }

  } else if (aIndex < 0) {
    child = nsBox::GetChildXULBox(scrolledBox);
    while(child) {
      rect = child->GetRect();
      if (count >= curIndex + aIndex) {
        break;
      }
      count++;
      child = nsBox::GetNextXULBox(child);

    }
  }

  nscoord csspixel = nsPresContext::CSSPixelsToAppUnits(1);
  if (horiz) {
    // In the left-to-right case we scroll so that the left edge of the
    // selected child is scrolled to the left edge of the scrollbox.
    // In the right-to-left case we scroll so that the right edge of the
    // selected child is scrolled to the right edge of the scrollbox.

    nsPoint pt(isLTR ? rect.x : rect.x + rect.width - frameWidth,
               cp.y);

    // Use a destination range that ensures the left edge (or right edge,
    // for RTL) will indeed be visible. Also ensure that the top edge
    // is visible.
    nsRect range(pt.x, pt.y, csspixel, 0);
    if (isLTR) {
      range.x -= csspixel;
    }
    sf->ScrollTo(pt, nsIScrollableFrame::INSTANT, &range);
  } else {
    // Use a destination range that ensures the top edge will be visible.
    nsRect range(cp.x, rect.y - csspixel, 0, csspixel);
    sf->ScrollTo(nsPoint(cp.x, rect.y), nsIScrollableFrame::INSTANT, &range);
  }
}

void
XULScrollElement::ScrollToElement(Element& child, ErrorResult& aRv)
{
  nsCOMPtr<nsIDocument> doc = GetComposedDoc();
  if (!doc){
    aRv.Throw(NS_ERROR_FAILURE);
  }

  nsCOMPtr<nsIPresShell> shell = doc->GetShell();
  if (!shell) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  shell->ScrollContentIntoView(&child,
                               nsIPresShell::ScrollAxis(
                                 nsIPresShell::SCROLL_TOP,
                                 nsIPresShell::SCROLL_ALWAYS),
                               nsIPresShell::ScrollAxis(
                                 nsIPresShell::SCROLL_LEFT,
                                 nsIPresShell::SCROLL_ALWAYS),
                               nsIPresShell::SCROLL_FIRST_ANCESTOR_ONLY |
                               nsIPresShell::SCROLL_OVERFLOW_HIDDEN);
}


void
XULScrollElement::EnsureElementIsVisible(Element& aChild, ErrorResult& aRv)
{
  nsCOMPtr<nsIDocument> doc = GetComposedDoc();
  if (!doc){
    aRv.Throw(NS_ERROR_FAILURE);
  }

  nsCOMPtr<nsIPresShell> shell = doc->GetShell();
  if (!shell) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  shell->ScrollContentIntoView(&aChild,
                               nsIPresShell::ScrollAxis(),
                               nsIPresShell::ScrollAxis(),
                               nsIPresShell::SCROLL_FIRST_ANCESTOR_ONLY |
                               nsIPresShell::SCROLL_OVERFLOW_HIDDEN);
}

} // namespace dom
} // namespace mozilla
