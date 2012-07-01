/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsIScrollBoxObject.h"
#include "nsBoxObject.h"
#include "nsIPresShell.h"
#include "nsIContent.h"
#include "nsIDOMElement.h"
#include "nsPresContext.h"
#include "nsIFrame.h"
#include "nsIScrollableFrame.h"

class nsScrollBoxObject : public nsIScrollBoxObject, public nsBoxObject
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSISCROLLBOXOBJECT

  nsScrollBoxObject();
  virtual ~nsScrollBoxObject();

  virtual nsIScrollableFrame* GetScrollFrame() {
    return do_QueryFrame(GetFrame(false));
  }

  /* additional members */
};

/* Implementation file */

NS_INTERFACE_MAP_BEGIN(nsScrollBoxObject)
  NS_INTERFACE_MAP_ENTRY(nsIScrollBoxObject)
NS_INTERFACE_MAP_END_INHERITING(nsBoxObject)

NS_IMPL_ADDREF_INHERITED(nsScrollBoxObject, nsBoxObject)
NS_IMPL_RELEASE_INHERITED(nsScrollBoxObject, nsBoxObject)

nsScrollBoxObject::nsScrollBoxObject()
{
  /* member initializers and constructor code */
}

nsScrollBoxObject::~nsScrollBoxObject()
{
  /* destructor code */
}

/* void scrollTo (in long x, in long y); */
NS_IMETHODIMP nsScrollBoxObject::ScrollTo(PRInt32 x, PRInt32 y)
{
  nsIScrollableFrame* sf = GetScrollFrame();
  if (!sf)
    return NS_ERROR_FAILURE;
  sf->ScrollToCSSPixels(nsIntPoint(x, y));
  return NS_OK;
}

/* void scrollBy (in long dx, in long dy); */
NS_IMETHODIMP nsScrollBoxObject::ScrollBy(PRInt32 dx, PRInt32 dy)
{
  PRInt32 x, y;
  nsresult rv = GetPosition(&x, &y);
  if (NS_FAILED(rv))
    return rv;

  return ScrollTo(x + dx, y + dy);
}

/* void scrollByLine (in long dlines); */
NS_IMETHODIMP nsScrollBoxObject::ScrollByLine(PRInt32 dlines)
{
  nsIScrollableFrame* sf = GetScrollFrame();
  if (!sf)
    return NS_ERROR_FAILURE;

  sf->ScrollBy(nsIntPoint(0, dlines), nsIScrollableFrame::LINES,
               nsIScrollableFrame::SMOOTH);
  return NS_OK;
}

// XUL <scrollbox> elements have a single box child element.
// Get a pointer to that box.
// Note that now that the <scrollbox> is just a regular box
// with 'overflow:hidden', the boxobject's frame is an nsXULScrollFrame,
// the <scrollbox>'s box frame is the scrollframe's "scrolled frame", and
// the <scrollbox>'s child box is a child of that.
static nsIFrame* GetScrolledBox(nsBoxObject* aScrollBox) {
  nsIFrame* frame = aScrollBox->GetFrame(false);
  if (!frame) 
    return nsnull;
  nsIScrollableFrame* scrollFrame = do_QueryFrame(frame);
  if (!scrollFrame) {
    NS_WARNING("nsIScrollBoxObject attached to something that's not a scroll frame!");
    return nsnull;
  }
  nsIFrame* scrolledFrame = scrollFrame->GetScrolledFrame();
  if (!scrolledFrame)
    return nsnull;
  return scrolledFrame->GetChildBox();
}

/* void scrollByIndex (in long dindexes); */
NS_IMETHODIMP nsScrollBoxObject::ScrollByIndex(PRInt32 dindexes)
{
    nsIScrollableFrame* sf = GetScrollFrame();
    if (!sf)
       return NS_ERROR_FAILURE;
    nsIFrame* scrolledBox = GetScrolledBox(this);
    if (!scrolledBox)
       return NS_ERROR_FAILURE;

    nsRect rect;

    // now get the scrolled boxes first child.
    nsIFrame* child = scrolledBox->GetChildBox();

    bool horiz = scrolledBox->IsHorizontal();
    nsPoint cp = sf->GetScrollPosition();
    nscoord diff = 0;
    PRInt32 curIndex = 0;
    bool isLTR = scrolledBox->IsNormalDirection();

    PRInt32 frameWidth = 0;
    if (!isLTR && horiz) {
      GetWidth(&frameWidth);
      nsCOMPtr<nsIPresShell> shell = GetPresShell(false);
      if (!shell) {
        return NS_ERROR_UNEXPECTED;
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
      child = child->GetNextBox();
      curIndex++;
    }

    PRInt32 count = 0;

    if (dindexes == 0)
       return NS_OK;

    if (dindexes > 0) {
      while(child) {
        child = child->GetNextBox();
        if (child)
          rect = child->GetRect();
        count++;
        if (count >= dindexes)
          break;
      }

   } else if (dindexes < 0) {
      child = scrolledBox->GetChildBox();
      while(child) {
        rect = child->GetRect();
        if (count >= curIndex + dindexes)
          break;

        count++;
        child = child->GetNextBox();

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

   return NS_OK;
}

/* void scrollToLine (in long line); */
NS_IMETHODIMP nsScrollBoxObject::ScrollToLine(PRInt32 line)
{
  nsIScrollableFrame* sf = GetScrollFrame();
  if (!sf)
     return NS_ERROR_FAILURE;
  
  nscoord y = sf->GetLineScrollAmount().height * line;
  nsRect range(0, y - nsPresContext::CSSPixelsToAppUnits(1),
               0, nsPresContext::CSSPixelsToAppUnits(1));
  sf->ScrollTo(nsPoint(0, y), nsIScrollableFrame::INSTANT, &range);
  return NS_OK;
}

/* void scrollToElement (in nsIDOMElement child); */
NS_IMETHODIMP nsScrollBoxObject::ScrollToElement(nsIDOMElement *child)
{
    NS_ENSURE_ARG_POINTER(child);

    nsCOMPtr<nsIPresShell> shell = GetPresShell(false);
    if (!shell) {
      return NS_ERROR_UNEXPECTED;
    }

    nsCOMPtr<nsIContent> content = do_QueryInterface(child);
    shell->ScrollContentIntoView(content,
                                 nsIPresShell::ScrollAxis(
                                   nsIPresShell::SCROLL_TOP,
                                   nsIPresShell::SCROLL_ALWAYS),
                                 nsIPresShell::ScrollAxis(
                                   nsIPresShell::SCROLL_LEFT,
                                   nsIPresShell::SCROLL_ALWAYS),
                                 nsIPresShell::SCROLL_FIRST_ANCESTOR_ONLY |
                                 nsIPresShell::SCROLL_OVERFLOW_HIDDEN);
    return NS_OK;
}

/* void scrollToIndex (in long index); */
NS_IMETHODIMP nsScrollBoxObject::ScrollToIndex(PRInt32 index)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void getPosition (out long x, out long y); */
NS_IMETHODIMP nsScrollBoxObject::GetPosition(PRInt32 *x, PRInt32 *y)
{
  nsIScrollableFrame* sf = GetScrollFrame();
  if (!sf)
     return NS_ERROR_FAILURE;

  nsPoint pt = sf->GetScrollPosition();
  *x = nsPresContext::AppUnitsToIntCSSPixels(pt.x);
  *y = nsPresContext::AppUnitsToIntCSSPixels(pt.y);

  return NS_OK;  
}

/* void getScrolledSize (out long width, out long height); */
NS_IMETHODIMP nsScrollBoxObject::GetScrolledSize(PRInt32 *width, PRInt32 *height)
{
    nsIFrame* scrolledBox = GetScrolledBox(this);
    if (!scrolledBox)
        return NS_ERROR_FAILURE;
        	
    nsRect scrollRect = scrolledBox->GetRect();

    *width  = nsPresContext::AppUnitsToIntCSSPixels(scrollRect.width);
    *height = nsPresContext::AppUnitsToIntCSSPixels(scrollRect.height);

    return NS_OK;
}

/* void ensureElementIsVisible (in nsIDOMElement child); */
NS_IMETHODIMP nsScrollBoxObject::EnsureElementIsVisible(nsIDOMElement *child)
{
    NS_ENSURE_ARG_POINTER(child);

    nsCOMPtr<nsIPresShell> shell = GetPresShell(false);
    if (!shell) {
      return NS_ERROR_UNEXPECTED;
    }

    nsCOMPtr<nsIContent> content = do_QueryInterface(child);
    shell->ScrollContentIntoView(content,
                                 nsIPresShell::ScrollAxis(),
                                 nsIPresShell::ScrollAxis(),
                                 nsIPresShell::SCROLL_FIRST_ANCESTOR_ONLY |
                                 nsIPresShell::SCROLL_OVERFLOW_HIDDEN);
    return NS_OK;
}

/* void ensureIndexIsVisible (in long index); */
NS_IMETHODIMP nsScrollBoxObject::EnsureIndexIsVisible(PRInt32 index)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void ensureLineIsVisible (in long line); */
NS_IMETHODIMP nsScrollBoxObject::EnsureLineIsVisible(PRInt32 line)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
NS_NewScrollBoxObject(nsIBoxObject** aResult)
{
  *aResult = new nsScrollBoxObject;
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}

