/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "msgCore.h" // for pre-compiled headers
#include "nsMsgMailSession.h"
#include "nsMsgLocalCID.h"
#include "nsMsgBaseCID.h"
#include "nsCOMPtr.h"
#include "nsIFileLocator.h"
#include "nsFileLocations.h"
#include "nsIMsgStatusFeedback.h"
#include "nsIMsgWindow.h"
#include "nsIMsgMessageService.h"
#include "nsMsgUtils.h"
#include "nsIURI.h"
#include "nsXPIDLString.h"

NS_IMPL_THREADSAFE_ISUPPORTS1(nsMsgMailSession, nsIMsgMailSession);

static NS_DEFINE_IID(kIFileLocatorIID,      NS_IFILELOCATOR_IID);
static NS_DEFINE_CID(kFileLocatorCID,       NS_FILELOCATOR_CID);
 

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
  mListeners->AppendElement(listener);
  return NS_OK;

}

NS_IMETHODIMP nsMsgMailSession::RemoveFolderListener(nsIFolderListener * listener)
{
  mListeners->RemoveElement(listener);
  return NS_OK;

}

NS_IMETHODIMP
nsMsgMailSession::NotifyFolderItemPropertyChanged(nsISupports *item,
                                                  nsIAtom *property,
                                                  const char* oldValue,
                                                  const char* newValue)
{
	nsresult rv;
	PRUint32 count;
	rv = mListeners->Count(&count);
	if (NS_FAILED(rv)) return rv;

	
	for(PRUint32 i = 0; i < count; i++)
	{
		nsCOMPtr<nsIFolderListener> listener = getter_AddRefs((nsIFolderListener*)mListeners->ElementAt(i));
		listener->OnItemPropertyChanged(item, property, oldValue, newValue);
	}

	return NS_OK;

}

NS_IMETHODIMP
nsMsgMailSession::NotifyFolderItemUnicharPropertyChanged(nsISupports *item,
                                                         nsIAtom *property,
                                                         const PRUnichar* oldValue,
                                                         const PRUnichar* newValue)
{
	nsresult rv;
	PRUint32 count;
	rv = mListeners->Count(&count);
	if (NS_FAILED(rv)) return rv;

	
	for(PRUint32 i = 0; i < count; i++)
	{
		nsCOMPtr<nsIFolderListener> listener = getter_AddRefs((nsIFolderListener*)mListeners->ElementAt(i));
		listener->OnItemUnicharPropertyChanged(item, property, oldValue, newValue);
	}

	return NS_OK;

}

NS_IMETHODIMP
nsMsgMailSession::NotifyFolderItemIntPropertyChanged(nsISupports *item,
                                                     nsIAtom *property,
                                                     PRInt32 oldValue,
                                                     PRInt32 newValue)
{
	nsresult rv;
	PRUint32 count;
	rv = mListeners->Count(&count);
	if (NS_FAILED(rv)) return rv;

	
	for(PRUint32 i = 0; i < count; i++)
	{
		nsCOMPtr<nsIFolderListener> listener = getter_AddRefs((nsIFolderListener*)mListeners->ElementAt(i));
		listener->OnItemIntPropertyChanged(item, property, oldValue, newValue);
	}

	return NS_OK;

}

NS_IMETHODIMP
nsMsgMailSession::NotifyFolderItemBoolPropertyChanged(nsISupports *item,
                                                      nsIAtom *property,
                                                      PRBool oldValue,
                                                      PRBool newValue)
{
	nsresult rv;
	PRUint32 count;
	rv = mListeners->Count(&count);
	if (NS_FAILED(rv)) return rv;

	
	for(PRUint32 i = 0; i < count; i++)
	{
		nsCOMPtr<nsIFolderListener> listener = getter_AddRefs((nsIFolderListener*)mListeners->ElementAt(i));
		listener->OnItemBoolPropertyChanged(item, property, oldValue, newValue);
	}

	return NS_OK;

}
NS_IMETHODIMP
nsMsgMailSession::NotifyFolderItemPropertyFlagChanged(nsISupports *item,
                                                      nsIAtom *property,
                                                      PRUint32 oldValue,
                                                      PRUint32 newValue)
{
	nsresult rv;
	PRUint32 count;
	rv = mListeners->Count(&count);
	if (NS_FAILED(rv)) return rv;

	
	for(PRUint32 i = 0; i < count; i++)
	{
		nsCOMPtr<nsIFolderListener> listener = getter_AddRefs((nsIFolderListener*)mListeners->ElementAt(i));
		listener->OnItemPropertyFlagChanged(item, property, oldValue, newValue);
	}

	return NS_OK;

}

NS_IMETHODIMP nsMsgMailSession::NotifyFolderItemAdded(nsISupports *parentItem, nsISupports *item, const char* viewString)
{
	nsresult rv;
	PRUint32 count;
	rv = mListeners->Count(&count);
	if (NS_FAILED(rv)) return rv;

	
	for(PRUint32 i = 0; i < count; i++)
	{
		nsCOMPtr<nsIFolderListener> listener = getter_AddRefs((nsIFolderListener*)mListeners->ElementAt(i));
		listener->OnItemAdded(parentItem, item, viewString);
	}

	return NS_OK;

}

NS_IMETHODIMP nsMsgMailSession::NotifyFolderItemDeleted(nsISupports *parentItem, nsISupports *item, const char* viewString)
{
	nsresult rv;
	PRUint32 count;
	rv = mListeners->Count(&count);
	if (NS_FAILED(rv)) return rv;

	
	for(PRUint32 i = 0; i < count; i++)
	{
		nsCOMPtr<nsIFolderListener> listener = getter_AddRefs((nsIFolderListener*)mListeners->ElementAt(i));
		listener->OnItemRemoved(parentItem, item, viewString);
	}
	return NS_OK;

}

NS_IMETHODIMP  nsMsgMailSession::NotifyFolderLoaded(nsIFolder *folder)
{

	nsresult rv;
	PRUint32 count;
	rv = mListeners->Count(&count);
	if (NS_FAILED(rv)) return rv;

	
	for(PRUint32 i = 0; i < count; i++)
	{
		nsCOMPtr<nsIFolderListener> listener = getter_AddRefs((nsIFolderListener*)mListeners->ElementAt(i));
		listener->OnFolderLoaded(folder);
	}
	return NS_OK;


}

NS_IMETHODIMP  nsMsgMailSession::NotifyDeleteOrMoveMessagesCompleted(nsIFolder *folder)
{

	nsresult rv;
	PRUint32 count;
	rv = mListeners->Count(&count);
	if (NS_FAILED(rv)) return rv;

	
	for(PRUint32 i = 0; i < count; i++)
	{
		nsCOMPtr<nsIFolderListener> listener = getter_AddRefs((nsIFolderListener*)mListeners->ElementAt(i));
		listener->OnDeleteOrMoveMessagesCompleted(folder);
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
	  nsCOMPtr<nsISupports> windowSupports = mWindows->ElementAt(0);
	  if(windowSupports)
		  rv = windowSupports->QueryInterface(NS_GET_IID(nsIMsgWindow), (void**)aMsgWindow);
	  if(NS_FAILED(rv))
		  return rv;
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
