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
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsIDOMDocument.h"
#include "nsReadableUtils.h"

NS_INTERFACE_MAP_BEGIN(nsHTMLIFrameRootAccessible)
NS_INTERFACE_MAP_END_INHERITING(nsRootAccessible)

NS_IMPL_ADDREF_INHERITED(nsHTMLIFrameRootAccessible, nsRootAccessible);
NS_IMPL_RELEASE_INHERITED(nsHTMLIFrameRootAccessible, nsRootAccessible);

NS_IMPL_ADDREF_INHERITED(nsHTMLIFrameAccessible, nsBlockAccessible);
NS_IMPL_RELEASE_INHERITED(nsHTMLIFrameAccessible, nsBlockAccessible);


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
  return nsBlockAccessible::QueryInterface(aIID, aInstancePtr); 
}


nsHTMLIFrameAccessible::nsHTMLIFrameAccessible(nsIDOMNode* aNode, nsIAccessible* aRoot, nsIWeakReference* aShell, nsIDocument *aDoc):
  nsBlockAccessible(aNode, aShell), mRootAccessible(aRoot), nsDocAccessibleMixin(aDoc)
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
  mOuterNode(aNode), nsRootAccessible(aShell)
{
}

//-----------------------------------------------------
// destruction
//-----------------------------------------------------
nsHTMLIFrameRootAccessible::~nsHTMLIFrameRootAccessible()
{
}

nsHTMLIFrameRootAccessible::Init()
{
  if (!mOuterAccessible) {
    nsCOMPtr<nsIDOMDocument> domDoc;
    mOuterNode->GetOwnerDocument(getter_AddRefs(domDoc));
    nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));
    if (doc) {
      nsCOMPtr<nsIPresShell> parentShell;
      doc->GetShellAt(0, getter_AddRefs(parentShell));
      if (parentShell) {
        nsCOMPtr<nsIContent> content(do_QueryInterface(mOuterNode));
        nsIFrame* frame = nsnull;
        parentShell->GetPrimaryFrameFor(content, &frame);
        NS_ASSERTION(frame, "No outer frame.");
        frame->GetAccessible(getter_AddRefs(mOuterAccessible));
        NS_ASSERTION(mOuterAccessible, "Something's wrong - there's no accessible for the outer parent of this frame.");
      }
    }
  }
}
  /* readonly attribute nsIAccessible accParent; */
NS_IMETHODIMP nsHTMLIFrameRootAccessible::GetAccParent(nsIAccessible * *_retval) 
{ 
  Init();
  return mOuterAccessible->GetAccParent(_retval);
}

  /* nsIAccessible getAccNextSibling (); */
NS_IMETHODIMP nsHTMLIFrameRootAccessible::GetAccNextSibling(nsIAccessible **_retval) 
{
  Init();
  return mOuterAccessible->GetAccNextSibling(_retval);
}

NS_IMETHODIMP nsHTMLIFrameRootAccessible::GetAccPreviousSibling(nsIAccessible **_retval) 
{
  Init();
  return mOuterAccessible->GetAccPreviousSibling(_retval);
}
