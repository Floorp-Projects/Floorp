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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsCOMPtr.h"
#include "nsAddrBookSession.h"
#include "nsIAddrBookSession.h"
#include "nsIFileSpec.h"
#include "nsIDirectoryService.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsXPIDLString.h"


NS_IMPL_THREADSAFE_ISUPPORTS(nsAddrBookSession, NS_GET_IID(nsIAddrBookSession));
    
nsAddrBookSession::nsAddrBookSession():
  mRefCnt(0)
{
	NS_INIT_REFCNT();

	mListeners = new nsVoidArray();
}

nsAddrBookSession::~nsAddrBookSession()
{
	if (mListeners) 
		delete mListeners;
}


// nsIAddrBookSession

NS_IMETHODIMP nsAddrBookSession::AddAddressBookListener(nsIAbListener * listener)
{
    NS_ENSURE_TRUE(mListeners, NS_ERROR_NULL_POINTER);
	mListeners->AppendElement(listener);
	return NS_OK;
}

NS_IMETHODIMP nsAddrBookSession::RemoveAddressBookListener(nsIAbListener * listener)
{
    NS_ENSURE_TRUE(mListeners, NS_ERROR_NULL_POINTER);
	mListeners->RemoveElement(listener);
	return NS_OK;
}

NS_IMETHODIMP nsAddrBookSession::NotifyItemPropertyChanged
(nsISupports *item, const char *property, const PRUnichar* oldValue, const PRUnichar* newValue)
{
    NS_ENSURE_TRUE(mListeners, NS_ERROR_NULL_POINTER);

	PRInt32 i;
	PRInt32 count = mListeners->Count();
	for(i = 0; i < count; i++)
	{
		nsIAbListener* listener =(nsIAbListener*)mListeners->ElementAt(i);
		listener->OnItemPropertyChanged(item, property, oldValue, newValue);
	}

	return NS_OK;

}

NS_IMETHODIMP nsAddrBookSession::NotifyDirectoryItemAdded(nsIAbDirectory *directory, nsISupports *item)
{
    NS_ENSURE_TRUE(mListeners, NS_ERROR_NULL_POINTER);

	PRInt32 i;
	PRInt32 count = mListeners->Count();
	for(i = 0; i < count; i++)
	{
		nsIAbListener *listener = (nsIAbListener*)mListeners->ElementAt(i);
		listener->OnItemAdded(directory, item);
	}

	return NS_OK;

}

NS_IMETHODIMP nsAddrBookSession::NotifyDirectoryItemDeleted(nsIAbDirectory *directory, nsISupports *item)
{
    NS_ENSURE_TRUE(mListeners, NS_ERROR_NULL_POINTER);

	PRInt32 i;
	PRInt32 count = mListeners->Count();
	for(i = 0; i < count; i++)
	{
		nsIAbListener *listener = (nsIAbListener*)mListeners->ElementAt(i);
		listener->OnItemRemoved(directory, item);
	}
	return NS_OK;

}

NS_IMETHODIMP nsAddrBookSession::GetUserProfileDirectory(nsFileSpec * *userDir)
{
    NS_ENSURE_ARG_POINTER(userDir);
    *userDir = nsnull;

	nsresult rv;		
	nsCOMPtr<nsIFile> profileDir;
	nsXPIDLCString pathBuf;
	
	rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(profileDir));
	if (NS_FAILED(rv)) return rv;
	rv = profileDir->GetPath(getter_Copies(pathBuf));
	if (NS_FAILED(rv)) return rv;
	
	*userDir = new nsFileSpec(pathBuf);
	NS_ENSURE_TRUE(*userDir, NS_ERROR_OUT_OF_MEMORY);

	return rv;
}


