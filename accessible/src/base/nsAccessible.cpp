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

//#define DEBUG_LEAKS

#ifdef DEBUG_LEAKS
static gnsAccessibles = 0;
#endif

/*
 * Class nsAccessible
 */

//-----------------------------------------------------
// construction 
//-----------------------------------------------------
nsAccessible::nsAccessible(nsIFrame* aFrame, nsIPresContext* aContext)
{
   NS_INIT_REFCNT();

   // get frame and node
   mFrame = aFrame;
   nsCOMPtr<nsIContent> content;
   aFrame->GetContent(getter_AddRefs(content));
   mNode = do_QueryInterface(content);
   mPresContext = aContext;

   // see if the frame implements nsIAccessible
   mAccessible = do_QueryInterface(aFrame);

   // if not see if its content node supports nsIAccessible
   if (!mAccessible)
      mAccessible = do_QueryInterface(content);

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

  // failed? Lets do some default behavior
  nsIFrame* parent;
  nsresult rv = GetFrame()->GetParent(&parent);
  if (NS_FAILED(rv))
    return rv;

  if (parent) 
  {
    *aAccParent = new nsAccessible(parent,mPresContext);
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
  nsIFrame* next;
  nsresult rv = GetFrame()->GetNextSibling(&next);
  if (NS_FAILED(rv))
    return rv;

  if (next) 
  {
    *aAccNextSibling = new nsAccessible(next,mPresContext);
    NS_ADDREF(*aAccNextSibling);
    return NS_OK;
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

    // get the parent
  nsIFrame* frame = GetFrame();
  nsIFrame* parent;
  nsresult rv = frame->GetParent(&parent);
  if (NS_FAILED(rv))
    return nsnull;

  // find us. What is the child before us.
  nsIFrame* child;
  rv = parent->FirstChild(mPresContext,nsnull,&child);
  if (NS_FAILED(rv))
    return nsnull;

  // if the child is not us
  if (child != mFrame)
  {
    nsIFrame* prev = nsnull;
    while(child) {
      prev = child;
      rv = child->GetNextSibling(&child);
      if (NS_FAILED(rv)) {
        prev = nsnull;
        break;
      }

      if (child == frame)
        break;
    }

    if (prev) 
    {
      *aAccPreviousSibling = new nsAccessible(prev,mPresContext);
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

  nsIFrame* child;
  nsresult rv = GetFrame()->FirstChild(mPresContext,nsnull,&child);
  if (NS_FAILED(rv))
    return nsnull;

  if (child) 
  {
    *aAccFirstChild = new nsAccessible(child,mPresContext);
    NS_ADDREF(*aAccFirstChild);
    return NS_OK;
  }

  *aAccFirstChild = nsnull;

  return NS_OK;  
}

  /* readonly attribute nsIAccessible accFirstChild; */
NS_IMETHODIMP nsAccessible::GetAccLastChild(nsIAccessible * *aAccFirstChild)
{
  
  // delegate
  if (mAccessible) {
    nsresult rv = mAccessible->GetAccLastChild(aAccFirstChild);
    if (NS_SUCCEEDED(rv))
      return rv;
  }
  

  // failed? Lets do some default behavior
  nsIFrame* child;
  nsresult rv = GetFrame()->FirstChild(mPresContext, nsnull,&child);
  if (NS_FAILED(rv)) {
    *aAccFirstChild = nsnull;
    return NS_ERROR_FAILURE;
  }

  nsIFrame* last = nsnull;
  while(child) 
  {
      last = child;
      rv = child->GetNextSibling(&child);
      if (NS_FAILED(rv)) {
        *aAccFirstChild = nsnull;
        return NS_ERROR_FAILURE;
      }
  }

  *aAccFirstChild = new nsAccessible(last,mPresContext);
  NS_ADDREF(*aAccFirstChild);
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
  *aAccChildCount = 0;

  nsIFrame* child;
  nsresult rv = GetFrame()->FirstChild(mPresContext,nsnull,&child);
  if (NS_FAILED(rv))
    return nsnull;

  if (child) 
  {
    child->GetNextSibling(&child);
    (*aAccChildCount)++;
  }

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

  // failed? Lets do some default behavior
  if (mNode) {
    nsAutoString name;
    mNode->GetNodeName(name);
    *aAccName = name.ToNewUnicode() ;
    return NS_OK;  
  }

  return NS_ERROR_FAILURE;
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

  // failed? Lets do some default behavior
  return NS_ERROR_FAILURE;  
}

NS_IMETHODIMP nsAccessible::SetAccName(const PRUnichar * aAccName) 
{ 
  // delegate
  if (mAccessible) {
    nsresult rv = mAccessible->SetAccName(aAccName);
    if (NS_SUCCEEDED(rv))
      return rv;
  }

  // failed? Lets do some default behavior
  return NS_OK;  
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

  // failed? Lets do some default behavior
  nsAutoString a;
  a.AssignWithConversion("mozilla value");
  *aAccValue = a.ToNewUnicode();
  return NS_OK;  
}

NS_IMETHODIMP nsAccessible::SetAccValue(const PRUnichar * aAccValue) { return NS_OK;  }

  /* readonly attribute wstring accDescription; */
NS_IMETHODIMP nsAccessible::GetAccDescription(PRUnichar * *aAccDescription) 
{ 
  // delegate
  if (mAccessible) {
    nsresult rv = mAccessible->GetAccDescription(aAccDescription);
    if (NS_SUCCEEDED(rv) && *aAccDescription != nsnull)
      return rv;
  }

  // failed? Lets do some default behavior
  nsAutoString a;
  a.AssignWithConversion("mozilla description");
  *aAccDescription = a.ToNewUnicode();
  return NS_OK;  
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

  // failed? Lets do some default behavior
  nsAutoString a;
  a.AssignWithConversion("mozilla role");
  *aAccRole = a.ToNewUnicode();
  return NS_OK;  
}

  /* readonly attribute wstring accState; */
NS_IMETHODIMP nsAccessible::GetAccState(PRUnichar * *aAccState) 
{ 
  // delegate
  if (mAccessible) {
    nsresult rv = mAccessible->GetAccState(aAccState);
    if (NS_SUCCEEDED(rv) && *aAccState != nsnull)
      return rv;
  }

  // failed? Lets do some default behavior
  nsAutoString a;
  a.AssignWithConversion("mozilla state");
  *aAccState = a.ToNewUnicode();
  return NS_OK;  
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
  nsAutoString a;
  a.AssignWithConversion("mozilla help");
  *aAccHelp = a.ToNewUnicode();
  return NS_OK;  
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
  nsIFrame* frame = GetFrame();

  if (!frame)
  {
    *x=*y=*width=*height=0;
    return NS_OK;
  }

  nsPoint offset(0,0);
  nsPoint pos(0,0);
  while(frame) {
    nsIScrollableView* scrollingView;
    nsIView*           view;
    // XXX hack
    frame->GetView(mPresContext, &view);
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
  mPresContext->GetTwipsToPixels(&t2p);

  nsRect r;
  GetFrame()->GetRect(r);
  *x = (PRInt32)(offset.x*t2p);
  *y = (PRInt32)(offset.y*t2p);
  *width = (PRInt32)(r.width*t2p);
  *height = (PRInt32)(r.height*t2p);

  return NS_OK;
}

// helpers

nsIFrame* nsAccessible::GetFrame()
{
  return mFrame;
}
