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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Original Author: Aaron Leventhal (aaronl@netscape.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsDocAccessible.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIFrame.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNSDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentType.h"
#include "nsIXULDocument.h"
#include "nsINameSpaceManager.h"
#include "nsIWebNavigation.h"
#include "nsIURI.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIServiceManager.h"

//=============================//
// nsDocAccessible  //
//=============================//

//-----------------------------------------------------
// construction 
//-----------------------------------------------------
nsDocAccessible::nsDocAccessible(nsIDOMNode *aDOMNode, nsIWeakReference* aShell):
  nsAccessibleWrap(aDOMNode, aShell), mWnd(nsnull)
{
  nsCOMPtr<nsIPresShell> shell(do_QueryReferent(mPresShell));
  if (shell) {
    shell->GetDocument(getter_AddRefs(mDocument));
  }
}

//-----------------------------------------------------
// destruction
//-----------------------------------------------------
nsDocAccessible::~nsDocAccessible()
{
}

NS_IMPL_ISUPPORTS_INHERITED1(nsDocAccessible, nsAccessible, nsIAccessibleDocument);

/* void addAccessibleEventListener (in nsIAccessibleEventListener aListener); */
NS_IMETHODIMP nsDocAccessible::AddAccessibleEventListener(nsIAccessibleEventListener *aListener)
{
#ifdef LATER_WHEN_CACHE_IMPLD
  NS_ASSERTION(aListener, "Trying to add a null listener!");
  if (!mListener) {
    mListener = aListener;
    AddContentDocListeners();
  }
#endif
  return NS_OK;
}

/* void removeAccessibleEventListener (); */
NS_IMETHODIMP nsDocAccessible::RemoveAccessibleEventListener()
{
#ifdef LATER_WHEN_CACHE_IMPLD
  if (mListener) {
    RemoveContentDocListeners();
    mListener = nsnull;
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP nsDocAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_PANE;
  return NS_OK;
}

NS_IMETHODIMP nsDocAccessible::GetAccName(nsAString& aAccName)
{
  return NS_OK;
}

NS_IMETHODIMP nsDocAccessible::GetAccValue(nsAString& aAccValue)
{
  return GetURL(aAccValue);
}

// ------- nsIAccessibleDocument Methods (5) ---------------

NS_IMETHODIMP nsDocAccessible::GetURL(nsAString& aURL)
{ 
  nsCOMPtr<nsISupports> container;
  mDocument->GetContainer(getter_AddRefs(container));
  nsCOMPtr<nsIWebNavigation> webNav(do_GetInterface(container));
  nsCAutoString theURL;
  if (webNav) {
    nsCOMPtr<nsIURI> pURI;
    webNav->GetCurrentURI(getter_AddRefs(pURI));
    if (pURI) 
      pURI->GetSpec(theURL);
  }
  //XXXaaronl Need to use CopyUTF8toUCS2(nsDependentCString(theURL), aURL); 
  //          when it's written
  aURL.Assign(NS_ConvertUTF8toUCS2(theURL)); 
  return NS_OK;
}

NS_IMETHODIMP nsDocAccessible::GetTitle(nsAString& aTitle)
{
  return mDocument->GetDocumentTitle(aTitle);                   
}

NS_IMETHODIMP nsDocAccessible::GetMimeType(nsAString& aMimeType)
{
  nsCOMPtr<nsIDOMNSDocument> domnsDocument(do_QueryInterface(mDocument));
  if (domnsDocument) {
    return domnsDocument->GetContentType(aMimeType);
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocAccessible::GetDocType(nsAString& aDocType)
{
  nsCOMPtr<nsIXULDocument> xulDoc(do_QueryInterface(mDocument));
  nsCOMPtr<nsIDOMDocument> domDoc(do_QueryInterface(mDocument));
  nsCOMPtr<nsIDOMDocumentType> docType;

  if (xulDoc) {
    aDocType = NS_LITERAL_STRING("window"); // doctype not implemented for XUL at time of writing - causes assertion
    return NS_OK;
  }
  else if (domDoc && NS_SUCCEEDED(domDoc->GetDoctype(getter_AddRefs(docType))) && docType) {
    return docType->GetName(aDocType);
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocAccessible::GetNameSpaceURIForID(PRInt16 aNameSpaceID, nsAString& aNameSpaceURI)
{
  if (mDocument) {
    nsCOMPtr<nsINameSpaceManager> nameSpaceManager =
        do_GetService(NS_NAMESPACEMANAGER_CONTRACTID);
    if (nameSpaceManager) 
      return nameSpaceManager->GetNameSpaceURI(aNameSpaceID, aNameSpaceURI);
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocAccessible::GetCaretAccessible(nsIAccessibleCaret **aCaretAccessible)
{
  // We only have a caret accessible on the root document
  *aCaretAccessible = nsnull;
  return NS_OK;
}

NS_IMETHODIMP nsDocAccessible::GetWindow(void **aWindow)
{
  *aWindow = mWnd;
  return NS_OK;
}

NS_IMETHODIMP nsDocAccessible::SetWindow(void *aWindow)
{
  mWnd = aWindow;
  return NS_OK;
}

NS_IMETHODIMP nsDocAccessible::Shutdown()
{
  mDocument = nsnull;
  return nsAccessibleWrap::Shutdown();
}

nsIFrame* nsDocAccessible::GetFrame()
{
  nsCOMPtr<nsIPresShell> shell(do_QueryReferent(mPresShell));

  nsIFrame* root = nsnull;
  if (shell) 
    shell->GetRootFrame(&root);

  return root;
}

void nsDocAccessible::GetBounds(nsRect& aBounds, nsIFrame** aRelativeFrame)
{
  *aRelativeFrame = GetFrame();
  if (*aRelativeFrame)
    (*aRelativeFrame)->GetRect(aBounds);
}

