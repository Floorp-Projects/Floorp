/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsCOMPtr.h"
#include "nsIScrollBoxObject.h"
#include "nsBoxObject.h"
#include "nsIPresShell.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMXULElement.h"
#include "nsIPresContext.h"
#include "nsIFrame.h"
#include "nsIScrollableView.h"
#include "nsIBox.h"


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
  NS_INIT_ISUPPORTS();
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
  
  return scrollableView->ScrollTo(x,y, NS_SCROLL_PROPERTY_ALWAYS_BLIT);
}

/* void scrollBy (in long dx, in long dy); */
NS_IMETHODIMP nsScrollBoxObject::ScrollBy(PRInt32 dx, PRInt32 dy)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void scrollByLine (in long dlines); */
NS_IMETHODIMP nsScrollBoxObject::ScrollByLine(PRInt32 dlines)
{
  nsIScrollableView* scrollableView = GetScrollableView();
  if (!scrollableView)
    return NS_ERROR_FAILURE;

  return scrollableView->ScrollByLines(0, dlines);
}

/* void scrollByIndex (in long dindexes); */
NS_IMETHODIMP nsScrollBoxObject::ScrollByIndex(PRInt32 dindexes)
{
    nsIScrollableView* scrollableView = GetScrollableView();
    if (!scrollableView)
       return NS_ERROR_FAILURE;

    // get our box
    nsIFrame* frame = GetFrame();
    nsCOMPtr<nsIBox> box (do_QueryInterface(frame));

    nsRect rect;
    nsIBox* scrolledBox;
    nsIBox* child;

    // get the scrolled box
    box->GetChildBox(&scrolledBox);

    // now get the scrolled boxes first child.
    scrolledBox->GetChildBox(&child);

    PRBool horiz = PR_FALSE;
    scrolledBox->GetOrientation(horiz);
    nsPoint cp;
    scrollableView->GetScrollPosition(cp.x,cp.y);
    nscoord diff = 0;
    PRInt32 curIndex = 0;

    // first find out what index we are currently at
    while(child) {
      child->GetBounds(rect);
      if (horiz) {
        diff = rect.x + rect.width;
        if (diff > cp.x) {
          break;
        }
      } else {
        diff = rect.y + rect.height;
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
          child->GetBounds(rect);
        count++;
        if (count >= dindexes)
          break;
      }

   } else if (dindexes < 0) {
      scrolledBox->GetChildBox(&child);
      while(child) {
        child->GetBounds(rect);
        if (count >= curIndex + dindexes)
          break;

        count++;
        child->GetNextBox(&child);

      }
   }

   if (horiz)
       return scrollableView->ScrollTo(rect.x, cp.y, NS_SCROLL_PROPERTY_ALWAYS_BLIT);
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
    nsIScrollableView* scrollableView = GetScrollableView();
    if (!scrollableView)
       return NS_ERROR_FAILURE;

    // prepare for twips
    nsCOMPtr<nsIPresContext> context;
    mPresShell->GetPresContext(getter_AddRefs(context));
    float pixelsToTwips = 0.0;
    context->GetPixelsToTwips(&pixelsToTwips);
    
    // get our box
    nsIFrame* frame = GetFrame();
    nsCOMPtr<nsIBox> box (do_QueryInterface(frame));

    nsRect rect;
    nsIBox* scrolledBox;
    nsCOMPtr<nsIDOMXULElement> childDOMXULElement (do_QueryInterface(child));
    nsIBoxObject * childBoxObject;
	childDOMXULElement->GetBoxObject(&childBoxObject);

    PRInt32 x,y;
    childBoxObject->GetX(&x);
    childBoxObject->GetY(&y);
    // get the twips rectangle from the boxobject (which has pixels)    
    rect.x = NSToIntRound(x * pixelsToTwips);
    rect.y = NSToIntRound(y * pixelsToTwips);
    
    // get the scrolled box
    box->GetChildBox(&scrolledBox);

    // TODO: make sure the child is inside the box

    // get our current info
    PRBool horiz = PR_FALSE;
    scrolledBox->GetOrientation(horiz);
    nsPoint cp;
    scrollableView->GetScrollPosition(cp.x,cp.y);
    nscoord newx=cp.x, newy=cp.y;

    // we only scroll in the direction of the scrollbox orientation
    if (horiz) {
        newx = rect.x;
    } else {
        newy = rect.y;
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
  
  return scrollableView->GetScrollPosition(*x,*y);
}

/* void getScrolledSize (out long width, out long height); */
NS_IMETHODIMP nsScrollBoxObject::GetScrolledSize(PRInt32 *width, PRInt32 *height)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void ensureElementIsVisible (in nsIDOMElement child); */
NS_IMETHODIMP nsScrollBoxObject::EnsureElementIsVisible(nsIDOMElement *child)
{
    nsIScrollableView* scrollableView = GetScrollableView();
    if (!scrollableView)
       return NS_ERROR_FAILURE;

    // prepare for twips
    nsCOMPtr<nsIPresContext> context;
    mPresShell->GetPresContext(getter_AddRefs(context));
    float pixelsToTwips = 0.0;
    context->GetPixelsToTwips(&pixelsToTwips);
    
    // get our box
    nsIFrame* frame = GetFrame();
    nsCOMPtr<nsIBox> box (do_QueryInterface(frame));

    nsRect rect, crect;
    nsIBox* scrolledBox;
    nsCOMPtr<nsIDOMXULElement> childDOMXULElement (do_QueryInterface(child));
    nsIBoxObject * childBoxObject;
    childDOMXULElement->GetBoxObject(&childBoxObject);

    PRInt32 x,y,width,height;
    childBoxObject->GetX(&x);
    childBoxObject->GetY(&y);
    childBoxObject->GetWidth(&width);
    childBoxObject->GetHeight(&height);
    // get the twips rectangle from the boxobject (which has pixels)    
    rect.x = NSToIntRound(x * pixelsToTwips);
    rect.y = NSToIntRound(y * pixelsToTwips);
    rect.width = NSToIntRound(width * pixelsToTwips);
    rect.height = NSToIntRound(height * pixelsToTwips);

    // get the scrolled box
    box->GetChildBox(&scrolledBox);

    // TODO: make sure the child is inside the box

    // get our current info
    PRBool horiz = PR_FALSE;
    scrolledBox->GetOrientation(horiz);

    nsPoint cp;
    scrollableView->GetScrollPosition(cp.x,cp.y);
    GetOffsetRect(crect);    
    crect.x = NSToIntRound(crect.x * pixelsToTwips);
    crect.y = NSToIntRound(crect.y * pixelsToTwips);
    crect.width = NSToIntRound(crect.width * pixelsToTwips);
    crect.height = NSToIntRound(crect.height * pixelsToTwips);


    nscoord newx=cp.x, newy=cp.y;

    // we only scroll in the direction of the scrollbox orientation
    if (horiz) {
        if (rect.x + rect.width > cp.x + crect.width) {
            newx = cp.x + ((rect.x + rect.width)-(cp.x + crect.width));
        } else if (rect.x < cp.x) {
            newx = rect.x;
        }
    } else {
        if (rect.y + rect.height > cp.y + crect.height) {
            newx = cp.y + ((rect.y + rect.height)-(cp.y + crect.height));
        } else if (rect.y < cp.y) {
            newy = rect.y;
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
  nsIFrame* frame = GetFrame();
  if (!frame) 
    return nsnull;
  
  nsCOMPtr<nsIPresContext> context;
  mPresShell->GetPresContext(getter_AddRefs(context));
  nsIView* view;
  frame->GetView(context, &view);
  nsIScrollableView* scrollingView = nsnull;
  if (NS_SUCCEEDED(view->QueryInterface(NS_GET_IID(nsIScrollableView), (void**)&scrollingView))) {  
    return scrollingView;
  }

  return nsnull;
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

