/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#include "nscore.h"
#include "nsIMemory.h"
#include "plstr.h"
#include "stdio.h"
#include "nsIWalletService.h"
#include "nsIServiceManager.h"
#include "nsIDOMWindowInternal.h"
#include "nsCOMPtr.h"
#include "nsIScriptGlobalObject.h"
#include "nsWalletPreview.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIBaseWindow.h"

static NS_DEFINE_IID(kWalletServiceCID, NS_WALLETSERVICE_CID);

////////////////////////////////////////////////////////////////////////

WalletPreviewImpl::WalletPreviewImpl()
{
  NS_INIT_REFCNT();
}

WalletPreviewImpl::~WalletPreviewImpl()
{
}

NS_IMPL_ISUPPORTS1(WalletPreviewImpl, nsIWalletPreview)

NS_IMETHODIMP
WalletPreviewImpl::GetPrefillValue(PRUnichar** aValue)
{
  NS_PRECONDITION(aValue != nsnull, "null ptr");
  if (!aValue) {
    return NS_ERROR_NULL_POINTER;
  }
  nsresult res;
  nsCOMPtr<nsIWalletService> walletservice = 
           do_GetService(kWalletServiceCID, &res);
  if (NS_FAILED(res)) return res;
  nsAutoString walletList;
  res = walletservice->WALLET_GetPrefillListForViewer(walletList);
  if (NS_SUCCEEDED(res)) {
    *aValue = walletList.ToNewUnicode();
  }
  return res;
}

static void DOMWindowToTreeOwner(
              nsIDOMWindow *DOMWindow,
              nsIDocShellTreeOwner** aTreeOwner)
{
  if (!DOMWindow) {
    return; // with webWindow unchanged -- its constructor gives it a null ptr
  }
  nsCOMPtr<nsIScriptGlobalObject> globalScript(do_QueryInterface(DOMWindow));
  nsCOMPtr<nsIDocShell> docShell;
  if (globalScript) {
    globalScript->GetDocShell(getter_AddRefs(docShell));
  }
  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(docShell));
  if(!docShellAsItem)
   return;

  docShellAsItem->GetTreeOwner(aTreeOwner);
}

NS_IMETHODIMP
WalletPreviewImpl::SetValue(const PRUnichar* aValue, nsIDOMWindowInternal* win)
{
  /* close the window */
  if (!win) {
    return NS_ERROR_FAILURE;
  }
  nsIDOMWindow* top;
  win->GetTop(&top);
  if (!top) {
    return NS_ERROR_FAILURE;
  }


  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  DOMWindowToTreeOwner(top, getter_AddRefs(treeOwner));
  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin(do_QueryInterface(treeOwner));
  if (treeOwnerAsWin) {
    treeOwnerAsWin->Destroy();
  }
  NS_RELEASE(top);

  /* process the value */
  NS_PRECONDITION(aValue != nsnull, "null ptr");
  if (! aValue) {
    return NS_ERROR_NULL_POINTER;
  }
  nsresult res;
  nsCOMPtr<nsIWalletService> walletservice = 
           do_GetService(kWalletServiceCID, &res);
  if (NS_FAILED(res)) return res;
  nsAutoString walletList( aValue );
  res = walletservice->WALLET_PrefillReturn(walletList);
  return res;
}
