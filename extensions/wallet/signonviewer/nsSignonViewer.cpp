/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#include "nscore.h"
#include "nsIMemory.h"
#include "plstr.h"
#include "stdio.h"
#include "nsIWalletService.h"
#include "nsIServiceManager.h"
#include "nsIDOMWindowInternal.h"
#include "nsCOMPtr.h"
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

NS_IMPL_ISUPPORTS1(SignonViewerImpl, nsISignonViewer)

NS_IMETHODIMP
SignonViewerImpl::GetNopreviewValue(PRUnichar** aValue)
{
  NS_PRECONDITION(aValue != nsnull, "null ptr");
  if (!aValue) {
    return NS_ERROR_NULL_POINTER;
  }
  nsresult res;
  nsCOMPtr<nsIWalletService> walletservice = 
           do_GetService(kWalletServiceCID, &res);
  if (NS_FAILED(res)) return res;
  nsAutoString nopreviewList;
  res = walletservice->WALLET_GetNopreviewListForViewer(nopreviewList);
  if (NS_SUCCEEDED(res)) {
    *aValue = nopreviewList.ToNewUnicode();
  }
  return res;
}

NS_IMETHODIMP
SignonViewerImpl::GetNocaptureValue(PRUnichar** aValue)
{
  NS_PRECONDITION(aValue != nsnull, "null ptr");
  if (!aValue) {
    return NS_ERROR_NULL_POINTER;
  }
  nsresult res;
  nsCOMPtr<nsIWalletService> walletservice = 
           do_GetService(kWalletServiceCID, &res);
  if (NS_FAILED(res)) return res;
  nsAutoString nocaptureList;
  res = walletservice->WALLET_GetNocaptureListForViewer(nocaptureList);
  if (NS_SUCCEEDED(res)) {
    *aValue = nocaptureList.ToNewUnicode();
  }
  return res;
}

NS_IMETHODIMP
SignonViewerImpl::SetValue(const PRUnichar* aValue, nsIDOMWindowInternal* win)
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
  res = walletservice->SI_SignonViewerReturn(walletList);
  return res;
}
