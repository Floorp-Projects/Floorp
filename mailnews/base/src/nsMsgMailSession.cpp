/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#include "msgCore.h" // for pre-compiled headers
#include "nsMsgBaseCID.h"
#include "nsMsgMailSession.h"
#include "nsCOMPtr.h"
#include "nsIMsgStatusFeedback.h"
#include "nsIMsgWindow.h"
#include "nsIMsgMessageService.h"
#include "nsMsgUtils.h"
#include "nsIURI.h"
#include "nsXPIDLString.h"
#include "nsIMsgAccountManager.h"
#include "nsIChromeRegistry.h"
#include "nsIDirectoryService.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsReadableUtils.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIWindowMediator.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDocShell.h"

NS_IMPL_THREADSAFE_ADDREF(nsMsgMailSession)
NS_IMPL_THREADSAFE_RELEASE(nsMsgMailSession)
NS_INTERFACE_MAP_BEGIN(nsMsgMailSession)
  NS_INTERFACE_MAP_ENTRY(nsIMsgMailSession)
  NS_INTERFACE_MAP_ENTRY(nsIFolderListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIMsgMailSession)
NS_INTERFACE_MAP_END_THREADSAFE
  
nsMsgMailSession::nsMsgMailSession()
{
}


nsMsgMailSession::~nsMsgMailSession()
{
  Shutdown();
}

nsresult nsMsgMailSession::Init()
{
  return NS_NewISupportsArray(getter_AddRefs(mWindows));
}

nsresult nsMsgMailSession::Shutdown()
{
  return NS_OK;
}

NS_IMETHODIMP nsMsgMailSession::AddFolderListener(nsIFolderListener * listener, PRUint32 notifyFlags)
{
  mListeners.AppendObject(listener);
  mListenerNotifyFlags.Add(notifyFlags);
  return NS_OK;
}

NS_IMETHODIMP nsMsgMailSession::RemoveFolderListener(nsIFolderListener * listener)
{
  PRInt32 index = mListeners.IndexOf(listener);
  NS_ASSERTION(index >= 0, "removing non-existent listener");
  if (index >= 0)
  {
    mListenerNotifyFlags.RemoveAt(index);
    mListeners.RemoveObjectAt(index);
  }
  return NS_OK;

}

NS_IMETHODIMP
nsMsgMailSession::OnItemPropertyChanged(nsIRDFResource *item,
                                        nsIAtom *property,
                                        const char* oldValue,
                                        const char* newValue)
{
  PRInt32 count = mListeners.Count();
  
  for(PRInt32 i = 0; i < count; i++)
  {
    if (mListenerNotifyFlags[i] & nsIFolderListener::propertyChanged) {
      nsCOMPtr<nsIFolderListener> listener = mListeners[i];
      NS_ASSERTION(listener, "listener is null");
      if (!listener) return NS_ERROR_FAILURE;
      listener->OnItemPropertyChanged(item, property, oldValue, newValue);
    }
  }
  
  return NS_OK;
  
}

NS_IMETHODIMP
nsMsgMailSession::OnItemUnicharPropertyChanged(nsIRDFResource *item,
                                               nsIAtom *property,
                                               const PRUnichar* oldValue,
                                               const PRUnichar* newValue)
{
  PRInt32 count = mListeners.Count();
  
  for(PRInt32 i = 0; i < count; i++)
  {
    if (mListenerNotifyFlags[i] & nsIFolderListener::unicharPropertyChanged) {
      nsCOMPtr<nsIFolderListener> listener = mListeners[i];
      NS_ASSERTION(listener, "listener is null");
      if (!listener) return NS_ERROR_FAILURE;
      listener->OnItemUnicharPropertyChanged(item, property, oldValue, newValue);
    }
  }
  
  return NS_OK;
  
}

NS_IMETHODIMP
nsMsgMailSession::OnItemIntPropertyChanged(nsIRDFResource *item,
                                           nsIAtom *property,
                                           PRInt32 oldValue,
                                           PRInt32 newValue)
{
  PRInt32 count = mListeners.Count();
  
  for(PRInt32 i = 0; i < count; i++)
  {
    if (mListenerNotifyFlags[i] & nsIFolderListener::intPropertyChanged) {
      nsCOMPtr<nsIFolderListener> listener = mListeners[i];
      NS_ASSERTION(listener, "listener is null");
      if (!listener) return NS_ERROR_FAILURE;
      listener->OnItemIntPropertyChanged(item, property, oldValue, newValue);
    }
  }
  
  return NS_OK;
  
}

NS_IMETHODIMP
nsMsgMailSession::OnItemBoolPropertyChanged(nsIRDFResource *item,
                                            nsIAtom *property,
                                            PRBool oldValue,
                                            PRBool newValue)
{
  PRInt32 count = mListeners.Count();
  
  for(PRInt32 i = 0; i < count; i++)
  {
    if (mListenerNotifyFlags[i] & nsIFolderListener::boolPropertyChanged) {
      nsCOMPtr<nsIFolderListener> listener = mListeners[i];
      NS_ASSERTION(listener, "listener is null");
      if (!listener) return NS_ERROR_FAILURE;
      listener->OnItemBoolPropertyChanged(item, property, oldValue, newValue);
    }
  }
  
  return NS_OK;
  
}
NS_IMETHODIMP
nsMsgMailSession::OnItemPropertyFlagChanged(nsISupports *item,
                                            nsIAtom *property,
                                            PRUint32 oldValue,
                                            PRUint32 newValue)
{
  PRInt32 count = mListeners.Count();

  for(PRInt32 i = 0; i < count; i++)
  {
    if (mListenerNotifyFlags[i] & nsIFolderListener::propertyFlagChanged) {
      nsCOMPtr<nsIFolderListener> listener = mListeners[i];
      NS_ASSERTION(listener, "listener is null");
      if (!listener) return NS_ERROR_FAILURE;
      listener->OnItemPropertyFlagChanged(item, property, oldValue, newValue);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsMsgMailSession::OnItemAdded(nsIRDFResource *parentItem, nsISupports *item)
{
  PRInt32 count = mListeners.Count();
  for(PRInt32 i = 0; i < count; i++)
  {
    if (mListenerNotifyFlags[i] & nsIFolderListener::added) {
      nsCOMPtr<nsIFolderListener> listener = mListeners[i];
      NS_ASSERTION(listener, "listener is null");
      if (!listener) return NS_ERROR_FAILURE;
      listener->OnItemAdded(parentItem, item);
    }
  }
  
  return NS_OK;
  
}

NS_IMETHODIMP nsMsgMailSession::OnItemRemoved(nsIRDFResource *parentItem, nsISupports *item)
{
  PRInt32 count = mListeners.Count();
  
  for(PRInt32 i = 0; i < count; i++)
  {
    if (mListenerNotifyFlags[i] & nsIFolderListener::removed) {
      nsCOMPtr<nsIFolderListener> listener = mListeners[i];
      NS_ASSERTION(listener, "listener is null");
      if (!listener) return NS_ERROR_FAILURE;
      listener->OnItemRemoved(parentItem, item);
    }
  }
  return NS_OK;
  
}


NS_IMETHODIMP nsMsgMailSession::OnItemEvent(nsIMsgFolder *aFolder,
                                            nsIAtom *aEvent)
{
  PRInt32 count = mListeners.Count();
  
  for(PRInt32 i = 0; i < count; i++)
  {
    if (mListenerNotifyFlags[i] & nsIFolderListener::event) {
      nsCOMPtr<nsIFolderListener> listener = mListeners[i];
      if(listener)
        listener->OnItemEvent(aFolder, aEvent);
    }
  }
  return NS_OK;
}

nsresult nsMsgMailSession::GetTopmostMsgWindow(nsIMsgWindow* *aMsgWindow)
{
  nsresult rv;

  if (!aMsgWindow) return NS_ERROR_NULL_POINTER;
  
  *aMsgWindow = nsnull;
 
  if (mWindows)
  {
    PRUint32 count;
    rv = mWindows->Count(&count);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIMsgWindow> msgWindow;

    if (count == 1)
    {
      msgWindow = do_QueryElementAt(mWindows, 0, &rv);
      NS_ENSURE_SUCCESS(rv, rv);
      NS_IF_ADDREF(*aMsgWindow = msgWindow);
    }
    // If multiple message windows then we have lots more work.
    else if (count > 1)
    {
      // The msgWindows array does not hold z-order info.
      // Use mediator to get the top most window then match that with the msgWindows array.
      nsCOMPtr<nsIWindowMediator> windowMediator = do_GetService(NS_WINDOWMEDIATOR_CONTRACTID, &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsISimpleEnumerator> windowEnum;
      rv = windowMediator->GetZOrderDOMWindowEnumerator(nsnull, PR_TRUE, getter_AddRefs(windowEnum));
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsISupports> windowSupports;
      nsCOMPtr<nsIDOMWindow> topMostWindow;
      nsCOMPtr<nsIDOMDocument> domDocument;
      nsCOMPtr<nsIDOMElement> domElement;
      nsAutoString windowType;
      PRBool more;

      // loop to get the top most with attibute "mail:3pane" or "mail:messageWindow"
      windowEnum->HasMoreElements(&more);
      while (more)
      {
        rv = windowEnum->GetNext(getter_AddRefs(windowSupports));
        NS_ENSURE_SUCCESS(rv, rv);
        NS_ENSURE_TRUE(windowSupports, NS_ERROR_FAILURE);

        topMostWindow = do_QueryInterface(windowSupports, &rv);
        NS_ENSURE_SUCCESS(rv, rv);
        NS_ENSURE_TRUE(topMostWindow, NS_ERROR_FAILURE);

        rv = topMostWindow->GetDocument(getter_AddRefs(domDocument));
        NS_ENSURE_SUCCESS(rv, rv);
        NS_ENSURE_TRUE(domDocument, NS_ERROR_FAILURE);

        rv = domDocument->GetDocumentElement(getter_AddRefs(domElement));
        NS_ENSURE_SUCCESS(rv, rv);
        NS_ENSURE_TRUE(domElement, NS_ERROR_FAILURE);

        rv = domElement->GetAttribute(NS_LITERAL_STRING("windowtype"), windowType);
        NS_ENSURE_SUCCESS(rv, rv);

        if (windowType.EqualsLiteral("mail:3pane") ||
            windowType.EqualsLiteral("mail:messageWindow"))
            break;

        windowEnum->HasMoreElements(&more);
      }

      // identified the top most window
      if (more)
      {
        nsCOMPtr<nsIScriptGlobalObject> globalObj = do_QueryInterface(topMostWindow, &rv);
        NS_ENSURE_SUCCESS(rv, rv);
  
        // use this for the match
        nsIDocShell *topDocShell = globalObj->GetDocShell();

        // loop for the msgWindow array to find the match
        nsCOMPtr<nsIDocShell> docShell;
        while (count)
        {
          msgWindow = do_QueryElementAt(mWindows, count-1, &rv);
          NS_ENSURE_SUCCESS(rv,rv);

          rv = msgWindow->GetRootDocShell(getter_AddRefs(docShell));
          NS_ENSURE_SUCCESS(rv,rv);
          if (topDocShell == docShell)
          {
            NS_IF_ADDREF(*aMsgWindow = msgWindow);
            break;
          }
          count--;
        }
      }
    }
  }

  return (*aMsgWindow) ? NS_OK : NS_ERROR_FAILURE;
}



NS_IMETHODIMP nsMsgMailSession::AddMsgWindow(nsIMsgWindow *msgWindow)
{
  mWindows->AppendElement(msgWindow);
  return NS_OK;
}

NS_IMETHODIMP nsMsgMailSession::RemoveMsgWindow(nsIMsgWindow *msgWindow)
{
    mWindows->RemoveElement(msgWindow);
    PRUint32 count = 0;
    mWindows->Count(&count);
    if (count == 0)
    {
        nsresult rv;
        nsCOMPtr<nsIMsgAccountManager> accountManager = 
                 do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
        if (NS_FAILED(rv)) return rv;
        accountManager->CleanupOnExit();
    }
	return NS_OK;
}

NS_IMETHODIMP nsMsgMailSession::GetMsgWindowsArray(nsISupportsArray * *aWindowsArray)
{
  if(!aWindowsArray)
    return NS_ERROR_NULL_POINTER;
  
  *aWindowsArray = mWindows;
  NS_IF_ADDREF(*aWindowsArray);
  return NS_OK;
}

NS_IMETHODIMP nsMsgMailSession::IsFolderOpenInWindow(nsIMsgFolder *folder, PRBool *aResult)
{
  if (!aResult)
    return NS_ERROR_NULL_POINTER;
  *aResult = PR_FALSE;
  
  PRUint32 count;
  nsresult rv = mWindows->Count(&count);
  if (NS_FAILED(rv)) return rv;
  
  if (mWindows)
  {
    for(PRUint32 i = 0; i < count; i++)
    {
      nsCOMPtr<nsIMsgWindow> openWindow = getter_AddRefs((nsIMsgWindow*)mWindows->ElementAt(i));
      nsCOMPtr<nsIMsgFolder> openFolder;
      if (openWindow)
        openWindow->GetOpenFolder(getter_AddRefs(openFolder));
      if (folder == openFolder.get())
      {
        *aResult = PR_TRUE;
        break;
      }
    }
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsMsgMailSession::ConvertMsgURIToMsgURL(const char *aURI, nsIMsgWindow *aMsgWindow, char **aURL)
{
  if ((!aURI) || (!aURL))
    return NS_ERROR_NULL_POINTER;

  // convert the rdf msg uri into a url that represents the message...
  nsCOMPtr <nsIMsgMessageService> msgService;
  nsresult rv = GetMessageServiceFromURI(aURI, getter_AddRefs(msgService));
  if (NS_FAILED(rv)) 
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIURI> tURI;
  rv = msgService->GetUrlForUri(aURI, getter_AddRefs(tURI), aMsgWindow);
  if (NS_FAILED(rv)) 
    return NS_ERROR_NULL_POINTER;

  nsCAutoString urlString;
  if (NS_SUCCEEDED(tURI->GetSpec(urlString)))
  {
    *aURL = ToNewCString(urlString);
    if (!(aURL))
      return NS_ERROR_NULL_POINTER;
  }
  return rv;
}

//----------------------------------------------------------------------------------------
// GetSelectedLocaleDataDir - If a locale is selected, appends the selected locale to the
//                            defaults data dir and returns that new defaults data dir
//----------------------------------------------------------------------------------------
nsresult
nsMsgMailSession::GetSelectedLocaleDataDir(nsIFile *defaultsDir)
{                                                                               
  NS_ENSURE_ARG_POINTER(defaultsDir);                                     

  nsresult rv;                                                                
  PRBool baseDirExists = PR_FALSE;                                            
  rv = defaultsDir->Exists(&baseDirExists);                               
  NS_ENSURE_SUCCESS(rv,rv);                                                   

  if (baseDirExists) {                                                        
    nsCOMPtr<nsIXULChromeRegistry> packageRegistry =
      do_GetService("@mozilla.org/chrome/chrome-registry;1", &rv);
    if (NS_SUCCEEDED(rv)) {                                                 
      nsCAutoString localeName;                                           
      rv = packageRegistry->GetSelectedLocale(NS_LITERAL_CSTRING("global-region"), localeName);

      if (NS_SUCCEEDED(rv) && !localeName.IsEmpty()) {
        PRBool localeDirExists = PR_FALSE;                              
        nsCOMPtr<nsIFile> localeDataDir;                                
        
        rv = defaultsDir->Clone(getter_AddRefs(localeDataDir));     
        NS_ENSURE_SUCCESS(rv,rv);                                       

        rv = localeDataDir->AppendNative(localeName);
        NS_ENSURE_SUCCESS(rv,rv);                                       

        rv = localeDataDir->Exists(&localeDirExists);                   
        NS_ENSURE_SUCCESS(rv,rv);                                       

        if (localeDirExists) {                                          
          // use locale provider instead                              
          rv = defaultsDir->AppendNative(localeName);
          NS_ENSURE_SUCCESS(rv,rv);                                   
        }                                                               
      }                                                                   
    }                                                                       
  }                                                                           
  return NS_OK;                                                               
} 

//----------------------------------------------------------------------------------------
// GetDataFilesDir - Gets the application's default folder and then appends the 
//                   subdirectory named passed in as param dirName. If there is a seleccted
//                   locale, will append that to the dir path before returning the value
//----------------------------------------------------------------------------------------
NS_IMETHODIMP
nsMsgMailSession::GetDataFilesDir(const char* dirName, nsIFile **dataFilesDir)
{                                                                                                                                                    
  NS_ENSURE_ARG_POINTER(dataFilesDir);

  nsresult rv;
  nsCOMPtr<nsIProperties> directoryService = 
    do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr<nsIFile> defaultsDir;
  rv = directoryService->Get(NS_APP_DEFAULTS_50_DIR, 
                             NS_GET_IID(nsIFile), 
                             getter_AddRefs(defaultsDir));
  NS_ENSURE_SUCCESS(rv,rv);

  rv = defaultsDir->AppendNative(nsDependentCString(dirName));
  if (NS_SUCCEEDED(rv))
    rv = GetSelectedLocaleDataDir(defaultsDir);

  NS_IF_ADDREF(*dataFilesDir = defaultsDir);

  return rv;
}
