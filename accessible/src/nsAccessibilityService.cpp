/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s): 
 */

#include "nsIAccessibilityService.h"
#include "nsAccessibilityService.h"
#include "nsAccessible.h"
#include "nsCOMPtr.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsRootAccessible.h"
#include "nsINameSpaceManager.h"
#include "nsMutableAccessible.h"
#include "nsLayoutAtoms.h"


//--------------------

class nsDelegateAccessible : public nsIAccessible
{
protected:

 nsCOMPtr<nsIAccessible> mAccessible;

public:
  NS_DECL_ISUPPORTS
  NS_FORWARD_NSIACCESSIBLE(mAccessible->);
  
  nsDelegateAccessible(nsIAccessible* aAccessible);

};

NS_IMPL_ISUPPORTS1(nsDelegateAccessible, nsIAccessible)

nsDelegateAccessible::nsDelegateAccessible(nsIAccessible* aAccessible)
{
  NS_INIT_ISUPPORTS();
  mAccessible = aAccessible;
}

//---------------------

class nsGenericAccessible : public nsIAccessible
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIACCESSIBLE

  nsGenericAccessible(nsIDOMNode* aNode);

  virtual ~nsGenericAccessible();

  nsCOMPtr<nsIDOMNode> mNode;
};

NS_IMPL_ISUPPORTS1(nsGenericAccessible, nsIAccessible)

nsGenericAccessible::nsGenericAccessible(nsIDOMNode* aNode)
{
  NS_INIT_ISUPPORTS();
  mNode = aNode;
}

nsGenericAccessible::~nsGenericAccessible()
{
}

NS_IMETHODIMP nsGenericAccessible::GetAccParent(nsIAccessible **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsGenericAccessible::GetAccNextSibling(nsIAccessible **_retval)
{ 
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsGenericAccessible::GetAccPreviousSibling(nsIAccessible **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsGenericAccessible::GetAccFirstChild(nsIAccessible **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsGenericAccessible::GetAccLastChild(nsIAccessible **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsGenericAccessible::GetAccChildCount(PRInt32 *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsGenericAccessible::GetAccName(PRUnichar **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsGenericAccessible::GetAccValue(PRUnichar **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsGenericAccessible::SetAccName(const PRUnichar *name)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsGenericAccessible::SetAccValue(const PRUnichar *value)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsGenericAccessible::GetAccDescription(PRUnichar **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsGenericAccessible::GetAccRole(PRUnichar **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsGenericAccessible::GetAccState(PRUnichar **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsGenericAccessible::GetAccDefaultAction(PRUnichar **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsGenericAccessible::GetAccHelp(PRUnichar **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsGenericAccessible::GetAccFocused(PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsGenericAccessible::AccGetAt(PRInt32 x, PRInt32 y, nsIAccessible **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsGenericAccessible::AccNavigateRight(nsIAccessible **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsGenericAccessible::AccNavigateLeft(nsIAccessible **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsGenericAccessible::AccNavigateUp(nsIAccessible **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsGenericAccessible::AccNavigateDown(nsIAccessible **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsGenericAccessible::AccGetBounds(PRInt32 *x, PRInt32 *y, PRInt32 *width, PRInt32 *height)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsGenericAccessible::AccAddSelection()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsGenericAccessible::AccRemoveSelection()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsGenericAccessible::AccExtendSelection()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsGenericAccessible::AccTakeSelection()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsGenericAccessible::AccTakeFocus()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsGenericAccessible::AccDoDefaultAction()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


//--------------------

class nsExtenderAccessible : public nsDelegateAccessible
{
public:
  
  nsExtenderAccessible(nsIAtom* aPopupAtom, nsIAccessible* aAccessible, nsIContent* aContent, nsIWeakReference* aShell);
  virtual ~nsExtenderAccessible() {}

  NS_IMETHOD GetAccNextSibling(nsIAccessible **_retval);
  NS_IMETHOD GetAccPreviousSibling(nsIAccessible **_retval);

  nsCOMPtr<nsIContent> mContent;
  nsWeakPtr mPresShell;
  nsCOMPtr<nsIAtom> mPopupAtom;
};

class nsHTMLSelectWindowAccessible : public nsAccessible
{
public:
  
  nsHTMLSelectWindowAccessible(nsIAtom* aPopupAtom, nsIAccessible* aParent, nsIAccessible* aPrev, nsIAccessible* aAccessible, nsIContent* aContent, nsIWeakReference* aShell);
  virtual ~nsHTMLSelectWindowAccessible() {}

  NS_IMETHOD GetAccParent(nsIAccessible **_retval);
  NS_IMETHOD GetAccNextSibling(nsIAccessible **_retval);
  NS_IMETHOD GetAccPreviousSibling(nsIAccessible **_retval);
  NS_IMETHOD GetAccLastChild(nsIAccessible **_retval);
  NS_IMETHOD GetAccFirstChild(nsIAccessible **_retval);
  NS_IMETHOD GetAccChildCount(PRInt32 *_retval);
  NS_IMETHOD GetAccName(PRUnichar **_retval);
  NS_IMETHOD GetAccRole(PRUnichar **_retval);
  NS_IMETHOD AccGetBounds(PRInt32 *x, PRInt32 *y, PRInt32 *width, PRInt32 *height);

  nsCOMPtr<nsIAccessible> mParent;
  nsCOMPtr<nsIAccessible> mPrev;
  nsCOMPtr<nsIAtom> mPopupAtom;
};

class nsHTMLSelectAccessible : public nsAccessible
{
public:
  
  nsHTMLSelectAccessible(nsIAtom* aPopupAtom, nsIAccessible* aAccessible, nsIContent* aContent, nsIWeakReference* aShell);
  NS_IMETHOD GetAccLastChild(nsIAccessible **_retval);
  NS_IMETHOD GetAccFirstChild(nsIAccessible **_retval);
  NS_IMETHOD GetAccName(PRUnichar **_retval);
  NS_IMETHOD GetAccRole(PRUnichar **_retval);
  NS_IMETHOD GetAccChildCount(PRInt32 *_retval);

  virtual ~nsHTMLSelectAccessible() {}

  nsCOMPtr<nsIAtom> mPopupAtom;
};

class nsHTMLSelectListAccessible : public nsAccessible
{
public:
  
  nsHTMLSelectListAccessible(nsIAtom* aPopupAtom, nsIAccessible* aAccessible, nsIContent* aContent, nsIWeakReference* aShell);
  virtual ~nsHTMLSelectListAccessible() {}

  NS_IMETHOD GetAccName(PRUnichar **_retval);
  NS_IMETHOD GetAccRole(PRUnichar **_retval);

  virtual nsIAtom* GetListName() { return mPopupAtom; }

  nsCOMPtr<nsIAtom> mPopupAtom;
};
 
nsHTMLSelectAccessible::nsHTMLSelectAccessible(nsIAtom* aPopupAtom, nsIAccessible* aAccessible, 
                                               nsIContent* aContent, 
                                               nsIWeakReference* aShell)
                                               :nsAccessible(aAccessible, aContent, aShell)
{
  mPopupAtom = aPopupAtom;
}

NS_IMETHODIMP nsHTMLSelectAccessible::GetAccName(PRUnichar **_retval)
{
  nsAutoString a;
  a.AssignWithConversion("Combo Box");
  *_retval = a.ToNewUnicode();
  return NS_OK;
}

NS_IMETHODIMP nsHTMLSelectAccessible::GetAccRole(PRUnichar **_retval)
{
  nsAutoString a;
  a.AssignWithConversion("combo box");
  *_retval = a.ToNewUnicode();
  return NS_OK;
}

NS_IMETHODIMP nsHTMLSelectAccessible::GetAccLastChild(nsIAccessible **_retval)
{

  // get the last child. Wrap it with a connector that connects it to the window accessible
  nsCOMPtr<nsIAccessible> last;
  nsresult rv = nsAccessible::GetAccLastChild(getter_AddRefs(last));
  if (NS_FAILED(rv))
    return rv;

  if (last) {
    *_retval = new nsExtenderAccessible(mPopupAtom, last, mContent, mPresShell);
  } else {
    // we have a parent but not previous
    nsCOMPtr<nsIAccessible> parent;
    GetAccParent(getter_AddRefs(parent));
    *_retval = new nsHTMLSelectWindowAccessible(mPopupAtom, parent, nsnull, nsnull, mContent, mPresShell);
  }

   NS_ADDREF(*_retval);

   return NS_OK;
}

NS_IMETHODIMP nsHTMLSelectAccessible::GetAccFirstChild(nsIAccessible **_retval)
{
  // get the last child. Wrap it with a connector that connects it to the window accessible
  nsCOMPtr<nsIAccessible> first;
  nsresult rv = nsAccessible::GetAccFirstChild(getter_AddRefs(first));
  if (NS_FAILED(rv))
    return rv;

  if (first) {
    *_retval = new nsExtenderAccessible(mPopupAtom, first, mContent, mPresShell);
  } else {
    nsCOMPtr<nsIAccessible> parent;
    GetAccParent(getter_AddRefs(parent));
    *_retval = new nsHTMLSelectWindowAccessible(mPopupAtom, parent, nsnull, nsnull, mContent, mPresShell);;
  }

  NS_ADDREF(*_retval);

  return NS_OK;
}

NS_IMETHODIMP nsHTMLSelectAccessible::GetAccChildCount(PRInt32 *_retval)
{
  nsresult rv = nsAccessible::GetAccChildCount(_retval);
  if (NS_FAILED(rv))
    return rv;

  // always have one more that is our window child
  (*_retval)++;
  return NS_OK;
}

//--------------------

nsExtenderAccessible::nsExtenderAccessible(nsIAtom* aPopupAtom, nsIAccessible* aAccessible, nsIContent* aContent, nsIWeakReference* aShell):
nsDelegateAccessible(aAccessible)
{
  mContent = aContent;
  mPresShell = aShell;
  mPopupAtom = aPopupAtom;
}

NS_IMETHODIMP nsExtenderAccessible::GetAccNextSibling(nsIAccessible **_retval)
{ 
  printf("extender next\n");
  nsCOMPtr<nsIAccessible> next;
  nsresult rv = nsDelegateAccessible::GetAccNextSibling(getter_AddRefs(next));
  if (NS_FAILED(rv))
    return rv;

  if (next) {
    *_retval = new nsExtenderAccessible(mPopupAtom, next, mContent, mPresShell);
    NS_ADDREF(*_retval);
    return NS_OK;
  }
  
  // ok no more siblings. Lets create our window
  nsCOMPtr<nsIAccessible> parent;
  GetAccParent(getter_AddRefs(parent));

  *_retval = new nsHTMLSelectWindowAccessible(mPopupAtom, parent, this, nsnull, mContent, mPresShell);
  NS_ADDREF(*_retval);

  return NS_OK;
} 

NS_IMETHODIMP nsExtenderAccessible::GetAccPreviousSibling(nsIAccessible **_retval)
{ 
  nsCOMPtr<nsIAccessible> prev;
  nsresult rv = nsDelegateAccessible::GetAccPreviousSibling(getter_AddRefs(prev));
  if (NS_FAILED(rv))
    return rv;

  if (prev) {
    *_retval = new nsExtenderAccessible(mPopupAtom, prev, mContent, mPresShell);
    NS_ADDREF(*_retval);
    return NS_OK;
  }
 
  *_retval = nsnull;
  return NS_OK;
} 


//---------------------


nsHTMLSelectWindowAccessible::nsHTMLSelectWindowAccessible(nsIAtom* aPopupAtom, nsIAccessible* aParent, nsIAccessible* aPrev, nsIAccessible* aAccessible, nsIContent* aContent, nsIWeakReference* aShell)
:nsAccessible(aAccessible, aContent, aShell)
{
  mParent = aParent;
  mPrev = aPrev;
  mPopupAtom = aPopupAtom;
}

NS_IMETHODIMP nsHTMLSelectWindowAccessible::GetAccParent(nsIAccessible **_retval)
{   
    *_retval = mParent;
    NS_ADDREF(*_retval);
    return NS_OK;
}

NS_IMETHODIMP nsHTMLSelectWindowAccessible::GetAccName(PRUnichar **_retval)
{
  nsAutoString a;
  a.AssignWithConversion("Window");
  *_retval = a.ToNewUnicode();
  return NS_OK;
}
 
NS_IMETHODIMP nsHTMLSelectWindowAccessible::GetAccRole(PRUnichar **_retval)
{
  nsAutoString a;
  a.AssignWithConversion("window");
  *_retval = a.ToNewUnicode();
  return NS_OK;
}
 
NS_IMETHODIMP nsHTMLSelectWindowAccessible::GetAccPreviousSibling(nsIAccessible **_retval)
{ 
  *_retval = mPrev;
  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP nsHTMLSelectWindowAccessible::GetAccNextSibling(nsIAccessible **_retval)
{
  *_retval = nsnull;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLSelectWindowAccessible::GetAccLastChild(nsIAccessible **_retval)
{
  *_retval = new nsHTMLSelectListAccessible(mPopupAtom, nsnull, mContent, mPresShell);
  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP nsHTMLSelectWindowAccessible::GetAccFirstChild(nsIAccessible **_retval)
{
  *_retval = new nsHTMLSelectListAccessible(mPopupAtom, nsnull, mContent, mPresShell);
  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP nsHTMLSelectWindowAccessible::GetAccChildCount(PRInt32 *_retval)
{
  *_retval = 1;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLSelectWindowAccessible::AccGetBounds(PRInt32 *x, PRInt32 *y, PRInt32 *width, PRInt32 *height)
{
  *x = *y = *width = *height = 0;
  return NS_OK;
}

//----------


nsHTMLSelectListAccessible::nsHTMLSelectListAccessible(nsIAtom* aPopupAtom, nsIAccessible* aAccessible, nsIContent* aContent, nsIWeakReference* aShell)
:nsAccessible(aAccessible, aContent, aShell)
{
    mPopupAtom = aPopupAtom;
}

NS_IMETHODIMP nsHTMLSelectListAccessible::GetAccName(PRUnichar **_retval)
{
  nsAutoString a;
  a.AssignWithConversion("List");
  *_retval = a.ToNewUnicode();
  return NS_OK;
}

NS_IMETHODIMP nsHTMLSelectListAccessible::GetAccRole(PRUnichar **_retval)
{
  nsAutoString a;
  a.AssignWithConversion("list");
  *_retval = a.ToNewUnicode();
  return NS_OK;
}
 
// ---------

nsAccessibilityService::nsAccessibilityService()
{
	NS_INIT_REFCNT();
}

nsAccessibilityService::~nsAccessibilityService()
{
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsAccessibilityService, nsIAccessibilityService);




////////////////////////////////////////////////////////////////////////////////
// nsIAccessibilityService methods:

NS_IMETHODIMP 
nsAccessibilityService::CreateRootAccessible(nsISupports* aPresContext, nsISupports* aFrame, nsIAccessible **_retval)
{
  nsIFrame* f = (nsIFrame*)aFrame;

  nsCOMPtr<nsIPresContext> c = do_QueryInterface(aPresContext);
  NS_ASSERTION(c,"Error non prescontext passed to accessible factory!!!");

  nsCOMPtr<nsIPresShell> s;
  c->GetShell(getter_AddRefs(s));

  NS_ASSERTION(s,"Error not presshell!!");

  nsCOMPtr<nsIWeakReference> wr = getter_AddRefs(NS_GetWeakReference(s));

  *_retval = new nsRootAccessible(wr,f);
  NS_ADDREF(*_retval);

  return NS_OK;
}

NS_IMETHODIMP 
nsAccessibilityService::CreateMutableAccessible(nsISupports* aNode, nsIMutableAccessible **_retval)
{
  *_retval = new nsMutableAccessible(aNode);
  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP 
nsAccessibilityService::CreateHTMLBlockAccessible(nsIAccessible* aAccessible, nsIDOMNode* node, nsISupports* aPresContext, nsIAccessible **_retval)
{
  nsCOMPtr<nsIContent> n = do_QueryInterface(node);
  NS_ASSERTION(n,"Error non nsIContent passed to accessible factory!!!");

  nsCOMPtr<nsIPresContext> c = do_QueryInterface(aPresContext);
  NS_ASSERTION(c,"Error non prescontext passed to accessible factory!!!");

  nsCOMPtr<nsIPresShell> s;
  c->GetShell(getter_AddRefs(s));

  NS_ASSERTION(s,"Error not presshell!!");

  nsCOMPtr<nsIWeakReference> wr = getter_AddRefs(NS_GetWeakReference(s));

  *_retval = new nsHTMLBlockAccessible(aAccessible, n,wr);
  NS_ADDREF(*_retval);

  return NS_OK;
}

NS_IMETHODIMP 
nsAccessibilityService::CreateHTMLSelectAccessible(nsIAtom* aPopupAtom, nsIDOMNode* node, nsISupports* aPresContext, nsIAccessible **_retval)
{
  nsCOMPtr<nsIContent> n = do_QueryInterface(node);
  NS_ASSERTION(n,"Error non nsIContent passed to accessible factory!!!");

  nsCOMPtr<nsIPresContext> c = do_QueryInterface(aPresContext);
  NS_ASSERTION(c,"Error non prescontext passed to accessible factory!!!");

  nsCOMPtr<nsIPresShell> s;
  c->GetShell(getter_AddRefs(s)); 

  nsCOMPtr<nsIWeakReference> wr = getter_AddRefs(NS_GetWeakReference(s));

  *_retval = new nsHTMLSelectAccessible(aPopupAtom, nsnull, n, wr);
  NS_ADDREF(*_retval);
  return NS_OK;

}

//////////////////////////////////////////////////////////////////////

nsresult
NS_NewAccessibilityService(nsIAccessibilityService** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    nsAccessibilityService* a = new nsAccessibilityService();
    if (a == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(a);
    *aResult = a;
    return NS_OK;
}

