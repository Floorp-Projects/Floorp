/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsCOMPtr.h"
#include "nsAddrBookSession.h"
#include "nsIAddrBookSession.h"
#include "nsIFileSpec.h"
#include "nsIFileLocator.h"
#include "nsFileLocations.h"


static NS_DEFINE_CID(kFileLocatorCID, NS_FILELOCATOR_CID);


NS_IMPL_ISUPPORTS(nsAddrBookSession, nsCOMTypeInfo<nsIAddrBookSession>::GetIID());
    
nsAddrBookSession::nsAddrBookSession():
  mRefCnt(0), mpUserDirectory(nsnull)
{
	NS_INIT_REFCNT();

	mListeners = new nsVoidArray();
}

nsAddrBookSession::~nsAddrBookSession()
{
	if (mListeners) 
		delete mListeners;
	if (mpUserDirectory)
		delete mpUserDirectory;
}


// nsIAddrBookSession

NS_IMETHODIMP nsAddrBookSession::AddAddressBookListener(nsIAbListener * listener)
{
	mListeners->AppendElement(listener);
	return NS_OK;
}

NS_IMETHODIMP nsAddrBookSession::RemoveAddressBookListener(nsIAbListener * listener)
{
	mListeners->RemoveElement(listener);
	return NS_OK;
}

NS_IMETHODIMP nsAddrBookSession::NotifyItemPropertyChanged
(nsISupports *item, const char *property, const char* oldValue, const char* newValue)
{
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
	nsresult rv = NS_OK;
	if (!mpUserDirectory)
		mpUserDirectory = new nsFileSpec();

	NS_WITH_SERVICE(nsIFileLocator, locator, kFileLocatorCID, &rv);
	if (NS_FAILED(rv))
		return rv;

	nsIFileSpec* profiledir;
	rv = locator->GetFileLocation(nsSpecialFileSpec::App_UserProfileDirectory50, &profiledir);
	if (NS_FAILED(rv))
		return rv;
	profiledir->GetFileSpec(mpUserDirectory);

	*userDir = mpUserDirectory;
	return rv;
}


