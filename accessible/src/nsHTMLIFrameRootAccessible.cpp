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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
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

#include "nsHTMLIFrameRootAccessible.h"
#include "nsCOMPtr.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsIDocShell.h"
#include "nsIWebShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIXULDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentType.h"
#include "nsINameSpaceManager.h"
#include "nsReadableUtils.h"

NS_INTERFACE_MAP_BEGIN(nsHTMLIFrameRootAccessible)
NS_INTERFACE_MAP_END_INHERITING(nsRootAccessible)

NS_IMPL_ADDREF_INHERITED(nsHTMLIFrameRootAccessible, nsRootAccessible);
NS_IMPL_RELEASE_INHERITED(nsHTMLIFrameRootAccessible, nsRootAccessible);

NS_IMPL_ADDREF_INHERITED(nsHTMLIFrameAccessible, nsHTMLBlockAccessible);
NS_IMPL_RELEASE_INHERITED(nsHTMLIFrameAccessible, nsHTMLBlockAccessible);


NS_IMETHODIMP
nsHTMLIFrameAccessible::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_ASSERTION(aInstancePtr, "QueryInterface requires a non-NULL destination!");
  if ( !aInstancePtr )
    return NS_ERROR_NULL_POINTER;
  if (aIID.Equals(NS_GET_IID(nsIAccessibleDocument))) {
    *aInstancePtr = (void*)(nsIAccessibleDocument*) this;
    NS_IF_ADDREF(this);
    return NS_OK;
  }
  return nsHTMLBlockAccessible::QueryInterface(aIID, aInstancePtr); 
}


nsHTMLIFrameAccessible::nsHTMLIFrameAccessible(nsIDOMNode* aNode, nsIAccessible* aRoot, nsIWeakReference* aShell, nsIDocument *aDoc):
  nsHTMLBlockAccessible(aNode, aShell), mRootAccessible(aRoot), nsDocAccessibleMixin(aDoc)
{
}

  /* attribute wstring accName; */
NS_IMETHODIMP nsHTMLIFrameAccessible::GetAccName(nsAWritableString& aAccName) 
{ 
  return GetTitle(aAccName);
}

NS_IMETHODIMP nsHTMLIFrameAccessible::GetAccValue(nsAWritableString& aAccValue) 
{ 
  return GetURL(aAccValue);
}

/* nsIAccessible getAccFirstChild (); */
NS_IMETHODIMP nsHTMLIFrameAccessible::GetAccFirstChild(nsIAccessible **_retval)
{
  return mRootAccessible->GetAccFirstChild(_retval);
}

/* nsIAccessible getAccLastChild (); */
NS_IMETHODIMP nsHTMLIFrameAccessible::GetAccLastChild(nsIAccessible **_retval)
{
  return mRootAccessible->GetAccLastChild(_retval);
}

/* long getAccChildCount (); */
NS_IMETHODIMP nsHTMLIFrameAccessible::GetAccChildCount(PRInt32 *_retval)
{
  return mRootAccessible->GetAccChildCount(_retval);
}

/* unsigned long getAccRole (); */
NS_IMETHODIMP nsHTMLIFrameAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_PANE;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLIFrameAccessible::GetAccState(PRUint32 *aAccState)
{
  return nsAccessible::GetAccState(aAccState);
}

NS_IMETHODIMP nsHTMLIFrameAccessible::GetURL(nsAWritableString& aURL)
{
  return nsDocAccessibleMixin::GetURL(aURL);
}

NS_IMETHODIMP nsHTMLIFrameAccessible::GetTitle(nsAWritableString& aTitle)
{
  return nsDocAccessibleMixin::GetTitle(aTitle);
}

NS_IMETHODIMP nsHTMLIFrameAccessible::GetMimeType(nsAWritableString& aMimeType)
{
  return nsDocAccessibleMixin::GetMimeType(aMimeType);
}

NS_IMETHODIMP nsHTMLIFrameAccessible::GetDocType(nsAWritableString& aDocType)
{
  return nsDocAccessibleMixin::GetDocType(aDocType);
}

NS_IMETHODIMP nsHTMLIFrameAccessible::GetNameSpaceURIForID(PRInt16 aNameSpaceID, nsAWritableString& aNameSpaceURI)
{
  return nsDocAccessibleMixin::GetNameSpaceURIForID(aNameSpaceID, aNameSpaceURI);
}

NS_IMETHODIMP nsHTMLIFrameAccessible::GetDocument(nsIDocument **doc)
{
  return nsDocAccessibleMixin::GetDocument(doc);
}

//=============================//
// nsHTMLIFrameRootAccessible  //
//=============================//


//-----------------------------------------------------
// construction 
//-----------------------------------------------------
nsHTMLIFrameRootAccessible::nsHTMLIFrameRootAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
  nsRootAccessible(aShell)
{
  mRealDOMNode = aNode;
}

//-----------------------------------------------------
// destruction
//-----------------------------------------------------
nsHTMLIFrameRootAccessible::~nsHTMLIFrameRootAccessible()
{
}

  /* readonly attribute nsIAccessible accParent; */
NS_IMETHODIMP nsHTMLIFrameRootAccessible::GetAccParent(nsIAccessible * *_retval) 
{ 
  nsCOMPtr<nsIAccessible> accessible;
  if (NS_SUCCEEDED(GetHTMLIFrameAccessible(getter_AddRefs(accessible))))
    return accessible->GetAccParent(_retval);

  *_retval = nsnull;
  return NS_OK;
}

  /* nsIAccessible getAccNextSibling (); */
NS_IMETHODIMP nsHTMLIFrameRootAccessible::GetAccNextSibling(nsIAccessible **_retval) 
{
  nsCOMPtr<nsIAccessible> accessible;

  if (NS_SUCCEEDED(GetHTMLIFrameAccessible(getter_AddRefs(accessible))))
    return accessible->GetAccNextSibling(_retval);

  *_retval = nsnull;
  return NS_ERROR_FAILURE;
}

  /* nsIAccessible getAccPreviousSibling (); */
NS_IMETHODIMP nsHTMLIFrameRootAccessible::GetAccPreviousSibling(nsIAccessible **_retval) 
{
  nsCOMPtr<nsIAccessible> accessible;

  if (NS_SUCCEEDED(GetHTMLIFrameAccessible(getter_AddRefs(accessible))))
    return accessible->GetAccPreviousSibling(_retval);

  *_retval = nsnull;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsHTMLIFrameRootAccessible::GetHTMLIFrameAccessible(nsIAccessible** aAcc)
{
  // Start by finding our PresShell and from that
  // we get our nsIDocShell in order to walk the DocShell tree
  nsCOMPtr<nsIPresShell> presShell(do_QueryReferent(mPresShell));
  if (!presShell) {
     *aAcc = nsnull;
     return NS_ERROR_FAILURE;  
  }

  nsCOMPtr<nsIDocShell> docShell;
  if (NS_SUCCEEDED(GetDocShellFromPS(presShell, getter_AddRefs(docShell)))) {
    // Now that we have the DocShell QI 
    // it to a tree item to find it's parent
    nsCOMPtr<nsIDocShellTreeItem> item(do_QueryInterface(docShell));
    if (item) {
      nsCOMPtr<nsIDocShellTreeItem> itemParent;
      item->GetParent(getter_AddRefs(itemParent));
      // QI to get the WebShell for the parent document
      nsCOMPtr<nsIDocShell> parentDocShell(do_QueryInterface(itemParent));
      if (parentDocShell) {
        // Get the PresShell/Content and 
        // Root Content Node of the parent document
        nsCOMPtr<nsIPresShell> parentPresShell;
        nsCOMPtr<nsIPresContext> parentPresContext;
        nsCOMPtr<nsIContent> rootContent;
        if (NS_SUCCEEDED(GetDocShellObjects(parentDocShell,
                                            getter_AddRefs(parentPresShell), 
                                            getter_AddRefs(parentPresContext),
                                            getter_AddRefs(rootContent)))) {
          // QI the DocShell (of this sub-doc) to a webshell
          nsCOMPtr<nsIWebShell> webShell(do_QueryInterface(docShell));
          if (webShell && parentPresShell && parentPresContext && rootContent) {
            // Now, find the Content in the parent document
            // that represents this sub-doc, 
            // we do that matching webshells
            nsCOMPtr<nsIContent> content;
            if (FindContentForWebShell(parentPresShell, 
                                       rootContent, 
                                       webShell,
                                       getter_AddRefs(content))) {
              // OK, we found the content node in the parent doc
              // that corresponds to this sub-doc
              // Get the frame for that content
              nsCOMPtr<nsIWeakReference> wr = do_GetWeakReference(parentPresShell);
              nsIFrame* frame = nsnull;
              parentPresShell->GetPrimaryFrameFor(content, &frame);
#ifdef NS_DEBUG_X
              printf("** Found: Con:%p  Fr:%p", content, frame);
              char * name;
              if (GetNameForFrame(frame, &name)) {
                printf(" Name:[%s]", name);
                nsMemory::Free(name);
              }
              printf("\n");
#endif


              nsCOMPtr<nsIDOMNode> node(do_QueryInterface(content));
              nsCOMPtr<nsIAccessible> acc(do_QueryInterface(frame));

              *aAcc = acc;
              NS_IF_ADDREF(*aAcc);

              return NS_OK;
            }
          }
        }
      }
    }
  }

  return NS_ERROR_FAILURE;
}

