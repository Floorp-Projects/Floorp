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
#include "nsICookieService.h"
#include "nsIServiceManager.h"
#include "nsIDOMWindowInternal.h"
#include "nsCOMPtr.h"
#include "nsIScriptGlobalObject.h"
#include "nsCookieViewer.h"
#include "nsIDocShell.h"
#include "nsIPresShell.h"
#include "nsIDocument.h"

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
  nsCOMPtr<nsICookieService> cookieservice = 
           do_GetService(kCookieServiceCID, &res);
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
  nsCOMPtr<nsICookieService> cookieservice = 
           do_GetService(kCookieServiceCID, &res);
  if (NS_FAILED(res)) return res;
  nsAutoString PermissionList;
  res = cookieservice->Cookie_GetPermissionListForViewer(PermissionList, type);
  if (NS_SUCCEEDED(res)) {
    *aValue = PermissionList.ToNewCString();
  }
  return res;
}

NS_IMETHODIMP
CookieViewerImpl::SetValue(const char* aValue, nsIDOMWindowInternal* win)
{
  /* process the value */
  NS_PRECONDITION(aValue != nsnull, "null ptr");
  if (! aValue) {
    return NS_ERROR_NULL_POINTER;
  }
  nsresult res;
  nsCOMPtr<nsICookieService> cookieservice = 
           do_GetService(kCookieServiceCID, &res);
  if (NS_FAILED(res)) return res;
  nsAutoString netList; netList.AssignWithConversion(aValue);
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
  nsCOMPtr<nsICookieService> cookieservice = 
           do_GetService(kCookieServiceCID, &res);
  if (NS_FAILED(res)) {
    return res;
  }
  nsAutoString imageURLAutoString; imageURLAutoString.AssignWithConversion(imageURL);
  res = cookieservice->Image_Block(imageURLAutoString);
  return res;
}

NS_IMETHODIMP
CookieViewerImpl::AddPermission(nsIDOMWindowInternal* aWin, PRBool permission, PRInt32 type)
{
  nsresult rv;

  /* all the following is just to get the url of the window */

  NS_PRECONDITION(aWin != nsnull, "null ptr");
  if (!aWin) {
    return NS_ERROR_NULL_POINTER;
  }

  nsCOMPtr<nsIScriptGlobalObject> scriptGlobalObject; 
  scriptGlobalObject = do_QueryInterface(aWin);
  if(!scriptGlobalObject) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDocShell> docShell; 
  rv = scriptGlobalObject->GetDocShell(getter_AddRefs(docShell)); 
  if(NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIPresShell> presShell;
  rv = docShell->GetPresShell(getter_AddRefs(presShell));
  if(NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIDocument> doc;
  rv = presShell->GetDocument(getter_AddRefs(doc));
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (!doc) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIURI> docURL;
  doc->GetDocumentURL(getter_AddRefs(docURL));
  if (!docURL) {
    return NS_ERROR_FAILURE;
  }

  char* spec;
  (void)docURL->GetSpec(&spec);
  nsAutoString objectURLAutoString; objectURLAutoString.AssignWithConversion(spec);
  Recycle(spec);

  /* got the url at last, now pass it on to the Permission_Add routie */

  nsCOMPtr<nsICookieService> cookieservice = 
           do_GetService(kCookieServiceCID, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = cookieservice->Permission_Add(objectURLAutoString, permission, type);
  return rv;
}
