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
#include "nsIAllocator.h"
#include "plstr.h"
#include "stdio.h"
#include "nsIWalletService.h"
#include "nsIServiceManager.h"
#include "nsIDOMWindow.h"
#include "nsCOMPtr.h"
#include "nsIWebShell.h"
#include "nsIWebShellWindow.h"
#include "nsIScriptGlobalObject.h"
#include "nsSignonViewer.h"

static NS_DEFINE_IID(kWalletServiceCID, NS_WALLETSERVICE_CID);

////////////////////////////////////////////////////////////////////////

SignonViewerImpl::SignonViewerImpl()
{
  NS_INIT_REFCNT();
}

SignonViewerImpl::~SignonViewerImpl()
{
}

NS_IMPL_ISUPPORTS(SignonViewerImpl, nsISignonViewer::GetIID());

NS_IMETHODIMP
SignonViewerImpl::GetSignonValue(char** aValue)
{
  NS_PRECONDITION(aValue != nsnull, "null ptr");
  if (!aValue) {
    return NS_ERROR_NULL_POINTER;
  }
  nsresult res;
  NS_WITH_SERVICE(nsIWalletService, walletservice, kWalletServiceCID, &res);
  if (NS_FAILED(res)) return res;
  nsAutoString signonList;
  res = walletservice->SI_GetSignonListForViewer(signonList);
  if (NS_SUCCEEDED(res)) {
    *aValue = signonList.ToNewCString();
  }
  return res;
}

NS_IMETHODIMP
SignonViewerImpl::GetRejectValue(char** aValue)
{
  NS_PRECONDITION(aValue != nsnull, "null ptr");
  if (!aValue) {
    return NS_ERROR_NULL_POINTER;
  }
  nsresult res;
  NS_WITH_SERVICE(nsIWalletService, walletservice, kWalletServiceCID, &res);
  if (NS_FAILED(res)) return res;
  nsAutoString rejectList;
  res = walletservice->SI_GetRejectListForViewer(rejectList);
  if (NS_SUCCEEDED(res)) {
    *aValue = rejectList.ToNewCString();
  }
  return res;
}

NS_IMETHODIMP
SignonViewerImpl::GetNopreviewValue(char** aValue)
{
  NS_PRECONDITION(aValue != nsnull, "null ptr");
  if (!aValue) {
    return NS_ERROR_NULL_POINTER;
  }
  nsresult res;
  NS_WITH_SERVICE(nsIWalletService, walletservice, kWalletServiceCID, &res);
  if (NS_FAILED(res)) return res;
  nsAutoString nopreviewList;
  res = walletservice->WALLET_GetNopreviewListForViewer(nopreviewList);
  if (NS_SUCCEEDED(res)) {
    *aValue = nopreviewList.ToNewCString();
  }
  return res;
}

NS_IMETHODIMP
SignonViewerImpl::GetNocaptureValue(char** aValue)
{
  NS_PRECONDITION(aValue != nsnull, "null ptr");
  if (!aValue) {
    return NS_ERROR_NULL_POINTER;
  }
  nsresult res;
  NS_WITH_SERVICE(nsIWalletService, walletservice, kWalletServiceCID, &res);
  if (NS_FAILED(res)) return res;
  nsAutoString nocaptureList;
  res = walletservice->WALLET_GetNocaptureListForViewer(nocaptureList);
  if (NS_SUCCEEDED(res)) {
    *aValue = nocaptureList.ToNewCString();
  }
  return res;
}

NS_IMETHODIMP
SignonViewerImpl::SetValue(const char* aValue, nsIDOMWindow* win)
{
  /* process the value */
  NS_PRECONDITION(aValue != nsnull, "null ptr");
  if (! aValue) {
    return NS_ERROR_NULL_POINTER;
  }
  nsresult res;
  NS_WITH_SERVICE(nsIWalletService, walletservice, kWalletServiceCID, &res);
  if (NS_FAILED(res)) return res;
  nsAutoString walletList = aValue;
  res = walletservice->SI_SignonViewerReturn(walletList);
  return res;
}
