/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsAccessible.h"
#include "nsCOMPtr.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsIScrollableView.h"
#include "nsRootAccessible.h"

//#define DEBUG_LEAKS

#ifdef DEBUG_LEAKS
static gnsAccessibles = 0;
#endif


class nsFrameTreeWalker {
public:
  nsFrameTreeWalker(nsIPresContext* aPresContext, nsAccessible* aOwner);
  nsIFrame* GetNextSibling(nsIFrame* aFrame);
  nsIFrame* GetPreviousSibling(nsIFrame* aFrame);
  nsIFrame* GetParent(nsIFrame* aFrame);
  nsIFrame* GetFirstChild(nsIFrame* aFrame);
  nsIFrame* GetLastChild(nsIFrame* aFrame);
  nsIFrame* GetChildBefore(nsIFrame* aParent, nsIFrame* aChild);
  PRInt32 GetCount(nsIFrame* aFrame);

  static PRBool IsSameContent(nsIFrame* aFrame1, nsIFrame* aFrame2);
  static void GetAccessible(nsIFrame* aFrame, nsCOMPtr<nsIAccessible>& aAccessible, nsCOMPtr<nsIContent>& aContent);

  nsCOMPtr<nsIPresContext> mPresContext;
  nsCOMPtr<nsIAccessible> mAccessible;
  nsCOMPtr<nsIContent> mContent;
  nsAccessible* mOwner;
};

nsFrameTreeWalker::nsFrameTreeWalker(nsIPresContext* aPresContext, nsAccessible* aOwner)
{
  mPresContext = aPresContext;
  mOwner = aOwner;
}

nsIFrame* nsFrameTreeWalker::GetParent(nsIFrame* aFrame)
{
  //printf("Get parent\n");

  nsIFrame* parent = nsnull;
  aFrame->GetParent(&parent);

  // if no parent then we hit the root
  // just return that top frame
  if (!parent) {
    mAccessible = nsnull;
    mContent = nsnull;
    return aFrame;
  }

  GetAccessible(parent, mAccessible, mContent);
  if (mAccessible)
    return parent;
  
  return GetParent(parent);
}

nsIFrame* nsFrameTreeWalker::GetNextSibling(nsIFrame* aFrame)
{
  //printf("Get next\n");

  // get next sibling
  nsIFrame* next = nsnull;
  aFrame->GetNextSibling(&next);

  // skip any frames with the same content node
  while(IsSameContent(aFrame,next)) 
    next->GetNextSibling(&next);

  // if failed
  if (!next)
  {
    // if parent has content
    nsIFrame* parent = nsnull;
    aFrame->GetParent(&parent);
    
    // if no parent fail
    if (!parent) {
      mAccessible = nsnull;
      mContent = nsnull;
      return nsnull;
    }

    // fail if we reach a parent that is accessible
    GetAccessible(parent, mAccessible, mContent);
    if (mAccessible)
    {
      // fail
      mAccessible = nsnull;
      mContent = nsnull;
      return nsnull;
    } else {
      // next on parent
      return GetNextSibling(parent);
    }
  }

  // if next has content
  GetAccessible(next, mAccessible, mContent);
  if (mAccessible)
  {
    // done
    return next;
  }

  // if next doesn't have node

  // call first on next
  nsIFrame* first = GetFirstChild(next);

  // if found
  if (first)
    return first;

  // call next on next
  return GetNextSibling(next);
}

nsIFrame* nsFrameTreeWalker::GetFirstChild(nsIFrame* aFrame)
{

  //printf("Get first\n");

  // get first child
  nsIFrame* child = nsnull;
  nsIAtom* list = nsnull;
  mOwner->GetListAtomForFrame(aFrame, list);
  aFrame->FirstChild(mPresContext, list, &child);

  while(child)
  {
    // if first has a content node
    GetAccessible(child, mAccessible, mContent);
    if (mAccessible)
    {
      // done
      return child;
    } else {
      // call first on child
      nsIFrame* first = GetFirstChild(child);

      // if succeeded
      if (first)
      {
        // return child
        return first;
      }
    }

    // get next sibling
    nsIFrame* next;
    child->GetNextSibling(&next);

    // skip children with duplicate content nodes
    while(IsSameContent(child,next)) 
      next->GetNextSibling(&next);

    child = next;
  }

  // fail
  mAccessible = nsnull;
  mContent = nsnull;
  return nsnull;
}

nsIFrame* nsFrameTreeWalker::GetChildBefore(nsIFrame* aParent, nsIFrame* aChild)
{
  nsIFrame* child = GetFirstChild(aParent);

  // if the child is not us
  if (child == aChild) {
    mAccessible = nsnull;
    mContent = nsnull;
    return nsnull;
  }

  nsIFrame* prev = child;
  nsCOMPtr<nsIContent> prevContent = mContent;
  nsCOMPtr<nsIAccessible> prevAccessible = mAccessible;

  while(child) 
  {
    child = GetNextSibling(child);

    if (child == aChild)
      break;

    prev = child;
    prevContent = mContent;
    prevAccessible = mAccessible;
  }

  mAccessible = prevAccessible;
  mContent = prevContent;
  return prev;
}

nsIFrame* nsFrameTreeWalker::GetPreviousSibling(nsIFrame* aFrame)
{
  //printf("Get previous\n");

  nsIFrame* parent = GetParent(aFrame);
  
  return GetChildBefore(parent, aFrame);
}

nsIFrame* nsFrameTreeWalker::GetLastChild(nsIFrame* aFrame)
{
  //printf("Get last\n");

  return GetChildBefore(aFrame, nsnull);
}

PRInt32 nsFrameTreeWalker::GetCount(nsIFrame* aFrame)
{

  //printf("Get count\n");
  nsIFrame* child = GetFirstChild(aFrame);

  PRInt32 count = 0;
  while(child) 
  {
    count++;
    child = GetNextSibling(child);
  }

  return count;
}

void nsFrameTreeWalker::GetAccessible(nsIFrame* aFrame, nsCOMPtr<nsIAccessible>& aAccessible, nsCOMPtr<nsIContent>& aContent)
{
  aContent = nsnull;
  aAccessible = nsnull;

  aFrame->GetContent(getter_AddRefs(aContent));

  if (!aContent)
    return;

  nsCOMPtr<nsIDocument> document;
  aContent->GetDocument(*getter_AddRefs(document));
  if (!document)
    return;

  PRInt32 shells = document->GetNumberOfShells();
  NS_ASSERTION(shells > 0,"Error no shells!");
  nsIPresShell* shell = document->GetShellAt(0);
  nsIFrame* frame = nsnull;
  shell->GetPrimaryFrameFor(aContent, &frame);

  if (!frame)
    return;

  aAccessible = do_QueryInterface(aFrame);

  if (!aAccessible)
    aAccessible = do_QueryInterface(aContent);

 // if (aAccessible)
 //   printf("Found accessible!\n");
}

PRBool nsFrameTreeWalker::IsSameContent(nsIFrame* aFrame1, nsIFrame* aFrame2)
{
  if (!aFrame1 || !aFrame2)
    return PR_FALSE;

  nsCOMPtr<nsIContent> content1;
  nsCOMPtr<nsIContent> content2;

  aFrame1->GetContent(getter_AddRefs(content1));
  aFrame2->GetContent(getter_AddRefs(content2));

  if (content1 == content2 && content1 != nsnull)
    return PR_TRUE;
  
  return PR_FALSE;
}

/*
 * Class nsAccessible
 */

//-----------------------------------------------------
// construction 
//-----------------------------------------------------
nsAccessible::nsAccessible(nsIAccessible* aAccessible, nsIContent* aContent, nsIWeakReference* aShell)
{
   NS_INIT_REFCNT();

   // get frame and node
   mContent = aContent;
   mAccessible = aAccessible;
   mPresShell = aShell;

#ifdef DEBUG_LEAKS
  printf("nsAccessibles=%d\n", ++gnsAccessibles);
#endif

}

//-----------------------------------------------------
// destruction
//-----------------------------------------------------
nsAccessible::~nsAccessible()
{
#ifdef DEBUG_LEAKS
  printf("nsAccessibles=%d\n", --gnsAccessibles);
#endif
}

//NS_IMPL_ISUPPORTS2(nsAccessible, nsIAccessible, nsIAccessibleWidgetAccess);
NS_IMPL_ISUPPORTS1(nsAccessible, nsIAccessible);

  /* readonly attribute nsIAccessible accParent; */
NS_IMETHODIMP nsAccessible::GetAccParent(nsIAccessible * *aAccParent) 
{ 
  // delegate
  if (mAccessible) {
    nsresult rv = mAccessible->GetAccParent(aAccParent);
    if (NS_SUCCEEDED(rv))
      return rv;
  }

  nsCOMPtr<nsIPresContext> context;
  GetPresContext(context);

  if (context) {
    nsFrameTreeWalker walker(context, this); 

    // failed? Lets do some default behavior
    walker.GetParent(GetFrame());

    // if no content or accessible then we hit the root
    if (!walker.mContent || !walker.mAccessible)
    {
      *aAccParent = new nsRootAccessible(mPresShell);
      NS_ADDREF(*aAccParent);
      return NS_OK;
    }

    *aAccParent = CreateNewParentAccessible(walker.mAccessible, walker.mContent, mPresShell);
    NS_ADDREF(*aAccParent);
    return NS_OK;
  }

  *aAccParent = nsnull;
  return NS_OK;
}
  /* readonly attribute nsIAccessible accNextSibling; */
NS_IMETHODIMP nsAccessible::GetAccNextSibling(nsIAccessible * *aAccNextSibling) 
{ 
  // delegate
 
  if (mAccessible) {
    nsresult rv = mAccessible->GetAccNextSibling(aAccNextSibling);
    if (NS_SUCCEEDED(rv))
      return rv;
  }

  // failed? Lets do some default behavior

  nsCOMPtr<nsIPresContext> context;
  GetPresContext(context);

  if (context) {
    nsFrameTreeWalker walker(context, this);

    nsIFrame* next = walker.GetNextSibling(GetFrame());
  
    if (next && walker.mAccessible && walker.mContent) 
    {
      *aAccNextSibling = CreateNewNextAccessible(walker.mAccessible, walker.mContent, mPresShell);
      NS_ADDREF(*aAccNextSibling);
      return NS_OK;
    }
  }

  *aAccNextSibling = nsnull;

  return NS_OK;  
}

  /* readonly attribute nsIAccessible accPreviousSibling; */
NS_IMETHODIMP nsAccessible::GetAccPreviousSibling(nsIAccessible * *aAccPreviousSibling) 
{ 
// delegate
 
  if (mAccessible) {
    nsresult rv = mAccessible->GetAccPreviousSibling(aAccPreviousSibling);
    if (NS_SUCCEEDED(rv))
      return rv;
  }
 
  // failed? Lets do some default behavior
  nsCOMPtr<nsIPresContext> context;
  GetPresContext(context);

  if (context) {
    nsFrameTreeWalker walker(context, this); 
    nsIFrame* prev = walker.GetPreviousSibling(GetFrame());

    if (prev && walker.mAccessible && walker.mContent) 
    {
      *aAccPreviousSibling = CreateNewPreviousAccessible(walker.mAccessible, walker.mContent, mPresShell);
      NS_ADDREF(*aAccPreviousSibling);
      return NS_OK;
    }
  }

  *aAccPreviousSibling = nsnull;

  return NS_OK;  
}

  /* readonly attribute nsIAccessible accFirstChild; */
NS_IMETHODIMP nsAccessible::GetAccFirstChild(nsIAccessible * *aAccFirstChild) 
{ 
    
  // delegate
  if (mAccessible) {
    nsresult rv = mAccessible->GetAccFirstChild(aAccFirstChild);
    if (NS_SUCCEEDED(rv))
      return rv;
  }

  nsCOMPtr<nsIPresContext> context;
  GetPresContext(context);

  if (context) {

    nsFrameTreeWalker walker(context, this); 
    nsIFrame* child = walker.GetFirstChild(GetFrame());

    if (child && walker.mAccessible && walker.mContent) 
    {
      *aAccFirstChild = CreateNewFirstAccessible(walker.mAccessible, walker.mContent, mPresShell);
      NS_ADDREF(*aAccFirstChild);
      return NS_OK;
    }
  }

  *aAccFirstChild = nsnull;

  return NS_OK;  
}

  /* readonly attribute nsIAccessible accFirstChild; */
NS_IMETHODIMP nsAccessible::GetAccLastChild(nsIAccessible * *aAccLastChild)
{  
  // delegate
  if (mAccessible) {
    nsresult rv = mAccessible->GetAccLastChild(aAccLastChild);
    if (NS_SUCCEEDED(rv))
      return rv;
  }

  nsCOMPtr<nsIPresContext> context;
  GetPresContext(context);

  if (context) {
    nsFrameTreeWalker walker(context, this); 
    nsIFrame* last = walker.GetLastChild(GetFrame());

    if (last && walker.mAccessible && walker.mContent) 
    {
      *aAccLastChild = CreateNewLastAccessible(walker.mAccessible, walker.mContent, mPresShell);
      NS_ADDREF(*aAccLastChild);
      return NS_OK;
    }
  }

  *aAccLastChild = nsnull;

  return NS_OK;
}

/* readonly attribute long accChildCount; */
NS_IMETHODIMP nsAccessible::GetAccChildCount(PRInt32 *aAccChildCount) 
{
  
  // delegate
  if (mAccessible) {
    nsresult rv = mAccessible->GetAccChildCount(aAccChildCount);
    if (NS_SUCCEEDED(rv))
      return rv;
  }
  

  // failed? Lets do some default behavior
  nsCOMPtr<nsIPresContext> context;
  GetPresContext(context);

  if (context) {
    nsFrameTreeWalker walker(context, this); 
    *aAccChildCount = walker.GetCount(GetFrame());
  } else 
    *aAccChildCount = 0;

  return NS_OK;  
}


  /* attribute wstring accName; */
NS_IMETHODIMP nsAccessible::GetAccName(PRUnichar * *aAccName) 
{ 
  // delegate
  if (mAccessible) {
    nsresult rv = mAccessible->GetAccName(aAccName);
    if (NS_SUCCEEDED(rv) && *aAccName != nsnull)
      return rv;
  }

  *aAccName = 0;
  return NS_ERROR_NOT_IMPLEMENTED;
}

  /* attribute wstring accName; */
NS_IMETHODIMP nsAccessible::GetAccDefaultAction(PRUnichar * *aDefaultAction) 
{ 
  // delegate
  if (mAccessible) {
    nsresult rv = mAccessible->GetAccDefaultAction(aDefaultAction);
    if (NS_SUCCEEDED(rv) && *aDefaultAction != nsnull)
      return rv;
  }

  *aDefaultAction = 0;
  return NS_ERROR_NOT_IMPLEMENTED;  
}

NS_IMETHODIMP nsAccessible::SetAccName(const PRUnichar * aAccName) 
{ 
  // delegate
  if (mAccessible) {
    nsresult rv = mAccessible->SetAccName(aAccName);
    if (NS_SUCCEEDED(rv))
      return rv;
  }

  return NS_ERROR_NOT_IMPLEMENTED;  
}

  /* attribute wstring accValue; */
NS_IMETHODIMP nsAccessible::GetAccValue(PRUnichar * *aAccValue) 
{ 
  // delegate
  if (mAccessible) {
    nsresult rv = mAccessible->GetAccValue(aAccValue);

    if (NS_SUCCEEDED(rv) && *aAccValue != nsnull)
      return rv;
  }

  *aAccValue = 0;
  return NS_ERROR_NOT_IMPLEMENTED;  
}

NS_IMETHODIMP nsAccessible::SetAccValue(const PRUnichar * aAccValue) { return NS_ERROR_NOT_IMPLEMENTED;  }

  /* readonly attribute wstring accDescription; */
NS_IMETHODIMP nsAccessible::GetAccDescription(PRUnichar * *aAccDescription) 
{ 
  // delegate
  if (mAccessible) {
    nsresult rv = mAccessible->GetAccDescription(aAccDescription);
    if (NS_SUCCEEDED(rv) && *aAccDescription != nsnull)
      return rv;
  }

  return NS_ERROR_NOT_IMPLEMENTED;  
}

  /* readonly attribute wstring accRole; */
NS_IMETHODIMP nsAccessible::GetAccRole(PRUnichar * *aAccRole) 
{ 
  // delegate
  if (mAccessible) {
    nsresult rv = mAccessible->GetAccRole(aAccRole);
    if (NS_SUCCEEDED(rv) && *aAccRole != nsnull)
      return rv;
  }

  return NS_ERROR_NOT_IMPLEMENTED;  
}

  /* readonly attribute wstring accState; */
NS_IMETHODIMP nsAccessible::GetAccState(PRUint32 *aAccState) 
{ 
  // delegate
  if (mAccessible) 
    return mAccessible->GetAccState(aAccState);

  return NS_ERROR_NOT_IMPLEMENTED;  
}

NS_IMETHODIMP nsAccessible::GetAccExtState(PRUint32 *aAccExtState) 
{ 
  // delegate
  if (mAccessible) 
    return mAccessible->GetAccExtState(aAccExtState);

  return NS_ERROR_NOT_IMPLEMENTED;  
}

  /* readonly attribute wstring accHelp; */
NS_IMETHODIMP nsAccessible::GetAccHelp(PRUnichar * *aAccHelp) 
{ 
  // delegate
  if (mAccessible) {
    nsresult rv = mAccessible->GetAccHelp(aAccHelp);
    if (NS_SUCCEEDED(rv) && *aAccHelp != nsnull)
      return rv;
  }

  // failed? Lets do some default behavior
  return NS_ERROR_NOT_IMPLEMENTED;  
}

  /* readonly attribute boolean accFocused; */
NS_IMETHODIMP nsAccessible::GetAccFocused(PRBool *aAccFocused) { return NS_OK;  }

  /* nsIAccessible accGetChildAt (in long x, in long y); */
NS_IMETHODIMP nsAccessible::AccGetAt(PRInt32 tx, PRInt32 ty, nsIAccessible **_retval)
{
  PRInt32 x,y,w,h;
  AccGetBounds(&x,&y,&w,&h);
  if (tx > x && tx < x + w && ty > y && ty < y + h)
  {
    nsCOMPtr<nsIAccessible> child;
    nsCOMPtr<nsIAccessible> next;
    GetAccFirstChild(getter_AddRefs(child));
    PRInt32 cx,cy,cw,ch;

    while(child) {
      child->AccGetBounds(&cx,&cy,&cw,&ch);
      
      if (tx > cx && tx < cx + cw && ty > cy && ty < cy + ch) 
      {
        *_retval = child;
        NS_ADDREF(*_retval);
        return NS_OK;
      }
      child->GetAccNextSibling(getter_AddRefs(next));
      child = next;
    }


    *_retval = this;
    NS_ADDREF(this);
    return NS_OK;
  }

  *_retval = nsnull;
  return NS_OK;
}

  /* void accNavigateRight (); */
NS_IMETHODIMP nsAccessible::AccNavigateRight(nsIAccessible **_retval) { return NS_OK;  }

  /* void navigateLeft (); */
NS_IMETHODIMP nsAccessible::AccNavigateLeft(nsIAccessible **_retval) { return NS_OK;  }

  /* void navigateUp (); */

NS_IMETHODIMP nsAccessible::AccNavigateUp(nsIAccessible **_retval) { return NS_OK;  }

  /* void navigateDown (); */
NS_IMETHODIMP nsAccessible::AccNavigateDown(nsIAccessible **_retval) { return NS_OK;  }



  /* void addSelection (); */
NS_IMETHODIMP nsAccessible::AccAddSelection(void) { return NS_OK;  }

  /* void removeSelection (); */
NS_IMETHODIMP nsAccessible::AccRemoveSelection(void) { return NS_OK;  }

  /* void extendSelection (); */
NS_IMETHODIMP nsAccessible::AccExtendSelection(void) { return NS_OK;  }

  /* void takeSelection (); */
NS_IMETHODIMP nsAccessible::AccTakeSelection(void) { return NS_OK;  }

  /* void takeFocus (); */
NS_IMETHODIMP nsAccessible::AccTakeFocus(void) { return NS_OK;  }

  /* void doDefaultAction (); */
NS_IMETHODIMP nsAccessible::AccDoDefaultAction(void) 
{ 
  // delegate
  if (mAccessible) {
    nsresult rv = mAccessible->AccDoDefaultAction();
    if (NS_SUCCEEDED(rv))
      return rv;
  }

  return NS_ERROR_FAILURE;  
}

  /* void accGetBounds (out long x, out long y, out long width, out long height); */
NS_IMETHODIMP nsAccessible::AccGetBounds(PRInt32 *x, PRInt32 *y, PRInt32 *width, PRInt32 *height)
{
  nsCOMPtr<nsIPresContext> context;
  GetPresContext(context);

  nsIFrame* frame = GetBoundsFrame();

  if (!frame || !context)
  {
    *x = *y = *width = *height = 0;
    return NS_OK;
  }

   // sum up all rects of frames with the same content node
  nsRect r;
  nsIFrame* start = frame;
  nsIFrame* next = nsnull;
  start->GetNextSibling(&next);

  start->GetRect(r);
  
  while (nsFrameTreeWalker::IsSameContent(start, next))
  {
    nsRect r2;
    next->GetRect(r2);
    r.UnionRect(r,r2);
    next->GetNextSibling(&next);
  }

  nsPoint offset(r.x,r.y);

  frame->GetParent(&frame);

  nsPoint pos(0,0);
  while(frame) {
    nsIScrollableView* scrollingView;
    nsIView*           view;
    // XXX hack
    frame->GetView(context, &view);
    if (view) {
     nsresult result = view->QueryInterface(NS_GET_IID(nsIScrollableView), (void**)&scrollingView);
     if (NS_SUCCEEDED(result)) {
         nscoord xoff = 0;
         nscoord yoff = 0;
         scrollingView->GetScrollPosition(xoff, yoff);
         offset.x -= xoff;
         offset.y -= yoff;
     }
    }

    frame->GetOrigin(pos);
    offset += pos;
    frame->GetParent(&frame);
  }

  float t2p;
  context->GetTwipsToPixels(&t2p);

  *x = (PRInt32)(offset.x*t2p);
  *y = (PRInt32)(offset.y*t2p);
  *width = (PRInt32)(r.width*t2p);
  *height = (PRInt32)(r.height*t2p);

  return NS_OK;
}

// helpers

nsIFrame* nsAccessible::GetBoundsFrame()
{
   return GetFrame();
}

nsIFrame* nsAccessible::GetFrame()
{
   nsCOMPtr<nsIPresShell> shell = do_QueryReferent(mPresShell);
   nsIFrame* frame = nsnull;
   shell->GetPrimaryFrameFor(mContent, &frame);
   return frame;
}

void nsAccessible::GetPresContext(nsCOMPtr<nsIPresContext>& aContext)
{
  nsCOMPtr<nsIPresShell> shell = do_QueryReferent(mPresShell);

  if (shell) {
    shell->GetPresContext(getter_AddRefs(aContext));
  } else
    aContext = nsnull;
}

nsIAccessible* nsAccessible::CreateNewNextAccessible(nsIAccessible* aAccessible, nsIContent* aContent, nsIWeakReference* aShell)
{
  return CreateNewAccessible(aAccessible, aContent, aShell);
}

nsIAccessible* nsAccessible::CreateNewPreviousAccessible(nsIAccessible* aAccessible, nsIContent* aContent, nsIWeakReference* aShell)
{
  return CreateNewAccessible(aAccessible, aContent, aShell);
}

nsIAccessible* nsAccessible::CreateNewParentAccessible(nsIAccessible* aAccessible, nsIContent* aContent, nsIWeakReference* aShell)
{
  return CreateNewAccessible(aAccessible, aContent, aShell);
}

nsIAccessible* nsAccessible::CreateNewFirstAccessible(nsIAccessible* aAccessible, nsIContent* aContent, nsIWeakReference* aShell)
{
  return CreateNewAccessible(aAccessible, aContent, aShell);
}

nsIAccessible* nsAccessible::CreateNewLastAccessible(nsIAccessible* aAccessible, nsIContent* aContent, nsIWeakReference* aShell)
{
  return CreateNewAccessible(aAccessible, aContent, aShell);
}

nsIAccessible* nsAccessible::CreateNewAccessible(nsIAccessible* aAccessible, nsIContent* aContent, nsIWeakReference* aShell)
{
  NS_ASSERTION(aAccessible && aContent,"Error not accessible or content");
  return new nsAccessible(aAccessible, aContent, aShell);
}


// ------- nsHTMLBlockAccessible ------

nsHTMLBlockAccessible::nsHTMLBlockAccessible(nsIAccessible* aAccessible, nsIContent* aContent, nsIWeakReference* aShell):nsAccessible(aAccessible, aContent, aShell)
{

}

nsIAccessible* nsHTMLBlockAccessible::CreateNewAccessible(nsIAccessible* aAccessible, nsIContent* aContent, nsIWeakReference* aShell)
{
  NS_ASSERTION(aAccessible && aContent,"Error not accessible or content");
  return new nsHTMLBlockAccessible(aAccessible, aContent, aShell);
}

/* nsIAccessible accGetAt (in long x, in long y); */
NS_IMETHODIMP nsHTMLBlockAccessible::AccGetAt(PRInt32 tx, PRInt32 ty, nsIAccessible **_retval)
{
  PRInt32 x,y,w,h;
  AccGetBounds(&x,&y,&w,&h);
  if (tx > x && tx < x + w && ty > y && ty < y + h)
  {
    nsCOMPtr<nsIAccessible> child;
    nsCOMPtr<nsIAccessible> smallestChild;
    PRInt32 smallestArea = -1;
    nsCOMPtr<nsIAccessible> next;
    GetAccFirstChild(getter_AddRefs(child));
    PRInt32 cx,cy,cw,ch;

    while(child) {
      child->AccGetBounds(&cx,&cy,&cw,&ch);
      
      // ok if there are multiple frames the contain the point 
      // and they overlap then pick the smallest. We need to do this
      // for text frames.
      if (tx > cx && tx < cx + cw && ty > cy && ty < cy + ch) 
      {
        if (smallestArea == -1 || cw*ch < smallestArea) {
          smallestArea = cw*ch;
          smallestChild = child;
        }
      }
      child->GetAccNextSibling(getter_AddRefs(next));
      child = next;
    }

    if (smallestChild != nsnull)
    {
      *_retval = smallestChild;
      NS_ADDREF(*_retval);
      return NS_OK;
    }

    *_retval = this;
    NS_ADDREF(this);
    return NS_OK;
  }

  *_retval = nsnull;
  return NS_OK;
}





