/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
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
#include "nsIWalletPreview.h"

static NS_DEFINE_IID(kIWalletServiceIID, NS_IWALLETSERVICE_IID);
static NS_DEFINE_IID(kWalletServiceCID, NS_WALLETSERVICE_CID);

class WalletPreviewImpl : public nsIWalletPreview
{
public:
  WalletPreviewImpl();
  virtual ~WalletPreviewImpl();

  // nsISupports interface
  NS_DECL_ISUPPORTS

  // nsIWalletPreview interface
  NS_DECL_NSIWALLETPREVIEW
};

////////////////////////////////////////////////////////////////////////

nsresult
NS_NewWalletPreview(nsIWalletPreview** aWalletPreview)
{
  NS_PRECONDITION(aWalletPreview != nsnull, "null ptr");
  if (!aWalletPreview) {
    return NS_ERROR_NULL_POINTER;
  }
  *aWalletPreview = new WalletPreviewImpl();
  if (! *aWalletPreview) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  NS_ADDREF(*aWalletPreview);
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////

WalletPreviewImpl::WalletPreviewImpl()
{
  NS_INIT_REFCNT();
}

WalletPreviewImpl::~WalletPreviewImpl()
{
}

NS_IMPL_ISUPPORTS(WalletPreviewImpl, nsIWalletPreview::GetIID());

NS_IMETHODIMP
WalletPreviewImpl::GetPrefillValue(char** aValue)
{
  NS_PRECONDITION(aValue != nsnull, "null ptr");
  if (!aValue) {
    return NS_ERROR_NULL_POINTER;
  }
  nsresult res;
  NS_WITH_SERVICE(nsIWalletService, walletservice, kWalletServiceCID, &res);
  if (NS_FAILED(res)) return res;
  nsAutoString walletList;
  res = walletservice->WALLET_GetPrefillListForViewer(walletList);
  if (NS_SUCCEEDED(res)) {
    *aValue = walletList.ToNewCString();
  }
  return res;
}

static void DOMWindowToWebShellWindow(
              nsIDOMWindow *DOMWindow,
              nsCOMPtr<nsIWebShellWindow> *webWindow)
{
  if (!DOMWindow) {
    return; // with webWindow unchanged -- its constructor gives it a null ptr
  }
  nsCOMPtr<nsIScriptGlobalObject> globalScript(do_QueryInterface(DOMWindow));
  nsCOMPtr<nsIWebShell> webshell, rootWebshell;
  if (globalScript) {
    globalScript->GetWebShell(getter_AddRefs(webshell));
  }
  if (webshell) {
    webshell->GetRootWebShellEvenIfChrome(*getter_AddRefs(rootWebshell));
  }
  if (rootWebshell) {
    nsCOMPtr<nsIWebShellContainer> webshellContainer;
    rootWebshell->GetContainer(*getter_AddRefs(webshellContainer));
    *webWindow = do_QueryInterface(webshellContainer);
  }
}

NS_IMETHODIMP
WalletPreviewImpl::SetValue(const char* aValue, nsIDOMWindow* win)
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
  nsCOMPtr<nsIWebShellWindow> parent;
  DOMWindowToWebShellWindow(top, &parent);
  if (parent) {
    parent->Close();
  }
  NS_IF_RELEASE(win);

  /* process the value */
  NS_PRECONDITION(aValue != nsnull, "null ptr");
  if (! aValue) {
    return NS_ERROR_NULL_POINTER;
  }
  nsresult res;
  NS_WITH_SERVICE(nsIWalletService, walletservice, kWalletServiceCID, &res);
  if (NS_FAILED(res)) return res;
  nsAutoString walletList = aValue;
  res = walletservice->WALLET_PrefillReturn(walletList);
  return res;
}
