/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *    Travis Bogard <travis@netscape.com> 
 */

#include "nsCOMPtr.h"
#include "nscore.h"
#include "nsHistory.h"
#include "nsIDOMWindowInternal.h"
#include "nsIScriptGlobalObject.h"
#include "nsIWebShell.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIWebNavigation.h"
#include "nsISHEntry.h"
#include "nsIURI.h"
#include "nsXPIDLString.h"

//
//  History class implementation 
//
HistoryImpl::HistoryImpl(nsIDocShell* aDocShell) : mDocShell(aDocShell),
   mScriptObject(nsnull)
{
  NS_INIT_REFCNT();
}

HistoryImpl::~HistoryImpl()
{
}

NS_IMPL_ADDREF(HistoryImpl)
NS_IMPL_RELEASE(HistoryImpl)

NS_INTERFACE_MAP_BEGIN(HistoryImpl)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIScriptObjectOwner)
   NS_INTERFACE_MAP_ENTRY(nsIScriptObjectOwner)
   NS_INTERFACE_MAP_ENTRY(nsIDOMHistory)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
HistoryImpl::SetScriptObject(void *aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}

NS_IMETHODIMP
HistoryImpl::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
  NS_PRECONDITION(nsnull != aScriptObject, "null arg");
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) {
    nsIScriptGlobalObject *global = aContext->GetGlobalObject();
    res = NS_NewScriptHistory(aContext, NS_STATIC_CAST(nsIDOMHistory *, this),
                              global, &mScriptObject);
    NS_IF_RELEASE(global);
  }
  
  *aScriptObject = mScriptObject;
  return res;
}

NS_IMETHODIMP_(void)       
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
  nsCOMPtr<nsISHEntry> curEntry;
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
  nsCOMPtr<nsISHEntry> prevEntry;
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
  nsCOMPtr<nsISHEntry> nextEntry;
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
HistoryImpl::Go(JSContext* cx, jsval* argv, PRUint32 argc)
{
  nsresult result = NS_OK;
  nsCOMPtr<nsISHistory>  sHistory;
     
  GetSessionHistoryFromDocShell(mDocShell, getter_AddRefs(sHistory));
  NS_ENSURE_TRUE(sHistory, NS_ERROR_FAILURE);

  // QI SHistory to nsIWebNavigation
  nsCOMPtr<nsIWebNavigation> shWebnav(do_QueryInterface(sHistory));
  NS_ENSURE_TRUE(shWebnav, NS_ERROR_FAILURE);
 
  if (argc > 0) {
    if (JSVAL_IS_INT(argv[0])) {
      PRInt32 delta = JSVAL_TO_INT(argv[0]);
      PRInt32 curIndex=-1;
 
      result = sHistory->GetIndex(&curIndex);
      result = shWebnav->GotoIndex(curIndex + delta);     
    }
    else {
      JSString* jsstr = JS_ValueToString(cx, argv[0]);
      PRInt32 i, count;
       
      if (nsnull != jsstr) {
        nsAutoString substr; substr.AssignWithConversion(JS_GetStringBytes(jsstr));
        result = sHistory->GetCount(&count);
        for (i = 0; (i < count) && NS_SUCCEEDED(result); i++) {
          nsCOMPtr<nsISHEntry>   shEntry;
          nsCOMPtr<nsIURI>   uri;

          result = sHistory->GetEntryAtIndex(i, PR_FALSE, getter_AddRefs(shEntry));
          if (!shEntry)
            continue;
          result = shEntry->GetURI(getter_AddRefs(uri));
          if (!uri)
            continue;          
          nsAutoString url;
          nsXPIDLCString   urlCString;
          result = uri->GetSpec(getter_Copies(urlCString));
          url.AssignWithConversion(urlCString);
 
          if (-1 != url.Find(substr)) {
            result = shWebnav->GotoIndex(i);
            break;
          }
        }   //for
      }
    }
  }
  return result;
}


NS_IMETHODIMP
HistoryImpl::Item(PRUint32 aIndex, nsAWritableString& aReturn)
{
  aReturn.Truncate();

  nsresult result = NS_OK;
  nsCOMPtr<nsISHistory>  sHistory;

  GetSessionHistoryfromDocShell(mDocShell, getter_AddRefs(sHistory));
  NS_ENSURE_TRUE(sHistory, NS_ERROR_FAILURE);

 	nsCOMPtr<nsISHEntry> shEntry;
 	nsCOMPtr<nsIURI> uri;

  result = sHistory->GetEntryAtIndex(aIndex, PR_FALSE, getter_AddRefs(shEntry));
  
  if (!shEntry)
    return NS_OK;

  result = shEntry->GetURI(getter_AddRefs(uri));
  if (!uri)
    return NS_OK;

  nsXPIDLCString   urlCString;
  result = uri->GetSpec(getter_Copies(urlCString));

  aReturn.Assign(NS_ConvertASCIItoUCS2(urlCString));

  return NS_OK;
}

NS_IMETHODIMP
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

