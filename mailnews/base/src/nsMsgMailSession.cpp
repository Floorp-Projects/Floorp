/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

NS_IMPL_THREADSAFE_ADDREF(nsMsgMailSession)
NS_IMPL_THREADSAFE_RELEASE(nsMsgMailSession)
NS_INTERFACE_MAP_BEGIN(nsMsgMailSession)
  NS_INTERFACE_MAP_ENTRY(nsIMsgMailSession)
  NS_INTERFACE_MAP_ENTRY(nsIFolderListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIMsgMailSession)
NS_INTERFACE_MAP_END_THREADSAFE
  
static NS_DEFINE_CID(kMsgAccountManagerCID, NS_MSGACCOUNTMANAGER_CID);

nsMsgMailSession::nsMsgMailSession():
  mRefCnt(0)
{

	NS_INIT_REFCNT();
}


nsMsgMailSession::~nsMsgMailSession()
{
	Shutdown();
}

nsresult nsMsgMailSession::Init()
{
	nsresult rv = NS_NewISupportsArray(getter_AddRefs(mListeners));
	if(NS_FAILED(rv))
		return rv;

	rv = NS_NewISupportsArray(getter_AddRefs(mWindows));
	return rv;

}

nsresult nsMsgMailSession::Shutdown()
{
  return NS_OK;
}


NS_IMETHODIMP nsMsgMailSession::AddFolderListener(nsIFolderListener * listener)
{
  NS_ASSERTION(mListeners, "no listeners");
  if (!mListeners) return NS_ERROR_FAILURE;

  mListeners->AppendElement(listener);
  return NS_OK;
}

NS_IMETHODIMP nsMsgMailSession::RemoveFolderListener(nsIFolderListener * listener)
{
  NS_ASSERTION(mListeners, "no listeners");
  if (!mListeners) return NS_ERROR_FAILURE;

  mListeners->RemoveElement(listener);
  return NS_OK;

}

NS_IMETHODIMP
nsMsgMailSession::OnItemPropertyChanged(nsISupports *item,
                                        nsIAtom *property,
                                        const char* oldValue,
                                        const char* newValue)
{
	nsresult rv;
	PRUint32 count;

    NS_ASSERTION(mListeners, "no listeners");
    if (!mListeners) return NS_ERROR_FAILURE;

	rv = mListeners->Count(&count);
	if (NS_FAILED(rv)) return rv;

	
	for(PRUint32 i = 0; i < count; i++)
	{
		nsCOMPtr<nsIFolderListener> listener = getter_AddRefs((nsIFolderListener*)mListeners->ElementAt(i));
		NS_ASSERTION(listener, "listener is null");
		if (!listener) return NS_ERROR_FAILURE;
		listener->OnItemPropertyChanged(item, property, oldValue, newValue);
	}

	return NS_OK;

}

NS_IMETHODIMP
nsMsgMailSession::OnItemUnicharPropertyChanged(nsISupports *item,
                                               nsIAtom *property,
                                               const PRUnichar* oldValue,
                                               const PRUnichar* newValue)
{
	nsresult rv;
	PRUint32 count;

    NS_ASSERTION(mListeners, "no listeners");
    if (!mListeners) return NS_ERROR_FAILURE;

	rv = mListeners->Count(&count);
	if (NS_FAILED(rv)) return rv;

	
	for(PRUint32 i = 0; i < count; i++)
	{
		nsCOMPtr<nsIFolderListener> listener = getter_AddRefs((nsIFolderListener*)mListeners->ElementAt(i));
		NS_ASSERTION(listener, "listener is null");
		if (!listener) return NS_ERROR_FAILURE;
		listener->OnItemUnicharPropertyChanged(item, property, oldValue, newValue);
	}

	return NS_OK;

}

NS_IMETHODIMP
nsMsgMailSession::OnItemIntPropertyChanged(nsISupports *item,
                                                     nsIAtom *property,
                                                     PRInt32 oldValue,
                                                     PRInt32 newValue)
{
	nsresult rv;
	PRUint32 count;

	NS_ASSERTION(mListeners, "no listeners");
	if (!mListeners) return NS_ERROR_FAILURE;

	rv = mListeners->Count(&count);
	if (NS_FAILED(rv)) return rv;

	
	for(PRUint32 i = 0; i < count; i++)
	{
		nsCOMPtr<nsIFolderListener> listener = getter_AddRefs((nsIFolderListener*)mListeners->ElementAt(i));
		NS_ASSERTION(listener, "listener is null");
		if (!listener) return NS_ERROR_FAILURE;
		listener->OnItemIntPropertyChanged(item, property, oldValue, newValue);
	}

	return NS_OK;

}

NS_IMETHODIMP
nsMsgMailSession::OnItemBoolPropertyChanged(nsISupports *item,
                                            nsIAtom *property,
                                            PRBool oldValue,
                                            PRBool newValue)
{
	nsresult rv;
	PRUint32 count;

	NS_ASSERTION(mListeners, "no listeners");
	if (!mListeners) return NS_ERROR_FAILURE;

	rv = mListeners->Count(&count);
	if (NS_FAILED(rv)) return rv;

	
	for(PRUint32 i = 0; i < count; i++)
	{
		nsCOMPtr<nsIFolderListener> listener = getter_AddRefs((nsIFolderListener*)mListeners->ElementAt(i));
		NS_ASSERTION(listener, "listener is null");
		if (!listener) return NS_ERROR_FAILURE;
		listener->OnItemBoolPropertyChanged(item, property, oldValue, newValue);
	}

	return NS_OK;

}
NS_IMETHODIMP
nsMsgMailSession::OnItemPropertyFlagChanged(nsISupports *item,
                                            nsIAtom *property,
                                            PRUint32 oldValue,
                                            PRUint32 newValue)
{
	nsresult rv;
	PRUint32 count;

	NS_ASSERTION(mListeners, "no listeners");
	if (!mListeners) return NS_ERROR_FAILURE;

	rv = mListeners->Count(&count);
	if (NS_FAILED(rv)) return rv;

	
	for(PRUint32 i = 0; i < count; i++)
	{
		nsCOMPtr<nsIFolderListener> listener = getter_AddRefs((nsIFolderListener*)mListeners->ElementAt(i));
		NS_ASSERTION(listener, "listener is null");
		if (!listener) return NS_ERROR_FAILURE;
		listener->OnItemPropertyFlagChanged(item, property, oldValue, newValue);
	}

	return NS_OK;

}

NS_IMETHODIMP nsMsgMailSession::OnItemAdded(nsISupports *parentItem, nsISupports *item, const char* viewString)
{
	nsresult rv;
	PRUint32 count;

	NS_ASSERTION(mListeners, "no listeners");
	if (!mListeners) return NS_ERROR_FAILURE;

	rv = mListeners->Count(&count);
	if (NS_FAILED(rv)) return rv;

	
	for(PRUint32 i = 0; i < count; i++)
	{
		nsCOMPtr<nsIFolderListener> listener = getter_AddRefs((nsIFolderListener*)mListeners->ElementAt(i));
		NS_ASSERTION(listener, "listener is null");
		if (!listener) return NS_ERROR_FAILURE;
		listener->OnItemAdded(parentItem, item, viewString);
	}

	return NS_OK;

}

NS_IMETHODIMP nsMsgMailSession::OnItemRemoved(nsISupports *parentItem, nsISupports *item, const char* viewString)
{
	nsresult rv;
	PRUint32 count;

	NS_ASSERTION(mListeners, "no listeners");
	if (!mListeners) return NS_ERROR_FAILURE;

	rv = mListeners->Count(&count);
	if (NS_FAILED(rv)) return rv;

	
	for(PRUint32 i = 0; i < count; i++)
	{
		nsCOMPtr<nsIFolderListener> listener = getter_AddRefs((nsIFolderListener*)mListeners->ElementAt(i));
		NS_ASSERTION(listener, "listener is null");
		if (!listener) return NS_ERROR_FAILURE;
		listener->OnItemRemoved(parentItem, item, viewString);
	}
	return NS_OK;

}


NS_IMETHODIMP nsMsgMailSession::OnItemEvent(nsIFolder *aFolder,
                                                  nsIAtom *aEvent)
{
	nsresult rv;
	PRUint32 count;

	NS_ASSERTION(mListeners, "no listeners");
	if (!mListeners) return NS_ERROR_FAILURE;

	rv = mListeners->Count(&count);
	if (NS_FAILED(rv)) return rv;

	for(PRUint32 i = 0; i < count; i++)
	{
		nsCOMPtr<nsIFolderListener> listener = getter_AddRefs((nsIFolderListener*)mListeners->ElementAt(i));
		if(listener)
			listener->OnItemEvent(aFolder, aEvent);
	}
	return NS_OK;
}

nsresult nsMsgMailSession::GetTopmostMsgWindow(nsIMsgWindow* *aMsgWindow)
{
 
  //for right now just return the first msg window.  Eventually have to figure out which is topmost.
  nsresult rv;

  if (!aMsgWindow) return NS_ERROR_NULL_POINTER;
  
  *aMsgWindow = nsnull;
 
  if(mWindows)
  {
	PRUint32 count;
	rv = mWindows->Count(&count);
	if(NS_FAILED(rv))
		return rv;

    if(count > 0)
    {
      nsCOMPtr<nsISupports> windowSupports = getter_AddRefs(mWindows->ElementAt(count - 1));
      nsCOMPtr<nsIMsgWindow> msgWindow = do_QueryInterface(windowSupports, &rv);
      NS_ENSURE_SUCCESS(rv,rv);
      *aMsgWindow = msgWindow;
      NS_IF_ADDREF(*aMsgWindow);
    }
  }

  return NS_OK;
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
                 do_GetService(kMsgAccountManagerCID, &rv);
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
  nsIMsgMessageService  *msgService = nsnull;
  nsresult rv = GetMessageServiceFromURI(aURI, &msgService);
  if (NS_FAILED(rv)) 
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIURI> tURI;
  rv = msgService->GetUrlForUri(aURI, getter_AddRefs(tURI), aMsgWindow);
  if (NS_FAILED(rv)) 
    return NS_ERROR_NULL_POINTER;

  nsXPIDLCString urlString;
  if (NS_SUCCEEDED(tURI->GetSpec(getter_Copies(urlString))))
  {
    *aURL = nsCRT::strdup(urlString);
    if (!(aURL))
      return NS_ERROR_NULL_POINTER;
  }

  ReleaseMessageServiceFromURI(aURI, msgService);
  return rv;
}
