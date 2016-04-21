/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ScrollBoxObject.h"
#include "mozilla/dom/ScrollBoxObjectBinding.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ToJSValue.h"
#include "nsCOMPtr.h"
#include "nsIPresShell.h"
#include "nsIContent.h"
#include "nsIDOMElement.h"
#include "nsPresContext.h"
#include "nsBox.h"
#include "nsIScrollableFrame.h"

namespace mozilla {
namespace dom {

ScrollBoxObject::ScrollBoxObject()
{
}

ScrollBoxObject::~ScrollBoxObject()
{
}

JSObject* ScrollBoxObject::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return ScrollBoxObjectBinding::Wrap(aCx, this, aGivenProto);
}

nsIScrollableFrame* ScrollBoxObject::GetScrollFrame()
{
  return do_QueryFrame(GetFrame(false));
}

void ScrollBoxObject::ScrollTo(int32_t x, int32_t y, ErrorResult& aRv)
{
  nsIScrollableFrame* sf = GetScrollFrame();
  if (!sf) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  sf->ScrollToCSSPixels(CSSIntPoint(x, y));
}

void ScrollBoxObject::ScrollBy(int32_t dx, int32_t dy, ErrorResult& aRv)
{
  CSSIntPoint pt;
  GetPosition(pt, aRv);

  if (aRv.Failed()) {
    return;
  }

  ScrollTo(pt.x + dx, pt.y + dy, aRv);
}

void ScrollBoxObject::ScrollByLine(int32_t dlines, ErrorResult& aRv)
{
  nsIScrollableFrame* sf = GetScrollFrame();
  if (!sf) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  sf->ScrollBy(nsIntPoint(0, dlines), nsIScrollableFrame::LINES,
               nsIScrollableFrame::SMOOTH);
}

// XUL <scrollbox> elements have a single box child element.
// Get a pointer to that box.
// Note that now that the <scrollbox> is just a regular box
// with 'overflow:hidden', the boxobject's frame is an nsXULScrollFrame,
// the <scrollbox>'s box frame is the scrollframe's "scrolled frame", and
// the <scrollbox>'s child box is a child of that.
static nsIFrame* GetScrolledBox(BoxObject* aScrollBox) {
  nsIFrame* frame = aScrollBox->GetFrame(false);
  if (!frame) {
    return nullptr;
  }

  nsIScrollableFrame* scrollFrame = do_QueryFrame(frame);
  if (!scrollFrame) {
    NS_WARNING("ScrollBoxObject attached to something that's not a scroll frame!");
    return nullptr;
  }

  nsIFrame* scrolledFrame = scrollFrame->GetScrolledFrame();
  if (!scrolledFrame)
    return nullptr;
  return nsBox::GetChildXULBox(scrolledFrame);
}

void ScrollBoxObject::ScrollByIndex(int32_t dindexes, ErrorResult& aRv)
{
    nsIScrollableFrame* sf = GetScrollFrame();
    if (!sf) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }

    nsIFrame* scrolledBox = GetScrolledBox(this);
    if (!scrolledBox) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }

    nsRect rect;

    // now get the scrolled boxes first child.
    nsIFrame* child = nsBox::GetChildXULBox(scrolledBox);

    bool horiz = scrolledBox->IsXULHorizontal();
    nsPoint cp = sf->GetScrollPosition();
    nscoord diff = 0;
    int32_t curIndex = 0;
    bool isLTR = scrolledBox->IsXULNormalDirection();

    int32_t frameWidth = 0;
    if (!isLTR && horiz) {
      GetWidth(&frameWidth);
      nsCOMPtr<nsIPresShell> shell = GetPresShell(false);
      if (!shell) {
        aRv.Throw(NS_ERROR_UNEXPECTED);
        return;
      }
      frameWidth = nsPresContext::CSSPixelsToAppUnits(frameWidth);
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
        // the scrollbox.
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

    if (dindexes == 0)
      return;

    if (dindexes > 0) {
      while(child) {
        child = nsBox::GetNextXULBox(child);
        if (child) {
          rect = child->GetRect();
        }
        count++;
        if (count >= dindexes) {
          break;
        }
      }

   } else if (dindexes < 0) {
      child = nsBox::GetChildXULBox(scrolledBox);
      while(child) {
        rect = child->GetRect();
        if (count >= curIndex + dindexes) {
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

void ScrollBoxObject::ScrollToLine(int32_t line, ErrorResult& aRv)
{
  nsIScrollableFrame* sf = GetScrollFrame();
  if (!sf) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nscoord y = sf->GetLineScrollAmount().height * line;
  nsRect range(0, y - nsPresContext::CSSPixelsToAppUnits(1),
               0, nsPresContext::CSSPixelsToAppUnits(1));
  sf->ScrollTo(nsPoint(0, y), nsIScrollableFrame::INSTANT, &range);
}

void ScrollBoxObject::ScrollToElement(Element& child, ErrorResult& aRv)
{
  nsCOMPtr<nsIPresShell> shell = GetPresShell(false);
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

void ScrollBoxObject::ScrollToIndex(int32_t index, ErrorResult& aRv)
{
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
}

int32_t ScrollBoxObject::GetPositionX(ErrorResult& aRv)
{
  CSSIntPoint pt;
  GetPosition(pt, aRv);
  return pt.x;
}

int32_t ScrollBoxObject::GetPositionY(ErrorResult& aRv)
{
  CSSIntPoint pt;
  GetPosition(pt, aRv);
  return pt.y;
}

int32_t ScrollBoxObject::GetScrolledWidth(ErrorResult& aRv)
{
  nsRect scrollRect;
  GetScrolledSize(scrollRect, aRv);
  return scrollRect.width;
}

int32_t ScrollBoxObject::GetScrolledHeight(ErrorResult& aRv)
{
  nsRect scrollRect;
  GetScrolledSize(scrollRect, aRv);
  return scrollRect.height;
}

/* private helper */
void ScrollBoxObject::GetPosition(CSSIntPoint& aPos, ErrorResult& aRv)
{
  nsIScrollableFrame* sf = GetScrollFrame();
  if (!sf) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  aPos = sf->GetScrollPositionCSSPixels();
}

/* private helper */
void ScrollBoxObject::GetScrolledSize(nsRect& aRect, ErrorResult& aRv)
{
    nsIFrame* scrolledBox = GetScrolledBox(this);
    if (!scrolledBox) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }

    aRect = scrolledBox->GetRect();
    aRect.width  = nsPresContext::AppUnitsToIntCSSPixels(aRect.width);
    aRect.height = nsPresContext::AppUnitsToIntCSSPixels(aRect.height);
}

void ScrollBoxObject::GetPosition(JSContext* cx,
                                  JS::Handle<JSObject*> x,
                                  JS::Handle<JSObject*> y,
                                  ErrorResult& aRv)
{
  CSSIntPoint pt;
  GetPosition(pt, aRv);
  JS::Rooted<JS::Value> v(cx);
  if (!ToJSValue(cx, pt.x, &v) ||
      !JS_SetProperty(cx, x, "value", v)) {
    aRv.Throw(NS_ERROR_XPC_CANT_SET_OUT_VAL);
    return;
  }
  if (!ToJSValue(cx, pt.y, &v) ||
      !JS_SetProperty(cx, y, "value", v)) {
    aRv.Throw(NS_ERROR_XPC_CANT_SET_OUT_VAL);
    return;
  }
}

void ScrollBoxObject::GetScrolledSize(JSContext* cx,
                                      JS::Handle<JSObject*> width,
                                      JS::Handle<JSObject*> height,
                                      ErrorResult& aRv)
{
  nsRect rect;
  GetScrolledSize(rect, aRv);
  JS::Rooted<JS::Value> v(cx);
  if (!ToJSValue(cx, rect.width, &v) ||
      !JS_SetProperty(cx, width, "value", v)) {
    aRv.Throw(NS_ERROR_XPC_CANT_SET_OUT_VAL);
    return;
  }
  if (!ToJSValue(cx, rect.height, &v) ||
      !JS_SetProperty(cx, height, "value", v)) {
    aRv.Throw(NS_ERROR_XPC_CANT_SET_OUT_VAL);
    return;
  }
}

void ScrollBoxObject::EnsureElementIsVisible(Element& child, ErrorResult& aRv)
{
    nsCOMPtr<nsIPresShell> shell = GetPresShell(false);
    if (!shell) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return;
    }

    shell->ScrollContentIntoView(&child,
                                 nsIPresShell::ScrollAxis(),
                                 nsIPresShell::ScrollAxis(),
                                 nsIPresShell::SCROLL_FIRST_ANCESTOR_ONLY |
                                 nsIPresShell::SCROLL_OVERFLOW_HIDDEN);
}

void ScrollBoxObject::EnsureIndexIsVisible(int32_t index, ErrorResult& aRv)
{
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
}

void ScrollBoxObject::EnsureLineIsVisible(int32_t line, ErrorResult& aRv)
{
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
}

} // namespace dom
} // namespace mozilla

// Creation Routine ///////////////////////////////////////////////////////////////////////

using namespace mozilla::dom;

nsresult
NS_NewScrollBoxObject(nsIBoxObject** aResult)
{
  NS_ADDREF(*aResult = new ScrollBoxObject());
  return NS_OK;
}
