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
#ifdef NECKO
#include "nsICookieService.h"
#else
#include "nsINetService.h"
#endif
#include "nsIServiceManager.h"
#include "nsIDOMWindow.h"
#include "nsCOMPtr.h"
#include "nsIWebShell.h"
#include "nsIWebShellWindow.h"
#include "nsIScriptGlobalObject.h"
#include "nsICookieViewer.h"

#ifdef NECKO
static NS_DEFINE_IID(kICookieServiceIID, NS_ICOOKIESERVICE_IID);
static NS_DEFINE_IID(kCookieServiceCID, NS_COOKIESERVICE_CID);
#else
static NS_DEFINE_IID(kINetServiceIID, NS_INETSERVICE_IID);
static NS_DEFINE_IID(kNetServiceCID, NS_NETSERVICE_CID);
#endif

class CookieViewerImpl : public nsICookieViewer
{
public:
  CookieViewerImpl();
  virtual ~CookieViewerImpl();

  // nsISupports interface
  NS_DECL_ISUPPORTS

  // nsICookieViewer interface
  NS_DECL_NSICOOKIEVIEWER
};

////////////////////////////////////////////////////////////////////////

nsresult
NS_NewCookieViewer(nsICookieViewer** aCookieViewer)
{
  NS_PRECONDITION(aCookieViewer != nsnull, "null ptr");
  if (!aCookieViewer) {
    return NS_ERROR_NULL_POINTER;
  }
  *aCookieViewer = new CookieViewerImpl();
  if (! *aCookieViewer) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  NS_ADDREF(*aCookieViewer);
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////

CookieViewerImpl::CookieViewerImpl()
{
  NS_INIT_REFCNT();
}

CookieViewerImpl::~CookieViewerImpl()
{
}

NS_IMPL_ISUPPORTS(CookieViewerImpl, nsICookieViewer::GetIID());

NS_IMETHODIMP
CookieViewerImpl::GetCookieValue(char** aValue)
{
  NS_PRECONDITION(aValue != nsnull, "null ptr");
  if (!aValue) {
    return NS_ERROR_NULL_POINTER;
  }

  nsresult res;
#ifdef NECKO
  NS_WITH_SERVICE(nsICookieService, cookieservice, kCookieServiceCID, &res);
  if (NS_FAILED(res)) return res;
  nsAutoString cookieList;
  res = cookieservice->Cookie_GetCookieListForViewer(cookieList);
  if (NS_SUCCEEDED(res)) {
    *aValue = cookieList.ToNewCString();
  }
#else
  nsINetService *netservice;
  res = nsServiceManager::GetService(kNetServiceCID,
                                     kINetServiceIID,
                                     (nsISupports **)&netservice);
  if ((NS_SUCCEEDED(res)) && (nsnull != netservice)) {
    nsAutoString cookieList;
    res = netservice->Cookie_GetCookieListForViewer(cookieList);
    if (NS_SUCCEEDED(res)) {
      *aValue = cookieList.ToNewCString();
    }
  }
#endif
  return res;
}

NS_IMETHODIMP
CookieViewerImpl::GetPermissionValue(char** aValue)
{
  NS_PRECONDITION(aValue != nsnull, "null ptr");
  if (!aValue) {
    return NS_ERROR_NULL_POINTER;
  }

  nsresult res;
#ifdef NECKO
  NS_WITH_SERVICE(nsICookieService, cookieservice, kCookieServiceCID, &res);
  if (NS_FAILED(res)) return res;
  nsAutoString PermissionList;
  res = cookieservice->Cookie_GetPermissionListForViewer(PermissionList);
  if (NS_SUCCEEDED(res)) {
    *aValue = PermissionList.ToNewCString();
  }
#else
  nsINetService *netservice;
  res = nsServiceManager::GetService(kNetServiceCID,
                                     kINetServiceIID,
                                     (nsISupports **)&netservice);
  if ((NS_SUCCEEDED(res)) && (nsnull != netservice)) {
    nsAutoString PermissionList;
    res = netservice->Cookie_GetPermissionListForViewer(PermissionList);
    if (NS_SUCCEEDED(res)) {
      *aValue = PermissionList.ToNewCString();
    }
  }
#endif
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
CookieViewerImpl::SetValue(const char* aValue, nsIDOMWindow* win)
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
#ifdef NECKO
  NS_WITH_SERVICE(nsICookieService, cookieservice, kCookieServiceCID, &res);
  if (NS_FAILED(res)) return res;
  nsAutoString netList = aValue;
  res = cookieservice->Cookie_CookieViewerReturn(netList);
#else
  nsINetService *netservice;
  res = nsServiceManager::GetService(kNetServiceCID,
                                     kINetServiceIID,
                                     (nsISupports **)&netservice);
  if ((NS_SUCCEEDED(res)) && (nsnull != netservice)) {
    nsAutoString netList = aValue;
    res = netservice->Cookie_CookieViewerReturn(netList);
  }
#endif
  return res;
}
