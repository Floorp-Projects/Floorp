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
 *    Travis Bogard <travis@netscape.com> 
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

#include "nsCOMPtr.h"
#include "nscore.h"
#include "nsHistory.h"
#include "nsIDOMWindowInternal.h"
#include "nsIScriptGlobalObject.h"
#include "nsIWebShell.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIWebNavigation.h"
#include "nsIHistoryEntry.h"
#include "nsIURI.h"
#include "nsIServiceManager.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsDOMClassInfo.h"

//
//  History class implementation 
//
HistoryImpl::HistoryImpl(nsIDocShell* aDocShell) : mDocShell(aDocShell)
{
  NS_INIT_REFCNT();
}

HistoryImpl::~HistoryImpl()
{
}


// QueryInterface implementation for HistoryImpl
NS_INTERFACE_MAP_BEGIN(HistoryImpl)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHistory)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSHistory)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(History)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(HistoryImpl)
NS_IMPL_RELEASE(HistoryImpl)


void
HistoryImpl::SetDocShell(nsIDocShell *aDocShell)
{
  mDocShell = aDocShell; // Weak Reference
}

NS_IMETHODIMP
HistoryImpl::GetLength(PRInt32* aLength)
{
  nsCOMPtr<nsISHistory>   sHistory;

  // Get session History from docshell
  GetSessionHistoryFromDocShell(mDocShell, getter_AddRefs(sHistory));
  NS_ENSURE_TRUE(sHistory, NS_ERROR_FAILURE);
  return sHistory->GetCount(aLength);
}

NS_IMETHODIMP
HistoryImpl::GetCurrent(nsAWritableString& aCurrent)
{
  PRInt32 curIndex=0;
  char *  curURL=nsnull;
  nsCOMPtr<nsISHistory> sHistory;

  // Get SessionHistory from docshell
  GetSessionHistoryFromDocShell(mDocShell, getter_AddRefs(sHistory));
  NS_ENSURE_TRUE(sHistory, NS_ERROR_FAILURE);

  // Get the current index at session History
  sHistory->GetIndex(&curIndex);
  nsCOMPtr<nsIHistoryEntry> curEntry;
  nsCOMPtr<nsIURI>     uri;

  // Get the SH entry for the current index
  sHistory->GetEntryAtIndex(curIndex, PR_FALSE, getter_AddRefs(curEntry));
  NS_ENSURE_TRUE(curEntry, NS_ERROR_FAILURE);

  // Get the URI for the current entry
  curEntry->GetURI(getter_AddRefs(uri));
  NS_ENSURE_TRUE(uri, NS_ERROR_FAILURE);
  uri->GetSpec(&curURL);
  aCurrent.Assign(NS_ConvertASCIItoUCS2(curURL));
  nsCRT::free(curURL);

  return NS_OK;
}

NS_IMETHODIMP
HistoryImpl::GetPrevious(nsAWritableString& aPrevious)
{
  PRInt32 curIndex;
  char *  prevURL = nsnull;
  nsCOMPtr<nsISHistory>  sHistory;

  // Get session History from docshell
  GetSessionHistoryFromDocShell(mDocShell, getter_AddRefs(sHistory));
  NS_ENSURE_TRUE(sHistory, NS_ERROR_FAILURE);

  // Get the current index at session History
  sHistory->GetIndex(&curIndex);
  nsCOMPtr<nsIHistoryEntry> prevEntry;
  nsCOMPtr<nsIURI>     uri;

  // Get the previous SH entry
  sHistory->GetEntryAtIndex((curIndex-1), PR_FALSE, getter_AddRefs(prevEntry));
  NS_ENSURE_TRUE(prevEntry, NS_ERROR_FAILURE);

  // Get the URI for the previous entry
  prevEntry->GetURI(getter_AddRefs(uri));
  NS_ENSURE_TRUE(uri, NS_ERROR_FAILURE);
  uri->GetSpec(&prevURL);
  aPrevious.Assign(NS_ConvertASCIItoUCS2(prevURL));
  nsCRT::free(prevURL);

  return NS_OK;
}

NS_IMETHODIMP
HistoryImpl::GetNext(nsAWritableString& aNext)
{
  PRInt32 curIndex;
  char * nextURL = nsnull;
  nsCOMPtr<nsISHistory>  sHistory;

  // Get session History from docshell
  GetSessionHistoryFromDocShell(mDocShell, getter_AddRefs(sHistory));
  NS_ENSURE_TRUE(sHistory, NS_ERROR_FAILURE);

  // Get the current index at session History
  sHistory->GetIndex(&curIndex);
  nsCOMPtr<nsIHistoryEntry> nextEntry;
  nsCOMPtr<nsIURI>     uri;

  // Get the next SH entry
  sHistory->GetEntryAtIndex((curIndex+1), PR_FALSE, getter_AddRefs(nextEntry));
  NS_ENSURE_TRUE(nextEntry, NS_ERROR_FAILURE);

  // Get the URI for the next entry
  nextEntry->GetURI(getter_AddRefs(uri));
  NS_ENSURE_TRUE(uri, NS_ERROR_FAILURE);
  uri->GetSpec(&nextURL); 
  aNext.Assign(NS_ConvertASCIItoUCS2(nextURL));
  nsCRT::free(nextURL);

  return NS_OK;
}

NS_IMETHODIMP
HistoryImpl::Back()
{
  nsCOMPtr<nsISHistory>  sHistory;

  GetSessionHistoryFromDocShell(mDocShell, getter_AddRefs(sHistory));
  NS_ENSURE_TRUE(sHistory, NS_ERROR_FAILURE);

  //QI SHistory to WebNavigation
  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(sHistory));
  NS_ENSURE_TRUE(webNav, NS_ERROR_FAILURE);
  webNav->GoBack();

  return NS_OK;
}

NS_IMETHODIMP
HistoryImpl::Forward()
{
  nsCOMPtr<nsISHistory>  sHistory;

  GetSessionHistoryFromDocShell(mDocShell, getter_AddRefs(sHistory));
  NS_ENSURE_TRUE(sHistory, NS_ERROR_FAILURE);

  //QI SHistory to WebNavigation
  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(sHistory));
  NS_ENSURE_TRUE(webNav, NS_ERROR_FAILURE);
  webNav->GoForward();

  return NS_OK;
}

NS_IMETHODIMP
HistoryImpl::GoIndex(PRInt32 aDelta)
{
  nsCOMPtr<nsISHistory> session_history;

  GetSessionHistoryFromDocShell(mDocShell, getter_AddRefs(session_history));
  NS_ENSURE_TRUE(session_history, NS_ERROR_FAILURE);

  // QI SHistory to nsIWebNavigation
  nsCOMPtr<nsIWebNavigation> webnav(do_QueryInterface(session_history));
  NS_ENSURE_TRUE(webnav, NS_ERROR_FAILURE);

  PRInt32 curIndex=-1;
  PRInt32 len = 0;
  nsresult rv = session_history->GetIndex(&curIndex);  
  rv = session_history->GetCount(&len);
  
  PRInt32 index = curIndex + aDelta;
  if (index > -1  &&  index < len)
     webnav->GotoIndex(index);
  // We always want to return a NS_OK, since returning errors 
  // from GotoIndex() can lead to exceptions and a possible leak
  // of history length
  return NS_OK;
}

NS_IMETHODIMP
HistoryImpl::GoUri(const nsAReadableString& aUriSubstring)
{
  nsCOMPtr<nsISHistory> session_history;

  GetSessionHistoryFromDocShell(mDocShell, getter_AddRefs(session_history));
  NS_ENSURE_TRUE(session_history, NS_ERROR_FAILURE);

  // QI SHistory to nsIWebNavigation
  nsCOMPtr<nsIWebNavigation> webnav(do_QueryInterface(session_history));
  NS_ENSURE_TRUE(webnav, NS_ERROR_FAILURE);

  nsresult rv = NS_OK;
  PRInt32 i, count;

  rv = session_history->GetCount(&count);

  for (i = 0; (i < count) && NS_SUCCEEDED(rv); i++) {
    nsCOMPtr<nsIHistoryEntry> sh_entry;

    rv = session_history->GetEntryAtIndex(i, PR_FALSE,
                                          getter_AddRefs(sh_entry));
    if (sh_entry) {
      nsCOMPtr<nsIURI> uri;

      rv = sh_entry->GetURI(getter_AddRefs(uri));

      if (uri) {
        nsXPIDLCString   urlCString;
        rv = uri->GetSpec(getter_Copies(urlCString));

        nsAutoString url;
        url.AssignWithConversion(urlCString);

        nsReadingIterator<PRUnichar> start;
        nsReadingIterator<PRUnichar> end;

        url.BeginReading(start);
        url.EndReading(end);

        if (FindInReadable(aUriSubstring, start, end)) {
          rv = webnav->GotoIndex(i);

          break;
        }
      }
    }
  }

  return rv;
}

NS_IMETHODIMP
HistoryImpl::Go()
{
  nsresult rv;
  nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID(), &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIXPCNativeCallContext> ncc;

  rv = xpc->GetCurrentNativeCallContext(getter_AddRefs(ncc));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!ncc)
    return NS_ERROR_NOT_AVAILABLE;

  PRUint32 argc;

  ncc->GetArgc(&argc);

  if (argc == 0) {
    return NS_OK;
  }

  jsval *argv = nsnull;

  ncc->GetArgvPtr(&argv);
  NS_ENSURE_TRUE(argv, NS_ERROR_UNEXPECTED);

  JSContext *cx = nsnull;

  rv = ncc->GetJSContext(&cx);
  NS_ENSURE_SUCCESS(rv, rv);

  if (JSVAL_IS_INT(argv[0])) {
    PRInt32 delta = JSVAL_TO_INT(argv[0]);
 
    return GoIndex(delta);
  }

  JSString* jsstr = JS_ValueToString(cx, argv[0]);

  return GoUri(nsDependentString(NS_REINTERPRET_CAST(const PRUnichar *,
                                                   ::JS_GetStringChars(jsstr)),
                               ::JS_GetStringLength(jsstr)));
}

NS_IMETHODIMP
HistoryImpl::Item(PRUint32 aIndex, nsAWritableString& aReturn)
{
  aReturn.Truncate();

  nsresult rv = NS_OK;
  nsCOMPtr<nsISHistory>  session_history;

  GetSessionHistoryFromDocShell(mDocShell, getter_AddRefs(session_history));
  NS_ENSURE_TRUE(session_history, NS_ERROR_FAILURE);

 	nsCOMPtr<nsIHistoryEntry> sh_entry;
 	nsCOMPtr<nsIURI> uri;

  rv = session_history->GetEntryAtIndex(aIndex, PR_FALSE,
                                        getter_AddRefs(sh_entry));

  if (sh_entry) {
    rv = sh_entry->GetURI(getter_AddRefs(uri));
  }

  if (uri) {
    nsXPIDLCString   urlCString;
    rv = uri->GetSpec(getter_Copies(urlCString));

    aReturn.Assign(NS_ConvertASCIItoUCS2(urlCString));
  }

  return rv;
}

nsresult
HistoryImpl::GetSessionHistoryFromDocShell(nsIDocShell * aDocShell, 
                                           nsISHistory ** aReturn)
{

  NS_ENSURE_TRUE(aDocShell, NS_ERROR_FAILURE);
  /* The docshell we have may or may not be
   * the root docshell. So, get a handle to
   * SH from the root docshell
   */
  
  // QI mDocShell to nsIDocShellTreeItem
  nsCOMPtr<nsIDocShellTreeItem> dsTreeItem(do_QueryInterface(mDocShell));
  NS_ENSURE_TRUE(dsTreeItem, NS_ERROR_FAILURE);

  // Get the root DocShell from it
  nsCOMPtr<nsIDocShellTreeItem> root;
  dsTreeItem->GetSameTypeRootTreeItem(getter_AddRefs(root));
  NS_ENSURE_TRUE(root, NS_ERROR_FAILURE);
  
  //QI root to nsIWebNavigation
  nsCOMPtr<nsIWebNavigation>   webNav(do_QueryInterface(root));
  NS_ENSURE_TRUE(webNav, NS_ERROR_FAILURE);

  //Get  SH from nsIWebNavigation
  return webNav->GetSessionHistory(aReturn);
  
}

