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
#include "nsICookieService.h"
#include "nsIServiceManager.h"
#include "nsIDOMWindow.h"
#include "nsCOMPtr.h"
#include "nsIWebShell.h"
#include "nsIWebShellWindow.h"
#include "nsIScriptGlobalObject.h"
#include "nsCookieViewer.h"

static NS_DEFINE_IID(kCookieServiceCID, NS_COOKIESERVICE_CID);

////////////////////////////////////////////////////////////////////////

CookieViewerImpl::CookieViewerImpl()
{
  NS_INIT_REFCNT();
}

CookieViewerImpl::~CookieViewerImpl()
{
}

NS_IMPL_ISUPPORTS1(CookieViewerImpl, nsICookieViewer)

NS_IMETHODIMP
CookieViewerImpl::GetCookieValue(char** aValue)
{
  NS_PRECONDITION(aValue != nsnull, "null ptr");
  if (!aValue) {
    return NS_ERROR_NULL_POINTER;
  }

  nsresult res;
  NS_WITH_SERVICE(nsICookieService, cookieservice, kCookieServiceCID, &res);
  if (NS_FAILED(res)) return res;
  nsAutoString cookieList;
  res = cookieservice->Cookie_GetCookieListForViewer(cookieList);
  if (NS_SUCCEEDED(res)) {
    *aValue = cookieList.ToNewCString();
  }
  return res;
}

NS_IMETHODIMP
CookieViewerImpl::GetPermissionValue(PRInt32 type, char** aValue)
{
  NS_PRECONDITION(aValue != nsnull, "null ptr");
  if (!aValue) {
    return NS_ERROR_NULL_POINTER;
  }

  nsresult res;
  NS_WITH_SERVICE(nsICookieService, cookieservice, kCookieServiceCID, &res);
  if (NS_FAILED(res)) return res;
  nsAutoString PermissionList;
  res = cookieservice->Cookie_GetPermissionListForViewer(PermissionList, type);
  if (NS_SUCCEEDED(res)) {
    *aValue = PermissionList.ToNewCString();
  }
  return res;
}

NS_IMETHODIMP
CookieViewerImpl::SetValue(const char* aValue, nsIDOMWindow* win)
{
  /* process the value */
  NS_PRECONDITION(aValue != nsnull, "null ptr");
  if (! aValue) {
    return NS_ERROR_NULL_POINTER;
  }
  nsresult res;
  NS_WITH_SERVICE(nsICookieService, cookieservice, kCookieServiceCID, &res);
  if (NS_FAILED(res)) return res;
  nsAutoString netList = aValue;
  res = cookieservice->Cookie_CookieViewerReturn(netList);
  return res;
}

NS_IMETHODIMP
CookieViewerImpl::BlockImage(const char* imageURL)
{
  NS_PRECONDITION(imageURL != nsnull, "null ptr");
  if (! imageURL) {
    return NS_ERROR_NULL_POINTER;
  }
  nsresult res;
  NS_WITH_SERVICE(nsICookieService, cookieservice, kCookieServiceCID, &res);
  if (NS_FAILED(res)) {
    return res;
  }
  nsAutoString imageURLAutoString = imageURL;
  res = cookieservice->Image_Block(imageURLAutoString);
  return res;
}
