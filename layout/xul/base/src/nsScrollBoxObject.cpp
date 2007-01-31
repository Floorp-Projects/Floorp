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
 *   Original Author: David W. Hyatt (hyatt@netscape.com)
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
#include "nsCOMPtr.h"
#include "nsIScrollBoxObject.h"
#include "nsBoxObject.h"
#include "nsIPresShell.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNSDocument.h"
#include "nsIDOMElement.h"
#include "nsPresContext.h"
#include "nsIFrame.h"
#include "nsIScrollableView.h"
#include "nsIScrollableFrame.h"


class nsScrollBoxObject : public nsIScrollBoxObject, public nsBoxObject
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSISCROLLBOXOBJECT

  nsScrollBoxObject();
  virtual ~nsScrollBoxObject();

  virtual nsIScrollableView* GetScrollableView();

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
  nsIScrollableView* scrollableView = GetScrollableView();
  if (!scrollableView)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIPresShell> shell = GetPresShell(PR_FALSE);
  if (!shell) {
    return NS_ERROR_UNEXPECTED;
  }
  
  float pixelsToTwips = shell->GetPresContext()->PixelsToTwips();

  return scrollableView->ScrollTo(NSToIntRound(x * pixelsToTwips),
                                  NSToIntRound(y * pixelsToTwips),
                                  NS_SCROLL_PROPERTY_ALWAYS_BLIT);
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
  nsIScrollableView* scrollableView = GetScrollableView();
  if (!scrollableView)
    return NS_ERROR_FAILURE;

  return scrollableView->ScrollByLines(0, dlines);
}

// XUL <scrollbox> elements have a single box child element.
// Get a pointer to that box.
// Note that now that the <scrollbox> is just a regular box
// with 'overflow:hidden', the boxobject's frame is an nsXULScrollFrame,
// the <scrollbox>'s box frame is the scrollframe's "scrolled frame", and
// the <scrollbox>'s child box is a child of that.
static nsIFrame* GetScrolledBox(nsBoxObject* aScrollBox) {
  nsIFrame* frame = aScrollBox->GetFrame(PR_FALSE);
  if (!frame) 
    return nsnull;
  nsIScrollableFrame* scrollFrame;
  if (NS_FAILED(CallQueryInterface(frame, &scrollFrame))) {
    NS_WARNING("nsIScrollBoxObject attached to something that's not a scroll frame!");
    return nsnull;
  }
  nsIFrame* scrolledFrame = scrollFrame->GetScrolledFrame();
  if (!scrolledFrame)
    return nsnull;
  nsIBox* scrolledBox;
  if (NS_FAILED(scrolledFrame->GetChildBox(&scrolledBox)))
    return nsnull;
  return scrolledBox;
}

/* void scrollByIndex (in long dindexes); */
NS_IMETHODIMP nsScrollBoxObject::ScrollByIndex(PRInt32 dindexes)
{
    nsIScrollableView* scrollableView = GetScrollableView();
    if (!scrollableView)
       return NS_ERROR_FAILURE;
    nsIFrame* scrolledBox = GetScrolledBox(this);
    if (!scrolledBox)
       return NS_ERROR_FAILURE;

    nsRect rect;
    nsIFrame* child;

    // now get the scrolled boxes first child.
    scrolledBox->GetChildBox(&child);

    PRBool horiz = scrolledBox->IsHorizontal();
    nsPoint cp;
    scrollableView->GetScrollPosition(cp.x,cp.y);
    nscoord diff = 0;
    PRInt32 curIndex = 0;
    PRBool isLTR = scrolledBox->IsNormalDirection();

    PRInt32 frameWidth = 0;
    if (!isLTR && horiz) {
      GetWidth(&frameWidth);
      nsCOMPtr<nsIPresShell> shell = GetPresShell(PR_FALSE);
      if (!shell) {
        return NS_ERROR_UNEXPECTED;
      }
      frameWidth = NSToIntRound(frameWidth *
                                shell->GetPresContext()->PixelsToTwips());
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
      child->GetNextBox(&child);
      curIndex++;
    }

    PRInt32 count = 0;

    if (dindexes == 0)
       return NS_OK;

    if (dindexes > 0) {
      while(child) {
        child->GetNextBox(&child);
        if (child)
          rect = child->GetRect();
        count++;
        if (count >= dindexes)
          break;
      }

   } else if (dindexes < 0) {
      scrolledBox->GetChildBox(&child);
      while(child) {
        rect = child->GetRect();
        if (count >= curIndex + dindexes)
          break;

        count++;
        child->GetNextBox(&child);

      }
   }

   if (horiz)
       // In the left-to-right case we scroll so that the left edge of the
       // selected child is scrolled to the left edge of the scrollbox.
       // In the right-to-left case we scroll so that the right edge of the
       // selected child is scrolled to the right edge of the scrollbox.
       return scrollableView->ScrollTo((isLTR) ? rect.x :
                                       rect.x + rect.width - frameWidth, 
                                       cp.y, NS_SCROLL_PROPERTY_ALWAYS_BLIT);
   else
       return scrollableView->ScrollTo(cp.x, rect.y, NS_SCROLL_PROPERTY_ALWAYS_BLIT);
}

/* void scrollToLine (in long line); */
NS_IMETHODIMP nsScrollBoxObject::ScrollToLine(PRInt32 line)
{
  nsIScrollableView* scrollableView = GetScrollableView();
  if (!scrollableView)
    return NS_ERROR_FAILURE;
  
  nscoord height = 0;
  scrollableView->GetLineHeight(&height);
  scrollableView->ScrollTo(0,height*line, NS_SCROLL_PROPERTY_ALWAYS_BLIT);

  return NS_OK;
}

/* void scrollToElement (in nsIDOMElement child); */
NS_IMETHODIMP nsScrollBoxObject::ScrollToElement(nsIDOMElement *child)
{
    NS_ENSURE_ARG_POINTER(child);
    nsIScrollableView* scrollableView = GetScrollableView();
    if (!scrollableView)
       return NS_ERROR_FAILURE;

    nsCOMPtr<nsIPresShell> shell = GetPresShell(PR_FALSE);
    if (!shell) {
      return NS_ERROR_UNEXPECTED;
    }
    
    // prepare for twips
    float pixelsToTwips = shell->GetPresContext()->PixelsToTwips();
    
    nsIFrame* scrolledBox = GetScrolledBox(this);
    if (!scrolledBox)
       return NS_ERROR_FAILURE;

    nsRect rect, crect;
    nsCOMPtr<nsIDOMDocument> doc;
    child->GetOwnerDocument(getter_AddRefs(doc));
    nsCOMPtr<nsIDOMNSDocument> nsDoc(do_QueryInterface(doc));
    if(!nsDoc)
      return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsIBoxObject> childBoxObject;
    nsDoc->GetBoxObjectFor(child, getter_AddRefs(childBoxObject));
    if(!childBoxObject)
      return NS_ERROR_UNEXPECTED;

    PRInt32 x,y;
    childBoxObject->GetX(&x);
    childBoxObject->GetY(&y);
    // get the twips rectangle from the boxobject (which has pixels)    
    rect.x = NSToIntRound(x * pixelsToTwips);
    rect.y = NSToIntRound(y * pixelsToTwips);
    
    // TODO: make sure the child is inside the box

    // get our current info
    nsPoint cp;
    scrollableView->GetScrollPosition(cp.x,cp.y);

    GetOffsetRect(crect);    
    crect.x = NSToIntRound(crect.x * pixelsToTwips);
    crect.y = NSToIntRound(crect.y * pixelsToTwips);
    nscoord newx=cp.x, newy=cp.y;

    // we only scroll in the direction of the scrollbox orientation
    // always scroll to left or top edge of child element
    if (scrolledBox->IsHorizontal()) {
        newx = rect.x - crect.x;
    } else {
        newy = rect.y - crect.y;
    }
    // scroll away
    return scrollableView->ScrollTo(newx, newy, NS_SCROLL_PROPERTY_ALWAYS_BLIT);
}

/* void scrollToIndex (in long index); */
NS_IMETHODIMP nsScrollBoxObject::ScrollToIndex(PRInt32 index)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void getPosition (out long x, out long y); */
NS_IMETHODIMP nsScrollBoxObject::GetPosition(PRInt32 *x, PRInt32 *y)
{
  nsIScrollableView* scrollableView = GetScrollableView();
  if (!scrollableView)
    return NS_ERROR_FAILURE;

  nscoord xc, yc;
  nsresult rv = scrollableView->GetScrollPosition(xc, yc);
  if (NS_FAILED(rv))
    return rv;

  nsIPresShell* shell = GetPresShell(PR_FALSE);
  if (!shell) {
    return NS_ERROR_UNEXPECTED;
  }

  float twipsToPixels = shell->GetPresContext()->TwipsToPixels();

  *x = NSToIntRound(xc * twipsToPixels);
  *y = NSToIntRound(yc * twipsToPixels);

  return NS_OK;  
}

/* void getScrolledSize (out long width, out long height); */
NS_IMETHODIMP nsScrollBoxObject::GetScrolledSize(PRInt32 *width, PRInt32 *height)
{
    nsIFrame* scrolledBox = GetScrolledBox(this);
    if (!scrolledBox)
        return NS_ERROR_FAILURE;
        	
    nsRect scrollRect = scrolledBox->GetRect();

    nsIPresShell* shell = GetPresShell(PR_FALSE);
    if (!shell) {
      return NS_ERROR_UNEXPECTED;
    }

    float twipsToPixels = shell->GetPresContext()->TwipsToPixels();
 
    *width  = NSTwipsToIntPixels(scrollRect.width, twipsToPixels);
    *height = NSTwipsToIntPixels(scrollRect.height, twipsToPixels);

    return NS_OK;
}

/* void ensureElementIsVisible (in nsIDOMElement child); */
NS_IMETHODIMP nsScrollBoxObject::EnsureElementIsVisible(nsIDOMElement *child)
{
    NS_ENSURE_ARG_POINTER(child);

    // Start with getting info about the child, since that will flush
    // layout and possibly destroy scrollable views, presshells, etc.
    nsCOMPtr<nsIDOMDocument> doc;
    // XXXbz sXBL/XBL2 issue -- which document?
    child->GetOwnerDocument(getter_AddRefs(doc));
    nsCOMPtr<nsIDOMNSDocument> nsDoc(do_QueryInterface(doc));
    if(!nsDoc)
        return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsIBoxObject> childBoxObject;
    nsDoc->GetBoxObjectFor(child, getter_AddRefs(childBoxObject));
    if(!childBoxObject)
      return NS_ERROR_UNEXPECTED;

    PRInt32 x, y, width, height;
    childBoxObject->GetX(&x);
    childBoxObject->GetY(&y);
    childBoxObject->GetWidth(&width);
    childBoxObject->GetHeight(&height);

    nsIScrollableView* scrollableView = GetScrollableView();
    if (!scrollableView)
       return NS_ERROR_FAILURE;

    nsIPresShell* shell = GetPresShell(PR_FALSE);
    if (!shell) {
      return NS_ERROR_UNEXPECTED;
    }

    // prepare for twips
    float pixelsToTwips = 0.0;
    pixelsToTwips = shell->GetPresContext()->PixelsToTwips();
    
    nsIFrame* scrolledBox = GetScrolledBox(this);
    if (!scrolledBox)
       return NS_ERROR_FAILURE;

    nsRect rect, crect;
    // get the twips rectangle from the boxobject (which has pixels)    
    rect.x = NSToIntRound(x * pixelsToTwips);
    rect.y = NSToIntRound(y * pixelsToTwips);
    rect.width = NSToIntRound(width * pixelsToTwips);
    rect.height = NSToIntRound(height * pixelsToTwips);

    // TODO: make sure the child is inside the box

    // get our current info
    nsPoint cp;
    scrollableView->GetScrollPosition(cp.x,cp.y);
    GetOffsetRect(crect);    
    crect.x = NSToIntRound(crect.x * pixelsToTwips);
    crect.y = NSToIntRound(crect.y * pixelsToTwips);
    crect.width = NSToIntRound(crect.width * pixelsToTwips);
    crect.height = NSToIntRound(crect.height * pixelsToTwips);

    nscoord newx=cp.x, newy=cp.y;

    // we only scroll in the direction of the scrollbox orientation
    if (scrolledBox->IsHorizontal()) {
        if ((rect.x - crect.x) + rect.width > cp.x + crect.width) {
            newx = cp.x + (((rect.x - crect.x) + rect.width)-(cp.x + crect.width));
        } else if (rect.x - crect.x < cp.x) {
            newx = rect.x - crect.x;
        }
    } else {
        if ((rect.y - crect.y) + rect.height > cp.y + crect.height) {
            newy = cp.y + (((rect.y - crect.y) + rect.height)-(cp.y + crect.height));
        } else if (rect.y - crect.y < cp.y) {
            newy = rect.y - crect.y;
        }
    }
    
    // scroll away
    return scrollableView->ScrollTo(newx, newy, NS_SCROLL_PROPERTY_ALWAYS_BLIT);
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

nsIScrollableView* 
nsScrollBoxObject::GetScrollableView()
{
  // get the frame.
  nsIFrame* frame = GetFrame(PR_FALSE);
  if (!frame) 
    return nsnull;
  
  nsIScrollableFrame* scrollFrame;
  if (NS_FAILED(CallQueryInterface(frame, &scrollFrame)))
    return nsnull;

  nsIScrollableView* scrollingView = scrollFrame->GetScrollableView();
  if (!scrollingView)
    return nsnull;

  return scrollingView;
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

