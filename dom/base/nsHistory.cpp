/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHistory.h"

#include "nsCOMPtr.h"
#include "nscore.h"
#include "nsPIDOMWindow.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIWebNavigation.h"
#include "nsIHistoryEntry.h"
#include "nsIURI.h"
#include "nsIServiceManager.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsDOMClassInfoID.h"
#include "nsContentUtils.h"
#include "nsISHistoryInternal.h"
#include "mozilla/Preferences.h"

using namespace mozilla;

static const char* sAllowPushStatePrefStr  =
  "browser.history.allowPushState";
static const char* sAllowReplaceStatePrefStr =
  "browser.history.allowReplaceState";

//
//  History class implementation 
//
nsHistory::nsHistory(nsPIDOMWindow* aInnerWindow)
  : mInnerWindow(do_GetWeakReference(aInnerWindow))
{
}

nsHistory::~nsHistory()
{
}


DOMCI_DATA(History, nsHistory)

// QueryInterface implementation for nsHistory
NS_INTERFACE_MAP_BEGIN(nsHistory)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMHistory)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHistory)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(History)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsHistory)
NS_IMPL_RELEASE(nsHistory)


NS_IMETHODIMP
nsHistory::GetLength(PRInt32* aLength)
{
  nsCOMPtr<nsISHistory>   sHistory;

  // Get session History from docshell
  GetSessionHistoryFromDocShell(GetDocShell(), getter_AddRefs(sHistory));
  NS_ENSURE_TRUE(sHistory, NS_ERROR_FAILURE);
  return sHistory->GetCount(aLength);
}

NS_IMETHODIMP
nsHistory::GetCurrent(nsAString& aCurrent)
{
  if (!nsContentUtils::IsCallerTrustedForRead())
    return NS_ERROR_DOM_SECURITY_ERR;

  PRInt32 curIndex=0;
  nsCAutoString curURL;
  nsCOMPtr<nsISHistory> sHistory;

  // Get SessionHistory from docshell
  GetSessionHistoryFromDocShell(GetDocShell(), getter_AddRefs(sHistory));
  NS_ENSURE_TRUE(sHistory, NS_ERROR_FAILURE);

  // Get the current index at session History
  sHistory->GetIndex(&curIndex);
  nsCOMPtr<nsIHistoryEntry> curEntry;
  nsCOMPtr<nsIURI>     uri;

  // Get the SH entry for the current index
  sHistory->GetEntryAtIndex(curIndex, false, getter_AddRefs(curEntry));
  NS_ENSURE_TRUE(curEntry, NS_ERROR_FAILURE);

  // Get the URI for the current entry
  curEntry->GetURI(getter_AddRefs(uri));
  NS_ENSURE_TRUE(uri, NS_ERROR_FAILURE);
  uri->GetSpec(curURL);
  CopyUTF8toUTF16(curURL, aCurrent);

  return NS_OK;
}

NS_IMETHODIMP
nsHistory::GetPrevious(nsAString& aPrevious)
{
  if (!nsContentUtils::IsCallerTrustedForRead())
    return NS_ERROR_DOM_SECURITY_ERR;

  PRInt32 curIndex;
  nsCAutoString prevURL;
  nsCOMPtr<nsISHistory>  sHistory;

  // Get session History from docshell
  GetSessionHistoryFromDocShell(GetDocShell(), getter_AddRefs(sHistory));
  NS_ENSURE_TRUE(sHistory, NS_ERROR_FAILURE);

  // Get the current index at session History
  sHistory->GetIndex(&curIndex);
  nsCOMPtr<nsIHistoryEntry> prevEntry;
  nsCOMPtr<nsIURI>     uri;

  // Get the previous SH entry
  sHistory->GetEntryAtIndex((curIndex-1), false, getter_AddRefs(prevEntry));
  NS_ENSURE_TRUE(prevEntry, NS_ERROR_FAILURE);

  // Get the URI for the previous entry
  prevEntry->GetURI(getter_AddRefs(uri));
  NS_ENSURE_TRUE(uri, NS_ERROR_FAILURE);
  uri->GetSpec(prevURL);
  CopyUTF8toUTF16(prevURL, aPrevious);

  return NS_OK;
}

NS_IMETHODIMP
nsHistory::GetNext(nsAString& aNext)
{
  if (!nsContentUtils::IsCallerTrustedForRead())
    return NS_ERROR_DOM_SECURITY_ERR;

  PRInt32 curIndex;
  nsCAutoString nextURL;
  nsCOMPtr<nsISHistory>  sHistory;

  // Get session History from docshell
  GetSessionHistoryFromDocShell(GetDocShell(), getter_AddRefs(sHistory));
  NS_ENSURE_TRUE(sHistory, NS_ERROR_FAILURE);

  // Get the current index at session History
  sHistory->GetIndex(&curIndex);
  nsCOMPtr<nsIHistoryEntry> nextEntry;
  nsCOMPtr<nsIURI>     uri;

  // Get the next SH entry
  sHistory->GetEntryAtIndex((curIndex+1), false, getter_AddRefs(nextEntry));
  NS_ENSURE_TRUE(nextEntry, NS_ERROR_FAILURE);

  // Get the URI for the next entry
  nextEntry->GetURI(getter_AddRefs(uri));
  NS_ENSURE_TRUE(uri, NS_ERROR_FAILURE);
  uri->GetSpec(nextURL); 
  CopyUTF8toUTF16(nextURL, aNext);

  return NS_OK;
}

NS_IMETHODIMP
nsHistory::Back()
{
  nsCOMPtr<nsISHistory>  sHistory;

  GetSessionHistoryFromDocShell(GetDocShell(), getter_AddRefs(sHistory));
  NS_ENSURE_TRUE(sHistory, NS_ERROR_FAILURE);

  //QI SHistory to WebNavigation
  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(sHistory));
  NS_ENSURE_TRUE(webNav, NS_ERROR_FAILURE);
  webNav->GoBack();

  return NS_OK;
}

NS_IMETHODIMP
nsHistory::Forward()
{
  nsCOMPtr<nsISHistory>  sHistory;

  GetSessionHistoryFromDocShell(GetDocShell(), getter_AddRefs(sHistory));
  NS_ENSURE_TRUE(sHistory, NS_ERROR_FAILURE);

  //QI SHistory to WebNavigation
  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(sHistory));
  NS_ENSURE_TRUE(webNav, NS_ERROR_FAILURE);
  webNav->GoForward();

  return NS_OK;
}

NS_IMETHODIMP
nsHistory::Go(PRInt32 aDelta)
{
  if (aDelta == 0) {
    nsCOMPtr<nsPIDOMWindow> window(do_GetInterface(GetDocShell()));

    if (window && window->IsHandlingResizeEvent()) {
      // history.go(0) (aka location.reload()) was called on a window
      // that is handling a resize event. Sites do this since Netscape
      // 4.x needed it, but we don't, and it's a horrible experience
      // for nothing.  In stead of reloading the page, just clear
      // style data and reflow the page since some sites may use this
      // trick to work around gecko reflow bugs, and this should have
      // the same effect.

      nsCOMPtr<nsIDocument> doc =
        do_QueryInterface(window->GetExtantDocument());

      nsIPresShell *shell;
      nsPresContext *pcx;
      if (doc && (shell = doc->GetShell()) && (pcx = shell->GetPresContext())) {
        pcx->RebuildAllStyleData(NS_STYLE_HINT_REFLOW);
      }

      return NS_OK;
    }
  }

  nsCOMPtr<nsISHistory> session_history;

  GetSessionHistoryFromDocShell(GetDocShell(), getter_AddRefs(session_history));
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
nsHistory::PushState(nsIVariant *aData, const nsAString& aTitle,
                     const nsAString& aURL, JSContext* aCx)
{
  // Check that PushState hasn't been pref'ed off.
  if (!Preferences::GetBool(sAllowPushStatePrefStr, false)) {
    return NS_OK;
  }

  nsCOMPtr<nsPIDOMWindow> win(do_QueryReferent(mInnerWindow));
  if (!win)
    return NS_ERROR_NOT_AVAILABLE;

  if (!nsContentUtils::CanCallerAccess(win->GetOuterWindow()))
    return NS_ERROR_DOM_SECURITY_ERR;

  // AddState might run scripts, so we need to hold a strong reference to the
  // docShell here to keep it from going away.
  nsCOMPtr<nsIDocShell> docShell = win->GetDocShell();

  NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);

  // false tells the docshell to add a new history entry instead of
  // modifying the current one.
  nsresult rv = docShell->AddState(aData, aTitle, aURL, false, aCx);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsHistory::ReplaceState(nsIVariant *aData, const nsAString& aTitle,
                        const nsAString& aURL, JSContext* aCx)
{
  // Check that ReplaceState hasn't been pref'ed off
  if (!Preferences::GetBool(sAllowReplaceStatePrefStr, false)) {
    return NS_OK;
  }

  nsCOMPtr<nsPIDOMWindow> win(do_QueryReferent(mInnerWindow));
  if (!win)
    return NS_ERROR_NOT_AVAILABLE;

  if (!nsContentUtils::CanCallerAccess(win->GetOuterWindow()))
    return NS_ERROR_DOM_SECURITY_ERR;

  // AddState might run scripts, so we need to hold a strong reference to the
  // docShell here to keep it from going away.
  nsCOMPtr<nsIDocShell> docShell = win->GetDocShell();

  NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);

  // true tells the docshell to modify the current SHEntry, rather than
  // create a new one.
  return docShell->AddState(aData, aTitle, aURL, true, aCx);
}

NS_IMETHODIMP
nsHistory::GetState(nsIVariant **aState)
{
  *aState = nsnull;

  nsCOMPtr<nsPIDOMWindow> win(do_QueryReferent(mInnerWindow));
  if (!win)
    return NS_ERROR_NOT_AVAILABLE;

  if (!nsContentUtils::CanCallerAccess(win->GetOuterWindow()))
    return NS_ERROR_DOM_SECURITY_ERR;

  nsCOMPtr<nsIDocument> doc =
    do_QueryInterface(win->GetExtantDocument());
  if (!doc)
    return NS_ERROR_NOT_AVAILABLE;

  return doc->GetStateObject(aState);
}

NS_IMETHODIMP
nsHistory::Item(PRUint32 aIndex, nsAString& aReturn)
{
  aReturn.Truncate();
  if (!nsContentUtils::IsCallerTrustedForRead()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsresult rv = NS_OK;
  nsCOMPtr<nsISHistory>  session_history;

  GetSessionHistoryFromDocShell(GetDocShell(), getter_AddRefs(session_history));
  NS_ENSURE_TRUE(session_history, NS_ERROR_FAILURE);

  nsCOMPtr<nsIHistoryEntry> sh_entry;
  nsCOMPtr<nsIURI> uri;

  rv = session_history->GetEntryAtIndex(aIndex, false,
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
nsHistory::GetSessionHistoryFromDocShell(nsIDocShell * aDocShell, 
                                         nsISHistory ** aReturn)
{

  NS_ENSURE_TRUE(aDocShell, NS_ERROR_FAILURE);
  /* The docshell we have may or may not be
   * the root docshell. So, get a handle to
   * SH from the root docshell
   */
  
  // QI mDocShell to nsIDocShellTreeItem
  nsCOMPtr<nsIDocShellTreeItem> dsTreeItem(do_QueryInterface(aDocShell));
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

