/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nscore.h"
#include "nsIMemory.h"
#include "plstr.h"
#include "stdio.h"
#include "nsIWalletService.h"
#include "nsIServiceManager.h"
#include "nsIDOMWindowInternal.h"
#include "nsCOMPtr.h"
#include "nsIDocShell.h"
#include "nsIScriptGlobalObject.h"
#include "nsWalletEditor.h"

static NS_DEFINE_IID(kWalletServiceCID, NS_WALLETSERVICE_CID);

////////////////////////////////////////////////////////////////////////

WalletEditorImpl::WalletEditorImpl()
{
  NS_INIT_REFCNT();
}

WalletEditorImpl::~WalletEditorImpl()
{
}

NS_IMPL_ISUPPORTS1(WalletEditorImpl, nsIWalletEditor);

NS_IMETHODIMP
WalletEditorImpl::GetValue(PRUnichar** aValue)
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
  res = walletservice->WALLET_PreEdit(walletList);
  if (NS_SUCCEEDED(res)) {
    *aValue = walletList.ToNewUnicode();
  }
  return res;
}

NS_IMETHODIMP
WalletEditorImpl::SetValue(const PRUnichar* aValue, nsIDOMWindowInternal* win)
{
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
  res = walletservice->WALLET_PostEdit(walletList);
  return res;
}
