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
 *    travis@netscape.com 
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
  NS_INIT_REFCNT();
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
    NS_WARN_IF_FALSE(!item || (item && globalObject),
                     "Couldn't get to the globalObject");

    if (globalObject) {
      CallQueryInterface(globalObject, aReturn);
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

    mDocShellNode->FindChildWithName(PromiseFlatString(aName).get(),
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
