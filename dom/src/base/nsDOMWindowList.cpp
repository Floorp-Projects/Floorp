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
 *    travis@netscape.com 
 */

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
#include "nsIScriptGlobalObject.h"
#include "nsIWebNavigation.h"

nsDOMWindowList::nsDOMWindowList(nsIDocShell *aDocShell)
{
  NS_INIT_REFCNT();
  mScriptObject = nsnull;
  SetDocShell(aDocShell);
}

nsDOMWindowList::~nsDOMWindowList()
{
}

NS_IMPL_ADDREF(nsDOMWindowList)
NS_IMPL_RELEASE(nsDOMWindowList)

NS_INTERFACE_MAP_BEGIN(nsDOMWindowList)
   NS_INTERFACE_MAP_ENTRY(nsIDOMWindowCollection)
   NS_INTERFACE_MAP_ENTRY(nsIScriptObjectOwner)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMWindowCollection)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
nsDOMWindowList::SetDocShell(nsIDocShell* aDocShell)
{
  nsCOMPtr<nsIDocShellTreeNode> docShellAsNode(do_QueryInterface(aDocShell));
  mDocShellNode = docShellAsNode.get(); // Weak Reference

  return NS_OK;
}

NS_IMETHODIMP 
nsDOMWindowList::GetLength(PRUint32* aLength)
{
  nsresult ret = NS_OK;
  PRInt32 length;

  *aLength = 0;
  if (mDocShellNode) {
    nsCOMPtr<nsIWebNavigation> shellAsNav = do_QueryInterface(mDocShellNode);
    if (shellAsNav) {
      nsCOMPtr<nsIDOMDocument> domdoc;
      
      shellAsNav->GetDocument(getter_AddRefs(domdoc));
      if (domdoc) {
        nsCOMPtr<nsIDocument> doc = do_QueryInterface(domdoc);
        if (doc) {
          doc->FlushPendingNotifications();
        }
      }
    }
    
    ret = mDocShellNode->GetChildCount(&length);
    *aLength = length;
  }

  return ret;
}

NS_IMETHODIMP 
nsDOMWindowList::Item(PRUint32 aIndex, nsIDOMWindow** aReturn)
{
  nsCOMPtr<nsIDocShellTreeItem> item;

  *aReturn = nsnull;
  if (mDocShellNode) {
    nsCOMPtr<nsIWebNavigation> shellAsNav = do_QueryInterface(mDocShellNode);
    if (shellAsNav) {
      nsCOMPtr<nsIDOMDocument> domdoc;
      
      shellAsNav->GetDocument(getter_AddRefs(domdoc));
      if (domdoc) {
        nsCOMPtr<nsIDocument> doc = do_QueryInterface(domdoc);
        if (doc) {
          doc->FlushPendingNotifications();
        }
      }
    }

    mDocShellNode->GetChildAt(aIndex, getter_AddRefs(item));
    
    nsCOMPtr<nsIScriptGlobalObject> globalObject(do_GetInterface(item));
    NS_ASSERTION(globalObject, "Couldn't get to the globalObject");
    if (globalObject) {
      CallQueryInterface(globalObject.get(), aReturn);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP 
nsDOMWindowList::NamedItem(const nsAReadableString& aName, nsIDOMWindow** aReturn)
{
  nsCOMPtr<nsIDocShellTreeItem> item;

  *aReturn = nsnull;
  if (mDocShellNode) {
    nsCOMPtr<nsIWebNavigation> shellAsNav = do_QueryInterface(mDocShellNode);
    if (shellAsNav) {
      nsCOMPtr<nsIDOMDocument> domdoc;
      
      shellAsNav->GetDocument(getter_AddRefs(domdoc));
      if (domdoc) {
        nsCOMPtr<nsIDocument> doc = do_QueryInterface(domdoc);
        if (doc) {
          doc->FlushPendingNotifications();
        }
      }
    }

    mDocShellNode->FindChildWithName(nsPromiseFlatString(aName).get(),
                                     PR_FALSE, PR_FALSE,
                                     nsnull, getter_AddRefs(item));

    nsCOMPtr<nsIScriptGlobalObject> globalObject(do_GetInterface(item));
    NS_ASSERTION(globalObject, "Couldn't get to the globalObject");
    if (globalObject) {
      CallQueryInterface(globalObject.get(), aReturn);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsDOMWindowList::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
  nsresult res = NS_OK;
  nsIScriptGlobalObject *global = aContext->GetGlobalObject();

  if (nsnull == mScriptObject) {
    res = NS_NewScriptWindowCollection(aContext, (nsISupports *)(nsIDOMWindowCollection *)this, global, (void**)&mScriptObject);
  }
  *aScriptObject = mScriptObject;

  NS_RELEASE(global);
  return res;
}

NS_IMETHODIMP 
nsDOMWindowList::SetScriptObject(void *aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}
