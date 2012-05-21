/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Local Includes
#include "nsDOMWindowList.h"

// Helper classes
#include "nsCOMPtr.h"

// Interfaces needed
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMWindow.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIScriptGlobalObject.h"
#include "nsIWebNavigation.h"

nsDOMWindowList::nsDOMWindowList(nsIDocShell *aDocShell)
{
  SetDocShell(aDocShell);
}

nsDOMWindowList::~nsDOMWindowList()
{
}

NS_IMPL_ADDREF(nsDOMWindowList)
NS_IMPL_RELEASE(nsDOMWindowList)

NS_INTERFACE_MAP_BEGIN(nsDOMWindowList)
   NS_INTERFACE_MAP_ENTRY(nsIDOMWindowCollection)
   NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
nsDOMWindowList::SetDocShell(nsIDocShell* aDocShell)
{
  nsCOMPtr<nsIDocShellTreeNode> docShellAsNode(do_QueryInterface(aDocShell));
  mDocShellNode = docShellAsNode; // Weak Reference

  return NS_OK;
}

NS_IMETHODIMP 
nsDOMWindowList::GetLength(PRUint32* aLength)
{
  nsresult rv = NS_OK;

  *aLength = 0;

  nsCOMPtr<nsIWebNavigation> shellAsNav(do_QueryInterface(mDocShellNode));

  if (shellAsNav) {
    nsCOMPtr<nsIDOMDocument> domdoc;
    shellAsNav->GetDocument(getter_AddRefs(domdoc));

    nsCOMPtr<nsIDocument> doc(do_QueryInterface(domdoc));

    if (doc) {
      doc->FlushPendingNotifications(Flush_ContentAndNotify);
    }
  }

  // The above flush might cause mDocShellNode to be cleared, so we
  // need to check that it's still non-null here.

  if (mDocShellNode) {
    PRInt32 length;
    rv = mDocShellNode->GetChildCount(&length);

    *aLength = length;
  }

  return rv;
}

NS_IMETHODIMP 
nsDOMWindowList::Item(PRUint32 aIndex, nsIDOMWindow** aReturn)
{
  nsCOMPtr<nsIDocShellTreeItem> item;

  *aReturn = nsnull;

  nsCOMPtr<nsIWebNavigation> shellAsNav = do_QueryInterface(mDocShellNode);

  if (shellAsNav) {
    nsCOMPtr<nsIDOMDocument> domdoc;
    shellAsNav->GetDocument(getter_AddRefs(domdoc));

    nsCOMPtr<nsIDocument> doc = do_QueryInterface(domdoc);

    if (doc) {
      doc->FlushPendingNotifications(Flush_ContentAndNotify);
    }
  }

  // The above flush might cause mDocShellNode to be cleared, so we
  // need to check that it's still non-null here.

  if (mDocShellNode) {
    mDocShellNode->GetChildAt(aIndex, getter_AddRefs(item));

    nsCOMPtr<nsIScriptGlobalObject> globalObject(do_GetInterface(item));
    NS_ASSERTION(!item || (item && globalObject),
                 "Couldn't get to the globalObject");

    if (globalObject) {
      CallQueryInterface(globalObject, aReturn);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP 
nsDOMWindowList::NamedItem(const nsAString& aName, nsIDOMWindow** aReturn)
{
  nsCOMPtr<nsIDocShellTreeItem> item;

  *aReturn = nsnull;

  nsCOMPtr<nsIWebNavigation> shellAsNav(do_QueryInterface(mDocShellNode));

  if (shellAsNav) {
    nsCOMPtr<nsIDOMDocument> domdoc;
    shellAsNav->GetDocument(getter_AddRefs(domdoc));

    nsCOMPtr<nsIDocument> doc(do_QueryInterface(domdoc));

    if (doc) {
      doc->FlushPendingNotifications(Flush_ContentAndNotify);
    }
  }

  // The above flush might cause mDocShellNode to be cleared, so we
  // need to check that it's still non-null here.

  if (mDocShellNode) {
    mDocShellNode->FindChildWithName(PromiseFlatString(aName).get(),
                                     false, false, nsnull,
                                     nsnull, getter_AddRefs(item));

    nsCOMPtr<nsIScriptGlobalObject> globalObject(do_GetInterface(item));
    if (globalObject) {
      CallQueryInterface(globalObject.get(), aReturn);
    }
  }

  return NS_OK;
}
