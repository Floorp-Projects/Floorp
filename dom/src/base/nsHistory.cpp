/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   Travis Bogard <travis@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
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
#include "nsContentUtils.h"

//
//  History class implementation 
//
HistoryImpl::HistoryImpl(nsIDocShell* aDocShell) : mDocShell(aDocShell)
{
}

HistoryImpl::~HistoryImpl()
{
}


// QueryInterface implementation for HistoryImpl
NS_INTERFACE_MAP_BEGIN(HistoryImpl)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMHistory)
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
HistoryImpl::GetCurrent(nsAString& aCurrent)
{
  PRInt32 curIndex=0;
  nsCAutoString curURL;
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
  uri->GetSpec(curURL);
  CopyUTF8toUTF16(curURL, aCurrent);

  return NS_OK;
}

NS_IMETHODIMP
HistoryImpl::GetPrevious(nsAString& aPrevious)
{
  PRInt32 curIndex;
  nsCAutoString prevURL;
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
  uri->GetSpec(prevURL);
  CopyUTF8toUTF16(prevURL, aPrevious);

  return NS_OK;
}

NS_IMETHODIMP
HistoryImpl::GetNext(nsAString& aNext)
{
  PRInt32 curIndex;
  nsCAutoString nextURL;
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
  uri->GetSpec(nextURL); 
  CopyUTF8toUTF16(nextURL, aNext);

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
HistoryImpl::Go(PRInt32 aDelta)
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
HistoryImpl::Go()
{
  nsCOMPtr<nsIXPCNativeCallContext> ncc;
  nsresult rv = nsContentUtils::XPConnect()->
    GetCurrentNativeCallContext(getter_AddRefs(ncc));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!ncc)
    return NS_ERROR_NOT_AVAILABLE;

  PRUint32 argc;
  ncc->GetArgc(&argc);

  PRInt32 delta = 0;

  if (argc > 0) {
    jsval *argv = nsnull;

    ncc->GetArgvPtr(&argv);
    NS_ENSURE_TRUE(argv, NS_ERROR_UNEXPECTED);

    if (!JSVAL_IS_INT(argv[0])) {
      // Not an index, don't do anything.

      return NS_OK;
    }

    delta = JSVAL_TO_INT(argv[0]);
  }

  return Go(delta);
}

NS_IMETHODIMP
HistoryImpl::Item(PRUint32 aIndex, nsAString& aReturn)
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
    nsCAutoString urlCString;
    rv = uri->GetSpec(urlCString);

    CopyUTF8toUTF16(urlCString, aReturn);
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

